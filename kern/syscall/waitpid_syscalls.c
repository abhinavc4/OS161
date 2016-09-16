//
// Created by abhinav on 3/7/16.
//
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

pid_t wait___pid(pid_t pid, userptr_t returncode, int flags)
{

    int returnVal = 0;
    if(NULL == returncode)
    {
        return EFAULT;
    }
    int result = copyin((const_userptr_t)returncode,&returnVal, sizeof(int));
    if(result!=0)
    {
        return 0-result;
    }

    if(!(0==flags||WNOHANG == flags || WUNTRACED == flags || (WUNTRACED|WNOHANG)== flags))
    {
        return 0-EINVAL;
    }
    if(pid<PID_MIN || pid >OPEN_MAX)
    {
        return 0-ESRCH;
    }

    if(procTable[pid]==NULL)
    {
        return 0-ESRCH;
    }
    struct proc* childProc = procTable[pid];
    if(childProc->ppid != curproc->pid)
    {
        return 0-ECHILD;
    }
    lock_acquire(childProc->lk_exit);
    while(childProc->hasExited == false)
    {
        cv_wait(childProc->cv_exit,childProc->lk_exit);
    }
    lock_release(childProc->lk_exit);
    returnVal = childProc->exitCode;
    int value= copyout((const void*)&returnVal,returncode, sizeof(int));
    if(value != 0)
    {
        return 0-EINVAL;
    }
    proc_destroy(childProc);

    returnPID(pid);
    return pid;
}


pid_t wait__pid(pid_t pid, int* returncode, int flags)
{

    if(!(0==flags||WNOHANG == flags || WUNTRACED == flags || (WUNTRACED|WNOHANG)== flags))
    {
        return 0-EINVAL;
    }
    if(procTable[pid]==NULL)
    {
        return 0-ESRCH;
    }
    struct proc* childProc = procTable[pid];
    if(childProc->pid>PID_MIN && childProc->ppid != curproc->pid)
    {
        return 0-ECHILD;
    }
    lock_acquire(childProc->lk_exit);
    while(childProc->hasExited == false)
    {
        cv_wait(childProc->cv_exit,childProc->lk_exit);
    }
    lock_release(childProc->lk_exit);
    int value = copyout((const void*)&childProc->exitCode,(userptr_t )returncode, sizeof(int));
    if(value != 0)
    {
        return 0-EINVAL;
    }
    //*returncode = childProc->exitCode;
    proc_destroy(childProc);
    returnPID(pid);
    return pid;
}
