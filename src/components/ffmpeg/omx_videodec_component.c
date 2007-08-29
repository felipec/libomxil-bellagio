/**
  @file src/components/ffmpeg/omx_videodec_component.c
  
  This component implements mpeg4 and avc video decoder. 
	The MPEG4 and avc Video decoder is based on ffmpeg software library.

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
#include <omx_videodec_component.h>
#include<OMX_Video.h>

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_VIDEODEC 4

/** Counter of Video Component Instance*/
OMX_U32 noVideoDecInstance = 0;

/** The output decoded color format */
#define OUTPUT_DECODED_COLOR_FMT OMX_COLOR_FormatYUV420Planar

/** define the max output buffer size */
#define MAX_VIDEO_OUTPUT_BUF_SIZE 460800 //640 * 480 * 1.5
#define MIN_VIDEO_OUTPUT_BUF_SIZE 176*144*3 //640 * 480 * 1.5

/** The Constructor of the video decoder component
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_videodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;	
  omx_videodec_component_PrivateType* omx_videodec_component_Private;
  omx_videodec_component_PortType *inPort,*outPort;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_videodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?	
    */
  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */
  omx_videodec_component_Private = (omx_videodec_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  /** Allocate Ports and Call base port constructor. */	
  if (omx_videodec_component_Private->sPortTypesParam.nPorts && !omx_videodec_component_Private->ports) {
    omx_videodec_component_Private->ports = calloc(omx_videodec_component_Private->sPortTypesParam.nPorts, sizeof (omx_base_PortType *));
    if (!omx_videodec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_videodec_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; size of the struct is for derived class port type
        * this could be refactored as a smarter factory function instead?
        */
      omx_videodec_component_Private->ports[i] = calloc(1, sizeof(omx_videodec_component_PortType));
      if (!omx_videodec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  omx_videodec_component_Private->PortConstructor(openmaxStandComp, &omx_videodec_component_Private->ports[0], 0, OMX_TRUE);
  omx_videodec_component_Private->PortConstructor(openmaxStandComp, &omx_videodec_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports. 	
    * first we set the parameter common to both formats
    */
  //common parameters related to input port
  inPort = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  inPort->sPortParam.eDomain = OMX_PortDomainVideo;
  inPort->sPortParam.format.video.pNativeRender = 0;
  inPort->sPortParam.format.video.bFlagErrorConcealment = OMX_FALSE;
  inPort->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  inPort->sPortParam.format.video.cMIMEType = (OMX_STRING)malloc(sizeof(char)*128);
  strcpy(inPort->sPortParam.format.video.cMIMEType, "raw/video");
  inPort->sPortParam.format.video.nFrameWidth = 0; 
  inPort->sPortParam.format.video.nFrameHeight = 0; 
  inPort->sPortParam.format.video.nStride = 0;
  inPort->sPortParam.format.video.nSliceHeight = 0;
  inPort->sPortParam.format.video.nBitrate = 0;
  inPort->sPortParam.format.video.xFramerate = 25;
  inPort->sPortParam.format.video.eColorFormat = OMX_COLOR_FormatUnused;
  inPort->sPortParam.format.video.pNativeWindow = NULL;

  //common parameters related to output port
  outPort = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.eDomain = OMX_PortDomainVideo;
  outPort->sPortParam.format.video.cMIMEType = "raw";
  outPort->sPortParam.format.video.pNativeRender = 0;
  outPort->sPortParam.format.video.bFlagErrorConcealment = OMX_FALSE;
  outPort->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  outPort->sPortParam.format.video.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sPortParam.nBufferSize = MIN_VIDEO_OUTPUT_BUF_SIZE;
  outPort->sPortParam.format.video.cMIMEType = (OMX_STRING)malloc(sizeof(char)*128);
  strcpy(outPort->sPortParam.format.video.cMIMEType, "raw/video");
  outPort->sPortParam.format.video.nFrameWidth = 0; 
  outPort->sPortParam.format.video.nFrameHeight = 0; 
  outPort->sPortParam.format.video.nStride = 0;
  outPort->sPortParam.format.video.nSliceHeight = 0;
  outPort->sPortParam.format.video.nBitrate = 0;
  outPort->sPortParam.format.video.xFramerate = 25;
  outPort->sPortParam.format.video.pNativeWindow = NULL;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_DEC_MPEG4_NAME)) { 
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
  } else if(!strcmp(cComponentName, VIDEO_DEC_H264_NAME)) { 
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  } else if (!strcmp(cComponentName, VIDEO_DEC_BASE_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name 
    return OMX_ErrorInvalidComponentName;
  }  

  if(!omx_videodec_component_Private->avCodecSyncSem) {
    omx_videodec_component_Private->avCodecSyncSem = malloc(sizeof(tsem_t));
    if(omx_videodec_component_Private->avCodecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_videodec_component_Private->avCodecSyncSem, 0);
  }

  SetInternalVideoParameters(openmaxStandComp);

  /** settings of output port parameter definition */
  setHeader(&outPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
  outPort->sVideoParam.nPortIndex = 1;
  outPort->sVideoParam.nIndex = 1;
  outPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingUnused;
  outPort->sVideoParam.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sVideoParam.xFramerate = 25;

  omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;

  if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  } else {
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }

  /** general configuration irrespective of any video formats
    * setting other parameters of omx_videodec_component_private	
    */
  omx_videodec_component_Private->avCodec = NULL;
  omx_videodec_component_Private->avCodecContext= NULL;
  omx_videodec_component_Private->avcodecReady = OMX_FALSE;
  omx_videodec_component_Private->BufferMgmtCallback = omx_videodec_component_BufferMgmtCallback;

  /** initializing the codec context etc that was done earlier by ffmpeglibinit function */
  omx_videodec_component_Private->messageHandler = omx_videodec_component_MessageHandler;
  omx_videodec_component_Private->destructor = omx_videodec_component_Destructor;
  openmaxStandComp->SetParameter = omx_videodec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_videodec_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_videodec_component_ComponentRoleEnum;

  noVideoDecInstance++;

  if(noVideoDecInstance > MAX_COMPONENT_VIDEODEC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}


/** The destructor of the video decoder component
  */
OMX_ERRORTYPE omx_videodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videodec_component_PortType *pPort;
  OMX_U32 i;

  /* frees port/s */
  if (omx_videodec_component_Private->sPortTypesParam.nPorts && omx_videodec_component_Private->ports) {
    for (i=0; i < omx_videodec_component_Private->sPortTypesParam.nPorts; i++) {
      pPort = (omx_videodec_component_PortType *)omx_videodec_component_Private->ports[i];
      if(pPort->sPortParam.format.video.cMIMEType != NULL) {
        free(pPort->sPortParam.format.video.cMIMEType);
        pPort->sPortParam.format.video.cMIMEType = NULL;
      }
    }
  }

  //tsem_deinit(omx_videodec_component_Private->avCodecSyncSem);

  if(omx_videodec_component_Private->avCodecSyncSem) {
    free(omx_videodec_component_Private->avCodecSyncSem);
    omx_videodec_component_Private->avCodecSyncSem = NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of video decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  noVideoDecInstance--;

  return OMX_ErrorNone;
}


/** It initializates the ffmpeg framework, and opens an ffmpeg videodecoder of type specified by IL client 
  */ 
OMX_ERRORTYPE omx_videodec_component_ffmpegLibInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {

  OMX_U32 target_codecID;  
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_videodec_component_Private->video_coding_type) {
    case OMX_VIDEO_CodingMPEG4 :
      target_codecID = CODEC_ID_MPEG4;
      break;
    case OMX_VIDEO_CodingAVC : 
      target_codecID = CODEC_ID_H264;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "\n codec other than mpeg4 and avc(h264) is not supported -- codec not found\n");
      return OMX_ErrorComponentNotFound;
  }

  /** Find the  decoder corresponding to the video type specified by IL client*/
  omx_videodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_videodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec Not found\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_videodec_component_Private->avCodecContext = avcodec_alloc_context();

  /** necessary flags for mpeg4 or h264 stream */
  if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4 || 
          omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
    omx_videodec_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED; 
  }
  omx_videodec_component_Private->avFrame = avcodec_alloc_frame ();

  if (avcodec_open(omx_videodec_component_Private->avCodecContext, omx_videodec_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }
  tsem_up(omx_videodec_component_Private->avCodecSyncSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");

  return OMX_ErrorNone;
}

/** It Deinitializates the ffmpeg framework, and close the ffmpeg video decoder of selected coding type
  */
void omx_videodec_component_ffmpegLibDeInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {

  avcodec_close(omx_videodec_component_Private->avCodecContext);
  if (omx_videodec_component_Private->avCodecContext->priv_data) {
    avcodec_close (omx_videodec_component_Private->avCodecContext);
  }
  if (omx_videodec_component_Private->avCodecContext->extradata) {
    av_free (omx_videodec_component_Private->avCodecContext->extradata);
    omx_videodec_component_Private->avCodecContext->extradata = NULL;
  }
  av_free (omx_videodec_component_Private->avCodecContext);

  av_free(omx_videodec_component_Private->avFrame);

}

/** internal function to set codec related parameters in the private type structure 
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private;
  omx_videodec_component_PortType *inPort ; 

  omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;;

  if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/mpeg4");
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    setHeader(&omx_videodec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));    
    omx_videodec_component_Private->pVideoMpeg4.nPortIndex = 0;                                                                    
    omx_videodec_component_Private->pVideoMpeg4.nSliceHeaderSpacing = 0;
    omx_videodec_component_Private->pVideoMpeg4.bSVH = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.bGov = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.nPFrames = 0;
    omx_videodec_component_Private->pVideoMpeg4.nBFrames = 0;
    omx_videodec_component_Private->pVideoMpeg4.nIDCVLCThreshold = 0;
    omx_videodec_component_Private->pVideoMpeg4.bACPred = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.nMaxPacketSize = 0;
    omx_videodec_component_Private->pVideoMpeg4.nTimeIncRes = 0;
    omx_videodec_component_Private->pVideoMpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    omx_videodec_component_Private->pVideoMpeg4.eLevel = OMX_VIDEO_MPEG4Level0;
    omx_videodec_component_Private->pVideoMpeg4.nAllowedPictureTypes = 0;
    omx_videodec_component_Private->pVideoMpeg4.nHeaderExtension = 0;
    omx_videodec_component_Private->pVideoMpeg4.bReversibleVLC = OMX_FALSE;

    inPort = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&inPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    inPort->sVideoParam.nPortIndex = 0;
    inPort->sVideoParam.nIndex = 1;
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
    strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/avc(h264)");
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    setHeader(&omx_videodec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    omx_videodec_component_Private->pVideoAvc.nPortIndex = 0;
    omx_videodec_component_Private->pVideoAvc.nSliceHeaderSpacing = 0;
    omx_videodec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_videodec_component_Private->pVideoAvc.nPFrames = 0;
    omx_videodec_component_Private->pVideoAvc.nBFrames = 0;
    omx_videodec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_videodec_component_Private->pVideoAvc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    omx_videodec_component_Private->pVideoAvc.eLevel = OMX_VIDEO_AVCLevel1;
    omx_videodec_component_Private->pVideoAvc.nAllowedPictureTypes = 0;
    omx_videodec_component_Private->pVideoAvc.bFrameMBsOnly = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefIdx10ActiveMinus1 = 0;
    omx_videodec_component_Private->pVideoAvc.nRefIdx11ActiveMinus1 = 0;
    omx_videodec_component_Private->pVideoAvc.bEnableUEP = OMX_FALSE;  
    omx_videodec_component_Private->pVideoAvc.bEnableFMO = OMX_FALSE;  
    omx_videodec_component_Private->pVideoAvc.bEnableASO = OMX_FALSE;  
    omx_videodec_component_Private->pVideoAvc.bEnableRS = OMX_FALSE;   

    omx_videodec_component_Private->pVideoAvc.bMBAFF = OMX_FALSE;               
    omx_videodec_component_Private->pVideoAvc.bEntropyCodingCABAC = OMX_FALSE;  
    omx_videodec_component_Private->pVideoAvc.bWeightedPPrediction = OMX_FALSE; 
    omx_videodec_component_Private->pVideoAvc.nWeightedBipredicitonMode = 0; 
    omx_videodec_component_Private->pVideoAvc.bconstIpred = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bDirect8x8Inference = OMX_FALSE;  
    omx_videodec_component_Private->pVideoAvc.bDirectSpatialTemporal = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nCabacInitIdc = 0;
    omx_videodec_component_Private->pVideoAvc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;

    inPort = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    setHeader(&inPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    inPort->sVideoParam.nPortIndex = 0;
    inPort->sVideoParam.nIndex = 1;
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }
}


/** The Initialization function of the video decoder
  */
OMX_ERRORTYPE omx_videodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /** Temporary First Output buffer size */
  omx_videodec_component_Private->inputCurrBuffer = NULL;
  omx_videodec_component_Private->inputCurrLength = 0;
  nBufferSize = omx_videodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_videodec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(nBufferSize);
  memset(omx_videodec_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_videodec_component_Private->isFirstBuffer = 1;
  omx_videodec_component_Private->positionInOutBuf = 0;
  omx_videodec_component_Private->isNewBuffer = 1;

  return eError;
}

/** The Deinitialization function of the video decoder  
  */
OMX_ERRORTYPE omx_videodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (omx_videodec_component_Private->avcodecReady) {
    omx_videodec_component_ffmpegLibDeInit(omx_videodec_component_Private);
    omx_videodec_component_Private->avcodecReady = OMX_FALSE;
  }

  if(omx_videodec_component_Private->internalOutputBuffer) {
    free(omx_videodec_component_Private->internalOutputBuffer);
    omx_videodec_component_Private->internalOutputBuffer = NULL;
  }

  return eError;
} 

/** This function is used to process the input buffer and provide one output buffer
  */
void omx_videodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videodec_component_PortType *outPort = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  AVPicture pic;

  OMX_S32 nOutputFilled = 0;
  OMX_U8* outputCurrBuffer;
  OMX_U32 nLen = 0;
  int internalOutputFilled=0;
  int nSize;
  struct SwsContext *imgConvertYuvCtx = NULL;

  /** Fill up the current input buffer when a new buffer has arrived */
  if(omx_videodec_component_Private->isNewBuffer) {
    omx_videodec_component_Private->inputCurrBuffer = pInputBuffer->pBuffer;
    omx_videodec_component_Private->inputCurrLength = pInputBuffer->nFilledLen;
    omx_videodec_component_Private->positionInOutBuf = 0;
    omx_videodec_component_Private->isNewBuffer = 0;
    DEBUG(DEB_LEV_FULL_SEQ, "New Buffer FilledLen = %d\n", (int)pInputBuffer->nFilledLen);
  }

  outputCurrBuffer = pOutputBuffer->pBuffer;
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  while (!nOutputFilled) {
    if (omx_videodec_component_Private->isFirstBuffer) {
      tsem_down(omx_videodec_component_Private->avCodecSyncSem);
      omx_videodec_component_Private->isFirstBuffer = 0;
    }
    omx_videodec_component_Private->avCodecContext->frame_number++;

    nLen = avcodec_decode_video(omx_videodec_component_Private->avCodecContext, 
          omx_videodec_component_Private->avFrame, 
			    (int*)&internalOutputFilled,
			    omx_videodec_component_Private->inputCurrBuffer, 
			    omx_videodec_component_Private->inputCurrLength);

    if (nLen < 0) {
      DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
    }

    if((outPort->sPortParam.format.video.nFrameWidth != omx_videodec_component_Private->avCodecContext->width) ||
        (outPort->sPortParam.format.video.nFrameHeight != omx_videodec_component_Private->avCodecContext->height)) {
      DEBUG(DEB_LEV_ERR, "---->Sending Port Settings Change Event in video decoder\n");

      switch(omx_videodec_component_Private->video_coding_type) {
        case OMX_VIDEO_CodingMPEG4 :
        case OMX_VIDEO_CodingAVC :
          outPort->sPortParam.format.video.nFrameWidth = omx_videodec_component_Private->avCodecContext->width;
          outPort->sPortParam.format.video.nFrameHeight = omx_videodec_component_Private->avCodecContext->height;
          break;
        default :
          DEBUG(DEB_LEV_ERR, "Video format other than mpeg4 & avc not supported\nCodec not found\n");
          break;           
      }

      /** Send Port Settings changed call back */
      (*(omx_videodec_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_videodec_component_Private->callbackData,
        OMX_EventPortSettingsChanged, // The command was completed 
        nLen,  //to adjust the file pointer to resume the correct decode process
        1, // This is the output port index 
        NULL);
    }

    if ( nLen >= 0 && internalOutputFilled) {
      omx_videodec_component_Private->inputCurrBuffer += nLen;
      omx_videodec_component_Private->inputCurrLength -= nLen;
      pInputBuffer->nFilledLen -= nLen;

      //Buffer is fully consumed. Request for new Input Buffer
      if(pInputBuffer->nFilledLen == 0) {
        omx_videodec_component_Private->isNewBuffer = 1;
      }
      
      nSize = avpicture_get_size (omx_videodec_component_Private->eOutFramePixFmt,
                                  omx_videodec_component_Private->avCodecContext->width,
                                  omx_videodec_component_Private->avCodecContext->height);

      if(pOutputBuffer->nAllocLen < nSize) {
        DEBUG(DEB_LEV_ERR, "Ouch!!!! Output buffer Alloc Len %d less than Frame Size %d\n",(int)pOutputBuffer->nAllocLen,nSize);
        return;
      }

      avpicture_fill (&pic, (unsigned char*)(outputCurrBuffer + (omx_videodec_component_Private->positionInOutBuf * nSize)),
                      omx_videodec_component_Private->eOutFramePixFmt, 
                      omx_videodec_component_Private->avCodecContext->width, 
                      omx_videodec_component_Private->avCodecContext->height);

      if ( !imgConvertYuvCtx ) {
        imgConvertYuvCtx = sws_getContext( omx_videodec_component_Private->avCodecContext->width, 
                                              omx_videodec_component_Private->avCodecContext->height, 
                                              omx_videodec_component_Private->avCodecContext->pix_fmt,
                                              omx_videodec_component_Private->avCodecContext->width,
                                              omx_videodec_component_Private->avCodecContext->height,
                                              omx_videodec_component_Private->eOutFramePixFmt, SWS_FAST_BILINEAR, NULL, NULL, NULL );
      }

      sws_scale(imgConvertYuvCtx, omx_videodec_component_Private->avFrame->data, 
                omx_videodec_component_Private->avFrame->linesize, 0, 
                omx_videodec_component_Private->avCodecContext->height, pic.data, pic.linesize );

      if (imgConvertYuvCtx ) {
        sws_freeContext(imgConvertYuvCtx);
      }

      DEBUG(DEB_LEV_FULL_SEQ, "nSize=%d,frame linesize=%d,height=%d,pic linesize=%d PixFmt=%d\n",nSize,
        omx_videodec_component_Private->avFrame->linesize[0], 
        omx_videodec_component_Private->avCodecContext->height, 
        pic.linesize[0],omx_videodec_component_Private->eOutFramePixFmt);

      pOutputBuffer->nFilledLen += nSize;

    } else {
      /**  This condition becomes true when the input buffer has completely be consumed.
        * In this case is immediately switched because there is no real buffer consumption 
        */
      pInputBuffer->nFilledLen = 0;
      /** Few bytes may be left in the input buffer but can't generate one output frame. 
        *	Request for new Input Buffer
        */
      omx_videodec_component_Private->isNewBuffer = 1;
      pOutputBuffer->nFilledLen = 0;
    }

    /** We are done with output buffer */
    omx_videodec_component_Private->positionInOutBuf++;

    if(1) {
      //internalOutputFilled = 0;
      omx_videodec_component_Private->positionInOutBuf = 0;
    }
    //if(pOutputBuffer->nFilledLen!=0) {
    nOutputFilled = 1;
    //}
  }
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x nLen=%d is full returning in video decoder\n", 
            (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_videodec_component_SetParameter(
OMX_IN  OMX_HANDLETYPE hComponent,
OMX_IN  OMX_INDEXTYPE nParamIndex,
OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_VIDEO_PARAM_MPEG4TYPE * pVideoMpeg4;
  OMX_VIDEO_PARAM_AVCTYPE * pVideoAvc; 
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videodec_component_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      portIndex = pVideoPortFormat->nPortIndex;
      /*Check Structure Header and verify component state*/
      eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      if(eError!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
        break;
      } 
      if (portIndex <= 1) {
        port = (omx_videodec_component_PortType *) omx_videodec_component_Private->ports[portIndex];
        memcpy(&port->sVideoParam, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        omx_videodec_component_Private->ports[portIndex]->sPortParam.format.video.eColorFormat = port->sVideoParam.eColorFormat;
        
        switch(port->sVideoParam.eColorFormat) {
        case OMX_COLOR_Format24bitRGB888 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB24;
          break; 
        case OMX_COLOR_Format24bitBGR888 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR24;
          break;
        case OMX_COLOR_Format32bitBGRA8888 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR32;
          break;
        case OMX_COLOR_Format32bitARGB8888 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB32;
          break; 
        case OMX_COLOR_Format16bitARGB1555 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB555;
          break;
        case OMX_COLOR_Format16bitRGB565 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB565;
          break; 
        case OMX_COLOR_Format16bitBGR565 :
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR565;
          break;
        default:
          omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;
          break;
        }
      
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;	
    case OMX_IndexParamVideoAvc:
      pVideoAvc = (OMX_VIDEO_PARAM_AVCTYPE*)ComponentParameterStructure;
      portIndex = pVideoAvc->nPortIndex;
      eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
      if(eError!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
        break;
      } 
      memcpy(&omx_videodec_component_Private->pVideoAvc, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
      break;
    case OMX_IndexParamStandardComponentRole:
      pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
      if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE)) {
        omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
      } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE)) {
        omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
      } else {
        return OMX_ErrorBadParameter;
      }
      SetInternalVideoParameters(openmaxStandComp);
      break;
    case OMX_IndexParamVideoMpeg4:
      pVideoMpeg4 = (OMX_VIDEO_PARAM_MPEG4TYPE*) ComponentParameterStructure;
      portIndex = pVideoMpeg4->nPortIndex;
      eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
      if(eError!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
        break;
      } 
      if (pVideoMpeg4->nPortIndex == 0) {
        memcpy(&omx_videodec_component_Private->pVideoMpeg4, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return eError;
}

OMX_ERRORTYPE omx_videodec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;	
  OMX_VIDEO_PARAM_AVCTYPE * pVideoAvc; 
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
  omx_videodec_component_PortType *port;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
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
      memcpy(ComponentParameterStructure, &omx_videodec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
      break;		
    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
        break;
      }
      if (pVideoPortFormat->nPortIndex <= 1) {
        port = (omx_videodec_component_PortType *)omx_videodec_component_Private->ports[pVideoPortFormat->nPortIndex];
        memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;		
    case OMX_IndexParamVideoMpeg4:
      pVideoMpeg4 = (OMX_VIDEO_PARAM_MPEG4TYPE*)ComponentParameterStructure;
      if (pVideoMpeg4->nPortIndex != 0) {
        return OMX_ErrorBadPortIndex;
      }
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE))) != OMX_ErrorNone) { 
        break;
      }
      memcpy(pVideoMpeg4, &omx_videodec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
      break; 
    case OMX_IndexParamVideoAvc:
      pVideoAvc = (OMX_VIDEO_PARAM_AVCTYPE*)ComponentParameterStructure;
      if (pVideoAvc->nPortIndex != 0) {
        return OMX_ErrorBadPortIndex;
      }
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_AVCTYPE))) != OMX_ErrorNone) { 
        break;
      }
      memcpy(pVideoAvc, &omx_videodec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
      break;
    case OMX_IndexParamStandardComponentRole:
      pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
        break;
      }
      if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
        strcpy((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE);
      } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
        strcpy((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE);
      } else {
        strcpy((char *)pComponentRole->cRole,"\0");
      }
      break;
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_videodec_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = (omx_videodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle ) && (omx_videodec_component_Private->state == OMX_StateLoaded)) {
      if (!omx_videodec_component_Private->avcodecReady) {
        eError = omx_videodec_component_ffmpegLibInit(omx_videodec_component_Private);
        if (eError != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_videodec_component_Private->avcodecReady = OMX_TRUE;
      }
      eError = omx_videodec_component_Init(openmaxStandComp);
      if(eError!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Init Failed Error=%x\n",__func__,eError); 
        return eError;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_videodec_component_Private->state == OMX_StateIdle)) {
      eError = omx_videodec_component_Deinit(openmaxStandComp);
      if(eError!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Deinit Failed Error=%x\n",__func__,eError); 
        return eError;
      } 
    }
  }
  // Execute the base message handling
  return omx_base_component_MessageHandler(openmaxStandComp,message);
}

OMX_ERRORTYPE omx_videodec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, VIDEO_DEC_MPEG4_ROLE);
  } else if (nIndex == 1) {
    strcpy((char *)cRole, VIDEO_DEC_H264_ROLE);
  }	else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}
