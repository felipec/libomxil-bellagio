/**
 * @file src/components/audio_effects/omx_volume_component.h
 * 
 * OpenMax volume control component. This component implements a filter that 
 * controls the volume level of the audio PCM stream.
 * 
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Ukri NIEMIMUUKKO, Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI
 * 
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 * 2006/05/11:  audio volume control component version 0.2
 *
 */


#ifndef _OMX_VOLUME_COMPONENT_H_
#define _OMX_VOLUME_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <base_filter_component.h>


/** Volume component port structure.
 */
DERIVEDCLASS(omx_volume_component_PortType, base_component_PortType)
#define omx_volume_component_PortType_FIELDS base_component_PortType_FIELDS \
	/** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; 
ENDCLASS(omx_volume_component_PortType)

/** Twoport component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_volume_component_PrivateType, base_filter_component_PrivateType)
#define omx_volume_component_PrivateType_FIELDS base_filter_component_PrivateType_FIELDS \
	/** @param gain the volume gain value */ \
	float gain;
ENDCLASS(omx_volume_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_volume_component_Constructor(stComponentType*);

void omx_volume_component_BufferMgmtCallback(
	stComponentType* stComponent,
	OMX_BUFFERHEADERTYPE* inputbuffer,
	OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_volume_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_volume_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_volume_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_volume_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_volume_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef);

#endif //_OMX_VOLUME_COMPONENT_H_
