/**
 * @file src/components/newch/base_filter_component.c
 * 
 * OpenMax Base Filter component. This component does not perform any multimedia
 * processing. It derives from base component and contains two ports. It can be used 
 * as base class for codec and filter components.
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
 * 2006/05/11:  Base Filter component version 0.2
 *
 */


#include <omxcore.h>

#include <base_filter_component.h>

OMX_ERRORTYPE base_filter_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	base_filter_component_PrivateType* base_filter_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(base_filter_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	/*Call the base class constructory*/
	err = base_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	base_filter_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDir = OMX_DirInput;
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;

	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDir = OMX_DirOutput;
	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;
	
	base_filter_component_Private->sPortTypesParam.nPorts = 2;
	base_filter_component_Private->sPortTypesParam.nStartPortNumber = 0;	
	
	base_filter_component_Private->BufferMgmtFunction = base_filter_component_BufferMgmtFunction;
	base_filter_component_Private->FlushPort = &base_filter_component_FlushPort;

	return err;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* base_filter_component_BufferMgmtFunction(void* param) {
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	base_filter_component_PrivateType* base_filter_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferSem;
	tsem_t* pOutputSem = base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferQueue;
	queue_t* pOutputQueue = base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_condition = OMX_FALSE;
	OMX_BOOL isInputBufferEnded,flag;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *inbufferUnderProcess=&base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->mutex;
	pthread_mutex_t *pOutmutex=&base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &base_filter_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &base_filter_component_Private->executingCondition;
	base_component_PortType *pInPort=(base_component_PortType *)base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	base_component_PortType *pOutPort=(base_component_PortType *)base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	

	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting ||  stComponent->state == OMX_StatePause || 
		((pInPort->transientState == OMX_StateIdle) && 
		(pOutPort->transientState == OMX_StateIdle))){

		/*Wait till the ports are being flushed*/
		pthread_mutex_lock(&base_filter_component_Private->flush_mutex);
		while(! PORT_IS_BEING_FLUSHED(pInPort) || 
			  ! PORT_IS_BEING_FLUSHED(pOutPort))
			pthread_cond_wait(&base_filter_component_Private->flush_condition,&base_filter_component_Private->flush_mutex);
		pthread_mutex_unlock(&base_filter_component_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(base_filter_component_Private, exit_condition))
			break;

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
		if(! PORT_IS_BEING_FLUSHED(pInPort)) {
			/*Return Input Buffer*/
			base_component_returnInputBuffer(stComponent,pInputBuffer,pInPort);
			continue;
		}
		
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded && 
			PORT_IS_BEING_FLUSHED(pInPort) &&
			PORT_IS_BEING_FLUSHED(pOutPort)) {
			
			DEBUG(DEB_LEV_FULL_SEQ, "Waiting for output buffer\n");
			tsem_down(pOutputSem);
			DEBUG(DEB_LEV_FULL_SEQ, "Output buffer arrived\n");
			if(! PORT_IS_BEING_FLUSHED(pInPort)) {
				continue;
			}
			/*If Component is De-initializing, exit buffer management thread*/
			if(IS_COMPONENT_DEINIT(base_filter_component_Private, exit_condition)) {
				break;
			}
			pthread_mutex_lock(pOutmutex);
			*outbufferUnderProcess = OMX_TRUE;
			pthread_mutex_unlock(pOutmutex);
			/* Get a new buffer from the output queue */
			pOutputBuffer = dequeue(pOutputQueue);
			if(pOutputBuffer == NULL){
				DEBUG(DEB_LEV_ERR, "What the hell!! had NULL output buffer!!\n");
				break;
			}
						
			if(base_filter_component_Private->pMark!=NULL){
				pOutputBuffer->hMarkTargetComponent=base_filter_component_Private->pMark->hMarkTargetComponent;
				pOutputBuffer->pMarkData=base_filter_component_Private->pMark->pMarkData;
				base_filter_component_Private->pMark=NULL;
			}
			target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
			if(target_component==(OMX_COMPONENTTYPE *)&stComponent->omx_component) {
				/*Clear the mark and generate an event*/
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventMark, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						pInputBuffer->pMarkData);
			} else if(pInputBuffer->hMarkTargetComponent!=NULL){
				/*If this is not the target component then pass the mark*/
				pOutputBuffer->hMarkTargetComponent	= pInputBuffer->hMarkTargetComponent;
				pOutputBuffer->pMarkData				= pInputBuffer->pMarkData;
			}
			if(nFlags==OMX_BUFFERFLAG_EOS) {
				pOutputBuffer->nFlags=nFlags;
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventBufferFlag, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						nFlags, /* The state has been changed in message->messageParam2 */
						NULL);
			}
			
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers to fill on output port\n", __func__);

			/* This calls the actual algorithm; fp must be set */			
			if (base_filter_component_Private->BufferMgmtCallback) {
				(*(base_filter_component_Private->BufferMgmtCallback))(stComponent, pInputBuffer, pOutputBuffer);
			}
			else {
				/*It no buffer management call back the explicitly consume input buffer*/
				pInputBuffer->nFilledLen==0;
			}
			/*It is assumed that on return from buffer management callback input buffer should be consumed fully*/
			if(pInputBuffer->nFilledLen==0)
				isInputBufferEnded = OMX_TRUE;

			/*Wait if state is pause*/
			if(stComponent->state==OMX_StatePause) {
				if(pOutPort->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
				}
			}

			/*Return Output Buffer*/
			base_component_returnOutputBuffer(stComponent,pOutputBuffer,pOutPort);
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(pInPort->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}
		/*Return Input Buffer*/
		base_component_returnInputBuffer(stComponent,pInputBuffer,pInPort);
		
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(base_filter_component_Private, exit_condition))
			break;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
	return NULL;
}

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE base_filter_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	base_filter_component_PrivateType* base_filter_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferSem;
	tsem_t* pOutputSem = base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->pBufferQueue;
	queue_t* pOutputQueue = base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_BOOL *inbufferUnderProcess=&base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->mutex;
	pthread_mutex_t *pOutmutex=&base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->mutex;
	pthread_cond_t* executingCondition = &base_filter_component_Private->executingCondition;
	OMX_BOOL dummyInc=OMX_FALSE;
	base_component_PortType *pInPort=(base_component_PortType *)base_filter_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	base_component_PortType *pOutPort=(base_component_PortType *)base_filter_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX || portIndex == OMX_BASE_FILTER_ALLPORT_INDEX) {
		if (! PORT_IS_TUNNELED(pInPort)) { // Port is not tunnelled 
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d outsemval=%d ib=%ld,ibcb=%ld\n",
				pInputSem->semval,pOutputSem->semval,base_filter_component_Private->inbuffer,base_filter_component_Private->inbuffercb);
			pthread_mutex_lock(pOutmutex);
			/*This dummy increment is to handle the situation  that input buffer is provided 
			but no output buffer and that time port flush command received*/
			if(pOutputSem->semval==0 && *inbufferUnderProcess==OMX_TRUE && *outbufferUnderProcess==OMX_FALSE) {
				/*This is to free the input buffer being processed and where no output buffer is available*/
				tsem_up(pOutputSem);
				dummyInc=OMX_TRUE;
			}
			pthread_mutex_unlock(pOutmutex);
			
			/*First return the Buffer presently being processed,if any*/
			pthread_mutex_lock(pInmutex);
			if(IS_BUFFER_UNDER_PROCESS(pInPort)) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pInPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}

				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pInPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pInPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pInputBuffer);
				base_filter_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);

			if(pOutputSem->semval>0 && dummyInc==OMX_TRUE) {
				/*This is to free the input buffer being processed, 
				if dummy increment did not decremented then decrement it now*/
				tsem_down(pOutputSem);
			}
		}
		else if (PORT_IS_TUNNELED(pInPort) && 
			(! PORT_IS_BUFFER_SUPPLIER(pInPort))) { // Port is tunnelled but non-supplier
			/*This dummy increment is to handle the situation  that input buffer is provided 
			but no output buffer and that time port flush command received*/
			if(pOutputSem->semval==0 && *inbufferUnderProcess==OMX_TRUE && *outbufferUnderProcess==OMX_FALSE) {
				/*This is to free the input buffer being processed and where no output buffer is available*/
				tsem_up(pOutputSem);
				dummyInc=OMX_TRUE;
			}
			/*First return the Buffer presently being processed,if any*/
			pthread_mutex_lock(pInmutex);
			if(IS_BUFFER_UNDER_PROCESS(pInPort)) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pInPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pInPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pInPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(pInPort->hTunneledComponent, pInputBuffer);
				base_filter_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
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
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pInPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pInPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pInPort->bWaitingFlushSem=OMX_FALSE;
			}
			pthread_mutex_unlock(pInmutex);
		}
	} 
	if (portIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX || portIndex == OMX_BASE_FILTER_ALLPORT_INDEX) {
		if (! PORT_IS_TUNNELED(pOutPort)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing output ports outsemval=%d ob=%ld obcb=%ld\n",
				pOutputSem->semval,base_filter_component_Private->outbuffer,base_filter_component_Private->outbuffercb);
			/*Return All output buffers*/
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, pOutputBuffer);
				base_filter_component_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(pOutPort) && 
			(! PORT_IS_BUFFER_SUPPLIER(pOutPort))) {
			/*Return All output buffers*/
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				OMX_EmptyThisBuffer(pOutPort->hTunneledComponent, pOutputBuffer);
				base_filter_component_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(pOutPort) && 
				 PORT_IS_BUFFER_SUPPLIER(pOutPort)) {
			/*Flush all output buffers*/
			pthread_mutex_lock(pOutmutex);
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutPort->nNumBufferFlushed++;
			}
			/*Tunnel is supplier wait till all the buffers are returned*/
			if(pOutPort->nNumBufferFlushed<pOutPort->nNumTunnelBuffer) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pOutPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pOutmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffer being processed waitoutg for output flush sem*/
				tsem_down(pOutPort->pFlushSem);
				pthread_mutex_lock(pOutmutex);
				/*Indicate that pFlushSem waiting is over*/
				pOutPort->bWaitingFlushSem=OMX_FALSE;
			}
			pthread_mutex_unlock(pOutmutex);
		}
	} 
	 
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
