/**
  * @file src/base/omx_base_sink.c
  * 
  * OpenMax base sink component. This component does not perform any multimedia
  * processing. It derives from base component and contains a single port. It can be used 
  * as base class for sink and source components.
  * 
  * Copyright (C) 2006  Nokia and STMicroelectronics
  * @author Ukri NIEMIMUUKKO, Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI
  * 
  * This library is free software; you can redistribute it and/or modify it under
  * the terms of the GNU Lesser General Public License as published by the Free
  * Software Foundation; either version 2.1 of the License, or (at your option)
  * any later version.
  *
  * This library is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  * details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St, Fifth Floor, Boston, MA
  * 02110-1301  USA
  *
  * 2006/05/11:  base sink component version 0.2
  *
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

  // oh well, for the time being, set the port params, now that the ports exist	
  /*
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.eDir = OMX_DirInput;
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
  omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;
  */

  omx_base_sink_Private->sPortTypesParam.nPorts = 1;
  omx_base_sink_Private->sPortTypesParam.nStartPortNumber = 0;	

  omx_base_sink_Private->BufferMgmtFunction = omx_base_sink_BufferMgmtFunction;
  //omx_base_sink_Private->FlushPort = &omx_base_sink_FlushPort;

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
  OMX_BOOL exit_condition = OMX_FALSE;
  OMX_BOOL isInputBufferEnded;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_BOOL *inbufferUnderProcess=&pInPort->bBufferUnderProcess;
  pthread_mutex_t *pInmutex=&pInPort->mutex;
  OMX_COMPONENTTYPE* target_component;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE;
  static int inBufExchanged=0;

  DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
  DEBUG(DEB_LEV_ERR, "In %s \n", __func__);
  while(omx_base_component_Private->state == OMX_StateIdle || omx_base_component_Private->state == OMX_StateExecuting ||  omx_base_component_Private->state == OMX_StatePause || 
    omx_base_component_Private->transientState == OMX_StateIdle){

    if(omx_base_component_Private->state==OMX_StatePause ) {
      /*
      pthread_mutex_lock(executingMutex);
      pthread_cond_wait(executingCondition,executingMutex);
      pthread_mutex_unlock(executingMutex);
      */
    }

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_sink_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort)) {
      DEBUG(DEB_LEV_ERR, "In %s signalling flush all condition \n", __func__);
      pthread_cond_signal(&omx_base_sink_Private->flush_all_condition);
      pthread_cond_wait(&omx_base_sink_Private->flush_condition,&omx_base_sink_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_sink_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if(pInputSem->semval==0) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer \n");
      tsem_wait(omx_base_sink_Private->bMgmtSem);
    }

    if(omx_base_sink_Private->state == OMX_StateLoaded) {
      DEBUG(DEB_LEV_ERR, "In %s Buffer Management Thread is exiting\n");
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
    
    if(isInputBufferNeeded==OMX_FALSE) {
      if(pInputBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
        
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          pInputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
        pInputBuffer->nFlags=0;
      }
      if(omx_base_sink_Private->pMark!=NULL){
         omx_base_sink_Private->pMark=NULL;
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
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
      }
      if (omx_base_sink_Private->BufferMgmtCallback) {
        (*(omx_base_sink_Private->BufferMgmtCallback))(openmaxStandComp, pInputBuffer);
      }
      else {
        /*It no buffer management call back the explicitly consume input buffer*/
        pInputBuffer->nFilledLen = 0;
      }
      /*Input Buffer has been completely consumed. So, get new input buffer*/
      if(pInputBuffer->nFilledLen==0)
        isInputBufferNeeded = OMX_TRUE;

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

/*Deprecated function. Will not be used in future version*/

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* omx_base_sink_BufferMgmtFunction_old(void* param) {
	OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
	omx_base_sink_PrivateType* omx_base_sink_Private = openmaxStandComp->pComponentPrivate;
	tsem_t* pInputSem = omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_condition = OMX_FALSE;
	OMX_BOOL isInputBufferEnded;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *inbufferUnderProcess=&omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	//pthread_mutex_t* executingMutex = &omx_base_sink_Private->executingMutex;
	//pthread_cond_t* executingCondition = &omx_base_sink_Private->executingCondition;
	omx_base_PortType *pPort=(omx_base_PortType *)omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(omx_base_sink_Private->state == OMX_StateIdle || omx_base_sink_Private->state == OMX_StateExecuting || omx_base_sink_Private->state == OMX_StatePause || 
		(omx_base_sink_Private->transientState == OMX_StateIdle)){

		/*Wait till the ports are being flushed*/
		pthread_mutex_lock(&omx_base_sink_Private->flush_mutex);
		while( PORT_IS_BEING_FLUSHED(pPort))
			pthread_cond_wait(&omx_base_sink_Private->flush_condition,&omx_base_sink_Private->flush_mutex);
		pthread_mutex_unlock(&omx_base_sink_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		//If Component is De-initializing, exit buffer management thread//
		//if(IS_COMPONENT_DEINIT(omx_base_sink_Private, exit_condition))
		//	break;

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
		}
		if(pPort->bIsPortFlushed==OMX_TRUE) {
			/*Return Input Buffer*/
			//base_component_returnInputBuffer(omx_base_sink_Private,pInputBuffer,pPort);
			continue;
		}
		/*If Component is De-initializing, exit buffer management thread*/
		//if(IS_COMPONENT_DEINIT(omx_base_sink_Private, exit_condition))
		//	break;
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded) {
		/**  This condition becomes true when the input buffer has completely be consumed.
			*  In this case is immediately switched because there is no real buffer consumption */
			isInputBufferEnded = OMX_TRUE;
									
			if(omx_base_sink_Private->pMark!=NULL){
				omx_base_sink_Private->pMark=NULL;
			}
			target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
			if(target_component==(OMX_COMPONENTTYPE *)&omx_base_sink_Private->openmaxStandComp) {
				/*Clear the mark and generate an event*/
				(*(omx_base_sink_Private->callbacks->EventHandler))
					(openmaxStandComp,
						omx_base_sink_Private->callbackData,
						OMX_EventMark, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						pInputBuffer->pMarkData);
			} else if(pInputBuffer->hMarkTargetComponent!=NULL){
				/*If this is not the target component then pass the mark*/
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
			}

			/*Need to be verified pankaj*/
			if(nFlags==OMX_BUFFERFLAG_EOS) {
				(*(omx_base_sink_Private->callbacks->EventHandler))
					(openmaxStandComp,
						omx_base_sink_Private->callbackData,
						OMX_EventBufferFlag, /* The command was completed */
						0, /* The commands was a OMX_CommandStateSet */
						nFlags, /* The state has been changed in message->messageParam2 */
						NULL);
			}
			
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers \n", __func__);

			/* This calls the actual algorithm; fp must be set */			
			if (omx_base_sink_Private->BufferMgmtCallback) {
				(*(omx_base_sink_Private->BufferMgmtCallback))(openmaxStandComp, pInputBuffer);
			}
		}
		/*Wait if state is pause*/
    /*
		if(omx_base_sink_Private->state==OMX_StatePause) {
			if(pPort->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}
    */

		/*Return Input Buffer*/
		//base_component_returnInputBuffer(omx_base_sink_Private,pInputBuffer,pPort);

		/*If Component is De-initializing, exit buffer management thread*/
		//if(IS_COMPONENT_DEINIT(omx_base_sink_Private, exit_condition))
		//	break;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
	return NULL;
}

/*Deprecated function. Will not be used in future version*/

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param omx_base_sink_Private the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE omx_base_sink_FlushPort(OMX_COMPONENTTYPE *openmaxStandComp,OMX_U32 portIndex)
{
	omx_base_sink_PrivateType* omx_base_sink_Private = openmaxStandComp->pComponentPrivate;
	tsem_t* pInputSem = omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	pthread_mutex_t *pInmutex=&omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->mutex;
//	pthread_cond_t* executingCondition = &omx_base_sink_Private->executingCondition;
	omx_base_PortType *pPort=(omx_base_PortType *)omx_base_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_BASE_SINK_INPUTPORT_INDEX || portIndex == OMX_BASE_SINK_ALLPORT_INDEX) {
		if (!PORT_IS_TUNNELED(pPort)) { // Port is not tunnelled 
			//DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d ib=%ld,ibcb=%ld\n",
			//	pInputSem->semval,omx_base_sink_Private->inbuffer,omx_base_sink_Private->inbuffercb);
			/*First return the Buffer presently being processed,if any*/
			pthread_mutex_lock(pInmutex);
			if(IS_BUFFER_UNDER_PROCESS(pPort)) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(omx_base_sink_Private->state==OMX_StatePause) {
					//pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				//tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(omx_base_sink_Private->callbacks->EmptyBufferDone))
					(openmaxStandComp, omx_base_sink_Private->callbackData, pInputBuffer);
				//omx_base_sink_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
		}
		else if (PORT_IS_TUNNELED(pPort) && 
			(!PORT_IS_BUFFER_SUPPLIER(pPort))) { 	// Port is tunnelled but non-supplier
			/*First return the Buffer presently being processed,if any*/
			pthread_mutex_lock(pInmutex);
			if(IS_BUFFER_UNDER_PROCESS(pPort)) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(omx_base_sink_Private->state==OMX_StatePause) {
					//pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				//tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(pPort->hTunneledComponent, pInputBuffer);
				//omx_base_sink_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
		}
		else if (PORT_IS_TUNNELED(pPort) && 
			PORT_IS_BUFFER_SUPPLIER(pPort)) { // Port is tunnelled & supplier
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pInmutex);
			/*Flush all input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pPort->nNumBufferFlushed++;
			}
			/*Wait till the buffer sent to the tunneled component returns*/
			if(pPort->nNumBufferFlushed<pPort->nNumTunnelBuffer) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(omx_base_sink_Private->state==OMX_StatePause) {
					//pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				//tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			pthread_mutex_unlock(pInmutex);
		}
	} 
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
