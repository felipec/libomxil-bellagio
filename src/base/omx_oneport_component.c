/**
 * @file src/components/newch/omx_oneport_component.c
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
 */


#include <omxcore.h>

#include <omx_oneport_component.h>

OMX_ERRORTYPE omx_oneport_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	omx_oneport_component_PrivateType* omx_oneport_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_oneport_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = base_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_oneport_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.eDir = OMX_DirInput;
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.nBufferSize = 0;
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;

	omx_oneport_component_Private->sPortTypesParam.nPorts = 1;
	omx_oneport_component_Private->sPortTypesParam.nStartPortNumber = 0;	
	
	omx_oneport_component_Private->BufferMgmtFunction = omx_oneport_component_BufferMgmtFunction;
	omx_oneport_component_Private->FlushPort = &omx_oneport_component_FlushPort;

	return err;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* omx_oneport_component_BufferMgmtFunction(void* param) {
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	omx_oneport_component_PrivateType* omx_oneport_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_thread = OMX_FALSE;
	OMX_BOOL isInputBufferEnded,flag;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *inbufferUnderProcess=&omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &omx_oneport_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &omx_oneport_component_Private->executingCondition;
	

	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting || 
		(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->transientState = OMX_StateIdle)){

		while(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bIsPortFlushed==OMX_TRUE )
			pthread_cond_wait(&omx_oneport_component_Private->flush_condition,&omx_oneport_component_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		pthread_mutex_lock(&omx_oneport_component_Private->exit_mutex);
		exit_thread = omx_oneport_component_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&omx_oneport_component_Private->exit_mutex);
		if(exit_thread == OMX_TRUE) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting from exec thread...\n");
			break;
		}
		pthread_mutex_lock(pInmutex);
		*inbufferUnderProcess = OMX_TRUE;
		pthread_mutex_unlock(pInmutex);

		pInputBuffer = dequeue(pInputQueue);
		if(pInputBuffer == NULL){
			DEBUG(DEB_LEV_ERR, "What the hell!! had NULL input buffer!!\n");
			base_component_Panic();
		}
		nFlags=pInputBuffer->nFlags;
		if(nFlags==OMX_BUFFERFLAG_EOS) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");
		}
		if(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bIsPortFlushed==OMX_TRUE) {
			base_component_returnInputBuffer(stComponent,pInputBuffer,omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]);
			/*Return Input Buffer*/
			continue;
		}
		
		pthread_mutex_lock(&omx_oneport_component_Private->exit_mutex);
		exit_thread=omx_oneport_component_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&omx_oneport_component_Private->exit_mutex);
		
		if(exit_thread==OMX_TRUE) {
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded) {
		/**  This condition becomes true when the input buffer has completely be consumed.
			*  In this case is immediately switched because there is no real buffer consumption */
			isInputBufferEnded = OMX_TRUE;
									
			if(omx_oneport_component_Private->pMark!=NULL){
				omx_oneport_component_Private->pMark=NULL;
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
			if (omx_oneport_component_Private->BufferMgmtCallback) {
				(*(omx_oneport_component_Private->BufferMgmtCallback))(stComponent, pInputBuffer);
			}
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

		base_component_returnInputBuffer(stComponent,pInputBuffer,omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]);

		pthread_mutex_lock(&omx_oneport_component_Private->exit_mutex);
		exit_thread=omx_oneport_component_Private->bExit_buffer_thread;
		if(exit_thread == OMX_TRUE) {
			pthread_mutex_unlock(&omx_oneport_component_Private->exit_mutex);
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}
		else 
			pthread_mutex_unlock(&omx_oneport_component_Private->exit_mutex);
		continue;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
	return NULL;
}

/** Flushes all the buffers under processing by the given port. 
	* This function si called due to a state change of the component, typically
	* @param stComponent the component which owns the port to be flushed
	* @param portIndex the ID of the port to be flushed
	*/
OMX_ERRORTYPE omx_oneport_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	omx_oneport_component_PrivateType* omx_oneport_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_BOOL *inbufferUnderProcess=&omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->mutex;
	pthread_cond_t* executingCondition = &omx_oneport_component_Private->executingCondition;
	OMX_BOOL flag;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_ONEPORT_INPUTPORT_INDEX || portIndex == OMX_ONEPORT_ALLPORT_INDEX) {
		if (!(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nTunnelFlags & TUNNEL_ESTABLISHED)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d ib=%ld,ibcb=%ld\n",
				pInputSem->semval,omx_oneport_component_Private->inbuffer,omx_oneport_component_Private->inbuffercb);
			
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
				omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}

				/*Buffering being processed waiting for input flush sem*/
				tsem_down(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pFlushSem);
				pthread_mutex_lock(pInmutex);
				omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			pthread_mutex_lock(pInmutex);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				(*(stComponent->callbacks->EmptyBufferDone))
					(pHandle, stComponent->callbackData, pInputBuffer);
				omx_oneport_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
		}
		else if ((omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(!(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nTunnelFlags & TUNNEL_IS_SUPPLIER))) {
			
			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
				omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pFlushSem);
				pthread_mutex_lock(pInmutex);
				omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			pthread_mutex_lock(pInmutex);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->hTunneledComponent, pInputBuffer);
				omx_oneport_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
		}
		else if ((omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nTunnelFlags & TUNNEL_ESTABLISHED) && 
			(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pInmutex);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nNumBufferFlushed++;
			}
			pthread_mutex_unlock(pInmutex);

			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nNumBufferFlushed<omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->nNumTunnelBuffer) {
			omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->pFlushSem);
			pthread_mutex_lock(pInmutex);
			omx_oneport_component_Private->ports[OMX_ONEPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else
				pthread_mutex_unlock(pInmutex);
		}
	} 
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
