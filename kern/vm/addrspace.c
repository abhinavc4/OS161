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
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include "../include/spl.h"
#include "../include/proc.h"
#include "../arch/mips/include/tlb.h"
#include "../include/addrspace.h"
#include "../include/vm.h"
#include "../include/pagetable.h"
#include "../include/current.h"
#include "../arch/mips/include/vm.h"
#include "../include/lib.h"
#include "../include/uio.h"
#include "../include/vnode.h"
#include "../include/spinlock.h"

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
    struct addrspace *as = kmalloc(sizeof(struct addrspace));
    if (as==NULL) {
        return NULL;
    }
    as->as_regions = NULL;
    as->as_heapbase = 0;
    as->as_heaptop = 0;
    as->as_stackFixedBase = MIPS_KSEG0;
    as->as_stacktop = MIPS_KSEG0 - PAGE_SIZE * 1024;
    //MIPS_KSEG0 - PAGE_SIZE * 1024;

    return as;
}

void
as_activate(void)
{
    int i, spl;
    struct addrspace *as;

    as = proc_getas();
    if (as == NULL) {
        return;
    }

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

    for (i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

void
as_deactivate(void)
{
    int spl = splhigh();

    for (int i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */

void initRegionNode(struct regionList *node) {
    node->exec_permission = false;
    node->read_permission = false;
    node->write_permission = false;
    node->as_npages = 0;
    node->as_vbase = 0;
    node->as_pbase = 0;
    node->next = NULL;
}

int addToRegionList(struct addrspace *as, vaddr_t as_vbase, size_t as_npages,off_t offset, int permission)
{
    struct regionList *temp = (struct regionList*)kmalloc(sizeof(struct regionList));
    if(NULL == temp)
    {
        return ENOMEM;
    }
    as->regionCount++;
    initRegionNode(temp);
    temp->as_vbase = as_vbase;
    temp->as_npages = as_npages;
    temp->as_offset = offset;
    if(permission & 1) {
        temp->exec_permission = true;
    }
    if(permission & 2) {
        temp->write_permission = true;
    }
    if(permission & 4) {
        temp->read_permission = true;
    }
    if(as->as_regions == NULL)
    {
        as->as_regions = temp;
        as->rlTail = NULL;
    }
    else if(as->rlTail == NULL)
    {
        as->rlTail = temp;
        as->as_regions->next = as->rlTail;
    }
    else
    {
        as->rlTail->next = temp;
        as->rlTail = temp;
    }
    return 0;
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz, off_t offset,
                 int readable, int writeable, int executable)
{
    size_t npages;

    //dumbvm_can_sleep();

    /* Align the region. First, the base... */
    sz += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    /* ...and now the length. */
    sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = sz / PAGE_SIZE;

    int permission = readable + writeable + executable;

    if(0!=addToRegionList(as,vaddr,npages,offset,permission))
    {
        return ENOMEM;
    }
    return 0;
}

void
as_zero_region(paddr_t paddr, unsigned npages)
{
    bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
    /*KASSERT(as->as_pbase1 == 0);
    KASSERT(as->as_pbase2 == 0);
    KASSERT(as->as_stackFixedBase == 0);

    //dumbvm_can_sleep();

    as->as_pbase1 = getppages(as->as_npages1);
    if (as->as_pbase1 == 0) {
        return ENOMEM;
    }

    as->as_pbase2 = getppages(as->as_npages2);
    if (as->as_pbase2 == 0) {
        return ENOMEM;
    }

    as->as_stackFixedBase = getppages(DUMBVM_STACKPAGES);
    if (as->as_stackFixedBase == 0) {
        return ENOMEM;
    }

    as_zero_region(as->as_pbase1, as->as_npages1);
    as_zero_region(as->as_pbase2, as->as_npages2);
    as_zero_region(as->as_stackFixedBase, DUMBVM_STACKPAGES);*/
    struct regionList *iterator = as->as_regions;
    while (iterator != NULL) {
        iterator->read_permission_bkp = iterator->read_permission;
        iterator->write_permission_bkp = iterator->write_permission;
        iterator->exec_permission_bkp = iterator->exec_permission;
        iterator->read_permission = true;
        iterator->write_permission = true;
        iterator->exec_permission = true;
        iterator = iterator->next;
    }
    struct pageTableEntry *pteIterator = curproc->pagetable;
    while (pteIterator != NULL) {
        pteIterator->read_permission = true;
        pteIterator->write_permission = true;
        pteIterator->exec_permission = true;
        pteIterator = pteIterator->next;
    }
    return 0;
}

void
as_destroy(struct addrspace *as, struct proc *proc)
{
    //return;
    //dumbvm_can_sleep();
    /*struct regionList *ptr = as->as_regions;
    while (NULL!=ptr)
    {
        struct regionList *tempPtr = ptr;
        ptr = ptr->next;
        if(tempPtr!=NULL)
        {
            kfree(tempPtr);
            tempPtr = NULL;
        }
    }
    kfree(as);
    as = NULL;*/
    if(as) {

    }

    struct coremap_entry *iterator = allocatablePage;

    for (unsigned int i = numberOfPagesToMarkAsKernel;i<numberOfPages;i++) {
        if(!iterator[i].pid == proc->pid) {
            coremap->chunk_size = 0;
            coremap->isAllocated = false;
            coremap->isFixed = false;
            coremap->isDirty = false;
            coremap->pid = 0;
            coremapUsedBytes -= PAGE_SIZE;
        }
    }


    /*struct pageTableEntry *prev;
    while (pte != NULL) {
        prev = pte;
        pte = pte->next;
        if(NULL!=prev) //pin release disk
        {
            if(prev->state) {
                free_kpages(PADDR_TO_KVADDR(prev->paddr));
                //kfree(prev);
                //prev = NULL;
            }
            else {
                char *pageOfZeroes = (char *)kmalloc(PAGE_SIZE);
                memset(pageOfZeroes,0,PAGE_SIZE);
                struct iovec iov ;
                struct uio myuio;
                uio_kinit(&iov, &myuio, &pageOfZeroes, PAGE_SIZE, (off_t) prev->swap_index * PAGE_SIZE, UIO_WRITE);
                int result = VOP_WRITE(lhdrawVnode,&myuio);
                KASSERT(result == 0);
                if(isSwappingEnabled)
                {
                    swapmap[prev->swap_index].virtual_addr = 1;
                    swapmap[prev->swap_index].pid = 0;
                }

                //kfree(prev);
                //prev=NULL;
                //kfree(pageOfZeroes);
            }
        }
    }*/
}

int
as_complete_load(struct addrspace *as)
{
    vm_tlbshootdown_all();
    struct regionList *iterator = as->as_regions;
    for(;iterator != NULL;iterator = iterator->next) {
        iterator->read_permission = iterator->read_permission_bkp;
        iterator->write_permission = iterator->write_permission_bkp;
        iterator->exec_permission = iterator->exec_permission_bkp;
    }
    struct pageTableEntry *pageIterator = curproc->pagetable;
    for(;pageIterator != NULL;pageIterator = pageIterator->next) {
        pageIterator->read_permission = pageIterator->read_permission_bkp;
        pageIterator->write_permission = pageIterator->write_permission_bkp;
        pageIterator->exec_permission = pageIterator->exec_permission_bkp;
    }
    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
    KASSERT(as->as_stackFixedBase != 0);

    *stackptr = USERSTACK;
    return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret,struct proc *childProc)
{
    struct addrspace *new;

    new = as_create();
    if (new==NULL) {
        return ENOMEM;
    }
    new->as_heapbase = old->as_heapbase;
    new->as_heaptop = old->as_heaptop;
    new->as_stackFixedBase = old->as_stackFixedBase;
    new->as_stacktop = old->as_stacktop;

    //new->regionCount = old->regionCount;
    //new->rlTail = old->rlTail;
    //new->vn = old->vn;

    /*new->as_vbase1 = old->as_vbase1;
    new->as_npages1 = old->as_npages1;
    new->as_vbase2 = old->as_vbase2;
    new->as_npages2 = old->as_npages2;*/
    struct regionList* iterator = old->as_regions;
    for(int i = 0 ;i<old->regionCount;iterator= iterator->next,i++)
    {
        int permission = iterator->read_permission * 4 + iterator->write_permission * 2 + iterator->exec_permission;
        int result = addToRegionList(new,iterator->as_vbase,iterator->as_npages,iterator->as_offset,permission);
        if(result == ENOMEM)
        {
            return ENOMEM;
        }
    }

    struct pageTableEntry *pageIterator = curproc->pagetable;
    for(;pageIterator!=NULL;pageIterator = pageIterator->next)
    {
        struct pageTableEntry *ptEntry = (struct pageTableEntry*)kmalloc(sizeof(struct pageTableEntry));
        if(ptEntry == NULL)
        {
            return ENOMEM;
        }
        ptEntry->read_permission = pageIterator->read_permission;
        ptEntry->read_permission_bkp = pageIterator->read_permission_bkp;

        ptEntry->write_permission = pageIterator->write_permission;
        ptEntry->write_permission_bkp = pageIterator->write_permission_bkp;

        ptEntry->exec_permission = pageIterator->exec_permission;
        ptEntry->exec_permission_bkp = pageIterator->exec_permission_bkp;
        ptEntry->next = NULL;

        ptEntry->swap_index = pageIterator->swap_index;
        ptEntry->state = pageIterator->state;

        paddr_t paddr = page_alloc();
        if(paddr == 0)
        {
            return ENOMEM;
        }
        struct coremap_entry *iterator = allocatablePage;
        int indexIntoCM  = paddr/PAGE_SIZE;
        iterator[indexIntoCM].isAllocated = true;
        iterator[indexIntoCM].chunk_size = 1;
        iterator[indexIntoCM].isFixed = false;
        iterator[indexIntoCM].isDirty = true;
        iterator[indexIntoCM].pid = childProc->pid;
        as_zero_region(paddr,1);
        ptEntry->paddr = paddr&PAGE_FRAME;

        ptEntry->vaddr = pageIterator->vaddr;

        memmove((void *)PADDR_TO_KVADDR(ptEntry->paddr),
                (const void *)PADDR_TO_KVADDR(pageIterator->paddr),
                PAGE_SIZE);

        if(childProc->pagetable == NULL)
        {
            childProc->pagetable = ptEntry;
        }
        else if(childProc->tail == NULL)
        {
            childProc->tail = ptEntry;
            childProc->pagetable->next=childProc->tail;
        }
        else
        {
            childProc->tail->next = ptEntry;
            childProc->tail = ptEntry;
        }
    }
    /* (Mis)use as_prepare_load to allocate some physical memory. */
    /*if (as_prepare_load(new)) {
        as_destroy(new);
        return ENOMEM;
    }

    KASSERT(new->as_pbase1 != 0);
    KASSERT(new->as_pbase2 != 0);
    KASSERT(new->as_stackFixedBase != 0);

    memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
            (const void *)PADDR_TO_KVADDR(old->as_pbase1),
            old->as_npages1*PAGE_SIZE);

    memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
            (const void *)PADDR_TO_KVADDR(old->as_pbase2),
            old->as_npages2*PAGE_SIZE);

    memmove((void *)PADDR_TO_KVADDR(new->as_stackFixedBase),
            (const void *)PADDR_TO_KVADDR(old->as_stackFixedBase),
            DUMBVM_STACKPAGES*PAGE_SIZE);
    */
    *ret = new;
    return 0;
}


