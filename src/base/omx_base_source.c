/**
 * @file src/base/omx_base_source.c
 * 
 * OpenMax base source component. This component does not perform any multimedia
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
 * 2006/05/11:  base source component version 0.2
 *
 */


#include <omxcore.h>

#include <omx_base_source.h>

OMX_ERRORTYPE omx_base_source_Constructor(OMX_COMPONENTTYPE *openmaxStandComp) {
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
	err = base_component_Constructor(openmaxStandComp);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_base_source_Private = stComponent->omx_component.pComponentPrivate;
	
	// oh well, for the time being, set the port params, now that the ports exist	
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.eDir = OMX_DirOutput;
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.nBufferCountActual = 2;
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.nBufferCountMin = 2;
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.bEnabled = OMX_TRUE;
	omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->sPortParam.bPopulated = OMX_FALSE;

	omx_base_source_Private->sPortTypesParam.nPorts = 1;
	omx_base_source_Private->sPortTypesParam.nStartPortNumber = 0;	
	
	omx_base_source_Private->BufferMgmtFunction = omx_base_source_BufferMgmtFunction;
	omx_base_source_Private->FlushPort = &omx_base_source_FlushPort;

	return err;
}

/** This is the central function for component processing. It
	* is executed in a separate thread, is synchronized with 
	* semaphores at each port, those are released each time a new buffer
	* is available on the given port.
	*/
void* omx_base_source_BufferMgmtFunction(void* param) {
	stComponentType* stComponent = (stComponentType*)param;
	OMX_COMPONENTTYPE* pHandle = &stComponent->omx_component;
	omx_base_source_PrivateType* omx_base_source_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pOutputSem = omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pOutputQueue = omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BOOL exit_condition = OMX_FALSE;
	OMX_BOOL isOutputBufferEnded;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	OMX_U32  nFlags;
	OMX_BOOL *outbufferUnderProcess=&omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->bBufferUnderProcess;
	pthread_mutex_t *pOutmutex=&omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->mutex;
	OMX_COMPONENTTYPE* target_component;
	pthread_mutex_t* executingMutex = &omx_base_source_Private->executingMutex;
	pthread_cond_t* executingCondition = &omx_base_source_Private->executingCondition;
	base_component_PortType *pPort=(base_component_PortType *)omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
	while(stComponent->state == OMX_StateIdle || stComponent->state == OMX_StateExecuting ||   stComponent->state == OMX_StatePause ||
		(pPort->transientState == OMX_StateIdle)){

		/*Wait till the ports are being flushed*/
		pthread_mutex_lock(&omx_base_source_Private->flush_mutex);
		while( PORT_IS_BEING_FLUSHED(pPort))
			pthread_cond_wait(&omx_base_source_Private->flush_condition,&omx_base_source_Private->flush_mutex);
		pthread_mutex_unlock(&omx_base_source_Private->flush_mutex);

		DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for output buffer semval=%d \n",pOutputSem->semval);
		tsem_down(pOutputSem);
		DEBUG(DEB_LEV_FULL_SEQ, "Output buffer arrived\n");
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(omx_base_source_Private, exit_condition))
			break;

		pthread_mutex_lock(pOutmutex);
		*outbufferUnderProcess = OMX_TRUE;
		pthread_mutex_unlock(pOutmutex);

		pOutputBuffer = dequeue(pOutputQueue);
		if(pOutputBuffer == NULL){
			DEBUG(DEB_LEV_ERR, "What the hell!! had NULL output buffer!!\n");
			break;
		}
		nFlags=pOutputBuffer->nFlags;
		if(nFlags==OMX_BUFFERFLAG_EOS) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in output buffer\n");
		}
		if(pPort->bIsPortFlushed==OMX_TRUE) {
			/*Return Output Buffer*/
			base_component_returnOutputBuffer(stComponent,pOutputBuffer,pPort);
			continue;
		}
		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(omx_base_source_Private, exit_condition))
			break;
		isOutputBufferEnded = OMX_FALSE;
		
		while(!isOutputBufferEnded) {
		/**  This condition becomes true when the output buffer has completely be consumed.
			*  In this case is immediately switched because there is no real buffer consumption */
			isOutputBufferEnded = OMX_TRUE;
									
			if(omx_base_source_Private->pMark!=NULL){
				omx_base_source_Private->pMark=NULL;
			}
			target_component=(OMX_COMPONENTTYPE*)pOutputBuffer->hMarkTargetComponent;
			if(target_component==(OMX_COMPONENTTYPE *)&stComponent->omx_component) {
				/*Clear the mark and generate an event*/
				(*(stComponent->callbacks->EventHandler))
					(pHandle,
						stComponent->callbackData,
						OMX_EventMark, /* The command was completed */
						1, /* The commands was a OMX_CommandStateSet */
						0, /* The state has been changed in message->messageParam2 */
						pOutputBuffer->pMarkData);
			} else if(pOutputBuffer->hMarkTargetComponent!=NULL){
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
			if (omx_base_source_Private->BufferMgmtCallback) {
				(*(omx_base_source_Private->BufferMgmtCallback))(stComponent, pOutputBuffer);
			}
		}
		/*Wait if state is pause*/
		if(stComponent->state==OMX_StatePause) {
			if(pPort->bWaitingFlushSem!=OMX_TRUE) {
				pthread_cond_wait(executingCondition,executingMutex);
			}
		}

		/*Return Output Buffer*/
		base_component_returnOutputBuffer(stComponent,pOutputBuffer,pPort);

		/*If Component is De-initializing, exit buffer management thread*/
		if(IS_COMPONENT_DEINIT(omx_base_source_Private, exit_condition))
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
OMX_ERRORTYPE omx_base_source_FlushPort(stComponentType* stComponent, OMX_U32 portIndex)
{
	OMX_COMPONENTTYPE* pHandle=&stComponent->omx_component;
	omx_base_source_PrivateType* omx_base_source_Private = stComponent->omx_component.pComponentPrivate;
	tsem_t* pOutputSem = omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->pBufferSem;
	queue_t* pOutputQueue = omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->pBufferQueue;
	OMX_BUFFERHEADERTYPE* pOutputBuffer;
	pthread_mutex_t *pOutmutex=&omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX]->mutex;
	pthread_cond_t* executingCondition = &omx_base_source_Private->executingCondition;
	base_component_PortType *pPort=(base_component_PortType *)omx_base_source_Private->ports[OMX_BASE_SOURCE_OUTPUTPORT_INDEX];
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s portIndex=%ld\n", __func__,portIndex);

	if (portIndex == OMX_BASE_SOURCE_OUTPUTPORT_INDEX || portIndex == OMX_BASE_SOURCE_ALLPORT_INDEX) {
		if (! PORT_IS_TUNNELED(pPort)) {
			DEBUG(DEB_LEV_PARAMS,"Flashing output ports outsemval=%d ob=%ld obcb=%ld\n",
				pOutputSem->semval,omx_base_source_Private->outbuffer,omx_base_source_Private->outbuffercb);
			/*Return All output buffers*/
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				(*(stComponent->callbacks->FillBufferDone))
					(pHandle, stComponent->callbackData, pOutputBuffer);
				omx_base_source_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(pPort) && 
			(! PORT_IS_BUFFER_SUPPLIER(pPort))) {
			/*Return All output buffers*/
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pOutputBuffer = dequeue(pOutputQueue);
				OMX_EmptyThisBuffer(pPort->hTunneledComponent, pOutputBuffer);
				omx_base_source_Private->outbuffercb++;
			}
		}
		else if (PORT_IS_TUNNELED(pPort) && 
				 PORT_IS_BUFFER_SUPPLIER(pPort)) {
			/*Flush all output buffers*/
			pthread_mutex_lock(pOutmutex);
			while(pOutputSem->semval>0) {
				tsem_down(pOutputSem);
				pPort->nNumBufferFlushed++;
			}
			/*Tunnel is supplier wait till all the buffers are returned*/
			if(pPort->nNumBufferFlushed<pPort->nNumTunnelBuffer) {
				/*Indicate that pFlushSem is waiting for buffer for flushing*/
				pPort->bWaitingFlushSem=OMX_TRUE;
				pthread_mutex_unlock(pOutmutex);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s in\n",__func__);
				/*Dummy signaling to buffer management thread,if waiting in the paused condition and
				now port flush request received*/
				if(stComponent->state==OMX_StatePause) {
					pthread_cond_signal(executingCondition);
				}
				/*Buffer being processed waitoutg for output flush sem*/
				tsem_down(pPort->pFlushSem);
				pthread_mutex_lock(pOutmutex);
				pPort->bWaitingFlushSem=OMX_FALSE;
			}
			pthread_mutex_unlock(pOutmutex);
		}
	}
		
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s \n", __func__);

	return OMX_ErrorNone;
}
