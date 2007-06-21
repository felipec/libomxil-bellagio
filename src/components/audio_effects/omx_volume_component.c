/**
  @file src/components/audio_effects/omx_volume_component.c

  OpenMax volume control component. This component implements a filter that 
  controls the volume level of the audio PCM stream.

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
#include <omx_volume_component.h>
#include<OMX_Audio.h>

/* gain value */
#define GAIN_VALUE 100.0f

/* Max allowable volume component instance */
#define MAX_COMPONENT_VOLUME 1

/** Maximum Number of Volume Component Instance*/
OMX_U32 noVolumeCompInstance = 0;


OMX_ERRORTYPE omx_volume_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_volume_component_PrivateType* omx_volume_component_Private;
  omx_volume_component_PortType *inPort, *outPort;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_volume_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }
  omx_volume_component_Private = openmaxStandComp->pComponentPrivate;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?	
    */
  err = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */
  omx_volume_component_Private = (omx_volume_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  strcpy(omx_volume_component_Private->name, cComponentName);

  /** Allocate Ports and Call base port constructor. */	
  if (omx_volume_component_Private->sPortTypesParam.nPorts && !omx_volume_component_Private->ports) {
    omx_volume_component_Private->ports = calloc(omx_volume_component_Private->sPortTypesParam.nPorts, sizeof (omx_base_PortType *));
    if (!omx_volume_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_volume_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; 
        * size of the struct is for derived class port type
        * this could be refactored as a smarter factory function instead?
        */
      omx_volume_component_Private->ports[i] = calloc(1, sizeof(omx_volume_component_PortType));
      if (!omx_volume_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_port_Constructor(openmaxStandComp, &omx_volume_component_Private->ports[0], 0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp, &omx_volume_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports. */	
  omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
  omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
 
  omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
  omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  inPort = (omx_volume_component_PortType *) omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  outPort = (omx_volume_component_PortType *) omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  setHeader(&inPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  inPort->sAudioParam.nPortIndex = 0;
  inPort->sAudioParam.nIndex = 0;
  inPort->sAudioParam.eEncoding = 0;

  setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  outPort->sAudioParam.nPortIndex = 1;
  outPort->sAudioParam.nIndex = 0;
  outPort->sAudioParam.eEncoding = 0;

  omx_volume_component_Private->gain = GAIN_VALUE; //100.0f; // default gain
  omx_volume_component_Private->destructor = omx_volume_component_Destructor;
  openmaxStandComp->SetParameter = omx_volume_component_SetParameter;
  openmaxStandComp->GetParameter = omx_volume_component_GetParameter;
  openmaxStandComp->GetConfig = omx_volume_component_GetConfig;
  openmaxStandComp->SetConfig = omx_volume_component_SetConfig;
  omx_volume_component_Private->BufferMgmtCallback = omx_volume_component_BufferMgmtCallback;

  noVolumeCompInstance++;
  if(noVolumeCompInstance > MAX_COMPONENT_VOLUME) {
    return OMX_ErrorInsufficientResources;
  }

  return err;
}


/** The destructor
  */
OMX_ERRORTYPE omx_volume_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  int i;
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;

  /** frees the port structures */
  if (omx_volume_component_Private->sPortTypesParam.nPorts && omx_volume_component_Private->ports) {
    for (i=0; i < omx_volume_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_volume_component_Private->ports[i]) {
        base_port_Destructor(omx_volume_component_Private->ports[i]);
      }
    }
    free(omx_volume_component_Private->ports);
    omx_volume_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audiodecoder component is called\n");
  omx_base_filter_Destructor(openmaxStandComp);
  noVolumeCompInstance--;

  return OMX_ErrorNone;
}

/** This function is used to process the input buffer and provide one output buffer
  */
void omx_volume_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {
  int i;
  int sampleCount = pInputBuffer->nFilledLen / 2; // signed 16 bit samples assumed
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;

  if(omx_volume_component_Private->gain != GAIN_VALUE) {
    for (i = 0; i < sampleCount; i++) {
      ((OMX_S16*) pOutputBuffer->pBuffer)[i] = (OMX_S16)
              (((OMX_S16*) pInputBuffer->pBuffer)[i] * (omx_volume_component_Private->gain / 100.0f));
    }
  } else {
    memcpy(pOutputBuffer->pBuffer,pInputBuffer->pBuffer,pInputBuffer->nFilledLen);
  }
  pOutputBuffer->nFilledLen = pInputBuffer->nFilledLen;
  pInputBuffer->nFilledLen=0;
}

/** setting configurations */
OMX_ERRORTYPE omx_volume_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_AUDIO_CONFIG_VOLUMETYPE* pVolume;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;

  switch (nIndex) {
    case OMX_IndexConfigAudioVolume : 
      pVolume = (OMX_AUDIO_CONFIG_VOLUMETYPE*) pComponentConfigStructure;
      if(pVolume->sVolume.nValue > 100) {
        return OMX_ErrorBadParameter;
      }
      omx_volume_component_Private->gain = pVolume->sVolume.nValue;
      return OMX_ErrorNone;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_volume_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure) {
  OMX_AUDIO_CONFIG_VOLUMETYPE* pVolume;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;
	
  switch (nIndex) {
    case OMX_IndexConfigAudioVolume : 
      pVolume = (OMX_AUDIO_CONFIG_VOLUMETYPE*) pComponentConfigStructure;
      setHeader(pVolume,sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE));
      pVolume->sVolume.nValue = omx_volume_component_Private->gain;
      pVolume->sVolume.nMin = 0;
      pVolume->sVolume.nMax = 100;
      pVolume->bLinear = OMX_TRUE;
      return OMX_ErrorNone;
    default :
      return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_volume_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_U32 portIndex;
  omx_volume_component_PortType *port;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamAudioPortFormat:
      pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      portIndex = pAudioPortFormat->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
        break;
      }
      if (portIndex <= 1) {
        port= (omx_volume_component_PortType *)omx_volume_component_Private->ports[portIndex];
        memcpy(&port->sAudioParam, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;	
    default:
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_volume_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_volume_component_PortType *port;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_volume_component_PrivateType* omx_volume_component_Private = openmaxStandComp->pComponentPrivate;
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
      memcpy(ComponentParameterStructure, &omx_volume_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
      break;		
    case OMX_IndexParamAudioPortFormat:
      pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
        break;
      }
      if (pAudioPortFormat->nPortIndex <= 1) {
        port= (omx_volume_component_PortType *)omx_volume_component_Private->ports[pAudioPortFormat->nPortIndex];
        memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
    break;		
    case OMX_IndexParamAudioPcm:
      pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) { 
        break;
      }

      if (pAudioPcmMode->nPortIndex > 1) {
        return OMX_ErrorBadPortIndex;
      }
      pAudioPcmMode->nChannels = 2;
      pAudioPcmMode->eNumData = OMX_NumericalDataSigned;
      pAudioPcmMode->eEndian = OMX_EndianBig;
      pAudioPcmMode->bInterleaved = OMX_TRUE;
      pAudioPcmMode->nBitPerSample = 16;
      pAudioPcmMode->nSamplingRate = 0;
      pAudioPcmMode->ePCMMode = OMX_AUDIO_PCMModeLinear;
      break;
    default:
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}
