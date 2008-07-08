/**
  @file test/components/clocksrc/omxclocksrctest.c
  
  This simple application test clock component.
  
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
  
  $Date: 2008-06-27 11:40:05 +0200 (Fri, 27 Jun 2008) $
  Revision $Rev: 1417 $
  Author $Author: mmathur $
*/

#include "omxclocksrctest.h"

#define CLOCK_SRC              "OMX.st.clocksrc"
#define MAX_CLOCK_PORTS           8

static OMX_U32 nClockPort = MAX_CLOCK_PORTS;

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

OMX_CALLBACKTYPE clocksrccallbacks = {
  .EventHandler    = clocksrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = clocksrcFillBufferDone
};

/* Application private date: should go in the component field (segs...) */
appPrivateType*  appPriv;
OMX_ERRORTYPE    err;
OMX_HANDLETYPE   handle;
OMX_BUFFERHEADERTYPE *outBuffer[8];
static OMX_BOOL bEOS=OMX_FALSE;

int main(int argc, char** argv) {
  OMX_TIME_CONFIG_TIMESTAMPTYPE       sClientTimeStamp;
  OMX_TIME_CONFIG_CLOCKSTATETYPE      sClockState;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  sRefClock;
  OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
  OMX_S32 i;

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));
  appPriv->eventSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eventSem, 0);
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eofSem, 0);
  appPriv->clockEventSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->clockEventSem, 0); 
  

  /* Initialize the OpenMAX */
  err = OMX_Init();
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
    exit(1);
  }

  /* get the handle to the clock component */
  err = OMX_GetHandle(&handle, CLOCK_SRC, NULL, &clocksrccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_GetHandle failed\n");
    exit(1);
  }
  DEBUG(DEFAULT_MESSAGES,"Got the handle \n");

  /*Disabling last port*/
  err = OMX_SendCommand(handle, OMX_CommandPortDisable, 7, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"clock port disable failed\n");
    exit(1);
  }

  /*Wait for sink Ports Disable Event*/
  tsem_down(appPriv->eventSem);

  err = OMX_SendCommand(handle, OMX_CommandPortDisable, 6, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"clock port disable failed\n");
    exit(1);
  }

  /*Wait for sink Ports Disable Event*/
  tsem_down(appPriv->eventSem);

  /*Since last 2 ports are disabled*/
  nClockPort = 6;

  sPortDef.nPortIndex = 0;
  setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(handle,OMX_IndexParamPortDefinition, &sPortDef);

  DEBUG(DEFAULT_MESSAGES,"Changing state Idle \n");
  /** now set the Clock Component component to idle and executing state */
  OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  for(i=0;i<nClockPort;i++) {
    err = OMX_AllocateBuffer(handle, &outBuffer[i], i, NULL, sPortDef.nBufferSize);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer %i\n",(int)i);
      exit(1);
    }
  }

  /*Wait for Clock Component state change to */
  tsem_down(appPriv->eventSem);

  DEBUG(DEFAULT_MESSAGES,"Changing state Executing \n");

  OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  /*Wait for Clock Component state change to */
  tsem_down(appPriv->eventSem);

  /*Send output buffer to clock component*/
  for(i=0;i<nClockPort;i++) {
    err = OMX_FillThisBuffer(handle, outBuffer[i]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      exit(1);
    }
  }

  /* query the current wall clock time */
  //setHeader(&sClientTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
  //err = OMX_GetConfig(handle, OMX_IndexConfigTimeCurrentWallTime, &sClientTimeStamp);
//  if(err==OMX_ErrorNone) {
//    DEBUG(DEB_LEV_ERR,"the wall time is %0x \n",sClientTimeStamp.nTimestamp);
//  }
  
  /* set the clock state to OMX_TIME_ClockStateWaitingForStartTime */
  setHeader(&sClockState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
  err = OMX_GetConfig(handle, OMX_IndexConfigTimeClockState, &sClockState);
  sClockState.nWaitMask = 0x3F;
  sClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;
  err = OMX_SetConfig(handle, OMX_IndexConfigTimeClockState, &sClockState);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig \n",err);
    exit(1);
  }

 /* pass dummy  reference time from a client */

  for(i=0;i<nClockPort;i++) {
    setHeader(&sClientTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
    err = OMX_GetConfig(handle, OMX_IndexConfigTimeCurrentWallTime, &sClientTimeStamp);
    sClientTimeStamp.nPortIndex=i;

    DEBUG(DEFAULT_MESSAGES,"In %s Sending a start time for port %d\n",__func__,(int)sClientTimeStamp.nPortIndex);
    err = OMX_SetConfig(handle, OMX_IndexConfigTimeClientStartTime, &sClientTimeStamp);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig 0 \n",err);
    }
  }

  /*Wait for clock state running event*/
  for(i=0;i<nClockPort;i++) {
    DEBUG(DEFAULT_MESSAGES,"Waiting for clock event %i \n",(int)i);
    tsem_down(appPriv->clockEventSem); 
  }
  /*

  err = OMX_GetConfig(handle, OMX_IndexConfigTimeCurrentWallTime, &sClientTimeStamp);
  sClientTimeStamp.nPortIndex=1;
printf("sending a start time for port %d from application\n",sClientTimeStamp.nPortIndex);
  err = OMX_SetConfig(handle, OMX_IndexConfigTimeClientStartTime, &sClientTimeStamp);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig 0 \n",err);
  }*/

  /* set the audio as the master */
  setHeader(&sRefClock, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
  sRefClock.eClock = OMX_TIME_RefClockAudio;
  OMX_SetConfig(handle, OMX_IndexConfigTimeActiveRefClock,&sRefClock);
  sClockState.nWaitMask = 0x3F;
  sClockState.eState = OMX_TIME_ClockStateStopped;
  err = OMX_SetConfig(handle, OMX_IndexConfigTimeClockState, &sClockState);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig \n",err);
    exit(1);
  }

  /*Wait for clock state stop event*/
  for(i=0;i<nClockPort;i++) {
    tsem_down(appPriv->clockEventSem); 
  }
  /*set eos so that, further FTB is not called*/
  bEOS=OMX_TRUE;

  DEBUG(DEFAULT_MESSAGES,"Changing state Idle \n");
  OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  /*Wait for Clock Component state change to */
  tsem_down(appPriv->eventSem);

  DEBUG(DEFAULT_MESSAGES,"Changing state Loaded \n");
  OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  for(i=0;i<nClockPort;i++) {
    err = OMX_FreeBuffer(handle, i,outBuffer[i]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
      exit(1);
    }
  }
  /*Wait for Clock Component state change to */
  tsem_down(appPriv->eventSem);

  OMX_FreeHandle(handle);
printf("after the free handle call\n");

//  free(appPriv->eventSem);
  free(appPriv);
  return 0;
}

OMX_ERRORTYPE clocksrcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  DEBUG(DEFAULT_MESSAGES, "Hi there, I am in the %s callback\n", __func__);

  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "File Reader State changed in ");
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
      tsem_up(appPriv->eventSem);
    } else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->eventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->eventSem);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Received Event Event=%d Data1=%d,Data2=%d\n",__func__,eEvent,(int)Data1,(int)Data2);
    }
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Setting Changed event\n");
    /*Signal Port Setting Changed*/
    tsem_up(appPriv->eventSem);
  } else if(eEvent == OMX_EventPortFormatDetected) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port Format Detected %x\n", __func__,(int)Data1);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE clocksrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  /* Output data to audio decoder */

  if(pBuffer != NULL){
    if(!bEOS) {
      DEBUG(DEFAULT_MESSAGES,"Sending clock event for port %i \n",(int)pBuffer->nOutputPortIndex);
      /*Signal receiving of clock event*/
      tsem_up(appPriv->clockEventSem); 

      err = OMX_FillThisBuffer(handle, pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
      if(pBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Calling Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
        bEOS=OMX_TRUE;
      }
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

