/**
	@file src/tsemaphore.h
	
	Implements a simple inter-thread semaphore so not to have to deal with IPC
	creation and the like.
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

	This library is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free
	Software Foundation; either version 2.1 of the License, or (at your option)
	any later version.

	This library is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
	details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation, Inc.,
	51 Franklin St, Fifth Floor, Boston, MA
	02110-1301  USA
	
	2006/02/08:  Threads semaphore version 0.1

*/

#ifndef __TSEMAPHORE
#define __TSEMAPHORE

typedef struct tsem_t{
	pthread_cond_t condition;
	pthread_mutex_t mutex;
	unsigned int semval;
}tsem_t;

/** Initializes the semaphore at a given value
 * \param tsem the semaphore to initialize
 * \param val the initial value of the semaphore
 */
void tsem_init(tsem_t* tsem, unsigned int val);

/** Destroy the semaphore 
 * \param tsem the semaphore to destroy
 */
void tsem_deinit(tsem_t* tsem);

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero.
 * \param tsem the semaphore to decrease
 */
void tsem_down(tsem_t* tsem);

/** Increases the value of the semaphore
 * \param tsem the semaphore to increase
 */
void tsem_up(tsem_t* tsem);

/** Reset the value of the semaphore
 * \param tsem the semaphore to reset
 */
void tsem_reset(tsem_t* tsem);

#endif
