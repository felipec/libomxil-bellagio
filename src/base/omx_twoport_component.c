/**
 * @file src/components/newch/omx_twoport_component.c
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

#include <omx_twoport_component.h>

OMX_ERRORTYPE omx_twoport_component_Constructor(stComponentType* stComponent) {
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	omx_twoport_component_PrivateType* omx_twoport_component_Private;

	if (!stComponent->omx_component.pComponentPrivate) {
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_twoport_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}
	
	/*Call the base class constructory*/
	err = base_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_twoport_component_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.eDir = OMX_DirInput;
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.nBufferSize = 0;
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;

	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.eDir = OMX_DirOutput;
	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = 0;
	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;
	
	omx_twoport_component_Private->sPortTypesParam.nPorts = 2;
	omx_twoport_component_Private->sPortTypesParam.nStartPortNumber = 0;	
	
	omx_twoport_component_Private->BufferMgmtFunction = omx_twoport_component_BufferMgmtFunction;
	omx_twoport_component_Private->FlushPort = &omx_twoport_component_FlushPort;

	return err;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* omx_twoport_component_BufferMgmtFunction(void* param) {
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	omx_twoport_component_PrivateType* omx_twoport_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pBufferSem;
	tsem_t* pOutputSem = omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pBufferQueue;
	queue_t* pOutputQueue = omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_thread = OMX_FALSE;
	OMX_BOOL isInputBufferEnded,flag;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *inbufferUnderProcess=&omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->mutex;
	pthread_mutex_t *pOutmutex=&omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &omx_twoport_component_Private->executingMutex;
	pthread_cond_t* executingCondition = &omx_twoport_component_Private->executingCondition;
	

	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting || 
		((omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->transientState = OMX_StateIdle) && 
		(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->transientState = OMX_StateIdle))){

		while(! PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]) || 
			  ! PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]))
			pthread_cond_wait(&omx_twoport_component_Private->flush_condition,&omx_twoport_component_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
		tsem_down(pInputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Input buffer arrived\n");
		pthread_mutex_lock(&omx_twoport_component_Private->exit_mutex);
		exit_thread = omx_twoport_component_Private->bExit_buffer_thread;
		pthread_mutex_unlock(&omx_twoport_component_Private->exit_mutex);
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
		if(! PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX])) {
			base_component_returnInputBuffer(stComponent,pInputBuffer,omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]);
			/*Return Input Buffer*/
			continue;
		}
		
		isInputBufferEnded = OMX_FALSE;
		
		while(!isInputBufferEnded && 
			PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]) &&
			PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX])) {
			
			DEBUG(DEB_LEV_FULL_SEQ, "Waiting for output buffer\n");
			tsem_down(pOutputSem);
			pthread_mutex_lock(pOutmutex);
			*outbufferUnderProcess = OMX_TRUE;
			pthread_mutex_unlock(pOutmutex);
			DEBUG(DEB_LEV_FULL_SEQ, "Output buffer arrived\n");
			if(! PORT_IS_BEING_FLUSHED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX])) {
				/*Return Input Buffer*/
				//base_component_returnInputBuffer(stComponent,pInputBuffer,omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]);
				pthread_mutex_lock(pOutmutex);
				*outbufferUnderProcess = OMX_FALSE;
				pthread_mutex_unlock(pOutmutex);
				continue;
			}

			pthread_mutex_lock(&omx_twoport_component_Private->exit_mutex);
			exit_thread=omx_twoport_component_Private->bExit_buffer_thread;
			pthread_mutex_unlock(&omx_twoport_component_Private->exit_mutex);
			
			if(exit_thread==OMX_TRUE) {
				DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
				pthread_mutex_lock(pOutmutex);
				*outbufferUnderProcess = OMX_FALSE;
				pthread_mutex_unlock(pOutmutex);
				break;
			}
			/* Get a new buffer from the output queue */
			pOutputBuffer = dequeue(pOutputQueue);
			if(pOutputBuffer == NULL){
				DEBUG(DEB_LEV_ERR, "What the hell!! had NULL output buffer!!\n");
				base_component_Panic();
			}
						
			if(omx_twoport_component_Private->pMark!=NULL){
				pOutputBuffer->hMarkTargetComponent=omx_twoport_component_Private->pMark->hMarkTargetComponent;
				pOutputBuffer->pMarkData=omx_twoport_component_Private->pMark->pMarkData;
				omx_twoport_component_Private->pMark=NULL;
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
			if (omx_twoport_component_Private->BufferMgmtCallback) {
				(*(omx_twoport_component_Private->BufferMgmtCallback))(stComponent, pInputBuffer, pOutputBuffer);
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
				if(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
				}
			}

			/*Call ETB in case port is enabled*/
			base_component_returnOutputBuffer(stComponent,pOutputBuffer,omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]);
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

		base_component_returnInputBuffer(stComponent,pInputBuffer,omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]);

		pthread_mutex_lock(&omx_twoport_component_Private->exit_mutex);
		exit_thread=omx_twoport_component_Private->bExit_buffer_thread;
		if(exit_thread == OMX_TRUE) {
			pthread_mutex_unlock(&omx_twoport_component_Private->exit_mutex);
			DEBUG(DEB_LEV_FULL_SEQ, "Exiting from exec thread...\n");
			break;
		}
		else 
			pthread_mutex_unlock(&omx_twoport_component_Private->exit_mutex);
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
OMX_ERRORTYPE omx_twoport_component_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	omx_twoport_component_PrivateType* omx_twoport_component_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pInputSem = omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pBufferSem;
	tsem_t* pOutputSem = omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pInputQueue = omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pBufferQueue;
	queue_t* pOutputQueue = omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	OMX_BUFFERHEADERTYPE* pInputBuffer;
	OMX_BOOL *inbufferUnderProcess=&omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bBufferUnderProcess;
	OMX_BOOL *outbufferUnderProcess=&omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pInmutex=&omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->mutex;
	pthread_mutex_t *pOutmutex=&omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->mutex;
	pthread_cond_t* executingCondition = &omx_twoport_component_Private->executingCondition;
	OMX_BOOL flag,dummyInc=OMX_FALSE;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_TWOPORT_INPUTPORT_INDEX || portIndex == OMX_TWOPORT_ALLPORT_INDEX) {
		if (! PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX])) {
			DEBUG(DEB_LEV_PARAMS,"Flashing input ports insemval=%d outsemval=%d ib=%ld,ibcb=%ld\n",
				pInputSem->semval,pOutputSem->semval,omx_twoport_component_Private->inbuffer,omx_twoport_component_Private->inbuffercb);
			pthread_mutex_lock(pOutmutex);
			if(pOutputSem->semval==0 && *inbufferUnderProcess==OMX_TRUE && *outbufferUnderProcess==OMX_FALSE) {
				/*This is to free the input buffer being processed*/
				tsem_up(pOutputSem);
				dummyInc=OMX_TRUE;
			}
			pthread_mutex_unlock(pOutmutex);

			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
				omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}

				/*Buffering being processed waiting for input flush sem*/
				tsem_down(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pFlushSem);
				pthread_mutex_lock(pInmutex);
				omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
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
				omx_twoport_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);

			if(pOutputSem->semval>0 && dummyInc==OMX_TRUE) {
				/*This is to free the input buffer being processed*/
				tsem_down(pOutputSem);
			}
		}
		else if (PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]) && 
			  (! PORT_IS_BUFFER_SUPPLIER(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]))) {
			if(pOutputSem->semval==0 && *inbufferUnderProcess==OMX_TRUE && *outbufferUnderProcess==OMX_FALSE) {
				/*This is to free the input buffer being processed*/
				tsem_up(pOutputSem);
				dummyInc=OMX_TRUE;
			}

			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(flag==OMX_TRUE) {
				omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pInmutex);
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffering being processed waiting for input flush sem*/
				tsem_down(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pFlushSem);
				pthread_mutex_lock(pInmutex);
				omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(pInmutex);
			}
			else {
				pthread_mutex_unlock(pInmutex);
			}
			pthread_mutex_lock(pInmutex);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				pInputBuffer = dequeue(pInputQueue);
				OMX_FillThisBuffer(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->hTunneledComponent, pInputBuffer);
				omx_twoport_component_Private->inbuffercb++;
			}
			pthread_mutex_unlock(pInmutex);
			
			if(pOutputSem->semval>0 && dummyInc==OMX_TRUE) {
				/*This is to free the input buffer being processed*/
				tsem_down(pOutputSem);
			}
		}
		else if (PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]) && 
				 PORT_IS_BUFFER_SUPPLIER(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX])) {

			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pInmutex);
			while(pInputSem->semval>0) {
				tsem_down(pInputSem);
				omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->nNumBufferFlushed++;
			}
			pthread_mutex_unlock(pInmutex);

			pthread_mutex_lock(pInmutex);
			flag=*inbufferUnderProcess;
			if(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->nNumBufferFlushed<omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->nNumTunnelBuffer) {
			omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
			pthread_mutex_unlock(pInmutex);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
			if(stComponent->state==OMX_StatePause) {
				pthread_cond_signal(executingCondition);
			}
			/*Buffering being processed waiting for input flush sem*/
			tsem_down(omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->pFlushSem);
			pthread_mutex_lock(pInmutex);
			omx_twoport_component_Private->ports[OMX_TWOPORT_INPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
			pthread_mutex_unlock(pInmutex);
			}
			else
				pthread_mutex_unlock(pInmutex);
		}
	} 
	if (portIndex == OMX_TWOPORT_OUTPUTPORT_INDEX || portIndex == OMX_TWOPORT_ALLPORT_INDEX) {
		if (! PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX])) {
			DEBUG(DEB_LEV_PARAMS,"Flashing output ports outsemval=%d ob=%ld obcb=%ld\n",
				pOutputSem->semval,omx_twoport_component_Private->outbuffer,omx_twoport_component_Private->outbuffercb);
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, pOutputBuffer);
				omx_twoport_component_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]) && 
			(! PORT_IS_BUFFER_SUPPLIER(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]))) {
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				OMX_EmptyThisBuffer(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->hTunneledComponent, pOutputBuffer);
				omx_twoport_component_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]) && 
				 PORT_IS_BUFFER_SUPPLIER(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX])) {

			pthread_mutex_lock(pOutmutex);
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->nNumBufferFlushed++;
			}
			pthread_mutex_unlock(pOutmutex);
			/*Tunnel is supplier wait till all the buffers are returned*/
			pthread_mutex_lock(pOutmutex);
			flag=*outbufferUnderProcess;
			if(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->nNumBufferFlushed<omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->nNumTunnelBuffer) {
				omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pOutmutex);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Bufferoutg beoutg processed waitoutg for output flush sem*/
				tsem_down(omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->pFlushSem);
				pthread_mutex_lock(pOutmutex);
				omx_twoport_component_Private->ports[OMX_TWOPORT_OUTPUTPORT_INDEX]->bWaitingFlushSem=OMX_FALSE;
				pthread_mutex_unlock(pOutmutex);
			}
			else 
				pthread_mutex_unlock(pOutmutex);
		}
	} 
	 
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
