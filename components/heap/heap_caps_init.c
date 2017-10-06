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
    
    multi_heap_check(&iheap_handle, true);
    multi_heap_check(&dheap_handle, true);
    multi_heap_check(&sheap_handle, true);
}

void heap_free_initram()
{
    ESP_LOGI("heap_free_initram", "Releasing %d bytes of init code...", (&initiram_handle)[-1] - (void*)&initiram_handle + 4);
    heap_caps_free(&initiram_handle);
    ESP_LOGI("heap_free_initram", "Releasing %d bytes of init data...", (&initdram_handle)[-1] - (void*)&initdram_handle + 4);
    heap_caps_free(&initdram_handle);
}
