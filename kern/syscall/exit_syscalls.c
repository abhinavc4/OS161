
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
#include "synch.h"
#include "../include/kern/wait.h"

/*
 * : open file.
 */


void sys___exit(int code)
{
    struct proc** procBack= procTable;
    struct proc* temp = curproc;
    if(procBack&&temp)
    {

    }
    if(procTable[curproc->ppid] != NULL)
    {
        lock_acquire(curproc->lk_exit);
        curproc->hasExited = true;
        if(code != 0) {
            curproc->exitCode = _MKWAIT_SIG(code);
        }
        else {
            curproc->exitCode = _MKWAIT_EXIT(code);
        }
        cv_signal(curproc->cv_exit,curproc->lk_exit);
        lock_release(curproc->lk_exit);
        thread_exit();
        //proc_destroy(curproc);
    }
}

