/**
  @file src/components/xvideo/omx_xvideo_sink_component.h
  
  OpenMAX X-Video sink component. 

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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

  $Date: 2008-08-08 10:26:06 +0530 (Fri, 08 Aug 2008) $
  Revision $Rev: 581 $
  Author $Author: pankaj_sen $
*/

#ifndef _OMX_XVIDEO_SINK_COMPONENT_H_
#define _OMX_XVIDEO_SINK_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/resource.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XShm.h>

#include <omx_base_video_port.h>
#include <omx_base_sink.h>

/**  Filename of devnode for framebuffer device
  *  Should somehow be passed from client
  */
#define FBDEV_FILENAME  "/dev/fb0" 

/** FBDEV sink port component port structure.
  */
DERIVEDCLASS(omx_xvideo_sink_component_PortType, omx_base_video_PortType)
#define omx_xvideo_sink_component_PortType_FIELDS omx_base_video_PortType_FIELDS \
  /** @param omxConfigCrop Crop rectangle of image */ \
  OMX_CONFIG_RECTTYPE omxConfigCrop; \
  /** @param omxConfigRotate Set rotation angle of image */ \
  OMX_CONFIG_ROTATIONTYPE omxConfigRotate; \
  /** @param omxConfigMirror Set mirroring of image */ \
  OMX_CONFIG_MIRRORTYPE omxConfigMirror; \
  /** @param omxConfigScale Set scale factors */ \
  OMX_CONFIG_SCALEFACTORTYPE omxConfigScale; \
  /** @param omxConfigOutputPosition Top-Left offset from intermediate buffer to output buffer */ \
  OMX_CONFIG_POINTTYPE omxConfigOutputPosition;
ENDCLASS(omx_xvideo_sink_component_PortType)

/** FBDEV sink port component private structure.
  * see the define above
  * @param fd The file descriptor for the framebuffer 
  * @param vscr_info The fb_var_screeninfo structure for the framebuffer 
  * @param fscr_info The fb_fix_screeninfo structure for the framebuffer
  * @param scr_data Pointer to the mmapped memory for the framebuffer 
  * @param fbpxlfmt frame buffer pixel format
  * @param fbwidth frame buffer display width 
  * @param fbheight frame buffer display height 
  * @param fbbpp frame buffer pixel depth
  * @param fbstride frame buffer display stride 
  * @param xScale the scale of the media clock
  * @param eState the state of the media clock
  * @param product frame buffer memory area 
  * @param frameDropFlag the flag active on scale change indicates that frames are to be dropped 
  * @param dropFrameCount counts the number of frames dropped 
  */
DERIVEDCLASS(omx_xvideo_sink_component_PrivateType, omx_base_sink_PrivateType)
#define omx_xvideo_sink_component_PrivateType_FIELDS omx_base_sink_PrivateType_FIELDS \
  OMX_BOOL                     frameDropFlag;\
  int                          dropFrameCount; \
  int xv_port; \
  int screen; \
  int CompletionType; \
  unsigned int ver; \
  unsigned int rel; \
  unsigned int req; \
  unsigned int ev; \
  unsigned int err; \
  unsigned int adapt; \
  Display *dpy; \
  Window window; \
  XSizeHints hint; \
  XSetWindowAttributes xswa; \
  XWindowAttributes attribs; \
  XVisualInfo vinfo; \
  XEvent event; \
  GC gc; \
  XvAdaptorInfo *ai; \
  XvImage *yuv_image; \
  XShmSegmentInfo yuv_shminfo; \
  Atom wmDeleteWindow;
ENDCLASS(omx_xvideo_sink_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_xvideo_sink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_xvideo_sink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_xvideo_sink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_xvideo_sink_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_xvideo_sink_component_MessageHandler(OMX_COMPONENTTYPE* , internalRequestMessageType*);

void omx_xvideo_sink_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* pInputBuffer);

OMX_ERRORTYPE omx_xvideo_sink_component_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer);

/* to handle the communication at the clock port */
OMX_BOOL omx_xvideo_sink_component_ClockPortHandleFunction(
  omx_xvideo_sink_component_PrivateType* omx_xvideo_sink_component_Private,
  OMX_BUFFERHEADERTYPE* inputbuffer);

OMX_ERRORTYPE omx_xvideo_sink_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_xvideo_sink_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_xvideo_sink_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_xvideo_sink_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

/** function prototypes of some internal functions */

OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** Returns a time value in milliseconds based on a clock starting at
 *  some arbitrary base. Given a call to GetTime that returns a value
 *  of n a subsequent call to GetTime made m milliseconds later should 
 *  return a value of (approximately) (n+m). This method is used, for
 *  instance, to compute the duration of call. */
long GetTime();

#endif
