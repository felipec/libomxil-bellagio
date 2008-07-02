/**
 * @file src/components/components_symbian/outputstream/src/omx_symbianoutputstreamsink_component.c
 * 
 * OpenMAX Symbian output stream sink component. 
 * This component is an audio sink that uses Symbian MMF output stream class.
 * 
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "omxcore.h"
#include "omx_symbianoutputstreamsink_component.h"
#include "omx_symbian_output_stream_wrapper.h"

OMX_ERRORTYPE 
omx_symbianoutputstreamsink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) 
{
    OMX_ERRORTYPE err = OMX_ErrorNone;	
    OMX_S32 i;
    omx_symbianoutputstreamsink_component_PortType *pPort;
    omx_symbianoutputstreamsink_component_PrivateType* omx_symbianoutputstreamsink_component_Private;

    if (!openmaxStandComp->pComponentPrivate) 
    {
        openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_symbianoutputstreamsink_component_PrivateType));
        if(openmaxStandComp->pComponentPrivate==NULL)
        {
            return OMX_ErrorInsufficientResources;
        }
    }

    err = omx_base_sink_Constructor(openmaxStandComp,cComponentName); 

    omx_symbianoutputstreamsink_component_Private = openmaxStandComp->pComponentPrivate;
  
    omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 0;
    omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 1;

    if (omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts && !omx_symbianoutputstreamsink_component_Private->ports) 
    {
        omx_symbianoutputstreamsink_component_Private->ports = calloc(omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts,sizeof (omx_base_PortType *));

        if (!omx_symbianoutputstreamsink_component_Private->ports) 
        {
            return OMX_ErrorInsufficientResources;
        }
        
        for (i=0; i < omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) 
        {
            omx_symbianoutputstreamsink_component_Private->ports[i] = calloc(1, sizeof(omx_symbianoutputstreamsink_component_PortType));
            if (!omx_symbianoutputstreamsink_component_Private->ports[i]) 
            {
                return OMX_ErrorInsufficientResources;
            }
        }
    }
    else 
    {
        DEBUG(DEB_LEV_ERR, "In %s Not allocated ports\n", __func__);
    }
     
    base_port_Constructor(openmaxStandComp,&omx_symbianoutputstreamsink_component_Private->ports[0],0, OMX_TRUE);

    pPort = (omx_symbianoutputstreamsink_component_PortType *) omx_symbianoutputstreamsink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

    // set the pPort params, now that the ports exist	
    /** Domain specific section for the ports. */	
    pPort->sPortParam.eDir = OMX_DirInput;
    pPort->sPortParam.nBufferCountActual = 2;
    pPort->sPortParam.nBufferCountMin = 2;
    pPort->sPortParam.bEnabled = OMX_TRUE;
    pPort->sPortParam.bPopulated = OMX_FALSE;
    pPort->sPortParam.eDomain = OMX_PortDomainAudio;
    pPort->sPortParam.format.audio.pNativeRender = 0;
    pPort->sPortParam.format.audio.cMIMEType = "raw";
    pPort->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
    /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
    pPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

    omx_symbianoutputstreamsink_component_Private->BufferMgmtCallback = omx_symbianoutputstreamsink_component_BufferMgmtCallback;
    
    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = 0;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;

    /* OMX_AUDIO_PARAM_PCMMODETYPE */
    pPort->omxAudioParamPcmMode.nPortIndex = 0;
    pPort->omxAudioParamPcmMode.nChannels = 2;
    pPort->omxAudioParamPcmMode.eNumData = OMX_NumericalDataSigned;
    pPort->omxAudioParamPcmMode.eEndian = OMX_EndianLittle;
    pPort->omxAudioParamPcmMode.bInterleaved = OMX_TRUE;
    pPort->omxAudioParamPcmMode.nBitPerSample = 16;
    pPort->omxAudioParamPcmMode.nSamplingRate = 44100;
    pPort->omxAudioParamPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
    pPort->omxAudioParamPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

    omx_symbianoutputstreamsink_component_Private->destructor = omx_symbianoutputstreamsink_component_Destructor;

    /* Allocate the playback handle and the hardware parameter structure */
    if (create_output_stream(&pPort->output_handle) < 0) 
    {
        DEBUG(DEB_LEV_ERR, "cannot create audio device\n");
        return OMX_ErrorHardware;
    }
    else
    {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %08x %08X\n", (int)pPort->output_handle, (int)&pPort->output_handle);
    }

    DEBUG(DEB_LEV_PARAMS, "output stream component open pointer is %p\n", pPort->output_handle);
  
    openmaxStandComp->SetParameter  = omx_symbianoutputstreamsink_component_SetParameter;
    openmaxStandComp->GetParameter  = omx_symbianoutputstreamsink_component_GetParameter;
    openmaxStandComp->SetConfig     = omx_symbianoutputstreamsink_component_SetConfig;
    openmaxStandComp->GetConfig     = omx_symbianoutputstreamsink_component_GetConfig;

    /* Write in the default paramenters */
    omx_symbianoutputstreamsink_component_SetParameter(openmaxStandComp, OMX_IndexParamAudioInit, &omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio]);
    pPort->AudioPCMConfigured	= 0;

    if (!pPort->AudioPCMConfigured) 
    {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
        omx_symbianoutputstreamsink_component_SetParameter(openmaxStandComp, OMX_IndexParamAudioPcm, &pPort->omxAudioParamPcmMode);
    }

    return err;
}

/** The Destructor 
 */
OMX_ERRORTYPE 
omx_symbianoutputstreamsink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp)
{
	omx_symbianoutputstreamsink_component_PrivateType* omx_symbianoutputstreamsink_component_Private = openmaxStandComp->pComponentPrivate;

	omx_symbianoutputstreamsink_component_PortType* pPort = (omx_symbianoutputstreamsink_component_PortType *) omx_symbianoutputstreamsink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

    if(pPort->output_handle)
    {
        close_output_stream(pPort->output_handle);
    }	

	return omx_base_sink_Destructor(openmaxStandComp);
}

/** 
 * This function plays the input buffer. When fully consumed it returns.
 */
void 
omx_symbianoutputstreamsink_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer) 
{
    OMX_S32 totalBuffer;
    omx_symbianoutputstreamsink_component_PrivateType* omx_symbianoutputstreamsink_component_Private = openmaxStandComp->pComponentPrivate;
    omx_symbianoutputstreamsink_component_PortType *port = (omx_symbianoutputstreamsink_component_PortType *) omx_symbianoutputstreamsink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

    if(inputbuffer->nFilledLen <= 0)
    {
        DEBUG(DEB_LEV_FULL_SEQ, "inputBuffer filledLen is %d\n", inputbuffer->nFilledLen);
        return;
    }

    totalBuffer = inputbuffer->nFilledLen;

    DEBUG(DEB_LEV_FULL_SEQ, "opening stream at symbianoutputstreamsink\n");
    
    open_output_stream(port->output_handle, 44100, 2);

    DEBUG(DEB_LEV_FULL_SEQ, "writing audio data at symbianoutputstreamsink\n");

    write_audio_data(port->output_handle, inputbuffer->pBuffer, totalBuffer);
    
    DEBUG(DEB_LEV_FULL_SEQ, "audio data  written at symbianoutputstreamsink\n");

    inputbuffer->nFilledLen=0;
}

OMX_ERRORTYPE omx_symbianoutputstreamsink_component_SetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure) 
{
    /*Call the base component function*/
    return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE omx_symbianoutputstreamsink_component_GetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    /*Call the base component function*/
    return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE 
omx_symbianoutputstreamsink_component_SetParameter(OMX_IN  OMX_HANDLETYPE hComponent,
                                                   OMX_IN  OMX_INDEXTYPE nParamIndex,
                                                   OMX_IN  OMX_PTR ComponentParameterStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
    OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
    OMX_U32 portIndex;
	OMX_AUDIO_PARAM_PCMMODETYPE* omxAudioParamPcmMode;
    
    /* Check which structure we are being fed and make control its header */
    OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
    omx_symbianoutputstreamsink_component_PrivateType* omx_symbianoutputstreamsink_component_Private = openmaxStandComp->pComponentPrivate;
    omx_symbianoutputstreamsink_component_PortType* pPort = (omx_symbianoutputstreamsink_component_PortType *) omx_symbianoutputstreamsink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
 
    if (ComponentParameterStructure == NULL) 
    {
        return OMX_ErrorBadParameter;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

    switch(nParamIndex) 
    {
        case OMX_IndexParamAudioInit:
            /*Check Structure Header*/
            err = checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
            if(err!=OMX_ErrorNone) 
            { 
                DEBUG(DEB_LEV_ERR, "Header Check Error=%x\n",err); 
                break;
            }
            memcpy(&omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio],ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
            break;

        case OMX_IndexParamAudioPortFormat:
            pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
            portIndex = pAudioPortFormat->nPortIndex;
            /*Check Structure Header and verify component state*/
            err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            if(err!=OMX_ErrorNone) 
            { 
                DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
                break;
            }
            if (portIndex < 1) 
            {
                memcpy(&pPort->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            } 
            else 
            {
                return OMX_ErrorBadPortIndex;
            }
            break;

        case OMX_IndexParamAudioPcm:
            omxAudioParamPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
            pPort->AudioPCMConfigured	= 1;
            if(omxAudioParamPcmMode->nPortIndex != pPort->omxAudioParamPcmMode.nPortIndex)
            {
                DEBUG(DEB_LEV_ERR, "Error setting input pPort index\n");
                err = OMX_ErrorBadParameter;
                break;
            }	
            break;

        case OMX_IndexParamAudioMp3:
            pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
            /*Check Structure Header and verify component state*/
            err = omx_base_component_ParameterSanityCheck(hComponent, pAudioMp3->nPortIndex, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
            if(err!=OMX_ErrorNone) 
            { 
                DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
                break;
            }
            break;

        default: /*Call the base component function*/
            return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
omx_symbianoutputstreamsink_component_GetParameter(OMX_IN  OMX_HANDLETYPE hComponent,
                                                   OMX_IN  OMX_INDEXTYPE nParamIndex,
                                                   OMX_INOUT OMX_PTR ComponentParameterStructure)
{
    OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
    
    OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
    omx_symbianoutputstreamsink_component_PrivateType* omx_symbianoutputstreamsink_component_Private = openmaxStandComp->pComponentPrivate;
    omx_symbianoutputstreamsink_component_PortType *pPort = (omx_symbianoutputstreamsink_component_PortType *) omx_symbianoutputstreamsink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];	
    
    if (ComponentParameterStructure == NULL) 
    {
        return OMX_ErrorBadParameter;
    }
    
    DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
    
    /* Check which structure we are being fed and fill its header */
    switch(nParamIndex) 
    {
        case OMX_IndexParamAudioInit:
            setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
            memcpy(ComponentParameterStructure, &omx_symbianoutputstreamsink_component_Private->sPortTypesParam[OMX_PortDomainAudio], sizeof(OMX_PORT_PARAM_TYPE));
            break;

        case OMX_IndexParamAudioPortFormat:
            pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
            setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            if (pAudioPortFormat->nPortIndex < 1) 
            {
                memcpy(pAudioPortFormat, &pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            } 
            else 
            {
                return OMX_ErrorBadPortIndex;
            }
            break;		

        case OMX_IndexParamAudioPcm:
            if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
                pPort->omxAudioParamPcmMode.nPortIndex)
            {
                return OMX_ErrorBadParameter;
            }
            memcpy(ComponentParameterStructure, &pPort->omxAudioParamPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            break;

        default: /*Call the base component function*/
            return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }

    return OMX_ErrorNone;
}
