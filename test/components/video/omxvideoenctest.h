/**
  @file test/components/video/omxvideoenctest.h
  
  Test application that uses a OpenMAX component, a generic video encoder. 
  The application receives an video stream (.yuv) and encodes it in
  MPEG-4 Video format (.m4v).
  
  Copyright (C) 2008  STMicroelectronics and Nokia
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
  51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  
  $Date$
  Revision $Rev$
  Author $Author$
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Video.h>

#include <tsemaphore.h>
#include <user_debug_levels.h>

/* Application's private data */
typedef struct appPrivateType{
  tsem_t* encoderEventSem;
  tsem_t* eofSem;
  tsem_t* sourceEventSem;
  OMX_HANDLETYPE videosrchandle;
  OMX_HANDLETYPE videoenchandle;
}appPrivateType;

#define BUFFER_IN_SIZE 176*144*3/2

/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0


/* Callback prototypes for video source */
OMX_ERRORTYPE videosrcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE videosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/* Callback prototypes for video encoder */
OMX_ERRORTYPE videoencEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE videoencEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE videoencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** function prototype declaration */

/** display general help  */
void display_help();

/**  this function sets the video source and encoder port characteristics */
int setPortParameters();

