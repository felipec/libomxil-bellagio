/**
  @file src/components/filereader/omx_filereader_component.h

  OpenMax file reader component. This component is an file reader that detects the input
  file format so that client calls the appropriate decoder.

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

#ifndef _OMX_FILEREADER_COMPONENT_H_
#define _OMX_FILEREADER_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <pthread.h>
#include <omx_base_source.h>
#include <string.h>

/* Specific include files for ffmpeg library related decoding*/
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avio.h>

/** Maximum number of base_component component instances */
#define MAX_NUM_OF_filereader_component_INSTANCES 1

/** Filereaderport component port structure.
 */
DERIVEDCLASS(omx_filereader_component_PortType, omx_base_PortType)
#define omx_filereader_component_PortType_FIELDS omx_base_PortType_FIELDS \
  /** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
  OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; 	\
  /** @param sTimeStamp Store Time Stamp to be set*/ \
  OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
ENDCLASS(omx_filereader_component_PortType)

/** Filereader component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_filereader_component_PrivateType, omx_base_source_PrivateType)
#define omx_filereader_component_PrivateType_FIELDS omx_base_source_PrivateType_FIELDS \
  /** @param avformatcontext is the ffmpeg audio format context */ \
  AVFormatContext *avformatcontext; \
  /** @param avformatparameters is the ffmpeg audio format related parameters */ \
  AVFormatParameters *avformatparameters; \
  /** @param avinputformat is the ffmpeg audio format related settings */ \
  AVInputFormat *avinputformat; \
  /** @param pkt is the ffmpeg packet structure for data delivery */\ 
  AVPacket pkt; \
  /** @param isFirstBuffer is the int variable signifying first processed buffer */ \
  OMX_U32 isFirstBuffer; \
  /** @param isNewBuffer is the int variable signifying need of new buffer for processing*/ \
  OMX_U32 isNewBuffer; \
  /** @param sInputFileName is the input filename provided by client */ \
  OMX_STRING sInputFileName; \
  /** @param audio_coding_type is the coding type determined by input file */ \
  OMX_U32 audio_coding_type; \
  /** @param semaphore for avformat syncrhonization */\
  tsem_t* avformatSyncSem; \
  /** @param avformatReady boolean flag that is true when the audio format has been initialized */ \
  OMX_BOOL avformatReady;	\
  /** @param avformatReady boolean flag that is true when the audio format has been initialized */ \
  OMX_BOOL bIsEOSSent;	
ENDCLASS(omx_filereader_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_filereader_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_filereader_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_filereader_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
OMX_ERRORTYPE omx_filereader_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_filereader_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);


void omx_filereader_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_filereader_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_filereader_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_filereader_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_filereader_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_filereader_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType);

#endif

