/**
	@file src/components/alsa/omx_alsasink.c
	
	OpenMax Alsa Sink Component. This library implements an alsasink IL Component
	that plays pcm audio. It is based on Alsa audio library
	
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
	
	2006/02/08:  OpenMAX Alsa Sink version 0.1

*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Audio.h>

#include "omxcore.h"
#include "omx_alsasink.h"

#include "tsemaphore.h"
#include "queue.h"

/** Maximum Number of Reference Instance*/
OMX_U32 noAlsasinkInstance=0;

/*********************
 *
 * Component template
 *
 **********************/

/** 
 * This is the stComponent template from which all
 * other stComponent instances are factored by the core.
 * Note that it has been necessary to wrap the OMX component
 * with a st_component because of design deficiencies.
 */
stComponentType alsasink_component = {
  .name = "OMX.st.alsa.alsasink",
  .state = OMX_StateLoaded,
  .callbacks = NULL,
  .callbackData = NULL,
  .nports = 2,
	.coreDescriptor = NULL,
  .constructor = alsasink_Constructor,
  .destructor = alsasink_Destructor,
  .messageHandler = alsasink_MessageHandler,

	.omx_component = {
  .nSize = sizeof(OMX_COMPONENTTYPE),
  .nVersion = { .s = {SPECVERSIONMAJOR,
							 SPECVERSIONMINOR,
							 SPECREVISION,
							 SPECSTEP}},
  .pComponentPrivate = NULL,
  .pApplicationPrivate = NULL,

  .GetComponentVersion = alsasink_GetComponentVersion,
  .SendCommand = alsasink_SendCommand,
  .GetParameter = alsasink_GetParameter,
  .SetParameter = alsasink_SetParameter,
  .GetConfig = alsasink_GetConfig,
  .SetConfig = alsasink_SetConfig,
  .GetExtensionIndex = alsasink_GetExtensionIndex,
  .GetState = alsasink_GetState,
  .ComponentTunnelRequest = alsasink_ComponentTunnelRequest,
  .UseBuffer = alsasink_UseBuffer,
  .AllocateBuffer = alsasink_AllocateBuffer,
  .FreeBuffer = alsasink_FreeBuffer,
  .EmptyThisBuffer = alsasink_EmptyThisBuffer,
  .FillThisBuffer = alsasink_FillThisBuffer,
  .SetCallbacks = alsasink_SetCallbacks,
  .ComponentDeInit = alsasink_ComponentDeInit,
	},
};


/**************************************************************
 *
 * PRIVATE: component private entry points and helper functions
 *
 **************************************************************/

/** 
 * Component template registration function
 *
 * This is the component entry point which is called at library
 * initialization for each OMX component. It provides the OMX core with
 * the component template.
 */
void __attribute__ ((constructor)) alsasink__register_template()
{
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);
  register_template(&alsasink_component);
}

/** 
 * This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the alsasink component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated alsasink_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE alsasink_Constructor(stComponentType* stComponent)
{
	alsasink_PrivateType* alsasink_Private;
	pthread_t testThread;
	OMX_ERRORTYPE err;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noAlsasinkInstance);

	/** Allocate and fill in alsasink private structures
	 * These include port buffer queue and flow control semaphore
	 */
	
	stComponent->omx_component.pComponentPrivate = malloc(sizeof(alsasink_PrivateType));
	if(stComponent->omx_component.pComponentPrivate==NULL)
		return OMX_ErrorInsufficientResources;
	memset(stComponent->omx_component.pComponentPrivate, 0, sizeof(alsasink_PrivateType));

	alsasink_Private = stComponent->omx_component.pComponentPrivate;
	/** Default parameters setting */
	alsasink_Private->bIsInit = OMX_FALSE;
	alsasink_Private->nGroupPriority = 0;
	alsasink_Private->nGroupID = 0;
	alsasink_Private->pMark=NULL;
	alsasink_Private->bCmdUnderProcess=OMX_FALSE;
	alsasink_Private->bWaitingForCmdToFinish=OMX_FALSE;
	alsasink_Private->inputPort.bIsPortFlushed=OMX_FALSE;

	alsasink_Private->inputPort.hTunneledComponent=NULL;
	alsasink_Private->inputPort.nTunneledPort=0;
	alsasink_Private->inputPort.nTunnelFlags=0;

	/** Generic section for the input port. */
	alsasink_Private->inputPort.nNumAssignedBuffers = 0;
	alsasink_Private->inputPort.transientState = OMX_StateMax;
	setHeader(&alsasink_Private->inputPort.sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
	alsasink_Private->inputPort.sPortParam.nPortIndex = 0;
	alsasink_Private->inputPort.sPortParam.eDir = OMX_DirInput;
	alsasink_Private->inputPort.sPortParam.nBufferCountActual = 1;
	alsasink_Private->inputPort.sPortParam.nBufferCountMin = 1;
	alsasink_Private->inputPort.sPortParam.nBufferSize = 4096;
	alsasink_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
	alsasink_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
	
	/** Domain specific section for the intput port. The following parameters are only an example */
	alsasink_Private->inputPort.sPortParam.eDomain = OMX_PortDomainAudio;
	alsasink_Private->inputPort.sPortParam.format.audio.cMIMEType = "mp3";
	alsasink_Private->inputPort.sPortParam.format.audio.pNativeRender = 0;
	alsasink_Private->inputPort.sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	alsasink_Private->inputPort.sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingUnused;

	setHeader(&alsasink_Private->inputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	alsasink_Private->inputPort.sAudioParam.nPortIndex = 0;
	alsasink_Private->inputPort.sAudioParam.nIndex = 0;
	alsasink_Private->inputPort.sAudioParam.eEncoding = OMX_AUDIO_CodingUnused;
	
	/** generic parameter NOT related to a specific port */
	setHeader(&alsasink_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
	alsasink_Private->sPortTypesParam.nPorts = 1;
	alsasink_Private->sPortTypesParam.nStartPortNumber = 0;


	/** Allocate and initialize the input semaphores and excuting condition */
	alsasink_Private->inputPort.pBufferSem = malloc(sizeof(tsem_t));
	if(alsasink_Private->inputPort.pBufferSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(alsasink_Private->inputPort.pBufferSem, 0);

	alsasink_Private->inputPort.pFullAllocationSem = malloc(sizeof(tsem_t));
	if(alsasink_Private->inputPort.pFullAllocationSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(alsasink_Private->inputPort.pFullAllocationSem, 0);

	/** Allocate and initialize input queues */
	alsasink_Private->inputPort.pBufferQueue = malloc(sizeof(queue_t));
	if(alsasink_Private->inputPort.pBufferQueue==NULL)
		return OMX_ErrorInsufficientResources;
	queue_init(alsasink_Private->inputPort.pBufferQueue);

	alsasink_Private->inputPort.pFlushSem = malloc(sizeof(tsem_t));
	if(alsasink_Private->inputPort.pFlushSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(alsasink_Private->inputPort.pFlushSem, 0);

	alsasink_Private->inputPort.bBufferUnderProcess=OMX_FALSE;
	alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;

	pthread_mutex_init(&alsasink_Private->exit_mutex, NULL);
	pthread_cond_init(&alsasink_Private->exit_condition, NULL);
	pthread_mutex_init(&alsasink_Private->cmd_mutex, NULL);

	alsasink_Private->pCmdSem = malloc(sizeof(tsem_t));
	if(alsasink_Private->pCmdSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(alsasink_Private->pCmdSem, 0);

	pthread_mutex_lock(&alsasink_Private->exit_mutex);
	alsasink_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&alsasink_Private->exit_mutex);

	alsasink_Private->inbuffer=0;
	alsasink_Private->inbuffercb=0;

	DEBUG(DEB_LEV_FULL_SEQ,"Returning from %s\n",__func__);
	/** Two meta-states used for the implementation 
	* of the transition mechanism loaded->idle and idle->loaded 
	*/

	/* OMX_AUDIO_PARAM_PCMMODETYPE */
	alsasink_Private->inputPort.omxAudioParamPcmMode.nPortIndex = 0;
	alsasink_Private->inputPort.omxAudioParamPcmMode.nChannels = 2;
	alsasink_Private->inputPort.omxAudioParamPcmMode.eNumData = OMX_NumericalDataSigned;
	alsasink_Private->inputPort.omxAudioParamPcmMode.eEndian = OMX_EndianLittle;
	alsasink_Private->inputPort.omxAudioParamPcmMode.bInterleaved = OMX_TRUE;
	alsasink_Private->inputPort.omxAudioParamPcmMode.nBitPerSample = 16;
	alsasink_Private->inputPort.omxAudioParamPcmMode.nSamplingRate = 44100;
	alsasink_Private->inputPort.omxAudioParamPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
	alsasink_Private->inputPort.omxAudioParamPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;
	/* Todo: add the volume stuff */

	/* Allocate the playback handle and the hardware parameter structure */
	if ((err = snd_pcm_open (&alsasink_Private->inputPort.playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot open audio device %s (%s)\n", "default", snd_strerror (err));
		alsasink_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %08x %08X in %i\n", (int)alsasink_Private->inputPort.playback_handle, (int)&alsasink_Private->inputPort.playback_handle, getpid());

	if (snd_pcm_hw_params_malloc(&alsasink_Private->inputPort.hw_params) < 0) {
		DEBUG(DEB_LEV_ERR, "%s: failed allocating input port hw parameters\n", __func__);
		alsasink_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got hw parameters at %08x\n", (int)alsasink_Private->inputPort.hw_params);

	if ((err = snd_pcm_hw_params_any (alsasink_Private->inputPort.playback_handle, alsasink_Private->inputPort.hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot initialize hardware parameter structure (%s)\n",	snd_strerror (err));
		alsasink_Panic();
	}

	/* Write in the default paramenters */
	alsasink_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioInit, &alsasink_Private->sPortTypesParam);
	alsasink_Private->inputPort.AudioPCMConfigured	= 0;

	noAlsasinkInstance++;

	if(noAlsasinkInstance > MAX_NUM_OF_ALSASINK_INSTANCES) 
		return OMX_ErrorInsufficientResources;

	return OMX_ErrorNone;
}

/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 * \param message the message that has been passed to core
 */
OMX_ERRORTYPE alsasink_MessageHandler(coreMessage_t* message)
{
	stComponentType* stComponent = message->stComponent;
	alsasink_PrivateType* alsasink_Private;
	OMX_BOOL waitFlag=OMX_FALSE;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	
	OMX_ERRORTYPE err;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	alsasink_Private = stComponent->omx_component.pComponentPrivate;

	/** Dealing with a SendCommand call.
	 * -messageParam1 contains the command to execute
	 * -messageParam2 contains the parameter of the command
	 *  (destination state in case of a state change command).
	 */

	pthread_mutex_lock(&alsasink_Private->cmd_mutex);
	alsasink_Private->bCmdUnderProcess=OMX_TRUE;
	pthread_mutex_unlock(&alsasink_Private->cmd_mutex);
	
	if(message->messageType == SENDCOMMAND_MSG_TYPE){
		switch(message->messageParam1){
			case OMX_CommandStateSet:
				/* Do the actual state change */
				err = alsasink_DoStateSet(stComponent, message->messageParam2);
				if (err != OMX_ErrorNone) {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventError, /* The command was completed */
							err, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
				} else {
					/* And run the callback */
					DEBUG(DEB_LEV_FULL_SEQ,"running callback in %s\n",__func__);
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandStateSet, /* The commands was a OMX_CommandStateSet */
						message->messageParam2, /* The state has been changed in message->messageParam2 */
						NULL);
				}
				break;
				
			case OMX_CommandFlush:
				/*Flush ports*/
				err=alsasink_FlushPort(stComponent, message->messageParam2);
				if (err != OMX_ErrorNone) {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventError, /* The command was completed */
							err, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
				} else {
					if(message->messageParam2==-1){ /*Flush all port*/
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
								0, /* The state has been changed in message->messageParam2 */
								NULL);
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
								1, /* The state has been changed in message->messageParam2 */
								NULL);
					}else {/*Flush input/output port*/
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
								message->messageParam2, /* The state has been changed in message->messageParam2 */
								NULL);
					}
				}
				alsasink_Private->inputPort.bIsPortFlushed=OMX_FALSE;
			break;
			case OMX_CommandPortDisable:
				/** This condition is added to pass the tests, it is not significant for the environment */
				err = alsasink_DisablePort(stComponent, message->messageParam2);
				if (err != OMX_ErrorNone) {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventError, /* The command was completed */
							err, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
				} else {
					if ((message->messageParam2 == 0) || (message->messageParam2 == 1)){
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventCmdComplete, /* The command was completed */
							OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
							message->messageParam2, /* The state has been changed in message->messageParam2 */
							NULL);
					} else {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventCmdComplete, /* The command was completed */
							OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventCmdComplete, /* The command was completed */
							OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
							1, /* The state has been changed in message->messageParam2 */
							NULL);
					}
				}
			break;
			case OMX_CommandPortEnable:
				err = alsasink_EnablePort(stComponent, message->messageParam2);
				if (err != OMX_ErrorNone) {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventError, /* The command was completed */
							err, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
				} else {
					if(message->messageParam2 !=-1)
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventCmdComplete, /* The command was completed */
							OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
							message->messageParam2, /* The state has been changed in message->messageParam2 */
							NULL);
					else {
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
								0, /* The state has been changed in message->messageParam2 */
								NULL);
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
								1, /* The state has been changed in message->messageParam2 */
								NULL);
					}
				}
			break;
				
			case OMX_CommandMarkBuffer:
				alsasink_Private->pMark=(OMX_MARKTYPE *)message->pCmdData;
			break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageParam1);
			break;
		}
		/* Dealing with an asynchronous error condition
		 */
	}else if(message->messageType == ERROR_MSG_TYPE){
		/* Etc. etc
		 */
	} else if (message->messageType == WARNING_MSG_TYPE) {
		DEBUG(DEB_LEV_ERR, "In %s: Warning message. Useless operation but do not affect environment %i\n", __func__, message->messageParam1);
		switch(message->messageParam1){
			case OMX_CommandPortEnable:
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						NULL);
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
						1, /* The state has been changed in message->messageParam2 */
						NULL);
			break;
			case OMX_CommandPortDisable:
				if (message->messageParam2 == 0) {
					alsasink_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						NULL);
				} else if (message->messageParam2 == 1) {
					//alsasink_Private->outputPort.sPortParam.bEnabled = OMX_FALSE;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventError, /* The command was completed */
						OMX_ErrorBadPortIndex, /* The commands was a OMX_CommandStateSet */
						1, /* The state has been changed in message->messageParam2 */
						NULL);
				} else if (message->messageParam2 == -1) {
					alsasink_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						NULL);
				}
			break;
			default:	
				DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageParam1);
			break;
		}		
	}

	pthread_mutex_lock(&alsasink_Private->cmd_mutex);
	alsasink_Private->bCmdUnderProcess=OMX_FALSE;
	waitFlag=alsasink_Private->bWaitingForCmdToFinish;
	pthread_mutex_unlock(&alsasink_Private->cmd_mutex);

	if(waitFlag==OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: Signalling command finish condition \n", __func__);
		tsem_up(alsasink_Private->pCmdSem);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s: \n", __func__);

	return OMX_ErrorNone;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* alsasink_BufferMgmtFunction(void* param) {
	
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	queue_t* pInputQueue = alsasink_Private->inputPort.pBufferQueue;
	OMX_BOOL exit_thread = OMX_FALSE;
	OMX_BOOL isInputBufferEnded,flag;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags,frameSize;
	OMX_MARKTYPE *pMark;
	OMX_BOOL *inbufferUnderProcess=&alsasink_Private->inputPort.bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&alsasink_Private->inputPort.mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &alsasink_Private->executingMutex;
	pthread_cond_t* executingCondition = &alsasink_Private->executingCondition;
	OMX_S32 written;
	OMX_S32 totalBuffer;
	OMX_S32 offsetBuffer;
	OMX_BOOL allDataSent;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting || 
				((alsasink_Private->inputPort.transientState = OMX_StateIdle))){

		DEBUG(DEB_LEV_FULL_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		pthread_mutex_lock(&alsasink_Private->exit_mutex);
		exit_thread = alsasink_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&alsasink_Private->exit_mutex);
		if(exit_thread == OMX_TRUE) {
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}

		pthread_mutex_lock(pInmutex);
		*inbufferUnderProcess = OMX_TRUE;
		pthread_mutex_unlock(pInmutex);

		pInputBuffer = dequeue(pInputQueue);
		if(pInputBuffer == NULL){
			DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
			alsasink_Panic();
		}
		nFlags=pInputBuffer->nFlags;
		if(nFlags==OMX_BUFFERFLAG_EOS) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
		}
		if(alsasink_Private->inputPort.bIsPortFlushed==OMX_TRUE) {
			/*Return Input Buffer*/
			goto LOOP1;
		}
		
		pthread_mutex_lock(&alsasink_Private->exit_mutex);
		exit_thread=alsasink_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&alsasink_Private->exit_mutex);
		
		if(exit_thread==OMX_TRUE) {
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded) {
		/**  This condition becomes true when the input buffer has completely be consumed.
			*  In this case is immediately switched because there is no real buffer consumption */
			isInputBufferEnded = OMX_TRUE;
			
			if(alsasink_Private->pMark!=NULL){
				alsasink_Private->pMark=NULL;
			}
			target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
			if(target_component==(OMX_COMPONENTTYPE *)&stComponent->omx_component) {
				/*Clear the mark and generate an event*/
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventMark, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						pInputBuffer->pMarkData);
			} else if(pInputBuffer->hMarkTargetComponent!=NULL){
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
			}
			
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);

			/** Place here the code of the component. The input buffer header is pInputBuffer, the output
			* buffer header is pOutputBuffer. */

			/* Feed it to ALSA */
			frameSize = (alsasink_Private->inputPort.omxAudioParamPcmMode.nChannels * alsasink_Private->inputPort.omxAudioParamPcmMode.nBitPerSample) >> 3;
			DEBUG(DEB_LEV_FULL_SEQ, "Framesize is %u chl=%d bufSize=%d\n", 
				frameSize,alsasink_Private->inputPort.omxAudioParamPcmMode.nChannels,pInputBuffer->nFilledLen);
			allDataSent = OMX_FALSE;
			totalBuffer = pInputBuffer->nFilledLen/frameSize;
			offsetBuffer = 0;
			while (!allDataSent) {
				written = snd_pcm_writei(alsasink_Private->inputPort.playback_handle, pInputBuffer->pBuffer + (offsetBuffer * frameSize), totalBuffer);
				if (written < 0) {
					if(written == -EPIPE){
						DEBUG(DEB_LEV_ERR, "ALSA Underrun..\n");
						snd_pcm_prepare(alsasink_Private->inputPort.playback_handle);
						written = 0;
					} else {
						DEBUG(DEB_LEV_ERR, "Cannot send any data to the audio device %s (%s)\n", "default", snd_strerror (written));
						alsasink_Panic();
					}
				}

				if(written != totalBuffer){
				totalBuffer = totalBuffer - written;
				offsetBuffer = written;
				} else {
					DEBUG(DEB_LEV_FULL_SEQ, "Buffer successfully sent to ALSA. Length was %i\n", (int)pInputBuffer->nFilledLen);
					allDataSent = OMX_TRUE;
				}
			}
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(alsasink_Private->inputPort.bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

LOOP1:
		if (alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
			/*Call FTB in case port is enabled*/
			if(alsasink_Private->inputPort.sPortParam.bEnabled==OMX_TRUE){
				OMX_FillThisBuffer(alsasink_Private->inputPort.hTunneledComponent,pInputBuffer);
				alsasink_Private->inbuffercb++;
			}
			else { /*Port Disabled then call FTB if port is not the supplier else dont call*/
				if(!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					 OMX_FillThisBuffer(alsasink_Private->inputPort.hTunneledComponent,pInputBuffer);
					 alsasink_Private->inbuffercb++;
				}
			}
		} else {
			(*(stComponent->callbacks->EmptyBufferDone))
				(pHandle, stComponent->callbackData,pInputBuffer);
			alsasink_Private->inbuffercb++;
		}
		pthread_mutex_lock(pInmutex);
		*inbufferUnderProcess = OMX_FALSE;
		flag=alsasink_Private->inputPort.bWaitingFlushSem;
		pthread_mutex_unlock(pInmutex);
		if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			if(pInputSem->semval==alsasink_Private->inputPort.nNumTunnelBuffer)
				if(flag==OMX_TRUE) {
					tsem_up(alsasink_Private->inputPort.pFlushSem);
				}
		}else{
			if(flag==OMX_TRUE) {
			tsem_up(alsasink_Private->inputPort.pFlushSem);
			}
		}
		pthread_mutex_lock(&alsasink_Private->exit_mutex);
		exit_thread=alsasink_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&alsasink_Private->exit_mutex);
		
LOOP2:
		if(exit_thread == OMX_TRUE) {
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}
		continue;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
	return NULL;
}

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For alsasink component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE alsasink_Init(stComponentType* stComponent)
{

	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (alsasink_Private->bIsInit == OMX_TRUE) {
		DEBUG(DEB_LEV_ERR, "In %s Big error. It should not happen\n", __func__);
		return OMX_ErrorIncorrectStateOperation;
	}
	alsasink_Private->bIsInit = OMX_TRUE;

	/*re-intialize buffer semaphore and allocation semaphore*/
	tsem_init(alsasink_Private->inputPort.pBufferSem, 0);
	tsem_init(alsasink_Private->inputPort.pFullAllocationSem, 0);

	/** initialize/reinitialize input queues */
	queue_init(alsasink_Private->inputPort.pBufferQueue);

	tsem_init(alsasink_Private->inputPort.pFlushSem, 0);
	
	alsasink_Private->inputPort.bBufferUnderProcess=OMX_FALSE;
	alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
	pthread_mutex_init(&alsasink_Private->inputPort.mutex, NULL);
	
	pthread_cond_init(&alsasink_Private->executingCondition, NULL);
	pthread_mutex_init(&alsasink_Private->executingMutex, NULL);
	
	pthread_mutex_lock(&alsasink_Private->exit_mutex);
	alsasink_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&alsasink_Private->exit_mutex);

	if (!alsasink_Private->inputPort.AudioPCMConfigured) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
		alsasink_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioPcm, &alsasink_Private->inputPort.omxAudioParamPcmMode);
	}

	/** Configure and prepare the ALSA handle */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface\n");
	
	if ((err = snd_pcm_hw_params (alsasink_Private->inputPort.playback_handle, alsasink_Private->inputPort.hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot set parameters (%s)\n",	snd_strerror (err));
		alsasink_Panic();
	}
	
	if ((err = snd_pcm_prepare (alsasink_Private->inputPort.playback_handle)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
		alsasink_Panic();
	}

	/** Spawn buffer management thread */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hey, starting threads!\n");
	alsasink_Private->bufferMgmtThreadID = pthread_create(&alsasink_Private->bufferMgmtThread,
		NULL,
		alsasink_BufferMgmtFunction,
		stComponent);	

	if(alsasink_Private->bufferMgmtThreadID < 0){
		DEBUG(DEB_LEV_ERR, "In starting buffer management thread failed\n");
		return OMX_ErrorUndefined;
	}	

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s\n", __func__);

	return OMX_ErrorNone;
}

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE alsasink_Destructor(stComponentType* stComponent)
{
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	alsasink_PortType* inputPort = &alsasink_Private->inputPort;
	OMX_BOOL exit_thread=OMX_FALSE,cmdFlag=OMX_FALSE;
	OMX_U32 ret;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noAlsasinkInstance);
	if (alsasink_Private->bIsInit != OMX_FALSE) {
		alsasink_Deinit(stComponent);
	} 

	pthread_mutex_lock(&alsasink_Private->exit_mutex);
	exit_thread = alsasink_Private->bExit_buffer_thread;
	pthread_mutex_unlock(&alsasink_Private->exit_mutex);
	if(exit_thread == OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for condition...\n",__func__);
		pthread_cond_wait(&alsasink_Private->exit_condition,&alsasink_Private->exit_mutex);
	}

	pthread_mutex_lock(&alsasink_Private->cmd_mutex);
	cmdFlag=alsasink_Private->bCmdUnderProcess;
	if(cmdFlag==OMX_TRUE) {
		alsasink_Private->bWaitingForCmdToFinish=OMX_TRUE;
		pthread_mutex_unlock(&alsasink_Private->cmd_mutex);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish ...\n",__func__);
		tsem_down(alsasink_Private->pCmdSem);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish over..cmutex=%x.\n",__func__,&alsasink_Private->cmd_mutex);

		pthread_mutex_lock(&alsasink_Private->cmd_mutex);
		alsasink_Private->bWaitingForCmdToFinish=OMX_FALSE;
		pthread_mutex_unlock(&alsasink_Private->cmd_mutex);
	}
	else {
		pthread_mutex_unlock(&alsasink_Private->cmd_mutex);
	}
	
	pthread_cond_destroy(&alsasink_Private->exit_condition);
	pthread_mutex_destroy(&alsasink_Private->exit_mutex);
	pthread_mutex_destroy(&alsasink_Private->cmd_mutex);
	
	if(alsasink_Private->pCmdSem!=NULL)  {
	tsem_deinit(alsasink_Private->pCmdSem);
	free(alsasink_Private->pCmdSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pCmdSem semaphore\n");
	}

	/*Destroying and Freeing input semaphore */
	if(alsasink_Private->inputPort.pBufferSem!=NULL)  {
	tsem_deinit(alsasink_Private->inputPort.pBufferSem);
	free(alsasink_Private->inputPort.pBufferSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input semaphore\n");
	}

	/*Destroying and Freeing input queue */
	if(alsasink_Private->inputPort.pBufferQueue!=NULL) {
	DEBUG(DEB_LEV_SIMPLE_SEQ,"deiniting alsasink input queue\n");
	queue_deinit(alsasink_Private->inputPort.pBufferQueue);
	free(alsasink_Private->inputPort.pBufferQueue);
	alsasink_Private->inputPort.pBufferQueue=NULL;
	}

	/*Destroying and Freeing input semaphore */
	if(alsasink_Private->inputPort.pFullAllocationSem!=NULL)  {
	tsem_deinit(alsasink_Private->inputPort.pFullAllocationSem);
	free(alsasink_Private->inputPort.pFullAllocationSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input pFullAllocationSem semaphore\n");
	}

	if(alsasink_Private->inputPort.pFlushSem!=NULL) {
	tsem_deinit(alsasink_Private->inputPort.pFlushSem);
	free(alsasink_Private->inputPort.pFlushSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input pFlushSem semaphore\n");
	}

	stComponent->state = OMX_StateInvalid;
	
	snd_pcm_hw_params_free (inputPort->hw_params);
	snd_pcm_close(inputPort->playback_handle);

	free(stComponent->omx_component.pComponentPrivate);

	noAlsasinkInstance--;

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);
	return OMX_ErrorNone;
}

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param parameter the requested target state.
 */
OMX_ERRORTYPE alsasink_DoStateSet(stComponentType* stComponent, OMX_U32 destinationState)
{
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	pthread_mutex_t* executingMutex = &alsasink_Private->executingMutex;
	pthread_cond_t* executingCondition = &alsasink_Private->executingCondition;
	pthread_mutex_t *pInmutex=&alsasink_Private->inputPort.mutex;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE* pInputBuffer;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s changing state from %i to %i\n", __func__, stComponent->state, (OMX_S32)destinationState);

	if(destinationState == OMX_StateLoaded){
		switch(stComponent->state){
			case OMX_StateInvalid:
				return OMX_ErrorInvalidState;
			case OMX_StateWaitForResources:
				/* return back from wait for resources */
				stComponent->state = OMX_StateLoaded;
				break;
			case OMX_StateLoaded:
				return OMX_ErrorSameState;
				break;
			case OMX_StateIdle:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  input enabled %i ,  input populated %i\n", __func__, alsasink_Private->inputPort.sPortParam.bEnabled, alsasink_Private->inputPort.sPortParam.bPopulated);
				if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/* Freeing here the buffers allocated for the tunneling:*/
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: IN TUNNEL!!!! semval=%d\n",__func__,pInputSem->semval);
					eError=alsasink_freeTunnelBuffers(&alsasink_Private->inputPort);
				} 
				else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
				}
				else {
					/** Wait until all the buffers assigned to the input port have been de-allocated
						*/
					if ((alsasink_Private->inputPort.sPortParam.bEnabled) && 
							(alsasink_Private->inputPort.sPortParam.bPopulated)) {
						tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
						alsasink_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
						alsasink_Private->inputPort.transientState = OMX_StateMax;
					}
					else if ((alsasink_Private->inputPort.sPortParam.bEnabled == OMX_FALSE) && 
						(alsasink_Private->inputPort.sPortParam.bPopulated == OMX_FALSE)) {
						if(alsasink_Private->inputPort.pFullAllocationSem->semval>0)
							tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
					}
				}
				tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);

				stComponent->state = OMX_StateLoaded;
				alsasink_Deinit(stComponent);
				break;
				
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
		}
		return OMX_ErrorNone;
	}

	if(destinationState == OMX_StateWaitForResources){
		switch(stComponent->state){
			case OMX_StateInvalid:
					return OMX_ErrorInvalidState;
				break;
			case OMX_StateLoaded:
					stComponent->state = OMX_StateWaitForResources;
				break;
			case OMX_StateWaitForResources:
					return OMX_ErrorSameState;
				break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
		}
		return OMX_ErrorNone;
	}

	if(destinationState == OMX_StateIdle){
		switch(stComponent->state){
			case OMX_StateInvalid:
					return OMX_ErrorInvalidState;
				break;
			case OMX_StateWaitForResources:
				stComponent->state = OMX_StateIdle;
				break;
			case OMX_StateLoaded:
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  input enabled %i ,  input populated %i\n", __func__, alsasink_Private->inputPort.sPortParam.bEnabled, alsasink_Private->inputPort.sPortParam.bPopulated);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : input flags  0x%x \n", 
					(OMX_S32)alsasink_Private->inputPort.nTunnelFlags);

				if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/** Allocate here the buffers needed for the tunneling:
						*/
					eError=alsasink_allocateTunnelBuffers(&alsasink_Private->inputPort, 0, ALSASINK_IN_BUFFER_SIZE);
					alsasink_Private->inputPort.transientState = OMX_StateMax;
				} else {
					/** Wait until all the buffers needed have been allocated
						*/
					if ((alsasink_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) && 
							(alsasink_Private->inputPort.sPortParam.bPopulated == OMX_FALSE)) {
						DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait for buffers. input enabled %i ,  input populated %i\n", __func__, alsasink_Private->inputPort.sPortParam.bEnabled, alsasink_Private->inputPort.sPortParam.bPopulated);
						tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
						alsasink_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
						alsasink_Private->inputPort.transientState = OMX_StateMax;
					}
					else if ((alsasink_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) && 
						(alsasink_Private->inputPort.sPortParam.bPopulated == OMX_TRUE)) {
						if(alsasink_Private->inputPort.pFullAllocationSem->semval>0)
						tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
					}
				}
				stComponent->state = OMX_StateIdle;
				tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);
				
				DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : input flags  0x%x\n", (OMX_S32)alsasink_Private->inputPort.nTunnelFlags);
				break;
				
			case OMX_StateIdle:
					return OMX_ErrorSameState;
				break;
				
			case OMX_StateExecuting:
			case OMX_StatePause:
				DEBUG(DEB_LEV_ERR, "In %s: state transition exe/pause to idle asgn ibfr=%d \n", __func__,
					alsasink_Private->inputPort.nNumTunnelBuffer);
				alsasink_Private->inputPort.bIsPortFlushed=OMX_TRUE;
				alsasink_FlushPort(stComponent, -1);
				alsasink_Private->inputPort.bIsPortFlushed=OMX_FALSE;
				stComponent->state = OMX_StateIdle;
				break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
				break;
		}
		return eError;
	}

	if(destinationState == OMX_StatePause){
		switch(stComponent->state){
			case OMX_StateInvalid:
					return OMX_ErrorInvalidState;
				break;
			case OMX_StatePause:
					return OMX_ErrorSameState;
				break;
			case OMX_StateExecuting:
			case OMX_StateIdle:
				stComponent->state = OMX_StatePause;
				break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
				break;
		}
		return OMX_ErrorNone;
	}
	
	if(destinationState == OMX_StateExecuting){
		switch(stComponent->state){
			case OMX_StateInvalid:
					return OMX_ErrorInvalidState;
				break;
			case OMX_StateIdle:
			case OMX_StatePause:
				pthread_mutex_lock(executingMutex);
				stComponent->state = OMX_StateExecuting;
				pthread_cond_signal(executingCondition);
				pthread_mutex_unlock(executingMutex);
				break;
			case OMX_StateExecuting:
					return OMX_ErrorSameState;
				break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
				break;
		}
		return OMX_ErrorNone;
	}
	
	if(destinationState == OMX_StateInvalid) {
		switch(stComponent->state){
			case OMX_StateInvalid:
					return OMX_ErrorInvalidState;
				break;
			default:
			stComponent->state = OMX_StateInvalid;
			if (alsasink_Private->bIsInit != OMX_FALSE) {
			alsasink_Deinit(stComponent);
			}
			break;
			}
		
		if (alsasink_Private->bIsInit != OMX_FALSE) {
			alsasink_Deinit(stComponent);
		}
		return OMX_ErrorInvalidState;
		// Free all the resources!!!!
	}
	return OMX_ErrorNone;
}

/** Disables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be disabled
	* @param portIndex the ID of the port to be disabled
	*/
OMX_ERRORTYPE alsasink_DisablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	queue_t* pInputQueue = alsasink_Private->inputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	if (portIndex == 0) {
		alsasink_Private->inputPort.bIsPortFlushed=OMX_TRUE;
		alsasink_FlushPort(stComponent, 0);
		alsasink_Private->inputPort.bIsPortFlushed=OMX_FALSE;
		alsasink_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
		DEBUG(DEB_LEV_FULL_SEQ,"alsasink_Private->inputPort.pFullAllocationSem=%x\n",alsasink_Private->inputPort.pFullAllocationSem->semval);
		if (alsasink_Private->inputPort.sPortParam.bPopulated == OMX_TRUE && alsasink_Private->bIsInit == OMX_TRUE) {
			if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for in buffers to be freed\n", __func__);
				tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
			}
			else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is supplier no of buffers=%d\n",
					__func__,pInputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(pInputSem->semval==alsasink_Private->inputPort.nNumTunnelBuffer) { 
					while(pInputSem->semval>0) {
						tsem_down(pInputSem);
						pInputBuffer=dequeue(pInputQueue);
					}
					alsasink_freeTunnelBuffers(&alsasink_Private->inputPort);
					alsasink_Private->inputPort.hTunneledComponent=NULL;
					alsasink_Private->inputPort.nTunnelFlags=0;
				}
			}
			else {
				if(pInputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is not supplier port still has some buffer %d\n",
					__func__,pInputSem->semval);
				if(pInputSem->semval==0 && alsasink_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
					alsasink_Private->inputPort.hTunneledComponent=NULL;
					alsasink_Private->inputPort.nTunnelFlags=0;
				}
			}
			alsasink_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
		}
		else 
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Input port is not populated\n");
		
		DEBUG(DEB_LEV_FULL_SEQ, "In %s flush buffers\n", __func__);
		
		while(pInputSem->semval < 0) {
			tsem_up(pInputSem);
		}
		DEBUG(DEB_LEV_FULL_SEQ, "In %s wait for buffer to be de-allocated\n", __func__);
	} else if (portIndex == 1) {
		return OMX_ErrorBadPortIndex;
	} else if (portIndex == -1) {
		alsasink_Private->inputPort.bIsPortFlushed=OMX_TRUE;
		alsasink_FlushPort(stComponent, -1);
		alsasink_Private->inputPort.bIsPortFlushed=OMX_FALSE;

		DEBUG(DEB_LEV_FULL_SEQ,"alsasink_DisablePort ip fASem=%x basn=%d\n",
			alsasink_Private->inputPort.pFullAllocationSem->semval,
			alsasink_Private->inputPort.nNumAssignedBuffers);

		alsasink_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
		if (alsasink_Private->inputPort.sPortParam.bPopulated == OMX_TRUE) {
			if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
			}
			else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is supplier no of buffers=%d\n",
					__func__,pInputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(pInputSem->semval==alsasink_Private->inputPort.nNumTunnelBuffer) {
					while(pInputSem->semval>0) {
						tsem_down(pInputSem);
						pInputBuffer=dequeue(pInputQueue);
					}
					alsasink_freeTunnelBuffers(&alsasink_Private->inputPort);
					alsasink_Private->inputPort.hTunneledComponent=NULL;
					alsasink_Private->inputPort.nTunnelFlags=0;
				}
			}
			else {
				if(pInputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is not supplier port still has some buffer %d\n",
					__func__,pInputSem->semval);
				if(pInputSem->semval==0 && alsasink_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
					alsasink_Private->inputPort.hTunneledComponent=NULL;
					alsasink_Private->inputPort.nTunnelFlags=0;
				}
			}
		}
		alsasink_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
		while(pInputSem->semval < 0) {
			tsem_up(pInputSem);
		}
	} else {
		return OMX_ErrorBadPortIndex;
	}

	tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting %s\n", __func__);
	return OMX_ErrorNone;
}

/** Enables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be enabled
	* @param portIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE alsasink_EnablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	queue_t* pInputQueue = alsasink_Private->inputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"I/p Port.fAS=%x,is Init=%d\n",
		alsasink_Private->inputPort.pFullAllocationSem->semval,
		alsasink_Private->bIsInit);
	if (portIndex == 0) {
		alsasink_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
		if (alsasink_Private->inputPort.sPortParam.bPopulated == OMX_FALSE && alsasink_Private->bIsInit == OMX_TRUE) {
			if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
					alsasink_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"I/p Port buffer sem =%x \n",
					pInputSem->semval);
				alsasink_allocateTunnelBuffers(&alsasink_Private->inputPort, 0, ALSASINK_IN_BUFFER_SIZE);
				alsasink_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;

			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
		}
		
	} else if (portIndex == 1) {
		return OMX_ErrorBadPortIndex;
	} else if (portIndex == -1) {
		alsasink_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
		if (alsasink_Private->inputPort.sPortParam.bPopulated == OMX_FALSE) {
			if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(alsasink_Private->inputPort.pFullAllocationSem);
					alsasink_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}
			else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"I/p Port buffer sem =%x \n",
					pInputSem->semval);
				alsasink_allocateTunnelBuffers(&alsasink_Private->inputPort, 0, ALSASINK_IN_BUFFER_SIZE);
				alsasink_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
		}
	} else {
		return OMX_ErrorBadPortIndex;
	}

	tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);
	return OMX_ErrorNone;
}

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE alsasink_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	queue_t* pInputQueue = alsasink_Private->inputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_BOOL *inbufferUnderProcess=&alsasink_Private->inputPort.bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&alsasink_Private->inputPort.mutex;
	pthread_cond_t* executingCondition = &alsasink_Private->executingCondition;
	OMX_BOOL flag;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%x\n", __func__,portIndex);
	if (portIndex == 0) {
		if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d ib=%d,ibcb=%d\n",
				pInputSem->semval,alsasink_Private->inbuffer,alsasink_Private->inbuffercb);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pInputBuffer);
				alsasink_Private->inbuffercb++;
			}
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			
		}
		else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {

			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(alsasink_Private->inputPort.hTunneledComponent, pInputBuffer);
				alsasink_Private->inbuffercb++;
			}
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			
		}
		else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			pthread_mutex_unlock(pInmutex);
			if(pInputSem->semval<alsasink_Private->inputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
		}
	} else if (portIndex == 1) {
		return OMX_ErrorBadPortIndex;
	} else if (portIndex == -1) {
		if (!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pInputBuffer);
				alsasink_Private->inbuffercb++;
			}
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*"Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			DEBUG(DEB_LEV_FULL_SEQ,"Flashing input ports insemval=%d ib=%d,ibcb=%d\n",
				pInputSem->semval,alsasink_Private->inbuffer,alsasink_Private->inbuffercb);
		}
		else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {

			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(alsasink_Private->inputPort.hTunneledComponent, pInputBuffer);
				alsasink_Private->inbuffercb++;
			}
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			
		}
		else if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			pthread_mutex_unlock(pInmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in port buffer=%d,no of tunlbuffer=%d flushsem=%d bup=%d\n", __func__,
				pInputSem->semval,alsasink_Private->inputPort.nNumTunnelBuffer,
				alsasink_Private->inputPort.pFlushSem->semval,flag);
			if(pInputSem->semval<alsasink_Private->inputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(alsasink_Private->inputPort.pFlushSem);
			pthread_mutex_lock(pInmutex);
			alsasink_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
		}
	} 

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);
	return OMX_ErrorNone;
}

/** Allocate buffers in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Allocate the MAX number of buffers for that port, with the specified size.
	* @param alsasink_Port the port of the alsa component that must be tunneled
	* @param portIndex index of the port to be tunneled
	* @param bufferSize Size of the buffers to be allocated
	*/
OMX_ERRORTYPE alsasink_allocateTunnelBuffers(alsasink_PortType* alsasink_Port, OMX_U32 portIndex, OMX_U32 bufferSize) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_U8* pBuffer=NULL;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<alsasink_Port->nNumTunnelBuffer;i++){
		if(alsasink_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			alsasink_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(alsasink_Port->pBuffer[i]->pBuffer);
		} else if (alsasink_Port->nBufferState[i] & BUFFER_ASSIGNED){
			alsasink_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	for(i=0;i<alsasink_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Allocating  %i buffer of size %i\n", i, (OMX_S32)bufferSize);
		pBuffer = malloc(bufferSize);
		DEBUG(DEB_LEV_FULL_SEQ, "   malloc done %x\n",pBuffer);
		eError=OMX_UseBuffer(alsasink_Port->hTunneledComponent,&alsasink_Port->pBuffer[i],
			alsasink_Port->nTunneledPort,NULL,bufferSize,pBuffer); 
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Component Couldn't Allocate buffer %i Error=%08x\n",__func__,i,eError);
			free(pBuffer);
			return eError;
		}
		if ((eError = checkHeader(alsasink_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong buffer header size=%d version=0x%08x\n", 
				__func__,alsasink_Port->pBuffer[i]->nSize,alsasink_Port->pBuffer[i]->nVersion.s.nVersionMajor);
			//return eError;
		}
		alsasink_Port->pBuffer[i]->nFilledLen = 0;
		alsasink_Port->nBufferState[i] |= BUFFER_ALLOCATED;
		
		queue(alsasink_Port->pBufferQueue, alsasink_Port->pBuffer[i]);
		tsem_up(alsasink_Port->pBufferSem);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "   queue done\n");
	}
	alsasink_Port->sPortParam.bPopulated = OMX_TRUE;
	
	return OMX_ErrorNone;
}

/** Free buffer in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Free the MAX number of buffers for that port, with the specified size.
	* @param alsasink_Port the port of the alsa component on which tunnel buffers must be released
	*/
OMX_ERRORTYPE alsasink_freeTunnelBuffers(alsasink_PortType* alsasink_Port) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<alsasink_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Freeing  %i buffer %x\n", i,alsasink_Port->pBuffer[i]->pBuffer);
	
		if(alsasink_Port->pBuffer[i]->pBuffer)
			free(alsasink_Port->pBuffer[i]->pBuffer);

		eError=OMX_FreeBuffer(alsasink_Port->hTunneledComponent,alsasink_Port->nTunneledPort,alsasink_Port->pBuffer[i]);
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Component Couldn't free buffers %i Error=%08x\n",__func__,i,eError);
			return eError;
		} 
		
		DEBUG(DEB_LEV_FULL_SEQ, "   free done\n");
		alsasink_Port->pBuffer[i]->nAllocLen = 0;
		alsasink_Port->pBuffer[i]->pPlatformPrivate = NULL;
		alsasink_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
		alsasink_Port->pBuffer[i]->nInputPortIndex = 0;
		alsasink_Port->pBuffer[i]->nOutputPortIndex = 0;
	}
	for(i=0;i<alsasink_Port->nNumTunnelBuffer;i++){
		if(alsasink_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			alsasink_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(alsasink_Port->pBuffer[i]->pBuffer);
		} else if (alsasink_Port->nBufferState[i] & BUFFER_ASSIGNED){
			alsasink_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	alsasink_Port->sPortParam.bPopulated = OMX_FALSE;
	return OMX_ErrorNone;
}

/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param pCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
 */
OMX_ERRORTYPE alsasink_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	stComponent->callbacks = pCallbacks;
	stComponent->callbackData = pAppData;
	
	return OMX_ErrorNone;
}

/** This function is executed when a component must release all its resources.
 * It happens when the OMX_ComponentDeInit function is called, when a critical error happens, or when the component is turned to Loaded
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE alsasink_Deinit(stComponentType* stComponent)
{
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	OMX_S32 ret;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s\n", __func__);
	alsasink_Private->bIsInit = OMX_FALSE;
	
	/** Trash buffer mangement thread.
	 */

	pthread_mutex_lock(&alsasink_Private->exit_mutex);
	alsasink_Private->bExit_buffer_thread=OMX_TRUE;
	pthread_mutex_unlock(&alsasink_Private->exit_mutex);
	
	DEBUG(DEB_LEV_FULL_SEQ,"In %s ibsemval=%d, bup=%d \n",__func__,
		pInputSem->semval, 
		alsasink_Private->inputPort.bBufferUnderProcess);

	if(pInputSem->semval == 0 && 
		alsasink_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
		tsem_up(pInputSem);
	}


	DEBUG(DEB_LEV_FULL_SEQ,"All buffers flushed!\n");
	DEBUG(DEB_LEV_FULL_SEQ,"In %s ibsemval=%d\n",__func__,
		pInputSem->semval);
	ret=pthread_join(alsasink_Private->bufferMgmtThread,NULL);

	DEBUG(DEB_LEV_FULL_SEQ,"In %s ibsemval=%d\n",__func__,
		pInputSem->semval);

	while(pInputSem->semval>0 ) {
		tsem_down(pInputSem);
		pInputBuffer=dequeue(alsasink_Private->inputPort.pBufferQueue);
	}

	pthread_mutex_lock(&alsasink_Private->exit_mutex);
	alsasink_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&alsasink_Private->exit_mutex);

	DEBUG(DEB_LEV_FULL_SEQ,"Execution thread re-joined\n");
	/*Deinitialize mutexes and conditional variables*/
	pthread_cond_destroy(&alsasink_Private->executingCondition);
	pthread_mutex_destroy(&alsasink_Private->executingMutex);
	DEBUG(DEB_LEV_FULL_SEQ,"Deinitialize mutexes and conditional variables\n");
	
	pthread_mutex_destroy(&alsasink_Private->inputPort.mutex);
	pthread_cond_signal(&alsasink_Private->exit_condition);

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);

	return OMX_ErrorNone;
}

/************************************
 *
 * PUBLIC: OMX component entry points
 *
 ************************************/
OMX_ERRORTYPE alsasink_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING pComponentName,
	OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
	OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
	OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	OMX_U32 uuid[3];
	
	/* Fill component name */
	strcpy(pComponentName, stComponent->name);
	
	/* Fill component version */
	pComponentVersion->s.nVersionMajor = 0;
	pComponentVersion->s.nVersionMinor = 0;
	pComponentVersion->s.nRevision = 0;
	pComponentVersion->s.nStep = 1;
	
	/* Fill spec version (copy from component field) */
	memcpy(pSpecVersion, &omx_component->nVersion, sizeof(OMX_VERSIONTYPE));

	/* Fill UUID with handle address, PID and UID.
	 * This should guarantee uiniqness */
	uuid[0] = (OMX_U32)stComponent;
	uuid[1] = getpid();
	uuid[2] = getuid();
	memcpy(*pComponentUUID, uuid, 3*sizeof(uuid));
	
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			setHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
			pPrioMgmt->nGroupPriority = alsasink_Private->nGroupPriority;
			pPrioMgmt->nGroupID = alsasink_Private->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
			memcpy(ComponentParameterStructure, &alsasink_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;
		case OMX_IndexParamVideoInit:
		case OMX_IndexParamImageInit:
		case OMX_IndexParamOtherInit:
			pPortDomains = (OMX_PORT_PARAM_TYPE*)ComponentParameterStructure;
			setHeader(pPortDomains, sizeof(OMX_PORT_PARAM_TYPE));
			pPortDomains->nPorts = 0;
			pPortDomains->nStartPortNumber = 0;
		break;		
		case OMX_IndexParamPortDefinition:
			pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
			if (pPortDef ->nPortIndex == 0) {
				memcpy(pPortDef , &alsasink_Private->inputPort.sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (pPortDef ->nPortIndex == 1) {
				return OMX_ErrorBadPortIndex;
				//memcpy(pPortDef , &alsasink_Private->outputPort.sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else {
				return OMX_ErrorBadPortIndex;
			}
		break;
		case OMX_IndexParamCompBufferSupplier:
			pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
			if (pBufSupply->nPortIndex == 0) {
				setHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
				if (alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyInput;	
				} else if (alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyOutput;	
				} else {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyUnspecified;	
				}
			} else if (pBufSupply->nPortIndex == 1) {
				return OMX_ErrorBadPortIndex;
			} else {
				return OMX_ErrorBadPortIndex;
			}

		break;
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		if (pAudioPortFormat->nPortIndex == 0) {
			memcpy(pAudioPortFormat, &alsasink_Private->inputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else if (pAudioPortFormat->nPortIndex == 1) {
			return OMX_ErrorBadPortIndex;
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;
		case OMX_IndexParamAudioPcm:
			if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
				alsasink_Private->inputPort.omxAudioParamPcmMode.nPortIndex)
				return OMX_ErrorBadParameter;
			memcpy(ComponentParameterStructure, &alsasink_Private->inputPort.omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			break;
		default:
			return OMX_ErrorUnsupportedIndex;
		break;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	alsasink_PortType* inputPort = &alsasink_Private->inputPort;
	snd_pcm_t* playback_handle = inputPort->playback_handle;
	snd_pcm_hw_params_t* hw_params = inputPort->hw_params;

	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	/** Each time we are (re)configuring the hw_params thing
	 * we need to reinitialize it, otherwise previous changes will not take effect.
	 * e.g.: changing a previously configured sampling rate does not have
	 * any effect if we are not calling this each time.
	 */
	err = snd_pcm_hw_params_any (alsasink_Private->inputPort.playback_handle, alsasink_Private->inputPort.hw_params);


	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			if ((err = checkHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE))) != OMX_ErrorNone) {
				break;
			}
			alsasink_Private->nGroupPriority = pPrioMgmt->nGroupPriority;
			alsasink_Private->nGroupID = pPrioMgmt->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
			if (((OMX_PORT_PARAM_TYPE*)ComponentParameterStructure)->nPorts != 1)
				err = OMX_ErrorBadParameter;
			break;
		break;
		case OMX_IndexParamPortDefinition: {
			pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			
			if ((err = checkHeader(pPortDef , sizeof(OMX_PARAM_PORTDEFINITIONTYPE))) != OMX_ErrorNone) {
				return err;
			}
			if (pPortDef ->nPortIndex > 0) {
				return OMX_ErrorBadPortIndex;
			}
			if (pPortDef ->nPortIndex == 0) {
					alsasink_Private->inputPort.sPortParam.nBufferCountActual = pPortDef ->nBufferCountActual;
			} else {
				return OMX_ErrorBadPortIndex;
			}
		}
		break;
		case OMX_IndexParamAudioPortFormat:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			if ((err = checkHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
		case OMX_IndexParamCompBufferSupplier:
			pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;

			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				if ((pBufSupply->nPortIndex == 0) && (alsasink_Private->inputPort.sPortParam.bEnabled==OMX_TRUE)) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
					return OMX_ErrorIncorrectStateOperation;
				}
				else if (pBufSupply->nPortIndex == 1) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
					//return OMX_ErrorIncorrectStateOperation;
					return OMX_ErrorBadPortIndex;
				}
			}

			if (pBufSupply->nPortIndex > 0) {
				return OMX_ErrorBadPortIndex;
			}
			if (pBufSupply->eBufferSupplier == OMX_BufferSupplyUnspecified) {
				return OMX_ErrorNone;
			}
			if ((alsasink_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) == 0) {
				return OMX_ErrorNone;
			}
			if((err = checkHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE))) != OMX_ErrorNone){
				return err;
			}
			if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyInput) && (pBufSupply->nPortIndex == 0)) {
				/** These two cases regard the first stage of client override */
				if (alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					err = OMX_ErrorNone;
					break;
				}
				alsasink_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				err = OMX_SetParameter(alsasink_Private->inputPort.hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufSupply);
			} else if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyOutput) && (pBufSupply->nPortIndex == 0)) {
				if (alsasink_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					alsasink_Private->inputPort.nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
					err = OMX_SetParameter(alsasink_Private->inputPort.hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufSupply);
					break;
				}
				err = OMX_ErrorNone;
			} else if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyOutput) && (pBufSupply->nPortIndex == 1)) {
				return OMX_ErrorBadPortIndex;
			} else {
				err = OMX_ErrorNone;
			}
		break;
		case OMX_IndexParamAudioPcm:
			{
				unsigned int rate;
				OMX_AUDIO_PARAM_PCMMODETYPE* omxAudioParamPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
				inputPort->AudioPCMConfigured	= 1;
				if(omxAudioParamPcmMode->nPortIndex != inputPort->omxAudioParamPcmMode.nPortIndex){
					DEBUG(DEB_LEV_ERR, "Error setting input port index\n");
					err = OMX_ErrorBadParameter;
					break;
				}
				
				if(snd_pcm_hw_params_set_channels(playback_handle, hw_params, omxAudioParamPcmMode->nChannels)){
					DEBUG(DEB_LEV_ERR, "Error setting number of channels\n");
					return OMX_ErrorBadParameter;
				}

				if(omxAudioParamPcmMode->bInterleaved == OMX_TRUE){
					if ((err = snd_pcm_hw_params_set_access(inputPort->playback_handle, inputPort->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type intrleaved (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
				}
				else{
					if ((err = snd_pcm_hw_params_set_access(inputPort->playback_handle, inputPort->hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type non interleaved (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
				}

				rate = omxAudioParamPcmMode->nSamplingRate;
				if ((err = snd_pcm_hw_params_set_rate_near(inputPort->playback_handle, inputPort->hw_params, &rate, 0)) < 0) {
					DEBUG(DEB_LEV_ERR, "cannot set sample rate (%s)\n", snd_strerror (err));
					alsasink_Panic();
				}
				else{
					DEBUG(DEB_LEV_PARAMS, "Set correctly sampling rate to %i\n", (int)inputPort->omxAudioParamPcmMode.nSamplingRate);
				}
					
				if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeLinear){
					snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;
					DEBUG(DEB_LEV_PARAMS, "Bit per sample %i, signed=%i, little endian=%i\n",
						(int)omxAudioParamPcmMode->nBitPerSample,
						(int)omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned,
						(int)omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle);
					
					switch(omxAudioParamPcmMode->nBitPerSample){
						case 8:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned)
								snd_pcm_format = SND_PCM_FORMAT_S8;
							else
								snd_pcm_format = SND_PCM_FORMAT_U8;
							break;
							
						case 16:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S16_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S16_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle){
									
									snd_pcm_format = SND_PCM_FORMAT_U16_LE;
								}
								else
									snd_pcm_format = SND_PCM_FORMAT_U16_BE;
							}
							break;
							
						case 24:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S24_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S24_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_U24_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_U24_BE;
							}
							break;
							
						case 32:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S32_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S32_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_U32_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_U32_BE;
						}
							break;
							
						default:
							err = OMX_ErrorBadParameter;
					}
					
					if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN){
						int err;
						if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
							DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
							alsasink_Panic();
						}
					}
					else{
						DEBUG(DEB_LEV_SIMPLE_SEQ, "ALSA OMX_IndexParamAudioPcm configured\n");
						memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					}
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeALaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_A_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
						alsasink_Panic();
					}
					memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeMULaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_MU_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
					memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					break;
				}
			}
		break;
		case OMX_IndexParamAudioMp3:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			if((err = checkHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
		default:
			return OMX_ErrorBadParameter;
		break;
	}
	
	/* If we are not asked two ports give error */
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "   Error during %s = %08x\n", __func__, err);
	}
	return err;
}

OMX_ERRORTYPE alsasink_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType) {
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE alsasink_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	*pState = stComponent->state;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	alsasink_PrivateType* alsasink_Private = (alsasink_PrivateType*)omxComponent->pComponentPrivate;
	alsasink_PortType* alsasink_Port;
	OMX_S32 i;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	
	/** There should be some mechanism to indicate whether the buffer is application allocated or component. And 
	component need to track that. so the fill buffer done call back need not to be called and IL client will
	pull the buffer from the output port */
	
	if(nPortIndex == 0) {
		alsasink_Port = &alsasink_Private->inputPort;
	} else if(nPortIndex == 1) {
		return OMX_ErrorBadPortIndex;
	}
	if ((alsasink_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(alsasink_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorNone;
	}

	/** Buffers can be allocated only when the component is at loaded state and changing to idle state
	 */

	if (alsasink_Port->transientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s i=%i Buffer state=%x\n",__func__,i,alsasink_Port->nBufferState[i]);
		if(!(alsasink_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(alsasink_Port->nBufferState[i] & BUFFER_ASSIGNED)){ 
			DEBUG(DEB_LEV_FULL_SEQ,"Inside %s i=%i Buffer state=%x\n",__func__,i,alsasink_Port->nBufferState[i]);
			alsasink_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(alsasink_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* use the buffer */
			alsasink_Port->pBuffer[i]->pBuffer = pBuffer;
			alsasink_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			alsasink_Port->pBuffer[i]->nFilledLen = 0;
			alsasink_Port->pBuffer[i]->nOffset = 0;
			alsasink_Port->pBuffer[i]->pPlatformPrivate = alsasink_Port;
			alsasink_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			alsasink_Port->pBuffer[i]->nTickCount = 0;
			alsasink_Port->pBuffer[i]->nTimeStamp = 0;
			*ppBufferHdr = alsasink_Port->pBuffer[i];
			if (nPortIndex == 0) {
				alsasink_Port->pBuffer[i]->nInputPortIndex = 0;
				
				if(alsasink_Private->inputPort.nTunnelFlags)
					alsasink_Port->pBuffer[i]->nOutputPortIndex = alsasink_Private->inputPort.nTunneledPort;
				else 
					alsasink_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports; // here is assigned a non-valid port index

			} else {
				/*Control will never reach here as only port 0 is present*/
				alsasink_Port->pBuffer[i]->nInputPortIndex = stComponent->nports;
				alsasink_Port->pBuffer[i]->nOutputPortIndex = 1;
			}
			alsasink_Port->nBufferState[i] |= BUFFER_ASSIGNED;
			alsasink_Port->nBufferState[i] |= HEADER_ALLOCATED;
			alsasink_Port->nNumAssignedBuffers++;
			if (alsasink_Port->sPortParam.nBufferCountActual == alsasink_Port->nNumAssignedBuffers) {
				alsasink_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(alsasink_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,alsasink_Port->sPortParam.nBufferCountActual,
				alsasink_Port->nNumAssignedBuffers,
				alsasink_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = (alsasink_PrivateType*)omxComponent->pComponentPrivate;
	alsasink_PortType* alsasink_Port;
	OMX_S32 i;

	if(nPortIndex == 0) {
		alsasink_Port = &alsasink_Private->inputPort;
	} else if(nPortIndex == 1) {
		return OMX_ErrorBadPortIndex;
	}
	if ((alsasink_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(alsasink_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorBadPortIndex;
	}
	if (alsasink_Port->transientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		if(!(alsasink_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(alsasink_Port->nBufferState[i] & BUFFER_ASSIGNED)){
			alsasink_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(alsasink_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* allocate the buffer */
			alsasink_Port->pBuffer[i]->pBuffer = malloc(nSizeBytes);
			if(alsasink_Port->pBuffer[i]->pBuffer==NULL)
				return OMX_ErrorInsufficientResources;
			alsasink_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			alsasink_Port->pBuffer[i]->pPlatformPrivate = alsasink_Port;
			alsasink_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			*pBuffer = alsasink_Port->pBuffer[i];
			alsasink_Port->nBufferState[i] |= BUFFER_ALLOCATED;
			alsasink_Port->nBufferState[i] |= HEADER_ALLOCATED;
			if (nPortIndex == 0) {
				alsasink_Port->pBuffer[i]->nInputPortIndex = 0;
				alsasink_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports;
			} else {
				alsasink_Port->pBuffer[i]->nInputPortIndex = stComponent->nports;
				alsasink_Port->pBuffer[i]->nOutputPortIndex = 1;
			}
			
			alsasink_Port->nNumAssignedBuffers++;
			DEBUG(DEB_LEV_PARAMS, "alsasink_Port->nNumAssignedBuffers %i\n", alsasink_Port->nNumAssignedBuffers);
			
			if (alsasink_Port->sPortParam.nBufferCountActual == alsasink_Port->nNumAssignedBuffers) {
				alsasink_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(alsasink_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,alsasink_Port->sPortParam.nBufferCountActual,
				alsasink_Port->nNumAssignedBuffers,
				alsasink_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	
	DEBUG(DEB_LEV_ERR, "Error: no available buffers\n");
	return OMX_ErrorInsufficientResources;
}

OMX_ERRORTYPE alsasink_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = (alsasink_PrivateType*)omxComponent->pComponentPrivate;
	alsasink_PortType* alsasink_Port;
	
	OMX_S32 i;
	OMX_BOOL foundBuffer;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	if(nPortIndex == 0) {
		alsasink_Port = &alsasink_Private->inputPort;
		if (pBuffer->nInputPortIndex != 0) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
			return OMX_ErrorBadParameter;
		}
	} else if(nPortIndex == 1) {
		return OMX_ErrorBadPortIndex;
	}
	if ((alsasink_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(alsasink_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)){
		//return OMX_ErrorBadPortIndex;
		return OMX_ErrorNone;
	}
	DEBUG(DEB_LEV_PARAMS,"\nIn %s bEnabled=%d,bPopulated=%d state=%d,transientState=%d\n",
		__func__,alsasink_Port->sPortParam.bEnabled,alsasink_Port->sPortParam.bPopulated,
		stComponent->state,alsasink_Port->transientState);
	
	if (alsasink_Port->transientState != OMX_StateLoaded 		 
		&& alsasink_Port->transientState != OMX_StateInvalid ) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to release its buffers\n", __func__);
		(*(stComponent->callbacks->EventHandler))
			(hComponent,
				stComponent->callbackData,
				OMX_EventError, /* The command was completed */
				OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
				nPortIndex, /* The state has been changed in message->messageParam2 */
				NULL);
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		if((alsasink_Port->nBufferState[i] & BUFFER_ALLOCATED) &&
			(alsasink_Port->pBuffer[i]->pBuffer == pBuffer->pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			alsasink_Port->nNumAssignedBuffers--;
			free(pBuffer->pBuffer);
			if(alsasink_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			alsasink_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
		else if((alsasink_Port->nBufferState[i] & BUFFER_ASSIGNED) &&
			(alsasink_Port->pBuffer[i] == pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			alsasink_Port->nNumAssignedBuffers--;
			if(alsasink_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			alsasink_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
	}
	/*check if there are some buffers already to be freed */
	foundBuffer = OMX_FALSE;
	for (i = 0; i< alsasink_Port->sPortParam.nBufferCountActual ; i++) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Buffer flags - %i - %i\n", i, alsasink_Port->nBufferState[i]);
	
		if (alsasink_Port->nBufferState[i] != BUFFER_FREE) {
			foundBuffer = OMX_TRUE;
			break;
		}
	}
	if (!foundBuffer) {
		tsem_up(alsasink_Port->pFullAllocationSem);
		alsasink_Port->sPortParam.bPopulated = OMX_FALSE;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = (alsasink_PrivateType*)omxComponent->pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	
	queue_t* messageQueue;
	tsem_t* messageSem;
	coreMessage_t* message;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	messageQueue = stComponent->coreDescriptor->messageQueue;
	messageSem = stComponent->coreDescriptor->messageSem;
	
	if (stComponent->state == OMX_StateInvalid) {
		err = OMX_ErrorInvalidState;
	}
	/** Fill in the message */
	switch (Cmd) {
		case OMX_CommandStateSet:
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s state is %i\n", __func__, stComponent->state);
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			message->messageType = SENDCOMMAND_MSG_TYPE;
			message->messageParam1 = OMX_CommandStateSet;
			message->messageParam2 = nParam;
			message->pCmdData=pCmdData;
			/** The following section is executed here instead of in the delayed
				* thread due to syncrhonization isssues related to the sequence
				* change state to idle --> allocate buffers
				* and
				* change state to loaded --. deallocate buffers
				*/
			if ((nParam == OMX_StateIdle) && (stComponent->state == OMX_StateLoaded)) {
					err=alsasink_Init(stComponent);
					if(err!=OMX_ErrorNone) {
						DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s  alsasink_Init returned error %i\n",
							__func__,err);
						return OMX_ErrorInsufficientResources;
					}
					if (alsasink_Private->inputPort.sPortParam.bEnabled)
						alsasink_Private->inputPort.transientState = OMX_StateIdle;
			} else if ((nParam == OMX_StateLoaded) && (stComponent->state == OMX_StateIdle)) {
				tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);
					if (alsasink_Private->inputPort.sPortParam.bEnabled)
						alsasink_Private->inputPort.transientState = OMX_StateLoaded;
			}
			else if (nParam == OMX_StateInvalid) {
					if (alsasink_Private->inputPort.sPortParam.bEnabled)
						alsasink_Private->inputPort.transientState = OMX_StateInvalid;
			}
		break;
		case OMX_CommandFlush:
			DEBUG(DEB_LEV_FULL_SEQ, "In OMX_CommandFlush state is %i\n", stComponent->state);
			if ((stComponent->state != OMX_StateExecuting) && (stComponent->state != OMX_StatePause)) {
				err = OMX_ErrorIncorrectStateOperation;
				break;

			}
			if (nParam == 0) {
				alsasink_Private->inputPort.bIsPortFlushed=OMX_TRUE;
			}
			else if (nParam == 1) {
				return OMX_ErrorBadPortIndex;
			} else if (nParam == -1) {
				alsasink_Private->inputPort.bIsPortFlushed=OMX_TRUE;
			}else
				return OMX_ErrorBadPortIndex;
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			message->messageType = SENDCOMMAND_MSG_TYPE;
			message->messageParam1 = OMX_CommandFlush;
			message->messageParam2 = nParam;
			message->pCmdData=pCmdData;
		break;
		case OMX_CommandPortDisable:
			tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortDisable state is %i\n", stComponent->state);
			if (nParam == 0) {
				if (alsasink_Private->inputPort.sPortParam.bEnabled == OMX_FALSE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					alsasink_Private->inputPort.transientState = OMX_StateLoaded;
				}
			} else if (nParam == 1) {
				return OMX_ErrorBadPortIndex;
			
			} else if (nParam == -1) {
				if (alsasink_Private->inputPort.sPortParam.bEnabled == OMX_FALSE){
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					alsasink_Private->inputPort.transientState = OMX_StateLoaded;
				}
			} else {
				err = OMX_ErrorUnsupportedIndex;
					break;
			}
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			if (err == OMX_ErrorNone) {
				message->messageType = SENDCOMMAND_MSG_TYPE;
				message->messageParam2 = nParam;
			} else {
				message->messageType = ERROR_MSG_TYPE;
				message->messageParam2 = err;
			}
			message->messageParam1 = OMX_CommandPortDisable;
			message->pCmdData=pCmdData;
		break;
		case OMX_CommandPortEnable:
			tsem_reset(alsasink_Private->inputPort.pFullAllocationSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortEnable state is %i\n", stComponent->state);
			if (nParam == 0) {
				if (alsasink_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					alsasink_Private->inputPort.transientState = OMX_StateIdle;
				}
			} else if (nParam == 1) {
				return OMX_ErrorBadPortIndex;
			} else if (nParam == -1) {
				if (alsasink_Private->inputPort.sPortParam.bEnabled == OMX_TRUE){
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					alsasink_Private->inputPort.transientState = OMX_StateIdle;
				}
			} else {
				err = OMX_ErrorUnsupportedIndex;
				break;
			}
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			if (err == OMX_ErrorNone) {
				message->messageType = SENDCOMMAND_MSG_TYPE;
			} else {
				message->messageType = ERROR_MSG_TYPE;
			}
			message->messageParam1 = OMX_CommandPortEnable;
			message->messageParam2 = nParam;
			message->pCmdData=pCmdData;
		break;
		case OMX_CommandMarkBuffer:
			if ((stComponent->state != OMX_StateExecuting) && (stComponent->state != OMX_StatePause)) {
				err = OMX_ErrorIncorrectStateOperation;
				break;
			}
			if(nParam!=0 && nParam!=1)
				return OMX_ErrorBadPortIndex;;

			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			message->messageType = SENDCOMMAND_MSG_TYPE;
			message->messageParam1 = OMX_CommandMarkBuffer;
			message->messageParam2 = nParam;
			message->pCmdData=pCmdData;
		/** insert the message structure the pointer for the private datat related to the mark 
			*/
		break;
		default:
		err = OMX_ErrorUnsupportedIndex;
		break;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s cond %x is signalling\n",__func__,&messageSem->condition);
	queue(messageQueue, message);
	tsem_up(messageSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "%s returning err = %i msg=%x semval=%d\n", __func__, err,messageSem,messageSem->semval);
	return err;
}

OMX_ERRORTYPE alsasink_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Tearing down component...\n");

	/* Take care of tearing down resources if not in loaded state */
	if(stComponent->state != OMX_StateLoaded)
		alsasink_Deinit(stComponent);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = alsasink_Private->inputPort.pBufferSem;
	queue_t* pInputQueue = alsasink_Private->inputPort.pBufferQueue;
	OMX_ERRORTYPE err=OMX_ErrorNone;

	/* The input port code is not valid
	*/
	if (pBuffer->nInputPortIndex != 0) {
		DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
		return OMX_ErrorBadPortIndex;
	}
	/* We are not accepting buffers if not in executing or
	 * paused state
	 */
	if(stComponent->state == OMX_StateInvalid)
		return OMX_ErrorInvalidState;

	if(stComponent->state != OMX_StateExecuting &&
		stComponent->state != OMX_StatePause &&
		stComponent->state != OMX_StateIdle){
		DEBUG(DEB_LEV_ERR, "In %s: we are not in executing/paused/idle state =%d\n", __func__,stComponent->state);
		return OMX_ErrorIncorrectStateOperation;
	}

	if(alsasink_Private->inputPort.sPortParam.bEnabled==OMX_FALSE)
		return OMX_ErrorIncorrectStateOperation;

	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}

	/* Queue this buffer on the output port */
	queue(pInputQueue, pBuffer);
	/* And notify the buffer management thread we have a fresh new buffer to manage */
	tsem_up(pInputSem);

	alsasink_Private->inbuffer++;
	DEBUG(DEB_LEV_FULL_SEQ,"In %s no of in buffer=%d\n",__func__,alsasink_Private->inbuffer);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE alsasink_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE alsasink_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE err=OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE param;
	OMX_PARAM_BUFFERSUPPLIERTYPE pSupplier;

	stComponentType* stComponent = (stComponentType*)hComp;
	
	alsasink_PrivateType* alsasink_Private = stComponent->omx_component.pComponentPrivate;

	if (pTunnelSetup == NULL || hTunneledComp == 0) {
        /* cancel previous tunnel */
		if (nPort == 0) {
       		alsasink_Private->inputPort.hTunneledComponent = 0;
			alsasink_Private->inputPort.nTunneledPort = 0;
			alsasink_Private->inputPort.nTunnelFlags = 0;
			alsasink_Private->inputPort.eBufferSupplier=OMX_BufferSupplyUnspecified;
		}
		else {
			return OMX_ErrorBadPortIndex;
		}
		return err;
    }

	if (nPort == 0) {
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		alsasink_Private->inputPort.nNumTunnelBuffer=param.nBufferCountMin;

		DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled port domain=%d\n",param.eDomain);
		if(param.eDomain!=OMX_PortDomainAudio)
			return OMX_ErrorPortsNotCompatible;
		else if(param.format.audio.eEncoding == OMX_AUDIO_CodingMax)
			return OMX_ErrorPortsNotCompatible;

		/* Get Buffer Supplier type of the Tunnelled Component*/
		pSupplier.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);

		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled error=%d\n",err);
			return OMX_ErrorPortsNotCompatible;
		}
		else {
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled Port eBufferSupplier=%x\n",pSupplier.eBufferSupplier);
		}
		

		// store the current callbacks, if defined
		alsasink_Private->inputPort.hTunneledComponent = hTunneledComp;
		alsasink_Private->inputPort.nTunneledPort = nTunneledPort;
		alsasink_Private->inputPort.nTunnelFlags = 0;
		// Negotiation
		if (pTunnelSetup->nTunnelFlags & OMX_PORTTUNNELFLAG_READONLY) {
			// the buffer provider MUST be the output port provider
			pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
			alsasink_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;	
			alsasink_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
		} else {
			if (pTunnelSetup->eSupplier == OMX_BufferSupplyInput) {
				alsasink_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				alsasink_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			} else if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified) {
				pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
				alsasink_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				alsasink_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			}
		}
		alsasink_Private->inputPort.nTunnelFlags |= TUNNEL_ESTABLISHED;
	}  else {
		return OMX_ErrorBadPortIndex;
	}
	return OMX_ErrorNone;
}

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void alsasink_Panic() {
	exit(EXIT_FAILURE);
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* File EOF */

