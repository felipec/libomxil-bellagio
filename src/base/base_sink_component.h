/**
 * @file src/base/base_sink_component.h
 * 
 * OpenMax base sink component. This component is an audio sink that uses ALSA library.
 * 
 * OpenMax base sink component. This component does not perform any multimedia
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
 * 2006/05/11:  base sink component version 0.2
 *
 */


#ifndef _OMX_BASE_SINK_COMPONENT_H_
#define _OMX_BASE_SINK_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_base_component.h>
#include <stdlib.h>


#define OMX_BASE_SINK_INPUTPORT_INDEX 0
#define OMX_BASE_SINK_ALLPORT_INDEX -1


/** base sink component private structure.
 */
DERIVEDCLASS(base_sink_component_PrivateType, base_component_PrivateType)
#define base_sink_component_PrivateType_FIELDS base_component_PrivateType_FIELDS \
	/** @param BufferMgmtCallback function pointer for algorithm callback */ \
	void (*BufferMgmtCallback)(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer);
ENDCLASS(base_sink_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE base_sink_component_Constructor(stComponentType*);

/** This is the central function for component processing. It
 * is executed in a separate thread, is synchronized with 
 * semaphores at each port, those are released each time a new buffer
 * is available on the given port.
 */
void* base_sink_component_BufferMgmtFunction(void* param);
/** Flushes all the buffers under processing by the given port. 
 * This function si called due to a state change of the component, typically
 * @param stComponent the component which owns the port to be flushed
 * @param portIndex the ID of the port to be flushed
 */
OMX_ERRORTYPE base_sink_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex);

#endif //_OMX_BASE_SINK_COMPONENT_H_
