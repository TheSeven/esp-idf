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
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include "multi_heap.h"


typedef struct multi_heap_info heap_t;

extern heap_t iheap_handle, dheap_handle, sheap_handle;
extern int _iheap_start, _iheap_end, _dheap_start, _dheap_end, _sheap_start, _sheap_end;
extern int initiram_handle, initdram_handle;
