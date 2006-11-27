/**
	@file src/queue.c
	
 Implements a simple LIFO structure used for queueing OMX buffers.
	
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
	
	2006/05/11:  Buffer queue version 0.2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>

#include "queue.h"
#include "omx_comp_debug_levels.h"

void queue_init(queue_t* queue)
{
	int i;
	qelem_t* newelem;
	qelem_t* current;
	queue->first = malloc(sizeof(qelem_t));
	memset(queue->first, 0, sizeof(qelem_t));
	current = queue->last = queue->first;
	queue->nelem = 0;
	for (i = 0; i<MAX_QUEUE_ELEMENTS - 2; i++) {
		newelem = malloc(sizeof(qelem_t));
		memset(newelem, 0, sizeof(qelem_t));
		current->q_forw = newelem;
		current = newelem;
	}
	current->q_forw = queue->first;
}

void queue_deinit(queue_t* queue)
{
	int i;
	qelem_t* current;
	current = queue->first;
	for (i = 0; i<MAX_QUEUE_ELEMENTS - 2; i++) {
 		if (current != NULL) {
 			current = current->q_forw;
 			free(queue->first);
 			queue->first = current;
 		}
 	}
}

void queue(queue_t* queue, void* data)
{
	if (queue->last->data != NULL) {
		return;
	}
	queue->last->data = data;
	queue->last = queue->last->q_forw;
	queue->nelem++;
}

void* dequeue(queue_t* queue)
{
	void* data;
	if (queue->first->data == NULL) {
		return NULL;
	}
	data = queue->first->data;
	queue->first->data = NULL;
	queue->first = queue->first->q_forw;
	queue->nelem--;
	
	return data;
}

int getquenelem(queue_t* queue)
{
	return queue->nelem;
}
