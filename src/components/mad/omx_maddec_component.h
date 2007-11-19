/**
  @file src/components/mad/omx_maddec_component.h

  This component implements and mp3 decoder based on mad
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

#ifndef _OMX_MADDEC_COMPONENT_H_
#define _OMX_MADDEC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>

//specific include files
#include <mad.h>  

#define AUDIO_DEC_BASE_NAME "OMX.st.audio_decoder"
#define AUDIO_DEC_MP3_NAME "OMX.st.audio_decoder.mp3.mad"
#define AUDIO_DEC_MP3_ROLE "mad_decoder.mp3"

/** Mp3Dec mad component private structure.
 */
DERIVEDCLASS(omx_maddec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_maddec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param stream structure pointer to mad stream structure */ \
  struct mad_stream *stream;  \
  /** @param frame structure pointer to mad frame structure */ \
  struct mad_frame *frame;  \
  /** @param synth structure pointer to mad synth structure */ \
  struct mad_synth *synth;  \
  /** @param semaphore for mad decoder access syncrhonization */\
  tsem_t* madDecSyncSem; \
  /** @param pAudioMp3 Referece to OMX_AUDIO_PARAM_MP3TYPE structure*/	\
  OMX_AUDIO_PARAM_MP3TYPE pAudioMp3;	\
  /** @param pAudioPcmMode Referece to OMX_AUDIO_PARAM_PCMMODETYPE structure*/	\
  OMX_AUDIO_PARAM_PCMMODETYPE pAudioPcmMode;	\
  /** @param maddecReady boolean flag that is true when the audio coded has been initialized */ \
  OMX_BOOL maddecReady;	\
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
  OMX_U32 audio_coding_type;  \
  /** @param temporary_buffer temporary buffer used for audio decoding */ \
  OMX_BUFFERHEADERTYPE *temporary_buffer; \
  /** @param need_mad_stream boolean indicate whether new mad stream required */ \
  OMX_U32 need_mad_stream;
ENDCLASS(omx_maddec_component_PrivateType)

//-------------------------------------------------------------------------------------------------------------------


/* Component private entry points declaration */
OMX_ERRORTYPE omx_maddec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_maddec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_maddec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_maddec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_mad_decoder_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);

void omx_maddec_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_maddec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_maddec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

void omx_maddec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp);

#endif
