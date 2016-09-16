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
#include "opt-dumbvm.h"
#include "../../../include/addrspace.h"
#include "../../../include/pagetable.h"
#include "../../../include/vm.h"
#include "../../../include/kern/signal.h"
#include "../include/vm.h"
#include "../../../include/lib.h"
#include "../../../include/vfs.h"
#include "../../../include/vnode.h"
#include "../../../include/synch.h"
#include "../include/kern/stat.h"
#include "../../../include/kern/fcntl.h"
#include "../../../include/kern/errno.h"
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */

/*
 * Wrap ram_stealmem in a spinlock.
 */
#if OPT_DUMBVM
#else
extern vaddr_t firstfree;
static paddr_t coreMapPaddr;
struct coremap_entry *coremap = NULL;
struct coremap_entry *allocatablePage = NULL;
struct spinlock coremap_spinlock = SPINLOCK_INITIALIZER;
unsigned int numberOfPages= 0;
unsigned int numberOfPagesToMarkAsKernel = 0;
unsigned int coremapUsedBytes=0;
unsigned int allocatedStackPages=0;
unsigned int nextFreeIndex = 0;
void coremap_initialise(void)
{
    paddr_t lastPhyAddr = ram_getsize();
    numberOfPages = (lastPhyAddr + PAGE_SIZE -1) / PAGE_SIZE;
    size_t coreMapSize = numberOfPages * sizeof(struct coremap_entry);
    int cmPages = ( coreMapSize + PAGE_SIZE -1)/PAGE_SIZE;
    coreMapPaddr = ram_stealmem(cmPages);
    vaddr_t coreMapVaddr = PADDR_TO_KVADDR(coreMapPaddr);
    coremap = (struct coremap_entry *)coreMapVaddr;
    paddr_t startAddressOfCoreMap = ram_getfirstNew();
    numberOfPagesToMarkAsKernel =(startAddressOfCoreMap + PAGE_SIZE - 1)/PAGE_SIZE;
    coremapUsedBytes = numberOfPagesToMarkAsKernel*PAGE_SIZE;
    for(unsigned int i = 0 ; i<numberOfPagesToMarkAsKernel ; i++ )
    {
        coremap->chunk_size = 0;
        coremap->isAllocated = true;
        coremap->isFixed = true;
        coremap->isDirty = false;
        coremap->pid = 0;
        coremap++;
    }
    allocatablePage = coremap;
    for(unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages;i++)
    {
        coremap->chunk_size = 0;
        coremap->isAllocated = false;
        coremap->isFixed = false;
        coremap->isDirty = false;
        coremap->pid = 0;
        coremap++;
    }
}

void swapmap_initialise(void)
{
    kprintf("NUmber of pages to print as kernel %u",numberOfPages);
    char * fileName =(char*)"lhd0raw:";

    int vfsopenErr = vfs_open(fileName,O_RDWR,0, &lhdrawVnode);
    if(0 != vfsopenErr ){
        if(ENODEV == vfsopenErr)
        {
            isSwappingEnabled = false;
            return;
        }
    }
    else
    {
        isSwappingEnabled = true;
    }

    disklock = lock_create("disklock");

    struct stat stats;
    int staterr = 0;
    staterr = VOP_STAT(lhdrawVnode, &stats);
    if (0 != staterr) {
        panic("Cannot get disk size disk");
    }
    long diskSize = (long)stats.st_size;
    mapsize = diskSize/PAGE_SIZE;
    swapmap = (struct swap_entry *)kmalloc(mapsize * sizeof(struct swap_entry));
    for(int i =0; i <mapsize;i++) {
        swapmap[i].pid = 0;
        swapmap[i].virtual_addr = 1;
    }
}


void
vm_bootstrap(void)
{
    /* Do nothing. */
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */

paddr_t
getppages(unsigned long npages)
{
    paddr_t addr;
    addr = ram_smartmem(npages);
    return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
    //Find out if you are assigning single or multiple pages
    // If single find the page and return it , otherwise
    // a set of consecutive pages and mark them with the chunk
    // variable
    paddr_t pa;

    pa = getppages(npages);
    if (pa==0) {
        return 0;
    }
    return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr)
{
    /* nothing - leak the memory. */
    ram_removeMem(addr);
    (void)addr;
}

unsigned
int
coremap_used_bytes() {

    /* dumbvm doesn't track page allocations. Return 0 so that khu works. */
    //kprintf("returning %d\n",coremapUsedBytes);
    return coremapUsedBytes;
}

void
vm_tlbshootdown_all(void)
{
    //panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
    (void)ts;
    panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    //vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
    paddr_t paddr = 0;
    int i;
    uint32_t ehi, elo;
    struct addrspace *as;
    int spl;
    vaddr_t originalFaultAddress = faultaddress;
    faultaddress &= PAGE_FRAME;

    DEBUG(DB_VM, "Smartvm: fault: 0x%x\n", faultaddress);

    switch (faulttype) {
        case VM_FAULT_READONLY://pin
            /* We always create pages read-write, so we can't get this */
            panic("dumbvm: got VM_FAULT_READONLY\n");
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            break;
        default:
            kprintf("faultaddress %d ERR 1" , faultaddress);
            return EINVAL;
    }

    if (curproc == NULL) {
        /*
         * No process. This is probably a kernel fault early
         * in boot. Return EFAULT so as to panic instead of
         * getting into an infinite faulting loop.
         */
        kprintf("faultaddress %d ERR 2" , faultaddress);
        return EFAULT;
    }

    as = proc_getas();
    if (as == NULL) {
        /*
         * No address space set up. This is probably also a
         * kernel fault early in boot.
         */
        kprintf("faultaddress %d ERR 3" , faultaddress);
        return EFAULT;
    }

    /* Assert that the address space has been set up properly. */
    /*KASSERT(as->as_vbase1 != 0);
    KASSERT(as->as_pbase1 != 0);
    KASSERT(as->as_npages1 != 0);
    KASSERT(as->as_vbase2 != 0);
    KASSERT(as->as_pbase2 != 0);
    KASSERT(as->as_npages2 != 0);
    KASSERT(as->as_stackFixedBase != 0);
    KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
    KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
    KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
    KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);*/

    struct regionList *iterator = as->as_regions;

    while(iterator != NULL) {
        KASSERT(iterator->as_vbase != 0);
        KASSERT(iterator->as_npages != 0);
        iterator = iterator->next;
    }
    KASSERT((as->as_heaptop & PAGE_FRAME) == as->as_heaptop);
    KASSERT((as->as_stacktop & PAGE_FRAME) == as->as_stacktop);

    if(originalFaultAddress>as->as_heaptop && originalFaultAddress <as->as_stacktop)
    {
        kprintf("faultaddress %d ERR 4" , faultaddress);
        return EFAULT;
    }
    /*vbase1 = as->as_vbase1;
    vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
    vbase2 = as->as_vbase2;
    vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
    stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;*/

    /*if (faultaddress >= vbase1 && faultaddress < vtop1) {
        paddr = (faultaddress - vbase1) + as->as_pbase1;
    }
    else if (faultaddress >= vbase2 && faultaddress < vtop2) {
        paddr = (faultaddress - vbase2) + as->as_pbase2;
    }
    else if (faultaddress >= stackbase && faultaddress < stacktop) {
        paddr = (faultaddress - stackbase) + as->as_stackFixedBase;
    }
    else {
        return EFAULT;
    }*/
    iterator = as->as_regions;
    if(originalFaultAddress < as->as_heapbase) {
        for (; iterator != NULL; iterator = iterator->next) {
            if (faultaddress >= iterator->as_vbase &&
                faultaddress < (iterator->as_vbase + iterator->as_npages * PAGE_SIZE)) {
                int errortype = 0;
                paddr = findPTE(faultaddress, faulttype, &errortype);
                if((paddr & PAGE_FRAME)!= paddr) {
                    panic("3 page not aligned %u",paddr);
                }
                if (errortype != 0) {
                    int errorCode = 0;
                    paddr = createPTE(faultaddress,iterator,&errorCode);
                    if((paddr & PAGE_FRAME)!= paddr) {
                        panic("4 page not aligned %u",paddr);
                    }
                    if(paddr == 0) {
                        panic("Out of memory 4");
                    }
                    if(errorCode!=0)
                    {
                        kprintf("faultaddress %d ERR 5" , faultaddress);
                        return ENOMEM;
                    }
                }
                break;
            }
        }
        if(iterator == NULL && as->as_regions == NULL) {
            kprintf("faultaddress %d ERR 6" , faultaddress);
            return EFAULT;
        }
    }
    else if(originalFaultAddress >= as->as_heapbase && originalFaultAddress < as->as_stackFixedBase) {
        int errortype;
        struct regionList *tempRegion = (struct regionList*)kmalloc(sizeof(struct regionList));
        tempRegion->read_permission_bkp= true;
        tempRegion->write_permission_bkp = true;
        tempRegion->exec_permission_bkp= true;
        tempRegion->read_permission= true;
        tempRegion->write_permission = true;
        tempRegion->exec_permission= true;

        if (originalFaultAddress >= as->as_heapbase &&
                originalFaultAddress <= as->as_heaptop) {
            paddr = findPTE(faultaddress, faulttype, &errortype);
            if((paddr & PAGE_FRAME)!= paddr) {
                panic("5 page not aligned %u",paddr);
            }
            if (errortype != 0) {
                int errorCode = 0;
                paddr = createPTE(faultaddress, tempRegion,&errorCode);
                if((paddr & PAGE_FRAME)!= paddr) {
                    panic(" 6 page not aligned %u",paddr);
                }
                if(paddr == 0) {
                    panic("Out of memory 5");
                }
                if(errorCode!=0)
                {
                    kfree(tempRegion);
                    kprintf("faultaddress %d ERR 7" , faultaddress);
                    return ENOMEM;
                }
            }
        }
        else if (originalFaultAddress <= as->as_stackFixedBase &&
                originalFaultAddress > as->as_stacktop) {
            paddr = findPTE(faultaddress, faulttype, &errortype);
            if((paddr & PAGE_FRAME)!= paddr) {
                panic("1 page not aligned %u",paddr);
            }
            if (errortype != 0) {
                int errorCode = 0;
                if(allocatedStackPages < 1024) {
                    paddr = createPTE(faultaddress, tempRegion,&errorCode);
                    if((paddr & PAGE_FRAME)!= paddr) {
                        panic(" 7 page not aligned %u",paddr);
                    }
                    if(paddr == 0) {
                        panic("Out of memory 6");
                    }
                    if(errorCode!=0)
                    {
                        kprintf("faultaddress %d ERR 8" , faultaddress);
                        return ENOMEM;
                    }
                    allocatedStackPages +=1;
                }
                else {
                    kprintf("faultaddress %d ERR 9" , faultaddress);
                    return EFAULT;
                }
                if(errorCode!=0)
                {
                    kprintf("faultaddress %d ERR 10" , faultaddress);
                    kfree(tempRegion);
                    return EFAULT;
                }
                KASSERT((as->as_stacktop & PAGE_FRAME) == as->as_stacktop);
            }
        }
        else
        {
            kprintf("faultaddress %d ERR 11" , faultaddress);
            kfree(tempRegion);
            return EFAULT;
        }
        kfree(tempRegion);
    }
    else
    {
        kprintf("faultaddress %d ERR 12" , faultaddress);
        return EFAULT;
    }

    // make sure it's page-aligned
    if((paddr & PAGE_FRAME)!= paddr) {
        panic(" 8 page not aligned %u",paddr);
    }
    KASSERT(paddr!=0);

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();
    int result = tlb_probe(faultaddress,0);
    if(result)
    {

    }
    for (i=0; i<NUM_TLB; i++) {
        tlb_read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress;
        elo = paddr| TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);

        tlb_write(ehi, elo,i);
        int result = tlb_probe(faultaddress,0);
        if(result)
        {

        }
        splx(spl);
        return 0;
    }
    ehi = faultaddress;
    elo = paddr| TLBLO_DIRTY | TLBLO_VALID;
    tlb_random(ehi,elo);
    splx(spl);
    return 0;
}
#endif



