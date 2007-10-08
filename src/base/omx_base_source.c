/**
  @file src/base/omx_base_source.c

  OpenMax base source component. This component does not perform any multimedia
  processing. It derives from base component and contains a single input port. 
  It can be used as base class for source components.

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
#include <omx_base_source.h>

OMX_ERRORTYPE omx_base_source_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_base_source_PrivateType* omx_base_source_Private;

  if (openmaxStandComp->pComponentPrivate) {
    omx_base_source_Private = (omx_base_source_PrivateType*)openmaxStandComp->pComponentPrivate;
  } else {
    omx_base_source_Private = malloc(sizeof(omx_base_source_PrivateType));
    if (!omx_base_source_Private) {
      return OMX_ErrorInsufficientResources;
    }
  }

  // we could create our own port structures here
  // fixme maybe the base class could use a "port factory" function pointer?	
  err = omx_base_component_Constructor(openmaxStandComp, cComponentName);

  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_base_source_Private = openmaxStandComp->pComponentPrivate;
  omx_base_source_Private->sPortTypesParam.nPorts = 1;
  omx_base_source_Private->sPortTypesParam.nStartPortNumber = 0;	
  omx_base_source_Private->BufferMgmtFunction = omx_base_source_BufferMgmtFunction;

  return err;
}

OMX_ERRORTYPE omx_base_source_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_base_source_PrivateType* omx_base_source_Private  = (omx_base_source_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  /* frees port/s */
  if (omx_base_source_Private->sPortTypesParam.nPorts && omx_base_source_Private->ports) {
    for (i=0; i < omx_base_source_Private->sPortTypesParam.nPorts; i++) {
      if(omx_base_source_Private->ports[i])
        omx_base_source_Private->ports[i]->PortDestructor(omx_base_source_Private->ports[i]);
    }
    free(omx_base_source_Private->ports);
    omx_base_source_Private->ports=NULL;
  }

  return omx_base_component_Destructor(openmaxStandComp);
}

/** This is the central function for component processing. It
  * is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_base_source_BufferMgmtFunction (void* param) {

  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_source_PrivateType* omx_base_source_Private = (omx_base_source_PrivateType*)omx_base_component_Private;
  omx_base_PortType *pOutPort = (omx_base_PortType *)omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
  tsem_t* pOutputSem = pOutPort->pBufferSem;
  queue_t* pOutputQueue = pOutPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pOutputBuffer = NULL;
  OMX_COMPONENTTYPE* target_component;
  OMX_BOOL isOutputBufferNeeded = OMX_TRUE;
  int outBufExchanged = 0;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);
  while(omx_base_component_Private->state == OMX_StateIdle || omx_base_component_Private->state == OMX_StateExecuting ||  
    omx_base_component_Private->state == OMX_StatePause || omx_base_component_Private->transientState == OMX_TransStateLoadedToIdle){

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_source_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pOutPort)) {
      pthread_mutex_unlock(&omx_base_source_Private->flush_mutex);

      if(isOutputBufferNeeded == OMX_FALSE) {
        pOutPort->ReturnBufferFunction(pOutPort, pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer = NULL;
        isOutputBufferNeeded = OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning output buffer\n");
      }
      DEBUG(DEB_LEV_FULL_SEQ, "In %s signalling flush all condition \n", __func__);
      
      pthread_mutex_lock(&omx_base_source_Private->flush_mutex);
      pthread_cond_signal(&omx_base_source_Private->flush_all_condition);
      pthread_cond_wait(&omx_base_source_Private->flush_condition, &omx_base_source_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_source_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if((isOutputBufferNeeded==OMX_TRUE && pOutputSem->semval==0) && 
      (omx_base_source_Private->state != OMX_StateLoaded && omx_base_source_Private->state != OMX_StateInvalid)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for output buffer \n");
      tsem_down(omx_base_source_Private->bMgmtSem);
    }

    if(omx_base_source_Private->state == OMX_StateLoaded || omx_base_source_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for output buffer semval=%d \n",pOutputSem->semval);
    if(pOutputSem->semval > 0 && isOutputBufferNeeded == OMX_TRUE ) {
      tsem_down(pOutputSem);
      if(pOutputQueue->nelem>0){
        outBufExchanged++;
        isOutputBufferNeeded = OMX_FALSE;
        pOutputBuffer = dequeue(pOutputQueue);
        if(pOutputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "In %s Had NULL output buffer!!\n",__func__);
          break;
        }
      }
    }
    
    if(isOutputBufferNeeded == OMX_FALSE) {
      if(pOutputBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in output buffer\n");
        
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          0, /* The commands was a OMX_CommandStateSet */
          pOutputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
        pOutputBuffer->nFlags = 0;
      }
      if(omx_base_source_Private->pMark!=NULL){
         omx_base_source_Private->pMark=NULL;
      }
      target_component = (OMX_COMPONENTTYPE*)pOutputBuffer->hMarkTargetComponent;
      if(target_component == (OMX_COMPONENTTYPE *)openmaxStandComp) {
        /*Clear the mark and generate an event*/
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventMark, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          0, /* The state has been changed in message->messageParam2 */
          pOutputBuffer->pMarkData);
      } else if(pOutputBuffer->hMarkTargetComponent != NULL) {
        /*If this is not the target component then pass the mark*/
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Source!!\n");
      }

      if (omx_base_source_Private->BufferMgmtCallback && pOutputBuffer->nFilledLen == 0) {
        (*(omx_base_source_Private->BufferMgmtCallback))(openmaxStandComp, pOutputBuffer);
      } else {
        /*It no buffer management call back the explicitly consume output buffer*/
        pOutputBuffer->nFilledLen = 0;
      }
      if(omx_base_source_Private->state == OMX_StatePause && !PORT_IS_BEING_FLUSHED(pOutPort)) {
        /*Waiting at paused state*/
        tsem_wait(omx_base_source_Private->bStateSem);
      }

      /*Output Buffer has been completely consumed. So, return output buffer and get new buffer*/
      if(pOutputBuffer->nFilledLen != 0 || pOutputBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        pOutPort->ReturnBufferFunction(pOutPort, pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer = NULL;
        isOutputBufferNeeded = OMX_TRUE;
      }

    }
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting Buffer Management Thread\n");
  return NULL;
}

