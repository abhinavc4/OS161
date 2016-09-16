//
// Created by archana on 3/7/16.
//

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
#include "../include/proc.h"

/*
 * : open file.
 */
int sys___close(int fd) {

    if(!(fd >= 0 && fd < OPEN_MAX)) {
        return 0-EBADF;
    }

    struct filehandle* handle = getFileHandle(fd);
    if(NULL==handle)
    {
        return 0-EBADF;
    }
    if(fd >2) {
        lock_acquire(handle->filelock);
        handle->refcount--;
        curproc->p_fileTable[fd] = NULL;
        if(handle->refcount == 0){
            vfs_close(handle->vnode_obj);
            kfree(handle->uio_obj);
            lock_release(handle->filelock);
            lock_destroy(handle->filelock);
            returnFileDescriptor(fd);
            kfree(handle);
            handle= NULL;
        }
        else {
            handle->refcount--;
            lock_release(handle->filelock);
        }

    }


    return 0;

}
