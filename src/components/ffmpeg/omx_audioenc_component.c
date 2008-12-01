/**
  @file src/components/ffmpeg/omx_audioenc_component.c

  This component implements an audio (MP3/AAC/G726) encoder. The encoder is based on ffmpeg
  software library.

  Copyright (C) 2008  STMicroelectronics and Nokia

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

  $Date: 2008-06-17 12:39:25 +0530 (Tue, 17 Jun 2008) $
  Revision $Rev: 1401 $
  Author $Author: pankaj_sen $
*/

#include <omxcore.h>
#include <omx_base_audio_port.h>
#include <omx_audioenc_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

/* For FFMPEG_ENCODER_VERSION */
#include <config.h>

#define MAX_COMPONENT_AUDIOENC 4

/** Number of Audio Component Instance*/
static OMX_U32 noaudioencInstance=0;

/** The Constructor 
 */
OMX_ERRORTYPE omx_audioenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_audioenc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else 
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__, (int)openmaxStandComp->pComponentPrivate);
  
  omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audioenc_component_Private->ports = NULL;

  /** Calling base filter constructor */
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);
  
  omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 0;
  omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 2;

  /** Allocate Ports and call port constructor. */  
  if (omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts && !omx_audioenc_component_Private->ports) {
    omx_audioenc_component_Private->ports = calloc(omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts, sizeof(omx_base_PortType *));
    if (!omx_audioenc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      omx_audioenc_component_Private->ports[i] = calloc(1, sizeof(omx_base_audio_PortType));
      if (!omx_audioenc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_audio_port_Constructor(openmaxStandComp, &omx_audioenc_component_Private->ports[0], 0, OMX_TRUE);
  base_audio_port_Constructor(openmaxStandComp, &omx_audioenc_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports. */  
  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_audioenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize =  DEFAULT_OUT_BUFFER_SIZE;
  //common parameters related to output port
  omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_ENC_MP3_NAME))   // mp3 format encoder
    omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  else if(!strcmp(cComponentName, AUDIO_ENC_AAC_NAME))   // AAC format encoder   
    omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;

  else if(!strcmp(cComponentName, AUDIO_ENC_G726_NAME))  // G726 format encoder   
    omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingG726;
    
  else if (!strcmp(cComponentName, AUDIO_ENC_BASE_NAME))// general audio encoder
    omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;

  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_audioenc_component_Private->avCodecSyncSem) {
    omx_audioenc_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_audioenc_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_audioenc_component_Private->avCodecSyncSem, 0);
  }

  omx_audioenc_component_SetInternalParameters(openmaxStandComp);

  //settings of output port
  //output is pcm mode for all encoders - so generalise it 
  setHeader(&omx_audioenc_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_audioenc_component_Private->pAudioPcmMode.nPortIndex = 0;
  omx_audioenc_component_Private->pAudioPcmMode.nChannels = 2;
  omx_audioenc_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_audioenc_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_audioenc_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_audioenc_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_audioenc_component_Private->pAudioPcmMode.nSamplingRate = 44100;
  omx_audioenc_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_audioenc_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_audioenc_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  //general configuration irrespective of any audio formats
  //setting other parameters of omx_audioenc_component_private  
  omx_audioenc_component_Private->avCodec = NULL;
  omx_audioenc_component_Private->avCodecContext= NULL;
  omx_audioenc_component_Private->avcodecReady = OMX_FALSE;

  omx_audioenc_component_Private->BufferMgmtCallback = omx_audioenc_component_BufferMgmtCallback;

  /** first initializing the codec context etc that was done earlier by ffmpeglibinit function */
  avcodec_init();
  av_register_all();
  omx_audioenc_component_Private->avCodecContext = avcodec_alloc_context();
                                         
  omx_audioenc_component_Private->messageHandler = omx_audioenc_component_MessageHandler;
  omx_audioenc_component_Private->destructor = omx_audioenc_component_Destructor;
  openmaxStandComp->SetParameter = omx_audioenc_component_SetParameter;
  openmaxStandComp->GetParameter = omx_audioenc_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_audioenc_component_ComponentRoleEnum;
  
  noaudioencInstance++;

  if(noaudioencInstance>MAX_COMPONENT_AUDIOENC)
    return OMX_ErrorInsufficientResources;

  return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_audioenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) 
{
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;
  
  /*Free Codec Context*/
  av_free (omx_audioenc_component_Private->avCodecContext);

  if(omx_audioenc_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_audioenc_component_Private->avCodecSyncSem);
    free(omx_audioenc_component_Private->avCodecSyncSem);
    omx_audioenc_component_Private->avCodecSyncSem=NULL;
  }

  /* frees port/s */
  if (omx_audioenc_component_Private->ports) {
    for (i=0; i < omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      if(omx_audioenc_component_Private->ports[i])
        omx_audioenc_component_Private->ports[i]->PortDestructor(omx_audioenc_component_Private->ports[i]);
    }
    free(omx_audioenc_component_Private->ports);
    omx_audioenc_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audioencoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noaudioencInstance--;

  return OMX_ErrorNone;
}

/** 
  It initializates the ffmpeg framework, and opens an ffmpeg audioencoder of type specified by IL client - currently only used for mp3 encoding
*/ 
OMX_ERRORTYPE omx_audioenc_component_ffmpegLibInit(omx_audioenc_component_PrivateType* omx_audioenc_component_Private) {
  OMX_U32 target_codecID;  // id of ffmpeg codec to be used for different audio formats 

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_audioenc_component_Private->audio_coding_type){
  case OMX_AUDIO_CodingMP3 :
    target_codecID = CODEC_ID_MP3;
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
  
  /*Find the  encoder corresponding to the audio type specified by IL client*/
  omx_audioenc_component_Private->avCodec = avcodec_find_encoder(target_codecID);
  if (omx_audioenc_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec %x Not found \n",(int)target_codecID);
    return OMX_ErrorInsufficientResources;
  }

  /* put sample parameters */
  switch(omx_audioenc_component_Private->audio_coding_type) {
  case OMX_AUDIO_CodingMP3 :
    omx_audioenc_component_Private->avCodecContext->channels = omx_audioenc_component_Private->pAudioMp3.nChannels;
    omx_audioenc_component_Private->avCodecContext->bit_rate = (int)omx_audioenc_component_Private->pAudioMp3.nBitRate;
    omx_audioenc_component_Private->avCodecContext->sample_rate = omx_audioenc_component_Private->pAudioMp3.nSampleRate;
    omx_audioenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S16;
    break;
  case OMX_AUDIO_CodingAAC :  
    omx_audioenc_component_Private->avCodecContext->channels = omx_audioenc_component_Private->pAudioAac.nChannels;
    omx_audioenc_component_Private->avCodecContext->bit_rate = (int)omx_audioenc_component_Private->pAudioAac.nBitRate;
    omx_audioenc_component_Private->avCodecContext->sample_rate = omx_audioenc_component_Private->pAudioAac.nSampleRate;
    omx_audioenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S16;
    break;
  case OMX_AUDIO_CodingG726 :
    omx_audioenc_component_Private->avCodecContext->channels = omx_audioenc_component_Private->pAudioG726.nChannels;
    switch(omx_audioenc_component_Private->pAudioG726.eG726Mode) {
    case OMX_AUDIO_G726Mode16 :          /**< 16 kbps */
      omx_audioenc_component_Private->avCodecContext->bit_rate = 16000;
      break;
    case OMX_AUDIO_G726Mode24 :          /**< 24 kbps */
      omx_audioenc_component_Private->avCodecContext->bit_rate = 24000;
      break;
    case OMX_AUDIO_G726Mode32 :          /**< 32 kbps, most common rate, also G721 */
      omx_audioenc_component_Private->avCodecContext->bit_rate = 32000;
      break;
    case OMX_AUDIO_G726Mode40 :
      omx_audioenc_component_Private->avCodecContext->bit_rate = 40000;
      break;
    default:
      omx_audioenc_component_Private->avCodecContext->bit_rate = 16000;
      break;
    }
    omx_audioenc_component_Private->avCodecContext->sample_rate = 8000;
    omx_audioenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S16;
    omx_audioenc_component_Private->avCodecContext->strict_std_compliance = FF_COMPLIANCE_STRICT;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than not MP3/AAC/G726 is supported\n");
    break;
  }
  
  DEBUG(DEB_LEV_FULL_SEQ, "In %s Coding Type=%x target id=%x\n",__func__,(int)omx_audioenc_component_Private->audio_coding_type,(int)target_codecID);
  /*open the avcodec if mp3,aac,g726 format selected */
  if (avcodec_open(omx_audioenc_component_Private->avCodecContext, omx_audioenc_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }

  /* apply flags */
  //omx_audioenc_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  
  //omx_audioenc_component_Private->avCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  omx_audioenc_component_Private->avCodecContext->workaround_bugs |= FF_BUG_AUTODETECT;
  
  omx_audioenc_component_Private->temp_buffer = malloc(DEFAULT_OUT_BUFFER_SIZE*2); 
  omx_audioenc_component_Private->temp_buffer_filledlen=0;
 
  if(omx_audioenc_component_Private->avCodecContext->frame_size == 0) {
    omx_audioenc_component_Private->avCodecContext->frame_size = 80;
  }
  DEBUG(DEB_LEV_ERR, "In %s frame size=%d\n",__func__,omx_audioenc_component_Private->avCodecContext->frame_size);
  omx_audioenc_component_Private->frame_length = omx_audioenc_component_Private->avCodecContext->frame_size*2*omx_audioenc_component_Private->avCodecContext->channels;
  
  tsem_up(omx_audioenc_component_Private->avCodecSyncSem);
  
  return OMX_ErrorNone;
}

/** 
  It Deinitializates the ffmpeg framework, and close the ffmpeg encoder
*/
void omx_audioenc_component_ffmpegLibDeInit(omx_audioenc_component_PrivateType* omx_audioenc_component_Private) {
  
  avcodec_close(omx_audioenc_component_Private->avCodecContext);

  free(omx_audioenc_component_Private->temp_buffer);
   
}

void omx_audioenc_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private;
  omx_base_audio_PortType *pPort;

  omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;

  pPort = (omx_base_audio_PortType *) omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  if (omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
    strcpy(omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/mpeg");
    omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

    setHeader(&omx_audioenc_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
    omx_audioenc_component_Private->pAudioMp3.nPortIndex = 1;                                                                    
    omx_audioenc_component_Private->pAudioMp3.nChannels = 2;                                                                    
    omx_audioenc_component_Private->pAudioMp3.nBitRate = 128000;                                                                  
    omx_audioenc_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
    omx_audioenc_component_Private->pAudioMp3.nAudioBandWidth = 0;
    omx_audioenc_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    omx_audioenc_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

    pPort->sAudioParam.nIndex = OMX_IndexParamAudioMp3;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
  } else if(omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {
    strcpy(omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/aac");
    omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
                                                                                                                             
    setHeader(&omx_audioenc_component_Private->pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    omx_audioenc_component_Private->pAudioAac.nPortIndex = 1;
    omx_audioenc_component_Private->pAudioAac.nChannels = 2;                                                                                                                          
    omx_audioenc_component_Private->pAudioAac.nBitRate = 128000;
    omx_audioenc_component_Private->pAudioAac.nSampleRate = 44100;
    omx_audioenc_component_Private->pAudioAac.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audioenc_component_Private->pAudioAac.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    omx_audioenc_component_Private->pAudioAac.nFrameLength = 0; //encoder decides the framelength
    
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioAac;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingAAC;

  } else if(omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingG726) {
    strcpy(omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/g726");
    omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingG726;
                                                                                                                             
    setHeader(&omx_audioenc_component_Private->pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE)); 
    omx_audioenc_component_Private->pAudioG726.nPortIndex = 1;
    omx_audioenc_component_Private->pAudioG726.nChannels = 1;                                                                                                                          
    omx_audioenc_component_Private->pAudioG726.eG726Mode = OMX_AUDIO_G726Mode16;
    
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioG726;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingG726;

  } else
    return;

}

//int headersent=0;
//unsigned char *tmp;
/** The Initialization function 
 */
OMX_ERRORTYPE omx_audioenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)
{
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /*Temporary First Output buffer size*/
  omx_audioenc_component_Private->inputCurrBuffer=NULL;
  omx_audioenc_component_Private->inputCurrLength=0;
  nBufferSize=omx_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_audioenc_component_Private->internalOutputBuffer = malloc(nBufferSize);
  memset(omx_audioenc_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_audioenc_component_Private->positionInOutBuf = 0;
  omx_audioenc_component_Private->isNewBuffer=1;
  omx_audioenc_component_Private->isFirstBuffer = 1;

  return err;
  
};

/** The Deinitialization function 
 */
OMX_ERRORTYPE omx_audioenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (omx_audioenc_component_Private->avcodecReady) {
    omx_audioenc_component_ffmpegLibDeInit(omx_audioenc_component_Private);
    omx_audioenc_component_Private->avcodecReady = OMX_FALSE;
  }

  free(omx_audioenc_component_Private->internalOutputBuffer);
  omx_audioenc_component_Private->internalOutputBuffer = NULL;

//  free(tmp);

  return err;
}

/** buffer management callback function for encoding in new standard 
 * of ffmpeg library 
 */
void omx_audioenc_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) 
{
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  int nLen;
  OMX_S32 nOutputFilled = 0;
  static OMX_U8* data;
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n",__func__);

  /*If temporary buffer exist then process that first*/
  if(omx_audioenc_component_Private->temp_buffer_filledlen > 0 && omx_audioenc_component_Private->isNewBuffer) {
    nLen = omx_audioenc_component_Private->frame_length-omx_audioenc_component_Private->temp_buffer_filledlen;
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s copying residue buffer %d\n",__func__,nLen);
    memcpy(&omx_audioenc_component_Private->temp_buffer[omx_audioenc_component_Private->temp_buffer_filledlen],pInputBuffer->pBuffer,nLen);
    omx_audioenc_component_Private->isNewBuffer = 0; 
    data = &pInputBuffer->pBuffer[nLen];
    pInputBuffer->nFilledLen -= nLen;
    omx_audioenc_component_Private->temp_buffer_filledlen = 0;

    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Using Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d temp_buffer_filledlen=%d\n",
      __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen,(int)omx_audioenc_component_Private->temp_buffer_filledlen);

    while (!nOutputFilled) {
      omx_audioenc_component_Private->avCodecContext->frame_number++;

      nLen = avcodec_encode_audio(omx_audioenc_component_Private->avCodecContext,
                                  pOutputBuffer->pBuffer,
                                  omx_audioenc_component_Private->avCodecContext->frame_size,
                                  (short*)omx_audioenc_component_Private->temp_buffer);

      if (nLen < 0) {
        DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not encoded?\n");
      }

      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Consumed 2 frame_length=%d,pInputBuffer->nFilledLen=%d nLen=%d\n",
      __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen,nLen);
    
      if ( nLen >= 0) {
        pOutputBuffer->nFilledLen += nLen;
      } 
      nOutputFilled = 1;
    }

    if((pInputBuffer->nFilledLen > 0) && (pInputBuffer->nFilledLen < omx_audioenc_component_Private->frame_length)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Copying to Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d\n",
        __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen);
      memcpy(omx_audioenc_component_Private->temp_buffer,data,pInputBuffer->nFilledLen);
      omx_audioenc_component_Private->temp_buffer_filledlen = pInputBuffer->nFilledLen;
      pInputBuffer->nFilledLen = 0;
      omx_audioenc_component_Private->isNewBuffer = 1;
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s 2 pInputBuffer->nFilledLen=%d nLen=%d \n",
      __func__,(int)pInputBuffer->nFilledLen,(int)nLen);
    //return;
  } else {

    if(omx_audioenc_component_Private->isNewBuffer) {
      omx_audioenc_component_Private->isNewBuffer = 0; 
      data = pInputBuffer->pBuffer;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s frame_length=%d,pInputBuffer->nFilledLen=%d temp_buffer_filledlen=%d\n",
      __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen,(int)omx_audioenc_component_Private->temp_buffer_filledlen);

      
    while (!nOutputFilled) {
      if (!omx_audioenc_component_Private->avcodecReady) {
        tsem_down(omx_audioenc_component_Private->avCodecSyncSem);
      }
      omx_audioenc_component_Private->avCodecContext->frame_number++;

      nLen = avcodec_encode_audio(omx_audioenc_component_Private->avCodecContext,
                                  pOutputBuffer->pBuffer,
                                  omx_audioenc_component_Private->avCodecContext->frame_size,
                                  (short*)data);

      if (nLen < 0) {
        DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not encoded?\n");
      }

      pInputBuffer->nFilledLen -= omx_audioenc_component_Private->frame_length;

      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Consumed 1 frame_length=%d,pInputBuffer->nFilledLen=%d nLen=%d\n",
        __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen,(int)nLen);
    
      if ( nLen >= 0) {
        pOutputBuffer->nFilledLen += nLen;
      } 
      nOutputFilled = 1;
    }

    if(pInputBuffer->nFilledLen != 0) {
      data +=omx_audioenc_component_Private->frame_length;
    }

    if((pInputBuffer->nFilledLen > 0) && (pInputBuffer->nFilledLen < omx_audioenc_component_Private->frame_length)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Copying to Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d\n",
        __func__,(int)omx_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen);
      memcpy(omx_audioenc_component_Private->temp_buffer,data,pInputBuffer->nFilledLen);
      omx_audioenc_component_Private->temp_buffer_filledlen = pInputBuffer->nFilledLen;
      pInputBuffer->nFilledLen = 0;
      omx_audioenc_component_Private->isNewBuffer = 1;
    }

    if(pInputBuffer->nFilledLen == 0) {
      omx_audioenc_component_Private->isNewBuffer = 1;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s 1 pInputBuffer->nFilledLen=%d nLen=%d \n",__func__,(int)pInputBuffer->nFilledLen,nLen);
    /*
    if(omx_audioenc_component_Private->avCodecContext->extradata_size >0 && !headersent) {
      headersent =1;
      DEBUG(DEB_LEV_ERR, "In %s extradata size=%d Len=%d alloclen=%d\n",__func__,omx_audioenc_component_Private->avCodecContext->extradata_size,nLen,pOutputBuffer->nAllocLen);
      memcpy(tmp,pOutputBuffer->pBuffer,nLen);
      memcpy(pOutputBuffer->pBuffer,omx_audioenc_component_Private->avCodecContext->extradata,omx_audioenc_component_Private->avCodecContext->extradata_size);
      memcpy(&pOutputBuffer->pBuffer[omx_audioenc_component_Private->avCodecContext->extradata_size],tmp,nLen);
      pOutputBuffer->nFilledLen += omx_audioenc_component_Private->avCodecContext->extradata_size;
    }
    */
  }

  /** return output buffer */
}

OMX_ERRORTYPE omx_audioenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_AUDIO_PARAM_AACPROFILETYPE *pAudioAac; //support for AAC format
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_G726TYPE *pAudioG726;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
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
      port = (omx_base_audio_PortType *) omx_audioenc_component_Private->ports[portIndex];
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
    if(pAudioPcmMode->nPortIndex == 0)
      memcpy(&omx_audioenc_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    else
      return OMX_ErrorBadPortIndex;
    break;

  case OMX_IndexParamAudioAac:  
    pAudioAac = (OMX_AUDIO_PARAM_AACPROFILETYPE*) ComponentParameterStructure;
    portIndex = pAudioAac->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (pAudioAac->nPortIndex == 1) {
      memcpy(&omx_audioenc_component_Private->pAudioAac,pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
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
    if (pAudioMp3->nPortIndex == 1) {
      memcpy(&omx_audioenc_component_Private->pAudioMp3,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
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
    if (pAudioG726->nPortIndex == 1) {
      memcpy(&omx_audioenc_component_Private->pAudioG726,pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp((char*)pComponentRole->cRole, AUDIO_ENC_MP3_ROLE)) {
      omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    } else if (!strcmp((char*)pComponentRole->cRole, AUDIO_ENC_AAC_ROLE)) {
      omx_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;
    } else {
      return OMX_ErrorBadParameter;
    }
    omx_audioenc_component_SetInternalParameters(openmaxStandComp);
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_audioenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;  
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_AUDIO_PARAM_AACPROFILETYPE *pAudioAac; //support for AAC format   
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
  OMX_AUDIO_PARAM_G726TYPE *pAudioG726;
  omx_base_audio_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = (omx_audioenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
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
    memcpy(ComponentParameterStructure, &omx_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio], sizeof(OMX_PORT_PARAM_TYPE));
    break;    

  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_base_audio_PortType *)omx_audioenc_component_Private->ports[pAudioPortFormat->nPortIndex];
      memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;  
    
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    if (pAudioPcmMode->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioPcmMode,&omx_audioenc_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;

  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    if (pAudioMp3->nPortIndex != 1) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioMp3,&omx_audioenc_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    break;
    
  case OMX_IndexParamAudioAac:  
    pAudioAac = (OMX_AUDIO_PARAM_AACPROFILETYPE*)ComponentParameterStructure;
    if (pAudioAac->nPortIndex != 1) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioAac,&omx_audioenc_component_Private->pAudioAac,sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
    break;

  case OMX_IndexParamAudioG726:
    pAudioG726 = (OMX_AUDIO_PARAM_G726TYPE*) ComponentParameterStructure;
    if (pAudioG726->nPortIndex != 1) {
       return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_G726TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioG726,&omx_audioenc_component_Private->pAudioG726,sizeof(OMX_AUDIO_PARAM_G726TYPE));
    break;
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }

    if (omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy((char*)pComponentRole->cRole, AUDIO_ENC_MP3_ROLE);
    } else if (omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {  
      strcpy((char*)pComponentRole->cRole, AUDIO_ENC_AAC_ROLE);
    } else if (omx_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingG726) {  
      strcpy((char*)pComponentRole->cRole, AUDIO_ENC_G726_ROLE);
    } else {
      strcpy((char*)pComponentRole->cRole,"\0");;
    }
    break;

  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_audioenc_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)
{
  omx_audioenc_component_PrivateType* omx_audioenc_component_Private = (omx_audioenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_audioenc_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_audioenc_component_Private->state == OMX_StateIdle)) {
      if (!omx_audioenc_component_Private->avcodecReady) {
        err = omx_audioenc_component_ffmpegLibInit(omx_audioenc_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_audioenc_component_Private->avcodecReady = OMX_TRUE;
      }
    } 
    else if ((message->messageParam == OMX_StateIdle ) && (omx_audioenc_component_Private->state == OMX_StateLoaded)) {
      err = omx_audioenc_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Audio Encoder Init Failed Error=%x\n",__func__,err); 
        return err;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_audioenc_component_Private->state == OMX_StateIdle)) {
      err = omx_audioenc_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Audio Encoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      } 
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
   if ((message->messageParam == OMX_StateIdle  ) && (eCurrentState == OMX_StateExecuting)) {
      if (omx_audioenc_component_Private->avcodecReady) {
        omx_audioenc_component_ffmpegLibDeInit(omx_audioenc_component_Private);
        omx_audioenc_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}

OMX_ERRORTYPE omx_audioenc_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex)
{
  if (nIndex == 0) {
    strcpy((char*)cRole, AUDIO_ENC_MP3_ROLE);
  } else if (nIndex == 1) {
    strcpy((char*)cRole, AUDIO_ENC_AAC_ROLE);
  } else if (nIndex == 3) {           
    strcpy((char*)cRole, AUDIO_ENC_G726_ROLE);
  } else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}
