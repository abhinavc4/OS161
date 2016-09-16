//
// Created by abhinav on 3/5/16.
//

// The filehandle is a structure that needs to store the
// following information

// A Vnode - A reference to the file that is being operated on
// This is returned by the VFS_OPEN

// A UIO Object that contains a reference to the file that is being read

#ifndef OS161_FILEHANDLE_H
#define OS161_FILEHANDLE_H
#include <types.h>

struct filehandle
{
    int flags;
    mode_t rwmode;
    struct uio *uio_obj;
    struct vnode *vnode_obj;
    int refcount;
    off_t pos;
    struct lock *filelock;
};

#endif //OS161_FILEHANDLE_H
