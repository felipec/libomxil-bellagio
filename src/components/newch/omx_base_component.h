#ifndef __OMX_BASE_COMPONENT_H__
#define __OMX_BASE_COMPONENT_H__

/**
 * fixme
 * 
	@file src/components/reference/omx_reference.h
	
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

#include <pthread.h>

#include "tsemaphore.h"
#include "queue.h"

/** Maximum number of buffers for the input port */
#define MAX_BUFFERS      4

/** Default size of the internal input buffer */
#define INTERNAL_IN_BUFFER_SIZE  8  * 1024
/** Default size of the internal output buffer */
#define INTERNAL_OUT_BUFFER_SIZE 16 * 1024


/**
 * Components Private Structure
 */
typedef struct omx_base_component_PrivateType{
	/// @param bIsInit Indicate whether component has been already initialized
	OMX_BOOL bIsInit;
	/// @param sPortTypesParam OpenMAX standard parameter that contains a short description of the available ports
	OMX_PORT_PARAM_TYPE sPortTypesParam;

	/// @param bufferMgmtThread The main component thread that receives input buffers and sends it to the output device
	pthread_t bufferMgmtThread;
	/// @param bufferMgmtThreadID The main component thread ID
	OMX_S32 bufferMgmtThreadID;
		
	/// @param executingMutex The mutex that synchronizes the start of the main thread and the state transition of the component to Executing
	pthread_mutex_t executingMutex;
	/// @param executingCondition The executing mutex condition
	pthread_cond_t executingCondition;
	/// @param bExit_buffer_thread boolean flag for the exit condition from the main component thread
	OMX_BOOL bExit_buffer_thread;
	/// @param exit_mutex mutex for the exit condition from the main component thread
	pthread_mutex_t exit_mutex;
	/// @param The exit mutex condition
	pthread_cond_t exit_condition;
	/// @param cmd_mutex Mutex uses for message access synchronization
	pthread_mutex_t cmd_mutex;
	/// @param pCmdSem Semaphore for message handling completion signaling
	tsem_t* pCmdSem;
	
	/// @param nGroupPriority Resource management field: component priority (common to a group of components)
	OMX_U32 nGroupPriority;
	/// @param nGroupID ID of a group of components that share the same logical chain
	OMX_U32 nGroupID;
	
	/// @param pMark This field holds the private data associated with a mark request, if any
	OMX_MARKTYPE *pMark;
	
	/// @param bCmdUnderProcess Boolean variable indicates whether command under process
	OMX_BOOL bCmdUnderProcess;
	/// @param bWaitingForCmdToFinish Boolean variable indicates whether cmdSem is waiting for command to finish
	OMX_BOOL bWaitingForCmdToFinish;	
	
	OMX_ERRORTYPE (*Init)(stComponentType* stComponent);
	OMX_ERRORTYPE (*Deinit)(stComponentType* stComponent);
	OMX_ERRORTYPE (*DoStateSet)(stComponentType*, OMX_U32);
	void* (*BufferMgmtFunction)(void* param);
} omx_base_component_PrivateType;

/* Component private entry points declaration */
stComponentType* omx_base_component_CreateComponentStruct();
OMX_ERRORTYPE omx_base_component_Constructor(stComponentType*);
OMX_ERRORTYPE omx_base_component_MessageHandler(coreMessage_t* message);
void* omx_base_component_BufferMgmtFunction(void* param);
OMX_ERRORTYPE omx_base_component_Init(stComponentType* stComponent);
OMX_ERRORTYPE omx_base_component_Deinit(stComponentType* stComponent);
OMX_ERRORTYPE omx_base_component_Destructor(stComponentType* stComponent);
OMX_ERRORTYPE omx_base_component_DoStateSet(stComponentType*, OMX_U32);

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void omx_base_component_Panic();



/** Component entry points declarations */
OMX_ERRORTYPE omx_base_component_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING pComponentName,
	OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
	OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
	OMX_OUT OMX_UUIDTYPE* pComponentUUID);

OMX_ERRORTYPE omx_base_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_base_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_base_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_base_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_base_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType);

OMX_ERRORTYPE omx_base_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState);

OMX_ERRORTYPE omx_base_component_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer);

OMX_ERRORTYPE omx_base_component_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE omx_base_component_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE omx_base_component_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData);

OMX_ERRORTYPE omx_base_component_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData);

OMX_ERRORTYPE omx_base_component_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE omx_base_component_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE omx_base_component_DeInit(
	OMX_IN  OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE omx_base_component_TunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

#endif //__OMX_BASE_COMPONENT_H__
