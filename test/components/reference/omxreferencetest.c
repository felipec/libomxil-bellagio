/**
	@file test/components/reference/omxreferencetest.c
	
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
#include <user_debug_levels.h>

#include "omxreferencetest.h"
#include "tsemaphore.h"

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;
int fd = 0;
unsigned int filesize;
OMX_ERRORTYPE err;
OMX_HANDLETYPE handle;

int main(int argc, char** argv){
	//	appPrivateType* appPriv;
	OMX_CALLBACKTYPE callbacks = { .EventHandler = dummyEventHandler,
								 .EmptyBufferDone = dummyEmptyBufferDone,
								 .FillBufferDone = dummyFillBufferDone,
	};

	OMX_VERSIONTYPE componentVersion;
	OMX_VERSIONTYPE specVersion;
	OMX_UUIDTYPE uuid;
	char buffer[128];
	OMX_STRING name = buffer;
	OMX_PORT_PARAM_TYPE param;
	OMX_BUFFERHEADERTYPE *inBuffer1, *inBuffer2, *outBuffer1, *outBuffer2;
	int data_read1;
	int data_read2;
	OMX_PARAM_PORTDEFINITIONTYPE paramPort;

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
	appPriv->eofSem = malloc(sizeof(tsem_t));
	tsem_init(appPriv->eofSem, 0);
	
	err = OMX_Init();
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
		exit(1);
	}
	/** Ask the core for a handle to the dummy component
	 */
	err = OMX_GetHandle(&handle, "OMX.st.reference.name", NULL /*appPriv */, &callbacks);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "OMX_GetHandle failed\n");
		exit(1);
	}
	
	/* Get component information. Name, version, etc
	 */
	err = OMX_GetComponentVersion(handle,
		name,
		&componentVersion,
		&specVersion,
		&uuid);
	
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "OMX_GetComponentVersion failed\n");
		exit(1);
	}
	DEBUG(DEB_LEV_PARAMS, "Component name is %s\n", name);
	DEBUG(DEB_LEV_PARAMS, "Spec      version is %i.%i.%i.%i\n",
		specVersion.s.nVersionMajor,
		specVersion.s.nVersionMinor,
		specVersion.s.nRevision,
		specVersion.s.nStep);
	DEBUG(DEB_LEV_PARAMS, "Component version is %i.%i.%i.%i\n",
		componentVersion.s.nVersionMajor,
		componentVersion.s.nVersionMinor,
		componentVersion.s.nRevision,
		componentVersion.s.nStep);

	/** Set the number of ports for the parameter structure
	 */
	param.nPorts = 2;
	err = OMX_GetParameter(handle, OMX_IndexParamAudioInit, &param);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}

	paramPort.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	paramPort.nPortIndex = 0;
	err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &paramPort);
	
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}
	paramPort.nPortIndex = 1;
	err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &paramPort);
	
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}

	
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

	inBuffer1 = inBuffer2 = outBuffer1 = outBuffer2 = NULL;
	err = OMX_AllocateBuffer(handle, &inBuffer1, 0, NULL, BUFFER_IN_SIZE);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 1%i\n", err);
		exit(1);
	}
	err = OMX_AllocateBuffer(handle, &inBuffer2, 0, NULL, BUFFER_IN_SIZE);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 2 %i\n", err);
		exit(1);
	}
	err = OMX_AllocateBuffer(handle, &outBuffer1, 1, NULL, BUFFER_IN_SIZE);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 1 %i\n", err);
		exit(1);
	}
	err = OMX_AllocateBuffer(handle, &outBuffer2, 1, NULL, BUFFER_IN_SIZE);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 2 %i\n", err);
		exit(1);
	}
	
	tsem_down(appPriv->eventSem);
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

	/* Wait for commands to complete */
	tsem_down(appPriv->eventSem);
	
	DEBUG(DEB_LEV_PARAMS, "Had buffers at:\n0x%08x\n0x%08x\n0x%08x\n0x%08x\n", (int)inBuffer1->pBuffer,
		(int)inBuffer2->pBuffer,
		(int)outBuffer1->pBuffer,
		(int)outBuffer2->pBuffer);
	DEBUG(DEB_LEV_PARAMS, "After switch to executing\n");
	/* Schedule a couple of buffers to be filled on the output port
	 * The callback itself will re-schedule them.
	 */
	err = OMX_FillThisBuffer(handle, outBuffer1);
	err = OMX_FillThisBuffer(handle, outBuffer2);
	
	data_read1 = read(fd, inBuffer1->pBuffer, BUFFER_IN_SIZE);
	inBuffer1->nFilledLen = data_read1;
	filesize -= data_read1;
		
	data_read2 = read(fd, inBuffer2->pBuffer, BUFFER_IN_SIZE);
	inBuffer2->nFilledLen = data_read2;
	filesize -= data_read2;

	DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", inBuffer1);
	err = OMX_EmptyThisBuffer(handle, inBuffer1);
	DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", inBuffer2);
	err = OMX_EmptyThisBuffer(handle, inBuffer2);

	tsem_down(appPriv->eofSem);

	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	/* Wait for commands to complete */
	tsem_down(appPriv->eventSem);
	
	err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	err = OMX_FreeBuffer(handle, 0, inBuffer1);
	err = OMX_FreeBuffer(handle, 0, inBuffer2);
	err = OMX_FreeBuffer(handle, 1, outBuffer1);
	err = OMX_FreeBuffer(handle, 1, outBuffer2);

	/* Wait for commands to complete */
	tsem_down(appPriv->eventSem);
	
	OMX_FreeHandle(handle);

	free(appPriv->eventSem);
	free(appPriv);
	
	return 0;
}

/* Callbacks implementation */
OMX_ERRORTYPE dummyEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_IN OMX_PTR pEventData)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
	DEBUG(DEB_LEV_PARAMS, "Param1 is %i\n", (int)Data1);
	DEBUG(DEB_LEV_PARAMS, "Param2 is %i\n", (int)Data2);
	tsem_up(appPriv->eventSem);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE dummyEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	int data_read;
	DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
	data_read = read(fd, pBuffer->pBuffer, BUFFER_IN_SIZE);
	if (data_read <= 0) {
		tsem_up(appPriv->eofSem);
		return OMX_ErrorNone;
	}
	pBuffer->nFilledLen = data_read;
	filesize -= data_read;
	
	DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", pBuffer);
	err = OMX_EmptyThisBuffer(handle, pBuffer);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE dummyFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	int i;	

	DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback. Got buflen %i for buffer at 0x%08x\n",
		__func__, (int)pBuffer->nFilledLen, (int)pBuffer);

	/* Output data to standard output */
	if(pBuffer != NULL){
		if (pBuffer->nFilledLen == 0) {
			DEBUG(DEB_LEV_ERR, "Ouch! In %s: no data in the output buffer!\n", __func__);
			return OMX_ErrorNone;
		}
		for(i=0;i<pBuffer->nFilledLen;i++){
			putchar(*(char*)(pBuffer->pBuffer + i));
		}
		pBuffer->nFilledLen = 0;
	}
	else
		DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);

	/* Reschedule the fill buffer request */
	err = OMX_FillThisBuffer(hComponent, pBuffer);
	return OMX_ErrorNone;
}

/** Gets the file descriptor's size
 * @return the size of the file. If size cannot be computed
 * (i.e. stdin, zero is returned)
 */
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
