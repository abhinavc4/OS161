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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include "../include/synch.h"
#include "../include/lib.h"

/*
 * Called by the driver during initialization.
 */
static struct cv *cv_male;
static struct cv *cv_female;
static struct cv *cv_matchmaker;
static struct cv *cv_magic;

static struct lock *whalelock;
static volatile int male_count = 0;
static volatile int female_count = 0;
static volatile int matchmaker_count = 0;
static volatile int magic = 0;
static struct semaphore *sem_male = NULL;
static struct semaphore *sem_female = NULL;
static struct semaphore *sem_matchmaker = NULL;

void whalemating_init() {
	cv_male = cv_create("cv_male");
	cv_female = cv_create("cv_female");
	cv_matchmaker = cv_create("cv_female");
	cv_magic = cv_create("cv_magic");
	whalelock = lock_create("whalelock");
	sem_male = sem_create("sem_male", 0);
	sem_female = sem_create("sem_female", 0);
	sem_matchmaker = sem_create("sem_matchmaker", 0);
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	cv_destroy(cv_female);
	cv_destroy(cv_male);
	cv_destroy(cv_matchmaker);
	lock_destroy(whalelock);
}
//males must know about females and females must know about males
// males must know about mm and mm must know about males
void
male(uint32_t index)
{
	lock_acquire(whalelock);
	//increment the count
	male_start(index);
	male_count++;

	if(female_count >0 && matchmaker_count > 0)
	{
		cv_signal(cv_female,whalelock);
		cv_signal(cv_matchmaker,whalelock);
		female_count--;
		matchmaker_count --;
		male_count--;
	}
	else//there is a mm and a female available
	{
		//wait for a signal from a female
		cv_wait(cv_male,whalelock);
	}

	//now we have both the female and the mathmaker ,
	// lets exit myself and tell them to exit as well
	//tell a female i m available
	//cv_signal(cv_female,whalelock);
	//tell a matchmaker i m available
	//cv_signal(cv_matchmaker,whalelock);

	lock_release(whalelock);
	male_end(index);
	return;
}

void
female(uint32_t index)
{
	lock_acquire(whalelock);
	female_start(index);
	female_count++;

	if(male_count >0 && matchmaker_count > 0)
	{
		cv_signal(cv_male,whalelock);
		cv_signal(cv_matchmaker,whalelock);
		female_count--;
		matchmaker_count --;
		male_count--;
	}
	else
	{
		cv_wait(cv_female,whalelock);
	}
	lock_release(whalelock);
	female_end(index);
	return;
}

void
matchmaker(uint32_t index)
{
	matchmaker_start(index);
	lock_acquire(whalelock);
	matchmaker_count++;
	if(female_count >0 && male_count > 0)
	{
		cv_signal(cv_female,whalelock);
		cv_signal(cv_male,whalelock);
		female_count--;
		matchmaker_count --;
		male_count--;
	}
	else
	{
		cv_wait(cv_matchmaker,whalelock);
	}

	lock_release(whalelock);
	matchmaker_end(index);
	return;
}
