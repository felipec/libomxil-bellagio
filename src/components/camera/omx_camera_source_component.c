/**
  @file src/components/camera/omx_camera_source_component.c

  OpenMAX camera source component.
  The OpenMAX camera component is a V4L2-based video source whose functionalities
  include preview, video capture, image capture, video thumbnail and image
  thumbnail. It has 3 (output) ports: Port 0 is used for preview; Port 1 is used
  for video and image capture; Port 2 is used for video and image thumbnail.

  Copyright (C) 2007-2008  Motorola and STMicroelectronics

  This code is licensed under LGPL see README for full LGPL notice.

  Date                             Author                Comment
  Mon, 09 Jul 2007                 Motorola              File created
  Tue, 06 Apr 2008                 STM                   Modified to support Video for Linux Two(V4L2)

  This Program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  $Date$
  Revision $Rev$
  Author $Author$

*/

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include <OMX_Core.h>
#include <omx_comp_debug_levels.h>

#include "omx_camera_source_component.h"

#define DEFAULT_FRAME_RATE 15

#define DEFAULT_FRAME_WIDTH 320
#define DEFAULT_FRAME_HEIGHT 240

#define DEFAULT_COLOR_FORMAT OMX_COLOR_FormatYUV420PackedPlanar

#define GET_SYSERR_STRING() strerror(errno)

/* Thumbnail (snapshot) index from video captured frame */
#define OMX_CAM_VC_SNAPSHOT_INDEX    5

#define CLEAR(x) memset (&(x), 0, sizeof (x))



/* V4L2 Mapping Queue Interface */
#define OMX_MAPBUFQUEUE_ISEMPTY(_queue_) (0 == (_queue_).nBufCountTotal)
#define OMX_MAPBUFQUEUE_ISFULL(_queue_) ((_queue_).nBufCountTotal > 0 && (_queue_).nNextCaptureIndex == (_queue_).nLastBufIndex)
#define OMX_MAPBUFQUEUE_NOBUFCAPTURED(_queue_) (0 == (_queue_).nBufCountCaptured)
#define OMX_MAPBUFQUEUE_HASBUFCAPTURED(_queue_) (!(OMX_MAPBUFQUEUE_NOBUFCAPTURED(_queue_)))
#define OMX_MAPBUFQUEUE_NOBUFWAITTOCAPTURE(_queue_) (OMX_MAPBUFQUEUE_HASBUFCAPTURED(_queue_) && (_queue_).nNextWaitIndex == (_queue_).nNextCaptureIndex)
#define OMX_MAPBUFQUEUE_HASBUFWAITTOCAPTURE(_queue_) (!(OMX_MAPBUFQUEUE_NOBUFWAITTOCAPTURE(_queue_)))

#define OMX_MAPBUFQUEUE_GETMAXLEN(_queue_) ((_queue_).nFrame)

#define OMX_MAPBUFQUEUE_GETNEXTCAPTURE(_queue_) ((_queue_).nNextCaptureIndex)
#define OMX_MAPBUFQUEUE_GETNEXTWAIT(_queue_) ((_queue_).nNextWaitIndex)
#define OMX_MAPBUFQUEUE_GETLASTBUFFER(_queue_) ((_queue_).nLastBufIndex)
#define OMX_MAPBUFQUEUE_GETNEXTINDEX(_queue_, _curindex_) ((_curindex_ + 1) % (_queue_).nFrame)

#define OMX_MAPBUFQUEUE_GETBUFCOUNTCAPTURED(_queue_) ((_queue_).nBufCountCaptured)

#define OMX_MAPBUFQUEUE_GETBUFADDR(_queue_, _bufindex_) ((OMX_PTR) (_queue_).buffers[(_bufindex_)].pCapAddrStart)

#define OMX_MAPBUFQUEUE_MAKEEMPTY(_queue_) do \
                                                                               { \
                                                                                   (_queue_).nNextCaptureIndex = 0; \
                                                                                   (_queue_).nNextWaitIndex = 0; \
                                                                                   (_queue_).nLastBufIndex = 0; \
                                                                                   (_queue_).nBufCountTotal = 0; \
                                                                                   (_queue_).nBufCountCaptured = 0; \
                                                                               } while (0)
#define OMX_MAPBUFQUEUE_ENQUEUE(_queue_) do \
                                                                               { \
                                                                                   (_queue_).nNextCaptureIndex = ((_queue_).nNextCaptureIndex + 1) % (_queue_).nFrame; \
                                                                                   (_queue_).nBufCountTotal ++; \
                                                                               } while (0)
#define OMX_MAPBUFQUEUE_DEQUEUE(_queue_) do \
                                                                               { \
                                                                                   (_queue_).nLastBufIndex = ((_queue_).nLastBufIndex + 1) % (_queue_).nFrame; \
                                                                                   (_queue_).nBufCountTotal --; \
                                                                                   (_queue_).nBufCountCaptured --; \
                                                                               } while (0)

#define OMX_MAPBUFQUEUE_ADDCAPTUREDBUF(_queue_) do \
                                                                               { \
                                                                                   (_queue_).nNextWaitIndex = ((_queue_).nNextWaitIndex + 1) % (_queue_).nFrame; \
                                                                                   (_queue_).nBufCountCaptured ++; \
                                                                               } while (0)

#define OMX_MAPBUFQUEUE_GETTIMESTAMP(_queue_, _bufindex_) ((_queue_).qTimeStampQueue[(_bufindex_)])
#define OMX_MAPBUFQUEUE_SETTIMESTAMP(_queue_, _bufindex_, _timestamp_) do \
                                                                               { \
                                                                                   (_queue_).qTimeStampQueue[(_bufindex_)] = (OMX_TICKS)(_timestamp_); \
                                                                               } while (0)


typedef struct CAM_SENSOR_OMXV4LCOLORTYPE
{
  OMX_COLOR_FORMATTYPE eOmxColorFormat;
  V4L2_COLOR_FORMATTYPE sV4lColorFormat;
} CAM_SENSOR_OMXV4LCOLORTYPE;


typedef struct CAM_CAPTURE_FRAMESIZETYPE
{
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} CAM_CAPTURE_FRAMESIZETYPE;


static const CAM_SENSOR_OMXV4LCOLORTYPE g_SupportedColorTable[] = {
  {OMX_COLOR_FormatL8, {V4L2_PIX_FMT_GREY, 8}},
  {OMX_COLOR_Format16bitRGB565,{V4L2_PIX_FMT_RGB565,16}},
  {OMX_COLOR_Format24bitRGB888,{V4L2_PIX_FMT_RGB24,24}},
  {OMX_COLOR_FormatYCbYCr,{V4L2_PIX_FMT_YUYV,16}},
  {OMX_COLOR_FormatYUV422PackedPlanar,{V4L2_PIX_FMT_YUV422P,16}},
  {OMX_COLOR_FormatYUV420PackedPlanar,{V4L2_PIX_FMT_YUV420,12}}
};


/* Table for supported capture framsizes (based on the WebEye V2000 camera) */
static const CAM_CAPTURE_FRAMESIZETYPE g_SupportedFramesizeTable[] =
{
    {64, 48},
    {72, 64},
    {88, 72},
    {96, 128},
    {128, 96},
    {144, 176},
    {160, 120},
    {176, 144},
    {200, 80},
    {224, 96},
    {256, 144},
    {320, 240},
    {352, 288},
    {432, 256},
    {512, 376},
    {568, 400},
    {640, 480}
    /* more settings can be added ... */
};

static int camera_init_mmap(omx_camera_source_component_PrivateType* omx_camera_source_component_Private);

static int xioctl(int fd, int request, void *arg)
{
  int r;

  do
    r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}

static OMX_ERRORTYPE omx_camera_source_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT  OMX_PTR ComponentParameterStructure);

static OMX_ERRORTYPE omx_camera_source_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

static OMX_ERRORTYPE omx_camera_source_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nConfigIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE omx_camera_source_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nConfigIndex,
  OMX_IN OMX_PTR pComponentConfigStructure);

/** This is the central function for buffer processing.
  * It is executed in a separate thread.
  * @param param input parameter, a pointer to the OMX standard structure
  */
static void* omx_camera_source_component_BufferMgmtFunction (void* param);

/** Specific operations for camera component when state changes
 * @param openmaxStandComp the openmax component which state is to be changed
 * @param destinationState the requested target state
 */
static OMX_ERRORTYPE omx_camera_source_component_DoStateSet(OMX_COMPONENTTYPE *openmaxStandComp, OMX_U32 destinationState);

static OMX_ERRORTYPE camera_CheckSupportedColorFormat(OMX_IN OMX_COLOR_FORMATTYPE eColorFormat);
static OMX_ERRORTYPE camera_CheckSupportedFramesize(
  OMX_IN OMX_U32 nFrameWidth,
  OMX_IN  OMX_U32 nFrameHeight);
static OMX_ERRORTYPE camera_MapColorFormatOmxToV4l(
  OMX_IN OMX_COLOR_FORMATTYPE eOmxColorFormat,
  OMX_INOUT V4L2_COLOR_FORMATTYPE* pV4lColorFormat );

static OMX_ERRORTYPE camera_MapColorFormatV4lToOmx(
  OMX_IN V4L2_COLOR_FORMATTYPE* pV4lColorFormat,
  OMX_INOUT OMX_COLOR_FORMATTYPE* pOmxColorFormat );

static OMX_U32 camera_CalculateBufferSize(
  OMX_IN OMX_U32 nWidth,
  OMX_IN OMX_U32 nHeight,
  OMX_IN OMX_COLOR_FORMATTYPE eOmxColorFormat);

static OMX_ERRORTYPE camera_SetConfigCapturing(
  OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,
  OMX_IN OMX_CONFIG_BOOLEANTYPE *pCapturing );


static OMX_ERRORTYPE camera_InitCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_DeinitCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_StartCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_StopCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_HandleThreadBufferCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_GenerateTimeStamp(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_SendCapturedBuffers(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_SendLastCapturedBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_UpdateCapturedBufferQueue(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_ProcessPortOneBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,  OMX_IN OMX_U32 nPortIndex );
static OMX_ERRORTYPE camera_DropLastCapturedBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_ReformatVideoFrame(
  OMX_IN OMX_PTR              pSrcFrameAddr,
  OMX_IN OMX_U32              nSrcFrameWidth,
  OMX_IN OMX_U32              nSrcFrameHeight,
  OMX_IN V4L2_COLOR_FORMATTYPE sSrcV4l2ColorFormat,
  OMX_IN OMX_PTR              pDstFrameAddr,
  OMX_IN OMX_U32              nDstFrameWidth,
  OMX_IN OMX_U32              nDstFrameHeight,
  OMX_IN OMX_S32              nDstFrameStride,
  OMX_IN OMX_COLOR_FORMATTYPE eDstOmxColorFormat,
  OMX_IN OMX_BOOL bStrideAlign );
static OMX_ERRORTYPE camera_AddTimeStamp(
  OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,
  OMX_IN OMX_BUFFERHEADERTYPE *pBufHeader);
static OMX_ERRORTYPE camera_UpdateThumbnailCondition(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_HandleStillImageCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);
static OMX_ERRORTYPE camera_HandleThumbnailCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private);


/* Check whether eColorFormat is supported */
static OMX_ERRORTYPE camera_CheckSupportedColorFormat(OMX_IN OMX_COLOR_FORMATTYPE eColorFormat) {
  OMX_U32 i;

  for (i = 0; i < sizeof(g_SupportedColorTable)/sizeof(g_SupportedColorTable[0]); i++)
  {
    if (eColorFormat == g_SupportedColorTable[i].eOmxColorFormat)
    {
      return OMX_ErrorNone;
    }
  }

  /* Not found supported color format */
  return OMX_ErrorUnsupportedSetting;
}


/* Check whether the frame size (nFrameWidth, nFrameHeight) is supported */
static OMX_ERRORTYPE camera_CheckSupportedFramesize(
  OMX_IN OMX_U32 nFrameWidth,
  OMX_IN  OMX_U32 nFrameHeight) {
  OMX_U32 i;

  for (i = 0; i < sizeof(g_SupportedFramesizeTable)/sizeof(g_SupportedFramesizeTable[0]); i++)
  {
    if (nFrameWidth == g_SupportedFramesizeTable[i].nWidth &&
         nFrameHeight == g_SupportedFramesizeTable[i].nHeight)
    {
      return OMX_ErrorNone;
    }
  }

  /* Not found supported frame size */
  return OMX_ErrorUnsupportedSetting;
}

/* Map OMX color format to V4L2 color format */
static OMX_ERRORTYPE camera_MapColorFormatOmxToV4l(
  OMX_IN OMX_COLOR_FORMATTYPE eOmxColorFormat,
  OMX_INOUT V4L2_COLOR_FORMATTYPE* pV4lColorFormat ) {
  OMX_U32 i;

  for (i = 0; i < sizeof(g_SupportedColorTable)/sizeof(g_SupportedColorTable[0]); i++) {
    if (eOmxColorFormat == g_SupportedColorTable[i].eOmxColorFormat) {
      (*pV4lColorFormat) = g_SupportedColorTable[i].sV4lColorFormat;
      return OMX_ErrorNone;
    }
  }

  /* Not found supported color format */
  return OMX_ErrorUnsupportedSetting;
}

/* Map V4L2 color format to OMX color format */
static OMX_ERRORTYPE camera_MapColorFormatV4lToOmx(
  OMX_IN V4L2_COLOR_FORMATTYPE* pV4lColorFormat,
  OMX_INOUT OMX_COLOR_FORMATTYPE* pOmxColorFormat ) {
  OMX_U32 i;

  for (i = 0; i < sizeof(g_SupportedColorTable)/sizeof(g_SupportedColorTable[0]); i++) {
    if (pV4lColorFormat->v4l2Pixfmt == g_SupportedColorTable[i].sV4lColorFormat.v4l2Pixfmt &&
        pV4lColorFormat->v4l2Depth == g_SupportedColorTable[i].sV4lColorFormat.v4l2Depth) {
      (*pOmxColorFormat) = g_SupportedColorTable[i].eOmxColorFormat;
      return OMX_ErrorNone;
    }
  }

  /* Not found supported color format */
  return OMX_ErrorUnsupportedSetting;
}

/* Calculate buffer size according to (width,height,color format) */
static OMX_U32 camera_CalculateBufferSize(
  OMX_IN OMX_U32 nWidth,
  OMX_IN OMX_U32 nHeight,
  OMX_IN OMX_COLOR_FORMATTYPE eOmxColorFormat) {
  OMX_U32 i;

  for (i = 0; i < sizeof(g_SupportedColorTable)/sizeof(g_SupportedColorTable[0]); i++) {
    if (eOmxColorFormat == g_SupportedColorTable[i].eOmxColorFormat) {
      return (nWidth * nHeight * g_SupportedColorTable[i].sV4lColorFormat.v4l2Depth + 7) / 8;
    }
  }

  /* Not found supported color format, return 0 */
  return 0;
}

/* Set capturing configuration in OMX_SetConfig */
static OMX_ERRORTYPE camera_SetConfigCapturing(
  OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,
  OMX_IN OMX_CONFIG_BOOLEANTYPE *pCapturing ) {
  omx_camera_source_component_PortType *pCapturePort;
  struct timeval now;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  pthread_mutex_lock(&omx_camera_source_component_Private->setconfig_mutex);
  if ( pCapturing->bEnabled != omx_camera_source_component_Private->bCapturingNext ) {
    if (pCapturing->bEnabled == OMX_TRUE) {
      omx_camera_source_component_Private->bIsFirstFrame = OMX_TRUE;
      gettimeofday(&now, NULL);
      omx_camera_source_component_Private->nRefWallTime = (OMX_TICKS)(now.tv_sec * 1000000 + now.tv_usec);
    }
    omx_camera_source_component_Private->bCapturingNext = pCapturing->bEnabled;
    pCapturePort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
    if (PORT_IS_ENABLED(pCapturePort) &&
         pCapturing->bEnabled == OMX_FALSE &&
         omx_camera_source_component_Private->bAutoPause == OMX_TRUE) {
      /* In autopause mode, command camera component to pause state */
      if ((err = omx_camera_source_component_DoStateSet(omx_camera_source_component_Private->openmaxStandComp,
                     (OMX_U32)OMX_StatePause)) != OMX_ErrorNone ) {
        goto EXIT;
      }
    }
  }

EXIT:
  pthread_mutex_unlock(&omx_camera_source_component_Private->setconfig_mutex);
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Initialize the camera device */
static OMX_ERRORTYPE camera_InitCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *pPreviewPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_VF];
  omx_camera_source_component_PortType *pCapturePort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
  omx_camera_source_component_PortType *pThumbnailPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP_T];
  omx_camera_source_component_PortType *pPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  /* Open camera device file */
  omx_camera_source_component_Private->fdCam = open(V4L2DEV_FILENAME, O_RDWR  | O_NONBLOCK, 0);

  if ( omx_camera_source_component_Private->fdCam < 0 )
  {
    DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Open camera failed: %s\n",__func__,GET_SYSERR_STRING());
    err = OMX_ErrorHardware;
    goto ERR_HANDLE;
  }

  /* Query camera capability */
  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_QUERYCAP, &omx_camera_source_component_Private->cap)) {
    if (EINVAL == errno) {
      DEBUG(DEB_LEV_ERR, "%s is no V4L2 device\n", V4L2DEV_FILENAME);
      err = OMX_ErrorHardware;
      goto ERR_HANDLE;
    } else {
      DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_QUERYCAP", errno, strerror(errno));
      err = OMX_ErrorHardware;
      goto ERR_HANDLE;
    }
  }

  if (!(omx_camera_source_component_Private->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    DEBUG(DEB_LEV_ERR, "%s is no video capture device\n", V4L2DEV_FILENAME);
    return OMX_ErrorHardware;
  }

  if (!(omx_camera_source_component_Private->cap.capabilities & V4L2_CAP_STREAMING)) {
    DEBUG(DEB_LEV_ERR, "%s does not support streaming i/o\n", V4L2DEV_FILENAME);
    return OMX_ErrorHardware;
  }

  /* Select video input, video standard and tune here. */
  omx_camera_source_component_Private->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_CROPCAP, &omx_camera_source_component_Private->cropcap)) {
    /* Errors ignored. */
  }

  omx_camera_source_component_Private->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  omx_camera_source_component_Private->crop.c = omx_camera_source_component_Private->cropcap.defrect;  /* reset to default */

  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_S_CROP, &omx_camera_source_component_Private->crop)) {
    switch (errno) {
    case EINVAL:
      /* Cropping not supported. */
      break;
    default:
      /* Errors ignored. */
      break;
    }
  }

  CLEAR(omx_camera_source_component_Private->fmt);

  /* Get V4L2 buffer map information */
  camera_init_mmap(omx_camera_source_component_Private);

  /* About camera color format settings.... */

  /* Get the camera sensor color format from an enabled port */
  if (PORT_IS_ENABLED(pPreviewPort)) {
    omx_camera_source_component_Private->eOmxColorFormat = pPreviewPort->sPortParam.format.video.eColorFormat;
  }
  else if (PORT_IS_ENABLED(pCapturePort)) {
    omx_camera_source_component_Private->eOmxColorFormat = pCapturePort->sPortParam.format.video.eColorFormat;
  }
  else if (PORT_IS_ENABLED(pThumbnailPort)) {
    omx_camera_source_component_Private->eOmxColorFormat = pThumbnailPort->sPortParam.format.video.eColorFormat;
  }
  else {
    omx_camera_source_component_Private->eOmxColorFormat = DEFAULT_COLOR_FORMAT;
  }

  if ((err = camera_MapColorFormatOmxToV4l(omx_camera_source_component_Private->eOmxColorFormat, &omx_camera_source_component_Private->sV4lColorFormat)) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- map color from omx to v4l failed!\n",__func__);
    goto ERR_HANDLE;
  }

  /** Initialize video capture pixel format */
  /* First get original color format from camera device */
  omx_camera_source_component_Private->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_G_FMT, &omx_camera_source_component_Private->fmt)) {
    DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_G_FMT", errno, strerror(errno));
    err = OMX_ErrorHardware;
    goto ERR_HANDLE;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: v4l2_format.fmt.pix.pixelformat (Before set) = %c%c%c%c\n",__func__,
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>8),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>16),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>24));
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: v4l2_format.fmt.pix.field (Before set) = %d\n",__func__, omx_camera_source_component_Private->fmt.fmt.pix.field);

  /* Set color format and frame size to camera device */
  omx_camera_source_component_Private->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  omx_camera_source_component_Private->fmt.fmt.pix.width = pPort->sPortParam.format.video.nFrameWidth;
  omx_camera_source_component_Private->fmt.fmt.pix.height = pPort->sPortParam.format.video.nFrameHeight;
  omx_camera_source_component_Private->fmt.fmt.pix.pixelformat = omx_camera_source_component_Private->sV4lColorFormat.v4l2Pixfmt;
  omx_camera_source_component_Private->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_S_FMT, &omx_camera_source_component_Private->fmt)) {
    DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_S_FMT", errno, strerror(errno));
    err = OMX_ErrorHardware;
    goto ERR_HANDLE;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: v4l2_format.fmt.pix.pixelformat (After set) = %c%c%c%c\n",__func__,
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>8),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>16),
    (char)(omx_camera_source_component_Private->fmt.fmt.pix.pixelformat>>24));
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: v4l2_format.fmt.pix.field (After set) = %d\n",__func__, omx_camera_source_component_Private->fmt.fmt.pix.field);


  /* Note VIDIOC_S_FMT may change width and height. */
  pPort->sPortParam.format.video.nFrameWidth = omx_camera_source_component_Private->fmt.fmt.pix.width;
  pPort->sPortParam.format.video.nFrameHeight = omx_camera_source_component_Private->fmt.fmt.pix.height;

  /*output frame size*/
  omx_camera_source_component_Private->oFrameSize = pPort->sPortParam.format.video.nFrameWidth*
                                                    pPort->sPortParam.format.video.nFrameHeight*
                                                    omx_camera_source_component_Private->sV4lColorFormat.v4l2Depth/8; /*Eg 12/8 for YUV420*/

  DEBUG(DEB_LEV_SIMPLE_SEQ,"Frame Width=%d, Height=%d, Frame Size=%d nFrame=%d\n",
    (int)pPort->sPortParam.format.video.nFrameWidth,
    (int)pPort->sPortParam.format.video.nFrameHeight,
    (int)omx_camera_source_component_Private->oFrameSize,
    (int)omx_camera_source_component_Private->sMapbufQueue.nFrame);


  /* Allocate time stamp queue */

  omx_camera_source_component_Private->sMapbufQueue.qTimeStampQueue = calloc(omx_camera_source_component_Private->sMapbufQueue.nFrame, sizeof(OMX_TICKS));
  if (omx_camera_source_component_Private->sMapbufQueue.qTimeStampQueue == NULL) {
    DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Allocate time stamp queue failed!\n",__func__);
    err = OMX_ErrorInsufficientResources;
    goto ERR_HANDLE;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;

ERR_HANDLE:
  camera_DeinitCameraDevice(omx_camera_source_component_Private);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Deinitialize the camera device */
static OMX_ERRORTYPE camera_DeinitCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);


  if (omx_camera_source_component_Private->sMapbufQueue.qTimeStampQueue != NULL) {
    free(omx_camera_source_component_Private->sMapbufQueue.qTimeStampQueue);
    omx_camera_source_component_Private->sMapbufQueue.qTimeStampQueue = NULL;
  }

  if(omx_camera_source_component_Private->sMapbufQueue.buffers != NULL ) {
    for (i = 0; i < OMX_MAPBUFQUEUE_GETMAXLEN( omx_camera_source_component_Private->sMapbufQueue ); ++i) {
      DEBUG(DEB_LEV_PARAMS, "i=%d,addr=%x,length=%d\n",(int)i,
        (int)omx_camera_source_component_Private->sMapbufQueue.buffers[i].pCapAddrStart,
        (int)omx_camera_source_component_Private->sMapbufQueue.buffers[i].length);

      if (-1 == munmap(omx_camera_source_component_Private->sMapbufQueue.buffers[i].pCapAddrStart,
                       omx_camera_source_component_Private->sMapbufQueue.buffers[i].length)) {
        DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "munmap", errno, strerror(errno));
        err = OMX_ErrorHardware;
      }
    }

    free(omx_camera_source_component_Private->sMapbufQueue.buffers);

    omx_camera_source_component_Private->sMapbufQueue.buffers = NULL;
  }

  if ( omx_camera_source_component_Private->fdCam >= 0 ) {
    close(omx_camera_source_component_Private->fdCam);
    omx_camera_source_component_Private->fdCam = -1;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Start the camera device */
static OMX_ERRORTYPE camera_StartCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  struct timeval now;
  struct timespec sleepTime;
  OMX_U32 i = 0;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  enum v4l2_buf_type type;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  for ( i = 0; i < OMX_MAPBUFQUEUE_GETMAXLEN( omx_camera_source_component_Private->sMapbufQueue ); i++ ) {
    /* Instruct the camera hardware to start capture */
    DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Start to capture buffer [%d], [width, height] = [%d, %d], pixelformat = %d\n",__func__,(int)i,
      omx_camera_source_component_Private->fmt.fmt.pix.width,
      omx_camera_source_component_Private->fmt.fmt.pix.height,
      omx_camera_source_component_Private->fmt.fmt.pix.pixelformat);

    if ( i == (OMX_MAPBUFQUEUE_GETMAXLEN( omx_camera_source_component_Private->sMapbufQueue ) - 1) ) {
      /* record last capture time */
      gettimeofday(&now, NULL);
      omx_camera_source_component_Private->nLastCaptureTimeInMilliSec = ((OMX_U32)now.tv_sec) * 1000 + ((OMX_U32)now.tv_usec) / 1000;
    }

    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_QBUF, &buf)) {
      DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Instruct the camera hardware to start capture failed 1: %s\n",__func__,GET_SYSERR_STRING());
      err = OMX_ErrorHardware;
      goto EXIT;
    }

    OMX_MAPBUFQUEUE_ENQUEUE( omx_camera_source_component_Private->sMapbufQueue );

    if ( i != (OMX_MAPBUFQUEUE_GETMAXLEN( omx_camera_source_component_Private->sMapbufQueue ) - 1) )
    {
      /* Sleep for a frame interval */
      sleepTime.tv_sec = omx_camera_source_component_Private->nFrameIntervalInMilliSec / 1000;
      sleepTime.tv_nsec = (omx_camera_source_component_Private->nFrameIntervalInMilliSec % 1000) * 1000000;
      nanosleep(&sleepTime, NULL);
    }
  }

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_STREAMON, &type)) {
    DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_STREAMON", errno, strerror(errno));
    err = OMX_ErrorHardware;
    goto EXIT;
  }


EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Stop the camera device */
static OMX_ERRORTYPE camera_StopCameraDevice(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  OMX_U32 i = 0;
  omx_camera_source_component_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int ioErr = 0;
  enum v4l2_buf_type type;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  /* Wait for un-processed buffers */
  i = OMX_MAPBUFQUEUE_GETNEXTWAIT( omx_camera_source_component_Private->sMapbufQueue );

  while ( OMX_MAPBUFQUEUE_HASBUFWAITTOCAPTURE( omx_camera_source_component_Private->sMapbufQueue ) ) {
    /* Wait the camera hardware to finish capturing */
    DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Wait on buffer [%d], [width, height] = [%d, %d], pixelformat = %d\n",__func__,(int)i,
      omx_camera_source_component_Private->fmt.fmt.pix.width,
      omx_camera_source_component_Private->fmt.fmt.pix.height,
      omx_camera_source_component_Private->fmt.fmt.pix.pixelformat);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    do {
      ioErr = xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_STREAMOFF, &type);
    } while ( ioErr < 0 && EINTR == errno );
    if ( ioErr < 0 ) {
      DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Wait the camera hardware to finish capturing failed: %s\n",__func__,GET_SYSERR_STRING());
      err = OMX_ErrorHardware;
      goto EXIT;
    }

    OMX_MAPBUFQUEUE_ADDCAPTUREDBUF( omx_camera_source_component_Private->sMapbufQueue );

    i = OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue, i );
  }

  /* Reset Mapping Buffer Queue */
  OMX_MAPBUFQUEUE_MAKEEMPTY( omx_camera_source_component_Private->sMapbufQueue );

  /* Reset port mapbuf queue index */
  for ( i = 0; i < NUM_CAMERAPORTS; i++ ) {
    port = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[i];
    port->nIndexMapbufQueue = 0;
  }


EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}



/** The Constructor for camera source component
  * @param openmaxStandComp is the pointer to the OMX component
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_camera_source_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_S32 i;
  omx_camera_source_component_PortType *port;
  omx_camera_source_component_PrivateType* omx_camera_source_component_Private;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  /** Allocate camera component private structure */
  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_camera_source_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  /* Call base source constructor */
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  err = omx_base_source_Constructor(openmaxStandComp, cComponentName);

  /* Overwrite default settings by base source */
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = NUM_CAMERAPORTS;

  omx_camera_source_component_Private->BufferMgmtFunction = omx_camera_source_component_BufferMgmtFunction;

  /** Init camera private parameters by default values */
  pthread_mutex_init(&omx_camera_source_component_Private->idle_state_mutex, NULL);
  pthread_cond_init(&omx_camera_source_component_Private->idle_wait_condition, NULL);
  pthread_cond_init(&omx_camera_source_component_Private->idle_process_condition, NULL);
  omx_camera_source_component_Private->bWaitingOnIdle = OMX_FALSE;

  pthread_mutex_init(&omx_camera_source_component_Private->setconfig_mutex, NULL);

  setHeader(&omx_camera_source_component_Private->sSensorMode, sizeof(OMX_PARAM_SENSORMODETYPE));
  omx_camera_source_component_Private->sSensorMode.nPortIndex = 0;
  omx_camera_source_component_Private->sSensorMode.nFrameRate = DEFAULT_FRAME_RATE; /* default 15 pps */
  omx_camera_source_component_Private->sSensorMode.bOneShot = OMX_FALSE; /* default video capture */
  setHeader(&omx_camera_source_component_Private->sSensorMode.sFrameSize, sizeof(OMX_FRAMESIZETYPE));
  omx_camera_source_component_Private->sSensorMode.sFrameSize.nPortIndex = 0;
  omx_camera_source_component_Private->sSensorMode.sFrameSize.nWidth = DEFAULT_FRAME_WIDTH;
  omx_camera_source_component_Private->sSensorMode.sFrameSize.nHeight = DEFAULT_FRAME_HEIGHT;
  omx_camera_source_component_Private->nFrameIntervalInMilliSec = 1000 / (omx_camera_source_component_Private->sSensorMode.nFrameRate);

  omx_camera_source_component_Private->eOmxColorFormat = DEFAULT_COLOR_FORMAT;
  omx_camera_source_component_Private->sV4lColorFormat.v4l2Pixfmt = V4L2_PIX_FMT_YUV420;
  omx_camera_source_component_Private->sV4lColorFormat.v4l2Depth = 12;

  omx_camera_source_component_Private->fdCam = -1;
  memset(&omx_camera_source_component_Private->sMapbufQueue,0, sizeof(OMX_V4L2_MAPBUFFER_QUEUETYPE));
  omx_camera_source_component_Private->sMapbufQueue.nFrame = 0;
  omx_camera_source_component_Private->bCapturing = OMX_FALSE;
  omx_camera_source_component_Private->bCapturingNext = OMX_FALSE;
  omx_camera_source_component_Private->bIsFirstFrame = OMX_FALSE;
  omx_camera_source_component_Private->bAutoPause = OMX_FALSE;
  omx_camera_source_component_Private->bThumbnailStart = OMX_FALSE;
  omx_camera_source_component_Private->nCapturedCount = 0;


  /** Allocate Ports. */
  if (omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_camera_source_component_Private->ports) {
    omx_camera_source_component_Private->ports = calloc(omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof (omx_base_PortType *));
    if (!omx_camera_source_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      /** this is the important thing separating this from the base class; size of the struct is for derived class port type
        * this could be refactored as a smarter factory function instead?
        */
      omx_camera_source_component_Private->ports[i] = calloc(1, sizeof(omx_camera_source_component_PortType));
      if (!omx_camera_source_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  for (i=0; i<omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
    /** Call base port constructor */
    base_video_port_Constructor(openmaxStandComp, &omx_camera_source_component_Private->ports[i], i, OMX_FALSE);
    port = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[i];
    /** Init port parameters by default values */
    port->sPortParam.nBufferSize = DEFAULT_FRAME_WIDTH*DEFAULT_FRAME_HEIGHT*3/2;
    port->sPortParam.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    port->sPortParam.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    port->sPortParam.format.video.nStride = DEFAULT_FRAME_WIDTH;
    port->sPortParam.format.video.nSliceHeight = DEFAULT_FRAME_HEIGHT;
    port->sPortParam.format.video.xFramerate = DEFAULT_FRAME_RATE;
    port->sPortParam.format.video.eColorFormat = DEFAULT_COLOR_FORMAT;
    port->nIndexMapbufQueue = 0;
  }

  /** set the function pointers */
  omx_camera_source_component_Private->DoStateSet = &omx_camera_source_component_DoStateSet;
  omx_camera_source_component_Private->destructor = omx_camera_source_component_Destructor;
  openmaxStandComp->SetParameter = omx_camera_source_component_SetParameter;
  openmaxStandComp->GetParameter = omx_camera_source_component_GetParameter;
  openmaxStandComp->SetConfig = omx_camera_source_component_SetConfig;
  openmaxStandComp->GetConfig = omx_camera_source_component_GetConfig;

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/** The Destructor for camera source component
  * @param openmaxStandComp is the pointer to the OMX component
  */
OMX_ERRORTYPE omx_camera_source_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_camera_source_component_PrivateType *omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  /** frees the port structures*/
  if (omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && omx_camera_source_component_Private->ports) {
    for (i = 0; i < omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      if(omx_camera_source_component_Private->ports[i]) {
        base_port_Destructor(omx_camera_source_component_Private->ports[i]);
      }
    }
    free(omx_camera_source_component_Private->ports);
    omx_camera_source_component_Private->ports = NULL;
  }

  pthread_mutex_destroy(&omx_camera_source_component_Private->idle_state_mutex);
  pthread_cond_destroy(&omx_camera_source_component_Private->idle_wait_condition);
  pthread_cond_destroy(&omx_camera_source_component_Private->idle_process_condition);

  pthread_mutex_destroy(&omx_camera_source_component_Private->setconfig_mutex);

  camera_DeinitCameraDevice(omx_camera_source_component_Private);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, OMX_ErrorNone);
  return omx_base_source_Destructor(openmaxStandComp);;
}

/** Specific operations for camera component when state changes
 * @param openmaxStandComp the OpenMAX component which state is to be changed
 * @param destinationState the requested target state
 */
static OMX_ERRORTYPE omx_camera_source_component_DoStateSet(OMX_COMPONENTTYPE *openmaxStandComp, OMX_U32 destinationState) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_camera_source_component_PrivateType *omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Before base DoStateSet: destinationState=%ld,omx_camera_source_component_Private->state=%d,omx_camera_source_component_Private->transientState=%d\n", __func__,destinationState,omx_camera_source_component_Private->state,omx_camera_source_component_Private->transientState);

  if (omx_camera_source_component_Private->state == OMX_StateLoaded && destinationState == OMX_StateIdle) {
    /* Loaded --> Idle */
    if ((err = camera_InitCameraDevice(omx_camera_source_component_Private)) != OMX_ErrorNone) {
      goto EXIT;
    }
  }
  else if (omx_camera_source_component_Private->state == OMX_StateIdle && destinationState == OMX_StateExecuting) {
    /* Idle --> Exec */
    pthread_mutex_lock(&omx_camera_source_component_Private->idle_state_mutex);
    if (!omx_camera_source_component_Private->bWaitingOnIdle) {
      pthread_cond_wait(&omx_camera_source_component_Private->idle_process_condition,&omx_camera_source_component_Private->idle_state_mutex);
    }
    pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
  }
  else if (omx_camera_source_component_Private->state == OMX_StateIdle && destinationState == OMX_StateLoaded) {
    /* Idle --> Loaded*/
    pthread_mutex_lock(&omx_camera_source_component_Private->idle_state_mutex);
    if (!omx_camera_source_component_Private->bWaitingOnIdle) {
      pthread_cond_wait(&omx_camera_source_component_Private->idle_process_condition,&omx_camera_source_component_Private->idle_state_mutex);
    }
    camera_DeinitCameraDevice(omx_camera_source_component_Private);
    if (omx_camera_source_component_Private->bWaitingOnIdle) {
      pthread_cond_signal(&omx_camera_source_component_Private->idle_wait_condition);
    }
    pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
  }


  omx_camera_source_component_Private->eLastState = omx_camera_source_component_Private->state;
  err = omx_base_component_DoStateSet(openmaxStandComp, destinationState);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: After base DoStateSet: destinationState=%ld,omx_camera_source_component_Private->state=%d,omx_camera_source_component_Private->transientState=%d\n", __func__,destinationState,omx_camera_source_component_Private->state,omx_camera_source_component_Private->transientState);


  if (omx_camera_source_component_Private->eLastState == OMX_StateIdle && omx_camera_source_component_Private->state == OMX_StateExecuting) {
    /* Idle --> Exec */
    pthread_mutex_lock(&omx_camera_source_component_Private->idle_state_mutex);
    if (omx_camera_source_component_Private->bWaitingOnIdle) {
      if ((err = camera_StartCameraDevice(omx_camera_source_component_Private)) != OMX_ErrorNone) {
        pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
        goto EXIT;
      }
      pthread_cond_signal(&omx_camera_source_component_Private->idle_wait_condition);
    }
    pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
  }
  else if (omx_camera_source_component_Private->eLastState == OMX_StateExecuting && omx_camera_source_component_Private->state == OMX_StateIdle) {
    /* Exec --> Idle */
    pthread_mutex_lock(&omx_camera_source_component_Private->idle_state_mutex);
    if (!omx_camera_source_component_Private->bWaitingOnIdle) {
      pthread_cond_wait(&omx_camera_source_component_Private->idle_process_condition,&omx_camera_source_component_Private->idle_state_mutex);
    }
    camera_StopCameraDevice(omx_camera_source_component_Private);
    pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
  }


EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}


/** The GetParameter method for camera source component
  * @param hComponent input parameter, the handle of V4L2 camera component
  * @param nParamIndex input parameter, the index of the structure to be filled
  * @param ComponentParameterStructure inout parameter, a pointer to the structure that receives parameters
  */
static OMX_ERRORTYPE omx_camera_source_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp;
  omx_camera_source_component_PrivateType* omx_camera_source_component_Private;
  omx_camera_source_component_PortType *pPort;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_PARAM_SENSORMODETYPE *pSensorMode;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Getting parameter %i\n", __func__, nParamIndex);

  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      memcpy(ComponentParameterStructure,&omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo],sizeof(OMX_PORT_PARAM_TYPE));
      break;

    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      if (pVideoPortFormat->nPortIndex < NUM_CAMERAPORTS)
      {
        pPort = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[pVideoPortFormat->nPortIndex];
        pVideoPortFormat->eCompressionFormat = pPort->sPortParam.format.video.eCompressionFormat;
        pVideoPortFormat->eColorFormat = pPort->sPortParam.format.video.eColorFormat;
      }
      else
      {
        err = OMX_ErrorBadPortIndex;
      }
      break;

    case OMX_IndexParamCommonSensorMode:
      pSensorMode = (OMX_PARAM_SENSORMODETYPE *)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_SENSORMODETYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      memcpy(pSensorMode, &omx_camera_source_component_Private->sSensorMode, sizeof(OMX_PARAM_SENSORMODETYPE));
      break;

    default: /*Call the base component function*/
      err = omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
      break;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/** The SetParameter method for camera source component
  * @param hComponent input parameter, the handle of V4L2 camera component
  * @param nParamIndex input parameter, the index of the structure to be set
  * @param ComponentParameterStructure input parameter, a pointer to the parameter structure
  */
static OMX_ERRORTYPE omx_camera_source_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp;
  omx_camera_source_component_PrivateType* omx_camera_source_component_Private;
  omx_camera_source_component_PortType *pPort;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_PARAM_SENSORMODETYPE *pSensorMode;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Setting parameter %i\n", __func__, nParamIndex);

  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      memcpy(&omx_camera_source_component_Private->sPortTypesParam[OMX_PortDomainVideo],ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
      break;

    case OMX_IndexParamPortDefinition:
      pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *) ComponentParameterStructure;
      err = camera_CheckSupportedColorFormat(pPortDef->format.video.eColorFormat);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Supported Color Format Check failed!\n", __func__, __LINE__);
        break;
      }
      err = camera_CheckSupportedFramesize(pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Supported Frame Size Check failed!\n", __func__, __LINE__);
        break;
      }
      /*Call the base component function*/
      err = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Call base SetParameter failed!\n", __func__, __LINE__);
        break;
      }
      pPort = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[pPortDef->nPortIndex];
      memcpy(&pPort->sPortParam, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      pPort->sPortParam.nBufferSize = camera_CalculateBufferSize(pPort->sPortParam.format.video.nFrameWidth, pPort->sPortParam.format.video.nFrameHeight, pPort->sPortParam.format.video.eColorFormat);
      break;

    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
      err = omx_base_component_ParameterSanityCheck(hComponent, pVideoPortFormat->nPortIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Parameter Sanity Check failed!\n", __func__, __LINE__);
        break;
      }
      err = camera_CheckSupportedColorFormat(pVideoPortFormat->eColorFormat);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Supported Color Format Check failed!\n", __func__, __LINE__);
        break;
      }
      pPort = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[pVideoPortFormat->nPortIndex];
      pPort->sPortParam.format.video.eCompressionFormat = pVideoPortFormat->eCompressionFormat;
      pPort->sPortParam.format.video.eColorFormat = pVideoPortFormat->eColorFormat;
      break;

    case OMX_IndexParamCommonSensorMode:
      pSensorMode = (OMX_PARAM_SENSORMODETYPE *)ComponentParameterStructure;
      err = omx_base_component_ParameterSanityCheck(hComponent, pSensorMode->nPortIndex, pSensorMode, sizeof(OMX_PARAM_SENSORMODETYPE));
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Parameter Sanity Check failed!\n", __func__, __LINE__);
        break;
      }
      err = camera_CheckSupportedFramesize(pSensorMode->sFrameSize.nWidth, pSensorMode->sFrameSize.nHeight);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Supported Frame Size Check failed!\n", __func__, __LINE__);
        break;
      }
      memcpy(&omx_camera_source_component_Private->sSensorMode, pSensorMode, sizeof(OMX_PARAM_SENSORMODETYPE));
      omx_camera_source_component_Private->nFrameIntervalInMilliSec = 1000 / (pSensorMode->nFrameRate);
      break;

    default: /*Call the base component function*/
      err = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
      break;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/** The GetConfig method for camera source component
  * @param hComponent input parameter, the handle of V4L2 camera component
  * @param nConfigIndex input parameter, the index of the structure to be filled
  * @param pComponentConfigStructure inout parameter, a pointer to the structure that receives configurations
  */
static OMX_ERRORTYPE omx_camera_source_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nConfigIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp;
  omx_camera_source_component_PrivateType* omx_camera_source_component_Private;
  OMX_CONFIG_BOOLEANTYPE *pCapturing;
  OMX_CONFIG_BOOLEANTYPE *pAutoPause;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Getting configuration %i\n", __func__, nConfigIndex);

  switch (nConfigIndex) {
    case OMX_IndexConfigCapturing:
      pCapturing = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_BOOLEANTYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      pCapturing->bEnabled = omx_camera_source_component_Private->bCapturingNext;
      break;
    case OMX_IndexAutoPauseAfterCapture:
      pAutoPause = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_BOOLEANTYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      pAutoPause->bEnabled = omx_camera_source_component_Private->bAutoPause;
      break;
    default:
      err = omx_base_component_GetConfig(hComponent, nConfigIndex, pComponentConfigStructure);
      break;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/** The SetConfig method for camera source component
  * @param hComponent input parameter, the handle of V4L2 camera component
  * @param nConfigIndex input parameter, the index of the structure to be set
  * @param pComponentConfigStructure input parameter, a pointer to the configuration structure
  */
static OMX_ERRORTYPE omx_camera_source_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nConfigIndex,
  OMX_IN OMX_PTR pComponentConfigStructure) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp;
  omx_camera_source_component_PrivateType* omx_camera_source_component_Private;
  OMX_CONFIG_BOOLEANTYPE *pCapturing;
  OMX_CONFIG_BOOLEANTYPE *pAutoPause;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_camera_source_component_Private = (omx_camera_source_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Setting configuration %i\n", __func__, nConfigIndex);

  switch (nConfigIndex) {
    case OMX_IndexConfigCapturing:
      pCapturing = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_BOOLEANTYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      err = camera_SetConfigCapturing( omx_camera_source_component_Private, pCapturing );
      break;
    case OMX_IndexAutoPauseAfterCapture:
      pAutoPause = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_BOOLEANTYPE))) != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "%s (line %d): Check header failed!\n", __func__, __LINE__);
        break;
      }
      pthread_mutex_lock(&omx_camera_source_component_Private->setconfig_mutex);
      omx_camera_source_component_Private->bAutoPause = pAutoPause->bEnabled;
      pthread_mutex_unlock(&omx_camera_source_component_Private->setconfig_mutex);
      break;
    default:
      err = omx_base_component_SetConfig(hComponent, nConfigIndex, pComponentConfigStructure);
      break;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/** This is the central function for buffer processing.
  * It is executed in a separate thread.
  * @param param input parameter, a pointer to the OMX standard structure
  */
static void* omx_camera_source_component_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)param;
  omx_camera_source_component_PrivateType *omx_camera_source_component_Private = (omx_camera_source_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_camera_source_component_PortType *pPreviewPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_VF];
  omx_camera_source_component_PortType *pCapturePort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
  omx_camera_source_component_PortType *pThumbnailPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP_T];
  tsem_t* pPreviewBufSem = pPreviewPort->pBufferSem;
  tsem_t* pCaptureBufSem = pCapturePort->pBufferSem;
  tsem_t* pThumbnailBufSem = pThumbnailPort->pBufferSem;
  OMX_BUFFERHEADERTYPE* pPreviewBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pCaptureBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pThumbnailBuffer=NULL;


  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  while(omx_camera_source_component_Private->state == OMX_StateIdle ||
            omx_camera_source_component_Private->state == OMX_StateExecuting ||
            omx_camera_source_component_Private->state == OMX_StatePause ||
            omx_camera_source_component_Private->transientState == OMX_TransStateLoadedToIdle) {

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_camera_source_component_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pPreviewPort) ||
           PORT_IS_BEING_FLUSHED(pCapturePort) ||
           PORT_IS_BEING_FLUSHED(pThumbnailPort)) {
      pthread_mutex_unlock(&omx_camera_source_component_Private->flush_mutex);

      DEBUG(DEB_LEV_FULL_SEQ, "In %s 1 signalling flush all cond PrevewSemVal=%d,CaptureSemval=%d, ThumbnailSemval=%d\n",
        __func__,pPreviewBufSem->semval,pCaptureBufSem->semval, pThumbnailBufSem->semval);
      DEBUG(DEB_LEV_FULL_SEQ, "In %s 1 signalling flush all cond pPreviewBuffer=0x%lX,pCaptureBuffer=0x%lX, pThumbnailBuffer=0x%lX\n",
        __func__,(OMX_U32)pPreviewBuffer,(OMX_U32)pCaptureBuffer, (OMX_U32)pThumbnailBuffer);

      if(pPreviewBuffer!=NULL && PORT_IS_BEING_FLUSHED(pPreviewPort)) {
        pPreviewPort->ReturnBufferFunction((omx_base_PortType *)pPreviewPort,pPreviewBuffer);
        pPreviewBuffer=NULL;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning Preview buffer\n");
      }

      if(pCaptureBuffer!=NULL && PORT_IS_BEING_FLUSHED(pCapturePort)) {
        pCapturePort->ReturnBufferFunction((omx_base_PortType *)pCapturePort,pCaptureBuffer);
        pCaptureBuffer=NULL;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning Capture buffer\n");
      }

      if(pThumbnailBuffer!=NULL && PORT_IS_BEING_FLUSHED(pThumbnailPort)) {
        pThumbnailPort->ReturnBufferFunction((omx_base_PortType *)pThumbnailPort,pThumbnailBuffer);
        pThumbnailBuffer=NULL;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning Thumbnail buffer\n");
      }

      DEBUG(DEB_LEV_FULL_SEQ, "In %s 2 signalling flush all cond PrevewSemVal=%d,CaptureSemval=%d, ThumbnailSemval=%d\n",
        __func__,pPreviewBufSem->semval,pCaptureBufSem->semval, pThumbnailBufSem->semval);
      DEBUG(DEB_LEV_FULL_SEQ, "In %s 2 signalling flush all cond pPreviewBuffer=0x%lX,pCaptureBuffer=0x%lX, pThumbnailBuffer=0x%lX\n",
        __func__,(OMX_U32)pPreviewBuffer,(OMX_U32)pCaptureBuffer, (OMX_U32)pThumbnailBuffer);

      tsem_up(omx_camera_source_component_Private->flush_all_condition);
      tsem_down(omx_camera_source_component_Private->flush_condition);
      pthread_mutex_lock(&omx_camera_source_component_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_camera_source_component_Private->flush_mutex);

    pthread_mutex_lock(&omx_camera_source_component_Private->setconfig_mutex);
    if (!omx_camera_source_component_Private->bCapturing && omx_camera_source_component_Private->bCapturingNext) {
      pCapturePort->nIndexMapbufQueue = OMX_MAPBUFQUEUE_GETLASTBUFFER(omx_camera_source_component_Private->sMapbufQueue);
    }
    else if (omx_camera_source_component_Private->bCapturing && !omx_camera_source_component_Private->bCapturingNext) {
      omx_camera_source_component_Private->nCapturedCount = 0;
    }
    omx_camera_source_component_Private->bCapturing = omx_camera_source_component_Private->bCapturingNext;
    pthread_mutex_unlock(&omx_camera_source_component_Private->setconfig_mutex);

    if(omx_camera_source_component_Private->state==OMX_StatePause &&
        !(PORT_IS_BEING_FLUSHED(pPreviewPort) || PORT_IS_BEING_FLUSHED(pCapturePort) || PORT_IS_BEING_FLUSHED(pThumbnailPort))) {
      /*Waiting at paused state*/
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait at State %d\n", __func__, omx_camera_source_component_Private->state);
      tsem_wait(omx_camera_source_component_Private->bStateSem);
    }

    pthread_mutex_lock(&omx_camera_source_component_Private->idle_state_mutex);
    if(omx_camera_source_component_Private->state==OMX_StateIdle &&
        !(PORT_IS_BEING_FLUSHED(pPreviewPort) || PORT_IS_BEING_FLUSHED(pCapturePort) || PORT_IS_BEING_FLUSHED(pThumbnailPort))) {
      /*Waiting at idle state*/
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait at State %d\n", __func__, omx_camera_source_component_Private->state);
      omx_camera_source_component_Private->bWaitingOnIdle = OMX_TRUE;
      pthread_cond_signal(&omx_camera_source_component_Private->idle_process_condition);
      pthread_cond_wait(&omx_camera_source_component_Private->idle_wait_condition,&omx_camera_source_component_Private->idle_state_mutex);
      if(omx_camera_source_component_Private->transientState == OMX_TransStateIdleToLoaded) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
        pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);
        break;
      }
    }
    omx_camera_source_component_Private->bWaitingOnIdle = OMX_FALSE;
    pthread_mutex_unlock(&omx_camera_source_component_Private->idle_state_mutex);

    /* After cemera does start, capture video data from camera */
    camera_HandleThreadBufferCapture( omx_camera_source_component_Private );

  }


  DEBUG(DEB_LEV_FUNCTION_NAME, "Exiting Buffer Management Thread: %s\n",__func__);
  return NULL;
}

/* Buffer capture routine for the buffer management thread */
static OMX_ERRORTYPE camera_HandleThreadBufferCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  OMX_U32 nCurTimeInMilliSec;
  OMX_S32 nTimeToWaitInMilliSec;
  struct timeval now;
  struct timespec sleepTime;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  struct v4l2_buffer buf;

  CLEAR(buf);


  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  DEBUG(DEB_LEV_FULL_SEQ, "%s: mapbuf queue index [last, wait, capture] = [%ld, %ld, %ld]\n", __func__,
              omx_camera_source_component_Private->sMapbufQueue.nLastBufIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextWaitIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextCaptureIndex );

  DEBUG(DEB_LEV_FULL_SEQ, "%s: mapbuf queue count [captured, total] = [%ld, %ld]\n", __func__,
              omx_camera_source_component_Private->sMapbufQueue.nBufCountCaptured,
              omx_camera_source_component_Private->sMapbufQueue.nBufCountTotal );

  /* Wait to sync buffer */
  if ( OMX_MAPBUFQUEUE_HASBUFWAITTOCAPTURE( omx_camera_source_component_Private->sMapbufQueue ) ) {

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_DQBUF, &buf)) {
      switch (errno) {
      case EAGAIN:
        return OMX_ErrorHardware;;
      case EIO:
        /* Could ignore EIO, see spec. */
        /* fall through */

      default:
        DEBUG(DEB_LEV_ERR,"In %s error VIDIOC_DQBUF\n",__func__);
        return OMX_ErrorHardware;;
      }
    }


    /* Generate time stamp for the new captured buffer */
    if ((err = camera_GenerateTimeStamp(omx_camera_source_component_Private)) != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Generate time stamp failed!\n",__func__);
      goto EXIT;
    }

    OMX_MAPBUFQUEUE_ADDCAPTUREDBUF( omx_camera_source_component_Private->sMapbufQueue );
  }

  /* Try to send buffers */
  if ( OMX_MAPBUFQUEUE_HASBUFCAPTURED( omx_camera_source_component_Private->sMapbufQueue ) ) {
    camera_SendCapturedBuffers( omx_camera_source_component_Private );
  }

  /* Calculate waiting time */
  gettimeofday(&now, NULL);
  nCurTimeInMilliSec = ((OMX_U32)now.tv_sec) * 1000 + ((OMX_U32)now.tv_usec) / 1000;
  nTimeToWaitInMilliSec = (OMX_S32) (omx_camera_source_component_Private->nFrameIntervalInMilliSec -
                (nCurTimeInMilliSec - omx_camera_source_component_Private->nLastCaptureTimeInMilliSec));

  DEBUG(DEB_LEV_FULL_SEQ, "%s: [current time, last capture time, time to wait]=[%lu, %lu, %ld]\n", __func__,
                nCurTimeInMilliSec,omx_camera_source_component_Private->nLastCaptureTimeInMilliSec,nTimeToWaitInMilliSec);

  /* Wait some time according to frame rate */
  if ( nTimeToWaitInMilliSec > 0 ) {
    sleepTime.tv_sec = nTimeToWaitInMilliSec / 1000;
    sleepTime.tv_nsec = (nTimeToWaitInMilliSec % 1000) * 1000000;
    DEBUG(DEB_LEV_FULL_SEQ, "%s: Actually wait for %ld msec\n", __func__,
                nTimeToWaitInMilliSec);
    nanosleep(&sleepTime, NULL);
  }

  /* record last capture time */
  gettimeofday(&now, NULL);
  omx_camera_source_component_Private->nLastCaptureTimeInMilliSec = ((OMX_U32)now.tv_sec) * 1000 + ((OMX_U32)now.tv_usec) / 1000;

  if ( OMX_MAPBUFQUEUE_GETBUFCOUNTCAPTURED( omx_camera_source_component_Private->sMapbufQueue ) >= OMX_MAPBUFQUEUE_GETMAXLEN( omx_camera_source_component_Private->sMapbufQueue ) / 2 &&
        OMX_MAPBUFQUEUE_ISFULL( omx_camera_source_component_Private->sMapbufQueue ) ) {
    /* Try to send otherwise drop the last captured buffer */
    camera_SendLastCapturedBuffer( omx_camera_source_component_Private );
    if ( OMX_MAPBUFQUEUE_ISFULL( omx_camera_source_component_Private->sMapbufQueue ) ) {
      /* If the mapbuf queue is still full, drop the last captured buffer */
      DEBUG(DEB_LEV_FULL_SEQ, "%s: Drop buffer [%ld]\n", __func__,
                OMX_MAPBUFQUEUE_GETLASTBUFFER( omx_camera_source_component_Private->sMapbufQueue ));
      camera_DropLastCapturedBuffer( omx_camera_source_component_Private );
    }
  }

  /* Start to capture the next buffer */
  if ( !OMX_MAPBUFQUEUE_ISFULL( omx_camera_source_component_Private->sMapbufQueue ) ) {

    if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_QBUF, &buf)) {
      DEBUG(DEB_LEV_ERR,"In %s error VIDIOC_DQBUF\n",__func__);
      err = OMX_ErrorHardware;
    }

    OMX_MAPBUFQUEUE_ENQUEUE( omx_camera_source_component_Private->sMapbufQueue );
  }

EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Generate time stamp for the new captured buffer */
static OMX_ERRORTYPE camera_GenerateTimeStamp(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferIndex;
  struct timeval now;
  OMX_TICKS nCurrentWallTime;
  OMX_TICKS nTimeStamp;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  nBufferIndex = OMX_MAPBUFQUEUE_GETNEXTWAIT(omx_camera_source_component_Private->sMapbufQueue);

  gettimeofday(&now, NULL);
  nCurrentWallTime = (OMX_TICKS)(now.tv_sec * 1000000 + now.tv_usec);

  /* To protect nRefWallTime */
  pthread_mutex_lock(&omx_camera_source_component_Private->setconfig_mutex);
  nTimeStamp = nCurrentWallTime - omx_camera_source_component_Private->nRefWallTime;
  pthread_mutex_unlock(&omx_camera_source_component_Private->setconfig_mutex);

  OMX_MAPBUFQUEUE_SETTIMESTAMP(omx_camera_source_component_Private->sMapbufQueue, nBufferIndex, nTimeStamp);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Try to send captured buffers in mapbuf queue to each port.
 * Note: In this function, multiple buffers may be sent.
 */
static OMX_ERRORTYPE camera_SendCapturedBuffers(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *port;
  OMX_U32 nBufferCountCur = 0;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_S32 nPortIndex;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  for (nPortIndex = OMX_CAMPORT_INDEX_CP; nPortIndex >= OMX_CAMPORT_INDEX_VF; nPortIndex--) {
    DEBUG(DEB_LEV_FULL_SEQ, "%s: try to send buffers to port [%ld]\n", __func__, nPortIndex);
    port = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[nPortIndex];
    if (PORT_IS_ENABLED(port) &&
         (omx_camera_source_component_Private->bCapturing || OMX_CAMPORT_INDEX_CP != nPortIndex)) {
      nBufferCountCur = port->pBufferSem->semval;
      DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nBufferCountCur = %ld\n", __func__, nPortIndex, nBufferCountCur);
      DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nIndexMapbufQueue = %ld\n", __func__, nPortIndex, port->nIndexMapbufQueue);
      DEBUG(DEB_LEV_FULL_SEQ, "%s: Before returning buffers, mapbuf queue index [last, wait, capture] = [%ld, %ld, %ld]\n", __func__,
                  omx_camera_source_component_Private->sMapbufQueue.nLastBufIndex,
                  omx_camera_source_component_Private->sMapbufQueue.nNextWaitIndex,
                  omx_camera_source_component_Private->sMapbufQueue.nNextCaptureIndex );

      while(nBufferCountCur > 0 &&
                (port->nIndexMapbufQueue != OMX_MAPBUFQUEUE_GETNEXTWAIT( omx_camera_source_component_Private->sMapbufQueue ) ||
                  (OMX_MAPBUFQUEUE_ISFULL( omx_camera_source_component_Private->sMapbufQueue ) &&
                   OMX_MAPBUFQUEUE_NOBUFWAITTOCAPTURE( omx_camera_source_component_Private->sMapbufQueue)
                  )
                )
              ) {
        DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nBufferCountCur = %ld\n", __func__, nPortIndex, nBufferCountCur);
        camera_ProcessPortOneBuffer( omx_camera_source_component_Private, (OMX_U32) nPortIndex );

        port->nIndexMapbufQueue = OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue,
                                                     port->nIndexMapbufQueue );
        nBufferCountCur--;
      }
    }
 }

  err = camera_UpdateCapturedBufferQueue( omx_camera_source_component_Private );

  DEBUG(DEB_LEV_FULL_SEQ, "%s: After returning buffers, mapbuf queue index [last, wait, capture] = [%ld, %ld, %ld]\n", __func__,
              omx_camera_source_component_Private->sMapbufQueue.nLastBufIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextWaitIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextCaptureIndex );

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Try to send the last captured buffer in mapbuf queue to each port.
 * Note: In this function, only ONE buffer be sent.
 */
static OMX_ERRORTYPE camera_SendLastCapturedBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *port;
  OMX_U32 nBufferCountCur = 0;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_S32 nPortIndex;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  for (nPortIndex = OMX_CAMPORT_INDEX_CP; nPortIndex >= OMX_CAMPORT_INDEX_VF; nPortIndex--) {
    DEBUG(DEB_LEV_FULL_SEQ, "%s: try to send buffers to port [%ld]\n", __func__, nPortIndex);
    port = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[nPortIndex];
    if (PORT_IS_ENABLED(port) &&
         (omx_camera_source_component_Private->bCapturing || OMX_CAMPORT_INDEX_CP != nPortIndex)) {
      nBufferCountCur = port->pBufferSem->semval;
      DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nBufferCountCur = %ld\n", __func__, nPortIndex, nBufferCountCur);
      DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nIndexMapbufQueue = %ld\n", __func__, nPortIndex, port->nIndexMapbufQueue);
      DEBUG(DEB_LEV_FULL_SEQ, "%s: Before returning buffers, mapbuf queue index [last, wait, capture] = [%ld, %ld, %ld]\n", __func__,
                  omx_camera_source_component_Private->sMapbufQueue.nLastBufIndex,
                  omx_camera_source_component_Private->sMapbufQueue.nNextWaitIndex,
                  omx_camera_source_component_Private->sMapbufQueue.nNextCaptureIndex );

      if(nBufferCountCur > 0 &&
          port->nIndexMapbufQueue == OMX_MAPBUFQUEUE_GETLASTBUFFER( omx_camera_source_component_Private->sMapbufQueue)) {
        DEBUG(DEB_LEV_FULL_SEQ, "%s: port [%ld] nBufferCountCur = %ld\n", __func__, nPortIndex, nBufferCountCur);
        camera_ProcessPortOneBuffer( omx_camera_source_component_Private, (OMX_U32) nPortIndex );

        port->nIndexMapbufQueue = OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue,
                                                     port->nIndexMapbufQueue );
      }
    }
 }

  err = camera_UpdateCapturedBufferQueue( omx_camera_source_component_Private );

  DEBUG(DEB_LEV_FULL_SEQ, "%s: After returning buffers, mapbuf queue index [last, wait, capture] = [%ld, %ld, %ld]\n", __func__,
              omx_camera_source_component_Private->sMapbufQueue.nLastBufIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextWaitIndex,
              omx_camera_source_component_Private->sMapbufQueue.nNextCaptureIndex );

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}


/* Update captured buffer queue in mapbuf queue */
static OMX_ERRORTYPE camera_UpdateCapturedBufferQueue(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *pPreviewPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_VF];
  omx_camera_source_component_PortType *pCapturePort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  while ( OMX_MAPBUFQUEUE_HASBUFCAPTURED( omx_camera_source_component_Private->sMapbufQueue ) ) {
    if (PORT_IS_ENABLED(pPreviewPort) &&
         pPreviewPort->nIndexMapbufQueue == OMX_MAPBUFQUEUE_GETLASTBUFFER( omx_camera_source_component_Private->sMapbufQueue ) ) {
      break;
    }

    if (PORT_IS_ENABLED(pCapturePort) && omx_camera_source_component_Private->bCapturing &&
         pCapturePort->nIndexMapbufQueue == OMX_MAPBUFQUEUE_GETLASTBUFFER( omx_camera_source_component_Private->sMapbufQueue ) ) {
      break;
    }

    OMX_MAPBUFQUEUE_DEQUEUE( omx_camera_source_component_Private->sMapbufQueue );
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Process one buffer on the specified port */
static OMX_ERRORTYPE camera_ProcessPortOneBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,  OMX_IN OMX_U32 nPortIndex ) {
  omx_camera_source_component_PortType *port = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[nPortIndex];
  OMX_BUFFERHEADERTYPE* pBufHeader = NULL;
  OMX_BOOL bStrideAlign = OMX_FALSE;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  /* If buffer queue is not empty */
  if (port->pBufferSem->semval > 0) {
    /* Dequeue a buffer from buffer queue */
    tsem_down(port->pBufferSem);
    pBufHeader = dequeue(port->pBufferQueue);
    if(pBufHeader == NULL){
      DEBUG(DEB_LEV_ERR, "%s: <ERROR> --Had NULL buffer from port [%ld]!!\n", __func__, nPortIndex);
      err = OMX_ErrorBadParameter;
      goto EXIT;
    }

    if ( OMX_CAMPORT_INDEX_CP == nPortIndex ) {
      if ( OMX_FALSE == omx_camera_source_component_Private->sSensorMode.bOneShot ) {
        /* Video capture use case */
        if ((err = camera_AddTimeStamp(omx_camera_source_component_Private, pBufHeader)) != OMX_ErrorNone) {
          goto EXIT;
        }

        if ((err = camera_UpdateThumbnailCondition( omx_camera_source_component_Private )) != OMX_ErrorNone ) {
          goto EXIT;
        }
      }
      else {
        /* Still image capture use case */
        if ( (err = camera_HandleStillImageCapture( omx_camera_source_component_Private )) != OMX_ErrorNone ) {
          goto EXIT;
        }
      }

      if ( OMX_TRUE == omx_camera_source_component_Private->bThumbnailStart ) {
        /* Handle thumbnail image capture */
        if ( (err = camera_HandleThumbnailCapture( omx_camera_source_component_Private )) != OMX_ErrorNone ) {
          goto EXIT;
        }
      }
    }

    /* Translate color format and frame size */
    err = camera_ReformatVideoFrame( (OMX_PTR) OMX_MAPBUFQUEUE_GETBUFADDR(
                                                           omx_camera_source_component_Private->sMapbufQueue,
                                                           port->nIndexMapbufQueue  ),
                                                         omx_camera_source_component_Private->sSensorMode.sFrameSize.nWidth,
                                                         omx_camera_source_component_Private->sSensorMode.sFrameSize.nHeight,
                                                         omx_camera_source_component_Private->sV4lColorFormat,
                                                         (OMX_PTR) (pBufHeader->pBuffer + pBufHeader->nOffset),
                                                         port->sPortParam.format.video.nFrameWidth,
                                                         port->sPortParam.format.video.nFrameHeight,
                                                         port->sPortParam.format.video.nStride,
                                                         port->sPortParam.format.video.eColorFormat,
                                                         bStrideAlign );

    if ( err != OMX_ErrorNone ) {
      goto EXIT;
    }

    /* Return buffer */
    pBufHeader->nFilledLen = omx_camera_source_component_Private->oFrameSize;
    DEBUG(DEB_LEV_FULL_SEQ, "%s: return buffer [%ld] on port [%ld]: nFilledLen = %ld\n", __func__,
                port->nIndexMapbufQueue, nPortIndex, pBufHeader->nFilledLen);
    port->ReturnBufferFunction((omx_base_PortType *)port, pBufHeader);
  }

EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Drop the last captured buffer in mapbuf queue */
static OMX_ERRORTYPE camera_DropLastCapturedBuffer(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *port;
  OMX_U32 i = 0;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  for ( i = OMX_CAMPORT_INDEX_VF; i <= OMX_CAMPORT_INDEX_CP; i++ ) {
    port = (omx_camera_source_component_PortType *) omx_camera_source_component_Private->ports[i];
    if ( PORT_IS_ENABLED( port ) &&
         port->nIndexMapbufQueue == OMX_MAPBUFQUEUE_GETLASTBUFFER(
         omx_camera_source_component_Private->sMapbufQueue ) ) {
      if ( OMX_CAMPORT_INDEX_CP != i || omx_camera_source_component_Private->bCapturing ) {
        port->nIndexMapbufQueue = OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue,
                                                                            port->nIndexMapbufQueue );
      }
    }
  }

  OMX_MAPBUFQUEUE_DEQUEUE( omx_camera_source_component_Private->sMapbufQueue );

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Reformat one frame in terms of frame size and color format from source to destination address.
 * Note: This function is currently implemented as a simple memory copy, because the color conversion
 * and image resizing can be performed by a color conversion component that is tunneled with the
 * camera component.
 */
static OMX_ERRORTYPE camera_ReformatVideoFrame(
                                                    OMX_IN OMX_PTR pSrcFrameAddr,
                                                    OMX_IN OMX_U32 nSrcFrameWidth,
                                                    OMX_IN OMX_U32 nSrcFrameHeight,
                                                    OMX_IN V4L2_COLOR_FORMATTYPE sSrcV4l2ColorFormat,
                                                    OMX_IN OMX_PTR pDstFrameAddr,
                                                    OMX_IN OMX_U32 nDstFrameWidth,
                                                    OMX_IN OMX_U32 nDstFrameHeight,
                                                    OMX_IN OMX_S32 nDstFrameStride,
                                                    OMX_IN OMX_COLOR_FORMATTYPE eDstOmxColorFormat,
                                                    OMX_IN OMX_BOOL bStrideAlign ) {
  OMX_COLOR_FORMATTYPE eSrcOmxColorFormat;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if ( (err = camera_MapColorFormatV4lToOmx( &sSrcV4l2ColorFormat,
         &eSrcOmxColorFormat )) != OMX_ErrorNone ) {
    DEBUG(DEB_LEV_ERR, "%s: <ERROR> -- Unsupported V4L2 color format (palette, depth) = (%d, %d)\n",__func__,sSrcV4l2ColorFormat.v4l2Pixfmt, sSrcV4l2ColorFormat.v4l2Depth);
    goto EXIT;
  }

  DEBUG(DEB_LEV_FULL_SEQ, "%s: src (width, height, color)=(%ld,%ld,%d)\n", __func__, nSrcFrameWidth, nSrcFrameHeight, eSrcOmxColorFormat);
  DEBUG(DEB_LEV_FULL_SEQ, "%s: dst (width, height, stride, color)=(%ld,%ld,%ld,%d)\n", __func__, nDstFrameWidth, nDstFrameHeight, nDstFrameStride, eDstOmxColorFormat);

  /* Now the camera does not support color conversion and frame resize;
     To do this job, Pls resort to a color conversion component */
  if (nSrcFrameWidth != nDstFrameWidth ||
       nSrcFrameHeight != nDstFrameHeight ||
       eSrcOmxColorFormat != eDstOmxColorFormat) {
    err = OMX_ErrorUnsupportedSetting;
    goto EXIT;
  }

  memcpy(pDstFrameAddr, pSrcFrameAddr, camera_CalculateBufferSize(nSrcFrameWidth, nSrcFrameHeight, eSrcOmxColorFormat));

EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Add time stamp to a buffer header */
static OMX_ERRORTYPE camera_AddTimeStamp(
  OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private,
  OMX_IN OMX_BUFFERHEADERTYPE *pBufHeader) {
  omx_camera_source_component_PortType *pCapturePort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (omx_camera_source_component_Private->bIsFirstFrame ) {
    pBufHeader->nFlags = OMX_BUFFERFLAG_STARTTIME;
    omx_camera_source_component_Private->bIsFirstFrame = OMX_FALSE;
    DEBUG(DEB_LEV_SIMPLE_SEQ, "%s: Set StartTime Flag!\n",__func__);
  }
  else {
    pBufHeader->nFlags = 0;
  }

  pBufHeader->nTimeStamp = OMX_MAPBUFQUEUE_GETTIMESTAMP(omx_camera_source_component_Private->sMapbufQueue, pCapturePort->nIndexMapbufQueue);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Update the condition for thumbnail to occur */
static OMX_ERRORTYPE camera_UpdateThumbnailCondition(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *pThumbnailPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP_T];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if ( OMX_FALSE == omx_camera_source_component_Private->sSensorMode.bOneShot ) {
    if ( omx_camera_source_component_Private->nCapturedCount < (OMX_CAM_VC_SNAPSHOT_INDEX + 1) ) {
      omx_camera_source_component_Private->nCapturedCount++;
    }

    if ( PORT_IS_ENABLED(pThumbnailPort) &&
         OMX_CAM_VC_SNAPSHOT_INDEX == omx_camera_source_component_Private->nCapturedCount ) {
      omx_camera_source_component_Private->bThumbnailStart = OMX_TRUE;
    }
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Handle still image capture use case */
static OMX_ERRORTYPE camera_HandleStillImageCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *pThumbnailPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP_T];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  pthread_mutex_lock(&omx_camera_source_component_Private->setconfig_mutex);
  omx_camera_source_component_Private->bIsFirstFrame = OMX_FALSE;
  omx_camera_source_component_Private->bCapturingNext = OMX_FALSE;
  if ( PORT_IS_ENABLED(pThumbnailPort)  ) {
    omx_camera_source_component_Private->bThumbnailStart = OMX_TRUE;
  }

  if (omx_camera_source_component_Private->bAutoPause) {
    /* In autopause mode, command camera component to pause state */
    if ((err = omx_camera_source_component_DoStateSet(omx_camera_source_component_Private->openmaxStandComp,
                   (OMX_U32)OMX_StatePause)) != OMX_ErrorNone ) {
      goto EXIT;
    }
  }

EXIT:
  pthread_mutex_unlock(&omx_camera_source_component_Private->setconfig_mutex);
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

/* Handle thumbnail image capture */
static OMX_ERRORTYPE camera_HandleThumbnailCapture(OMX_IN omx_camera_source_component_PrivateType *omx_camera_source_component_Private) {
  omx_camera_source_component_PortType *pThumbnailPort = (omx_camera_source_component_PortType *)omx_camera_source_component_Private->ports[OMX_CAMPORT_INDEX_CP_T];
  OMX_BUFFERHEADERTYPE* pBufHeader = NULL;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BOOL bStrideAlign = OMX_FALSE;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  omx_camera_source_component_Private->bThumbnailStart = OMX_FALSE;

  /* If buffer queue on thumbnail port is not empty */
  if (pThumbnailPort->pBufferSem->semval > 0) {
    /* Dequeue a buffer from buffer queue */
    tsem_down(pThumbnailPort->pBufferSem);
    pBufHeader = dequeue(pThumbnailPort->pBufferQueue);
    if(pBufHeader == NULL){
      DEBUG(DEB_LEV_ERR, "%s: <ERROR> --Had NULL buffer from thumbnail port!!\n", __func__);
      err = OMX_ErrorBadParameter;
      goto EXIT;
    }

    /* Determine the buffer for thumbnail */
    while (OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue,
               pThumbnailPort->nIndexMapbufQueue ) != OMX_MAPBUFQUEUE_GETNEXTWAIT( omx_camera_source_component_Private->sMapbufQueue )
             ) {
      pThumbnailPort->nIndexMapbufQueue =
              OMX_MAPBUFQUEUE_GETNEXTINDEX( omx_camera_source_component_Private->sMapbufQueue,pThumbnailPort->nIndexMapbufQueue );
    }

    /* Translate color format and frame size */
    err = camera_ReformatVideoFrame( (OMX_PTR) OMX_MAPBUFQUEUE_GETBUFADDR(
                                                           omx_camera_source_component_Private->sMapbufQueue,
                                                           pThumbnailPort->nIndexMapbufQueue ),
                                                         omx_camera_source_component_Private->sSensorMode.sFrameSize.nWidth,
                                                         omx_camera_source_component_Private->sSensorMode.sFrameSize.nHeight,
                                                         omx_camera_source_component_Private->sV4lColorFormat,
                                                         (OMX_PTR) (pBufHeader->pBuffer + pBufHeader->nOffset),
                                                         pThumbnailPort->sPortParam.format.video.nFrameWidth,
                                                         pThumbnailPort->sPortParam.format.video.nFrameHeight,
                                                         pThumbnailPort->sPortParam.format.video.nStride,
                                                         pThumbnailPort->sPortParam.format.video.eColorFormat,
                                                         bStrideAlign );

    if ( err != OMX_ErrorNone ) {
      goto EXIT;
    }

    /* Return buffer */
    pBufHeader->nFilledLen = omx_camera_source_component_Private->oFrameSize;
    DEBUG(DEB_LEV_FULL_SEQ, "%s: return buffer [%ld] on thumbnail port: nFilledLen = %ld\n", __func__,
                pThumbnailPort->nIndexMapbufQueue, pBufHeader->nFilledLen);
    pThumbnailPort->ReturnBufferFunction((omx_base_PortType *)pThumbnailPort, pBufHeader);
  }

EXIT:
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}

static int camera_init_mmap(omx_camera_source_component_PrivateType* omx_camera_source_component_Private)
{
  struct v4l2_requestbuffers req;
  OMX_U32 i;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      DEBUG(DEB_LEV_ERR, "%s does not support "
        "memory mapping\n", V4L2DEV_FILENAME);
      return OMX_ErrorHardware;
    } else {
      DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_REQBUFS", errno, strerror(errno));
      return OMX_ErrorHardware;
    }
  }

  if (req.count < 2) {
    DEBUG(DEB_LEV_ERR, "Insufficient buffer memory on %s\n", V4L2DEV_FILENAME);
    return OMX_ErrorHardware;
  }

  omx_camera_source_component_Private->sMapbufQueue.nFrame = req.count;

  omx_camera_source_component_Private->sMapbufQueue.buffers = calloc(req.count, sizeof(*omx_camera_source_component_Private->sMapbufQueue.buffers));

  if (!omx_camera_source_component_Private->sMapbufQueue.buffers) {
    DEBUG(DEB_LEV_ERR,"Out of memory\n");
    return OMX_ErrorHardware;
  }

  for (i = 0; i < req.count; ++i) {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == xioctl(omx_camera_source_component_Private->fdCam, VIDIOC_QUERYBUF, &buf)) {
      DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_QUERYBUF", errno, strerror(errno));
      return OMX_ErrorHardware;
    }

    omx_camera_source_component_Private->sMapbufQueue.buffers[i].length = buf.length;
    omx_camera_source_component_Private->sMapbufQueue.buffers[i].pCapAddrStart = mmap(NULL /* start anywhere */ ,
            buf.length,
            PROT_READ | PROT_WRITE /* required */ ,
            MAP_SHARED /* recommended */ ,
            omx_camera_source_component_Private->fdCam, buf.m.offset);

    if (MAP_FAILED == omx_camera_source_component_Private->sMapbufQueue.buffers[i].pCapAddrStart) {
      DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "mmap", errno, strerror(errno));
      return OMX_ErrorHardware;
    }

    DEBUG(DEB_LEV_PARAMS, "i=%d,addr=%x,length=%d\n",(int)i,
      (int)omx_camera_source_component_Private->sMapbufQueue.buffers[i].pCapAddrStart,
      (int)omx_camera_source_component_Private->sMapbufQueue.buffers[i].length);
  }

  return OMX_ErrorNone;
}

