/**
  @file src/components/ffmpeg/omx_audiodec_component.c

  This component implements an AUDIO(MP3/AAC/VORBIS/G726) decoder. The decoder is based on FFmpeg
  software library.

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies)

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
#include <omx_base_audio_port.h>
#include <omx_audiodec_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

/* For FFMPEG_DECODER_VERSION */
#include <config.h>

#define MAX_COMPONENT_AUDIODEC 4

/** output length arguement passed along decoding function */
#define OUTPUT_LEN_STANDARD_FFMPEG 192000

/** Number of Audio Component Instance*/
static OMX_U32 noAudioDecInstance=0;

/** The Constructor
 */
OMX_ERRORTYPE omx_audiodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_base_audio_PortType *pPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_audiodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__, (int)openmaxStandComp->pComponentPrivate);

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audiodec_component_Private->ports = NULL;

  /** Calling base filter constructor */
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);

  omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 0;
  omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  if (omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts && !omx_audiodec_component_Private->ports) {
    omx_audiodec_component_Private->ports = calloc(omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts, sizeof(omx_base_PortType *));
    if (!omx_audiodec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      omx_audiodec_component_Private->ports[i] = calloc(1, sizeof(omx_base_audio_PortType));
      if (!omx_audiodec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_audio_port_Constructor(openmaxStandComp, &omx_audiodec_component_Private->ports[0], 0, OMX_TRUE);
  base_audio_port_Constructor(openmaxStandComp, &omx_audiodec_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports. */
  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  //common parameters related to output port
  pPort = (omx_base_audio_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  pPort->sAudioParam.nIndex    = OMX_IndexParamAudioPcm;
  pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;
  pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  pPort->sPortParam.nBufferSize            = DEFAULT_OUT_BUFFER_SIZE;

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))   // MP3 format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  else if(!strcmp(cComponentName, AUDIO_DEC_VORBIS_NAME))   // VORBIS format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;

  else if(!strcmp(cComponentName, AUDIO_DEC_AAC_NAME))   // AAC format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;

  else if(!strcmp(cComponentName, AUDIO_DEC_G726_NAME))  // G726 format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingG726;

  else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME))// general audio decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;

  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_audiodec_component_Private->avCodecSyncSem) {
    omx_audiodec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_audiodec_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_audiodec_component_Private->avCodecSyncSem, 0);
  }

  //set internal port parameters
  omx_audiodec_component_SetInternalParameters(openmaxStandComp);

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
  openmaxStandComp->SetConfig    = omx_audiodec_component_SetConfig;
  openmaxStandComp->ComponentRoleEnum = omx_audiodec_component_ComponentRoleEnum;
  openmaxStandComp->GetExtensionIndex = omx_audiodec_component_GetExtensionIndex;

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
  OMX_U32 i;

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

  /* frees port/s */
  if (omx_audiodec_component_Private->ports) {
    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      if(omx_audiodec_component_Private->ports[i])
        omx_audiodec_component_Private->ports[i]->PortDestructor(omx_audiodec_component_Private->ports[i]);
    }
    free(omx_audiodec_component_Private->ports);
    omx_audiodec_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audiodecoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noAudioDecInstance--;

  return OMX_ErrorNone;
}

/**
  It initializates the FFmpeg framework, and opens an FFmpeg audiodecoder of type specified by IL client - currently only used for MP3 decoding
*/
OMX_ERRORTYPE omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {
  OMX_U32 target_codecID;  // id of FFmpeg codec to be used for different audio formats

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
  case OMX_AUDIO_CodingG726 :
    target_codecID = CODEC_ID_ADPCM_G726;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than not supported\nCodec not found\n");
    return OMX_ErrorComponentNotFound;
  }

  /*Find the  decoder corresponding to the audio type specified by IL client*/
  omx_audiodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_audiodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec %x Not found\n",target_codecID);
    return OMX_ErrorInsufficientResources;
  }

  omx_audiodec_component_Private->avCodecContext->extradata = omx_audiodec_component_Private->extradata;
  omx_audiodec_component_Private->avCodecContext->extradata_size = (int)omx_audiodec_component_Private->extradata_size;

  //omx_audiodec_component_Private->avCodecContext->channels = 1;
  //omx_audiodec_component_Private->avCodecContext->bit_rate = 12200;
  //omx_audiodec_component_Private->avCodecContext->sample_rate = 8000;
  //omx_audiodec_component_Private->avCodecContext->strict_std_compliance = FF_COMPLIANCE_INOFFICIAL;

  //DEBUG(DEB_LEV_ERR, "Extra Data Size=%d\n",(int)omx_audiodec_component_Private->extradata_size);

  /*open the avcodec if MP3,AAC,VORBIS format selected */
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
  It Deinitializates the FFmpeg framework, and close the FFmpeg MP3 decoder
*/
void omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {

  avcodec_close(omx_audiodec_component_Private->avCodecContext);
  omx_audiodec_component_Private->extradata_size = 0;

}

void omx_audiodec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *pPort = (omx_base_audio_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

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

  if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {

    strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/mpeg");
    pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioMp3;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

    setHeader(&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    omx_audiodec_component_Private->pAudioMp3.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioMp3.nChannels = 2;
    omx_audiodec_component_Private->pAudioMp3.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioMp3.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioMp3.nAudioBandWidth = 0;
    omx_audiodec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    omx_audiodec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

  } else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {

    pPort->sAudioParam.nIndex = OMX_IndexParamAudioVorbis;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingVORBIS;
    strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/vorbis");
    pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;

    setHeader(&omx_audiodec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    omx_audiodec_component_Private->pAudioVorbis.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioVorbis.nChannels = 2;
    omx_audiodec_component_Private->pAudioVorbis.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioVorbis.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioVorbis.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audiodec_component_Private->pAudioVorbis.nQuality = 3; //default quality

  } else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {

    pPort->sAudioParam.nIndex = OMX_IndexParamAudioAac;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingAAC;
    strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/aac");
    pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAAC;

    setHeader(&omx_audiodec_component_Private->pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE)); /* MONA -  comented */
    omx_audiodec_component_Private->pAudioAac.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioAac.nChannels = 2;
    omx_audiodec_component_Private->pAudioAac.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioAac.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioAac.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audiodec_component_Private->pAudioAac.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    omx_audiodec_component_Private->pAudioAac.nFrameLength = 0; //encoder decides the framelength

  } else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingG726) {

    strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/g726");
    pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingG726;

    pPort->sAudioParam.nIndex = OMX_IndexParamAudioG726;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingG726;

    setHeader(&omx_audiodec_component_Private->pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
    omx_audiodec_component_Private->pAudioG726.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioG726.nChannels = 1;
    omx_audiodec_component_Private->pAudioG726.eG726Mode = OMX_AUDIO_G726Mode16;

  } else {
    return;
  }
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
  omx_audiodec_component_Private->internalOutputBuffer = calloc(1,nBufferSize);
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

/** buffer management callback function for MP3 decoding in new standard
 * of FFmpeg library
 */
void omx_audiodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  int output_length, len;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  if(omx_audiodec_component_Private->isNewBuffer) {
    omx_audiodec_component_Private->isNewBuffer = 0;
  }
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset=0;
  /** resetting output length to a predefined value */
  output_length = OUTPUT_LEN_STANDARD_FFMPEG;
#if FFMPEG_DECODER_VERSION >= 2
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
    /* has MP3 dependency--requires modification */
    //switch for different audio formats---parameter settings accordingly
    switch(omx_audiodec_component_Private->audio_coding_type)  {
      /*Update Parameter which has changed from avCodecContext*/
    case OMX_AUDIO_CodingMP3 :
      /*pAudioMp3 is for input port MP3 data*/
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
    case OMX_AUDIO_CodingG726:
      omx_audiodec_component_Private->pAudioG726.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "Audio format other than MP3, VORBIS or AAC not supported\nCodec type %lu not found\n",omx_audiodec_component_Private->audio_coding_type);
      break;
    }//end of switch

    /*pAudioPcmMode is for output port PCM data*/
    omx_audiodec_component_Private->pAudioPcmMode.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
    if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S16)
      omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 16;
    else if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S32)
      omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 32;
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
    DEBUG(DEB_LEV_ERR,"error in packet decoding in audio decoder \n");
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
  OMX_AUDIO_PARAM_G726TYPE *pAudioG726;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *port;
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
      port = (omx_base_audio_PortType *) omx_audiodec_component_Private->ports[portIndex];
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

    if (omx_audiodec_component_Private->state != OMX_StateLoaded && omx_audiodec_component_Private->state != OMX_StateWaitForResources) {
      DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_audiodec_component_Private->state,__LINE__);
      return OMX_ErrorIncorrectStateOperation;
    }

    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
      break;
    }

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
  case OMX_IndexParamAudioG726:
    pAudioG726 = (OMX_AUDIO_PARAM_G726TYPE*) ComponentParameterStructure;
    portIndex = pAudioG726->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
      break;
    }
    if (pAudioG726->nPortIndex == 0) {
      memcpy(&omx_audiodec_component_Private->pAudioG726,pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
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
  OMX_AUDIO_PARAM_G726TYPE *pAudioG726;
  omx_base_audio_PortType *port;
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
    memcpy(ComponentParameterStructure, &omx_audiodec_component_Private->sPortTypesParam[OMX_PortDomainAudio], sizeof(OMX_PORT_PARAM_TYPE));
    break;
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_base_audio_PortType *)omx_audiodec_component_Private->ports[pAudioPortFormat->nPortIndex];
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

  case OMX_IndexParamAudioG726:
    pAudioG726 = (OMX_AUDIO_PARAM_G726TYPE*) ComponentParameterStructure;
    if (pAudioG726->nPortIndex != 0) {
       return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_G726TYPE))) != OMX_ErrorNone) {
      break;
    }
    memcpy(pAudioG726,&omx_audiodec_component_Private->pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
      break;
    }

    if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
    }  else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE);
    }  else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {
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


OMX_ERRORTYPE omx_audiodec_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VENDOR_EXTRADATATYPE* pExtradata;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);
  /* Check which structure we are being fed and fill its header */
  switch (nIndex) {
    case OMX_IndexVendorAudioExtraData :
      pExtradata = (OMX_VENDOR_EXTRADATATYPE*)pComponentConfigStructure;
      if (pExtradata->nPortIndex <= 1) {
        /** copy the extradata in the codec context private structure */
        omx_audiodec_component_Private->extradata_size = (OMX_U32)pExtradata->nDataSize;
        if(omx_audiodec_component_Private->extradata_size > 0) {
          if(omx_audiodec_component_Private->extradata) {
            free(omx_audiodec_component_Private->extradata);
          }
          omx_audiodec_component_Private->extradata = malloc((int)pExtradata->nDataSize);
          memcpy(omx_audiodec_component_Private->extradata, pExtradata->pData,pExtradata->nDataSize);
        } else {
            DEBUG(DEB_LEV_SIMPLE_SEQ,"extradata size is 0 !!!\n");
        }
      } else {
          return OMX_ErrorBadPortIndex;
      }
      break;

    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_audiodec_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  if(strcmp(cParameterName,"OMX.ST.index.config.audioextradata") == 0) {
    *pIndexType = OMX_IndexVendorAudioExtraData;
  } else {
    return OMX_ErrorBadParameter;
  }
  return OMX_ErrorNone;
}



