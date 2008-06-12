/**
  @file src/components/audio_effects/omx_audio_mixer_component.h

  OpenMAX audio_mixer control component. This component implements a mixer that 
  mixes multiple audio PCM streams and produces a single output stream.

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

#ifndef _OMX_AUDIO_MIXER_COMPONENT_H_
#define _OMX_AUDIO_MIXER_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <omx_base_filter.h>
#include <omx_base_audio_port.h>

#define MAX_PORTS   5 // Maximum number of ports supported by the mixer. 4 input and 1 output
#define MAX_CHANNEL 6 // Maximum number of channels supported in a single stream 5.1

/** Audio Mixer port structure.
  */
DERIVEDCLASS(omx_audio_mixer_component_PortType, omx_base_audio_PortType)
#define omx_audio_mixer_component_PortType_FIELDS omx_base_audio_PortType_FIELDS \
  /** @param pAudioPcmMode Referece to OMX_AUDIO_PARAM_PCMMODETYPE structure*/  \
  OMX_AUDIO_PARAM_PCMMODETYPE pAudioPcmMode; \
  /** @param gain the audio_mixer gain value */ \
  float gain; \
  /** @param sVolume Audio Volume adjustment for a port */ \
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume; \
  /** @param sChannelVolume Audio Volume adjustment for a channel */ \
  OMX_AUDIO_CONFIG_CHANNELVOLUMETYPE sChannelVolume[MAX_CHANNEL]; 
ENDCLASS(omx_audio_mixer_component_PortType)

/** Twoport component private structure.
* see the define above
*/
DERIVEDCLASS(omx_audio_mixer_component_PrivateType, omx_base_filter_PrivateType)
#define omx_audio_mixer_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** This class is empty for the time being */ 
ENDCLASS(omx_audio_mixer_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_audio_mixer_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_audio_mixer_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_audio_mixer_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_audio_mixer_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audio_mixer_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audio_mixer_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_audio_mixer_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

/** This is the central function for component processing, overridden for audio mixer. It
  * is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_audio_mixer_BufferMgmtFunction (void* param);

#endif
