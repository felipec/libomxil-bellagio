/**
	@file src/base/omx_base_filter.h
	
	OpenMax Base Filter component. This component does not perform any multimedia
	processing. It derives from base component and contains two ports. It can be used 
	as base class for codec and filter components.
	
	Copyright (C) 2007  STMicroelectronics and Nokia

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
	
	$Date: 2007-03-12 07:45:54 +0100 (Mon, 12 Mar 2007) $
	Revision $Rev: 703 $
	Author $Author: pankaj_sen $

*/


#ifndef __OMX_BASE_FILTER_H__
#define __OMX_BASE_FILTER_H__

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <omx_base_component.h>

#define OMX_BASE_FILTER_INPUTPORT_INDEX 0
#define OMX_BASE_FILTER_OUTPUTPORT_INDEX 1
#define OMX_BASE_FILTER_ALLPORT_INDEX -1
//#define MIN_PAYLOAD_ALLOWED 10

/** Base Filter component private structure.
 */
DERIVEDCLASS(omx_base_filter_PrivateType, omx_base_component_PrivateType)
#define omx_base_filter_PrivateType_FIELDS omx_base_component_PrivateType_FIELDS \
  /** @param pPendingOutputBuffer pending Output Buffer pointer */ \
  OMX_BUFFERHEADERTYPE* pPendingOutputBuffer; \
  /** @param BufferMgmtCallback function pointer for algorithm callback */ \
  void (*BufferMgmtCallback)(OMX_COMPONENTTYPE* openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer);
ENDCLASS(omx_base_filter_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_base_filter_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);

/** This is the central function for component processing. It
 * is executed in a separate thread, is synchronized with 
 * semaphores at each port, those are released each time a new buffer
 * is available on the given port.
 */
void* omx_base_filter_BufferMgmtFunction(void* param);
/** Flushes all the buffers under processing by the given port. 
 * This function si called due to a state change of the component, typically
 * @param stComponent the component which owns the port to be flushed
 * @param portIndex the ID of the port to be flushed
 */
OMX_ERRORTYPE omx_base_filter_FlushProcessingBuffers(OMX_COMPONENTTYPE *openmaxStandComp,OMX_U32 portIndex);

#endif //_OMX_BASE_FILTER_COMPONENT_H_
