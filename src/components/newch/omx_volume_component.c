/**
 * @file src/components/newch/omx_volume_component.c
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
#include <omx_volume_component.h>


void __attribute__ ((constructor)) omx_volume_component_register_template() {
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	stComponentType *component = base_component_CreateComponentStruct();
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

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_volume_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = omx_twoport_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
		/** Domain specific section for the ports. */	
	omx_volume_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_volume_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_volume_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;

	omx_volume_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_volume_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_volume_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	
	omx_volume_component_Private->gain = 100.0f; // default gain
	omx_volume_component_Private->BufferMgmtCallback = omx_volume_component_BufferMgmtCallback;
	return err;
}

void omx_volume_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
	int i;
	int sampleCount = inputbuffer->nFilledLen / 2; // signed 16 bit samples assumed

	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;

	for (i = 0; i < sampleCount; i++) {
		((OMX_S16*) outputbuffer->pBuffer)[i] = (OMX_S16)
			(((OMX_S16*) inputbuffer->pBuffer)[i] * (omx_volume_component_Private->gain / 100.0f));
	}
	outputbuffer->nFilledLen = inputbuffer->nFilledLen;
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

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
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
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_volume_component_PrivateType* omx_volume_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			memcpy(ComponentParameterStructure, &omx_volume_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;		
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		if (pAudioPortFormat->nPortIndex <= 1) {
			memcpy(pAudioPortFormat, &omx_volume_component_Private->ports[pAudioPortFormat->nPortIndex]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
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
