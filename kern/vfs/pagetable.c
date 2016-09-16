//
// Created by archana on 4/16/16.
//
#include "../include/pagetable.h"
#include "../include/current.h"
#include "../include/vm.h"
#include "../include/elf.h"
#include "../include/uio.h"
#include "../include/addrspace.h"
#include "../include/lib.h"
#include "../arch/mips/include/vm.h"
#include "../include/spl.h"
#include "../../build/install/include/kern/errno.h"
#include "../arch/mips/include/tlb.h"
#include <proc.h>



paddr_t findPTE(vaddr_t vaddr, int faulttype, int *errortype) {
    vaddr = vaddr&PAGE_FRAME;
    struct pageTableEntry *iterator = curproc->pagetable;
    *errortype = 0;
    for(;iterator!=NULL;iterator=iterator->next) {
        if(vaddr == iterator->vaddr) {
            if(!iterator->state) {
                if(isSwappingEnabled == false)
                {
                    panic("How can this happen");
                }
                iterator->paddr = swap_in(iterator->swap_index);
                iterator->state = true;
                return iterator->paddr;
            }
            //(void)faulttype;
            //return iterator->paddr;
            switch (faulttype) {
                case VM_FAULT_READ:
                    if(iterator->read_permission) {
                        return iterator->paddr;
                    } else {
                        *errortype = EFAULT;
                        return 0;
                    }
                    break;
                case VM_FAULT_WRITE:
                    if(iterator->write_permission) {
                        return iterator->paddr;
                    }
                    else {
                        *errortype = EFAULT;
                        return 0;
                    }
                    break;
                default:
                    *errortype = EINVAL;
                    return 0;
            }
        }
    }
    *errortype = ENOSYS;
    return 0;
}

paddr_t createPTE(vaddr_t faultaddr, struct regionList *regionNode,int *errorCode) {
    //uint32_t ehi, elo;
    *errorCode = 0;
    struct pageTableEntry *newEntry = (struct pageTableEntry*)kmalloc(sizeof(struct pageTableEntry));
    if(newEntry == NULL)
    {
        *errorCode = -5;
        return 0;
    }
    //newEntry->valid = true;
    newEntry->read_permission =  regionNode->read_permission;
    newEntry->write_permission = regionNode->write_permission;
    newEntry->exec_permission =  regionNode->exec_permission;
    newEntry->read_permission_bkp = regionNode->read_permission_bkp;
    newEntry->write_permission_bkp = regionNode->write_permission_bkp;
    newEntry->exec_permission_bkp = regionNode->exec_permission_bkp;
    newEntry->next = NULL;
    newEntry->state = true;
    //newEntry->referenced = true;
    newEntry->vaddr = faultaddr&PAGE_FRAME;

    paddr_t paddr = page_alloc();
    //kprintf("Assigning paddr = %u",paddr);
    KASSERT(paddr!=0);
    as_zero_region(paddr,1);
    newEntry->paddr = paddr&PAGE_FRAME;

    int addEntryErr = addPTE(newEntry);
    if(addEntryErr < 0) {
        *errorCode = -5;
        return 0;
    }
    else {
        return newEntry->paddr;
    }
}

int addPTE(struct pageTableEntry *pte) {

    if(NULL == pte)
    {
        return -1;
    }
    pte->next = NULL;
    if(curproc->pagetable == NULL)
    {
        curproc->pagetable = pte;
        curproc->tail = NULL;
    }
    else if(curproc->tail == NULL)
    {
        curproc->tail = pte;
        curproc->pagetable->next=curproc->tail;
    }
    else
    {
        curproc->tail->next = pte;
        curproc->tail = pte;
    }
    return 0;
}

/*void destroyPageTable(struct pageTableEntry *pte)
{
    struct pageTableEntry *prev;
    while (pte != NULL) {
        prev = pte;
        pte = pte->next;
        if(NULL!=prev)
        {
            free_kpages(PADDR_TO_KVADDR(prev->paddr));
            kfree(prev);
            prev = NULL;
        }
    }
}*/