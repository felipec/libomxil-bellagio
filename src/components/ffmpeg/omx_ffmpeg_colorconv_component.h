/**
  @file src/components/ffmpeg/omx_ffmpeg_colorconv_component.h

  This component implements a color converter using the ffmpeg
  software library.

  Originally developed by Peter Littlefield
	Copyright (C) 2007  STMicroelectronics and Agere Systems

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

#ifndef _OMX_FFMPEG_COLORCONV_COMPONENT_H_
#define _OMX_FFMPEG_COLORCONV_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avutil.h>
#include <ffmpeg/swscale.h>

#define COLOR_CONV_BASE_NAME "OMX.st.video_colorconv.ffmpeg"
#define COLOR_CONV_FFMPEG_NAME "OMX.st.video_colorconv.ffmpeg"
#define COLOR_CONV_FFMPEG_ROLE "video_colorconv.ffmpeg"

#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))

/** ffmpeg color converter component port structure.
  */
DERIVEDCLASS(omx_ffmpeg_colorconv_component_PortType, omx_base_PortType)
#define omx_ffmpeg_colorconv_component_PortType_FIELDS omx_base_PortType_FIELDS \
  /** @param sVideoParam Domain specific (IVCommon) OpenMAX port parameter */ \
  OMX_VIDEO_PARAM_PORTFORMATTYPE sVideoParam; \
  /** @param omxConfigCrop Crop rectangle of image */ \
  OMX_CONFIG_RECTTYPE omxConfigCrop; \
  /** @param omxConfigRotate Set rotation angle of image */ \
  OMX_CONFIG_ROTATIONTYPE omxConfigRotate; \
  /** @param omxConfigMirror Set mirroring of image */ \
  OMX_CONFIG_MIRRORTYPE omxConfigMirror; \
  /** @param omxConfigScale Set scale factors */ \
  OMX_CONFIG_SCALEFACTORTYPE omxConfigScale; \
  /** @param omxConfigOutputPosition Top-Left offset from intermediate buffer to output buffer */ \
  OMX_CONFIG_POINTTYPE omxConfigOutputPosition; \
  /** @param ffmpeg_pixelformat The ffmpeg enum for pixel format */ \
  enum PixelFormat ffmpeg_pxlfmt;
ENDCLASS(omx_ffmpeg_colorconv_component_PortType)

/** ffmpeg color converter component private structure.
  */
DERIVEDCLASS(omx_ffmpeg_colorconv_component_PrivateType, omx_base_filter_PrivateType)
#define omx_ffmpeg_colorconv_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param in_buffer Buffer for the input data */ \
  unsigned char *in_buffer; \
  /** @param conv_buffer Buffer to hold converted data */ \
  unsigned char *conv_buffer; \
  /** @param in_frame The ffmpeg AVFrame structure for the input buffer */ \
  AVFrame *in_frame; \
  /** @param conv_frame the ffmpeg AVFrame structure for the conversion output */ \
  AVFrame *conv_frame; \
  /** @param in_alloc_size Allocated size of the input buffer */ \
  unsigned int in_alloc_size; \
  /** @param conv_alloc_size Allocated size of the conversion buffer */ \
  unsigned int conv_alloc_size; 
ENDCLASS(omx_ffmpeg_colorconv_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_ffmpeg_colorconv_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName);
OMX_ERRORTYPE omx_ffmpeg_colorconv_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_ffmpeg_colorconv_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_ffmpeg_colorconv_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_video_colorconv_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message);

void omx_ffmpeg_colorconv_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_ffmpeg_colorconv_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_ffmpeg_colorconv_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_ffmpeg_colorconv_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_ffmpeg_colorconv_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

/** finds pixel format */
enum PixelFormat find_ffmpeg_pxlfmt(OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** stride calculation */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** image copy function */
void omx_img_copy(OMX_U8* src_ptr, OMX_S32 src_stride, OMX_U32 src_width, OMX_U32 src_height, 
                  OMX_S32 src_offset_x, OMX_S32 src_offset_y,
					        OMX_U8* dest_ptr, OMX_S32 dest_stride, OMX_U32 dest_width,  OMX_U32 dest_height, 
                  OMX_S32 dest_offset_x, OMX_S32 dest_offset_y, 
					        OMX_S32 cpy_width, OMX_U32 cpy_height, OMX_COLOR_FORMATTYPE colorformat );


OMX_ERRORTYPE omx_video_colorconv_UseEGLImage (
        OMX_HANDLETYPE hComponent,
        OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
        void* eglImage);

#endif
