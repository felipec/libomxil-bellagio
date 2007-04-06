/**
	@file src/components/alsa/omx_alsasink_component.h
	
  OpenMax alsa sink component. This component is an audio sink that uses ALSA library.
	
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

#ifndef _OMX_ALSASINK_COMPONENT_H_
#define _OMX_ALSASINK_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <pthread.h>
#include <omx_base_sink.h>
#include <alsa/asoundlib.h>

/** Maximum number of base_component component instances */
#define MAX_NUM_OF_alsasink_component_INSTANCES 1

/** Alsasinkport component port structure.
 */
DERIVEDCLASS(omx_alsasink_component_PortType, omx_base_PortType)
#define omx_alsasink_component_PortType_FIELDS omx_base_PortType_FIELDS \
	/** @param sAudioParam Domain specific (audio) OpenMAX port parameter */ \
	OMX_AUDIO_PARAM_PORTFORMATTYPE sAudioParam; \
	/** @param omxAudioParamPcmMode Audio PCM pecific OpenMAX parameter */ \
	OMX_AUDIO_PARAM_PCMMODETYPE omxAudioParamPcmMode;	\
	/** @param omxAudioConfigVolume Audio Volume OpenMAX parameter */	\
	OMX_AUDIO_CONFIG_VOLUMETYPE omxAudioConfigVolume;	\
	/** @param omxAudioChannelVolume Audio Volume OpenMAX parameter for channel */	\
	OMX_AUDIO_CONFIG_CHANNELVOLUMETYPE omxAudioChannelVolume;	\
	/** @param omxAudioChannelMute Audio Volume OpenMAX parameter for mute for channel */	\
	OMX_AUDIO_CONFIG_CHANNELMUTETYPE omxAudioChannelMute;	\
	/** @param omxAudioMute Audio Volume OpenMAX parameter for mute */	\
	OMX_AUDIO_CONFIG_MUTETYPE omxAudioMute;	\
	/** @param AudioPCMConfigured boolean flag to check if the audio has been configured */	\
	char AudioPCMConfigured;	\
	/** @param playback_handle ALSA specif handle for audio player */	\
	snd_pcm_t* playback_handle;	\
	/** @param hw_params ALSA specif hardware parameters */	\
	snd_pcm_hw_params_t* hw_params;	\
	/** @param rate Audio sample rate */	\
	OMX_U32 rate;	
ENDCLASS(omx_alsasink_component_PortType)

/** Alsasinkport component private structure.
 * see the define above
 */
DERIVEDCLASS(omx_alsasink_component_PrivateType, omx_base_sink_PrivateType)
#define omx_alsasink_component_PrivateType_FIELDS omx_base_sink_PrivateType_FIELDS 
ENDCLASS(omx_alsasink_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_alsasink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_alsasink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_alsasink_component_BufferMgmtCallback(
	OMX_COMPONENTTYPE *openmaxStandComp,
	OMX_BUFFERHEADERTYPE* inputbuffer);

OMX_ERRORTYPE omx_alsasink_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_alsasink_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_alsasink_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_alsasink_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_alsasink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_alsasink_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef);

#endif //_OMX_ALSASINK_COMPONENT_H_
