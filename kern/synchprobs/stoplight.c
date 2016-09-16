/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include "../include/synch.h"

/*
 * Called by the driver during initialization.
 */
static struct semaphore *controlEntry;

static struct lock *lk_quad0;
static struct lock *lk_quad1;
static struct lock *lk_quad2;
static struct lock *lk_quad3;
void acquireLock(unsigned int direction);
void releaseLock(unsigned int direction) ;

void sortArray(int * myQuadrant);
void
stoplight_init(void) {
	lk_quad0 = lock_create("lk_quad0");
	lk_quad1 = lock_create("lk_quad1");
	lk_quad2 = lock_create("lk_quad2");
	lk_quad3 = lock_create("lk_quad3");
	controlEntry = sem_create("controlEntry",2);
	if(controlEntry == NULL)
	{
		panic("stoplight_init ,creating semaphore %s failed\n",controlEntry->sem_name);
	}
	return;
}

void stoplight_cleanup(void) {

	lock_destroy(lk_quad0);
	lock_destroy(lk_quad1);
	lock_destroy(lk_quad2);
	lock_destroy(lk_quad3);
	sem_destroy(controlEntry);
	return;
}
/****************IMPLEMENT THIS************/

void acquireLock(unsigned int direction) {
	switch(direction) {
		case 0:
			lock_acquire(lk_quad0);
			break;
		case 1:
			lock_acquire(lk_quad1);
			break;
		case 2:
			lock_acquire(lk_quad2);
			break;
		case 3:
			lock_acquire(lk_quad3);
			break;
	}
}

void releaseLock(unsigned int direction) {
	switch(direction) {
		case 0:
			lock_release(lk_quad0);
			break;
		case 1:
			lock_release(lk_quad1);
			break;
		case 2:
			lock_release(lk_quad2);
			break;
		case 3:
			lock_release(lk_quad3);
			break;
	}
}

void
turnright(unsigned int direction, unsigned int index)
{
	P(controlEntry);
	acquireLock(direction);
	inQuadrant(direction,index);
	leaveIntersection(index);
	releaseLock(direction);
	V(controlEntry);
	return;
}

void gostraight(unsigned int direction , unsigned int index)
{
	P(controlEntry);
	int smallerQuadrant = -1;
	int largerQuadrant = -1;
	int myQuadrantArray[2] ={direction,(direction+3)%4};
	if(myQuadrantArray[0] > myQuadrantArray[1])
	{
		smallerQuadrant = myQuadrantArray[1];
		largerQuadrant = myQuadrantArray[0];
	}
	else
	{
		smallerQuadrant = myQuadrantArray[0];
		largerQuadrant = myQuadrantArray[1];
	}
	acquireLock(smallerQuadrant);
	acquireLock(largerQuadrant);
	inQuadrant(myQuadrantArray[0],index);
	inQuadrant(myQuadrantArray[1],index);
	releaseLock(myQuadrantArray[0]);
	leaveIntersection(index);
	releaseLock(myQuadrantArray[1]);
	V(controlEntry);
}

void sortArray(int * myQuadrant)
{
	for(int i = 0 ; i < 3 ; i++)
	{
		for(int j = i+1 ; j <3 ; j++)
		{
			if(myQuadrant[i]>myQuadrant[j])
			{
				int temp = myQuadrant[i];
				myQuadrant[i] = myQuadrant[j];
				myQuadrant[j] = temp;
			}
		}
	}
}

void turnleft(unsigned int direction , unsigned int index)
{
	P(controlEntry);
	//try to aquire the lock that has the
	// smallest index to go straight
	int secondQuadrant = (direction + 3) % 4;
	int thirdQuadrant = (secondQuadrant + 3) % 4;
	int myQuadrantArray[3] ={direction, secondQuadrant, thirdQuadrant};
	int sortedMyQuadrantArray[3] = {direction, secondQuadrant, thirdQuadrant};
	sortArray(sortedMyQuadrantArray);

	acquireLock(sortedMyQuadrantArray[0]);
	acquireLock(sortedMyQuadrantArray[1]);
	acquireLock(sortedMyQuadrantArray[2]);
	inQuadrant(myQuadrantArray[0],index);
	inQuadrant(myQuadrantArray[1],index);
	releaseLock(myQuadrantArray[0]);
	inQuadrant(myQuadrantArray[2],index);
	releaseLock(myQuadrantArray[1]);
	leaveIntersection(index);
	releaseLock(myQuadrantArray[2]);
	V(controlEntry);
}