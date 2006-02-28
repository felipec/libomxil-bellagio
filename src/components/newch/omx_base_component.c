/**
 * fixme
 * 
	@file src/components/reference/omx_reference.c
	
	OpenMax reference component. This component does not perform any multimedia
	processing.	It is used as a reference for new components development.
	
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
	
	2006/02/08:  OpenMAX reference component version 0.1

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

#include "omxcore.h"
#include "omx_base_component.h"

#include "tsemaphore.h"
#include "queue.h"

/*********************
 *
 * Component template
 *
 **********************/

stComponentType* omx_base_component_CreateComponentStruct() {
	stComponentType *omx_base_component = (stComponentType *) malloc(sizeof(stComponentType)); 
	if (omx_base_component) {
	  omx_base_component->name = "OMX.st.base";
	  omx_base_component->state = OMX_StateLoaded;
	  omx_base_component->callbacks = NULL;
	  omx_base_component->callbackData = NULL;
	  omx_base_component->nports = 2;
		omx_base_component->coreDescriptor = NULL;
	  omx_base_component->constructor = omx_base_component_Constructor;
	  omx_base_component->destructor = omx_base_component_Destructor;
	  omx_base_component->messageHandler = omx_base_component_MessageHandler;
		omx_base_component->omx_component.nSize = sizeof(OMX_COMPONENTTYPE);
	  omx_base_component->omx_component.pComponentPrivate = NULL;
	  omx_base_component->omx_component.pApplicationPrivate = NULL;
	  omx_base_component->omx_component.GetComponentVersion = omx_base_component_GetComponentVersion;
	  omx_base_component->omx_component.SendCommand = omx_base_component_SendCommand;
	  omx_base_component->omx_component.GetParameter = omx_base_component_GetParameter;
	  omx_base_component->omx_component.SetParameter = omx_base_component_SetParameter;
	  omx_base_component->omx_component.GetConfig = omx_base_component_GetConfig;
	  omx_base_component->omx_component.SetConfig = omx_base_component_SetConfig;
	  omx_base_component->omx_component.GetExtensionIndex = omx_base_component_GetExtensionIndex;
	  omx_base_component->omx_component.GetState = omx_base_component_GetState;
	  omx_base_component->omx_component.ComponentTunnelRequest = omx_base_component_TunnelRequest;
	  omx_base_component->omx_component.UseBuffer = omx_base_component_UseBuffer;
	  omx_base_component->omx_component.AllocateBuffer = omx_base_component_AllocateBuffer;
	  omx_base_component->omx_component.FreeBuffer = omx_base_component_FreeBuffer;
	  omx_base_component->omx_component.EmptyThisBuffer = omx_base_component_EmptyThisBuffer;
	  omx_base_component->omx_component.FillThisBuffer = omx_base_component_FillThisBuffer;
	  omx_base_component->omx_component.SetCallbacks = omx_base_component_SetCallbacks;
	  omx_base_component->omx_component.ComponentDeInit = omx_base_component_DeInit;
	  omx_base_component->omx_component.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
	  omx_base_component->omx_component.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
	  omx_base_component->omx_component.nVersion.s.nRevision = SPECREVISION;
	  omx_base_component->omx_component.nVersion.s.nStep = SPECSTEP;
	}
	return omx_base_component;	
}

/**************************************************************
 *
 * PRIVATE: component private entry points and helper functions
 *
 **************************************************************/

/** This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the reference component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated omx_base_component_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE omx_base_component_Constructor(stComponentType* stComponent)
{
	omx_base_component_PrivateType* omx_base_component_Private;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	/** Allocate and fill in reference private structures 
	 */
	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = malloc(sizeof(omx_base_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
		memset(stComponent->omx_component.pComponentPrivate, 0, sizeof(omx_base_component_PrivateType));
	}
	omx_base_component_Private = stComponent->omx_component.pComponentPrivate;

	/** Default parameters setting */
	omx_base_component_Private->bIsInit = OMX_FALSE;
	omx_base_component_Private->nGroupPriority = 0;
	omx_base_component_Private->nGroupID = 0;
	omx_base_component_Private->pMark=NULL;
	omx_base_component_Private->bCmdUnderProcess=OMX_FALSE;
	omx_base_component_Private->bWaitingForCmdToFinish=OMX_FALSE;

	/** generic parameter NOT related to a specific port */
	setHeader(&omx_base_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
	omx_base_component_Private->sPortTypesParam.nPorts = 0;
	omx_base_component_Private->sPortTypesParam.nStartPortNumber = 0;	

	/* Set default function pointers */
	omx_base_component_Private->Init = &omx_base_component_Init;
	omx_base_component_Private->Deinit = &omx_base_component_Deinit;
	omx_base_component_Private->DoStateSet = &omx_base_component_DoStateSet;
	
	pthread_mutex_init(&omx_base_component_Private->exit_mutex, NULL);
	pthread_cond_init(&omx_base_component_Private->exit_condition, NULL);
	pthread_mutex_init(&omx_base_component_Private->cmd_mutex, NULL);	
	
	pthread_mutex_init(&omx_base_component_Private->cmd_mutex, NULL);
	omx_base_component_Private->pCmdSem = malloc(sizeof(tsem_t));
	if(omx_base_component_Private->pCmdSem==NULL)
		return OMX_ErrorInsufficientResources;
	tsem_init(omx_base_component_Private->pCmdSem, 0);

	pthread_mutex_lock(&omx_base_component_Private->exit_mutex);
	omx_base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&omx_base_component_Private->exit_mutex);
	
	DEBUG(DEB_LEV_FULL_SEQ,"Returning from %s\n",__func__);
	
	return OMX_ErrorNone;
}

/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 * \param the message that has been passed to core
 */
OMX_ERRORTYPE omx_base_component_MessageHandler(coreMessage_t* message)
{
	stComponentType* stComponent = message->stComponent;
	omx_base_component_PrivateType* omx_base_component_Private;
	OMX_BOOL waitFlag=OMX_FALSE;
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;	
	
	OMX_ERRORTYPE err;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	omx_base_component_Private = stComponent->omx_component.pComponentPrivate;

	/** Dealing with a SendCommand call.
	 * -messageParam1 contains the command to execute
	 * -messageParam2 contains the parameter of the command
	 *  (destination state in case of a state change command).
	 */

	pthread_mutex_lock(&omx_base_component_Private->cmd_mutex);
	omx_base_component_Private->bCmdUnderProcess=OMX_TRUE;
	pthread_mutex_unlock(&omx_base_component_Private->cmd_mutex);	 
	 
	if(message->messageType == SENDCOMMAND_MSG_TYPE){
		switch(message->messageParam1){
			case OMX_CommandStateSet:
				/* Do the actual state change */
				err =  (*(omx_base_component_Private->DoStateSet))(stComponent, message->messageParam2);
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
					DEBUG(DEB_LEV_SIMPLE_SEQ,"running callback in %s\n",__func__);
					(*(stComponent->callbacks->EventHandler))
						(pHandle,
							stComponent->callbackData,
							OMX_EventCmdComplete, /* The command was completed */
							OMX_CommandStateSet, /* The commands was a OMX_CommandStateSet */
							message->messageParam2, /* The state has been changed in message->messageParam2 */
							NULL);
				}
				break;
			case OMX_CommandMarkBuffer:
				omx_base_component_Private->pMark=(OMX_MARKTYPE *)message->pCmdData;
			break;				
			default:
				DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageParam1);
			break;
		}
		/* Dealing with an asynchronous error condition
		 */
	}
	
	pthread_mutex_lock(&omx_base_component_Private->cmd_mutex);
	omx_base_component_Private->bCmdUnderProcess=OMX_FALSE;
	waitFlag=omx_base_component_Private->bWaitingForCmdToFinish;
	pthread_mutex_unlock(&omx_base_component_Private->cmd_mutex);

	if(waitFlag==OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: Signalling command finish condition \n", __func__);
		tsem_up(omx_base_component_Private->pCmdSem);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s: \n", __func__);	

	return OMX_ErrorNone;
}


/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For reference component, the following is done:
 * 1) Put the component in IDLE state
 * 2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE omx_base_component_Init(stComponentType* stComponent)
{
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (omx_base_component_Private->bIsInit == OMX_TRUE) {
		DEBUG(DEB_LEV_ERR, "In %s Big error. It should not happen\n", __func__);
		return OMX_ErrorIncorrectStateOperation;
	}
	omx_base_component_Private->bIsInit = OMX_TRUE;
	
	pthread_cond_init(&omx_base_component_Private->executingCondition, NULL);
	pthread_mutex_init(&omx_base_component_Private->executingMutex, NULL);

	pthread_mutex_lock(&omx_base_component_Private->exit_mutex);
	omx_base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&omx_base_component_Private->exit_mutex);
	
	/** Spawn buffer management thread 
	 * The pointer omx_base_component_Private->BufferMgmtFunction must be set
	 * This is an abstract class, set it in the constructor of your base class
	 */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hey, starting threads!\n");
	omx_base_component_Private->bufferMgmtThreadID = pthread_create(&omx_base_component_Private->bufferMgmtThread,
		NULL,
		omx_base_component_Private->BufferMgmtFunction,
		stComponent);	

	if(omx_base_component_Private->bufferMgmtThreadID < 0){
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
OMX_ERRORTYPE omx_base_component_Deinit(stComponentType* stComponent)
{
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 ret;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	omx_base_component_Private->bIsInit = OMX_FALSE;

	/** Trash buffer mangement thread.
	 */
	pthread_mutex_lock(&omx_base_component_Private->exit_mutex);
	omx_base_component_Private->bExit_buffer_thread=OMX_TRUE;
	pthread_mutex_unlock(&omx_base_component_Private->exit_mutex);
	
	ret=pthread_join(omx_base_component_Private->bufferMgmtThread,NULL);

	pthread_mutex_lock(&omx_base_component_Private->exit_mutex);
	omx_base_component_Private->bExit_buffer_thread=OMX_FALSE;
	pthread_mutex_unlock(&omx_base_component_Private->exit_mutex);
	
	DEBUG(DEB_LEV_FULL_SEQ,"Execution thread re-joined\n");
	/*Deinitialize mutexes and conditional variables*/
	pthread_cond_destroy(&omx_base_component_Private->executingCondition);
	pthread_mutex_destroy(&omx_base_component_Private->executingMutex);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Deinitialize mutexes and conditional variables\n");

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);
	return OMX_ErrorNone;
}

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE omx_base_component_Destructor(stComponentType* stComponent)
{
	OMX_BOOL exit_thread=OMX_FALSE,cmdFlag=OMX_FALSE;
	OMX_U32 ret;
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	if (omx_base_component_Private->bIsInit != OMX_FALSE) {
		(*(omx_base_component_Private->Deinit))(stComponent);
	}
	
	pthread_mutex_lock(&omx_base_component_Private->exit_mutex);
	exit_thread = omx_base_component_Private->bExit_buffer_thread;
	pthread_mutex_unlock(&omx_base_component_Private->exit_mutex);
	if(exit_thread == OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for condition...\n",__func__);
		pthread_cond_wait(&omx_base_component_Private->exit_condition,&omx_base_component_Private->exit_mutex);
	}

	pthread_mutex_lock(&omx_base_component_Private->cmd_mutex);
	cmdFlag=omx_base_component_Private->bCmdUnderProcess;
	if(cmdFlag==OMX_TRUE) {
		omx_base_component_Private->bWaitingForCmdToFinish=OMX_TRUE;
		pthread_mutex_unlock(&omx_base_component_Private->cmd_mutex);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish ...\n",__func__);
		tsem_down(omx_base_component_Private->pCmdSem);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish over..cmutex=%x.\n",__func__,&omx_base_component_Private->cmd_mutex);

		pthread_mutex_lock(&omx_base_component_Private->cmd_mutex);
		omx_base_component_Private->bWaitingForCmdToFinish=OMX_FALSE;
		pthread_mutex_unlock(&omx_base_component_Private->cmd_mutex);
	}
	else {
		pthread_mutex_unlock(&omx_base_component_Private->cmd_mutex);
	}
	
	pthread_cond_destroy(&omx_base_component_Private->exit_condition);
	pthread_mutex_destroy(&omx_base_component_Private->exit_mutex);
	pthread_mutex_destroy(&omx_base_component_Private->cmd_mutex);
	
	if(omx_base_component_Private->pCmdSem!=NULL)  {
	tsem_deinit(omx_base_component_Private->pCmdSem);
	free(omx_base_component_Private->pCmdSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Destroyed and Freeing pCmdSem semaphore\n");
	}

	stComponent->state = OMX_StateInvalid;
	
	free(stComponent->omx_component.pComponentPrivate);	
	
	stComponent->state = OMX_StateInvalid;
	free(stComponent->omx_component.pComponentPrivate);
	return OMX_ErrorNone;
}


/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param parameter the requested target state.
 */
OMX_ERRORTYPE omx_base_component_DoStateSet(stComponentType* stComponent, OMX_U32 destinationState)
{
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
	pthread_mutex_t* executingMutex = &omx_base_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &omx_base_component_Private->executingCondition;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s changing state from %i to %i\n", __func__, stComponent->state, (int)destinationState);

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
				stComponent->state = OMX_StateLoaded;
				(*(omx_base_component_Private->Deinit))(stComponent);
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
				stComponent->state = OMX_StateIdle;
				break;
				
			case OMX_StateIdle:
					return OMX_ErrorSameState;
				break;
				
			case OMX_StateExecuting:
			case OMX_StatePause:
				stComponent->state = OMX_StateIdle;
				break;
			default:
				DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
				return OMX_ErrorIncorrectStateTransition;
				break;
		}
		return OMX_ErrorNone;
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
				(*(omx_base_component_Private->Init))(stComponent);
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
			(*(omx_base_component_Private->Deinit))(stComponent);
			break;
			}
		
		if (omx_base_component_Private->bIsInit != OMX_FALSE) {
			(*(omx_base_component_Private->Deinit))(stComponent);
		}
		return OMX_ErrorNone;
		// Free all the resources!!!!
	}
	return OMX_ErrorNone;
}

/************************************
 *
 * PUBLIC: OMX component entry points
 *
 ************************************/

OMX_ERRORTYPE omx_base_component_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
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

/** Ask the component current parameter settings.
 * Number of ports is fixed to two.
 */
OMX_ERRORTYPE omx_base_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamPriorityMgmt:
			pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
			setHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
			pPrioMgmt->nGroupPriority = omx_base_component_Private->nGroupPriority;
			pPrioMgmt->nGroupID = omx_base_component_Private->nGroupID;
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
		//todo; port related stuff
		break;
		case OMX_IndexParamCompBufferSupplier:
		//todo; port related stuff
		break;
		default:
			return OMX_ErrorUnsupportedIndex;
		break;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_base_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
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
			omx_base_component_Private->nGroupPriority = pPrioMgmt->nGroupPriority;
			omx_base_component_Private->nGroupID = pPrioMgmt->nGroupID;
		break;
		case OMX_IndexParamAudioInit:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			err = OMX_ErrorBadParameter;
		break;
		case OMX_IndexParamPortDefinition: 
		//todo; port related stuff
		break;
		case OMX_IndexParamCompBufferSupplier:
		//todo; port related stuff
		break;
		default:
			// why not unsupported index? UN
			return OMX_ErrorBadParameter; 
		break;
	}
	
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "   Error during %s = %i\n", __func__, err);
	}
	return err;
}

OMX_ERRORTYPE omx_base_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE omx_base_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE omx_base_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType) {
	return OMX_ErrorNotImplemented;
}

/** Fill the internal component state
 */
OMX_ERRORTYPE omx_base_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	*pState = stComponent->state;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_base_component_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer)
{
	//todo; port related stuff
	return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE omx_base_component_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes)
{
	//todo; port related stuff
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE omx_base_component_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	//todo; port related stuff
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE omx_base_component_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	stComponent->callbacks = pCallbacks;
	stComponent->callbackData = pAppData;
	
	return OMX_ErrorNone;
}


/** The omx_base_component sendCommand method.
 * This has to return immediately.
 * Pack queue and command in a message and send it
 * to the core for handling it.
 */
OMX_ERRORTYPE omx_base_component_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
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
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandStateSet state is %i\n", stComponent->state);
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
				/** fixme; port related stuff UN
			if ((nParam == OMX_StateIdle) && (stComponent->state == OMX_StateLoaded)) {
					(*(omx_base_component_Private->omx_base_component_Init))(stComponent);
					if (omx_base_component_Private->inputPort.portParam.bEnabled)
						omx_base_component_Private->inputPort.transientState = OMX_StateIdle;
					if (omx_base_component_Private->outputPort.portParam.bEnabled)
						omx_base_component_Private->outputPort.transientState = OMX_StateIdle;
			} else if ((nParam == OMX_StateLoaded) && (stComponent->state == OMX_StateIdle)) {
					if (omx_base_component_Private->inputPort.portParam.bEnabled)
						omx_base_component_Private->inputPort.transientState = OMX_StateLoaded;
					if (omx_base_component_Private->outputPort.portParam.bEnabled)
						omx_base_component_Private->outputPort.transientState = OMX_StateLoaded;
			} */
		break;
		case OMX_CommandFlush:
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In OMX_CommandFlush state is %i\n", stComponent->state);
			if ((stComponent->state != OMX_StateExecuting) && (stComponent->state != OMX_StatePause)) {
				err = OMX_ErrorIncorrectStateOperation;
				break;

			}
			message = malloc(sizeof(coreMessage_t));
			message->stComponent = stComponent;
			message->messageType = SENDCOMMAND_MSG_TYPE;
			message->messageParam1 = OMX_CommandFlush;
			message->messageParam2 = nParam;
		break;
		default:
		err = OMX_ErrorUnsupportedIndex;
		break;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "%s returning err = %i\n", __func__, err);
	queue(messageQueue, message);
	tsem_up(messageSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "%s returning err = %i\n", __func__, err);
	return err;
}

OMX_ERRORTYPE omx_base_component_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE omx_base_component_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
	return OMX_ErrorNotImplemented;
}


/** This is called to free all component resources.
 * Still not clear why this is not done in the freehandle() call instead...
 */
OMX_ERRORTYPE omx_base_component_DeInit(
	OMX_IN  OMX_HANDLETYPE hComponent)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_base_component_PrivateType* omx_base_component_Private = stComponent->omx_component.pComponentPrivate;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Tearing down component...\n");

	/* Take care of tearing down resources if not in loaded state */
	if(stComponent->state != OMX_StateLoaded)
		(*(omx_base_component_Private->Deinit))(stComponent);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_base_component_TunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup) 
{
	return OMX_ErrorNotImplemented;
}

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void omx_base_component_Panic() {
	exit(EXIT_FAILURE);
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* File EOF */

