/**
  @file src/components/alsa/omx_alsasink_component.c

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

#include <omxcore.h>
#include <omx_alsasink_component.h>

#define MAX_COMPONENT_ALSASINK 1

/** Maximum Number of AlsaSink Instance*/
OMX_U32 noAlsasinkInstance=0;

/** The Constructor 
 */
OMX_ERRORTYPE omx_alsasink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  OMX_S32 i;
  omx_alsasink_component_PortType *pPort;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_alsasink_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  err = omx_base_sink_Constructor(openmaxStandComp,cComponentName);

  omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  
  if (omx_alsasink_component_Private->sPortTypesParam.nPorts && !omx_alsasink_component_Private->ports) {
    omx_alsasink_component_Private->ports = calloc(omx_alsasink_component_Private->sPortTypesParam.nPorts,sizeof (omx_base_PortType *));
    if (!omx_alsasink_component_Private->ports) return OMX_ErrorInsufficientResources;

    for (i=0; i < omx_alsasink_component_Private->sPortTypesParam.nPorts; i++) {
      // this is the important thing separating this from the base class; size of the struct is for derived class pPort type
      // this could be refactored as a smarter factory function instead?
      omx_alsasink_component_Private->ports[i] = calloc(1, sizeof(omx_alsasink_component_PortType));
      if (!omx_alsasink_component_Private->ports[i]) return OMX_ErrorInsufficientResources;
    }
  }
  else 
  DEBUG(DEB_LEV_ERR, "In %s Not allocated ports\n", __func__);
	 
  omx_alsasink_component_Private->PortConstructor(openmaxStandComp,&omx_alsasink_component_Private->ports[0],0, OMX_TRUE);

  pPort = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  // set the pPort params, now that the ports exist	
  /** Domain specific section for the ports. */	
  pPort->sPortParam.eDomain = OMX_PortDomainAudio;
  pPort->sPortParam.format.audio.pNativeRender = 0;
  pPort->sPortParam.format.audio.cMIMEType = "raw";
  pPort->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
  pPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  omx_alsasink_component_Private->BufferMgmtCallback = omx_alsasink_component_BufferMgmtCallback;
  omx_alsasink_component_Private->destructor = omx_alsasink_component_Destructor;

  setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  pPort->sAudioParam.nPortIndex = 0;
  pPort->sAudioParam.nIndex = 0;
  pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;

  /* OMX_AUDIO_PARAM_PCMMODETYPE */
  setHeader(&pPort->omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  pPort->omxAudioParamPcmMode.nPortIndex = 0;
  pPort->omxAudioParamPcmMode.nChannels = 2;
  pPort->omxAudioParamPcmMode.eNumData = OMX_NumericalDataSigned;
  pPort->omxAudioParamPcmMode.eEndian = OMX_EndianLittle;
  pPort->omxAudioParamPcmMode.bInterleaved = OMX_TRUE;
  pPort->omxAudioParamPcmMode.nBitPerSample = 16;
  pPort->omxAudioParamPcmMode.nSamplingRate = 44100;
  pPort->omxAudioParamPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  pPort->omxAudioParamPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

  noAlsasinkInstance++;
  if(noAlsasinkInstance > MAX_COMPONENT_ALSASINK) {
    return OMX_ErrorInsufficientResources;
  }
	
  /* Allocate the playback handle and the hardware parameter structure */
  if ((err = snd_pcm_open (&pPort->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    DEBUG(DEB_LEV_ERR, "cannot open audio device %s (%s)\n", "default", snd_strerror (err));
    return OMX_ErrorHardware;
  }
  else
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %08x %08X in %i\n", (int)pPort->playback_handle, (int)&pPort->playback_handle, getpid());

  if (snd_pcm_hw_params_malloc(&pPort->hw_params) < 0) {
    DEBUG(DEB_LEV_ERR, "%s: failed allocating input pPort hw parameters\n", __func__);
    return OMX_ErrorHardware;
  }
  else
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Got hw parameters at %08x\n", (int)pPort->hw_params);

  if ((err = snd_pcm_hw_params_any (pPort->playback_handle, pPort->hw_params)) < 0) {
    DEBUG(DEB_LEV_ERR, "cannot initialize hardware parameter structure (%s)\n",	snd_strerror (err));
    return OMX_ErrorHardware;
  }

  openmaxStandComp->SetParameter  = omx_alsasink_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_alsasink_component_GetParameter;

  /* Write in the default paramenters */
  pPort->AudioPCMConfigured	= 0;

  if (!pPort->AudioPCMConfigured) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
    err = omx_alsasink_component_SetParameter(openmaxStandComp, OMX_IndexParamAudioPcm, &pPort->omxAudioParamPcmMode);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "In %s Error %08x\n",__func__,err);
    }
  }

  return err;
}

/** The Destructor 
 */
OMX_ERRORTYPE omx_alsasink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_alsasink_component_PortType* pPort = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  if(pPort->hw_params) {
    snd_pcm_hw_params_free (pPort->hw_params);
  }
  if(pPort->playback_handle) {
    snd_pcm_close(pPort->playback_handle);
  }

  noAlsasinkInstance--;

  return omx_base_sink_Destructor(openmaxStandComp);

}

/** 
 * This function plays the input buffer. When fully consumed it returns.
 */
void omx_alsasink_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer) {
  OMX_U32  frameSize;
  OMX_S32 written;
  OMX_S32 totalBuffer;
  OMX_S32 offsetBuffer;
  OMX_BOOL allDataSent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_alsasink_component_PortType *pPort = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  /* Feed it to ALSA */
  frameSize = (pPort->omxAudioParamPcmMode.nChannels * pPort->omxAudioParamPcmMode.nBitPerSample) >> 3;
  DEBUG(DEB_LEV_FULL_SEQ, "Framesize is %u chl=%d bufSize=%d\n", 
  (int)frameSize, (int)pPort->omxAudioParamPcmMode.nChannels, (int)inputbuffer->nFilledLen);

  if(inputbuffer->nFilledLen < frameSize){
    DEBUG(DEB_LEV_ERR, "Ouch!! In %s input buffer filled len(%d) less than frame size(%d)\n",__func__, (int)inputbuffer->nFilledLen, (int)frameSize);
    return;
  }

  allDataSent = OMX_FALSE;
  totalBuffer = inputbuffer->nFilledLen/frameSize;
  offsetBuffer = 0;
  while (!allDataSent) {
    written = snd_pcm_writei(pPort->playback_handle, inputbuffer->pBuffer + (offsetBuffer * frameSize), totalBuffer);
    if (written < 0) {
      if(written == -EPIPE){
        DEBUG(DEB_LEV_ERR, "ALSA Underrun..\n");
        snd_pcm_prepare(pPort->playback_handle);
        written = 0;
      } else {
        DEBUG(DEB_LEV_ERR, "Cannot send any data to the audio device %s (%s)\n", "default", snd_strerror (written));
        DEBUG(DEB_LEV_ERR, "IB FilledLen=%d,totalBuffer=%d,frame size=%d,offset=%d\n", 
        (int)inputbuffer->nFilledLen, (int)totalBuffer, (int)frameSize, (int)offsetBuffer);
        break;
        return;
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

OMX_ERRORTYPE omx_alsasink_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_alsasink_component_PortType* pPort = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  snd_pcm_t* playback_handle = pPort->playback_handle;
  snd_pcm_hw_params_t* hw_params = pPort->hw_params;

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

  /** Each time we are (re)configuring the hw_params thing
  * we need to reinitialize it, otherwise previous changes will not take effect.
  * e.g.: changing a previously configured sampling rate does not have
  * any effect if we are not calling this each time.
  */
  err = snd_pcm_hw_params_any (pPort->playback_handle, pPort->hw_params);

  switch(nParamIndex) {
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (portIndex < 1) {
      memcpy(&pPort->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioPcm:
    {
      unsigned int rate;
      OMX_AUDIO_PARAM_PCMMODETYPE* omxAudioParamPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;

      portIndex = omxAudioParamPcmMode->nPortIndex;
      /*Check Structure Header and verify component state*/
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
        break;
      } 

      pPort->AudioPCMConfigured	= 1;
      if(omxAudioParamPcmMode->nPortIndex != pPort->omxAudioParamPcmMode.nPortIndex){
        DEBUG(DEB_LEV_ERR, "Error setting input pPort index\n");
        err = OMX_ErrorBadParameter;
        break;
      }

      if(snd_pcm_hw_params_set_channels(playback_handle, hw_params, omxAudioParamPcmMode->nChannels)){
        DEBUG(DEB_LEV_ERR, "Error setting number of channels\n");
        return OMX_ErrorBadParameter;
      }

      if(omxAudioParamPcmMode->bInterleaved == OMX_TRUE){
        if ((err = snd_pcm_hw_params_set_access(pPort->playback_handle, pPort->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set access type intrleaved (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
      }
      else{
        if ((err = snd_pcm_hw_params_set_access(pPort->playback_handle, pPort->hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set access type non interleaved (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
      }
      rate = omxAudioParamPcmMode->nSamplingRate;
      if ((err = snd_pcm_hw_params_set_rate_near(pPort->playback_handle, pPort->hw_params, &rate, 0)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot set sample rate (%s)\n", snd_strerror (err));
        return OMX_ErrorHardware;
      }
      else{
        DEBUG(DEB_LEV_PARAMS, "Set correctly sampling rate to %i\n", (int)pPort->omxAudioParamPcmMode.nSamplingRate);
      }

      if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeLinear){
        snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;
        DEBUG(DEB_LEV_PARAMS, "Bit per sample %i, signed=%i, little endian=%i\n",
        (int)omxAudioParamPcmMode->nBitPerSample,
        (int)omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned,
        (int)omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle);

        switch(omxAudioParamPcmMode->nBitPerSample){
        case 8:
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned) {
            snd_pcm_format = SND_PCM_FORMAT_S8;
          } else {
            snd_pcm_format = SND_PCM_FORMAT_U8;
          }
          break;
        case 16:
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
            if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S16_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S16_BE;
            }
          }
        if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
          if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle){
            snd_pcm_format = SND_PCM_FORMAT_U16_LE;
          } else {
            snd_pcm_format = SND_PCM_FORMAT_U16_BE;
          }
        }
        break;
        case 24:
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
            if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S24_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S24_BE;
            }
          }
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
            if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_U24_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_U24_BE;
            }
          }
          break;

        case 32:
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataSigned){
            if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S32_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S32_BE;
            }
          }
          if(omxAudioParamPcmMode->eNumData == OMX_NumericalDataUnsigned){
            if(omxAudioParamPcmMode->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_U32_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_U32_BE;
            }
          }
          break;
        default:
          err = OMX_ErrorBadParameter;
          break;
        }

        if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN){
          if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
            DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
            return OMX_ErrorHardware;
          }
          memcpy(&pPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        } else{
          DEBUG(DEB_LEV_SIMPLE_SEQ, "ALSA OMX_IndexParamAudioPcm configured\n");
          memcpy(&pPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
      }
      else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeALaw){
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
        if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_A_LAW)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",	snd_strerror (err));
          return OMX_ErrorHardware;
        }
        memcpy(&pPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      }
      else if(omxAudioParamPcmMode->ePCMMode == OMX_AUDIO_PCMModeMULaw){
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
        if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_MU_LAW)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
        memcpy(&pPort->omxAudioParamPcmMode, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      }
      /** Configure and prepare the ALSA handle */
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface\n");
      if ((err = snd_pcm_hw_params (pPort->playback_handle, pPort->hw_params)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot set parameters (%s)\n",	snd_strerror (err));
        return OMX_ErrorHardware;
      }

      if ((err = snd_pcm_prepare (pPort->playback_handle)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
        return OMX_ErrorHardware;
      }
    }
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, pAudioMp3->nPortIndex, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_alsasink_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_alsasink_component_PortType *pPort = (omx_alsasink_component_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];	
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex < 1) {
      memcpy(pAudioPortFormat, &pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;		
  case OMX_IndexParamAudioPcm:
    if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
      pPort->omxAudioParamPcmMode.nPortIndex) {
      return OMX_ErrorBadParameter;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &pPort->omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;
  default: /*Call the base component function*/
  return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}
