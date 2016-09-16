//
// Created by archana on 3/9/16.
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

/*
 * : open file.
 */

int sys___getcwd(userptr_t dest_buf, size_t buflen) {

    void * kern_buffer = kmalloc(buflen);
    int copyInErr = 0;
    copyInErr = copyin(dest_buf,kern_buffer,buflen);
    if(0 != copyInErr) {
        return 0-copyInErr;
    }

    int result = vfs_getcwd(kern_buffer);
    return result;

}