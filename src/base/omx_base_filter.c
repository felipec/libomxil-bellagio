/**
	@file src/base/omx_base_filter.c
	
	OpenMax Base Filter component. This component does not perform any multimedia
	processing. It derives from base component and contains two ports. It can be used 
	as base class for codec and filter components.
	
	Copyright (C) 2007  STMicroelectronics and Nokia

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI, Ukri NIEMIMUUKKO

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
	
	$Date: 2007-03-16 10:01:36 +0100 (Fri, 16 Mar 2007) $
	Revision $Rev: 711 $
	Author $Author: pankaj_sen $

*/

#include <omxcore.h>

#include "omx_base_filter.h"

OMX_ERRORTYPE omx_base_filter_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_base_filter_PrivateType* omx_base_filter_Private;

  if (openmaxStandComp->pComponentPrivate) {
    omx_base_filter_Private = (omx_base_filter_PrivateType*)openmaxStandComp->pComponentPrivate;
  } else {
    omx_base_filter_Private = malloc(sizeof(omx_base_filter_PrivateType));
    if (!omx_base_filter_Private) {
      return OMX_ErrorInsufficientResources;
    }
    openmaxStandComp->pComponentPrivate=omx_base_filter_Private;
  }

  /*Call the base class constructory*/
  err = omx_base_component_Constructor(openmaxStandComp,cComponentName);

  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_base_filter_Private = openmaxStandComp->pComponentPrivate;

  omx_base_filter_Private->sPortTypesParam.nPorts = 2;
  omx_base_filter_Private->sPortTypesParam.nStartPortNumber = 0;	

  // create ports, but only if the subclass hasn't done it
  /*
  if (!omx_base_filter_Private->ports) {
  omx_base_filter_Private->ports = calloc(omx_base_filter_Private->sPortTypesParam.nPorts,sizeof (omx_base_PortType *));
  }

  if (!omx_base_filter_Private->ports) return OMX_ErrorInsufficientResources;

  base_port_Constructor(openmaxStandComp,&omx_base_filter_Private->ports[0],0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp,&omx_base_filter_Private->ports[1],1, OMX_FALSE);
  */

  omx_base_filter_Private->BufferMgmtFunction = omx_base_filter_BufferMgmtFunction;

  return err;
}

OMX_ERRORTYPE omx_base_filter_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  
  return omx_base_component_Destructor(openmaxStandComp);
}
/** This is the central function for component processing. It
  * is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_base_filter_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private=(omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_filter_PrivateType* omx_base_filter_Private = (omx_base_filter_PrivateType*)omx_base_component_Private;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  tsem_t* pOutputSem = pOutPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  queue_t* pOutputQueue = pOutPort->pBufferQueue;
  OMX_BOOL exit_condition = OMX_FALSE;
  OMX_BOOL isInputBufferEnded;
  OMX_BUFFERHEADERTYPE* pOutputBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_BOOL *inbufferUnderProcess=&pInPort->bBufferUnderProcess;
  OMX_BOOL *outbufferUnderProcess=&pOutPort->bBufferUnderProcess;
  pthread_mutex_t *pInmutex=&pInPort->mutex;
  pthread_mutex_t *pOutmutex=&pOutPort->mutex;
  OMX_COMPONENTTYPE* target_component;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE,isOutputBufferNeeded=OMX_TRUE;
  int inBufExchanged=0,outBufExchanged=0;

  DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
  DEBUG(DEB_LEV_ERR, "In %s \n", __func__);
  while(omx_base_filter_Private->state == OMX_StateIdle || omx_base_filter_Private->state == OMX_StateExecuting ||  omx_base_filter_Private->state == OMX_StatePause || 
    omx_base_filter_Private->transientState == OMX_StateIdle){

    if(omx_base_filter_Private->state==OMX_StatePause ) {
      /*
      pthread_mutex_lock(executingMutex);
      pthread_cond_wait(executingCondition,executingMutex);
      pthread_mutex_unlock(executingMutex);
      */
    }

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort) || 
           PORT_IS_BEING_FLUSHED(pOutPort)) {
      pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);
      
      DEBUG(DEB_LEV_FULL_SEQ, "In %s 1 signalling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n", 
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);

      if(isOutputBufferNeeded==OMX_FALSE) {
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer=NULL;
        isOutputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_ERR, "Ports are flushing,so returning output buffer\n");
      }

      if(isInputBufferNeeded==OMX_FALSE) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
        isInputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_ERR, "Ports are flushing,so returning input buffer\n");
      }

      DEBUG(DEB_LEV_ERR, "In %s 2 signalling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n", 
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);
  
      pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
      pthread_cond_signal(&omx_base_filter_Private->flush_all_condition);
      pthread_cond_wait(&omx_base_filter_Private->flush_condition,&omx_base_filter_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if(((isInputBufferNeeded==OMX_TRUE && pInputSem->semval==0)|| (isOutputBufferNeeded==OMX_TRUE && pOutputSem->semval==0)) && 
      (omx_base_filter_Private->state != OMX_StateLoaded && omx_base_filter_Private->state != OMX_StateInvalid)) {
      //TO DO: use some condition & mutex. Which will be signalled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for input and output buffer \n");
      tsem_wait(omx_base_filter_Private->bMgmtSem);
      
    }
    if(omx_base_filter_Private->state == OMX_StateLoaded || omx_base_filter_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_ERR, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }
 
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
    if(pInputSem->semval>0 && isInputBufferNeeded==OMX_TRUE ) {
      tsem_down(pInputSem);
      inBufExchanged++;
      isInputBufferNeeded=OMX_FALSE;
      pthread_mutex_lock(pInmutex);
      *inbufferUnderProcess = OMX_TRUE;
      pthread_mutex_unlock(pInmutex);

      pInputBuffer = dequeue(pInputQueue);
      if(pInputBuffer == NULL){
        DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
        break;
      }
    }
    /*When we have input buffer to process then get one output buffer*/
    if(pOutputSem->semval>0 && isOutputBufferNeeded==OMX_TRUE) {
      tsem_down(pOutputSem);
      outBufExchanged++;
      isOutputBufferNeeded=OMX_FALSE;
      pthread_mutex_lock(pOutmutex);
      *outbufferUnderProcess = OMX_TRUE;
      pthread_mutex_unlock(pOutmutex);

      pOutputBuffer = dequeue(pOutputQueue);
      if(pOutputBuffer == NULL){
        DEBUG(DEB_LEV_ERR, "What the hell!! had NULL output buffer!! op is=%d,iq=%d\n",pOutputSem->semval,pOutputQueue->nelem);
        break;
      }
    }

    if(isInputBufferNeeded==OMX_FALSE && isOutputBufferNeeded==OMX_FALSE) {
      if(pInputBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "Detected EOS flags in input buffer filled len=%d\n",pInputBuffer->nFilledLen);
        pOutputBuffer->nFlags=pInputBuffer->nFlags;
        pInputBuffer->nFlags=0;
        (*(omx_base_filter_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_filter_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          pOutputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
      }
      if(omx_base_filter_Private->pMark!=NULL){
        pOutputBuffer->hMarkTargetComponent=omx_base_filter_Private->pMark->hMarkTargetComponent;
        pOutputBuffer->pMarkData=omx_base_filter_Private->pMark->pMarkData;
        omx_base_filter_Private->pMark=NULL;
      }
      target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
      if(target_component==(OMX_COMPONENTTYPE *)openmaxStandComp) {
        /*Clear the mark and generate an event*/
        (*(omx_base_filter_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_filter_Private->callbackData,
          OMX_EventMark, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          0, /* The state has been changed in message->messageParam2 */
          pInputBuffer->pMarkData);
      } else if(pInputBuffer->hMarkTargetComponent!=NULL){
        /*If this is not the target component then pass the mark*/
        pOutputBuffer->hMarkTargetComponent	= pInputBuffer->hMarkTargetComponent;
        pOutputBuffer->pMarkData = pInputBuffer->pMarkData;
        pInputBuffer->pMarkData=NULL;
      }

      if (omx_base_filter_Private->BufferMgmtCallback && pInputBuffer->nFilledLen != 0) {
        (*(omx_base_filter_Private->BufferMgmtCallback))(openmaxStandComp, pInputBuffer, pOutputBuffer);
      }
      else {
        /*It no buffer management call back the explicitly consume input buffer*/
        pInputBuffer->nFilledLen = 0;
      }
      /*Input Buffer has been completely consumed. So, get new input buffer*/
      if(pInputBuffer->nFilledLen==0)
        isInputBufferNeeded = OMX_TRUE;

      /*If EOS and Input buffer Filled Len Zero then Return output buffer immediately*/
      if(pOutputBuffer->nFilledLen!=0 || (pOutputBuffer->nFlags==OMX_BUFFERFLAG_EOS)){
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer=NULL;
        isOutputBufferNeeded=OMX_TRUE;
      }
    }

    DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");

    /*Input Buffer has been completely consumed. So, return input buffer*/
    if(isInputBufferNeeded == OMX_TRUE && pInputBuffer!=NULL) {
      pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
      inBufExchanged--;
      pInputBuffer=NULL;
    }

  }
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}

/*Deprecated function. Will not be used in future version*/

/** This is the central function for component processing. It
  * is executed in a separate thread, is synchronized with 
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_base_filter_BufferMgmtFunction_Old (void* param) {
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private=(omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_filter_PrivateType* omx_base_filter_Private = (omx_base_filter_PrivateType*)omx_base_component_Private;
  tsem_t* pInputSem = omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferSem;
  tsem_t* pOutputSem = omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferSem;
  queue_t* pInputQueue = omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferQueue;
  queue_t* pOutputQueue = omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferQueue;
  OMX_BOOL exit_condition = OMX_FALSE;
  OMX_BOOL isInputBufferEnded;
  OMX_BUFFERHEADERTYPE* pOutputBuffer;
  OMX_BUFFERHEADERTYPE* pInputBuffer;
  OMX_U32  nFlags;
  OMX_BOOL *inbufferUnderProcess=&omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->bBufferUnderProcess;
  OMX_BOOL *outbufferUnderProcess=&omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->bBufferUnderProcess;
  pthread_mutex_t *pInmutex=&omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->mutex;
  pthread_mutex_t *pOutmutex=&omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->mutex;
  OMX_COMPONENTTYPE* target_component;
  //pthread_mutex_t* executingMutex = &omx_base_filter_Private->executingMutex;
  //pthread_cond_t* executingCondition = &omx_base_filter_Private->executingCondition;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

  pthread_mutex_lock(pOutmutex);
  omx_base_filter_Private->pPendingOutputBuffer=NULL;
  pthread_mutex_unlock(pOutmutex);
  DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
  while(omx_base_component_Private->state == OMX_StateIdle || omx_base_component_Private->state == OMX_StateExecuting ||  omx_base_component_Private->state == OMX_StatePause || 
    omx_base_component_Private->transientState == OMX_StateIdle){

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort) || 
      PORT_IS_BEING_FLUSHED(pOutPort)) {
      /*Signal waiting flush condition to flush all other buffers and wait till flush complete*/
      pthread_cond_signal(&omx_base_filter_Private->flush_all_condition);
      pthread_cond_wait(&omx_base_filter_Private->flush_condition,&omx_base_filter_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);

    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
    tsem_down(pInputSem);
    DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
    /*If Component is De-initializing, exit buffer management thread*/
    /*
    if(IS_COMPONENT_DEINIT(omx_base_filter_Private, exit_condition)) {
      DEBUG(DEB_LEV_FULL_SEQ, "Deinit condition detected exiting bmgmt thread bf=%d,tb=%d,is=%d,iq=%d\n",
      pInPort->nNumBufferFlushed,pInPort->nNumTunnelBuffer,
      pInputSem->semval,pInputQueue->nelem);
      if(pInputSem->semval==pInputQueue->nelem)
        break;
    }
    */

    pthread_mutex_lock(pInmutex);
    *inbufferUnderProcess = OMX_TRUE;
    pthread_mutex_unlock(pInmutex);

    pInputBuffer = dequeue(pInputQueue);
    if(pInputBuffer == NULL){
      DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
      break;
    }
		  
    nFlags=pInputBuffer->nFlags;
    if(nFlags==OMX_BUFFERFLAG_EOS) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
      pInputBuffer->nFlags=0;
    }
    if( PORT_IS_BEING_FLUSHED(pInPort)) {
      /*Return Input Buffer*/
      pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
      continue;
    }

    isInputBufferEnded = OMX_FALSE;
		  
    while(!isInputBufferEnded && 
      (!PORT_IS_BEING_FLUSHED(pInPort)) &&
      (!PORT_IS_BEING_FLUSHED(pOutPort))) {
      /*If we don't have pending output buffer then Get New Output buffer*/
      pthread_mutex_lock(pOutmutex);
      if(omx_base_filter_Private->pPendingOutputBuffer==NULL) {
        pthread_mutex_unlock(pOutmutex);
        DEBUG(DEB_LEV_FULL_SEQ, "Waiting for output buffer\n");
        tsem_down(pOutputSem);
        DEBUG(DEB_LEV_FULL_SEQ, "Output buffer arrived\n");
        pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
        DEBUG(DEB_LEV_FULL_SEQ, "Before flush check o/p sem inc 2.0 op is=%d,iq=%d\n",
                 pOutputSem->semval,pOutputQueue->nelem);
        if(PORT_IS_BEING_FLUSHED(pInPort)) {
          DEBUG(DEB_LEV_FULL_SEQ, "Dummy o/p sem inc 2.0.1 exiting while op is=%d,iq=%d\n",
                   pOutputSem->semval,pOutputQueue->nelem);
          pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);
            continue;
        }
        pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);

        /*If Component is De-initializing, exit buffer management thread*/
        /*
        if(IS_COMPONENT_DEINIT(omx_base_filter_Private, exit_condition)) {
          DEBUG(DEB_LEV_FULL_SEQ, "Deinit condition detected exiting bmgmt thread bf=%d,tb=%d\n",pInPort->nNumBufferFlushed,pInPort->nNumTunnelBuffer);
          break;
        }*/

        pthread_mutex_lock(pOutmutex);
        *outbufferUnderProcess = OMX_TRUE;
        pthread_mutex_unlock(pOutmutex);
        /* Get a new buffer from the output queue */
        pOutputBuffer = dequeue(pOutputQueue);
        if(pOutputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "What the hell!! had NULL output buffer!! op is=%d,iq=%d\n",pOutputSem->semval,pOutputQueue->nelem);
          break;
        }
      }
      else { // If there exist any pending buffer then use that output buffer
        pOutputBuffer = omx_base_filter_Private->pPendingOutputBuffer;
        omx_base_filter_Private->pPendingOutputBuffer=NULL;
        pthread_mutex_unlock(pOutmutex);
      }

      if(omx_base_filter_Private->pMark!=NULL){
        pOutputBuffer->hMarkTargetComponent=omx_base_filter_Private->pMark->hMarkTargetComponent;
        pOutputBuffer->pMarkData=omx_base_filter_Private->pMark->pMarkData;
        omx_base_filter_Private->pMark=NULL;
      }
      target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
      if(target_component==(OMX_COMPONENTTYPE *)&omx_base_component_Private->openmaxStandComp) {
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
        pOutputBuffer->hMarkTargetComponent	= pInputBuffer->hMarkTargetComponent;
        pOutputBuffer->pMarkData = pInputBuffer->pMarkData;
      }
      if(nFlags==OMX_BUFFERFLAG_EOS) {
        pOutputBuffer->nFlags=nFlags;
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
      }

      DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);

      /* This calls the actual algorithm; fp must be set */			
      if (omx_base_filter_Private->BufferMgmtCallback) {
        (*(omx_base_filter_Private->BufferMgmtCallback))(omx_base_filter_Private, pInputBuffer, pOutputBuffer);
      }
      else {
        /*It no buffer management call back the explicitly consume input buffer*/
        pInputBuffer->nFilledLen = 0;
      }
      /*It is assumed that on return from buffer management callback input buffer should be consumed fully*/
      if(pInputBuffer->nFilledLen==0)
        isInputBufferEnded = OMX_TRUE;

      /*Wait if state is pause*/
      /*
      if(omx_base_component_Private->state==OMX_StatePause && ((! PORT_IS_BEING_FLUSHED(pInPort)) && (! PORT_IS_BEING_FLUSHED(pOutPort)))) {
        if(!PORT_IS_WAITING_FLUSH_SEMAPHORE(pOutPort)) {
          pthread_mutex_lock(executingMutex);
          pthread_cond_wait(executingCondition,executingMutex);
          pthread_mutex_unlock(executingMutex);
        }
      }*/

      if(pOutputBuffer->nFilledLen!=0 || (PORT_IS_BEING_FLUSHED(pOutPort)) || (nFlags==OMX_BUFFERFLAG_EOS)) {
        /*Return Output Buffer*/
        //base_component_returnOutputBuffer(omx_base_component_Private,pOutputBuffer,pOutPort);
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
      }
      else {
        /*When output buffer size Zero then store Output buffer pointer to be filled later*/
        pthread_mutex_lock(pOutmutex);
        omx_base_filter_Private->pPendingOutputBuffer=pOutputBuffer; 
        pthread_mutex_unlock(pOutmutex);
      }
    }
    /*Wait if state is pause*/
    /*
    if(omx_base_component_Private->state==OMX_StatePause &&  ((!PORT_IS_BEING_FLUSHED(pInPort)) && (!PORT_IS_BEING_FLUSHED(pOutPort)))) {
      if(!PORT_IS_WAITING_FLUSH_SEMAPHORE(pInPort) && !PORT_IS_WAITING_FLUSH_SEMAPHORE(pOutPort)) {
        pthread_mutex_lock(executingMutex);
        pthread_cond_wait(executingCondition,executingMutex);
        pthread_mutex_unlock(executingMutex);
      }
    }*/
    /*Return Input Buffer*/
    //base_component_returnInputBuffer(omx_base_component_Private,pInputBuffer,pInPort);
    pInPort->ReturnBufferFunction(pInPort,pInputBuffer);

    /*If Component is De-initializing, exit buffer management thread*/
    /*
    if(IS_COMPONENT_DEINIT(omx_base_filter_Private, exit_condition)) {
      DEBUG(DEB_LEV_FULL_SEQ, "Deinit condition detected exiting bmgmt thread bf=%d,tb=%d\n",pInPort->nNumBufferFlushed,pInPort->nNumTunnelBuffer);
      break;
    }*/
	}
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}

/*Deprecated function. Will not be used in future version*/

/** Flushes all the buffers under processing by the given port. 
  * This function si called due to a state change of the component, typically
  * @param omx_base_component_Private the component which owns the port to be flushed
  * @param portIndex the ID of the port to be flushed
  */
OMX_ERRORTYPE omx_base_filter_FlushProcessingBuffers(OMX_COMPONENTTYPE *openmaxStandComp,OMX_U32 portIndex)
{
  OMX_COMPONENTTYPE* pHandle=openmaxStandComp;
  omx_base_filter_PrivateType* omx_base_filter_Private = openmaxStandComp->pComponentPrivate;
  tsem_t* pInputSem = omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferSem;
  tsem_t* pOutputSem = omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferSem;
  queue_t* pInputQueue = omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferQueue;
  queue_t* pOutputQueue = omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pOutputBuffer;
  OMX_BUFFERHEADERTYPE* pInputBuffer;
  OMX_BOOL *inbufferUnderProcess=&omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->bBufferUnderProcess;
  OMX_BOOL *outbufferUnderProcess=&omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->bBufferUnderProcess;
  pthread_mutex_t *pInmutex=&omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->mutex;
  pthread_mutex_t *pOutmutex=&omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->mutex;
//  pthread_cond_t* executingCondition = &omx_base_filter_Private->executingCondition;
//  pthread_mutex_t* executingMutex = &omx_base_filter_Private->executingMutex;
  OMX_BOOL dummyInc=OMX_FALSE;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

//  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld,In Port tunnelled=%d,supplier=%d,On Port tunnelled=%d,supplier=%d\n", 
  //    __func__,portIndex,PORT_IS_TUNNELED(pInPort),PORT_IS_BUFFER_SUPPLIER(pInPort),
  //    PORT_IS_TUNNELED(pOutPort),PORT_IS_BUFFER_SUPPLIER(pOutPort));

  if (portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX || portIndex == OMX_BASE_FILTER_ALLPORT_INDEX) {
    if ((!PORT_IS_TUNNELED(pInPort)) || (PORT_IS_TUNNELED(pInPort) && (! PORT_IS_BUFFER_SUPPLIER(pInPort)))) { // Port is not tunnelled 
      DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d outsemval=%d\n",
      pInputSem->semval,pOutputSem->semval);
      pthread_mutex_lock(pOutmutex);
      /*This dummy increment is to handle the situation  that input buffer is provided 
      but no output buffer and that time port flush command received*/
      if(pOutputSem->semval==0 && *inbufferUnderProcess==OMX_TRUE && *outbufferUnderProcess==OMX_FALSE) {
        /*This is to free the input buffer being processed and where no output buffer is available*/
        DEBUG(DEB_LEV_FULL_SEQ, "In %s Incrementing dummy o/p sem val=%d,q=%d\n",__func__,pOutputSem->semval,pOutputQueue->nelem);
        tsem_up(pOutputSem);
        dummyInc=OMX_TRUE;
      }
      pthread_mutex_unlock(pOutmutex);

      /*First return the Buffer presently being processed,if any*/
      pthread_mutex_lock(pInmutex);
      if(IS_BUFFER_UNDER_PROCESS(pInPort)) {
        /*Indicate that pAllocNFlushSem is waiting for buffer for flushing*/
        pInPort->bWaitingFlushSem=OMX_TRUE;
        pthread_mutex_unlock(pInmutex);
        /*Dummy signaling to buffer management thread,if waiting in the paused condition and
        now port flush request received*/
        /*
        if(omx_base_filter_Private->state==OMX_StatePause) {
          pthread_mutex_lock(executingMutex);
          pthread_cond_signal(executingCondition);
          pthread_mutex_unlock(executingMutex);
        }*/

        /*Buffering being processed waiting for input flush sem*/
        tsem_down(pInPort->pAllocSem);
        pthread_mutex_lock(pInmutex);
        /*Indicate that pAllocNFlushSem waiting is over*/
        pInPort->bWaitingFlushSem=OMX_FALSE;
      }
      pthread_mutex_unlock(pInmutex);
      /*Return All other input buffers*/
      while(pInputSem->semval>0) {
        tsem_down(pInputSem);
        pInputBuffer = dequeue(pInputQueue);
        if (!PORT_IS_TUNNELED(pInPort)) 
          (*(omx_base_filter_Private->callbacks->EmptyBufferDone))
            (pHandle, omx_base_filter_Private->callbackData, pInputBuffer);
        else
          OMX_FillThisBuffer(pInPort->hTunneledComponent, pInputBuffer);
      }
      if(pOutputSem->semval>0 && dummyInc==OMX_TRUE) {
      /*This is to free the input buffer being processed, 
      if dummy increment did not decremented then decrement it now*/
      tsem_down(pOutputSem);
      }
    }
    else if (PORT_IS_TUNNELED(pInPort) && 
      PORT_IS_BUFFER_SUPPLIER(pInPort)) {
      /*Tunnel is supplier wait till all the buffers are returned*/
      pthread_mutex_lock(pInmutex);
      /*Flush all input buffers*/
      while(pInputSem->semval>0) {
        tsem_down(pInputSem);
        pInPort->nNumBufferFlushed++;
      }
      /*Wait till the buffer sent to the tunneled component returns*/
      if(pInPort->nNumBufferFlushed<pInPort->nNumTunnelBuffer) {
        /*Indicate that pAllocNFlushSem is waiting for buffer for flushing*/
        pInPort->bWaitingFlushSem=OMX_TRUE;
        pthread_mutex_unlock(pInmutex);
        /*Dummy signaling to buffer management thread,if waiting in the paused condition and
        now port flush request received*/
        /*
        if(omx_base_filter_Private->state==OMX_StatePause) {
          pthread_mutex_lock(executingMutex);
          pthread_cond_signal(executingCondition);
          pthread_mutex_unlock(executingMutex);
        }*/
        /*Buffering being processed waiting for input flush sem*/
        tsem_down(pInPort->pAllocSem);
        pthread_mutex_lock(pInmutex);
        /*Indicate that pAllocNFlushSem waiting is over*/
        pInPort->bWaitingFlushSem=OMX_FALSE;
      }
      pthread_mutex_unlock(pInmutex);
    }
  } 

  if (portIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX || portIndex == OMX_BASE_FILTER_ALLPORT_INDEX) {
    if ((!PORT_IS_TUNNELED(pOutPort)) || (PORT_IS_TUNNELED(pOutPort) && (! PORT_IS_BUFFER_SUPPLIER(pOutPort)))) {
      DEBUG(DEB_LEV_PARAMS,"Flashing output ports outsemval=%d \n",pOutputSem->semval);
      pthread_mutex_lock(pOutmutex);
      if(omx_base_filter_Private->pPendingOutputBuffer!=NULL) {
        pthread_mutex_unlock(pOutmutex);
        /*Return Output Buffer*/
        (*(omx_base_filter_Private->callbacks->FillBufferDone))
          (pHandle, omx_base_filter_Private->callbackData, omx_base_filter_Private->pPendingOutputBuffer);
        pthread_mutex_lock(pOutmutex);
        omx_base_filter_Private->pPendingOutputBuffer=NULL;
      }
      pthread_mutex_unlock(pOutmutex);
      /*Return All output buffers*/
      while(pOutputSem->semval>0) {
        tsem_down(pOutputSem);
        pOutputBuffer = dequeue(pOutputQueue);
        if (!PORT_IS_TUNNELED(pInPort)) 
          (*(omx_base_filter_Private->callbacks->FillBufferDone))
            (pHandle, omx_base_filter_Private->callbackData, pOutputBuffer);
        else 
          OMX_EmptyThisBuffer(pOutPort->hTunneledComponent, pOutputBuffer);
    }
  }
  else if (PORT_IS_TUNNELED(pOutPort) && 
    PORT_IS_BUFFER_SUPPLIER(pOutPort)) {
    /*Flush all output buffers*/
    pthread_mutex_lock(pOutmutex);
    if(omx_base_filter_Private->pPendingOutputBuffer!=NULL) {
      /*Return Output Buffer*/
      omx_base_filter_Private->pPendingOutputBuffer=NULL;
      pOutPort->nNumBufferFlushed++;
    }
    while(pOutputSem->semval>0) {
      tsem_down(pOutputSem);
      pOutPort->nNumBufferFlushed++;
    }
    /*Tunnel is supplier wait till all the buffers are returned*/
    if(pOutPort->nNumBufferFlushed<pOutPort->nNumTunnelBuffer) {
      /*Indicate that pAllocNFlushSem is waiting for buffer for flushing*/
      pOutPort->bWaitingFlushSem=OMX_TRUE;
      pthread_mutex_unlock(pOutmutex);
      /*Dummy signaling to buffer management thread,if waiting in the paused condition and
      now port flush request received*/
      /*
      if(omx_base_filter_Private->state==OMX_StatePause) {
        pthread_mutex_lock(executingMutex);
        pthread_cond_signal(executingCondition);
        pthread_mutex_unlock(executingMutex);
      }*/
      /*Buffer being processed waitoutg for output flush sem*/
      tsem_down(pOutPort->pAllocSem);
      pthread_mutex_lock(pOutmutex);
      /*Indicate that pAllocNFlushSem waiting is over*/
      pOutPort->bWaitingFlushSem=OMX_FALSE;
      }
      pthread_mutex_unlock(pOutmutex);
    }
  } 
	 
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
