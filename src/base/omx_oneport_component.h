/**
 * @file src/base/omx_oneport_component.h
 * 
 * OpenMax alsa sink component. This component is an audio sink that uses ALSA library.
 * 
 * OpenMax one port component. This component does not perform any multimedia
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
 * 2006/05/11:  one port component version 0.2
 *
 */


#ifndef _OMX_ONEPORT_COMPONENT_H_
#define _OMX_ONEPORT_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_base_component.h>

#define OMX_ONEPORT_INPUTPORT_INDEX 0
#define OMX_ONEPORT_ALLPORT_INDEX -1


/** Oneport component private structure.
 */
DERIVEDCLASS(omx_oneport_component_PrivateType, base_component_PrivateType)
#define omx_oneport_component_PrivateType_FIELDS base_component_PrivateType_FIELDS \
	/** @param BufferMgmtCallback function pointer for algorithm callback */ \
	void (*BufferMgmtCallback)(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer);
ENDCLASS(omx_oneport_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_oneport_component_Constructor(stComponentType*);

void* omx_oneport_component_BufferMgmtFunction(void* param);
OMX_ERRORTYPE omx_oneport_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex);

#endif //_OMX_ONEPORT_COMPONENT_H_
