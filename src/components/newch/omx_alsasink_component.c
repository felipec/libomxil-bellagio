/**
 * @file src/components/newch/omx_alsasink_component.c
 * 
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Ukri NIEMIMUUKKO, Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI
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


#include <omxcore.h>
#include <omx_alsasink_component.h>


void __attribute__ ((constructor)) omx_alsasink_component_register_template() {
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	stComponentType *component = base_component_CreateComponentStruct();
	/** here you can override the base component defaults */
	component->name = "OMX.volume.component";
	component->constructor = omx_alsasink_component_Constructor;
	component->omx_component.SetConfig = omx_alsasink_component_SetConfig;
	component->omx_component.GetConfig = omx_alsasink_component_GetConfig;
	component->omx_component.SetParameter = omx_alsasink_component_SetParameter;
	component->omx_component.GetParameter = omx_alsasink_component_GetParameter;
	  
	// port count must be set for the base class constructor (if we call it, and we will)
	component->nports = 1;
  
 	register_template(component);
}

OMX_ERRORTYPE omx_alsasink_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_alsasink_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = omx_oneport_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
		/** Domain specific section for the ports. */	
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;

	setHeader(&omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.nPortIndex = 0;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.nIndex = 0;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.eEncoding = 0;

	omx_alsasink_component_Private->BufferMgmtCallback = omx_alsasink_component_BufferMgmtCallback;

	/* OMX_AUDIO_PARAM_PCMMODETYPE */
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nPortIndex = 0;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nChannels = 2;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.eNumData = OMX_NumericalDataSigned;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.eEndian = OMX_EndianLittle;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.bInterleaved = OMX_TRUE;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nBitPerSample = 16;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nSamplingRate = 44100;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;
	/* Todo: add the volume stuff */

	/* Allocate the playback handle and the hardware parameter structure */
	if ((err = snd_pcm_open (&omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot open audio device %s (%s)\n", "default", snd_strerror (err));
		base_component_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %08x %08X in %i\n", (int)omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, (int)&omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, getpid());

	if (snd_pcm_hw_params_malloc(&omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hw_params) < 0) {
		DEBUG(DEB_LEV_ERR, "%s: failed allocating input port hw parameters\n", __func__);
		base_component_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got hw parameters at %08x\n", (int)omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hw_params);

	if ((err = snd_pcm_hw_params_any (omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot initialize hardware parameter structure (%s)\n",	snd_strerror (err));
		base_component_Panic();
	}

	/* Write in the default paramenters */
	omx_alsasink_component_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioInit, &omx_alsasink_component_Private->sPortTypesParam);
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->AudioPCMConfigured	= 0;

	omx_alsasink_component_Private->Init = &omx_alsasink_component_Init;

	return err;
}

OMX_ERRORTYPE omx_alsasink_component_Init(stComponentType* stComponent)
{
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	base_component_Init(stComponent);

	if (!omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->AudioPCMConfigured) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
		alsasink_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioPcm, &omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode);
	}

	/** Configure and prepare the ALSA handle */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface\n");
	
	if ((err = snd_pcm_hw_params (omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot set parameters (%s)\n",	snd_strerror (err));
		base_component_Panic();
	}
	
	if ((err = snd_pcm_prepare (omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
		base_component_Panic();
	}
	
};


void omx_alsasink_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer) {
	int i;
	//int sampleCount = inputbuffer->nFilledLen / 2; // signed 16 bit samples assumed
	OMX_U32  frameSize;
	OMX_S32 written;
	OMX_S32 totalBuffer;
	OMX_S32 offsetBuffer;
	OMX_BOOL allDataSent;

	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;

	/*
	for (i = 0; i < sampleCount; i++) {
		((OMX_S16*) outputbuffer->pBuffer)[i] = (OMX_S16)
			(((OMX_S16*) inputbuffer->pBuffer)[i] * (omx_alsasink_component_Private->gain / 100.0f));
	}
	outputbuffer->nFilledLen = inputbuffer->nFilledLen;
	*/

	/* Feed it to ALSA */
	frameSize = (omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nChannels * omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nBitPerSample) >> 3;
	DEBUG(DEB_LEV_FULL_SEQ, "Framesize is %u chl=%d bufSize=%d\n", 
		frameSize,omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nChannels,inputbuffer->nFilledLen);
	allDataSent = OMX_FALSE;
	totalBuffer = inputbuffer->nFilledLen/frameSize;
	offsetBuffer = 0;
	while (!allDataSent) {
		written = snd_pcm_writei(omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, inputbuffer->pBuffer + (offsetBuffer * frameSize), totalBuffer);
		if (written < 0) {
			if(written == -EPIPE){
				DEBUG(DEB_LEV_ERR, "ALSA Underrun..\n");
				snd_pcm_prepare(omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle);
				written = 0;
			} else {
				DEBUG(DEB_LEV_ERR, "Cannot send any data to the audio device %s (%s)\n", "default", snd_strerror (written));
				alsasink_Panic();
			}
		}

		if(written != totalBuffer){
		totalBuffer = totalBuffer - written;
		offsetBuffer = written;
		} else {
			DEBUG(DEB_LEV_FULL_SEQ, "Buffer successfully sent to ALSA. Length was %i\n", (int)inputbuffer->nFilledLen);
			allDataSent = OMX_TRUE;
		}
	}
}

OMX_ERRORTYPE omx_alsasink_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) 
{
	return base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE omx_alsasink_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	//fixme
	return base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE omx_alsasink_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
	OMX_U32 portIndex;
	
	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	omx_alsasink_component_PortType* inputPort = omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];
	snd_pcm_t* playback_handle = inputPort->playback_handle;
	snd_pcm_hw_params_t* hw_params = inputPort->hw_params;

	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

	/** Each time we are (re)configuring the hw_params thing
	 * we need to reinitialize it, otherwise previous changes will not take effect.
	 * e.g.: changing a previously configured sampling rate does not have
	 * any effect if we are not calling this each time.
	 */
	err = snd_pcm_hw_params_any (omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->playback_handle, omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hw_params);

	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
			if (err != OMX_ErrorNone)
				return err;
			memcpy(&omx_alsasink_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
			break;	
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (err != OMX_ErrorNone)
					return err;
			if (portIndex < 1) {
				memcpy(&omx_alsasink_component_Private->ports[portIndex]->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} else {
					return OMX_ErrorBadPortIndex;
			}
			break;
			case OMX_IndexParamAudioPcm:
			{
				unsigned int rate;
				OMX_AUDIO_PARAM_PCMMODETYPE* omxAudioParamPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
				inputPort->AudioPCMConfigured	= 1;
				if(omxAudioParamPcmMode->nPortIndex != inputPort->omxAudioParamPcmMode.nPortIndex){
					DEBUG(DEB_LEV_ERR, "Error setting input port index\n");
					err = OMX_ErrorBadParameter;
					break;
				}
				
				if(snd_pcm_hw_params_set_channels(playback_handle, hw_params, omxAudioParamPcmMode->nChannels)){
					DEBUG(DEB_LEV_ERR, "Error setting number of channels\n");
					return OMX_ErrorBadParameter;
				}

				if(omxAudioParamPcmMode->bInterleaved == OMX_TRUE){
					if ((err = snd_pcm_hw_params_set_access(inputPort->playback_handle, inputPort->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type intrleaved (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
				}
				else{
					if ((err = snd_pcm_hw_params_set_access(inputPort->playback_handle, inputPort->hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type non interleaved (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
				}

				rate = omxAudioParamPcmMode->nSamplingRate;
				if ((err = snd_pcm_hw_params_set_rate_near(inputPort->playback_handle, inputPort->hw_params, &rate, 0)) < 0) {
					DEBUG(DEB_LEV_ERR, "cannot set sample rate (%s)\n", snd_strerror (err));
					alsasink_Panic();
				}
				else{
					DEBUG(DEB_LEV_PARAMS, "Set correctly sampling rate to %i\n", (int)inputPort->omxAudioParamPcmMode.nSamplingRate);
				}
					
				if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeLinear){
					snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;
					DEBUG(DEB_LEV_PARAMS, "Bit per sample %i, signed=%i, little endian=%i\n",
						(int)omxAudioParamPcmMode->nBitPerSample,
						(int)omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned,
						(int)omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle);
					
					switch(omxAudioParamPcmMode->nBitPerSample){
						case 8:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned)
								snd_pcm_format = SND_PCM_FORMAT_S8;
							else
								snd_pcm_format = SND_PCM_FORMAT_U8;
							break;
							
						case 16:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S16_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S16_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle){
									
									snd_pcm_format = SND_PCM_FORMAT_U16_LE;
								}
								else
									snd_pcm_format = SND_PCM_FORMAT_U16_BE;
							}
							break;
							
						case 24:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S24_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S24_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_U24_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_U24_BE;
							}
							break;
							
						case 32:
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_S32_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_S32_BE;
							}
							if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
								if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle)
									snd_pcm_format = SND_PCM_FORMAT_U32_LE;
								else
									snd_pcm_format = SND_PCM_FORMAT_U32_BE;
						}
							break;
							
						default:
							err = OMX_ErrorBadParameter;
					}
					
					if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN){
						int err;
						if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
							DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
							alsasink_Panic();
						}
					}
					else{
						DEBUG(DEB_LEV_SIMPLE_SEQ, "ALSA OMX_IndexParamAudioPcm configured\n");
						memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					}
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeALaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_A_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
						alsasink_Panic();
					}
					memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeMULaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_MU_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n", snd_strerror (err));
						alsasink_Panic();
					}
					memcpy(&inputPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					break;
				}
			}
		break;
		case OMX_IndexParamAudioMp3:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			if((err = checkHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
		//fixme
		default:
			return base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_alsasink_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	OMX_U32 portIndex;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
			memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;		
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		if (pAudioPortFormat->nPortIndex < 1) {
			memcpy(pAudioPortFormat, &omx_alsasink_component_Private->ports[pAudioPortFormat->nPortIndex]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;		
		case OMX_IndexParamAudioPcm:
			if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
				omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode.nPortIndex)
				return OMX_ErrorBadParameter;
			memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			break;
		default:
			return base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}
