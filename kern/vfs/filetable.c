//
// Created by abhinav on 3/5/16.
//

#include <filetable.h>
#include <types.h>
#include <lib.h>
#include <limits.h>
#include "../include/current.h"
#include <proc.h>

int addAvailableFD(int fileDescId)
{
    struct ListOfOpenFD *temp = (struct ListOfOpenFD*)kmalloc(sizeof(struct ListOfOpenFD));
    if(NULL == temp)
    {
        return -1;
    }
    temp->availableFD = fileDescId;
    temp->next = NULL;
    if(fdHead == NULL)
    {
        fdHead = temp;
    }
    else if(fdTail == NULL)
    {
        fdTail = temp;
        fdHead->next=fdTail;
    }
    else
    {
        fdTail->next = temp;
        fdTail = temp;
    }
    return 0;
}
void initFileDescriptors()
{

    for(int i = 3 ; i < OPEN_MAX ; i++)
    {
        //if(-1 == addAvailableFD(i))
        //{
        //    break;
        //}

    }
}

int getNextFileDescriptor()
{
    //struct ListOfOpenFD *temp = fdHead;
    //fdHead= fdHead->next;
    int fd= 0;
    for(int i = 3 ; i < OPEN_MAX;i++)
    {
        if(NULL == curproc->p_fileTable[i])
        {
            fd = i;
        }
    }
    if(fd ==0)
    {
        return -1;
    }
    //kfree(temp);
    if(-1 == createFileHandle(fd))
    {
        return -1;
    }
    return fd;
}

struct filehandle* getFileHandle(int fd)
{
    if(NULL != curproc->p_fileTable[fd])
    {
        return curproc->p_fileTable[fd];
    }
    return NULL;
}

void returnFileDescriptor(int fileDescId)
{
    if(NULL!=curproc->p_fileTable[fileDescId])
    {
        kfree(curproc->p_fileTable[fileDescId]);
        curproc->p_fileTable[fileDescId] = NULL;
    }
    /*struct ListOfOpenFD *temp = (struct ListOfOpenFD*)kmalloc(sizeof(struct ListOfOpenFD ));
    if(temp == NULL)
    {
        return;
    }
    temp->availableFD = fileDescId;
    temp->next = NULL;
    if(fdTail && (fdTail->availableFD < fileDescId))
    {
        fdTail->next = temp;
        fdTail = temp;
    }
    else if(fdHead && (fdHead->availableFD > fileDescId))
    {
        temp->next = fdHead;
        fdHead = temp;
    }
    else
    {
        struct ListOfOpenFD *curr = fdHead;
        while(curr->next && curr->next->availableFD < fileDescId)
        {
            curr = curr->next;
        }
        temp->next = curr->next;
        curr->next = temp;
    }*/
}

int createFileHandle(int fd)
{
    if(NULL == curproc->p_fileTable[fd])
    {
        curproc->p_fileTable[fd] = (struct filehandle *) kmalloc(sizeof(struct filehandle));
        if(curproc->p_fileTable[fd] == NULL)
        {
            return -1;
        }
        return 0;
    }
    return -1;
}

void initEntireFileTable(struct filehandle ** fileTable)
{
    for(int i = 3 ; i < OPEN_MAX;i++)
    {
        fileTable[i] = NULL;
    }
}

void destroyFileTables(struct filehandle ** fileTable)
{
    for(int i = 3 ; i < OPEN_MAX;i++)
    {
        if(NULL!=fileTable[i])
        {
            //kfree(fileTable[i]);
            fileTable[i]= NULL;
        }

    }
    //kfree(fileTable);
}