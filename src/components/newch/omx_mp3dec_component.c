/**
 * @file src/components/newch/omx_mp3dec_component.c
 * 
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Pankaj SEN,Ukri NIEMIMUUKKO, Diego MELPIGNANO, , David SIORPAES, Giulio URLINI
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
#include <omx_mp3dec_component.h>


void __attribute__ ((constructor)) omx_mp3dec_component_register_template() {
	stComponentType *component;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	component = base_component_CreateComponentStruct();
	/** here you can override the base component defaults */
	component->name = "OMX.ffmpeg.mp3dec.component";
	component->constructor = omx_mp3dec_component_Constructor;
	component->omx_component.SetConfig = omx_mp3dec_component_SetConfig;
	component->omx_component.GetConfig = omx_mp3dec_component_GetConfig;
	component->omx_component.SetParameter = omx_mp3dec_component_SetParameter;
	component->omx_component.GetParameter = omx_mp3dec_component_GetParameter;
	  
	// port count must be set for the base class constructor (if we call it, and we will)
	component->nports = 2;
  
 	register_template(component);
}

/** 
	It initializates the ffmpeg framework, and opens an ffmpeg mp3 decoder
*/
void omx_mp3dec_component_ffmpegLibInit(omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private) {
	avcodec_init();
	av_register_all();

	DEBUG(DEB_LEV_FULL_SEQ, "FFMpeg Library/codec iniited\n");

	omx_mp3dec_component_Private->avCodec = avcodec_find_decoder(CODEC_ID_MP2);
	if (!omx_mp3dec_component_Private->avCodec) {
		DEBUG(DEB_LEV_ERR, "Codec Not found\n");
		base_component_Panic();
	}
	omx_mp3dec_component_Private->avCodecContext = avcodec_alloc_context();
    //open it
    if (avcodec_open(omx_mp3dec_component_Private->avCodecContext, omx_mp3dec_component_Private->avCodec) < 0) {
		DEBUG(DEB_LEV_ERR, "Could not open codec\n");
		base_component_Panic();
    }
}

/** 
	It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder
*/
void omx_mp3dec_component_ffmpegLibDeInit(omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private) {
	
	avcodec_close(omx_mp3dec_component_Private->avCodecContext);

	if (omx_mp3dec_component_Private->avCodecContext->priv_data)
		avcodec_close (omx_mp3dec_component_Private->avCodecContext);
	
	if (omx_mp3dec_component_Private->avCodecContext->extradata) {
		av_free (omx_mp3dec_component_Private->avCodecContext->extradata);
		omx_mp3dec_component_Private->avCodecContext->extradata = NULL;
	}
	/*Free Codec Context*/
	av_free (omx_mp3dec_component_Private->avCodecContext);
    
}

OMX_ERRORTYPE omx_mp3dec_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_mp3dec_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = omx_twoport_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_mp3dec_component_Private = (omx_mp3dec_component_PrivateType *)stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
		/** Domain specific section for the ports. */	
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/mpeg";
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = 0;

	setHeader(&omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sAudioParam.nPortIndex = 0;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sAudioParam.nIndex = 0;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

	setHeader(&omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sAudioParam.nPortIndex = 1;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sAudioParam.nIndex = 0;
	omx_mp3dec_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sAudioParam.eEncoding = 0;
	
	omx_mp3dec_component_Private->avCodec = NULL;
	omx_mp3dec_component_Private->avCodecContext= NULL;
	omx_mp3dec_component_Private->avcodecReady = OMX_FALSE;
	omx_mp3dec_component_Private->BufferMgmtCallback = omx_mp3dec_component_BufferMgmtCallback;
	omx_mp3dec_component_Private->Init = omx_mp3dec_component_Init;
	omx_mp3dec_component_Private->Deinit = omx_mp3dec_component_Deinit;

	return err;
}

OMX_ERRORTYPE omx_mp3dec_component_Init(stComponentType* stComponent)
{
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	
	base_component_Init(stComponent);

	if (!omx_mp3dec_component_Private->avcodecReady) {
		omx_mp3dec_component_ffmpegLibInit(omx_mp3dec_component_Private);
		omx_mp3dec_component_Private->avcodecReady = OMX_TRUE;
	}

	omx_mp3dec_component_Private->inputCurrBuffer=NULL;
	omx_mp3dec_component_Private->inputCurrLength=0;
	omx_mp3dec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(INTERNAL_OUT_BUFFER_SIZE*2);
	memset(omx_mp3dec_component_Private->internalOutputBuffer, 0, INTERNAL_OUT_BUFFER_SIZE*2);
	omx_mp3dec_component_Private->isFirstBuffer=1;
	omx_mp3dec_component_Private->positionInOutBuf = 0;
	omx_mp3dec_component_Private->isNewBuffer=1;

	return err;
	
};

OMX_ERRORTYPE omx_mp3dec_component_Deinit(stComponentType* stComponent) {
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	if (omx_mp3dec_component_Private->avcodecReady) {
		omx_mp3dec_component_ffmpegLibDeInit(omx_mp3dec_component_Private);
		omx_mp3dec_component_Private->avcodecReady = OMX_FALSE;
	}

	free(omx_mp3dec_component_Private->internalOutputBuffer);
	base_component_Deinit(stComponent);

	return err;
}

void omx_mp3dec_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 outputfilled = 0;
	OMX_U8* outputBuffer;
	OMX_U8* outputCurrBuffer;
	OMX_U32 outputLength;
	OMX_U32 len = 0;
	OMX_S32 internalOutputFilled=0;
			

	/*This is just a work arround that minimum input buffer size should be 10.
	Though may be or should be higher. I didnot measured any statistics*/
	if(inputbuffer->nFilledLen < 10) {
		inputbuffer->nFilledLen=0;
		return;
	}
	/**Fill up the current input buffer when a new buffer has arrived*/
	if(omx_mp3dec_component_Private->isNewBuffer) {
	omx_mp3dec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
	omx_mp3dec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
	omx_mp3dec_component_Private->positionInOutBuf = 0;
	omx_mp3dec_component_Private->isNewBuffer=0;
	}
	outputBuffer = outputbuffer->pBuffer;
	outputCurrBuffer = outputBuffer;
	outputLength = outputbuffer->nAllocLen;
	outputbuffer->nFilledLen = 0;

	while (!outputfilled) {
		if (omx_mp3dec_component_Private->isFirstBuffer) {
			len = avcodec_decode_audio(omx_mp3dec_component_Private->avCodecContext, 
									(short*)omx_mp3dec_component_Private->internalOutputBuffer, 
									(int*)&internalOutputFilled,
									omx_mp3dec_component_Private->inputCurrBuffer, 
									omx_mp3dec_component_Private->inputCurrLength);
			DEBUG(DEB_LEV_FULL_SEQ, "Frequency = %i channels = %i\n", omx_mp3dec_component_Private->avCodecContext->sample_rate, omx_mp3dec_component_Private->avCodecContext->channels);
			omx_mp3dec_component_Private->minBufferLength = internalOutputFilled;
			DEBUG(DEB_LEV_FULL_SEQ, "buffer length %i buffer given %i len=%d\n", (OMX_S32)internalOutputFilled, (OMX_S32)outputLength,len);
		
			if (internalOutputFilled > outputLength) {
				DEBUG(DEB_LEV_ERR, "---> Ouch! the output buffer is too small!!!! iof=%d,ol=%d\n",internalOutputFilled,outputLength);
				inputbuffer->nFilledLen=0;
				/*Simply return the output buffer without writing anything*/
				internalOutputFilled = 0;
				omx_mp3dec_component_Private->positionInOutBuf = 0;
				outputfilled = 1;
				break;
				//base_component_Panic();
			}
		} else {
			len = avcodec_decode_audio(omx_mp3dec_component_Private->avCodecContext,
									(short*)(outputCurrBuffer + (omx_mp3dec_component_Private->positionInOutBuf * omx_mp3dec_component_Private->minBufferLength)), 
									(int*)&internalOutputFilled,
									omx_mp3dec_component_Private->inputCurrBuffer, 
									omx_mp3dec_component_Private->inputCurrLength);

		}
		if (len < 0){
			DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
		}
		if (internalOutputFilled > 0) {
			omx_mp3dec_component_Private->inputCurrBuffer += len;
			omx_mp3dec_component_Private->inputCurrLength -= len;
			inputbuffer->nFilledLen -= len;
			DEBUG(DEB_LEV_FULL_SEQ, "Buf Consumed IbLen=%d Len=%d minlen=%d\n", 
				inputbuffer->nFilledLen,len,omx_mp3dec_component_Private->minBufferLength);
			if(inputbuffer->nFilledLen==0)
				omx_mp3dec_component_Private->isNewBuffer=1;
		} else {
			/**  This condition becomes true when the input buffer has completely be consumed.
				*  In this case is immediately switched because there is no real buffer consumption */
			DEBUG(DEB_LEV_FULL_SEQ, "New Buf Reqd IbLen=%d Len=%d  iof=%d\n", inputbuffer->nFilledLen,len,internalOutputFilled);
			inputbuffer->nFilledLen=0;
			omx_mp3dec_component_Private->isNewBuffer=1;
		}

		if (omx_mp3dec_component_Private->isFirstBuffer) {
			memcpy(outputCurrBuffer, omx_mp3dec_component_Private->internalOutputBuffer, internalOutputFilled);
			omx_mp3dec_component_Private->isFirstBuffer = 0;
			
		}
		if (internalOutputFilled > 0) {
			outputbuffer->nFilledLen += internalOutputFilled;
		}

		/* We are done with output buffer */
		omx_mp3dec_component_Private->positionInOutBuf++;
		
		if ((omx_mp3dec_component_Private->minBufferLength > 
			(outputLength - (omx_mp3dec_component_Private->positionInOutBuf * omx_mp3dec_component_Private->minBufferLength))) || (internalOutputFilled <= 0)) {
			internalOutputFilled = 0;
			omx_mp3dec_component_Private->positionInOutBuf = 0;
			outputfilled = 1;
			/*Send the output buffer*/
		}
	}
	DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n",outputbuffer->pBuffer,outputbuffer->nFilledLen);

	
}

OMX_ERRORTYPE omx_mp3dec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) {
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
		
	switch (nIndex) {
		default: // delegate to superclass
			return base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_mp3dec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	//fixme
	return base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE omx_mp3dec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
	OMX_AUDIO_PARAM_MP3TYPE * pAuidoMp3;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
			if (err != OMX_ErrorNone)
				return err;
			memcpy(&omx_mp3dec_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
			break;	
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (err != OMX_ErrorNone)
					return err;
			if (portIndex <= 1) {
				memcpy(&omx_mp3dec_component_Private->ports[portIndex]->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} else {
					return OMX_ErrorBadPortIndex;
			}
			break;	
		case OMX_IndexParamAudioPcm:
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
		break;
		case OMX_IndexParamAudioMp3:
			pAuidoMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAuidoMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
		break;
		//fixme
		default:
			return base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_mp3dec_component_GetParameter(
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
	OMX_AUDIO_PARAM_MP3TYPE * pAuidoMp3;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_mp3dec_component_PrivateType* omx_mp3dec_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
			memcpy(ComponentParameterStructure, &omx_mp3dec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
			break;		
		case OMX_IndexParamAudioPortFormat:
		pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
		if (pAudioPortFormat->nPortIndex <= 1) {
			memcpy(pAudioPortFormat, &omx_mp3dec_component_Private->ports[pAudioPortFormat->nPortIndex]->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
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
		case OMX_IndexParamAudioMp3:
			pAuidoMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			setHeader(pAuidoMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			if (pAuidoMp3->nPortIndex != 0) {
				return OMX_ErrorBadPortIndex;
			}
			pAuidoMp3->nChannels = 2;
			pAuidoMp3->nBitRate = 0;
			pAuidoMp3->nSampleRate = 0;
			pAuidoMp3->nAudioBandWidth = 0;
			pAuidoMp3->eChannelMode = OMX_AUDIO_ChannelModeStereo;
		break;
		default:
			return base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
}
