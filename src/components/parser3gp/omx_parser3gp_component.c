/**
  @file src/components/parser3gp/omx_parser3gp_component.c

  OpenMax parser3gp component. This component is a 3gp stream parser that parses the input
  file format so that client calls the appropriate decoder.

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
#include <omx_base_video_port.h>
#include <omx_base_audio_port.h>  
#include <omx_parser3gp_component.h>

#define MAX_COMPONENT_PARSER_3GP 1

/** Maximum Number of parser3gp Instance*/
static OMX_U32 noParser3gpInstance=0;
#define DEFAULT_FILENAME_LENGTH 256
#define VIDEO_PORT_INDEX 0
#define AUDIO_PORT_INDEX 1
#define VIDEO_STREAM 0
#define AUDIO_STREAM 1

/** The Constructor 
 */
OMX_ERRORTYPE omx_parser3gp_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
 
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_base_video_PortType *pPortV;
  omx_base_audio_PortType *pPortA;  
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_parser3gp_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  /*Assign size of the derived port class,so that proper memory for port class can be allocated*/
  omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  omx_parser3gp_component_Private->ports = NULL;

  err = omx_base_source_Constructor(openmaxStandComp, cComponentName);
  omx_parser3gp_component_Private->sPortTypesParam.nPorts=2;  /* Setting two ouput ports 0: video, 1: audio */

   /** Allocate Ports and call port constructor. */
  if (omx_parser3gp_component_Private->sPortTypesParam.nPorts && !omx_parser3gp_component_Private->ports) {
    omx_parser3gp_component_Private->ports = calloc(omx_parser3gp_component_Private->sPortTypesParam.nPorts, sizeof(omx_base_PortType *));
    if (!omx_parser3gp_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    /* allocate video port*/
   omx_parser3gp_component_Private->ports[VIDEO_PORT_INDEX] = calloc(1, sizeof(omx_base_video_PortType)); 
   if (!omx_parser3gp_component_Private->ports[VIDEO_PORT_INDEX]) 
       return OMX_ErrorInsufficientResources;
   /* allocate audio port*/
   omx_parser3gp_component_Private->ports[AUDIO_PORT_INDEX] = calloc(1, sizeof(omx_base_audio_PortType)); 
   if (!omx_parser3gp_component_Private->ports[AUDIO_PORT_INDEX]) 
       return OMX_ErrorInsufficientResources;
  }

  base_video_port_Constructor(openmaxStandComp, &omx_parser3gp_component_Private->ports[VIDEO_PORT_INDEX], VIDEO_PORT_INDEX, OMX_FALSE);
  base_audio_port_Constructor(openmaxStandComp, &omx_parser3gp_component_Private->ports[AUDIO_PORT_INDEX], AUDIO_PORT_INDEX, OMX_FALSE); 

  pPortV = (omx_base_video_PortType *) omx_parser3gp_component_Private->ports[VIDEO_PORT_INDEX];
  pPortA = (omx_base_audio_PortType *) omx_parser3gp_component_Private->ports[AUDIO_PORT_INDEX]; 

  /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
  pPortV->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
  pPortA->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  omx_parser3gp_component_Private->BufferMgmtCallback = omx_parser3gp_component_BufferMgmtCallback;
  omx_parser3gp_component_Private->BufferMgmtFunction = omx_base_source_twoport_BufferMgmtFunction; 

  setHeader(&omx_parser3gp_component_Private->sTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
  omx_parser3gp_component_Private->sTimeStamp.nPortIndex=0;
  omx_parser3gp_component_Private->sTimeStamp.nTimestamp=0x0;

  omx_parser3gp_component_Private->destructor = omx_parser3gp_component_Destructor;
  omx_parser3gp_component_Private->messageHandler = omx_parser3gp_component_MessageHandler;

  noParser3gpInstance++;
  if(noParser3gpInstance > MAX_COMPONENT_PARSER_3GP) {
    return OMX_ErrorInsufficientResources;
  }

  openmaxStandComp->SetParameter  = omx_parser3gp_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_parser3gp_component_GetParameter;
  openmaxStandComp->SetConfig     = omx_parser3gp_component_SetConfig;
  openmaxStandComp->GetConfig     = omx_parser3gp_component_GetConfig;
  openmaxStandComp->GetExtensionIndex = omx_parser3gp_component_GetExtensionIndex;

  /* Write in the default paramenters */

  omx_parser3gp_component_Private->pTmpOutputBuffer = (OMX_BUFFERHEADERTYPE*) malloc(sizeof(OMX_BUFFERHEADERTYPE));
  omx_parser3gp_component_Private->pTmpOutputBuffer->pBuffer = (OMX_U8*) malloc(DEFAULT_OUT_BUFFER_SIZE);
  memset(omx_parser3gp_component_Private->pTmpOutputBuffer->pBuffer, 0, DEFAULT_OUT_BUFFER_SIZE);
  omx_parser3gp_component_Private->pTmpOutputBuffer->nFilledLen=0;
  omx_parser3gp_component_Private->pTmpOutputBuffer->nAllocLen=DEFAULT_OUT_BUFFER_SIZE;
  omx_parser3gp_component_Private->pTmpOutputBuffer->nOffset=0;
 
  omx_parser3gp_component_Private->avformatReady = OMX_FALSE;
  if(!omx_parser3gp_component_Private->avformatSyncSem) {
    omx_parser3gp_component_Private->avformatSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_parser3gp_component_Private->avformatSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_parser3gp_component_Private->avformatSyncSem, 0);
  }
  omx_parser3gp_component_Private->sInputFileName = (char *)malloc(DEFAULT_FILENAME_LENGTH);
  memset(omx_parser3gp_component_Private->sInputFileName,0,DEFAULT_FILENAME_LENGTH);
  /*Default Coding type*/
  omx_parser3gp_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  omx_parser3gp_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3; 
  av_register_all();  /* without this file opening gives an error */

  return err;
}

/* The Destructor */
OMX_ERRORTYPE omx_parser3gp_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_parser3gp_component_Private->avformatSyncSem) {
    tsem_deinit(omx_parser3gp_component_Private->avformatSyncSem);
    free(omx_parser3gp_component_Private->avformatSyncSem);
    omx_parser3gp_component_Private->avformatSyncSem=NULL;
  }

  if(omx_parser3gp_component_Private->sInputFileName) {
    free(omx_parser3gp_component_Private->sInputFileName);
  }

  if(omx_parser3gp_component_Private->pTmpOutputBuffer) {
    free(omx_parser3gp_component_Private->pTmpOutputBuffer);
  }
  
  /* frees port/s */
  if (omx_parser3gp_component_Private->ports) {
    for (i=0; i < omx_parser3gp_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_parser3gp_component_Private->ports[i])
        omx_parser3gp_component_Private->ports[i]->PortDestructor(omx_parser3gp_component_Private->ports[i]);
    }
    free(omx_parser3gp_component_Private->ports);
    omx_parser3gp_component_Private->ports=NULL;
  }
  
  noParser3gpInstance--;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);
  return omx_base_source_Destructor(openmaxStandComp);
}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_parser3gp_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPortV;
  omx_base_audio_PortType *pPortA;
  int error;
  
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  /** initialization of parser3gp  component private data structures */
  /** opening the input file whose name is already set via setParameter */
  error = av_open_input_file(&omx_parser3gp_component_Private->avformatcontext, 
                            (char*)omx_parser3gp_component_Private->sInputFileName,
                            omx_parser3gp_component_Private->avinputformat,
                            0,
                            omx_parser3gp_component_Private->avformatparameters);

  if(error != 0) {
    DEBUG(DEB_LEV_ERR,"Couldn't Open Input Stream error=%d File Name=%s\n",
      error,(char*)omx_parser3gp_component_Private->sInputFileName);
  
    return OMX_ErrorBadParameter;
  }

  av_find_stream_info(omx_parser3gp_component_Private->avformatcontext);

 /* Setting the audio and video coding types of the audio and video ports based on the information obtained from the stream */
 /* for the video port */
  pPortV = (omx_base_video_PortType *) omx_parser3gp_component_Private->ports[VIDEO_PORT_INDEX];
  switch(omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_PORT_INDEX]->codec->codec_id){
    case  CODEC_ID_H264:
        pPortV->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC; 
        pPortV->sPortParam.format.video.nFrameWidth =omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->width;   
        pPortV->sPortParam.format.video.nFrameHeight =omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->height;   
        omx_parser3gp_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
        DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Video Coding Type h.264\n",__func__);
        break;
    case CODEC_ID_MPEG4:
        pPortV->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4; 
        pPortV->sPortParam.format.video.nFrameWidth =omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->width;   
        pPortV->sPortParam.format.video.nFrameHeight =omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->height;   
        omx_parser3gp_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
        DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Video Coding Type Mpeg4\n",__func__);
        break;
    default :
    (*(omx_parser3gp_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_parser3gp_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      OMX_ErrorFormatNotDetected, /* Format Not Detected */ 
      VIDEO_PORT_INDEX, /* This is the output port index*/ 
      NULL);  
    DEBUG(DEB_LEV_ERR,"Trouble in %s No Video Coding Type Selected (only H264 and MPEG4 codecs supported)\n",__func__);
    return OMX_ErrorBadParameter;
  }

  /* for the audio port*/
  pPortA = (omx_base_audio_PortType *) omx_parser3gp_component_Private->ports[AUDIO_PORT_INDEX];
  switch(omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->codec_id){
    case CODEC_ID_MP3:
        pPortA->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
        pPortA->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
        omx_parser3gp_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
        break;
    case CODEC_ID_AAC:
        pPortA->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
        pPortA->sAudioParam.eEncoding = OMX_AUDIO_CodingAAC;
        omx_parser3gp_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;
        break;
    default:
    (*(omx_parser3gp_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_parser3gp_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      OMX_ErrorFormatNotDetected, /* Format Not Detected */ 
      AUDIO_PORT_INDEX, /* This is the output port index */
      NULL);  
    DEBUG(DEB_LEV_ERR,"Trouble in %s No Audio Coding Type Selected (only MP3 and AAC codecs supported)\n",__func__);
    return OMX_ErrorBadParameter;

  }

  DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Video Extra data size=%d\n",__func__,omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->extradata_size);
  DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Audio Extra data size=%d\n",__func__,omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->extradata_size); 
  /** initialization for buff mgmt callback function */

  /** send callback regarding codec context extradata which will be required to 
    * open the codec in the audio  and video decoder component 
    */    
  (*(omx_parser3gp_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_parser3gp_component_Private->callbackData,
    OMX_EventPortFormatDetected, /* The command was completed */
    OMX_IndexParamVideoPortFormat, /* port Format Detected */ 
    VIDEO_PORT_INDEX, /* This is the output port index */
    NULL);  

  (*(omx_parser3gp_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_parser3gp_component_Private->callbackData,
    OMX_EventPortSettingsChanged, /* The command was completed */
    OMX_IndexParamCommonExtraQuantData, /* port settings changed */ 
    VIDEO_PORT_INDEX, /* This is the output port index */  
    NULL);  

  (*(omx_parser3gp_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_parser3gp_component_Private->callbackData,
    OMX_EventPortFormatDetected, /* The command was completed */
    OMX_IndexParamVideoPortFormat, /* port Format Detected */ 
    AUDIO_PORT_INDEX, /* This is the output port index */
    NULL);

  (*(omx_parser3gp_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_parser3gp_component_Private->callbackData,
    OMX_EventPortSettingsChanged, /* The command was completed */
    OMX_IndexParamCommonExtraQuantData, /* port settings changed */ 
    AUDIO_PORT_INDEX, /* This is the output port index */
    NULL);  

  omx_parser3gp_component_Private->avformatReady = OMX_TRUE;
/*Indicate that avformat is ready*/
  tsem_up(omx_parser3gp_component_Private->avformatSyncSem);

  return OMX_ErrorNone;
}

/** The DeInitialization function 
 */
OMX_ERRORTYPE omx_parser3gp_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
  /** closing input file */
  av_close_input_file(omx_parser3gp_component_Private->avformatcontext);
  
  omx_parser3gp_component_Private->avformatReady = OMX_FALSE;
  tsem_reset(omx_parser3gp_component_Private->avformatSyncSem);

  return OMX_ErrorNone;
}

/** 
 * This function processes the input file and returns packet by packet as an output data
 * this packet is used in audio/video decoder component for decoding
 */
void omx_parser3gp_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_BUFFERHEADERTYPE*  temp_buffer;
  int error;

  temp_buffer = omx_parser3gp_component_Private->pTmpOutputBuffer;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (omx_parser3gp_component_Private->avformatReady == OMX_FALSE) {
    if(omx_parser3gp_component_Private->state == OMX_StateExecuting) {
      /*wait for avformat to be ready*/
      tsem_down(omx_parser3gp_component_Private->avformatSyncSem);
    } else {
      return;
    }
  }

  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  if(temp_buffer->nFilledLen==0) {  /* no data available in temporary buffer*/
     error = av_read_frame(omx_parser3gp_component_Private->avformatcontext, &omx_parser3gp_component_Private->pkt);
     if(error < 0) {
       DEBUG(DEB_LEV_FULL_SEQ,"In %s EOS - no more packet,state=%x\n",__func__, omx_parser3gp_component_Private->state);
       pOutputBuffer->nFlags = OMX_BUFFERFLAG_EOS;
     } else {
       DEBUG(DEB_LEV_SIMPLE_SEQ,"\n packet size : %d \n",omx_parser3gp_component_Private->pkt.size);
       if((omx_parser3gp_component_Private->pkt.stream_index==VIDEO_STREAM && pOutputBuffer->nOutputPortIndex==VIDEO_PORT_INDEX) || 
          (omx_parser3gp_component_Private->pkt.stream_index==AUDIO_STREAM && pOutputBuffer->nOutputPortIndex==AUDIO_PORT_INDEX)){  
         /** copying the packetized data in the output buffer that will be decoded in the decoder component  */
         if(pOutputBuffer->nAllocLen >= omx_parser3gp_component_Private->pkt.size) {
           memcpy(pOutputBuffer->pBuffer, omx_parser3gp_component_Private->pkt.data, omx_parser3gp_component_Private->pkt.size);
           pOutputBuffer->nFilledLen = omx_parser3gp_component_Private->pkt.size;
         } else {
           DEBUG(DEB_LEV_ERR,"In %s Buffer Size=%d less than Pkt size=%d buffer=%x port_index=%d \n",__func__,(int)pOutputBuffer->nAllocLen,(int)omx_parser3gp_component_Private->pkt.size,(unsigned int)pOutputBuffer,(int)pOutputBuffer->nOutputPortIndex);
         }
       }else { /* the port type and the stream data do not match so keep the data in temporary buffer*/
         if(temp_buffer->nAllocLen >= omx_parser3gp_component_Private->pkt.size) {
           memcpy(temp_buffer->pBuffer, omx_parser3gp_component_Private->pkt.data, omx_parser3gp_component_Private->pkt.size);
           temp_buffer->nFilledLen = omx_parser3gp_component_Private->pkt.size;
           temp_buffer->nOutputPortIndex = omx_parser3gp_component_Private->pkt.stream_index; /* keep the stream_index in OutputPortIndex for identification */
         } else {
           DEBUG(DEB_LEV_ERR,"In %s Buffer Size=%d less than Pkt size=%d\n",__func__,
             (int)temp_buffer->nAllocLen,(int)omx_parser3gp_component_Private->pkt.size);
         }
       }
     }
   } else {  /* data available in temporary buffer */
     DEBUG(DEB_LEV_SIMPLE_SEQ,"\n data size in temp buffer : %d \n",(int)temp_buffer->nFilledLen);
     if(( temp_buffer->nOutputPortIndex==VIDEO_STREAM && pOutputBuffer->nOutputPortIndex==VIDEO_PORT_INDEX) ||
        ( temp_buffer->nOutputPortIndex==AUDIO_STREAM && pOutputBuffer->nOutputPortIndex==AUDIO_PORT_INDEX)){
       /** copying the packetized data from the temp buffer in the output buffer that will be decoded in the decoder component  */
       if(pOutputBuffer->nAllocLen >= temp_buffer->nFilledLen) {
         memcpy(pOutputBuffer->pBuffer, temp_buffer->pBuffer, temp_buffer->nFilledLen);
         pOutputBuffer->nFilledLen = temp_buffer->nFilledLen;
         temp_buffer->nFilledLen = 0;
       } else {
         DEBUG(DEB_LEV_ERR,"In %s Buffer Size=%d less than Pkt size=%d\n",__func__,
           (int)pOutputBuffer->nAllocLen,(int)omx_parser3gp_component_Private->pkt.size);
       }
     }
   }

  av_free_packet(&omx_parser3gp_component_Private->pkt);
  
  /** return the current output buffer */
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n", (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_parser3gp_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_VIDEO_PARAM_AVCTYPE * pVideoAvc;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_U32 portIndex;
  OMX_U32 nFileNameLength;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType* pVideoPort = (omx_base_video_PortType *) omx_parser3gp_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  omx_base_audio_PortType* pAudioPort = (omx_base_audio_PortType *) omx_parser3gp_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX_1];

  if(ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

  switch(nParamIndex) {
  case OMX_IndexParamVideoPortFormat:
    pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pVideoPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    if (portIndex < 1) {
      memcpy(&pVideoPort->sVideoParam,pVideoPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamVideoAvc: 
    pVideoAvc = (OMX_VIDEO_PARAM_AVCTYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, pVideoAvc->nPortIndex, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    break;
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
      break;
    }
    if (portIndex == 1) {
      memcpy(&pAudioPort->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, pAudioMp3->nPortIndex, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
      break;
    }
    break;
  case OMX_IndexVendorParser3gpInputFilename : 
    nFileNameLength = strlen((char *)ComponentParameterStructure) * sizeof(char) + 1;
    if(nFileNameLength > DEFAULT_FILENAME_LENGTH) {
      free(omx_parser3gp_component_Private->sInputFileName);
      omx_parser3gp_component_Private->sInputFileName = (char *)malloc(nFileNameLength);
    }
    strcpy(omx_parser3gp_component_Private->sInputFileName, (char *)ComponentParameterStructure);
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_parser3gp_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PORT_PARAM_TYPE *pVideoPortParam, *pAudioPortParam;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_VENDOR_EXTRADATATYPE sExtraData;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pVideoPort = (omx_base_video_PortType *) omx_parser3gp_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  omx_base_audio_PortType* pAudioPort = (omx_base_audio_PortType *) omx_parser3gp_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX_1];


  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Getting parameter %08x\n",__func__, nParamIndex);

  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamVideoInit:
    pVideoPortParam = (OMX_PORT_PARAM_TYPE*)  ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    pVideoPortParam->nStartPortNumber = 0;
    pVideoPortParam->nPorts = 1;
    break;
  case OMX_IndexParamVideoPortFormat:
    pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pVideoPortFormat->nPortIndex < 1) {
      memcpy(pVideoPortFormat, &pVideoPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioInit:
    pAudioPortParam = (OMX_PORT_PARAM_TYPE *) ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
      break;
    }
    pAudioPortParam->nStartPortNumber = 1;
    pAudioPortParam->nPorts = 1;
    break;
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      memcpy(pAudioPortFormat, &pAudioPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case  OMX_IndexVendorParser3gpInputFilename:
    strcpy((char *)ComponentParameterStructure, "still no filename");
    break;
  case OMX_IndexVendorVideoExtraData:
    sExtraData.nPortIndex = VIDEO_PORT_INDEX;
    sExtraData.nDataSize  = omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->extradata_size;  
    sExtraData.pData      = omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->extradata;
    memcpy(ComponentParameterStructure, &sExtraData, sizeof(OMX_VENDOR_EXTRADATATYPE));
    break;
  case OMX_IndexVendorAudioExtraData:
    sExtraData.nPortIndex = AUDIO_PORT_INDEX;
    sExtraData.nDataSize  = omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->extradata_size;  
    sExtraData.pData      = omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->extradata;
    memcpy(ComponentParameterStructure, &sExtraData, sizeof(OMX_VENDOR_EXTRADATATYPE));
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

/** This function initializes and deinitializes the library related initialization
  * needed for file parsing
  */
OMX_ERRORTYPE omx_parser3gp_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = (omx_parser3gp_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_STATETYPE oldState = omx_parser3gp_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  /* Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){ 
    if ((message->messageParam == OMX_StateExecuting) && (oldState == OMX_StateIdle)) {
      err = omx_parser3gp_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s  parser3gp Init Failed Error=%x\n",__func__,err); 
        return err;
      }
    } else if ((message->messageParam == OMX_StateIdle) && (oldState == OMX_StateExecuting)) {
      err = omx_parser3gp_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s parser3gp Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }
  }

  return err;
}

/** setting configurations */
OMX_ERRORTYPE omx_parser3gp_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_TIME_CONFIG_TIMESTAMPTYPE* sTimeStamp;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_base_video_PortType *pPort;

  switch (nIndex) {
    case OMX_IndexConfigTimePosition :
      sTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;
      /*Check Structure Header and verify component state*/
      if (sTimeStamp->nPortIndex >= (omx_parser3gp_component_Private->sPortTypesParam.nStartPortNumber + omx_parser3gp_component_Private->sPortTypesParam.nPorts)) {
        DEBUG(DEB_LEV_ERR, "Bad Port index %i when the component has %i ports\n", (int)sTimeStamp->nPortIndex, (int)omx_parser3gp_component_Private->sPortTypesParam.nPorts);
        return OMX_ErrorBadPortIndex;
      }

      err= checkHeader(sTimeStamp , sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      if(err != OMX_ErrorNone) {
        return err;
      }

      if (sTimeStamp->nPortIndex < 1) {
        pPort= (omx_base_video_PortType *)omx_parser3gp_component_Private->ports[sTimeStamp->nPortIndex];
        memcpy(&omx_parser3gp_component_Private->sTimeStamp,sTimeStamp,sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

/** getting configurations */
OMX_ERRORTYPE omx_parser3gp_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_VENDOR_EXTRADATATYPE sExtraData;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_parser3gp_component_PrivateType* omx_parser3gp_component_Private = openmaxStandComp->pComponentPrivate;

  switch (nIndex) {
    case OMX_IndexVendorVideoExtraData:
      sExtraData.nPortIndex = VIDEO_PORT_INDEX;
      sExtraData.nDataSize  = omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->extradata_size;
      sExtraData.pData      = omx_parser3gp_component_Private->avformatcontext->streams[VIDEO_STREAM]->codec->extradata;
      memcpy(pComponentConfigStructure, &sExtraData, sizeof(OMX_VENDOR_EXTRADATATYPE));
      break;
    case OMX_IndexVendorAudioExtraData:
      sExtraData.nPortIndex = AUDIO_PORT_INDEX;
      sExtraData.nDataSize  = omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->extradata_size;
      sExtraData.pData      = omx_parser3gp_component_Private->avformatcontext->streams[AUDIO_STREAM]->codec->extradata;
      memcpy(pComponentConfigStructure, &sExtraData, sizeof(OMX_VENDOR_EXTRADATATYPE));
      break;
    default: // delegate to superclass
      return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_parser3gp_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  if(strcmp(cParameterName,"OMX.ST.index.param.parser3gp.inputfilename") == 0) {
    *pIndexType = OMX_IndexVendorParser3gpInputFilename;
  } else if(strcmp(cParameterName,"OMX.ST.index.config.videoextradata") == 0) {
    *pIndexType = OMX_IndexVendorVideoExtraData;
  } else if(strcmp(cParameterName,"OMX.ST.index.config.audioextradata") == 0) {
    *pIndexType = OMX_IndexVendorAudioExtraData;
  } else {
    return OMX_ErrorBadParameter;
  }
  return OMX_ErrorNone;
}


