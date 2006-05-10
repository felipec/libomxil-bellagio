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

/** Maximum Number of base_component Instance*/
OMX_U32 noAlsasinkInstance=0;

void __attribute__ ((constructor)) omx_alsasink_component_register_template() {
	stComponentType *component;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	component = base_component_CreateComponentStruct();
	/** here you can override the base component defaults */
	component->name = "OMX.st.alsa.alsasink";
	component->constructor = omx_alsasink_component_Constructor;
	component->destructor = omx_alsasink_component_Destructor;
	component->omx_component.SetConfig = omx_alsasink_component_SetConfig;
	component->omx_component.GetConfig = omx_alsasink_component_GetConfig;
	component->omx_component.SetParameter = omx_alsasink_component_SetParameter;
	component->omx_component.GetParameter = omx_alsasink_component_GetParameter;
	  
	// port count must be set for the base class constructor (if we call it, and we will)
	component->nports = 1;
  
 	register_template(component);
}


OMX_ERRORTYPE omx_alsasink_component_Destructor(stComponentType* stComponent) {
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;

	omx_alsasink_component_PortType* port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];

	noAlsasinkInstance--;

	if(port->hw_params)
		snd_pcm_hw_params_free (port->hw_params);
	if(port->playback_handle)
		snd_pcm_close(port->playback_handle);
	
	base_component_Destructor(stComponent);
}

OMX_ERRORTYPE omx_alsasink_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	OMX_S32 i;
	omx_alsasink_component_PortType *port;
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_alsasink_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}

	omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;

	// fixme maybe the base class could use a "port factory" function pointer?	
	if (stComponent->nports && !omx_alsasink_component_Private->ports) {
		omx_alsasink_component_Private->ports = calloc(stComponent->nports,
												sizeof (base_component_PortType *));

		if (!omx_alsasink_component_Private->ports) return OMX_ErrorInsufficientResources;
		
		for (i=0; i < stComponent->nports; i++) {
			// this is the important thing separating this from the base class; size of the struct is for derived class port type
			// this could be refactored as a smarter factory function instead?
			omx_alsasink_component_Private->ports[i] = calloc(1, sizeof(omx_alsasink_component_PortType));
			if (!omx_alsasink_component_Private->ports[i]) return OMX_ErrorInsufficientResources;

			omx_alsasink_component_Private->ports[i]->transientState = OMX_StateMax;
			setHeader(&omx_alsasink_component_Private->ports[i]->sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
			omx_alsasink_component_Private->ports[i]->sPortParam.nPortIndex = i;
			
			omx_alsasink_component_Private->ports[i]->pBufferSem = malloc(sizeof(tsem_t));
			if(omx_alsasink_component_Private->ports[i]->pBufferSem==NULL) return OMX_ErrorInsufficientResources;
			tsem_init(omx_alsasink_component_Private->ports[i]->pBufferSem, 0);
		
			omx_alsasink_component_Private->ports[i]->pFullAllocationSem = malloc(sizeof(tsem_t));
			if(omx_alsasink_component_Private->ports[i]->pFullAllocationSem==NULL) return OMX_ErrorInsufficientResources;
			tsem_init(omx_alsasink_component_Private->ports[i]->pFullAllocationSem, 0);
		
			/** Allocate and initialize buffer queue */
			omx_alsasink_component_Private->ports[i]->pBufferQueue = malloc(sizeof(queue_t));
			if(omx_alsasink_component_Private->ports[i]->pBufferQueue==NULL) return OMX_ErrorInsufficientResources;
			queue_init(omx_alsasink_component_Private->ports[i]->pBufferQueue);
		
			omx_alsasink_component_Private->ports[i]->pFlushSem = malloc(sizeof(tsem_t));
			if(omx_alsasink_component_Private->ports[i]->pFlushSem==NULL)	return OMX_ErrorInsufficientResources;
			tsem_init(omx_alsasink_component_Private->ports[i]->pFlushSem, 0);

			omx_alsasink_component_Private->ports[i]->hTunneledComponent = NULL;
			omx_alsasink_component_Private->ports[i]->nTunneledPort=0;
			omx_alsasink_component_Private->ports[i]->nTunnelFlags=0;

		}
		base_component_SetPortFlushFlag(stComponent, -1, OMX_FALSE);
		base_component_SetNumBufferFlush(stComponent, -1, 0);
	}

	err = omx_oneport_component_Constructor(stComponent);
	
	// set the port params, now that the ports exist	
	/** Domain specific section for the ports. */	
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;

	/*
	setHeader(&omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.nPortIndex = 0;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.nIndex = 0;
	omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sAudioParam.eEncoding = 0;
	*/
	
	omx_alsasink_component_Private->BufferMgmtCallback = omx_alsasink_component_BufferMgmtCallback;

	port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];

	setHeader(&port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	port->sAudioParam.nPortIndex = 0;
	port->sAudioParam.nIndex = 0;
	port->sAudioParam.eEncoding = 0;

	/* OMX_AUDIO_PARAM_PCMMODETYPE */
	port->omxAudioParamPcmMode.nPortIndex = 0;
	port->omxAudioParamPcmMode.nChannels = 2;
	port->omxAudioParamPcmMode.eNumData = OMX_NumericalDataSigned;
	port->omxAudioParamPcmMode.eEndian = OMX_EndianLittle;
	port->omxAudioParamPcmMode.bInterleaved = OMX_TRUE;
	port->omxAudioParamPcmMode.nBitPerSample = 16;
	port->omxAudioParamPcmMode.nSamplingRate = 44100;
	port->omxAudioParamPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
	port->omxAudioParamPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

	noAlsasinkInstance++;
	if(noAlsasinkInstance > MAX_NUM_OF_alsasink_component_INSTANCES) 
		return OMX_ErrorInsufficientResources;
	/* Todo: add the volume stuff */

	/* Allocate the playback handle and the hardware parameter structure */
	if ((err = snd_pcm_open (&port->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot open audio device %s (%s)\n", "default", snd_strerror (err));
		base_component_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %08x %08X in %i\n", (int)port->playback_handle, (int)&port->playback_handle, getpid());

	if (snd_pcm_hw_params_malloc(&port->hw_params) < 0) {
		DEBUG(DEB_LEV_ERR, "%s: failed allocating input port hw parameters\n", __func__);
		base_component_Panic();
	}
	else
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Got hw parameters at %08x\n", (int)port->hw_params);

	if ((err = snd_pcm_hw_params_any (port->playback_handle, port->hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot initialize hardware parameter structure (%s)\n",	snd_strerror (err));
		base_component_Panic();
	}

	/* Write in the default paramenters */
	omx_alsasink_component_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioInit, &omx_alsasink_component_Private->sPortTypesParam);
	port->AudioPCMConfigured	= 0;

	omx_alsasink_component_Private->Init = &omx_alsasink_component_Init;

	return err;
}

OMX_ERRORTYPE omx_alsasink_component_Init(stComponentType* stComponent)
{
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	omx_alsasink_component_PortType *port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];
	
	base_component_Init(stComponent);

	if (!port->AudioPCMConfigured) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
		omx_alsasink_component_SetParameter(&stComponent->omx_component, OMX_IndexParamAudioPcm, &port->omxAudioParamPcmMode);
	}

	/** Configure and prepare the ALSA handle */
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface\n");
	
	if ((err = snd_pcm_hw_params (port->playback_handle, port->hw_params)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot set parameters (%s)\n",	snd_strerror (err));
		base_component_Panic();
	}
	
	if ((err = snd_pcm_prepare (port->playback_handle)) < 0) {
		DEBUG(DEB_LEV_ERR, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
		base_component_Panic();
	}
	
};


void omx_alsasink_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer) {
	OMX_U32  frameSize;
	OMX_S32 written;
	OMX_S32 totalBuffer;
	OMX_S32 offsetBuffer;
	OMX_BOOL allDataSent;
	omx_alsasink_component_PrivateType* omx_alsasink_component_Private = stComponent->omx_component.pComponentPrivate;
	omx_alsasink_component_PortType *port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];

	/* Feed it to ALSA */
	frameSize = (port->omxAudioParamPcmMode.nChannels * port->omxAudioParamPcmMode.nBitPerSample) >> 3;
	DEBUG(DEB_LEV_FULL_SEQ, "Framesize is %u chl=%d bufSize=%d\n", 
		frameSize,port->omxAudioParamPcmMode.nChannels,inputbuffer->nFilledLen);

	if(inputbuffer->nFilledLen < frameSize)
		return;

	allDataSent = OMX_FALSE;
	totalBuffer = inputbuffer->nFilledLen/frameSize;
	offsetBuffer = 0;
	while (!allDataSent) {
		written = snd_pcm_writei(port->playback_handle, inputbuffer->pBuffer + (offsetBuffer * frameSize), totalBuffer);
		if (written < 0) {
			if(written == -EPIPE){
				DEBUG(DEB_LEV_ERR, "ALSA Underrun..\n");
				snd_pcm_prepare(port->playback_handle);
				written = 0;
			} else {
				DEBUG(DEB_LEV_ERR, "Cannot send any data to the audio device %s (%s)\n", "default", snd_strerror (written));
				base_component_Panic();
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
	inputbuffer->nFilledLen=0;
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
	omx_alsasink_component_PortType* port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];
	snd_pcm_t* playback_handle = port->playback_handle;
	snd_pcm_hw_params_t* hw_params = port->hw_params;

	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

	/** Each time we are (re)configuring the hw_params thing
	 * we need to reinitialize it, otherwise previous changes will not take effect.
	 * e.g.: changing a previously configured sampling rate does not have
	 * any effect if we are not calling this each time.
	 */
	err = snd_pcm_hw_params_any (port->playback_handle, port->hw_params);

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
				memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} else {
					return OMX_ErrorBadPortIndex;
			}
			break;
			case OMX_IndexParamAudioPcm:
			{
				unsigned int rate;
				OMX_AUDIO_PARAM_PCMMODETYPE* omxAudioParamPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
				port->AudioPCMConfigured	= 1;
				if(omxAudioParamPcmMode->nPortIndex != port->omxAudioParamPcmMode.nPortIndex){
					DEBUG(DEB_LEV_ERR, "Error setting input port index\n");
					err = OMX_ErrorBadParameter;
					break;
				}
				
				if(snd_pcm_hw_params_set_channels(playback_handle, hw_params, omxAudioParamPcmMode->nChannels)){
					DEBUG(DEB_LEV_ERR, "Error setting number of channels\n");
					return OMX_ErrorBadParameter;
				}

				if(omxAudioParamPcmMode->bInterleaved == OMX_TRUE){
					if ((err = snd_pcm_hw_params_set_access(port->playback_handle, port->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type intrleaved (%s)\n", snd_strerror (err));
						base_component_Panic();
					}
				}
				else{
					if ((err = snd_pcm_hw_params_set_access(port->playback_handle, port->hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set access type non interleaved (%s)\n", snd_strerror (err));
						base_component_Panic();
					}
				}

				rate = omxAudioParamPcmMode->nSamplingRate;
				if ((err = snd_pcm_hw_params_set_rate_near(port->playback_handle, port->hw_params, &rate, 0)) < 0) {
					DEBUG(DEB_LEV_ERR, "cannot set sample rate (%s)\n", snd_strerror (err));
					base_component_Panic();
				}
				else{
					DEBUG(DEB_LEV_PARAMS, "Set correctly sampling rate to %i\n", (int)port->omxAudioParamPcmMode.nSamplingRate);
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
							base_component_Panic();
						}
					}
					else{
						DEBUG(DEB_LEV_SIMPLE_SEQ, "ALSA OMX_IndexParamAudioPcm configured\n");
						memcpy(&port->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					}
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeALaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_A_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
						base_component_Panic();
					}
					memcpy(&port->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
					break;
				}
				else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeMULaw){
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
					if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_MU_LAW)) < 0) {
						DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n", snd_strerror (err));
						base_component_Panic();
					}
					memcpy(&port->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
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
	omx_alsasink_component_PortType *port = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX];	
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
			memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;		
		case OMX_IndexParamAudioPcm:
			if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
				port->omxAudioParamPcmMode.nPortIndex)
				return OMX_ErrorBadParameter;
			memcpy(ComponentParameterStructure, &port->omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			break;
		default:
			return base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}
