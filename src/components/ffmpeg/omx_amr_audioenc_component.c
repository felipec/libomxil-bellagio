/**
  @file src/components/ffmpeg/omx_amr_audioenc_component.c

  This component implements an audio(AMR) encoder. The encoder is based on ffmpeg
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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#include <omxcore.h>
#include <omx_base_audio_port.h>
#include <omx_amr_audioenc_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

/* For FFMPEG_ENCODER_VERSION */
#include <config.h>

static const char AMR_header [] = "#!AMR\n";
static const char AMRWB_header [] = "#!AMR-WB\n";

#define MAX_COMPONENT_AUDIOENC 4

/** Number of Audio Component Instance*/
static OMX_U32 noamr_audioencInstance=0;

/** The Constructor 
 */
OMX_ERRORTYPE omx_amr_audioenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_amr_audioenc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else 
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__, (int)openmaxStandComp->pComponentPrivate);
  
  omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_amr_audioenc_component_Private->ports = NULL;

  /** Calling base filter constructor */
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);
  
  omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 0;
  omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 2;

  /** Allocate Ports and call port constructor. */  
  if (omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts && !omx_amr_audioenc_component_Private->ports) {
    omx_amr_audioenc_component_Private->ports = calloc(omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts, sizeof(omx_base_PortType *));
    if (!omx_amr_audioenc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      omx_amr_audioenc_component_Private->ports[i] = calloc(1, sizeof(omx_base_audio_PortType));
      if (!omx_amr_audioenc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_amr_audioenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize =  DEFAULT_OUT_BUFFER_SIZE;
  //common parameters related to output port
  omx_amr_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  
  base_audio_port_Constructor(openmaxStandComp, &omx_amr_audioenc_component_Private->ports[0], 0, OMX_TRUE);
  base_audio_port_Constructor(openmaxStandComp, &omx_amr_audioenc_component_Private->ports[1], 1, OMX_FALSE);

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_ENC_AMR_NAME))  // AMR format encoder
    omx_amr_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingAMR;
  /** Domain specific section for the ports. */  
  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_amr_audioenc_component_Private->avCodecSyncSem) {
    omx_amr_audioenc_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_amr_audioenc_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_amr_audioenc_component_Private->avCodecSyncSem, 0);
  }

  omx_amr_audioenc_component_SetInternalParameters(openmaxStandComp);

  //general configuration irrespective of any audio formats
  //setting other parameters of omx_amr_audioenc_component_private  
  omx_amr_audioenc_component_Private->avCodec = NULL;
  omx_amr_audioenc_component_Private->avCodecContext= NULL;
  omx_amr_audioenc_component_Private->avcodecReady = OMX_FALSE;

  omx_amr_audioenc_component_Private->BufferMgmtCallback = omx_amr_audioenc_component_BufferMgmtCallback;

  /** first initializing the codec context etc that was done earlier by ffmpeglibinit function */
  avcodec_init();
  av_register_all();
  omx_amr_audioenc_component_Private->avCodecContext = avcodec_alloc_context();
                                         
  omx_amr_audioenc_component_Private->messageHandler = omx_amr_audioenc_component_MessageHandler;
  omx_amr_audioenc_component_Private->destructor = omx_amr_audioenc_component_Destructor;
  openmaxStandComp->SetParameter = omx_amr_audioenc_component_SetParameter;
  openmaxStandComp->GetParameter = omx_amr_audioenc_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_amr_audioenc_component_ComponentRoleEnum;
  
  noamr_audioencInstance++;

  if(noamr_audioencInstance>MAX_COMPONENT_AUDIOENC)
    return OMX_ErrorInsufficientResources;

  return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_amr_audioenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) 
{
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;
  
  /*Free Codec Context*/
  av_free (omx_amr_audioenc_component_Private->avCodecContext);

  if(omx_amr_audioenc_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_amr_audioenc_component_Private->avCodecSyncSem);
    free(omx_amr_audioenc_component_Private->avCodecSyncSem);
    omx_amr_audioenc_component_Private->avCodecSyncSem=NULL;
  }

  /* frees port/s */
  if (omx_amr_audioenc_component_Private->ports) {
    for (i=0; i < omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts; i++) {
      if(omx_amr_audioenc_component_Private->ports[i])
        omx_amr_audioenc_component_Private->ports[i]->PortDestructor(omx_amr_audioenc_component_Private->ports[i]);
    }
    free(omx_amr_audioenc_component_Private->ports);
    omx_amr_audioenc_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of amr_audioencoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noamr_audioencInstance--;

  return OMX_ErrorNone;
}

/** 
  It initializates the ffmpeg framework, and opens an ffmpeg amr_audioencoder of type specified by IL client - currently only used for AMR encoding
*/ 
OMX_ERRORTYPE omx_amr_audioenc_component_ffmpegLibInit(omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private) {
  OMX_U32 target_codecID = 0;  // id of ffmpeg codec to be used for different audio formats 

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_amr_audioenc_component_Private->audio_coding_type){
  case OMX_AUDIO_CodingAMR :
    if(omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
      target_codecID = CODEC_ID_AMR_NB;
    } else if(omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8) {
      target_codecID = CODEC_ID_AMR_WB;
      omx_amr_audioenc_component_Private->pAudioPcmMode.nSamplingRate = 16000; /*AMR - WB Support only 16k Sample Rate*/
    } 
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than not supported\nCodec not found\n");
    return OMX_ErrorComponentNotFound;
  }
  
  DEBUG(DEB_ALL_MESS, "Opening Codec %x ch=%d,rate=%d,sr=%d Band Mode=%d bps=%d\n",(int)target_codecID,
    (int)omx_amr_audioenc_component_Private->pAudioAmr.nChannels,
    (int)omx_amr_audioenc_component_Private->pAudioAmr.nBitRate,
    (int)omx_amr_audioenc_component_Private->pAudioPcmMode.nSamplingRate,
    (int)omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode,
    (int)omx_amr_audioenc_component_Private->pAudioPcmMode.nBitPerSample);

  /*Find the  encoder corresponding to the audio type specified by IL client*/
  omx_amr_audioenc_component_Private->avCodec = avcodec_find_encoder(target_codecID);
  if (omx_amr_audioenc_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec %x Not found \n",(int)target_codecID);
    return OMX_ErrorInsufficientResources;
  }

  /* put sample parameters */
  switch(omx_amr_audioenc_component_Private->audio_coding_type) {
  case OMX_AUDIO_CodingAMR :
    omx_amr_audioenc_component_Private->avCodecContext->channels = omx_amr_audioenc_component_Private->pAudioPcmMode.nChannels;
    omx_amr_audioenc_component_Private->avCodecContext->bit_rate = (int)omx_amr_audioenc_component_Private->pAudioAmr.nBitRate;
    omx_amr_audioenc_component_Private->avCodecContext->sample_rate = omx_amr_audioenc_component_Private->pAudioPcmMode.nSamplingRate;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than not AMR is supported\n");
    break;
  }

  if(omx_amr_audioenc_component_Private->pAudioPcmMode.nBitPerSample == 16) {
    omx_amr_audioenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S16;
  } else if(omx_amr_audioenc_component_Private->pAudioPcmMode.nBitPerSample == 32) {
    omx_amr_audioenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S32;
  }
  
  DEBUG(DEB_LEV_FULL_SEQ, "In %s Coding Type=%x target id=%x\n",__func__,(int)omx_amr_audioenc_component_Private->audio_coding_type,(int)target_codecID);
  /*open the avcodec if amr selected */
  if (avcodec_open(omx_amr_audioenc_component_Private->avCodecContext, omx_amr_audioenc_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }

  /* apply flags */
  //omx_amr_audioenc_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  
  //omx_amr_audioenc_component_Private->avCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  omx_amr_audioenc_component_Private->avCodecContext->workaround_bugs |= FF_BUG_AUTODETECT;
  
  omx_amr_audioenc_component_Private->temp_buffer = malloc(DEFAULT_OUT_BUFFER_SIZE*2); 
  omx_amr_audioenc_component_Private->temp_buffer_filledlen=0;
 
  if(omx_amr_audioenc_component_Private->avCodecContext->frame_size == 0) {
    omx_amr_audioenc_component_Private->avCodecContext->frame_size = 80;
  }
  DEBUG(DEB_LEV_ERR, "In %s frame size=%d\n",__func__,omx_amr_audioenc_component_Private->avCodecContext->frame_size);
  omx_amr_audioenc_component_Private->frame_length = omx_amr_audioenc_component_Private->avCodecContext->frame_size*
                                                 (omx_amr_audioenc_component_Private->pAudioPcmMode.nBitPerSample/8)*
                                                 omx_amr_audioenc_component_Private->avCodecContext->channels;
  
  tsem_up(omx_amr_audioenc_component_Private->avCodecSyncSem);
  
  return OMX_ErrorNone;
}

/** 
  It Deinitializates the ffmpeg framework, and close the ffmpeg encoder
*/
void omx_amr_audioenc_component_ffmpegLibDeInit(omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private) {
  
  avcodec_close(omx_amr_audioenc_component_Private->avCodecContext);

  free(omx_amr_audioenc_component_Private->temp_buffer);
   
}

void omx_amr_audioenc_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *pPort = (omx_base_audio_PortType *) omx_amr_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  //settings of input port
  //input is pcm mode for all encoders - so generalise it 
  setHeader(&omx_amr_audioenc_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_amr_audioenc_component_Private->pAudioPcmMode.nPortIndex = 0;
  omx_amr_audioenc_component_Private->pAudioPcmMode.nChannels = 1;
  omx_amr_audioenc_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_amr_audioenc_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_amr_audioenc_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_amr_audioenc_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_amr_audioenc_component_Private->pAudioPcmMode.nSamplingRate = 8000;
  omx_amr_audioenc_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_amr_audioenc_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_amr_audioenc_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  if(omx_amr_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingAMR) {
    strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/amr");
    pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
                                                                                                                             
    setHeader(&omx_amr_audioenc_component_Private->pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE)); 
    omx_amr_audioenc_component_Private->pAudioAmr.nPortIndex = 1;
    omx_amr_audioenc_component_Private->pAudioAmr.nChannels = 1;    
    omx_amr_audioenc_component_Private->pAudioAmr.nBitRate = 12200;
    omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode = OMX_AUDIO_AMRBandModeNB7;
    omx_amr_audioenc_component_Private->pAudioAmr.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
    omx_amr_audioenc_component_Private->pAudioAmr.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatConformance;
    
    pPort->sAudioParam.nIndex = OMX_IndexParamAudioAmr;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingAMR;

  } else
    return;

}

//int headersent=0;
//unsigned char *tmp;
/** The Initialization function 
 */
OMX_ERRORTYPE omx_amr_audioenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)
{
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /*Temporary First Output buffer size*/
  omx_amr_audioenc_component_Private->inputCurrBuffer=NULL;
  omx_amr_audioenc_component_Private->inputCurrLength=0;
  nBufferSize=omx_amr_audioenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_amr_audioenc_component_Private->internalOutputBuffer = malloc(nBufferSize);
  memset(omx_amr_audioenc_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_amr_audioenc_component_Private->positionInOutBuf = 0;
  omx_amr_audioenc_component_Private->isNewBuffer=1;
  omx_amr_audioenc_component_Private->isFirstBuffer = 1;

  return err;
  
};

/** The Deinitialization function 
 */
OMX_ERRORTYPE omx_amr_audioenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (omx_amr_audioenc_component_Private->avcodecReady) {
    omx_amr_audioenc_component_ffmpegLibDeInit(omx_amr_audioenc_component_Private);
    omx_amr_audioenc_component_Private->avcodecReady = OMX_FALSE;
  }

  free(omx_amr_audioenc_component_Private->internalOutputBuffer);
  omx_amr_audioenc_component_Private->internalOutputBuffer = NULL;

//  free(tmp);

  return err;
}

/** buffer management callback function for encoding in new standard 
 * of ffmpeg library 
 */
void omx_amr_audioenc_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) 
{
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
  int nLen;
  OMX_S32 nOutputFilled = 0;
  static OMX_U8* data;
  unsigned char *pBuffer = NULL;
  AVRational  bq = { 1, 1000000 };

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n",__func__);

  if((omx_amr_audioenc_component_Private->isFirstBuffer == 1) && (omx_amr_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingAMR)) {
    if(omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
      memcpy(pOutputBuffer->pBuffer,AMR_header,6);
      pBuffer = pOutputBuffer->pBuffer + 6;
      pOutputBuffer->nFilledLen = 6;
    } else if(omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8) {
      memcpy(pOutputBuffer->pBuffer,AMRWB_header,9);
      pBuffer = pOutputBuffer->pBuffer + 9;
      pOutputBuffer->nFilledLen = 9;
    } 
    
    omx_amr_audioenc_component_Private->isFirstBuffer = 0;
    pOutputBuffer->nOffset=0;
  } else {
    pBuffer = pOutputBuffer->pBuffer;
    pOutputBuffer->nFilledLen = 0;
    pOutputBuffer->nOffset=0;
  }
  
  
  /*If temporary buffer exist then process that first*/
  if(omx_amr_audioenc_component_Private->temp_buffer_filledlen > 0 && omx_amr_audioenc_component_Private->isNewBuffer) {
    nLen = omx_amr_audioenc_component_Private->frame_length-omx_amr_audioenc_component_Private->temp_buffer_filledlen;
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s copying residue buffer %d\n",__func__,nLen);
    memcpy(&omx_amr_audioenc_component_Private->temp_buffer[omx_amr_audioenc_component_Private->temp_buffer_filledlen],pInputBuffer->pBuffer,nLen);
    omx_amr_audioenc_component_Private->isNewBuffer = 0; 
    data = &pInputBuffer->pBuffer[nLen];
    pInputBuffer->nFilledLen -= nLen;
    omx_amr_audioenc_component_Private->temp_buffer_filledlen = 0;

    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Using Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d temp_buffer_filledlen=%d\n",
      __func__,(int)omx_amr_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen,(int)omx_amr_audioenc_component_Private->temp_buffer_filledlen);

    while (!nOutputFilled) {
      omx_amr_audioenc_component_Private->avCodecContext->frame_number++;

      nLen = avcodec_encode_audio(omx_amr_audioenc_component_Private->avCodecContext,
                                  pBuffer,
                                  omx_amr_audioenc_component_Private->avCodecContext->frame_size,
                                  (short*)omx_amr_audioenc_component_Private->temp_buffer);

      if (nLen < 0) {
        DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not encoded?\n");
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nBit Rate =%d\n",__func__,(int)omx_amr_audioenc_component_Private->avCodecContext->bit_rate);
      }

      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Consumed 2 frame_length=%d,frame_size=%d,pInputBuffer->nFilledLen=%d nLen=%d\n",
      __func__,(int)omx_amr_audioenc_component_Private->frame_length,
      (int)omx_amr_audioenc_component_Private->avCodecContext->frame_size,(int)pInputBuffer->nFilledLen,nLen);
    
      if ( nLen >= 0) {
        pOutputBuffer->nFilledLen += nLen;
        pOutputBuffer->nTimeStamp = av_rescale_q(omx_amr_audioenc_component_Private->avCodecContext->coded_frame->pts,
          omx_amr_audioenc_component_Private->avCodecContext->time_base, bq);
      } 
      nOutputFilled = 1;
    }

    if((pInputBuffer->nFilledLen > 0) && (pInputBuffer->nFilledLen < omx_amr_audioenc_component_Private->frame_length)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Copying to Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d\n",
        __func__,(int)omx_amr_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen);
      memcpy(omx_amr_audioenc_component_Private->temp_buffer,data,pInputBuffer->nFilledLen);
      omx_amr_audioenc_component_Private->temp_buffer_filledlen = pInputBuffer->nFilledLen;
      pInputBuffer->nFilledLen = 0;
      omx_amr_audioenc_component_Private->isNewBuffer = 1;
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s 2 pInputBuffer->nFilledLen=%d nLen=%d \n",
      __func__,(int)pInputBuffer->nFilledLen,(int)nLen);
    //return;
  } else {

    if(omx_amr_audioenc_component_Private->isNewBuffer) {
      omx_amr_audioenc_component_Private->isNewBuffer = 0; 
      data = pInputBuffer->pBuffer;
    }
      
    while (!nOutputFilled) {
      if (!omx_amr_audioenc_component_Private->avcodecReady) {
        tsem_down(omx_amr_audioenc_component_Private->avCodecSyncSem);
      }
      omx_amr_audioenc_component_Private->avCodecContext->frame_number++;

      nLen = avcodec_encode_audio(omx_amr_audioenc_component_Private->avCodecContext,
                                  pBuffer,
                                  omx_amr_audioenc_component_Private->avCodecContext->frame_size,
                                  (short*)data);

      if (nLen < 0) {
        DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not encoded?\n");
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nBit Rate =%d\n",__func__,(int)omx_amr_audioenc_component_Private->avCodecContext->bit_rate);
      }

      pInputBuffer->nFilledLen -= omx_amr_audioenc_component_Private->frame_length;

      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Consumed 1 frame_length=%d,frame_size=%d pInputBuffer->nFilledLen=%d nLen=%d\n",
        __func__,(int)omx_amr_audioenc_component_Private->frame_length,
        (int)omx_amr_audioenc_component_Private->avCodecContext->frame_size,(int)pInputBuffer->nFilledLen,(int)nLen);
    
      if ( nLen >= 0) {
        pOutputBuffer->nFilledLen += nLen;
        pOutputBuffer->nTimeStamp = av_rescale_q(omx_amr_audioenc_component_Private->avCodecContext->coded_frame->pts,
          omx_amr_audioenc_component_Private->avCodecContext->time_base, bq);
      } 
      nOutputFilled = 1;
    }

    if(pInputBuffer->nFilledLen != 0) {
      data +=omx_amr_audioenc_component_Private->frame_length;
    }

    if((pInputBuffer->nFilledLen > 0) && (pInputBuffer->nFilledLen < omx_amr_audioenc_component_Private->frame_length)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Copying to Temp buffer frame_length=%d,pInputBuffer->nFilledLen=%d\n",
        __func__,(int)omx_amr_audioenc_component_Private->frame_length,(int)pInputBuffer->nFilledLen);
      memcpy(omx_amr_audioenc_component_Private->temp_buffer,data,pInputBuffer->nFilledLen);
      omx_amr_audioenc_component_Private->temp_buffer_filledlen = pInputBuffer->nFilledLen;
      pInputBuffer->nFilledLen = 0;
      omx_amr_audioenc_component_Private->isNewBuffer = 1;
    }

    if(pInputBuffer->nFilledLen == 0) {
      omx_amr_audioenc_component_Private->isNewBuffer = 1;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s 1 pInputBuffer->nFilledLen=%d nLen=%d \n",__func__,(int)pInputBuffer->nFilledLen,nLen);
  }

  /** return output buffer */
}

OMX_ERRORTYPE omx_amr_audioenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_AMRTYPE *pAudioAmr;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = openmaxStandComp->pComponentPrivate;
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
      port = (omx_base_audio_PortType *) omx_amr_audioenc_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      err = OMX_ErrorBadPortIndex;
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
    if(pAudioPcmMode->nPortIndex == 0) {
      if((pAudioPcmMode->nSamplingRate != 8000 && 
         pAudioPcmMode->nSamplingRate != 16000 ) ||
         pAudioPcmMode->nChannels != 1) {
        DEBUG(DEB_LEV_ERR, "AMR-NB Support only 8000Hz Mono \n AMR-WB Support only 16000Hz Mono\n");
        err = OMX_ErrorBadParameter;
        break;
      }
      memcpy(&omx_amr_audioenc_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    } else {
      err = OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamAudioAmr:  
    pAudioAmr = (OMX_AUDIO_PARAM_AMRTYPE*) ComponentParameterStructure;
    portIndex = pAudioAmr->nPortIndex;

    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (pAudioAmr->nPortIndex == 1) {

      if(pAudioAmr->nChannels != 1) {
        DEBUG(DEB_LEV_ERR, "AMR-NB/WB Support only Mono\n");
        err = OMX_ErrorBadParameter;
        break;
      }

      switch(pAudioAmr->eAMRBandMode) {
      case OMX_AUDIO_AMRBandModeNB0:                 /**< AMRNB Mode 0 =  4750 bps */
        pAudioAmr->nBitRate = 4750; 
        break;
      case OMX_AUDIO_AMRBandModeNB1:                 /**< AMRNB Mode 1 =  5150 bps */
        pAudioAmr->nBitRate = 5150;
        break;
      case OMX_AUDIO_AMRBandModeNB2:                 /**< AMRNB Mode 2 =  5900 bps */
        pAudioAmr->nBitRate = 5900;
        break;
      case OMX_AUDIO_AMRBandModeNB3:                 /**< AMRNB Mode 3 =  6700 bps */
        pAudioAmr->nBitRate = 6700;
        break;
      case OMX_AUDIO_AMRBandModeNB4:                 /**< AMRNB Mode 4 =  7400 bps */
        pAudioAmr->nBitRate = 7400;
        break;
      case OMX_AUDIO_AMRBandModeNB5:                 /**< AMRNB Mode 5 =  7950 bps */
        pAudioAmr->nBitRate = 7950;
        break;
      case OMX_AUDIO_AMRBandModeNB6:                 /**< AMRNB Mode 6 = 10200 bps */
        pAudioAmr->nBitRate = 10200;
        break;
      case OMX_AUDIO_AMRBandModeNB7:                /**< AMRNB Mode 7 = 12200 bps */
        pAudioAmr->nBitRate = 12200;
        break;
      case OMX_AUDIO_AMRBandModeWB0:                 /**< AMRWB Mode 0 =  6600 bps */
        pAudioAmr->nBitRate = 6600; 
        break;
      case OMX_AUDIO_AMRBandModeWB1:                 /**< AMRWB Mode 1 =  8850 bps */
        pAudioAmr->nBitRate = 8850;
        break;
      case OMX_AUDIO_AMRBandModeWB2:                 /**< AMRWB Mode 2 =  12650 bps */
        pAudioAmr->nBitRate = 12650;
        break;
      case OMX_AUDIO_AMRBandModeWB3:                 /**< AMRWB Mode 3 =  14250 bps */
        pAudioAmr->nBitRate = 14250;
        break;
      case OMX_AUDIO_AMRBandModeWB4:                 /**< AMRWB Mode 4 =  15850 bps */
        pAudioAmr->nBitRate = 15850;
        break;
      case OMX_AUDIO_AMRBandModeWB5:                 /**< AMRWB Mode 5 =  18250 bps */
        pAudioAmr->nBitRate = 18250;
        break;
      case OMX_AUDIO_AMRBandModeWB6:                 /**< AMRWB Mode 6 = 19850 bps */
        pAudioAmr->nBitRate = 19850;
        break;
      case OMX_AUDIO_AMRBandModeWB7:                /**< AMRWB Mode 7 = 23050 bps */
        pAudioAmr->nBitRate = 23050;
        break;
      case OMX_AUDIO_AMRBandModeWB8:                /**< AMRWB Mode 8 = 23850 bps */
        pAudioAmr->nBitRate = 23850;
        break;
      default:
        DEBUG(DEB_LEV_ERR, "In %s AMR Band Mode %x Not Supported\n",__func__,pAudioAmr->eAMRBandMode); 
        return OMX_ErrorBadParameter;
        break;
      }

      memcpy(&omx_amr_audioenc_component_Private->pAudioAmr,pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE));
    } else {
      err = OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;

    if (omx_amr_audioenc_component_Private->state != OMX_StateLoaded && omx_amr_audioenc_component_Private->state != OMX_StateWaitForResources) {
      DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_amr_audioenc_component_Private->state,__LINE__);
      return OMX_ErrorIncorrectStateOperation;
    }
  
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }

    if (!strcmp((char*)pComponentRole->cRole, AUDIO_ENC_AMR_ROLE)) {
      omx_amr_audioenc_component_Private->audio_coding_type = OMX_AUDIO_CodingAMR;
    } else {
      err = OMX_ErrorBadParameter;
      break;
    }
    omx_amr_audioenc_component_SetInternalParameters(openmaxStandComp);
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_amr_audioenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;  
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_AMRTYPE *pAudioAmr;
  omx_base_audio_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = (omx_amr_audioenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
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
    memcpy(ComponentParameterStructure, &omx_amr_audioenc_component_Private->sPortTypesParam[OMX_PortDomainAudio], sizeof(OMX_PORT_PARAM_TYPE));
    break;    

  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_base_audio_PortType *)omx_amr_audioenc_component_Private->ports[pAudioPortFormat->nPortIndex];
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
    memcpy(pAudioPcmMode,&omx_amr_audioenc_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;

  case OMX_IndexParamAudioAmr:  
    pAudioAmr = (OMX_AUDIO_PARAM_AMRTYPE*)ComponentParameterStructure;
    if (pAudioAmr->nPortIndex != 1) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AMRTYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioAmr,&omx_amr_audioenc_component_Private->pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE));
    break;
    
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }

    if (omx_amr_audioenc_component_Private->audio_coding_type == OMX_AUDIO_CodingAMR && 
        omx_amr_audioenc_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
        strcpy((char*)pComponentRole->cRole, AUDIO_ENC_AMR_ROLE);
    } else {
      strcpy((char*)pComponentRole->cRole,"\0");;
    }
    break;

  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_amr_audioenc_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)
{
  omx_amr_audioenc_component_PrivateType* omx_amr_audioenc_component_Private = (omx_amr_audioenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_amr_audioenc_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_amr_audioenc_component_Private->state == OMX_StateIdle)) {
      if (!omx_amr_audioenc_component_Private->avcodecReady) {
        err = omx_amr_audioenc_component_ffmpegLibInit(omx_amr_audioenc_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_amr_audioenc_component_Private->avcodecReady = OMX_TRUE;
      }
    } 
    else if ((message->messageParam == OMX_StateIdle ) && (omx_amr_audioenc_component_Private->state == OMX_StateLoaded)) {
      err = omx_amr_audioenc_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Audio Encoder Init Failed Error=%x\n",__func__,err); 
        return err;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_amr_audioenc_component_Private->state == OMX_StateIdle)) {
      err = omx_amr_audioenc_component_Deinit(openmaxStandComp);
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
      if (omx_amr_audioenc_component_Private->avcodecReady) {
        omx_amr_audioenc_component_ffmpegLibDeInit(omx_amr_audioenc_component_Private);
        omx_amr_audioenc_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}

OMX_ERRORTYPE omx_amr_audioenc_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex)
{
  if (nIndex == 0) {
    strcpy((char*)cRole, AUDIO_ENC_AMR_ROLE);
  } else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}
