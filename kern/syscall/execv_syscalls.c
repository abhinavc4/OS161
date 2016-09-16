
#include <types.h>
#include <copyinout.h>
#include <syscall.h>
//#include "../include/types.h"
#include "../include/vfs.h"
#include "../include/copyinout.h"
#include "../include/kern/fcntl.h"
#include "../include/kern/errno.h"
#include <addrspace.h>
#include "../include/proc.h"
#include <cpu.h>
#include "current.h"
#include "../include/filetable.h"
#include <machine/trapframe.h>
#include "../include/processtable.h"

/*
 * : open file.
 */
struct filehandle;
struct trapframe;

char *
localStrcpy(char *dest, const char *src,size_t len,bool add)
{
    size_t i;

    /*
     * Copy characters until we hit the null terminator.
     */
    for (i=0; i<len&&src[i]; i++) {
        dest[i] = src[i];
    }

    /*
     * Add null terminator to result.
     */
    if(add)
    {
        dest[i] = 0;
    }

    return dest;
}

int sys___execv (const_userptr_t path, char ** argv)
{
    int indexStartCount = 0;//4 * (count+1);
    int currentIndexCount = indexStartCount;
    char *packBuffer = (char*)kmalloc(100*1024);
    if(packBuffer == NULL)
    {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
        }
        return 0-ENOMEM;
    }
    int *countOfStringOffsets = (int*)kmalloc(200);
    int count = 0;
    //size_t properSize = 0;
    struct addrspace *as = NULL;
    vaddr_t entrypoint=0, stackptr = 0 ,oldStackptr = 0,tempStackptr = 0,baseOldStackptr=0;
    for(int i =0; argv[i]!=NULL;i++) {
        countOfStringOffsets[i] = currentIndexCount;

        char * arg = argv[i];
        int numberOfSlots = 0;
        int length = strlen(arg) +1;
        if(length<4)
        {
            localStrcpy(packBuffer+currentIndexCount,arg,length,true);
            numberOfSlots = 1;
        }
        else if(length%4 ==0)
        {
            localStrcpy(packBuffer+currentIndexCount,arg,length,false);
            numberOfSlots = length/4;
        }
        else
        {
            localStrcpy(packBuffer+currentIndexCount,arg,length,true);
            numberOfSlots = length/4 + 1;
        }
        currentIndexCount += 4*numberOfSlots;
        count++;
    }

    countOfStringOffsets[count] = currentIndexCount;

    if(argv == NULL)
    {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        return 0-EFAULT;
    }

    char * pathBuf = (char *)kmalloc(PATH_MAX);
    if(pathBuf == NULL)
    {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-ENOMEM;
    }

    int pathNameErr = 0;
    size_t actualSize =0;
    struct vnode *v = NULL;
    //int uargsErr = 0;

    pathNameErr = copyinstr((const_userptr_t) path,
                            (char*)pathBuf,
                            __PATH_MAX,
                            &actualSize);
    if(0 == strlen(pathBuf))
    {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-EINVAL;
    }
    if(pathNameErr != 0)
    {
        return 0-pathNameErr;
    }

    //char **argvBuff = (char**)kmalloc(100*1024);
    //char *argvBuff = (char*)kmalloc(100*1024);
    /*if(argvBuff == NULL)
    {
        return 0-ENOMEM;
    }
    for(int i = 0 ; i < count; i++)
    {
        argvBuff[i] = (char*)kmalloc(1024);
        if(argvBuff[i]==NULL)
        {
            return 0-ENOMEM;
        }
    }*/


    /*for(int i = 0 ; NULL!=argv[i];i++)
    {
        copyinstr((const_userptr_t) (argv)[i],
                             (char*)(argvBuff)[i],
                             strlen((char*)(argv)[i])+1,
                             &properSize
                             );
        //if(uargsErr!=0)
        //{
        //    return 0-EFAULT;
        //}
        count++;
    }*/
    int execOpenResult = vfs_open(pathBuf, O_RDONLY, 0, &v);

    if (execOpenResult) {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-execOpenResult;
    }

    as = as_create();
    if (as == NULL) {
        vfs_close(v);
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-ENOMEM;
    }

    proc_setas(as);
    as_activate();

    /* Load the executable. */
    int loadElfResult = load_elf(v, &entrypoint);
    if (loadElfResult) {
        /* p_addrspace will go away when curproc is destroyed */
        vfs_close(v);
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-loadElfResult;
    }
    vfs_close(v);

    int stackDefineResult = as_define_stack(as, &stackptr);
    if (stackDefineResult) {
        if(packBuffer != NULL)
        {
            kfree(packBuffer);
            packBuffer = NULL;
        }
        if(countOfStringOffsets !=NULL)
        {
            kfree(countOfStringOffsets);
            countOfStringOffsets = NULL;
        }
        if(pathBuf != NULL)
        {
            kfree(pathBuf);
            pathBuf = NULL;
        }
        return 0-stackDefineResult;
    }



    //kprintf("%d %d",currentIndexCount,count*maxStringCount);

    stackptr -= currentIndexCount;

    copyout((const void *)packBuffer,(userptr_t)stackptr,currentIndexCount);

    //store beginnning of strings on the stack
    oldStackptr = stackptr;
    //

    tempStackptr = stackptr;
    while (tempStackptr!=MIPS_KSEG0)
    {
        //kprintf("Address is %x\n ",tempStackptr);
        //kprintf("Value is %s\n",(char*)tempStackptr);
        tempStackptr +=4;
    }

    tempStackptr = oldStackptr;
    //decrement stackptr by the number of pointers we will need
    stackptr -= 4*(count+1);

    baseOldStackptr = oldStackptr;
    for(int i = 0 ; i < count ; i++)
    {
        oldStackptr = baseOldStackptr + countOfStringOffsets[i];
        char *stringLocation = (char*)oldStackptr;
        char **addStringLocation =  &stringLocation;
        copyout(addStringLocation,(userptr_t)stackptr, 4);
        (stackptr) += 4;
    }
    char *val = NULL;
    char **addStringLocation =   &val;
    copyout(addStringLocation,(userptr_t)stackptr, 4);

    stackptr -= (4*count);
    enter_new_process(count /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
                      NULL /*userspace addr of environment*/,
                      stackptr, entrypoint);
    if(packBuffer != NULL)
    {
        kfree(packBuffer);
        packBuffer = NULL;
    }
    if(countOfStringOffsets !=NULL)
    {
        kfree(countOfStringOffsets);
        countOfStringOffsets = NULL;
    }
    if(pathBuf != NULL)
    {
        kfree(pathBuf);
        pathBuf = NULL;
    }
    return 0;
}