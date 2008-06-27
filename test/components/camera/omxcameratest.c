/**
  @file test/components/camera/omxcameratest.c

  Test application that uses two OpenMAX components, a camera and a fbsink.
  The preview port of the camera is tunneled with the fbsink component;
  The output video/image data of the capture port and thumbnail port of the
  camera are saved in disk files, respectively.

  Copyright (C) 2007-2008  Motorola and STMicroelectronics

  This code is licensed under LGPL see README for full LGPL notice.

  Date                           Author                Comment
  Fri, 06 Jul 2007               Motorola              File created
  Fri, 15 Feb 2008               Motorola              Update: The current implementation for this
                                                       test app can only support one color format and
                                                       image size on each port. To convert color formats
                                                       and image sizes to other choices on some port,
                                                       that port must be tunneled with a color conversion
                                                       component.
  Tue, 06 Apr 2008               STM                   Update: Adding support for the color converter

  This Program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  $Date$
  Revision $Rev$
  Author $Author$

*/



#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>

#include "omxcameratest.h"



/* Callbacks for camera component */
OMX_CALLBACKTYPE camera_source_callbacks = {
  .EventHandler = camera_sourceEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone = camera_sourceFillBufferDone
};

/* Callbacks for color converter component */
OMX_CALLBACKTYPE colorconv_callbacks = {
  .EventHandler = colorconvEventHandler,
  .EmptyBufferDone = colorconvEmptyBufferDone,
  .FillBufferDone = colorconvFillBufferDone
};

/* Callbacks for fbsink component */
OMX_CALLBACKTYPE fbsink_callbacks = {
  .EventHandler = fbsinkEventHandler,
  .EmptyBufferDone = fbsinkEmptyBufferDone,
  .FillBufferDone = NULL
};

appPrivateType* appPriv = NULL;

OMX_PORTBUFFERCTXT sCameraPortBufferList[NUM_CAMERAPORTS];

FILE* fCapture = NULL;
char g_DefaultCaptureFileName[] = "capture.yuv";
FILE* fThumbnail = NULL;
char g_DefaultThumbnailFileName[] = "thumbnail.yuv";


/** callbacks implementation of camera component */
OMX_ERRORTYPE camera_sourceEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: event type code (eEvent)=%d\n", __func__, eEvent);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Set state to ");
      switch ((int)Data2) {
        case OMX_StateInvalid:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
          break;
        case OMX_StateLoaded:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
          break;
        case OMX_StateIdle:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
          break;
        case OMX_StateExecuting:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
          break;
        case OMX_StatePause:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
          break;
        case OMX_StateWaitForResources:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
          break;
      }    
      tsem_up(appPriv->cameraSourceEventSem);
    }
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE camera_sourceFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  DEBUG(DEB_LEV_FULL_SEQ, "%s: Get returned buffer (0x%lX) from port[%ld], nFilledLen=%ld\n", __func__, (OMX_U32)pBuffer, pBuffer->nOutputPortIndex, pBuffer->nFilledLen);

  if (pBuffer->nOutputPortIndex == OMX_CAMPORT_INDEX_CP) {
    if ((pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME) != 0) {
      DEBUG(DEB_LEV_FULL_SEQ, "%s: Get buffer flag OMX_BUFFERFLAG_STARTTIME!\n", __func__);
      pBuffer->nFlags = 0;
    }
    /* Print time stamp of each buffer */
#ifndef OMX_SKIP64BIT
    DEBUG(DEB_LEV_FULL_SEQ, "%s: buffer[0x%lX] time stamp: 0x%016llX\n", __func__, (OMX_U32)pBuffer, pBuffer->nTimeStamp);
#else
    DEBUG(DEB_LEV_FULL_SEQ, "%s: buffer[0x%lX] time stamp: 0x%08lX%08lX\n", __func__, (OMX_U32)pBuffer, pBuffer->nTimeStamp.nHighPart, pBuffer->nTimeStamp.nLowPart);
#endif
  }

  if (pBuffer->nOutputPortIndex == OMX_CAMPORT_INDEX_CP &&  fCapture != NULL) {
    DEBUG(DEB_LEV_FULL_SEQ, "%s: writing to file",__func__);
    fwrite(pBuffer->pBuffer + pBuffer->nOffset, 1, pBuffer->nFilledLen, fCapture);
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    OMX_FillThisBuffer(appPriv->camerahandle, pBuffer);
  }
  else if (pBuffer->nOutputPortIndex == OMX_CAMPORT_INDEX_CP_T &&  fThumbnail != NULL) {
    fwrite(pBuffer->pBuffer + pBuffer->nOffset, 1, pBuffer->nFilledLen, fThumbnail);
    pBuffer->nFilledLen = 0;
    pBuffer->nOffset = 0;
    OMX_FillThisBuffer(appPriv->camerahandle, pBuffer);
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}

/** callbacks implementation of color converter component */
OMX_ERRORTYPE colorconvEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: event type code (eEvent)=%d\n", __func__, eEvent);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Set state to ");
      switch ((int)Data2) {
        case OMX_StateInvalid:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
          break;
        case OMX_StateLoaded:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
          break;
        case OMX_StateIdle:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
          break;
        case OMX_StateExecuting:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
          break;
        case OMX_StatePause:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
          break;
        case OMX_StateWaitForResources:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
          break;
      }    
      tsem_up(appPriv->colorconvEventSem);
    }
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE colorconvEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Get returned buffer (0x%lX) from colorconv input port, nFilledLen=%ld\n", __func__, (OMX_U32)pBuffer, pBuffer->nFilledLen);


  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE colorconvFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Get returned buffer (0x%lX) from colorconv output port, nFilledLen=%ld\n", __func__, (OMX_U32)pBuffer, pBuffer->nFilledLen);


  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}

/** callbacks implementation of fbsink component */
OMX_ERRORTYPE fbsinkEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: event type code (eEvent)=%d\n", __func__, eEvent);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Set state to ");
      switch ((int)Data2) {
        case OMX_StateInvalid:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
          break;
        case OMX_StateLoaded:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
          break;
        case OMX_StateIdle:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
          break;
        case OMX_StateExecuting:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
          break;
        case OMX_StatePause:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
          break;
        case OMX_StateWaitForResources:
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
          break;
      }    
      tsem_up(appPriv->fbsinkEventSem);
    } else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->fbsinkEventSem);
    } 
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE fbsinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Get returned buffer (0x%lX) from fbsink input port, nFilledLen=%ld\n", __func__, (OMX_U32)pBuffer, pBuffer->nFilledLen);


  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;

  return OMX_ErrorNone;
}


OMX_ERRORTYPE setCameraParameters(OMX_BOOL bCameraStillImageMode) {
  OMX_ERRORTYPE errRet = OMX_ErrorNone;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PARAM_SENSORMODETYPE sOmxSensorMode;
  OMX_PARAM_PORTDEFINITIONTYPE sOmxPortDefinition;

  /* set sensor mode */
  setHeader(&sOmxSensorMode, sizeof(OMX_PARAM_SENSORMODETYPE));
  sOmxSensorMode.nPortIndex = 0;
  setHeader(&sOmxSensorMode.sFrameSize, sizeof(OMX_FRAMESIZETYPE));
  sOmxSensorMode.sFrameSize.nPortIndex = 0;
  if ((err = OMX_GetParameter(appPriv->camerahandle, OMX_IndexParamCommonSensorMode, &sOmxSensorMode)) != OMX_ErrorNone) {
    errRet = err;
  }
  else {
    sOmxSensorMode.nFrameRate = DEFAULT_FRAME_RATE;
    sOmxSensorMode.bOneShot = bCameraStillImageMode;
    sOmxSensorMode.sFrameSize.nWidth = DEFAULT_FRAME_WIDTH;
    sOmxSensorMode.sFrameSize.nHeight = DEFAULT_FRAME_HEIGHT;
    if ((err = OMX_SetParameter(appPriv->camerahandle, OMX_IndexParamCommonSensorMode, &sOmxSensorMode)) != OMX_ErrorNone) {
      errRet = err;
    }
  }

  /* set preview port */
  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = OMX_CAMPORT_INDEX_VF;
  if ((err = OMX_GetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_CAMERA_COLOR_FORMAT;
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      errRet = err;
    }
  }

  /* set capture port */
  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = OMX_CAMPORT_INDEX_CP;
  if ((err = OMX_GetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_CAMERA_COLOR_FORMAT;
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      errRet = err;
    }
  }

  /* set thumbnail port */
  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = OMX_CAMPORT_INDEX_CP_T;
  if ((err = OMX_GetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_CAMERA_COLOR_FORMAT;
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      errRet = err;
    }
  }


  return errRet;
}

OMX_ERRORTYPE setColorConvParameters() {
  OMX_ERRORTYPE errRet = OMX_ErrorNone;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE sVideoPortFormat;
  OMX_PARAM_PORTDEFINITIONTYPE sOmxPortDefinition;

  /* set input port */
  setHeader(&sVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
  sVideoPortFormat.nPortIndex = 0;
  sVideoPortFormat.nIndex = 0;
  if ((err = OMX_GetParameter(appPriv->colorconvhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting colorconv input port video format failed!\n");
    errRet = err;
  }
  else {
    sVideoPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sVideoPortFormat.eColorFormat = DEFAULT_CAMERA_COLOR_FORMAT; 
    sVideoPortFormat.xFramerate = 0;
    if ((err = OMX_SetParameter(appPriv->colorconvhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting colorconv input port video format failed!\n");
      errRet = err;
    }
  }

  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = 0;
  if ((err = OMX_GetParameter(appPriv->colorconvhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting colorconv input port parameters failed!\n");
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_CAMERA_COLOR_FORMAT; 
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->colorconvhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting colorconv input port parameters failed!\n");
      errRet = err;
    }
  }

  /* set output port */
  setHeader(&sVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
  sVideoPortFormat.nPortIndex = 1;
  sVideoPortFormat.nIndex = 0;
  if ((err = OMX_GetParameter(appPriv->colorconvhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting colorconv input port video format failed!\n");
    errRet = err;
  }
  else {
    sVideoPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sVideoPortFormat.eColorFormat = DEFAULT_FBSINK_COLOR_FORMAT; 
    sVideoPortFormat.xFramerate = 0;
    if ((err = OMX_SetParameter(appPriv->colorconvhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting colorconv input port video format failed!\n");
      errRet = err;
    }
  }

  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = 1;
  if ((err = OMX_GetParameter(appPriv->colorconvhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting colorconv input port parameters failed!\n");
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_FBSINK_COLOR_FORMAT; 
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->colorconvhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting colorconv input port parameters failed!\n");
      errRet = err;
    }
  }

  

  return errRet;
}



OMX_ERRORTYPE setFbsinkParameters() {
  OMX_ERRORTYPE errRet = OMX_ErrorNone;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PORTFORMATTYPE sVideoPortFormat;
  OMX_PARAM_PORTDEFINITIONTYPE sOmxPortDefinition;
  OMX_CONFIG_RECTTYPE sConfigCrop;
  OMX_CONFIG_POINTTYPE sConfigOutputPosition;

  /* set input port */
  setHeader(&sVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
  sVideoPortFormat.nPortIndex = 0;
  sVideoPortFormat.nIndex = 0;
  if ((err = OMX_GetParameter(appPriv->fbsinkhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting fbsink input port video format failed!\n");
    errRet = err;
  }
  else {
    sVideoPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sVideoPortFormat.eColorFormat = DEFAULT_FBSINK_COLOR_FORMAT;
    sVideoPortFormat.xFramerate = 0;
    if ((err = OMX_SetParameter(appPriv->fbsinkhandle, OMX_IndexParamVideoPortFormat, &sVideoPortFormat)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting fbsink input port video format failed!\n");
      errRet = err;
    }
  }

  setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sOmxPortDefinition.nPortIndex = 0;
  if ((err = OMX_GetParameter(appPriv->fbsinkhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting fbsink input port parameters failed!\n");
    errRet = err;
  }
  else {
    sOmxPortDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.nStride = DEFAULT_FRAME_WIDTH;
    sOmxPortDefinition.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    sOmxPortDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    sOmxPortDefinition.format.video.eColorFormat = DEFAULT_FBSINK_COLOR_FORMAT;
    sOmxPortDefinition.nBufferSize = sOmxPortDefinition.format.video.nStride * sOmxPortDefinition.format.video.nFrameHeight * 3;
    if ((err = OMX_SetParameter(appPriv->fbsinkhandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting fbsink input port parameters failed!\n");
      errRet = err;
    }
  }

  
  setHeader(&sConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
  sConfigCrop.nPortIndex = 0;
  if ((err = OMX_GetConfig(appPriv->fbsinkhandle, OMX_IndexConfigCommonInputCrop, &sConfigCrop)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting fbsink input port Crop config failed!\n");
    errRet = err;
  }
  else {
    sConfigCrop.nLeft = 0;
    sConfigCrop.nTop = 0;
    sConfigCrop.nWidth = DEFAULT_FRAME_WIDTH;
    sConfigCrop.nHeight = DEFAULT_FRAME_HEIGHT;
    if ((err = OMX_SetConfig(appPriv->fbsinkhandle, OMX_IndexConfigCommonInputCrop, &sConfigCrop)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting fbsink input port Crop config failed!\n");
      errRet = err;
    }
  }
  

  /* set display position (x, y) */
  setHeader(&sConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
  sConfigOutputPosition.nPortIndex = 0;
  if ((err = OMX_GetConfig(appPriv->fbsinkhandle, OMX_IndexConfigCommonOutputPosition, &sConfigOutputPosition)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting fbsink output port Position config failed!\n");
    errRet = err;
  }
  else {
    sConfigOutputPosition.nX = 0;
    sConfigOutputPosition.nY = 0;
    if ((err = OMX_SetConfig(appPriv->fbsinkhandle, OMX_IndexConfigCommonOutputPosition, &sConfigOutputPosition)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Setting fbsink output port Position config failed!\n");
      errRet = err;
    }
  }
  

  return errRet;
}


void display_help(char* cSelfName) {
  fprintf(stdout, "\n");
  fprintf(stdout, "Usage: %s [-h] [-i] [-p] [-t preview_time] [-s capture_time] [-c capture_file] [-m thumbnail_file] [-n run_count]\n", cSelfName);
  fprintf(stdout, "\n");
  fprintf(stdout, "    -i: If this option is specified, the camera will be running in still image capture mode\n");
  fprintf(stdout, "        Else, the camera will be running in video capture mode by default\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "    -p: If this option is specified, the camera will be running in autopause mode\n");
  fprintf(stdout, "        Else, the camera will not be in autopause mode by default\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "    -t preview_time: If this option is specified, the camera will stay in preview state for \"preview_time\" seconds\n");
  fprintf(stdout, "                     Else, the camera will preview for 5 seconds by default\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "    -s capture_time: After preview, the camera will start capturing videos\n");
  fprintf(stdout, "                     If this option is specified, the camera will stay in video-capture state for \"capture_time\" seconds\n");
  fprintf(stdout, "                     Else, the camera will capture videos for 5 seconds by default\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "    -c capture_file: If this option is specified, the camera captured videos will be saved in file \"capture_file\"\n");
  fprintf(stdout, "                     Else, captured videos will be saved in file \"%s\" by default\n", g_DefaultCaptureFileName);
  fprintf(stdout, "\n");
  fprintf(stdout, "    -m thumbnail_file: If this option is specified, the camera thumbnail/snapshot image will be saved in file \"thumbnail_file\"\n");
  fprintf(stdout, "                     Else, thumbnail image will be saved in file \"%s\" by default\n", g_DefaultThumbnailFileName);
  fprintf(stdout, "\n");
  fprintf(stdout, "    -n run_count: If this option is specified, this test app will run for \"run_count\" times\n");
  fprintf(stdout, "                     Else, run only once by default\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "    -h: Displays this help\n");
  fprintf(stdout, "\n");
}


int main(int argc, char** argv) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BOOL bOmxInitialized = OMX_FALSE;
  OMX_PARAM_PORTDEFINITIONTYPE sOmxPortDefinition;
  OMX_CONFIG_BOOLEANTYPE sOmxCapturing;
  OMX_CONFIG_BOOLEANTYPE sOmxAutoPause;
  OMX_STATETYPE sOmxState;
  OMX_U32 nBufferCount;
  OMX_U32 nBufferSize;
  OMX_U32 nPortIndex;
  OMX_U32 i;
  unsigned int nPreviewTime = 5;/* By default, running for 5 sec for preview */
  unsigned int nCaptureTime = 5;/* By default, running for 5 sec for video capture */
  char *cCaptureFileName = g_DefaultCaptureFileName;
  char *cThumbnailFileName = g_DefaultThumbnailFileName;
  OMX_BOOL bCameraStillImageMode = OMX_FALSE; /* By default, the camera is running in video capture mode */
  OMX_BOOL bCameraAutoPause = OMX_FALSE; /* By default, the camera is not running in autopause mode */
  unsigned int nMaxRunCount = 1;/* By default, running once */
  unsigned int nRunCount = 0;

  /* Parse arguments */
  for ( i = 1; i < argc && argv[i][0] == '-'; i++) {
    switch (argv[i][1]) {
      case 'i':
        bCameraStillImageMode = OMX_TRUE;
        break;

      case 'p':
        bCameraAutoPause = OMX_TRUE;
        break;

      case 't':
        i++;
        if (i>=argc ||argv[i][0] == '-') {
          DEBUG(DEB_LEV_ERR, "preview_time expected!\n");
          display_help(argv[0]);
          exit(-1);
        }
        nPreviewTime = (unsigned int)atoi(argv[i]);
        break;

      case 's':
        i++;
        if (i>=argc ||argv[i][0] == '-') {
          DEBUG(DEB_LEV_ERR, "capture_time expected!\n");
          display_help(argv[0]);
          exit(-1);
        }
        nCaptureTime = (unsigned int)atoi(argv[i]);
        break;

      case 'c':
        i++;
        if (i>=argc ||argv[i][0] == '-') {
          DEBUG(DEB_LEV_ERR, "capture_file expected!\n");
          display_help(argv[0]);
          exit(-1);
        }
        cCaptureFileName = argv[i];
        break;

      case 'm':
        i++;
        if (i>=argc ||argv[i][0] == '-') {
          DEBUG(DEB_LEV_ERR, "thumbnail_file expected!\n");
          display_help(argv[0]);
          exit(-1);
        }
        cThumbnailFileName = argv[i];
        break;

      case 'n':
        i++;
        if (i>=argc ||argv[i][0] == '-') {
          DEBUG(DEB_LEV_ERR, "run_count expected!\n");
          display_help(argv[0]);
          exit(-1);
        }
        nMaxRunCount = (unsigned int)atoi(argv[i]);
        break;

      case 'h':
        display_help(argv[0]);
        exit(0);
        break;

      default:
        DEBUG(DEB_LEV_ERR, "Unrecognized option -%c!\n", argv[i][1]);
        display_help(argv[0]);
        exit(-1);
      }
   }


  /* Init the Omx core */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Init the OMX core\n");
  if ((err = OMX_Init()) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
    goto EXIT;
  }
  bOmxInitialized =  OMX_TRUE;

  /* Initialize application private data */
  appPriv = (appPrivateType *)malloc(sizeof(appPrivateType));
  if (appPriv == NULL) {
    DEBUG(DEB_LEV_ERR, "Allocate app private data failed!Exiting...\n");
    err = OMX_ErrorInsufficientResources;
    goto EXIT;
  }
  memset(appPriv, 0, sizeof(appPrivateType));

  memset(&sCameraPortBufferList, 0, NUM_CAMERAPORTS*sizeof(OMX_PORTBUFFERCTXT));

  /* Open output file for camera capture and thumbnail port */
  fCapture=fopen(cCaptureFileName, "wb");
  fThumbnail=fopen(cThumbnailFileName, "wb");


  /* Getting camera component handle */
  if ((err = OMX_GetHandle(&appPriv->camerahandle, "OMX.st.v4l.camera_source", appPriv, &camera_source_callbacks)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting camera component handle failed!Exiting...\n");
    goto EXIT;
  }

  /* Getting fbsink component handle */
  if ((err = OMX_GetHandle(&appPriv->colorconvhandle, "OMX.st.video_colorconv.ffmpeg", appPriv, &colorconv_callbacks)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting color conv component handle failed!Exiting...\n");
    goto EXIT;
  }

  /* Getting fbsink component handle */
  if ((err = OMX_GetHandle(&appPriv->fbsinkhandle, "OMX.st.fbdev.fbdev_sink", appPriv, &fbsink_callbacks)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Getting fbsink component handle failed!Exiting...\n");
    goto EXIT;
  }

  /* Setting parameters for camera component */
  if ((err = setCameraParameters(bCameraStillImageMode)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set camera parameters failed! Use default settings...\n");
    /* Do not exit! */
  }

  /* Setting parameters for color converter component */
  if ((err = setColorConvParameters()) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set fbsink parameters failed! Use default settings...\n");
    /* Do not exit! */
  }


  /* Setting parameters for fbsink component */
  if ((err = setFbsinkParameters()) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set fbsink parameters failed! Use default settings...\n");
    /* Do not exit! */
  }

  /* Allocate and init semaphores */
  appPriv->cameraSourceEventSem = (tsem_t *)malloc(sizeof(tsem_t));
  if (appPriv->cameraSourceEventSem == NULL) {
    DEBUG(DEB_LEV_ERR, "Allocate camera event semaphore failed!Exiting...\n");
    err = OMX_ErrorInsufficientResources;
    goto EXIT;
  }
  tsem_init(appPriv->cameraSourceEventSem, 0);

  appPriv->fbsinkEventSem = (tsem_t *)malloc(sizeof(tsem_t));
  if (appPriv->fbsinkEventSem == NULL) {
    DEBUG(DEB_LEV_ERR, "Allocate fbsink event semaphore failed!Exiting...\n");
    err = OMX_ErrorInsufficientResources;
    goto EXIT;
  }
  tsem_init(appPriv->fbsinkEventSem, 0);

  appPriv->colorconvEventSem = (tsem_t *)malloc(sizeof(tsem_t));
  if (appPriv->colorconvEventSem == NULL) {
    DEBUG(DEB_LEV_ERR, "Allocate colorconv event semaphore failed!Exiting...\n");
    err = OMX_ErrorInsufficientResources;
    goto EXIT;
  }
  tsem_init(appPriv->colorconvEventSem, 0);

  /* Setup tunnel between camera preview port, color converter and fbsink */
  if ((err = OMX_SetupTunnel(appPriv->camerahandle, OMX_CAMPORT_INDEX_VF, appPriv->colorconvhandle, 0)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Setup tunnel between camera preview port and color converter failed!Exiting...\n");
    goto EXIT;
  }
  if ((err = OMX_SetupTunnel(appPriv->colorconvhandle, 1, appPriv->fbsinkhandle, 0)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Setup tunnel between color conv port and fbsink failed!Exiting...\n");
    goto EXIT;
  }

  /* disable the clock port of the video sink */
  err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandPortDisable, 1, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"video sink clock port disable failed err=%x \n",err);
    exit(1);
  }
  tsem_down(appPriv->fbsinkEventSem); /* video sink clock port disabled */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Video Sink Clock Port Disabled\n", __func__);

RUN_AGAIN:

  /* Transition camera component Loaded-->Idle */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Loaded-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition color conv component Loaded-->Idle */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Color Conv Loaded-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Loaded-->Idle */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Loaded-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Allocate port buffers for camera component */
  for (nPortIndex = OMX_CAMPORT_INDEX_CP; nPortIndex <= OMX_CAMPORT_INDEX_CP_T; nPortIndex++) {
    setHeader(&sOmxPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    sOmxPortDefinition.nPortIndex = nPortIndex;
    if ((err = OMX_GetParameter(appPriv->camerahandle, OMX_IndexParamPortDefinition, &sOmxPortDefinition)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "OMX_GetParameter for camera on OMX_IndexParamPortDefinition index failed!Exiting...\n");
      goto EXIT;
    }
    nBufferCount = sOmxPortDefinition.nBufferCountActual;
    nBufferSize = sOmxPortDefinition.nBufferSize;
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Camera port[%ld] needs %ld buffers each of which is %ld bytes\n", nPortIndex, nBufferCount, nBufferSize);

    for (i = 0; i < nBufferCount; i++) {
      if ((err = OMX_AllocateBuffer(appPriv->camerahandle, &sCameraPortBufferList[nPortIndex].pBufHeaderList[i], nPortIndex, NULL, nBufferSize)) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Allocate port buffer for camera failed!Exiting...\n");
        goto EXIT;
      }
      sCameraPortBufferList[nPortIndex].nBufferCountActual++;
    }
  }

  /* Wait camera (Loaded-->Idle) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait fbsink (Loaded-->Idle) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Loaded-->Idle) to complete */
  tsem_down(appPriv->fbsinkEventSem);



  /* Transition camera component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition color conv component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Colorconv Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Wait camera (Idle-->Exec) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait color conv (Idle-->Exec) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Idle-->Exec) to complete */
  tsem_down(appPriv->fbsinkEventSem);


  fprintf(stdout, "Start preview, for %d sec...\n", nPreviewTime);
  sleep(nPreviewTime);

  /* Fill buffers to camera capture port */
  for (i = 0; i < sCameraPortBufferList[OMX_CAMPORT_INDEX_CP].nBufferCountActual; i++) {
    sCameraPortBufferList[OMX_CAMPORT_INDEX_CP].pBufHeaderList[i]->nFilledLen = 0;
    sCameraPortBufferList[OMX_CAMPORT_INDEX_CP].pBufHeaderList[i]->nOffset = 0;
    if ((err = OMX_FillThisBuffer(appPriv->camerahandle, sCameraPortBufferList[OMX_CAMPORT_INDEX_CP].pBufHeaderList[i])) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Fill buffer to camera capture port failed!Exiting...\n");
      goto EXIT;
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Fill buffer[%ld] (0x%lX) to camera capture port\n", __func__, i, (OMX_U32)sCameraPortBufferList[OMX_CAMPORT_INDEX_CP].pBufHeaderList[i]);
  }

  /* Fill buffers to camera thumbnail port */
  for (i = 0; i < sCameraPortBufferList[OMX_CAMPORT_INDEX_CP_T].nBufferCountActual; i++) {
    sCameraPortBufferList[OMX_CAMPORT_INDEX_CP_T].pBufHeaderList[i]->nFilledLen = 0;
    sCameraPortBufferList[OMX_CAMPORT_INDEX_CP_T].pBufHeaderList[i]->nOffset = 0;
    if ((err = OMX_FillThisBuffer(appPriv->camerahandle, sCameraPortBufferList[OMX_CAMPORT_INDEX_CP_T].pBufHeaderList[i])) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Fill buffer to camera capture port failed!Exiting...\n");
      goto EXIT;
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Fill buffer[%ld] (0x%lX) to camera capture port\n", __func__, i, (OMX_U32)sCameraPortBufferList[OMX_CAMPORT_INDEX_CP_T].pBufHeaderList[i]);
  }

  /* Set up autopause mode */
  setHeader(&sOmxAutoPause, sizeof(OMX_CONFIG_BOOLEANTYPE));
  sOmxAutoPause.bEnabled = bCameraAutoPause;
  if ((err = OMX_SetConfig(appPriv->camerahandle, OMX_IndexAutoPauseAfterCapture, &sOmxAutoPause)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set autopause mode failed!Use default settings...\n");
    /* Do not exit */
  }

  /*  Start capturing */
  setHeader(&sOmxCapturing, sizeof(OMX_CONFIG_BOOLEANTYPE));
  sOmxCapturing.bEnabled = OMX_TRUE;
  if ((err = OMX_SetConfig(appPriv->camerahandle, OMX_IndexConfigCapturing, &sOmxCapturing)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Start capturing failed!Exiting...\n");
    goto EXIT;
  }

  fprintf(stdout, "Start capturing, for %d sec...\n", nCaptureTime);
  sleep(nCaptureTime);

  /*  Stop capturing */
  if (!bCameraStillImageMode) {
    setHeader(&sOmxCapturing, sizeof(OMX_CONFIG_BOOLEANTYPE));
    sOmxCapturing.bEnabled = OMX_FALSE;
    if ((err = OMX_SetConfig(appPriv->camerahandle, OMX_IndexConfigCapturing, &sOmxCapturing)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Stop capturing failed!Exiting...\n");
      goto EXIT;
    }
    fprintf(stdout, "Stop capturing...\n");
  }

  /* If in autopause mode, stay for a while before exit */ 
  if (bCameraAutoPause) {
    fprintf(stdout, "Now the camera is in autopause mode, wait for %d sec before out of this mode...\n", 5);
    sleep(5);
    /* Stop autopause mode */
    if ((err = OMX_GetState(appPriv->camerahandle,&sOmxState)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Get camera state failed!Exiting...\n");
      goto EXIT;
    }
    if (sOmxState == OMX_StatePause) {
      if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet,
                                                      OMX_StateExecuting, 0 )) != OMX_ErrorNone ) {
        DEBUG(DEB_LEV_ERR, "Pause-->Exec failed!Exiting...\n");
        goto EXIT;
      }
      /* Wait camera (Pause-->Exec) to complete */
      tsem_down(appPriv->cameraSourceEventSem);
      fprintf(stdout, "Now the camera is out of autopause mode, wait for %d sec before exit...\n", 5);
      sleep(5);
    }
    else {
      DEBUG(DEB_LEV_ERR, "The camera is not in Pause state in autopause mode, ignore...\n");
    }
  }

  /* Transition camera component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition colorconv component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Color conv Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Wait camera (Exec-->Idle) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait color conv (Exec-->Idle) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Exec-->Idle) to complete */
  tsem_down(appPriv->fbsinkEventSem);
  

  /* Transition camera component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition color conv component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Colorconv Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Idle-->Exec */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Idle-->Exec failed!Exiting...\n");
    goto EXIT;
  }

  /* Wait camera (Idle-->Exec) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait color conv (Exec-->Idle) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Idle-->Exec) to complete */
  tsem_down(appPriv->fbsinkEventSem);

  fprintf(stdout, "Continue to preview, for %d sec...\n", nPreviewTime);
  sleep(nPreviewTime);

  /* Transition camera component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition color conv component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Color conv Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Exec-->Idle */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }


  /* Wait camera (Exec-->Idle) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait color conv (Exec-->Idle) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Exec-->Idle) to complete */
  tsem_down(appPriv->fbsinkEventSem);


  /* Transition camera component Idle-->Loaded */
  if ((err = OMX_SendCommand(appPriv->camerahandle, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Camera Idle-->Loaded failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition color conv component Idle-->Loaded */
  if ((err = OMX_SendCommand(appPriv->colorconvhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Color conv Exec-->Idle failed!Exiting...\n");
    goto EXIT;
  }

  /* Transition fbsink component Idle-->Loaded */
  if ((err = OMX_SendCommand(appPriv->fbsinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Fbsink Idle-->Loaded failed!Exiting...\n");
    goto EXIT;
  }

  /* Free bufers for each non-tunneled port of camera component */
  for (nPortIndex = OMX_CAMPORT_INDEX_CP; nPortIndex <= OMX_CAMPORT_INDEX_CP_T; nPortIndex++) {
    for (i = 0; i < sCameraPortBufferList[nPortIndex].nBufferCountActual; i++) {
      if (sCameraPortBufferList[nPortIndex].pBufHeaderList[i] != NULL) {
        OMX_FreeBuffer(appPriv->camerahandle, nPortIndex, sCameraPortBufferList[nPortIndex].pBufHeaderList[i]);
      }
    }
    sCameraPortBufferList[nPortIndex].nBufferCountActual = 0;
  }

  /* Wait camera (Idle-->Loaded) to complete */
  tsem_down(appPriv->cameraSourceEventSem);
  /* Wait color conv (Exec-->Idle) to complete */
  tsem_down(appPriv->colorconvEventSem);
  /* Wait fbsink (Idle-->Loaded) to complete */
  tsem_down(appPriv->fbsinkEventSem);

  nRunCount++;
  if (nRunCount < nMaxRunCount) {
    goto RUN_AGAIN;
  }

  fprintf(stdout, "The captured videos are saved in file \"%s\"\n", cCaptureFileName);
  fprintf(stdout, "The thumbnail image is saved in file \"%s\"\n", cThumbnailFileName);

EXIT:
  if (fCapture != NULL) {
    fclose(fCapture);
  }
  if (fThumbnail != NULL) {
    fclose(fThumbnail);
  }

  /* Free app private data */
  if (appPriv != NULL) {
    /* Free semaphores */
    if (appPriv->cameraSourceEventSem != NULL) {
      tsem_deinit(appPriv->cameraSourceEventSem);
      free(appPriv->cameraSourceEventSem);
    }

    if (appPriv->colorconvEventSem != NULL) {
      tsem_deinit(appPriv->colorconvEventSem);
      free(appPriv->colorconvEventSem);
    }

    if (appPriv->fbsinkEventSem != NULL) {
      tsem_deinit(appPriv->fbsinkEventSem);
      free(appPriv->fbsinkEventSem);
    }

    /* Free camera component handle */
    if (appPriv->camerahandle != NULL) {
      OMX_FreeHandle(appPriv->camerahandle);
    }
    /* Free Color conv component handle */
    if (appPriv->colorconvhandle != NULL) {
      OMX_FreeHandle(appPriv->colorconvhandle);
    }

    /* Free fbsink component handle */
    if (appPriv->fbsinkhandle != NULL) {
      OMX_FreeHandle(appPriv->fbsinkhandle);
    }

    free(appPriv);
  }

  /* Deinit the Omx core */
  if (bOmxInitialized) {
    OMX_Deinit();
  }

  return (int) err;
}
