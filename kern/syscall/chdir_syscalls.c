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
//int chdir(const char *path);
int sys___chdir(const_userptr_t path) {

    void * kern_buffer = kmalloc(100);
    int copyInErr = 0;
    copyInErr = copyin(path,kern_buffer,100);
    if(0 != copyInErr) {
        return 0-copyInErr;
    }

    int result = vfs_chdir(kern_buffer);
    return result;

}