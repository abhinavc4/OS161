//
// Created by archana on 3/7/16.
//

#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/vnode.h"
#include "../include/copyinout.h"
#include "../include/filetable.h"
#include "../include/kern/seek.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include "../include/current.h"
#include "../include/lib.h"
#include "../include/kern/stat.h"
#include "../include/synch.h"


off_t sys___lseek(int fd, off_t pos,const_userptr_t whence) {

    if(!whence) {
        return 0-EINVAL;
    }
    int whenceKern;
    int whenceCopyErr = copyin(whence,&whenceKern, sizeof(int));
    if(0!=whenceCopyErr)
    {
        return 0-whenceCopyErr;
    }
    if(!(fd >= 0 && fd < OPEN_MAX)) {
        return 0-EBADF;
    }
    struct filehandle* handle = getFileHandle(fd);
    if(NULL==handle)
    {
        return 0-EBADF;
    }
    if(handle->vnode_obj->vn_fs==NULL) {
        return 0-ESPIPE;
    }
    lock_acquire(handle->filelock);
    switch (whenceKern) {
        case SEEK_SET:
            if(pos < 0) {
                return 0-EINVAL;
            }
            handle->pos = pos;
            break;
        case SEEK_CUR:
            if(handle->pos + pos < 0) {
                return 0-EINVAL;
            }
            handle->pos = handle->pos + pos;
            break;
        case SEEK_END: {
            struct stat statStruct;
            int staterr = 0;
            staterr = VOP_STAT(handle->vnode_obj, &statStruct);
            if (0 != staterr) {
                lock_release(handle->filelock);
                return 0-staterr;
            }
            if(statStruct.st_size + pos < 0) {
                return 0-EINVAL;
            }
            handle->pos = statStruct.st_size + pos;
        }
            break;
        default:
            lock_release(handle->filelock);
            return 0-EINVAL;
    }
    lock_release(handle->filelock);
    return handle->pos;
}
