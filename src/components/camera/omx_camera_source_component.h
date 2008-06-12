/**
  @file src/components/camera/omx_camera_source_component.h
  
  OpenMax camera source component.
  The OpenMAX camera component is a V4L2-based video source whose functionalities
  include preview, video capture, image capture, video thumbnail and image
  thumbnail. It has 3 (output) ports: Port 0 is used for preview; Port 1 is used
  for video and image capture; Port 2 is used for video and image thumbnail.
  
  Copyright (C) 2007  Motorola

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

#ifndef _OMX_CAMERA_SOURCE_COMPONENT_H_
#define _OMX_CAMERA_SOURCE_COMPONENT_H_


#include <omx_base_source.h>
#include <omx_base_video_port.h>

#include <linux/videodev2.h>


/* Camera Component Port Index */
enum 
{
    OMX_CAMPORT_INDEX_VF = 0, /* preview/viewfinder */
    OMX_CAMPORT_INDEX_CP,       /* captured video */
    OMX_CAMPORT_INDEX_CP_T,   /* thumbnail or snapshot for captured video */
    OMX_CAMPORT_INDEX_MAX
};


#define NUM_CAMERAPORTS (OMX_CAMPORT_INDEX_MAX)

#define V4L2DEV_FILENAME  "/dev/video0" 



typedef __u32 V4L2_PICTURE_PIX_FMT;
typedef __u32 V4L2_PICTURE_DEPTHTYPE;

typedef struct V4L2_COLOR_FORMATTYPE
{
    V4L2_PICTURE_PIX_FMT v4l2Pixfmt;
    V4L2_PICTURE_DEPTHTYPE v4l2Depth;
} V4L2_COLOR_FORMATTYPE;

struct buffer
{
  OMX_U8* pCapAddrStart; /* Starting address for captured data */
  unsigned int length;
};

/* Queue for V4L mapping buffers.
 *  Note: This is the key structure of the camera component. According to V4L,
 *  there may be multiple buffers that can be requested for buffer capturing
 *  one after another. All these buffers form a cyclic queue.
 */
typedef struct OMX_V4L2_MAPBUFFER_QUEUETYPE
{
    OMX_U32 nNextCaptureIndex; /* Which index to perform capture request */
    OMX_U32 nNextWaitIndex; /* Which index to sync buffer */
    OMX_U32 nLastBufIndex; /* The last ready buffer that has not been sent to some port */
    OMX_U32 nBufCountTotal; /* Number of total involved buffers, including all buffers that has been started and not been sent to some port */
    OMX_U32 nBufCountCaptured; /* Number of captured bufers, including all ready buffers that has not been sent to some port */
    struct buffer *buffers; /* V4L2 buffer map information */
    OMX_U32 nFrame;
    OMX_TICKS *qTimeStampQueue; /* Queue to store time stamps for each buffer */
} OMX_V4L2_MAPBUFFER_QUEUETYPE;


/** Camera source component port structure.
  */
DERIVEDCLASS(omx_camera_source_component_PortType, omx_base_video_PortType)
#define omx_camera_source_component_PortType_FIELDS omx_base_video_PortType_FIELDS \
  /** @param nIndexMapbufQueue Index to the next buffer which should be sent to this port */ \
  OMX_U32 nIndexMapbufQueue;
ENDCLASS(omx_camera_source_component_PortType)



/** Camera source component private structure.
  */
DERIVEDCLASS(omx_camera_source_component_PrivateType, omx_base_source_PrivateType)
#define omx_camera_source_component_PrivateType_FIELDS omx_base_source_PrivateType_FIELDS \
  /** @param idle_state_mutex mutex for idle state related operations */ \
  pthread_mutex_t idle_state_mutex; \
  /** @param idle_wait_condition condition for waiting on idle state */ \
  pthread_cond_t idle_wait_condition; \
  /** @param idle_process_condition condition for processing for idle state */ \
  pthread_cond_t idle_process_condition; \
  /** @param bWaitingOnIdle whether the buffer management thread is waiting on idle state */ \
  OMX_BOOL bWaitingOnIdle; \
  /** @param eLastState last state before the latest state transition */ \
  OMX_STATETYPE eLastState; \
  /** @param sSensorMode Sensor mode parameters */ \
  OMX_PARAM_SENSORMODETYPE sSensorMode; \
  /** @param nFrameIntervalInMilliSec Frame interval (in millisecond) calculated from frame rate */ \
  OMX_U32 nFrameIntervalInMilliSec; \
  /** @param eOmxColorFormat OMX color format for camera */ \
  OMX_COLOR_FORMATTYPE eOmxColorFormat; \
  /** @param sV4lColorFormat V4L color format (palette, depth) for camera */ \
  V4L2_COLOR_FORMATTYPE sV4lColorFormat; \
  /** @param fdCam File descriptor for camera device; -1 is the invalid value */ \
  int fdCam; \
  /** @sMapbufQueue V4L mapping buffer queue */ \
  OMX_V4L2_MAPBUFFER_QUEUETYPE sMapbufQueue; \
  /** @nLastCaptureTimeInMilliSec Time stamp for the last capture */ \
  OMX_U32 nLastCaptureTimeInMilliSec; \
  /** @setconfig_mutex mutex used to SetConfig */ \
  pthread_mutex_t setconfig_mutex; \
  /** @bCapturing Whether the camera is capturing */ \
  OMX_BOOL bCapturing; \
  /** @bCapturingNext The next state for bCapturing */ \
  OMX_BOOL bCapturingNext; \
  /** @bIsFirstFrame Whether the current video frame is the first frame being captured */ \
  OMX_BOOL bIsFirstFrame; \
  /** @bAutoPause Whether the camera is automaticly paused after image/video capture */ \
  OMX_BOOL bAutoPause; \
  /** @bThumbnailStart Whether the camera starts thumbnail */ \
  OMX_BOOL bThumbnailStart; \
  /** @nCapturedCount The number of captured video frames, to help to make thumbnail decision */ \
  OMX_U32 nCapturedCount; \
  /** @nRefWallTime Reference wall time, used to calculate time stamp for each captured buffer */ \
  OMX_TICKS nRefWallTime; \
  /** @param capability capability of the video capture device */ \
  struct v4l2_capability cap; \
  /** @param oFrameSize output frame size */ \
  OMX_U32 oFrameSize; \
  /** @param bOutBufferMemoryMapped boolean flag. True,if output buffer is memory mapped to avoid memcopy*/ \
  OMX_BOOL bOutBufferMemoryMapped; \
  /* @param cropcap input image cropping */ \
  struct v4l2_cropcap cropcap; \
  struct v4l2_crop crop; \
  /* @param fmt Stream data format */ \
  struct v4l2_format fmt;
ENDCLASS(omx_camera_source_component_PrivateType)



/* Component private entry points declaration */
OMX_ERRORTYPE omx_camera_source_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);

OMX_ERRORTYPE omx_camera_source_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

#endif
