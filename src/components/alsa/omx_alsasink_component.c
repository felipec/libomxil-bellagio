/**
  @file src/components/alsa/omx_alsasink_component.c

  OpenMAX ALSA sink component. This component is an audio sink that uses ALSA library.

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
#include <omx_base_audio_port.h>
#include <omx_base_clock_port.h>
#include <omx_alsasink_component.h>
#include <config.h>

/** Maximum Number of AlsaSink Instance*/
#define MAX_COMPONENT_ALSASINK 1

/** Number of AlsaSink Instance*/
static OMX_U32 noAlsasinkInstance=0;

#ifdef AV_SYNC_LOG  /* for checking AV sync */ //TODO : give seg fault if enabled
static FILE *fd = NULL;
#endif

/** The Constructor
 */
OMX_ERRORTYPE omx_alsasink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  int                                 err;
  int                                 omxErr;
  omx_base_audio_PortType             *pPort;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_alsasink_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_alsasink_component_Private->ports = NULL;

  omxErr = omx_base_sink_Constructor(openmaxStandComp,cComponentName);
  if (omxErr != OMX_ErrorNone) {
    return OMX_ErrorInsufficientResources;
  }

  omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nStartPortNumber = 0;
  omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts = 1;

  omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther].nStartPortNumber = 1;
  omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts = 1;

  /** Allocate Ports and call port constructor. */
  if ((omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
       omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)
       && !omx_alsasink_component_Private->ports) {
    omx_alsasink_component_Private->ports = calloc((omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                                    omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts), sizeof(omx_base_PortType *));
    if (!omx_alsasink_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    omx_alsasink_component_Private->ports[0] = calloc(1, sizeof(omx_base_audio_PortType));
    if (!omx_alsasink_component_Private->ports[0]) {
      return OMX_ErrorInsufficientResources;
    }
    base_audio_port_Constructor(openmaxStandComp, &omx_alsasink_component_Private->ports[0], 0, OMX_TRUE);

    omx_alsasink_component_Private->ports[1] = calloc(1, sizeof(omx_base_clock_PortType));
    if (!omx_alsasink_component_Private->ports[1]) {
      return OMX_ErrorInsufficientResources;
    }
    base_clock_port_Constructor(openmaxStandComp, &omx_alsasink_component_Private->ports[1], 1, OMX_TRUE);
    omx_alsasink_component_Private->ports[1]->sPortParam.bEnabled = OMX_FALSE;
  }

  pPort = (omx_base_audio_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  // set the pPort params, now that the ports exist
  /** Domain specific section for the ports. */
  pPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  /*Input pPort buffer size is equal to the size of the output buffer of the previous component*/
  pPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

 /* Initializing the function pointers */
  omx_alsasink_component_Private->BufferMgmtCallback  = omx_alsasink_component_BufferMgmtCallback;
  omx_alsasink_component_Private->destructor          = omx_alsasink_component_Destructor;
  pPort->Port_SendBufferFunction                      = omx_alsasink_component_port_SendBufferFunction;
  pPort->FlushProcessingBuffers                       = omx_alsasink_component_port_FlushProcessingBuffers;

  setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  pPort->sAudioParam.nPortIndex = 0;
  pPort->sAudioParam.nIndex = 0;
  pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;

  /* OMX_AUDIO_PARAM_PCMMODETYPE */
  setHeader(&omx_alsasink_component_Private->sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_alsasink_component_Private->sPCMModeParam.nPortIndex = 0;
  omx_alsasink_component_Private->sPCMModeParam.nChannels = 2;
  omx_alsasink_component_Private->sPCMModeParam.eNumData = OMX_NumericalDataSigned;
  omx_alsasink_component_Private->sPCMModeParam.eEndian = OMX_EndianLittle;
  omx_alsasink_component_Private->sPCMModeParam.bInterleaved = OMX_TRUE;
  omx_alsasink_component_Private->sPCMModeParam.nBitPerSample = 16;
  omx_alsasink_component_Private->sPCMModeParam.nSamplingRate = 44100;
  omx_alsasink_component_Private->sPCMModeParam.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_alsasink_component_Private->sPCMModeParam.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

/* testing the A/V sync */
#ifdef AV_SYNC_LOG
  fd = fopen("audio_timestamps.out","w");
  if(!fd) {
    DEBUG(DEB_LEV_ERR, "Couldn't open audio timestamp log err=%d\n",errno);
  }
#endif

  noAlsasinkInstance++;
  if(noAlsasinkInstance > MAX_COMPONENT_ALSASINK) {
    return OMX_ErrorInsufficientResources;
  }

  /* Allocate the playback handle and the hardware parameter structure */
  if ((err = snd_pcm_open (&omx_alsasink_component_Private->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    DEBUG(DEB_LEV_ERR, "cannot open audio device %s (%s)\n", "default", snd_strerror (err));
    return OMX_ErrorHardware;
  }
  else
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Got playback handle at %p %p in %i\n", omx_alsasink_component_Private->playback_handle, &omx_alsasink_component_Private->playback_handle, getpid());

  if (snd_pcm_hw_params_malloc(&omx_alsasink_component_Private->hw_params) < 0) {
    DEBUG(DEB_LEV_ERR, "%s: failed allocating input pPort hw parameters\n", __func__);
    return OMX_ErrorHardware;
  }
  else
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Got hw parameters at %p\n", omx_alsasink_component_Private->hw_params);

  if ((err = snd_pcm_hw_params_any (omx_alsasink_component_Private->playback_handle, omx_alsasink_component_Private->hw_params)) < 0) {
    DEBUG(DEB_LEV_ERR, "cannot initialize hardware parameter structure (%s)\n",  snd_strerror (err));
    return OMX_ErrorHardware;
  }

  openmaxStandComp->SetParameter  = omx_alsasink_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_alsasink_component_GetParameter;

  /* Write in the default parameters */
  omx_alsasink_component_Private->AudioPCMConfigured  = 0;
  omx_alsasink_component_Private->eState  = OMX_TIME_ClockStateStopped;
  omx_alsasink_component_Private->xScale  = 1<<16;

  if (!omx_alsasink_component_Private->AudioPCMConfigured) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface in the Init function\n");
    omxErr = omx_alsasink_component_SetParameter(openmaxStandComp, OMX_IndexParamAudioPcm, &omx_alsasink_component_Private->sPCMModeParam);
    if(omxErr != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "In %s Error %08x\n",__func__,omxErr);
    }
  }

  return OMX_ErrorNone;
}

/** The Destructor
 */
OMX_ERRORTYPE omx_alsasink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_alsasink_component_Private->hw_params) {
    snd_pcm_hw_params_free (omx_alsasink_component_Private->hw_params);
  }
  if(omx_alsasink_component_Private->playback_handle) {
    snd_pcm_close(omx_alsasink_component_Private->playback_handle);
  }

  /* frees port/s */
  if (omx_alsasink_component_Private->ports) {
    for (i=0; i < (omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                   omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts); i++) {
      if(omx_alsasink_component_Private->ports[i])
        omx_alsasink_component_Private->ports[i]->PortDestructor(omx_alsasink_component_Private->ports[i]);
    }
    free(omx_alsasink_component_Private->ports);
    omx_alsasink_component_Private->ports=NULL;
  }

#ifdef AV_SYNC_LOG
      fclose(fd);
#endif

  noAlsasinkInstance--;

  return omx_base_sink_Destructor(openmaxStandComp);

}

/** @brief the entry point for sending buffers to the alsa sink port
 *
 * This function can be called by the EmptyThisBuffer or FillThisBuffer. It depends on
 * the nature of the port, that can be an input or output port.
 */
OMX_ERRORTYPE omx_alsasink_component_port_SendBufferFunction(omx_base_PortType *openmaxStandPort, OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE                   err;
  OMX_U32                         portIndex;
  OMX_COMPONENTTYPE*              omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_BOOL                        SendFrame;
  omx_base_clock_PortType*        pClockPort;
#if NO_GST_OMX_PATCH
  unsigned int i;
#endif

  portIndex = (openmaxStandPort->sPortParam.eDir == OMX_DirInput)?pBuffer->nInputPortIndex:pBuffer->nOutputPortIndex;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s portIndex %lu\n", __func__, portIndex);

  if (portIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port for this operation portIndex=%d port->portIndex=%d\n",
           __func__, (int)portIndex, (int)openmaxStandPort->sPortParam.nPortIndex);
    return OMX_ErrorBadPortIndex;
  }

  if(omx_base_component_Private->state == OMX_StateInvalid) {
    DEBUG(DEB_LEV_ERR, "In %s: we are in OMX_StateInvalid\n", __func__);
    return OMX_ErrorInvalidState;
  }

  if(omx_base_component_Private->state != OMX_StateExecuting &&
    omx_base_component_Private->state != OMX_StatePause &&
    omx_base_component_Private->state != OMX_StateIdle) {
    DEBUG(DEB_LEV_ERR, "In %s: we are not in executing/paused/idle state, but in %d\n", __func__, omx_base_component_Private->state);
    return OMX_ErrorIncorrectStateOperation;
  }
  if (!PORT_IS_ENABLED(openmaxStandPort) || (PORT_IS_BEING_DISABLED(openmaxStandPort) && !PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) ||
      (omx_base_component_Private->transientState == OMX_TransStateExecutingToIdle &&
      (PORT_IS_TUNNELED(openmaxStandPort) && !PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)))) {
    DEBUG(DEB_LEV_ERR, "In %s: Port %d is disabled comp = %s \n", __func__, (int)portIndex,omx_base_component_Private->name);
    return OMX_ErrorIncorrectStateOperation;
  }

  /* Temporarily disable this check for gst-openmax */
#if NO_GST_OMX_PATCH
  {
  OMX_BOOL foundBuffer = OMX_FALSE;
  if(pBuffer!=NULL && pBuffer->pBuffer!=NULL) {
    for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (pBuffer->pBuffer == openmaxStandPort->pInternalBufferStorage[i]->pBuffer) {
      foundBuffer = OMX_TRUE;
      break;
    }
    }
  }
  if (!foundBuffer) {
    return OMX_ErrorBadParameter;
  }
  }
#endif

  if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
    return err;
  }

  pClockPort  = (omx_base_clock_PortType*)omx_base_component_Private->ports[OMX_BASE_SINK_CLOCKPORT_INDEX];
  if(PORT_IS_TUNNELED(pClockPort) && !PORT_IS_BEING_FLUSHED(openmaxStandPort) &&
      (omx_base_component_Private->transientState != OMX_TransStateExecutingToIdle) &&
      (pBuffer->nFlags != OMX_BUFFERFLAG_EOS)){
    SendFrame = omx_alsasink_component_ClockPortHandleFunction((omx_alsasink_component_PrivateType*)omx_base_component_Private, pBuffer);
    /* drop the frame */
    if(!SendFrame) pBuffer->nFilledLen=0;
  }

  /* And notify the buffer management thread we have a fresh new buffer to manage */
  if(!PORT_IS_BEING_FLUSHED(openmaxStandPort) && !(PORT_IS_BEING_DISABLED(openmaxStandPort) && PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort))){
      queue(openmaxStandPort->pBufferQueue, pBuffer);
      tsem_up(openmaxStandPort->pBufferSem);
      DEBUG(DEB_LEV_PARAMS, "In %s Signalling bMgmtSem Port Index=%d\n",__func__, (int)portIndex);
      tsem_up(omx_base_component_Private->bMgmtSem);
  }else if(PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)){
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s received io:%d buffer\n",
        __func__,omx_base_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
      queue(openmaxStandPort->pBufferQueue, pBuffer);
      tsem_up(openmaxStandPort->pBufferSem);
  }
  else { // If port being flushed and not tunneled then return error
    DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
    return OMX_ErrorIncorrectStateOperation;
  }
  return OMX_ErrorNone;
}

OMX_BOOL omx_alsasink_component_ClockPortHandleFunction(omx_alsasink_component_PrivateType* omx_alsasink_component_Private, OMX_BUFFERHEADERTYPE* inputbuffer){
  omx_base_clock_PortType*            pClockPort;
  OMX_BUFFERHEADERTYPE*               clockBuffer;
  OMX_TIME_MEDIATIMETYPE*             pMediaTime;
  OMX_HANDLETYPE                      hclkComponent;
  OMX_TIME_CONFIG_TIMESTAMPTYPE       sClientTimeStamp;
  OMX_ERRORTYPE                       err;
  OMX_BOOL                            SendFrame=OMX_TRUE;
  omx_base_audio_PortType             *pAudioPort;

  int static                          count=0; //frame counter

  pClockPort    = (omx_base_clock_PortType*)omx_alsasink_component_Private->ports[OMX_BASE_SINK_CLOCKPORT_INDEX];
  pAudioPort    = (omx_base_audio_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  hclkComponent = pClockPort->hTunneledComponent;
  setHeader(&pClockPort->sMediaTimeRequest, sizeof(OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE));

  /* if  first time stamp is received then notify the clock component */
  if(inputbuffer->nFlags == OMX_BUFFERFLAG_STARTTIME) {
    DEBUG(DEB_LEV_FULL_SEQ,"In %s  first time stamp = %llx \n", __func__,inputbuffer->nTimeStamp);
    inputbuffer->nFlags = 0;
    hclkComponent = pClockPort->hTunneledComponent;
    setHeader(&sClientTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
    sClientTimeStamp.nPortIndex = pClockPort->nTunneledPort;
    sClientTimeStamp.nTimestamp = inputbuffer->nTimeStamp;
    err = OMX_SetConfig(hclkComponent, OMX_IndexConfigTimeClientStartTime, &sClientTimeStamp);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig in func=%s \n",err,__func__);
    }

    if(!PORT_IS_BEING_FLUSHED(pAudioPort) && !PORT_IS_BEING_FLUSHED(pClockPort)) {
      tsem_down(pClockPort->pBufferSem); /* wait for state change notification from clock src*/

      /* update the clock state and clock scale info into the alsa sink private data */
      if(pClockPort->pBufferQueue->nelem > 0) {
        clockBuffer = dequeue(pClockPort->pBufferQueue);
        pMediaTime  = (OMX_TIME_MEDIATIMETYPE*)clockBuffer->pBuffer;
        omx_alsasink_component_Private->eState = pMediaTime->eState;
        omx_alsasink_component_Private->xScale = pMediaTime->xScale;
        pClockPort->ReturnBufferFunction((omx_base_PortType*)pClockPort,clockBuffer);
      }
    }
  }

  /* do not send the data to alsa and return back, if the clock is not running or the scale is anything but 1*/
  if(!(omx_alsasink_component_Private->eState==OMX_TIME_ClockStateRunning  && (omx_alsasink_component_Private->xScale>>16)==1)){
    inputbuffer->nFilledLen=0;
    //return;
    SendFrame = OMX_FALSE;
    return SendFrame;
  }

  /* check for any scale change information from the clock component */
  if(pClockPort->pBufferSem->semval>0){
    tsem_down(pClockPort->pBufferSem);
    if(pClockPort->pBufferQueue->nelem > 0) {
      clockBuffer = dequeue(pClockPort->pBufferQueue);
      pMediaTime  = (OMX_TIME_MEDIATIMETYPE*)clockBuffer->pBuffer;
      if(pMediaTime->eUpdateType==OMX_TIME_UpdateScaleChanged) {
       if(/*(omx_alsasink_component_Private->xScale>>16)==2 &&*/ (pMediaTime->xScale>>16)==1){ /* check with Q16 format only */
             /* rebase the clock time base when turning to normal play mode*/
          hclkComponent = pClockPort->hTunneledComponent;
          setHeader(&sClientTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
          sClientTimeStamp.nPortIndex = pClockPort->nTunneledPort;
          sClientTimeStamp.nTimestamp = inputbuffer->nTimeStamp;
          err = OMX_SetConfig(hclkComponent, OMX_IndexConfigTimeCurrentAudioReference, &sClientTimeStamp);
          if(err!=OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig in func=%s \n",err,__func__);
          }
       }
       omx_alsasink_component_Private->xScale = pMediaTime->xScale;
      }
      pClockPort->ReturnBufferFunction((omx_base_PortType*)pClockPort,clockBuffer);
    }
  }

  count++;
  if(count==15) { //send request for every 15th frame
    count=0;
    /* requesting for the timestamp for the data delivery */
    if(!PORT_IS_BEING_FLUSHED(pAudioPort) && !PORT_IS_BEING_FLUSHED(pClockPort)&&
        omx_alsasink_component_Private->transientState != OMX_TransStateExecutingToIdle) {
      pClockPort->sMediaTimeRequest.nOffset         = 100; /*set the requested offset */
      pClockPort->sMediaTimeRequest.nPortIndex      = pClockPort->nTunneledPort;
      pClockPort->sMediaTimeRequest.pClientPrivate  = NULL; /* fill the appropriate value */
      pClockPort->sMediaTimeRequest.nMediaTimestamp = inputbuffer->nTimeStamp;
      err = OMX_SetConfig(hclkComponent, OMX_IndexConfigTimeMediaTimeRequest, &pClockPort->sMediaTimeRequest);
      if(err!=OMX_ErrorNone) {
       DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig in func=%s \n",err,__func__);
      }
      if(!PORT_IS_BEING_FLUSHED(pAudioPort) && !PORT_IS_BEING_FLUSHED(pClockPort) &&
        omx_alsasink_component_Private->transientState != OMX_TransStateExecutingToIdle) {
        tsem_down(pClockPort->pBufferSem); /* wait for the request fullfillment */
        if(pClockPort->pBufferQueue->nelem > 0) {
          clockBuffer = dequeue(pClockPort->pBufferQueue);
          pMediaTime  = (OMX_TIME_MEDIATIMETYPE*)clockBuffer->pBuffer;
          if(pMediaTime->eUpdateType==OMX_TIME_UpdateScaleChanged) {
            omx_alsasink_component_Private->xScale = pMediaTime->xScale;
          }
          if(pMediaTime->eUpdateType==OMX_TIME_UpdateRequestFulfillment) {
            if((pMediaTime->nOffset)>0) {
#ifdef AV_SYNC_LOG
         fprintf(fd,"%lld %lld\n",inputbuffer->nTimeStamp,pMediaTime->nWallTimeAtMediaTime);
#endif
              SendFrame = OMX_TRUE; /* as offset is >0 send the data to the device */
            }
            else {
              SendFrame = OMX_FALSE; /* as offset is <0 do not send the data to the device */
            }
          }
          pClockPort->ReturnBufferFunction((omx_base_PortType*)pClockPort,clockBuffer);
        }
      }
    }
  }

  return(SendFrame);
}

/** @brief Releases buffers under processing.
 * This function must be implemented in the derived classes, for the
 * specific processing
 */
OMX_ERRORTYPE omx_alsasink_component_port_FlushProcessingBuffers(omx_base_PortType *openmaxStandPort) {
  omx_base_component_PrivateType* omx_base_component_Private;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private;
  OMX_BUFFERHEADERTYPE* pBuffer;
  omx_base_clock_PortType               *pClockPort;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_base_component_Private        = (omx_base_component_PrivateType*)openmaxStandPort->standCompContainer->pComponentPrivate;
  omx_alsasink_component_Private  = ( omx_alsasink_component_PrivateType*) omx_base_component_Private;

  pClockPort    = (omx_base_clock_PortType*) omx_alsasink_component_Private->ports[OMX_BASE_SINK_CLOCKPORT_INDEX];

  if(openmaxStandPort->sPortParam.eDomain!=OMX_PortDomainOther) { /* clock buffers not used in the clients buffer managment function */
    pthread_mutex_lock(&omx_base_component_Private->flush_mutex);
    openmaxStandPort->bIsPortFlushed=OMX_TRUE;
    /*Signal the buffer management thread of port flush,if it is waiting for buffers*/
    if(omx_base_component_Private->bMgmtSem->semval==0) {
      tsem_up(omx_base_component_Private->bMgmtSem);
    }

    if(omx_base_component_Private->state==OMX_StatePause ) {
      /*Waiting at paused state*/
      tsem_signal(omx_base_component_Private->bStateSem);
    }
    DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for flush all condition port index =%d\n", __func__,(int)openmaxStandPort->sPortParam.nPortIndex);
    /* Wait until flush is completed */
    pthread_mutex_unlock(&omx_base_component_Private->flush_mutex);

    /*Dummy signal to clock port*/
    if(pClockPort->pBufferSem->semval == 0) {
      tsem_up(pClockPort->pBufferSem);
      tsem_reset(pClockPort->pBufferSem);
    }
    tsem_down(omx_base_component_Private->flush_all_condition);
  }

  tsem_reset(omx_base_component_Private->bMgmtSem);

  /* Flush all the buffers not under processing */
  while (openmaxStandPort->pBufferSem->semval > 0) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s TFlag=%x Flusing Port=%d,Semval=%d Qelem=%d\n",
    __func__,(int)openmaxStandPort->nTunnelFlags,(int)openmaxStandPort->sPortParam.nPortIndex,
    (int)openmaxStandPort->pBufferSem->semval,(int)openmaxStandPort->pBufferQueue->nelem);

    tsem_down(openmaxStandPort->pBufferSem);
    pBuffer = dequeue(openmaxStandPort->pBufferQueue);
    if (PORT_IS_TUNNELED(openmaxStandPort) && !PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s is returning io:%d buffer\n",
        __func__,omx_base_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
      if (openmaxStandPort->sPortParam.eDir == OMX_DirInput) {
        ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->FillThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
      } else {
        ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->EmptyThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
      }
    } else if (PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
      queue(openmaxStandPort->pBufferQueue,pBuffer);
    } else {
      (*(openmaxStandPort->BufferProcessedCallback))(
        openmaxStandPort->standCompContainer,
        omx_base_component_Private->callbackData,
        pBuffer);
    }
  }
  /*Port is tunneled and supplier and didn't received all it's buffer then wait for the buffers*/
  if (PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    while(openmaxStandPort->pBufferQueue->nelem!= openmaxStandPort->nNumAssignedBuffers){
      tsem_down(openmaxStandPort->pBufferSem);
      DEBUG(DEB_LEV_PARAMS, "In %s Got a buffer qelem=%d\n",__func__,openmaxStandPort->pBufferQueue->nelem);
    }
    tsem_reset(openmaxStandPort->pBufferSem);
  }

  pthread_mutex_lock(&omx_base_component_Private->flush_mutex);
  openmaxStandPort->bIsPortFlushed=OMX_FALSE;
  pthread_mutex_unlock(&omx_base_component_Private->flush_mutex);

  tsem_up(omx_base_component_Private->flush_condition);

  DEBUG(DEB_LEV_FULL_SEQ, "Out %s Port Index=%d bIsPortFlushed=%d Component %s\n", __func__,
    (int)openmaxStandPort->sPortParam.nPortIndex,(int)openmaxStandPort->bIsPortFlushed,omx_base_component_Private->name);

  DEBUG(DEB_LEV_PARAMS, "In %s TFlag=%x Qelem=%d BSem=%d bMgmtsem=%d component=%s\n", __func__,
    (int)openmaxStandPort->nTunnelFlags,
    (int)openmaxStandPort->pBufferQueue->nelem,
    (int)openmaxStandPort->pBufferSem->semval,
    (int)omx_base_component_Private->bMgmtSem->semval,
    omx_base_component_Private->name);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out %s Port Index=%d\n", __func__,(int)openmaxStandPort->sPortParam.nPortIndex);

  return OMX_ErrorNone;
}

/**
 * This function plays the input buffer. When fully consumed it returns.
 */
void omx_alsasink_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer) {
  OMX_U32                             frameSize;
  OMX_S32                             written;
  OMX_S32                             totalBuffer;
  OMX_S32                             offsetBuffer;
  OMX_BOOL                            allDataSent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;

  /* Feed it to ALSA */
  frameSize = (omx_alsasink_component_Private->sPCMModeParam.nChannels * omx_alsasink_component_Private->sPCMModeParam.nBitPerSample) >> 3;
  DEBUG(DEB_LEV_FULL_SEQ, "Framesize is %u chl=%d bufSize=%d\n",
  (int)frameSize, (int)omx_alsasink_component_Private->sPCMModeParam.nChannels, (int)inputbuffer->nFilledLen);

  if(inputbuffer->nFilledLen < frameSize){
    DEBUG(DEB_LEV_ERR, "Ouch!! In %s input buffer filled len(%d) less than frame size(%d)\n",__func__, (int)inputbuffer->nFilledLen, (int)frameSize);
    return;
  }

  allDataSent = OMX_FALSE;

  totalBuffer = inputbuffer->nFilledLen/frameSize;
  offsetBuffer = 0;
  while (!allDataSent) {
//  DEBUG(DEB_LEV_ERR, "Writing to the device ..\n");
    written = snd_pcm_writei(omx_alsasink_component_Private->playback_handle, inputbuffer->pBuffer + (offsetBuffer * frameSize), totalBuffer);
    if (written < 0) {
      if(written == -EPIPE){
        DEBUG(DEB_LEV_ERR, "ALSA Underrun..\n");
        snd_pcm_prepare(omx_alsasink_component_Private->playback_handle);
        written = 0;
      } else {
        DEBUG(DEB_LEV_ERR, "Cannot send any data to the audio device %s (%s)\n", "default", snd_strerror (written));
        DEBUG(DEB_LEV_ERR, "IB FilledLen=%d,totalBuffer=%d,frame size=%d,offset=%d\n",
        (int)inputbuffer->nFilledLen, (int)totalBuffer, (int)frameSize, (int)offsetBuffer);
        break;
        return;
      }
    }

    if(written != totalBuffer){
      totalBuffer = totalBuffer - written;
      offsetBuffer = written;
    } else {
      DEBUG(DEB_LEV_FULL_SEQ, "Buffer successfully sent to ALSA. Length was %i\n", (int)inputbuffer->nFilledLen);
      allDataSent = OMX_TRUE;
    }
  }
  inputbuffer->nFilledLen=0;
}

OMX_ERRORTYPE omx_alsasink_component_SetParameter(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nParamIndex,
  OMX_PTR ComponentParameterStructure)
{
  int err;
  int omxErr = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_OTHER_PARAM_PORTFORMATTYPE *pOtherPortFormat;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType* pPort = (omx_base_audio_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  omx_base_clock_PortType *pClockPort;
  snd_pcm_t* playback_handle = omx_alsasink_component_Private->playback_handle;
  snd_pcm_hw_params_t* hw_params = omx_alsasink_component_Private->hw_params;

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);

  /** Each time we are (re)configuring the hw_params thing
  * we need to reinitialize it, otherwise previous changes will not take effect.
  * e.g.: changing a previously configured sampling rate does not have
  * any effect if we are not calling this each time.
  */
  err = snd_pcm_hw_params_any (playback_handle, hw_params);

  switch(nParamIndex) {
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    omxErr = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(omxErr!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n", __func__, omxErr);
      break;
    }
    if (portIndex < 1) {
      memcpy(&pPort->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  case  OMX_IndexParamOtherPortFormat:
      pOtherPortFormat = (OMX_OTHER_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      portIndex = pOtherPortFormat->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pOtherPortFormat, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE));
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
        break;
      }
      if(portIndex != 1) {
        return OMX_ErrorBadPortIndex;
      }
      pClockPort = (omx_base_clock_PortType *) omx_alsasink_component_Private->ports[portIndex];

      pClockPort->sOtherParam.eFormat = pOtherPortFormat->eFormat;
      break;
  case OMX_IndexParamAudioPcm:
    {
      unsigned int rate;
      OMX_AUDIO_PARAM_PCMMODETYPE* sPCMModeParam = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;

      portIndex = sPCMModeParam->nPortIndex;
      /*Check Structure Header and verify component state*/
      omxErr = omx_base_component_ParameterSanityCheck(hComponent, portIndex, sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if(omxErr!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n", __func__, omxErr);
        break;
      }

      omx_alsasink_component_Private->AudioPCMConfigured  = 1;
      if(sPCMModeParam->nPortIndex != omx_alsasink_component_Private->sPCMModeParam.nPortIndex){
        DEBUG(DEB_LEV_ERR, "Error setting input pPort index\n");
        omxErr = OMX_ErrorBadParameter;
        break;
      }

      if(snd_pcm_hw_params_set_channels(playback_handle, hw_params, sPCMModeParam->nChannels)){
        DEBUG(DEB_LEV_ERR, "Error setting number of channels\n");
        return OMX_ErrorBadParameter;
      }

      if(sPCMModeParam->bInterleaved == OMX_TRUE){
        if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set access type intrleaved (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
      }
      else{
        if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set access type non interleaved (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
      }
      rate = sPCMModeParam->nSamplingRate;
      if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot set sample rate (%s)\n", snd_strerror (err));
        return OMX_ErrorHardware;
      }
      else{
        sPCMModeParam->nSamplingRate = rate;
        DEBUG(DEB_LEV_PARAMS, "Set correctly sampling rate to %lu\n", sPCMModeParam->nSamplingRate);
      }

      if(sPCMModeParam->ePCMMode == OMX_AUDIO_PCMModeLinear){
        snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;
        DEBUG(DEB_LEV_PARAMS, "Bit per sample %i, signed=%i, little endian=%i\n",
        (int)sPCMModeParam->nBitPerSample,
        (int)sPCMModeParam->eNumData == OMX_NumericalDataSigned,
        (int)sPCMModeParam->eEndian ==  OMX_EndianLittle);

        switch(sPCMModeParam->nBitPerSample){
        case 8:
          if(sPCMModeParam->eNumData == OMX_NumericalDataSigned) {
            snd_pcm_format = SND_PCM_FORMAT_S8;
          } else {
            snd_pcm_format = SND_PCM_FORMAT_U8;
          }
          break;
        case 16:
          if(sPCMModeParam->eNumData == OMX_NumericalDataSigned){
            if(sPCMModeParam->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S16_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S16_BE;
            }
          }
        if(sPCMModeParam->eNumData == OMX_NumericalDataUnsigned){
          if(sPCMModeParam->eEndian ==  OMX_EndianLittle){
            snd_pcm_format = SND_PCM_FORMAT_U16_LE;
          } else {
            snd_pcm_format = SND_PCM_FORMAT_U16_BE;
          }
        }
        break;
        case 24:
          if(sPCMModeParam->eNumData == OMX_NumericalDataSigned){
            if(sPCMModeParam->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S24_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S24_BE;
            }
          }
          if(sPCMModeParam->eNumData == OMX_NumericalDataUnsigned){
            if(sPCMModeParam->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_U24_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_U24_BE;
            }
          }
          break;

        case 32:
          if(sPCMModeParam->eNumData == OMX_NumericalDataSigned){
            if(sPCMModeParam->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_S32_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_S32_BE;
            }
          }
          if(sPCMModeParam->eNumData == OMX_NumericalDataUnsigned){
            if(sPCMModeParam->eEndian ==  OMX_EndianLittle) {
              snd_pcm_format = SND_PCM_FORMAT_U32_LE;
            } else {
              snd_pcm_format = SND_PCM_FORMAT_U32_BE;
            }
          }
          break;
        default:
          omxErr = OMX_ErrorBadParameter;
          break;
        }

        if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN){
          if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, snd_pcm_format)) < 0) {
            DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",  snd_strerror (err));
            return OMX_ErrorHardware;
          }
          memcpy(&omx_alsasink_component_Private->sPCMModeParam, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        } else{
          DEBUG(DEB_LEV_SIMPLE_SEQ, "ALSA OMX_IndexParamAudioPcm configured\n");
          memcpy(&omx_alsasink_component_Private->sPCMModeParam, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
      }
      else if(sPCMModeParam->ePCMMode == OMX_AUDIO_PCMModeALaw){
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
        if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_A_LAW)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n",  snd_strerror (err));
          return OMX_ErrorHardware;
        }
        memcpy(&omx_alsasink_component_Private->sPCMModeParam, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      }
      else if(sPCMModeParam->ePCMMode == OMX_AUDIO_PCMModeMULaw){
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring ALAW format\n\n");
        if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_MU_LAW)) < 0) {
          DEBUG(DEB_LEV_ERR, "cannot set sample format (%s)\n", snd_strerror (err));
          return OMX_ErrorHardware;
        }
        memcpy(&omx_alsasink_component_Private->sPCMModeParam, ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      }
      /** Configure and prepare the ALSA handle */
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Configuring the PCM interface\n");
      if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot set parameters (%s)\n",  snd_strerror (err));
        return OMX_ErrorHardware;
      }

      if ((err = snd_pcm_prepare (playback_handle)) < 0) {
        DEBUG(DEB_LEV_ERR, "cannot prepare audio interface for use (%s)\n", snd_strerror (err));
        return OMX_ErrorHardware;
      }
    }
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    omxErr = omx_base_component_ParameterSanityCheck(hComponent, pAudioMp3->nPortIndex, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(omxErr != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n", __func__, omxErr);
      break;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return omxErr;
}

OMX_ERRORTYPE omx_alsasink_component_GetParameter(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nParamIndex,
  OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_OTHER_PARAM_PORTFORMATTYPE *pOtherPortFormat;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_alsasink_component_PrivateType* omx_alsasink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *pPort = (omx_base_audio_PortType *) omx_alsasink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  omx_base_clock_PortType *pClockPort = (omx_base_clock_PortType *) omx_alsasink_component_Private->ports[1];
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
      break;
    }
    memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainAudio], sizeof(OMX_PORT_PARAM_TYPE));
    break;
  case OMX_IndexParamOtherInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
      break;
    }
    memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->sPortTypesParam[OMX_PortDomainOther], sizeof(OMX_PORT_PARAM_TYPE));
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
  case OMX_IndexParamAudioPcm:
    if(((OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure)->nPortIndex !=
      omx_alsasink_component_Private->sPCMModeParam.nPortIndex) {
      return OMX_ErrorBadParameter;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) {
      break;
    }
    memcpy(ComponentParameterStructure, &omx_alsasink_component_Private->sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;
  case  OMX_IndexParamOtherPortFormat:

      pOtherPortFormat = (OMX_OTHER_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;

      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (pOtherPortFormat->nPortIndex == 1) {
        memcpy(pOtherPortFormat, &pClockPort->sOtherParam, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
  default: /*Call the base component function*/
  return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}
