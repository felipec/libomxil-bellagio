/**
 * @file src/base/omx_base_source.h
 * 
 * OpenMax base source component. This component is an audio sink that uses ALSA library.
 * 
 * OpenMax base source component. This component does not perform any multimedia
 * processing. It derives from base component and contains a single port. It can be used 
 * as base class for sink and source components.
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
 * 2006/05/11:  base source component version 0.2
 *
 */


#ifndef _OMX_BASE_SOURCE_COMPONENT_H_
#define _OMX_BASE_SOURCE_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_base_component.h>
#include <stdlib.h>


#define OMX_BASE_SOURCE_OUTPUTPORT_INDEX 0
#define OMX_BASE_SOURCE_ALLPORT_INDEX -1


/** Oneport component private structure.
 */
DERIVEDCLASS(omx_base_source_PrivateType, base_component_PrivateType)
#define omx_base_source_PrivateType_FIELDS base_component_PrivateType_FIELDS \
	/** @param BufferMgmtCallback function pointer for algorithm callback */ \
	void (*BufferMgmtCallback)(base_component_PrivateType* base_component_Private, OMX_BUFFERHEADERTYPE* outputbuffer);
ENDCLASS(omx_base_source_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_base_source_Constructor(OMX_COMPONENTTYPE *openmaxStandComp);

/** This is the central function for component processing. It
 * is executed in a separate thread, is synchronized with 
 * semaphores at each port, those are released each time a new buffer
 * is available on the given port.
 */
void* omx_base_source_BufferMgmtFunction(void* param);
/** Flushes all the buffers under processing by the given port. 
 * This function si called due to a state change of the component, typically
 * @param stComponent the component which owns the port to be flushed
 * @param portIndex the ID of the port to be flushed
 */
OMX_ERRORTYPE omx_base_source_FlushPort(base_component_PrivateType* base_component_Private, OMX_U32 portIndex);

#endif //_OMX_BASE_SOURCE_COMPONENT_H_
