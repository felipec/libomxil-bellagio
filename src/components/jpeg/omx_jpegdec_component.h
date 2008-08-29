/**
  @file src/components/jpeg/omx_jpegdec_component.h

  This component implements an JPEG decoder based on Tom Lane's jpeg library (http://www.ijg.org/files/)

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

  $Date$
  Revision $Rev$
  Author $Author$

*/

#ifndef _OMX_JPEGDEC_COMPONENT_H_
#define _OMX_JPEGDEC_COMPONENT_H_

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

#include "cdjpeg.h"

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
#define OUT_BUFFER_SIZE        DEFAULT_FRAME_WIDTH*DEFAULT_FRAME_HEIGHT*3 +54 /* 640 x 480 x 2bytes x(3/2) +54bytes header*/


#define IMAGE_DEC_BASE_NAME "OMX.st.image_decoder"
#define IMAGE_DEC_JPEG_NAME "OMX.st.image_decoder.jpeg"
#define IMAGE_DEC_JPEG_ROLE "image_decoder.jpeg"

/** Jpeg Decoder component private structure.
 */
DERIVEDCLASS(omx_jpegdec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_jpegdec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /* the jpeg line buffer */ \
  OMX_U8 **line[3]; \
  struct jpeg_decompress_struct cinfo; \
  struct jpeg_error_mgr jerr; \
  struct jpeg_source_mgr jsrc; \
  djpeg_dest_ptr dest_mgr; \
  /** @param image_coding_type Field that indicate the supported image coding type */ \
  OMX_U32 image_coding_type;   \
  /** @param jpegdecReady boolean flag that is true when the audio coded has been initialized */ \
  OMX_BOOL jpegdecReady;  \
  /** @param isFirstBuffer Field that the buffer is the first buffer */ \
  OMX_S32 isFirstBuffer;\
  /** @param isNewBuffer Field that indicate a new buffer has arrived*/ \
  OMX_S32 isNewBuffer; \
  /** @param semaphore for jpeg decoder access syncrhonization */\
  tsem_t* jpegdecSyncSem; \
  tsem_t* jpegdecSyncSem1; \
  OMX_BUFFERHEADERTYPE* pInBuffer; \
  OMX_COMPONENTTYPE* hMarkTargetComponent; \
  OMX_PTR            pMarkData; \
  OMX_U32            nFlags;
ENDCLASS(omx_jpegdec_component_PrivateType)


/* Component private entry points declaration */
OMX_ERRORTYPE omx_jpegdec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_jpegdec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegdec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegdec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_jpegdec_decoder_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
void* omx_jpegdec_component_BufferMgmtFunction(void* param);

void omx_jpegdec_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_jpegdec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_jpegdec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

void jpeg_data_src (
  j_decompress_ptr cinfo, 
  omx_jpegdec_component_PrivateType *omx_jpegdec_component_Private);

#endif
