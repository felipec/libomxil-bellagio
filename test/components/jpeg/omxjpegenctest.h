/** 
  @file test/components/image/omxjpegenctest.h
 
  OpenMAX Integration Layer JPEG component test program
  
  This test program operates the OMX IL jpeg encoder component
  which encodes an jpeg file.
  Input file is given as the first argument and output
  is written to another file.
  
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
  
  $Date: 2008-09-11 11:37:30 +0200 (Thu, 11 Sep 2008) $
  Revision $Rev: 1481 $
  Author $Author: pankaj_sen $

*/


#include <OMX_Types.h>
#include <OMX_Image.h>
#include <OMX_Core.h>
#include <OMX_Component.h>

#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tsemaphore.h"

/* Application's private data */
typedef struct appPrivateType{
  void* input_data;
  OMX_BUFFERHEADERTYPE* currentInputBuffer;
  OMX_HANDLETYPE handle;
  OMX_CALLBACKTYPE callbacks;
  OMX_BUFFERHEADERTYPE* pInBuffer[1];
  OMX_BUFFERHEADERTYPE* pOutBuffer[1];
  tsem_t stateSem;
  tsem_t eosSem;
  OMX_S32 cInBufIndex;
  OMX_S32 frame_width;
  OMX_S32 frame_height;
  OMX_S32 outframe_buffer_size;
  char*  output_file;
}appPrivateType;

/* Size of the buffers requested to the component */
#define INPUT_BUFFER_SIZE    304128      /*Max input buffer size*/
#define OUTPUT_BUFFER_SIZE   640*480      /*Max output buffer size*/

/* Size of the buffers requested to the component */
#define BUFFER_SIZE   4096
#define FRAME_WIDTH   640
#define FRAME_HEIGHT  480
#define FRAME_BUFFER_SIZE        FRAME_WIDTH*FRAME_HEIGHT*3 +54 /* 640 x 480 x 2bytes x(3/2) +54bytes header*/


/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0

/* Callback prototypes */
OMX_ERRORTYPE jpegEncEventHandler(
  OMX_IN OMX_HANDLETYPE hComponent, 
  OMX_IN OMX_PTR pAppData, 
  OMX_IN OMX_EVENTTYPE eEvent, 
  OMX_IN OMX_U32 nData1, 
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData);

OMX_ERRORTYPE jpegEncEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE jpegEncFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** Helper functions */
static void setHeader(OMX_PTR header, OMX_U32 size);
