//
// Created by abhinav on 3/5/16.
//

#include "../include/processtable.h"
#include <types.h>
#include <lib.h>
#include <limits.h>
#include "../include/current.h"
#include "../include/proc.h"


int addAvailablePID(int pid)
{
    struct ListOfOpenPID *temp = (struct ListOfOpenPID*)kmalloc(sizeof(struct ListOfOpenPID));
    if(temp == NULL)
    {
        return -1;
    }
    temp->availablePID = pid;
    temp->next = NULL;
    if(pidHead == NULL)
    {
        pidHead = temp;
    }
    else if(pidTail == NULL)
    {
        pidTail = temp;
        pidHead->next=pidTail;
    }
    else
    {
        pidTail->next = temp;
        pidTail = temp;
    }
    return 0;
}
int initPIDDescriptors()
{
    //for(int i = PID_MIN ; i < OPEN_MAX ; i++)
    //{
    //    int result = addAvailablePID(i);
    //    if(result == -1)
    //    {
    //        return result;
    //    }
    //}
    return 0;
}

int getNextAvailablePID(void) {
    //struct ListOfOpenPID *temp = pidHead;
    //if(pidHead == NULL)
    //{
    //    return -1;
    //}
    //pidHead= pidHead->next;
    //int pid= temp->availablePID;
    //kfree(temp);
    //return pid;
    for (int i = PID_MIN; i < OPEN_MAX; i++) {
        if (procTable[i] == NULL) {
            return i;
        }
    }
    //panic("IT HAS HAPPENED");
    return -1;
}

struct proc* getProc(int pid)
{
    //if(NULL != procTable[pid])
    {
    //    return procTable[pid];
    }
    if(pid)//hack
    {}

    return NULL;
}

void addProc(int pid , struct proc* process)
{
    if(procTable[pid] == NULL )
    {
        //struct proc * proc1 = (struct proc*)kmalloc(sizeof(struct proc));
        //memcpy((void *)proc1,(const void *)process, sizeof(struct proc));
        procTable[pid] = process;
    }
}

void returnPID(int pid)
{
    if(procTable[pid]!=NULL)
    {
        //kfree(procTable[pid]);
        procTable[pid] = NULL;
    }

    /*struct ListOfOpenPID *temp = (struct ListOfOpenPID*)kmalloc(sizeof(struct ListOfOpenPID ));
    if(temp == NULL)
    {
        return;
    }
    temp->availablePID = pid;
    temp->next = NULL;
    if(pidTail && (pidTail->availablePID < pid))
    {
        pidTail->next = temp;
        pidTail = temp;
    }
    else if(pidHead && (pidHead->availablePID > pid))
    {
        temp->next = pidHead;
        pidHead = temp;
    }
    else
    {
        struct ListOfOpenPID *curr = pidHead;
        while(curr && curr->next && curr->next->availablePID < pid)
        {
            curr = curr->next;
        }
        temp->next = curr->next;
        curr->next = temp;
    }*/
}

void destroyProcTables(struct proc **procTable)
{
    //for(int i = 0 ; i < OPEN_MAX;i++)
    //{
        //kfree(procTable[i]);
    //}
    //kfree(procTable);
    if(procTable)
    {}

}


void initProcessTable(struct proc **procTable)
{
    for(int i = PID_MIN ; i < OPEN_MAX;i++)
    {
        procTable[i] = NULL;
    }
}

