//
// Created by abhinav on 3/5/16.
//

// This file contains the filetable
// The filetable is an array of filehandles
//
#ifndef OS161_FILETABLE_H
#define OS161_FILETABLE_H

#include "filehandle.h"
#include "limits.h"
struct ListOfOpenFD{
    int availableFD;
    struct ListOfOpenFD *next;
};

struct ListOfOpenFD *fdHead;
struct ListOfOpenFD *fdTail;
int addAvailableFD(int fileDescId);
void initFileDescriptors(void);
void returnFileDescriptor(int fileDescId);
int getNextFileDescriptor(void);
void initFileTables(void);
void destroyFileTables(struct filehandle ** fileTable);
void initEntireFileTable(struct filehandle ** fileTable);
int createFileHandle(int fd);
struct filehandle* getFileHandle(int fd);

#endif //OS161_FILETABLE_H
