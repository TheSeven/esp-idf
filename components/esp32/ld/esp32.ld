#include "sdkconfig.h"

#ifndef CONFIG_BT_RESERVE_DRAM
#define CONFIG_BT_RESERVE_DRAM 0
#endif
#ifndef CONFIG_BT_ENABLED
#define CONFIG_BT_ENABLED 0
#endif
#ifndef CONFIG_MEMMAP_TRACEMEM_TWOBANKS
#define CONFIG_MEMMAP_TRACEMEM_TWOBANKS 0
#endif

#if CONFIG_BT_ENABLED
#define DRAM_BASE 0x3FFAFF10
#else
#define DRAM_BASE 0x3FFAE2A0
#endif
#if CONFIG_MEMMAP_TRACEMEM_TWOBANKS
#define SRAM_END 0x3FFF8000
#else
#define SRAM_END 0x40000000
#endif

MEMORY
{
  drom0_0_seg (R) :                  org = 0x3F400018, len = 0x400000-0x18
  dram0_0_seg (RW) : org = DRAM_BASE, len = 0x3FFE0000 - DRAM_BASE
  sram0_0_seg (RW) : org = 0x3FFE0440, len = SRAM_END - 0x3FFE0440
  iram0_0_seg (RX) : org = 0x40080000, len = 0x20000
  rtc_iram_seg (RWX) : org = 0x400C0000, len = 0x2000
  iram0_2_seg (RX) :                 org = 0x400D0018, len = 0x330000-0x18
  rtc_slow_seg(RW) : org = 0x50000000 + CONFIG_ULP_COPROC_RESERVE_MEM, len = 0x1000 - CONFIG_ULP_COPROC_RESERVE_MEM
}

/*  Default entry point:  */
ENTRY(cpu0_entrypoint);

SECTIONS
{
  /* RTC fast memory holds RTC wake stub code,
     including from any source file named rtc_wake_stub*.c
  */
  .rtc.text :
  {
    . = ALIGN(4);
    *(.rtc.literal .rtc.text)
    *rtc_wake_stub*.o(.literal .text .literal.* .text.*)
  } >rtc_iram_seg

  /* RTC slow memory holds RTC wake stub
     data/rodata, including from any source file
     named rtc_wake_stub*.c
  */
  .rtc.data :
  {
    _rtc_data_start = ABSOLUTE(.);
    *(.rtc.data)
    *(.rtc.rodata)
    *rtc_wake_stub*.o(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);
  } > rtc_slow_seg

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.o(.bss .bss.*)
    *rtc_wake_stub*.o(COMMON)
    _rtc_bss_end = ABSOLUTE(.);
  } > rtc_slow_seg

  /* Send .iram0 code to iram */
  .iram0.vectors :
  {
    /* Vectors go to IRAM */
    _init_start = ABSOLUTE(.);
    /* Vectors according to builds/RF-2015.2-win32/esp108_v1_2_s5_512int_2/config.html */
    . = 0x0;
    KEEP(*(.WindowVectors.text));
    . = 0x180;
    KEEP(*(.Level2InterruptVector.text));
    . = 0x1c0;
    KEEP(*(.Level3InterruptVector.text));
    . = 0x200;
    KEEP(*(.Level4InterruptVector.text));
    . = 0x240;
    KEEP(*(.Level5InterruptVector.text));
    . = 0x280;
    KEEP(*(.DebugExceptionVector.text));
    . = 0x2c0;
    KEEP(*(.NMIExceptionVector.text));
    . = 0x300;
    KEEP(*(.KernelExceptionVector.text));
    . = 0x340;
    KEEP(*(.UserExceptionVector.text));
    . = 0x3C0;
    KEEP(*(.DoubleExceptionVector.text));
    . = 0x400;
    *(.*Vector.literal)

    *(.UserEnter.literal);
    *(.UserEnter.text);
    . = ALIGN(4);
    *(.entry.text)
    *(.init.literal)
    *(.init)
    _init_end = ABSOLUTE(.);

    /* This goes here, not at top of linker script, so addr2line finds it last,
       and uses it in preference to the first symbol in IRAM */
    _iram_start = ABSOLUTE(0);
  } > iram0_0_seg

  .iram0.text :
  {
    /* Code marked as runnning out of IRAM */
    _iram_text_start = ABSOLUTE(.);
    *(.iram1 .iram1.*)
    *libfreertos.a:(.literal .text .literal.* .text.*)
    *libheap.a:multi_heap.o(.literal .text .literal.* .text.*)
    *libheap.a:multi_heap_poisoning.o(.literal .text .literal.* .text.*)
    *libesp32.a:panic.o(.literal .text .literal.* .text.*)
    *libesp32.a:core_dump.o(.literal .text .literal.* .text.*)
    *libapp_trace.a:(.literal .text .literal.* .text.*)
    *libxtensa-debug-module.a:eri.o(.literal .text .literal.* .text.*)
    *libphy.a:(.literal .text .literal.* .text.*)
    *librtc.a:(.literal .text .literal.* .text.*)
    *libsoc.a:(.literal .text .literal.* .text.*)
    *libhal.a:(.literal .text .literal.* .text.*)
    *libgcc.a:lib2funcs.o(.literal .text .literal.* .text.*)
    *libspi_flash.a:spi_flash_rom_patch.o(.literal .text .literal.* .text.*)
    *libgcov.a:(.literal .text .literal.* .text.*)
    INCLUDE esp32.spiram.rom-functions-iram.ld
    _iram_text_end = ABSOLUTE(.);
    . = ALIGN(4);
    iheap_handle = .;
    _iheap_start = ABSOLUTE(.);
    LONG(iheap_mtx);
    LONG(_iheap_end - _iheap_start + _inittext_start - _inittext_end - 0x28);
    LONG(_iheap_end - _iheap_start + _inittext_start - _inittext_end - 0x28);
    LONG(_iheap_last_block);
    LONG(_iheap_init_block | 1);
    LONG(_iheap_free_block);
    _iheap_init_block = ABSOLUTE(.);
    LONG(_iheap_free_block);
    initiram_handle = .;
    _inittext_start = ABSOLUTE(.);
    *(.initiram.literal .initiram .initiram.literal.* .initiram.*)
    . = ALIGN(4);
    _inittext_end = ABSOLUTE(.);
    _iheap_free_block = ABSOLUTE(.);
    LONG(_iheap_last_block | 1);
    LONG(_iheap_last_block);
  } > iram0_0_seg

  .iram0.end 0x4009fff8 :
  {
    _iheap_last_block = ABSOLUTE(.);
    LONG(1);
    LONG(0);
    _iheap_end = ABSOLUTE(.);
  } > iram0_0_seg

#if CONFIG_BT_ENABLED
  .dram0.btgap0_header :
  {
    dheap_handle = .;
    _dheap_start = ABSOLUTE(.);
    LONG(dheap_mtx);
#if CONFIG_BT_DRAM_RELEASE
    LONG(_dheap_end - _dheap_start + bss_start - _data_end + initdata_start - initbss_end - 0x9040);
    LONG(_dheap_end - _dheap_start + bss_start - _data_end + initdata_start - initbss_end - 0x9040);
#else
    LONG(_dheap_end - _dheap_start + bss_start - _data_end + initdata_start - initbss_end - 0x10030);
    LONG(_dheap_end - _dheap_start + bss_start - _data_end + initdata_start - initbss_end - 0x10030);
#endif
    LONG(_dheap_last_block);
    LONG(_dheap_btgap0_block | 1);
    LONG(_dheap_btgap0_block);
    _dheap_btgap0_block = ABSOLUTE(.);
    LONG(_dheap_btmem0_block | 1);
    LONG(_dheap_btgap1_block);
  } > dram0_0_seg

  .dram0.btmem0_header 0x3ffafffc :
  {
    _dheap_btmem0_block = ABSOLUTE(.);
#if CONFIG_BT_DRAM_RELEASE
    LONG(_dheap_btgap1_block);
  } > dram0_0_seg

  .dram0.btgap1_header 0x3ffb3000 :
  {
    _dheap_btgap1_block = ABSOLUTE(.);
    LONG(_dheap_btmem1_block | 1);
    LONG(_dheap_btgap2_block);
  } > dram0_0_seg

  .dram0.btmem1_header 0x3ffb7ffc :
  {
    _dheap_btmem1_block = ABSOLUTE(.);
    LONG(_dheap_btgap2_block);
  } > dram0_0_seg

  .dram0.btgap2_header 0x3ffbbb28 :
  {
    _dheap_btgap2_block = ABSOLUTE(.);
    LONG(_dheap_btmem2_block | 1);
    LONG(_dheap_free_block);
  } > dram0_0_seg

  .dram0.btgap2 0x3ffbdb24 :
  {
    _dheap_btmem2_block = ABSOLUTE(.);
#endif
    LONG(_dheap_init_block);
  } > dram0_0_seg

  .dram0.bss 0x3ffc0000 (NOLOAD) :
#else
  .dram0.bss (NOLOAD) :
#endif
  {
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.share.mem)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = ABSOLUTE(.);
  } >dram0_0_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    *(.dram1 .dram1.*)
    *libesp32.a:panic.o(.rodata .rodata.*)
    *libphy.a:(.rodata .rodata.*)
    *libapp_trace.a:(.rodata .rodata.*)
    *libgcov.a:(.rodata .rodata.*)
    *libheap.a:multi_heap.o(.rodata .rodata.*)
    *libheap.a:multi_heap_poisoning.o(.rodata .rodata.*)
    INCLUDE esp32.spiram.rom-functions-dram.ld
    KEEP(*(.heapmtx .heapmtx.*))
    _data_end = ABSOLUTE(.);
#if !CONFIG_BT_ENABLED
    . = ALIGN(4);
    dheap_handle = .;
    _dheap_start = ABSOLUTE(.);
    LONG(dheap_mtx);
    LONG(_dheap_end - _dheap_start + _initdata_start - _initbss_end - 0x28);
    LONG(_dheap_end - _dheap_start + _initdata_start - _initbss_end - 0x28);
    LONG(_dheap_last_block);
    LONG(_dheap_init_block | 1);
    LONG(_dheap_free_block);
#endif
    _dheap_init_block = ABSOLUTE(.);
    LONG(_dheap_free_block);
    initdram_handle = .;
    _initdata_start = ABSOLUTE(.);
    *(.initdram)
    *(.initdram.*)
    _initdata_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > dram0_0_seg

  .dram0.initbss (NOLOAD) :
  {
    . = ALIGN(16);
    _cpu0_initstack = .;
    . += 0x400;
    _cpu0_initstack_top = .;
    _cpu1_initstack = .;
    . += 0x200;
    _cpu1_initstack_top = .;
    _initbss_start = ABSOLUTE(.);
    *(.initbss)
    *(.initbss.*)
    . = ALIGN(4);
    _initbss_end = ABSOLUTE(.);
  } > dram0_0_seg

  .dram0.free_header :
  {
    _dheap_free_block = ABSOLUTE(.);
    LONG(_dheap_last_block | 1);
    LONG(_dheap_last_block);
  } > dram0_0_seg

  .dram0.end 0x3ffdfff8 :
  {
    _dheap_last_block = ABSOLUTE(.);
    LONG(1);
    LONG(0);
    _dheap_end = ABSOLUTE(.);
  } > dram0_0_seg

  .sram0.heap :
  {
    sheap_handle = .;
    _sheap_start = ABSOLUTE(.);
    LONG(sheap_mtx);
    LONG(_sheap_end - _sheap_start - 0x37c);
    LONG(_sheap_end - _sheap_start - 0x37c);
    LONG(_sheap_last_block);
    LONG(_sheap_gap_block | 1);
    LONG(_sheap_gap_block);
    _sheap_gap_block = ABSOLUTE(.);
    LONG(_sheap_rom_block | 1);
    LONG(_sheap_free_block);
  } > sram0_0_seg

  .sram0.rom_header 0x3ffe3ffc :
  {
    _sheap_rom_block = ABSOLUTE(.);
    LONG(_sheap_free_block);
  } > sram0_0_seg

  .sram0.free_header 0x3ffe4350 :
  {
    _sheap_free_block = ABSOLUTE(.);
    LONG(_sheap_last_block | 1);
    LONG(_sheap_last_block);
  } > sram0_0_seg

#if CONFIG_MEMMAP_TRACEMEM
  .sram0.end 0x3fff7ff8 :
#else
  .sram0.end 0x3ffffff8 :
#endif
  {
    _sheap_last_block = ABSOLUTE(.);
    LONG(1);
    LONG(0);
    _sheap_end = ABSOLUTE(.);
  } > sram0_0_seg

  .flash.rodata :
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.irom1.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;
    /*  C++ constructor and destructor tables, properly ordered:  */
    __init_array_start = ABSOLUTE(.);
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __init_array_end = ABSOLUTE(.);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    _rodata_end = ABSOLUTE(.);
    /* Literals are also RO data. */
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
  } >drom0_0_seg

  .flash.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;

    /* Similar to _iram_start, this symbol goes here so it is
       resolved by addr2line in preference to the first symbol in
       the flash.text segment.
    */
    _flash_cache_start = ABSOLUTE(0);
  } >iram0_2_seg
}
