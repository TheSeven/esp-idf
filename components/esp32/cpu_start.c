// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_err.h"

#include "rom/ets_sys.h"
#include "rom/uart.h"
#include "rom/rtc.h"
#include "rom/cache.h"

#include "soc/cpu.h"
#include "soc/rtc.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/timer_group_reg.h"

#include "driver/rtc_io.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

#include "tcpip_adapter.h"

#include "esp_heap_caps_init.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_event.h"
#include "esp_ipc.h"
#include "esp_crosscore_int.h"
#include "esp_dport_access.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_newlib.h"
#include "esp_brownout.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_phy_init.h"
#include "esp_cache_err_int.h"
#include "esp_coexist.h"
#include "esp_panic.h"
#include "esp_core_dump.h"
#include "esp_app_trace.h"
#include "esp_spiram.h"
#include "esp_clk.h"
#include "esp_timer.h"
#include "trax.h"

#define STRINGIFY(s) STRINGIFY2(s)
#define STRINGIFY2(s) #s

void start_cpu0() INITIRAM_ATTR __attribute__((weak, alias("start_cpu0_default"))) __attribute__((noreturn));
void start_cpu0_default() INITIRAM_ATTR __attribute__((noreturn));
#if !CONFIG_FREERTOS_UNICORE
extern void cpu1_entrypoint();
void start_cpu1() INITIRAM_ATTR __attribute__((weak, alias("start_cpu1_default"))) __attribute__((noreturn));
void start_cpu1_default() INITIRAM_ATTR __attribute__((noreturn));
static volatile bool app_cpu_started INITBSS_ATTR = false;
#endif //!CONFIG_FREERTOS_UNICORE

static void do_global_ctors() INITIRAM_ATTR;
static void main_task(void* args);
extern void app_main();
extern esp_err_t esp_pthread_init(void);

extern int _bss_start;
extern int _bss_end;
extern int _initbss_start;
extern int _initbss_end;
extern int _rtc_bss_start;
extern int _rtc_bss_end;
extern int _init_start;
extern void (*__init_array_start)();
extern void (*__init_array_end)();
extern volatile int port_xSchedulerRunning[2];

static const char* TAG INITDRAM_ATTR = "cpu_start";

struct object { long placeholder[ 10 ]; };
void __register_frame_info (const void *begin, struct object *ob);
extern char __eh_frame[];


INITIRAM_ATTR __attribute__((noreturn)) void cpu0_init()
{
    #if CONFIG_FREERTOS_UNICORE
    RESET_REASON rst_reas[1];
#else
    RESET_REASON rst_reas[2];
#endif
    cpu_configure_region_protection();

    //Move exception vectors to IRAM
    asm volatile ("wsr %0, vecbase\n" :: "r"(&_init_start));

    rst_reas[0] = rtc_get_reset_reason(0);

#if !CONFIG_FREERTOS_UNICORE
    rst_reas[1] = rtc_get_reset_reason(1);
#endif

    // from panic handler we can be reset by RWDT or TG0WDT
    if (rst_reas[0] == RTCWDT_SYS_RESET || rst_reas[0] == TG0WDT_SYS_RESET
#if !CONFIG_FREERTOS_UNICORE
        || rst_reas[1] == RTCWDT_SYS_RESET || rst_reas[1] == TG0WDT_SYS_RESET
#endif
    ) {
        esp_panic_wdt_stop();
    }

    //Clear BSS. Please do not attempt to do any complex stuff (like early logging) before this.
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));
    memset(&_initbss_start, 0, (&_initbss_end - &_initbss_start) * sizeof(_initbss_start));
    
    /* Unless waking from deep sleep (implying RTC memory is intact), clear RTC bss */
    if (rst_reas[0] != DEEPSLEEP_RESET) {
        memset(&_rtc_bss_start, 0, (&_rtc_bss_end - &_rtc_bss_start) * sizeof(_rtc_bss_start));
    }

#if CONFIG_SPIRAM_BOOT_INIT
    if (esp_spiram_init() != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "Failed to init external RAM!");
        abort();
    }
#endif

    ESP_EARLY_LOGI(TAG, "Pro cpu up.");
    
#if !CONFIG_FREERTOS_UNICORE
    ESP_EARLY_LOGI(TAG, "Starting app cpu...");
    //Flush and enable icache for APP CPU
    Cache_Flush(1);
    Cache_Read_Enable(1);
    esp_cpu_unstall(1);
    ets_set_appcpu_boot_addr((uint32_t)cpu1_entrypoint);
    // Enable clock and reset APP CPU. Note that OpenOCD may have already
    // enabled clock and taken APP CPU out of reset. In this case don't reset
    // APP CPU again, as that will clear the breakpoints which may have already
    // been set.
    if (!DPORT_GET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN)) {
        DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
        DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_C_REG, DPORT_APPCPU_RUNSTALL);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
    }

    while (!app_cpu_started);
    ESP_EARLY_LOGI(TAG, "App cpu up.");
#else
    ESP_EARLY_LOGI(TAG, "Single core mode");
    DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
#endif


#if CONFIG_SPIRAM_MEMTEST
    bool ext_ram_ok=esp_spiram_test();
    if (!ext_ram_ok) {
        ESP_EARLY_LOGE(TAG, "External RAM failed memory test!");
        abort();
    }
#endif

    /* Initialize heap allocator. WARNING: This *needs* to happen *after* the app cpu has booted.
       If the heap allocator is initialized first, it will put free memory linked list items into
       memory also used by the ROM. Starting the app cpu will let its ROM initialize that memory,
       corrupting those linked lists. Initializing the allocator *after* the app cpu has booted
       works around this problem.
       With SPI RAM enabled, there's a second reason: half of the SPI RAM will be managed by the
       app CPU, and when that is not up yet, the memory will be inaccessible and heap_caps_init may
       fail initializing it properly. */
    heap_init();

    ESP_EARLY_LOGI(TAG, "Pro cpu start user code");
    start_cpu0();
}

#if !CONFIG_FREERTOS_UNICORE

static INITIRAM_ATTR void wdt_reset_cpu1_info_enable(void)
{
    DPORT_REG_SET_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_PDEBUG_ENABLE | DPORT_APP_CPU_RECORD_ENABLE);
    DPORT_REG_CLR_BIT(DPORT_APP_CPU_RECORD_CTRL_REG, DPORT_APP_CPU_RECORD_ENABLE);
}

INITIRAM_ATTR __attribute__((noreturn)) void cpu1_init()
{
    asm volatile ("wsr %0, vecbase\n" :: "r"(&_init_start));

    ets_set_appcpu_boot_addr(0);
    cpu_configure_region_protection();

#if CONFIG_CONSOLE_UART_NONE
    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
#else // CONFIG_CONSOLE_UART_NONE
    uartAttach();
    ets_install_uart_printf();
    uart_tx_switch(CONFIG_CONSOLE_UART_NUM);
#endif

    wdt_reset_cpu1_info_enable();
    app_cpu_started = 1;
    start_cpu1();
}
#endif //!CONFIG_FREERTOS_UNICORE

static INITIRAM_ATTR void intr_matrix_clear(void)
{
    //Clear all the interrupt matrix register
    for (int i = ETS_WIFI_MAC_INTR_SOURCE; i <= ETS_CACHE_IA_INTR_SOURCE; i++) {
        intr_matrix_set(0, i, ETS_INVALID_INUM);
#if !CONFIG_FREERTOS_UNICORE
        intr_matrix_set(1, i, ETS_INVALID_INUM);
#endif
    }
}

void start_cpu0_default(void)
{
    esp_err_t err;
    esp_setup_syscall_table();

#if CONFIG_SPIRAM_BOOT_INIT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC)
    esp_err_t r=esp_spiram_add_to_heapalloc();
    if (r != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "External RAM could not be added to heap!");
        abort();
    }
#if CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL
    r=esp_spiram_reserve_dma_pool(CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL);
    if (r != ESP_OK) {
        ESP_EARLY_LOGE(TAG, "Could not reserve internal/DMA pool!");
        abort();
    }
#endif
#if CONFIG_SPIRAM_USE_MALLOC
    heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
#endif
#endif

//Enable trace memory and immediately start trace.
#if CONFIG_ESP32_TRAX
#if CONFIG_ESP32_TRAX_TWOBANKS
    trax_enable(TRAX_ENA_PRO_APP);
#else
    trax_enable(TRAX_ENA_PRO);
#endif
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif
    esp_clk_init();
    esp_perip_clk_init();
    intr_matrix_clear();
#ifndef CONFIG_CONSOLE_UART_NONE
    uart_div_modify(CONFIG_CONSOLE_UART_NUM, (rtc_clk_apb_freq_get() << 4) / CONFIG_CONSOLE_UART_BAUDRATE);
#endif
#if CONFIG_BROWNOUT_DET
    esp_brownout_init();
#endif
    rtc_gpio_force_hold_dis_all();
    esp_vfs_dev_uart_register();
    esp_reent_init(_GLOBAL_REENT);
#ifndef CONFIG_CONSOLE_UART_NONE
    const char* default_uart_dev = "/dev/uart/" STRINGIFY(CONFIG_CONSOLE_UART_NUM);
    _GLOBAL_REENT->_stdin  = fopen(default_uart_dev, "r");
    _GLOBAL_REENT->_stdout = fopen(default_uart_dev, "w");
    _GLOBAL_REENT->_stderr = fopen(default_uart_dev, "w");
#else
    _GLOBAL_REENT->_stdin  = (FILE*) &__sf_fake_stdin;
    _GLOBAL_REENT->_stdout = (FILE*) &__sf_fake_stdout;
    _GLOBAL_REENT->_stderr = (FILE*) &__sf_fake_stderr;
#endif
    esp_timer_init();
    esp_set_time_from_rtc();
#if CONFIG_ESP32_APPTRACE_ENABLE
    err = esp_apptrace_init();
    assert(err == ESP_OK && "Failed to init apptrace module on PRO CPU!");
#endif
#if CONFIG_SYSVIEW_ENABLE
    SEGGER_SYSVIEW_Conf();
#endif
    err = esp_pthread_init();
    assert(err == ESP_OK && "Failed to init pthread module!");

    do_global_ctors();
#if CONFIG_INT_WDT
    esp_int_wdt_init();
#endif
#if CONFIG_TASK_WDT
    esp_task_wdt_init();
#endif
    esp_cache_err_int_init();
    esp_crosscore_int_init();
    esp_ipc_init();
#ifndef CONFIG_FREERTOS_UNICORE
    esp_dport_access_int_init();
#endif
    spi_flash_init();
    /* init default OS-aware flash access critical section */
    spi_flash_guard_set(&g_flash_guard_default_ops);

#if CONFIG_ESP32_ENABLE_COREDUMP
    esp_core_dump_init();
#endif

    portBASE_TYPE res = xTaskCreatePinnedToCore(&main_task, "main",
                                                ESP_TASK_MAIN_STACK, NULL,
                                                ESP_TASK_MAIN_PRIO, NULL, 0);
    assert(res == pdTRUE);
    ESP_EARLY_LOGI(TAG, "Starting scheduler on PRO CPU.");
    vTaskStartScheduler();
    abort(); /* Only get to here if not enough free heap to start scheduler */
}

#if !CONFIG_FREERTOS_UNICORE
void start_cpu1_default(void)
{
    // Wait for FreeRTOS initialization to finish on PRO CPU
    while (!port_xSchedulerRunning[0]);

#if CONFIG_ESP32_TRAX_TWOBANKS
    trax_start_trace(TRAX_DOWNCOUNT_WORDS);
#endif
#if CONFIG_ESP32_APPTRACE_ENABLE
    esp_err_t err = esp_apptrace_init();
    assert(err == ESP_OK && "Failed to init apptrace module on APP CPU!");
#endif
    //Take care putting stuff here: if asked, FreeRTOS will happily tell you the scheduler
    //has started, but it isn't active *on this CPU* yet.
    esp_cache_err_int_init();
    esp_crosscore_int_init();
    esp_dport_access_int_init();

    ESP_EARLY_LOGI(TAG, "Starting scheduler on APP CPU.");
    xPortStartScheduler();
    abort(); /* Only get to here if FreeRTOS somehow very broken */
}
#endif //!CONFIG_FREERTOS_UNICORE

static void do_global_ctors(void)
{
    static struct object ob;
    __register_frame_info( __eh_frame, &ob );

    void (**p)(void);
    for (p = &__init_array_end - 1; p >= &__init_array_start; --p) {
        (*p)();
    }
}

static void main_task(void* args)
{
    // Now that the application is about to start, disable boot watchdogs
    REG_CLR_BIT(TIMG_WDTCONFIG0_REG(0), TIMG_WDT_FLASHBOOT_MOD_EN_S);
    REG_CLR_BIT(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_FLASHBOOT_MOD_EN);
#if !CONFIG_FREERTOS_UNICORE
    // Wait for FreeRTOS initialization to finish on APP CPU, before replacing its startup stack
    while (!port_xSchedulerRunning[1]);
#endif
    app_main();
    vTaskDelete(NULL);
}

