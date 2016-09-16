//
// Created by abhinav on 3/5/16.
//

// This file contains the filetable
// The filetable is an array of filehandles
//
#ifndef OS161_PROCESSTABLE_H
#define OS161_PROCESSTABLE_H

#include "limits.h"
#include <types.h>

struct ListOfOpenPID{
    int availablePID;
    struct ListOfOpenPID *next;
};

struct ListOfOpenPID *pidHead;
struct ListOfOpenPID *pidTail;
int addAvailablePID(int pisd);
int initPIDDescriptors(void);
int getNextAvailablePID(void);
struct proc* getProc(int pid);
void addProc(int pid , struct proc* process);
void returnPID(int pid);
void destroyProcTables(struct proc **procTable);
void initProcessTable(struct proc **procTable);
//void copyFileTable(struct filehandle **fileTable);
void newForkedThread(void *tf, unsigned long unused);
char * localStrcpy(char *dest, const char *src,size_t len,bool add);
#endif //OS161_FILETABLE_H
