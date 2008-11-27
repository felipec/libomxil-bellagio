/**
  @file src/components/video_src/omx_videosrc_component.c

  OpenMAX video source component. This component is a video source component
  that captures video from the video camera.This camera component is based on V4L2.

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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

#include <assert.h>
#include <omxcore.h>
#include <omx_base_video_port.h>
#include <omx_videosrc_component.h>

#define MAX_COMPONENT_VIDEOSRC 1

/** Maximum Number of Video Source Instance*/
static OMX_U32 noViderSrcInstance=0;

#define CLEAR(x) memset (&(x), 0, sizeof (x))

static unsigned int n_buffers = 0;

static int xioctl(int fd, int request, void *arg);
static int init_device(omx_videosrc_component_PrivateType* omx_videosrc_component_Private);
static int uninit_device(omx_videosrc_component_PrivateType* omx_videosrc_component_Private);
static int start_capturing(omx_videosrc_component_PrivateType* omx_videosrc_component_Private);
static int stop_capturing(omx_videosrc_component_PrivateType* omx_videosrc_component_Private);
static int init_mmap(omx_videosrc_component_PrivateType* omx_videosrc_component_Private);

static int errno_return(const char *s)
{
  DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", s, errno, strerror(errno));
  return OMX_ErrorHardware;
}


/** The Constructor 
 */
OMX_ERRORTYPE omx_videosrc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
 
  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_base_video_PortType *pPort;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_videosrc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videosrc_component_Private->ports = NULL;
  omx_videosrc_component_Private->deviceHandle = -1;
  
  err = omx_base_source_Constructor(openmaxStandComp, cComponentName);
  
  omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 1;

    /** Allocate Ports and call port constructor. */  
  if (omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_videosrc_component_Private->ports) {
    omx_videosrc_component_Private->ports = calloc(omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
    if (!omx_videosrc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      omx_videosrc_component_Private->ports[i] = calloc(1, sizeof(omx_base_video_PortType));
      if (!omx_videosrc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_video_port_Constructor(openmaxStandComp, &omx_videosrc_component_Private->ports[0], 0, OMX_FALSE);
  omx_videosrc_component_Private->ports[0]->Port_AllocateBuffer = videosrc_port_AllocateBuffer;
  omx_videosrc_component_Private->ports[0]->Port_FreeBuffer = videosrc_port_FreeBuffer;
  omx_videosrc_component_Private->ports[0]->Port_AllocateTunnelBuffer = videosrc_port_AllocateTunnelBuffer;
  omx_videosrc_component_Private->ports[0]->Port_FreeTunnelBuffer = videosrc_port_FreeTunnelBuffer;

  pPort = (omx_base_video_PortType *) omx_videosrc_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  
  pPort->sPortParam.format.video.nFrameWidth = 320;
  pPort->sPortParam.format.video.nFrameHeight= 240;
  pPort->sPortParam.format.video.eColorFormat= OMX_COLOR_FormatYUV420Planar;
  pPort->sVideoParam.eColorFormat = OMX_COLOR_FormatYUV420Planar;

  pPort->sPortParam.nBufferSize = pPort->sPortParam.format.video.nFrameWidth*
                                  pPort->sPortParam.format.video.nFrameHeight*3; // RGB888
  omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.nBufferSize;

  omx_videosrc_component_Private->BufferMgmtCallback = omx_videosrc_component_BufferMgmtCallback;
  omx_videosrc_component_Private->destructor = omx_videosrc_component_Destructor;
  omx_videosrc_component_Private->messageHandler = omx_videosrc_component_MessageHandler;

  noViderSrcInstance++;
  if(noViderSrcInstance > MAX_COMPONENT_VIDEOSRC) {
    return OMX_ErrorInsufficientResources;
  }

  openmaxStandComp->SetParameter  = omx_videosrc_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_videosrc_component_GetParameter;

  /* Write in the default paramenters */
  omx_videosrc_component_Private->videoReady = OMX_FALSE;
  if(!omx_videosrc_component_Private->videoSyncSem) {
    omx_videosrc_component_Private->videoSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_videosrc_component_Private->videoSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_videosrc_component_Private->videoSyncSem, 0);
  }

  omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_FALSE;

  /* Test if Camera Attached */
  omx_videosrc_component_Private->deviceHandle = open(VIDEO_DEV_NAME, O_RDWR /* required */  | O_NONBLOCK, 0);
  if (omx_videosrc_component_Private->deviceHandle < 0) {
    DEBUG(DEB_LEV_ERR, "In %s Unable to open video capture device %s! errno=%d  ENODEV : %d \n", 
      __func__,VIDEO_DEV_NAME,errno,ENODEV);
    return OMX_ErrorHardware;
  } 

  omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_YUV420;

  err = init_device(omx_videosrc_component_Private);

  err = init_mmap(omx_videosrc_component_Private);

  return err;
}

/** The Destructor 
 */
OMX_ERRORTYPE omx_videosrc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;  
  OMX_U32 i;
  
  if(omx_videosrc_component_Private->videoSyncSem) {
    tsem_deinit(omx_videosrc_component_Private->videoSyncSem);
    free(omx_videosrc_component_Private->videoSyncSem);
    omx_videosrc_component_Private->videoSyncSem=NULL;
  }

  err = uninit_device(omx_videosrc_component_Private);
 
  if(omx_videosrc_component_Private->deviceHandle != -1) {
    if(-1 == close(omx_videosrc_component_Private->deviceHandle)) {
      DEBUG(DEB_LEV_ERR, "In %s Closing video capture device failed \n",__func__);
    }
    omx_videosrc_component_Private->deviceHandle = -1;
  }

  /* frees port/s */
  if (omx_videosrc_component_Private->ports) {
    for (i=0; i < omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      if(omx_videosrc_component_Private->ports[i])
        omx_videosrc_component_Private->ports[i]->PortDestructor(omx_videosrc_component_Private->ports[i]);
    }
    free(omx_videosrc_component_Private->ports);
    omx_videosrc_component_Private->ports=NULL;
  }

  noViderSrcInstance--;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  return omx_base_source_Destructor(openmaxStandComp);
}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_videosrc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPort = (omx_base_video_PortType *)omx_videosrc_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  OMX_ERRORTYPE err = OMX_ErrorNone;  

  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  /* Presently V4L2_PIX_FMT_YUV420 format is supported by the camera */
  switch(pPort->sPortParam.format.video.eColorFormat) {
  case OMX_COLOR_FormatYUV420Planar:
  case OMX_COLOR_FormatYUV420PackedPlanar:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_YUV420;
    break;
  case OMX_COLOR_Format16bitRGB565:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_RGB565 ;       // 565 16 bit RGB //
    omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                 pPort->sPortParam.format.video.nFrameHeight*2;
    omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                               pPort->sPortParam.format.video.nFrameHeight*2;
    break;
  case OMX_COLOR_Format24bitRGB888:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_RGB24  ;       // 24bit RGB //
    omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                 pPort->sPortParam.format.video.nFrameHeight*3;
    break;
  case OMX_COLOR_Format32bitARGB8888:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_RGB32     ;       // 32bit RGB //
    omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                 pPort->sPortParam.format.video.nFrameHeight*4;
    break;
  case OMX_COLOR_FormatYUV422Planar:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_YUV422P   ;      // YUV 4:2:2 Planar //
    omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                 pPort->sPortParam.format.video.nFrameHeight*2;
    break;
  case OMX_COLOR_FormatYUV411Planar:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_YUV411P   ;      // YUV 4:1:1 Planar //
    break;
  default:
    omx_videosrc_component_Private->pixel_format = V4L2_PIX_FMT_YUV420;
    break;
  }

  /** Initialize video capture pixel format */
  omx_videosrc_component_Private->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  omx_videosrc_component_Private->fmt.fmt.pix.width = pPort->sPortParam.format.video.nFrameWidth;
  omx_videosrc_component_Private->fmt.fmt.pix.height = pPort->sPortParam.format.video.nFrameHeight;
  omx_videosrc_component_Private->fmt.fmt.pix.pixelformat = omx_videosrc_component_Private->pixel_format;
  omx_videosrc_component_Private->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_S_FMT, &omx_videosrc_component_Private->fmt))
    return errno_return("VIDIOC_S_FMT");

  // Note VIDIOC_S_FMT may change width and height. //
  pPort->sPortParam.format.video.nFrameWidth = omx_videosrc_component_Private->fmt.fmt.pix.width;
  pPort->sPortParam.format.video.nFrameHeight = omx_videosrc_component_Private->fmt.fmt.pix.height;

  /*output frame size*/
  omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                               pPort->sPortParam.format.video.nFrameHeight*3/2;

  DEBUG(DEB_ALL_MESS,"Frame Width=%d, Height=%d, Frame Size=%d n_buffers=%d\n",
    (int)pPort->sPortParam.format.video.nFrameWidth,
    (int)pPort->sPortParam.format.video.nFrameHeight,
    (int)omx_videosrc_component_Private->iFrameSize,n_buffers);

  /** initialization for buff mgmt callback function */
  omx_videosrc_component_Private->bIsEOSSent = OMX_FALSE;
  
  err = start_capturing(omx_videosrc_component_Private);

  omx_videosrc_component_Private->videoReady = OMX_TRUE;

  /*Indicate that video is ready*/
  tsem_up(omx_videosrc_component_Private->videoSyncSem);

  

  return err;
}

/** The DeInitialization function 
 */
OMX_ERRORTYPE omx_videosrc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  stop_capturing(omx_videosrc_component_Private);

  /** closing input file */
  omx_videosrc_component_Private->videoReady = OMX_FALSE;
  tsem_reset(omx_videosrc_component_Private->videoSyncSem);

  return OMX_ErrorNone;
}

/** 
 * This function processes the input file and returns packet by packet as an output data
 * this packet is used in video decoder component for decoding
 */
void omx_videosrc_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  struct v4l2_buffer buf;
  
  CLEAR(buf);
  
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (omx_videosrc_component_Private->videoReady == OMX_FALSE) {
    if(omx_videosrc_component_Private->state == OMX_StateExecuting) {
      /*wait for video to be ready*/
      tsem_down(omx_videosrc_component_Private->videoSyncSem);
    } else {
      return;
    }
  }

  pOutputBuffer->nOffset = 0;
  pOutputBuffer->nFilledLen = 0;

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_DQBUF, &buf)) {
    switch (errno) {
    case EAGAIN:
	    return;
	  case EIO:
	    /* Could ignore EIO, see spec. */
	    /* fall through */

	  default:
      DEBUG(DEB_LEV_ERR,"In %s error VIDIOC_DQBUF\n",__func__);
	    return;
    }
  }

  assert(buf.index < n_buffers);

  if(omx_videosrc_component_Private->bOutBufferMemoryMapped == OMX_FALSE) { /* In case OMX_UseBuffer copy frame to buffer metadata */
    memcpy(pOutputBuffer->pBuffer,omx_videosrc_component_Private->buffers[buf.index].start,omx_videosrc_component_Private->iFrameSize);
  }

  pOutputBuffer->nFilledLen = omx_videosrc_component_Private->iFrameSize;

  DEBUG(DEB_LEV_FULL_SEQ,"Camera output buffer nFilledLen=%d buf.length=%d\n",(int)pOutputBuffer->nFilledLen,buf.length);

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_QBUF, &buf)) {
    DEBUG(DEB_LEV_ERR,"In %s error VIDIOC_DQBUF\n",__func__);
  }

  /** return the current output buffer */
  return;
}

OMX_ERRORTYPE omx_videosrc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType* pPort = (omx_base_video_PortType *) omx_videosrc_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];

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
      memcpy(&pPort->sVideoParam,pVideoPortFormat,sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamPortDefinition: 
    err = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    if(err == OMX_ErrorNone) {
      if(pPort->sPortParam.format.video.nFrameWidth > 640 || pPort->sPortParam.format.video.nFrameWidth <160 || 
        pPort->sPortParam.format.video.nFrameHeight > 480 || pPort->sPortParam.format.video.nFrameHeight < 120) {
        pPort->sPortParam.format.video.nFrameWidth = 160;
        pPort->sPortParam.format.video.nFrameHeight = 120;
        DEBUG(DEB_LEV_ERR, "In %s Frame Width Range[160..640] Frame Height Range[120..480]\n",__func__); 
        return OMX_ErrorBadParameter;
      } else {
        pPort->sPortParam.nBufferSize = pPort->sPortParam.format.video.nFrameWidth*
                                  pPort->sPortParam.format.video.nFrameHeight*3/2; // YUV
      }
    }
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_videosrc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;  
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *pPort = (omx_base_video_PortType *) omx_videosrc_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];  
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Getting parameter %08x\n",__func__, nParamIndex);
  
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamVideoInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_videosrc_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
    break;    
  case OMX_IndexParamVideoPortFormat:
    pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pVideoPortFormat->nPortIndex < 1) {
      memcpy(pVideoPortFormat, &pPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;  
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

/** This function initializes and deinitializes the library related initialization
  * needed for file parsing
  */
OMX_ERRORTYPE omx_videosrc_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = (omx_videosrc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_STATETYPE oldState = omx_videosrc_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  /* Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet && err == OMX_ErrorNone){ 
    if ((message->messageParam == OMX_StateExecuting) && (oldState == OMX_StateIdle)) {    
      err = omx_videosrc_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Source Init Failed Error=%x\n",__func__,err); 
      }
    } else if ((message->messageParam == OMX_StateIdle) && (oldState == OMX_StateExecuting)) {
      err = omx_videosrc_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Source Deinit Failed Error=%x\n",__func__,err); 
      }
    }
  }
  return err;
}

OMX_ERRORTYPE videosrc_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes) {
  
  int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = (omx_videosrc_component_PrivateType*)omx_base_component_Private;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    return OMX_ErrorBadPortIndex;
  }
  if (PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateLoadedToIdle) {
    if (!openmaxStandPort->bIsTransientToEnabled) {
      DEBUG(DEB_LEV_ERR, "In %s: The port is not allowed to receive buffers\n", __func__);
      return OMX_ErrorIncorrectStateTransition;
    }
  }

  if(nSizeBytes < openmaxStandPort->sPortParam.nBufferSize) {
    DEBUG(DEB_LEV_ERR, "In %s: Requested Buffer Size %lu is less than Minimum Buffer Size %lu\n", __func__, nSizeBytes, openmaxStandPort->sPortParam.nBufferSize);
    return OMX_ErrorIncorrectStateTransition;
  }
  
  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_FREE) {
      openmaxStandPort->pInternalBufferStorage[i] = calloc(1,sizeof(OMX_BUFFERHEADERTYPE));
      if (!openmaxStandPort->pInternalBufferStorage[i]) {
        return OMX_ErrorInsufficientResources;
      }
      setHeader(openmaxStandPort->pInternalBufferStorage[i], sizeof(OMX_BUFFERHEADERTYPE));
      /* Map the buffer with the device's memory area*/
      if(i > n_buffers) {
        DEBUG(DEB_LEV_ERR, "In %s returning error i=%d, nframe=%d\n", __func__,i,n_buffers);
        return OMX_ErrorInsufficientResources;
      }
      
      omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_TRUE;
      openmaxStandPort->pInternalBufferStorage[i]->pBuffer = omx_videosrc_component_Private->buffers[i].start;
      openmaxStandPort->pInternalBufferStorage[i]->nAllocLen = (int)nSizeBytes;
      openmaxStandPort->pInternalBufferStorage[i]->pPlatformPrivate = openmaxStandPort;
      openmaxStandPort->pInternalBufferStorage[i]->pAppPrivate = pAppPrivate;
      *pBuffer = openmaxStandPort->pInternalBufferStorage[i];
      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_ALLOCATED;
      openmaxStandPort->bBufferStateAllocated[i] |= HEADER_ALLOCATED;
      if (openmaxStandPort->sPortParam.eDir == OMX_DirInput) {
        openmaxStandPort->pInternalBufferStorage[i]->nInputPortIndex = openmaxStandPort->sPortParam.nPortIndex;
      } else {
        openmaxStandPort->pInternalBufferStorage[i]->nOutputPortIndex = openmaxStandPort->sPortParam.nPortIndex;
      }
      openmaxStandPort->nNumAssignedBuffers++;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->sPortParam.nBufferCountActual == openmaxStandPort->nNumAssignedBuffers) {
        openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
        openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nPortIndex=%d\n",__func__,(int)nPortIndex);
        tsem_up(openmaxStandPort->pAllocSem);
      }
      return OMX_ErrorNone;
    }
  }
  DEBUG(DEB_LEV_ERR, "In %s Error: no available buffers\n",__func__);
  return OMX_ErrorInsufficientResources;
}
OMX_ERRORTYPE videosrc_port_FreeBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_U32 nPortIndex,
  OMX_BUFFERHEADERTYPE* pBuffer) {

  int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = (omx_videosrc_component_PrivateType*)omx_base_component_Private;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    return OMX_ErrorBadPortIndex;
  }
  if (PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateIdleToLoaded) {
    if (!openmaxStandPort->bIsTransientToDisabled) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to free the buffers\n", __func__);
      (*(omx_base_component_Private->callbacks->EventHandler))
        (omxComponent,
        omx_base_component_Private->callbackData,
        OMX_EventError, /* The command was completed */
        OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
        nPortIndex, /* The state has been changed in message->messageParam2 */
        NULL);
    }
  }
  
  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (openmaxStandPort->bBufferStateAllocated[i] & (BUFFER_ASSIGNED | BUFFER_ALLOCATED)) {

      openmaxStandPort->bIsFullOfBuffers = OMX_FALSE;
      if (openmaxStandPort->bBufferStateAllocated[i] & BUFFER_ALLOCATED) {
        if(openmaxStandPort->pInternalBufferStorage[i]->pBuffer){
          DEBUG(DEB_LEV_PARAMS, "In %s freeing %i pBuffer=%x\n",__func__, (int)i, (int)openmaxStandPort->pInternalBufferStorage[i]->pBuffer);
          openmaxStandPort->pInternalBufferStorage[i]->pBuffer=NULL;
          omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_FALSE;
        }
      } else if (openmaxStandPort->bBufferStateAllocated[i] & BUFFER_ASSIGNED) {
        free(pBuffer);
        pBuffer=NULL;
      }
      if(openmaxStandPort->bBufferStateAllocated[i] & HEADER_ALLOCATED) {
        free(openmaxStandPort->pInternalBufferStorage[i]);
        openmaxStandPort->pInternalBufferStorage[i]=NULL;
      }

      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_FREE;

      openmaxStandPort->nNumAssignedBuffers--;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->nNumAssignedBuffers == 0) {
        openmaxStandPort->sPortParam.bPopulated = OMX_FALSE;
        openmaxStandPort->bIsEmptyOfBuffers = OMX_TRUE;
        tsem_up(openmaxStandPort->pAllocSem);
      }
      return OMX_ErrorNone;
    }
  }
  return OMX_ErrorInsufficientResources;
}

OMX_ERRORTYPE videosrc_port_AllocateTunnelBuffer(omx_base_PortType *openmaxStandPort,OMX_IN OMX_U32 nPortIndex,OMX_IN OMX_U32 nSizeBytes)
{
  int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = (omx_videosrc_component_PrivateType*)omx_base_component_Private;
  OMX_U8* pBuffer=NULL;
  OMX_ERRORTYPE eError=OMX_ErrorNone;
  OMX_U32 numRetry=0;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: Bad Port Index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  if (! PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    DEBUG(DEB_LEV_ERR, "In %s: Port is not tunneled Flag=%x\n", __func__, (int)openmaxStandPort->nTunnelFlags);
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateLoadedToIdle) {
    if (!openmaxStandPort->bIsTransientToEnabled) {
      DEBUG(DEB_LEV_ERR, "In %s: The port is not allowed to receive buffers\n", __func__);
      return OMX_ErrorIncorrectStateTransition;
    }
  }
  
  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_FREE) {
      /* Map the buffer with the device's memory area*/
      if(i > n_buffers) {
        DEBUG(DEB_LEV_ERR, "In %s returning error i=%d, nframe=%d\n", __func__,i,n_buffers);
        return OMX_ErrorInsufficientResources;
      }
      omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_TRUE;
      pBuffer = omx_videosrc_component_Private->buffers[i].start;

      /*Retry more than once, if the tunneled component is not in Loaded->Idle State*/
      while(numRetry <TUNNEL_USE_BUFFER_RETRY) {
        eError=OMX_UseBuffer(openmaxStandPort->hTunneledComponent,&openmaxStandPort->pInternalBufferStorage[i],
                             openmaxStandPort->nTunneledPort,NULL,nSizeBytes,pBuffer); 
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_FULL_SEQ,"Tunneled Component Couldn't Use buffer %i From Comp=%s Retry=%d\n",
          i,omx_base_component_Private->name,(int)numRetry);

          if((eError ==  OMX_ErrorIncorrectStateTransition) && numRetry<TUNNEL_USE_BUFFER_RETRY) {
            DEBUG(DEB_LEV_FULL_SEQ,"Waiting for next try %i \n",(int)numRetry);
            usleep(TUNNEL_USE_BUFFER_RETRY_USLEEP_TIME);
            numRetry++;
            continue;
          }
          return eError;
        }
        else {
          break;
        }
      }
      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_ALLOCATED;
      openmaxStandPort->nNumAssignedBuffers++;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->sPortParam.nBufferCountActual == openmaxStandPort->nNumAssignedBuffers) {
        openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
        openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nPortIndex=%d\n",__func__, (int)nPortIndex);
      }
      queue(openmaxStandPort->pBufferQueue, openmaxStandPort->pInternalBufferStorage[i]);
    }
  }
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s Allocated all buffers\n",__func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE videosrc_port_FreeTunnelBuffer(omx_base_PortType *openmaxStandPort,OMX_U32 nPortIndex)
{
  int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = (omx_videosrc_component_PrivateType*)omx_base_component_Private;
  OMX_ERRORTYPE eError=OMX_ErrorNone;
  OMX_U32 numRetry=0;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: Bad Port Index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  if (! PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    DEBUG(DEB_LEV_ERR, "In %s: Port is not tunneled\n", __func__);
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateIdleToLoaded) {
    if (!openmaxStandPort->bIsTransientToDisabled) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to free the buffers\n", __func__);
      (*(omx_base_component_Private->callbacks->EventHandler))
        (omxComponent,
        omx_base_component_Private->callbackData,
        OMX_EventError, /* The command was completed */
        OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
        nPortIndex, /* The state has been changed in message->messageParam2 */
        NULL);
    }
  }

  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (openmaxStandPort->bBufferStateAllocated[i] & (BUFFER_ASSIGNED | BUFFER_ALLOCATED)) {

      openmaxStandPort->bIsFullOfBuffers = OMX_FALSE;
      if (openmaxStandPort->bBufferStateAllocated[i] & BUFFER_ALLOCATED) {
        openmaxStandPort->pInternalBufferStorage[i]->pBuffer = NULL;
        omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_FALSE;
      }
      /*Retry more than once, if the tunneled component is not in Idle->Loaded State*/
      while(numRetry <TUNNEL_USE_BUFFER_RETRY) {
        eError=OMX_FreeBuffer(openmaxStandPort->hTunneledComponent,openmaxStandPort->nTunneledPort,openmaxStandPort->pInternalBufferStorage[i]);
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't free buffer %i \n",i);
          if((eError ==  OMX_ErrorIncorrectStateTransition) && numRetry<TUNNEL_USE_BUFFER_RETRY) {
            DEBUG(DEB_LEV_ERR,"Waiting for next try %i \n",(int)numRetry);
            usleep(TUNNEL_USE_BUFFER_RETRY_USLEEP_TIME);
            numRetry++;
            continue;
          }
          return eError;
        } else {
          break;
        }
      }
      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_FREE;

      openmaxStandPort->nNumAssignedBuffers--;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->nNumAssignedBuffers == 0) {
        openmaxStandPort->sPortParam.bPopulated = OMX_FALSE;
        openmaxStandPort->bIsEmptyOfBuffers = OMX_TRUE;
        //tsem_up(openmaxStandPort->pAllocSem);
      }
    }
  }
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s Qelem=%d BSem=%d\n", __func__,openmaxStandPort->pBufferQueue->nelem,openmaxStandPort->pBufferSem->semval);
  return OMX_ErrorNone;
}

static int xioctl(int fd, int request, void *arg)
{
  int r;

  do
    r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}

static int init_device(omx_videosrc_component_PrivateType* omx_videosrc_component_Private)
{

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_QUERYCAP, &omx_videosrc_component_Private->cap)) {
    if (EINVAL == errno) {
	    DEBUG(DEB_LEV_ERR, "%s is no V4L2 device\n", VIDEO_DEV_NAME);
	    return OMX_ErrorHardware;
    } else {
	    return errno_return("VIDIOC_QUERYCAP");
	  }
  }

  if (!(omx_videosrc_component_Private->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    DEBUG(DEB_LEV_ERR, "%s is no video capture device\n", VIDEO_DEV_NAME);
    return OMX_ErrorHardware;
  }
  
  if (!(omx_videosrc_component_Private->cap.capabilities & V4L2_CAP_STREAMING)) {
	  DEBUG(DEB_LEV_ERR, "%s does not support streaming i/o\n", VIDEO_DEV_NAME);
	  return OMX_ErrorHardware;
	}

  /* Select video input, video standard and tune here. */
  omx_videosrc_component_Private->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_CROPCAP, &omx_videosrc_component_Private->cropcap)) {
    /* Errors ignored. */
  }

  omx_videosrc_component_Private->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  omx_videosrc_component_Private->crop.c = omx_videosrc_component_Private->cropcap.defrect;	/* reset to default */

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_S_CROP, &omx_videosrc_component_Private->crop)) {
    switch (errno) {
    case EINVAL:
	    /* Cropping not supported. */
	    break;
	  default:
	    /* Errors ignored. */
	    break;
	  }
  }

  CLEAR(omx_videosrc_component_Private->fmt);
  
  return OMX_ErrorNone;
}

static int start_capturing(omx_videosrc_component_PrivateType* omx_videosrc_component_Private)
{
  unsigned int i;
  enum v4l2_buf_type type;

  for (i = 0; i < n_buffers; ++i)
	{
	  struct v4l2_buffer buf;

	  CLEAR(buf);

	  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	  buf.memory = V4L2_MEMORY_MMAP;
	  buf.index = i;

	  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_QBUF, &buf))
	    return errno_return("VIDIOC_QBUF");
	}

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_STREAMON, &type))
	  return errno_return("VIDIOC_STREAMON");
   
  return OMX_ErrorNone;
}

static int stop_capturing(omx_videosrc_component_PrivateType* omx_videosrc_component_Private)
{
  enum v4l2_buf_type type;

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_STREAMOFF, &type))
    return errno_return("VIDIOC_STREAMOFF");
    
  return OMX_ErrorNone;
}

static int uninit_device(omx_videosrc_component_PrivateType* omx_videosrc_component_Private)
{
  unsigned int i;
  
  for (i = 0; i < n_buffers; ++i) {
    if (-1 == munmap(omx_videosrc_component_Private->buffers[i].start, omx_videosrc_component_Private->buffers[i].length))
      return errno_return("munmap");
  }

  free(omx_videosrc_component_Private->buffers);

  return OMX_ErrorNone;
}

static int init_mmap(omx_videosrc_component_PrivateType* omx_videosrc_component_Private)
{
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      DEBUG(DEB_LEV_ERR, "%s does not support "
        "memory mapping\n", VIDEO_DEV_NAME);
      return OMX_ErrorHardware;
    } else {
      return errno_return("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2) {
    DEBUG(DEB_LEV_ERR, "Insufficient buffer memory on %s\n", VIDEO_DEV_NAME);
    return OMX_ErrorHardware;
  }

  omx_videosrc_component_Private->buffers = calloc(req.count, sizeof(*omx_videosrc_component_Private->buffers));

  if (!omx_videosrc_component_Private->buffers) {
    DEBUG(DEB_LEV_ERR,"Out of memory\n");
    return OMX_ErrorHardware;
  }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == xioctl(omx_videosrc_component_Private->deviceHandle, VIDIOC_QUERYBUF, &buf))
	    return errno_return("VIDIOC_QUERYBUF");

    omx_videosrc_component_Private->buffers[n_buffers].length = buf.length;
    omx_videosrc_component_Private->buffers[n_buffers].start = mmap(NULL /* start anywhere */ ,
				    buf.length,
				    PROT_READ | PROT_WRITE /* required */ ,
				    MAP_SHARED /* recommended */ ,
				    omx_videosrc_component_Private->deviceHandle, buf.m.offset);

    if (MAP_FAILED == omx_videosrc_component_Private->buffers[n_buffers].start)
      return errno_return("mmap");
  }

  return OMX_ErrorNone;
}
