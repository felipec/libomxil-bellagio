/**
	@file test/components/alsasink/omxalsasinktest.c
	
	Simple application that uses an OpenMAX alsa sink component. The application receives
	a pcm stream from a file or, if not specified, from standard input.
	The audio stream is inteded as stereo, at 44100 Hz.
	
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
	
	2006/02/08:  Alsa Sink Test version 0.1

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>

#include "omxalsasinktest.h"
#include "tsemaphore.h"

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;

int main(int argc, char** argv){
	OMX_ERRORTYPE err;
	OMX_HANDLETYPE handle;
	OMX_CALLBACKTYPE callbacks = { .EventHandler = alsasinkEventHandler,
											 .EmptyBufferDone = alsasinkEmptyBufferDone,
											 .FillBufferDone = NULL
	};

	OMX_PORT_PARAM_TYPE param;
	OMX_PARAM_PORTDEFINITIONTYPE omxAudioPortDefinition;
	OMX_AUDIO_PARAM_PCMMODETYPE omxAudioParamPcmMode;
	OMX_BUFFERHEADERTYPE *omxbuffer1, *omxbuffer2;
	int fd = 0;
	unsigned int filesize;
	int data_read;
	int curr_buff;
	int i;

	/* Obtain file descriptor */
	if(argc == 2){
	  fd = open(argv[1], O_RDONLY);
	  if(fd < 0){
	    perror("Error opening input file\n");
	    exit(1);
	  }
	}
	else if(argc == 1){
	  fd = STDIN_FILENO;
	}else{
	  DEBUG(DEB_LEV_ERR, "Usage: %s [file]\n", argv[0]);		
	}
	
	filesize = getFileSize(fd);
	
	/* Initialize application private data */
	appPriv = malloc(sizeof(appPrivateType));
	pthread_cond_init(&appPriv->condition, NULL);
	pthread_mutex_init(&appPriv->mutex, NULL);
	appPriv->eventSem = malloc(sizeof(tsem_t));
	tsem_init(appPriv->eventSem, 0);
	
	err = OMX_Init();
	
	/** Ask the core for a handle to the alsasink component
	 */
	err = OMX_GetHandle(&handle, "OMX.st.alsa.alsasink", NULL /*appPriv */, &callbacks);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Component Not Found!!\n");
		exit(1);
	}
	/** Set the number of ports for the alsasink component
	 */
	err = OMX_GetParameter(handle, OMX_IndexParamAudioInit, &param);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}
	param.nPorts = 1;
	err = OMX_SetParameter(handle, OMX_IndexParamAudioInit, &param);
	if(err != OMX_ErrorNone){
	  DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
	  exit(1);
	}

	/** Set the port definition type
	 */
	omxAudioPortDefinition.nPortIndex = 0;
	err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &omxAudioPortDefinition);
	/** Set up the PCM parameters
	 */
	omxAudioParamPcmMode.nPortIndex = 0;
	err = OMX_GetParameter(handle, OMX_IndexParamAudioPcm, &omxAudioParamPcmMode);
	DEBUG(DEB_LEV_PARAMS, "Default PCM rate is %i\n", (int)omxAudioParamPcmMode.nSamplingRate);
	omxAudioParamPcmMode.nSamplingRate = 44100;
	err = OMX_SetParameter(handle, OMX_IndexParamAudioPcm, &omxAudioParamPcmMode);
	DEBUG(DEB_LEV_PARAMS, "Now PCM rate is %i\n", (int)omxAudioParamPcmMode.nSamplingRate);
	
	
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	omxbuffer1 = omxbuffer2 = NULL;
	err = OMX_AllocateBuffer(handle, &omxbuffer1, 0, NULL, BUFFER_SIZE);
	if (err != OMX_ErrorNone) {
	  DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 1 %i\n", err);
	  exit(1);
	}
	
	err = OMX_AllocateBuffer(handle, &omxbuffer2, 0, NULL, BUFFER_SIZE);
	if (err != OMX_ErrorNone) {
	  DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 2 %i\n", err);
	  exit(1);
	}

	tsem_down(appPriv->eventSem);

	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

	/* Wait for commands to complete */
	tsem_down(appPriv->eventSem);
	
	/* Now, toss down one buffer */
	data_read = read(fd, omxbuffer1->pBuffer, BUFFER_SIZE);
	omxbuffer1->nFilledLen = data_read;
	err = OMX_EmptyThisBuffer(handle, omxbuffer1);
	curr_buff = 1;
	
	/* Data processing loop */
	i = filesize/BUFFER_SIZE;
	while(--i){
		/* It's very important to mutual exclude this section.
		 * We dont wanna wakeup something which nobody is waiting on...
		 */
		pthread_mutex_lock(&appPriv->mutex);

		if(curr_buff == 2){
			data_read = read(fd, omxbuffer1->pBuffer, BUFFER_SIZE);
			if (data_read == 0)
				break;
			omxbuffer1->nFilledLen = data_read;
			err = OMX_EmptyThisBuffer(handle, omxbuffer1);
			curr_buff = 1;
		}
		else{
			data_read = read(fd, omxbuffer2->pBuffer, BUFFER_SIZE);
			if (data_read == 0)
				break;
			omxbuffer2->nFilledLen = data_read;
			err = OMX_EmptyThisBuffer(handle, omxbuffer2);
			curr_buff = 2;
		}
		
		DEBUG(DEB_LEV_FULL_SEQ, "Waiting for empty buffer to complete...\n");	
		pthread_cond_wait(&appPriv->condition, &appPriv->mutex);
		pthread_mutex_unlock(&appPriv->mutex);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer processed...\n");
	}
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	tsem_down(appPriv->eventSem);
	
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	
	err = OMX_FreeBuffer(handle, 0, omxbuffer1);
	err = OMX_FreeBuffer(handle, 0, omxbuffer2);
	
	tsem_down(appPriv->eventSem);
	
	OMX_FreeHandle(handle);

	free(appPriv->eventSem);
	free(appPriv);
	
	return 0;
}

/* Callbacks implementation */
OMX_ERRORTYPE alsasinkEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
	tsem_up(appPriv->eventSem);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasinkEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	pthread_mutex_lock(&appPriv->mutex);
	DEBUG(DEB_LEV_FULL_SEQ, "Notification for amoty buffer done %08X\n", (int)pBuffer);
	DEBUG(DEB_LEV_FULL_SEQ, "Waking up condition in %s", __func__);
	pthread_cond_signal(&appPriv->condition);
	pthread_mutex_unlock(&appPriv->mutex);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasinkFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	DEBUG(DEB_LEV_FULL_SEQ, "%s: Hmm... shouldn't be here...\n", __func__);
	return OMX_ErrorNotImplemented;
}

static int getFileSize(int fd)
{
	struct stat input_file_stat;
	int err;
	
	/* Obtain input file length */
	err = fstat(fd, &input_file_stat);
	if(err){
		DEBUG(DEB_LEV_ERR, "fstat failed");
		exit(-1);
	}
	return input_file_stat.st_size;
}
