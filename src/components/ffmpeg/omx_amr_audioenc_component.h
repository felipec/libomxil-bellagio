/**
  @file src/components/ffmpeg/omx_amr_audioenc_component.h

  This component implements an audio(AMR) encoder. The encoder is based on ffmpeg
  software library.

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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#ifndef _OMX_AMR_AUDIOENC_COMPONENT_H_
#define _OMX_AMR_AUDIOENC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>

/* Specific include files for FFmpeg*/
#if FFMPEG_LIBNAME_HEADERS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#else
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#endif

#define AUDIO_ENC_BASE_NAME    "OMX.st.audio_encoder"

#define AUDIO_ENC_AMR_NB_NAME     "OMX.st.audio_encoder.amrnb"
#define AUDIO_ENC_AMR_WB_NAME     "OMX.st.audio_encoder.amrwb"

#define AUDIO_ENC_AMR_NB_ROLE    "audio_encoder.amrnb"
#define AUDIO_ENC_AMR_WB_ROLE    "audio_encoder.amrwb"

/** AudioDec component private structure.
 */
DERIVEDCLASS(omx_amr_audioenc_component_PrivateType, omx_base_filter_PrivateType)
#define omx_amr_audioenc_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param avCodec pointer to the ffpeg audio encoder */ \
  AVCodec *avCodec;  \
  /** @param avCodecContext pointer to ffmpeg encoder context  */ \
  AVCodecContext *avCodecContext;  \
  /** @param semaphore for avcodec access syncrhonization */\
  tsem_t* avCodecSyncSem; \
  /** @param pAudioAmr Reference to  OMX_AUDIO_PARAM_AMRTYPE structure */ \
  OMX_AUDIO_PARAM_AMRTYPE pAudioAmr; \
  /** @param pAudioPcmMode Referece to OMX_AUDIO_PARAM_PCMMODETYPE structure*/  \
  OMX_AUDIO_PARAM_PCMMODETYPE pAudioPcmMode;  \
  /** @param avcodecReady boolean flag that is true when the audio coded has been initialized */ \
  OMX_BOOL avcodecReady;  \
  /** @param minBufferLength Field that stores the minimun allowed size for ffmpeg encoder */ \
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
  OMX_S32 isNewBuffer;  \
  /** @param audio_coding_type Field that indicate the supported audio format of audio encoder */ \
  OMX_U32 audio_coding_type; \
  /** @param temp_buffer Used for storing ununsed input buffer */ \
  OMX_U8* temp_buffer; \
  /** @param temp_buffer_filledlen Length of the ununsed input buffer */ \
  OMX_S32 temp_buffer_filledlen; \
  /** @param frame_length Length of each input audio frame. Depends of on sample rate, format and number of channel */ \
  OMX_S32 frame_length;
ENDCLASS(omx_amr_audioenc_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_amr_audioenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_amr_audioenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_amr_audioenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_amr_audioenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_amr_audioenc_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);

void omx_amr_audioenc_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);
  
OMX_ERRORTYPE omx_amr_audioenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_amr_audioenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_amr_audioenc_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex);

void omx_amr_audioenc_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp);

OMX_ERRORTYPE omx_amr_audioenc_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_amr_audioenc_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType);


#endif
