/**
  @file src/components/clocksrc/omx_clocksrc_component.c

  OpenMAX clocksrc_component component. This component does not perform any multimedia
  processing.  It is provides the media and the reference clock for all the clients connected to it.

  Copyright (C) 2008  STMicroelectronics
  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

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
#include <omx_base_clock_port.h>
#include <omx_clocksrc_component.h>
#include <config.h>
#include <unistd.h>

#define MAX_COMPONENT_CLOCKSRC 1

#ifdef AV_SYNC_LOG
static FILE *fd;
#endif

/** Maximum Number of Clocksrc Instance*/
static OMX_U32 noClocksrcInstance=0;

/** The Constructor 
 */
OMX_ERRORTYPE omx_clocksrc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  int                                 omxErr;
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_clocksrc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }

  omx_clocksrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_clocksrc_component_Private->ports = NULL;
  
  omxErr = omx_base_source_Constructor(openmaxStandComp,cComponentName);
  if (omxErr != OMX_ErrorNone) {
    return OMX_ErrorInsufficientResources;
  }

  omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nStartPortNumber = 0;
  omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts = 3;

  /** Allocate Ports and call port constructor. */  
  if (omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts && !omx_clocksrc_component_Private->ports) {
    omx_clocksrc_component_Private->ports = calloc(omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts, sizeof(omx_base_PortType *));
    if (!omx_clocksrc_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts; i++) {
      omx_clocksrc_component_Private->ports[i] = calloc(1, sizeof(omx_base_clock_PortType));
      if (!omx_clocksrc_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
      base_clock_port_Constructor(openmaxStandComp, &omx_clocksrc_component_Private->ports[i], i, OMX_FALSE);
      omx_clocksrc_component_Private->ports[i]->FlushProcessingBuffers = clocksrc_port_FlushProcessingBuffers;
    }
  }

  
  /* initializing the OMX_TIME_CONFIG_CLOCKSTATETYPE */
  setHeader(&omx_clocksrc_component_Private->sClockState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));  
  omx_clocksrc_component_Private->sClockState.eState     = OMX_TIME_ClockStateStopped;
  omx_clocksrc_component_Private->sClockState.nStartTime = 0;
  omx_clocksrc_component_Private->sClockState.nOffset    = 0;
  omx_clocksrc_component_Private->sClockState.nWaitMask  = 0xFF;

  setHeader(&omx_clocksrc_component_Private->sMinStartTime, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));  
  omx_clocksrc_component_Private->sMinStartTime.nTimestamp = 0;
  omx_clocksrc_component_Private->sMinStartTime.nPortIndex = 0;

  setHeader(&omx_clocksrc_component_Private->sConfigScale, sizeof(OMX_TIME_CONFIG_SCALETYPE));  
  omx_clocksrc_component_Private->sConfigScale.xScale = 1<<16;  /* normal play mode */

  setHeader(&omx_clocksrc_component_Private->sRefClock, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));  
  omx_clocksrc_component_Private->sRefClock.eClock = OMX_TIME_RefClockNone;
  omx_clocksrc_component_Private->eUpdateType = OMX_TIME_UpdateMax;

  if(!omx_clocksrc_component_Private->clockEventSem) {
    omx_clocksrc_component_Private->clockEventSem = calloc(1,sizeof(tsem_t));
    tsem_init(omx_clocksrc_component_Private->clockEventSem, 0);
  }

  if(!omx_clocksrc_component_Private->clockEventCompleteSem) {
    omx_clocksrc_component_Private->clockEventCompleteSem = calloc(1,sizeof(tsem_t));
    tsem_init(omx_clocksrc_component_Private->clockEventCompleteSem, 0);
  }

  omx_clocksrc_component_Private->BufferMgmtCallback = omx_clocksrc_component_BufferMgmtCallback;
  omx_clocksrc_component_Private->destructor = omx_clocksrc_component_Destructor;
  omx_clocksrc_component_Private->BufferMgmtFunction = omx_clocksrc_BufferMgmtFunction;

#ifdef AV_SYNC_LOG
 fd = fopen("clock_timestamps.out","w");
#endif

  noClocksrcInstance++;
  if(noClocksrcInstance > MAX_COMPONENT_CLOCKSRC) {
    return OMX_ErrorInsufficientResources;
  }
  
  openmaxStandComp->SetParameter  = omx_clocksrc_component_SetParameter;
  openmaxStandComp->GetParameter  = omx_clocksrc_component_GetParameter;

  openmaxStandComp->SetConfig  = omx_clocksrc_component_SetConfig;
  openmaxStandComp->GetConfig  = omx_clocksrc_component_GetConfig;
  openmaxStandComp->SendCommand = omx_clocksrc_component_SendCommand;

  return OMX_ErrorNone;
}

/** The Destructor 
 */
OMX_ERRORTYPE omx_clocksrc_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  omx_clocksrc_component_Private->sClockState.eState = OMX_TIME_ClockStateMax;

  /*Deinitialize and free message semaphore*/
  if(omx_clocksrc_component_Private->clockEventSem) {
    tsem_deinit(omx_clocksrc_component_Private->clockEventSem);
    free(omx_clocksrc_component_Private->clockEventSem);
    omx_clocksrc_component_Private->clockEventSem=NULL;
  }
  if(omx_clocksrc_component_Private->clockEventCompleteSem) {
    tsem_deinit(omx_clocksrc_component_Private->clockEventCompleteSem);
    free(omx_clocksrc_component_Private->clockEventCompleteSem);
    omx_clocksrc_component_Private->clockEventCompleteSem=NULL;
  }
  
  /* frees port/s */
  if (omx_clocksrc_component_Private->ports) {
    for (i=0; i < omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts; i++) {
      if(omx_clocksrc_component_Private->ports[i])
        omx_clocksrc_component_Private->ports[i]->PortDestructor(omx_clocksrc_component_Private->ports[i]);
    }
    free(omx_clocksrc_component_Private->ports);
    omx_clocksrc_component_Private->ports=NULL;
  }

#ifdef AV_SYNC_LOG
     fclose(fd);
#endif
  
  noClocksrcInstance--;

  return omx_base_source_Destructor(openmaxStandComp);
}

OMX_ERRORTYPE omx_clocksrc_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE                          err = OMX_ErrorNone;
  OMX_OTHER_PARAM_PORTFORMATTYPE         *pOtherPortFormat;
  OMX_COMPONENTTYPE                      *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_clocksrc_component_PrivateType*    omx_clocksrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_clock_PortType*               pPort; // = (omx_base_clock_PortType *) omx_clocksrc_component_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];  
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamOtherInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther], sizeof(OMX_PORT_PARAM_TYPE));
    break; 
  case OMX_IndexParamOtherPortFormat:
    pOtherPortFormat = (OMX_OTHER_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pOtherPortFormat->nPortIndex < omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) {
      pPort = (omx_base_clock_PortType *) omx_clocksrc_component_Private->ports[pOtherPortFormat->nPortIndex];  
      memcpy(pOtherPortFormat, &pPort->sOtherParam, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;    
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_clocksrc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE                         err = OMX_ErrorNone;
  OMX_OTHER_PARAM_PORTFORMATTYPE        *pOtherPortFormat;
  OMX_COMPONENTTYPE                     *openmaxStandComp = (OMX_COMPONENTTYPE*)hComponent;
  omx_clocksrc_component_PrivateType*   omx_clocksrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_clock_PortType*              pPort; 

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamOtherPortFormat:
    pOtherPortFormat = (OMX_OTHER_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, pOtherPortFormat->nPortIndex, pOtherPortFormat, sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n", __func__, err); 
      break;
    } 
    if (pOtherPortFormat->nPortIndex < omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) {
      pPort = (omx_base_clock_PortType *) omx_clocksrc_component_Private->ports[pOtherPortFormat->nPortIndex];  
      memcpy(&pPort->sOtherParam,pOtherPortFormat,sizeof(OMX_OTHER_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_clocksrc_component_SendCommand(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_COMMANDTYPE Cmd,
  OMX_IN  OMX_U32 nParam,
  OMX_IN  OMX_PTR pCmdData) {
  OMX_COMPONENTTYPE*                  omxComponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private = (omx_clocksrc_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_U32 nMask;

  switch (Cmd) {
  case OMX_CommandPortDisable:
    if (nParam >= omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    if(nParam == OMX_ALL) {
      nMask = 0xFF;
    } else {
      nMask = 0x1 << nParam;
    }
    omx_clocksrc_component_Private->sClockState.nWaitMask &= (~nMask);
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s nWaitMask =%08x Musk=%x\n",__func__,
      (int)omx_clocksrc_component_Private->sClockState.nWaitMask,(int)(~nMask));

    break;
  case OMX_CommandPortEnable:
    if (nParam >= omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    if(nParam == OMX_ALL) {
      nMask = 0xFF;
    } else {
      nMask = 0x1 << nParam;
    }
    omx_clocksrc_component_Private->sClockState.nWaitMask &= nMask;
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s nWaitMask =%08x Musk=%x\n",__func__,
      (int)omx_clocksrc_component_Private->sClockState.nWaitMask,(int)nMask);
    break;
  case OMX_CommandStateSet:
    if ((nParam == OMX_StateLoaded) && (omx_clocksrc_component_Private->state == OMX_StateIdle)) {
      omx_clocksrc_component_Private->transientState = OMX_TransStateIdleToLoaded;
      /*Signal buffer management thread to exit*/
      tsem_up(omx_clocksrc_component_Private->clockEventSem);
    } else if ((nParam == OMX_StateExecuting) && (omx_clocksrc_component_Private->state == OMX_StatePause)) {
      /*Dummy signal to the clock buffer management function*/
      omx_clocksrc_component_Private->transientState = OMX_TransStatePauseToExecuting;
      tsem_up(omx_clocksrc_component_Private->clockEventSem);
    } else if (nParam == OMX_StateInvalid) {
      omx_clocksrc_component_Private->transientState = OMX_TransStateInvalid;
      /*Signal buffer management thread to exit*/
      tsem_up(omx_clocksrc_component_Private->clockEventSem);
    }
    break;
  default:
    break;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s calling omx_base_component_SendCommand\n",__func__);

  return omx_base_component_SendCommand(hComponent,Cmd,nParam,pCmdData);
}

OMX_ERRORTYPE omx_clocksrc_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure) {

  OMX_COMPONENTTYPE*                  omxComponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private = (omx_clocksrc_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_TIME_CONFIG_CLOCKSTATETYPE*     clockstate;
  OMX_TIME_CONFIG_TIMESTAMPTYPE*      timestamp;
  OMX_TIME_CONFIG_SCALETYPE           *pConfigScale;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  *pRefClock;
  struct timeval                      tv;
  struct timezone                     zv;

  switch (nIndex) {
  case OMX_IndexConfigTimeClockState :
    clockstate = (OMX_TIME_CONFIG_CLOCKSTATETYPE*) pComponentConfigStructure;
    memcpy(clockstate, &omx_clocksrc_component_Private->sClockState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
    break;
  case OMX_IndexConfigTimeCurrentWallTime :
    timestamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*) pComponentConfigStructure;
    gettimeofday(&tv,&zv);
    timestamp->nTimestamp =  (tv.tv_sec)*1000+tv.tv_usec;  // converting the time read from gettimeofday into microseconds
    DEBUG(DEB_LEV_SIMPLE_SEQ,"wall time obtained in %s =%x\n",__func__,(int)timestamp->nTimestamp);
    break;
  case OMX_IndexConfigTimeCurrentMediaTime :
    DEBUG(DEB_LEV_SIMPLE_SEQ," TBD  portindex to be returned is OMX_ALL, OMX_IndexConfigTimeCurrentMediaTime in %s \n",__func__);
    break;
  case OMX_IndexConfigTimeScale:
    pConfigScale = (OMX_TIME_CONFIG_SCALETYPE*) pComponentConfigStructure;
    memcpy(pConfigScale, &omx_clocksrc_component_Private->sConfigScale, sizeof(OMX_TIME_CONFIG_SCALETYPE));
    break;
  case OMX_IndexConfigTimeActiveRefClock :
     pRefClock = (OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE*) pComponentConfigStructure;
     memcpy(pRefClock,&omx_clocksrc_component_Private->sRefClock, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
     break;
  default:
    return OMX_ErrorBadParameter;
    break;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_clocksrc_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {

  OMX_COMPONENTTYPE*                  omxComponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private = (omx_clocksrc_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_TIME_CONFIG_CLOCKSTATETYPE*     clockstate;
  OMX_TIME_CONFIG_TIMESTAMPTYPE*      sRefTimeStamp;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE* pRefClock;
  OMX_U32                             portIndex;
  omx_base_clock_PortType             *pPort;
  OMX_TIME_CONFIG_SCALETYPE           *pConfigScale;
  OMX_U32                             nMask;
  OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE* sMediaTimeRequest;
  int                                 i;
  struct timeval                      tv;
  struct timezone                     zv;
  OMX_TICKS                           walltime, mediatime, mediaTimediff, wallTimediff;
  OMX_S32                             Scale;
  unsigned int                        sleeptime;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  switch (nIndex) {
  case OMX_IndexConfigTimeClockState : {
    clockstate = (OMX_TIME_CONFIG_CLOCKSTATETYPE*) pComponentConfigStructure;
    switch (clockstate->eState) {
      case OMX_TIME_ClockStateRunning:
        if(omx_clocksrc_component_Private->sClockState.eState == OMX_TIME_ClockStateRunning) {
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received OMX_TIME_ClockStateRunning again\n",__func__);
        }
        DEBUG(DEB_LEV_SIMPLE_SEQ,"in  %s ...set to OMX_TIME_ClockStateRunning\n",__func__);
        memcpy(&omx_clocksrc_component_Private->sClockState, clockstate, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
        omx_clocksrc_component_Private->eUpdateType = OMX_TIME_UpdateClockStateChanged;  
        /* update the state change in all port */
        for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
          pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[i];
          pPort->sMediaTime.eUpdateType                      = OMX_TIME_UpdateClockStateChanged;
          pPort->sMediaTime.eState                           = OMX_TIME_ClockStateRunning;
          pPort->sMediaTime.xScale                           = omx_clocksrc_component_Private->sConfigScale.xScale;
        }
        /*Signal Buffer Management Thread*/ 
        tsem_up(omx_clocksrc_component_Private->clockEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for Clock Running Event for all ports\n");
        tsem_down(omx_clocksrc_component_Private->clockEventCompleteSem);
      break;
      case OMX_TIME_ClockStateWaitingForStartTime:
        if(omx_clocksrc_component_Private->sClockState.eState == OMX_TIME_ClockStateRunning) {
          return OMX_ErrorIncorrectStateTransition;
        } else if(omx_clocksrc_component_Private->sClockState.eState == OMX_TIME_ClockStateWaitingForStartTime) {
          return OMX_ErrorSameState;
        }
        DEBUG(DEB_LEV_SIMPLE_SEQ," in  %s ...set to OMX_TIME_ClockStateWaitingForStartTime  mask sent=%d\n",__func__,(int)clockstate->nWaitMask);
        memcpy(&omx_clocksrc_component_Private->sClockState, clockstate, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
      break;
      case OMX_TIME_ClockStateStopped:
        DEBUG(DEB_LEV_SIMPLE_SEQ," in  %s ...set to OMX_TIME_ClockStateStopped\n",__func__);
        memcpy(&omx_clocksrc_component_Private->sClockState, clockstate, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
        omx_clocksrc_component_Private->eUpdateType = OMX_TIME_UpdateClockStateChanged;
        /* update the state change in all port */
        for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
          pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[i];
          pPort->sMediaTime.eUpdateType                      = OMX_TIME_UpdateClockStateChanged;
          pPort->sMediaTime.eState                           = OMX_TIME_ClockStateStopped;
          pPort->sMediaTime.xScale                           = omx_clocksrc_component_Private->sConfigScale.xScale;
        }
        /*Signal Buffer Management Thread*/
        tsem_up(omx_clocksrc_component_Private->clockEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for Clock Stop Event for all ports\n");
        tsem_down(omx_clocksrc_component_Private->clockEventCompleteSem);  
      break;
      default:
      break;
    }
   }
    break;
  case OMX_IndexConfigTimeClientStartTime:
    sRefTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*) pComponentConfigStructure;
    portIndex = sRefTimeStamp->nPortIndex;
    if(portIndex > omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) {
     return OMX_ErrorBadPortIndex;
    }

    pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[portIndex];
    if(!PORT_IS_ENABLED(pPort)) {
     DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port is disabled \n",__func__);
     return OMX_ErrorBadParameter;
    }
    memcpy(&pPort->sTimeStamp, sRefTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));  

    /* update the nWaitMask to clear the flag for the client which has sent its start time */
    if(omx_clocksrc_component_Private->sClockState.nWaitMask) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"refTime set is =%x \n",(int)pPort->sTimeStamp.nTimestamp);
      nMask = ~(0x1 << portIndex);
      omx_clocksrc_component_Private->sClockState.nWaitMask = omx_clocksrc_component_Private->sClockState.nWaitMask & nMask;
      if(omx_clocksrc_component_Private->sMinStartTime.nTimestamp >= pPort->sTimeStamp.nTimestamp){
         omx_clocksrc_component_Private->sMinStartTime.nTimestamp = pPort->sTimeStamp.nTimestamp;
         omx_clocksrc_component_Private->sMinStartTime.nPortIndex = pPort->sTimeStamp.nPortIndex;
      } 
    }
    if(!omx_clocksrc_component_Private->sClockState.nWaitMask && 
       omx_clocksrc_component_Private->sClockState.eState == OMX_TIME_ClockStateWaitingForStartTime) {
       omx_clocksrc_component_Private->sClockState.eState = OMX_TIME_ClockStateRunning;
      omx_clocksrc_component_Private->sClockState.nStartTime = omx_clocksrc_component_Private->sMinStartTime.nTimestamp; 
      omx_clocksrc_component_Private->MediaTimeBase          = omx_clocksrc_component_Private->sMinStartTime.nTimestamp; 
      gettimeofday(&tv,&zv);
      walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
      omx_clocksrc_component_Private->WallTimeBase          = walltime; 
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Mediatimebase=%llx walltimebase=%llx \n",omx_clocksrc_component_Private->MediaTimeBase,omx_clocksrc_component_Private->WallTimeBase);
      omx_clocksrc_component_Private->eUpdateType        = OMX_TIME_UpdateClockStateChanged;
      /* update the state change in all port */
      for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
        pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[i];
        pPort->sMediaTime.eUpdateType                      = OMX_TIME_UpdateClockStateChanged;
        pPort->sMediaTime.eState                           = OMX_TIME_ClockStateRunning;
        pPort->sMediaTime.xScale                           = omx_clocksrc_component_Private->sConfigScale.xScale;
      }
      /*Signal Buffer Management Thread*/
      tsem_up(omx_clocksrc_component_Private->clockEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"setting the state to running from %s \n",__func__);  
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Waiting for Clock Running Event for all ports in case OMX_IndexConfigTimeClientStartTime\n");
      tsem_down(omx_clocksrc_component_Private->clockEventCompleteSem);
    }
    break;

  case OMX_IndexConfigTimeActiveRefClock :
     pRefClock = (OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE*) pComponentConfigStructure;
     memcpy(&omx_clocksrc_component_Private->sRefClock, pRefClock, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
  break;

  case OMX_IndexConfigTimeCurrentAudioReference:
    sRefTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*) pComponentConfigStructure;
    portIndex = sRefTimeStamp->nPortIndex;
    if(portIndex > omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) {
     return OMX_ErrorBadPortIndex;
    }
    pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[portIndex];
    memcpy(&pPort->sTimeStamp, sRefTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
    gettimeofday(&tv,&zv);
    walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
    omx_clocksrc_component_Private->WallTimeBase   = walltime;
    omx_clocksrc_component_Private->MediaTimeBase  = sRefTimeStamp->nTimestamp; /* set the mediatime base of the received time stamp*/
  break;

  case OMX_IndexConfigTimeCurrentVideoReference:
    sRefTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE*) pComponentConfigStructure;
    portIndex = sRefTimeStamp->nPortIndex;
    if(portIndex > omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) {
      return OMX_ErrorBadPortIndex;
    }
    pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[portIndex];
    memcpy(&pPort->sTimeStamp, sRefTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
    gettimeofday(&tv,&zv);
    walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
    omx_clocksrc_component_Private->WallTimeBase   = walltime;
    omx_clocksrc_component_Private->MediaTimeBase  = sRefTimeStamp->nTimestamp; /* set the mediatime base of the received time stamp*/
  break;

  case OMX_IndexConfigTimeScale:
    /* update the mediatime base and walltime base using the current scale value*/
    Scale = omx_clocksrc_component_Private->sConfigScale.xScale >> 16;  //* the scale currently in use, right shifted as Q16 format is used for the scale
    gettimeofday(&tv,&zv);
    walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
    mediatime = omx_clocksrc_component_Private->MediaTimeBase + Scale*(walltime - omx_clocksrc_component_Private->WallTimeBase);
    omx_clocksrc_component_Private->WallTimeBase   = walltime; // suitable start time to be used here
    omx_clocksrc_component_Private->MediaTimeBase  = mediatime;  // TODO - needs to be checked 

    /* update the new scale value */
    pConfigScale = (OMX_TIME_CONFIG_SCALETYPE*) pComponentConfigStructure;
    memcpy( &omx_clocksrc_component_Private->sConfigScale,pConfigScale, sizeof(OMX_TIME_CONFIG_SCALETYPE));
    omx_clocksrc_component_Private->eUpdateType = OMX_TIME_UpdateScaleChanged;
    /* update the scale change in all ports */
    for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
      pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[i];
      pPort->sMediaTime.eUpdateType                      = OMX_TIME_UpdateScaleChanged;
      pPort->sMediaTime.eState                           = OMX_TIME_ClockStateRunning;
      pPort->sMediaTime.xScale                           = omx_clocksrc_component_Private->sConfigScale.xScale;
      pPort->sMediaTime.nMediaTimestamp                  = omx_clocksrc_component_Private->MediaTimeBase;
      pPort->sMediaTime.nWallTimeAtMediaTime             = omx_clocksrc_component_Private->WallTimeBase;
      }
    /*Signal Buffer Management Thread*/
    tsem_up(omx_clocksrc_component_Private->clockEventSem);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for Scale Change Event for all ports\n");
    tsem_down(omx_clocksrc_component_Private->clockEventCompleteSem);
  break;

  case OMX_IndexConfigTimeMediaTimeRequest:
    Scale = omx_clocksrc_component_Private->sConfigScale.xScale >> 16;

    if(omx_clocksrc_component_Private->sClockState.eState != OMX_TIME_ClockStateStopped && Scale != 0) {//TODO-  what happens if request comes in pause mode

      sMediaTimeRequest = (OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE*) pComponentConfigStructure;
      portIndex = sMediaTimeRequest->nPortIndex; 
      pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[portIndex];
      memcpy(&pPort->sMediaTimeRequest, sMediaTimeRequest, sizeof(OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE));  

      gettimeofday(&tv,&zv);
      walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
      mediatime = omx_clocksrc_component_Private->MediaTimeBase + Scale*(walltime - omx_clocksrc_component_Private->WallTimeBase);
      int thresh=2000;  // TODO - what is a good threshold to use
      mediaTimediff = (sMediaTimeRequest->nMediaTimestamp - (sMediaTimeRequest->nOffset*Scale)) - mediatime;
      DEBUG(DEB_LEV_SIMPLE_SEQ," pI=%d MTD=%lld MT=%lld RT=%lld offset=%lld, Scale=%d\n",
               (int)portIndex,mediaTimediff,mediatime,sMediaTimeRequest->nMediaTimestamp,sMediaTimeRequest->nOffset,(int)Scale);
      if((mediaTimediff<0 && Scale>0) || (mediaTimediff>0 && Scale<0)) { /* if mediatime has already elapsed then request can not be fullfilled */
        DEBUG(DEB_LEV_SIMPLE_SEQ," pI=%d RNF MTD<0 MB=%lld WB=%lld MT=%lld RT=%lld WT=%lld offset=%lld, Scale=%d\n",
                 (int)portIndex,omx_clocksrc_component_Private->MediaTimeBase,omx_clocksrc_component_Private->WallTimeBase,
                  mediatime,sMediaTimeRequest->nMediaTimestamp,walltime,sMediaTimeRequest->nOffset,(int)Scale);
        pPort->sMediaTime.eUpdateType          =  OMX_TIME_UpdateRequestFulfillment; // TODO : to be checked 
        pPort->sMediaTime.nMediaTimestamp      = sMediaTimeRequest->nMediaTimestamp;
        pPort->sMediaTime.nOffset              = 0xFFFFFFFF;  
       }else{
         wallTimediff  = mediaTimediff/Scale;
         if(mediaTimediff){
            if(wallTimediff>thresh) {
                sleeptime = (unsigned int) (wallTimediff-thresh);
                usleep(sleeptime);
                wallTimediff = thresh;  // ask : can I use this as the new walltimediff
                gettimeofday(&tv,&zv);
                walltime = ((OMX_TICKS)tv.tv_sec)*1000000 + ((OMX_TICKS)tv.tv_usec);
                mediatime = omx_clocksrc_component_Private->MediaTimeBase + Scale*(walltime - omx_clocksrc_component_Private->WallTimeBase);
            }
            //pPort->sMediaTime.nMediaTimestamp      = mediatime;
            pPort->sMediaTime.nMediaTimestamp      = sMediaTimeRequest->nMediaTimestamp;  ///????
            pPort->sMediaTime.nWallTimeAtMediaTime = walltime + wallTimediff;        //??????
            pPort->sMediaTime.nOffset              = wallTimediff;                   //????
            pPort->sMediaTime.xScale               = Scale;
            pPort->sMediaTime.eUpdateType          = OMX_TIME_UpdateRequestFulfillment;
            DEBUG(DEB_LEV_SIMPLE_SEQ,"pI=%d MB=%lld WB=%lld MT=%lld RT=%lld WT=%lld \n",(int)portIndex,
                omx_clocksrc_component_Private->MediaTimeBase,omx_clocksrc_component_Private->WallTimeBase, mediatime,sMediaTimeRequest->nMediaTimestamp,walltime);
#ifdef AV_SYNC_LOG
              fprintf(fd,"%d %lld %lld %lld %lld %lld\n",
                  (int)portIndex,sMediaTimeRequest->nMediaTimestamp,mediatime,pPort->sMediaTime.nWallTimeAtMediaTime,wallTimediff,mediaTimediff);
#endif
         } 
      }
      /*Signal Buffer Management Thread*/
      tsem_up(omx_clocksrc_component_Private->clockEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for Scale Change Event for all ports\n");
      tsem_down(omx_clocksrc_component_Private->clockEventCompleteSem);
    } else {
       DEBUG(DEB_LEV_ERR,"In %s Clock State=%x Scale=%x Line=%d \n",
          __func__,(int)omx_clocksrc_component_Private->sClockState.eState,(int)Scale,__LINE__);
    }
  break;

  default:
    return OMX_ErrorBadParameter;
    break;
  }
  return OMX_ErrorNone;
}

/** 
 * This function plays the input buffer. When fully consumed it returns.
 */
void omx_clocksrc_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* outputbuffer) {
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_clock_PortType             *pPort = (omx_base_clock_PortType*)omx_clocksrc_component_Private->ports[outputbuffer->nOutputPortIndex];

  memcpy(outputbuffer->pBuffer,&pPort->sMediaTime,sizeof(OMX_TIME_MEDIATIMETYPE));
  outputbuffer->nFilledLen = sizeof(OMX_TIME_MEDIATIMETYPE);
  pPort->sMediaTime.eUpdateType = OMX_TIME_UpdateMax; /* clear the update type */

}
/** This is the central function for buffer processing of a two port source component.
  * It is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */

void* omx_clocksrc_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE*                  openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType*     omx_base_component_Private=(omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private  = (omx_clocksrc_component_PrivateType*)omx_base_component_Private;
  omx_base_clock_PortType             *pOutPort[MAX_CLOCK_PORTS];    
  tsem_t*                             pOutputSem[MAX_CLOCK_PORTS];
  queue_t*                            pOutputQueue[MAX_CLOCK_PORTS];
  OMX_BUFFERHEADERTYPE*               pOutputBuffer[MAX_CLOCK_PORTS];
  OMX_BOOL                            isOutputBufferNeeded[MAX_CLOCK_PORTS],bPortsBeingFlushed = OMX_FALSE;
  int                                 i,j,outBufExchanged[MAX_CLOCK_PORTS];

  for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
    pOutPort[i]             = (omx_base_clock_PortType *)omx_clocksrc_component_Private->ports[i];
    pOutputSem[i]           = pOutPort[i]->pBufferSem;
    pOutputQueue[i]         = pOutPort[i]->pBufferQueue;
    pOutputBuffer[i]        = NULL;
    isOutputBufferNeeded[i] = OMX_TRUE;
    outBufExchanged[i]      = 0;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  while(omx_clocksrc_component_Private->state == OMX_StateIdle 
         || omx_clocksrc_component_Private->state == OMX_StateExecuting 
         || omx_clocksrc_component_Private->state == OMX_StatePause 
         || omx_clocksrc_component_Private->transientState == OMX_TransStateLoadedToIdle){

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_clocksrc_component_Private->flush_mutex);
    for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
      bPortsBeingFlushed |= PORT_IS_BEING_FLUSHED(pOutPort[i]);
    }
    while(bPortsBeingFlushed) {
      pthread_mutex_unlock(&omx_clocksrc_component_Private->flush_mutex);
      for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
          if(isOutputBufferNeeded[i]==OMX_FALSE && PORT_IS_BEING_FLUSHED(pOutPort[i])) {
          pOutPort[i]->ReturnBufferFunction((omx_base_PortType*)pOutPort[i],pOutputBuffer[i]);
          outBufExchanged[i]--;
          pOutputBuffer[1]=NULL;
          isOutputBufferNeeded[i]=OMX_TRUE;
          DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning output buffer for port %i\n",i);
        }
      }
      
      tsem_up(omx_clocksrc_component_Private->flush_all_condition);
      tsem_down(omx_clocksrc_component_Private->flush_condition);
      pthread_mutex_lock(&omx_clocksrc_component_Private->flush_mutex);

      bPortsBeingFlushed = OMX_FALSE;
      for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
        bPortsBeingFlushed |= PORT_IS_BEING_FLUSHED(pOutPort[i]);
      }
    }
    pthread_mutex_unlock(&omx_clocksrc_component_Private->flush_mutex);

    /*Wait for clock state event*/
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Waiting for clock event\n",__func__);
    tsem_down(omx_clocksrc_component_Private->clockEventSem);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s clock event occured semval=%d \n",__func__,omx_clocksrc_component_Private->clockEventSem->semval);

    /*If port is not tunneled then simply return the buffer except paused state*/
    if(omx_clocksrc_component_Private->transientState == OMX_TransStatePauseToExecuting) {
      for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
        if(!PORT_IS_TUNNELED(pOutPort[i])) {

          if(pOutputSem[i]->semval>0 && isOutputBufferNeeded[i]==OMX_TRUE ) {
            tsem_down(pOutputSem[i]);
            if(pOutputQueue[i]->nelem>0){
              outBufExchanged[i]++;
              isOutputBufferNeeded[i]=OMX_FALSE;
              pOutputBuffer[i] = dequeue(pOutputQueue[i]);
              if(pOutputBuffer[i] == NULL){
                DEBUG(DEB_LEV_ERR, "Had NULL output buffer!!\n");
                break;
              }
            }
          }

          if(isOutputBufferNeeded[i]==OMX_FALSE) {
            /*Output Buffer has been produced or EOS. So, return output buffer and get new buffer*/
            if(pOutputBuffer[i]->nFilledLen!=0) {
              DEBUG(DEB_LEV_ERR, "In %s Returning Output nFilledLen=%d (line=%d)\n",
                __func__,(int)pOutputBuffer[i]->nFilledLen,__LINE__);
              pOutPort[i]->ReturnBufferFunction((omx_base_PortType*)pOutPort[i],pOutputBuffer[i]);
              outBufExchanged[i]--;
              pOutputBuffer[i]=NULL;
              isOutputBufferNeeded[i]=OMX_TRUE;
            }
          }
        }
      }
      omx_clocksrc_component_Private->transientState = OMX_TransStateMax;
    }
    
    if(omx_clocksrc_component_Private->state == OMX_StateLoaded  || 
       omx_clocksrc_component_Private->state == OMX_StateInvalid ||
       omx_clocksrc_component_Private->transientState == OMX_TransStateIdleToLoaded ||
       omx_clocksrc_component_Private->transientState == OMX_TransStateInvalid) {

      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting (line %d)\n",__func__,__LINE__);
      break;
    }

    for(i=0;i<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;i++) {
      if(pOutPort[i]->sMediaTime.eUpdateType == OMX_TIME_UpdateClockStateChanged || 
         pOutPort[i]->sMediaTime.eUpdateType == OMX_TIME_UpdateScaleChanged      || 
         pOutPort[i]->sMediaTime.eUpdateType == OMX_TIME_UpdateRequestFulfillment) {  

        if((isOutputBufferNeeded[i]==OMX_TRUE && pOutputSem[i]->semval==0) && 
          (omx_clocksrc_component_Private->state != OMX_StateLoaded && omx_clocksrc_component_Private->state != OMX_StateInvalid) 
          && PORT_IS_ENABLED(pOutPort[i])) {
          //Signalled from EmptyThisBuffer or FillThisBuffer or some where else
          DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next output buffer %i\n",i);
          tsem_down(omx_clocksrc_component_Private->bMgmtSem);
        }
        if(omx_clocksrc_component_Private->state == OMX_StateLoaded  || 
           omx_clocksrc_component_Private->state == OMX_StateInvalid ||
           omx_clocksrc_component_Private->transientState == OMX_TransStateIdleToLoaded ||
           omx_clocksrc_component_Private->transientState == OMX_TransStateInvalid) {
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting (line %d)\n",__func__,__LINE__);
          break;
        }
        
        if(pOutputSem[i]->semval>0 && isOutputBufferNeeded[i]==OMX_TRUE ) {
          tsem_down(pOutputSem[i]);
          if(pOutputQueue[i]->nelem>0){
            outBufExchanged[i]++;
            isOutputBufferNeeded[i]=OMX_FALSE;
            pOutputBuffer[i] = dequeue(pOutputQueue[i]);
            if(pOutputBuffer[i] == NULL){
              DEBUG(DEB_LEV_ERR, "Had NULL output buffer!!\n");
              break;
            }
          }
        } else {
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Output buffer not available Port %d (line=%d)\n",__func__,(int)i,__LINE__);

          /*Check if any dummy bMgmtSem signal and ports are flushing*/
          pthread_mutex_lock(&omx_clocksrc_component_Private->flush_mutex);
          bPortsBeingFlushed = OMX_FALSE;
          for(j=0;j<omx_clocksrc_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;j++) {
            bPortsBeingFlushed |= PORT_IS_BEING_FLUSHED(pOutPort[j]);
          }
          pthread_mutex_unlock(&omx_clocksrc_component_Private->flush_mutex);
          if(bPortsBeingFlushed) {
            DEBUG(DEB_LEV_ERR, "In %s Ports are being flushed - breaking (line %d)\n",__func__,__LINE__);
            break;
          }
        }
        /*Process Output buffer of Port i */
        if(isOutputBufferNeeded[i]==OMX_FALSE) {
          if (omx_clocksrc_component_Private->BufferMgmtCallback) {
            (*(omx_clocksrc_component_Private->BufferMgmtCallback))(openmaxStandComp, pOutputBuffer[i]);
          } else {
            /*If no buffer management call back then don't produce any output buffer*/
            pOutputBuffer[i]->nFilledLen = 0;
          }
      
           /*Output Buffer has been produced or EOS. So, return output buffer and get new buffer*/
          if(pOutputBuffer[i]->nFilledLen!=0) {
            pOutPort[i]->ReturnBufferFunction((omx_base_PortType*)pOutPort[i],pOutputBuffer[i]);
            outBufExchanged[i]--;
            pOutputBuffer[i]=NULL;
            isOutputBufferNeeded[i]=OMX_TRUE;
          }
        }
      }
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "Sent Clock Event for all ports\n");
    tsem_up(omx_clocksrc_component_Private->clockEventCompleteSem);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}

/** @brief Releases buffers under processing.
 * This function must be implemented in the derived classes, for the
 * specific processing
 */
OMX_ERRORTYPE clocksrc_port_FlushProcessingBuffers(omx_base_PortType *openmaxStandPort) {
  omx_clocksrc_component_PrivateType* omx_clocksrc_component_Private;
  OMX_BUFFERHEADERTYPE* pBuffer;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_clocksrc_component_Private = (omx_clocksrc_component_PrivateType*)openmaxStandPort->standCompContainer->pComponentPrivate;

  pthread_mutex_lock(&omx_clocksrc_component_Private->flush_mutex);
  openmaxStandPort->bIsPortFlushed=OMX_TRUE;
  /*Signal the buffer management thread of port flush,if it is waiting for buffers*/
  if(omx_clocksrc_component_Private->bMgmtSem->semval==0) {
    tsem_up(omx_clocksrc_component_Private->bMgmtSem);
  }
  tsem_up(omx_clocksrc_component_Private->clockEventSem);
  tsem_up(omx_clocksrc_component_Private->clockEventCompleteSem);

  if(omx_clocksrc_component_Private->state==OMX_StatePause ) {
    /*Waiting at paused state*/
    tsem_signal(omx_clocksrc_component_Private->bStateSem);
  }
  DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for flush all condition port index =%d\n", __func__,(int)openmaxStandPort->sPortParam.nPortIndex);
  /* Wait until flush is completed */
  pthread_mutex_unlock(&omx_clocksrc_component_Private->flush_mutex);
  tsem_down(omx_clocksrc_component_Private->flush_all_condition);

  tsem_reset(omx_clocksrc_component_Private->bMgmtSem);
  tsem_reset(omx_clocksrc_component_Private->clockEventSem);

  /* Flush all the buffers not under processing */
  while (openmaxStandPort->pBufferSem->semval > 0) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s TFlag=%x Flusing Port=%d,Semval=%d Qelem=%d\n", 
    __func__,(int)openmaxStandPort->nTunnelFlags,(int)openmaxStandPort->sPortParam.nPortIndex,
    (int)openmaxStandPort->pBufferSem->semval,(int)openmaxStandPort->pBufferQueue->nelem);

    tsem_down(openmaxStandPort->pBufferSem);
    pBuffer = dequeue(openmaxStandPort->pBufferQueue);
    if (PORT_IS_TUNNELED(openmaxStandPort) && !PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s is returning io:%d buffer\n", 
        __func__,omx_clocksrc_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
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
        omx_clocksrc_component_Private->callbackData,
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

  pthread_mutex_lock(&omx_clocksrc_component_Private->flush_mutex);
  openmaxStandPort->bIsPortFlushed=OMX_FALSE;
  pthread_mutex_unlock(&omx_clocksrc_component_Private->flush_mutex);

  tsem_up(omx_clocksrc_component_Private->flush_condition);

  DEBUG(DEB_LEV_FULL_SEQ, "Out %s Port Index=%d bIsPortFlushed=%d Component %s\n", __func__,
    (int)openmaxStandPort->sPortParam.nPortIndex,(int)openmaxStandPort->bIsPortFlushed,omx_clocksrc_component_Private->name);

  DEBUG(DEB_LEV_PARAMS, "In %s TFlag=%x Qelem=%d BSem=%d bMgmtsem=%d component=%s\n", __func__,
    (int)openmaxStandPort->nTunnelFlags,
    (int)openmaxStandPort->pBufferQueue->nelem,
    (int)openmaxStandPort->pBufferSem->semval,
    (int)omx_clocksrc_component_Private->bMgmtSem->semval,
    omx_clocksrc_component_Private->name);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out %s Port Index=%d\n", __func__,(int)openmaxStandPort->sPortParam.nPortIndex);

  return OMX_ErrorNone;
}

