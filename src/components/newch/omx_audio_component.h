/**
	@file src/components/audio_component/omx_audio_component.h
	
	OpenMax audio_component component. This component does not perform any multimedia
	processing.	It is used as a audio_component for new components development.
	
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
	
	2006/02/08:  OpenMAX audio_component component version 0.1

*/

#include <pthread.h>

#include "tsemaphore.h"
#include "queue.h"


 */

typedef struct audio_component_PortType{
	base_component_PortType basePort;
	/// @param sAudioParam Domain specific (audio) OpenMAX port parameter
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam;
}audio_component_PortType;

/**
 * Components Private Structure
 */
typedef struct audio_component_PrivateType{
	base_component_PrivateType basePrivate;

	OMX_ERRORTYPE (*Init)(stComponentType* stComponent);
	OMX_ERRORTYPE (*Deinit)(stComponentType* stComponent);
	OMX_ERRORTYPE (*DoStateSet)(stComponentType*, OMX_U32);
	void* (*BufferMgmtFunction)(void* param);
} audio_component_PrivateType;

/** 
 * This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the audio_component component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated audio_component_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE audio_component_Constructor(stComponentType*);

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE audio_component_Destructor(stComponentType* stComponent);

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For audio_component component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE audio_component_Init(stComponentType* stComponent);


/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void audio_component_Panic();

/** Component entry points declarations */

OMX_ERRORTYPE audio_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE audio_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE audio_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE audio_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE audio_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType);

OMX_ERRORTYPE audio_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState);

OMX_ERRORTYPE audio_component_UseBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes,
	OMX_IN OMX_U8* pBuffer);

OMX_ERRORTYPE audio_component_AllocateBuffer(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_IN OMX_U32 nPortIndex,
	OMX_IN OMX_PTR pAppPrivate,
	OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE audio_component_FreeBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_U32 nPortIndex,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE audio_component_SendCommand(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_COMMANDTYPE Cmd,
	OMX_IN  OMX_U32 nParam,
	OMX_IN  OMX_PTR pCmdData);

OMX_ERRORTYPE audio_component_ComponentDeInit(
	OMX_IN  OMX_HANDLETYPE hComponent);
	  
OMX_ERRORTYPE audio_component_EmptyThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE audio_component_FillThisBuffer(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE audio_component_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);


