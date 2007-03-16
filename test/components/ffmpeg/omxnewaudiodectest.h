/**
	@file test/components/ffmpeg/omx11audiodectest.h
	
	Test application that uses two OpenMAX components, a gneric audio decoder and 
	an alsa sink. The application receives an audio stream decoded by a multiple format decoder component.
	The decoded output is sent to an alsa sink. 
	It is possible to specify the option -t that requests a tunnel between OpenMAX components.
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

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
	
	2006/02/08:  Mp3 decoder and tunnel test version 0.1

*/

#include <OMX_Core.h>
#include <OMX_Types.h>

#include <pthread.h>
#include <sys/stat.h>

#include "tsemaphore.h"

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

//#include <user_debug_levels.h>
/* Application's private data */
typedef struct appPrivateType{
	tsem_t* decoderEventSem;
	tsem_t* eofSem;
	OMX_HANDLETYPE audiodechandle;

}appPrivateType;

#define BUFFER_IN_SIZE 4096
#define BUFFER_OUT_SIZE 8192

/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    0
#define VERSIONREVISION 0
#define VERSIONSTEP     0

/* Callback prototypes */
OMX_ERRORTYPE audiodecEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData);

OMX_ERRORTYPE audiodecEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE audiodecFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

