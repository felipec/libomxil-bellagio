/**
	@file src/components/vorbis/omx_vorbisdec_component.h
	
  This component implements a ogg-vorbis decoder. The vorbis decoder is based on libvorbis
  software library.
	
	Copyright (C) 2007  STMicroelectronics and Nokia

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
	
	$Date: 2007-04-05 12:45:25 +0200 (Thu, 05 Apr 2007) $
	Revision $Rev: 786 $
	Author $Author: giulio_urlini $

*/

#ifndef _OMX_VORBISDEC_COMPONENT_H_
#define _OMX_VORBISDEC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>

/* Specific include files for vorbis decoding */
#include <vorbis/codec.h>
#include <math.h>

#define AUDIO_DEC_BASE_NAME "OMX.st.audio_decoder"
#define AUDIO_DEC_VORBIS_NAME "OMX.st.audio_decoder.ogg.single"
#define AUDIO_DEC_VORBIS_ROLE "audio_decoder.ogg"

/** Vorbisdec component port structure.
 */
DERIVEDCLASS(omx_vorbisdec_component_PortType, omx_base_PortType)
#define omx_vorbisdec_component_PortType_FIELDS omx_base_PortType_FIELDS \
	/** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; 
ENDCLASS(omx_vorbisdec_component_PortType)

/** Vorbisdec component private structure.
 */
DERIVEDCLASS(omx_vorbisdec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_vorbisdec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param semaphore for avcodec access syncrhonization */\
  tsem_t* avCodecSyncSem; \
	/** @param pAudioVorbis Reference to OMX_AUDIO_PARAM_VORBISTYPE structure */ \
  OMX_AUDIO_PARAM_VORBISTYPE pAudioVorbis;  \
  /** @param pAudioPcmMode Referece to OMX_AUDIO_PARAM_PCMMODETYPE structure*/	\
  OMX_AUDIO_PARAM_PCMMODETYPE pAudioPcmMode;	\
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
  OMX_S32 isNewBuffer;	\
  /** @param audio_coding_type Field that indicate the supported audio format of audio decoder */ \
  OMX_U8 audio_coding_type;   
ENDCLASS(omx_vorbisdec_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_vorbisdec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_vorbisdec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_vorbisdec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_vorbisdec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_vorbis_decoder_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
	
void omx_vorbisdec_component_BufferMgmtCallbackVorbis(
	OMX_COMPONENTTYPE *openmaxStandComp,
	OMX_BUFFERHEADERTYPE* inputbuffer,
	OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_vorbisdec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_vorbisdec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_vorbisdec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_vorbisdec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_vorbisdec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef);

void SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp);


#endif //_OMX_VORBISDEC_COMPONENT_H_
