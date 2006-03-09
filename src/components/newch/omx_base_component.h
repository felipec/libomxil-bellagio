/**
	@file src/components/newch/omx_base_component.h
	
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
#ifndef _OMX_BASE_COMPONENT_H_
#define _OMX_BASE_COMPONENT_H_

#include <pthread.h>

#include "tsemaphore.h"
#include "queue.h"
#include "omx_classmagic.h"

/** Maximum number of buffers for the input port */
#define MAX_BUFFERS      4

/** Default size of the internal input buffer */
#define INTERNAL_IN_BUFFER_SIZE  8  * 1024
/** Default size of the internal output buffer */
#define INTERNAL_OUT_BUFFER_SIZE 16 * 1024

/** Maximum number of base_component component instances */
#define MAX_NUM_OF_base_component_INSTANCES 10


//UN command processing changes
#define UN_CPC

/**
 * The structure for port Type.
 */
CLASS(base_component_PortType)
#define base_component_PortType_FIELDS \
	/** @param pBuffer An array of pointers to buffer headers. */ \
	OMX_BUFFERHEADERTYPE *pBuffer[MAX_BUFFERS]; \
	/** @param nBufferState The State of the Buffer whether assigned or allocated */ \
	OMX_U32 nBufferState[MAX_BUFFERS]; \
	/** @param nNumAssignedBuffers Number of buffer assigned on each port */ \
	OMX_U32 nNumAssignedBuffers; \
	/** @param pBufferQueue queue for buffer to be processed by the port */ \
	queue_t* pBufferQueue; \
	/** @param pBufferSem Semaphore for buffer queue access synchronization */ \
	tsem_t* pBufferSem; \
	/** @param pFullAllocationSem Semaphore used to hold the state transition until the all the buffers needed have been allocated */ \
	tsem_t* pFullAllocationSem; \
	/** @param transientState A state transition between Loaded to Idle or Ilde to Loaded */ \
	OMX_STATETYPE transientState; \
	/** @param pFlushSem Semaphore that locks the execution until the buffers have been flushed, if needed */ \
	tsem_t* pFlushSem; \
	/** @param bWaitingFlushSem  Boolean variables indicate whether flush sem is down */ \
	OMX_BOOL bWaitingFlushSem; \
	/** @param bBufferUnderProcess  Boolean variables indicate whether the port is processing any buffer */ \
	OMX_BOOL bBufferUnderProcess; \
	/** @param mutex Mutex used for access synchronization to the processing thread */ \
	pthread_mutex_t mutex; \
	/** @param sPortParam General OpenMAX port parameter */ \
	OMX_PARAM_PORTDEFINITIONTYPE sPortParam; \
	/** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; \
	/** @param hTunneledComponent Handle to the tunnelled component */ \
	OMX_HANDLETYPE hTunneledComponent; \
	/** @param nTunneledPort Tunnelled port number */ \
	OMX_U32 nTunneledPort; \
	/** @param nTunnelFlags Tunnelling flag */ \
	OMX_U32 nTunnelFlags; \
	/** @param eBufferSupplier the type of supplier in case of tunneling */ \
	OMX_BUFFERSUPPLIERTYPE eBufferSupplier; \
	/** @param nNumTunnelBuffer Number of buffer to be tunnelled */ \
	OMX_U32 nNumTunnelBuffer; \
	/** @param bIsPortFlushed Boolean variables indicate whether the port is flushed */ \
	OMX_BOOL bIsPortFlushed;
ENDCLASS(base_component_PortType)



/**
 * Components Private Structure
 */
CLASS(base_component_PrivateType)
#define base_component_PrivateType_FIELDS \
	/** @param ports The ports of the component */ \
	base_component_PortType **ports; \
	/** @param bIsInit Indicate whether component has been already initialized */ \
	OMX_BOOL bIsInit; \
	/** @param sPortTypesParam OpenMAX standard parameter that contains a short description of the available ports */ \
	OMX_PORT_PARAM_TYPE sPortTypesParam; \
	/** @param bufferMgmtThread The main component thread that receives input buffers and sends it to the output device */ \
	pthread_t bufferMgmtThread; \
	/** @param bufferMgmtThreadID The main component thread ID */ \
	OMX_S32 bufferMgmtThreadID; \
	/** @param executingMutex The mutex that synchronizes the start of the main thread and the state transition of the component to Executing */ \
	pthread_mutex_t executingMutex; \
	/** @param executingCondition The executing mutex condition */ \
	pthread_cond_t executingCondition; \
	/** @param bExit_buffer_thread boolean flag for the exit condition from the main component thread */ \
	OMX_BOOL bExit_buffer_thread; \
	/** @param exit_mutex mutex for the exit condition from the main component thread */ \
	pthread_mutex_t exit_mutex; \
	/** @param The exit mutex condition */ \
	pthread_cond_t exit_condition; \
	/** @param cmd_mutex Mutex uses for message access synchronization */ \
	pthread_mutex_t cmd_mutex; \
	/** @param pCmdSem Semaphore for message handling completion signaling */ \
	tsem_t* pCmdSem; \
	/** @param pCmdQueue Component command queue */ \
	queue_t *pCmdQueue; \
	/** @param nGroupPriority Resource management field: component priority (common to a group of components) */ \
	OMX_U32 nGroupPriority; \
	/** @param nGroupID ID of a group of components that share the same logical chain */ \
	OMX_U32 nGroupID; \
	/** @param pMark This field holds the private data associated with a mark request, if any */ \
	OMX_MARKTYPE *pMark; \
	/** @param bCmdUnderProcess Boolean variable indicates whether command under process */ \
	OMX_BOOL bCmdUnderProcess; \
	/** @param bWaitingForCmdToFinish Boolean variable indicates whether cmdSem is waiting for command to finish */ \
	OMX_BOOL bWaitingForCmdToFinish; \
	/** @param inbuffer Counter of input buffers. Only for debug */ \
	OMX_U32 inbuffer; \
	/** @param inbuffercb Counter of input buffers. Only for debug */ \
	OMX_U32 inbuffercb; \
	/** @param outbuffer Counter of output buffers. Only for debug */ \
	OMX_U32 outbuffer; \
	/** @param outbuffercb Counter of output buffers. Only for debug */ \
	OMX_U32 outbuffercb; \
	/** @param Init Counter of output buffers. Only for debug */ \
	OMX_ERRORTYPE (*Init)(stComponentType* stComponent); \
	/** @param Deinit Counter of output buffers. Only for debug */ \
	OMX_ERRORTYPE (*Deinit)(stComponentType* stComponent); \
	/** @param DoStateSet Counter of output buffers. Only for debug */ \
	OMX_ERRORTYPE (*DoStateSet)(stComponentType*, OMX_U32); \
	/** @param AllocateTunnelBuffers Counter of output buffers. Only for debug */ \
	OMX_ERRORTYPE (*AllocateTunnelBuffers)(base_component_PortType*, OMX_U32, OMX_U32);	 \
	/** @param FreeTunnelBuffers Counter of output buffers. Only for debug */ \
	OMX_ERRORTYPE (*FreeTunnelBuffers)(base_component_PortType*); \
	/** @param BufferMgmtFunction Counter of output buffers. Only for debug */ \
	void* (*BufferMgmtFunction)(void* param);
ENDCLASS(base_component_PrivateType)

/**
 * FIXME comment
 */
stComponentType* base_component_CreateComponentStruct(void);

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
OMX_ERRORTYPE base_component_Constructor(stComponentType*);

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE base_component_Destructor(stComponentType* stComponent);

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* base_component_BufferMgmtFunction(void* param);

/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 * \param message the message that has been passed to core
 */
OMX_ERRORTYPE base_component_MessageHandler(coreMessage_t* message);

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For base_component component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE base_component_Init(stComponentType* stComponent);

/** FIXME comment this
 * 
 */
OMX_ERRORTYPE base_component_Deinit(stComponentType* stComponent);

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param destinationState the requested target state.
 */
OMX_ERRORTYPE base_component_DoStateSet(stComponentType*, OMX_U32);

/** Disables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be disabled
	* @param portIndex the ID of the port to be disabled
	*/
OMX_ERRORTYPE base_component_DisablePort(stComponentType* stComponent, OMX_U32 portIndex);

/** Enables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be enabled
	* @param portIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE base_component_EnablePort(stComponentType* stComponent, OMX_U32 portIndex);

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE base_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex);

/** Allocate buffers in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Allocate the MAX number of buffers for that port, with the specified size.
	* @param base_component_Port the port of the alsa component that must be tunneled
	* @param portIndex index of the port to be tunneled
	* @param bufferSize Size of the buffers to be allocated
	*/
OMX_ERRORTYPE base_component_AllocateTunnelBuffers(base_component_PortType* base_component_Port, OMX_U32 portIndex, OMX_U32 bufferSize);

/** Free buffer in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Free the MAX number of buffers for that port, with the specified size.
	* @param base_component_Port the port of the alsa component on which tunnel buffers must be released
	*/
OMX_ERRORTYPE base_component_FreeTunnelBuffers(base_component_PortType* base_component_Port);

/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param pCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
 */
OMX_ERRORTYPE base_component_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData);

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void base_component_Panic();

/** Component entry points declarations */
OMX_ERRORTYPE base_component_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING pComponentName,
	OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
	OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
	OMX_OUT OMX_UUIDTYPE* pComponentUUID);

OMX_ERRORTYPE base_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE base_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE base_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE base_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE base_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType);

OMX_ERRORTYPE base_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState);

OMX_ERRORTYPE base_component_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer);

OMX_ERRORTYPE base_component_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE base_component_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE base_component_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData);

OMX_ERRORTYPE base_component_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent);
	  
OMX_ERRORTYPE base_component_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE base_component_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE base_component_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);


#endif // _OMX_BASE_COMPONENT_H_
