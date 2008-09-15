/**
  @file src/components/jpeg/omx_jpegenc_component.h

  This component implements an JPEG encoder based on Tom Lane's jpeg library (http://www.ijg.org/files/)

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

  $Date: 2008-09-05 17:23:02 +0530 (Fri, 05 Sep 2008) $
  Revision $Rev: 603 $
  Author $Author: pankaj_sen $

*/

#ifndef _OMX_JPEGENC_COMPONENT_H_
#define _OMX_JPEGENC_COMPONENT_H_

#ifdef HAVE_STDLIB_H
# undef HAVE_STDLIB_H
#endif
#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Image.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>

#ifndef _CDJPEG_H_
#define _CDJPEG_H_
#include "libjpeg-6c/cdjpeg.h"
#endif

/** Port private definition.
 * Contains component allocated buffer descriptions.
 * Only MAX_BUFFERS buffers can be allocated by the component
 * Buffers have an allocated state: The user has requested a buffer/has not
 */
#define BUFFER_ALLOCATED (1 << 0)
#define MAX_BUFFERS      2
#define DEFAULT_FRAME_WIDTH   640
#define DEFAULT_FRAME_HEIGHT  480
#define IN_BUFFER_SIZE        4096
#define ENC_OUT_BUFFER_SIZE        DEFAULT_FRAME_WIDTH*DEFAULT_FRAME_HEIGHT /* 640 x 480 x 2bytes x(3/2) +54bytes header*/


#define IMAGE_ENC_BASE_NAME "OMX.st.image_encoder"
#define IMAGE_ENC_JPEG_NAME "OMX.st.image_encoder.jpeg"
#define IMAGE_ENC_JPEG_ROLE "image_encoder.jpeg"

/** Jpeg Decoder component private structure.
 */
DERIVEDCLASS(omx_jpegenc_component_PrivateType, omx_base_filter_PrivateType)
#define omx_jpegenc_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /* the jpeg line buffer */ \
  OMX_U8 **line[3]; \
  struct jpeg_compress_struct cinfo; \
  struct jpeg_error_mgr jerr; \
  struct jpeg_source_mgr jsrc; \
  struct cdjpeg_progress_mgr progress; \
  cjpeg_source_ptr src_mgr; \
  struct jpeg_destination_mgr dest_mgr; \
  /** @param image_coding_type Field that indicate the supported image coding type */ \
  OMX_U32 image_coding_type;   \
  /** @param jpegencReady boolean flag that is true when the audio coded has been initialized */ \
  OMX_BOOL jpegencReady;  \
  /** @param isFirstBuffer Field that the buffer is the first buffer */ \
  OMX_S32 isFirstBuffer;\
  /** @param isNewBuffer Field that indicate a new buffer has arrived*/ \
  OMX_S32 isNewBuffer; \
  /** @param semaphore for jpeg encoder access syncrhonization */\
  tsem_t* jpegencSyncSem; \
  tsem_t* jpegencSyncSem1; \
  OMX_BUFFERHEADERTYPE* pInBuffer; \
  OMX_COMPONENTTYPE* hMarkTargetComponent; \
  OMX_PTR            pMarkData; \
  OMX_U32            nFlags;
ENDCLASS(omx_jpegenc_component_PrivateType)


/* Component private entry points declaration */
OMX_ERRORTYPE omx_jpegenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_jpegenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegenc_encoder_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
void* omx_jpegenc_component_BufferMgmtFunction(void* param);

void omx_jpegenc_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_jpegenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_jpegenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

#endif
