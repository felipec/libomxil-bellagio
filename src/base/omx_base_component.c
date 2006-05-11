/**
	@file src/components/base_component/omx_base_component.c
	
	OpenMax base_component component. This component does not perform any multimedia
	processing.	It is used as a base_component for new components development.
	
	Copyright (C) 2006  STMicroelectronics and Nokia

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI, Ukri NIEMIMUUKKO

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
	
	2006/02/08:  OpenMAX base_component component version 0.1

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
//#include <OMX_Audio.h>

#include "omxcore.h"
#include "omx_base_component.h"

#include "tsemaphore.h"
#include "queue.h"


/** Maximum Number of base_component Instance*/
OMX_U32 noRefInstance=0;


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
stComponentType* base_component_CreateComponentStruct() {
	stComponentType *base_component = (stComponentType *) malloc(sizeof(stComponentType)); 
	if (base_component) {
	  base_component->name = "OMX.st.base";
	  base_component->state = OMX_StateLoaded;
	  base_component->callbacks = NULL;
	  base_component->callbackData = NULL;
	  base_component->nports = 0;
		base_component->coreDescriptor = NULL;
	  base_component->constructor = base_component_Constructor;
	  base_component->destructor = base_component_Destructor;
	  base_component->messageHandler = base_component_MessageHandler;
		base_component->omx_component.nSize = sizeof(OMX_COMPONENTTYPE);
	  base_component->omx_component.pComponentPrivate = NULL;
	  base_component->omx_component.pApplicationPrivate = NULL;
	  base_component->omx_component.GetComponentVersion = base_component_GetComponentVersion;
	  base_component->omx_component.SendCommand = base_component_SendCommand;
	  base_component->omx_component.GetParameter = base_component_GetParameter;
	  base_component->omx_component.SetParameter = base_component_SetParameter;
	  base_component->omx_component.GetConfig = base_component_GetConfig;
	  base_component->omx_component.SetConfig = base_component_SetConfig;
	  base_component->omx_component.GetExtensionIndex = base_component_GetExtensionIndex;
	  base_component->omx_component.GetState = base_component_GetState;
	  base_component->omx_component.ComponentTunnelRequest = base_component_ComponentTunnelRequest;
	  base_component->omx_component.UseBuffer = base_component_UseBuffer;
	  base_component->omx_component.AllocateBuffer = base_component_AllocateBuffer;
	  base_component->omx_component.FreeBuffer = base_component_FreeBuffer;
	  base_component->omx_component.EmptyThisBuffer = base_component_EmptyThisBuffer;
	  base_component->omx_component.FillThisBuffer = base_component_FillThisBuffer;
	  base_component->omx_component.SetCallbacks = base_component_SetCallbacks;
	  base_component->omx_component.ComponentDeInit = base_component_ComponentDeInit;
	  base_component->omx_component.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
	  base_component->omx_component.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
	  base_component->omx_component.nVersion.s.nRevision = SPECREVISION;
	  base_component->omx_component.nVersion.s.nStep = SPECSTEP;
	}
	return base_component;
}

/** 
 * This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the base_component component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated base_component_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE base_component_Constructor(stComponentType* stComponent)
{
	base_component_PrivateType* base_component_Private;
	pthread_t testThread;
	int i;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noRefInstance);

	/** Allocate and fill in base_component private structures
	 * These include output port buffer queue and flow control semaphore
	 */
	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(base_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	base_component_Private = stComponent->omx_component.pComponentPrivate;
	/** Default parameters setting */
	base_component_Private->bIsInit = OMX_FALSE;
	base_component_Private->nGroupPriority = 0;
	base_component_Private->nGroupID = 0;
	base_component_Private->pMark=NULL;
	base_component_Private->bCmdUnderProcess=OMX_FALSE;
	base_component_Private->bWaitingForCmdToFinish=OMX_FALSE;

	base_component_Private->Init = &base_component_Init;
	base_component_Private->Deinit = &base_component_Deinit;
	base_component_Private->DoStateSet = &base_component_DoStateSet;
	base_component_Private->BufferMgmtFunction = &base_component_BufferMgmtFunction;
	base_component_Private->FlushPort = &base_component_FlushPort;
	base_component_Private->AllocateTunnelBuffers = &base_component_AllocateTunnelBuffers;
	base_component_Private->FreeTunnelBuffers = &base_component_FreeTunnelBuffers;
	base_component_Private->DomainCheck		  = &base_component_DomainCheck;
	
	/** generic parameter NOT related to a specific port */
	setHeader(&base_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
	//subclasses define the port count (default is 0)	
	
	// create ports, but only if the subclass hasn't done it
	if (stComponent->nports && !base_component_Private->ports) {
		base_component_Private->ports = calloc(stComponent->nports,
																			sizeof (base_component_PortType *));

		if (!base_component_Private->ports) return OMX_ErrorInsufficientResources;
		
		for (i=0; i < stComponent->nports; i++) {
			base_component_Private->ports[i] = calloc(1, sizeof(base_component_PortType));
			if (!base_component_Private->ports[i]) return OMX_ErrorInsufficientResources;

			base_component_Private->ports[i]->transientState = OMX_StateMax;
			setHeader(&base_component_Private->ports[i]->sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
			base_component_Private->ports[i]->sPortParam.nPortIndex = i;
			
			base_component_Private->ports[i]->pBufferSem = malloc(sizeof(tsem_t));
			if(base_component_Private->ports[i]->pBufferSem==NULL) return OMX_ErrorInsufficientResources;
			tsem_init(base_component_Private->ports[i]->pBufferSem, 0);
		
			base_component_Private->ports[i]->pFullAllocationSem = malloc(sizeof(tsem_t));
			if(base_component_Private->ports[i]->pFullAllocationSem==NULL) return OMX_ErrorInsufficientResources;
			tsem_init(base_component_Private->ports[i]->pFullAllocationSem, 0);
		
			/** Allocate and initialize buffer queue */
			base_component_Private->ports[i]->pBufferQueue = malloc(sizeof(queue_t));
			if(base_component_Private->ports[i]->pBufferQueue==NULL) return OMX_ErrorInsufficientResources;
			queue_init(base_component_Private->ports[i]->pBufferQueue);
		
			base_component_Private->ports[i]->pFlushSem = malloc(sizeof(tsem_t));
			if(base_component_Private->ports[i]->pFlushSem==NULL)	return OMX_ErrorInsufficientResources;
			tsem_init(base_component_Private->ports[i]->pFlushSem, 0);

			base_component_Private->ports[i]->hTunneledComponent = NULL;
			base_component_Private->ports[i]->nTunneledPort=0;
			base_component_Private->ports[i]->nTunnelFlags=0;

		}
		base_component_SetPortFlushFlag(stComponent, -1, OMX_FALSE);
		base_component_SetNumBufferFlush(stComponent, -1, 0);
	}
	
	pthread_mutex_init(&base_component_Private->exit_mutex, NULL);
	pthread_cond_init(&base_component_Private->exit_condition, NULL);
	pthread_mutex_init(&base_component_Private->cmd_mutex, NULL);
	pthread_mutex_init(&base_component_Private->flush_mutex, NULL);
	pthread_cond_init(&base_component_Private->flush_condition, NULL);

	base_component_Private->pCmdSem = malloc(sizeof(tsem_t));
	if(base_component_Private->pCmdSem==NULL) return OMX_ErrorInsufficientResources;
	tsem_init(base_component_Private->pCmdSem, 0);

	pthread_mutex_lock(&base_component_Private->exit_mutex);
	base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&base_component_Private->exit_mutex);

	base_component_Private->inbuffer=0;
	base_component_Private->inbuffercb=0;
	base_component_Private->outbuffer=0;
	base_component_Private->outbuffercb=0;

	DEBUG(DEB_LEV_FULL_SEQ,"Returning from %s\n",__func__);
	/** Two meta-states used for the implementation 
	* of the transition mechanism loaded->idle and idle->loaded 
	*/

	noRefInstance++;

	if(noRefInstance > MAX_NUM_OF_base_component_INSTANCES) 
		return OMX_ErrorInsufficientResources;

	return OMX_ErrorNone;
}

/** sets the port flush flag to a value
 * Assumes sanity checked input
 * fixme params
 */
void base_component_SetPortFlushFlag(stComponentType* stComponent, OMX_S32 index, OMX_BOOL value) {
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 i;
	if (index == -1) {
		for (i = 0; i < stComponent->nports; i++) {
			base_component_Private->ports[i]->bIsPortFlushed = value;
		}		
	} else {
		base_component_Private->ports[index]->bIsPortFlushed = value;
	}

}

/** sets number of buffer flush value
 */
void base_component_SetNumBufferFlush(stComponentType* stComponent, OMX_S32 index, OMX_S32 value) {
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	int i;
	if (index == -1) {
		for (i = 0; i < stComponent->nports; i++) {
			base_component_Private->ports[i]->nNumBufferFlushed = value;
		}		
	} else {
		base_component_Private->ports[index]->nNumBufferFlushed = value;
	}

}

/**
 * This function verify Component State and Structure header
 */
OMX_ERRORTYPE base_component_ParameterSanityCheck(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_PTR pStructure,
	OMX_IN  size_t size)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	
	if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
		return OMX_ErrorIncorrectStateOperation;
	}
	if (nPortIndex >= stComponent->nports) {
		return OMX_ErrorBadPortIndex;
	}
	return checkHeader(pStructure , size);
} 

/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 * \param message the message that has been passed to core
 */
OMX_ERRORTYPE base_component_MessageHandler(coreMessage_t* message)
{
	stComponentType* stComponent = message->stComponent;
	base_component_PrivateType* base_component_Private;
	OMX_BOOL waitFlag=OMX_FALSE;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	OMX_U32 i;
	OMX_ERRORTYPE err;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	base_component_Private = stComponent->omx_component.pComponentPrivate;

	/** Dealing with a SendCommand call.
	 * -messageParam1 contains the command to execute
	 * -messageParam2 contains the parameter of the command
	 *  (destination state in case of a state change command).
	 */
	pthread_mutex_lock(&base_component_Private->cmd_mutex);
	base_component_Private->bCmdUnderProcess=OMX_TRUE;
	pthread_mutex_unlock(&base_component_Private->cmd_mutex);
	
	if(message->messageType == SENDCOMMAND_MSG_TYPE){
		switch(message->messageParam1){
			case OMX_CommandStateSet:
				/* Do the actual state change */
				err = (*(base_component_Private->DoStateSet))(stComponent, message->messageParam2);				
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
				base_component_SetNumBufferFlush(stComponent, -1, 0);
				/*Flush ports*/
				err = (*(base_component_Private->FlushPort))(stComponent, message->messageParam2);	

				base_component_SetNumBufferFlush(stComponent, -1, 0);
				/*Signal Flush Port to Buffer Management Thread*/
				pthread_cond_signal(&base_component_Private->flush_condition);
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
						for (i = 0; i < stComponent->nports; i++) {
							(*(stComponent->callbacks->EventHandler))
								(pHandle,
									stComponent->callbackData,
									OMX_EventCmdComplete, /* The command was completed */
									OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
									i, /* The state has been changed in message->messageParam2 */
									NULL);
						}		
					} else {/*Flush input/output port*/
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
								message->messageParam2, /* The state has been changed in message->messageParam2 */
								NULL);
					}
				}
				base_component_SetPortFlushFlag(stComponent, -1, OMX_FALSE);
			break;
			case OMX_CommandPortDisable:
				/** This condition is added to pass the tests, it is not significant for the environment */
				err = base_component_DisablePort(stComponent, message->messageParam2);
				if (err != OMX_ErrorNone) {
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventError, /* The command was completed */
							err, /* The commands was a OMX_CommandStateSet */
							0, /* The state has been changed in message->messageParam2 */
							NULL);
				} else {
					if(message->messageParam2==-1){ /*Disable all ports*/
						for (i = 0; i < stComponent->nports; i++) {
							(*(stComponent->callbacks->EventHandler))
								(pHandle,
									stComponent->callbackData,
									OMX_EventCmdComplete, /* The command was completed */
									OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
									i, /* The state has been changed in message->messageParam2 */
									NULL);
						}
					} else {
						(*(stComponent->callbacks->EventHandler))
							(pHandle,
								stComponent->callbackData,
								OMX_EventCmdComplete, /* The command was completed */
								OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
								message->messageParam2, /* The state has been changed in message->messageParam2 */
								NULL);
					} 					
				}
			break;
			case OMX_CommandPortEnable:
				err = base_component_EnablePort(stComponent, message->messageParam2);
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
						for (i = 0; i < stComponent->nports; i++) {
							(*(stComponent->callbacks->EventHandler))
								(pHandle,
									stComponent->callbackData,
									OMX_EventCmdComplete, /* The command was completed */
									OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
									i, /* The state has been changed in message->messageParam2 */
									NULL);							
						}
					}
				}
			break;
				
			case OMX_CommandMarkBuffer:
				base_component_Private->pMark=(OMX_MARKTYPE *)message->pCmdData;
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
			//FIXME
			default:	
				DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageParam1);
			break;
		}		
	}


	pthread_mutex_lock(&base_component_Private->cmd_mutex);
	base_component_Private->bCmdUnderProcess=OMX_FALSE;
	waitFlag=base_component_Private->bWaitingForCmdToFinish;
	pthread_mutex_unlock(&base_component_Private->cmd_mutex);

	if(waitFlag==OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: Signalling command finish condition \n", __func__);
		tsem_up(base_component_Private->pCmdSem);
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s: \n", __func__);

	return OMX_ErrorNone;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* base_component_BufferMgmtFunction(void* param) {
	
	return NULL;
}
/** Dummy Function to check domain specific parameters of the Component
 */

OMX_ERRORTYPE base_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef){
	return OMX_ErrorNone;
}

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For base_component component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE base_component_Init(stComponentType* stComponent)
{
	OMX_S32 i;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (base_component_Private->bIsInit == OMX_TRUE) {
		DEBUG(DEB_LEV_ERR, "In %s Big error. It should not happen\n", __func__);
		return OMX_ErrorIncorrectStateOperation;
	}
	base_component_Private->bIsInit = OMX_TRUE;

	for (i=0; i < stComponent->nports; i++) {
		tsem_init(base_component_Private->ports[i]->pBufferSem, 0);
		tsem_init(base_component_Private->ports[i]->pFullAllocationSem, 0);
		tsem_init(base_component_Private->ports[i]->pFlushSem, 0);
		queue_init(base_component_Private->ports[i]->pBufferQueue);
		pthread_mutex_init(&base_component_Private->ports[i]->mutex, NULL);

		base_component_Private->ports[i]->bBufferUnderProcess=OMX_FALSE;
		base_component_Private->ports[i]->bWaitingFlushSem=OMX_FALSE;
	}

	pthread_cond_init(&base_component_Private->executingCondition, NULL);
	pthread_mutex_init(&base_component_Private->executingMutex, NULL);
	
	pthread_mutex_lock(&base_component_Private->exit_mutex);
	base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&base_component_Private->exit_mutex);
	
	/** Spawn buffer management thread */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hey, starting threads!\n");
	base_component_Private->bufferMgmtThreadID = pthread_create(&base_component_Private->bufferMgmtThread,
		NULL,
		base_component_Private->BufferMgmtFunction,
		stComponent);	

	if(base_component_Private->bufferMgmtThreadID < 0){
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
OMX_ERRORTYPE base_component_Deinit(stComponentType* stComponent)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pSem;
	OMX_S32 ret;
	OMX_U32 i;
	OMX_BUFFERHEADERTYPE* pBuffer;
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s\n", __func__);
	base_component_Private->bIsInit = OMX_FALSE;
	
	/** Trash buffer mangement thread.
	 */
	pthread_mutex_lock(&base_component_Private->exit_mutex);
	base_component_Private->bExit_buffer_thread=OMX_TRUE;
	pthread_mutex_unlock(&base_component_Private->exit_mutex);

	for (i = 0; i < stComponent->nports; i++) {
		pSem = base_component_Private->ports[i]->pBufferSem;
		if(pSem->semval == 0 && 
	 		 !IS_BUFFER_UNDER_PROCESS(base_component_Private->ports[i])) {
			tsem_up(pSem);
	  }
	}

	DEBUG(DEB_LEV_FULL_SEQ,"All buffers flushed!\n");
	ret=pthread_join(base_component_Private->bufferMgmtThread,NULL);

	for (i = 0; i < stComponent->nports; i++) {
		pSem = base_component_Private->ports[i]->pBufferSem;
		tsem_reset(pSem);
		pthread_mutex_destroy(&base_component_Private->ports[i]->mutex);
	}


	pthread_mutex_lock(&base_component_Private->exit_mutex);
	base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&base_component_Private->exit_mutex);

	DEBUG(DEB_LEV_FULL_SEQ,"Execution thread re-joined\n");
	/*Deinitialize mutexes and conditional variables*/
	pthread_cond_destroy(&base_component_Private->executingCondition);
	pthread_mutex_destroy(&base_component_Private->executingMutex);
	DEBUG(DEB_LEV_FULL_SEQ,"Deinitialize mutexes and conditional variables\n");
	
	pthread_cond_signal(&base_component_Private->exit_condition);

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);

	return OMX_ErrorNone;
}

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE base_component_Destructor(stComponentType* stComponent)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_BOOL exit_thread=OMX_FALSE,cmdFlag=OMX_FALSE;
	OMX_U32 ret;
	OMX_U32 i;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noRefInstance);
	if (base_component_Private->bIsInit != OMX_FALSE) {
		(*(base_component_Private->Deinit))(stComponent);		
	} 

	pthread_mutex_lock(&base_component_Private->exit_mutex);
	exit_thread = base_component_Private->bExit_buffer_thread;
	pthread_mutex_unlock(&base_component_Private->exit_mutex);
	if(exit_thread == OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for condition...\n",__func__);
		pthread_cond_wait(&base_component_Private->exit_condition,&base_component_Private->exit_mutex);
	}

	pthread_mutex_lock(&base_component_Private->cmd_mutex);
	cmdFlag=base_component_Private->bCmdUnderProcess;
	if(cmdFlag==OMX_TRUE) {
		base_component_Private->bWaitingForCmdToFinish=OMX_TRUE;
		pthread_mutex_unlock(&base_component_Private->cmd_mutex);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish ...\n",__func__);
		tsem_down(base_component_Private->pCmdSem);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish over..cmutex=%x.\n",__func__,&base_component_Private->cmd_mutex);

		pthread_mutex_lock(&base_component_Private->cmd_mutex);
		base_component_Private->bWaitingForCmdToFinish=OMX_FALSE;
		pthread_mutex_unlock(&base_component_Private->cmd_mutex);
	}
	else {
		pthread_mutex_unlock(&base_component_Private->cmd_mutex);
	}
	
	pthread_cond_destroy(&base_component_Private->flush_condition);
	pthread_mutex_destroy(&base_component_Private->flush_mutex);
	pthread_cond_destroy(&base_component_Private->exit_condition);
	pthread_mutex_destroy(&base_component_Private->exit_mutex);
	pthread_mutex_destroy(&base_component_Private->cmd_mutex);
	
	if(base_component_Private->pCmdSem!=NULL)  {
	tsem_deinit(base_component_Private->pCmdSem);
	free(base_component_Private->pCmdSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pCmdSem semaphore\n");
	}

	for (i = 0; i < stComponent->nports; i++) {
		if(base_component_Private->ports[i]->pBufferSem != NULL)  {
			tsem_deinit(base_component_Private->ports[i]->pBufferSem);
			free(base_component_Private->ports[i]->pBufferSem);	
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pBufferSemaphore\n");		
		}
		if(base_component_Private->ports[i]->pBufferQueue!=NULL) {
			DEBUG(DEB_LEV_SIMPLE_SEQ,"deiniting base_component queue\n");
			queue_deinit(base_component_Private->ports[i]->pBufferQueue);
			free(base_component_Private->ports[i]->pBufferQueue);
			base_component_Private->ports[i]->pBufferQueue=NULL;
		}
		if(base_component_Private->ports[i]->pFullAllocationSem!=NULL)  {
			tsem_deinit(base_component_Private->ports[i]->pFullAllocationSem);
			free(base_component_Private->ports[i]->pFullAllocationSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pFullAllocationSem semaphore\n");
		}
		if(base_component_Private->ports[i]->pFlushSem!=NULL) {
			tsem_deinit(base_component_Private->ports[i]->pFlushSem);
			free(base_component_Private->ports[i]->pFlushSem);
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing output pFlushSem semaphore\n");
		}
		free(base_component_Private->ports[i]);
	}
	free(base_component_Private->ports);
	
	stComponent->state = OMX_StateInvalid;
	
	free(stComponent->omx_component.pComponentPrivate);

	noRefInstance--;

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);
	return OMX_ErrorNone;
}


/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param destinationState the requested target state.
 */
OMX_ERRORTYPE base_component_DoStateSet(stComponentType* stComponent, OMX_U32 destinationState)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	pthread_mutex_t* executingMutex = &base_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &base_component_Private->executingCondition;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_U32 i,j;

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
				for (i = 0; i < stComponent->nports; i++) {
						DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  port enabled %i,  port populated %i\n", __func__, base_component_Private->ports[i]->sPortParam.bEnabled, base_component_Private->ports[i]->sPortParam.bPopulated);					
					if (PORT_IS_TUNNELED(base_component_Private->ports[i]) &&
						PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[i])) {
						/* Freeing here the buffers allocated for the tunneling:*/
						eError = (*(base_component_Private->FreeTunnelBuffers))(base_component_Private->ports[i]);
					} 
					else if (PORT_IS_TUNNELED(base_component_Private->ports[i]) &&
							! PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[i])) {
						tsem_down(base_component_Private->ports[i]->pFullAllocationSem);
					}
					else {
						/** Wait until all the buffers assigned to the port have been de-allocated
							*/
						if (PORT_IS_ENABLED(base_component_Private->ports[i]) && 
							PORT_IS_POPULATED(base_component_Private->ports[i])) {
							tsem_down(base_component_Private->ports[i]->pFullAllocationSem);
							base_component_Private->ports[i]->sPortParam.bPopulated = OMX_FALSE;
							base_component_Private->ports[i]->transientState = OMX_StateMax;
						}
					}
					/*Reset the Allocation buffers*/
					tsem_reset(base_component_Private->ports[i]->pFullAllocationSem);
				}

				stComponent->state = OMX_StateLoaded;
				(*(base_component_Private->Deinit))(stComponent);				
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
				for (i = 0; i < stComponent->nports; i++) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s:  port enabled %i ,  port populated %i\n", __func__, base_component_Private->ports[i]->sPortParam.bEnabled, base_component_Private->ports[i]->sPortParam.bPopulated);					
					if (PORT_IS_TUNNELED(base_component_Private->ports[i]) &&
						PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[i])) {
						/** Allocate here the buffers needed for the tunneling:
							*/
						if(base_component_Private->ports[i]->sPortParam.eDir == OMX_DirInput)
							eError=(*(base_component_Private->AllocateTunnelBuffers))(base_component_Private->ports[i], i, INTERNAL_IN_BUFFER_SIZE);						
						else if(base_component_Private->ports[i]->sPortParam.eDir == OMX_DirOutput)
							eError=(*(base_component_Private->AllocateTunnelBuffers))(base_component_Private->ports[i], i, INTERNAL_OUT_BUFFER_SIZE);

						base_component_Private->ports[i]->transientState = OMX_StateMax;
					} else {
						/** Wait until all the buffers needed have been allocated
							*/
						if (PORT_IS_ENABLED(base_component_Private->ports[i]) && 
							!PORT_IS_POPULATED(base_component_Private->ports[i])) {
							DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait for buffers. port enabled %i,  port populated %i\n", __func__, base_component_Private->ports[i]->sPortParam.bEnabled, base_component_Private->ports[i]->sPortParam.bPopulated);
							tsem_down(base_component_Private->ports[i]->pFullAllocationSem);
							base_component_Private->ports[i]->sPortParam.bPopulated = OMX_TRUE;
							base_component_Private->ports[i]->transientState = OMX_StateMax;
						}
					}
					/*Reset the Allocation buffers*/
					tsem_reset(base_component_Private->ports[i]->pFullAllocationSem);					
					DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : port %d flags  0x%x\n", i,(OMX_S32)base_component_Private->ports[i]->nTunnelFlags);
				}
				stComponent->state = OMX_StateIdle;
				
				break;
				
			case OMX_StateIdle:
					return OMX_ErrorSameState;
				break;
				
			case OMX_StateExecuting:
			case OMX_StatePause:
				base_component_SetNumBufferFlush(stComponent, -1, 0);
				base_component_SetPortFlushFlag(stComponent, -1, OMX_TRUE);

				(*(base_component_Private->FlushPort))(stComponent, -1);
				
				base_component_SetPortFlushFlag(stComponent, -1, OMX_FALSE);
				base_component_SetNumBufferFlush(stComponent, -1, 0);
				/*Signal Flush Port to Buffer Management Thread*/
				pthread_cond_signal(&base_component_Private->flush_condition);
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
				/*Send Tunneled Buffer to the Neighbouring Components*/
				for (i = 0; i < stComponent->nports; i++) {
					if (PORT_IS_TUNNELED(base_component_Private->ports[i]) &&
						PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[i])) {
						for(j=0;j<base_component_Private->ports[i]->nNumTunnelBuffer;j++) {
							tsem_up(base_component_Private->ports[i]->pBufferSem);
						}
					}
				}
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
			if (base_component_Private->bIsInit != OMX_FALSE) {
				(*(base_component_Private->Deinit))(stComponent);									
			}
			break;
			}
		
		if (base_component_Private->bIsInit != OMX_FALSE) {
				(*(base_component_Private->Deinit))(stComponent);							
		}
		return OMX_ErrorInvalidState;
		// Free all the resources!!!!
	}
	return OMX_ErrorNone;
}

/**
 * Disable Single Port
 */
void base_component_DisableSinglePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pSem = base_component_Private->ports[portIndex]->pBufferSem;
	queue_t* pQueue = base_component_Private->ports[portIndex]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pBuffer;
		
	base_component_Private->ports[portIndex]->sPortParam.bEnabled = OMX_FALSE;
	DEBUG(DEB_LEV_FULL_SEQ,"Port %d pFullAllocationSem=%ld\n",portIndex,
		base_component_Private->ports[portIndex]->pFullAllocationSem->semval);
	if (PORT_IS_POPULATED(base_component_Private->ports[portIndex]) && base_component_Private->bIsInit == OMX_TRUE) {
		if (!PORT_IS_TUNNELED(base_component_Private->ports[portIndex])) {
			tsem_down(base_component_Private->ports[portIndex]->pFullAllocationSem);
		}
		else if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex]) && 
			PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
			/*Free buffers and remove the tunnel*/
			if(base_component_Private->ports[portIndex]->nNumBufferFlushed==base_component_Private->ports[portIndex]->nNumTunnelBuffer) { 
				while(base_component_Private->ports[portIndex]->nNumBufferFlushed>0) {
					pBuffer=dequeue(pQueue);
					base_component_Private->ports[portIndex]->nNumBufferFlushed--;
				}
				(*(base_component_Private->FreeTunnelBuffers))(base_component_Private->ports[portIndex]);				
				base_component_Private->ports[portIndex]->hTunneledComponent=NULL;
				base_component_Private->ports[portIndex]->nTunnelFlags=0;
			}
		}
		else {
			if(pSem->semval==0 && !IS_BUFFER_UNDER_PROCESS(base_component_Private->ports[portIndex])) {
				base_component_Private->ports[portIndex]->hTunneledComponent=NULL;
				base_component_Private->ports[portIndex]->nTunnelFlags=0;
			}
		}
		base_component_Private->ports[portIndex]->sPortParam.bPopulated = OMX_FALSE;
	}
	
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Port %ld is not populated\n",portIndex);
	
	tsem_reset(base_component_Private->ports[portIndex]->pBufferSem);
	base_component_Private->ports[portIndex]->nNumBufferFlushed=0;

	tsem_reset(base_component_Private->ports[portIndex]->pFullAllocationSem);	
}


/** Disables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be disabled
	* @param portIndex the ID of the port to be disabled
	*/
OMX_ERRORTYPE base_component_DisablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 i;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == -1) {
		for (i = 0; i < stComponent->nports; i++) 
			base_component_Private->ports[i]->bIsPortFlushed=OMX_TRUE;

		(*(base_component_Private->FlushPort))(stComponent,portIndex);

		for (i = 0; i < stComponent->nports; i++) 
			base_component_Private->ports[i]->bIsPortFlushed=OMX_FALSE;
	} else { 
			base_component_Private->ports[portIndex]->bIsPortFlushed=OMX_TRUE;
			(*(base_component_Private->FlushPort))(stComponent, portIndex);
			base_component_Private->ports[portIndex]->bIsPortFlushed=OMX_FALSE;
	}

	if (portIndex != -1) {
		base_component_DisableSinglePort(stComponent, portIndex);
	} else { 
		for (i = 0; i < stComponent->nports; i++) {
			base_component_DisableSinglePort(stComponent, i);
		}
	}

	/*Signal Flush Port to Buffer Management Thread*/
	pthread_cond_signal(&base_component_Private->flush_condition);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting %s\n", __func__);
	return OMX_ErrorNone;
}


/**
 * Enable Single Port
 */
void base_component_EnableSinglePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pSem = base_component_Private->ports[portIndex]->pBufferSem;
	queue_t* pQueue = base_component_Private->ports[portIndex]->pBufferQueue;
	OMX_S32 i;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Port.fAS=%x, is Init=%d\n",
		base_component_Private->ports[portIndex]->pFullAllocationSem->semval,
		base_component_Private->bIsInit);

	base_component_Private->ports[portIndex]->sPortParam.bEnabled = OMX_TRUE;
	if (!PORT_IS_POPULATED(base_component_Private->ports[portIndex]) && base_component_Private->bIsInit == OMX_TRUE) {
		if (!PORT_IS_TUNNELED(base_component_Private->ports[portIndex])) {
			if(stComponent->state!=OMX_StateLoaded && stComponent->state!=OMX_StateWaitForResources) {
				tsem_down(base_component_Private->ports[portIndex]->pFullAllocationSem);
				base_component_Private->ports[portIndex]->sPortParam.bPopulated = OMX_TRUE;
			}
		}else if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex]) && 
				  PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
			DEBUG(DEB_LEV_FULL_SEQ,"I/p Port buffer sem =%x \n",
				pSem->semval);

			if (base_component_Private->ports[portIndex]->sPortParam.eDir == OMX_DirInput)
				(*(base_component_Private->AllocateTunnelBuffers))(base_component_Private->ports[portIndex], portIndex, INTERNAL_IN_BUFFER_SIZE);
			else
				(*(base_component_Private->AllocateTunnelBuffers))(base_component_Private->ports[portIndex], portIndex, INTERNAL_OUT_BUFFER_SIZE);

			for(i=0;i<base_component_Private->ports[portIndex]->nNumTunnelBuffer;i++)
					tsem_up(base_component_Private->ports[portIndex]->pBufferSem);				
			base_component_Private->ports[portIndex]->sPortParam.bPopulated = OMX_TRUE;
		}
		else {
			DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s else \n",__func__);
		}
	}
	tsem_reset(base_component_Private->ports[portIndex]->pFullAllocationSem);
}

/** Enables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be enabled
	* @param portIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE base_component_EnablePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	int i;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (portIndex != -1) {
		base_component_EnableSinglePort(stComponent, portIndex);
	} else {
		for (i = 0; i < stComponent->nports; i++) {
			base_component_EnableSinglePort(stComponent, i);
		}
	}

	return OMX_ErrorNone;
}

/** I couldn't make a generic Flush Function as it is 
 * dependent on the Buffer Management Function
 */
OMX_ERRORTYPE base_component_FlushSinglePort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	base_component_PortType *pPort;
	OMX_BUFFERHEADERTYPE* pBuffer;

	OMX_BOOL flag;
	pPort = base_component_Private->ports[portIndex];
	
	if (pPort->sPortParam.eDir == OMX_DirInput) {
		if (! PORT_IS_TUNNELED(pPort)) {
			pthread_mutex_lock(&pPort->mutex);
			//flag=pPort->bBufferUnderProcess;
			if(IS_BUFFER_UNDER_PROCESS(pPort)) {
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(&pPort->mutex);
				
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(&base_component_Private->executingCondition);
				}

				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(&pPort->mutex);
				pPort->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(&pPort->mutex);
			}
			else {
				pthread_mutex_unlock(&pPort->mutex);
			}
			pthread_mutex_lock(&pPort->mutex);
			while(pPort->pBufferSem->semval>0) {
				tsem_down(pPort->pBufferSem);
				pBuffer = dequeue(pPort->pBufferQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pBuffer);
				base_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(&pPort->mutex);
		}
		else if (PORT_IS_TUNNELED(pPort) && 
			(! PORT_IS_BUFFER_SUPPLIER(pPort))) {
				
			pthread_mutex_lock(&pPort->mutex);
			//flag=pPort->bBufferUnderProcess;
			if(IS_BUFFER_UNDER_PROCESS(pPort)) {
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(&pPort->mutex);
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(&base_component_Private->executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(&pPort->mutex);
				pPort->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(&pPort->mutex);
			}
			else {
				pthread_mutex_unlock(&pPort->mutex);
			}
			pthread_mutex_lock(&pPort->mutex);
			while(pPort->pBufferSem->semval>0) {
				tsem_down(pPort->pBufferSem);
				pBuffer = dequeue(pPort->pBufferQueue);
				OMX_FillThisBuffer(pPort->hTunneledComponent, pBuffer);
				base_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(&pPort->mutex);
		}
		else if (PORT_IS_TUNNELED(pPort) && 
				 PORT_IS_BUFFER_SUPPLIER(pPort)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(&pPort->mutex);
			while(pPort->pBufferSem->semval>0) {
				tsem_down(pPort->pBufferSem);
				pPort->nNumBufferFlushed++;
			}
			pthread_mutex_unlock(&pPort->mutex);

			pthread_mutex_lock(&pPort->mutex);
			//flag=pPort->bBufferUnderProcess;
			if(pPort->nNumBufferFlushed<pPort->nNumTunnelBuffer) {
			pPort->bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(&pPort->mutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(&base_component_Private->executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(pPort->pFlushSem);
			pthread_mutex_lock(&pPort->mutex);
			pPort->bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(&pPort->mutex);
			}
			else
				pthread_mutex_unlock(&pPort->mutex);
		}
	} else /* if (pPort->sPortParam.eDir == OMX_DirOutput) */ {
		if (! PORT_IS_TUNNELED(pPort)) {
			while(pPort->pBufferSem->semval > 0) {
				tsem_down(pPort->pBufferSem);
				pBuffer = dequeue(pPort->pBufferQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, pBuffer);
				base_component_Private->outbuffercb++;
			}
		}
		else if ( PORT_IS_TUNNELED(pPort) && 
			(! PORT_IS_BUFFER_SUPPLIER(pPort))) {
			while(pPort->pBufferSem->semval > 0) {
				tsem_down(pPort->pBufferSem);
				pBuffer = dequeue(pPort->pBufferQueue);
				OMX_EmptyThisBuffer(pPort->hTunneledComponent, pBuffer);
				base_component_Private->outbuffercb++;
			}
		}
		else if ( PORT_IS_TUNNELED(pPort) && 
			PORT_IS_BUFFER_SUPPLIER(pPort)) {
			
			pthread_mutex_lock(&pPort->mutex);
			while(pPort->pBufferSem->semval>0) {
				tsem_down(pPort->pBufferSem);
				pPort->nNumBufferFlushed++;
			}
			pthread_mutex_unlock(&pPort->mutex);
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(&pPort->mutex);
			//flag=pPort->bBufferUnderProcess;
			if(pPort->nNumBufferFlushed<pPort->nNumTunnelBuffer) {
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(&pPort->mutex);
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(&base_component_Private->executingCondition);
				}
				/*Buffers being processed waiting for output flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(&pPort->mutex);
				pPort->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(&pPort->mutex);
			}
			else 
				pthread_mutex_unlock(&pPort->mutex);
		}
	}	
}

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE base_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	OMX_U32 i;
	
	if (portIndex != -1) {
		base_component_FlushSinglePort(stComponent, portIndex);
	} else {
		for (i = 0; i < stComponent->nports; i++) {
			base_component_FlushSinglePort(stComponent, i);
		}
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);
	return OMX_ErrorNone;
}

/** Allocate buffers in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Allocate the MAX number of buffers for that port, with the specified size.
	* @param base_component_Port the port of the alsa component that must be tunneled
	* @param portIndex index of the port to be tunneled
	* @param bufferSize Size of the buffers to be allocated
	*/
OMX_ERRORTYPE base_component_AllocateTunnelBuffers(base_component_PortType* base_component_Port, OMX_U32 portIndex, OMX_U32 bufferSize) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_U8* pBuffer=NULL;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<base_component_Port->nNumTunnelBuffer;i++){
		if(base_component_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			base_component_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(base_component_Port->pBuffer[i]->pBuffer);
		} else if (base_component_Port->nBufferState[i] & BUFFER_ASSIGNED){
			base_component_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	for(i=0;i<base_component_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Allocating  %i buffer of size %i\n", i, (OMX_S32)bufferSize);
		pBuffer = malloc(bufferSize);
		DEBUG(DEB_LEV_FULL_SEQ, "   malloc done %x\n",pBuffer);
		eError=OMX_UseBuffer(base_component_Port->hTunneledComponent,&base_component_Port->pBuffer[i],
			base_component_Port->nTunneledPort,NULL,bufferSize,pBuffer); 
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't Use buffer %i \n",i);
			free(pBuffer);
			return eError;
		}
		if ((eError = checkHeader(base_component_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "In %s: wrong buffer header size=%d version=0x%08x\n", 
				__func__,base_component_Port->pBuffer[i]->nSize,base_component_Port->pBuffer[i]->nVersion.s.nVersionMajor);
			//return eError;
		}
		base_component_Port->pBuffer[i]->nFilledLen = 0;
		base_component_Port->nBufferState[i] |= BUFFER_ALLOCATED;
		
		queue(base_component_Port->pBufferQueue, base_component_Port->pBuffer[i]);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "   queue done\n");
	}
	base_component_Port->sPortParam.bPopulated = OMX_TRUE;
	
	return OMX_ErrorNone;
}

/** Free buffer in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Free the MAX number of buffers for that port, with the specified size.
	* @param base_component_Port the port of the alsa component on which tunnel buffers must be released
	*/
OMX_ERRORTYPE base_component_FreeTunnelBuffers(base_component_PortType* base_component_Port) {
	OMX_S32 i;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	for(i=0;i<base_component_Port->nNumTunnelBuffer;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "   Freeing  %i buffer %x\n", i,base_component_Port->pBuffer[i]->pBuffer);
	
		if(base_component_Port->pBuffer[i]->pBuffer)
			free(base_component_Port->pBuffer[i]->pBuffer);

		eError=OMX_FreeBuffer(base_component_Port->hTunneledComponent,base_component_Port->nTunneledPort,base_component_Port->pBuffer[i]);
		if(eError!=OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't free buffer %i \n",i);
			return eError;
		} 
		
		DEBUG(DEB_LEV_FULL_SEQ, "   free done\n");
		base_component_Port->pBuffer[i]->nAllocLen = 0;
		base_component_Port->pBuffer[i]->pPlatformPrivate = NULL;
		base_component_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
		base_component_Port->pBuffer[i]->nInputPortIndex = 0;
		base_component_Port->pBuffer[i]->nOutputPortIndex = 0;
	}
	for(i=0;i<base_component_Port->nNumTunnelBuffer;i++){
		if(base_component_Port->nBufferState[i] & BUFFER_ALLOCATED){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer\n", i);
			base_component_Port->nBufferState[i] &= ~BUFFER_ALLOCATED;
			free(base_component_Port->pBuffer[i]->pBuffer);
		} else if (base_component_Port->nBufferState[i] & BUFFER_ASSIGNED){
			base_component_Port->nBufferState[i] &= ~BUFFER_ASSIGNED;
		}
	}
	base_component_Port->sPortParam.bPopulated = OMX_FALSE;
	return OMX_ErrorNone;
}

/************************************
 *
 * PUBLIC: OMX component entry points
 *
 ************************************/

OMX_ERRORTYPE base_component_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
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

OMX_ERRORTYPE base_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	OMX_U32 portIndex;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			setHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
			pPrioMgmt->nGroupPriority = base_component_Private->nGroupPriority;
			pPrioMgmt->nGroupID = base_component_Private->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
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
			portIndex = pPortDef->nPortIndex;
			if (portIndex >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}
			memcpy(pPortDef, &base_component_Private->ports[portIndex]->sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		break;
		case OMX_IndexParamCompBufferSupplier:
			pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
			portIndex = pBufSupply->nPortIndex;
			if (portIndex >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}
			setHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
			
			if (base_component_Private->ports[portIndex]->sPortParam.eDir == OMX_DirInput) {
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyInput;	
				} else if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex])) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyOutput;	
				} else {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyUnspecified;	
				}
			} else {
				setHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyOutput;	
				} else if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex])) {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyInput;	
				} else {
					pBufSupply->eBufferSupplier = OMX_BufferSupplyUnspecified;	
				}
			}
		break;
		default:
			return OMX_ErrorUnsupportedIndex;
		break;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

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
			base_component_Private->nGroupPriority = pPrioMgmt->nGroupPriority;
			base_component_Private->nGroupID = pPrioMgmt->nGroupID;
		break;
		case OMX_IndexParamPortDefinition: {
			pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
			portIndex = pPortDef->nPortIndex;
			
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
			if (err != OMX_ErrorNone)
				return err;
				
			base_component_Private->ports[portIndex]->sPortParam.nBufferCountActual = pPortDef->nBufferCountActual;
										   }
		break;
		case OMX_IndexParamCompBufferSupplier:
			pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
			portIndex = pBufSupply->nPortIndex;
			
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
			if(err==OMX_ErrorIncorrectStateOperation)
			{
				if (PORT_IS_ENABLED(base_component_Private->ports[pBufSupply->nPortIndex])) {
					DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
					return OMX_ErrorIncorrectStateOperation;
				}
			}
			else if (err != OMX_ErrorNone)
				return err;

			if (pBufSupply->eBufferSupplier == OMX_BufferSupplyUnspecified) {
				return OMX_ErrorNone;
			}
			if ((PORT_IS_TUNNELED(base_component_Private->ports[portIndex])) == 0) {
				return OMX_ErrorNone;
			}

			if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyInput) && 
				  (base_component_Private->ports[portIndex]->sPortParam.eDir == OMX_DirInput)) {
				/** These two cases regard the first stage of client override */
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					err = OMX_ErrorNone;
					break;
				}
				base_component_Private->ports[portIndex]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				err = OMX_SetParameter(base_component_Private->ports[portIndex]->hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufSupply);
			} else if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyOutput) && 
			           (base_component_Private->ports[portIndex]->sPortParam.eDir == OMX_DirInput)) {
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					base_component_Private->ports[portIndex]->nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
					err = OMX_SetParameter(base_component_Private->ports[portIndex]->hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufSupply);
					break;
				}
				err = OMX_ErrorNone;
			} else if ((pBufSupply->eBufferSupplier == OMX_BufferSupplyOutput) && 
								 (base_component_Private->ports[portIndex]->sPortParam.eDir == OMX_DirOutput)) {
				/** these two cases regard the second stage of client override */
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					err = OMX_ErrorNone;
					break;
				}
				base_component_Private->ports[portIndex]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
			} else {
				if (PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
					base_component_Private->ports[portIndex]->nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
					err = OMX_ErrorNone;
					break;
				}
				err = OMX_ErrorNone;
			}
		break;
		default:
			return OMX_ErrorBadParameter;
		break;
	}
	
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "   Error during %s = %i\n", __func__, err);
	}
	return err;
}

OMX_ERRORTYPE base_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType) {
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE base_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	*pState = stComponent->state;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	base_component_PrivateType* base_component_Private = (base_component_PrivateType*)omxComponent->pComponentPrivate;
	base_component_PortType* base_component_Port;
	OMX_S32 i;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	
	/** There should be some mechanism to indicate whether the buffer is application allocated or component. And 
	component need to track that. so the fill buffer done call back need not to be called and IL client will
	pull the buffer from the output port*/

	if (nPortIndex >= stComponent->nports) {
		return OMX_ErrorBadPortIndex;
	}

	base_component_Port = base_component_Private->ports[nPortIndex];
	
	if (PORT_IS_TUNNELED(base_component_Port) && 
		PORT_IS_BUFFER_SUPPLIER(base_component_Port)) {
		return OMX_ErrorNone;
	}

	/* Buffers can be allocated only when the component is at loaded state and changing to idle state
	 */

	if (base_component_Port->transientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	
	for(i=0;i<MAX_BUFFERS;i++){
		DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s i=%i Buffer state=%x\n",__func__,i,base_component_Port->nBufferState[i]);
		if(!(base_component_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(base_component_Port->nBufferState[i] & BUFFER_ASSIGNED)){ 
			DEBUG(DEB_LEV_FULL_SEQ,"Inside %s i=%i Buffer state=%x\n",__func__,i,base_component_Port->nBufferState[i]);
			base_component_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(base_component_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* use the buffer */
			base_component_Port->pBuffer[i]->pBuffer = pBuffer;
			base_component_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			base_component_Port->pBuffer[i]->nFilledLen = 0;
			base_component_Port->pBuffer[i]->nOffset = 0;
			base_component_Port->pBuffer[i]->pPlatformPrivate = base_component_Port;
			base_component_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			base_component_Port->pBuffer[i]->nTickCount = 0;
			base_component_Port->pBuffer[i]->nTimeStamp = 0;
			*ppBufferHdr = base_component_Port->pBuffer[i];
			if (base_component_Port->sPortParam.eDir == OMX_DirInput) {
				base_component_Port->pBuffer[i]->nInputPortIndex = nPortIndex;
				if(base_component_Port->nTunnelFlags)
					base_component_Port->pBuffer[i]->nOutputPortIndex = base_component_Port->nTunneledPort;
				else 
					base_component_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports; // here is assigned a non-valid port index
			} else {				
				base_component_Port->pBuffer[i]->nOutputPortIndex = nPortIndex;				
				if(base_component_Port->nTunnelFlags)
					base_component_Port->pBuffer[i]->nInputPortIndex = base_component_Port->nTunneledPort;
				else 
					base_component_Port->pBuffer[i]->nInputPortIndex = stComponent->nports; // here is assigned a non-valid port index

			}
			base_component_Port->nBufferState[i] |= BUFFER_ASSIGNED;
			base_component_Port->nBufferState[i] |= HEADER_ALLOCATED;
			base_component_Port->nNumAssignedBuffers++;
			if (base_component_Port->sPortParam.nBufferCountActual == base_component_Port->nNumAssignedBuffers) {
				base_component_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(base_component_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,base_component_Port->sPortParam.nBufferCountActual,
				base_component_Port->nNumAssignedBuffers,
				base_component_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = (base_component_PrivateType*)omxComponent->pComponentPrivate;
	base_component_PortType* base_component_Port;
	OMX_S32 i;

	if (nPortIndex >= stComponent->nports) {
		return OMX_ErrorBadPortIndex;
	}

	base_component_Port = base_component_Private->ports[nPortIndex];
	
	if (PORT_IS_TUNNELED(base_component_Port) && 
		PORT_IS_BUFFER_SUPPLIER(base_component_Port)) {
		return OMX_ErrorBadPortIndex;
	}
	if (base_component_Port->transientState != OMX_StateIdle) {
		DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to receive buffers\n", __func__);
		return OMX_ErrorIncorrectStateTransition;
	}
	for(i=0;i<MAX_BUFFERS;i++){
		if(!(base_component_Port->nBufferState[i] & BUFFER_ALLOCATED) && 
			!(base_component_Port->nBufferState[i] & BUFFER_ASSIGNED)){
			base_component_Port->pBuffer[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
			setHeader(base_component_Port->pBuffer[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* allocate the buffer */
			base_component_Port->pBuffer[i]->pBuffer = malloc(nSizeBytes);
			if(base_component_Port->pBuffer[i]->pBuffer==NULL)
				return OMX_ErrorInsufficientResources;
			base_component_Port->pBuffer[i]->nAllocLen = nSizeBytes;
			base_component_Port->pBuffer[i]->pPlatformPrivate = base_component_Port;
			base_component_Port->pBuffer[i]->pAppPrivate = pAppPrivate;
			*pBuffer = base_component_Port->pBuffer[i];
			base_component_Port->nBufferState[i] |= BUFFER_ALLOCATED;
			base_component_Port->nBufferState[i] |= HEADER_ALLOCATED;
			
			if (base_component_Port->sPortParam.eDir == OMX_DirInput) {
				base_component_Port->pBuffer[i]->nInputPortIndex = nPortIndex;
				// here is assigned a non-valid port index
				base_component_Port->pBuffer[i]->nOutputPortIndex = stComponent->nports;
			} else {
				// here is assigned a non-valid port index
				base_component_Port->pBuffer[i]->nInputPortIndex = stComponent->nports;
				base_component_Port->pBuffer[i]->nOutputPortIndex = nPortIndex;
			}
			
			base_component_Port->nNumAssignedBuffers++;
			DEBUG(DEB_LEV_PARAMS, "base_component_Port->nNumAssignedBuffers %i\n", base_component_Port->nNumAssignedBuffers);
			
			if (base_component_Port->sPortParam.nBufferCountActual == base_component_Port->nNumAssignedBuffers) {
				base_component_Port->sPortParam.bPopulated = OMX_TRUE;
				tsem_up(base_component_Port->pFullAllocationSem);
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s actual buffer=%d,assinged=%d fas=%d\n",
				__func__,base_component_Port->sPortParam.nBufferCountActual,
				base_component_Port->nNumAssignedBuffers,
				base_component_Port->pFullAllocationSem->semval);
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_ERR, "Error: no available buffers\n");
	return OMX_ErrorInsufficientResources;
}

OMX_ERRORTYPE base_component_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = (base_component_PrivateType*)omxComponent->pComponentPrivate;
	base_component_PortType* base_component_Port;
	
	OMX_S32 i;
	OMX_BOOL foundBuffer;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	
	if (nPortIndex >= stComponent->nports) {
		return OMX_ErrorBadPortIndex;
	}

	base_component_Port = base_component_Private->ports[nPortIndex];
		
	if (PORT_IS_TUNNELED(base_component_Port) && 
		PORT_IS_BUFFER_SUPPLIER(base_component_Port)){
		return OMX_ErrorNone;
	}
	DEBUG(DEB_LEV_PARAMS,"\nIn %s bEnabled=%d,bPopulated=%d state=%d,transientState=%d\n",
		__func__,base_component_Port->sPortParam.bEnabled,base_component_Port->sPortParam.bPopulated,
		stComponent->state,base_component_Port->transientState);
	
	if (base_component_Port->transientState != OMX_StateLoaded 		 
		&& base_component_Port->transientState != OMX_StateInvalid ) {
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
		if((base_component_Port->nBufferState[i] & BUFFER_ALLOCATED) &&
			(base_component_Port->pBuffer[i]->pBuffer == pBuffer->pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			base_component_Port->nNumAssignedBuffers--;
			free(pBuffer->pBuffer);
			if(base_component_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			base_component_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
		else if((base_component_Port->nBufferState[i] & BUFFER_ASSIGNED) &&
			(base_component_Port->pBuffer[i] == pBuffer)){
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing %i buffer of port %i\n", i, nPortIndex);
			
			base_component_Port->nNumAssignedBuffers--;
			if(base_component_Port->nBufferState[i] & HEADER_ALLOCATED ) {
				free(pBuffer);
			}
			base_component_Port->nBufferState[i] = BUFFER_FREE;
			break;
		}
	}
	/*check if there are some buffers already to be freed */
	foundBuffer = OMX_FALSE;
	for (i = 0; i< base_component_Port->sPortParam.nBufferCountActual ; i++) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Buffer flags - %i - %i\n", i, base_component_Port->nBufferState[i]);
	
		if (base_component_Port->nBufferState[i] != BUFFER_FREE) {
			foundBuffer = OMX_TRUE;
			break;
		}
	}
	if (!foundBuffer) {
		tsem_up(base_component_Port->pFullAllocationSem);
		base_component_Port->sPortParam.bPopulated = OMX_FALSE;
	}
	return OMX_ErrorNone;
}

/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param pCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
 */
OMX_ERRORTYPE base_component_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	stComponent->callbacks = pCallbacks;
	stComponent->callbackData = pAppData;
	
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = (base_component_PrivateType*)omxComponent->pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_U32 i;
	
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
					err=(*(base_component_Private->Init))(stComponent);
					if(err!=OMX_ErrorNone) {
						DEBUG(DEB_LEV_ERR, "In %s  base_component_Init returned error %i\n",
							__func__,err);
						return OMX_ErrorInsufficientResources;
					}
					for (i = 0; i < stComponent->nports; i++) {
						base_component_Private->ports[i]->transientState = OMX_StateIdle;
					}
			} else if ((nParam == OMX_StateLoaded) && (stComponent->state == OMX_StateIdle)) {
					for (i = 0; i < stComponent->nports; i++) {
						tsem_reset(base_component_Private->ports[i]->pFullAllocationSem);
						if (PORT_IS_ENABLED(base_component_Private->ports[i]))						
							base_component_Private->ports[i]->transientState = OMX_StateLoaded;
					}
			}	else if (nParam == OMX_StateInvalid) {
					for (i = 0; i < stComponent->nports; i++) {
						if (PORT_IS_ENABLED(base_component_Private->ports[i]))						
							base_component_Private->ports[i]->transientState = OMX_StateInvalid;
					}
			}
		break;
		case OMX_CommandFlush:
			DEBUG(DEB_LEV_FULL_SEQ, "In OMX_CommandFlush state is %i\n", stComponent->state);
			if ((stComponent->state != OMX_StateExecuting) && (stComponent->state != OMX_StatePause)) {
				err = OMX_ErrorIncorrectStateOperation;
				break;

			}
			if ((nParam != -1) && nParam >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}
			base_component_SetPortFlushFlag(stComponent, nParam, OMX_TRUE);
			
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			message->messageType = SENDCOMMAND_MSG_TYPE;
			message->messageParam1 = OMX_CommandFlush;
			message->messageParam2 = nParam;
			message->pCmdData=pCmdData;
		break;
		case OMX_CommandPortDisable:
			if ((nParam != -1) && nParam >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}
			
			for (i = 0; i < stComponent->nports; i++) {
				tsem_reset(base_component_Private->ports[i]->pFullAllocationSem);
			}

			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortDisable state is %i\n", stComponent->state);
			if (nParam == -1) {
				for (i = 0; i < stComponent->nports; i++) {
					if (!PORT_IS_ENABLED(base_component_Private->ports[i])) {
						err = OMX_ErrorIncorrectStateOperation;
						break;
					} else {
						base_component_Private->ports[i]->transientState = OMX_StateLoaded;
					}
				}
			} else {
					if (!PORT_IS_ENABLED(base_component_Private->ports[nParam])) {
						err = OMX_ErrorIncorrectStateOperation;
						break;
					} else {
						base_component_Private->ports[nParam]->transientState = OMX_StateLoaded;
					}				
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
			if ((nParam != -1) && nParam >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}		
			for (i = 0; i < stComponent->nports; i++) {
				tsem_reset(base_component_Private->ports[i]->pFullAllocationSem);
			}
			
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandPortEnable state is %i\n", stComponent->state);
			
			if (nParam == -1) {
				for (i = 0; i < stComponent->nports; i++) {
					if (PORT_IS_ENABLED(base_component_Private->ports[i])) {
						err = OMX_ErrorIncorrectStateOperation;
						break;
					} else {
						base_component_Private->ports[i]->transientState = OMX_StateIdle;
					}
				}
			} else {
					if (PORT_IS_ENABLED(base_component_Private->ports[nParam])) {
						err = OMX_ErrorIncorrectStateOperation;
						break;
					} else {
						base_component_Private->ports[nParam]->transientState = OMX_StateIdle;
					}				
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
			
			if ((nParam != -1) && nParam >= stComponent->nports) {
				return OMX_ErrorBadPortIndex;
			}

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

OMX_ERRORTYPE base_component_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Tearing down component...\n");

	/* Take care of tearing down resources if not in loaded state */
	if(stComponent->state != OMX_StateLoaded)
		(*(base_component_Private->Deinit))(stComponent);

	return OMX_ErrorNone;
}

/** Increment the flush semaphore falue
 */
inline void base_component_IncFlushSem(base_component_PortType* pPort, pthread_mutex_t* mutex) {
	OMX_BOOL flag;

	pthread_mutex_lock(mutex);
	flag=pPort->bWaitingFlushSem;
	if(flag==OMX_TRUE) {
		if (PORT_IS_TUNNELED(pPort) && 
		    PORT_IS_BUFFER_SUPPLIER(pPort)) {
			if(pPort->nNumBufferFlushed==pPort->nNumTunnelBuffer) {
				tsem_up(pPort->pFlushSem);
			}
		}
	}
	pthread_mutex_unlock(mutex);

}


OMX_ERRORTYPE base_component_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_U32 portIndex;
	tsem_t* pInputSem;
	pthread_mutex_t *pInmutex;
	queue_t* pInputQueue;
	OMX_ERRORTYPE err=OMX_ErrorNone;
	OMX_BOOL flag;

 	portIndex = pBuffer->nInputPortIndex;
	if (portIndex >= stComponent->nports ||
			base_component_Private->ports[portIndex]->sPortParam.eDir != OMX_DirInput) {
		DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
		return OMX_ErrorBadPortIndex;
	}
	
	pInputSem = base_component_Private->ports[portIndex]->pBufferSem;
	pInputQueue = base_component_Private->ports[portIndex]->pBufferQueue;
	pInmutex = &base_component_Private->ports[portIndex]->mutex;
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

	if(!PORT_IS_ENABLED(base_component_Private->ports[portIndex]))
		return OMX_ErrorIncorrectStateOperation;

	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}

	/* Queue this buffer on the output port */
	queue(pInputQueue, pBuffer);
	
	/* And notify the buffer management thread we have a fresh new buffer to manage */
	if(base_component_Private->ports[portIndex]->bIsPortFlushed==OMX_FALSE) {
		base_component_Private->inbuffer++;
		tsem_up(pInputSem);
	}
    else {
		if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex]) && 
			PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
			pthread_mutex_lock(pInmutex);
			base_component_Private->ports[portIndex]->nNumBufferFlushed++;
			pthread_mutex_unlock(pInmutex);
		}
		else  {
			base_component_Private->inbuffer++;
			tsem_up(pInputSem);
		}
	}

	base_component_IncFlushSem(base_component_Private->ports[portIndex], &base_component_Private->ports[portIndex]->mutex);
	
	DEBUG(DEB_LEV_FULL_SEQ,"In %s no of in buffer=%d\n",__func__,base_component_Private->inbuffer);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_U32 portIndex;
	
	tsem_t* pOutputSem;
	pthread_mutex_t *pOutmutex;
	queue_t* pOutputQueue;
	OMX_ERRORTYPE err=OMX_ErrorNone;
	OMX_BOOL flag;
 
 	portIndex = pBuffer->nOutputPortIndex;
	if (portIndex >= stComponent->nports ||
			base_component_Private->ports[portIndex]->sPortParam.eDir != OMX_DirOutput) {
		DEBUG(DEB_LEV_ERR, "In %s: wrong port code for this operation\n", __func__);
		return OMX_ErrorBadPortIndex;
	}
	
	pOutputSem = base_component_Private->ports[portIndex]->pBufferSem;
	pOutputQueue = base_component_Private->ports[portIndex]->pBufferQueue;
	pOutmutex = &base_component_Private->ports[portIndex]->mutex;
		 	
	/* We are not accepting buffers if not in executing or
	 * paused or idle state
	 */
	if(stComponent->state != OMX_StateExecuting &&
		stComponent->state != OMX_StatePause &&
		stComponent->state != OMX_StateIdle){
		DEBUG(DEB_LEV_ERR, "In %s: we are not in executing or paused state\n", __func__);
		return OMX_ErrorInvalidState;
	}

	if(!PORT_IS_ENABLED(base_component_Private->ports[portIndex]))
		return OMX_ErrorIncorrectStateOperation;

	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}
	
	/* Queue this buffer on the output port */
	queue(pOutputQueue, pBuffer);

	/* Signal that some new buffers are available on out put port */
	DEBUG(DEB_LEV_FULL_SEQ, "In %s: signalling the presence of new buffer on output port\n", __func__);

	if(base_component_Private->ports[portIndex]->bIsPortFlushed==OMX_FALSE) {
		base_component_Private->outbuffer++;
		tsem_up(pOutputSem);
	}
    else {
		if (PORT_IS_TUNNELED(base_component_Private->ports[portIndex]) && 
			PORT_IS_BUFFER_SUPPLIER(base_component_Private->ports[portIndex])) {
			pthread_mutex_lock(pOutmutex);
			base_component_Private->ports[portIndex]->nNumBufferFlushed++;
			pthread_mutex_unlock(pOutmutex);
		}
		else {
			base_component_Private->outbuffer++;
			tsem_up(pOutputSem);
		}
	}

	base_component_IncFlushSem(base_component_Private->ports[portIndex], &base_component_Private->ports[portIndex]->mutex);


	
	DEBUG(DEB_LEV_FULL_SEQ,"In %s no of out buffer=%d\n",__func__,base_component_Private->outbuffer);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE base_component_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE param;
	OMX_PARAM_BUFFERSUPPLIERTYPE pSupplier;

	stComponentType* stComponent = (stComponentType*)hComp;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;

	if (nPort >= stComponent->nports) {
	return OMX_ErrorBadPortIndex;
	}

	if (pTunnelSetup == NULL || hTunneledComp == 0) {
	/* cancel previous tunnel */
		base_component_Private->ports[nPort]->hTunneledComponent = 0;
		base_component_Private->ports[nPort]->nTunneledPort = 0;
		base_component_Private->ports[nPort]->nTunnelFlags = 0;
		base_component_Private->ports[nPort]->eBufferSupplier=OMX_BufferSupplyUnspecified;
		return OMX_ErrorNone;
	}

	if (base_component_Private->ports[nPort]->sPortParam.eDir == OMX_DirInput) {
		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		/// \todo insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Port Definition error=0x%08x Line=%d\n",__func__,err,__LINE__);
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		base_component_Private->ports[nPort]->nNumTunnelBuffer=param.nBufferCountMin;

		/*Check domain of the tunnelled component*/
		err = (*(base_component_Private->DomainCheck))(param);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Domain Check Error=%08x\n",err);
			return err;
		}

		/* Get Buffer Supplier type of the Tunnelled Component*/
		pSupplier.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);

		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Buffer Supplier error=0x%08x Line=%d\n",__func__,err,__LINE__);
			return OMX_ErrorPortsNotCompatible;
		}
		else {
			DEBUG(DEB_LEV_FULL_SEQ,"Tunneled Port eBufferSupplier=%x\n",pSupplier.eBufferSupplier);
		}

		// store the current callbacks, if defined
		base_component_Private->ports[nPort]->hTunneledComponent = hTunneledComp;
		base_component_Private->ports[nPort]->nTunneledPort = nTunneledPort;
		base_component_Private->ports[nPort]->nTunnelFlags = 0;
		// Negotiation
		if (pTunnelSetup->nTunnelFlags & OMX_PORTTUNNELFLAG_READONLY) {
			// the buffer provider MUST be the output port provider
			pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
			base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;	
			base_component_Private->ports[nPort]->eBufferSupplier=OMX_BufferSupplyInput;
		} else {
			if (pTunnelSetup->eSupplier == OMX_BufferSupplyInput) {
				base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				base_component_Private->ports[nPort]->eBufferSupplier=OMX_BufferSupplyInput;
			} else if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified) {
				pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
				base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				base_component_Private->ports[nPort]->eBufferSupplier=OMX_BufferSupplyInput;
			}
		}
		base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_ESTABLISHED;

		/* Set Buffer Supplier type of the Tunnelled Component after final negotiation*/
		pSupplier.nPortIndex=nTunneledPort;
		pSupplier.eBufferSupplier=base_component_Private->ports[nPort]->eBufferSupplier;
		err = OMX_SetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Buffer Supplier error=0x%08x Line=%d\n",__func__,err,__LINE__);
			base_component_Private->ports[nPort]->nTunnelFlags=0;
			return OMX_ErrorPortsNotCompatible;
		}
	} else  {
		// output port
		// all the consistency checks are under other component responsibility

		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		/// \todo insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Port Definition error=0x%08x Line=%d\n",__func__,err,__LINE__);
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		base_component_Private->ports[nPort]->nNumTunnelBuffer=param.nBufferCountMin;

		base_component_Private->ports[nPort]->hTunneledComponent = hTunneledComp;
		base_component_Private->ports[nPort]->nTunneledPort = nTunneledPort;
		pTunnelSetup->eSupplier = OMX_BufferSupplyOutput;
		base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
		base_component_Private->ports[nPort]->nTunnelFlags |= TUNNEL_ESTABLISHED;

		base_component_Private->ports[nPort]->eBufferSupplier=OMX_BufferSupplyOutput;
	}
	return OMX_ErrorNone;
}

void base_component_returnInputBuffer(stComponentType* stComponent,OMX_BUFFERHEADERTYPE* pInputBuffer,base_component_PortType *pPort) {
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	queue_t* pInputQueue = pPort->pBufferQueue;
	OMX_BOOL flag;
	OMX_BOOL *inbufferUnderProcess=&pPort->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&pPort->mutex;	
	
	if (PORT_IS_TUNNELED(pPort)) {
		if(PORT_IS_ENABLED(pPort)){
			if(PORT_IS_BEING_FLUSHED(pPort)) {
				OMX_FillThisBuffer(pPort->hTunneledComponent, pInputBuffer);
				base_component_Private->inbuffercb++;
			} else { // port is not being flushed
				if(!PORT_IS_BUFFER_SUPPLIER(pPort)) {
					OMX_FillThisBuffer(pPort->hTunneledComponent, pInputBuffer);
					base_component_Private->inbuffercb++;
				} else { // port is buffer supplier
					queue(pInputQueue,pInputBuffer);
					pthread_mutex_lock(pInmutex);
					pPort->nNumBufferFlushed++;
					pthread_mutex_unlock(pInmutex);
				}
			}
		}
		else { /*Port Disabled then call FTB if port is not the supplier else dont call*/
			if(!PORT_IS_BUFFER_SUPPLIER(pPort)) {
				OMX_FillThisBuffer(pPort->hTunneledComponent, pInputBuffer);
				base_component_Private->inbuffercb++;
			} else { // pPort is buffer supplier
				queue(pInputQueue,pInputBuffer);
				pthread_mutex_lock(pInmutex);
				pPort->nNumBufferFlushed++;
				pthread_mutex_unlock(pInmutex);
			}
		}
	} else { // port is not tunneled
		(*(stComponent->callbacks->EmptyBufferDone))
			(pHandle, stComponent->callbackData, pInputBuffer);
		base_component_Private->inbuffercb++;
	}
	
	pthread_mutex_lock(pInmutex);
	*inbufferUnderProcess = OMX_FALSE;
	if(PORT_IS_WAITING_FLUSH_SEMAPHORE(pPort)) {		
		if ( ! (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort) &&
				pPort->nNumBufferFlushed != pPort->nNumTunnelBuffer) ) {
			tsem_up(pPort->pFlushSem);
		}
	}
	pthread_mutex_unlock(pInmutex);		
}

void base_component_returnOutputBuffer(stComponentType* stComponent,OMX_BUFFERHEADERTYPE* pOutputBuffer,base_component_PortType *pPort){ 
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	base_component_PrivateType* base_component_Private = stComponent->omx_component.pComponentPrivate;
	queue_t* pOutputQueue = pPort->pBufferQueue;
	OMX_BOOL flag;
	OMX_BOOL *outbufferUnderProcess=&pPort->bBufferUnderProcess;
	pthread_mutex_t *pOutmutex=&pPort->mutex;
	
	if (PORT_IS_TUNNELED(pPort)) {
		if(PORT_IS_ENABLED(pPort)){
			if(PORT_IS_BEING_FLUSHED(pPort)) {
				OMX_EmptyThisBuffer(pPort->hTunneledComponent, pOutputBuffer);
				base_component_Private->outbuffercb++;
			}
			else { // port is not being flushed
				if(!PORT_IS_BUFFER_SUPPLIER(pPort)) {
					OMX_EmptyThisBuffer(pPort->hTunneledComponent, pOutputBuffer);
					base_component_Private->outbuffercb++;
				}
				else { // port is buffer supplier
					queue(pOutputQueue,pOutputBuffer);
					pthread_mutex_lock(pOutmutex);
					pPort->nNumBufferFlushed++;
					pthread_mutex_unlock(pOutmutex);
				}
			}
		}
		else { /*Port Disabled then call ETB if port is not the supplier else dont call*/
			if(!PORT_IS_BUFFER_SUPPLIER(pPort)) {
				OMX_EmptyThisBuffer(pPort->hTunneledComponent, pOutputBuffer);
				base_component_Private->outbuffercb++;
			}
			else { // pPort is buffer supplier
				queue(pOutputQueue,pOutputBuffer);
				pthread_mutex_lock(pOutmutex);
				pPort->nNumBufferFlushed++;
				pthread_mutex_unlock(pOutmutex);
			}
		}
	} else {
		(*(stComponent->callbacks->FillBufferDone))
			(pHandle, stComponent->callbackData, pOutputBuffer);
		base_component_Private->outbuffercb++;
	}

	pthread_mutex_lock(pOutmutex);
	*outbufferUnderProcess = OMX_FALSE;
	if(PORT_IS_WAITING_FLUSH_SEMAPHORE(pPort)) {		
		if ( ! (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort) &&
				pPort->nNumBufferFlushed != pPort->nNumTunnelBuffer) ) {
			tsem_up(pPort->pFlushSem);
		}
	}
	pthread_mutex_unlock(pOutmutex);		
}

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void base_component_Panic() {
	exit(EXIT_FAILURE);
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* File EOF */

