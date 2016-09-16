
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

ssize_t sys___write(int fd, const_userptr_t source_buf, size_t size){

    struct iovec iov ;
    struct uio myuio ;

    if(!(fd >= 0 && fd < OPEN_MAX)) {
        return 0-EBADF;
    }
    if(!source_buf) {
        return 0-EFAULT;
    }
    struct filehandle* handle = getFileHandle(fd);
    if(NULL==handle)
    {
        return 0-EBADF;
    }
    if(handle->rwmode == O_RDONLY) {
        return 0-EBADF;
    }
    lock_acquire(handle->filelock);
    if(handle->uio_obj)
    {
        myuio = *handle->uio_obj;
    }
    char kern_buffer[size];
    int copyinErr = copyin(source_buf,&kern_buffer,size);
    if(0!=copyinErr) {
        return 0-copyinErr;
    }
    uio_kinit(&iov, &myuio, &kern_buffer, size, handle->pos, UIO_WRITE);
    int result = VOP_WRITE(handle->vnode_obj, &myuio);
    if(0!=result)
    {
        lock_release(handle->filelock);
        return  0-result;
    }
    handle->pos = myuio.uio_offset;
    lock_release(handle->filelock);
    return (size - myuio.uio_resid);
}
