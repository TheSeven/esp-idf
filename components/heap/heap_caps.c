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
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/param.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "multi_heap.h"
#include "esp_log.h"
#include "heap_private.h"

/*
  This takes a memory chunk in a region that can be addressed as both DRAM as well as IRAM. It will convert it to
  IRAM in such a way that it can be later freed. It assumes both the address as wel as the length to be word-aligned.
  It returns a region that's 1 word smaller than the region given because it stores the original Dram address there.

  In theory, we can also make this work by prepending a struct that looks similar to the block link struct used by the
  heap allocator itself, which will allow inspection tools relying on any block returned from any sort of malloc to
  have such a block in front of it, work. We may do this later, if/when there is demand for it. For now, a simple
  pointer is used.
*/
IRAM_ATTR static void *dram_alloc_to_iram_addr(void *addr, size_t len)
{
    uint32_t dstart = (int)addr; //First word
    uint32_t dend = ((int)addr) + len - 4; //Last word
    assert(dstart >= SOC_DIRAM_DRAM_LOW);
    assert(dend <= SOC_DIRAM_DRAM_HIGH);
    assert((dstart & 3) == 0);
    assert((dend & 3) == 0);
    uint32_t istart = SOC_DIRAM_IRAM_LOW + (SOC_DIRAM_DRAM_HIGH - dend);
    uint32_t *iptr = (uint32_t *)istart;
    *iptr = dstart;
    return (void *)(iptr + 1);
}

/*
Routine to allocate a bit of memory with certain capabilities. caps is a bitfield of MALLOC_CAP_* bits.
*/
IRAM_ATTR void* heap_caps_malloc(size_t size, uint32_t caps)
{
    void* ret = NULL;
    if (caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))
    {
        ret = multi_heap_malloc(&dheap_handle, size);
        if (ret) return ret;
        return multi_heap_malloc(&sheap_handle, size);
    }
    else
    {
        if (caps & MALLOC_CAP_EXEC) return NULL;
        ret = multi_heap_malloc(&iheap_handle, size);
        if (ret) return ret;
        ret = multi_heap_malloc(&sheap_handle, size + 4);
        if (!ret) return NULL;
        return dram_alloc_to_iram_addr(ret, size + 4);
    }
}

/* Find the heap which belongs to ptr, or return NULL if it's
   not in any heap.

   (This confirms if ptr is inside the heap's region, doesn't confirm if 'ptr'
   is an allocated block or is some other random address inside the heap.)
*/
IRAM_ATTR static multi_heap_handle_t find_containing_heap(void* ptr)
{
    if (ptr >= (void*)&_iheap_start && ptr < (void*)&_iheap_end) return &iheap_handle;
    if (ptr >= (void*)&_dheap_start && ptr < (void*)&_dheap_end) return &dheap_handle;
    if (ptr >= (void*)&_sheap_start && ptr < (void*)&_sheap_end) return &sheap_handle;
    return NULL;
}

IRAM_ATTR void heap_caps_free(void *ptr)
{
    if (!ptr) return;
    if (ptr >= (void*)SOC_DIRAM_IRAM_LOW && ptr <= (void*)SOC_DIRAM_IRAM_HIGH)
    {
        //Memory allocated here is actually allocated in the DRAM alias region and
        //cannot be de-allocated as usual. dram_alloc_to_iram_addr stores a pointer to
        //the equivalent DRAM address, though; free that.
        ptr = ((void**)ptr)[-1];
    }
    multi_heap_handle_t heap = find_containing_heap(ptr);
    if (heap) multi_heap_free(heap, ptr);
}

IRAM_ATTR void *heap_caps_realloc(void *ptr, size_t size, int caps)
{
    if (!ptr) return heap_caps_malloc(size, caps);
    if (!size)
    {
        heap_caps_free(ptr);
        return NULL;
    }

    multi_heap_handle_t heap = find_containing_heap(ptr);
    bool compatible = false;
    if (heap == &iheap_handle && !(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) compatible = true;
    else if (heap == &dheap_handle && !(caps & (MALLOC_CAP_EXEC))) compatible = true;
    else if (heap == &sheap_handle && !(caps & (MALLOC_CAP_EXEC))) compatible = true;

    if (compatible)
    {
        // try to reallocate this memory within the same heap
        // (which will resize the block if it can)
        void *r = multi_heap_realloc(heap, ptr, size);
        if (r) return r;
    }

    // if we couldn't do that, try to see if we can reallocate
    // in a different heap with requested capabilities.
    void *new_p = heap_caps_malloc(size, caps);
    if (!new_p) return NULL;
    memcpy(new_p, ptr, multi_heap_get_allocated_size(heap, ptr));
    heap_caps_free(ptr);
    return new_p;
}

size_t heap_caps_get_free_size(uint32_t caps)
{
    size_t ret = 0;
    if (!(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) ret += multi_heap_free_size(&iheap_handle);
    if (!(caps & MALLOC_CAP_EXEC)) ret += multi_heap_free_size(&dheap_handle);
    if (!(caps & MALLOC_CAP_EXEC) || !(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) ret += multi_heap_free_size(&sheap_handle);
    return ret;
}

size_t heap_caps_get_minimum_free_size(uint32_t caps)
{
    size_t ret = 0;
    if (!(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) ret += multi_heap_minimum_free_size(&iheap_handle);
    if (!(caps & MALLOC_CAP_EXEC)) ret += multi_heap_minimum_free_size(&dheap_handle);
    if (!(caps & MALLOC_CAP_EXEC) || !(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) ret += multi_heap_minimum_free_size(&sheap_handle);
    return ret;
}

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps);
    return info.largest_free_block;
}

static void heap_caps_merge_info(multi_heap_info_t* info, multi_heap_handle_t heap)
{
    multi_heap_info_t hinfo;
    multi_heap_get_info(heap, &hinfo);
    info->total_free_bytes += hinfo.total_free_bytes;
    info->total_allocated_bytes += hinfo.total_allocated_bytes;
    info->largest_free_block = MAX(info->largest_free_block, hinfo.largest_free_block);
    info->minimum_free_bytes += hinfo.minimum_free_bytes;
    info->allocated_blocks += hinfo.allocated_blocks;
    info->free_blocks += hinfo.free_blocks;
    info->total_blocks += hinfo.total_blocks;
}

void heap_caps_get_info(multi_heap_info_t* info, uint32_t caps)
{
    bzero(info, sizeof(multi_heap_info_t));
    if (!(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) heap_caps_merge_info(info, &iheap_handle);
    if (!(caps & MALLOC_CAP_EXEC)) heap_caps_merge_info(info, &dheap_handle);
    if (!(caps & MALLOC_CAP_EXEC) || !(caps & (MALLOC_CAP_8BIT | MALLOC_CAP_DMA))) heap_caps_merge_info(info, &sheap_handle);
}

void heap_caps_print_heap_info(uint32_t caps)
{
    multi_heap_info_t info;
    printf("Heap summary for capabilities 0x%08X:\n", caps);
    heap_caps_get_info(&info, caps);
    printf("    free %d allocated %d min_free %d largest_free_block %d\n", info.total_free_bytes, info.total_allocated_bytes, info.minimum_free_bytes, info.largest_free_block);
}
