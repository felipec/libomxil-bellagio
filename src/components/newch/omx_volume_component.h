/**
 * @file src/components/newch/omx_volume_component.h
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
 */


#ifndef _OMX_VOLUME_COMPONENT_H_
#define _OMX_VOLUME_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_twoport_component.h>



/** Twoport component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_volume_component_PrivateType, omx_twoport_component_PrivateType)
#define omx_volume_component_PrivateType_FIELDS omx_twoport_component_PrivateType_FIELDS \
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

#endif //_OMX_VOLUME_COMPONENT_H_
