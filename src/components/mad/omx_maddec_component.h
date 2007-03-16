/**
 * @file src/components/mad/omx_maddec_component.h
 * 
 * This component implements and mp3 decoder. The Mp3 decoder is based on mad
 * software library.
 *
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Pankaj SEN,Ukri NIEMIMUUKKO, Diego MELPIGNANO, David SIORPAES, Giulio URLINI, SOURYA BHATTACHARYYA
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
 *
 * 2006/10/17:  mad mp3 decoder component version 0.2
 *
 */


#ifndef _OMX_MADDEC_COMPONENT_H_
#define _OMX_MADDEC_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <base_filter_component.h>

//specific include files
#include <mad.h>  //placed in /usr/local/include


#define AUDIO_DEC_BASE_NAME "OMX.st.mad.audio_decoder"
#define AUDIO_DEC_MP3_NAME "OMX.st.mad.audio_decoder.mp3"
//#define AUDIO_DEC_WMA_NAME "OMX.st.audio_decoder.wma"
#define AUDIO_DEC_MP3_ROLE "mad.audio_decoder.mp3"
//#define AUDIO_DEC_WMA_ROLE "audio_decoder.wma"


/** Mp3 mad Decoder component port structure.
 */
DERIVEDCLASS(omx_maddec_component_PortType, base_component_PortType)
#define omx_maddec_component_PortType_FIELDS base_component_PortType_FIELDS \
	/** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; 
ENDCLASS(omx_maddec_component_PortType)


//------------------------------------------------------------------------------------------------------------------

/** Mp3Dec mad component private structure.
 */
DERIVEDCLASS(omx_maddec_component_PrivateType, base_filter_component_PrivateType)
#define omx_maddec_component_PrivateType_FIELDS base_filter_component_PrivateType_FIELDS \
	/** @param decoder structure pointer to mad decoder structure */ \
	struct mad_decoder *decoder;  \
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
	OMX_U8 audio_coding_type;  
ENDCLASS(omx_maddec_component_PrivateType)

//-------------------------------------------------------------------------------------------------------------------


/* Component private entry points declaration */
OMX_ERRORTYPE omx_maddec_component_Constructor(stComponentType*);
OMX_ERRORTYPE omx_maddec_component_Destructor(stComponentType*);
OMX_ERRORTYPE omx_maddec_component_Init(stComponentType* stComponent);
OMX_ERRORTYPE omx_maddec_component_Deinit(stComponentType* stComponent);
OMX_ERRORTYPE omx_mad_decoder_MessageHandler(coreMessage_t* message);

void omx_maddec_component_BufferMgmtCallback(
	stComponentType* stComponent,
	OMX_BUFFERHEADERTYPE* inputbuffer,
	OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_maddec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_maddec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_maddec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_maddec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_maddec_component_ComponentRoleEnum(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING cRole,
	OMX_IN OMX_U32 nNameLength,
	OMX_IN OMX_U32 nIndex);

/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_maddec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef);


#endif //_OMX_MADDEC_COMPONENT_H_
