/**
	@file src/components/ffmpeg/omx_ffmpeg_mp3dec.h
	
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

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <pthread.h>

#include "tsemaphore.h"
#include "queue.h"

/* Specific include files */
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

/** Maximum number of buffers for the input port */
#define MAX_BUFFERS      4

/** Default size of the internal input buffer */
#define INTERNAL_IN_BUFFER_SIZE  8  * 1024
/** Default size of the internal output buffer */
#define INTERNAL_OUT_BUFFER_SIZE 16 * 1024

/** Maximum number of ffmpeg mp3 decoder component instances */
#define MAX_NUM_OF_MP3DEC_INSTANCES 10
/**
 * The structure for port Type.
 */
typedef struct ffmpeg_mp3dec_PortType{
	/// @param pBuffer An array of pointers to buffer headers. 
	OMX_BUFFERHEADERTYPE *pBuffer[MAX_BUFFERS];
	/// @param nBufferState The State of the Buffer whether assigned or allocated
	OMX_U32 nBufferState[MAX_BUFFERS];
	/// @param nNumAssignedBuffers Number of buffer assigned on each port
	OMX_U32 nNumAssignedBuffers;
	/** buffer queue descriptors */
	/// @param pBufferQueue queue for buffer to be processed by the port
	queue_t* pBufferQueue;
	/** Data flow control semaphore.
	 * Buffer processing thread will wait until
	 * buffers are available on the port */
	/// @param pBufferSem Semaphore for buffer queue access synchronization
	tsem_t* pBufferSem;
	/// @param pFullAllocationSem Semaphore used to hold the state transition until the all the buffers needed have been allocated
	tsem_t* pFullAllocationSem;
	/// @param transientState A state transition between Loaded to Idle or Ilde to Loaded 
	OMX_STATETYPE eTransientState;
	/// @param pFlushSem Semaphore that locks the execution until the buffers have been flushed, if needed
	tsem_t* pFlushSem;
	/// @param bWaitingFlushSem  Boolean variables indicate whether flush sem is down
	OMX_BOOL bWaitingFlushSem;
	/// @param bBufferUnderProcess  Boolean variables indicate whether the port is processing any buffer
	OMX_BOOL bBufferUnderProcess;
	/// @param mutex Mutex used for access synchronization to the processing thread
	pthread_mutex_t mutex;
	
	/// @param sPortParam General OpenMAX port parameter
	OMX_PARAM_PORTDEFINITIONTYPE sPortParam;
	/// @param sAudioParam Domain specific (audio) OpenMAX port parameter
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam;
	/// @param hTunneledComponent Handle to the tunnelled component
	OMX_HANDLETYPE hTunneledComponent;
	/// @param nTunneledPort Tunnelled port number
	OMX_U32 nTunneledPort;
	/// @param nTunnelFlags Tunnelling flag
	OMX_U32 nTunnelFlags;
	/// @param eBufferSupplier the type of supplier in case of tunneling
	OMX_BUFFERSUPPLIERTYPE eBufferSupplier;
	/// @param nNumTunnelBuffer Number of buffer to be tunnelled
	OMX_U32 nNumTunnelBuffer;
	/// @param bIsPortFlushed Boolean variables indicate whether the port is flushed
	OMX_BOOL bIsPortFlushed;

}ffmpeg_mp3dec_PortType;

/**
 * Components Private Structure
 */

typedef struct ffmpeg_mp3dec_PrivateType{
	/// @param inputPort Input port of the component
	ffmpeg_mp3dec_PortType inputPort;
	/// @param outputPort Output port of the component
	ffmpeg_mp3dec_PortType outputPort;

	/// @param bIsInit Indicate whether component has been already initialized
	OMX_BOOL bIsInit;
	/// @param sPortTypesParam OpenMAX standard parameter that contains a short description of the available ports
	OMX_PORT_PARAM_TYPE sPortTypesParam;
	/* buffer management thread */
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
	/// @param exit_condition The exit mutex condition
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

	/// @param inbuffer Counter of input buffers. Only for debug
	OMX_U32 inbuffer;
	/// @param inbuffercb Counter of input buffers. Only for debug
	OMX_U32 inbuffercb;
	/// @param outbuffer Counter of output buffers. Only for debug
	OMX_U32 outbuffer;
	/// @param outbuffercb Counter of output buffers. Only for debug
	OMX_U32 outbuffercb;

	/// @param avCodec pointer to the ffpeg audio decoder
	AVCodec *avCodec;
	/// @param avCodecContext pointer to ffmpeg decoder context 
	AVCodecContext *avCodecContext;
	/// @param avcodecReady boolean flag that is true when the audio coded has been initialized
	OMX_BOOL avcodecReady;

	// audio format section
	OMX_U16 minBufferLength; /// @param minBufferLength Field that stores the minimun allowed size for ffmpeg decoder
} ffmpeg_mp3dec_PrivateType;

/* Component private entry points declaration */
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
OMX_ERRORTYPE ffmpeg_mp3dec_Constructor(stComponentType*);

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_Destructor(stComponentType* stComponent);

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* ffmpeg_mp3dec_BufferMgmtFunction(void* param);

/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 * \param message the message that has been passed to core
 */
OMX_ERRORTYPE ffmpeg_mp3dec_MessageHandler(coreMessage_t* message);

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For ffmpeg_mp3dec component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE ffmpeg_mp3dec_Init(stComponentType* stComponent);

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param stComponent the ST component which state is to be changed
 * \param destinationState the requested target state.
 */
OMX_ERRORTYPE ffmpeg_mp3dec_DoStateSet(stComponentType*, OMX_U32);

/** Disables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be disabled
	* @param portIndex the ID of the port to be disabled
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_DisablePort(stComponentType* stComponent, OMX_U32 portIndex);

/** Enables the specified port. This function is called due to a request by the IL client
	* @param stComponent the component which owns the port to be enabled
	* @param portIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_EnablePort(stComponentType* stComponent, OMX_U32 portIndex);

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_FlushPort(stComponentType* stComponent, OMX_U32 portIndex);

/** Allocate buffers in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Allocate the MAX number of buffers for that port, with the specified size.
	* @param ffmpeg_mp3dec_Port the port of the alsa component that must be tunneled
	* @param portIndex index of the port to be tunneled
	* @param bufferSize Size of the buffers to be allocated
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_allocateTunnelBuffers(ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port, OMX_U32 portIndex, OMX_U32 bufferSize);

/** Free buffer in case of a tunneled port.
	* The operations performed are:
	*  - Free any buffer associated with the list of buffers of the specified port
	*  - Free the MAX number of buffers for that port, with the specified size.
	* @param ffmpeg_mp3dec_Port the port of the alsa component on which tunnel buffers must be released
	*/
OMX_ERRORTYPE ffmpeg_mp3dec_freeTunnelBuffers(ffmpeg_mp3dec_PortType* ffmpeg_mp3dec_Port);

/** It initializates the ffmpeg framework, and opens an ffmpeg mp3 decoder */
void ffmpeg_mp3dec_ffmpegLibInit(ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private);

/** It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder */
void ffmpeg_mp3dec_ffmpegLibDeInit(ffmpeg_mp3dec_PrivateType* ffmpeg_mp3dec_Private);

/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param pCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
 */
OMX_ERRORTYPE ffmpeg_mp3dec_SetCallbacks(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
	OMX_IN  OMX_PTR pAppData);

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void ffmpeg_mp3dec_Panic();

/** Component entry points declarations */
OMX_ERRORTYPE ffmpeg_mp3dec_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING pComponentName,
	OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
	OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
	OMX_OUT OMX_UUIDTYPE* pComponentUUID);

OMX_ERRORTYPE ffmpeg_mp3dec_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE ffmpeg_mp3dec_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE ffmpeg_mp3dec_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE ffmpeg_mp3dec_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE ffmpeg_mp3dec_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType);

OMX_ERRORTYPE ffmpeg_mp3dec_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState);

OMX_ERRORTYPE ffmpeg_mp3dec_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer);

OMX_ERRORTYPE ffmpeg_mp3dec_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE ffmpeg_mp3dec_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE ffmpeg_mp3dec_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData);

OMX_ERRORTYPE ffmpeg_mp3dec_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent);
	  
OMX_ERRORTYPE ffmpeg_mp3dec_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE ffmpeg_mp3dec_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE ffmpeg_mp3dec_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);



