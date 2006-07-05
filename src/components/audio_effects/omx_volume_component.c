/**
 * @file src/components/audio_effects/omx_volume_component.c
 * 
 * OpenMax volume control component. This component implements a filter that 
 * controls the volume level of the audio PCM stream.
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
 *
 * 2006/05/11:  audio volume control component version 0.2
 *
 */


#include <omxcore.h>
#include <omx_volume_component.h>


void __attribute__ ((constructor)) omx_volume_component_register_template() {
	stComponentType *component;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	component = base_component_CreateComponentStruct();
	/** here you can override the base component defaults */
	component->name = "OMX.volume.component";
	component->constructor = omx_volume_component_Constructor;
	component->omx_component.SetConfig = omx_volume_component_SetConfig;
	component->omx_component.GetConfig = omx_volume_component_GetConfig;
	component->omx_component.SetParameter = omx_volume_component_SetParameter;
	component->omx_component.GetParameter = omx_volume_component_GetParameter;
	  
	// port count must be set for the base class constructor (if we call it, and we will)
	component->nports = 2;
  
 	register_template(component);
}

OMX_ERRORTYPE omx_volume_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	omx_volume_component_PrivateType* omx_volume_component_Private;
	omx_volume_component_PortType *inPort,*outPort;
	OMX_S32 i;
	
	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_volume_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}

	omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// fixme maybe the base class could use a "port factory" function pointer?	
	if (stComponent->nports && !omx_volume_component_Private->ports) {
		omx_volume_component_Private->ports = calloc(stComponent->nports,
												sizeof (base_component_PortType *));

		if (!omx_volume_component_Private->ports) return OMX_ErrorInsufficientResources;
		
		for (i=0; i < stComponent->nports; i++) {
			// this is the important thing separating this from the base class; size of the struct is for derived class port type
			// this could be refactored as a smarter factory function instead?
			omx_volume_component_Private->ports[i] = calloc(1, sizeof(omx_volume_component_PortType));
			if (!omx_volume_component_Private->ports[i]) return OMX_ErrorInsufficientResources;
		}
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = base_filter_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
		/** Domain specific section for the ports. */	
	omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_volume_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;

	omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_volume_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;

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
	
	omx_volume_component_Private->gain = 100.0f; // default gain
	omx_volume_component_Private->BufferMgmtCallback = omx_volume_component_BufferMgmtCallback;
	omx_volume_component_Private->DomainCheck	 = &omx_volume_component_DomainCheck;
	return err;
}

/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_volume_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef){
	if(pDef.eDomain!=OMX_PortDomainAudio)
		return OMX_ErrorPortsNotCompatible;
	else if(pDef.format.audio.eEncoding == OMX_AUDIO_CodingMax)
		return OMX_ErrorPortsNotCompatible;

	return OMX_ErrorNone;
}

/** This function is used to process the input buffer and provide one output buffer
 */
void omx_volume_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
	int i;
	int sampleCount = inputbuffer->nFilledLen / 2; // signed 16 bit samples assumed

	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;

	for (i = 0; i < sampleCount; i++) {
		((OMX_S16*) outputbuffer->pBuffer)[i] = (OMX_S16)
			(((OMX_S16*) inputbuffer->pBuffer)[i] * (omx_volume_component_Private->gain / 100.0f));
	}
	outputbuffer->nFilledLen = inputbuffer->nFilledLen;
	inputbuffer->nFilledLen=0;
}

OMX_ERRORTYPE omx_volume_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) {
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
		
	switch (nIndex) {
		case OMX_IndexConfigAudioVolume: {
			OMX_AUDIO_CONFIG_VOLUMETYPE* volume;
			volume = (OMX_AUDIO_CONFIG_VOLUMETYPE*) pComponentConfigStructure;
			omx_volume_component_Private->gain = volume->sVolume.nValue;
			return OMX_ErrorNone;
		}
		default: // delegate to superclass
			return base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_volume_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	//fixme
	return base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE omx_volume_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
	OMX_U32 portIndex;
	omx_volume_component_PortType *port;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
			if (err != OMX_ErrorNone)
				return err;
			memcpy(&omx_volume_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
			break;	
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (err != OMX_ErrorNone)
					return err;
			if (portIndex <= 1) {
				port= (omx_volume_component_PortType *)omx_volume_component_Private->ports[portIndex];
				memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} else {
					return OMX_ErrorBadPortIndex;
			}
			break;	
		//fixme
		default:
			return base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_volume_component_GetParameter(
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
	omx_volume_component_PortType *port;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
			memcpy(ComponentParameterStructure, &omx_volume_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;		
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		if (pAudioPortFormat->nPortIndex <= 1) {
			port= (omx_volume_component_PortType *)omx_volume_component_Private->ports[pAudioPortFormat->nPortIndex];
			memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;		
		case OMX_IndexParamAudioPcm:
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			setHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
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
			return base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}
