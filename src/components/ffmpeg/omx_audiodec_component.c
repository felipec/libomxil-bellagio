/**
 * @file src/components/ffmpeg/omx_audiodec_component.c
 *
 * This component implements and mp3 decoder. The Mp3/WMA decoder is based on ffmpeg
 * software library.
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
 *
 * 2006/05/11:  ffmpeg mp3 decoder component version 0.2
 *
 */


#include <omxcore.h>
#include <omx_audiodec_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

/** The Constructor 
 */

OMX_ERRORTYPE omx_audiodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_audiodec_component_PortType *inPort,*outPort;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_audiodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else 
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__,openmaxStandComp->pComponentPrivate);

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;

  // we could create our own port structures here
  // fixme maybe the base class could use a "port factory" function pointer?	
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);

  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_audiodec_component_Private = (omx_audiodec_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  //debug statement
  DEBUG(DEB_LEV_SIMPLE_SEQ,"constructor of mp3decoder component is called\n");

  /** Allocate Ports and Call base port constructor. */	
  if (omx_audiodec_component_Private->sPortTypesParam.nPorts && !omx_audiodec_component_Private->ports) {
    omx_audiodec_component_Private->ports = calloc(omx_audiodec_component_Private->sPortTypesParam.nPorts,sizeof (omx_base_PortType *));
    if (!omx_audiodec_component_Private->ports) return OMX_ErrorInsufficientResources;

    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam.nPorts; i++) {
      // this is the important thing separating this from the base class; size of the struct is for derived class port type
      // this could be refactored as a smarter factory function instead?
      omx_audiodec_component_Private->ports[i] = calloc(1, sizeof(omx_audiodec_component_PortType));
      if (!omx_audiodec_component_Private->ports[i]) return OMX_ErrorInsufficientResources;
    }
  }

  base_port_Constructor(openmaxStandComp,&omx_audiodec_component_Private->ports[0],0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp,&omx_audiodec_component_Private->ports[1],1, OMX_FALSE);

  /** Domain specific section for the ports. */	
  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  //common parameters related to output port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))   // mp3 format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  else if(!strcmp(cComponentName, AUDIO_DEC_WMA_NAME))   // WMA format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingWMA;
  else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME))// general audio decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;
  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_audiodec_component_Private->avCodecSyncSem) {
    omx_audiodec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_audiodec_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_audiodec_component_Private->avCodecSyncSem, 0);
  }

  SetInternalParameters(openmaxStandComp);

  outPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  outPort->sAudioParam.nPortIndex = 1;
  outPort->sAudioParam.nIndex = 0;
  outPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;


  //settings of output port
  //output is pcm mode for all decoders - so generalise it outside switch
  setHeader(&omx_audiodec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_audiodec_component_Private->pAudioPcmMode.nPortIndex = 1;
  omx_audiodec_component_Private->pAudioPcmMode.nChannels = 2;
  omx_audiodec_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_audiodec_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_audiodec_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = 44100;
  omx_audiodec_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_audiodec_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_audiodec_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  //general configuration irrespective of any audio formats
  //setting other parameters of omx_mp3dec-component_private	
  omx_audiodec_component_Private->avCodec = NULL;
  omx_audiodec_component_Private->avCodecContext= NULL;
  omx_audiodec_component_Private->avcodecReady = OMX_FALSE;
  omx_audiodec_component_Private->BufferMgmtCallback = omx_audiodec_component_BufferMgmtCallback;
  omx_audiodec_component_Private->messageHandler = omx_audio_decoder_MessageHandler;
  omx_audiodec_component_Private->destructor = omx_audiodec_component_Destructor;
  //omx_audiodec_component_Private->Init = omx_audiodec_component_Init;
  //omx_audiodec_component_Private->Deinit = omx_audiodec_component_Deinit;
  //omx_audiodec_component_Private->DomainCheck	 = &omx_audiodec_component_DomainCheck;

  openmaxStandComp->SetParameter = omx_audiodec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_audiodec_component_GetParameter;

  return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_audiodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) 
{
  int i;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audiodec_component_PortType* port = (omx_audiodec_component_PortType *)omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

  if(omx_audiodec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_audiodec_component_Private->avCodecSyncSem);
    free(omx_audiodec_component_Private->avCodecSyncSem);
    omx_audiodec_component_Private->avCodecSyncSem=NULL;
  }

  //frees the locally dynamic allocated memory
  if (omx_audiodec_component_Private->sPortTypesParam.nPorts && omx_audiodec_component_Private->ports) {
    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_audiodec_component_Private->ports[i])
        base_port_Destructor(omx_audiodec_component_Private->ports[i]);
    }
    free(omx_audiodec_component_Private->ports);
    omx_audiodec_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audiodecoder component is called\n");

  return omx_base_filter_Destructor(openmaxStandComp);
}

/** 
	It initializates the ffmpeg framework, and opens an ffmpeg audiodecoder of type specified by IL client
*/ 
OMX_ERRORTYPE omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {

  OMX_U32 target_codecID;  // id of ffmpeg codec to be used for different audio formats 
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_audiodec_component_Private->audio_coding_type){
  case OMX_AUDIO_CodingMP3 :
    target_codecID = CODEC_ID_MP2;
    break;
  case OMX_AUDIO_CodingWMA : 
    target_codecID = CODEC_ID_WMAV1;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than mp3 & wma not supported\nCodec not found\n");
    return OMX_ErrorComponentNotFound;
  }//end of switch

  /*Find the  decoder corresponding to the audio type specified by IL client*/
  omx_audiodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_audiodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec Not found\n");
    return OMX_ErrorInsufficientResources;
  }

  DEBUG(DEB_LEV_ERR, "In %s \n",__func__);

  omx_audiodec_component_Private->avCodecContext = avcodec_alloc_context();

  /* specification of different fields like bit rate & sample rate etc in structure avCodecContext for wma format prior         to call avcodec_open)() function -- */
  if(target_codecID == CODEC_ID_WMAV1) {                                                                                                                     		omx_audiodec_component_Private->avCodecContext->bit_rate = 28000;
    omx_audiodec_component_Private->avCodecContext->sample_rate = 44100;
    omx_audiodec_component_Private->avCodecContext->channels = 2;
    omx_audiodec_component_Private->avCodecContext->block_align = 512;
  }//end if

  /* end of specification */

  /*open the avcodec */
  if (avcodec_open(omx_audiodec_component_Private->avCodecContext, omx_audiodec_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }
  tsem_up(omx_audiodec_component_Private->avCodecSyncSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");
  return OMX_ErrorNone;
}

/** 
	It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder
*/
void omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {
	
  avcodec_close(omx_audiodec_component_Private->avCodecContext);

  if (omx_audiodec_component_Private->avCodecContext->priv_data)
    avcodec_close (omx_audiodec_component_Private->avCodecContext);

  if (omx_audiodec_component_Private->avCodecContext->extradata) {
    av_free (omx_audiodec_component_Private->avCodecContext->extradata);
    omx_audiodec_component_Private->avCodecContext->extradata = NULL;
  }
  /*Free Codec Context*/
  av_free (omx_audiodec_component_Private->avCodecContext);
    
}

void SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_audiodec_component_PortType *inPort,*outPort;

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
	
  if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/mpeg";
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

    setHeader(&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
    omx_audiodec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
    omx_audiodec_component_Private->pAudioMp3.nChannels = 2;                                                                    
    omx_audiodec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
    omx_audiodec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
    omx_audiodec_component_Private->pAudioMp3.nAudioBandWidth = 0;
    omx_audiodec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    setHeader(&omx_audiodec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    omx_audiodec_component_Private->pAudioMp3.nPortIndex=0;
    omx_audiodec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

    inPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&inPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    inPort->sAudioParam.nPortIndex = 0;
    inPort->sAudioParam.nIndex = 0;
    inPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
  } else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingWMA) {

    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "";
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingWMA;

    setHeader(&omx_audiodec_component_Private->pAudioWma,sizeof(OMX_AUDIO_PARAM_WMATYPE));
    omx_audiodec_component_Private->pAudioWma.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioWma.nChannels = 2;
    omx_audiodec_component_Private->pAudioWma.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioWma.eFormat = OMX_AUDIO_WMAFormat9 | OMX_AUDIO_WMAFormat8 | OMX_AUDIO_WMAFormat7 ;

    inPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&inPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    inPort->sAudioParam.nPortIndex = 0;
    inPort->sAudioParam.nIndex = 0;
    inPort->sAudioParam.eEncoding = OMX_AUDIO_CodingWMA;
  }
}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_audiodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  //omx_base_component_Init(openmaxStandComp);


  /*Temporary First Output buffer size*/
  omx_audiodec_component_Private->inputCurrBuffer=NULL;
  omx_audiodec_component_Private->inputCurrLength=0;
  nBufferSize=omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_audiodec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(nBufferSize);
  memset(omx_audiodec_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_audiodec_component_Private->isFirstBuffer=1;
  omx_audiodec_component_Private->positionInOutBuf = 0;
  omx_audiodec_component_Private->isNewBuffer=1;

  return err;
	
};

/** The Deinitialization function 
 */
OMX_ERRORTYPE omx_audiodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (omx_audiodec_component_Private->avcodecReady) {
    omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_Private);
    omx_audiodec_component_Private->avcodecReady = OMX_FALSE;
  }

  free(omx_audiodec_component_Private->internalOutputBuffer);
  //omx_base_component_Deinit(openmaxStandComp);

  return err;
}

/*Deprecated function. May or may not be used in future version*/
/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_audiodec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef){
  if(pDef.eDomain!=OMX_PortDomainAudio)
    return OMX_ErrorPortsNotCompatible;
  else if(pDef.format.audio.eEncoding == OMX_AUDIO_CodingMax)
    return OMX_ErrorPortsNotCompatible;

  return OMX_ErrorNone;
}

/** This function is used to process the input buffer and provide one output buffer
 */
void omx_audiodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_S32 outputfilled = 0;
  OMX_U8* outputCurrBuffer;
  OMX_U32 outputLength;
  OMX_U32 len = 0;
  OMX_S32 internalOutputFilled=0;


  /**Fill up the current input buffer when a new buffer has arrived*/
  if(omx_audiodec_component_Private->isNewBuffer) {
    omx_audiodec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
    omx_audiodec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
    omx_audiodec_component_Private->positionInOutBuf = 0;
    omx_audiodec_component_Private->isNewBuffer=0;
  }
  outputCurrBuffer = outputbuffer->pBuffer;
  outputLength = outputbuffer->nAllocLen;
  outputbuffer->nFilledLen = 0;
  outputbuffer->nOffset=0;

  while (!outputfilled) {
    if (omx_audiodec_component_Private->isFirstBuffer) {
      tsem_down(omx_audiodec_component_Private->avCodecSyncSem);
      len = avcodec_decode_audio(omx_audiodec_component_Private->avCodecContext, 
              (short*)omx_audiodec_component_Private->internalOutputBuffer, 
              (int*)&internalOutputFilled,
              omx_audiodec_component_Private->inputCurrBuffer, 
              omx_audiodec_component_Private->inputCurrLength);

      DEBUG(DEB_LEV_FULL_SEQ, "Frequency = %i channels = %i\n", omx_audiodec_component_Private->avCodecContext->sample_rate, omx_audiodec_component_Private->avCodecContext->channels);
      omx_audiodec_component_Private->minBufferLength = internalOutputFilled;
      DEBUG(DEB_LEV_FULL_SEQ, "buffer length %i buffer given %i len=%d\n", internalOutputFilled, outputLength,len);
		
      if (internalOutputFilled > outputLength) {
        DEBUG(DEB_LEV_ERR, "---> Ouch! the output buffer is too small!!!! iof=%d,ol=%d\n",internalOutputFilled,outputLength);
        inputbuffer->nFilledLen=0;
        /*Simply return the output buffer without writing anything*/
        internalOutputFilled = 0;
        omx_audiodec_component_Private->positionInOutBuf = 0;
        outputfilled = 1;
        break;
      }

      if((omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate != omx_audiodec_component_Private->avCodecContext->sample_rate) ||
        ( omx_audiodec_component_Private->pAudioPcmMode.nChannels!=omx_audiodec_component_Private->avCodecContext->channels)) {
        DEBUG(DEB_LEV_FULL_SEQ, "---->Sending Port Settings Change Event\n");
				/* has mp3 dependency--requires modification */
			//switch for different audio formats---parameter settings accordingly
        switch(omx_audiodec_component_Private->audio_coding_type)	{
        case OMX_AUDIO_CodingMP3 :
					/*Update Parameter which has changed from avCodecContext*/
          /*pAudioMp3 is for input port Mp3 data*/
          omx_audiodec_component_Private->pAudioMp3.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
          omx_audiodec_component_Private->pAudioMp3.nBitRate = omx_audiodec_component_Private->avCodecContext->bit_rate;
          omx_audiodec_component_Private->pAudioMp3.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          /*pAudioPcmMode is for output port PCM data*/
          omx_audiodec_component_Private->pAudioPcmMode.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
          omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = omx_audiodec_component_Private->avCodecContext->bits_per_sample;
          omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          break;
        case OMX_AUDIO_CodingWMA :
          /*Update Parameter which has changed from avCodecContext*/
          /*pAudioWMA is for input port WMA data*/
          omx_audiodec_component_Private->pAudioWma.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
          omx_audiodec_component_Private->pAudioWma.nBitRate = omx_audiodec_component_Private->avCodecContext->bit_rate;
          //sample rate is not present in WMA structure
          //omx_audiodec_component_Private->pAudioWMA.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          //format field present in WMA structure-----change accordingly
          omx_audiodec_component_Private->pAudioWma.eFormat = omx_audiodec_component_Private->avCodecContext->sample_fmt;
          /*pAudioPcmMode is for output port PCM data*/
          omx_audiodec_component_Private->pAudioPcmMode.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
          omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = omx_audiodec_component_Private->avCodecContext->bits_per_sample;
          omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          break;
        default :
          DEBUG(DEB_LEV_ERR, "Audio format other than mp3 & wma not supported\nCodec not found\n");
          break;                       
        }//end of switch
				/*Send Port Settings changed call back*/
        (*(omx_audiodec_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_audiodec_component_Private->callbackData,
          OMX_EventPortSettingsChanged, /* The command was completed */
          0, 
          1, /* This is the output port index */
          NULL);
			}
    } else {
      len = avcodec_decode_audio(omx_audiodec_component_Private->avCodecContext,
              (short*)(outputCurrBuffer + (omx_audiodec_component_Private->positionInOutBuf * omx_audiodec_component_Private->minBufferLength)), 
              (int*)&internalOutputFilled,
              omx_audiodec_component_Private->inputCurrBuffer, 
              omx_audiodec_component_Private->inputCurrLength);
		}
    if (len < 0){
      DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
    }
    if (internalOutputFilled > 0) {
      omx_audiodec_component_Private->inputCurrBuffer += len;
      omx_audiodec_component_Private->inputCurrLength -= len;
      inputbuffer->nFilledLen -= len;
      DEBUG(DEB_LEV_FULL_SEQ, "Buf Consumed IbLen=%d Len=%d minlen=%d\n", 
      inputbuffer->nFilledLen,len,omx_audiodec_component_Private->minBufferLength);
      /*Buffer is fully consumed. Request for new Input Buffer*/
      if(inputbuffer->nFilledLen==0)
        omx_audiodec_component_Private->isNewBuffer=1;
    } else {
      /**  This condition becomes true when the input buffer has completely be consumed.
      *  In this case is immediately switched because there is no real buffer consumption */
      DEBUG(DEB_LEV_FULL_SEQ, "New Buf Reqd IbLen=%d Len=%d  iof=%d\n", inputbuffer->nFilledLen,len,internalOutputFilled);
      inputbuffer->nFilledLen=0;
      /*Few bytes may be left in the input buffer but can't generate one output frame. 
      Request for new Input Buffer*/
      omx_audiodec_component_Private->isNewBuffer=1;
    }

    if (omx_audiodec_component_Private->isFirstBuffer) {
      memcpy(outputCurrBuffer, omx_audiodec_component_Private->internalOutputBuffer, internalOutputFilled);
      omx_audiodec_component_Private->isFirstBuffer = 0;
    }
    if (internalOutputFilled > 0) {
      outputbuffer->nFilledLen += internalOutputFilled;
    }

    /* We are done with output buffer */
    omx_audiodec_component_Private->positionInOutBuf++;

    if ((omx_audiodec_component_Private->minBufferLength > 
      (outputLength - (omx_audiodec_component_Private->positionInOutBuf * omx_audiodec_component_Private->minBufferLength))) || (internalOutputFilled <= 0)) {
      internalOutputFilled = 0;
      omx_audiodec_component_Private->positionInOutBuf = 0;
      outputfilled = 1;
      /*Send the output buffer*/
    }
  }
    DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n",outputbuffer->pBuffer,outputbuffer->nFilledLen);
}

OMX_ERRORTYPE omx_audiodec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) 
{
		
  switch (nIndex) {
  default: // delegate to superclass
    return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
  switch (nIndex) {
  default: // delegate to superclass
    return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_AUDIO_PARAM_WMATYPE * pAudioWma; //support for WMA format
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audiodec_component_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    /*Check Structure Header*/
    err = checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
    if (err != OMX_ErrorNone)
      return err;
    memcpy(&omx_audiodec_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
    break;	
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if (err != OMX_ErrorNone)
      return err;
    if (portIndex <= 1) {
      port = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;	
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    portIndex = pAudioPcmMode->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    if (err != OMX_ErrorNone)
      return err;
    memcpy(&omx_audiodec_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
    break;
  case OMX_IndexParamAudioWma:
    pAudioWma = (OMX_AUDIO_PARAM_WMATYPE*)ComponentParameterStructure;
    portIndex = pAudioWma->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioWma,sizeof(OMX_AUDIO_PARAM_WMATYPE));
    if(err!=OMX_ErrorNone)
      return err;
    memcpy(&omx_audiodec_component_Private->pAudioWma,pAudioWma,sizeof(OMX_AUDIO_PARAM_WMATYPE));
    break;
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    } else if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_WMA_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingWMA;
    } else {
      return OMX_ErrorBadParameter;
    }
    SetInternalParameters(openmaxStandComp);
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
    portIndex = pAudioMp3->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone)
      return err;
    if (pAudioMp3->nPortIndex == 0) {
      memcpy(&omx_audiodec_component_Private->pAudioMp3,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_GetParameter(
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
  OMX_AUDIO_PARAM_WMATYPE * pAudioWma; //support for WMA format
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
  omx_audiodec_component_PortType *port;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
    memcpy(ComponentParameterStructure, &omx_audiodec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_audiodec_component_PortType *)omx_audiodec_component_Private->ports[pAudioPortFormat->nPortIndex];
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
    memcpy(pAudioPcmMode,&omx_audiodec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    if (pAudioMp3->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    setHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    memcpy(pAudioMp3,&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    break;
  case OMX_IndexParamAudioWma:
    pAudioWma = (OMX_AUDIO_PARAM_WMATYPE*)ComponentParameterStructure;
    if (pAudioWma->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    setHeader(pAudioWma, sizeof(OMX_AUDIO_PARAM_WMATYPE));
    memcpy(pAudioWma,&omx_audiodec_component_Private->pAudioWma,sizeof(OMX_AUDIO_PARAM_WMATYPE));
    break;
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    setHeader(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));

    if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
    } else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingWMA) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_WMA_ROLE);
    } else {
      strcpy((char*)pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audio_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_audiodec_component_Private->state == OMX_StateLoaded)) {
      DEBUG(DEB_LEV_ERR, "In %s \n",__func__);
      if (!omx_audiodec_component_Private->avcodecReady) {
        DEBUG(DEB_LEV_ERR, "In %s \n",__func__);
        err = omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_audiodec_component_Private->avcodecReady = OMX_TRUE;
      }
      err = omx_audiodec_component_Init(openmaxStandComp);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s\n",__func__);
        return err;
      }
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_audiodec_component_Private->state == OMX_StateIdle)) {
    err = omx_audiodec_component_Deinit(openmaxStandComp);
    CHECK_ERROR(err);
    }
  }
  // Execute the base message handling
  return omx_base_component_MessageHandler(openmaxStandComp,message);
}

OMX_ERRORTYPE omx_audiodec_component_ComponentRoleEnum(
    OMX_IN OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_U8 *cRole,
	OMX_IN OMX_U32 nIndex)
{
	if (nIndex == 0) {
		strcpy((char*)cRole, AUDIO_DEC_MP3_ROLE);
	} else if (nIndex == 1) {
		strcpy((char*)cRole, AUDIO_DEC_WMA_ROLE);
	}	else {
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}



