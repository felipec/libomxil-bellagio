/**
  @file src/components/muxer/omx_mux_component.c

  OpenMAX mux component. This component is a 3gp stream muxer that muxes the inputs and
  write into an output file.

  Copyright (C) 2008  STMicroelectronics
  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

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
#include <omx_mux_component.h>

#define MAX_COMPONENT_MUX_3GP 1

/** Maximum Number of mux Instance*/
static OMX_U32 noMuxInstance=0;
#define DEFAULT_FILENAME_LENGTH 256
#define VIDEO_PORT_INDEX 0
#define AUDIO_PORT_INDEX 1
#define CLOCK_PORT_INDEX 2
#define VIDEO_STREAM 0
#define AUDIO_STREAM 1

//#define STREAM_DURATION   5.0
//#define STREAM_FRAME_RATE 10 /* 25 images/s */
//#define STREAM_NB_FRAMES  ((int)(STREAM_DURATION * STREAM_FRAME_RATE))
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */

static AVStream *add_audio_stream(OMX_COMPONENTTYPE* openmaxStandComp,AVFormatContext *oc, int codec_id);
static AVStream *add_video_stream(OMX_COMPONENTTYPE* openmaxStandComp,AVFormatContext *oc, int codec_id);

/** The Constructor 
 */
OMX_ERRORTYPE omx_mux_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
 
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_base_video_PortType *pPortVideo;
  omx_base_audio_PortType *pPortAudio;  
  omx_mux_component_PrivateType* omx_mux_component_Private;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_mux_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  /*Assign size of the derived port class,so that proper memory for port class can be allocated*/
  omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_mux_component_Private->ports = NULL;

  err = omx_base_sink_Constructor(openmaxStandComp, cComponentName);

  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 1;

  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 1;
  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 1;

  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nStartPortNumber = 2;
  omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts = 0;

   /** Allocate Ports and call port constructor. */
  if ((omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts  +
       omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
       omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts ) && !omx_mux_component_Private->ports) {
       omx_mux_component_Private->ports = calloc((omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts  +
                                                        omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                                        omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts ), sizeof(omx_base_PortType *));
    if (!omx_mux_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }

    /* allocate video port*/
    omx_mux_component_Private->ports[VIDEO_PORT_INDEX] = calloc(1, sizeof(omx_base_video_PortType)); 
    if (!omx_mux_component_Private->ports[VIDEO_PORT_INDEX]) 
       return OMX_ErrorInsufficientResources;
    /* allocate audio port*/
    omx_mux_component_Private->ports[AUDIO_PORT_INDEX] = calloc(1, sizeof(omx_base_audio_PortType)); 
    if (!omx_mux_component_Private->ports[AUDIO_PORT_INDEX]) 
       return OMX_ErrorInsufficientResources;
       
  }

  base_video_port_Constructor(openmaxStandComp, &omx_mux_component_Private->ports[VIDEO_PORT_INDEX], VIDEO_PORT_INDEX, OMX_TRUE);
  base_audio_port_Constructor(openmaxStandComp, &omx_mux_component_Private->ports[AUDIO_PORT_INDEX], AUDIO_PORT_INDEX, OMX_TRUE); 

  pPortVideo = (omx_base_video_PortType *) omx_mux_component_Private->ports[VIDEO_PORT_INDEX];
  pPortAudio = (omx_base_audio_PortType *) omx_mux_component_Private->ports[AUDIO_PORT_INDEX]; 

  /*Input pPort buffer size is equal to the size of the input buffer of the previous component*/
  pPortVideo->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
  pPortAudio->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  omx_mux_component_Private->BufferMgmtCallback = omx_mux_component_BufferMgmtCallback;
  omx_mux_component_Private->BufferMgmtFunction = omx_base_sink_twoport_BufferMgmtFunction; 

  setHeader(&omx_mux_component_Private->sTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
  omx_mux_component_Private->sTimeStamp.nPortIndex=0;
  omx_mux_component_Private->sTimeStamp.nTimestamp=0x0;

  omx_mux_component_Private->destructor = omx_mux_component_Destructor;
  omx_mux_component_Private->messageHandler = omx_mux_component_MessageHandler;

  noMuxInstance++;
  if(noMuxInstance > MAX_COMPONENT_MUX_3GP) {
    return OMX_ErrorInsufficientResources;
  }

  openmaxStandComp->SetParameter  = omx_mux_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_mux_component_GetParameter;
  openmaxStandComp->SetConfig     = omx_mux_component_SetConfig;
  openmaxStandComp->GetExtensionIndex = omx_mux_component_GetExtensionIndex;

  /* Write in the default paramenters */

  omx_mux_component_Private->pTmpInputBuffer = calloc(1,sizeof(OMX_BUFFERHEADERTYPE));
  omx_mux_component_Private->pTmpInputBuffer->pBuffer = calloc(1,DEFAULT_OUT_BUFFER_SIZE);
  omx_mux_component_Private->pTmpInputBuffer->nFilledLen=0;
  omx_mux_component_Private->pTmpInputBuffer->nAllocLen=DEFAULT_OUT_BUFFER_SIZE;
  omx_mux_component_Private->pTmpInputBuffer->nOffset=0;
 
  omx_mux_component_Private->avformatReady = OMX_FALSE;
  if(!omx_mux_component_Private->avformatSyncSem) {
    omx_mux_component_Private->avformatSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_mux_component_Private->avformatSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_mux_component_Private->avformatSyncSem, 0);
  }
  omx_mux_component_Private->sOutputFileName = malloc(DEFAULT_FILENAME_LENGTH);
  /*Default Coding type*/
  omx_mux_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  omx_mux_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3; 
  av_register_all();  /* without this file opening gives an error */

  SetInternalVideoParameters(openmaxStandComp);
  SetInternalAudioParameters(openmaxStandComp);

  return err;
}

/* The Destructor */
OMX_ERRORTYPE omx_mux_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_mux_component_Private->avformatSyncSem) {
    tsem_deinit(omx_mux_component_Private->avformatSyncSem);
    free(omx_mux_component_Private->avformatSyncSem);
    omx_mux_component_Private->avformatSyncSem=NULL;
  }

  if(omx_mux_component_Private->sOutputFileName) {
    free(omx_mux_component_Private->sOutputFileName);
    omx_mux_component_Private->sOutputFileName = NULL;
  }

  if(omx_mux_component_Private->pTmpInputBuffer) {
    free(omx_mux_component_Private->pTmpInputBuffer);
  }
  
  /* frees port/s */
  if (omx_mux_component_Private->ports) {
    for (i=0; i < (omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts  +
                   omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts ); i++) {
      if(omx_mux_component_Private->ports[i])
        omx_mux_component_Private->ports[i]->PortDestructor(omx_mux_component_Private->ports[i]);
    }
    free(omx_mux_component_Private->ports);
    omx_mux_component_Private->ports=NULL;
  }
  
  noMuxInstance--;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);
  return omx_base_sink_Destructor(openmaxStandComp);
}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_mux_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPortVideo = (omx_base_video_PortType *)omx_mux_component_Private->ports[VIDEO_PORT_INDEX];
  //omx_base_audio_PortType *pPortAudio = (omx_base_audio_PortType *) omx_mux_component_Private->ports[AUDIO_PORT_INDEX];
  
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  /* auto detect the output format from the name. default is 3gp. */
  omx_mux_component_Private->avoutputformat = guess_format(NULL, (char*)omx_mux_component_Private->sOutputFileName, NULL);
  if (!omx_mux_component_Private->avoutputformat) {
      printf("Could not deduce output format from file extension: using MPEG.\n");
      omx_mux_component_Private->avoutputformat = guess_format("mpeg", NULL, NULL);
  }
  if (!omx_mux_component_Private->avoutputformat) {
      DEBUG(DEB_LEV_ERR, "Could not find suitable output format\n");
      return OMX_ErrorBadParameter;
  }

  /* allocate the output media context */
  omx_mux_component_Private->avformatcontext = av_alloc_format_context();
  if (!omx_mux_component_Private->avformatcontext) {
      DEBUG(DEB_LEV_ERR, "Memory error\n");
      return OMX_ErrorBadParameter;
  }
  omx_mux_component_Private->avformatcontext->oformat = omx_mux_component_Private->avoutputformat;
  snprintf(omx_mux_component_Private->avformatcontext->filename, sizeof(omx_mux_component_Private->avformatcontext->filename), "%s", (char*)omx_mux_component_Private->sOutputFileName);

  /* add the audio and video streams using the default format codecs
     and initialize the codecs */
  omx_mux_component_Private->video_st = NULL;
  omx_mux_component_Private->audio_st = NULL;
  if (omx_mux_component_Private->avoutputformat->video_codec != CODEC_ID_NONE) {
    omx_mux_component_Private->video_st = add_video_stream(openmaxStandComp,omx_mux_component_Private->avformatcontext, CODEC_ID_MPEG4);
  }
  if (omx_mux_component_Private->avoutputformat->audio_codec != CODEC_ID_NONE) {
    if(omx_mux_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
      omx_mux_component_Private->audio_st = add_audio_stream(openmaxStandComp,omx_mux_component_Private->avformatcontext, CODEC_ID_AMR_NB);
    } else if(omx_mux_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8) {
      omx_mux_component_Private->audio_st = add_audio_stream(openmaxStandComp,omx_mux_component_Private->avformatcontext, CODEC_ID_AMR_WB);
    } 
  }

  /* set the output parameters (must be done even if no
     parameters). */

  omx_mux_component_Private->video_st->codec->codec_id = CODEC_ID_MPEG4;
  omx_mux_component_Private->video_st->time_base.den   = pPortVideo->sPortParam.format.video.xFramerate;
  omx_mux_component_Private->video_st->time_base.num   = 1;
  omx_mux_component_Private->video_st->pts.den         = pPortVideo->sPortParam.format.video.xFramerate;
  omx_mux_component_Private->video_st->pts.num         = 0;
  omx_mux_component_Private->video_st->pts.val         = 0;
  

  if(omx_mux_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
    omx_mux_component_Private->audio_st->codec->codec_id   = CODEC_ID_AMR_NB;
    omx_mux_component_Private->audio_st->time_base.den     = 8000;
    omx_mux_component_Private->audio_st->pts.den           = 8000;
    omx_mux_component_Private->audio_st->codec->sample_rate = 8000;
    omx_mux_component_Private->audio_st->codec->channels   = 1;
    omx_mux_component_Private->audio_st->codec->frame_size = 160;
    omx_mux_component_Private->audio_st->time_base.num     = 1;
  } else if(omx_mux_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8) {
    omx_mux_component_Private->audio_st->codec->codec_id   = CODEC_ID_AMR_WB;
    omx_mux_component_Private->audio_st->codec->sample_rate = 16000;
    omx_mux_component_Private->audio_st->codec->channels   = 1;
    omx_mux_component_Private->audio_st->time_base.den     = 16000;
    omx_mux_component_Private->audio_st->pts.den           = 16000;
    omx_mux_component_Private->audio_st->codec->frame_size = 320;
    omx_mux_component_Private->audio_st->time_base.num     = 1;
  }

  DEBUG(DEB_LEV_PARAMS, "In %s val=%lld, num=%d, den=%d frame_size=%d\n", __func__,
      omx_mux_component_Private->audio_st->pts.val,
      omx_mux_component_Private->audio_st->time_base.num,
      omx_mux_component_Private->audio_st->time_base.den,
      omx_mux_component_Private->audio_st->codec->frame_size);
  
  omx_mux_component_Private->audio_st->codec->time_base.den = 1;
  omx_mux_component_Private->audio_st->codec->time_base.num = 0;
  omx_mux_component_Private->audio_st->pts.num              = 0;
  omx_mux_component_Private->audio_st->pts.val              = 0;

  omx_mux_component_Private->video_frame_count = 0;

  if (av_set_parameters(omx_mux_component_Private->avformatcontext, NULL) < 0) {
      DEBUG(DEB_LEV_ERR, "Invalid output format parameters\n");
      return OMX_ErrorBadParameter;
  }

  dump_format(omx_mux_component_Private->avformatcontext, 0, (char*)omx_mux_component_Private->sOutputFileName, 1);

  /* open the output file, if needed */
  if (!(omx_mux_component_Private->avoutputformat->flags & AVFMT_NOFILE)) {
    if (url_fopen(&omx_mux_component_Private->avformatcontext->pb, (char*)omx_mux_component_Private->sOutputFileName, URL_WRONLY) < 0) {
      DEBUG(DEB_LEV_ERR, "Could not open '%s'\n", (char*)omx_mux_component_Private->sOutputFileName);
      return OMX_ErrorBadParameter;
    }
  }
  
  /* write the stream header, if any */
  av_write_header(omx_mux_component_Private->avformatcontext);

  omx_mux_component_Private->avformatReady = OMX_TRUE;
  /*Indicate that avformat is ready*/
  tsem_up(omx_mux_component_Private->avformatSyncSem);

  return OMX_ErrorNone;
}

static int total_video_frame = 0 , total_audio_frame = 0;

/** The DeInitialization function 
 */
OMX_ERRORTYPE omx_mux_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  DEBUG(DEB_LEV_ERR, "In %s Total Video Frame=%d, Audio Frame=%d\n",__func__,total_video_frame, total_audio_frame);

  /* write the trailer, if any */
  av_write_trailer(omx_mux_component_Private->avformatcontext);

  /* free the streams */
  for(i = 0; i < omx_mux_component_Private->avformatcontext->nb_streams; i++) {
      av_freep(&omx_mux_component_Private->avformatcontext->streams[i]->codec);
      av_freep(&omx_mux_component_Private->avformatcontext->streams[i]);
  }

  if (!(omx_mux_component_Private->avoutputformat->flags & AVFMT_NOFILE)) {
       /* close the output file */
#if FFMPEG_LIBNAME_HEADERS
       url_fclose(omx_mux_component_Private->avformatcontext->pb);
#else
      url_fclose(&omx_mux_component_Private->avformatcontext->pb);
#endif
   }

  /* free the stream */
  av_free(omx_mux_component_Private->avformatcontext);
  
  omx_mux_component_Private->avformatReady = OMX_FALSE;
  tsem_reset(omx_mux_component_Private->avformatSyncSem);

  return OMX_ErrorNone;
}

/** 
 * This function processes the input file and returns packet by packet as an output data
 * this packet is used in audio/video decoder component for decoding
 */
void omx_mux_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer) {
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  int error;
  static int first_amr_packet = 1;
  static double video_pts = 0.0,audio_pts = 0.0;
  
  
  if (omx_mux_component_Private->avformatReady == OMX_FALSE) {
    if(omx_mux_component_Private->state == OMX_StateExecuting) {
      /*wait for avformat to be ready*/
      tsem_down(omx_mux_component_Private->avformatSyncSem);
    } else {
      return;
    }
  }
  
  if (pInputBuffer->nInputPortIndex == 0) {
    video_pts = (double)(omx_mux_component_Private->video_st->pts.val) * 
                         omx_mux_component_Private->video_st->time_base.num / 
                         omx_mux_component_Private->video_st->time_base.den;
  }

  if (pInputBuffer->nInputPortIndex == 1) {
    audio_pts = (double)(omx_mux_component_Private->audio_st->pts.val) * 
                         omx_mux_component_Private->audio_st->time_base.num / 
                         omx_mux_component_Private->audio_st->time_base.den;
    
    DEBUG(DEB_LEV_FULL_SEQ, "In val=%lld, num=%d, den=%d apts =%f frame_size=%d\n",
      omx_mux_component_Private->audio_st->pts.val,
      omx_mux_component_Private->audio_st->time_base.num,
      omx_mux_component_Private->audio_st->time_base.den,
      audio_pts,
      omx_mux_component_Private->audio_st->codec->frame_size);
      
  }

  /* write interleaved audio and video frames */
  if ((audio_pts < video_pts) && ((video_pts - audio_pts) > 0.05) && (pInputBuffer->nInputPortIndex == 0)) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping video pts audio=%f video=%f\n",__func__, audio_pts, video_pts);
    return;
  }
 
  if((audio_pts > video_pts) && ((audio_pts - video_pts) > 0.08) && (pInputBuffer->nInputPortIndex == 1)) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping audio pts audio=%f video=%f diff = %f \n",__func__, audio_pts, video_pts,(audio_pts - video_pts));
    return;
  }

  AVPacket pkt;
  av_init_packet(&pkt);

  pkt.stream_index= pInputBuffer->nInputPortIndex;
  pkt.data= (uint8_t *)pInputBuffer->pBuffer;
  pkt.size= pInputBuffer->nFilledLen;


  //DEBUG(DEB_LEV_ERR, "In %s pts audio=%f video=%f writing for port=%d Video Frame=%d, Audio Frame=%d \n",__func__, audio_pts,video_pts,(int)pInputBuffer->nInputPortIndex,total_video_frame, total_audio_frame);
  if(pInputBuffer->nInputPortIndex == 0) {
    pkt.pts = ++omx_mux_component_Private->video_frame_count;
    omx_mux_component_Private->video_st->pts.val++;

    total_video_frame++;

    if(pInputBuffer->nFlags & OMX_BUFFERFLAG_KEY_FRAME) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s received key frame size=%d nFlag=%x\n",__func__,
          (int)pInputBuffer->nFilledLen,(int)pInputBuffer->nFlags);

      pkt.flags |= PKT_FLAG_KEY;
      pInputBuffer->nFlags = pInputBuffer->nFlags & ~OMX_BUFFERFLAG_KEY_FRAME;
    }
    //Sample Video Str den=25,num=1,codec den=25,num=1 pkt->pts=7b dts=8000000000000000 st pts den=25 num=0
  } else  {
    pkt.pts = 0;
    pkt.flags |= PKT_FLAG_KEY;

    total_audio_frame++;

    if(first_amr_packet == 1 ) {
      first_amr_packet = 0;
      if(omx_mux_component_Private->pAudioAmr.eAMRBandMode <= OMX_AUDIO_AMRBandModeNB7) {
        pkt.data= (uint8_t *)&pInputBuffer->pBuffer[6];
        pkt.size= pInputBuffer->nFilledLen - 6;
      } else {
        pkt.data= (uint8_t *)&pInputBuffer->pBuffer[9];
        pkt.size= pInputBuffer->nFilledLen - 9;
      }
    }
    //Sample Audio Str den=8000,num=1,codec den=1,num=0 pkt->pts=0 dts=8000000000000000 st pts den=8000 num=0
  }
  
  DEBUG(DEB_LEV_FULL_SEQ,"In %s Video Str den=%d,num=%d,codec den=%d,num=%d\n",__func__,
    omx_mux_component_Private->video_st->time_base.den,
    omx_mux_component_Private->video_st->time_base.num,
    omx_mux_component_Private->video_st->codec->time_base.den,
    omx_mux_component_Private->video_st->codec->time_base.num);
  
  //DEBUG(DEB_LEV_ERR, "In %s pts audio=%f video=%f writing for port=%d Video Frame=%d, Audio Frame=%d \n",__func__, audio_pts,video_pts,(int)pInputBuffer->nInputPortIndex,total_video_frame, total_audio_frame);
  error = av_write_frame(omx_mux_component_Private->avformatcontext, &pkt);
  
  av_free_packet(&pkt);

  pInputBuffer->nFilledLen = 0;
  pInputBuffer->nOffset = 0;
    
  /** return the current input buffer */
  DEBUG(DEB_LEV_FULL_SEQ, "One input buffer %x len=%d is full returning\n", (int)pInputBuffer->pBuffer, (int)pInputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_mux_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE                   err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_AMRTYPE        *pAudioAmr;
  OMX_VIDEO_PARAM_MPEG4TYPE      *pVideoMpeg4;
  OMX_U32                         portIndex;
  OMX_U32                         nFileNameLength;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType* pVideoPort = (omx_base_video_PortType *) omx_mux_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  omx_base_audio_PortType* pAudioPort = (omx_base_audio_PortType *) omx_mux_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX_1];

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
      err = OMX_ErrorBadPortIndex;
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
      err = OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexVendorOutputFilename : 
    nFileNameLength = strlen((char *)ComponentParameterStructure) * sizeof(char) + 1;
    if(nFileNameLength > DEFAULT_FILENAME_LENGTH) {
      free(omx_mux_component_Private->sOutputFileName);
      omx_mux_component_Private->sOutputFileName = malloc(nFileNameLength);
    }
    strcpy(omx_mux_component_Private->sOutputFileName, (char *)ComponentParameterStructure);
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
        pAudioAmr->nBitRate = 7900;
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
      case OMX_AUDIO_AMRBandModeWB1:                 /**< AMRWB Mode 1 =  8840 bps */
        pAudioAmr->nBitRate = 8840;
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
        err = OMX_ErrorBadParameter;
        break;
      }

      memcpy(&omx_mux_component_Private->pAudioAmr,pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE));
    } else {
      err = OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamVideoMpeg4:
    pVideoMpeg4 = ComponentParameterStructure;
    portIndex = pVideoMpeg4->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    if(err != OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    if (pVideoMpeg4->nPortIndex == 0) {
      memcpy(&omx_mux_component_Private->pVideoMpeg4, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    } else {
      err = OMX_ErrorBadPortIndex;
    }
    break;
  default: /*Call the base component function*/
    err = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_mux_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE                   err = OMX_ErrorNone;
  OMX_PORT_PARAM_TYPE            *pVideoPortParam, *pAudioPortParam;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_AMRTYPE        *pAudioAmr;
  OMX_VIDEO_PARAM_MPEG4TYPE      *pVideoMpeg4;
  
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pVideoPort = (omx_base_video_PortType *) omx_mux_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  omx_base_audio_PortType* pAudioPort = (omx_base_audio_PortType *) omx_mux_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX_1];


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
      err = OMX_ErrorBadPortIndex;
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
      err = OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioAmr:  
    pAudioAmr = (OMX_AUDIO_PARAM_AMRTYPE*)ComponentParameterStructure;
    if (pAudioAmr->nPortIndex != 1) {
      err = OMX_ErrorBadPortIndex;
      break;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AMRTYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioAmr,&omx_mux_component_Private->pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE));
    break;
  case OMX_IndexParamVideoMpeg4:
    pVideoMpeg4 = ComponentParameterStructure;
    if (pVideoMpeg4->nPortIndex != 0) {
      err = OMX_ErrorBadPortIndex;
      break;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pVideoMpeg4, &omx_mux_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    break;
  case  OMX_IndexVendorOutputFilename:
    strcpy((char *)ComponentParameterStructure, "still no filename");
    break;
  default: /*Call the base component function*/
    err = omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

/** This function initializes and deinitializes the library related initialization
  * needed for file parsing
  */
OMX_ERRORTYPE omx_mux_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_mux_component_PrivateType* omx_mux_component_Private = (omx_mux_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_STATETYPE oldState = omx_mux_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  /* Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){ 
    if ((message->messageParam == OMX_StateExecuting) && (oldState == OMX_StateIdle)) {
      err = omx_mux_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s  mux Init Failed Error=%x\n",__func__,err); 
        return err;
      }
    } else if ((message->messageParam == OMX_StateIdle) && (oldState == OMX_StateExecuting)) {
      err = omx_mux_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s mux Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }
  }

  return err;
}

/** setting configurations */
OMX_ERRORTYPE omx_mux_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_TIME_CONFIG_TIMESTAMPTYPE* sTimeStamp;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_base_video_PortType *pPort;

  switch (nIndex) {
    case OMX_IndexConfigTimePosition :
      sTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;
      /*Check Structure Header and verify component state*/
      if (sTimeStamp->nPortIndex >= (omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts  +
                                     omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                     omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts )) {
        DEBUG(DEB_LEV_ERR, "Bad Port index %i when the component has %i ports\n", (int)sTimeStamp->nPortIndex, (int)(omx_mux_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts  +
                                                                                                                     omx_mux_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                                                                                                     omx_mux_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts ));
        return OMX_ErrorBadPortIndex;
      }

      err= checkHeader(sTimeStamp , sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      if(err != OMX_ErrorNone) {
        return err;
      }

      if (sTimeStamp->nPortIndex < 1) {
        pPort= (omx_base_video_PortType *)omx_mux_component_Private->ports[sTimeStamp->nPortIndex];
        memcpy(&omx_mux_component_Private->sTimeStamp,sTimeStamp,sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_mux_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  if(strcmp(cParameterName,"OMX.ST.index.param.outputfilename") == 0) {
    *pIndexType = OMX_IndexVendorOutputFilename;
  } else {
    return OMX_ErrorBadParameter;
  }
  return OMX_ErrorNone;
}


/* add a video input stream */
static AVStream *add_video_stream(OMX_COMPONENTTYPE* openmaxStandComp,AVFormatContext *oc, int codec_id)
{
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPortVideo = (omx_base_video_PortType *)omx_mux_component_Private->ports[VIDEO_PORT_INDEX]; 
  AVCodecContext *c;
  AVStream *st;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);

  st = av_new_stream(oc, 0);
  if (!st) {
      fprintf(stderr, "Could not alloc stream\n");
      exit(1);
  }

  c = st->codec;
  c->codec_id = codec_id;
  c->codec_type = CODEC_TYPE_VIDEO;

  /* put sample parameters */
  c->bit_rate = 400000;
  /* resolution must be a multiple of two */
  c->width  = pPortVideo->sPortParam.format.video.nFrameWidth;
  c->height = pPortVideo->sPortParam.format.video.nFrameHeight;
  /* time base: this is the fundamental unit of time (in seconds) in terms
     of which frame timestamps are represented. for fixed-fps content,
     timebase should be 1/framerate and timestamp increments should be
     identically 1. */
  c->time_base.den = pPortVideo->sPortParam.format.video.xFramerate;
  c->time_base.num = 1;
  c->gop_size = omx_mux_component_Private->pVideoMpeg4.nPFrames + 1; /* emit one intra frame every twelve frames at most */
  c->pix_fmt = STREAM_PIX_FMT;
  if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
      /* just for testing, we also add B frames */
      c->max_b_frames = 2;
  }
  if (c->codec_id == CODEC_ID_MPEG1VIDEO){
      /* Needed to avoid using macroblocks in which some coeffs overflow.
         This does not happen with normal video, it just happens here as
         the motion of the chroma plane does not match the luma plane. */
      c->mb_decision=2;
  }
  // some formats want stream headers to be separate
  if(!strcmp(oc->oformat->name, "mp4") || !strcmp(oc->oformat->name, "mov") || !strcmp(oc->oformat->name, "3gp"))
      c->flags |= CODEC_FLAG_GLOBAL_HEADER;

  return st;
}

/*
 * add an audio input stream
 */
static AVStream *add_audio_stream(OMX_COMPONENTTYPE* openmaxStandComp,AVFormatContext *oc, int codec_id)
{
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  AVCodecContext *c;
  AVStream *st;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);

  st = av_new_stream(oc, 1);
  if (!st) {
      fprintf(stderr, "Could not alloc stream\n");
      exit(1);
  }

  c = st->codec;
  c->codec_id = codec_id;
  c->codec_type = CODEC_TYPE_AUDIO;

  /* put sample parameters */
  c->bit_rate = omx_mux_component_Private->pAudioAmr.nBitRate;
  c->sample_rate = 8000;
  c->channels = 1;
  return st;
}

/** internal function to set video port parameters
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPortVideo = (omx_base_video_PortType *)omx_mux_component_Private->ports[VIDEO_PORT_INDEX]; 

  strcpy(pPortVideo->sPortParam.format.video.cMIMEType,"video/mpeg4");
  pPortVideo->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  pPortVideo->sVideoParam.eCompressionFormat            = OMX_VIDEO_CodingMPEG4;
  pPortVideo->sPortParam.format.video.nFrameWidth   = 320;
  pPortVideo->sPortParam.format.video.nFrameHeight  = 240;
  pPortVideo->sPortParam.format.video.xFramerate    = 10;
  
  setHeader(&omx_mux_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));    
  omx_mux_component_Private->pVideoMpeg4.nPortIndex           = 0; 
  omx_mux_component_Private->pVideoMpeg4.nSliceHeaderSpacing  = 0;
  omx_mux_component_Private->pVideoMpeg4.bSVH                 = OMX_FALSE;
  omx_mux_component_Private->pVideoMpeg4.bGov                 = OMX_TRUE;
  omx_mux_component_Private->pVideoMpeg4.nPFrames             = 11;
  omx_mux_component_Private->pVideoMpeg4.nBFrames             = 0;
  omx_mux_component_Private->pVideoMpeg4.nIDCVLCThreshold     = 0;
  omx_mux_component_Private->pVideoMpeg4.bACPred              = OMX_FALSE;
  omx_mux_component_Private->pVideoMpeg4.nMaxPacketSize       = 0;
  omx_mux_component_Private->pVideoMpeg4.nTimeIncRes          = 0;
  omx_mux_component_Private->pVideoMpeg4.eProfile             = OMX_VIDEO_MPEG4ProfileSimple;
  omx_mux_component_Private->pVideoMpeg4.eLevel               = OMX_VIDEO_MPEG4Level0;
  omx_mux_component_Private->pVideoMpeg4.nAllowedPictureTypes = 0;
  omx_mux_component_Private->pVideoMpeg4.nHeaderExtension     = 0;
  omx_mux_component_Private->pVideoMpeg4.bReversibleVLC       = OMX_FALSE;

}

void SetInternalAudioParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_mux_component_PrivateType* omx_mux_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *pPortAudio = (omx_base_audio_PortType *) omx_mux_component_Private->ports[AUDIO_PORT_INDEX];

  strcpy(pPortAudio->sPortParam.format.audio.cMIMEType, "audio/amr");
  pPortAudio->sPortParam.format.audio.eEncoding  = OMX_AUDIO_CodingAMR;
  pPortAudio->sAudioParam.nIndex                 = OMX_IndexParamAudioAmr;
  pPortAudio->sAudioParam.eEncoding              = OMX_AUDIO_CodingAMR;
                                                                                                                           
  setHeader(&omx_mux_component_Private->pAudioAmr,sizeof(OMX_AUDIO_PARAM_AMRTYPE)); 
  omx_mux_component_Private->pAudioAmr.nPortIndex      = 1;
  omx_mux_component_Private->pAudioAmr.nChannels       = 1;    
  omx_mux_component_Private->pAudioAmr.nBitRate        = 12200;
  omx_mux_component_Private->pAudioAmr.eAMRBandMode    = OMX_AUDIO_AMRBandModeNB7;
  omx_mux_component_Private->pAudioAmr.eAMRDTXMode     = OMX_AUDIO_AMRDTXModeOff;
  omx_mux_component_Private->pAudioAmr.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatConformance;
}
