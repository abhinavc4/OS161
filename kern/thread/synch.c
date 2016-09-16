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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include "../include/lib.h"
#include "../include/types.h"
#include "../include/synch.h"
#include "../include/wchan.h"
#include "../include/spinlock.h"

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

    lock->lk_wchan = wchan_create(lock->lk_name);
    if(lock->lk_wchan == NULL) {
        kfree(lock->lk_name);
        kfree(lock);
        return NULL;
    }

    lock->lk_available = true;

    lock->lk_curthread = NULL;

    spinlock_init(&lock->lk_spinlock);

    return lock;

	// add stuff here as needed

	//return lock;
}

void
lock_destroy(struct lock *lock)
{
    if(lock == NULL) {
        panic("Invalid! No lock to destroy.");
    }
    if(lock->lk_available == false) {
        panic("Invalid! Need to release lock before destroying");
    }
	KASSERT(lock != NULL);
    // release cur_thread ??
    lock->lk_curthread = NULL;
	// add stuff here as needed
    spinlock_cleanup(&lock->lk_spinlock);
    wchan_destroy(lock->lk_wchan);
	kfree(lock->lk_name);
    kfree(lock);
    lock = NULL;
}

void
lock_acquire(struct lock *lock)
{
	// Write this
    KASSERT(lock != NULL);
    if(NULL != lock->lk_curthread && lock->lk_curthread == curthread) {
    //    panic("Deadlock on lock %s\n", lock->lk_name);
        return;
    }

    KASSERT(curthread->t_in_interrupt == false);

    spinlock_acquire(&lock->lk_spinlock);
    while(lock->lk_available == false) {
        wchan_sleep(lock->lk_wchan,&lock->lk_spinlock);
    }
    KASSERT(lock->lk_available == true);
    lock->lk_available = false;
    lock->lk_curthread = curthread;
    spinlock_release(&lock->lk_spinlock);
	//(void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
	// Write this
    if(lock == NULL) {
        panic("Invalid! No lock to release");
    }
    if(lock->lk_curthread == NULL) {
        panic("Invalid! Must acquire lock before release");
    }
    spinlock_acquire(&lock->lk_spinlock);
    if(curthread == lock->lk_curthread) {
        lock->lk_available = true;
        wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);
        lock->lk_curthread = NULL;
    }
    spinlock_release(&lock->lk_spinlock);

	//(void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock)
{
	// Write this
    if(curthread == lock->lk_curthread) {
        return true;
    }
    else {
        return false;
    }
    //(void)lock;  // suppress warning until code gets written
	//return true; // dummy until code gets written
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}
	//kprintf("Created cv %s\n",name);
	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	cv->cv_wchan = wchan_create(cv->cv_name);
	if(cv->cv_wchan  == NULL)
	{
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}
	spinlock_init(&cv->cv_spinlock);
	cv->cv_lock = NULL;
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);
	wchan_destroy(cv->cv_wchan);
	spinlock_cleanup(&cv->cv_spinlock);
	cv->cv_lock = NULL;
	kfree(cv->cv_name);
	kfree(cv);
	cv = NULL;
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	//checking if the thread that called this holds the lock that was passed in
	if (false == lock_do_i_hold(lock))
	{
		panic("Not the thread that has the lock" );
	}

	//put the current thread to sleep
	spinlock_acquire(&cv->cv_spinlock);
	lock_release(lock);
	cv->cv_lock = lock;
	wchan_sleep(cv->cv_wchan,&cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	if(!cv->cv_lock && cv->cv_lock != lock)
	{
		panic("CV_signal : Not the correct lock%s",lock->lk_name );
	}
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeone(cv->cv_wchan,&cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	if(!cv->cv_lock && cv->cv_lock != lock)
	{
		panic("CV_broadcast : Not the correct lock%s",lock->lk_name );
	}
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeall(cv->cv_wchan,&cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
}

struct rwlock * rwlock_create(const char *rw_name) {
	struct rwlock *rwlock;

	rwlock = kmalloc(sizeof(*rwlock));
	if(rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlock_name = kstrdup(rw_name);
	if(rwlock->rwlock_name == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->lk_reader = lock_create("lk_reader");
	if(rwlock->lk_reader == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}
	/*rwlock->lk_writer = lock_create("lk_writer");
	if(rwlock->lk_writer == NULL) {
		kfree(rwlock->rwlock_name);
		lock_destroy(rwlock->lk_reader);
		kfree(rwlock);
		return NULL;
	}*/

	rwlock->cv_reader = cv_create("reader_lock");
	if(rwlock->cv_reader == NULL) {
		kfree(rwlock->rwlock_name);
		lock_destroy(rwlock->lk_reader);
		//lock_destroy(rwlock->lk_writer);
		kfree(rwlock);
		return NULL;
	}

	rwlock->cv_writer = cv_create("writer_lock");
	if(rwlock->cv_writer == NULL) {
		kfree(rwlock->rwlock_name);
		lock_destroy(rwlock->lk_reader);
		//lock_destroy(rwlock->lk_writer);
		cv_destroy(rwlock->cv_reader);
		kfree(rwlock);
		return NULL;
	}
	rwlock->cv_writer->cv_lock = rwlock->lk_reader;
	rwlock->cv_reader->cv_lock = rwlock->lk_reader;
	rwlock->reader_count = 0;
	rwlock->is_writer_present = false;
	rwlock->is_reader_waiting = false;
	rwlock->writer_waiting_count = 0;
	return rwlock;

}
void rwlock_destroy(struct rwlock *rw) {

	KASSERT(rw != NULL);
	if(rw->reader_count > 0 || rw->is_writer_present == true
	   || rw->writer_waiting_count>0) {
		panic("Readers or writers are present");
	}
	cv_destroy(rw->cv_reader);
	cv_destroy(rw->cv_writer);
	lock_destroy(rw->lk_reader);
	//lock_destroy(rw->lk_writer);
	kfree(rw->rwlock_name);
	kfree(rw);
	rw = NULL;
}


void rwlock_acquire_read(struct rwlock *rw) {
	lock_acquire(rw->lk_reader);
	while(rw->is_writer_present == true || rw->writer_waiting_count>0)
	{
		//rw->is_reader_waiting = true;
		cv_wait(rw->cv_reader,rw->lk_reader);
	}
	rw->reader_count++;
	lock_release(rw->lk_reader);
}
void rwlock_release_read(struct rwlock *rw) {
	lock_acquire(rw->lk_reader);
	if(rw->reader_count>0)
	{
		rw->reader_count--;
	}
	else
	{
		panic("Trying to release more readers than there are present");
	}
	if(rw->reader_count == 0)
	{
		cv_broadcast(rw->cv_writer,rw->lk_reader);
		cv_broadcast(rw->cv_reader,rw->lk_reader);
	}
	lock_release(rw->lk_reader);
}
void rwlock_acquire_write(struct rwlock *rw) {
	lock_acquire(rw->lk_reader);
	while (rw->is_writer_present == true||rw->reader_count > 0 )
	{
		rw->writer_waiting_count++;
		cv_wait(rw->cv_writer,rw->lk_reader);
		rw->writer_waiting_count--;
	}
	rw->is_writer_present = true;
	lock_release(rw->lk_reader);
}
void rwlock_release_write(struct rwlock *rw) {
	lock_acquire(rw->lk_reader);
	if(rw->is_writer_present == false && rw->writer_waiting_count == 0)
	{
		panic("No write lock to release");
	}
	cv_broadcast(rw->cv_reader,rw->lk_reader);
	cv_signal(rw->cv_writer,rw->lk_reader);
	rw->is_writer_present = false;
	lock_release(rw->lk_reader);
}