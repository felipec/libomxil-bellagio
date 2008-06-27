/**
  @file src/components/clocksrc/omx_clocksrc_component.h

  OpenMAX clocksrc_component component. This component does not perform any multimedia
  processing.  It is provides the media and the reference clock for all the clients connected to it.

  Copyright (C) 2008  STMicroelectronics
  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

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

  $Date$
  Revision $Rev$
  Author $Author$

*/

#ifndef _OMX_CLOCKSRC_COMPONENT_H_
#define _OMX_CLOCKSRC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <pthread.h>
#include <omx_base_source.h>
#include <string.h>
#include <sys/time.h>

/** Maximum number of clock ports */
#define MAX_CLOCK_PORTS                          8


/** Clock component private structure.
 * see the define above
 * @param sClockState This structure holds the state of the clock 
 * @param startTimeSem the semaphore that coordinates the arrival of start times from all clients
 * @param clockEventSem the semaphore that coordinates clock event received from the client
 * @param clockEventCompleteSem the semaphore that coordinates clock event sent to the client
 * @param WallTimeBase the wall time at which the clock was started
 * @param MediaTimeBase the Media time at which the clock was started
 * @param eUpdateType indicates the type of update received from the clock src component
 * @param sMinStartTime keeps the minimum starttime of the clients
 * @param sConfigScale Representing the current media time scale factor
 */
DERIVEDCLASS(omx_clocksrc_component_PrivateType, omx_base_source_PrivateType)
#define omx_clocksrc_component_PrivateType_FIELDS omx_base_source_PrivateType_FIELDS \
  OMX_TIME_CONFIG_CLOCKSTATETYPE      sClockState; \
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  sRefClock; \
  tsem_t*                             startTimeSem; \
  tsem_t*                             clockEventSem; \
  tsem_t*                             clockEventCompleteSem; \
  OMX_TICKS                           WallTimeBase; \
  OMX_TICKS                           MediaTimeBase; \
  OMX_TIME_UPDATETYPE                 eUpdateType; \
  OMX_TIME_CONFIG_TIMESTAMPTYPE       sMinStartTime; \
  OMX_TIME_CONFIG_SCALETYPE           sConfigScale; 
ENDCLASS(omx_clocksrc_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_clocksrc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_clocksrc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_clocksrc_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer);

OMX_ERRORTYPE omx_clocksrc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_clocksrc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_clocksrc_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_clocksrc_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

void* omx_clocksrc_BufferMgmtFunction (void* param);

OMX_ERRORTYPE omx_clocksrc_component_SendCommand(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_COMMANDTYPE Cmd,
  OMX_IN  OMX_U32 nParam,
  OMX_IN  OMX_PTR pCmdData);

OMX_ERRORTYPE clocksrc_port_FlushProcessingBuffers(omx_base_PortType *openmaxStandPort);

#endif
