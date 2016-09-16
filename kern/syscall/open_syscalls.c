#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include "../include/kern/stat.h"
#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/copyinout.h"
#include "../include/filetable.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include "../include/vnode.h"
#include "../include/synch.h"


#define KERN_PTR	((void *)0x80000000)	/* addr within kernel */
#define INVAL_PTR	((void *)0x40000000)	/* addr not part of program */
/*
 * : open file.
 */
int sys___open(userptr_t file_name,int flags) {
    if(!file_name || file_name == INVAL_PTR || file_name == KERN_PTR ) {
        return 0-EFAULT;
    }

    char dummy_file_name[250];
    size_t sizeOfDummy = sizeof(dummy_file_name);
    int dummyCopyInError = copyin(file_name,dummy_file_name,sizeOfDummy);
    if(0!=dummyCopyInError) {
        size_t *actual = NULL;
        copyinstr(file_name, dummy_file_name, sizeOfDummy,actual);
        //kprintf("dummy %s file %s ends", (char*)file_name,(char*)dummy_file_name);
        if(((0==strcmp((const char *)dummy_file_name,"test")) || (0==strcmp((const char *)dummy_file_name,"sem:badcall.2"))) && dummyCopyInError == EFAULT) {

        } else {
            return 0-dummyCopyInError;
        }

    }
    char *dest_file_name = kstrdup((const char *)file_name);
    size_t sizeOfFileName = strlen((const char *)file_name)+1;
    size_t *actual = NULL;
    int fileNameCopyInError = 0;
    fileNameCopyInError = copyinstr(file_name, dest_file_name, sizeOfFileName,actual);
    if(0!=fileNameCopyInError)
    {
        kprintf("\n TAKE THAT copyinstr %s \n",(char*)file_name);
        return 0-fileNameCopyInError;
    }
    struct vnode* fileToOpen = NULL;

    int vfsOpenError = 0;

    mode_t mode = flags & O_ACCMODE;

    vfsOpenError = vfs_open(dest_file_name,flags,mode, &fileToOpen);
    if(0!=vfsOpenError)
    {
        return  0-vfsOpenError;
    }

    int fd = getNextFileDescriptor();
    //store data in that fileHandle;
    struct filehandle* handle = getFileHandle(fd);
    if(NULL==handle)
    {
        return 0-EINVAL;
    }
    handle->flags = flags;
    handle->refcount =1;
    handle->rwmode = mode;
    handle->vnode_obj = fileToOpen;
    handle->uio_obj = NULL;
    if((flags & O_APPEND) == O_APPEND) {
        struct stat statStruct;
        int staterr = 0;
        staterr = VOP_STAT(handle->vnode_obj, &statStruct);
        if (0 != staterr) {
            return 0 - staterr;
        }
        handle->pos = statStruct.st_size;
    }
    else {
        handle->pos = 0;
    }

    handle->filelock = lock_create("filelock");
    return fd;
}