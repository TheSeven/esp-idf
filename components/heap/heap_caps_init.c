// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "heap_private.h"
#include <multi_heap.h>
#include <esp_heap_caps.h>
#include <esp_log.h>


portMUX_TYPE iheap_mtx __attribute__((section(".heapmtx"))) = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE dheap_mtx __attribute__((section(".heapmtx"))) = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE sheap_mtx __attribute__((section(".heapmtx"))) = portMUX_INITIALIZER_UNLOCKED;


void heap_init()
{
    // Repair app CPU startup damage
    *(uint32_t*)0x3ffe3ffc = 0x3ffe4350;
    *(uint32_t*)0x3ffe4350 = 0x3ffffff9;
    *(uint32_t*)0x3ffe4354 = 0x3ffffff8;
    
#ifdef CONFIG_HEAP_BOOT_CHECK
    multi_heap_check(&iheap_handle, true);
    multi_heap_check(&dheap_handle, true);
    multi_heap_check(&sheap_handle, true);
#endif
}

void heap_free_initram()
{
#ifdef CONFIG_HEAP_BOOT_CHECK
    multi_heap_check(&iheap_handle, true);
    multi_heap_check(&dheap_handle, true);
    multi_heap_check(&sheap_handle, true);
#endif

#ifdef CONFIG_HEAP_BOOT_DUMP
    multi_heap_dump(&iheap_handle);
    multi_heap_dump(&dheap_handle);
    multi_heap_dump(&sheap_handle);
#endif

    ESP_LOGI("heap_free_initram", "Releasing %d bytes of init code...", (&initiram_handle)[-1] - (void*)&initiram_handle + 4);
    heap_caps_free(&initiram_handle);
    ESP_LOGI("heap_free_initram", "Releasing %d bytes of init data...", (&initdram_handle)[-1] - (void*)&initdram_handle + 4);
    heap_caps_free(&initdram_handle);

#ifdef CONFIG_HEAP_BOOT_STATS
    heap_t* const heaps[] = {&iheap_handle, &dheap_handle, &sheap_handle};
    const char* const names[] = {"IHEAP", "DHEAP", "SHEAP"};
    multi_heap_info_t info;
    for (uint32_t i = 0; i < 3; i++)
    {
        multi_heap_get_info(heaps[i], &info);
        ESP_LOGI("heap", "%s used: %d bytes in %d blocks, free: %d bytes in %d blocks (largest: %d bytes), minimum free: %d bytes", names[i],
                 info.total_allocated_bytes, info.allocated_blocks, info.total_free_bytes, info.free_blocks, info.largest_free_block, info.minimum_free_bytes);
    }
#endif
}
