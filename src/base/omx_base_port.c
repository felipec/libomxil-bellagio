/**
  @file src/base/omx_base_port.c

  Base class for OpenMAX ports to be used in derived components.

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

#include <stdlib.h>
#include <string.h>
#include <omxcore.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "st_static_component_loader.h"
#include "omx_base_component.h"
#include "omx_base_port.h"


/** 
  * @brief the base contructor for the generic openmax ST port
  * 
  * This function is executed by the component that uses a port.
  * The parameter contains the info about the component.
  * It takes care of constructing the instance of the port and 
  * every object needed by the base port.
  * 
  * @param openmaxStandPort the ST port to be initialized
  * @param isInput specifices if the port is an input or an output
  * 
  * @return OMX_ErrorInsufficientResources if a memory allocation fails
  */

OMX_ERRORTYPE base_port_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,omx_base_PortType **openmaxStandPort,OMX_U32 nPortIndex, OMX_BOOL isInput) {
	
  omx_base_component_PrivateType* omx_base_component_Private;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;

  // create ports, but only if the subclass hasn't done it
  if (!(*openmaxStandPort)) {
    *openmaxStandPort = (omx_base_PortType *)calloc(1,sizeof (omx_base_PortType));
  }

  if (!(*openmaxStandPort)) return OMX_ErrorInsufficientResources;

  (*openmaxStandPort)->hTunneledComponent = NULL;
  (*openmaxStandPort)->nTunnelFlags=0;
  (*openmaxStandPort)->nTunneledPort=0;
  (*openmaxStandPort)->eBufferSupplier=OMX_BufferSupplyUnspecified; 
  (*openmaxStandPort)->nNumTunnelBuffer=0;

  if((*openmaxStandPort)->pAllocSem==NULL) {
    (*openmaxStandPort)->pAllocSem = calloc(1,sizeof(tsem_t));
    if((*openmaxStandPort)->pAllocSem==NULL)	return OMX_ErrorInsufficientResources;
    tsem_init((*openmaxStandPort)->pAllocSem, 0);
  }
  (*openmaxStandPort)->bWaitingFlushSem=OMX_FALSE;
  (*openmaxStandPort)->bBufferUnderProcess=OMX_FALSE;
  (*openmaxStandPort)->nNumBufferFlushed=0; 
  (*openmaxStandPort)->bIsPortFlushed=OMX_FALSE;
  /** Allocate and initialize buffer queue */
  if(!(*openmaxStandPort)->pBufferQueue) {
    (*openmaxStandPort)->pBufferQueue = calloc(1,sizeof(queue_t));
    if((*openmaxStandPort)->pBufferQueue==NULL) return OMX_ErrorInsufficientResources;
    queue_init((*openmaxStandPort)->pBufferQueue);
  }
  /*Allocate and initialise port semaphores*/
  if(!(*openmaxStandPort)->pBufferSem) {
    (*openmaxStandPort)->pBufferSem = calloc(1,sizeof(tsem_t));
    if((*openmaxStandPort)->pBufferSem==NULL) return OMX_ErrorInsufficientResources;
    tsem_init((*openmaxStandPort)->pBufferSem, 0);
  }

  (*openmaxStandPort)->nNumAssignedBuffers=0;
  setHeader(&(*openmaxStandPort)->sPortParam, sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
  (*openmaxStandPort)->sPortParam.nPortIndex = nPortIndex;
  (*openmaxStandPort)->sPortParam.nBufferCountActual=2;
  (*openmaxStandPort)->sPortParam.nBufferCountMin=2;   
  (*openmaxStandPort)->sPortParam.bEnabled = OMX_TRUE;
  (*openmaxStandPort)->sPortParam.bPopulated = OMX_FALSE; 
  (*openmaxStandPort)->sPortParam.eDir  =  (isInput == OMX_TRUE)?OMX_DirInput:OMX_DirOutput;

  (*openmaxStandPort)->bBufferStateAllocated=(BUFFER_STATUS_FLAG *)calloc(1,(*openmaxStandPort)->sPortParam.nBufferCountActual*sizeof(BUFFER_STATUS_FLAG *));
  (*openmaxStandPort)->pInternalBufferStorage=(OMX_BUFFERHEADERTYPE **)calloc(1,(*openmaxStandPort)->sPortParam.nBufferCountActual*sizeof(OMX_BUFFERHEADERTYPE *));

  for(i=0; i < (*openmaxStandPort)->sPortParam.nBufferCountActual; i++)
    (*openmaxStandPort)->bBufferStateAllocated[i] = BUFFER_FREE;

  (*openmaxStandPort)->standCompContainer=openmaxStandComp; 
  (*openmaxStandPort)->bIsEnabled=OMX_TRUE;
  (*openmaxStandPort)->bIsTransientToEnabled=OMX_FALSE;
  (*openmaxStandPort)->bIsTransientToDisabled=OMX_FALSE;
  (*openmaxStandPort)->bIsInputPort = isInput;
  (*openmaxStandPort)->bIsFullOfBuffers=OMX_FALSE;
  (*openmaxStandPort)->bIsEmptyOfBuffers=OMX_FALSE;
  (*openmaxStandPort)->nPortIndex=nPortIndex;

  pthread_mutex_init(&(*openmaxStandPort)->mutex, NULL);

  (*openmaxStandPort)->PortDestructor = &base_port_Destructor;
  (*openmaxStandPort)->Port_AllocateBuffer = &base_port_AllocateBuffer;
  (*openmaxStandPort)->Port_UseBuffer = &base_port_UseBuffer;
  (*openmaxStandPort)->Port_FreeBuffer = &base_port_FreeBuffer;
  (*openmaxStandPort)->Port_DisablePort = &base_port_DisablePort;
  (*openmaxStandPort)->Port_EnablePort = &base_port_EnablePort;
  (*openmaxStandPort)->Port_SendBufferFunction = &base_port_SendBufferFunction;
  (*openmaxStandPort)->FlushProcessingBuffers = &base_port_FlushProcessingBuffers;
  (*openmaxStandPort)->ReturnBufferFunction = &base_port_ReturnBufferFunction;
  (*openmaxStandPort)->ComponentTunnelRequest = &base_port_ComponentTunnelRequest;
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE base_port_Destructor(omx_base_PortType *openmaxStandPort){

  if(openmaxStandPort->pAllocSem) {
    tsem_deinit(openmaxStandPort->pAllocSem);
    free(openmaxStandPort->pAllocSem);
    openmaxStandPort->pAllocSem=NULL;
  }
  /** Allocate and initialize buffer queue */
  if(openmaxStandPort->pBufferQueue) {
    queue_deinit(openmaxStandPort->pBufferQueue);
    free(openmaxStandPort->pBufferQueue);
    openmaxStandPort->pBufferQueue=NULL;
  }
  /*Allocate and initialise port semaphores*/
  if(openmaxStandPort->pBufferSem) {
    tsem_deinit(openmaxStandPort->pBufferSem);
    free(openmaxStandPort->pBufferSem);
    openmaxStandPort->pBufferSem=NULL;
  }

  free(openmaxStandPort->bBufferStateAllocated);
  openmaxStandPort->bBufferStateAllocated=NULL;
  free(openmaxStandPort->pInternalBufferStorage);
  openmaxStandPort->pInternalBufferStorage=NULL;

  free(openmaxStandPort);

  return OMX_ErrorNone;
}
/** @brief Disables the port.
 * 
 * This function is called due to a request by the IL client
 * 
 * @param openmaxStandPort the reference to the port
 * 
 */
OMX_ERRORTYPE base_port_DisablePort(omx_base_PortType *openmaxStandPort) {
  omx_base_component_PrivateType* omx_base_component_Private;
  OMX_BUFFERHEADERTYPE* pBuffer;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s Port Index=%d\n", __func__,openmaxStandPort->nPortIndex);
  omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandPort->standCompContainer->pComponentPrivate;
  if (!openmaxStandPort->bIsEnabled) {
    return OMX_ErrorNone;
  }
  openmaxStandPort->bIsTransientToDisabled = OMX_TRUE;

  if(omx_base_component_Private->state!=OMX_StateLoaded && PORT_IS_POPULATED(openmaxStandPort)) {
    /*First Flush all buffers then disable the port*/
    openmaxStandPort->FlushProcessingBuffers(openmaxStandPort);

    /*Wait till all buffers are freed*/
    tsem_down(openmaxStandPort->pAllocSem);
  }

  openmaxStandPort->bIsEnabled=OMX_FALSE;
  openmaxStandPort->bIsTransientToDisabled = OMX_FALSE;
  openmaxStandPort->sPortParam.bEnabled = OMX_FALSE;
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out %s Port Index=%d isEnabled=%d\n", __func__,openmaxStandPort->nPortIndex,openmaxStandPort->sPortParam.bEnabled);
  return OMX_ErrorNone;
}

/** @brief Enables the port.
 * 
 * This function is called due to a request by the IL client
 * 
 * @param openmaxStandPort the reference to the port
 * 
 */
OMX_ERRORTYPE base_port_EnablePort(omx_base_PortType *openmaxStandPort) {
  omx_base_component_PrivateType* omx_base_component_Private;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  if (openmaxStandPort->bIsEnabled) {
    return OMX_ErrorNone;
  }
  omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandPort->standCompContainer->pComponentPrivate;
  /*
  if ((omx_base_component_Private->state != OMX_StateIdle) &&
    (omx_base_component_Private->state != OMX_StateExecuting) &&
    (omx_base_component_Private->state != OMX_StatePause)) {
    return OMX_ErrorIncorrectStateOperation;
  }
  */

  openmaxStandPort->sPortParam.bEnabled = OMX_TRUE;
  openmaxStandPort->bIsEnabled=OMX_TRUE;

  openmaxStandPort->bIsTransientToEnabled = OMX_TRUE;

  DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for buffers to be allocated\n", __func__);
  if (!PORT_IS_TUNNELED(openmaxStandPort)) {
    /*Wait Till All buffers are allocated if the component state is not Loaded*/
    if (omx_base_component_Private->state!=OMX_StateLoaded && omx_base_component_Private->state!=OMX_StateWaitForResources)  {
      tsem_down(openmaxStandPort->pAllocSem);
    }
  }
  else { //TODO: Handle the case of Port Tunneled and supplier. Then allocate tunnel buffers
    DEBUG(DEB_LEV_FULL_SEQ, "In %s waiting for buffers to be allocated\n", __func__);
  }

  openmaxStandPort->bIsTransientToEnabled = OMX_FALSE;
  openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);

  return OMX_ErrorNone;
}

/** @brief Called by the standard allocate buffer, it implements a base functionality.
 *  
 * This function can be overriden if the allocation of the buffer is not a simply malloc call.
 * The parameters are the same as the standard function, except for the handle of the port 
 * instead of the handler of the component
 * When the buffers needed by this port are all assigned or allocated, the variable 
 * bIsFullOfBuffers becomes equal to OMX_TRUE
 */
OMX_ERRORTYPE base_port_AllocateBuffer(
	omx_base_PortType *openmaxStandPort,
	OMX_BUFFERHEADERTYPE** pBuffer,
	OMX_U32 nPortIndex,
	OMX_PTR pAppPrivate,
	OMX_U32 nSizeBytes) {
	
	int i;
	OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
	omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

	if (nPortIndex != openmaxStandPort->nPortIndex) {
		return OMX_ErrorBadPortIndex;
	}
	if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && (openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorBadPortIndex;
	}
	
	if (omx_base_component_Private->transientState != OMX_StateIdle) {
		if (!openmaxStandPort->bIsTransientToEnabled) {
			DEBUG(DEB_LEV_ERR, "In %s: The port is not allowed to receive buffers\n", __func__);
			return OMX_ErrorIncorrectStateTransition;
		}
	}
	
	for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
		if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_FREE) {
			
			openmaxStandPort->pInternalBufferStorage[i] = calloc(1,sizeof(OMX_BUFFERHEADERTYPE));
			if (!openmaxStandPort->pInternalBufferStorage[i]) {
				return OMX_ErrorInsufficientResources;
			}
			setHeader(openmaxStandPort->pInternalBufferStorage[i], sizeof(OMX_BUFFERHEADERTYPE));
			/* allocate the buffer */
			openmaxStandPort->pInternalBufferStorage[i]->pBuffer = calloc(1,nSizeBytes);
			if(openmaxStandPort->pInternalBufferStorage[i]->pBuffer==NULL) {
				return OMX_ErrorInsufficientResources;
			}
			openmaxStandPort->pInternalBufferStorage[i]->nAllocLen = nSizeBytes;
			openmaxStandPort->pInternalBufferStorage[i]->pPlatformPrivate = openmaxStandPort;
			openmaxStandPort->pInternalBufferStorage[i]->pAppPrivate = pAppPrivate;
			*pBuffer = openmaxStandPort->pInternalBufferStorage[i];
			openmaxStandPort->bBufferStateAllocated[i] = BUFFER_ALLOCATED;
			
			if (openmaxStandPort->bIsInputPort) {
				openmaxStandPort->pInternalBufferStorage[i]->nInputPortIndex = openmaxStandPort->nPortIndex;
			} else {
				openmaxStandPort->pInternalBufferStorage[i]->nOutputPortIndex = openmaxStandPort->nPortIndex;
			}
			openmaxStandPort->nNumAssignedBuffers++;
			DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", openmaxStandPort->nNumAssignedBuffers);
			
			if (openmaxStandPort->sPortParam.nBufferCountActual == openmaxStandPort->nNumAssignedBuffers) {
				openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
				openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
        			DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nPortIndex=%d port=%x pAllocSem=%x\n",
          			__func__,nPortIndex,openmaxStandPort,openmaxStandPort->pAllocSem);
        			tsem_up(openmaxStandPort->pAllocSem);
			}
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_ERR, "Error: no available buffers\n");
	return OMX_ErrorInsufficientResources;
}
	
/** @brief Called by the standard use buffer, it implements a base functionality.
 *  
 * This function can be overriden if the use buffer implicate more complicated operations.
 * The parameters are the same as the standard function, except for the handle of the port 
 * instead of the handler of the component.
 * When the buffers needed by this port are all assigned or allocated, the variable 
 * bIsFullOfBuffers becomes equal to OMX_TRUE
 */
OMX_ERRORTYPE base_port_UseBuffer(
	omx_base_PortType *openmaxStandPort,
	OMX_BUFFERHEADERTYPE** ppBufferHdr,
	OMX_U32 nPortIndex,
	OMX_PTR pAppPrivate,
	OMX_U32 nSizeBytes,
	OMX_U8* pBuffer) {
	
	int i;
	OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
	omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

	if (nPortIndex != openmaxStandPort->nPortIndex) {
		return OMX_ErrorBadPortIndex;
	}
	if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && (openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorBadPortIndex;
	}
	
	if (omx_base_component_Private->transientState != OMX_StateIdle) {
		if (!openmaxStandPort->bIsTransientToEnabled) {
			DEBUG(DEB_LEV_ERR, "In %s: The port is not allowed to receive buffers\n", __func__);
			return OMX_ErrorIncorrectStateTransition;
		}
	}
	
	for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
		if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_FREE) {
			
			openmaxStandPort->pInternalBufferStorage[i] = calloc(1,sizeof(OMX_BUFFERHEADERTYPE));
			if (!openmaxStandPort->pInternalBufferStorage[i]) {
				return OMX_ErrorInsufficientResources;
			}
			openmaxStandPort->bIsEmptyOfBuffers = OMX_FALSE;
			setHeader(openmaxStandPort->pInternalBufferStorage[i], sizeof(OMX_BUFFERHEADERTYPE));

			openmaxStandPort->pInternalBufferStorage[i]->pBuffer = pBuffer;
			openmaxStandPort->pInternalBufferStorage[i]->nAllocLen = nSizeBytes;
			openmaxStandPort->pInternalBufferStorage[i]->pPlatformPrivate = openmaxStandPort;
			openmaxStandPort->pInternalBufferStorage[i]->pAppPrivate = pAppPrivate;
			*ppBufferHdr = openmaxStandPort->pInternalBufferStorage[i];
			openmaxStandPort->bBufferStateAllocated[i] = BUFFER_ASSIGNED;
			
			if (openmaxStandPort->bIsInputPort) {
				openmaxStandPort->pInternalBufferStorage[i]->nInputPortIndex = openmaxStandPort->nPortIndex;
			} else {
				openmaxStandPort->pInternalBufferStorage[i]->nOutputPortIndex = openmaxStandPort->nPortIndex;
			}
			openmaxStandPort->nNumAssignedBuffers++;
			DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", openmaxStandPort->nNumAssignedBuffers);
			
			if (openmaxStandPort->sPortParam.nBufferCountActual == openmaxStandPort->nNumAssignedBuffers) {
				openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
				openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
        			tsem_up(openmaxStandPort->pAllocSem);
			}
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_ERR, "Error: no available buffers\n");
	return OMX_ErrorInsufficientResources;
}

/** @brief Called by the standard function.
 * 
 * It frees the buffer header and in case also the buffer itself, if needed.
 * When all the bufers are done, the variable bIsEmptyOfBuffers is set to OMX_TRUE
 */
OMX_ERRORTYPE base_port_FreeBuffer(
	omx_base_PortType *openmaxStandPort,
	OMX_U32 nPortIndex,
	OMX_BUFFERHEADERTYPE* pBuffer) {
	
	int i;
	OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
	omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

	if (nPortIndex != openmaxStandPort->nPortIndex) {
		return OMX_ErrorBadPortIndex;
	}
	if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && (openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
		return OMX_ErrorBadPortIndex;
	}
	
	if (omx_base_component_Private->transientState != OMX_StateLoaded) {
		if (!openmaxStandPort->bIsTransientToDisabled) {
			DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to free the buffers\n", __func__);
			//return OMX_ErrorIncorrectStateTransition;
		  (*(omx_base_component_Private->callbacks->EventHandler))
			    (omxComponent,
				  omx_base_component_Private->callbackData,
				  OMX_EventError, /* The command was completed */
				  OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
				  nPortIndex, /* The state has been changed in message->messageParam2 */
				  NULL);
		}
	}
	
	for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
		if (openmaxStandPort->bBufferStateAllocated[i] & (BUFFER_ASSIGNED | BUFFER_ALLOCATED)) {
			
			openmaxStandPort->bIsFullOfBuffers = OMX_FALSE;
			if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_ALLOCATED) {
				free(openmaxStandPort->pInternalBufferStorage[i]->pBuffer);
			}
			free(openmaxStandPort->pInternalBufferStorage[i]);
			openmaxStandPort->bBufferStateAllocated[i] = BUFFER_FREE;
			
			openmaxStandPort->nNumAssignedBuffers--;
			DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", openmaxStandPort->nNumAssignedBuffers);
			
			if (openmaxStandPort->nNumAssignedBuffers == 0) {
				openmaxStandPort->sPortParam.bPopulated = OMX_FALSE;
				openmaxStandPort->bIsEmptyOfBuffers = OMX_TRUE;
			        tsem_up(openmaxStandPort->pAllocSem);
			}
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorInsufficientResources;
}

/** @brief the entry point for sending buffers to the port
 * 
 * This function can be called by the EmptyThisBuffer or FillThisBuffer. It depends on
 * the nature of the port, that can be an input or output port.
 */
OMX_ERRORTYPE base_port_SendBufferFunction(
	omx_base_PortType *openmaxStandPort,
	OMX_BUFFERHEADERTYPE* pBuffer) {
	
	OMX_ERRORTYPE err;
	OMX_U32 portIndex;
	OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
	omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
	
	portIndex = openmaxStandPort->bIsInputPort?pBuffer->nInputPortIndex:pBuffer->nOutputPortIndex;

	if (portIndex != openmaxStandPort->nPortIndex) {
		DEBUG(DEB_LEV_ERR, "In %s: wrong port for this operation portIndex=%d port->portIndex=%d\n", __func__,portIndex,openmaxStandPort->nPortIndex);
		return OMX_ErrorBadPortIndex;
	}
	
	if(omx_base_component_Private->state == OMX_StateInvalid) {
		DEBUG(DEB_LEV_ERR, "In %s: we are in OMX_StateInvalid\n", __func__);
		return OMX_ErrorInvalidState;
	}
	
	if(omx_base_component_Private->state != OMX_StateExecuting &&
			omx_base_component_Private->state != OMX_StatePause &&
			omx_base_component_Private->state != OMX_StateIdle) {
		DEBUG(DEB_LEV_ERR, "In %s: we are not in executing/paused/idle state, but in %d\n", __func__, omx_base_component_Private->state);
		return OMX_ErrorIncorrectStateOperation;
	}
	if (!openmaxStandPort->bIsEnabled) {
		DEBUG(DEB_LEV_ERR, "In %s: Port %d is disabled\n", __func__,portIndex);
		return OMX_ErrorIncorrectStateOperation;
	}
	if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
		return err;
	}
	
	/* And notify the buffer management thread we have a fresh new buffer to manage */
	if(!openmaxStandPort->bIsPortFlushed){
		queue(openmaxStandPort->pBufferQueue, pBuffer);
		tsem_up(openmaxStandPort->pBufferSem);
    		DEBUG(DEB_LEV_PARAMS, "In %s Signalling bMgmtSem Port Index=%d\n",__func__,portIndex);
    		tsem_signal(omx_base_component_Private->bMgmtSem);
  }
	return OMX_ErrorNone;
	
}

/** @brief Releases buffers under processing.
 * This function must be implemented in the derived classes, for the
 * specific processing
 */
OMX_ERRORTYPE base_port_FlushProcessingBuffers(omx_base_PortType *openmaxStandPort) {
  omx_base_component_PrivateType* omx_base_component_Private;
  OMX_BUFFERHEADERTYPE* pBuffer;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandPort->standCompContainer->pComponentPrivate;

  pthread_mutex_lock(&omx_base_component_Private->flush_mutex);
  openmaxStandPort->bIsPortFlushed=OMX_TRUE;
  /*Signal the buffer management thread of port flush,if it is waiting for buffers*/
  tsem_signal(omx_base_component_Private->bMgmtSem);
  DEBUG(DEB_LEV_ERR, "In %s waiting for flush all condition port index =%d\n", __func__,openmaxStandPort->nPortIndex);
  /*Wait till flush is completed*/
  pthread_cond_wait(&omx_base_component_Private->flush_all_condition,&omx_base_component_Private->flush_mutex);
  pthread_mutex_unlock(&omx_base_component_Private->flush_mutex);

  /*Flush all the buffer not under processing, in other words the buffers pending on the queue*/
  while (openmaxStandPort->pBufferSem->semval > 0) {
    DEBUG(DEB_LEV_ERR, "In %s Flusing Port=%d,Semval=%d\n", 
    __func__,openmaxStandPort->nPortIndex,openmaxStandPort->pBufferSem->semval);

    tsem_down(openmaxStandPort->pBufferSem);
    pBuffer = dequeue(openmaxStandPort->pBufferQueue);
    if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && !(openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
      if (openmaxStandPort->bIsInputPort) {
        ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->FillThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
      } else {
        ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->EmptyThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
      }
    }	
    else if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && (openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
      queue(openmaxStandPort->pBufferQueue,pBuffer);
    }
    else {
      (*(openmaxStandPort->BufferProcessedCallback))(
        openmaxStandPort->standCompContainer,
        omx_base_component_Private->callbackData,
        pBuffer);
    }
  }

  openmaxStandPort->bIsPortFlushed=OMX_FALSE;

  pthread_mutex_lock(&omx_base_component_Private->flush_mutex);
  pthread_cond_signal(&omx_base_component_Private->flush_condition);
  pthread_mutex_unlock(&omx_base_component_Private->flush_mutex);

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out %s Port Index=%d\n", __func__,openmaxStandPort->nPortIndex);

  return OMX_ErrorNone;
}

/**
 * Returns Input/Output Buffer to the IL client or Tunneled Component
 */
OMX_ERRORTYPE base_port_ReturnBufferFunction(omx_base_PortType* openmaxStandPort,OMX_BUFFERHEADERTYPE* pBuffer){ 
  omx_base_component_PrivateType* omx_base_component_Private=openmaxStandPort->standCompContainer->pComponentPrivate;
  queue_t* pQueue = openmaxStandPort->pBufferQueue;
  pthread_mutex_t *pMutex=&openmaxStandPort->mutex;

  if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && 
    !(openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER)) {
    if (openmaxStandPort->bIsInputPort) {
      ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->FillThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
    } else {
      ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->EmptyThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
    }
  }	
  else if ((openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED) && 
          (openmaxStandPort->nTunnelFlags & TUNNEL_IS_SUPPLIER) && 
          (openmaxStandPort->bIsPortFlushed==OMX_FALSE)) {
    if (openmaxStandPort->bIsInputPort) {
      ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->FillThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
    } else {
      ((OMX_COMPONENTTYPE*)(openmaxStandPort->hTunneledComponent))->EmptyThisBuffer(openmaxStandPort->hTunneledComponent, pBuffer);
    }
  }else if (!(openmaxStandPort->nTunnelFlags & TUNNEL_ESTABLISHED)){
    (*(openmaxStandPort->BufferProcessedCallback))(
      openmaxStandPort->standCompContainer,
      omx_base_component_Private->callbackData,
      pBuffer);
  }
  else {
    queue(pQueue,pBuffer);
    pthread_mutex_lock(pMutex);
    openmaxStandPort->nNumBufferFlushed++;
    pthread_mutex_unlock(pMutex);
  }

  pthread_mutex_lock(pMutex);
  openmaxStandPort->bBufferUnderProcess = OMX_FALSE;
  /*
  if(PORT_IS_WAITING_FLUSH_SEMAPHORE(openmaxStandPort)) {		
  if ( ! (PORT_IS_TUNNELED(openmaxStandPort) && PORT_IS_BUFFER_SUPPLIER(openmaxStandPort) &&
  openmaxStandPort->nNumBufferFlushed != openmaxStandPort->nNumTunnelBuffer) ) {
  tsem_up(openmaxStandPort->pAllocNFlushSem);
  }
  }
  */
  pthread_mutex_unlock(pMutex);

  return OMX_ErrorNone;
}


OMX_ERRORTYPE base_port_ComponentTunnelRequest(omx_base_PortType* openmaxStandPort,OMX_IN  OMX_HANDLETYPE hTunneledComp,OMX_IN  OMX_U32 nTunneledPort,OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE param;
	OMX_PARAM_BUFFERSUPPLIERTYPE pSupplier;

	omx_base_component_PrivateType* omx_base_component_Private = openmaxStandPort->standCompContainer->pComponentPrivate;

  if (pTunnelSetup == NULL || hTunneledComp == 0) {
	/* cancel previous tunnel */
		openmaxStandPort->hTunneledComponent = 0;
		openmaxStandPort->nTunneledPort = 0;
		openmaxStandPort->nTunnelFlags = 0;
		openmaxStandPort->eBufferSupplier=OMX_BufferSupplyUnspecified;
		return OMX_ErrorNone;
	}

	if (openmaxStandPort->sPortParam.eDir == OMX_DirInput) {
		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		/// \todo insert here a detailed comparison with the OMX_AUDIO_PORTDEFINITIONTYPE
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Port Definition error=0x%08x Line=%d\n",__func__,err,__LINE__);
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}

		openmaxStandPort->nNumTunnelBuffer=param.nBufferCountMin;

		/*Check domain of the tunnelled component*/
    /*
		err = (*(omx_base_component_Private->DomainCheck))(param);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Domain Check Error=%08x\n",err);
			return err;
		}
    */
    if(param.eDomain!=openmaxStandPort->sPortParam.eDomain)
		  return OMX_ErrorPortsNotCompatible;

    if(param.eDomain==OMX_PortDomainAudio) {
      if(param.format.audio.eEncoding == OMX_AUDIO_CodingMax)
		    return OMX_ErrorPortsNotCompatible;
    }
    else if(param.eDomain==OMX_PortDomainVideo) {
      if(param.format.video.eCompressionFormat == OMX_VIDEO_CodingMax)
		    return OMX_ErrorPortsNotCompatible;
    }
		


		/* Get Buffer Supplier type of the Tunnelled Component*/
		pSupplier.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);

		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Buffer Supplier error=0x%08x Line=%d\n",__func__,err,__LINE__);
			return OMX_ErrorPortsNotCompatible;
		}
		else {
			DEBUG(DEB_LEV_FULL_SEQ,"Tunneled Port eBufferSupplier=%x\n",pSupplier.eBufferSupplier);
		}

		// store the current callbacks, if defined
		openmaxStandPort->hTunneledComponent = hTunneledComp;
		openmaxStandPort->nTunneledPort = nTunneledPort;
		openmaxStandPort->nTunnelFlags = 0;
		// Negotiation
		if (pTunnelSetup->nTunnelFlags & OMX_PORTTUNNELFLAG_READONLY) {
			// the buffer provider MUST be the output port provider
			pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
			openmaxStandPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;	
			openmaxStandPort->eBufferSupplier=OMX_BufferSupplyInput;
		} else {
			if (pTunnelSetup->eSupplier == OMX_BufferSupplyInput) {
				openmaxStandPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				openmaxStandPort->eBufferSupplier=OMX_BufferSupplyInput;
			} else if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified) {
				pTunnelSetup->eSupplier = OMX_BufferSupplyInput;
				openmaxStandPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
				openmaxStandPort->eBufferSupplier=OMX_BufferSupplyInput;
			}
		}
		openmaxStandPort->nTunnelFlags |= TUNNEL_ESTABLISHED;

		/* Set Buffer Supplier type of the Tunnelled Component after final negotiation*/
		pSupplier.nPortIndex=nTunneledPort;
		pSupplier.eBufferSupplier=openmaxStandPort->eBufferSupplier;
		err = OMX_SetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &pSupplier);
		if (err != OMX_ErrorNone) {
			// compatibility not reached
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Buffer Supplier error=0x%08x Line=%d\n",__func__,err,__LINE__);
			openmaxStandPort->nTunnelFlags=0;
			return OMX_ErrorPortsNotCompatible;
		}
	} else  {
		// output port
		// all the consistency checks are under other component responsibility

		/* Get Port Definition of the Tunnelled Component*/
		param.nPortIndex=nTunneledPort;
		err = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &param);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"In %s Tunneled Port Definition error=0x%08x Line=%d\n",__func__,err,__LINE__);
			// compatibility not reached
			return OMX_ErrorPortsNotCompatible;
		}
		/*Check domain of the tunnelled component*/
    /*
		err = (*(omx_base_component_Private->DomainCheck))(param);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR,"Domain Check Error=%08x\n",err);
			return err;
		}*/

    if(param.eDomain!=openmaxStandPort->sPortParam.eDomain)
		  return OMX_ErrorPortsNotCompatible;

    if(param.eDomain==OMX_PortDomainAudio) {
      if(param.format.audio.eEncoding == OMX_AUDIO_CodingMax)
		    return OMX_ErrorPortsNotCompatible;
    }
    else if(param.eDomain==OMX_PortDomainVideo) {
      if(param.format.video.eCompressionFormat == OMX_VIDEO_CodingMax)
		    return OMX_ErrorPortsNotCompatible;
    }

		openmaxStandPort->nNumTunnelBuffer=param.nBufferCountMin;

		openmaxStandPort->hTunneledComponent = hTunneledComp;
		openmaxStandPort->nTunneledPort = nTunneledPort;
		pTunnelSetup->eSupplier = OMX_BufferSupplyOutput;
		openmaxStandPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
		openmaxStandPort->nTunnelFlags |= TUNNEL_ESTABLISHED;

		openmaxStandPort->eBufferSupplier=OMX_BufferSupplyOutput;
	}
	return OMX_ErrorNone;
}
