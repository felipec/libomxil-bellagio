/**
  @file test/components/camera/omxcameratest.h

  Test application that uses two OpenMAX components, a camera and a fbsink.
  The preview port of the camera is tunneled with the fbsink component;
  The output video/image data of the capture port and thumbnail port of the
  camera are saved in disk files, respectively.

  Copyright (C) 2007  Motorola

  This code is licensed under LGPL see README for full LGPL notice.

  Date                             Author                Comment
  Fri, 06 Jul 2007                 Motorola              File created
  Tue, 06 Apr 2008               STM                     Update: Adding support for the color converter

  This Program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  $Date$
  Revision $Rev$
  Author $Author$

*/

#ifndef _OMX_CAMERA_TEST_H_
#define _OMX_CAMERA_TEST_H_

#include <pthread.h>

#include "tsemaphore.h"
#include <user_debug_levels.h>



/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0


/** Default settings */
#define DEFAULT_FRAME_RATE 15

#define DEFAULT_FRAME_WIDTH 320
#define DEFAULT_FRAME_HEIGHT 240

#define DEFAULT_CAMERA_COLOR_FORMAT OMX_COLOR_FormatYUV420PackedPlanar
#define DEFAULT_FBSINK_COLOR_FORMAT OMX_COLOR_Format24bitRGB888
#define DEFAULT_CAPTURE_COLOR_FORMAT DEFAULT_CAMERA_COLOR_FORMAT

#define MAXBUFNUM_PERPORT 16

/** Port Index for Camera Component */
enum 
{
    OMX_CAMPORT_INDEX_VF = 0, /* preview/viewfinder */
    OMX_CAMPORT_INDEX_CP,       /* captured video */
    OMX_CAMPORT_INDEX_CP_T,   /* thumbnail or snapshot for captured video */
    OMX_CAMPORT_INDEX_MAX
};

#define NUM_CAMERAPORTS (OMX_CAMPORT_INDEX_MAX)


/** Application's private data */
typedef struct appPrivateType{
  tsem_t* cameraSourceEventSem;
  tsem_t* colorconvEventSem;
  tsem_t* fbsinkEventSem;
  OMX_HANDLETYPE camerahandle;
  OMX_HANDLETYPE colorconvhandle;
  OMX_HANDLETYPE fbsinkhandle;
}appPrivateType;


/** Buffer context structure */
typedef struct OMX_PORTBUFFERCTXT{
  OMX_BUFFERHEADERTYPE* pBufHeaderList[MAXBUFNUM_PERPORT];
  OMX_U32 nBufferCountActual;
}OMX_PORTBUFFERCTXT;




/** Callback prototypes for camera component */
OMX_ERRORTYPE camera_sourceEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE camera_sourceFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** callbacks implementation of color converter component */
OMX_ERRORTYPE colorconvEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE colorconvEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE colorconvFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** Callback prototypes for fbsink component */
OMX_ERRORTYPE fbsinkEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE fbsinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);


/** Initialize header fields (size and version) of OMX structure */
OMX_ERRORTYPE setHeader(OMX_PTR header, OMX_U32 size);

/** Set parameters for camera/fbsink components */
OMX_ERRORTYPE setCameraParameters(OMX_BOOL bCameraStillImageMode);
OMX_ERRORTYPE setColorConvParameters();
OMX_ERRORTYPE setFbsinkParameters();


#endif
