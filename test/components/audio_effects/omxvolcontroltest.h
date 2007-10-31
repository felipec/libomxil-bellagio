/**
  @file test/components/audio_effects/omxvolcontroltest.c
	
	This simple test application provides a tesnting stream for the volume control component. 
	It will be added in the more complex audio test application in the next release  
	
	Copyright (C) 2007  STMicroelectronics and Nokia

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
	
	$Date$
	Revision $Rev$
	Author $Author$
*/

#ifndef __OMXVOLCONTROLTEST_H__
#define __OMXVOLCONTROLTEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>

#include <tsemaphore.h>
#include <user_debug_levels.h>

/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0

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
#define BUFFER_IN_SIZE 8192

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

#endif
