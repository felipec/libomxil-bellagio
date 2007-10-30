/**
  @file src/components/filereader/omx_filereader_component.c

  OpenMax file reader component. This component is an file reader that detects the input
  file format so that client calls the appropriate decoder.

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
#include <omx_filereader_component.h>

#define MAX_COMPONENT_FILEREADER 1

/** Maximum Number of FileReader Instance*/
OMX_U32 noFilereaderInstance=0;
#define DEFAULT_FILENAME_LENGTH 256

/** The Constructor 
 */
OMX_ERRORTYPE omx_filereader_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
 
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  OMX_S32 i;
  omx_filereader_component_PortType *pPort;
  omx_filereader_component_PrivateType* omx_filereader_component_Private;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_filereader_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  err = omx_base_source_Constructor(openmaxStandComp, cComponentName);
  omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  
  if (omx_filereader_component_Private->sPortTypesParam.nPorts && !omx_filereader_component_Private->ports) {
    omx_filereader_component_Private->ports = calloc(omx_filereader_component_Private->sPortTypesParam.nPorts, sizeof (omx_base_PortType *));

    if (!omx_filereader_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_filereader_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; 
        * size of the struct is for derived class pPort type
        * this could be refactored as a smarter factory function instead?
        */
      omx_filereader_component_Private->ports[i] = calloc(1, sizeof(omx_filereader_component_PortType));
      if (!omx_filereader_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  } else {
    DEBUG(DEB_LEV_ERR, "In %s Not allocated ports\n", __func__);
  }
  /** allocating file reader component output port */	 
  omx_filereader_component_Private->PortConstructor(openmaxStandComp, &omx_filereader_component_Private->ports[0], 0, OMX_FALSE);

  pPort = (omx_filereader_component_PortType *) omx_filereader_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  
  /** for the moment, the file reader works with audio component
  * so the domain is set to audio
  * in future it may be assigned after checking the input file format 
  * or there can be seperate file reader of video component
  */  
  pPort->sPortParam.eDomain = OMX_PortDomainAudio;
  pPort->sPortParam.format.audio.pNativeRender = 0;
  pPort->sPortParam.format.audio.cMIMEType = (OMX_STRING)malloc(sizeof(char)*DEFAULT_MIME_STRING_LENGTH);
  strcpy(pPort->sPortParam.format.audio.cMIMEType, "raw/audio");
  pPort->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
  pPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  omx_filereader_component_Private->BufferMgmtCallback = omx_filereader_component_BufferMgmtCallback;

  setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  pPort->sAudioParam.nPortIndex = 0;
  pPort->sAudioParam.nIndex = 0;
  //pPort->sAudioParam.eEncoding = 0;

  setHeader(&pPort->sTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
  pPort->sTimeStamp.nPortIndex=0;
  pPort->sTimeStamp.nTimestamp=0x0;

  omx_filereader_component_Private->destructor = omx_filereader_component_Destructor;
  omx_filereader_component_Private->messageHandler = omx_filereader_component_MessageHandler;

  noFilereaderInstance++;
  if(noFilereaderInstance > MAX_COMPONENT_FILEREADER) {
    return OMX_ErrorInsufficientResources;
  }

  openmaxStandComp->SetParameter  = omx_filereader_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_filereader_component_GetParameter;
  openmaxStandComp->SetConfig     = omx_filereader_component_SetConfig;
  openmaxStandComp->GetConfig     = omx_filereader_component_GetConfig;

  /* Write in the default paramenters */

  omx_filereader_component_Private->avformatReady = OMX_FALSE;
  if(!omx_filereader_component_Private->avformatSyncSem) {
    omx_filereader_component_Private->avformatSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_filereader_component_Private->avformatSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_filereader_component_Private->avformatSyncSem, 0);
  }
  omx_filereader_component_Private->sInputFileName = (char *)malloc(DEFAULT_FILENAME_LENGTH);
  memset(omx_filereader_component_Private->sInputFileName,0,DEFAULT_FILENAME_LENGTH);
  /*Default Coding type*/
  omx_filereader_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  return err;
}

/** The Destructor 
 */
OMX_ERRORTYPE omx_filereader_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
	omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  omx_filereader_component_PortType *pPort = (omx_filereader_component_PortType *)omx_filereader_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];

  if(pPort->sPortParam.format.audio.cMIMEType != NULL) {
    free(pPort->sPortParam.format.audio.cMIMEType);
    pPort->sPortParam.format.audio.cMIMEType = NULL;
  }

  if(omx_filereader_component_Private->avformatSyncSem) {
    tsem_deinit(omx_filereader_component_Private->avformatSyncSem);
    free(omx_filereader_component_Private->avformatSyncSem);
    omx_filereader_component_Private->avformatSyncSem=NULL;
  }

  if(omx_filereader_component_Private->sInputFileName) {
    free(omx_filereader_component_Private->sInputFileName);
  }
  
  noFilereaderInstance--;
  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);
  return omx_base_source_Destructor(openmaxStandComp);
}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_filereader_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  int error;
  OMX_VENDOR_EXTRADATATYPE *pExtraData;
	
	DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);
	
  /** initialization of file reader component private data structures */
  /** opening the input file whose name is already set via setParameter */
  error = av_open_input_file(&omx_filereader_component_Private->avformatcontext, 
                            (char*)omx_filereader_component_Private->sInputFileName,
                            omx_filereader_component_Private->avinputformat,
                            0,
                            omx_filereader_component_Private->avformatparameters);

  if(error != 0) {
    DEBUG(DEB_LEV_ERR,"Couldn't Open Input Stream error=%d File Name=%s--\n",
      error,(char*)omx_filereader_component_Private->sInputFileName);
  
    (*(omx_filereader_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_filereader_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      OMX_ErrorFormatNotDetected, /* Format Not Detected */ 
      0, /* This is the output port index - only one port*/
      NULL);  

    return OMX_ErrorBadParameter;
  }

  av_find_stream_info(omx_filereader_component_Private->avformatcontext);

  if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Audio Coding Type Mp3\n",__func__);
  } else if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Audio Coding Type OGG\n",__func__);
  } else if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {   
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Audio Coding Type AAC\n",__func__);
  } else {
    DEBUG(DEB_LEV_ERR,"In %s Ouch!! No Audio Coding Type Selected\n",__func__);
  }
  
  DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Extra data size=%d\n",__func__,omx_filereader_component_Private->avformatcontext->streams[0]->codec->extradata_size);

  /** initialization for buff mgmt callback function */
  omx_filereader_component_Private->isFirstBuffer = 1;
  omx_filereader_component_Private->isNewBuffer = 1;
  omx_filereader_component_Private->bIsEOSSent = OMX_FALSE;

  /** send callback regarding codec context extradata which will be required to 
    * open the codec in the audio decoder component 
    */   	 
  /* filling up the OMX_VENDOR_EXTRADATATYPE structure */
  pExtraData = (OMX_VENDOR_EXTRADATATYPE *)malloc(sizeof(OMX_VENDOR_EXTRADATATYPE));
  pExtraData->nPortIndex = 0; //output port index
  pExtraData->nDataSize = omx_filereader_component_Private->avformatcontext->streams[0]->codec->extradata_size;
  pExtraData->pData =  omx_filereader_component_Private->avformatcontext->streams[0]->codec->extradata;

  (*(omx_filereader_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_filereader_component_Private->callbackData,
    OMX_EventPortFormatDetected, /* The command was completed */
    OMX_IndexParamAudioPortFormat, /* port Format Detected */ 
    0, /* This is the output port index - only one port*/
    NULL);  

  (*(omx_filereader_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_filereader_component_Private->callbackData,
    OMX_EventPortSettingsChanged, /* The command was completed */
    OMX_IndexParamCommonExtraQuantData, /* port settings changed */ 
    0, /* This is the output port index - only one port*/
    pExtraData);  

  omx_filereader_component_Private->avformatReady = OMX_TRUE;
  /*Indicate that avformat is ready*/
  tsem_up(omx_filereader_component_Private->avformatSyncSem);

  free(pExtraData);
  return OMX_ErrorNone;
}

/** The DeInitialization function 
 */
OMX_ERRORTYPE omx_filereader_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
  /** closing input file */
  av_close_input_file(omx_filereader_component_Private->avformatcontext);
  
  omx_filereader_component_Private->avformatReady = OMX_FALSE;
  tsem_reset(omx_filereader_component_Private->avformatSyncSem);

  /** Deinitialization with respect to buff mgmt callback function */
  omx_filereader_component_Private->isFirstBuffer = 0;
  omx_filereader_component_Private->isNewBuffer = 0;

  return OMX_ErrorNone;
}


/** 
 * This function processes the input file and returns packet by packet as an output data
 * this packet is used in audio decoder component for decoding
 */
void omx_filereader_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  omx_filereader_component_PortType *pPort = (omx_filereader_component_PortType *)omx_filereader_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  int error;

  DEBUG(DEB_LEV_FUNCTION_NAME,"In %s \n",__func__);

  if (omx_filereader_component_Private->avformatReady == OMX_FALSE) {
    if(omx_filereader_component_Private->state == OMX_StateExecuting) {
      /*wait for avformat to be ready*/
      tsem_down(omx_filereader_component_Private->avformatSyncSem);
    } else {
      return;
    }
  }

  if(omx_filereader_component_Private->isNewBuffer) {
    omx_filereader_component_Private->isNewBuffer = 0;
  }
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  if(pPort->sTimeStamp.nTimestamp != 0x0) {
    av_seek_frame(omx_filereader_component_Private->avformatcontext, 0, pPort->sTimeStamp.nTimestamp, AVSEEK_FLAG_ANY);
    DEBUG(DEB_LEV_ERR, "Seek Timestamp %llx \n",pPort->sTimeStamp.nTimestamp);
    pPort->sTimeStamp.nTimestamp = 0x0;
  }

  error = av_read_frame(omx_filereader_component_Private->avformatcontext, &omx_filereader_component_Private->pkt);
  if(error < 0) {
    DEBUG(DEB_LEV_FULL_SEQ,"In %s EOS - no more packet,state=%x\n",__func__,
      omx_filereader_component_Private->state);
    if(omx_filereader_component_Private->bIsEOSSent == OMX_FALSE) {
      pOutputBuffer->nFlags = OMX_BUFFERFLAG_EOS;
      omx_filereader_component_Private->bIsEOSSent = OMX_TRUE;
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"\n packet size : %d \n",omx_filereader_component_Private->pkt.size);
    /** copying the packetized data in the output buffer that will be decoded in the decoder component  */
    memcpy(pOutputBuffer->pBuffer, omx_filereader_component_Private->pkt.data, omx_filereader_component_Private->pkt.size);
    pOutputBuffer->nFilledLen = omx_filereader_component_Private->pkt.size;
    pOutputBuffer->nTimeStamp = omx_filereader_component_Private->pkt.dts;

    if(pOutputBuffer->nTimeStamp == 0x80000000) { //Skip -ve timestamp
      pOutputBuffer->nTimeStamp=0x0;
    }
    /** request for new output buffer */
    omx_filereader_component_Private->isNewBuffer = 1;
  }

  av_free_packet(&omx_filereader_component_Private->pkt);
  
  /** return the current output buffer */
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n", (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_filereader_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_U32 portIndex;
  OMX_U32 i;
  OMX_U32 nFileNameLength;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  omx_filereader_component_PortType* pPort = (omx_filereader_component_PortType *) omx_filereader_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];

  if(ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

  switch(nParamIndex) {
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    if (portIndex < 1) {
      memcpy(&pPort->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, pAudioMp3->nPortIndex, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    break;
  case OMX_IndexVendorFileReadInputFilename : 
    nFileNameLength = strlen((char *)ComponentParameterStructure) * sizeof(char) + 1;
    if(nFileNameLength > DEFAULT_FILENAME_LENGTH) {
      free(omx_filereader_component_Private->sInputFileName);
      omx_filereader_component_Private->sInputFileName = (char *)malloc(nFileNameLength);
    }
    strcpy(omx_filereader_component_Private->sInputFileName, (char *)ComponentParameterStructure);
    omx_filereader_component_Private->bIsEOSSent = OMX_FALSE;
    /** determine the audio coding type */ 
    for(i = 0; omx_filereader_component_Private->sInputFileName[i] != '\0'; i++);
    if(omx_filereader_component_Private->sInputFileName[i - 1] == '3') {
      omx_filereader_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    } else if(omx_filereader_component_Private->sInputFileName[i - 1] == 'g') {
      omx_filereader_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
    } else if(omx_filereader_component_Private->sInputFileName[i - 1] == 'c') { 
      omx_filereader_component_Private->audio_coding_type = OMX_AUDIO_CodingAAC;
    } else {
      return OMX_ErrorBadParameter;	
    }
    /** now set the port parameters according to selected audio coding type */
    if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/mpeg");
      pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    } else if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
      strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/vorbis");
      pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
    } else if(omx_filereader_component_Private->audio_coding_type == OMX_AUDIO_CodingAAC) {   
      strcpy(pPort->sPortParam.format.audio.cMIMEType, "audio/aac");
      pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_filereader_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_VENDOR_EXTRADATATYPE sExtraData;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  omx_filereader_component_PortType *pPort = (omx_filereader_component_PortType *) omx_filereader_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];	
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Getting parameter %08x\n",__func__, nParamIndex);
	
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_filereader_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex < 1) {
      memcpy(pAudioPortFormat, &pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
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
OMX_ERRORTYPE omx_filereader_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_filereader_component_PrivateType* omx_filereader_component_Private = (omx_filereader_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_STATETYPE oldState = omx_filereader_component_Private->state;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  /* Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){ 
    if ((message->messageParam == OMX_StateExecuting) && (oldState == OMX_StateIdle)) {		
      err = omx_filereader_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s File Reader Init Failed Error=%x\n",__func__,err); 
        return err;
      }
    } else if ((message->messageParam == OMX_StateIdle) && (oldState == OMX_StateExecuting)) {
      err = omx_filereader_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s File Reader Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }
  }

  return err;
}


/** setting configurations */
OMX_ERRORTYPE omx_filereader_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_TIME_CONFIG_TIMESTAMPTYPE* sTimeStamp;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_filereader_component_PortType *pPort;

  switch (nIndex) {
    case OMX_IndexConfigTimePosition : 
      sTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;
      /*Check Structure Header and verify component state*/
      if (sTimeStamp->nPortIndex >= (omx_filereader_component_Private->sPortTypesParam.nStartPortNumber + omx_filereader_component_Private->sPortTypesParam.nPorts)) {
        DEBUG(DEB_LEV_ERR, "Bad Port index %i when the component has %i ports\n", (int)sTimeStamp->nPortIndex, (int)omx_filereader_component_Private->sPortTypesParam.nPorts);
        return OMX_ErrorBadPortIndex;
      }

      err= checkHeader(sTimeStamp , sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      if(err != OMX_ErrorNone) {
        return err;
      }

      if (sTimeStamp->nPortIndex < 1) {
        pPort= (omx_filereader_component_PortType *)omx_filereader_component_Private->ports[sTimeStamp->nPortIndex];
        memcpy(&pPort->sTimeStamp,sTimeStamp,sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      return OMX_ErrorNone;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

/** setting configurations */
OMX_ERRORTYPE omx_filereader_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_VENDOR_EXTRADATATYPE sExtraData;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_filereader_component_PrivateType* omx_filereader_component_Private = openmaxStandComp->pComponentPrivate;

  switch (nIndex) {
    case OMX_IndexVendorExtraData:
      sExtraData.nPortIndex = 0;
      sExtraData.nDataSize  = omx_filereader_component_Private->avformatcontext->streams[0]->codec->extradata_size;
      sExtraData.pData      = omx_filereader_component_Private->avformatcontext->streams[0]->codec->extradata;
      memcpy(pComponentConfigStructure, &sExtraData, sizeof(OMX_VENDOR_EXTRADATATYPE));
      break;
    default: // delegate to superclass
      return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

