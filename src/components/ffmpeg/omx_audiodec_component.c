/**
  @file src/components/ffmpeg/omx_audiodec_component.c

  This component implements and mp3 decoder. The Mp3 decoder is based on ffmpeg
  software library.

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
#include <omx_audiodec_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

#define MAX_COMPONENT_AUDIODEC 4

/** output length arguement passed along decoding function */
#define OUTPUT_LEN_STANDARD_FFMPEG 192000

/** Number of Audio Component Instance*/
OMX_U32 noAudioDecInstance=0;

/** The Constructor 
 */
OMX_ERRORTYPE omx_audiodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_audiodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else 
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__, (int)openmaxStandComp->pComponentPrivate);

  // we could create our own port structures here
  // fixme maybe the base class could use a "port factory" function pointer?	
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);
  
  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_audiodec_component_Private = (omx_audiodec_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  
  //debug statement
  DEBUG(DEB_LEV_SIMPLE_SEQ,"constructor of audiodecoder component is called\n");

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

  omx_audiodec_component_Private->PortConstructor(openmaxStandComp,&omx_audiodec_component_Private->ports[0],0, OMX_TRUE);
  omx_audiodec_component_Private->PortConstructor(openmaxStandComp,&omx_audiodec_component_Private->ports[1],1, OMX_FALSE);

  /** Domain specific section for the ports. */	
  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = (OMX_STRING)malloc(sizeof(char)*DEFAULT_MIME_STRING_LENGTH);

  //common parameters related to output port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = (OMX_STRING)malloc(sizeof(char)*DEFAULT_MIME_STRING_LENGTH);
  strcpy(omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "raw/audio");
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))   // mp3 format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  else if(!strcmp(cComponentName, AUDIO_DEC_VORBIS_NAME))   // VORBIS format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
		
  else if(!strcmp(cComponentName, AUDIO_DEC_AAC_NAME))   // AAC format decoder   
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;
		
  else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME))// general audio decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;

  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_audiodec_component_Private->avCodecSyncSem) {
    omx_audiodec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_audiodec_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_audiodec_component_Private->avCodecSyncSem, 0);
  }

  omx_audiodec_component_SetInternalParameters(openmaxStandComp);

  //settings of output port
  //output is pcm mode for all decoders - so generalise it 
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
  //setting other parameters of omx_audiodec_component_private	
  omx_audiodec_component_Private->avCodec = NULL;
  omx_audiodec_component_Private->avCodecContext= NULL;
  omx_audiodec_component_Private->avcodecReady = OMX_FALSE;
  omx_audiodec_component_Private->extradata = NULL;
  omx_audiodec_component_Private->extradata_size = 0;

  omx_audiodec_component_Private->BufferMgmtCallback = omx_audiodec_component_BufferMgmtCallback;

  /** first initializing the codec context etc that was done earlier by ffmpeglibinit function */
  avcodec_init();
  av_register_all();
  omx_audiodec_component_Private->avCodecContext = avcodec_alloc_context();
                                         
  omx_audiodec_component_Private->messageHandler = omx_audiodec_component_MessageHandler;
  omx_audiodec_component_Private->destructor = omx_audiodec_component_Destructor;
  openmaxStandComp->SetParameter = omx_audiodec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_audiodec_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_audiodec_component_ComponentRoleEnum;

  noAudioDecInstance++;

  if(noAudioDecInstance>MAX_COMPONENT_AUDIODEC)
    return OMX_ErrorInsufficientResources;

  return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_audiodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) 
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audiodec_component_PortType *pPort;
  OMX_U32 i;

  /* frees port/s */
  if (omx_audiodec_component_Private->sPortTypesParam.nPorts && omx_audiodec_component_Private->ports) {
    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam.nPorts; i++) {
      pPort = (omx_audiodec_component_PortType *)omx_audiodec_component_Private->ports[i];
      if(pPort->sPortParam.format.audio.cMIMEType != NULL) {
        free(pPort->sPortParam.format.audio.cMIMEType);
        pPort->sPortParam.format.audio.cMIMEType = NULL;
      }
    }
  }

  if(omx_audiodec_component_Private->extradata) {
    free(omx_audiodec_component_Private->extradata);
  }

  /*Free Codec Context*/
  av_free (omx_audiodec_component_Private->avCodecContext);

  if(omx_audiodec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_audiodec_component_Private->avCodecSyncSem);
    free(omx_audiodec_component_Private->avCodecSyncSem);
    omx_audiodec_component_Private->avCodecSyncSem=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audiodecoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noAudioDecInstance--;

  return OMX_ErrorNone;
}

/** 
	It initializates the ffmpeg framework, and opens an ffmpeg audiodecoder of type specified by IL client - currently only used for mp3 decoding
*/ 
OMX_ERRORTYPE omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {
  OMX_U32 target_codecID;  // id of ffmpeg codec to be used for different audio formats 

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_audiodec_component_Private->audio_coding_type){
  case OMX_AUDIO_CodingMP3 :
    target_codecID = CODEC_ID_MP3;
    break;
  case OMX_AUDIO_CodingVORBIS :
    target_codecID = CODEC_ID_VORBIS;
    break;
  case OMX_AUDIO_CodingAAC :  
    target_codecID = CODEC_ID_AAC;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than not supported\nCodec not found\n");
    return OMX_ErrorComponentNotFound;
  }
	
  /*Find the  decoder corresponding to the audio type specified by IL client*/
  omx_audiodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_audiodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec Not found\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_audiodec_component_Private->avCodecContext->extradata = omx_audiodec_component_Private->extradata;
  omx_audiodec_component_Private->avCodecContext->extradata_size = (int)omx_audiodec_component_Private->extradata_size;

  /*open the avcodec if mp3,aac,vorbis format selected */
  if (avcodec_open(omx_audiodec_component_Private->avCodecContext, omx_audiodec_component_Private->avCodec) < 0) {
  	DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }

  /* apply flags */
  //omx_audiodec_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  omx_audiodec_component_Private->avCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  omx_audiodec_component_Private->avCodecContext->workaround_bugs |= FF_BUG_AUTODETECT;
 
  	
  tsem_up(omx_audiodec_component_Private->avCodecSyncSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");
  return OMX_ErrorNone;
}

/** 
	It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder
*/
void omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {
	
  avcodec_close(omx_audiodec_component_Private->avCodecContext);
  omx_audiodec_component_Private->extradata_size = 0;
   
}

void omx_audiodec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_audiodec_component_PortType *pPort;

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;

  pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  pPort->sAudioParam.nPortIndex = 1;
  pPort->sAudioParam.nIndex = OMX_IndexParamAudioPcm;
  pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;
	
  if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) 
  {
    strcpy(omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/mpeg");
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

    pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioMp3;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
  } 

  else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS)
  {
    strcpy(omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/vorbis");
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
                                                                                                                             
    setHeader(&omx_audiodec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    omx_audiodec_component_Private->pAudioVorbis.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioVorbis.nChannels = 2;                                                                                                                          
    omx_audiodec_component_Private->pAudioVorbis.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioVorbis.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioVorbis.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audiodec_component_Private->pAudioVorbis.nQuality = 3; //default quality
		
    pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioVorbis;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingVORBIS;

  }
  else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) 
  {
    strcpy(omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/aac");
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
                                                                                                                             
    setHeader(&omx_audiodec_component_Private->pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE)); /* MONA -	comented */
    omx_audiodec_component_Private->pAudioAac.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioAac.nChannels = 2;                                                                                                                          
    omx_audiodec_component_Private->pAudioAac.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioAac.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioAac.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audiodec_component_Private->pAudioAac.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    omx_audiodec_component_Private->pAudioAac.nFrameLength = 0; //encoder decides the framelength
		
    pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioAac;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingAAC;

  }
  else
  	return;

}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_audiodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /*Temporary First Output buffer size*/
  omx_audiodec_component_Private->inputCurrBuffer=NULL;
  omx_audiodec_component_Private->inputCurrLength=0;
  nBufferSize=omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_audiodec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(nBufferSize);
  memset(omx_audiodec_component_Private->internalOutputBuffer, 0, nBufferSize);
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
	omx_audiodec_component_Private->internalOutputBuffer = NULL;

  return err;
}

/** buffer management callback function for mp3 decoding in new standard 
 * of ffmpeg library 
 */
void omx_audiodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) 
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  int output_length;
  OMX_U32 len = 0;

  //DEBUG(DEB_LEV_ERR, "In %s\n",__func__);

  if(omx_audiodec_component_Private->isNewBuffer) {
    omx_audiodec_component_Private->isNewBuffer = 0; 
  }
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset=0;
  /** resetting output length to a predefined value */
  output_length = OUTPUT_LEN_STANDARD_FFMPEG;
#ifdef FFMPEG_DECODER_VERSION
  len  = avcodec_decode_audio2(omx_audiodec_component_Private->avCodecContext,
                              (short*)(pOutputBuffer->pBuffer),
                              &output_length,
                              pInputBuffer->pBuffer,
                              pInputBuffer->nFilledLen);
#else
	len  = avcodec_decode_audio(omx_audiodec_component_Private->avCodecContext,
                              (short*)(pOutputBuffer->pBuffer),
                              &output_length,
                              pInputBuffer->pBuffer,
                              pInputBuffer->nFilledLen);
#endif
  if((omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate != omx_audiodec_component_Private->avCodecContext->sample_rate) ||
     ( omx_audiodec_component_Private->pAudioPcmMode.nChannels!=omx_audiodec_component_Private->avCodecContext->channels)) {
    DEBUG(DEB_LEV_FULL_SEQ, "---->Sending Port Settings Change Event\n");
		/* has mp3 dependency--requires modification */
	  //switch for different audio formats---parameter settings accordingly
    switch(omx_audiodec_component_Private->audio_coding_type)	{
			/*Update Parameter which has changed from avCodecContext*/
    case OMX_AUDIO_CodingMP3 :
      /*pAudioMp3 is for input port Mp3 data*/
      omx_audiodec_component_Private->pAudioMp3.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
      omx_audiodec_component_Private->pAudioMp3.nBitRate = omx_audiodec_component_Private->avCodecContext->bit_rate;
      omx_audiodec_component_Private->pAudioMp3.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
            break;
    case OMX_AUDIO_CodingVORBIS:
      omx_audiodec_component_Private->pAudioVorbis.nChannels = omx_audiodec_component_Private->avCodecContext->channels;                                                                                                                          
      omx_audiodec_component_Private->pAudioVorbis.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
      break;
    case OMX_AUDIO_CodingAAC :  
      /*pAudioAAC is for input port AAC data*/
      omx_audiodec_component_Private->pAudioAac.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
      omx_audiodec_component_Private->pAudioAac.nBitRate = omx_audiodec_component_Private->avCodecContext->bit_rate;
      omx_audiodec_component_Private->pAudioAac.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
      omx_audiodec_component_Private->pAudioAac.eAACStreamFormat = OMX_AUDIO_AACStreamFormatRAW;
      switch(omx_audiodec_component_Private->avCodecContext->profile){   
	case  FF_PROFILE_AAC_MAIN:
	  omx_audiodec_component_Private->pAudioAac.eAACProfile = OMX_AUDIO_AACObjectMain;
	  break;
	case  FF_PROFILE_AAC_LOW:
	  omx_audiodec_component_Private->pAudioAac.eAACProfile = OMX_AUDIO_AACObjectLC;
	  break;
	case  FF_PROFILE_AAC_SSR:
	  omx_audiodec_component_Private->pAudioAac.eAACProfile = OMX_AUDIO_AACObjectSSR;
	  break;
	case  FF_PROFILE_AAC_LTP:
	  omx_audiodec_component_Private->pAudioAac.eAACProfile = OMX_AUDIO_AACObjectLTP;
	  break;
	case  FF_PROFILE_UNKNOWN:
	  omx_audiodec_component_Private->pAudioAac.eAACProfile = OMX_AUDIO_AACObjectNull;
	  break;
      }
            break;
    default :
      DEBUG(DEB_LEV_ERR, "Audio format other than mp3, vorbis or AAC not supported\nCodec type %lu not found\n",omx_audiodec_component_Private->audio_coding_type);
      break;                       
    }//end of switch

    /*pAudioPcmMode is for output port PCM data*/
    omx_audiodec_component_Private->pAudioPcmMode.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
    if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S16)
      omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 16;
    else if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S32)
      omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = 32;
    omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = omx_audiodec_component_Private->avCodecContext->sample_rate;

		/*Send Port Settings changed call back*/
    (*(omx_audiodec_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_audiodec_component_Private->callbackData,
      OMX_EventPortSettingsChanged, /* The command was completed */
      0, 
      1, /* This is the output port index */
      NULL);
	}

  if(len < 0) {
    DEBUG(DEB_LEV_ERR,"error in packet decoding in audio dcoder \n");
  } else {
    /*If output is max length it might be an error, so Don't send output buffer*/
    if((output_length != OUTPUT_LEN_STANDARD_FFMPEG) || (output_length <= pOutputBuffer->nAllocLen)) {
      pOutputBuffer->nFilledLen += output_length;
    }
    pInputBuffer->nFilledLen = 0;
    omx_audiodec_component_Private->isNewBuffer = 1;	
  }

  /** return output buffer */
}

OMX_ERRORTYPE omx_audiodec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; //support for Vorbis format
  OMX_AUDIO_PARAM_AACPROFILETYPE *pAudioAac; //support for AAC format
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_VENDOR_EXTRADATATYPE* pExtradata;
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
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (portIndex <= 1) {
      port = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;	
  case OMX_IndexVendorExtraData :  
    pExtradata = (OMX_VENDOR_EXTRADATATYPE*)ComponentParameterStructure;
    portIndex = pExtradata->nPortIndex;
    if (portIndex <= 1) {
      /** copy the extradata in the codec context private structure */
      omx_audiodec_component_Private->extradata_size = (OMX_U32)pExtradata->nDataSize;
      if(omx_audiodec_component_Private->extradata_size > 0) {
        if(omx_audiodec_component_Private->extradata) {
          free(omx_audiodec_component_Private->extradata);
        }
        omx_audiodec_component_Private->extradata = (unsigned char *)malloc((int)pExtradata->nDataSize*sizeof(char));
        memcpy(omx_audiodec_component_Private->extradata,(unsigned char*)(pExtradata->pData),pExtradata->nDataSize);
      } else {
      		DEBUG(DEB_LEV_SIMPLE_SEQ,"extradata size is 0 !!!\n");
      }	
    } else {
      	return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    portIndex = pAudioPcmMode->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    memcpy(&omx_audiodec_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
    break;

  case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    portIndex = pAudioVorbis->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if(pAudioVorbis->nPortIndex == 0)
      memcpy(&omx_audiodec_component_Private->pAudioVorbis,pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    else
      return OMX_ErrorBadPortIndex;
    break;
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    } else if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
    } else if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_AAC_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;
    } else {
      return OMX_ErrorBadParameter;
    }
    omx_audiodec_component_SetInternalParameters(openmaxStandComp);
    break;
		
  case OMX_IndexParamAudioAac:  
    pAudioAac = (OMX_AUDIO_PARAM_AACPROFILETYPE*) ComponentParameterStructure;
    portIndex = pAudioAac->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (pAudioAac->nPortIndex == 0) {
      memcpy(&omx_audiodec_component_Private->pAudioAac,pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
    portIndex = pAudioMp3->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (pAudioMp3->nPortIndex == 0) {
      memcpy(&omx_audiodec_component_Private->pAudioMp3,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_audiodec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; //support for Vorbis format	
  OMX_AUDIO_PARAM_AACPROFILETYPE *pAudioAac; //support for AAC format	 
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
  omx_audiodec_component_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
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
    memcpy(ComponentParameterStructure, &omx_audiodec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_audiodec_component_PortType *)omx_audiodec_component_Private->ports[pAudioPortFormat->nPortIndex];
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
    memcpy(pAudioPcmMode,&omx_audiodec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    if (pAudioMp3->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioMp3,&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    break;
		
  case OMX_IndexParamAudioAac:  
    pAudioAac = (OMX_AUDIO_PARAM_AACPROFILETYPE*)ComponentParameterStructure;
    if (pAudioAac->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioAac,&omx_audiodec_component_Private->pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    break;
		
  case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    if (pAudioVorbis->nPortIndex != 0) {
       return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_VORBISTYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioVorbis,&omx_audiodec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    break;
	
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }

    if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
    }	else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE);
    }	else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {  
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_AAC_ROLE);
    } else {
      strcpy((char*)pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_audiodec_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_audiodec_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_audiodec_component_Private->state == OMX_StateIdle)) {
      if (!omx_audiodec_component_Private->avcodecReady /* &&  omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3 */) {
        err = omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_audiodec_component_Private->avcodecReady = OMX_TRUE;
      }
    } 
    else if ((message->messageParam == OMX_StateIdle ) && (omx_audiodec_component_Private->state == OMX_StateLoaded)) {
      err = omx_audiodec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Audio Decoder Init Failed Error=%x\n",__func__,err); 
        return err;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_audiodec_component_Private->state == OMX_StateIdle)) {
      err = omx_audiodec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Audio Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      } 
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
   if ((message->messageParam == OMX_StateIdle  ) && (eCurrentState == OMX_StateExecuting)) {
      if (omx_audiodec_component_Private->avcodecReady) {
        omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_Private);
        omx_audiodec_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}

OMX_ERRORTYPE omx_audiodec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex)
{
  if (nIndex == 0) {
    strcpy((char*)cRole, AUDIO_DEC_MP3_ROLE);
  } else if (nIndex == 1) {
    strcpy((char*)cRole, AUDIO_DEC_VORBIS_ROLE);
  } else if (nIndex == 2) {           
    strcpy((char*)cRole, AUDIO_DEC_AAC_ROLE);
  }else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}



