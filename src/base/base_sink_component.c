/**
 * @file src/base/base_sink_component.c
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

#include <base_sink_component.h>

OMX_ERRORTYPE base_sink_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	base_sink_component_PrivateType* base_sink_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(base_sink_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = base_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	base_sink_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.eDir = OMX_DirInput;
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;

	base_sink_component_Private->sPortTypesParam.nPorts = 1;
	base_sink_component_Private->sPortTypesParam.nStartPortNumber = 0;	
	
	base_sink_component_Private->BufferMgmtFunction = base_sink_component_BufferMgmtFunction;
	base_sink_component_Private->FlushPort = &base_sink_component_FlushPort;

	return err;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* base_sink_component_BufferMgmtFunction(void* param) {
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	base_sink_component_PrivateType* base_sink_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_condition = OMX_FALSE;
	OMX_BOOL isInputBufferEnded;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *inbufferUnderProcess=&base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &base_sink_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &base_sink_component_Private->executingCondition;
	base_component_PortType *pPort=(base_component_PortType *)base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting || stComponent->state == OMX_StatePause || 
		(pPort->transientState == OMX_StateIdle)){

		/*Wait till the ports are being flushed*/
		pthread_mutex_lock(&base_sink_component_Private->flush_mutex);
		while(! PORT_IS_BEING_FLUSHED(pPort))
			pthread_cond_wait(&base_sink_component_Private->flush_condition,&base_sink_component_Private->flush_mutex);
		pthread_mutex_unlock(&base_sink_component_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(base_sink_component_Private, exit_condition))
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
		if(pPort->bIsPortFlushed==OMX_TRUE) {
			/*Return Input Buffer*/
			base_component_returnInputBuffer(stComponent,pInputBuffer,pPort);
			continue;
		}
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(base_sink_component_Private, exit_condition))
			break;
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded) {
		/**  This condition becomes true when the input buffer has completely be consumed.
			*  In this case is immediately switched because there is no real buffer consumption */
			isInputBufferEnded = OMX_TRUE;
									
			if(base_sink_component_Private->pMark!=NULL){
				base_sink_component_Private->pMark=NULL;
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
				DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
			}

			/*Need to be verified pankaj*/
			if(nFlags==OMX_BUFFERFLAG_EOS) {
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventBufferFlag, /* The command was completed */
						0, /* The commands was a OMX_CommandStateSet */
						nFlags, /* The state has been changed in message->messageParam2 */
						NULL);
			}
			
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: got some buffers \n", __func__);

			/* This calls the actual algorithm; fp must be set */			
			if (base_sink_component_Private->BufferMgmtCallback) {
				(*(base_sink_component_Private->BufferMgmtCallback))(stComponent, pInputBuffer);
			}
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(pPort->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

		/*Return Input Buffer*/
		base_component_returnInputBuffer(stComponent,pInputBuffer,pPort);

		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(base_sink_component_Private, exit_condition))
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
OMX_ERRORTYPE base_sink_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	base_sink_component_PrivateType* base_sink_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	pthread_mutex_t *pInmutex=&base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX]->mutex;
	pthread_cond_t* executingCondition = &base_sink_component_Private->executingCondition;
	base_component_PortType *pPort=(base_component_PortType *)base_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_BASE_SINK_INPUTPORT_INDEX || portIndex == OMX_BASE_SINK_ALLPORT_INDEX) {
		if (!PORT_IS_TUNNELED(pPort)) { // Port is not tunnelled 
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d ib=%ld,ibcb=%ld\n",
				pInputSem->semval,base_sink_component_Private->inbuffer,base_sink_component_Private->inbuffercb);
			/*First return the Buffer presently being processed,if any*/
			pthread_mutex_lock(pInmutex);
			if(IS_BUFFER_UNDER_PROCESS(pPort)) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pInputBuffer);
				base_sink_component_Private->inbuffercb++;
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
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pInmutex);
				/*Indicate that pFlushSem waiting is over*/
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			/*Return All other input buffers*/
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(pPort->hTunneledComponent, pInputBuffer);
				base_sink_component_Private->inbuffercb++;
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
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(pPort->pFlushSem);
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
