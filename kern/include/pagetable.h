//
// Created by archana on 4/16/16.
//

#include "../arch/mips/include/types.h"
#include "types.h"
#include "addrspace.h"

#ifndef OS161_PAGETABLE_H_H
#define OS161_PAGETABLE_H_H

#endif //OS161_PAGETABLE_H_H

struct pageTableEntry {
    vaddr_t vaddr;
    paddr_t paddr;
    bool read_permission:1;
    bool write_permission:1;
    bool exec_permission:1;
    bool read_permission_bkp:1;
    bool write_permission_bkp:1;
    bool exec_permission_bkp:1;
    bool state:1;
    __u16 swap_index;
    //bool valid:1;
    //bool referenced:1;
    struct pageTableEntry *next;
};

paddr_t findPTE(vaddr_t vaddr, int faulttype, int *errortype);

paddr_t createPTE(vaddr_t faultaddr,
                  struct regionList *regionNode,int *errorCode);

int addPTE(struct pageTableEntry *pte);
//void destroyPageTable(struct pageTableEntry *pte);

