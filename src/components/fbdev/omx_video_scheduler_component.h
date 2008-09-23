/**
  @file src/components/ffmpeg/omx_ffmpeg_video_scheduler_component.h

  This component implements a video scheduler using the FFmpeg
  software library.

  Originally developed by Peter Littlefield
  Copyright (C) 2007-2008  STMicroelectronics and Agere Systems

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

  $Date: 2008-06-27 15:30:23 +0530 (Fri, 27 Jun 2008) $
  Revision $Rev: 554 $
  Author $Author: pankaj_sen $
*/

#ifndef _OMX_FFMPEG_VIDEO_SCHEDULER_H_
#define _OMX_FFMPEG_VIDEO_SCHEDULER_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>
#include <omx_base_video_port.h>
#include <omx_base_clock_port.h>

/** ffmpeg color converter component private structure.
  * @param xScale the scale of the media clock
  * @param eState the state of the media clock
  * @param frameDropFlag the flag active on scale change indicates that frames are to be dropped 
  * @param dropFrameCount counts the number of frames dropped 
  */
DERIVEDCLASS(omx_video_scheduler_component_PrivateType, omx_base_filter_PrivateType)
#define omx_video_scheduler_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  OMX_S32                      xScale; \
  OMX_TIME_CLOCKSTATE          eState; \
  OMX_BOOL                     frameDropFlag;\
  int                          dropFrameCount;
ENDCLASS(omx_video_scheduler_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_video_scheduler_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName);
OMX_ERRORTYPE omx_fbdev_sink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_fbdev_sink_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);

OMX_ERRORTYPE omx_video_scheduler_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_video_scheduler_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_video_scheduler_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_video_scheduler_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

/* to handle the communication at the clock port */
OMX_BOOL omx_video_scheduler_component_ClockPortHandleFunction(
  omx_video_scheduler_component_PrivateType* omx_video_scheduler_component_Private,
  OMX_BUFFERHEADERTYPE* inputbuffer);

OMX_ERRORTYPE omx_video_scheduler_component_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer);

#endif
