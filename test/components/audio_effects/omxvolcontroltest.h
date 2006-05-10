/**
	@file test/components/reference/omxreferencetest.h
	
	Test application that uses the reference component. 
	This application does not apply any stream manipulation, but reproduces
	only the input on the output.
	
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
	
	2006/02/08:  Reference Component Test version 0.1

*/

#include <OMX_Core.h>
#include <OMX_Types.h>

#include <pthread.h>
#include <sys/stat.h>

#include "tsemaphore.h"

/* Application's private data */
typedef struct appPrivateType{
	pthread_cond_t condition;
	pthread_mutex_t mutex;
	void* input_data;
	OMX_BUFFERHEADERTYPE* currentInputBuffer;
	tsem_t* eventSem;
	tsem_t* eofSem;
}appPrivateType;

/* Size of the buffers requested to the component */
//#define BUFFER_SIZE 2048
#define BUFFER_IN_SIZE 2048

/* Callback prototypes */
OMX_ERRORTYPE volcEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_IN OMX_PTR pEventData);

OMX_ERRORTYPE volcEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE volcFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** Helper functions */
static int getFileSize(int fd);
