/**
	@file test/components/ffmpeg/omxmp3dectest.c
	
	Test application that uses two OpenMAX components, an mp3 decoder and 
	an alsa sink. The application receives an mp3 stream in inupt, from a file
	or from standard input, decodes it and sends it to the alsa sink. 
	It is possible to specify the option -t that requests a tunnel between 
	OpenMAX components.
	
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
	
	2006/02/08:  Mp3 decoder and tunnel test version 0.1

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
#include <OMX_Video.h>

#include "omxmp3convertest.h"
#include "tsemaphore.h"

appPrivateType* appPriv;

unsigned int nextBuffer = 0;

OMX_BUFFERHEADERTYPE *inBuffer1, *inBuffer2, *outBuffer1, *outBuffer2, *inAlsaBuffer1, *inAlsaBuffer2;
int isFirstBuffer = 1;
int buffer_in_size = BUFFER_IN_SIZE;
//int buffer_out_size = BUFFER_OUT_SIZE * 2;
int buffer_out_size = BUFFER_OUT_SIZE;
OMX_PARAM_PORTDEFINITIONTYPE paramPort;
int fd = 0;

int main(int argc, char** argv){
	//	appPrivateType* appPriv;
	
	OMX_ERRORTYPE err;
	int isTunneled = 0;
	int data_read;
	
	OMX_CALLBACKTYPE mp3callbacks = { .EventHandler = mp3EventHandler,
											 .EmptyBufferDone = mp3EmptyBufferDone,
											 .FillBufferDone = mp3FillBufferDone
	};
	
	OMX_CALLBACKTYPE alsasinkcallbacks = { .EventHandler = alsasinkEventHandler,
											 .EmptyBufferDone = alsasinkEmptyBufferDone,
											 .FillBufferDone = NULL
	};

	if(argc < 2){
		DEBUG(DEB_LEV_ERR, "Usage: omx11audiodectest filename.mp3");
		exit(1);
	} else {
		fd = open(argv[1], O_RDONLY);
	}
	/* Initialize application private data */
	appPriv = malloc(sizeof(appPrivateType));
	
	appPriv->decoderEventSem = malloc(sizeof(tsem_t));
	appPriv->alsasinkEventSem = malloc(sizeof(tsem_t));
	appPriv->eofSem = malloc(sizeof(tsem_t));
	
	tsem_init(appPriv->decoderEventSem, 0);
	tsem_init(appPriv->alsasinkEventSem, 0);
	tsem_init(appPriv->eofSem, 0);

	err = OMX_Init();


	/** Ask the core for a handle to the dummy component
	 */
	err = OMX_GetHandle(&appPriv->mp3handle, "OMX.st.audio_decoder.mp3", NULL /*appPriv */, &mp3callbacks);
//	err = OMX_GetHandle(&appPriv->alsasinkhandle, "OMX.st.alsa.alsasink", NULL /*appPriv */, &alsasinkcallbacks);

	/** Set the number of ports for the dummy component
	 */
	
	paramPort.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	paramPort.nPortIndex = 0;
	err = OMX_GetParameter(appPriv->mp3handle, OMX_IndexParamPortDefinition, &paramPort);
	
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(appPriv->mp3handle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}
	if (!isTunneled) {
		paramPort.nPortIndex = 1;
		err = OMX_GetParameter(appPriv->mp3handle, OMX_IndexParamPortDefinition, &paramPort);
	
		paramPort.nBufferCountActual = 2;
		err = OMX_SetParameter(appPriv->mp3handle, OMX_IndexParamPortDefinition, &paramPort);
		if(err != OMX_ErrorNone){
			DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
			exit(1);
		}
		paramPort.nPortIndex = 0;
//		err = OMX_GetParameter(appPriv->alsasinkhandle, OMX_IndexParamPortDefinition, &paramPort);
	
		paramPort.nBufferCountActual = 2;
//		err = OMX_SetParameter(appPriv->alsasinkhandle, OMX_IndexParamPortDefinition, &paramPort);
		if(err != OMX_ErrorNone){
			DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
			exit(1);
		}
	}
	
	inBuffer1 = inBuffer2 = outBuffer1 = outBuffer2 = inAlsaBuffer1 = inAlsaBuffer2 = NULL;

//	if (isTunneled) {
//		err=OMX_SetupTunnel(appPriv->mp3handle, 1, appPriv->alsasinkhandle, 0);
//		if(err!=OMX_ErrorNone) 
//		{
//			DEBUG(DEB_LEV_ERR, "Setup Tunnel failed Error=%08x\n",err);
//		}
//	}
	
	err = OMX_SendCommand(appPriv->mp3handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
//	err = OMX_SendCommand(appPriv->alsasinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	
	err = OMX_AllocateBuffer(appPriv->mp3handle, &inBuffer1, 0, NULL, buffer_in_size);
	err = OMX_AllocateBuffer(appPriv->mp3handle, &inBuffer2, 0, NULL, buffer_in_size);
	
	if (!isTunneled) {
		err = OMX_AllocateBuffer(appPriv->mp3handle, &outBuffer1, 1, NULL, buffer_out_size);
		err = OMX_AllocateBuffer(appPriv->mp3handle, &outBuffer2, 1, NULL, buffer_out_size);
//		err = OMX_UseBuffer(appPriv->alsasinkhandle, &inAlsaBuffer1, 0, NULL, buffer_out_size, outBuffer1->pBuffer);
//		err = OMX_UseBuffer(appPriv->alsasinkhandle, &inAlsaBuffer2, 0, NULL, buffer_out_size, outBuffer2->pBuffer);
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
	tsem_down(appPriv->decoderEventSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "decoder Sem free\n");
//	tsem_down(appPriv->alsasinkEventSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "alsa sink Sem free\n");
	
	err = OMX_SendCommand(appPriv->mp3handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
//	err = OMX_SendCommand(appPriv->alsasinkhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	tsem_down(appPriv->decoderEventSem);
//	tsem_down(appPriv->alsasinkEventSem);
	if (!isTunneled){
		outBuffer1->nOutputPortIndex = 1;
		outBuffer1->nInputPortIndex = 0;
		outBuffer2->nOutputPortIndex = 1;
		outBuffer2->nInputPortIndex = 0;
		
		err = OMX_FillThisBuffer(appPriv->mp3handle, outBuffer1);
		err = OMX_FillThisBuffer(appPriv->mp3handle, outBuffer2);
	}
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and decoderMutex\n");

	
	data_read = read(fd, inBuffer1->pBuffer, buffer_in_size);
	inBuffer1->nFilledLen = data_read;
		
	data_read = read(fd, inBuffer2->pBuffer, buffer_in_size);
	inBuffer2->nFilledLen = data_read;

	DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", inBuffer1);
	err = OMX_EmptyThisBuffer(appPriv->mp3handle, inBuffer1);
	DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", inBuffer2);
	err = OMX_EmptyThisBuffer(appPriv->mp3handle, inBuffer2);

	tsem_down(appPriv->eofSem);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Stop mp3 dec\n");
	err = OMX_SendCommand(appPriv->mp3handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Stop alsa sink\n");
//	err = OMX_SendCommand(appPriv->alsasinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	
//	tsem_down(appPriv->alsasinkEventSem);
	tsem_down(appPriv->decoderEventSem);
	
	err = OMX_SendCommand(appPriv->mp3handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
//	err = OMX_SendCommand(appPriv->alsasinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	DEBUG(DEB_LEV_PARAMS, "alsa sink to loaded\n");
//	if (!isTunneled) {
//		err = OMX_FreeBuffer(appPriv->alsasinkhandle, 0, inAlsaBuffer1);
//		err = OMX_FreeBuffer(appPriv->alsasinkhandle, 0, inAlsaBuffer2);
//	}
	
	DEBUG(DEB_LEV_PARAMS, "Mp3 dec to loaded\n");
	
	err = OMX_FreeBuffer(appPriv->mp3handle, 0, inBuffer1);
	err = OMX_FreeBuffer(appPriv->mp3handle, 0, inBuffer2);
	if (!isTunneled) {
		DEBUG(DEB_LEV_PARAMS, "Free Mp3 dec output ports\n");
		err = OMX_FreeBuffer(appPriv->mp3handle, 1, outBuffer1);
		err = OMX_FreeBuffer(appPriv->mp3handle, 1, outBuffer2);
	}
	tsem_down(appPriv->decoderEventSem);
//	tsem_down(appPriv->alsasinkEventSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");
	
//	OMX_FreeHandle(appPriv->alsasinkhandle);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Alsa sink freed\n");
	OMX_FreeHandle(appPriv->mp3handle);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "mp3 dec freed\n");
	OMX_Deinit();
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "All components freed. Closing...\n");
	free(appPriv->decoderEventSem);
//	free(appPriv->alsasinkEventSem);
	free(appPriv->eofSem);
	free(appPriv);
	
	return 0;
}

/* Callbacks implementation */

OMX_ERRORTYPE mp3EventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
	if (Data1 == OMX_CommandStateSet) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "State changed in ");
		switch ((int)Data2) {
			case OMX_StateInvalid:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
				break;
			case OMX_StateLoaded:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
				break;
			case OMX_StateIdle:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
				break;
			case OMX_StateExecuting:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
				break;
			case OMX_StatePause:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
				break;
			case OMX_StateWaitForResources:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
				break;
		}
		
	} else {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
	}
	tsem_up(appPriv->decoderEventSem);
}

OMX_ERRORTYPE mp3EmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	int data_read;
	DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
	data_read = read(fd, pBuffer->pBuffer, buffer_in_size);
	if (data_read <= 0) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
		tsem_up(appPriv->eofSem);
		return OMX_ErrorNone;
	}
	pBuffer->nFilledLen = data_read;
	
	DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", pBuffer);
	err = OMX_EmptyThisBuffer(hComponent, pBuffer);

	return OMX_ErrorNone;
}

int accumulator = 0;
int total_size = 0;
int old_total_size = 0;

OMX_ERRORTYPE mp3FillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	int i;	
	OMX_STATETYPE eState;
	
	OMX_GetState(hComponent,&eState);

	/* Output data to alsa sink */
	if(pBuffer != NULL){
		if (pBuffer->nFilledLen == 0) {
			DEBUG(DEB_LEV_ERR, "Ouch! In %s: no data in the output buffer! Already done %i diff %i\n", __func__, total_size, (total_size - old_total_size));
			old_total_size = total_size;
			accumulator++;
			if (accumulator>20)
				return OMX_ErrorNone;
		} else {
			accumulator = 0;
			total_size += pBuffer->nFilledLen;
		}
		for(i=0;i<pBuffer->nFilledLen;i++){
			putchar(*(char*)(pBuffer->pBuffer + i));
		}
		pBuffer->nFilledLen = 0;
//		if(eState==OMX_StateExecuting || eState==OMX_StatePause)
//			err = OMX_EmptyThisBuffer(appPriv->alsasinkhandle, pBuffer);
	}
	else {
		DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
	}
	err = OMX_FillThisBuffer(hComponent, pBuffer);
}

OMX_ERRORTYPE alsasinkEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
	if (Data1 == OMX_CommandStateSet) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "State changed in ");
		switch ((int)Data2) {
			case OMX_StateInvalid:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
				break;
			case OMX_StateLoaded:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
				break;
			case OMX_StateIdle:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
				break;
			case OMX_StateExecuting:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
				break;
			case OMX_StatePause:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
				break;
			case OMX_StateWaitForResources:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
				break;
		}
		
	} else {
		DEBUG(DEB_LEV_PARAMS, "Param1 is %i\n", (int)Data1);
		DEBUG(DEB_LEV_PARAMS, "Param2 is %i\n", (int)Data2);
	}
	tsem_up(appPriv->alsasinkEventSem);
}

OMX_ERRORTYPE alsasinkEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	DEBUG(DEB_LEV_FULL_SEQ, "---> In %s \n" , __func__);
	DEBUG(DEB_LEV_FULL_SEQ, "Waking up condition in %s\n", __func__);
	pBuffer->nFilledLen = 0;
	err = OMX_FillThisBuffer(appPriv->mp3handle, pBuffer);
}


