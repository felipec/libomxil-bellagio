/**
  @file src/base/omx_base_sink.c

  OpenMax base sink component. This component does not perform any multimedia
  processing. It derives from base component and contains a single input port. 
  It can be used as base class for sink components.

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

  $Date: 2007-04-06 13:15:30 +0200 (Fri, 06 Apr 2007) $
  Revision $Rev: 788 $
  Author $Author: giulio_urlini $
*/

#include <omxcore.h>

#include <omx_base_sink.h>

OMX_ERRORTYPE omx_base_sink_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_base_sink_PrivateType* omx_base_sink_Private;

  if (openmaxStandComp->pComponentPrivate) {
    omx_base_sink_Private = (omx_base_sink_PrivateType*)openmaxStandComp->pComponentPrivate;
  } else {
    omx_base_sink_Private = malloc(sizeof(omx_base_sink_PrivateType));
    if (!omx_base_sink_Private) {
      return OMX_ErrorInsufficientResources;
    }
  }

  // we could create our own port structures here
  // fixme maybe the base class could use a "port factory" function pointer?	
  err = omx_base_component_Constructor(openmaxStandComp,cComponentName);

  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_base_sink_Private = openmaxStandComp->pComponentPrivate;

  omx_base_sink_Private->sPortTypesParam.nPorts = 1;
  omx_base_sink_Private->sPortTypesParam.nStartPortNumber = 0;	

  omx_base_sink_Private->BufferMgmtFunction = omx_base_sink_BufferMgmtFunction;

  return err;
}

OMX_ERRORTYPE omx_base_sink_Destructor(OMX_COMPONENTTYPE *openmaxStandComp)
{
  return omx_base_component_Destructor(openmaxStandComp);
}

/** This is the central function for component processing. It
  * is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_base_sink_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private=(omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_sink_PrivateType* omx_base_sink_Private = (omx_base_sink_PrivateType*)omx_base_component_Private;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_COMPONENTTYPE* target_component;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE;
  static int inBufExchanged=0;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);
  while(omx_base_component_Private->state == OMX_StateIdle || omx_base_component_Private->state == OMX_StateExecuting ||  omx_base_component_Private->state == OMX_StatePause || 
    omx_base_component_Private->transientState == OMX_StateIdle){

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_sink_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort)) {
      pthread_mutex_unlock(&omx_base_sink_Private->flush_mutex);

      if(isInputBufferNeeded==OMX_FALSE) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
        isInputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning input buffer\n");
      }
      DEBUG(DEB_LEV_FULL_SEQ, "In %s signalling flush all condition \n", __func__);
      
      pthread_mutex_lock(&omx_base_sink_Private->flush_mutex);
      pthread_cond_signal(&omx_base_sink_Private->flush_all_condition);
      pthread_cond_wait(&omx_base_sink_Private->flush_condition,&omx_base_sink_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_sink_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if(pInputSem->semval==0 && 
      (omx_base_sink_Private->state != OMX_StateLoaded && omx_base_sink_Private->state != OMX_StateInvalid)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer \n");
      tsem_wait(omx_base_sink_Private->bMgmtSem);
    }

    if(omx_base_sink_Private->state == OMX_StateLoaded || omx_base_sink_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
    if(pInputSem->semval>0 && isInputBufferNeeded==OMX_TRUE ) {
      tsem_down(pInputSem);
      inBufExchanged++;
      isInputBufferNeeded=OMX_FALSE;
      pInputBuffer = dequeue(pInputQueue);
      if(pInputBuffer == NULL){
        DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
        break;
      }
    }
    
    if(isInputBufferNeeded==OMX_FALSE) {
      if(pInputBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
        
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          0, /* The commands was a OMX_CommandStateSet */
          pInputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
        pInputBuffer->nFlags=0;
      }
      if(omx_base_sink_Private->pMark!=NULL){
         omx_base_sink_Private->pMark=NULL;
      }
      target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
      if(target_component==(OMX_COMPONENTTYPE *)openmaxStandComp) {
        /*Clear the mark and generate an event*/
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventMark, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          0, /* The state has been changed in message->messageParam2 */
          pInputBuffer->pMarkData);
      } else if(pInputBuffer->hMarkTargetComponent!=NULL){
        /*If this is not the target component then pass the mark*/
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
      }
      if (omx_base_sink_Private->BufferMgmtCallback && pInputBuffer->nFilledLen != 0) {
        (*(omx_base_sink_Private->BufferMgmtCallback))(openmaxStandComp, pInputBuffer);
      }
      else {
        /*It no buffer management call back the explicitly consume input buffer*/
        pInputBuffer->nFilledLen = 0;
      }
      /*Input Buffer has been completely consumed. So, get new input buffer*/
      if(pInputBuffer->nFilledLen==0)
        isInputBufferNeeded = OMX_TRUE;

      if(omx_base_sink_Private->state==OMX_StatePause && !PORT_IS_BEING_FLUSHED(pInPort)) {
        /*Waiting at paused state*/
        tsem_wait(omx_base_sink_Private->bStateSem);
      }

      /*Input Buffer has been completely consumed. So, return input buffer*/
      if(pInputBuffer->nFilledLen==0) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
      }

    }
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}
