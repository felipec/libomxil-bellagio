/**
  @file src/components/video_src/omx_videosrc_component.c

  OpenMAX video source component. This component is a video source component
  that captures video from the video camera.

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
#include <omx_base_video_port.h>
#include <omx_videosrc_component.h>

#define MAX_COMPONENT_VIDEOSRC 1

/** Maximum Number of Video Source Instance*/
static OMX_U32 noViderSrcInstance=0;

#define DEFAULT_FILENAME_LENGTH 256

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
  
  /** Allocate Ports and call port constructor. */  
  if (omx_videosrc_component_Private->sPortTypesParam.nPorts && !omx_videosrc_component_Private->ports) {
    omx_videosrc_component_Private->ports = calloc(omx_videosrc_component_Private->sPortTypesParam.nPorts, sizeof(omx_base_PortType *));
    if (!omx_videosrc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_videosrc_component_Private->sPortTypesParam.nPorts; i++) {
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
  
  /** for the moment, the file reader works with video component
  * so the domain is set to video
  * in future it may be assigned after checking the input file format 
  * or there can be seperate file reader of video component
  */  
  /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
  pPort->sPortParam.format.video.nFrameWidth = 176;
  pPort->sPortParam.format.video.nFrameHeight= 144;
  pPort->sPortParam.format.video.eColorFormat= OMX_COLOR_FormatYUV420Planar;
  pPort->sVideoParam.eColorFormat = OMX_COLOR_FormatYUV420Planar;

  pPort->sPortParam.nBufferSize = pPort->sPortParam.format.video.nFrameWidth*
                                  pPort->sPortParam.format.video.nFrameHeight*3; // RGB
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

  omx_videosrc_component_Private->mmaps = NULL;
  omx_videosrc_component_Private->memoryMap = NULL;
  omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_FALSE;

  /* Test if Camera Attached */
  omx_videosrc_component_Private->deviceHandle = open(VIDEO_DEV_NAME, O_RDWR);
  if (omx_videosrc_component_Private->deviceHandle < 0) {
    DEBUG(DEB_LEV_ERR, "In %s Unable to open video capture device %s! errno=%d  ENODEV : %d \n", 
      __func__,VIDEO_DEV_NAME,errno,ENODEV);
    return OMX_ErrorHardware;
  } 

  if ((omx_videosrc_component_Private->capability.type & VID_TYPE_SCALES) != 0)
  {       // supports the ability to scale captured images
    
    omx_videosrc_component_Private->captureWindow.x = 0;
    omx_videosrc_component_Private->captureWindow.y = 0;
    omx_videosrc_component_Private->captureWindow.width = pPort->sPortParam.format.video.nFrameWidth;
    omx_videosrc_component_Private->captureWindow.height = pPort->sPortParam.format.video.nFrameHeight;
    omx_videosrc_component_Private->captureWindow.chromakey = -1;
    omx_videosrc_component_Private->captureWindow.flags = 0;
    omx_videosrc_component_Private->captureWindow.clips = 0;
    omx_videosrc_component_Private->captureWindow.clipcount = 0;
    if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCSWIN, &omx_videosrc_component_Private->captureWindow) == -1)
    {       // could not set window values for capture
      DEBUG(DEB_LEV_ERR,"could not set window values for capture\n");
    }
  }
  if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCGMBUF, &omx_videosrc_component_Private->memoryBuffer) == -1)
  { // failed to retrieve information about capture memory space
    DEBUG(DEB_LEV_ERR,"failed to retrieve information about capture memory space\n");
  }
 
  // obtain memory mapped area
  omx_videosrc_component_Private->memoryMap = mmap (0, omx_videosrc_component_Private->memoryBuffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, omx_videosrc_component_Private->deviceHandle, 0);
  if (omx_videosrc_component_Private->memoryMap == NULL)
  { // failed to retrieve pointer to memory mapped area
    DEBUG(DEB_LEV_ERR,"failed to retrieve pointer to memory mapped area\n");
  }

  return err;
}

/** The Destructor 
 */
OMX_ERRORTYPE omx_videosrc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;
  
  if(omx_videosrc_component_Private->videoSyncSem) {
    tsem_deinit(omx_videosrc_component_Private->videoSyncSem);
    free(omx_videosrc_component_Private->videoSyncSem);
    omx_videosrc_component_Private->videoSyncSem=NULL;
  }

  /* free the video_mmap structures */
  if(omx_videosrc_component_Private->mmaps != NULL) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Freeing mmaps \n",__func__);
    free (omx_videosrc_component_Private->mmaps);
    omx_videosrc_component_Private->mmaps = NULL;
  }

  /* unmap the capture memory */
  if(omx_videosrc_component_Private->memoryMap != NULL) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Freeing memoryMap \n",__func__);
    munmap (omx_videosrc_component_Private->memoryMap, omx_videosrc_component_Private->memoryBuffer.size);
    omx_videosrc_component_Private->memoryMap = NULL;
  }
 
  if(omx_videosrc_component_Private->deviceHandle != -1) {
    if(-1 == close(omx_videosrc_component_Private->deviceHandle)) {
      DEBUG(DEB_LEV_ERR, "In %s Closing video capture device failed \n",__func__);
    }
    omx_videosrc_component_Private->deviceHandle = -1;
  }

  /* frees port/s */
  if (omx_videosrc_component_Private->ports) {
    for (i=0; i < omx_videosrc_component_Private->sPortTypesParam.nPorts; i++) {
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
  OMX_U32 i = 0;
  struct video_channel vchannel;
  struct video_audio audio;

  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  /** Video Capability Query */
  if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCGCAP, &omx_videosrc_component_Private->capability) != -1)
  {       // query was successful
    DEBUG(DEB_LEV_FULL_SEQ, "Video Capability Query Successful\n");
  }
  else
  {       // query failed
    DEBUG(DEB_LEV_ERR, "Video Capability Query Failed\n");
  }

  if ((omx_videosrc_component_Private->capability.type & VID_TYPE_CAPTURE) != 0)
  {       // this device can capture video to memory
    DEBUG(DEB_LEV_FULL_SEQ,"This device can capture video to memory\n");

    DEBUG(DEB_LEV_PARAMS,"Name=%s,Type=%x,channel=%d,audios=%d, \nmaxW=%d,maxH=%d,minW=%d,minH=%d\n",omx_videosrc_component_Private->capability.name,
        omx_videosrc_component_Private->capability.type,
        omx_videosrc_component_Private->capability.channels,   /* Num channels */
        omx_videosrc_component_Private->capability.audios,     /* Num audio devices */
        omx_videosrc_component_Private->capability.maxwidth,   /* Supported width */
        omx_videosrc_component_Private->capability.maxheight,  /* And height */
        omx_videosrc_component_Private->capability.minwidth,   /* Supported width */
        omx_videosrc_component_Private->capability.minheight  /* And height */);
  }
  else
  {       // this device cannot capture video to memory
    DEBUG(DEB_LEV_ERR,"This device cannot capture video to memory\n");
  }

  DEBUG(DEB_LEV_FULL_SEQ,"CaptureWindow Type=%x x=%d,y=%d,w=%d,h=%d, chromakey=%d,flags =%x clipcount=%x\n",
    omx_videosrc_component_Private->capability.type,
      omx_videosrc_component_Private->captureWindow.x,
    omx_videosrc_component_Private->captureWindow.y,
    omx_videosrc_component_Private->captureWindow.width,
    omx_videosrc_component_Private->captureWindow.height ,
    omx_videosrc_component_Private->captureWindow.chromakey ,
    omx_videosrc_component_Private->captureWindow.flags ,
    omx_videosrc_component_Private->captureWindow.clipcount);

  if ((omx_videosrc_component_Private->capability.type & VID_TYPE_SCALES) != 0)
  {       // supports the ability to scale captured images
    
    omx_videosrc_component_Private->captureWindow.x = 0;
    omx_videosrc_component_Private->captureWindow.y = 0;
    omx_videosrc_component_Private->captureWindow.width = pPort->sPortParam.format.video.nFrameWidth;
    omx_videosrc_component_Private->captureWindow.height = pPort->sPortParam.format.video.nFrameHeight;
    omx_videosrc_component_Private->captureWindow.chromakey = -1;
    omx_videosrc_component_Private->captureWindow.flags = 0;
    omx_videosrc_component_Private->captureWindow.clips = 0;
    omx_videosrc_component_Private->captureWindow.clipcount = 0;
    if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCSWIN, &omx_videosrc_component_Private->captureWindow) == -1)
    {       // could not set window values for capture
      DEBUG(DEB_LEV_ERR,"could not set window values for capture\n");
    }
  }

  /*Default output frame size*/
  omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                               pPort->sPortParam.format.video.nFrameHeight*3/2;
  
  // get image properties
  if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCGPICT, &omx_videosrc_component_Private->imageProperties) != -1)
  {       // successfully retrieved the default image properties

    DEBUG(DEB_LEV_PARAMS,"Imp Prop br=%d hue=%d,col=%d,con=%d,white=%d, depth=%d,palette =%d \n",
      omx_videosrc_component_Private->imageProperties.brightness,
      omx_videosrc_component_Private->imageProperties.hue,
      omx_videosrc_component_Private->imageProperties.colour,
      omx_videosrc_component_Private->imageProperties.contrast,
      omx_videosrc_component_Private->imageProperties.whiteness,      /* Black and white only */
      omx_videosrc_component_Private->imageProperties.depth,          /* Capture depth */
      omx_videosrc_component_Private->imageProperties.palette);        /* Palette in use */

    omx_videosrc_component_Private->imageProperties.depth = 12;

    /*Presently VIDEO_PALETTE_YUV420P format is supported by the camera*/
    switch(pPort->sPortParam.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_YUV420P;
      break;
    case OMX_COLOR_Format16bitRGB565:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_RGB565 ;       /* 565 16 bit RGB */
      omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                   pPort->sPortParam.format.video.nFrameHeight*2;
      omx_videosrc_component_Private->imageProperties.depth = 16;
      omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                 pPort->sPortParam.format.video.nFrameHeight*2;
      break;
    case OMX_COLOR_Format24bitRGB888:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_RGB24  ;       /* 24bit RGB */
      omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                   pPort->sPortParam.format.video.nFrameHeight*3;
      omx_videosrc_component_Private->imageProperties.depth = 24;
      break;
    case OMX_COLOR_Format32bitARGB8888:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_RGB32     ;       /* 32bit RGB */
      omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                   pPort->sPortParam.format.video.nFrameHeight*4;
      omx_videosrc_component_Private->imageProperties.depth = 32;
      break;
    case OMX_COLOR_FormatYUV422Planar:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_YUV422P   ;      /* YUV 4:2:2 Planar */
      omx_videosrc_component_Private->iFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                   pPort->sPortParam.format.video.nFrameHeight*2;
      omx_videosrc_component_Private->imageProperties.depth = 16;
      break;
    case OMX_COLOR_FormatYUV411Planar:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_YUV411P   ;      /* YUV 4:1:1 Planar */
      break;
    default:
      omx_videosrc_component_Private->imageProperties.palette = VIDEO_PALETTE_YUV420P;
      break;
    }
    
    DEBUG(DEB_LEV_FULL_SEQ,"Frame Size=%d\n",(int)omx_videosrc_component_Private->iFrameSize);

    if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCSPICT, &omx_videosrc_component_Private->imageProperties) == -1)
    {       // failed to set the image properties
      DEBUG(DEB_LEV_ERR,"failed to set the image properties\n");
    }
  }
  
  vchannel.channel=0;
  if(ioctl(omx_videosrc_component_Private->deviceHandle, VIDIOCGCHAN, &vchannel)==-1) {
    DEBUG(DEB_LEV_ERR,"failed to get video channel\n");
  } else {
    DEBUG(DEB_LEV_FULL_SEQ,"video channel=%d,name=%s,tuners=%d,flags=%d,type=%x norm=%d\n",
       vchannel.channel,
       vchannel.name,
       vchannel.tuners,
       vchannel.flags,
       vchannel.type,
       vchannel.norm);

    vchannel.flags = VIDEO_VC_TUNER;
    vchannel.norm = VIDEO_MODE_AUTO;

    if(ioctl(omx_videosrc_component_Private->deviceHandle, VIDIOCSCHAN, &vchannel)==-1) {
      DEBUG(DEB_LEV_ERR,"failed to set video channel\n");
    }
  } 

  /* mute audio */
  audio.audio = 0;
  if(ioctl(omx_videosrc_component_Private->deviceHandle, VIDIOCGAUDIO, &audio)==-1) {
    DEBUG(DEB_LEV_ERR,"failed to get audio\n");
  } else {
    DEBUG(DEB_LEV_FULL_SEQ,"audio=%d,vol=%d,bass=%d,treble=%d,flags=%x \nname=%s,mode=%d,balance=%d,step=%d\n",
      audio.audio,          /* Audio channel */
      audio.volume,         /* If settable */
      audio.bass, audio.treble,
      audio.flags,
      audio.name,
      audio.mode,
      audio.balance,        /* Stereo balance */
      audio.step);           /* Step actual volume uses */

    audio.flags = VIDEO_AUDIO_MUTE;
    audio.volume=0;
    if(ioctl(omx_videosrc_component_Private->deviceHandle, VIDIOCSAUDIO, &audio)==-1){
      DEBUG(DEB_LEV_ERR,"failed to set audio\n");
    } 
  }

  /**
    The pointer memoryMap and the offsets within memoryBuffer.offsets combine to give us the address of each buffered frame. For example:
    Buffered Frame 0 is located at:  memoryMap + memoryBuffer.offsets[0]
    Buffered Frame 1 is located at:  memoryMap + memoryBuffer.offsets[1]
    Buffered Frame 2 is located at:  memoryMap + memoryBuffer.offsets[2]
    etc...
    The number of buffered frames is stored in memoryBuffer.frames.
   */
 
  omx_videosrc_component_Private->mmaps = (malloc (omx_videosrc_component_Private->memoryBuffer.frames * sizeof (struct video_mmap)));

  i = 0;
  // fill out the fields
  while (i < omx_videosrc_component_Private->memoryBuffer.frames)
  {
    omx_videosrc_component_Private->mmaps[i].frame = i;
    omx_videosrc_component_Private->mmaps[i].width = pPort->sPortParam.format.video.nFrameWidth;
    omx_videosrc_component_Private->mmaps[i].height = pPort->sPortParam.format.video.nFrameHeight;
    omx_videosrc_component_Private->mmaps[i].format = omx_videosrc_component_Private->imageProperties.palette;
    ++ i;
  }
  
  /** initialization for buff mgmt callback function */
  omx_videosrc_component_Private->bIsEOSSent = OMX_FALSE;
  omx_videosrc_component_Private->iFrameIndex = 0 ;
  
  omx_videosrc_component_Private->videoReady = OMX_TRUE;
  /*Indicate that video is ready*/
  tsem_up(omx_videosrc_component_Private->videoSyncSem);

  DEBUG(DEB_LEV_FULL_SEQ,"Memory Buf Size=%d\n",omx_videosrc_component_Private->memoryBuffer.size);

  return OMX_ErrorNone;
}

/** The DeInitialization function 
 */
OMX_ERRORTYPE omx_videosrc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videosrc_component_PrivateType* omx_videosrc_component_Private = openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
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

  //Capturing using MMIO.
  if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCMCAPTURE, &omx_videosrc_component_Private->mmaps[omx_videosrc_component_Private->iFrameIndex]) == -1)
  {       // capture request failed
    DEBUG(DEB_LEV_ERR,"capture request failed\n");
  }
  // wait for the currently indexed frame to complete capture
  if (ioctl (omx_videosrc_component_Private->deviceHandle, VIDIOCSYNC, &omx_videosrc_component_Private->iFrameIndex) == -1)
  {       // sync request failed
    pOutputBuffer->nFilledLen = 0;
    DEBUG(DEB_LEV_ERR,"sync request failed\n");
    return;
  }

  DEBUG(DEB_LEV_FULL_SEQ,"%d-%d\n",(int)omx_videosrc_component_Private->iFrameIndex,(int)omx_videosrc_component_Private->memoryBuffer.frames);
  if(omx_videosrc_component_Private->bOutBufferMemoryMapped == OMX_FALSE) { // In case OMX_UseBuffer copy frame to buffer metadata
    DEBUG(DEB_LEV_ERR,"In %s copy frame to metadata\n",__func__);
    memcpy(pOutputBuffer->pBuffer,omx_videosrc_component_Private->memoryMap + omx_videosrc_component_Private->memoryBuffer.offsets[omx_videosrc_component_Private->iFrameIndex],omx_videosrc_component_Private->iFrameSize);
  }
  pOutputBuffer->nFilledLen = omx_videosrc_component_Private->iFrameSize;

  omx_videosrc_component_Private->iFrameIndex++;
  /*Wrap the frame buffer to the first buffer*/
  if(omx_videosrc_component_Private->iFrameIndex == omx_videosrc_component_Private->memoryBuffer.frames) {
    omx_videosrc_component_Private->iFrameIndex =0;
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
    memcpy(ComponentParameterStructure, &omx_videosrc_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
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
  case OMX_IndexVendorFileReadInputFilename : 
    strcpy((char *)ComponentParameterStructure, "still no filename");
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
      if(i > omx_videosrc_component_Private->memoryBuffer.frames) {
        DEBUG(DEB_LEV_ERR, "In %s returning error i=%d, nframe=%d\n", __func__,i,omx_videosrc_component_Private->memoryBuffer.frames);
        return OMX_ErrorInsufficientResources;
      }
      omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_TRUE;
      openmaxStandPort->pInternalBufferStorage[i]->pBuffer = (OMX_U8*)(omx_videosrc_component_Private->memoryMap + omx_videosrc_component_Private->memoryBuffer.offsets[i]);
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
          //free(openmaxStandPort->pInternalBufferStorage[i]->pBuffer);
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
      if(i > omx_videosrc_component_Private->memoryBuffer.frames) {
        DEBUG(DEB_LEV_ERR, "In %s returning error i=%d, nframe=%d\n", __func__,i,omx_videosrc_component_Private->memoryBuffer.frames);
        return OMX_ErrorInsufficientResources;
      }
      omx_videosrc_component_Private->bOutBufferMemoryMapped = OMX_TRUE;
      pBuffer = (OMX_U8*)(omx_videosrc_component_Private->memoryMap + omx_videosrc_component_Private->memoryBuffer.offsets[i]);

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
