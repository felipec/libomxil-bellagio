/**
  @file src/components/fbdev/omx_fbdev_sink_component.h
  
  OpenMax FBDEV sink component. This component is a video sink that copies
  data to a linux framebuffer device.

  Originally developed by Peter Littlefield
	Copyright (C) 2007  STMicroelectronics and Agere Systems

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

  $Date: 2007-05-22 14:25:04 +0200 (Tue, 22 May 2007) $
  Revision $Rev: 872 $
  Author $Author: giulio_urlini $
*/

#include <errno.h>
#include <omxcore.h>
#include <omx_fbdev_sink_component.h>

/** height offset - reqd tadjust the display position - at the centre of upper half of screen */
#define HEIGHT_OFFSET 50

/** we assume, frame rate = 25 fps ; so one frame processing time = 40000 us */
#define FRAME_PROCESS_TIME 40000 // in micro second

/** Counter of sink component instance*/
OMX_U32 nofbdev_sinkInstance=0;

/** global variables that stores the frame buffer settings */
OMX_COLOR_FORMATTYPE fbpxlfmt;
OMX_U32 fbwidth; //frame buffer display width
OMX_U32 fbheight; //frame buffer display height
OMX_U32 fbbpp; //frame buffer pixel depth
OMX_S32 fbstride; // frame buffer display stride 
OMX_U32 product; // frame buffer memory area 

/** global variables for image format adjustment */
OMX_U8* org_src_cpy_ptr;
OMX_U8* org_dst_cpy_ptr;
int j;
int cp_byte; //equal to source image byte per pixel value
OMX_U8 r,g,b,a;


/** Returns a time value in milliseconds based on a clock starting at
 *  some arbitrary base. Given a call to GetTime that returns a value
 *  of n a subsequent call to GetTime made m milliseconds later should 
 *  return a value of (approximately) (n+m). This method is used, for
 *  instance, to compute the duration of call. */
long GetTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((long)now.tv_sec) * 1000 + ((long)now.tv_usec) / 1000;
}

/** The Constructor 
  * @param cComponentName is the namje of the constructed component
  */
OMX_ERRORTYPE omx_fbdev_sink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  OMX_S32 i;
  omx_fbdev_sink_component_PortType *port;
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_fbdev_sink_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?	
    */
  err = omx_base_sink_Constructor(openmaxStandComp, cComponentName);
  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */
  omx_fbdev_sink_component_Private = (omx_fbdev_sink_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  /** Allocate Ports and Call base port constructor. */	
  if (omx_fbdev_sink_component_Private->sPortTypesParam.nPorts && !omx_fbdev_sink_component_Private->ports) {
    omx_fbdev_sink_component_Private->ports = calloc(omx_fbdev_sink_component_Private->sPortTypesParam.nPorts, sizeof (omx_base_PortType *));
    if (!omx_fbdev_sink_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_fbdev_sink_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; size of the struct is for derived class port type
        * this could be refactored as a smarter factory function instead?
        */
      omx_fbdev_sink_component_Private->ports[i] = calloc(1, sizeof(omx_fbdev_sink_component_PortType));
      if (!omx_fbdev_sink_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }
  base_port_Constructor(openmaxStandComp, &omx_fbdev_sink_component_Private->ports[0], 0, OMX_TRUE);
  port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  /** Domain specific section for the allocated port. */	
  port->sPortParam.eDomain = OMX_PortDomainVideo;
  port->sPortParam.format.video.cMIMEType = (OMX_STRING)malloc(sizeof(char)*128);
  strcpy(port->sPortParam.format.video.cMIMEType, "raw");
  port->sPortParam.format.video.pNativeRender = NULL;
  port->sPortParam.format.video.nFrameWidth = 0;
  port->sPortParam.format.video.nFrameHeight = 0;
  port->sPortParam.format.video.nStride = 0;
  port->sPortParam.format.video.nSliceHeight = 0;
  port->sPortParam.format.video.nBitrate = 0;
  port->sPortParam.format.video.xFramerate = 0;
  port->sPortParam.format.video.bFlagErrorConcealment = OMX_FALSE;

  setHeader(&port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
  port->sVideoParam.nPortIndex = 0;
  port->sVideoParam.nIndex = 0;
  port->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingUnused;
  port->sVideoParam.eColorFormat = OMX_COLOR_FormatUnused;

  /** Set configs */
  setHeader(&port->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));	
  port->omxConfigCrop.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  port->omxConfigCrop.nLeft = port->omxConfigCrop.nTop = 0;
  port->omxConfigCrop.nWidth = port->omxConfigCrop.nHeight = 0;

  setHeader(&port->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));	
  port->omxConfigRotate.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  port->omxConfigRotate.nRotation = 0;	//Default: No rotation (0 degrees)

  setHeader(&port->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));	
  port->omxConfigMirror.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  port->omxConfigMirror.eMirror = OMX_MirrorNone;	//Default: No mirroring

  setHeader(&port->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));	
  port->omxConfigScale.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  port->omxConfigScale.xWidth = port->omxConfigScale.xHeight = 0x10000;	//Default: No scaling (scale factor = 1)

  setHeader(&port->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));	
  port->omxConfigOutputPosition.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  port->omxConfigOutputPosition.nX = port->omxConfigOutputPosition.nY = 0; //Default: No shift in output position (0,0)

  /** set the function pointers */
  omx_fbdev_sink_component_Private->destructor = omx_fbdev_sink_component_Destructor;
  omx_fbdev_sink_component_Private->BufferMgmtCallback = omx_fbdev_sink_component_BufferMgmtCallback;
  openmaxStandComp->SetParameter = omx_fbdev_sink_component_SetParameter;
  openmaxStandComp->GetParameter = omx_fbdev_sink_component_GetParameter;
  omx_fbdev_sink_component_Private->messageHandler = omx_fbdev_sink_component_MessageHandler;

  nofbdev_sinkInstance++;
  if(nofbdev_sinkInstance > MAX_NUM_OF_fbdev_sink_component_INSTANCES) {
    return OMX_ErrorInsufficientResources;
  }

  return err;
}


/** The Destructor 
 */
OMX_ERRORTYPE omx_fbdev_sink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;  
  int i;

  /** frees the port structures*/
  if (omx_fbdev_sink_component_Private->sPortTypesParam.nPorts && omx_fbdev_sink_component_Private->ports) {
    for (i = 0; i < omx_fbdev_sink_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_fbdev_sink_component_Private->ports[i]) {
        base_port_Destructor(omx_fbdev_sink_component_Private->ports[i]);
      }
    }
    free(omx_fbdev_sink_component_Private->ports);
    omx_fbdev_sink_component_Private->ports = NULL;
  }

  omx_base_sink_Destructor(openmaxStandComp);
  nofbdev_sinkInstance--;

  return OMX_ErrorNone;
}


/** The initialization function 
  * This function opens the frame buffer device and allocates memory for display 
  * also it finds the frame buffer supported display formats
  */
OMX_ERRORTYPE omx_fbdev_sink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType* port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  omx_fbdev_sink_component_Private->fd = open(FBDEV_FILENAME, O_RDWR);
  if (omx_fbdev_sink_component_Private->fd < 0) {
    DEBUG(DEB_LEV_ERR, "Unable to open framebuffer %s!  open returned: %i, errno=%d  ENODEV : %d \n", FBDEV_FILENAME, omx_fbdev_sink_component_Private->fd,errno,ENODEV);
    return OMX_ErrorHardware;
  } 

  /** frame buffer display configuration get */
  if(ioctl(omx_fbdev_sink_component_Private->fd, FBIOGET_VSCREENINFO, &omx_fbdev_sink_component_Private->vscr_info) != 0 ||
    ioctl(omx_fbdev_sink_component_Private->fd, FBIOGET_FSCREENINFO, &omx_fbdev_sink_component_Private->fscr_info) != 0) {
    DEBUG(DEB_LEV_ERR, "Error during ioctl to get framebuffer parameters!\n");
    return OMX_ErrorHardware;
  }

  /** From the frame buffer display rgb formats, find the corresponding standard OMX format 
    * It is needed to convert the input rgb content onto frame buffer supported rgb content
    */
  fbpxlfmt = find_omx_pxlfmt(&omx_fbdev_sink_component_Private->vscr_info);
  if (fbpxlfmt == OMX_COLOR_FormatUnused) {
    DEBUG(DEB_LEV_ERR,"\n in %s finding omx pixel format returned error\n", __func__);
    return OMX_ErrorUnsupportedSetting;
  }

  fbwidth = omx_fbdev_sink_component_Private->vscr_info.xres;
  fbheight = port->sPortParam.format.video.nFrameHeight;
  fbbpp = omx_fbdev_sink_component_Private->vscr_info.bits_per_pixel;
  fbstride = calcStride(fbwidth, fbpxlfmt);

  /** the allocated memory has more vertical reolution than needed because we want to show the 
    * output displayed not at the corner of screen, but at the centre of upper part of screen
    */
  product = fbstride * (fbheight + HEIGHT_OFFSET); 

  /** memory map frame buf memory */
  omx_fbdev_sink_component_Private->scr_ptr = (char*) mmap(0, product, PROT_READ | PROT_WRITE, MAP_SHARED, omx_fbdev_sink_component_Private->fd, 0);
  if (omx_fbdev_sink_component_Private->scr_ptr == NULL) {
    DEBUG(DEB_LEV_ERR, "in %s Failed to mmap framebuffer memory!\n", __func__);
    close (omx_fbdev_sink_component_Private->fd);
    return OMX_ErrorHardware;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Successfully opened %s for display.\n", "/dev/fb0");
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Display Size: %u x %u\n", (int)fbwidth, (int)fbheight);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Bitdepth: %u\n", (int)fbbpp);

  return OMX_ErrorNone;
}

/** The deinitialization function 
  * It deallocates the frame buffer memory, and closes frame buffer
  */
OMX_ERRORTYPE omx_fbdev_sink_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;

  if (omx_fbdev_sink_component_Private->scr_ptr) {
    munmap(omx_fbdev_sink_component_Private->scr_ptr, product);
  }
  if (close(omx_fbdev_sink_component_Private->fd) == -1) {
    return OMX_ErrorHardware;
  }
  return OMX_ErrorNone;
}

/** Check Domain of the Tunneled Component */
OMX_ERRORTYPE omx_fbdev_sink_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef){
  if(pDef.eDomain!=OMX_PortDomainVideo) {
    return OMX_ErrorPortsNotCompatible;
  }
  return OMX_ErrorNone;
}


/**	This function takes two inputs - 
  * @param width is the input picture width
  * @param omx_pxlfmt is the input openmax standard pixel format
  * It calculates the byte per pixel needed to display the picture with the input omx_pxlfmt
  * The output stride for display is thus product of input width and byte per pixel
  */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt) {
  OMX_U32 stride;
  OMX_U32 bpp; // bit per pixel

  switch(omx_pxlfmt) {
    case OMX_COLOR_FormatMonochrome:
      bpp = 1;
      break;
    case OMX_COLOR_FormatL2:
      bpp = 2;
    case OMX_COLOR_FormatL4:
      bpp = 4;
      break;
    case OMX_COLOR_FormatL8:
    case OMX_COLOR_Format8bitRGB332:
    case OMX_COLOR_FormatRawBayer8bit:
    case OMX_COLOR_FormatRawBayer8bitcompressed:
      bpp = 8;	
      break;
    case OMX_COLOR_FormatRawBayer10bit:
      bpp = 10;
      break;
    case OMX_COLOR_FormatYUV411Planar:
    case OMX_COLOR_FormatYUV411PackedPlanar:
    case OMX_COLOR_Format12bitRGB444:
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420PackedPlanar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV444Interleaved:
      bpp = 12;
      break;
    case OMX_COLOR_FormatL16:
    case OMX_COLOR_Format16bitARGB4444:
    case OMX_COLOR_Format16bitARGB1555:
    case OMX_COLOR_Format16bitRGB565:
    case OMX_COLOR_Format16bitBGR565:
    case OMX_COLOR_FormatYUV422Planar:
    case OMX_COLOR_FormatYUV422PackedPlanar:
    case OMX_COLOR_FormatYUV422SemiPlanar:
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYCrYCb:
    case OMX_COLOR_FormatCbYCrY:
    case OMX_COLOR_FormatCrYCbY:
      bpp = 16;
      break;
    case OMX_COLOR_Format18bitRGB666:
    case OMX_COLOR_Format18bitARGB1665:
      bpp = 18;
      break;
    case OMX_COLOR_Format19bitARGB1666:
      bpp = 19;
      break;
    case OMX_COLOR_FormatL24:
    case OMX_COLOR_Format24bitRGB888:
    case OMX_COLOR_Format24bitBGR888:
    case OMX_COLOR_Format24bitARGB1887:
      bpp = 24;
      break;
    case OMX_COLOR_Format25bitARGB1888:
      bpp = 25;
      break;
    case OMX_COLOR_FormatL32:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32bitARGB8888:
      bpp = 32;
      break;
    default:
      bpp = 0;
      break;
  }
  stride = (width * bpp) >> 3; // division by 8 to get byte per pixel value
  return (OMX_S32) stride;
}


/**	Returns the OpenMAX color format type corresponding to an fbdev fb_var_screeninfo structure 
  * @param vscr_info contains the frame buffer display settings
  * We extract the rgba configuration of the frame buffer from this structure and thereby 
  * apply the appropriate openmax standard color format equivalent to this configuration
  * the r,g,b,a content length is needed to get the bit per pixel values
  * r,g,b,a content offsets determine their respective positions
  */
OMX_COLOR_FORMATTYPE find_omx_pxlfmt(struct fb_var_screeninfo *vscr_info) {
  OMX_COLOR_FORMATTYPE omx_pxlfmt = OMX_COLOR_FormatUnused;

  /** check if gray scale -  if so then, switch according to bit per pixel criteria */
  if (vscr_info->grayscale) {
    switch (vscr_info->bits_per_pixel) {
      case 2:
        omx_pxlfmt = OMX_COLOR_FormatL2;
        break;
      case 4:
        omx_pxlfmt = OMX_COLOR_FormatL4;
        break;
      case 8:
        omx_pxlfmt = OMX_COLOR_FormatL8;
        break;
      case 16:
        omx_pxlfmt = OMX_COLOR_FormatL16;
        break;
      case 24:
        omx_pxlfmt = OMX_COLOR_FormatL24;
        break;
      case 32:
        omx_pxlfmt = OMX_COLOR_FormatL32;
        break;
      default:
        omx_pxlfmt = OMX_COLOR_FormatUnused;
        break;
    }
  } else {
    /** now check the rgba components and set the corresponding color formats */
    if(vscr_info->bits_per_pixel == 1) {
      omx_pxlfmt = OMX_COLOR_FormatMonochrome;
    } else if ( vscr_info->red.length == 3 && vscr_info->red.offset == 5 &&
                vscr_info->green.length == 3 && vscr_info->green.offset == 2 &&
                vscr_info->blue.length == 2 && vscr_info->blue.offset == 0 &&
                vscr_info->transp.length == 0) {
      omx_pxlfmt = OMX_COLOR_Format8bitRGB332;
    } else if ( vscr_info->red.length == 4 && vscr_info->red.offset == 8 &&
                vscr_info->green.length == 4 && vscr_info->green.offset == 4 &&
                vscr_info->blue.length == 4 && vscr_info->blue.offset == 0 &&
                vscr_info->transp.length == 0) {
      omx_pxlfmt = OMX_COLOR_Format12bitRGB444;
    } else if ( vscr_info->transp.length == 4 && vscr_info->transp.offset == 12 &&
                vscr_info->red.length == 4 && vscr_info->red.offset == 8 &&
                vscr_info->green.length == 4 && vscr_info->green.offset == 4 &&
                vscr_info->blue.length == 4 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitARGB4444;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 && 
                vscr_info->transp.length == 1 && vscr_info->transp.offset == 15 &&
                vscr_info->green.length == 5 && vscr_info->green.offset == 5 &&
                vscr_info->red.offset == 10 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitARGB1555;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 && 
                vscr_info->green.length == 6 && vscr_info->green.offset == 5 && 
                vscr_info->red.offset == 11 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitRGB565;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 && 
                vscr_info->green.length == 6 && vscr_info->green.offset == 5 &&
                vscr_info->red.offset == 0 && vscr_info->blue.offset == 11) {
      omx_pxlfmt = OMX_COLOR_Format16bitBGR565;
    } else if ( vscr_info->red.length == 6 && vscr_info->green.length == 6 &&
                vscr_info->transp.length == 0 && vscr_info->red.offset == 12 && vscr_info->green.offset == 6 &&
                vscr_info->blue.length == 6 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format18bitRGB666;
    } else if ( vscr_info->red.length == 6 && vscr_info->green.length == 6 && 
                vscr_info->transp.length == 1 && vscr_info->transp.offset == 17 &&
                vscr_info->red.offset == 11 && vscr_info->green.offset == 5 &&
                vscr_info->blue.length == 5 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format18bitARGB1665;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 18 &&
                vscr_info->red.length == 6 && vscr_info->red.offset ==  12 &&
                vscr_info->green.length == 6 && vscr_info->green.offset == 6 &&
                vscr_info->blue.length == 6 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format19bitARGB1666;
    } else if ( vscr_info->transp.length == 0 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->green.offset == 8 && vscr_info->red.offset == 16 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format24bitRGB888;
    } else if ( vscr_info->transp.length == 0 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->green.offset == 8 && vscr_info->red.offset == 0 && vscr_info->blue.offset == 16) {
      omx_pxlfmt = OMX_COLOR_Format24bitBGR888;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 23 &&
                vscr_info->red.length == 8 && vscr_info->red.offset == 15 &&
                vscr_info->green.length == 8 && vscr_info->green.offset == 7 &&
                vscr_info->blue.length == 7 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format24bitARGB1887;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 24 &&
                vscr_info->red.length == 8 && vscr_info->red.offset == 16 &&
                vscr_info->green.length == 8 && vscr_info->green.offset == 8 &&
                vscr_info->blue.length == 8 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format25bitARGB1888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->transp.offset == 24 && vscr_info->red.offset == 16 &&
                vscr_info->green.offset == 8 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format32bitARGB8888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 && 
                vscr_info->transp.offset == 0 && vscr_info->red.offset == 8 &&
                vscr_info->green.offset == 16 && vscr_info->blue.offset == 24) {
      omx_pxlfmt = OMX_COLOR_Format32bitBGRA8888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 && 
                vscr_info->transp.offset == 0 && vscr_info->red.offset == 0 &&
                vscr_info->green.offset == 0 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format32bitARGB8888;		
    } else {
      omx_pxlfmt = OMX_COLOR_FormatUnused;
    }
  }
  return omx_pxlfmt;
}


/**	This function copies source inmage to destination image of required dimension and color formats 
  * @param src_ptr is the source image strting pointer
  * @param src_stride is the source image stride (src_width * byte_per_pixel)
  * @param src_width is source image width & src_height is source image height
  * @param src_offset_x & src_offset_y are x,y offset values (if any) from starting pointer
  * @param dest_ptr is the destination image strting pointer
  * @param dest_stride is the destination image stride (dest_width * byte_per_pixel)
  * @param dest_width is destination image width & dest_height is destination image height
  * @param dest_offset_x dest_offset_y are x,y offset values (if any) from starting pointer
  * @param cpy_width cpy_height is the source image copy width and height - it determines the portion of 
    source image to be copied from source to destination image
  * @param colorformat is the source image color format
  */
void omx_img_copy(OMX_U8* src_ptr, OMX_S32 src_stride, OMX_U32 src_width, OMX_U32 src_height, 
                  OMX_S32 src_offset_x, OMX_S32 src_offset_y,
                  OMX_U8* dest_ptr, OMX_S32 dest_stride, OMX_U32 dest_width,  OMX_U32 dest_height, 
                  OMX_S32 dest_offset_x, OMX_S32 dest_offset_y, 
                  OMX_S32 cpy_width, OMX_U32 cpy_height, OMX_COLOR_FORMATTYPE colorformat ) {	

  OMX_U32 i;
  /**	CAUTION: We don't do any checking of boundaries! (FIXME - see omx_ffmpeg_colorconv_component_BufferMgmtCallback)
    * Input frame is planar, not interleaved
    * Feel free to add more formats if implementing them
    */
  if (colorformat == OMX_COLOR_FormatYUV411Planar ||		
      colorformat == OMX_COLOR_FormatYUV411PackedPlanar || 
      colorformat == OMX_COLOR_FormatYUV420Planar ||
      colorformat == OMX_COLOR_FormatYUV420PackedPlanar ||
      colorformat == OMX_COLOR_FormatYUV422Planar ||
      colorformat == OMX_COLOR_FormatYUV422PackedPlanar ) {

    OMX_U32 src_luma_width;			//	Width (in columns) of the source Y plane
    OMX_U32 src_luma_height;		//	Height (in rows) of source Y plane
    OMX_S32 src_luma_stride;		//	Stride in bytes of each source Y row
    OMX_U32 src_luma_offset_x;		//	Horizontal byte offset
    OMX_U32 src_luma_offset_y;		//	Vertical offset in rows from top of plane
    OMX_U32 src_luma_offset;		//	Total byte offset to rectangle

    OMX_U32 src_chroma_width;		//	Width (in columns) of source chroma planes
    OMX_U32 src_chroma_height;		//	Height (in rows) of source chroma planes
    OMX_S32 src_chroma_stride;		//	Stride in bytes of each source chroma row
    OMX_U32 src_chroma_offset_x;	//	Horizontal byte offset
    OMX_U32 src_chroma_offset_y;	//	Vertical offset in rows from top of plane
    OMX_U32 src_chroma_offset;		//	Bytes to crop rectangle from start of chroma plane

    OMX_U32 dest_luma_width;		//	Width (in columns) of the destination Y plane
    OMX_U32 dest_luma_height;		//	Height (in rows) of destination Y plane
    OMX_S32 dest_luma_stride;		//	Stride in bytes of each destination Y row
    OMX_U32 dest_luma_offset_x;		//	Horizontal byte offset
    OMX_U32 dest_luma_offset_y;		//	Vertical offset in rows from top of plane
    OMX_U32 dest_luma_offset;		//	Bytes to crop rectangle from start of Y plane

    OMX_U32 dest_chroma_width;		//	Width (in columns) of destination chroma planes
    OMX_U32 dest_chroma_height;		//	Height (in rows) of destination chroma planes
    OMX_S32 dest_chroma_stride;		//	Stride in bytes of each destination chroma row
    OMX_U32 dest_chroma_offset_x;	//	Horizontal byte offset
    OMX_U32 dest_chroma_offset_y;	//	Vertical offset in rows from top of plane
    OMX_U32 dest_chroma_offset;		//	Bytes to crop rectangle from start of chroma plane

    OMX_U32 luma_crop_width;		//	Width in bytes of a luma row in the crop rectangle
    OMX_U32 luma_crop_height;		//	Number of luma rows in the crop rectangle
    OMX_U32 chroma_crop_width;		//	Width in bytes of a chroma row in the crop rectangle
    OMX_U32 chroma_crop_height;		//	Number of chroma rows in crop rectangle

    switch (colorformat) {
      /**	Watch out for odd or non-multiple-of-4 (4:1:1) luma resolutions (I don't check)	*/
      /**	Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV411Planar:		
      case OMX_COLOR_FormatYUV411PackedPlanar:
        /**	OpenMAX IL spec says chroma channels are subsampled by 4x horizontally AND vertically in YUV 4:1:1.
          *	Conventional wisdom (wiki) tells us that it is only subsampled horizontally.
          *		Following OpenMAX spec anyway.	Technically I guess this would be YUV 4:1:0.	
          */				
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width  >> 2; 
        src_chroma_height = src_luma_height;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x  >> 2; 
        src_chroma_offset_y = src_luma_offset_y;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width  >> 2;
        dest_chroma_height = dest_luma_height;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x  >> 2; 
        dest_chroma_offset_y = dest_luma_offset_y;

        luma_crop_width = (OMX_U32) abs(cpy_width);
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width  >> 2; 
        chroma_crop_height = luma_crop_height;
        break;	

      /**	Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV420Planar:		
      case OMX_COLOR_FormatYUV420PackedPlanar:
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width >> 1;
        src_chroma_height = src_luma_height >> 1;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x >> 1;
        src_chroma_offset_y = src_luma_offset_y >> 1;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width >> 1;
        dest_chroma_height = dest_luma_height >> 1;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x >> 1;
        dest_chroma_offset_y = dest_luma_offset_y >> 1;

        luma_crop_width = cpy_width;
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width >> 1;
        chroma_crop_height = luma_crop_height >> 1;
        break;
      /**	Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV422Planar:		
      case OMX_COLOR_FormatYUV422PackedPlanar:
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width >> 1;
        src_chroma_height = src_luma_height;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x >> 1;
        src_chroma_offset_y = src_luma_offset_y;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width >> 1;
        dest_chroma_height = dest_luma_height;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x >> 1;
        dest_chroma_offset_y = dest_luma_offset_y;

        luma_crop_width = (OMX_U32) abs(cpy_width);
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width >> 1;
        chroma_crop_height = luma_crop_height;
        break;
      default:
        DEBUG(DEB_LEV_ERR,"\n color format not supported --error \n");
        return;
    }

    /**	Pointers to the start of each plane to make things easier */
    OMX_U8* Y_input_ptr = src_ptr;
    OMX_U8* U_input_ptr = Y_input_ptr + ((OMX_U32) abs(src_luma_stride) * src_luma_height);
    OMX_U8* V_input_ptr = U_input_ptr + ((OMX_U32) abs(src_chroma_stride) * src_chroma_height);

    /**	Figure out total offsets */
    src_luma_offset = (src_luma_offset_y * (OMX_U32) abs(src_luma_stride)) + src_luma_offset_x;
    src_chroma_offset = (src_chroma_offset_y * (OMX_U32) abs(src_chroma_stride)) + src_chroma_offset_x;

    /**	If input stride is negative, reverse source row order */
    if (src_stride < 0) {
      src_luma_offset += ((OMX_U32) abs(src_luma_stride)) * (src_luma_height - 1);
      src_chroma_offset += ((OMX_U32) abs(src_chroma_stride)) * (src_chroma_height - 1);

      if (src_luma_stride > 0) {
        src_luma_stride *= -1;
      }

      if (src_chroma_stride > 0) {
        src_chroma_stride *= -1;	
      }
    }

    /**	Pointers to use with memcpy */
    OMX_U8* src_Y_ptr = Y_input_ptr + src_luma_offset;		
    OMX_U8* src_U_ptr = U_input_ptr + src_chroma_offset;
    OMX_U8*	src_V_ptr = V_input_ptr + src_chroma_offset;

    /**	Pointers to destination planes to make things easier */
    OMX_U8* Y_output_ptr = dest_ptr;
    OMX_U8* U_output_ptr = Y_output_ptr + ((OMX_U32) abs(dest_luma_stride) * dest_luma_height);
    OMX_U8* V_output_ptr = U_output_ptr + ((OMX_U32) abs(dest_chroma_stride) * dest_chroma_height);	

    /**	Figure out total offsets */
    dest_luma_offset = (dest_luma_offset_y * (OMX_U32) abs(dest_luma_stride)) + dest_luma_offset_x;
    dest_chroma_offset = (dest_chroma_offset_y * (OMX_U32) abs(dest_chroma_stride)) + dest_chroma_offset_x;

    /**	If output stride is negative, reverse destination row order */
    if (dest_stride < 0) {
      dest_luma_offset += ((OMX_U32) abs(dest_luma_stride)) * (dest_luma_height - 1);
      dest_chroma_offset += ((OMX_U32) abs(dest_chroma_stride)) * (dest_chroma_height - 1);
      if (dest_luma_stride > 0) {
        dest_luma_stride *= -1;
      }
      if (dest_chroma_stride > 0) {
        dest_chroma_stride *= -1;	
      }
    }

    /**	Pointers to use with memcpy */
    OMX_U8* dest_Y_ptr = Y_output_ptr + dest_luma_offset;		
    OMX_U8* dest_U_ptr = U_output_ptr + dest_chroma_offset;
    OMX_U8*	dest_V_ptr = V_output_ptr + dest_chroma_offset;

    //	Y
    for (i = 0; i < luma_crop_height; ++i, src_Y_ptr += src_luma_stride, dest_Y_ptr += dest_luma_stride) {
      memcpy(dest_Y_ptr, src_Y_ptr, luma_crop_width);	//	Copy Y rows into in_buffer
    }
    //	U
    for (i = 0; i < chroma_crop_height; ++i, src_U_ptr += src_chroma_stride, dest_U_ptr += dest_chroma_stride) {
      memcpy(dest_U_ptr, src_U_ptr, chroma_crop_width);	//	Copy U rows into in_buffer
    }
    //	V
    for (i = 0; i < chroma_crop_height; ++i, src_V_ptr += src_chroma_stride, dest_V_ptr += dest_chroma_stride) {
      memcpy(dest_V_ptr, src_V_ptr, chroma_crop_width);	//	Copy V rows into in_buffer
    }
  } else {	

    OMX_U32 cpy_byte_width = calcStride((OMX_U32) abs(cpy_width), colorformat);	//	Bytes width to copy
    OMX_U32 src_byte_offset_x = calcStride((OMX_U32) abs(src_offset_x), colorformat);
    OMX_U32 dest_byte_offset_x = calcStride((OMX_U32) abs(dest_offset_x), colorformat);
    OMX_U32 src_byte_offset_y = src_offset_y * (OMX_U32) abs(src_stride);
    OMX_U32 dest_byte_offset_y = dest_offset_y * (OMX_U32) abs(dest_stride);

    if (src_stride < 0)	{
      //	If input stride is negative, start from bottom
      src_byte_offset_y += cpy_height * (OMX_U32) abs(src_stride);
    }	
    if (dest_stride < 0) {
      //	If output stride is negative, start from bottom
      dest_byte_offset_y += cpy_height * (OMX_U32) abs(dest_stride);
    }

    OMX_U8* src_cpy_ptr = src_ptr + src_byte_offset_y + src_byte_offset_x;
    OMX_U8* dest_cpy_ptr = dest_ptr + dest_byte_offset_y + dest_byte_offset_x;

    /** fbpxlfmt is the output (frame buffer supported) image color format 
      * here fbpxlfmt is OMX_COLOR_Format32bitARGB8888 always because 
      * the frame buffer has configuration of rgba 8/0 8/0 8/0 8/0 with pixel depth 8
      * if other configuration frame buffer is employed then appropriate conversion policy should be written
      */
    if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format24bitRGB888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) { 
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr; 
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          r = (OMX_U8) *(src_cpy_ptr + 0);
          g = (OMX_U8) *(src_cpy_ptr + 1);
          b = (OMX_U8) *(src_cpy_ptr + 2);
          //assign to detination 
          *(dest_cpy_ptr + 0) = b; 
          *(dest_cpy_ptr + 1) = g; 
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;     
          dest_cpy_ptr += 4;     
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format24bitBGR888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) { 
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr; 
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          b = (OMX_U8) *(src_cpy_ptr + 0);
          g = (OMX_U8) *(src_cpy_ptr + 1);
          r = (OMX_U8) *(src_cpy_ptr + 2);
          //assign to detination 
          *(dest_cpy_ptr + 0) = b; 
          *(dest_cpy_ptr + 1) = g; 
          *(dest_cpy_ptr + 2) = r; 
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;     
          dest_cpy_ptr += 4;     
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && (colorformat == OMX_COLOR_Format32bitBGRA8888 || colorformat == OMX_COLOR_Format32bitARGB8888)) {
      for (i = 0; i < cpy_height; ++i, src_cpy_ptr += src_stride, dest_cpy_ptr += dest_stride ) { 
        // same color format - so no extraction - only simple memcpy
        memcpy(dest_cpy_ptr, src_cpy_ptr, cpy_byte_width);	//	Copy rows
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format16bitARGB1555) { 
      cp_byte = 2;
      for (i = 0; i < cpy_height; ++i) { 
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr; 
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          // individual argb components are less than 1 byte 
          OMX_U16 temp_old, temp, *temp1;
          temp1=(OMX_U16*)src_cpy_ptr;
          temp = *temp1;
          temp_old = temp;
          a = (OMX_U8) ((temp >> 15) && 0x0001); //getting the 1 bit of a and setting all other bits to 0
          temp = temp_old;
          r = (OMX_U8) ((temp >> 10) & 0x001f); //getting 5 bits of r and setting all other bits to 0
          temp = temp_old;
          g = (OMX_U8) ((temp >> 5) & 0x001f); //getting 5 bits of g and setting all other bits to 0
          temp = temp_old;
          b = (OMX_U8) (temp & 0x001f); //getting 5 bits of b and setting all other bits to 0
          temp = temp_old;
          // assign them in perfect order
          *(dest_cpy_ptr + 0) = b<<3; 
          *(dest_cpy_ptr + 1) = g<<3; 
          *(dest_cpy_ptr + 2) = r<<3; 
          *(dest_cpy_ptr + 3) = a<<7; 
          src_cpy_ptr += cp_byte;     
          dest_cpy_ptr += 4;     
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && (colorformat == OMX_COLOR_Format16bitRGB565 || OMX_COLOR_Format16bitBGR565)) {
      cp_byte = 2;
      for (i = 0; i < cpy_height; ++i) { 
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr; 
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          // individual rgb components are less than 1 byte 
          OMX_U16 temp_old, temp,*temp1;
          temp1=(OMX_U16*)src_cpy_ptr;
          temp = *temp1;
          temp_old = temp;
          r = (OMX_U8) ((temp >> 11) & 0x001f); //getting 5 bits of r and setting all other bits to 0
          temp = temp_old;
          g = (OMX_U8) ((temp >> 5) & 0x003f); //getting 6 bits of g and setting all other bits to 0
          temp = temp_old;
          b = (OMX_U8) (temp & 0x001f); //getting 5 bits of b and setting all other bits to 0
          temp = temp_old;
          // assign them in perfect order
          *(dest_cpy_ptr + 0) = b<<3; 
          *(dest_cpy_ptr + 1) = g<<2; 
          *(dest_cpy_ptr + 2) = r<<3; 
          // last byte  - all 1
          *(dest_cpy_ptr + 3) = 0xff; 
          src_cpy_ptr += cp_byte;     
          dest_cpy_ptr += 4;     
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else {
      DEBUG(DEB_LEV_ERR, "\n the frame buffer supported pixel format is different from OMX_COLOR_Format32bitBGRA8888\n");
      DEBUG(DEB_LEV_ERR, "\n or the input rgb format is not supported\n");
    }
  }
}



/** buffer management callback function 
  * takes one input buffer and displays its contents 
  */
void omx_fbdev_sink_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer) {
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType *port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  static long old_time = 0, new_time = 0;
  long timediff=0;

  OMX_COLOR_FORMATTYPE input_colorformat = port->sVideoParam.eColorFormat;
  OMX_S32 input_cpy_width = (OMX_S32) port->omxConfigCrop.nWidth;			//	Width (in columns) of the crop rectangle
  OMX_U32 input_cpy_height = port->omxConfigCrop.nHeight;					//	Height (in rows) of the crop rectangle

  OMX_U8* input_src_ptr = (OMX_U8*) (pInputBuffer->pBuffer);
  OMX_S32 input_src_stride = port->sPortParam.format.video.nStride;			//	Negative means bottom-to-top (think Windows bmp)
  OMX_U32 input_src_width = port->sPortParam.format.video.nFrameWidth;
  OMX_U32 input_src_height = port->sPortParam.format.video.nSliceHeight;

  /**	FIXME: Configuration values should be clamped to prevent memory trampling and potential segfaults.
    *	It might be best to store clamped AND unclamped values on a per-port basis so that OMX_GetConfig 
    *	can still return the unclamped ones.
    */
  OMX_S32 input_src_offset_x = port->omxConfigCrop.nLeft;		//	Offset (in columns) to left side of crop rectangle
  OMX_S32 input_src_offset_y = port->omxConfigCrop.nTop;		//	Offset (in rows) from top of the image to crop rectangle

  OMX_U8* input_dest_ptr = (OMX_U8*) omx_fbdev_sink_component_Private->scr_ptr + (fbstride * (HEIGHT_OFFSET - 10) + fbwidth); 
  OMX_S32 input_dest_stride = (input_src_stride < 0) ? -1 * fbstride : fbstride;

  if (port->omxConfigMirror.eMirror == OMX_MirrorVertical || port->omxConfigMirror.eMirror == OMX_MirrorBoth) {
    input_dest_stride *= -1;
  }

  OMX_U32 input_dest_width = fbwidth;
  OMX_U32 input_dest_height = fbheight;

  OMX_U32 input_dest_offset_x = port->omxConfigOutputPosition.nX;
  OMX_U32 input_dest_offset_y = port->omxConfigOutputPosition.nY;

  /** getting current time */
  new_time = GetTime();
  if(old_time == 0) {
    old_time = new_time;
  } else {
    timediff = FRAME_PROCESS_TIME - ((new_time - old_time) * 1000);
    if(timediff>0) {
      usleep(timediff);
    }
    old_time = GetTime();
  }

  /**	Copy image data into in_buffer */
  omx_img_copy(	input_src_ptr, input_src_stride, input_src_width, input_src_height, 
                input_src_offset_x, input_src_offset_y,
                input_dest_ptr, input_dest_stride, input_dest_width, input_dest_height, 
                input_dest_offset_x, input_dest_offset_y,
                input_cpy_width, input_cpy_height, input_colorformat);

  pInputBuffer->nFilledLen = 0;
}


OMX_ERRORTYPE omx_fbdev_sink_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 portIndex;
  OMX_CONFIG_RECTTYPE *omxConfigCrop;
  OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
  OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
  OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
  OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType *port;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nIndex);

  switch (nIndex) {
    case OMX_IndexConfigCommonInputCrop:
      omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
      portIndex = omxConfigCrop->nPortIndex;
      err = checkHeader(omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
      CHECK_ERROR(err, "Check Header");
      if (portIndex == OMX_BASE_SINK_INPUTPORT_INDEX) {
        port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
        port->omxConfigCrop.nLeft = omxConfigCrop->nLeft;
        port->omxConfigCrop.nTop = omxConfigCrop->nTop;
        port->omxConfigCrop.nWidth = omxConfigCrop->nWidth;
        port->omxConfigCrop.nHeight = omxConfigCrop->nHeight;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonRotate:
      omxConfigRotate = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
      portIndex = omxConfigRotate->nPortIndex;
      err = checkHeader(omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
      CHECK_ERROR(err, "Check Header");
      if (portIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
        if (omxConfigRotate->nRotation != 0) {
          //	Rotation not supported (yet)
          return OMX_ErrorUnsupportedSetting;
        }
        port->omxConfigRotate.nRotation = omxConfigRotate->nRotation;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonMirror:
      omxConfigMirror = (OMX_CONFIG_MIRRORTYPE*)pComponentConfigStructure;
      portIndex = omxConfigMirror->nPortIndex;
      err = checkHeader(omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
      CHECK_ERROR(err, "Check Header");
      if (portIndex == 0) {
        if (omxConfigMirror->eMirror == OMX_MirrorBoth || omxConfigMirror->eMirror == OMX_MirrorHorizontal)	{
          //	Horizontal mirroring not yet supported
          return OMX_ErrorUnsupportedSetting;
        }
        port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
        port->omxConfigMirror.eMirror = omxConfigMirror->eMirror;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonScale:
      omxConfigScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
      portIndex = omxConfigScale->nPortIndex;
      err = checkHeader(omxConfigScale, sizeof(OMX_CONFIG_MIRRORTYPE));
      CHECK_ERROR(err, "Check Header");
      if (portIndex == 0) {
        if (omxConfigScale->xWidth != 0x10000 || omxConfigScale->xHeight != 0x10000)  {
          //	Scaling not yet supported
          return OMX_ErrorUnsupportedSetting;
        }
        port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
        port->omxConfigScale.xWidth = omxConfigScale->xWidth;
        port->omxConfigScale.xHeight = omxConfigScale->xHeight;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonOutputPosition:
      omxConfigOutputPosition = (OMX_CONFIG_POINTTYPE*)pComponentConfigStructure;
      portIndex = omxConfigOutputPosition->nPortIndex;
      err = checkHeader(omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
      CHECK_ERROR(err, "Check Header");
      if (portIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
        port->omxConfigOutputPosition.nX = omxConfigOutputPosition->nX;
        port->omxConfigOutputPosition.nY = omxConfigOutputPosition->nY;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}



OMX_ERRORTYPE omx_fbdev_sink_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure) {

  OMX_CONFIG_RECTTYPE *omxConfigCrop;
  OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
  OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
  OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
  OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType *port;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);
  /* Check which structure we are being fed and fill its header */
  switch (nIndex) {
    case OMX_IndexConfigCommonInputCrop:
      omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
      setHeader(omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
      if (omxConfigCrop->nPortIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *)omx_fbdev_sink_component_Private->ports[omxConfigCrop->nPortIndex];
        memcpy(omxConfigCrop, &port->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
    break;		
    case OMX_IndexConfigCommonRotate:
      omxConfigRotate = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
      setHeader(omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
      if (omxConfigRotate->nPortIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *)omx_fbdev_sink_component_Private->ports[omxConfigRotate->nPortIndex];
        memcpy(omxConfigRotate, &port->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;		
    case OMX_IndexConfigCommonMirror:
      omxConfigMirror = (OMX_CONFIG_MIRRORTYPE*)pComponentConfigStructure;
      setHeader(omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
      if (omxConfigMirror->nPortIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *)omx_fbdev_sink_component_Private->ports[omxConfigMirror->nPortIndex];
        memcpy(omxConfigMirror, &port->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;			
    case OMX_IndexConfigCommonScale:
      omxConfigScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
      setHeader(omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
      if (omxConfigScale->nPortIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *)omx_fbdev_sink_component_Private->ports[omxConfigScale->nPortIndex];
        memcpy(omxConfigScale, &port->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;		
    case OMX_IndexConfigCommonOutputPosition:
      omxConfigOutputPosition = (OMX_CONFIG_POINTTYPE*)pComponentConfigStructure;
      setHeader(omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
      if (omxConfigOutputPosition->nPortIndex == 0) {
        port = (omx_fbdev_sink_component_PortType *)omx_fbdev_sink_component_Private->ports[omxConfigOutputPosition->nPortIndex];
        memcpy(omxConfigOutputPosition, &port->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;		
    default: // delegate to superclass
      return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}



OMX_ERRORTYPE omx_fbdev_sink_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      /*Check Structure Header*/
      checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
      CHECK_ERROR(err, "Check Header");
      memcpy(&omx_fbdev_sink_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
      break;
    case OMX_IndexParamPortDefinition:
      pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
      portIndex = pPortDef->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      CHECK_ERROR(err, "Parameter Check");
      port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
      if(portIndex != 0) {
        return OMX_ErrorBadPortIndex;
      }
      port->sPortParam.nBufferCountActual = pPortDef->nBufferCountActual;

      //	Copy stuff from OMX_VIDEO_PORTDEFINITIONTYPE structure
      port->sPortParam.format.video.cMIMEType = pPortDef->format.video.cMIMEType;
      port->sPortParam.format.video.nFrameWidth = pPortDef->format.video.nFrameWidth;
      port->sPortParam.format.video.nFrameHeight = pPortDef->format.video.nFrameHeight;
      port->sPortParam.format.video.nBitrate = pPortDef->format.video.nBitrate;
      port->sPortParam.format.video.xFramerate = pPortDef->format.video.xFramerate;
      port->sPortParam.format.video.bFlagErrorConcealment = pPortDef->format.video.bFlagErrorConcealment;	

      //	Figure out stride, slice height, min buffer size
      port->sPortParam.format.video.nStride = calcStride(port->sPortParam.format.video.nFrameWidth, port->sVideoParam.eColorFormat);
      port->sPortParam.format.video.nSliceHeight = port->sPortParam.format.video.nFrameHeight;	//	No support for slices yet
      port->sPortParam.nBufferSize = (OMX_U32) abs(port->sPortParam.format.video.nStride) * port->sPortParam.format.video.nSliceHeight;

      port->omxConfigCrop.nWidth = port->sPortParam.format.video.nFrameWidth;
      port->omxConfigCrop.nHeight = port->sPortParam.format.video.nFrameHeight;

      //	Don't care so much about the other domains
      memcpy(&port->sPortParam.format.audio, &pPortDef->format.audio, sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));
      memcpy(&port->sPortParam.format.image, &pPortDef->format.image, sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));
      memcpy(&port->sPortParam.format.other, &pPortDef->format.other, sizeof(OMX_OTHER_PORTDEFINITIONTYPE));
      break;

    case OMX_IndexParamVideoPortFormat:
      //	FIXME: How do we handle the nIndex member?
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      portIndex = pVideoPortFormat->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      CHECK_ERROR(err, "Parameter Check");
      port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[portIndex];
      if(portIndex != 0) {
        return OMX_ErrorBadPortIndex;
      }
      if (pVideoPortFormat->eCompressionFormat != OMX_VIDEO_CodingUnused)	{
        //	No compression allowed
        return OMX_ErrorUnsupportedSetting;
      }
      port->sVideoParam.eCompressionFormat = pVideoPortFormat->eCompressionFormat;
      port->sVideoParam.eColorFormat = pVideoPortFormat->eColorFormat;
      //	Figure out stride, slice height, min buffer size
      port->sPortParam.format.video.nStride = calcStride(port->sPortParam.format.video.nFrameWidth, port->sVideoParam.eColorFormat);
      port->sPortParam.format.video.nSliceHeight = port->sPortParam.format.video.nFrameHeight;	//	No support for slices yet
      port->sPortParam.nBufferSize = (OMX_U32) abs(port->sPortParam.format.video.nStride) * port->sPortParam.format.video.nSliceHeight;
      break;
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}


OMX_ERRORTYPE omx_fbdev_sink_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;	

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_fbdev_sink_component_PortType *port = (omx_fbdev_sink_component_PortType *) omx_fbdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];	
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
      memcpy(ComponentParameterStructure, &omx_fbdev_sink_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
      break;		
    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      setHeader(pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      if (pVideoPortFormat->nPortIndex < 1) {
        memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;	
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}


OMX_ERRORTYPE omx_fbdev_sink_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {

  omx_fbdev_sink_component_PrivateType* omx_fbdev_sink_component_Private = (omx_fbdev_sink_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eState;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
  eState = omx_fbdev_sink_component_Private->state; //storing current state

  if (message->messageType == OMX_CommandStateSet){
    //if ((message->messageParam == OMX_StateIdle ) && (omx_fbdev_sink_component_Private->state == OMX_StateLoaded)) {
    if ((message->messageParam == OMX_StateExecuting ) && (omx_fbdev_sink_component_Private->state == OMX_StateIdle)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s sink component from loaded to idle \n", __func__);
      err = omx_fbdev_sink_component_Init(openmaxStandComp);
      CHECK_ERROR(err,"Video Sink Init Failed");
    } 
  }
  // Execute the base message handling
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet) {
    if ((message->messageParam == OMX_StateIdle ) && (omx_fbdev_sink_component_Private->state == OMX_StateIdle) && eState == OMX_StateExecuting) {
      err = omx_fbdev_sink_component_Deinit(openmaxStandComp);
      CHECK_ERROR(err,"Video Sink Deinit Failed");
    }
  }
  return err;
}
