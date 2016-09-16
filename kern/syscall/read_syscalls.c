
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

/*
 * : open file. OFF COURSE MR OBVIOUS >>>>
 */
ssize_t sys___read(int fd, userptr_t dest_buf, size_t size) {

    struct iovec iov ;
    struct uio myuio ;

    if(!(fd >= 0 && fd < OPEN_MAX)) {
        return 0-EBADF;
    }

    struct filehandle* handle = getFileHandle(fd);
    if(NULL==handle)
    {
        return 0-EBADF;
    }
    if(handle->rwmode == O_WRONLY) {
        return 0-EBADF;
    }
    lock_acquire(handle->filelock);
    //check permissions

    //struct iovec *iov = (struct iovec*)kmalloc(sizeof(struct iovec));

    if(handle->uio_obj)
    {
        myuio = *handle->uio_obj;
    }
    //struct uio *myuio = handle->uio_obj;
    //if(NULL == myuio)
    //{
    //    myuio = (struct uio*)kmalloc(sizeof(struct uio));
    //}
    //void * kern_buffer = kmalloc(size);
    //copyin(dest_buf,kern_buffer,size);
    uio_kinit(&iov, &myuio, dest_buf, size, handle->pos, UIO_READ);
    myuio.uio_segflg = UIO_USERSPACE;
    myuio.uio_space = curproc->p_addrspace;
    int result = VOP_READ(handle->vnode_obj, &myuio);
    handle->pos = myuio.uio_offset;
    lock_release(handle->filelock);
    if(0!=result) {
        return 0-result;
    }
    //copyout(kern_buffer,dest_buf,(size - myuio->uio_resid));
    //kfree(kern_buffer);
    return (size - myuio.uio_resid);
}
