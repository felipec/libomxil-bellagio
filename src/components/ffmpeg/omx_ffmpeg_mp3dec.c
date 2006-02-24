/**
	@file src/components/ffmpeg/omx_ffmpeg_mp3dec.c
	
	OpenMax Mp3 decoder component. This library implements an mp3 decoder
	based on ffmpeg library. 
	
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
	
	2006/02/08:  OpenMAX FFMPEG MP3 decoder version 0.1

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Audio.h>

#include "omxcore.h"
#include "omx_ffmpeg_mp3dec.h"

#include "tsemaphore.h"
#include "queue.h"

OMX_U32 noMp3decInstance=0;
/*********************
 *
 * Component template
 *
 **********************/

/** This is the stComponent template from which all
 * other stComponent instances are factored by the core.
 * Note that it has been necessary to wrap the OMX component
 * with a st_component because of design deficiencies.
 */
stComponentType ffmpeg_mp3dec_component = {
  .name = "OMX.st.ffmpeg.mp3dec",
  .state = OMX_StateLoaded,
  .callbacks = NULL,
  .callbackData = NULL,
  .nports = 2,
	.coreDescriptor = NULL,
  .constructor = ffmpeg_mp3dec_Constructor,
  .destructor = ffmpeg_mp3dec_Destructor,
  .messageHandler = ffmpeg_mp3dec_MessageHandler,

	.omx_component = {
  .nSize = sizeof(OMX_COMPONENTTYPE),
  .nVersion = { .s = {SPECVERSIONMAJOR,
							 SPECVERSIONMINOR,
							 SPECREVISION,
							 SPECSTEP}},
  .pComponentPrivate = NULL,
  .pApplicationPrivate = NULL,

  .GetComponentVersion = ffmpeg_mp3dec_GetComponentVersion,
  .SendCommand = ffmpeg_mp3dec_SendCommand,
  .GetParameter = ffmpeg_mp3dec_GetParameter,
  .SetParameter = ffmpeg_mp3dec_SetParameter,
  .GetConfig = ffmpeg_mp3dec_GetConfig,
  .SetConfig = ffmpeg_mp3dec_SetConfig,
  .GetExtensionIndex = ffmpeg_mp3dec_GetExtensionIndex,
  .GetState = ffmpeg_mp3dec_GetState,
  .ComponentTunnelRequest = ffmpeg_mp3dec_ComponentTunnelRequest,
  .UseBuffer = ffmpeg_mp3dec_UseBuffer,
  .AllocateBuffer = ffmpeg_mp3dec_AllocateBuffer,
  .FreeBuffer = ffmpeg_mp3dec_FreeBuffer,
  .EmptyThisBuffer = ffmpeg_mp3dec_EmptyThisBuffer,
  .FillThisBuffer = ffmpeg_mp3dec_FillThisBuffer,
  .SetCallbacks = ffmpeg_mp3dec_SetCallbacks,
  .ComponentDeInit = ffmpeg_mp3dec_ComponentDeInit,
	},
};


/**************************************************************
 *
 * PRIVATE: component private entry points and helper functions
 *
 **************************************************************/

/** Component template registration function
 *
 * This is the component entry point which is called at library
 * initialization for each OMX component. It provides the OMX core with
 * the component template.
 */
void __attribute__ ((constructor)) ffmpeg_mp3dec__register_template()
{
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);
  register_template(&ffmpeg_mp3dec_component);
}

/** 
	It initializates the ffmpeg framework, and opens an ffmpeg mp3 decoder
*/
void ffmpeg_mp3dec_ffmpegLibInit(ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private) {
	avcodec_init();
	av_register_all();

	ffmpeg_mp3dec_Private->avCodec = avcodec_find_decoder(CODEC_ID_MP2);
	if (!ffmpeg_mp3dec_Private->avCodec) {
		DEBUG(DEB_LEV_ERR, "Codec Not found\n");
		ffmpeg_mp3dec_Panic();
	}
	ffmpeg_mp3dec_Private->avCodecContext = avcodec_alloc_context();
    //open it
    if (avcodec_open(ffmpeg_mp3dec_Private->avCodecContext, ffmpeg_mp3dec_Private->avCodec) < 0) {
		DEBUG(DEB_LEV_ERR, "Could not open codec\n");
		ffmpeg_mp3dec_Panic();
    }
}

/** 
	It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder
*/
void ffmpeg_mp3dec_ffmpegLibDeInit(ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private) {
	
	avcodec_close(ffmpeg_mp3dec_Private->avCodecContext);

	if (ffmpeg_mp3dec_Private->avCodecContext->priv_data)
		avcodec_close (ffmpeg_mp3dec_Private->avCodecContext);
	
	if (ffmpeg_mp3dec_Private->avCodecContext->extradata) {
		av_free (ffmpeg_mp3dec_Private->avCodecContext->extradata);
		ffmpeg_mp3dec_Private->avCodecContext->extradata = NULL;
	}
	/*Free Codec Context*/
	av_free (ffmpeg_mp3dec_Private->avCodecContext);
    
}

/** This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the ffmpeg_mp3dec component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated ffmpeg_mp3dec_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE ffmpeg_mp3dec_Constructor(stComponentType* stComponent)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private;
	OMX_S32 testThId;
	pthread_t testThread;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noMp3decInstance);

	/** Allocate and fill in ffmpeg_mp3dec private structures
	 * These include output port buffer queue and flow control semaphore
	 */
	

	stComponent->omx_component.pComponentPrivate = malloc(sizeof(ffmpeg_mp3dec_PrivateType));
	if(stComponent->omx_component.pComponentPrivate==NULL)
		return OMX_ErrorInsufficientResources;
	memset(stComponent->omx_component.pComponentPrivate, 0, sizeof(ffmpeg_mp3dec_PrivateType));

	ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	/** Default parameters setting */
	ffmpeg_mp3dec_Private->bIsInit = OMX_FALSE;
	ffmpeg_mp3dec_Private->nGroupPriority = 0;
	ffmpeg_mp3dec_Private->nGroupID = 0;
	ffmpeg_mp3dec_Private->pMark=NULL;
	ffmpeg_mp3dec_Private->bCmdUnderProcess=OMX_FALSE;
	ffmpeg_mp3dec_Private->bWaitingForCmdToFinish=OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_FALSE;
	ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_FALSE;

	ffmpeg_mp3dec_Private->inputPort.hTunneledComponent=NULL;
	ffmpeg_mp3dec_Private->inputPort.nTunneledPort=0;
	ffmpeg_mp3dec_Private->inputPort.nTunnelFlags=0;

	ffmpeg_mp3dec_Private->outputPort.hTunneledComponent=NULL;
	ffmpeg_mp3dec_Private->outputPort.nTunneledPort=0;
	ffmpeg_mp3dec_Private->outputPort.nTunnelFlags=0;
		 
	/** Generic section for the input port. */
	ffmpeg_mp3dec_Private->inputPort.nNumAssignedBuffers = 0;
	ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateMax;
	setHeader(&ffmpeg_mp3dec_Private->inputPort.sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
	ffmpeg_mp3dec_Private->inputPort.sPortParam.nPortIndex = 0;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.eDir = OMX_DirInput;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.nBufferCountActual = 1;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.nBufferCountMin = 1;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.nBufferSize = 0;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
	
	/** Domain specific section for the intput port. The following parameters are only an example */
	ffmpeg_mp3dec_Private->inputPort.sPortParam.eDomain = OMX_PortDomainAudio;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.format.audio.cMIMEType = "audio/mpeg";
	ffmpeg_mp3dec_Private->inputPort.sPortParam.format.audio.pNativeRender = 0;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

	setHeader(&ffmpeg_mp3dec_Private->inputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	ffmpeg_mp3dec_Private->inputPort.sAudioParam.nPortIndex = 0;
	ffmpeg_mp3dec_Private->inputPort.sAudioParam.nIndex = 0;
	ffmpeg_mp3dec_Private->inputPort.sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
	
	/** Generic section for the output port. */
	ffmpeg_mp3dec_Private->outputPort.nNumAssignedBuffers = 0;
	ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateMax;
	setHeader(&ffmpeg_mp3dec_Private->outputPort.sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
	ffmpeg_mp3dec_Private->outputPort.sPortParam.nPortIndex = 1;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.eDir = OMX_DirOutput;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.nBufferCountActual = 1;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.nBufferCountMin = 1;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.nBufferSize = 0;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_TRUE;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_FALSE;
	
	/** Domain specific section for the output port. The following parameters are only an example */
	ffmpeg_mp3dec_Private->outputPort.sPortParam.eDomain = OMX_PortDomainAudio;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.format.audio.cMIMEType = "raw";
	ffmpeg_mp3dec_Private->outputPort.sPortParam.format.audio.pNativeRender = 0;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	ffmpeg_mp3dec_Private->outputPort.sPortParam.format.audio.eEncoding = 0;

	setHeader(&ffmpeg_mp3dec_Private->outputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	ffmpeg_mp3dec_Private->outputPort.sAudioParam.nPortIndex = 1;
	ffmpeg_mp3dec_Private->outputPort.sAudioParam.nIndex = 0;
	ffmpeg_mp3dec_Private->outputPort.sAudioParam.eEncoding = 0;

	/* generic parameter NOT related to a specific port */
	setHeader(&ffmpeg_mp3dec_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
	ffmpeg_mp3dec_Private->sPortTypesParam.nPorts = 2;
	ffmpeg_mp3dec_Private->sPortTypesParam.nStartPortNumber = 0;


	/** Allocate and initialize the input and output semaphores and excuting condition */
	ffmpeg_mp3dec_Private->outputPort.pBufferSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->outputPort.pBufferSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->outputPort.pBufferSem, 0);
	ffmpeg_mp3dec_Private->inputPort.pBufferSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->inputPort.pBufferSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pBufferSem, 0);

	ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem, 0);
	ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem, 0);

	/** Allocate and initialize input and output queues */
	ffmpeg_mp3dec_Private->outputPort.pBufferQueue = malloc(sizeof(queue_t));
	if(ffmpeg_mp3dec_Private->outputPort.pBufferQueue==NULL)
		return OMX_ErrorInsufficientResources;
	queue_init(ffmpeg_mp3dec_Private->outputPort.pBufferQueue);
	ffmpeg_mp3dec_Private->inputPort.pBufferQueue = malloc(sizeof(queue_t));
	if(ffmpeg_mp3dec_Private->inputPort.pBufferQueue==NULL)
		return OMX_ErrorInsufficientResources;
	queue_init(ffmpeg_mp3dec_Private->inputPort.pBufferQueue);

	ffmpeg_mp3dec_Private->outputPort.pFlushSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->outputPort.pFlushSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->outputPort.pFlushSem, 0);
	ffmpeg_mp3dec_Private->inputPort.pFlushSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->inputPort.pFlushSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pFlushSem, 0);

	ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess=OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess=OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
	ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_FALSE;

	pthread_mutex_init(&ffmpeg_mp3dec_Private->exit_mutex, NULL);
	pthread_cond_init(&ffmpeg_mp3dec_Private->exit_condition, NULL);
	pthread_mutex_init(&ffmpeg_mp3dec_Private->cmd_mutex, NULL);

	ffmpeg_mp3dec_Private->pCmdSem = malloc(sizeof(tsem_t));
	if(ffmpeg_mp3dec_Private->pCmdSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(ffmpeg_mp3dec_Private->pCmdSem, 0);

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
	ffmpeg_mp3dec_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);

	ffmpeg_mp3dec_Private->inbuffer=0;
	ffmpeg_mp3dec_Private->inbuffercb=0;
	ffmpeg_mp3dec_Private->outbuffer=0;
	ffmpeg_mp3dec_Private->outbuffercb=0;

	ffmpeg_mp3dec_Private->avCodec = NULL;
	ffmpeg_mp3dec_Private->avCodecContext= NULL;
	ffmpeg_mp3dec_Private->avcodecReady = OMX_FALSE;

	DEBUG(DEB_LEV_FULL_SEQ,"Returning from %s\n",__func__);
	/** Two meta-states used for the implementation 
		* of the transition mechanism loaded->idle and idle->loaded 
		*/
	noMp3decInstance++;
	if(noMp3decInstance > MAX_NUM_OF_MP3DEC_INSTANCES) 
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
OMX_ERRORTYPE ffmpeg_mp3dec_MessageHandler(coreMessage_t* message)
{
	stComponentType* stComponent = message->stComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private;
	OMX_BOOL waitFlag=OMX_FALSE;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	
	OMX_ERRORTYPE err;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;

	/** Dealing with a SendCommand call.
	 * -messageParam1 contains the command to execute
	 * -messageParam2 contains the parameter of the command
	 *  (destination state in case of a state change command).
	 */

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->cmd_mutex);
	ffmpeg_mp3dec_Private->bCmdUnderProcess=OMX_TRUE;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->cmd_mutex);
	
	if(message->messageType == SENDCOMMAND_MSG_TYPE){
		switch(message->messageParam1){
			case OMX_CommandStateSet:
				/* Do the actual state change */
				err = ffmpeg_mp3dec_DoStateSet(stComponent, message->messageParam2);
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
				err=ffmpeg_mp3dec_FlushPort(stComponent, message->messageParam2);
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
				ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_FALSE;
				ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_FALSE;
			break;
			case OMX_CommandPortDisable:
				/** This condition is added to pass the tests, it is not significant for the environment */
				err = ffmpeg_mp3dec_DisablePort(stComponent, message->messageParam2);
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
				err = ffmpeg_mp3dec_EnablePort(stComponent, message->messageParam2);
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
				ffmpeg_mp3dec_Private->pMark=(OMX_MARKTYPE *)message->pCmdData;
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
					ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						NULL);
				} else if (message->messageParam2 == 1) {
					ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_FALSE;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventCmdComplete, /* The command was completed */
						OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
						1, /* The state has been changed in message->messageParam2 */
						NULL);
				} else if (message->messageParam2 == -1) {
					ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
					ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_FALSE;
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
			break;
			default:	
				DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageParam1);
			break;
		}		
	}

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->cmd_mutex);
	ffmpeg_mp3dec_Private->bCmdUnderProcess=OMX_FALSE;
	waitFlag=ffmpeg_mp3dec_Private->bWaitingForCmdToFinish;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->cmd_mutex);

	if(waitFlag==OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: Signalling command finish condition \n", __func__);
		tsem_up(ffmpeg_mp3dec_Private->pCmdSem);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s: \n", __func__);

	return OMX_ErrorNone;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* ffmpeg_mp3dec_BufferMgmtFunction(void* param) {
	
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	OMX_BOOL exit_thread = OMX_FALSE;
	OMX_BOOL isInputBufferEnded,flag;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;
	OMX_U32  nFlags;
	OMX_MARKTYPE *pMark;
	OMX_BOOL *inbufferUnderProcess=&ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess;
	pthread_mutex_t *inmutex=&ffmpeg_mp3dec_Private->inputPort.mutex;
	pthread_mutex_t *outmutex=&ffmpeg_mp3dec_Private->outputPort.mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &ffmpeg_mp3dec_Private->executingMutex;
	pthread_cond_t* executingCondition = &ffmpeg_mp3dec_Private->executingCondition;

	
	OMX_U32 len = 0;
	OMX_U8* inputBuffer;
	OMX_U8* inputCurrBuffer;
	OMX_U32 inputLength;
	OMX_U32 inputCurrLength;
	
	OMX_U8* outputBuffer;
	OMX_U8* outputCurrBuffer;
	OMX_U32 outputLength;
	OMX_U32 outputCurrLength;

	OMX_U8* internalOutputBuffer;
	OMX_U32 internalOutputLength;
	OMX_S32 internalOutputFilled;
	OMX_S32 isFirstBuffer;
	OMX_S32 positionInOutBuf = 0;
	OMX_S32 outputfilled = 0;
	OMX_S32 ret=0;
	int oldtype = 0;

	ret=pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
	
	if (!ffmpeg_mp3dec_Private->avcodecReady) {
		ffmpeg_mp3dec_ffmpegLibInit(ffmpeg_mp3dec_Private);
		ffmpeg_mp3dec_Private->avcodecReady = OMX_TRUE;
	}
	isFirstBuffer = 1;
	
	internalOutputBuffer = malloc(INTERNAL_OUT_BUFFER_SIZE);
	internalOutputLength = INTERNAL_OUT_BUFFER_SIZE;
	internalOutputFilled = 0;
	memset(internalOutputBuffer, 0, internalOutputLength);
	

	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting || 
				((ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateIdle) && (ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateIdle))){
/*
		if(stComponent->state!=OMX_StateExecuting) {
			pthread_cond_wait(executingCondition,executingMutex);
		}
	*/
		DEBUG(DEB_LEV_FULL_SEQ, "Waiting for input buffer semval=%d \n",inputSem->semval);
		tsem_down(inputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
		exit_thread = ffmpeg_mp3dec_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
		if(exit_thread == OMX_TRUE) {
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}

		pthread_mutex_lock(inmutex);
		*inbufferUnderProcess = OMX_TRUE;
		pthread_mutex_unlock(inmutex);

		inputbuffer = dequeue(inputQueue);
		if(inputbuffer == NULL){
			DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
			ffmpeg_mp3dec_Panic();
		}
		nFlags=inputbuffer->nFlags;
		if(nFlags==OMX_BUFFERFLAG_EOS) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
		}
		if(ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed==OMX_TRUE) {
			/*Return Input Buffer*/
			goto LOOP1;
		}

		inputBuffer = inputbuffer->pBuffer;
		inputCurrBuffer = inputBuffer;
		inputLength = inputbuffer->nFilledLen;
		inputCurrLength = inputLength;

		len = 0;
		positionInOutBuf = 0;

		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded) {
			DEBUG(DEB_LEV_FULL_SEQ, "Waiting for output buffer\n");
			tsem_down(outputSem);
			pthread_mutex_lock(outmutex);
			*outbufferUnderProcess = OMX_TRUE;
			pthread_mutex_unlock(outmutex);
			DEBUG(DEB_LEV_FULL_SEQ, "Output buffer arrived\n");
			if(ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed==OMX_TRUE) {
				/*Return Input Buffer*/
				goto LOOP1;
			}
			
			pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
			exit_thread=ffmpeg_mp3dec_Private->bExit_buffer_thread;
			pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
			
			if(exit_thread==OMX_TRUE) {
				DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
				pthread_mutex_lock(outmutex);
				*outbufferUnderProcess = OMX_FALSE;
				pthread_mutex_unlock(outmutex);
				break;
			}

			/* Get a new buffer from the output queue */
			outputbuffer = dequeue(outputQueue);
			outputfilled = 0;
			if(outputbuffer == NULL) {
				DEBUG(DEB_LEV_ERR, "%s: Ouch! we got an empty buffer header\n", __func__);
				ffmpeg_mp3dec_Panic();
			}

			if(ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed==OMX_TRUE) {
				goto LOOP;
			}
			
			if(ffmpeg_mp3dec_Private->pMark!=NULL){
				outputbuffer->hMarkTargetComponent=ffmpeg_mp3dec_Private->pMark->hMarkTargetComponent;
				outputbuffer->pMarkData=ffmpeg_mp3dec_Private->pMark->pMarkData;
				ffmpeg_mp3dec_Private->pMark=NULL;
			}
			target_component=(OMX_COMPONENTTYPE*)inputbuffer->hMarkTargetComponent;
			if(target_component==(OMX_COMPONENTTYPE *)&stComponent->omx_component) {
				/*Clear the mark and generate an event*/
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventMark, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						inputbuffer->pMarkData);
			} else if(inputbuffer->hMarkTargetComponent!=NULL){
				/*If this is not the target component then pass the mark*/
				outputbuffer->hMarkTargetComponent	= inputbuffer->hMarkTargetComponent;
				outputbuffer->pMarkData				= inputbuffer->pMarkData;
			}
			if(nFlags==OMX_BUFFERFLAG_EOS) {
				outputbuffer->nFlags=nFlags;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventBufferFlag, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						nFlags, /* The state has been changed in message->messageParam2 */
						NULL);
			}
			outputBuffer = outputbuffer->pBuffer;
			outputCurrBuffer = outputBuffer;
			outputLength = outputbuffer->nAllocLen;
			outputCurrLength = outputLength;
			outputbuffer->nFilledLen = 0;
			
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);
			while (!outputfilled) {
				if (isFirstBuffer) {
					len = avcodec_decode_audio(ffmpeg_mp3dec_Private->avCodecContext, (short*)internalOutputBuffer, (int*)&internalOutputFilled,
																			inputCurrBuffer, inputCurrLength);
					DEBUG(DEB_LEV_FULL_SEQ, "Frequency = %i channels = %i\n", ffmpeg_mp3dec_Private->avCodecContext->sample_rate, ffmpeg_mp3dec_Private->avCodecContext->channels);
					ffmpeg_mp3dec_Private->minBufferLength = internalOutputFilled;
					DEBUG(DEB_LEV_FULL_SEQ, "buffer length %i buffer given %i\n", (OMX_S32)internalOutputFilled, (OMX_S32)outputLength);
				
					if (internalOutputFilled > outputLength) {
						DEBUG(DEB_LEV_ERR, "---> Ouch! the output file is too small!!!!\n");
						ffmpeg_mp3dec_Panic();
					}
				} else {
					len = avcodec_decode_audio(ffmpeg_mp3dec_Private->avCodecContext, (short*)(outputCurrBuffer + (positionInOutBuf * ffmpeg_mp3dec_Private->minBufferLength)), (int*)&internalOutputFilled,
																			inputCurrBuffer, inputCurrLength);
				}
				if (len < 0){
					DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
				}
				if (internalOutputFilled > 0) {
					inputCurrBuffer += len;
					inputCurrLength -= len;
				} else {
					/**  This condition becomes true when the input buffer has completely be consumed.
						*  In this case is immediately switched because there is no real buffer consumption */
					isInputBufferEnded = 1;
				}

				if (isFirstBuffer) {
					memcpy(outputCurrBuffer, internalOutputBuffer, internalOutputFilled);
					isFirstBuffer = 0;
				}
				if (internalOutputFilled > 0) {
					outputbuffer->nFilledLen += internalOutputFilled;
				}

				/* We are done with output buffer */
				positionInOutBuf++;
				
				if ((ffmpeg_mp3dec_Private->minBufferLength > (outputLength - (positionInOutBuf * ffmpeg_mp3dec_Private->minBufferLength))) || (internalOutputFilled <= 0)) {
					internalOutputFilled = 0;
					positionInOutBuf = 0;
					outputfilled = 1;
					/*Send the output buffer*/
				}
			}

			/*Wait if state is pause*/
			if(stComponent->state==OMX_StatePause) {
				if(ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem!=OMX_TRUE) {
					pthread_cond_wait(executingCondition,executingMutex);
				}
			}

			/*Call ETB in case port is enabled*/
LOOP:
			if (ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
				if(ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled==OMX_TRUE){
					OMX_EmptyThisBuffer(ffmpeg_mp3dec_Private->outputPort.hTunneledComponent, outputbuffer);
				}
				else { /*Port Disabled then call ETB if port is not the supplier else dont call*/
					if(!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))
						OMX_EmptyThisBuffer(ffmpeg_mp3dec_Private->outputPort.hTunneledComponent, outputbuffer);
				}
			} else {
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, outputbuffer);
			}

			pthread_mutex_lock(outmutex);
			*outbufferUnderProcess = OMX_FALSE;
			flag=ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem;
			pthread_mutex_unlock(outmutex);
			if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
				(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				if(outputSem->semval==ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer)
					if(flag==OMX_TRUE) {
						tsem_up(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
					}
			}else{
				if(flag==OMX_TRUE) {
				tsem_up(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
				}
			}
			ffmpeg_mp3dec_Private->outbuffercb++;
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

LOOP1:
		if (ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
			/*Call FTB in case port is enabled*/
			if(ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled==OMX_TRUE){
				OMX_FillThisBuffer(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent,inputbuffer);
				ffmpeg_mp3dec_Private->inbuffercb++;
			}
			else { /*Port Disabled then call FTB if port is not the supplier else dont call*/
				if(!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					 OMX_FillThisBuffer(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent,inputbuffer);
					 ffmpeg_mp3dec_Private->inbuffercb++;
				}
			}
		} else {
			(*(stComponent->callbacks->EmptyBufferDone))
				(pHandle, stComponent->callbackData,inputbuffer);
			ffmpeg_mp3dec_Private->inbuffercb++;
		}
		pthread_mutex_lock(inmutex);
		*inbufferUnderProcess = OMX_FALSE;
		flag=ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem;
		pthread_mutex_unlock(inmutex);
		if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			if(inputSem->semval==ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer)
				if(flag==OMX_TRUE) {
					tsem_up(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
				}
		}else{
			if(flag==OMX_TRUE) {
			tsem_up(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			}
		}
		pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
		exit_thread=ffmpeg_mp3dec_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
		
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
 * For ffmpeg_mp3dec component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE ffmpeg_mp3dec_Init(stComponentType* stComponent)
{

	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (ffmpeg_mp3dec_Private->bIsInit == OMX_TRUE) {
		DEBUG(DEB_LEV_ERR, "In %s Big error. It should not happen\n", __func__);
		return OMX_ErrorIncorrectStateOperation;
	}
	ffmpeg_mp3dec_Private->bIsInit = OMX_TRUE;

	/*re-intialize buffer semaphore and allocation semaphore*/
	tsem_init(ffmpeg_mp3dec_Private->outputPort.pBufferSem, 0);
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pBufferSem, 0);

	tsem_init(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem, 0);
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem, 0);

	/** initialize/reinitialize input and output queues */
	queue_init(ffmpeg_mp3dec_Private->outputPort.pBufferQueue);
	queue_init(ffmpeg_mp3dec_Private->inputPort.pBufferQueue);

	tsem_init(ffmpeg_mp3dec_Private->outputPort.pFlushSem, 0);
	tsem_init(ffmpeg_mp3dec_Private->inputPort.pFlushSem, 0);
	
	ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess=OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess=OMX_FALSE;
	ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
	ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_FALSE;
	pthread_mutex_init(&ffmpeg_mp3dec_Private->inputPort.mutex, NULL);
	pthread_mutex_init(&ffmpeg_mp3dec_Private->outputPort.mutex, NULL);
	
	pthread_cond_init(&ffmpeg_mp3dec_Private->executingCondition, NULL);
	pthread_mutex_init(&ffmpeg_mp3dec_Private->executingMutex, NULL);
	
	pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
	ffmpeg_mp3dec_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
	
	/** Spawn buffer management thread */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hey, starting threads!\n");
	ffmpeg_mp3dec_Private->bufferMgmtThreadID = pthread_create(&ffmpeg_mp3dec_Private->bufferMgmtThread,
		NULL,
		ffmpeg_mp3dec_BufferMgmtFunction,
		stComponent);	

	if(ffmpeg_mp3dec_Private->bufferMgmtThreadID < 0){
		DEBUG(DEB_LEV_ERR, "In starting buffer management thread failed\n");
		return OMX_ErrorUndefined;
	}	

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s\n", __func__);

	return OMX_ErrorNone;
}

/** This function is called upon a transition to the idle state.
 * The two buffer management threads are stopped and their message
 * queues destroyed.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_Deinit(stComponentType* stComponent)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	OMX_S32 ret;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s\n", __func__);
	ffmpeg_mp3dec_Private->bIsInit = OMX_FALSE;
	
	/** Trash buffer mangement thread.
	 */

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
	ffmpeg_mp3dec_Private->bExit_buffer_thread=OMX_TRUE;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
	
	DEBUG(DEB_LEV_FULL_SEQ,"In %s ibsemval=%d, bup=%d\n",__func__,
		inputSem->semval, 
		ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess);

	if(inputSem->semval == 0 && 
		ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
		tsem_up(inputSem);
	}

	DEBUG(DEB_LEV_FULL_SEQ,"In %s obsemval=%d, bup=%d\n",__func__,
		outputSem->semval, 
		ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess);
	if(outputSem->semval == 0 && 
		ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess==OMX_FALSE) {
		tsem_up(outputSem);
	}

	DEBUG(DEB_LEV_FULL_SEQ,"All buffers flushed!\n");
	DEBUG(DEB_LEV_FULL_SEQ,"In %s obsemval=%d, ibsemval=%d\n",__func__,
		outputSem->semval, 
		inputSem->semval);
	ret=pthread_join(ffmpeg_mp3dec_Private->bufferMgmtThread,NULL);

	DEBUG(DEB_LEV_FULL_SEQ,"In %s obsemval=%d, ibsemval=%d\n",__func__,
		outputSem->semval, 
		inputSem->semval);

	while(inputSem->semval>0 ) {
		tsem_down(inputSem);
		inputbuffer=dequeue(ffmpeg_mp3dec_Private->inputPort.pBufferQueue);
	}
	while(outputSem->semval>0) {
		tsem_down(outputSem);
		inputbuffer=dequeue(ffmpeg_mp3dec_Private->outputPort.pBufferQueue);
	}


	pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
	ffmpeg_mp3dec_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);

	DEBUG(DEB_LEV_FULL_SEQ,"Execution thread re-joined\n");
	/*Deinitialize mutexes and conditional variables*/
	pthread_cond_destroy(&ffmpeg_mp3dec_Private->executingCondition);
	pthread_mutex_destroy(&ffmpeg_mp3dec_Private->executingMutex);
	DEBUG(DEB_LEV_FULL_SEQ,"Deinitialize mutexes and conditional variables\n");
	
	pthread_mutex_destroy(&ffmpeg_mp3dec_Private->inputPort.mutex);
	pthread_mutex_destroy(&ffmpeg_mp3dec_Private->outputPort.mutex);

	pthread_cond_signal(&ffmpeg_mp3dec_Private->exit_condition);

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);

	return OMX_ErrorNone;
}

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_Destructor(stComponentType* stComponent)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 ret;
	OMX_BOOL exit_thread=OMX_FALSE,cmdFlag=OMX_FALSE;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noMp3decInstance);
	if (ffmpeg_mp3dec_Private->bIsInit != OMX_FALSE) {
		ffmpeg_mp3dec_Deinit(stComponent);
	} 

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->exit_mutex);
	exit_thread = ffmpeg_mp3dec_Private->bExit_buffer_thread;
	pthread_mutex_unlock(&ffmpeg_mp3dec_Private->exit_mutex);
	if(exit_thread == OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for condition...\n",__func__);
		pthread_cond_wait(&ffmpeg_mp3dec_Private->exit_condition,&ffmpeg_mp3dec_Private->exit_mutex);
	}

	pthread_mutex_lock(&ffmpeg_mp3dec_Private->cmd_mutex);
	cmdFlag=ffmpeg_mp3dec_Private->bCmdUnderProcess;

	if(cmdFlag==OMX_TRUE) {
		ffmpeg_mp3dec_Private->bWaitingForCmdToFinish=OMX_TRUE;
		pthread_mutex_unlock(&ffmpeg_mp3dec_Private->cmd_mutex);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish ...\n",__func__);
		tsem_down(ffmpeg_mp3dec_Private->pCmdSem);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish over..cmutex=%x.\n",__func__,&ffmpeg_mp3dec_Private->cmd_mutex);
		
		pthread_mutex_lock(&ffmpeg_mp3dec_Private->cmd_mutex);
		ffmpeg_mp3dec_Private->bWaitingForCmdToFinish=OMX_FALSE;
		pthread_mutex_unlock(&ffmpeg_mp3dec_Private->cmd_mutex);
	}
	else {
		pthread_mutex_unlock(&ffmpeg_mp3dec_Private->cmd_mutex);
	}
	
	pthread_cond_destroy(&ffmpeg_mp3dec_Private->exit_condition);
	pthread_mutex_destroy(&ffmpeg_mp3dec_Private->exit_mutex);
	pthread_mutex_destroy(&ffmpeg_mp3dec_Private->cmd_mutex);
	
	if(ffmpeg_mp3dec_Private->pCmdSem!=NULL)  {
	tsem_deinit(ffmpeg_mp3dec_Private->pCmdSem);
	free(ffmpeg_mp3dec_Private->pCmdSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pCmdSem semaphore\n");
	}

	/*Destroying and Freeing input semaphore */
	if(ffmpeg_mp3dec_Private->inputPort.pBufferSem!=NULL)  {
	tsem_deinit(ffmpeg_mp3dec_Private->inputPort.pBufferSem);
	free(ffmpeg_mp3dec_Private->inputPort.pBufferSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input semaphore\n");
	}
	/*Destroying and Freeing output semaphore */
	if(ffmpeg_mp3dec_Private->outputPort.pBufferSem!=NULL) {
	tsem_deinit(ffmpeg_mp3dec_Private->outputPort.pBufferSem);
	free(ffmpeg_mp3dec_Private->outputPort.pBufferSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing output semaphore\n");
	}

	/*Destroying and Freeing input queue */
	if(ffmpeg_mp3dec_Private->inputPort.pBufferQueue!=NULL) {
	DEBUG(DEB_LEV_SIMPLE_SEQ,"deiniting ffmpeg_mp3dec input queue\n");
	queue_deinit(ffmpeg_mp3dec_Private->inputPort.pBufferQueue);
	free(ffmpeg_mp3dec_Private->inputPort.pBufferQueue);
	ffmpeg_mp3dec_Private->inputPort.pBufferQueue=NULL;
	}
	/*Destroying and Freeing output queue */
	if(ffmpeg_mp3dec_Private->outputPort.pBufferQueue!=NULL) {
	DEBUG(DEB_LEV_SIMPLE_SEQ,"deiniting ffmpeg_mp3dec output queue\n");
	queue_deinit(ffmpeg_mp3dec_Private->outputPort.pBufferQueue);
	free(ffmpeg_mp3dec_Private->outputPort.pBufferQueue);
	ffmpeg_mp3dec_Private->outputPort.pBufferQueue=NULL;
	}

	/*Destroying and Freeing input semaphore */
	if(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem!=NULL)  {
	tsem_deinit(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
	free(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input pFullAllocationSem semaphore\n");
	}
	/*Destroying and Freeing output semaphore */
	if(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem!=NULL) {
	tsem_deinit(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
	free(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing output pFullAllocationSem semaphore\n");
	}

	if(ffmpeg_mp3dec_Private->outputPort.pFlushSem!=NULL) {
	tsem_deinit(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
	free(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing output pFlushSem semaphore\n");
	}

	if(ffmpeg_mp3dec_Private->inputPort.pFlushSem!=NULL) {
	tsem_deinit(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
	free(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing input pFlushSem semaphore\n");
	}

	stComponent->state = OMX_StateInvalid;
	
	free(stComponent->omx_component.pComponentPrivate);

	noMp3decInstance--;

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);
	return OMX_ErrorNone;
}

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param destinationState the requested target state.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_DoStateSet(stComponentType* stComponent, OMX_U32 destinationState)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	pthread_mutex_t* executingMutex = &ffmpeg_mp3dec_Private->executingMutex;
	pthread_cond_t* executingCondition = &ffmpeg_mp3dec_Private->executingCondition;
	pthread_mutex_t *inmutex=&ffmpeg_mp3dec_Private->inputPort.mutex;
	pthread_mutex_t *outmutex=&ffmpeg_mp3dec_Private->outputPort.mutex;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;

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
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  input enabled %i ,  input populated %i\n", __func__, ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: output enabled %i , output populated %i\n", __func__, ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated);
				if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/* Freeing here the buffers allocated for the tunneling:*/
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: IN TUNNEL!!!! semval=%d\n",__func__,inputSem->semval);
					eError=ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort);
				} 
				else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
				}
				else {
					/** Wait until all the buffers assigned to the input port have been de-allocated
						*/
					if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled) && 
							(ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated)) {
						tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
						ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
						ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateMax;
					}
					else if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_FALSE) && 
						(ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_FALSE)) {
						if(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem->semval>0)
							tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
					}
				}
				if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/* Freeing here the buffers allocated for the tunneling:*/
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: OUT TUNNEL!!!!\n",__func__);
					eError=ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort);
					
				} else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) { 
					DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s waiting for the supplier to free the buffer %d\n",
						__func__,ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem->semval);
					tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
				}
				else {
					/** Wait until all the buffers assigned to the output port have been de-allocated
						*/
					if ((ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled) && 
							(ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated)) {
						tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
						ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_FALSE;
						ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateMax;
					}
					else if ((ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_FALSE) && 
						(ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_FALSE)) {
						if(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem->semval>0)
							tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
					}
				}

				tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
				tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);

				stComponent->state = OMX_StateLoaded;
	
				ffmpeg_mp3dec_Deinit(stComponent);
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
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  input enabled %i ,  input populated %i\n", __func__, ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: output enabled %i , output populated %i\n", __func__, ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : input flags  0x%x output flags 0x%x \n", 
					(OMX_S32)ffmpeg_mp3dec_Private->inputPort.nTunnelFlags, 
					(OMX_S32)ffmpeg_mp3dec_Private->outputPort.nTunnelFlags);

				if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/** Allocate here the buffers needed for the tunneling:
						*/
					eError=ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort, 0, INTERNAL_IN_BUFFER_SIZE);
					ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateMax;
				} else {
					/** Wait until all the buffers needed have been allocated
						*/
					if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) && 
							(ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_FALSE)) {
						DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait for buffers. input enabled %i ,  input populated %i\n", __func__, ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated);
						tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
						ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
						ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateMax;
					}
					else if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) && 
						(ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_TRUE)) {
						if(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem->semval>0)
						tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
					}
				}
				if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) &&
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
					/** Allocate here the buffers needed for the tunneling:
						*/
					eError=ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort, 1, INTERNAL_OUT_BUFFER_SIZE);
					ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateMax;
				} else {
					/** Wait until all the buffers needed have been allocated
						*/
					if ((ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_TRUE) && 
							(ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_FALSE)) {
						DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait for buffers. output enabled %i , output populated %i\n", __func__, ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled, ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated);
						tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
						ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
						ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateMax;
					}
					else if ((ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_TRUE) && 
						(ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_TRUE)) {
						if(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem->semval>0)
						tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
					}
				}
				stComponent->state = OMX_StateIdle;
				tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
				tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
				
				DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : input flags  0x%x\n", (OMX_S32)ffmpeg_mp3dec_Private->inputPort.nTunnelFlags);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "                     output flags 0x%x\n", (OMX_S32)ffmpeg_mp3dec_Private->outputPort.nTunnelFlags);
				break;
				
			case OMX_StateIdle:
					return OMX_ErrorSameState;
				break;
				
			case OMX_StateExecuting:
			case OMX_StatePause:
				DEBUG(DEB_LEV_ERR, "In %s: state transition exe/pause to idle asgn ibfr=%d , obfr=%d\n", __func__,
					ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer,
					ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer);
				ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_TRUE;
				ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_TRUE;
				ffmpeg_mp3dec_FlushPort(stComponent, -1);
				ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_FALSE;
				ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_FALSE;
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
			if (ffmpeg_mp3dec_Private->bIsInit != OMX_FALSE) {
			ffmpeg_mp3dec_Deinit(stComponent);
			}
			break;
			}
		
		if (ffmpeg_mp3dec_Private->bIsInit != OMX_FALSE) {
			ffmpeg_mp3dec_Deinit(stComponent);
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
OMX_ERRORTYPE ffmpeg_mp3dec_DisablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	if (portIndex == 0) {
		ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_TRUE;
		ffmpeg_mp3dec_FlushPort(stComponent, 0);
		ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_FALSE;
		ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
		DEBUG(DEB_LEV_FULL_SEQ,"ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem=%x\n",ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem->semval);
		if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_TRUE && ffmpeg_mp3dec_Private->bIsInit == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for in buffers to be freed\n", __func__);
				tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
			}
			else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is supplier no of buffers=%d\n",
					__func__,inputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(inputSem->semval==ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer) { 
					while(inputSem->semval>0) {
						tsem_down(inputSem);
						inputbuffer=dequeue(inputQueue);
					}
					ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort);
					ffmpeg_mp3dec_Private->inputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->inputPort.nTunnelFlags=0;
				}
			}
			else {
				if(inputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is not supplier port still has some buffer %d\n",
					__func__,inputSem->semval);
				if(inputSem->semval==0 && ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
					ffmpeg_mp3dec_Private->inputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->inputPort.nTunnelFlags=0;
				}
			}
			ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
		}
		else 
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Input port is not populated\n");
		
		DEBUG(DEB_LEV_FULL_SEQ, "In %s flush buffers\n", __func__);
		
		while(inputSem->semval < 0) {
			tsem_up(inputSem);
		}
		DEBUG(DEB_LEV_FULL_SEQ, "In %s wait for buffer to be de-allocated\n", __func__);
	} else if (portIndex == 1) {
		ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_TRUE;
		ffmpeg_mp3dec_FlushPort(stComponent, 1);
		ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_FALSE;

		ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_FALSE;
		if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_TRUE && ffmpeg_mp3dec_Private->bIsInit == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
			}
			else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s output Tunnel is supplier no of buffer=%d\n",
					__func__,outputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(outputSem->semval==ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer) {
					while(outputSem->semval>0) {
						tsem_down(outputSem);
						inputbuffer=dequeue(outputQueue);
					}
					ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort);
					ffmpeg_mp3dec_Private->outputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->outputPort.nTunnelFlags=0;
				}
			}
			else {
				if(outputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s output Tunnel is not supplier port still has some buffer %d\n",
					__func__,outputSem->semval);
				if(outputSem->semval==0 && ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess==OMX_FALSE) {
					ffmpeg_mp3dec_Private->outputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->outputPort.nTunnelFlags=0;
				}
			}
			ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_FALSE;
		}
		
		while(outputSem->semval < 0) {
			tsem_up(outputSem);
		}
	} else if (portIndex == -1) {
		ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_TRUE;
		ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_TRUE;
		ffmpeg_mp3dec_FlushPort(stComponent, -1);
		ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_FALSE;
		ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_FALSE;

		DEBUG(DEB_LEV_FULL_SEQ,"ffmpeg_mp3dec_DisablePort ip fASem=%x basn=%d op fAS=%x basn=%d\n",
			ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem->semval,
			ffmpeg_mp3dec_Private->inputPort.nNumAssignedBuffers,
			ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem->semval,
			ffmpeg_mp3dec_Private->outputPort.nNumAssignedBuffers);

		ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_FALSE;
		if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
			}
			else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is supplier no of buffers=%d\n",
					__func__,inputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(inputSem->semval==ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer) {
					while(inputSem->semval>0) {
						tsem_down(inputSem);
						inputbuffer=dequeue(inputQueue);
					}
					ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort);
					ffmpeg_mp3dec_Private->inputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->inputPort.nTunnelFlags=0;
				}
			}
			else {
				if(inputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s Input Tunnel is not supplier port still has some buffer %d\n",
					__func__,inputSem->semval);
				if(inputSem->semval==0 && ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess==OMX_FALSE) {
					ffmpeg_mp3dec_Private->inputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->inputPort.nTunnelFlags=0;
				}
			}
		}
		ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_FALSE;
		ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_FALSE;
		if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
			}
			else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"In %s output Tunnel is supplier no of buffer=%d\n",
					__func__,outputSem->semval);
				/*Free buffers and remove the tunnel*/
				if(outputSem->semval==ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer) {
					while(outputSem->semval>0) {
						tsem_down(outputSem);
						inputbuffer=dequeue(outputQueue);
					}
					ffmpeg_mp3dec_freeTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort);
					ffmpeg_mp3dec_Private->outputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->outputPort.nTunnelFlags=0;
				}
			}
			else {
				if(outputSem->semval>0)
					DEBUG(DEB_LEV_FULL_SEQ,"In %s output Tunnel is not supplier port still has some buffer %d\n",
					__func__,outputSem->semval);
				if(outputSem->semval==0 && ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess==OMX_FALSE) {
					ffmpeg_mp3dec_Private->outputPort.hTunneledComponent=NULL;
					ffmpeg_mp3dec_Private->outputPort.nTunnelFlags=0;
				}
			}
		}
		ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_FALSE;
		while(inputSem->semval < 0) {
			tsem_up(inputSem);
		}
		while(outputSem->semval < 0) {
			tsem_up(outputSem);
		}
	} else {
		return OMX_ErrorBadPortIndex;
	}
	tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
	tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting %s\n", __func__);
	return OMX_ErrorNone;
}

/** Enables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be enabled
	* @param portIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_EnablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"I/p Port.fAS=%x, O/p Port fAS=%d,is Init=%d\n",
		ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem->semval,
		ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem->semval,
		ffmpeg_mp3dec_Private->bIsInit);
	if (portIndex == 0) {
		ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
		if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_FALSE && ffmpeg_mp3dec_Private->bIsInit == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
					ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"I/p Port buffer sem =%x \n",
					inputSem->semval);
				ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort, 0, INTERNAL_IN_BUFFER_SIZE);
				ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;

			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
			//ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
		}
		
	} else if (portIndex == 1) {
		ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_TRUE;
		if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_FALSE && ffmpeg_mp3dec_Private->bIsInit == OMX_TRUE) {
			if (!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
					ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}
			else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"O/p Port buffer sem =%x \n",
					outputSem->semval);
				ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort, 0, INTERNAL_OUT_BUFFER_SIZE);
				ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
			//ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
		}
		
	} else if (portIndex == -1) {
		ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled = OMX_TRUE;
		ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled = OMX_TRUE;
		if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated == OMX_FALSE) {
			if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
					ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}
			else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"I/p Port buffer sem =%x \n",
					inputSem->semval);
				ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->inputPort, 0, INTERNAL_IN_BUFFER_SIZE);
				ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
		}
		if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated == OMX_FALSE) {
			if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
				if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
					tsem_down(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
					ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
				}
			}
			else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
						(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
				DEBUG(DEB_LEV_FULL_SEQ,"O/p Port buffer sem =%x \n",
					outputSem->semval);
				ffmpeg_mp3dec_allocateTunnelBuffers(&ffmpeg_mp3dec_Private->outputPort, 0, INTERNAL_OUT_BUFFER_SIZE);
				ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
			}
			else {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
			}
		}
		//ffmpeg_mp3dec_Private->inputPort.sPortParam.bPopulated = OMX_TRUE;
		//ffmpeg_mp3dec_Private->outputPort.sPortParam.bPopulated = OMX_TRUE;
	} else {
		return OMX_ErrorBadPortIndex;
	}
	tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
	tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
	return OMX_ErrorNone;
}

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	OMX_BUFFERHEADERTYPE* outputbuffer;
	OMX_BUFFERHEADERTYPE* inputbuffer;
	OMX_BOOL *inbufferUnderProcess=&ffmpeg_mp3dec_Private->inputPort.bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&ffmpeg_mp3dec_Private->outputPort.bBufferUnderProcess;
	pthread_mutex_t *inmutex=&ffmpeg_mp3dec_Private->inputPort.mutex;
	pthread_mutex_t *outmutex=&ffmpeg_mp3dec_Private->outputPort.mutex;
	pthread_cond_t* executingCondition = &ffmpeg_mp3dec_Private->executingCondition;
	OMX_BOOL flag;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%i\n", __func__,portIndex);
	if (portIndex == 0) {
		if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d outsemval=%d ib=%d,ibcb=%d\n",
				inputSem->semval,outputSem->semval,ffmpeg_mp3dec_Private->inbuffer,ffmpeg_mp3dec_Private->inbuffercb);
			if(outputSem->semval==0) {
				/*This is to free the input buffer being processed*/
				tsem_up(outputSem);
			}
			
			while(inputSem->semval>0) {
				tsem_down(inputSem);
				inputbuffer = dequeue(inputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, inputbuffer);
				ffmpeg_mp3dec_Private->inbuffercb++;
			}
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}

			/*Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
			}
			else {
				pthread_mutex_unlock(inmutex);
			}
			
			if(outputSem->semval>0) {
				/*This is to free the input buffer being processed*/
				tsem_down(outputSem);
			}
		}
		else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {
			if(outputSem->semval==0) {
				/*This is to free the input buffer being processed*/
				tsem_up(outputSem);
			}

			while(inputSem->semval>0) {
				tsem_down(inputSem);
				inputbuffer = dequeue(inputQueue);
				OMX_FillThisBuffer(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent, inputbuffer);
				ffmpeg_mp3dec_Private->inbuffercb++;
			}
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
			}
			else {
				pthread_mutex_unlock(inmutex);
			}
			
			if(outputSem->semval>0) {
				/*This is to free the input buffer being processed*/
				tsem_down(outputSem);
			}
		}
		else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			pthread_mutex_unlock(inmutex);
			if(inputSem->semval<ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
			}
		}
	} else if (portIndex == 1) {
		if (!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing output ports outsemval=%d ob=%d obcb=%d\n",
				outputSem->semval,ffmpeg_mp3dec_Private->outbuffer,ffmpeg_mp3dec_Private->outbuffercb);
			while(outputSem->semval>0) {
				tsem_down(outputSem);
				outputbuffer = dequeue(outputQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, outputbuffer);
				ffmpeg_mp3dec_Private->outbuffercb++;
			}
		}
		else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {
			while(outputSem->semval>0) {
				tsem_down(outputSem);
				outputbuffer = dequeue(outputQueue);
				OMX_EmptyThisBuffer(ffmpeg_mp3dec_Private->outputPort.hTunneledComponent, outputbuffer);
				ffmpeg_mp3dec_Private->outbuffercb++;
			}
		}
		else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(outmutex);
			flag=*outbufferUnderProcess;
			pthread_mutex_unlock(outmutex);
			if(outputSem->semval<ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(outmutex);
			ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(outmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Bufferoutg beoutg processed waitoutg for output flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
			pthread_mutex_lock(outmutex);
			ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(outmutex);
			}
		}
	} else if (portIndex == -1) {
		if (!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing all ports outsemval=%d insemval=%d ib=%d,ibcb=%d,ob=%d,obcb=%d\n",
				outputSem->semval,inputSem->semval,ffmpeg_mp3dec_Private->inbuffer,ffmpeg_mp3dec_Private->inbuffercb,ffmpeg_mp3dec_Private->outbuffer,ffmpeg_mp3dec_Private->outbuffercb);
			while(outputSem->semval>0) {
				tsem_down(outputSem);
				outputbuffer = dequeue(outputQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, outputbuffer);
				ffmpeg_mp3dec_Private->outbuffercb++;
			}
		}
		else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {
			while(outputSem->semval>0) {
				tsem_down(outputSem);
				outputbuffer = dequeue(outputQueue);
				OMX_EmptyThisBuffer(ffmpeg_mp3dec_Private->outputPort.hTunneledComponent, outputbuffer);
				ffmpeg_mp3dec_Private->outbuffercb++;
			}
		}
		else if ((ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(outmutex);
			flag=*outbufferUnderProcess;
			pthread_mutex_unlock(outmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s out port buffer=%d,no of tunlbuffer=%d flushsem=%d bup=%d\n", __func__,
				outputSem->semval,ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer,
				ffmpeg_mp3dec_Private->outputPort.pFlushSem->semval,flag);
			if(outputSem->semval<ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(outmutex);
			ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(outmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Bufferoutg beoutg processed waitoutg for output flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->outputPort.pFlushSem);
			pthread_mutex_lock(outmutex);
			ffmpeg_mp3dec_Private->outputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(outmutex);
			}
		}
		
		if (!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED)) {
			if(outputSem->semval==0) {
				/*This is to free the input buffer being processed*/
				tsem_up(outputSem);
			}
			while(inputSem->semval>0) {
				tsem_down(inputSem);
				inputbuffer = dequeue(inputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, inputbuffer);
				ffmpeg_mp3dec_Private->inbuffercb++;
			}
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*"Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
			}
			else {
				pthread_mutex_unlock(inmutex);
			}
			if(outputSem->semval>0) {
				/*This is to free the input buffer being processed*/
				tsem_down(outputSem);
			}
			DEBUG(DEB_LEV_FULL_SEQ,"Flashing input ports insemval=%d outsemval=%d ib=%d,ibcb=%d\n",
				inputSem->semval,outputSem->semval,ffmpeg_mp3dec_Private->inbuffer,ffmpeg_mp3dec_Private->inbuffercb);
		}
		else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER))) {
			if(outputSem->semval==0) {
				/*This is to free the input buffer being processed*/
				tsem_up(outputSem);
			}

			while(inputSem->semval>0) {
				tsem_down(inputSem);
				inputbuffer = dequeue(inputQueue);
				OMX_FillThisBuffer(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent, inputbuffer);
				ffmpeg_mp3dec_Private->inbuffercb++;
			}
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
			}
			else {
				pthread_mutex_unlock(inmutex);
			}
			
			if(outputSem->semval>0) {
				/*This is to free the input buffer being processed*/
				tsem_down(outputSem);
			}
		}
		else if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(inmutex);
			flag=*inbufferUnderProcess;
			pthread_mutex_unlock(inmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in port buffer=%d,no of tunlbuffer=%d flushsem=%d bup=%d\n", __func__,
				inputSem->semval,ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer,
				ffmpeg_mp3dec_Private->inputPort.pFlushSem->semval,flag);
			if(inputSem->semval<ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer) {
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(inmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(ffmpeg_mp3dec_Private->inputPort.pFlushSem);
			pthread_mutex_lock(inmutex);
			ffmpeg_mp3dec_Private->inputPort.bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(inmutex);
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
	* @param ffmpeg_mp3dec_Port the port of the alsa component that must be tunneled
	* @param portIndex index of the port to be tunneled
	* @param bufferSize Size of the buffers to be allocated
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_allocateTunnelBuffers(ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port, OMX_U32 portIndex, OMX_U32 bufferSize) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_U8* pBuffer=NULL;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<ffmpeg_mp3dec_Port->nNumTunnelBuffer;i++){
		if(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			ffmpeg_mp3dec_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer);
		} else if (ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ASSIGNED){
			ffmpeg_mp3dec_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	for(i=0;i<ffmpeg_mp3dec_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Allocating  %i buffer of size %i\n", i, (OMX_S32)bufferSize);
		pBuffer = malloc(bufferSize);
		DEBUG(DEB_LEV_FULL_SEQ, "   malloc done %x\n",pBuffer);
		eError=OMX_UseBuffer(ffmpeg_mp3dec_Port->hTunneledComponent,&ffmpeg_mp3dec_Port->pBuffer[i],
			ffmpeg_mp3dec_Port->nTunneledPort,NULL,bufferSize,pBuffer); 
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't free buffer %i \n",i);
			free(pBuffer);
			return eError;
		}
		if ((eError = checkHeader(ffmpeg_mp3dec_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong buffer header size=%d version=0x%08x\n", 
				__func__,ffmpeg_mp3dec_Port->pBuffer[i]->nSize,ffmpeg_mp3dec_Port->pBuffer[i]->nVersion.s.nVersionMajor);
			//return eError;
		}
		
		ffmpeg_mp3dec_Port->pBuffer[i]->nFilledLen = 0;
		ffmpeg_mp3dec_Port->nBufferState[i] |= BUFFER_ALLOCATED;
		
		queue(ffmpeg_mp3dec_Port->pBufferQueue, ffmpeg_mp3dec_Port->pBuffer[i]);
		tsem_up(ffmpeg_mp3dec_Port->pBufferSem);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "   queue done\n");
	}
	ffmpeg_mp3dec_Port->sPortParam.bPopulated = OMX_TRUE;
	
	return OMX_ErrorNone;
}

/** Free buffer in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Free the MAX number of buffers for that port, with the specified size.
	* @param ffmpeg_mp3dec_Port the port of the alsa component on which tunnel buffers must be released
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_freeTunnelBuffers(ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<ffmpeg_mp3dec_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Freeing  %i buffer %x\n", i,ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer);
	
		if(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer)
			free(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer);

		eError=OMX_FreeBuffer(ffmpeg_mp3dec_Port->hTunneledComponent,ffmpeg_mp3dec_Port->nTunneledPort,ffmpeg_mp3dec_Port->pBuffer[i]);
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't free buffer %i \n",i);
			return eError;
		} 
		
		DEBUG(DEB_LEV_FULL_SEQ, "   free done\n");
		ffmpeg_mp3dec_Port->pBuffer[i]->nAllocLen = 0;
		ffmpeg_mp3dec_Port->pBuffer[i]->pPlatformPrivate = NULL;
		ffmpeg_mp3dec_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
		ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = 0;
		ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = 0;
	}
	for(i=0;i<ffmpeg_mp3dec_Port->nNumTunnelBuffer;i++){
		if(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			ffmpeg_mp3dec_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer);
		} else if (ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ASSIGNED){
			ffmpeg_mp3dec_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	ffmpeg_mp3dec_Port->sPortParam.bPopulated = OMX_FALSE;
	return OMX_ErrorNone;
}

/************************************
 *
 * PUBLIC: OMX component entry points
 *
 ************************************/

OMX_ERRORTYPE ffmpeg_mp3dec_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING pComponentName,
	OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
	OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
	OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omx_component;
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

/** Ask the component current parameter settings.
 * Number of ports is fixed to two.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* parammgm;
	OMX_PARAM_BUFFERSUPPLIERTYPE *paramsupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *paramaudioformat;
	OMX_PARAM_PORTDEFINITIONTYPE *paramPort;
	OMX_AUDIO_PARAM_MP3TYPE * parammp3;
	OMX_AUDIO_PARAM_PCMMODETYPE* parampcm;
	OMX_PORT_PARAM_TYPE* paramPortDomains;
	
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omx_component;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			parammgm = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			setHeader(parammgm, sizeof(OMX_PRIORITYMGMTTYPE));
			parammgm->nGroupPriority = ffmpeg_mp3dec_Private->nGroupPriority;
			parammgm->nGroupID = ffmpeg_mp3dec_Private->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
			memcpy(ComponentParameterStructure, &ffmpeg_mp3dec_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;
		case OMX_IndexParamVideoInit:
		case OMX_IndexParamImageInit:
		case OMX_IndexParamOtherInit:
			paramPortDomains = (OMX_PORT_PARAM_TYPE*)ComponentParameterStructure;
			setHeader(paramPortDomains, sizeof(OMX_PORT_PARAM_TYPE));
			paramPortDomains->nPorts = 0;
			paramPortDomains->nStartPortNumber = 0;
		break;		
		case OMX_IndexParamPortDefinition:
			paramPort = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
			//setHeader(paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			if (paramPort->nPortIndex == 0) {
				memcpy(paramPort, &ffmpeg_mp3dec_Private->inputPort.sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else if (paramPort->nPortIndex == 1) {
				memcpy(paramPort, &ffmpeg_mp3dec_Private->outputPort.sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			} else {
				return OMX_ErrorBadPortIndex;
			}
		break;
		case OMX_IndexParamCompBufferSupplier:
			paramsupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
			if (paramsupply->nPortIndex == 0) {
				setHeader(paramsupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
				if (ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					paramsupply->eBufferSupplier = OMX_BufferSupplyInput;	
				} else if (ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
					paramsupply->eBufferSupplier = OMX_BufferSupplyOutput;	
				} else {
					paramsupply->eBufferSupplier = OMX_BufferSupplyUnspecified;	
				}
			} else if (paramsupply->nPortIndex == 1) {
				setHeader(paramsupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
				if (ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					paramsupply->eBufferSupplier = OMX_BufferSupplyOutput;	
				} else if (ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_ESTABLISHED) {
					paramsupply->eBufferSupplier = OMX_BufferSupplyInput;	
				} else {
					paramsupply->eBufferSupplier = OMX_BufferSupplyUnspecified;	
				}
			} else {
				return OMX_ErrorBadPortIndex;
			}

		break;
		case OMX_IndexParamAudioPortFormat:
		paramaudioformat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		if (paramaudioformat->nPortIndex == 0) {
			memcpy(paramaudioformat, &ffmpeg_mp3dec_Private->inputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else if (paramaudioformat->nPortIndex == 1) {
			memcpy(paramaudioformat, &ffmpeg_mp3dec_Private->outputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;
		case OMX_IndexParamAudioPcm:
			parampcm = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			setHeader(parampcm, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			if (parampcm->nPortIndex != 1) {
				return OMX_ErrorBadPortIndex;
			}
			parampcm->nChannels = 2;
			parampcm->eNumData = OMX_NumericalDataSigned;
			parampcm->eEndian = OMX_EndianBig;
			parampcm->bInterleaved = OMX_TRUE;
			parampcm->nBitPerSample = 16;
			parampcm->nSamplingRate = 0;
			parampcm->ePCMMode = OMX_AUDIO_PCMModeLinear;
		break;
		case OMX_IndexParamAudioMp3:
			parammp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			setHeader(parammp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			if (parammp3->nPortIndex != 0) {
				return OMX_ErrorBadPortIndex;
			}
			parammp3->nChannels = 2;
			parammp3->nBitRate = 0;
			parammp3->nSampleRate = 0;
			parammp3->nAudioBandWidth = 0;
			parammp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
		break;
		default:
			return OMX_ErrorUnsupportedIndex;
		break;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE ffmpeg_mp3dec_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PRIORITYMGMTTYPE* parammgm;
	OMX_PARAM_BUFFERSUPPLIERTYPE *paramsupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *paramaudioformat;
	OMX_AUDIO_PARAM_MP3TYPE * parammp3;
	OMX_AUDIO_PARAM_PCMMODETYPE* parampcm;
	OMX_PARAM_PORTDEFINITIONTYPE *paramPort;

	/* Check which structure we are being fed and make control its header */
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omx_component;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	/*
	if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x\n",__func__,stComponent->state);
		return OMX_ErrorIncorrectStateOperation;
	}
	*/
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			parammgm = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			if ((err = checkHeader(parammgm, sizeof(OMX_PRIORITYMGMTTYPE))) != OMX_ErrorNone) {
				break;
			}
			ffmpeg_mp3dec_Private->nGroupPriority = parammgm->nGroupPriority;
			ffmpeg_mp3dec_Private->nGroupID = parammgm->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				//return OMX_ErrorIncorrectStateOperation;
			}
			//err = OMX_ErrorBadParameter;
		break;
		case OMX_IndexParamPortDefinition: {
			paramPort = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			
			if ((err = checkHeader(paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE))) != OMX_ErrorNone) {
				return err;
			}
			if (paramPort->nPortIndex > 1) {
				return OMX_ErrorBadPortIndex;
			}
			if (paramPort->nPortIndex == 0) {
					ffmpeg_mp3dec_Private->inputPort.sPortParam.nBufferCountActual = paramPort->nBufferCountActual;
			} else {
					ffmpeg_mp3dec_Private->outputPort.sPortParam.nBufferCountActual = paramPort->nBufferCountActual;
			}
		   }
		break;
		case OMX_IndexParamAudioPortFormat:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			paramaudioformat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			if ((err = checkHeader(paramaudioformat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
				return err;
			}
			// TODO check if this rule is correct
		break;
		case OMX_IndexParamCompBufferSupplier:
			paramsupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;

			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				if ((paramsupply->nPortIndex == 0) && (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled==OMX_TRUE)) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
					return OMX_ErrorIncorrectStateOperation;
				}
				else if ((paramsupply->nPortIndex == 1) && (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled==OMX_TRUE)) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
					return OMX_ErrorIncorrectStateOperation;
				}
			}

			if (paramsupply->nPortIndex > 1) {
				return OMX_ErrorBadPortIndex;
			}
			if (paramsupply->eBufferSupplier == OMX_BufferSupplyUnspecified) {
				return OMX_ErrorNone;
			}
			if ((ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_ESTABLISHED) == 0) {
				return OMX_ErrorNone;
			}
			if((err = checkHeader(paramsupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE))) != OMX_ErrorNone){
				return err;
			}
			if ((paramsupply->eBufferSupplier == OMX_BufferSupplyInput) && (paramsupply->nPortIndex == 0)) {
				/** These two cases regard the first stage of client override */
				if (ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					err = OMX_ErrorNone;
					break;
				}
				ffmpeg_mp3dec_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				err = OMX_SetParameter(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent, OMX_IndexParamCompBufferSupplier, paramsupply);
			} else if ((paramsupply->eBufferSupplier == OMX_BufferSupplyOutput) && (paramsupply->nPortIndex == 0)) {
				if (ffmpeg_mp3dec_Private->inputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					ffmpeg_mp3dec_Private->inputPort.nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
					err = OMX_SetParameter(ffmpeg_mp3dec_Private->inputPort.hTunneledComponent, OMX_IndexParamCompBufferSupplier, paramsupply);
					break;
				}
				err = OMX_ErrorNone;
			} else if ((paramsupply->eBufferSupplier == OMX_BufferSupplyOutput) && (paramsupply->nPortIndex == 1)) {
				/** these two cases regard the second stage of client override */
				if (ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					err = OMX_ErrorNone;
					break;
				}
				ffmpeg_mp3dec_Private->outputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
			} else {
				if (ffmpeg_mp3dec_Private->outputPort.nTunnelFlags & TUNNEL_IS_SUPPLIER) {
					ffmpeg_mp3dec_Private->outputPort.nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
					err = OMX_ErrorNone;
					break;
				}
				err = OMX_ErrorNone;
			}
		break;
		case OMX_IndexParamAudioPcm:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			parampcm = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			if((err = checkHeader(parampcm, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
		case OMX_IndexParamAudioMp3:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			parammp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			if((err = checkHeader(parammp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
		default:
			return OMX_ErrorBadParameter;
		break;
	}
	
	/* If we are not asked two ports give error */
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "   Error during %s = %i\n", __func__, err);
	}
	return err;
}

OMX_ERRORTYPE ffmpeg_mp3dec_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE ffmpeg_mp3dec_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE ffmpeg_mp3dec_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType) {
	return OMX_ErrorNotImplemented;
}

/** Fill the internal component state
 */
OMX_ERRORTYPE ffmpeg_mp3dec_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState)
{
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omx_component;
	*pState = stComponent->state;
	return OMX_ErrorNone;
}

/** Asks the component to make use of user-provided
 * buffers.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer)
{
// TODO add the registration of the buffer in the list
	//stComponentType* stComponent = getSTComponent(hComponent);
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	//OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = (ffmpeg_mp3dec_PrivateType*)omxComponent->pComponentPrivate;
	ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port;
	OMX_S32 i;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	/* Buffers can be allocated only when the component is at loaded state
	 */

	/*There should be some mechanism to indicate whether the buffer is application allocated or component. And 
	component need to track that. so the fill buffer done call back need not to be called and IL client will
	pull the buffer from the output port*/
	
	if(nPortIndex == 0) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->inputPort;
	} else if(nPortIndex == 1) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->outputPort;
	} else {
		return OMX_ErrorBadPortIndex;
	}
	if ((ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorNone;
	}
	if (ffmpeg_mp3dec_Port->eTransientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s i=%i Buffer state=%x\n",__func__,i,ffmpeg_mp3dec_Port->nBufferState[i]);
		if(!(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ASSIGNED)){ 
			DEBUG(DEB_LEV_FULL_SEQ,"Inside %s i=%i Buffer state=%x\n",__func__,i,ffmpeg_mp3dec_Port->nBufferState[i]);
			ffmpeg_mp3dec_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(ffmpeg_mp3dec_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* use the buffer */
			ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer = pBuffer;
			ffmpeg_mp3dec_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			ffmpeg_mp3dec_Port->pBuffer[i]->nFilledLen = 0;
			ffmpeg_mp3dec_Port->pBuffer[i]->nOffset = 0;
			ffmpeg_mp3dec_Port->pBuffer[i]->pPlatformPrivate = ffmpeg_mp3dec_Port;
			ffmpeg_mp3dec_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			ffmpeg_mp3dec_Port->pBuffer[i]->nTickCount = 0;
			ffmpeg_mp3dec_Port->pBuffer[i]->nTimeStamp = 0;
			*ppBufferHdr = ffmpeg_mp3dec_Port->pBuffer[i];
			if (nPortIndex == 0) {
				ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = 0;
				if(ffmpeg_mp3dec_Private->inputPort.nTunnelFlags)
					ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = ffmpeg_mp3dec_Private->inputPort.nTunneledPort;
				else 
					ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports; // here is assigned a non-valid port index
					
			} else {
				if(ffmpeg_mp3dec_Private->outputPort.nTunnelFlags)
					ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = ffmpeg_mp3dec_Private->outputPort.nTunneledPort;
				else 
					ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = stComponent->nports; // here is assigned a non-valid port index
				
				ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = 1;
			}
			ffmpeg_mp3dec_Port->nBufferState[i] |= BUFFER_ASSIGNED;
			ffmpeg_mp3dec_Port->nBufferState[i] |= HEADER_ALLOCATED;
			ffmpeg_mp3dec_Port->nNumAssignedBuffers++;
			if (ffmpeg_mp3dec_Port->sPortParam.nBufferCountActual == ffmpeg_mp3dec_Port->nNumAssignedBuffers) {
				ffmpeg_mp3dec_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(ffmpeg_mp3dec_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,ffmpeg_mp3dec_Port->sPortParam.nBufferCountActual,
				ffmpeg_mp3dec_Port->nNumAssignedBuffers,
				ffmpeg_mp3dec_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorNone;
}

/** ffmpeg_mp3dec component can allocate up to MAX_BUFFERS buffers per port.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = (ffmpeg_mp3dec_PrivateType*)omxComponent->pComponentPrivate;
	ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port;
	OMX_S32 i;

	if(nPortIndex == 0) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->inputPort;
	} else if(nPortIndex == 1) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->outputPort;
	} else {
		return OMX_ErrorBadPortIndex;
	}
	if ((ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorBadPortIndex;
	}
	if (ffmpeg_mp3dec_Port->eTransientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		if(!(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ASSIGNED)){
			ffmpeg_mp3dec_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(ffmpeg_mp3dec_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			ffmpeg_mp3dec_Port->pBuffer[i]->hMarkTargetComponent = NULL;
			/* allocate the buffer */
			ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer = malloc(nSizeBytes);
			if(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer==NULL)
				return OMX_ErrorInsufficientResources;
			ffmpeg_mp3dec_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			ffmpeg_mp3dec_Port->pBuffer[i]->pPlatformPrivate = ffmpeg_mp3dec_Port;
			ffmpeg_mp3dec_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			//TODO: what to put in pPortDefinition?
			*pBuffer = ffmpeg_mp3dec_Port->pBuffer[i];
			ffmpeg_mp3dec_Port->nBufferState[i] |= BUFFER_ALLOCATED;
			ffmpeg_mp3dec_Port->nBufferState[i] |= HEADER_ALLOCATED;
			if (nPortIndex == 0) {
				ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = 0;
				// here is assigned a non-valid port index
				ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports;
			} else {
				// here is assigned a non-valid port index
				ffmpeg_mp3dec_Port->pBuffer[i]->nInputPortIndex = stComponent->nports;
				ffmpeg_mp3dec_Port->pBuffer[i]->nOutputPortIndex = 1;
			}
			
			ffmpeg_mp3dec_Port->nNumAssignedBuffers++;
			DEBUG(DEB_LEV_PARAMS, "ffmpeg_mp3dec_Port->nNumAssignedBuffers %i\n", ffmpeg_mp3dec_Port->nNumAssignedBuffers);
			
			if (ffmpeg_mp3dec_Port->sPortParam.nBufferCountActual == ffmpeg_mp3dec_Port->nNumAssignedBuffers) {
				ffmpeg_mp3dec_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(ffmpeg_mp3dec_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,ffmpeg_mp3dec_Port->sPortParam.nBufferCountActual,
				ffmpeg_mp3dec_Port->nNumAssignedBuffers,
				ffmpeg_mp3dec_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	
	DEBUG(DEB_LEV_ERR, "Error: no available buffers\n");
	return OMX_ErrorInsufficientResources;
}

/** To be tested
 */
OMX_ERRORTYPE ffmpeg_mp3dec_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = (ffmpeg_mp3dec_PrivateType*)omxComponent->pComponentPrivate;
	ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port;
	
	OMX_S32 i;
	OMX_BOOL foundBuffer;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	if(nPortIndex == 0) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->inputPort;
		if (pBuffer->nInputPortIndex != 0) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
			return OMX_ErrorBadParameter;
		}
	} else if(nPortIndex == 1) {
		ffmpeg_mp3dec_Port = &ffmpeg_mp3dec_Private->outputPort;
		if (pBuffer->nOutputPortIndex != 1) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
			return OMX_ErrorBadParameter;
		}
	} else {
		return OMX_ErrorBadPortIndex;
	}
	if ((ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_ESTABLISHED) && 
		(ffmpeg_mp3dec_Port->nTunnelFlags & TUNNEL_IS_SUPPLIER)){
		//return OMX_ErrorBadPortIndex;
		return OMX_ErrorNone;
	}
	/*
	if (ffmpeg_mp3dec_Port->eTransientState != OMX_StateLoaded) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to release its buffers\n", __func__);
		DEBUG(DEB_LEV_FULL_SEQ, "\nTransient State=0x%08x, state=0x%08x\n",ffmpeg_mp3dec_Port->eTransientState,stComponent->state);
		return OMX_ErrorPortUnpopulated;
	}
	*/
	DEBUG(DEB_LEV_PARAMS,"\nIn %s bEnabled=%d,bPopulated=%d state=%d,eTransientState=%d\n",
		__func__,ffmpeg_mp3dec_Port->sPortParam.bEnabled,ffmpeg_mp3dec_Port->sPortParam.bPopulated,
		stComponent->state,ffmpeg_mp3dec_Port->eTransientState);
	
	if (ffmpeg_mp3dec_Port->eTransientState != OMX_StateLoaded 		 
		&& ffmpeg_mp3dec_Port->eTransientState != OMX_StateInvalid ) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to release its buffers\n", __func__);
		(*(stComponent->callbacks->EventHandler))
			(hComponent,
				stComponent->callbackData,
				OMX_EventError, /* The command was completed */
				OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
				nPortIndex, /* The state has been changed in message->messageParam2 */
				NULL);
		
		//return OMX_ErrorPortUnpopulated;
	}
	
	
	for(i=0;i<MAX_BUFFERS;i++){
		if((ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ALLOCATED) &&
			(ffmpeg_mp3dec_Port->pBuffer[i]->pBuffer == pBuffer->pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			ffmpeg_mp3dec_Port->nNumAssignedBuffers--;
			free(pBuffer->pBuffer);
			if(ffmpeg_mp3dec_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			ffmpeg_mp3dec_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
		else if((ffmpeg_mp3dec_Port->nBufferState[i] & BUFFER_ASSIGNED) &&
			(ffmpeg_mp3dec_Port->pBuffer[i] == pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			ffmpeg_mp3dec_Port->nNumAssignedBuffers--;
			//free(pBuffer->pBuffer);
			if(ffmpeg_mp3dec_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			ffmpeg_mp3dec_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
	}
	/*check if there are some buffers already to be freed */
	foundBuffer = OMX_FALSE;
	for (i = 0; i< ffmpeg_mp3dec_Port->sPortParam.nBufferCountActual ; i++) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Buffer flags - %i - %i\n", i, ffmpeg_mp3dec_Port->nBufferState[i]);
	
		if (ffmpeg_mp3dec_Port->nBufferState[i] != BUFFER_FREE) {
			foundBuffer = OMX_TRUE;
			break;
		}
	}
	if (!foundBuffer) {
		tsem_up(ffmpeg_mp3dec_Port->pFullAllocationSem);
		ffmpeg_mp3dec_Port->sPortParam.bPopulated = OMX_FALSE;
	}
	return OMX_ErrorNone;
}

/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param pCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
 */
OMX_ERRORTYPE ffmpeg_mp3dec_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData)
{
	OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omx_component;
	stComponent->callbacks = pCallbacks;
	stComponent->callbackData = pAppData;
	
	return OMX_ErrorNone;
}


/** The ffmpeg_mp3dec_Component sendCommand method.
 * This has to return immediately.
 * Pack queue and command in a message and send it
 * to the core for handling it.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = (ffmpeg_mp3dec_PrivateType*)omxComponent->pComponentPrivate;
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
					err=ffmpeg_mp3dec_Init(stComponent);
					if(err!=OMX_ErrorNone) {
						DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s  ffmpeg_mp3dec_Init returned error %i\n",
							__func__,err);
						return OMX_ErrorInsufficientResources;
					}
					if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateIdle;
					if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateIdle;
			} else if ((nParam == OMX_StateLoaded) && (stComponent->state == OMX_StateIdle)) {
				tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
				tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
					if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateLoaded;
					if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateLoaded;
			}
			else if (nParam == OMX_StateInvalid) {
					if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateInvalid;
					if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled)
						ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateInvalid;
			}
			/*
			else if(ffmpeg_mp3dec_Private->inputPort.eTransientState == OMX_StateIdle && 
				stComponent->state==OMX_StateLoaded && nParam == OMX_StateLoaded){
			}
			*/
		break;
		case OMX_CommandFlush:
			DEBUG(DEB_LEV_FULL_SEQ, "In OMX_CommandFlush state is %i\n", stComponent->state);
			if ((stComponent->state != OMX_StateExecuting) && (stComponent->state != OMX_StatePause)) {
				err = OMX_ErrorIncorrectStateOperation;
				break;

			}
			if (nParam == 0) {
				ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_TRUE;
			}
			else if (nParam == 1) {
				ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_TRUE;
			} else if (nParam == -1) {
				ffmpeg_mp3dec_Private->inputPort.bIsPortFlushed=OMX_TRUE;
				ffmpeg_mp3dec_Private->outputPort.bIsPortFlushed=OMX_TRUE;
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
			tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
			tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortDisable state is %i\n", stComponent->state);
			if (nParam == 0) {
				if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_FALSE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateLoaded;
				}
			} else if (nParam == 1) {
				if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_FALSE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateLoaded;
				}
			} else if (nParam == -1) {
				if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_FALSE) || (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_FALSE)){
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateLoaded;
					ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateLoaded;
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
			tsem_reset(ffmpeg_mp3dec_Private->inputPort.pFullAllocationSem);
			tsem_reset(ffmpeg_mp3dec_Private->outputPort.pFullAllocationSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortEnable state is %i\n", stComponent->state);
			if (nParam == 0) {
				if (ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateIdle;
				}
			} else if (nParam == 1) {
				if (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_TRUE) {
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateIdle;
				}
			} else if (nParam == -1) {
				if ((ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled == OMX_TRUE) || (ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled == OMX_TRUE)){
					err = OMX_ErrorIncorrectStateOperation;
					break;
				} else {
					ffmpeg_mp3dec_Private->inputPort.eTransientState = OMX_StateIdle;
					ffmpeg_mp3dec_Private->outputPort.eTransientState = OMX_StateIdle;
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

/** This is called to free all component resources.
 * Still not clear why this is not done in the freehandle() call instead...
 */
OMX_ERRORTYPE ffmpeg_mp3dec_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Tearing down component...\n");

	/* Take care of tearing down resources if not in loaded state */
	if(stComponent->state != OMX_StateLoaded)
		ffmpeg_mp3dec_Deinit(stComponent);

	return OMX_ErrorNone;
}

/** Queue a buffer on the input port and
 * ask the component to process it.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* inputSem = ffmpeg_mp3dec_Private->inputPort.pBufferSem;
	queue_t* inputQueue = ffmpeg_mp3dec_Private->inputPort.pBufferQueue;
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

	if(ffmpeg_mp3dec_Private->inputPort.sPortParam.bEnabled==OMX_FALSE)
		return OMX_ErrorIncorrectStateOperation;

	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}

	/* Queue this buffer on the output port */
	queue(inputQueue, pBuffer);
	/* And notify the buffer management thread we have a fresh new buffer to manage */
	tsem_up(inputSem);

	ffmpeg_mp3dec_Private->inbuffer++;
	DEBUG(DEB_LEV_FULL_SEQ,"In %s no of in buffer=%d\n",__func__,ffmpeg_mp3dec_Private->inbuffer);

	return OMX_ErrorNone;
}

/** Queue a buffer on the output port and
 * ask the component to fill it.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;

	tsem_t* outputSem = ffmpeg_mp3dec_Private->outputPort.pBufferSem;
	queue_t* outputQueue = ffmpeg_mp3dec_Private->outputPort.pBufferQueue;
	OMX_ERRORTYPE err=OMX_ErrorNone;
 
	/* The output port code is not valid
	*/
	if (pBuffer->nOutputPortIndex != 1) {
		DEBUG(DEB_LEV_ERR, "In %s: wrong port %d code for this operation\n", __func__,pBuffer->nOutputPortIndex);
		return OMX_ErrorBadPortIndex;
	}
	/* We are not accepting buffers if not in executing or
	 * paused state
	 */
	if(stComponent->state != OMX_StateExecuting &&
		stComponent->state != OMX_StatePause &&
		stComponent->state != OMX_StateIdle){
		DEBUG(DEB_LEV_ERR, "In %s: we are not in executing or paused state\n", __func__);
		return OMX_ErrorInvalidState;
	}

	if(ffmpeg_mp3dec_Private->outputPort.sPortParam.bEnabled==OMX_FALSE)
		return OMX_ErrorIncorrectStateOperation;

	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}
	
	/* Queue this buffer on the output port */
	queue(outputQueue, pBuffer);

	/* Signal that some new buffers are available on out put port */
	DEBUG(DEB_LEV_FULL_SEQ, "In %s: signalling the presence of new buffer on output port\n", __func__);

	tsem_up(outputSem);

	ffmpeg_mp3dec_Private->outbuffer++;
	DEBUG(DEB_LEV_FULL_SEQ,"In %s no of out buffer=%d\n",__func__,ffmpeg_mp3dec_Private->outbuffer);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE ffmpeg_mp3dec_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE err=OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE param;
	OMX_PARAM_BUFFERSUPPLIERTYPE pSupplier;

	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComp;
	stComponentType* stComponent = (stComponentType*)omxComponent;
	
	ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private = stComponent->omx_component.pComponentPrivate;

	if (pTunnelSetup == NULL || hTunneledComp == 0) {
        /* cancel previous tunnel */
		if (nPort == 0) {
       		ffmpeg_mp3dec_Private->inputPort.hTunneledComponent = 0;
			ffmpeg_mp3dec_Private->inputPort.nTunneledPort = 0;
			ffmpeg_mp3dec_Private->inputPort.nTunnelFlags = 0;
			ffmpeg_mp3dec_Private->inputPort.eBufferSupplier=OMX_BufferSupplyUnspecified;
		}
		else if (nPort == 1) {
			ffmpeg_mp3dec_Private->outputPort.hTunneledComponent = 0;
			ffmpeg_mp3dec_Private->outputPort.nTunneledPort = 0;
			ffmpeg_mp3dec_Private->outputPort.nTunnelFlags = 0;
			ffmpeg_mp3dec_Private->outputPort.eBufferSupplier=OMX_BufferSupplyUnspecified;
		}
		else {
			return OMX_ErrorBadPortIndex;
		}
		return err;
    }

	if (nPort == 0) {
		// the input port
		// check data constistency
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		// TODO insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		ffmpeg_mp3dec_Private->inputPort.nNumTunnelBuffer=param.nBufferCountMin;

		DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled port domain=%d\n",param.eDomain);
		if(param.eDomain!=OMX_PortDomainAudio)
			return OMX_ErrorPortsNotCompatible;
		else if(param.format.audio.eEncoding == OMX_AUDIO_CodingMax)
			return OMX_ErrorPortsNotCompatible;

		
		pSupplier.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);

		if (err != OMX_ErrorNone) {
			// compatibility not reached
			//return OMX_ErrorPortsNotCompatible;
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled error=%d\n",err);
				return OMX_ErrorPortsNotCompatible;
		}
		else {
			/*
			if ((err = checkHeader(&pSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE))) != OMX_ErrorNone) {
				DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled Header Mismatch error=%x\n",err);
				return err;
			}
			*/
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled Port eBufferSupplier=%x\n",pSupplier.eBufferSupplier);
		}
		

		// store the current callbacks, if defined
		ffmpeg_mp3dec_Private->inputPort.hTunneledComponent = hTunneledComp;
		ffmpeg_mp3dec_Private->inputPort.nTunneledPort = nTunneledPort;
		ffmpeg_mp3dec_Private->inputPort.nTunnelFlags = 0;
		// Negotiation
		if (pTunnelSetup->nTunnelFlags & OMX_PORTTUNNELFLAG_READONLY) {
			// the buffer provider MUST be the output port provider
			pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
			ffmpeg_mp3dec_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;	
			ffmpeg_mp3dec_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
		} else {
			if (pTunnelSetup->eSupplier == OMX_BufferSupplyInput) {
				ffmpeg_mp3dec_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				ffmpeg_mp3dec_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			} else if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified) {
				pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
				ffmpeg_mp3dec_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				ffmpeg_mp3dec_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			}
		}
		ffmpeg_mp3dec_Private->inputPort.nTunnelFlags |= TUNNEL_ESTABLISHED;
	} else if (nPort == 1) {
		// output port
		// all the consistency checks are under other component responsibility
		// check if it is a tunnel deletion request
		if (hTunneledComp == NULL) {
			ffmpeg_mp3dec_Private->outputPort.hTunneledComponent = NULL;
			ffmpeg_mp3dec_Private->outputPort.nTunneledPort = 0;
			ffmpeg_mp3dec_Private->outputPort.nTunnelFlags = 0;
			return OMX_ErrorNone;
		}

		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		// TODO insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		ffmpeg_mp3dec_Private->outputPort.nNumTunnelBuffer=param.nBufferCountMin;

		ffmpeg_mp3dec_Private->outputPort.hTunneledComponent = hTunneledComp;
		ffmpeg_mp3dec_Private->outputPort.nTunneledPort = nTunneledPort;
		pTunnelSetup->eSupplier = OMX_BufferSupplyOutput;
		ffmpeg_mp3dec_Private->outputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
		ffmpeg_mp3dec_Private->outputPort.nTunnelFlags |= TUNNEL_ESTABLISHED;

		ffmpeg_mp3dec_Private->outputPort.eBufferSupplier=OMX_BufferSupplyOutput;
	} else {
		return OMX_ErrorBadPortIndex;
	}
	return OMX_ErrorNone;
}

void ffmpeg_mp3dec_Panic() {
	exit(EXIT_FAILURE);
}

