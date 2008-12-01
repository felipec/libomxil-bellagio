/**
  @file test/components/audio/omxaudioenctest.c

  This test application can encode audio pcm stream into mp3/aac/amr format.  
  
  Copyright (C) 2008  STMicroelectronics and Nokia

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
  
  $Date: 2008-02-12 12:06:30 +0530 (Tue, 12 Feb 2008) $
  Revision $Rev: 1298 $
  Author $Author: pankaj_sen $
*/

#ifndef __OMX_AUDIO_ENC_TEST_H__
#define __OMX_AUDIO_ENC_TEST_H__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>

#include <tsemaphore.h>
#include <user_debug_levels.h>

/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0

/* Application's private data */
typedef struct appPrivateType{
  pthread_cond_t condition;
  pthread_mutex_t mutex;
  tsem_t* audiosrcEventSem;
  tsem_t* eventSem;
  tsem_t* eofSem;
  OMX_HANDLETYPE audioenchandle;
  OMX_HANDLETYPE audiosrchandle;
}appPrivateType;

/* Size of the buffers requested to the component */
#define BUFFER_IN_SIZE 4*8192
#define FRAME_SIZE 1152*2*2 // 1152 samples * 2 channels * 16bits per samples

/* Callback prototypes for audio source */
OMX_ERRORTYPE audiosrcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE audiosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/* Callback prototypes for audio encoder*/
OMX_ERRORTYPE audioencEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_IN OMX_PTR pEventData);

OMX_ERRORTYPE audioencEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE audioencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** Helper functions */
static int getFileSize(int fd);

#endif
