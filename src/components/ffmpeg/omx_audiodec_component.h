/**
  @file src/components/ffmpeg/omx_audiodec_component.h

  This component implements and mp3 decoder. The Mp3 decoder is based on ffmpeg
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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#ifndef _OMX_MP3DEC_COMPONENT_H_
#define _OMX_MP3DEC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>
/* Specific include files for ffmpeg*/
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#define AUDIO_DEC_BASE_NAME "OMX.st.audio_decoder"
#define AUDIO_DEC_MP3_NAME "OMX.st.audio_decoder.mp3"
#define AUDIO_DEC_VORBIS_NAME "OMX.st.audio_decoder.ogg"
#define AUDIO_DEC_AAC_NAME "OMX.st.audio_decoder.aac" 
#define AUDIO_DEC_MP3_ROLE "audio_decoder.mp3"
#define AUDIO_DEC_VORBIS_ROLE "audio_decoder.ogg"
#define AUDIO_DEC_AAC_ROLE "audio_decoder.aac"   

/** Audio Decoder component port structure.
 */
DERIVEDCLASS(omx_audiodec_component_PortType, omx_base_PortType)
#define omx_audiodec_component_PortType_FIELDS omx_base_PortType_FIELDS \
  /** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
  OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; 
ENDCLASS(omx_audiodec_component_PortType)

/** AudioDec component private structure.
 */
DERIVEDCLASS(omx_audiodec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_audiodec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param avCodec pointer to the ffpeg audio decoder */ \
  AVCodec *avCodec;	\
  /** @param avCodecContext pointer to ffmpeg decoder context  */ \
  AVCodecContext *avCodecContext;	\
  /** @param semaphore for avcodec access syncrhonization */\
  tsem_t* avCodecSyncSem; \
  /** @param pAudioMp3 Referece to OMX_AUDIO_PARAM_MP3TYPE structure*/	\
  OMX_AUDIO_PARAM_MP3TYPE pAudioMp3;	\
  /** @param pAudioVorbis Reference to OMX_AUDIO_PARAM_VORBISTYPE structure */ \
  OMX_AUDIO_PARAM_VORBISTYPE pAudioVorbis;  \
  /** @param pAudioAAC Reference to  OMX_AUDIO_PARAM_AACPROFILETYPE structure */ \
   OMX_AUDIO_PARAM_AACPROFILETYPE  pAudioAac;  \
  /** @param pAudioPcmMode Referece to OMX_AUDIO_PARAM_PCMMODETYPE structure*/	\
  OMX_AUDIO_PARAM_PCMMODETYPE pAudioPcmMode;	\
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
  OMX_S32 isNewBuffer;	\
  /** @param audio_coding_type Field that indicate the supported audio format of audio decoder */ \
  OMX_U8 audio_coding_type;   \
  /** @param extradata pointer to extradata*/ \
  OMX_U8* extradata; \
  /** @param extradata_size extradata size*/ \
  OMX_U32 extradata_size;
ENDCLASS(omx_audiodec_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_audiodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_audiodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_audiodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_audiodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_audiodec_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);

void omx_audiodec_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);
	
OMX_ERRORTYPE omx_audiodec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audiodec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audiodec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex);

void omx_audiodec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp);


#endif
