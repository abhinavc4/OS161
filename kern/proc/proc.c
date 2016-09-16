/*
 * Copyright (c) 2013
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

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <filetable.h>
#include "../include/filetable.h"
#include "../include/proc.h"
#include "../include/vfs.h"
#include "../include/kern/fcntl.h"
#include "../include/processtable.h"
#include "../include/synch.h"
#include "../include/addrspace.h"
#include "../include/pagetable.h"
#include "../include/current.h"
#include "../include/vm.h"

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

struct proc *procTable[OPEN_MAX];

unsigned int pidCounter;
struct vnode* stdinfileToOpen ;
struct vnode* lhdrawVnode ;
struct lock* disklock;
struct swap_entry *swapmap;
bool isSwappingEnabled;
int mapsize;


/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
        proc = NULL;
		return NULL;
	}

    proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

    initEntireFileTable(proc->p_fileTable);
    //initFileDescriptors();

    proc->cv_exit= cv_create("exit");
    if(NULL == proc->cv_exit)
    {
        if(proc!=NULL)
        {
            kfree(proc);
            proc = NULL;
        }
        if(proc->p_name!=NULL)
        {
            kfree(proc->p_name);
            proc->p_name = NULL;
        }
        return NULL;
    }
    proc->lk_exit= lock_create("exit_lock");
    if(NULL == proc->lk_exit)
    {
        if(proc!=NULL)
        {
            kfree(proc);
            proc= NULL;
        }
        if(proc->p_name!=NULL)
        {
            kfree(proc->p_name);
            proc->p_name = NULL;
        }
        if(proc->cv_exit!=NULL)
        {
            cv_destroy(proc->cv_exit);
            proc->cv_exit = NULL;
        }
        return NULL;
    }
    proc->cv_exit->cv_lock = proc->lk_exit;
    proc->exitCode=0;
    proc->hasExited = false;
	proc->pagetable = NULL;
	proc->tail = NULL;

    if(pidCounter == PID_MIN)
    {
        proc->ppid = 0;
        pidCounter++;
    }
    else
    {
        proc->ppid = curproc->pid;

    }
    proc->pid = getNextAvailablePID();
    if(proc->pid == -1)
    {
        if(proc!=NULL)
        {
            kfree(proc);
            proc= NULL;
        }
        if(proc->p_name!=NULL)
        {
            kfree(proc->p_name);
            proc->p_name = NULL;
        }
        if(proc->cv_exit!=NULL)
        {
            cv_destroy(proc->cv_exit);
            proc->cv_exit = NULL;
        }
        if(proc->lk_exit)
        {
            lock_destroy(proc->lk_exit);
            proc->lk_exit = NULL;
        }
        return NULL;
    }
    addProc(proc->pid , proc);
	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
        //kprintf("%d Used bytes",coremap_used_bytes());
		struct addrspace *as;
        struct pageTableEntry *pt;
        destroyFileTables(proc->p_fileTable);
		if (proc == curproc) {
			as = proc_setas(NULL);
            spinlock_acquire(&proc->p_lock);
            pt = proc->pagetable;
            proc->pagetable = NULL;
            spinlock_release(&proc->p_lock);
		}
		else {
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
            pt = proc->pagetable;
            proc->pagetable = NULL;
		}
        if(as || pt) {

        }
		as_destroy(as,proc);
        //as_deactivate();
        //destroyPageTable(pt);
        //kprintf("%d Used bytes",coremap_used_bytes());
    }


    if(proc->cv_exit!=NULL)
    {
        cv_destroy(proc->cv_exit);
        proc->cv_exit = NULL;
    }
    if(proc->lk_exit)
    {
        lock_destroy(proc->lk_exit);
        proc->lk_exit = NULL;
    }
	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
    pidCounter = PID_MIN;
    initProcessTable(procTable);
    int result = initPIDDescriptors();
    if(result == -1)
    {
        panic("proc_create for kproc failed\n");
    }
    kproc = proc_create("[kernel]");
    if (kproc == NULL) {
        panic("proc_create for kproc failed\n");
    }
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	newproc->p_addrspace = NULL;

    char * fileName =(char*)"con:";


    if(NULL == curproc->p_fileTable[0] )
    {

        vfs_open(fileName,O_RDWR,0, &stdinfileToOpen);
        struct filehandle * conRdHandle = (struct filehandle *)kmalloc(sizeof(struct filehandle));
        if(conRdHandle == NULL)
        {
            return NULL;
        }
        conRdHandle->vnode_obj = stdinfileToOpen;
        conRdHandle->uio_obj = NULL;
        conRdHandle->flags = O_RDONLY;
        conRdHandle->rwmode = 0 ;
        conRdHandle->refcount = 1;
        conRdHandle->filelock = lock_create("Read");
        newproc->p_fileTable[0] = conRdHandle;

        struct filehandle *conWrHandle = (struct filehandle *)kmalloc(sizeof(struct filehandle));
        if(conWrHandle == NULL)
        {
            return NULL;
        }
        conWrHandle->vnode_obj = stdinfileToOpen;
        conWrHandle->uio_obj = NULL;
        conWrHandle->flags = O_WRONLY;
        conWrHandle->rwmode = 1;
        conWrHandle->refcount = 1;
        conWrHandle->filelock = lock_create("write");
        newproc->p_fileTable[1] = conWrHandle;

        /* VFS fields */

        struct filehandle * conErrHandle  = (struct filehandle *)kmalloc(sizeof(struct filehandle));
        if(conErrHandle == NULL)
        {
            return NULL;
        }
        conErrHandle->vnode_obj = stdinfileToOpen;
        conErrHandle->uio_obj = NULL;
        conErrHandle->flags = O_WRONLY;
        conErrHandle->rwmode = 1;
        newproc->p_fileTable[2] = conErrHandle;
        conErrHandle->filelock = lock_create("err");
        conErrHandle->refcount = 1;

    }
    else{

        newproc->p_fileTable[1] = curproc->p_fileTable[1];
        newproc->p_fileTable[0] = curproc->p_fileTable[0];
        newproc->p_fileTable[2] = curproc->p_fileTable[2];
    }
    /*
     * Lock the current process to copy its current directory.
     * (We don't need to lock the new process, though, as we have
     * the only reference to it.)
     */
    spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}
