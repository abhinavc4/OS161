
#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/copyinout.h"
#include "../include/filetable.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include "../include/uio.h"
#include "../include/vnode.h"
#include "../include/proc.h"
#include "../include/current.h"
#include "../include/synch.h"
#include "../include/current.h"
#include "../include/lib.h"
#include "../include/synch.h"
#include "../include/proc.h"
#include "../include/addrspace.h"
#include "../include/pagetable.h"
#include "../arch/mips/include/vm.h"
#include "../include/vm.h"
#include "../include/spl.h"
#include "../arch/mips/include/tlb.h"

/*
 */
void *sys___sbrk(__intptr_t change,int *errorCode)
{
    if(change%PAGE_SIZE != 0) {
        *errorCode = EINVAL;
        return ((void *)-1);
    }
    if(change == (-4096*1024*256)) {
        *errorCode = EINVAL;
        return ((void *)-1);
    }
    vaddr_t previousValue = curproc->p_addrspace->as_heaptop;
    vaddr_t valueToReturn = curproc->p_addrspace->as_heaptop + change;
    if(valueToReturn < curproc->p_addrspace->as_heapbase)
    {
        *errorCode = EINVAL;
        return ((void *)-1);
    }
    if(valueToReturn >= curproc->p_addrspace->as_stacktop)
    {
        *errorCode = ENOMEM;
        return ((void *)-1);
    }
    if(curproc->p_addrspace->as_heaptop > valueToReturn) {
        int spl = splhigh();

        for (int i=0; i<NUM_TLB; i++) {
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
        splx(spl);
        //vaddr_t temp = valueToReturn;
        struct pageTableEntry *pteIterator = curproc->pagetable;
        if(pteIterator->vaddr >= (valueToReturn&PAGE_FRAME) && pteIterator->vaddr <= (previousValue&PAGE_FRAME)) {
                free_kpages(PADDR_TO_KVADDR(pteIterator->paddr));
                curproc->pagetable = curproc->pagetable->next;
                kfree(pteIterator);
                pteIterator = NULL;
        }
        struct pageTableEntry *prev = pteIterator;
        pteIterator = pteIterator->next;
        while(pteIterator != NULL) {
                if(pteIterator->vaddr >= (valueToReturn&PAGE_FRAME) && pteIterator->vaddr <= (previousValue&PAGE_FRAME)) {
                        free_kpages(PADDR_TO_KVADDR(pteIterator->paddr));
                        if(pteIterator->next != NULL) {
                            prev->next = pteIterator->next;
                            struct pageTableEntry *temp = pteIterator;
                            pteIterator = pteIterator->next;
                            kfree(temp);
                            temp = NULL;
                        }
                        else {
                            prev->next = NULL;
                            curproc->tail = prev;
                            kfree(pteIterator);
                            pteIterator = NULL;
                        }
                    }
                    else {
                        prev = pteIterator;
                        pteIterator = pteIterator->next;
                    }
                }
        }

    curproc->p_addrspace->as_heaptop = valueToReturn;

    return (void *)previousValue;
}

