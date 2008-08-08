/**
  @file src/components/fbdev/omx_fbdev_sink_component.h
  
  OpenMAX FBDEV sink component. This component is a video sink that copies
  data to a >inux framebuffer device.

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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#ifndef _OMX_FBDEV_SINK_COMPONENT_H_
#define _OMX_FBDEV_SINK_COMPONENT_H_

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

#include <omx_base_video_port.h>
#include <omx_base_clock_port.h>
#include <omx_base_sink.h>
#include <linux/fb.h>

/**  Filename of devnode for framebuffer device
  *  Should somehow be passed from client
  */
#define FBDEV_FILENAME  "/dev/fb0" 

/** FBDEV sink port component port structure.
  */
DERIVEDCLASS(omx_fbdev_sink_component_PortType, omx_base_video_PortType)
#define omx_fbdev_sink_component_PortType_FIELDS omx_base_video_PortType_FIELDS \
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
ENDCLASS(omx_fbdev_sink_component_PortType)

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
DERIVEDCLASS(omx_fbdev_sink_component_PrivateType, omx_base_sink_PrivateType)
#define omx_fbdev_sink_component_PrivateType_FIELDS omx_base_sink_PrivateType_FIELDS \
  int                          fd; \
  struct                       fb_var_screeninfo vscr_info; \
  struct                       fb_fix_screeninfo fscr_info; \
  unsigned char                *scr_ptr; \
  OMX_COLOR_FORMATTYPE         fbpxlfmt; \
  OMX_U32                      fbwidth; \
  OMX_U32                      fbheight; \
  OMX_U32                      fbbpp; \
  OMX_S32                      fbstride; \
  OMX_S32                      xScale; \
  OMX_TIME_CLOCKSTATE          eState; \
  OMX_U32                      product;\
  OMX_BOOL                     frameDropFlag;\
  int                          dropFrameCount;
ENDCLASS(omx_fbdev_sink_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_fbdev_sink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_fbdev_sink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_fbdev_sink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_fbdev_sink_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_fbdev_sink_component_MessageHandler(OMX_COMPONENTTYPE* , internalRequestMessageType*);

void omx_fbdev_sink_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* pInputBuffer);

OMX_ERRORTYPE omx_fbdev_sink_component_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer);

/* to handle the communication at the clock port */
OMX_BOOL omx_fbdev_sink_component_ClockPortHandleFunction(
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private,
  OMX_BUFFERHEADERTYPE* inputbuffer);

OMX_ERRORTYPE omx_fbdev_sink_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_fbdev_sink_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_fbdev_sink_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_fbdev_sink_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

/** function prototypes of some internal functions */

/** finds OpenMAX standard pixel format from screen info */
OMX_COLOR_FORMATTYPE find_omx_pxlfmt(struct fb_var_screeninfo *vscr_info);

/** finds FFmpeg supported pixel format from input OpenMAX standard pixel format */
enum PixelFormat find_ffmpeg_pxlfmt(OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** finds video stride  from input dimension and color format */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** image copy function */
void omx_img_copy(OMX_U8* src_ptr, OMX_S32 src_stride, OMX_U32 src_width, OMX_U32 src_height, 
                  OMX_S32 src_offset_x, OMX_S32 src_offset_y,
                  OMX_U8* dest_ptr, OMX_S32 dest_stride, OMX_U32 dest_width,  OMX_U32 dest_height, 
                  OMX_S32 dest_offset_x, OMX_S32 dest_offset_y, 
                  OMX_S32 cpy_width, OMX_U32 cpy_height, OMX_COLOR_FORMATTYPE colorformat,OMX_COLOR_FORMATTYPE fbpxlfmt);

/** Returns a time value in milliseconds based on a clock starting at
 *  some arbitrary base. Given a call to GetTime that returns a value
 *  of n a subsequent call to GetTime made m milliseconds later should 
 *  return a value of (approximately) (n+m). This method is used, for
 *  instance, to compute the duration of call. */
long GetTime();

OMX_ERRORTYPE omx_fbdev_sink_component_port_FlushProcessingBuffers(omx_base_PortType *openmaxStandPort);

#endif
