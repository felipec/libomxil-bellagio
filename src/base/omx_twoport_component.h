/**
 * @file src/base/omx_twoport_component.h
 * 
 * OpenMax two ports component. This component does not perform any multimedia
 * processing. It derives from base component and contains two ports. It can be used 
 * as base class for codec and filter components.
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
 * 2006/05/11:  two ports component version 0.2
 *
 */


#ifndef _OMX_TWOPORT_COMPONENT_H_
#define _OMX_TWOPORT_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <omx_base_component.h>

#define OMX_TWOPORT_INPUTPORT_INDEX 0
#define OMX_TWOPORT_OUTPUTPORT_INDEX 1
#define OMX_TWOPORT_ALLPORT_INDEX -1


/** Twoport component private structure.
 */
DERIVEDCLASS(omx_twoport_component_PrivateType, base_component_PrivateType)
#define omx_twoport_component_PrivateType_FIELDS base_component_PrivateType_FIELDS \
	/** @param BufferMgmtCallback function pointer for algorithm callback */ \
	void (*BufferMgmtCallback)(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer);
ENDCLASS(omx_twoport_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_twoport_component_Constructor(stComponentType*);

void* omx_twoport_component_BufferMgmtFunction(void* param);
OMX_ERRORTYPE omx_twoport_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex);

#endif //_OMX_TWOPORT_COMPONENT_H_
