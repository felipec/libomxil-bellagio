/**
  @file src/components/video_src/omx_videosrc_component.h

  OpenMAX video source component. This component is a video source component
  that captures video from the video camera.

  Copyright (C) 2007  STMicroelectronics and Nokia

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

#ifndef _OMX_VIDEOSRC_COMPONENT_H_
#define _OMX_VIDEOSRC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Video.h>
#include <pthread.h>
#include <omx_base_source.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

/** Maximum number of base_component component instances */
#define MAX_NUM_OF_videosrc_component_INSTANCES 1

#define VIDEO_DEV_NAME "/dev/video"

/** Video Source component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_videosrc_component_PrivateType, omx_base_source_PrivateType)
#define omx_videosrc_component_PrivateType_FIELDS omx_base_source_PrivateType_FIELDS \
  /** @param semaphore for video synchronization */\
  tsem_t* videoSyncSem; \
  /** @param videoReady boolean flag that is true when the video format has been initialized */ \
  OMX_BOOL videoReady;  \
  /** @param bIsEOSSent boolean flag that is true when EOS event is sent to the client */ \
  OMX_BOOL bIsEOSSent;  \
  /** @param deviceHandle handle to the video capture device */ \
  OMX_S32 deviceHandle; \
  /** @param capability capability of the video capture device */ \
  struct video_capability capability; \
  /** @param captureWindow capture window structor */ \
  struct video_window captureWindow; \
  /** @param imageProperties picture properties*/ \
  struct video_picture imageProperties; \
  /** @param memoryBuffer capture memory space */ \
  struct video_mbuf memoryBuffer; \
  /** @param memoryMap memory mapped area */ \
  char* memoryMap; \
  /** @param mmaps memory area structore to retrieve each frame */ \
  struct video_mmap* mmaps; \
  /** @param iFrameIndex Frame Index */ \
  OMX_U32 iFrameIndex; \
  /** @param iFrameSize output frame size */ \
  OMX_U32 iFrameSize; \
  /** @param bOutBufferMemoryMapped boolean flag. True,if output buffer is memory mapped to avoid memcopy*/ \
  OMX_BOOL bOutBufferMemoryMapped;
ENDCLASS(omx_videosrc_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_videosrc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_videosrc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_videosrc_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
OMX_ERRORTYPE omx_videosrc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_videosrc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_videosrc_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_videosrc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_videosrc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE videosrc_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes);

OMX_ERRORTYPE videosrc_port_FreeBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_U32 nPortIndex,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE videosrc_port_AllocateTunnelBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_IN OMX_U32 nPortIndex,
  OMX_IN OMX_U32 nSizeBytes);

OMX_ERRORTYPE videosrc_port_FreeTunnelBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_U32 nPortIndex);

#endif

