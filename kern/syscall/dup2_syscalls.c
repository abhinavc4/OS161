//
// Created by archana on 3/7/16.
//

//int dup2(int filehandle, int newhandle);
#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/copyinout.h"
#include "../include/filetable.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include "../include/current.h"
#include "../include/lib.h"
#include "../include/synch.h"
#include "../include/filehandle.h"
#include "../include/proc.h"

int sys___dup2(int oldfd, int newfd) {

    if(!(oldfd >= 0 && oldfd < OPEN_MAX)) {
        return 0-EBADF;
    }
    if(!(newfd >= 0 && newfd < OPEN_MAX)) {
        return 0-EBADF;
    }
    struct filehandle* ohandle = getFileHandle(oldfd);
    if(NULL==ohandle)
    {
        return 0-EBADF;
    }

    struct filehandle* nhandle = getFileHandle(newfd);
    /*if(NULL==nhandle)
    {
        return 0-EBADF;
    }*/
    lock_acquire(ohandle->filelock);

    if(NULL!=nhandle) {
        if(ohandle==nhandle) {
            lock_release(ohandle->filelock);
            return newfd;
        }
        else {
            lock_acquire(nhandle->filelock);
            if(nhandle->refcount > 0) {
                nhandle->refcount--;
            }
            else {
                lock_release(nhandle->filelock);
                lock_release(ohandle->filelock);
                return 0-EBADF;
            }
            if(nhandle->refcount == 0 && newfd > 2){
                vfs_close(nhandle->vnode_obj);
            }
            lock_release(nhandle->filelock);
        }

    }

    if(ohandle->refcount ==0 ) {
        ohandle->refcount=2;
    }
    else {
        ohandle->refcount++;
    }
    curproc->p_fileTable[newfd] = ohandle;
    nhandle = curproc->p_fileTable[newfd];
    lock_release(ohandle->filelock);
    return newfd;

}