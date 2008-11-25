/**
  @file src/components/ffmpeg/omx_videoenc_component.c

  This component implements MPEG-4 video encoder.
  The MPEG-4 Video encoder is based on FFmpeg software library.

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
#include <omx_base_video_port.h>
#include <omx_videoenc_component.h>
#include<OMX_Video.h>

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_VIDEOENC 4

/** Counter of Video Component Instance*/
static OMX_U32 noVideoEncInstance = 0;

/** The output encoded color format */
#define OUTPUT_ENCODED_COLOR_FMT OMX_COLOR_FormatYUV420Planar

/** The Constructor of the video encoder component
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_videoenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private;
  omx_base_video_PortType *inPort,*outPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_videoenc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videoenc_component_Private->ports = NULL;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?
    */
  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  if (omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_videoenc_component_Private->ports) {
    omx_videoenc_component_Private->ports = calloc(omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
    if (!omx_videoenc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      omx_videoenc_component_Private->ports[i] = calloc(1, sizeof(omx_base_video_PortType));
      if (!omx_videoenc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_video_port_Constructor(openmaxStandComp, &omx_videoenc_component_Private->ports[0], 0, OMX_TRUE);
  base_video_port_Constructor(openmaxStandComp, &omx_videoenc_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports.
    * first we set the parameter common to both formats
    */
  //common parameters related to input port
  inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  inPort->sPortParam.format.video.nFrameWidth = 176;
  inPort->sPortParam.format.video.nFrameHeight = 144;
  inPort->sPortParam.nBufferSize = inPort->sPortParam.format.video.nFrameWidth*
                                   inPort->sPortParam.format.video.nFrameHeight*3/2; //YUV 420
  inPort->sPortParam.format.video.xFramerate = 25;
  inPort->sPortParam.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  inPort->sVideoParam.eColorFormat = OMX_COLOR_FormatYUV420Planar;

  //common parameters related to output port
  outPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.nBufferSize = FF_MIN_BUFFER_SIZE;
  outPort->sPortParam.format.video.xFramerate = 25;
  outPort->sPortParam.format.video.nFrameWidth = 176;
  outPort->sPortParam.format.video.nFrameHeight = 144;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_ENC_MPEG4_NAME)) {
    omx_videoenc_component_Private->video_encoding_type = OMX_VIDEO_CodingMPEG4;
  } else if (!strcmp(cComponentName, VIDEO_ENC_BASE_NAME)) {
    omx_videoenc_component_Private->video_encoding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name
    DEBUG(DEB_LEV_ERR, "In valid component name\n");
    return OMX_ErrorInvalidComponentName;
  }

  if(!omx_videoenc_component_Private->avCodecSyncSem) {
    omx_videoenc_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_videoenc_component_Private->avCodecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_videoenc_component_Private->avCodecSyncSem, 0);
  }

  SetInternalVideoEncParameters(openmaxStandComp);

  omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;

  if(omx_videoenc_component_Private->video_encoding_type == OMX_VIDEO_CodingMPEG4) {
    omx_videoenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  }

  /** general configuration irrespective of any video formats
    * setting other parameters of omx_videoenc_component_private
    */
  omx_videoenc_component_Private->avCodec = NULL;
  omx_videoenc_component_Private->avCodecContext= NULL;
  omx_videoenc_component_Private->avcodecReady = OMX_FALSE;
  omx_videoenc_component_Private->BufferMgmtCallback = omx_videoenc_component_BufferMgmtCallback;

  /** initializing the coenc context etc that was done earlier by ffmpeglibinit function */
  omx_videoenc_component_Private->messageHandler = omx_videoenc_component_MessageHandler;
  omx_videoenc_component_Private->destructor = omx_videoenc_component_Destructor;
  openmaxStandComp->SetParameter = omx_videoenc_component_SetParameter;
  openmaxStandComp->GetParameter = omx_videoenc_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_videoenc_component_ComponentRoleEnum;

  noVideoEncInstance++;

  if(noVideoEncInstance > MAX_COMPONENT_VIDEOENC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}


/** The destructor of the video encoder component
  */
OMX_ERRORTYPE omx_videoenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_videoenc_component_Private->avCodecSyncSem) {
    free(omx_videoenc_component_Private->avCodecSyncSem);
    omx_videoenc_component_Private->avCodecSyncSem = NULL;
  }

  /* frees port/s */
  if (omx_videoenc_component_Private->ports) {
    for (i=0; i < omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      if(omx_videoenc_component_Private->ports[i])
        omx_videoenc_component_Private->ports[i]->PortDestructor(omx_videoenc_component_Private->ports[i]);
    }
    free(omx_videoenc_component_Private->ports);
    omx_videoenc_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of video encoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  noVideoEncInstance--;

  return OMX_ErrorNone;
}


/** It initializates the FFmpeg framework, and opens an FFmpeg videoencoder of type specified by IL client
  */
OMX_ERRORTYPE omx_videoenc_component_ffmpegLibInit(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {

  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  OMX_U32 target_coencID;
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFmpeg library/encoder initialized\n");

  switch(omx_videoenc_component_Private->video_encoding_type) {
    case OMX_VIDEO_CodingMPEG4 :
      target_coencID = CODEC_ID_MPEG4;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "\n encoders other than MPEG-4 are not supported -- encoder not found\n");
      return OMX_ErrorComponentNotFound;
  }

  /** Find the  encoder corresponding to the video type specified by IL client*/
  omx_videoenc_component_Private->avCodec = avcodec_find_encoder(target_coencID);
  if (omx_videoenc_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Encoder Not found\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_videoenc_component_Private->avCodecContext = avcodec_alloc_context();
  omx_videoenc_component_Private->picture = avcodec_alloc_frame ();

  /* put sample parameters */
  omx_videoenc_component_Private->avCodecContext->bit_rate = 200000; /* bit per second */
  omx_videoenc_component_Private->avCodecContext->bit_rate_tolerance = 4000000; /* bit per second */
  omx_videoenc_component_Private->avCodecContext->width  = inPort->sPortParam.format.video.nFrameWidth;
  omx_videoenc_component_Private->avCodecContext->height = inPort->sPortParam.format.video.nFrameHeight;

  /* frames per second */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Frame Rate=%d\n",(int)inPort->sPortParam.format.video.xFramerate);
  omx_videoenc_component_Private->avCodecContext->time_base= (AVRational){1,inPort->sPortParam.format.video.xFramerate};
  omx_videoenc_component_Private->avCodecContext->gop_size = omx_videoenc_component_Private->pVideoMpeg4.nPFrames + 1; /* emit one intra frame every twelve frames */
  omx_videoenc_component_Private->avCodecContext->pix_fmt = PIX_FMT_YUV420P;
  omx_videoenc_component_Private->avCodecContext->strict_std_compliance = FF_COMPLIANCE_NORMAL;
  omx_videoenc_component_Private->avCodecContext->sample_fmt = SAMPLE_FMT_S16;
  omx_videoenc_component_Private->avCodecContext->qmin = 2;
  omx_videoenc_component_Private->avCodecContext->qmax = 31;
  omx_videoenc_component_Private->avCodecContext->workaround_bugs |= FF_BUG_AUTODETECT;


  if(omx_videoenc_component_Private->pVideoMpeg4.eProfile == OMX_VIDEO_MPEG4ProfileAdvancedScalable) {
    omx_videoenc_component_Private->avCodecContext->max_b_frames = omx_videoenc_component_Private->pVideoMpeg4.nBFrames;
  }

  if(omx_videoenc_component_Private->pVideoMpeg4.bACPred == OMX_TRUE) {
    omx_videoenc_component_Private->avCodecContext->flags |= CODEC_FLAG_AC_PRED;
  }

#if 0 /*Add checking with bit rate and M4V levels*/
  switch(omx_videoenc_component_Private->pVideoMpeg4.eLevel) {
  case OMX_VIDEO_MPEG4Level0:                   /**< Level 0 */
  case OMX_VIDEO_MPEG4Level0b:                  /**< Level 0b */
  case OMX_VIDEO_MPEG4Level1:                   /**< Level 1 */
  case OMX_VIDEO_MPEG4Level2:                   /**< Level 2 */
  case OMX_VIDEO_MPEG4Level3:                   /**< Level 3 */
  case OMX_VIDEO_MPEG4Level4:                   /**< Level 4 */
  case OMX_VIDEO_MPEG4Level4a:                  /**< Level 4a */
  case OMX_VIDEO_MPEG4Level5:                   /**< Level 5 */
  case default:
    break;
  }
#endif
  if (avcodec_open(omx_videoenc_component_Private->avCodecContext, omx_videoenc_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open encoder\n");
    return OMX_ErrorInsufficientResources;
  }
  tsem_up(omx_videoenc_component_Private->avCodecSyncSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");

  return OMX_ErrorNone;
}

/** It Deinitializates the ffmpeg framework, and close the ffmpeg video encoder of selected coding type
  */
void omx_videoenc_component_ffmpegLibDeInit(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {

  avcodec_close(omx_videoenc_component_Private->avCodecContext);
  if (omx_videoenc_component_Private->avCodecContext->priv_data) {
    avcodec_close (omx_videoenc_component_Private->avCodecContext);
  }
  if (omx_videoenc_component_Private->avCodecContext->extradata) {
    av_free (omx_videoenc_component_Private->avCodecContext->extradata);
    omx_videoenc_component_Private->avCodecContext->extradata = NULL;
  }
  av_free (omx_videoenc_component_Private->avCodecContext);

  av_free(omx_videoenc_component_Private->picture);

}

/** internal function to set coenc related parameters in the private type structure
  */
void SetInternalVideoEncParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  if (omx_videoenc_component_Private->video_encoding_type == OMX_VIDEO_CodingMPEG4) {
    strcpy(outPort->sPortParam.format.video.cMIMEType,"video/mpeg4");
    outPort->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
    outPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    setHeader(&omx_videoenc_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    omx_videoenc_component_Private->pVideoMpeg4.nPortIndex = 1;
    omx_videoenc_component_Private->pVideoMpeg4.nSliceHeaderSpacing = 0;
    omx_videoenc_component_Private->pVideoMpeg4.bSVH = OMX_FALSE;
    omx_videoenc_component_Private->pVideoMpeg4.bGov = OMX_TRUE;
    omx_videoenc_component_Private->pVideoMpeg4.nPFrames = 11;
    omx_videoenc_component_Private->pVideoMpeg4.nBFrames = 0;
    omx_videoenc_component_Private->pVideoMpeg4.nIDCVLCThreshold = 0;
    omx_videoenc_component_Private->pVideoMpeg4.bACPred = OMX_FALSE;
    omx_videoenc_component_Private->pVideoMpeg4.nMaxPacketSize = 0;
    omx_videoenc_component_Private->pVideoMpeg4.nTimeIncRes = 0;
    omx_videoenc_component_Private->pVideoMpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    omx_videoenc_component_Private->pVideoMpeg4.eLevel = OMX_VIDEO_MPEG4Level0;
    omx_videoenc_component_Private->pVideoMpeg4.nAllowedPictureTypes = 0;
    omx_videoenc_component_Private->pVideoMpeg4.nHeaderExtension = 0;
    omx_videoenc_component_Private->pVideoMpeg4.bReversibleVLC = OMX_FALSE;

  }
}


/** The Initialization function of the video encoder
  */
OMX_ERRORTYPE omx_videoenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  /** Temporary First Output buffer size */
  omx_videoenc_component_Private->isFirstBuffer = 1;
  omx_videoenc_component_Private->isNewBuffer = 1;

  return eError;
}

/** The Deinitialization function of the video encoder
  */
OMX_ERRORTYPE omx_videoenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (omx_videoenc_component_Private->avcodecReady) {
    omx_videoenc_component_ffmpegLibDeInit(omx_videoenc_component_Private);
    omx_videoenc_component_Private->avcodecReady = OMX_FALSE;
  }

  return eError;
}

/** Executes all the required steps after an output buffer frame-size has changed.
*/
static inline void UpdateFrameSize(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  switch(inPort->sPortParam.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
      inPort->sPortParam.nBufferSize = inPort->sPortParam.format.video.nFrameWidth * inPort->sPortParam.format.video.nFrameHeight * 3/2;
      break;
    default:
      inPort->sPortParam.nBufferSize = inPort->sPortParam.format.video.nFrameWidth * inPort->sPortParam.format.video.nFrameHeight * 3;
      break;
  }
}

/** This function is used to process the input buffer and provide one output buffer
  */
void omx_videoenc_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

  OMX_S32 nOutputFilled = 0;
  OMX_U8* outputCurrBuffer;
  OMX_S32 nLen = 0;
  int size;

  size= inPort->sPortParam.format.video.nFrameWidth*inPort->sPortParam.format.video.nFrameHeight;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  /** Fill up the current input buffer when a new buffer has arrived */
  if(omx_videoenc_component_Private->isNewBuffer) {
    omx_videoenc_component_Private->isNewBuffer = 0;
    DEBUG(DEB_LEV_FULL_SEQ, "New Buffer FilledLen = %d\n", (int)pInputBuffer->nFilledLen);

    omx_videoenc_component_Private->picture->data[0] = pInputBuffer->pBuffer;
    omx_videoenc_component_Private->picture->data[1] = omx_videoenc_component_Private->picture->data[0] + size;
    omx_videoenc_component_Private->picture->data[2] = omx_videoenc_component_Private->picture->data[1] + size / 4;
    omx_videoenc_component_Private->picture->linesize[0] = inPort->sPortParam.format.video.nFrameWidth;
    omx_videoenc_component_Private->picture->linesize[1] = inPort->sPortParam.format.video.nFrameWidth / 2;
    omx_videoenc_component_Private->picture->linesize[2] = inPort->sPortParam.format.video.nFrameWidth / 2;
  }

  outputCurrBuffer = pOutputBuffer->pBuffer;
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  while (!nOutputFilled) {
    if (omx_videoenc_component_Private->isFirstBuffer) {
      tsem_down(omx_videoenc_component_Private->avCodecSyncSem);
      omx_videoenc_component_Private->isFirstBuffer = 0;
    }
    omx_videoenc_component_Private->avCodecContext->frame_number++;

    nLen = avcodec_encode_video(omx_videoenc_component_Private->avCodecContext,
                                outputCurrBuffer,
                                pOutputBuffer->nAllocLen,
                                omx_videoenc_component_Private->picture);

    if (nLen < 0) {
      DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not encoded?\n");
    }

    pInputBuffer->nFilledLen = 0;
      omx_videoenc_component_Private->isNewBuffer = 1;
    if ( nLen >= 0) {
      pOutputBuffer->nFilledLen = nLen;
    }
    nOutputFilled = 1;
  }
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x nLen=%d is full returning in video encoder\n",
            (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_videoenc_component_SetParameter(
OMX_IN  OMX_HANDLETYPE hComponent,
OMX_IN  OMX_INDEXTYPE nParamIndex,
OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamPortDefinition:
      {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
        pPortDef = ComponentParameterStructure;
        eError = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if(eError != OMX_ErrorNone) {
          break;
        }
        UpdateFrameSize (openmaxStandComp);
        port = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[pPortDef->nPortIndex];
        port->sVideoParam.eColorFormat = port->sPortParam.format.video.eColorFormat;
        port->sVideoParam.eCompressionFormat = port->sPortParam.format.video.eCompressionFormat;
        break;
      }
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
        pVideoPortFormat = ComponentParameterStructure;
        portIndex = pVideoPortFormat->nPortIndex;
        /*Check Structure Header and verify component state*/
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
          break;
        }
        if (portIndex <= 1) {
          port = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[portIndex];
          memcpy(&port->sVideoParam, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
          omx_videoenc_component_Private->ports[portIndex]->sPortParam.format.video.eColorFormat = port->sVideoParam.eColorFormat;
          omx_videoenc_component_Private->ports[portIndex]->sPortParam.format.video.eCompressionFormat = port->sVideoParam.eCompressionFormat;

          if (portIndex == 1) {
            switch(port->sVideoParam.eColorFormat) {
              case OMX_COLOR_Format24bitRGB888 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_RGB24;
                break;
              case OMX_COLOR_Format24bitBGR888 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_BGR24;
                break;
              case OMX_COLOR_Format32bitBGRA8888 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_BGR32;
                break;
              case OMX_COLOR_Format32bitARGB8888 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_RGB32;
                break;
              case OMX_COLOR_Format16bitARGB1555 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_RGB555;
                break;
              case OMX_COLOR_Format16bitRGB565 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_RGB565;
                break;
              case OMX_COLOR_Format16bitBGR565 :
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_BGR565;
                break;
              default:
                omx_videoenc_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;
                break;
            }
            UpdateFrameSize (openmaxStandComp);
          }
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if (!strcmp((char *)pComponentRole->cRole, VIDEO_ENC_MPEG4_ROLE)) {
          omx_videoenc_component_Private->video_encoding_type = OMX_VIDEO_CodingMPEG4;
        } else {
          return OMX_ErrorBadParameter;
        }
        SetInternalVideoEncParameters(openmaxStandComp);
        break;
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        portIndex = pVideoMpeg4->nPortIndex;
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
          break;
        }
        if (pVideoMpeg4->nPortIndex == 1) {
          memcpy(&omx_videoenc_component_Private->pVideoMpeg4, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return eError;
}

OMX_ERRORTYPE omx_videoenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  omx_base_video_PortType *port;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        break;
      }
      memcpy(ComponentParameterStructure, &omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
      break;
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
        pVideoPortFormat = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
          break;
        }
        if (pVideoPortFormat->nPortIndex <= 1) {
          port = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[pVideoPortFormat->nPortIndex];
          memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        if (pVideoMpeg4->nPortIndex != 1) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE))) != OMX_ErrorNone) {
          break;
        }
        memcpy(pVideoMpeg4, &omx_videoenc_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
          break;
        }
        if (omx_videoenc_component_Private->video_encoding_type == OMX_VIDEO_CodingMPEG4) {
          strcpy((char *)pComponentRole->cRole, VIDEO_ENC_MPEG4_ROLE);
        } else {
          strcpy((char *)pComponentRole->cRole,"\0");
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_videoenc_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {

  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle ) && (omx_videoenc_component_Private->state == OMX_StateLoaded)) {
      if (!omx_videoenc_component_Private->avcodecReady) {
        eError = omx_videoenc_component_ffmpegLibInit(omx_videoenc_component_Private);
        if (eError != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_videoenc_component_Private->avcodecReady = OMX_TRUE;
      }
      eError = omx_videoenc_component_Init(openmaxStandComp);
      if(eError!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Encoder Init Failed Error=%x\n",__func__,eError);
        return eError;
      }
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_videoenc_component_Private->state == OMX_StateIdle)) {
      eError = omx_videoenc_component_Deinit(openmaxStandComp);
      if(eError!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Encoder Deinit Failed Error=%x\n",__func__,eError);
        return eError;
      }
    }
  }
  // Execute the base message handling
  return omx_base_component_MessageHandler(openmaxStandComp,message);
}

OMX_ERRORTYPE omx_videoenc_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, VIDEO_ENC_MPEG4_ROLE);
  }  else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}
