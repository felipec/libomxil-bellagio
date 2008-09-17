/**
  @file src/components/jpeg/omx_jpegenc_component.c

  This component implements an JPEG encoder based on Tom Lane's jpeg library (http://www.ijg.org/files/)

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

  $Date: 2008-09-10 14:46:18 +0530 (Wed, 10 Sep 2008) $
  Revision $Rev: 610 $
  Author $Author: pankaj_sen $

*/

#include <omxcore.h>
#include <omx_base_image_port.h>

#include <ctype.h>    /* to declare isprint() */
#include "omx_jpegenc_component.h"

/** Maximum Number of Image Mad Decoder Component Instance*/
#define MAX_COMPONENT_JPEGDEC 4

/** Number of Mad Component Instance*/
static OMX_U32 nojpegencInstance=0;

/* Create the add-on message string table. */

#define JMESSAGE(code,string)  string ,

static const char * const cdjpeg_message_table[] = {
  NULL
};

static unsigned char *dest=NULL;
static int len =0;
static int destlen = 0;

static void mem_init_destination(j_compress_ptr cinfo) 
{ 
  struct jpeg_destination_mgr*dmgr = 
      (struct jpeg_destination_mgr*)(cinfo->dest);
  dmgr->next_output_byte = dest;
  dmgr->free_in_buffer = destlen;

  DEBUG(DEB_LEV_ERR, "In %s: free_in_buffer=%d next_output_byte=%x\n", __func__,dmgr->free_in_buffer,(int)dmgr->next_output_byte);

}

static boolean mem_empty_output_buffer(j_compress_ptr cinfo)
{ 
    DEBUG(DEB_LEV_ERR,"jpeg mem overflow!\n");
    exit(1);
}

static void mem_term_destination(j_compress_ptr cinfo) 
{ 
  struct jpeg_destination_mgr*dmgr = 
      (struct jpeg_destination_mgr*)(cinfo->dest);
  len = destlen - dmgr->free_in_buffer;

  DEBUG(DEB_LEV_ERR, "In %s: destlen=%d free_in_buffer=%d len=%d\n", __func__,destlen,dmgr->free_in_buffer,len);

  dmgr->free_in_buffer = 0;
}
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
#define DEFAULT_FMT  FMT_BMP
#endif

extern cjpeg_source_ptr jinit_read_bmp_mod (j_compress_ptr cinfo,unsigned char *inputbuffer,int inputLen);

/** The Constructor 
  *
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName name of the component to be constructed
  */
OMX_ERRORTYPE omx_jpegenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {
  
  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private;
  omx_base_image_PortType *pInPort,*pOutPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_jpegenc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)  {
      return OMX_ErrorInsufficientResources;
    }
  }  else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", 
              __func__, (int)openmaxStandComp->pComponentPrivate);
  }
  
  omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_jpegenc_component_Private->ports = NULL;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?  
    */
  err = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "constructor of mad encoder component is called\n");

  /** Domain specific section for the ports. */  
  /** first we set the parameter common to both formats
    * parameters related to input port which does not depend upon input image format
    */
  omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nStartPortNumber = 0;
  omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts = 2;

  /** Allocate Ports and call port constructor. */  
  if (omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts && !omx_jpegenc_component_Private->ports) {
    omx_jpegenc_component_Private->ports = calloc(omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts, sizeof(omx_base_PortType *));
    if (!omx_jpegenc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts; i++) {
      omx_jpegenc_component_Private->ports[i] = calloc(1, sizeof(omx_base_image_PortType));
      if (!omx_jpegenc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_image_port_Constructor(openmaxStandComp, &omx_jpegenc_component_Private->ports[0], 0, OMX_TRUE);
  base_image_port_Constructor(openmaxStandComp, &omx_jpegenc_component_Private->ports[1], 1, OMX_FALSE);
    
  /** parameters related to input port */
  pInPort = (omx_base_image_PortType *) omx_jpegenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  pInPort->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  strcpy(pInPort->sPortParam.format.image.cMIMEType, "image/mpeg");
  pInPort->sPortParam.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
  pInPort->sImageParam.eCompressionFormat = OMX_IMAGE_CodingJPEG;
  pInPort->sPortParam.nBufferCountActual = 1;
  pInPort->sPortParam.nBufferCountMin = 1;

  /** parameters related to output port */
  pOutPort = (omx_base_image_PortType *) omx_jpegenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  pOutPort->sPortParam.nBufferSize = ENC_OUT_BUFFER_SIZE;
  strcpy(pOutPort->sPortParam.format.image.cMIMEType, "image/rgb");
  pOutPort->sPortParam.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
  pOutPort->sImageParam.eCompressionFormat = OMX_IMAGE_CodingUnused;
  pOutPort->sPortParam.format.image.nFrameWidth=0;
  pOutPort->sPortParam.format.image.nFrameHeight=0;
  pOutPort->sPortParam.nBufferCountActual = 1;
  pOutPort->sPortParam.nBufferCountMin = 1;

  /** initialise the semaphore to be used for mad encoder access synchronization */
  if(!omx_jpegenc_component_Private->jpegencSyncSem) {
    omx_jpegenc_component_Private->jpegencSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_jpegenc_component_Private->jpegencSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_jpegenc_component_Private->jpegencSyncSem, 0);
  }

  if(!omx_jpegenc_component_Private->jpegencSyncSem1) {
    omx_jpegenc_component_Private->jpegencSyncSem1 = calloc(1,sizeof(tsem_t));
    if(omx_jpegenc_component_Private->jpegencSyncSem1 == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_jpegenc_component_Private->jpegencSyncSem1, 0);
  }

  /** general configuration irrespective of any image formats
    *  setting values of other fields of omx_jpegenc_component_Private structure  
    */ 
  omx_jpegenc_component_Private->jpegencReady = OMX_FALSE;
  omx_jpegenc_component_Private->hMarkTargetComponent = NULL;
  omx_jpegenc_component_Private->nFlags = 0x0;
  omx_jpegenc_component_Private->BufferMgmtFunction = omx_jpegenc_component_BufferMgmtFunction;
  omx_jpegenc_component_Private->messageHandler = omx_jpegenc_encoder_MessageHandler;
  omx_jpegenc_component_Private->destructor = omx_jpegenc_component_Destructor;
  openmaxStandComp->SetParameter = omx_jpegenc_component_SetParameter;
  openmaxStandComp->GetParameter = omx_jpegenc_component_GetParameter;

  nojpegencInstance++;

  if(nojpegencInstance>MAX_COMPONENT_JPEGDEC)
    return OMX_ErrorInsufficientResources;

  /** initialising mad structures */

  return err;
}

/** The destructor */
OMX_ERRORTYPE omx_jpegenc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_jpegenc_component_Private->jpegencSyncSem) {
    tsem_deinit(omx_jpegenc_component_Private->jpegencSyncSem);
    free(omx_jpegenc_component_Private->jpegencSyncSem);
    omx_jpegenc_component_Private->jpegencSyncSem = NULL;
  }
  
  if(omx_jpegenc_component_Private->jpegencSyncSem1) {
    tsem_deinit(omx_jpegenc_component_Private->jpegencSyncSem1);
    free(omx_jpegenc_component_Private->jpegencSyncSem1);
    omx_jpegenc_component_Private->jpegencSyncSem1 = NULL;
  }

  /* frees port/s */
  if (omx_jpegenc_component_Private->ports) {
    for (i=0; i < omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts; i++) {
      if(omx_jpegenc_component_Private->ports[i]) {
        omx_jpegenc_component_Private->ports[i]->PortDestructor(omx_jpegenc_component_Private->ports[i]);
      }
    }
    free(omx_jpegenc_component_Private->ports);
    omx_jpegenc_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of mad encoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  nojpegencInstance--;

  return OMX_ErrorNone;

}

/** The Initialization function  */
OMX_ERRORTYPE omx_jpegenc_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)  {

  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  /* Initialize the JPEG compression object with default error handling. */
  omx_jpegenc_component_Private->cinfo.err = jpeg_std_error(&omx_jpegenc_component_Private->jerr);
  jpeg_create_compress(&omx_jpegenc_component_Private->cinfo);
  /* Add some application-specific error messages (from cderror.h) */
  omx_jpegenc_component_Private->jerr.addon_message_table = cdjpeg_message_table;
  omx_jpegenc_component_Private->jerr.first_addon_message = JMSG_FIRSTADDONCODE;
  omx_jpegenc_component_Private->jerr.last_addon_message  = JMSG_LASTADDONCODE;

  /* Now safe to enable signal catcher. */
#ifdef NEED_SIGNAL_CATCHER
  enable_signal_catcher((j_common_ptr) &cinfo);
#endif

  /* Initialize JPEG parameters.
   * Much of this may be overridden later.
   * In particular, we don't yet know the input file's color space,
   * but we need to provide some value for jpeg_set_defaults() to work.
   */

  omx_jpegenc_component_Private->cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
  jpeg_set_defaults(&omx_jpegenc_component_Private->cinfo);
  //omx_jpegenc_component_Private->cinfo.dct_method = JDCT_IFAST;
  //int quality = 50;
  //jpeg_set_quality(&omx_jpegenc_component_Private->cinfo,quality,TRUE);

  omx_jpegenc_component_Private->isFirstBuffer = 1;
  omx_jpegenc_component_Private->pInBuffer = NULL;

  return err;
}

/** The Deinitialization function  */
OMX_ERRORTYPE omx_jpegenc_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  //omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
   
  return err;
}

/** this function sets the parameter values regarding image format & index */
OMX_ERRORTYPE omx_jpegenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)  {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_IMAGE_PARAM_PORTFORMATTYPE *pImagePortFormat;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
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
      port = (omx_base_image_PortType *) omx_jpegenc_component_Private->ports[portIndex];
      memcpy(&port->sImageParam, pImagePortFormat, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp( (char*) pComponentRole->cRole, IMAGE_ENC_JPEG_ROLE)) {
      omx_jpegenc_component_Private->image_coding_type = OMX_IMAGE_CodingJPEG;
    }  else {
      return OMX_ErrorBadParameter;
    }
    //omx_jpegenc_component_SetInternalParameters(openmaxStandComp);
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
  
}

/** this function gets the parameters regarding image formats and index */
OMX_ERRORTYPE omx_jpegenc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)  {

  OMX_IMAGE_PARAM_PORTFORMATTYPE *pImagePortFormat;  
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  omx_base_image_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;
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
    memcpy(ComponentParameterStructure, &omx_jpegenc_component_Private->sPortTypesParam[OMX_PortDomainImage], sizeof(OMX_PORT_PARAM_TYPE));
    break;    

  case OMX_IndexParamImagePortFormat:
    pImagePortFormat = (OMX_IMAGE_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pImagePortFormat->nPortIndex <= 1) {
      port = (omx_base_image_PortType *)omx_jpegenc_component_Private->ports[pImagePortFormat->nPortIndex];
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
    if (omx_jpegenc_component_Private->image_coding_type == OMX_IMAGE_CodingJPEG) {
      strcpy( (char*) pComponentRole->cRole, IMAGE_ENC_JPEG_ROLE);
    }  else {
      strcpy( (char*) pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
  
}

void* omx_jpegenc_component_BufferMgmtFunction(void* param)
{
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = openmaxStandComp->pComponentPrivate;

  omx_base_PortType *pInPort=(omx_base_PortType *)omx_jpegenc_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_jpegenc_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  tsem_t* pOutputSem = pOutPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  queue_t* pOutputQueue = pOutPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pOutputBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE,isOutputBufferNeeded=OMX_TRUE;
  int inBufExchanged=0,outBufExchanged=0,i;
  static OMX_S32 first=1;
  JDIMENSION num_scanlines;
  OMX_S32 width = 240 , height =  320;
    
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  while(omx_jpegenc_component_Private->state == OMX_StateIdle || omx_jpegenc_component_Private->state == OMX_StateExecuting ||  
        omx_jpegenc_component_Private->state == OMX_StatePause || 
        omx_jpegenc_component_Private->transientState == OMX_TransStateLoadedToIdle) {

    pthread_mutex_lock(&omx_jpegenc_component_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort) || 
           PORT_IS_BEING_FLUSHED(pOutPort)) {
      pthread_mutex_unlock(&omx_jpegenc_component_Private->flush_mutex);
      
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
  
      tsem_up(omx_jpegenc_component_Private->flush_all_condition);
      tsem_down(omx_jpegenc_component_Private->flush_condition);
      pthread_mutex_lock(&omx_jpegenc_component_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_jpegenc_component_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if((isInputBufferNeeded==OMX_TRUE && pInputSem->semval==0) && 
      (omx_jpegenc_component_Private->state != OMX_StateLoaded && omx_jpegenc_component_Private->state != OMX_StateInvalid)) {
      //Signalled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next input/output buffer\n");
      tsem_down(omx_jpegenc_component_Private->bMgmtSem);
      
    }
    if(omx_jpegenc_component_Private->state == OMX_StateLoaded || omx_jpegenc_component_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    /* Wait for buffers to be available on the output port */
    DEBUG(DEB_LEV_FULL_SEQ, "In %s: waiting on output port for some buffers to fill \n", __func__);

    if((isOutputBufferNeeded==OMX_TRUE /*&& pOutputSem->semval==0*/) && 
      (omx_jpegenc_component_Private->state != OMX_StateLoaded && omx_jpegenc_component_Private->state != OMX_StateInvalid) &&
       !(PORT_IS_BEING_FLUSHED(pInPort) || PORT_IS_BEING_FLUSHED(pOutPort))) {
      //Signalled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next input/output buffer\n");
      tsem_down(omx_jpegenc_component_Private->bMgmtSem);
      
    }
    if(omx_jpegenc_component_Private->state == OMX_StateLoaded || omx_jpegenc_component_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    if(pInputSem->semval>0 && isInputBufferNeeded==OMX_TRUE ) {
      tsem_down(pInputSem);
      if(pInputQueue->nelem>0){
        inBufExchanged++;
        isInputBufferNeeded=OMX_FALSE;
        pInputBuffer = dequeue(pInputQueue);
        if(pInputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL input buffer!!\n");
          break;
        }
      }
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

    if(first==1 && isOutputBufferNeeded==OMX_FALSE && isInputBufferNeeded==OMX_FALSE && pInputBuffer->nFilledLen != 0) {
      first=2;
      
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: input buffer fill length=%d\n", __func__,(int)pInputBuffer->nFilledLen);

      omx_jpegenc_component_Private->src_mgr = jinit_read_bmp_mod (&omx_jpegenc_component_Private->cinfo,(unsigned char*)pInputBuffer->pBuffer,pInputBuffer->nFilledLen);
      omx_jpegenc_component_Private->src_mgr->input_file = NULL;

      /* Read the input file header to obtain file size & colorspace. */
      (*omx_jpegenc_component_Private->src_mgr->start_input) (&omx_jpegenc_component_Private->cinfo, omx_jpegenc_component_Private->src_mgr);

      /* Now that we know input colorspace, fix colorspace-dependent defaults */
      jpeg_default_colorspace(&omx_jpegenc_component_Private->cinfo);

      dest= pOutputBuffer->pBuffer;
      destlen = pOutputBuffer->nAllocLen;
      omx_jpegenc_component_Private->dest_mgr.init_destination = mem_init_destination;
      omx_jpegenc_component_Private->dest_mgr.empty_output_buffer = mem_empty_output_buffer;
      omx_jpegenc_component_Private->dest_mgr.term_destination = mem_term_destination;
      omx_jpegenc_component_Private->cinfo.dest = &omx_jpegenc_component_Private->dest_mgr;

      jpeg_start_compress(&omx_jpegenc_component_Private->cinfo, TRUE);

      /* Process data */
      while (omx_jpegenc_component_Private->cinfo.next_scanline < omx_jpegenc_component_Private->cinfo.image_height) {
        num_scanlines = (*omx_jpegenc_component_Private->src_mgr->get_pixel_rows) (&omx_jpegenc_component_Private->cinfo, omx_jpegenc_component_Private->src_mgr);
        DEBUG(DEB_LEV_FULL_SEQ, "In %s: input buffer fill length=%d\n", __func__,(int)pInputBuffer->nFilledLen);
        (void) jpeg_write_scanlines(&omx_jpegenc_component_Private->cinfo, omx_jpegenc_component_Private->src_mgr->buffer, num_scanlines);
      }
      
      (*omx_jpegenc_component_Private->src_mgr->finish_input) (&omx_jpegenc_component_Private->cinfo, omx_jpegenc_component_Private->src_mgr);
      DEBUG(DEB_ALL_MESS,"Calling Jpeg Finish len=%d\n",len);
      jpeg_finish_compress(&omx_jpegenc_component_Private->cinfo);
      DEBUG(DEB_ALL_MESS,"Calling Jpeg Destroy\n");
      jpeg_destroy_compress(&omx_jpegenc_component_Private->cinfo);
      DEBUG(DEB_ALL_MESS,"Called Jpeg Destroy\n");

      pOutputBuffer->nFilledLen = len;
      pInputBuffer->nFilledLen = 0;
 
      if(omx_jpegenc_component_Private->pMark.hMarkTargetComponent != NULL){
        pOutputBuffer->hMarkTargetComponent = omx_jpegenc_component_Private->pMark.hMarkTargetComponent;
        pOutputBuffer->pMarkData            = omx_jpegenc_component_Private->pMark.pMarkData;
        omx_jpegenc_component_Private->pMark.hMarkTargetComponent = NULL;
        omx_jpegenc_component_Private->pMark.pMarkData            = NULL;
      }
    
      if(omx_jpegenc_component_Private->hMarkTargetComponent==(OMX_COMPONENTTYPE *)openmaxStandComp) {
        /*Clear the mark and generate an event*/
        (*(omx_jpegenc_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_jpegenc_component_Private->callbackData,
          OMX_EventMark, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          0, /* The state has been changed in message->messageParam2 */
          omx_jpegenc_component_Private->pMarkData);
      } else if(omx_jpegenc_component_Private->hMarkTargetComponent!=NULL){
        /*If this is not the target component then pass the mark*/
        pOutputBuffer->hMarkTargetComponent     = omx_jpegenc_component_Private->hMarkTargetComponent;
        pOutputBuffer->pMarkData                = omx_jpegenc_component_Private->pMarkData;
        omx_jpegenc_component_Private->pMarkData= NULL;
      }
    

      if(omx_jpegenc_component_Private->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_FULL_SEQ, "Detected EOS flags in input buffer filled\n");
        pOutputBuffer->nFlags=omx_jpegenc_component_Private->nFlags;
        (*(omx_jpegenc_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_jpegenc_component_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          pOutputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
      }
    }
    if(isInputBufferNeeded == OMX_FALSE && pInputBuffer->nFilledLen==0) {
      pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
      inBufExchanged--;
      pInputBuffer=NULL;
      isInputBufferNeeded = OMX_TRUE;
    }

    /* We are done with output buffer */
    /*If EOS and Input buffer Filled Len Zero then Return output buffer immediately*/
    if(isOutputBufferNeeded==OMX_FALSE) {
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

OMX_ERRORTYPE omx_jpegenc_encoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp, internalRequestMessageType *message)  {

  omx_jpegenc_component_PrivateType* omx_jpegenc_component_Private = (omx_jpegenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;  
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_jpegenc_component_Private->state;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_jpegenc_component_Private->state == OMX_StateLoaded)) {
      err = omx_jpegenc_component_Init(openmaxStandComp);
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
      err = omx_jpegenc_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s MAD Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }
  }

  return err;  
}



