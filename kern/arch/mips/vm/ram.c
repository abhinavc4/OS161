/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <lib.h>
#include <vm.h>
#include <mainbus.h>
#include "../include/vm.h"
#include "../../../include/mainbus.h"
#include "../include/types.h"
#include "../../../include/spinlock.h"
#include "../../../include/vm.h"
#include "../../../include/lib.h"
#include "opt-dumbvm.h"
#include "../../../include/current.h"
#include "../include/proc.h"
#include "../include/addrspace.h"
#include "../../../include/addrspace.h"
#include "../../../include/pagetable.h"
#include "../../../include/kern/iovec.h"
#include "../../../include/uio.h"
#include "../../../include/synch.h"
#include "../../../include/vnode.h"
#include "../../../include/spl.h"
#include "../include/tlb.h"
#include "../../../include/limits.h"

vaddr_t firstfree;   /* first free virtual address; set by start.S */

static paddr_t firstpaddr;  /* address of first free physical page */
static paddr_t lastpaddr;   /* one past end of last free physical page */

/*
 * Called very early in system boot to figure out how much physical
 * RAM is available.
 */
void
ram_bootstrap(void)
{
    size_t ramsize;

    /* Get size of RAM. */
    ramsize = mainbus_ramsize();

    /*
     * This is the same as the last physical address, as long as
     * we have less than 512 megabytes of memory. If we had more,
     * we wouldn't be able to access it all through kseg0 and
     * everything would get a lot more complicated. This is not a
     * case we are going to worry about.
     */
    if (ramsize > 512*1024*1024) {
        ramsize = 512*1024*1024;
    }

    lastpaddr = ramsize;

    /*
     * Get first free virtual address from where start.S saved it.
     * Convert to physical address.
     */
    firstpaddr = firstfree - MIPS_KSEG0;

    kprintf("%uk physical memory available\n",
            (lastpaddr-firstpaddr)/1024);
}

/*
 * This function is for allocating physical memory prior to VM
 * initialization.
 *
 * The pages it hands back will not be reported to the VM system when
 * the VM system calls ram_getsize(). If it's desired to free up these
 * pages later on after bootup is complete, some mechanism for adding
 * them to the VM system's page management must be implemented.
 * Alternatively, one can do enough VM initialization early so that
 * this function is never needed.
 *
 * Note: while the error return value of 0 is a legal physical address,
 * it's not a legal *allocatable* physical address, because it's the
 * page with the exception handlers on it.
 *
 * This function should not be called once the VM system is initialized,
 * so it is not synchronized.
 */
paddr_t
ram_stealmem(unsigned long npages)
{
    size_t size;
    paddr_t paddr;

    size = npages * PAGE_SIZE;

    if (firstpaddr + size > lastpaddr) {
        return 0;
    }

    paddr = firstpaddr;
    firstpaddr += size;

    return paddr;
}
#if OPT_DUMBVM
#else
void ram_removeMem(vaddr_t addr)
{
    paddr_t paddr;
    paddr = addr - MIPS_KSEG0;
    unsigned int coremap_index = paddr /PAGE_SIZE;
    bool flag = false;
    as_zero_region(paddr,1);

    if(coremap_index < numberOfPagesToMarkAsKernel) {
        kprintf("Illegal memory access! Will result in termination.");
        return;
    }
    if(!spinlock_do_i_hold(&coremap_spinlock)) {
        spinlock_acquire(&coremap_spinlock);
        flag = true;
    }
    else {
        kprintf("Condition 1");
    }
    struct coremap_entry *coreMapPointer = &allocatablePage[coremap_index];
    if(coreMapPointer->isAllocated) {
        __u16 i = coreMapPointer->chunk_size;
        coremapUsedBytes-=PAGE_SIZE*i;
        //curproc->p_addrspace->as_heaptop -= PAGE_SIZE*i;
        while (i>0) {
            //kprintf("\nTAKEN BACK\n");
            coreMapPointer->isAllocated = false;
            coreMapPointer->chunk_size = 0;
            coreMapPointer->isFixed = false;
            coreMapPointer->isDirty = false;
            coreMapPointer->pid = 0;
            coreMapPointer++;
            i--;
        }
    }
    if(flag==true) {
        spinlock_release(&coremap_spinlock);
    }
}
paddr_t allocate_one_page(void) {
    paddr_t paddr = 0;
    bool flag = false;
    //kprintf("\n####allocated####\n");
    struct coremap_entry *iterator = allocatablePage;

    if(!spinlock_do_i_hold(&coremap_spinlock)) {
        spinlock_acquire(&coremap_spinlock);
        flag = true;
    }
    else {
        kprintf("Condition 2");
    }
    for (unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages;i++) {
        if(!iterator[i].isAllocated) {
            iterator[i].isAllocated = true;
            iterator[i].chunk_size = 1;
            iterator[i].isFixed = true;
            iterator[i].isDirty = true;
            paddr = i*PAGE_SIZE;
            coremapUsedBytes += PAGE_SIZE;
            if(i == 118) {
                kprintf(" alloc one page used 118");
            }
            break;
        }
    }
    if(paddr == 0) {
        paddr = swap_out();
        //KASSERT(paddr!=0);
        if(paddr!=0)
        {
            int indexIntoCM  = paddr/PAGE_SIZE;
            iterator[indexIntoCM].isAllocated = true;
            iterator[indexIntoCM].chunk_size = 1;
            iterator[indexIntoCM].isFixed = true;
            iterator[indexIntoCM].isDirty = true;
            if(indexIntoCM == 118) {
                kprintf(" alloc one page swap out used 118");
            }
            //iterator[indexIntoCM].pid = curproc->pid;
        }
    }
    if(paddr!=0)
    {
        as_zero_region(paddr,1);
    }
    if(flag==true) {
        spinlock_release(&coremap_spinlock);
    }
    //kprintf("RETURING paddr from allocate_one_page %u",paddr);
    KASSERT((paddr & PAGE_FRAME) == paddr);
    return paddr;
}

paddr_t swap_in(__u16 swap_index) {
    if(isSwappingEnabled == false)
    {
        return 0;
    }
    //kprintf("ENTERING SWAP_IN\n");
    paddr_t paddr = 0;
    paddr = page_alloc();
    struct iovec iov;
    struct uio myuio;
    vaddr_t vaddr = PADDR_TO_KVADDR(paddr);
    uio_kinit(&iov,&myuio,(void*)vaddr,PAGE_SIZE,(off_t)swap_index*PAGE_SIZE,UIO_READ);
    //spinlock_release(&coremap_spinlock);
    int result = VOP_READ(lhdrawVnode,&myuio);
    //spinlock_acquire(&coremap_spinlock);
    KASSERT(result==0);
    KASSERT((paddr & PAGE_FRAME) == paddr);
    //kprintf("RETURING paddr from swap_in %u\n",paddr);
    return paddr;
}


paddr_t swap_out(void) {

    if(isSwappingEnabled == false)
    {
        return 0;
    }
    //kprintf("ENTERING SWAP_OUT\n");
    paddr_t paddr = 0;
    struct coremap_entry *iterator = allocatablePage;

    //lock_acquire(disklock);

    unsigned int i = 0;
    bool flagTrue = false;
    unsigned int randomNumber = 0;
    while (flagTrue == false)
    {
        randomNumber = random()%numberOfPages;
        if(randomNumber > numberOfPagesToMarkAsKernel && randomNumber < numberOfPages) {
            i = randomNumber;
            if(iterator[i].isAllocated == true
               && iterator[i].isFixed == false
               && iterator[i].chunk_size == 1)
                    //&&iterator[i].pid ==  curproc->pid)
            {
                flagTrue = true;
            }
        }
    }
    if(i <= numberOfPagesToMarkAsKernel || i >= numberOfPages)
    {
        panic("I is not correct");
    }
    //for (unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages;i++)
    if(iterator[i].isDirty == false)
    {
        paddr = i*PAGE_SIZE;
        //coremapUsedBytes += PAGE_SIZE;
    }
    else
    {
        //pin page table lock
        vaddr_t vaddrToEvict = 0;
        pid_t pidOfProcessToEvict = iterator[i].pid;
        struct pageTableEntry *ptePrev = NULL;
        struct pageTableEntry *pte = procTable[pidOfProcessToEvict]->pagetable;
        KASSERT(pte!=NULL);
        for( ;pte!=NULL; pte=pte->next ) {
            if(pte->paddr == i*PAGE_SIZE) {
                vaddrToEvict = pte->vaddr;
                break;
            }
            ptePrev = pte;
            if(ptePrev)
            {

            }
        }
        /*if(pte==NULL) {
            for(int k = 3; k < PID_MAX; k++) {
                if(procTable[k] != NULL) {
                    pte = procTable[k]->pagetable;
                    //if(pte == NULL) {
                    //    continue;
                    //}
                    for( ;pte!=NULL; pte=pte->next ) {
                        //kprintf("process id %d",k);
                        if(pte->paddr == i*PAGE_SIZE) {
                            vaddrToEvict = pte->vaddr;
                            pidOfProcessToEvict = k;
                            break;
                        }
                    }
                    if(vaddrToEvict != 0) {
                        break;
                    }
                }
                else{
                    kprintf("pid %d",k);
                    break;
                }
            }
        }

        if(pte==NULL) {
            kprintf("found leaked page");
            paddr = i*PAGE_SIZE;
            return paddr;
        }*/
        KASSERT(pte!=NULL);
        int spl = splhigh();

        int result = tlb_probe(vaddrToEvict,0);
        if(result > 0) {
            tlb_write(TLBHI_INVALID(result),TLBLO_INVALID(),result);
        }
        splx(spl);

        for(int j=0; j < mapsize; j++) {
            if(swapmap[j].pid == pidOfProcessToEvict && swapmap[j].virtual_addr == vaddrToEvict) {
                //Entry already exists in the swapmap
                struct iovec iov ;
                struct uio myuio;
                KASSERT(paddr==0);
                paddr = i*PAGE_SIZE;
                vaddr_t vaddrToWriteOut = PADDR_TO_KVADDR(paddr);
                uio_kinit(&iov, &myuio, (void *)vaddrToWriteOut, PAGE_SIZE, (off_t) j * PAGE_SIZE, UIO_WRITE);
                spinlock_release(&coremap_spinlock);
                int result = VOP_WRITE(lhdrawVnode,&myuio);
                spinlock_acquire(&coremap_spinlock);
                KASSERT(0==result);
                pte->state = false;
                pte->paddr = 1;
                pte->swap_index = j;
                break;
            }
        }
        if(paddr == 0) {
            for(int j=0; j < mapsize; j++) {
                if(swapmap[j].virtual_addr == 1) { // pin on release set vrtual address to 1
                    struct iovec iov ;
                    struct uio myuio;
                    KASSERT(paddr==0);
                    paddr = i*PAGE_SIZE;
                    vaddr_t vaddrToWriteOut = PADDR_TO_KVADDR(paddr);
                    uio_kinit(&iov, &myuio, (void*)vaddrToWriteOut, PAGE_SIZE, (off_t) j * PAGE_SIZE, UIO_WRITE);
                    spinlock_release(&coremap_spinlock);
                    int result = VOP_WRITE(lhdrawVnode,&myuio);
                    spinlock_acquire(&coremap_spinlock);
                    KASSERT(0==result);
                    pte->state = false;
                    pte->paddr = 1;
                    pte->swap_index = j;
                    swapmap[j].pid = pidOfProcessToEvict;
                    swapmap[j].virtual_addr = vaddrToEvict;
                    break;
                }
            }
        }
    }


    KASSERT(paddr!=0);
    KASSERT((paddr & PAGE_FRAME) == paddr);
    //lock_release(disklock);
    //kprintf("RETURING paddr from swap_out %u\n",paddr);
    return paddr;
}

paddr_t page_alloc(void) {
    paddr_t paddr = 0;
    //kprintf("\n####allocated####\n");
    struct coremap_entry *iterator = allocatablePage;
    bool flag = true;
    if(!spinlock_do_i_hold(&coremap_spinlock)) {
        spinlock_acquire(&coremap_spinlock);
        flag = true;
    } else {
        kprintf("Condition 3");
    }

    for (unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages;i++) {
        if(!iterator[i].isAllocated) {
            iterator[i].isAllocated = true;
            iterator[i].chunk_size = 1;
            iterator[i].isDirty = true;
            iterator[i].isFixed = false;
            iterator[i].pid = curproc->pid;
            paddr = i*PAGE_SIZE;
            coremapUsedBytes += PAGE_SIZE;
            break;
        }
    }
    if(paddr == 0) {
        paddr = swap_out();
        //KASSERT(paddr!=0);
        if(paddr!=0)
        {
            KASSERT((paddr & PAGE_FRAME) == paddr);
            int indexIntoCM  = paddr/PAGE_SIZE;
            iterator[indexIntoCM].isAllocated = true;
            iterator[indexIntoCM].chunk_size = 1;
            iterator[indexIntoCM].isFixed = false;
            iterator[indexIntoCM].isDirty = true;
            iterator[indexIntoCM].pid = curproc->pid;
            //kprintf("Swapped out updating pid %u paddr is %u",curproc->pid,paddr);
        }
    }
    if(paddr!=0)
    {
        as_zero_region(paddr,1);
    }
    if(flag == true) {
        spinlock_release(&coremap_spinlock);
    }
    KASSERT(paddr!=0);
    KASSERT((paddr & PAGE_FRAME) == paddr);
    //kprintf("RETURING paddr from page_alloc %u\n",paddr);
    return paddr;
}

paddr_t allocate_n_pages(unsigned long npages) { // pin handle swap otu multiple pages
    //kprintf("allocating npages %lu \n",npages);
    paddr_t paddr = 0;
    struct coremap_entry *iterator = allocatablePage;
    bool flag = true;
    struct coremap_entry *sizeIterator = NULL;
    bool chunkNotFree = false;
    if(!spinlock_do_i_hold(&coremap_spinlock)) {
        spinlock_acquire(&coremap_spinlock);
        flag = true;
    }
    for(unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages - npages;i++) {
        if(!iterator[i].isAllocated) {
            sizeIterator = &iterator[i];
            for(unsigned int j = 0 ;j<npages;j++)
            {
                if(sizeIterator[j].isAllocated) {
                    chunkNotFree = true;
                    break;
                }
            }
            if(chunkNotFree == false) {
                paddr = i*PAGE_SIZE;
                sizeIterator = &iterator[i];
                sizeIterator->chunk_size = npages;
                sizeIterator->isAllocated = true;
                sizeIterator->isFixed = true;
                sizeIterator->isDirty = true;
                //sizeIterator->pid = curproc->pid;
                //sizeIterator++;
                for(unsigned int j = 1 ;j<npages;j++)
                {
                    sizeIterator[j].isAllocated = true;
                    sizeIterator[j].chunk_size = 1;
                    sizeIterator[j].isFixed = true;
                    sizeIterator[j].isDirty = true;
                    //sizeIterator[j].pid = curproc->pid;
                }
                coremapUsedBytes += PAGE_SIZE*npages;
                break;
            }
            else
            {
                chunkNotFree = false;
            }
        }
    }
    if(paddr == 0) {
        paddr = swap_out_npages(npages);
    }
    if(paddr!=0)
    {
        as_zero_region(paddr,npages);
    }
    KASSERT(paddr!=0);
    if(flag==true) {
        spinlock_release(&coremap_spinlock);
    }
    //kprintf("RETURING paddr from allocate_n_pages %u",paddr);
    return paddr;
}

int swap_out_index(unsigned int index) {
    if(isSwappingEnabled == false)
    {
        return 0;
    }
    struct coremap_entry *iterator = allocatablePage;
    struct coremap_entry *sizeIterator = &iterator[index];
    //lock_acquire(disklock);
    paddr_t paddr = 0;
    vaddr_t vaddrToEvict = 0;
    pid_t pidOfProcessToEvict = sizeIterator->pid;
    struct pageTableEntry *pte = procTable[pidOfProcessToEvict]->pagetable;
    KASSERT(pte!=NULL);
    for( ;pte!=NULL; pte=pte->next ) {
        if(pte->paddr == index*PAGE_SIZE) { //pin i+j in loop
            vaddrToEvict = pte->vaddr;
            break;
        }
    }
    /*if(pte==NULL) {
        for(int k = 3; k < PID_MAX; k++) {
            if(procTable[k] != NULL) {
                pte = procTable[k]->pagetable;
                //if(pte == NULL) {
                //    continue;
                //}
                for( ;pte!=NULL; pte=pte->next ) {
                    //kprintf("process id %d",k);
                    if(pte->paddr == index*PAGE_SIZE) {
                        vaddrToEvict = pte->vaddr;
                        pidOfProcessToEvict = k;
                        break;
                    }
                }
                if(vaddrToEvict != 0) {
                    break;
                }
            }
            else{
                kprintf("pid %d",k);
                break;
            }
        }
    }

    if(pte==NULL) {
        kprintf("found leaked page");
        paddr = index*PAGE_SIZE;
        return paddr;
    }*/
    if(pte==NULL) {
        panic("invalid pte in multiple page allocation");
    }

    KASSERT(pte!=NULL);
    int spl = splhigh();

    int result = tlb_probe(vaddrToEvict,0);
    if(result > 0) {
        tlb_write(TLBHI_INVALID(result),TLBLO_INVALID(),result);
    }
    splx(spl);

    for(int j=0; j < mapsize; j++) {
        if(swapmap[j].pid == pidOfProcessToEvict && swapmap[j].virtual_addr == vaddrToEvict) {
            struct iovec iov ;
            struct uio myuio;
            KASSERT(paddr==0);
            paddr = index*PAGE_SIZE;
            vaddr_t vaddrToWriteOut = PADDR_TO_KVADDR(paddr);
            uio_kinit(&iov, &myuio, (void *)vaddrToWriteOut, PAGE_SIZE, (off_t) j * PAGE_SIZE, UIO_WRITE);
            spinlock_release(&coremap_spinlock);
            int result = VOP_WRITE(lhdrawVnode,&myuio);
            spinlock_acquire(&coremap_spinlock);
            KASSERT(0==result);
            pte->state = false;
            pte->paddr =1;
            pte->swap_index = j;
            break;
        }
    }
    if(paddr == 0) {
        for(int j=0; j < mapsize; j++) {
            if(swapmap[j].virtual_addr == 1) { // pin on release set vrtual address to 1
                struct iovec iov ;
                struct uio myuio;
                KASSERT(paddr==0);
                paddr = index*PAGE_SIZE;
                vaddr_t vaddrToWriteOut = PADDR_TO_KVADDR(paddr);
                uio_kinit(&iov, &myuio, (void*)vaddrToWriteOut, PAGE_SIZE, (off_t) j * PAGE_SIZE, UIO_WRITE);
                spinlock_release(&coremap_spinlock);
                int result = VOP_WRITE(lhdrawVnode,&myuio);
                spinlock_acquire(&coremap_spinlock);
                KASSERT(0==result);
                pte->state = false;
                pte->paddr = 1;
                pte->swap_index = j;
                swapmap[j].pid = pidOfProcessToEvict;
                swapmap[j].virtual_addr = vaddrToEvict;
                break;
            }
        }
    }
    //lock_release(disklock);
    if(paddr==0) {
        return 0;
    }
    else{
        return 1;
    }
}

paddr_t swap_out_npages(unsigned long npages) {
    if(isSwappingEnabled == false)
    {
        return 0;
    }
    kprintf("Trying to swap out n pages\n");
    paddr_t paddr = 0;
    struct coremap_entry *iterator = allocatablePage;
    struct coremap_entry *sizeIterator = NULL;
    bool chunkNotFree = false;

    for(unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages - npages;i++) {
        if(iterator[i].isFixed == false) {
            sizeIterator = &iterator[i];
            for(unsigned int j = 0 ;j<npages;j++)
            {
                if(sizeIterator[j].isFixed) {
                    chunkNotFree = true;
                    break;
                }
            }
            if(chunkNotFree == false) {
                paddr = i*PAGE_SIZE;
                sizeIterator = &iterator[i];
                if(sizeIterator->isAllocated) {//pin change later
                    //swapout
                    int result = swap_out_index(i);
                    if(result==0) {
                        panic("Hello this is happening");
                        return 0;
                    }
                }
                else{
                    coremapUsedBytes += PAGE_SIZE;
                }
                sizeIterator->chunk_size = npages;
                sizeIterator->isAllocated = true;
                sizeIterator->isFixed = true;
                sizeIterator->isDirty = true;
                sizeIterator->pid = curproc->pid;
                for(unsigned int j = 1 ;j<npages;j++)
                {
                    if(sizeIterator[j].isAllocated ) {//pin change later
                        //swapout
                        int result = swap_out_index(i+j);
                        if(result==0) {
                            panic("Hello this is happening");
                            return 0;
                        }
                    }
                    else
                    {
                        panic("Hello this is happening");
                    }
                    sizeIterator[j].isAllocated = true;
                    sizeIterator[j].chunk_size = 1;
                    sizeIterator[j].isFixed = true;
                    sizeIterator[j].isDirty = true;
                    sizeIterator[j].pid = curproc->pid;
                }
                break;
            }
            else {
                chunkNotFree = false;
            }
        }
    }
    kprintf("RETURING swap out n pages  from swap out n pages %u",paddr);
    return paddr;
}

paddr_t
ram_smartmem(unsigned long npages)
{
    size_t size;
    paddr_t paddr;

    size = npages * PAGE_SIZE;

    //check for if memory requested for is greater than total
    // allocatable memory
    if (firstpaddr + size > lastpaddr) {
        return 0;
    }

    //if we need to allocate only 1 page
    if(npages == 1) {
        paddr = allocate_one_page();
    }
        //if we need to allocate more than 1 page
    else if(npages > 1) {
        paddr = allocate_n_pages(npages);
    }
        //invalid number of pages
    else {
        paddr = 0;
    }

    //dumbvm code
    //paddr = firstpaddr;
    //firstpaddr += size;

    return paddr;
}
#endif


/*
 * This function is intended to be called by the VM system when it
 * initializes in order to find out what memory it has available to
 * manage. Physical memory begins at physical address 0 and ends with
 * the address returned by this function. We assume that physical
 * memory is contiguous. This is not universally true, but is true on
 * the MIPS platforms we intend to run on.
 *
 * lastpaddr is constant once set by ram_bootstrap(), so this function
 * need not be synchronized.
 *
 * It is recommended, however, that this function be used only to
 * initialize the VM system, after which the VM system should take
 * charge of knowing what memory exists.
 */
paddr_t
ram_getsize(void)
{
    return lastpaddr;
}

/*
 * This function is intended to be called by the VM system when it
 * initializes in order to find out what memory it has available to
 * manage.
 *
 * It can only be called once, and once called ram_stealmem() will
 * no longer work, as that would invalidate the result it returned
 * and lead to multiple things using the same memory.
 *
 * This function should not be called once the VM system is initialized,
 * so it is not synchronized.
 */
paddr_t
ram_getfirstfree(void)
{
    paddr_t ret;

    ret = firstpaddr;
    firstpaddr = lastpaddr = 0;
    return ret;
}

paddr_t
ram_getfirstNew(void)
{
    paddr_t ret;

    ret = firstpaddr;
    //firstpaddr = lastpaddr = 0;
    return ret;
}

