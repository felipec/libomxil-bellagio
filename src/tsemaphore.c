/**
	@file src/tsemaphore.c
	
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
	
	2006/05/11:  Threads semaphore version 0.2

*/

#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "tsemaphore.h"

void tsem_init(tsem_t* tsem, unsigned int val)
{
	pthread_cond_init(&tsem->condition, NULL);
	pthread_mutex_init(&tsem->mutex, NULL);
	tsem->semval = val;
}

void tsem_deinit(tsem_t* tsem)
{
	pthread_cond_destroy(&tsem->condition);
	pthread_mutex_destroy(&tsem->mutex);
}

void tsem_down(tsem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);
	while (tsem->semval == 0)
		pthread_cond_wait(&tsem->condition, &tsem->mutex);
	tsem->semval--;
	pthread_mutex_unlock(&tsem->mutex);
}

void tsem_up(tsem_t* tsem)
{
	pthread_mutex_lock(&tsem->mutex);
	tsem->semval++;
	pthread_cond_signal(&tsem->condition);
	pthread_mutex_unlock(&tsem->mutex);
}

void tsem_reset(tsem_t* tsem) {
	pthread_mutex_lock(&tsem->mutex);
	tsem->semval=0;
	pthread_mutex_unlock(&tsem->mutex);
}
