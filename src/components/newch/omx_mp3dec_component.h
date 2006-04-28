/**
 * @file src/components/newch/omx_mp3dec_component.h
 * 
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Pankaj SEN,Ukri NIEMIMUUKKO, Diego MELPIGNANO, David SIORPAES, Giulio URLINI
 * 
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _OMX_MP3DEC_COMPONENT_H_
#define _OMX_MP3DEC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_twoport_component.h>
/* Specific include files */
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>


/** Twoport component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_mp3dec_component_PrivateType, omx_twoport_component_PrivateType)
#define omx_mp3dec_component_PrivateType_FIELDS omx_twoport_component_PrivateType_FIELDS \
	/** @param avCodec pointer to the ffpeg audio decoder */ \
	AVCodec *avCodec;	\
	/** @param avCodecContext pointer to ffmpeg decoder context  */ \
	AVCodecContext *avCodecContext;	\
	/** @param avcodecReady boolean flag that is true when the audio coded has been initialized */ \
	OMX_BOOL avcodecReady;	\
	/** @param minBufferLength Field that stores the minimun allowed size for ffmpeg decoder */ \
	OMX_U16 minBufferLength; \
	/** @param inputCurrBuffer Field that stores pointer of the current input buffer position */ \
	OMX_U8* inputCurrBuffer;\
	/** @param inputCurrLength Field that stores current input buffer length in bytes */ \
	OMX_U32 inputCurrLength;\
	/** @param internalOutputBuffer Field used for first internal output buffer */ \
	OMX_U8* internalOutputBuffer;\
	/** @param isFirstBuffer Field that the buffer is the first buffer */ \
	OMX_S32 isFirstBuffer;\
	/** @param positionInOutBuf Field that used to calculate starting address of the next output frame to be written */ \
	OMX_S32 positionInOutBuf; \
	/** @param isNewBuffer Field that indicate a new buffer has arrived*/ \
	OMX_S32 isNewBuffer;
ENDCLASS(omx_mp3dec_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_mp3dec_component_Constructor(stComponentType*);
OMX_ERRORTYPE omx_mp3dec_component_Destructor(stComponentType*);
OMX_ERRORTYPE omx_mp3dec_component_Init(stComponentType* stComponent);
OMX_ERRORTYPE omx_mp3dec_component_Deinit(stComponentType* stComponent);

void omx_mp3dec_component_BufferMgmtCallback(
	stComponentType* stComponent,
	OMX_BUFFERHEADERTYPE* inputbuffer,
	OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_mp3dec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_mp3dec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_mp3dec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_mp3dec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

#endif //_OMX_MP3DEC_COMPONENT_H_
