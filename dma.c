/*
 * dma.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "dma.h"


// DMA address mapping by DMA number index
const static uint32_t dma_addr[] =
{
    DMA0,
    DMA1,
    DMA2,
    DMA3,
    DMA4,
    DMA5,
    DMA6,
    DMA7,
    DMA8,
    DMA9,
    DMA10,
    DMA11,
    DMA12,
    DMA13,
    DMA14,
    DMA15,
};


uint32_t dmanum_to_phys(int dmanum)
{
    int array_size = sizeof(dma_addr) / sizeof(dma_addr[0]);

    if (dmanum >= array_size)
    {
        return 0;
    }

    return dma_addr[dmanum];
}


dma_page_t *dma_page_add(dma_page_t *head, void *addr)
{
    dma_page_t *page = malloc(sizeof(dma_page_t));

    if (!page)
    {
        return NULL;
    }

    page->next = head;
    page->prev = head->prev;

    head->prev->next = page;
    head->prev = page;

    page->addr = addr;

    return page;
}

void dma_page_remove(dma_page_t *page)
{
    page->prev->next = page->next;
    page->next->prev = page->prev;

    free(page);
}

void dma_page_remove_all(dma_page_t *head)
{
    while (head->next != head)
    {
        dma_page_remove(head->next);
    }
}

dma_page_t *dma_page_next(dma_page_t *head, dma_page_t *page)
{
    if (page->next != head)
    {
        return page->next;
    }

    return NULL;
}

void dma_page_init(dma_page_t *page)
{
    memset(page, 0, sizeof(*page));

    page->next = page;
    page->prev = page;
}

void *dma_alloc(dma_page_t *head, uint32_t size)
{
    uint32_t pages = (size / PAGE_SIZE) + 1;
    void *vaddr;
    int i;

    vaddr = mmap(NULL, pages * PAGE_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE |
                 MAP_LOCKED, -1, 0);
    if (vaddr == MAP_FAILED)
    {
        perror("dma_alloc() mmap() failed");
        return NULL;
    }

    for (i = 0; i < pages; i++)
    {
        if (!dma_page_add(head, &((uint8_t *)vaddr)[PAGE_SIZE * i]))
        {
            dma_page_remove_all(head);
            munmap(vaddr, pages * PAGE_SIZE);
            return NULL;
        }
    }

    return vaddr;
}

dma_cb_t *dma_desc_alloc(uint32_t descriptors)
{
    uint32_t pages = ((descriptors * sizeof(dma_cb_t)) / PAGE_SIZE);
    dma_cb_t *vaddr;

    if (pages > 1)
    {
        return NULL;
    }

    vaddr = mmap(NULL, pages * PAGE_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE |
                 MAP_LOCKED, -1, 0);
    if (vaddr == MAP_FAILED)
    {
        perror("dma_desc_alloc() mmap() failed");
        return NULL;
    }

    return vaddr;
}

void dma_page_free(void *buffer, const uint32_t size)
{
    uint32_t pages = (size / PAGE_SIZE) + 1;

    munmap(buffer, pages * PAGE_SIZE);
}


