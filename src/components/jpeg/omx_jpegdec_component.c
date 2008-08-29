/**
  @file src/components/jpeg/omx_jpegdec_component.c

  This component implements an JPEG decoder based on Tom Lane's jpeg library (http://www.ijg.org/files/)

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

#include <omxcore.h>
#include <omx_base_image_port.h>

#include <ctype.h>    /* to declare isprint() */
#include "omx_jpegdec_component.h"

/** Maximum Number of Image Mad Decoder Component Instance*/
#define MAX_COMPONENT_JPEGDEC 4

/** Number of Mad Component Instance*/
static OMX_U32 nojpegdecInstance=0;

/* Create the add-on message string table. */

#define JMESSAGE(code,string)  string ,

static const char * const cdjpeg_message_table[] = {
#include "cderror.h"
  NULL
};

/*
 * This list defines the known output image formats
 * (not all of which need be supported by a given version).
 * You can change the default output format by defining DEFAULT_FMT;
 * indeed, you had better do so if you undefine PPM_SUPPORTED.
 */

typedef enum {
  FMT_BMP,    /* BMP format (Windows flavor) */
  FMT_GIF,    /* GIF format */
  FMT_OS2,    /* BMP format (OS/2 flavor) */
  FMT_PPM,    /* PPM/PGM (PBMPLUS formats) */
  FMT_RLE,    /* RLE format */
  FMT_TARGA,    /* Targa format */
  FMT_TIFF    /* TIFF format */
} IMAGE_FORMATS;

#ifndef DEFAULT_FMT    /* so can override from CFLAGS in Makefile */
#define DEFAULT_FMT  FMT_PPM
#endif

static IMAGE_FORMATS requested_fmt;
static int print_text_marker (j_decompress_ptr cinfo);
static unsigned int jpeg_getc (j_decompress_ptr cinfo);

/* Expanded data source object for stdio input */

typedef struct {
  struct jpeg_source_mgr pub;  /* public fields */

  JOCTET * buffer;    /* start of buffer */
  int buffer_len;    /* length of buffer */
  boolean start_of_file;  /* have we gotten any data yet? */
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private;
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096  /* choose an efficiently fread'able size */

/** The Constructor 
  *
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName name of the component to be constructed
  */
OMX_ERRORTYPE omx_jpegdec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {
  
  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private;
  omx_base_image_PortType *pInPort,*pOutPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_jpegdec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)  {
      return OMX_ErrorInsufficientResources;
    }
  }  else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", 
              __func__, (int)openmaxStandComp->pComponentPrivate);
  }
  
  omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_jpegdec_component_Private->ports = NULL;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?  
    */
  err = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "constructor of mad decoder component is called\n");

  /** Domain specific section for the ports. */  
  /** first we set the parameter common to both formats
    * parameters related to input port which does not depend upon input image format
    */
  omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nStartPortNumber = 0;
  omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts = 2;

  /** Allocate Ports and call port constructor. */  
  if (omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts && !omx_jpegdec_component_Private->ports) {
    omx_jpegdec_component_Private->ports = calloc(omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts, sizeof(omx_base_PortType *));
    if (!omx_jpegdec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts; i++) {
      omx_jpegdec_component_Private->ports[i] = calloc(1, sizeof(omx_base_image_PortType));
      if (!omx_jpegdec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_image_port_Constructor(openmaxStandComp, &omx_jpegdec_component_Private->ports[0], 0, OMX_TRUE);
  base_image_port_Constructor(openmaxStandComp, &omx_jpegdec_component_Private->ports[1], 1, OMX_FALSE);
    
  /** parameters related to input port */
  pInPort = (omx_base_image_PortType *) omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  pInPort->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  strcpy(pInPort->sPortParam.format.image.cMIMEType, "image/mpeg");
  pInPort->sPortParam.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
  pInPort->sImageParam.eCompressionFormat = OMX_IMAGE_CodingJPEG;

  /** parameters related to output port */
  pOutPort = (omx_base_image_PortType *) omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  pOutPort->sPortParam.nBufferSize = OUT_BUFFER_SIZE;
  strcpy(pOutPort->sPortParam.format.image.cMIMEType, "image/rgb");
  pOutPort->sPortParam.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
  pOutPort->sImageParam.eCompressionFormat = OMX_IMAGE_CodingUnused;
  pOutPort->sPortParam.format.image.nFrameWidth=0;
  pOutPort->sPortParam.format.image.nFrameHeight=0;
  pOutPort->sPortParam.nBufferCountActual = 1;
  pOutPort->sPortParam.nBufferCountMin = 1;

  /** initialise the semaphore to be used for mad decoder access synchronization */
  if(!omx_jpegdec_component_Private->jpegdecSyncSem) {
    omx_jpegdec_component_Private->jpegdecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_jpegdec_component_Private->jpegdecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_jpegdec_component_Private->jpegdecSyncSem, 0);
  }

  if(!omx_jpegdec_component_Private->jpegdecSyncSem1) {
    omx_jpegdec_component_Private->jpegdecSyncSem1 = calloc(1,sizeof(tsem_t));
    if(omx_jpegdec_component_Private->jpegdecSyncSem1 == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_jpegdec_component_Private->jpegdecSyncSem1, 0);
  }

  /** general configuration irrespective of any image formats
    *  setting values of other fields of omx_jpegdec_component_Private structure  
    */ 
  omx_jpegdec_component_Private->jpegdecReady = OMX_FALSE;
  omx_jpegdec_component_Private->hMarkTargetComponent = NULL;
  omx_jpegdec_component_Private->nFlags = 0x0;
  //omx_jpegdec_component_Private->BufferMgmtCallback = omx_jpegdec_component_BufferMgmtCallback;
  omx_jpegdec_component_Private->BufferMgmtFunction = omx_jpegdec_component_BufferMgmtFunction;
  omx_jpegdec_component_Private->messageHandler = omx_jpegdec_decoder_MessageHandler;
  omx_jpegdec_component_Private->destructor = omx_jpegdec_component_Destructor;
  openmaxStandComp->SetParameter = omx_jpegdec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_jpegdec_component_GetParameter;

  nojpegdecInstance++;

  if(nojpegdecInstance>MAX_COMPONENT_JPEGDEC)
    return OMX_ErrorInsufficientResources;

  /** initialising mad structures */

  return err;
}

/** The destructor */
OMX_ERRORTYPE omx_jpegdec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_jpegdec_component_Private->jpegdecSyncSem) {
    tsem_deinit(omx_jpegdec_component_Private->jpegdecSyncSem);
    free(omx_jpegdec_component_Private->jpegdecSyncSem);
    omx_jpegdec_component_Private->jpegdecSyncSem = NULL;
  }
  
  if(omx_jpegdec_component_Private->jpegdecSyncSem1) {
    tsem_deinit(omx_jpegdec_component_Private->jpegdecSyncSem1);
    free(omx_jpegdec_component_Private->jpegdecSyncSem1);
    omx_jpegdec_component_Private->jpegdecSyncSem1 = NULL;
  }

  /* frees port/s */
  if (omx_jpegdec_component_Private->ports) {
    for (i=0; i < omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts; i++) {
      if(omx_jpegdec_component_Private->ports[i]) {
        omx_jpegdec_component_Private->ports[i]->PortDestructor(omx_jpegdec_component_Private->ports[i]);
      }
    }
    free(omx_jpegdec_component_Private->ports);
    omx_jpegdec_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of mad decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  nojpegdecInstance--;

  return OMX_ErrorNone;

}

/** The Initialization function  */
OMX_ERRORTYPE omx_jpegdec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)  {

  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  /* Initialize the JPEG decompression object with default error handling. */
  omx_jpegdec_component_Private->cinfo.err = jpeg_std_error(&omx_jpegdec_component_Private->jerr);
  jpeg_create_decompress(&omx_jpegdec_component_Private->cinfo);
  /* Add some application-specific error messages (from cderror.h) */
  omx_jpegdec_component_Private->jerr.addon_message_table = cdjpeg_message_table;
  omx_jpegdec_component_Private->jerr.first_addon_message = JMSG_FIRSTADDONCODE;
  omx_jpegdec_component_Private->jerr.last_addon_message = JMSG_LASTADDONCODE;

  /* Insert custom marker processor for COM and APP12.
   * APP12 is used by some digital camera makers for textual info,
   * so we provide the ability to display it as text.
   * If you like, additional APPn marker types can be selected for display,
   * but don't try to override APP0 or APP14 this way (see libjpeg.doc).
   */
  jpeg_set_marker_processor(&omx_jpegdec_component_Private->cinfo, JPEG_COM, print_text_marker);
  jpeg_set_marker_processor(&omx_jpegdec_component_Private->cinfo, JPEG_APP0+12, print_text_marker);

  omx_jpegdec_component_Private->cinfo.err->trace_level = 0;
  omx_jpegdec_component_Private->isFirstBuffer = 1;
  omx_jpegdec_component_Private->pInBuffer = NULL;

  requested_fmt = FMT_BMP;

  return err;
}

/** The Deinitialization function  */
OMX_ERRORTYPE omx_jpegdec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  //omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
   
  return err;
}

/** Called whenever an unrecoverable error is detected
 * It causes the process to leave
 */
void jpegDecPanic()
{
  exit(EXIT_FAILURE);
}


/** this function sets the parameter values regarding image format & index */
OMX_ERRORTYPE omx_jpegdec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)  {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_IMAGE_PARAM_PORTFORMATTYPE *pImagePortFormat;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_image_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
  case OMX_IndexParamImagePortFormat:
    pImagePortFormat = (OMX_IMAGE_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pImagePortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pImagePortFormat, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    if (portIndex <= 1) {
      port = (omx_base_image_PortType *) omx_jpegdec_component_Private->ports[portIndex];
      memcpy(&port->sImageParam, pImagePortFormat, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp( (char*) pComponentRole->cRole, IMAGE_DEC_JPEG_ROLE)) {
      omx_jpegdec_component_Private->image_coding_type = OMX_IMAGE_CodingJPEG;
    }  else {
      return OMX_ErrorBadParameter;
    }
    //omx_jpegdec_component_SetInternalParameters(openmaxStandComp);
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
  
}

/** this function gets the parameters regarding image formats and index */
OMX_ERRORTYPE omx_jpegdec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)  {

  OMX_IMAGE_PARAM_PORTFORMATTYPE *pImagePortFormat;  
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  omx_base_image_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamImageInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_jpegdec_component_Private->sPortTypesParam[OMX_PortDomainImage], sizeof(OMX_PORT_PARAM_TYPE));
    break;    

  case OMX_IndexParamImagePortFormat:
    pImagePortFormat = (OMX_IMAGE_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pImagePortFormat->nPortIndex <= 1) {
      port = (omx_base_image_PortType *)omx_jpegdec_component_Private->ports[pImagePortFormat->nPortIndex];
      memcpy(pImagePortFormat, &port->sImageParam, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;    

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (omx_jpegdec_component_Private->image_coding_type == OMX_IMAGE_CodingJPEG) {
      strcpy( (char*) pComponentRole->cRole, IMAGE_DEC_JPEG_ROLE);
    }  else {
      strcpy( (char*) pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
  
}

/*
 * Marker processor for COM and interesting APPn markers.
 * This replaces the library's built-in processor, which just skips the marker.
 * We want to print out the marker as text, to the extent possible.
 * Note this code relies on a non-suspending data source.
 */

static unsigned int jpeg_getc (j_decompress_ptr cinfo)
/* Read next byte */
{
  struct jpeg_source_mgr * datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (cinfo))
      ERREXIT(cinfo, JERR_CANT_SUSPEND);
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}

static int print_text_marker (j_decompress_ptr cinfo)
{
  boolean traceit = (cinfo->err->trace_level >= 1);
  INT32 length;
  OMX_U32 ch;
  OMX_U32 lastch = 0;

  length = jpeg_getc(cinfo) << 8;
  length += jpeg_getc(cinfo);
  length -= 2;      /* discount the length word itself */

  if (traceit) {
    if (cinfo->unread_marker == JPEG_COM)
      DEBUG(DEB_LEV_PARAMS, "Comment, length %ld:\n", (long) length);
    else      /* assume it is an APPn otherwise */
      DEBUG(DEB_LEV_PARAMS, "APP%d, length %ld:\n",
        cinfo->unread_marker - JPEG_APP0, (long) length);
  }

  while (--length >= 0) {
    ch = jpeg_getc(cinfo);
    if (traceit) {
      /* Emit the character in a readable form.
       * Nonprintables are converted to \nnn form,
       * while \ is converted to \\.
       * Newlines in CR, CR/LF, or LF form will be printed as one newline.
       */
      if (ch == '\r') {
  DEBUG(DEB_LEV_PARAMS, "\n");
      } else if (ch == '\n') {
  if (lastch != '\r')
    DEBUG(DEB_LEV_PARAMS, "\n");
      } else if (ch == '\\') {
  DEBUG(DEB_LEV_PARAMS, "\\\\");
      } else if (isprint(ch)) {
  putc(ch, stderr);
      } else {
  DEBUG(DEB_LEV_PARAMS, "\\%03o", (unsigned int)ch);
      }
      lastch = ch;
    }
  }

  if (traceit)
    DEBUG(DEB_LEV_PARAMS, "\n");

  return TRUE;
}

void omx_jpegdec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate; 
  omx_base_image_PortType *pOutPort = (omx_base_image_PortType *)omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  int width,height;
  JDIMENSION num_scanlines;
  static int count=0;

  omx_jpegdec_component_Private->pInBuffer = pInputBuffer;
  
  DEBUG(DEB_LEV_ERR, "In %s: signalling buffer presence count=%d\n", __func__,count++);

  /*Signal fill_input_buffer*/
  tsem_up(omx_jpegdec_component_Private->jpegdecSyncSem);

  if(omx_jpegdec_component_Private->isFirstBuffer == 1) {
    omx_jpegdec_component_Private->isFirstBuffer = 0;
    DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);

    /* Get a new buffer from the output queue */
    
    jpeg_data_src (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private);

    jpeg_read_header (&omx_jpegdec_component_Private->cinfo, TRUE);

    DEBUG(DEB_LEV_ERR, "In %s (line %d)\n", __func__,__LINE__);

    omx_jpegdec_component_Private->dest_mgr = jinit_write_bmp(&omx_jpegdec_component_Private->cinfo, FALSE);
    omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);

    /* Start decompressor */
    (void) jpeg_start_decompress(&omx_jpegdec_component_Private->cinfo);

    DEBUG(DEB_LEV_ERR, "In %s (line %d)\n", __func__,__LINE__);

    /* Write output file header */
    (*omx_jpegdec_component_Private->dest_mgr->start_output) (&omx_jpegdec_component_Private->cinfo,omx_jpegdec_component_Private->dest_mgr);

    DEBUG(DEB_LEV_ERR, "In %s (line %d)\n", __func__,__LINE__);

    width = omx_jpegdec_component_Private->cinfo.output_width;
    height = omx_jpegdec_component_Private->cinfo.output_height;

    
    if((pOutPort->sPortParam.format.image.nFrameWidth != width) ||
       (pOutPort->sPortParam.format.image.nFrameHeight != width)) {
      pOutPort->sPortParam.format.image.nFrameWidth=width;
      pOutPort->sPortParam.format.image.nFrameHeight=height;
      pOutPort->sPortParam.nBufferSize= (width * height + width * height / 2)*2 + 54;

      /*Send Port Settings changed call back*/
      (*(omx_jpegdec_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
         omx_jpegdec_component_Private->callbackData,
         OMX_EventPortSettingsChanged, /* The command was completed */
         0, 
         1, /* This is the output port index */
         NULL);
      
      if(pOutputBuffer->nAllocLen< pOutPort->sPortParam.nBufferSize){ // 54 File header length
        DEBUG(DEB_LEV_ERR, "Output Buffer AllocLen %d less than required ouput %d", (int)pOutputBuffer->nAllocLen, (int)pOutPort->sPortParam.nBufferSize);
        //omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);
      }
    }
    pOutputBuffer->nFilledLen= (width * height + width * height / 2)*2 +54;
  }

  /*Wait for signal from fill_input_buffer*/
  tsem_down(omx_jpegdec_component_Private->jpegdecSyncSem1);

  DEBUG(DEB_LEV_ERR, "In %s Returning buffer\n", __func__);

  if(0) {
    /* Process data and produce output picture*/
    while (omx_jpegdec_component_Private->cinfo.output_scanline < omx_jpegdec_component_Private->cinfo.output_height) {
      num_scanlines = jpeg_read_scanlines(&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr->buffer,
            omx_jpegdec_component_Private->dest_mgr->buffer_height);
      (*omx_jpegdec_component_Private->dest_mgr->put_pixel_rows) (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr, num_scanlines);
    }
    
    /* Finish decompression and release memory.
     * I must do it in this order because output module has allocated memory
     * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
     */
    omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);

    (*omx_jpegdec_component_Private->dest_mgr->finish_output_buf) (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr,(char *)pOutputBuffer->pBuffer);

    
    (void) jpeg_finish_decompress(&omx_jpegdec_component_Private->cinfo);
    jpeg_destroy_decompress(&omx_jpegdec_component_Private->cinfo);
  }

}

#if 1
void* omx_jpegdec_component_BufferMgmtFunction(void* param)
{
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = openmaxStandComp->pComponentPrivate;

  omx_base_PortType *pInPort=(omx_base_PortType *)omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  tsem_t* pOutputSem = pOutPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  queue_t* pOutputQueue = pOutPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pOutputBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE,isOutputBufferNeeded=OMX_TRUE;
  int inBufExchanged=0,outBufExchanged=0;
  static OMX_S32 first=1;
  JDIMENSION num_scanlines;
  OMX_S32 width, height;
    
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  while(omx_jpegdec_component_Private->state == OMX_StateIdle || omx_jpegdec_component_Private->state == OMX_StateExecuting ||  
        omx_jpegdec_component_Private->state == OMX_StatePause || 
        omx_jpegdec_component_Private->transientState == OMX_TransStateLoadedToIdle) {

    pthread_mutex_lock(&omx_jpegdec_component_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort) || 
           PORT_IS_BEING_FLUSHED(pOutPort)) {
      pthread_mutex_unlock(&omx_jpegdec_component_Private->flush_mutex);
      
      DEBUG(DEB_LEV_FULL_SEQ, "In %s 1 signalling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n", 
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);

      if(isOutputBufferNeeded==OMX_FALSE && PORT_IS_BEING_FLUSHED(pOutPort)) {
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer=NULL;
        isOutputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning output buffer\n");
      }

      if(isInputBufferNeeded==OMX_FALSE && PORT_IS_BEING_FLUSHED(pInPort)) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
        isInputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning input buffer\n");
      }

      DEBUG(DEB_LEV_FULL_SEQ, "In %s 2 signalling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n", 
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);
  
      tsem_up(omx_jpegdec_component_Private->flush_all_condition);
      tsem_down(omx_jpegdec_component_Private->flush_condition);
      pthread_mutex_lock(&omx_jpegdec_component_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_jpegdec_component_Private->flush_mutex);

    /* Wait for buffers to be available on the output port */
    DEBUG(DEB_LEV_FULL_SEQ, "In %s: waiting on output port for some buffers to fill \n", __func__);

    if((isOutputBufferNeeded==OMX_TRUE /*&& pOutputSem->semval==0*/) && 
      (omx_jpegdec_component_Private->state != OMX_StateLoaded && omx_jpegdec_component_Private->state != OMX_StateInvalid) &&
       !(PORT_IS_BEING_FLUSHED(pInPort) || PORT_IS_BEING_FLUSHED(pOutPort))) {
      //Signalled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next input/output buffer\n");
      tsem_down(omx_jpegdec_component_Private->bMgmtSem);
      
    }
    if(omx_jpegdec_component_Private->state == OMX_StateLoaded || omx_jpegdec_component_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    /*When we have input buffer to process then get one output buffer*/
    if(pOutputSem->semval>0 && isOutputBufferNeeded==OMX_TRUE) {
      tsem_down(pOutputSem);
      if(pOutputQueue->nelem>0){
        outBufExchanged++;
        isOutputBufferNeeded=OMX_FALSE;
        pOutputBuffer = dequeue(pOutputQueue);
        if(pOutputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL output buffer!! op is=%d,iq=%d\n",pOutputSem->semval,pOutputQueue->nelem);
          break;
        }
      }
    }

    if(first==1 && isOutputBufferNeeded==OMX_FALSE) {
      first=2;
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);

      /* Get a new buffer from the output queue */
      
      jpeg_data_src (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private);

      jpeg_read_header (&omx_jpegdec_component_Private->cinfo, TRUE);

      omx_jpegdec_component_Private->dest_mgr = jinit_write_bmp(&omx_jpegdec_component_Private->cinfo, FALSE);
      omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);

      /* Start decompressor */
      (void) jpeg_start_decompress(&omx_jpegdec_component_Private->cinfo);

      /* Write output file header */
      (*omx_jpegdec_component_Private->dest_mgr->start_output) (&omx_jpegdec_component_Private->cinfo,omx_jpegdec_component_Private->dest_mgr);

      width = omx_jpegdec_component_Private->cinfo.output_width;
      height = omx_jpegdec_component_Private->cinfo.output_height;


      if((pOutPort->sPortParam.format.image.nFrameWidth != width) ||
         (pOutPort->sPortParam.format.image.nFrameHeight != width)) {
        pOutPort->sPortParam.format.image.nFrameWidth=width;
        pOutPort->sPortParam.format.image.nFrameHeight=height;
        pOutPort->sPortParam.nBufferSize= (width * height + width * height / 2)*2 + 54;

        /*Send Port Settings changed call back*/
        (*(omx_jpegdec_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
           omx_jpegdec_component_Private->callbackData,
           OMX_EventPortSettingsChanged, /* The command was completed */
           0, 
           1, /* This is the output port index */
           NULL);
        

        if(pOutputBuffer->nAllocLen< pOutPort->sPortParam.nBufferSize){ // 54 File header length
          DEBUG(DEB_LEV_ERR, "Output Buffer AllocLen %d less than required ouput %d\n", (int)pOutputBuffer->nAllocLen, (int)pOutPort->sPortParam.nBufferSize);
          //omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);
        }
      }
      pOutputBuffer->nFilledLen= (width * height + width * height / 2)*2 +54;
    }
    
    if(isOutputBufferNeeded==OMX_FALSE) {
    
    /* Process data */
    while (omx_jpegdec_component_Private->cinfo.output_scanline < omx_jpegdec_component_Private->cinfo.output_height) {
      num_scanlines = jpeg_read_scanlines(&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr->buffer,
            omx_jpegdec_component_Private->dest_mgr->buffer_height);
      (*omx_jpegdec_component_Private->dest_mgr->put_pixel_rows) (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr, num_scanlines);
    }
    
    /* Finish decompression and release memory.
     * I must do it in this order because output module has allocated memory
     * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
     */
    omx_jpegdec_component_Private->dest_mgr->buffer= &(pOutputBuffer->pBuffer);

    (*omx_jpegdec_component_Private->dest_mgr->finish_output_buf) (&omx_jpegdec_component_Private->cinfo, omx_jpegdec_component_Private->dest_mgr,(char *)pOutputBuffer->pBuffer);
    
    (void) jpeg_finish_decompress(&omx_jpegdec_component_Private->cinfo);
    jpeg_destroy_decompress(&omx_jpegdec_component_Private->cinfo);


    if(omx_jpegdec_component_Private->pMark!=NULL){
      pOutputBuffer->hMarkTargetComponent=omx_jpegdec_component_Private->hMarkTargetComponent;
      pOutputBuffer->pMarkData=omx_jpegdec_component_Private->pMark->pMarkData;
      omx_jpegdec_component_Private->pMark=NULL;
    }
    
    if(omx_jpegdec_component_Private->hMarkTargetComponent==(OMX_COMPONENTTYPE *)openmaxStandComp) {
      /*Clear the mark and generate an event*/
      (*(omx_jpegdec_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_jpegdec_component_Private->callbackData,
        OMX_EventMark, /* The command was completed */
        1, /* The commands was a OMX_CommandStateSet */
        0, /* The state has been changed in message->messageParam2 */
        omx_jpegdec_component_Private->pMarkData);
    } else if(omx_jpegdec_component_Private->hMarkTargetComponent!=NULL){
      /*If this is not the target component then pass the mark*/
      pOutputBuffer->hMarkTargetComponent     = omx_jpegdec_component_Private->hMarkTargetComponent;
      pOutputBuffer->pMarkData                = omx_jpegdec_component_Private->pMarkData;
      omx_jpegdec_component_Private->pMarkData= NULL;
    }
    

    if(omx_jpegdec_component_Private->nFlags==OMX_BUFFERFLAG_EOS) {
      DEBUG(DEB_LEV_FULL_SEQ, "Detected EOS flags in input buffer filled\n");
      pOutputBuffer->nFlags=omx_jpegdec_component_Private->nFlags;
      (*(omx_jpegdec_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_jpegdec_component_Private->callbackData,
        OMX_EventBufferFlag, /* The command was completed */
        1, /* The commands was a OMX_CommandStateSet */
        pOutputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
        NULL);
    }

    /* We are done with output buffer */
    /*If EOS and Input buffer Filled Len Zero then Return output buffer immediately*/
    if(pOutputBuffer->nFilledLen!=0 || pOutputBuffer->nFlags==OMX_BUFFERFLAG_EOS){
      pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
      outBufExchanged--;
      pOutputBuffer=NULL;
      isOutputBufferNeeded=OMX_TRUE;
    }
    }
    
  }
  
  return 0;
}

#endif

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

static void
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  src->start_of_file = TRUE;
}


/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

static int
fill_input_buffer (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;
  size_t nbytes = 0;
  omx_jpegdec_component_PrivateType *omx_jpegdec_component_Private =  src->omx_jpegdec_component_Private;
#if 0
  tsem_t* jpegdecSyncSem = omx_jpegdec_component_Private->jpegdecSyncSem;
  tsem_t* jpegdecSyncSem1 = omx_jpegdec_component_Private->jpegdecSyncSem1;
#endif
  OMX_BUFFERHEADERTYPE* pInputBuffer = NULL;// = omx_jpegdec_component_Private->pInBuffer;
  static int count = 0;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_jpegdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  
#if 0
  pInputBuffer = omx_jpegdec_component_Private->pInBuffer;
  /*Wait for signal from buffer management call back*/
  tsem_down(jpegdecSyncSem);

  DEBUG(DEB_LEV_ERR, "In %s Buffer count=%d\n", __func__,count++);
  nbytes= pInputBuffer->nFilledLen;
  memcpy(src->buffer,pInputBuffer->pBuffer,nbytes);
  
  pInputBuffer->nFilledLen = 0;
  /*Signal Buffer Management Call back*/
  tsem_up(jpegdecSyncSem1);
#endif

Label1:

  tsem_down(omx_jpegdec_component_Private->bMgmtSem);
      
    
  if(omx_jpegdec_component_Private->state == OMX_StateLoaded || 
     omx_jpegdec_component_Private->state == OMX_StateInvalid ||
     omx_jpegdec_component_Private->transientState == OMX_TransStateIdleToLoaded) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
    //break;
  } else {

    DEBUG(DEB_LEV_FULL_SEQ, "Waiting for input buffer semval=%d count=%d\n",pInputSem->semval,count++);
    if(pInputSem->semval>0) {
      tsem_down(pInputSem);
      if(pInputQueue->nelem>0){
        //inBufExchanged++;
        //isInputBufferNeeded=OMX_FALSE;
        pInputBuffer = dequeue(pInputQueue);
        if(pInputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL input buffer!!\n");
          //break;
          nbytes = 0;
        } else {
          nbytes = pInputBuffer->nFilledLen;
          memcpy(src->buffer,pInputBuffer->pBuffer,nbytes);
          pInputBuffer->nFilledLen = 0;
          if(pInputBuffer->hMarkTargetComponent != NULL) {
            omx_jpegdec_component_Private->hMarkTargetComponent=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
            omx_jpegdec_component_Private->pMarkData = pInputBuffer->pMarkData;
            pInputBuffer->hMarkTargetComponent = NULL;
            pInputBuffer->pMarkData = NULL;
          }
          if(pInputBuffer->nFlags != 0x0) {
            omx_jpegdec_component_Private->nFlags = pInputBuffer->nFlags;
            pInputBuffer->nFlags = 0x0;
          }
        }
      }
    }
  }

  if(pInputBuffer!=NULL) {
    pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
    //pInputBuffer=NULL;
  }

  if(nbytes == 0) {
    if(omx_jpegdec_component_Private->state != OMX_StateLoaded && 
       omx_jpegdec_component_Private->state != OMX_StateInvalid &&
       omx_jpegdec_component_Private->transientState != OMX_TransStateIdleToLoaded) {
      goto Label1;
    }
  }
  if (nbytes <= 0) {
    DEBUG(DEB_LEV_ERR, "Received ZERO Byte\n");
    //if (src>start_of_file)  /* Treat empty input file as fatal error */
    ERREXIT(cinfo, JERR_INPUT_EMPTY);
    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    src->buffer[0] = (JOCTET) 0xFF;
    src->buffer[1] = (JOCTET) JPEG_EOI;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;
  //src->start_of_file = FALSE;

  return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}


/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.  That method assumes that no backtracking
 * is possible.
 */


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static void
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

/*
 * Prepare for input from a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing decompression.
 */

void jpeg_data_src (j_decompress_ptr cinfo, omx_jpegdec_component_PrivateType *omx_jpegdec_component_Private)
{
  my_src_ptr src;

  /* The source object and input buffer are made permanent so that a series
   * of JPEG images can be read from the same file by calling jpeg_stdio_src
   * only before the first one.  (If we discarded the buffer at the end of
   * one image, we'd likely lose the start of the next one.)
   * This makes it unsafe to use this manager and a different source
   * manager serially with the same JPEG object.  Caveat programmer.
   */
  if (cinfo->src == NULL) {  /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,sizeof(my_source_mgr));
    src = (my_src_ptr) cinfo->src;
    src->buffer = (JOCTET *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,INPUT_BUF_SIZE * sizeof(char));
  }

  src = (my_src_ptr) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
  src->omx_jpegdec_component_Private=omx_jpegdec_component_Private;
}

OMX_ERRORTYPE omx_jpegdec_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp, internalRequestMessageType *message)  {

  omx_jpegdec_component_PrivateType* omx_jpegdec_component_Private = (omx_jpegdec_component_PrivateType*)openmaxStandComp->pComponentPrivate;  
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_jpegdec_component_Private->state;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_jpegdec_component_Private->state == OMX_StateLoaded)) {
      err = omx_jpegdec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s MAD Decoder Init Failed Error=%x\n",__func__,err); 
        return err;
      }
    } 
  }
  /** Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp, message);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateLoaded) && (eCurrentState == OMX_StateIdle)) {
      err = omx_jpegdec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s MAD Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }
  }

  return err;  
}
