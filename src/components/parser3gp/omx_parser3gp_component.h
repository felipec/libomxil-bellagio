/**
  @file src/components/parser3gp/omx_parser3gp_component.h

  OpenMAX parser3gp component. This component is a 3gp parser that parses the input
  3gp file to provide the audio and video streams outputs

  Copyright (C) 2008  STMicroelectronics
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
  51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA

  $Date$
  Revision $Rev$
  Author $Author$

*/

#ifndef _OMX_PARSER_3GP_COMPONENT_H_
#define _OMX_PARSER_3GP_COMPONENT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <omx_base_source.h>
#include <string.h>

/* Specific include files for FFmpeg library related decoding*/
#if FFMPEG_LIBNAME_HEADERS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#else
/* Specific include files for ffmpeg library related decoding*/
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avio.h>
#endif

/** Maximum number of base_component component instances */
#define MAX_NUM_OF_parser3gp_component_INSTANCES 1

/** Parser3gp component private structure.
 * see the define above
 * @param sTimeStamp Store Time Stamp to be set
 * @param avformatcontext is the ffmpeg video format context
 * @param avformatparameters is the ffmpeg video format related parameters 
 * @param avinputformat is the ffmpeg video format related settings 
 * @param pTmpOutputBuffer is a temporary buffer to hold the data from av_read_frame 
 * @param sInputFileName is the input filename provided by client 
 * @param video_coding_type is the coding type determined by input file 
 * @param audio_coding_type is the coding type determined by input file 
 * @param semaphore for avformat syncrhonization 
 * @param avformatReady boolean flag that is true when the video format has been initialized 
 * @param xScale the scale of the media clock
 * @param pkt is the ffmpeg packet structure for data delivery 
 * @param isFirstBufferAudio Field that the buffer is the first buffer of Audio Stream
 * @param isFirstBufferVideo Field that the buffer is the first buffer of Video Stream
 */
DERIVEDCLASS(omx_parser3gp_component_PrivateType, omx_base_source_PrivateType)
#define omx_parser3gp_component_PrivateType_FIELDS omx_base_source_PrivateType_FIELDS \
  OMX_TIME_CONFIG_TIMESTAMPTYPE       sTimeStamp; \
  AVFormatContext                     *avformatcontext; \
  AVFormatParameters                  *avformatparameters; \
  AVInputFormat                       *avinputformat; \
  OMX_BUFFERHEADERTYPE*               pTmpOutputBuffer; \
  OMX_STRING                          sInputFileName; \
  OMX_U32                             video_coding_type; \
  OMX_U32                             audio_coding_type; \
  tsem_t*                             avformatSyncSem; \
  OMX_BOOL                            avformatReady; \
  OMX_S32                             xScale; \
  AVPacket                            pkt; \
  OMX_S32                             isFirstBufferAudio; \
  OMX_S32                             isFirstBufferVideo;
ENDCLASS(omx_parser3gp_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_parser3gp_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_parser3gp_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_parser3gp_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);
OMX_ERRORTYPE omx_parser3gp_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_parser3gp_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_parser3gp_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_parser3gp_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_parser3gp_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_parser3gp_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_parser3gp_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_parser3gp_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType);

#endif

