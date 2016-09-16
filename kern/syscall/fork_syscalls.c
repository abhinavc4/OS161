
#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/copyinout.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include <addrspace.h>
#include "../include/proc.h"
#include <cpu.h>
#include "current.h"
#include "../include/filetable.h"
#include <machine/trapframe.h>
#include "../include/processtable.h"
#include "../include/synch.h"
#include "../include/addrspace.h"
#include "../include/lib.h"

/*
 * : open file.
 */
struct filehandle;
struct trapframe;
void newForkedThread(void *tf, unsigned long unused)
{
    //struct trapframe child_tf = (struct trapframe *)kmalloc(sizeof(struct trapframe ));
    struct trapframe child_tf;
    memcpy((void *)&child_tf,(const void *)tf, sizeof(struct trapframe));
    kfree(tf);
    as_activate();
    //trapFrame->tf_epc
    child_tf.tf_v0 = 0;
    child_tf.tf_a3 = 0;
    child_tf.tf_epc += 4;
    mips_usermode(&child_tf);
    if(unused)//hack
    {

    }
}

/*void copyFileTable(struct filehandle **fileTable)
{
    for (int i = 0; i < OPEN_MAX; i++) {
        if (fileTable[i] != NULL) {
            fileTable[i] = (struct filehandle *) kmalloc(sizeof(struct filehandle));
            fileTable[i] = curproc->p_fileTable[i];
            fileTable[i]->refcount++;
        }
    }
}*/


pid_t sys___fork(struct trapframe * tf)
{
    //Copy the address space
    struct proc *childProcess = proc_create_runprogram("handle");
    if(NULL == childProcess)
    {
        return 0-ENOMEM;
    }
    struct addrspace *procas = proc_getas();
    if(NULL == procas)
    {
        return 0-ENOMEM;
    }
    if(0!=as_copy(procas,&childProcess->p_addrspace,childProcess))
    {
        return 0-ENOMEM;
    }
    //childProcess->pid = getNextAvailablePID();
    //childProcess->pid = pidCounter++;
    //childProcess->ppid = curproc->pid;
    //addProc()
    //copyFileTable(childProcess->p_fileTable);

    for (int i = 0; i < OPEN_MAX; i++) {
        if (curproc->p_fileTable[i] ) {
            //childProcess->p_fileTable[i] = (struct filehandle *) kmalloc(sizeof(struct filehandle));
            childProcess->p_fileTable[i] = curproc->p_fileTable[i];
            lock_acquire(childProcess->p_fileTable[i]->filelock);
            childProcess->p_fileTable[i]->refcount++;
            lock_release(childProcess->p_fileTable[i]->filelock);
        }
    }
    struct trapframe* newTf = (struct trapframe*)kmalloc(sizeof(struct trapframe));
    if(newTf == NULL)
    {
        //proc_destroy(childProcess);
        return 0-ENOMEM;
    }
    memcpy((void*)newTf,(const void*)tf, sizeof(struct trapframe));
    int result = thread_fork(childProcess->p_name,
                         childProcess ,
                         newForkedThread /* thread function */,
                             newTf, 0);
    if(result != 0)
    {
        return 0-result;
    }
    return childProcess->pid;
}

