
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

pid_t get___pid(void)
{
    return kproc->pid;
}
