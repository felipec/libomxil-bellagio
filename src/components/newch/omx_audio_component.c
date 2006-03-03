/**
	@file src/components/audio_component/omx_audio_component.c
	
	OpenMax audio_component component. This component does not perform any multimedia
	processing.	It is used as a audio_component for new components development.
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

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
	
	2006/02/08:  OpenMAX audio_component component version 0.1

*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Audio.h>

#include "omxcore.h"
#include "omx_audio_component.h"

#include "tsemaphore.h"
#include "queue.h"

/*********************
 *
 * Component template
 *
 **********************/

/** 
 * This is the stComponent template from which all
 * other stComponent instances are factored by the core.
 * Note that it has been necessary to wrap the OMX component
 * with a st_component because of design deficiencies.
 */
stComponentType* audio_component_CreateComponentStruct() {
	stComponentType *audio_component = (stComponentType *) malloc(sizeof(stComponentType)); 
	if (audio_component) {
	  audio_component->name = "OMX.st.audio";
	  audio_component->state = OMX_StateLoaded;
	  audio_component->callbacks = NULL;
	  audio_component->callbackData = NULL;
	  audio_component->nports = 2;
		audio_component->coreDescriptor = NULL;
	  audio_component->constructor = audio_component_Constructor;
	  audio_component->destructor = audio_component_Destructor;
	  audio_component->messageHandler = audio_component_MessageHandler;
		audio_component->omx_component.nSize = sizeof(OMX_COMPONENTTYPE);
	  audio_component->omx_component.pComponentPrivate = NULL;
	  audio_component->omx_component.pApplicationPrivate = NULL;
	  audio_component->omx_component.GetComponentVersion = audio_component_GetComponentVersion;
	  audio_component->omx_component.SendCommand = audio_component_SendCommand;
	  audio_component->omx_component.GetParameter = audio_component_GetParameter;
	  audio_component->omx_component.SetParameter = audio_component_SetParameter;
	  audio_component->omx_component.GetConfig = audio_component_GetConfig;
	  audio_component->omx_component.SetConfig = audio_component_SetConfig;
	  audio_component->omx_component.GetExtensionIndex = audio_component_GetExtensionIndex;
	  audio_component->omx_component.GetState = audio_component_GetState;
	  audio_component->omx_component.ComponentTunnelRequest = audio_component_ComponentTunnelRequest;
	  audio_component->omx_component.UseBuffer = audio_component_UseBuffer;
	  audio_component->omx_component.AllocateBuffer = audio_component_AllocateBuffer;
	  audio_component->omx_component.FreeBuffer = audio_component_FreeBuffer;
	  audio_component->omx_component.EmptyThisBuffer = audio_component_EmptyThisBuffer;
	  audio_component->omx_component.FillThisBuffer = audio_component_FillThisBuffer;
	  audio_component->omx_component.SetCallbacks = audio_component_SetCallbacks;
	  audio_component->omx_component.ComponentDeInit = audio_component_ComponentDeInit;
	  audio_component->omx_component.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
	  audio_component->omx_component.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
	  audio_component->omx_component.nVersion.s.nRevision = SPECREVISION;
	  audio_component->omx_component.nVersion.s.nStep = SPECSTEP;
	}
	return audio_component;	
}

/** 
 * Component template registration function
 *
 * This is the component entry point which is called at library
 * initialization for each OMX component. It provides the OMX core with
 * the component template.
 */
void __attribute__ ((constructor)) audio_component_register_template()
{
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);
  register_template(audio_component_CreateComponentStruct());
}

/** 
 * This function takes care of constructing the instance of the component.
 * It is executed upon a getHandle() call.
 * For the audio_component component, the following is done:
 *
 * 1) Allocates the threadDescriptor structure
 * 2) Spawns the event manager thread
 * 3) Allocated audio_component_PrivateType private structure
 * 4) Fill all the generic parameters, and some of the
 *    specific as an example
 * \param stComponent the ST component to be initialized
 */
OMX_ERRORTYPE audio_component_Constructor(stComponentType* stComponent)
{
//	audio_component_PrivateType* audio_component_Private;
	audio_component_PrivateType* audio_component_Private = (audio_component_Privatetype*)stComponent->omx_component->pComponentPrivate;
	pthread_t testThread;
	int i;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noRefInstance);

	/** Allocate and fill in audio_component private structures
	 * These include output port buffer queue and flow control semaphore
	 */
	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(audio_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	audio_component_Private = stComponent->omx_component.pComponentPrivate;
	/** Default parameters setting */
	
	// create ports, but only if the subclass hasn't done it

	base_component_Constructor(stComponent)
	DEBUG(DEB_LEV_FULL_SEQ,"Returning from %s\n",__func__);
	/** Two meta-states used for the implementation 
	* of the transition mechanism loaded->idle and idle->loaded 
	*/

	return OMX_ErrorNone;
}

/** This function is executed when a loaded to idle transition occurs.
 * It is responsible of allocating all necessary resources for being
 * ready to process data.
 * For audio_component component, the following is done:
 * 1) Put the component in IDLE state
 *	2) Spawn the buffer management thread.
 * \param stComponent the ST component to startup
 */
OMX_ERRORTYPE audio_component_Init(stComponentType* stComponent)
{

	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);


	base_component_Init(stComponent);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s\n", __func__);

	return OMX_ErrorNone;
}

/** This function is called upon a transition to the idle state.
 * The two buffer management threads are stopped and their message
 * queues destroyed.
 */
OMX_ERRORTYPE audio_component_Deinit(stComponentType* stComponent)
{
	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 ret;
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s\n", __func__);
	
	base_component_Deinit(stComponent);

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);

	return OMX_ErrorNone;
}

/** This function is called by the omx core when the component 
	* is disposed by the IL client with a call to FreeHandle().
	* \param stComponent the ST component to be disposed
	*/
OMX_ERRORTYPE audio_component_Destructor(stComponentType* stComponent)
{
	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_BOOL exit_thread=OMX_FALSE,cmdFlag=OMX_FALSE;
	OMX_U32 ret;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s instance=%d\n", __func__,noRefInstance);
	if (audio_component_Private->bIsInit != OMX_FALSE) {
		audio_component_Deinit(stComponent);
	} 

	pthread_mutex_lock(&audio_component_Private->exit_mutex);
	exit_thread = audio_component_Private->bExit_buffer_thread;
	pthread_mutex_unlock(&audio_component_Private->exit_mutex);
	if(exit_thread == OMX_TRUE) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for condition...\n",__func__);
		pthread_cond_wait(&audio_component_Private->exit_condition,&audio_component_Private->exit_mutex);
	}

	pthread_mutex_lock(&audio_component_Private->cmd_mutex);
	cmdFlag=audio_component_Private->bCmdUnderProcess;
	if(cmdFlag==OMX_TRUE) {
		audio_component_Private->bWaitingForCmdToFinish=OMX_TRUE;
		pthread_mutex_unlock(&audio_component_Private->cmd_mutex);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish ...\n",__func__);
		tsem_down(audio_component_Private->pCmdSem);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting %s for command to finish over..cmutex=%x.\n",__func__,&audio_component_Private->cmd_mutex);

		pthread_mutex_lock(&audio_component_Private->cmd_mutex);
		audio_component_Private->bWaitingForCmdToFinish=OMX_FALSE;
		pthread_mutex_unlock(&audio_component_Private->cmd_mutex);
	}
	else {
		pthread_mutex_unlock(&audio_component_Private->cmd_mutex);
	}
	

	
	free(stComponent->omx_component.pComponentPrivate);

	DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning from %s \n",__func__);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audio_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			memcpy(ComponentParameterStructure, &audio_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		if (pAudioPortFormat->nPortIndex == 0) {
			memcpy(pAudioPortFormat, &audio_component_Private->inputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else if (pAudioPortFormat->nPortIndex == 1) {
			memcpy(pAudioPortFormat, &audio_component_Private->outputPort.sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		} else {
				return OMX_ErrorBadPortIndex;
		}
		break;
/*
		case OMX_IndexParamAudioPcm:
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			setHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			if (pAudioPcmMode->nPortIndex != 1) {
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
		case OMX_IndexParamAudioMp3:
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			setHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			if (pAudioMp3->nPortIndex != 0) {
				return OMX_ErrorBadPortIndex;
			}
			pAudioMp3->nChannels = 2;
			pAudioMp3->nBitRate = 0;
			pAudioMp3->nSampleRate = 0;
			pAudioMp3->nAudioBandWidth = 0;
			pAudioMp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
		break;
*/
		default:
			return base_component_GetParameter( hComponent,nParamIndex, ComponentParameterStructure);
		break;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audio_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_PARAM_BUFFERSUPPLIERTYPE *pBufSupply;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			err = OMX_ErrorBadParameter;
		break;
		case OMX_IndexParamAudioPortFormat:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			if ((err = checkHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
				return err;
			}
		break;
/*
		case OMX_IndexParamAudioPcm:
			if (stComponent->state != OMX_StateLoaded && stComponent->state != OMX_StateWaitForResources) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x lineno=%d\n",__func__,stComponent->state,__LINE__);
				return OMX_ErrorIncorrectStateOperation;
			}
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			if((err = checkHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) {
				return err;
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
*/
		default:
			return base_component_SetParameter( hComponent,nParamIndex, ComponentParameterStructure);
		break;
	}
	
	/* If we are not asked two ports give error */
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "   Error during %s = %i\n", __func__, err);
	}
	return err;
}

OMX_ERRORTYPE audio_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audio_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audio_component_GetExtensionIndex(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_STRING cParameterName,
	OMX_OUT OMX_INDEXTYPE* pIndexType) {
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE audio_component_GetState(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STATETYPE* pState)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	*pState = stComponent->state;
	return OMX_ErrorNone;
}


OMX_ERROrtype AUDIO_COMPONENT_componentDeinit(
	OMX_IN  OMX_HANDLETYPE hComponent)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Tearing down component...\n");

	/* Take care of tearing down resources if not in loaded state */
	if(stComponent->state != OMX_StateLoaded)
		audio_component_Deinit(stComponent);

	return OMX_ErrorNone;
}


OMX_ERRORTYPE audio_component_ComponentTunnelRequest(
	OMX_IN  OMX_HANDLETYPE hComp,
	OMX_IN  OMX_U32 nPort,
	OMX_IN  OMX_HANDLETYPE hTunneledComp,
	OMX_IN  OMX_U32 nTunneledPort,
	OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE err=OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE param;
	OMX_PARAM_BUFFERSUPPLIERTYPE pSupplier;

	stComponentType* stComponent = (stComponentType*)hComp;
	
	audio_component_PrivateType* audio_component_Private = stComponent->omx_component.pComponentPrivate;

	if (pTunnelSetup == NULL || hTunneledComp == 0) {
        /* cancel previous tunnel */
		if (nPort == 0) {
       		audio_component_Private->inputPort.hTunneledComponent = 0;
			audio_component_Private->inputPort.nTunneledPort = 0;
			audio_component_Private->inputPort.nTunnelFlags = 0;
			audio_component_Private->inputPort.eBufferSupplier=OMX_BufferSupplyUnspecified;
		}
		else if (nPort == 1) {
			audio_component_Private->outputPort.hTunneledComponent = 0;
			audio_component_Private->outputPort.nTunneledPort = 0;
			audio_component_Private->outputPort.nTunnelFlags = 0;
			audio_component_Private->outputPort.eBufferSupplier=OMX_BufferSupplyUnspecified;
		}
		else {
			return OMX_ErrorBadPortIndex;
		}
		return err;
    }

	if (nPort == 0) {
		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		/// \todo insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		audio_component_Private->inputPort.nNumTunnelBuffer=param.nBufferCountMin;

		DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled port domain=%d\n",param.eDomain);
		if(param.eDomain!=OMX_PortDomainAudio)
			return OMX_ErrorPortsNotCompatible;
		else if(param.format.audio.eEncoding == OMX_AUDIO_CodingMax)
			return OMX_ErrorPortsNotCompatible;

		/* Get Buffer Supplier type of the Tunnelled Component*/
		pSupplier.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);

		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled error=%d\n",err);
			return OMX_ErrorPortsNotCompatible;
		}
		else {
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Tunneled Port eBufferSupplier=%x\n",pSupplier.eBufferSupplier);
		}

		// store the current callbacks, if defined
		audio_component_Private->inputPort.hTunneledComponent = hTunneledComp;
		audio_component_Private->inputPort.nTunneledPort = nTunneledPort;
		audio_component_Private->inputPort.nTunnelFlags = 0;
		// Negotiation
		if (pTunnelSetup->nTunnelFlags & OMX_PORTTUNNELFLAG_READONLY) {
			// the buffer provider MUST be the output port provider
			pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
			audio_component_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;	
			audio_component_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
		} else {
			if (pTunnelSetup->eSupplier == OMX_BufferSupplyInput) {
				audio_component_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				audio_component_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			} else if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified) {
				pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
				audio_component_Private->inputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				audio_component_Private->inputPort.eBufferSupplier=OMX_BufferSupplyInput;
			}
		}
		audio_component_Private->inputPort.nTunnelFlags |= TUNNEL_ESTABLISHED;
	} else if (nPort == 1) {
		// output port
		// all the consistency checks are under other component responsibility
		// check if it is a tunnel deletion request
		if (hTunneledComp == NULL) {
			audio_component_Private->outputPort.hTunneledComponent = NULL;
			audio_component_Private->outputPort.nTunneledPort = 0;
			audio_component_Private->outputPort.nTunnelFlags = 0;
			return OMX_ErrorNone;
		}

		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		/// \todo insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		audio_component_Private->outputPort.nNumTunnelBuffer=param.nBufferCountMin;

		audio_component_Private->outputPort.hTunneledComponent = hTunneledComp;
		audio_component_Private->outputPort.nTunneledPort = nTunneledPort;
		pTunnelSetup->eSupplier = OMX_BufferSupplyOutput;
		audio_component_Private->outputPort.nTunnelFlags |= TUNNEL_IS_SUPPLIER;
		audio_component_Private->outputPort.nTunnelFlags |= TUNNEL_ESTABLISHED;

		audio_component_Private->outputPort.eBufferSupplier=OMX_BufferSupplyOutput;
	} else {
		return OMX_ErrorBadPortIndex;
	}
	return OMX_ErrorNone;
}

/** The panic function that exits from the application. This function is only for debug purposes and should be removed in the next releases */
void audio_component_Panic() {
	exit(EXIT_FAILURE);
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

/* File EOF */

