/**
  @file src/base/omx_base_component.c

  OpenMAX base_component component. This component does not perform any multimedia
  processing.  It is used as a base_component for new components development.

  Copyright (C) 2007-2008  STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <OMX_Core.h>
#include <OMX_Component.h>

#include "omxcore.h"
#include "omx_base_component.h"

#include "tsemaphore.h"
#include "queue.h"

/** 
 * @brief The base contructor for the OpenMAX st components
 * 
 * This function is executed by the ST static component loader.
 * It takes care of constructing the instance of the component.
 * For the base_component component, the following is done:
 *
 * 1) Fills the basic OpenMAX structure. The fields can be overwritten
 *    by derived components. 
 * 3) Allocates (if needed) the omx_base_component_PrivateType private structure
 * 
 * @param openmaxStandComp the ST component to be initialized
 * 
 * @return OMX_ErrorInsufficientResources if a memory allocation fails
 */
OMX_ERRORTYPE omx_base_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  omx_base_component_PrivateType* omx_base_component_Private;
  OMX_U32 i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (openmaxStandComp->pComponentPrivate) {
    omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  } else {
    omx_base_component_Private = calloc(1,sizeof(omx_base_component_PrivateType));
    if (!omx_base_component_Private) {
      return OMX_ErrorInsufficientResources;
    }
  }

  if(!omx_base_component_Private->messageQueue) {
    omx_base_component_Private->messageQueue = calloc(1,sizeof(queue_t));
    queue_init(omx_base_component_Private->messageQueue);
  }

  if(!omx_base_component_Private->messageSem) {
    omx_base_component_Private->messageSem = calloc(1,sizeof(tsem_t));
    tsem_init(omx_base_component_Private->messageSem, 0);
  }
  if(!omx_base_component_Private->bMgmtSem) {
    omx_base_component_Private->bMgmtSem = calloc(1,sizeof(tsem_t));
    tsem_init(omx_base_component_Private->bMgmtSem, 0);
  }

  if(!omx_base_component_Private->bStateSem) {
    omx_base_component_Private->bStateSem = calloc(1,sizeof(tsem_t));
    tsem_init(omx_base_component_Private->bStateSem, 0);
  }

  openmaxStandComp->nSize = sizeof(OMX_COMPONENTTYPE);
  openmaxStandComp->pApplicationPrivate = NULL;
  openmaxStandComp->GetComponentVersion = omx_base_component_GetComponentVersion;
  openmaxStandComp->SendCommand = omx_base_component_SendCommand;
  openmaxStandComp->GetParameter = omx_base_component_GetParameter;
  openmaxStandComp->SetParameter = omx_base_component_SetParameter;
  openmaxStandComp->GetConfig = omx_base_component_GetConfig;
  openmaxStandComp->SetConfig = omx_base_component_SetConfig;
  openmaxStandComp->GetExtensionIndex = omx_base_component_GetExtensionIndex;
  openmaxStandComp->GetState = omx_base_component_GetState;
  openmaxStandComp->SetCallbacks = omx_base_component_SetCallbacks;
  openmaxStandComp->ComponentDeInit = omx_base_component_ComponentDeInit;
  openmaxStandComp->ComponentRoleEnum = omx_base_component_ComponentRoleEnum;
  openmaxStandComp->ComponentTunnelRequest =omx_base_component_ComponentTunnelRequest;

  /*Will make Specific port Allocate buffer call*/
  openmaxStandComp->AllocateBuffer = omx_base_component_AllocateBuffer;
  openmaxStandComp->UseBuffer = omx_base_component_UseBuffer;
  openmaxStandComp->UseEGLImage = omx_base_component_UseEGLImage;
  openmaxStandComp->FreeBuffer = omx_base_component_FreeBuffer;
  openmaxStandComp->EmptyThisBuffer = omx_base_component_EmptyThisBuffer;
  openmaxStandComp->FillThisBuffer = omx_base_component_FillThisBuffer;
  
  openmaxStandComp->nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
  openmaxStandComp->nVersion.s.nVersionMinor = SPECVERSIONMINOR;
  openmaxStandComp->nVersion.s.nRevision = SPECREVISION;
  openmaxStandComp->nVersion.s.nStep = SPECSTEP;

  omx_base_component_Private->name = calloc(1,OMX_MAX_STRINGNAME_SIZE);
  if (!omx_base_component_Private->name) {
    return OMX_ErrorInsufficientResources;
  }
  strcpy(omx_base_component_Private->name,cComponentName);
  omx_base_component_Private->state = OMX_StateLoaded;
  omx_base_component_Private->transientState = OMX_TransStateMax;
  omx_base_component_Private->callbacks = NULL;
  omx_base_component_Private->callbackData = NULL;
  omx_base_component_Private->nGroupPriority = 0;
  omx_base_component_Private->nGroupID = 0;
  omx_base_component_Private->pMark=NULL;
  omx_base_component_Private->openmaxStandComp=openmaxStandComp;
  omx_base_component_Private->DoStateSet = &omx_base_component_DoStateSet;
  omx_base_component_Private->messageHandler = omx_base_component_MessageHandler;
  omx_base_component_Private->destructor = omx_base_component_Destructor;
  omx_base_component_Private->bufferMgmtThreadID = -1;

  pthread_mutex_init(&omx_base_component_Private->flush_mutex, NULL);
  pthread_cond_init(&omx_base_component_Private->flush_all_condition, NULL);
  pthread_cond_init(&omx_base_component_Private->flush_condition, NULL);

  for(i=0;i<NUM_DOMAINS;i++) {
    memset(&omx_base_component_Private->sPortTypesParam[i], 0, sizeof(OMX_PORT_PARAM_TYPE));
    setHeader(&omx_base_component_Private->sPortTypesParam[i], sizeof(OMX_PORT_PARAM_TYPE));
  }

  omx_base_component_Private->messageHandlerThreadID = pthread_create(&omx_base_component_Private->messageHandlerThread,
  NULL,
  compMessageHandlerFunction,
  openmaxStandComp);

  DEBUG(DEB_LEV_FUNCTION_NAME,"Out of %s\n",__func__);
  return OMX_ErrorNone;
}

/** @brief The base destructor for ST OpenMAX components
 * 
 * This function is called by the standard function ComponentDeInit()
 * that is called by the IL core during the execution of the  FreeHandle()
 * 
 * @param openmaxStandComp the ST OpenMAX component to be disposed
 */
OMX_ERRORTYPE omx_base_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  int err;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_base_component_Private->state = OMX_StateInvalid;
  omx_base_component_Private->callbacks=NULL;

  /*Send Dummy signal to Component Message handler to exit*/
  tsem_up(omx_base_component_Private->messageSem);

  /*Deinitialize and free message queue*/
  if(omx_base_component_Private->messageQueue) {
    queue_deinit(omx_base_component_Private->messageQueue);
    free(omx_base_component_Private->messageQueue);
    omx_base_component_Private->messageQueue=NULL;
  }
  
  err = pthread_join(omx_base_component_Private->messageHandlerThread,NULL);
  if(err!=0) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s pthread_join returned err=%d\n", __func__, err);
  }
  
  /*Deinitialize and free buffer management semaphore*/
  if(omx_base_component_Private->bMgmtSem){
    tsem_deinit(omx_base_component_Private->bMgmtSem);
    free(omx_base_component_Private->bMgmtSem);
    omx_base_component_Private->bMgmtSem=NULL;
  }

  /*Deinitialize and free message semaphore*/
  if(omx_base_component_Private->messageSem) {
    tsem_deinit(omx_base_component_Private->messageSem);
    free(omx_base_component_Private->messageSem);
    omx_base_component_Private->messageSem=NULL;
  }

  if(omx_base_component_Private->bStateSem){
    tsem_deinit(omx_base_component_Private->bStateSem);
    free(omx_base_component_Private->bStateSem);
    omx_base_component_Private->bStateSem=NULL;
  }

  if(omx_base_component_Private->name){
    free(omx_base_component_Private->name);
    omx_base_component_Private->name=NULL;
  }

  pthread_mutex_destroy(&omx_base_component_Private->flush_mutex);
  pthread_cond_destroy(&omx_base_component_Private->flush_all_condition);
  pthread_cond_destroy(&omx_base_component_Private->flush_condition);

  DEBUG(DEB_LEV_FUNCTION_NAME,"Out of %s\n",__func__);
  return OMX_ErrorNone;
}

/** @brief This standard functionality is called when the component is
 * destroyed in the FreeHandle standard call. 
 * 
 * In this way the implementation of the FreeHandle is standard, 
 * and it does not need a support by a specific component loader.
 * The implementaiton of the ComponentDeInit contains the 
 * implementation specific part of the destroying phase.
 */
OMX_ERRORTYPE omx_base_component_ComponentDeInit(
  OMX_IN  OMX_HANDLETYPE hComponent) {
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s pComponentPrivate =%x \n", __func__,(int)openmaxStandComp->pComponentPrivate);
  omx_base_component_Private->destructor(openmaxStandComp);    

  free(openmaxStandComp->pComponentPrivate);
  openmaxStandComp->pComponentPrivate=NULL;
  return OMX_ErrorNone;
}

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested. This base function cover only the state
 * changes that do not involve any port
 * 
 * @param openmaxStandComp the OpenMAX component which state is to be changed
 * @param destinationState the requested target state
 *
 * @return OMX_ErrorNotImplemented if the state change is noty handled in this base class, but needs
 * a specific handling
 */
OMX_ERRORTYPE omx_base_component_DoStateSet(OMX_COMPONENTTYPE *openmaxStandComp, OMX_U32 destinationState) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_PortType *pPort;
  OMX_U32 i,j,k;
  OMX_ERRORTYPE err=OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  DEBUG(DEB_LEV_PARAMS, "Changing state from %i to %i\n", omx_base_component_Private->state, (int)destinationState);

  if(destinationState == OMX_StateLoaded){
    switch(omx_base_component_Private->state){
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    case OMX_StateWaitForResources:
      /* return back from wait for resources */
      omx_base_component_Private->state = OMX_StateLoaded;
      break;
    case OMX_StateLoaded:
      err = OMX_ErrorSameState;
      break;
    case OMX_StateIdle:
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
            omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          
          pPort = omx_base_component_Private->ports[i];
          if (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort)) {
            while(pPort->pBufferQueue->nelem > 0) {
              DEBUG(DEB_LEV_PARAMS, "In %s Buffer %d remained in the port %d queue of comp%s\n",
                   __func__,(int)pPort->pBufferQueue->nelem,(int)i,omx_base_component_Private->name);
              dequeue(pPort->pBufferQueue);
            }
            /* Freeing here the buffers allocated for the tunneling:*/
            err = pPort->Port_FreeTunnelBuffer(pPort,i);
            if(err!=OMX_ErrorNone) { 
              DEBUG(DEB_LEV_ERR, "In %s Freeing Tunnel Buffer Error=%x\n",__func__,err); 
              return err; 
            } 
          } else {
            DEBUG(DEB_LEV_FULL_SEQ, "In %s nPortIndex=%d pAllocSem Semval=%x\n", __func__,(int)i,(int)pPort->pAllocSem->semval);

            /*If ports are enabled then wait till all buffers are freed*/
            if(PORT_IS_ENABLED(pPort)) {
              tsem_down(pPort->pAllocSem);
            }
          }
          pPort->sPortParam.bPopulated = OMX_FALSE;

          if(pPort->pInternalBufferStorage != NULL) {
            free(pPort->pInternalBufferStorage);
            pPort->pInternalBufferStorage=NULL;
          }
          
          if(pPort->bBufferStateAllocated != NULL) {
            free(pPort->bBufferStateAllocated);
            pPort->bBufferStateAllocated=NULL;
          }
        }
      }
      omx_base_component_Private->state = OMX_StateLoaded;

      if(omx_base_component_Private->bufferMgmtThreadID == 0 ){
        /*Signal Buffer Management thread to exit*/
        tsem_up(omx_base_component_Private->bMgmtSem);
        pthread_join(omx_base_component_Private->bufferMgmtThread,NULL);
        omx_base_component_Private->bufferMgmtThreadID = -1;
        if(err != 0) {
          DEBUG(DEB_LEV_FUNCTION_NAME,"In %s pthread_join returned err=%d\n",__func__,err);
        }
      }
      
      break;
    default:
      DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
      err = OMX_ErrorIncorrectStateTransition;
      break;
    }
    return err;
  }

  if(destinationState == OMX_StateWaitForResources){
    switch(omx_base_component_Private->state){
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    case OMX_StateLoaded:
      omx_base_component_Private->state = OMX_StateWaitForResources;
      break;
    case OMX_StateWaitForResources:
      err = OMX_ErrorSameState;
      break;
    default:
      DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
      err = OMX_ErrorIncorrectStateTransition;
      break;
    }
    return err;
  }

  if(destinationState == OMX_StateIdle){
    switch(omx_base_component_Private->state){
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    case OMX_StateWaitForResources:
      omx_base_component_Private->state = OMX_StateIdle;
      break;
    case OMX_StateLoaded:
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
        pPort = omx_base_component_Private->ports[i];
        if (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort)) {
          if(PORT_IS_ENABLED(pPort)) {
            /** Allocate here the buffers needed for the tunneling */
            err= pPort->Port_AllocateTunnelBuffer(pPort, i, omx_base_component_Private->ports[i]->sPortParam.nBufferSize);
            if(err!=OMX_ErrorNone) { 
              DEBUG(DEB_LEV_ERR, "In %s Allocating Tunnel Buffer Error=%x\n",__func__,err); 
              return err; 
            } 
          }
        } else {
          if(PORT_IS_ENABLED(pPort)) {
            DEBUG(DEB_LEV_FULL_SEQ, "In %s: wait for buffers. port enabled %i,  port populated %i\n", 
              __func__, pPort->sPortParam.bEnabled,pPort->sPortParam.bPopulated);
            tsem_down(pPort->pAllocSem);
            pPort->sPortParam.bPopulated = OMX_TRUE;
          }
          else 
            DEBUG(DEB_LEV_ERR, "In %s: Port %i Disabled So no wait\n",__func__,(int)i);
        }
        DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Tunnel status : port %d flags  0x%x\n",(int)i, (int)pPort->nTunnelFlags);
        }
      }
      omx_base_component_Private->state = OMX_StateIdle;
      /** starting buffer management thread */
      omx_base_component_Private->bufferMgmtThreadID = pthread_create(&omx_base_component_Private->bufferMgmtThread,
      NULL,
      omx_base_component_Private->BufferMgmtFunction,
      openmaxStandComp);  
      if(omx_base_component_Private->bufferMgmtThreadID < 0){
        DEBUG(DEB_LEV_ERR, "Starting buffer management thread failed\n");
        return OMX_ErrorUndefined;
      }  
      break;
    case OMX_StateIdle:
      err = OMX_ErrorSameState;
      break;
    case OMX_StateExecuting:
      /*Flush Ports*/
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
        i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
        omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          DEBUG(DEB_LEV_FULL_SEQ, "Flushing Port %i\n",(int)i);
          pPort = omx_base_component_Private->ports[i];
          if(PORT_IS_ENABLED(pPort)) {
            pPort->FlushProcessingBuffers(pPort);
          }
        }
      }
      omx_base_component_Private->state = OMX_StateIdle;
      break;
      case OMX_StatePause:
      omx_base_component_Private->state = OMX_StateIdle;
      /*Signal buffer management thread if waiting at paused state*/
      tsem_signal(omx_base_component_Private->bStateSem);
      break;
    default:
      DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
      err = OMX_ErrorIncorrectStateTransition;
      break;
    }
    return err;
  }

  if(destinationState == OMX_StatePause) {
  switch(omx_base_component_Private->state) {
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    case OMX_StatePause:
      err = OMX_ErrorSameState;
      break;
    case OMX_StateExecuting:
    case OMX_StateIdle:
      omx_base_component_Private->state = OMX_StatePause;
      break;
    default:
      DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
      err = OMX_ErrorIncorrectStateTransition;
      break;
    }
    return err;
  }

  if(destinationState == OMX_StateExecuting) {
    switch(omx_base_component_Private->state) {
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    case OMX_StateIdle:
      omx_base_component_Private->state=OMX_StateExecuting;
      /*Send Tunneled Buffer to the Neighbouring Components*/
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
        i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
          omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          pPort = omx_base_component_Private->ports[i];
          if (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort)) {
            for(k=0;k<pPort->nNumTunnelBuffer;k++) {
              tsem_up(pPort->pBufferSem);
              /*signal buffer management thread availability of buffers*/
              tsem_up(omx_base_component_Private->bMgmtSem);
            }
          }
        }
      }
      omx_base_component_Private->transientState = OMX_TransStateMax;
      err = OMX_ErrorNone;
      break;
    case OMX_StatePause:
      omx_base_component_Private->state=OMX_StateExecuting;

      /* Tunneled Supplier Ports were enabled in paused state. So signal buffer managment thread*/
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
          omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {

          pPort=omx_base_component_Private->ports[i];
          DEBUG(DEB_LEV_PARAMS, "In %s: state transition Paused 2 Executing, nelem=%d,semval=%d,Buf Count Actual=%d\n", __func__,
            pPort->pBufferQueue->nelem,pPort->pBufferSem->semval,(int)pPort->sPortParam.nBufferCountActual);

          if (PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(pPort) && 
            (pPort->pBufferQueue->nelem == (pPort->pBufferSem->semval + pPort->sPortParam.nBufferCountActual))) {
            for(k=0; k < pPort->sPortParam.nBufferCountActual;k++) {
              tsem_up(pPort->pBufferSem);
              tsem_up(omx_base_component_Private->bMgmtSem);
            }
          }
        }
      }
      /*Signal buffer management thread if waiting at paused state*/
      tsem_signal(omx_base_component_Private->bStateSem);
      break;
    case OMX_StateExecuting:
      err = OMX_ErrorSameState;
      break;
    default:
      DEBUG(DEB_LEV_ERR, "In %s: state transition not allowed\n", __func__);
      err = OMX_ErrorIncorrectStateTransition;
      break;
    }
    return err;
  }

  if(destinationState == OMX_StateInvalid) {
    switch(omx_base_component_Private->state) {
    case OMX_StateInvalid:
      err = OMX_ErrorInvalidState;
      break;
    default:
      omx_base_component_Private->state = OMX_StateInvalid;
      
      if(omx_base_component_Private->bufferMgmtThreadID == 0 ){
        /*Signal Buffer Management Thread to Exit*/
        tsem_up(omx_base_component_Private->bMgmtSem);
        pthread_join(omx_base_component_Private->bufferMgmtThread,NULL);
        omx_base_component_Private->bufferMgmtThreadID = -1;
        if(err!=0) {
          DEBUG(DEB_LEV_FUNCTION_NAME,"In %s pthread_join returned err=%d\n",__func__,err);
        }
      }
      err = OMX_ErrorInvalidState;
      break;
    }
    return err;
  }
  return OMX_ErrorNone;
}

/** @brief Checks the header of a structure for consistency 
 * with size and spec version
 * 
 * @param header Pointer to the structure to be checked
 * @param size Size of the structure. it is in general obtained
 * with a sizeof call applied to the structure
 * 
 * @return OMX error code. If the header has failed the check, 
 * OMX_ErrorVersionMismatch is returned. 
 * If the header fails the size check OMX_ErrorBadParameter is returned
 */
OMX_ERRORTYPE checkHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver;
  if (header == NULL) {
    DEBUG(DEB_LEV_ERR, "In %s the header is null\n",__func__);
    return OMX_ErrorBadParameter;
  }
  ver = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
  if(*((OMX_U32*)header) != size) {
    DEBUG(DEB_LEV_ERR, "In %s the header has a wrong size %i should be %i\n",__func__,(int)*((OMX_U32*)header),(int)size);
    return OMX_ErrorBadParameter;
  }
  if(ver->s.nVersionMajor != SPECVERSIONMAJOR ||
    ver->s.nVersionMinor != SPECVERSIONMINOR) {
    DEBUG(DEB_LEV_ERR, "The version does not match\n");
    return OMX_ErrorVersionMismatch;
  }
  return OMX_ErrorNone;
}

/** @brief Simply fills the first two fields in any OMX structure
 * with the size and the version
 * 
 * @param header pointer to the structure to be filled
 * @param size size of the structure. It can be obtained with 
 * a call to sizeof of the structure type 
 */
void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = SPECVERSIONMAJOR;
  ver->s.nVersionMinor = SPECVERSIONMINOR;
  ver->s.nRevision = SPECREVISION;
  ver->s.nStep = SPECSTEP;
}

/**
 * This function verify Component State and Structure header
 */
OMX_ERRORTYPE omx_base_component_ParameterSanityCheck(OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_U32 nPortIndex,
  OMX_IN  OMX_PTR pStructure,
  OMX_IN  size_t size) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
  omx_base_PortType *pPort;
  int nNumPorts;

  nNumPorts = omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
              omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
              omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
              omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts;

  if (nPortIndex >= nNumPorts) {
    DEBUG(DEB_LEV_ERR, "Bad Port index %i when the component has %i ports\n", (int)nPortIndex, (int)nNumPorts);
    return OMX_ErrorBadPortIndex;
  }

  pPort = omx_base_component_Private->ports[nPortIndex];

  if (omx_base_component_Private->state != OMX_StateLoaded && omx_base_component_Private->state != OMX_StateWaitForResources) {
    if(PORT_IS_ENABLED(pPort)) {
      DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_base_component_Private->state,__LINE__);
      return OMX_ErrorIncorrectStateOperation;
    }
  }
  
  return checkHeader(pStructure , size);
} 

/** @brief Standard OpenMAX function
 * 
 * it returns the version of the component. See OMX_Core.h
 */ 
OMX_ERRORTYPE omx_base_component_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_STRING pComponentName,
  OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
  OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
  OMX_OUT OMX_UUIDTYPE* pComponentUUID) {

  OMX_COMPONENTTYPE* omx_component = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omx_component->pComponentPrivate;

  OMX_U32 uuid[3];

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  /* Fill component name */
  strcpy(pComponentName, omx_base_component_Private->name);

  /* Fill component version */
  pComponentVersion->s.nVersionMajor = SPECVERSIONMAJOR;
  pComponentVersion->s.nVersionMinor = SPECVERSIONMINOR;
  pComponentVersion->s.nRevision = SPECREVISION;
  pComponentVersion->s.nStep = SPECSTEP;

  /* Fill spec version (copy from component field) */
  memcpy(pSpecVersion, &omx_component->nVersion, sizeof(OMX_VERSIONTYPE));

  /* Fill UUID with handle address, PID and UID.
   * This should guarantee uiniqness */
  uuid[0] = (OMX_U32)omx_component;
  uuid[1] = getpid();
  uuid[2] = getuid();
  memcpy(*pComponentUUID, uuid, 3*sizeof(uuid));

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
  return OMX_ErrorNone;
}

/** @brief Enumerates all the roles of the component.
 * 
 * This function is intended to be used only by a core. The ST static core 
 * in any case does not use this function, because it can not be used before the 
 * creation of the component, but uses a static list.
 * It is implemented only for API completion,and it will be not overriden 
 * by a derived component 
 */ 
OMX_ERRORTYPE omx_base_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {
  strcat((char*)cRole, "\0");
  return OMX_ErrorNoMore;
}

/** @brief standard OpenMAX function
 * 
 * it sets the callback functions given by the IL client. 
 * See OMX_Component.h
 */ 
OMX_ERRORTYPE omx_base_component_SetCallbacks(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
  OMX_IN  OMX_PTR pAppData) {
  
  OMX_COMPONENTTYPE *omxcomponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxcomponent->pComponentPrivate;
  omx_base_PortType *pPort;
  OMX_U32 i,j;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  omx_base_component_Private->callbacks = pCallbacks;
  omx_base_component_Private->callbackData = pAppData;  

  /* for all ports */
  for(j = 0; j < NUM_DOMAINS; j++) {
    for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
    i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
      omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
      pPort = omx_base_component_Private->ports[i];
      if (pPort->sPortParam.eDir == OMX_DirInput) {
        pPort->BufferProcessedCallback = omx_base_component_Private->callbacks->EmptyBufferDone;
      } else {
        pPort->BufferProcessedCallback = omx_base_component_Private->callbacks->FillBufferDone;
      }
    }
  }
  return OMX_ErrorNone;
}

/** @brief Part of the standard OpenMAX function
 * 
 * This function return the parameters not related to any port.
 * These parameters are handled in the derived components
 * See OMX_Core.h for standard reference
 * 
 * @return OMX_ErrorUnsupportedIndex if the index is not supported or not handled here
 */
OMX_ERRORTYPE omx_base_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {
  
  OMX_COMPONENTTYPE *omxcomponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxcomponent->pComponentPrivate;
  OMX_PRIORITYMGMTTYPE* pPrioMgmt;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
  OMX_PARAM_BUFFERSUPPLIERTYPE *pBufferSupplier;
  omx_base_PortType *pPort;
  OMX_PORT_PARAM_TYPE* pPortDomains;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  DEBUG(DEB_LEV_PARAMS, "Getting parameter %i\n", nParamIndex);
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
  case OMX_IndexParamVideoInit:
  case OMX_IndexParamImageInit:
  case OMX_IndexParamOtherInit:
    pPortDomains = (OMX_PORT_PARAM_TYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    pPortDomains->nPorts = 0;
    pPortDomains->nStartPortNumber = 0;
    break;    
  case OMX_IndexParamPortDefinition:
    pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_PORTDEFINITIONTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pPortDef->nPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                 omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                 omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                                 omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
      return OMX_ErrorBadPortIndex;
    }

    memcpy(pPortDef, &omx_base_component_Private->ports[pPortDef->nPortIndex]->sPortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    break;
  case OMX_IndexParamPriorityMgmt:
    pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PRIORITYMGMTTYPE))) != OMX_ErrorNone) { 
      break;
    }
    pPrioMgmt->nGroupPriority = omx_base_component_Private->nGroupPriority;
    pPrioMgmt->nGroupID = omx_base_component_Private->nGroupID;
    break;
  case OMX_IndexParamCompBufferSupplier:
    pBufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pBufferSupplier->nPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                        omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                        omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                                        omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
      return OMX_ErrorBadPortIndex;
    }

    pPort = omx_base_component_Private->ports[pBufferSupplier->nPortIndex];

    if (pPort->sPortParam.eDir == OMX_DirInput) {
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;  
      } else if (PORT_IS_TUNNELED(pPort)) {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;  
      } else {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;  
      }
    } else {
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;  
      } else if (PORT_IS_TUNNELED(pPort)) {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;  
      } else {
        pBufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;  
      }
    }
    break;
  default:
    err = OMX_ErrorUnsupportedIndex;
    break;
  }
  return err;
}

/** @brief Part of the standard OpenMAX function
 * 
 * This function sets the parameters not related to any port.
 * These parameters are handled in the derived components
 * See OMX_Core.h for standard reference
 * 
 * @return OMX_ErrorUnsupportedIndex if the index is not supported or not handled here
 */
OMX_ERRORTYPE omx_base_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure) {
    
  OMX_PRIORITYMGMTTYPE* pPrioMgmt;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *omxcomponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxcomponent->pComponentPrivate;
  OMX_PARAM_BUFFERSUPPLIERTYPE *pBufferSupplier;
  omx_base_PortType *pPort;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  DEBUG(DEB_LEV_PARAMS, "Setting parameter %i\n", nParamIndex);
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
 
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
  case OMX_IndexParamVideoInit:
  case OMX_IndexParamImageInit:
  case OMX_IndexParamOtherInit:
    /* pPortParam  = (OMX_PORT_PARAM_TYPE* ) ComponentParameterStructure;*/
    if (omx_base_component_Private->state != OMX_StateLoaded && 
      omx_base_component_Private->state != OMX_StateWaitForResources) {
      return OMX_ErrorIncorrectStateOperation;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    err = OMX_ErrorUndefined;
    break;  
  case OMX_IndexParamPortDefinition: 
    pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
    err = omx_base_component_ParameterSanityCheck(hComponent, pPortDef->nPortIndex, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    } 
    {
      OMX_PARAM_PORTDEFINITIONTYPE *pPortParam;
      OMX_U32 j,old_nBufferCountActual=0;
      pPortParam = &omx_base_component_Private->ports[pPortDef->nPortIndex]->sPortParam;
      if( pPortDef->nBufferCountActual < pPortParam->nBufferCountMin) {
        return OMX_ErrorBadParameter;
      }
      old_nBufferCountActual         = pPortParam->nBufferCountActual;
      pPortParam->nBufferCountActual = pPortDef->nBufferCountActual;

      switch(pPortDef->eDomain) {
      case OMX_PortDomainAudio:
        memcpy(&pPortParam->format.audio, &pPortDef->format.audio, sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));
        break;
      case OMX_PortDomainVideo:
        pPortParam->format.video.pNativeRender          = pPortDef->format.video.pNativeRender;
        pPortParam->format.video.nFrameWidth            = pPortDef->format.video.nFrameWidth;
        pPortParam->format.video.nFrameHeight           = pPortDef->format.video.nFrameHeight;
        pPortParam->format.video.nStride                = pPortDef->format.video.nStride;
        pPortParam->format.video.xFramerate             = pPortDef->format.video.xFramerate;
        pPortParam->format.video.bFlagErrorConcealment  = pPortDef->format.video.bFlagErrorConcealment;
        pPortParam->format.video.eCompressionFormat     = pPortDef->format.video.eCompressionFormat;
        pPortParam->format.video.eColorFormat           = pPortDef->format.video.eColorFormat;
        pPortParam->format.video.pNativeWindow          = pPortDef->format.video.pNativeWindow;
        break;
      case OMX_PortDomainImage:
        pPortParam->format.image.nFrameWidth            = pPortDef->format.image.nFrameWidth;
        pPortParam->format.image.nFrameHeight           = pPortDef->format.image.nFrameHeight;
        pPortParam->format.image.nStride                = pPortDef->format.image.nStride;
        pPortParam->format.image.bFlagErrorConcealment  = pPortDef->format.image.bFlagErrorConcealment;
        pPortParam->format.image.eCompressionFormat     = pPortDef->format.image.eCompressionFormat;
        pPortParam->format.image.eColorFormat           = pPortDef->format.image.eColorFormat;
        pPortParam->format.image.pNativeWindow          = pPortDef->format.image.pNativeWindow;
        break;
      case OMX_PortDomainOther:
        memcpy(&pPortParam->format.other, &pPortDef->format.other, sizeof(OMX_OTHER_PORTDEFINITIONTYPE));
        break;
      default:
        err = OMX_ErrorBadParameter;
        break;
      }
      
      /*If component state Idle/Pause/Executing and re-alloc the following private variables */
      if ((omx_base_component_Private->state == OMX_StateIdle || 
        omx_base_component_Private->state == OMX_StatePause  || 
        omx_base_component_Private->state == OMX_StateExecuting) && 
        (pPortParam->nBufferCountActual > old_nBufferCountActual)) {

        pPort = omx_base_component_Private->ports[pPortDef->nPortIndex];
        if(pPort->pInternalBufferStorage) {
          pPort->pInternalBufferStorage = realloc(pPort->pInternalBufferStorage,pPort->sPortParam.nBufferCountActual*sizeof(OMX_BUFFERHEADERTYPE *));
        }

        if(pPort->bBufferStateAllocated) {
          pPort->bBufferStateAllocated = realloc(pPort->bBufferStateAllocated,pPort->sPortParam.nBufferCountActual*sizeof(BUFFER_STATUS_FLAG));
          for(j=0; j < pPort->sPortParam.nBufferCountActual; j++) {
            pPort->bBufferStateAllocated[j] = BUFFER_FREE;
          }
        }
      }
    }
    break;
  case OMX_IndexParamPriorityMgmt:
    if (omx_base_component_Private->state != OMX_StateLoaded && 
      omx_base_component_Private->state != OMX_StateWaitForResources) {
      return OMX_ErrorIncorrectStateOperation;
    }
    pPrioMgmt = (OMX_PRIORITYMGMTTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PRIORITYMGMTTYPE))) != OMX_ErrorNone) { 
      break;
    }
    omx_base_component_Private->nGroupPriority = pPrioMgmt->nGroupPriority;
    omx_base_component_Private->nGroupID = pPrioMgmt->nGroupID;
    break;
  case OMX_IndexParamCompBufferSupplier:
    pBufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;
    
    DEBUG(DEB_LEV_PARAMS, "In %s Buf Sup Port index=%d\n", __func__,(int)pBufferSupplier->nPortIndex);

    if(pBufferSupplier == NULL) {
      return OMX_ErrorBadParameter;
    }
    if(pBufferSupplier->nPortIndex > (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                      omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                      omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                                      omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
      return OMX_ErrorBadPortIndex;
    }
    err = omx_base_component_ParameterSanityCheck(hComponent, pBufferSupplier->nPortIndex, pBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
    if(err==OMX_ErrorIncorrectStateOperation) {
      if (PORT_IS_ENABLED(omx_base_component_Private->ports[pBufferSupplier->nPortIndex])) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Incorrect State=%x\n",__func__,omx_base_component_Private->state);
        return OMX_ErrorIncorrectStateOperation;
      }
    } else if (err != OMX_ErrorNone) {
      break;
    }

    if (pBufferSupplier->eBufferSupplier == OMX_BufferSupplyUnspecified) {
      DEBUG(DEB_LEV_PARAMS, "In %s: port is already buffer supplier unspecified\n", __func__);
      return OMX_ErrorNone;
    }
    if ((PORT_IS_TUNNELED(omx_base_component_Private->ports[pBufferSupplier->nPortIndex])) == 0) {
      return OMX_ErrorNone;
    }

    pPort = omx_base_component_Private->ports[pBufferSupplier->nPortIndex];

    if ((pBufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) && 
        (pPort->sPortParam.eDir == OMX_DirInput)) {
      /** These two cases regard the first stage of client override */
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        err = OMX_ErrorNone;
      }
      pPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
      pBufferSupplier->nPortIndex = pPort->nTunneledPort;
      err = OMX_SetParameter(pPort->hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufferSupplier);
    } else if ((pBufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) && 
               (pPort->sPortParam.eDir == OMX_DirInput)) {
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        pPort->nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
        pBufferSupplier->nPortIndex = pPort->nTunneledPort;
        err = OMX_SetParameter(pPort->hTunneledComponent, OMX_IndexParamCompBufferSupplier, pBufferSupplier);
      }
      err = OMX_ErrorNone;
    } else if ((pBufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) && 
               (pPort->sPortParam.eDir == OMX_DirOutput)) {
      /** these two cases regard the second stage of client override */
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        err = OMX_ErrorNone;
      }
      pPort->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
    } else {
      if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
        pPort->nTunnelFlags &= ~TUNNEL_IS_SUPPLIER;
        err = OMX_ErrorNone;
      }
      err = OMX_ErrorNone;
    }
    DEBUG(DEB_LEV_PARAMS, "In %s port %d Tunnel flag=%x \n", __func__,(int)pBufferSupplier->nPortIndex, (int)pPort->nTunnelFlags);
    break;
  default:
    err = OMX_ErrorUnsupportedIndex;
    break;
  }
  return err;
}

/** @brief base GetConfig function
 * 
 * This base function is not implemented. If a derived component
 * needs to support any config, it must implement a derived 
 * version of this function and assign it to the correct pointer
 * in the private component descriptor
 */
OMX_ERRORTYPE omx_base_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure) {
  return OMX_ErrorNone;
}

/** @brief base SetConfig function
 * 
 * This base function is not implemented. If a derived component
 * needs to support any config, it must implement a derived 
 * version of this function and assign it to the correct pointer
 * in the private component descriptor
 */
OMX_ERRORTYPE omx_base_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure) {
  return OMX_ErrorNone;
}

/** @brief base function not implemented
 * 
 * This function can be eventually implemented by a
 * derived component if needed
 */
OMX_ERRORTYPE omx_base_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {    

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  return OMX_ErrorBadParameter;  
}

/** @return the state of the component
 * 
 * This function does not need any override by derived components
 */
OMX_ERRORTYPE omx_base_component_GetState(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_STATETYPE* pState) {
  OMX_COMPONENTTYPE *omxcomponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxcomponent->pComponentPrivate;
  *pState = omx_base_component_Private->state;
  return OMX_ErrorNone;
}

/** @brief standard SendCommand function
 * 
 * In general this function does not need a overwrite, but
 * a special derived component could do it.
 */  
OMX_ERRORTYPE omx_base_component_SendCommand(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_COMMANDTYPE Cmd,
  OMX_IN  OMX_U32 nParam,
  OMX_IN  OMX_PTR pCmdData) {
  OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE*)hComponent;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  internalRequestMessageType *message;
  queue_t* messageQueue;
  tsem_t* messageSem;
  OMX_U32 i,j,k;
  omx_base_PortType *pPort;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  messageQueue = omx_base_component_Private->messageQueue;
  messageSem = omx_base_component_Private->messageSem;

  if (omx_base_component_Private->state == OMX_StateInvalid) {
    return OMX_ErrorInvalidState;
  }
  
  message = calloc(1,sizeof(internalRequestMessageType));
  message->messageParam = nParam;
  message->pCmdData=pCmdData;
  /** Fill in the message */
  switch (Cmd) {
  case OMX_CommandStateSet:
    message->messageType = OMX_CommandStateSet;
    if ((nParam == OMX_StateIdle) && (omx_base_component_Private->state == OMX_StateLoaded)) {
      /*Allocate Internal Buffer Storage and Buffer Allocation State flags*/
       /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
              
          pPort = omx_base_component_Private->ports[i];
          
          if(pPort->pInternalBufferStorage == NULL) {
            pPort->pInternalBufferStorage = calloc(pPort->sPortParam.nBufferCountActual,sizeof(OMX_BUFFERHEADERTYPE *));
          }           

          if(pPort->bBufferStateAllocated == NULL) {
            pPort->bBufferStateAllocated = calloc(pPort->sPortParam.nBufferCountActual,sizeof(BUFFER_STATUS_FLAG));
          } 
          
          for(k=0; k < pPort->sPortParam.nBufferCountActual; k++) {
            pPort->bBufferStateAllocated[k] = BUFFER_FREE;
          }
        }
      }

      omx_base_component_Private->transientState = OMX_TransStateLoadedToIdle;
    } else if ((nParam == OMX_StateLoaded) && (omx_base_component_Private->state == OMX_StateIdle)) {
      omx_base_component_Private->transientState = OMX_TransStateIdleToLoaded;
    } else if ((nParam == OMX_StateIdle) && (omx_base_component_Private->state == OMX_StateExecuting)) {
      omx_base_component_Private->transientState = OMX_TransStateExecutingToIdle;
    }    
    break;
  case OMX_CommandFlush:
    if (nParam >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    message->messageType = OMX_CommandFlush;
    break;
  case OMX_CommandPortDisable:
    if (nParam >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    message->messageType = OMX_CommandPortDisable;
    if(message->messageParam == OMX_ALL) {
       /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          omx_base_component_Private->ports[i]->bIsTransientToDisabled = OMX_TRUE;
        }
      }
    } else {
      omx_base_component_Private->ports[message->messageParam]->bIsTransientToDisabled = OMX_TRUE;
    }
    break;
  case OMX_CommandPortEnable:
    if (nParam >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    message->messageType = OMX_CommandPortEnable;
    if(message->messageParam == OMX_ALL) {
       /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          omx_base_component_Private->ports[i]->bIsTransientToEnabled = OMX_TRUE;
        }
      }
    } else {
      omx_base_component_Private->ports[message->messageParam]->bIsTransientToEnabled = OMX_TRUE;
    }
    break;
  case OMX_CommandMarkBuffer:
    if (nParam >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts) && nParam != OMX_ALL) {
      return OMX_ErrorBadPortIndex;
    }
    message->messageType = OMX_CommandMarkBuffer;
    break;
  default:
    err = OMX_ErrorUnsupportedIndex;
    break;
  }

  if (err == OMX_ErrorNone)
  {
    queue(messageQueue, message);
    tsem_up(messageSem);
  }

  DEBUG(DEB_LEV_FULL_SEQ, "In %s messageSem up param=%d\n", __func__,message->messageParam);

  return err;
}

/** @brief Component's message handler thread function
 * 
 * Handles all messages coming from components and
 * processes them by dispatching them back to the
 * triggering component.
 */
void* compMessageHandlerFunction(void* param) {
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)param;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  internalRequestMessageType *message;

  while(1){
    /* Wait for an incoming message */
    if (omx_base_component_Private == NULL) {
      break;
    }
    tsem_down(omx_base_component_Private->messageSem);
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
    /*Destructor has been called. So exit from the loop*/
    if(omx_base_component_Private->state == OMX_StateInvalid) {
      break;
    }
    /* Dequeue it */
    message = dequeue(omx_base_component_Private->messageQueue);
    if(message == NULL){
      DEBUG(DEB_LEV_ERR, "In %s: ouch!! had null message!\n", __func__);
      break;
    }
    /* Process it by calling component's message handler method */
    omx_base_component_Private->messageHandler(openmaxStandComp,message);
    /* Message ownership has been transferred to us
    * so we gonna free it when finished.
    */
    free(message);
    message = NULL;
  }
  DEBUG(DEB_LEV_FUNCTION_NAME,"Exiting Message Handler thread\n");
  return NULL;
}

/** This is called by the component message entry point. 
 * In thea base version this function is named compMessageHandlerFunction
 * 
 * A request is made by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * 3) ...
 *
 * @param openmaxStandComp the component itself
 * @param message the message that has been passed to core
 */
OMX_ERRORTYPE omx_base_component_MessageHandler(OMX_COMPONENTTYPE *openmaxStandComp,internalRequestMessageType* message) {
  omx_base_component_PrivateType* omx_base_component_Private=openmaxStandComp->pComponentPrivate;
  OMX_U32                         i,j,k;
  OMX_ERRORTYPE                   err = OMX_ErrorNone;
  omx_base_PortType*              pPort;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  /* Dealing with a SendCommand call.
  * -messageType contains the command to execute
  * -messageParam contains the parameter of the command
  *  (destination state in case of a state change command).
  */
  switch(message->messageType){
  case OMX_CommandStateSet: {
    /* Do the actual state change */
    err = (*(omx_base_component_Private->DoStateSet))(openmaxStandComp, message->messageParam);        
    if (err != OMX_ErrorNone) {
      (*(omx_base_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_base_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      err, /* The commands was a OMX_CommandStateSet */
      0, /* The state has been changed in message->messageParam */
      NULL);
    } else {
      /* And run the callback */
      DEBUG(DEB_LEV_FULL_SEQ,"running callback in %s\n",__func__);
      (*(omx_base_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_base_component_Private->callbackData,
      OMX_EventCmdComplete, /* The command was completed */
      OMX_CommandStateSet, /* The commands was a OMX_CommandStateSet */
      message->messageParam, /* The state has been changed in message->messageParam */
      NULL);
    }
  }
  break;
  case OMX_CommandFlush: {
    /*Flush port/s*/
    if(message->messageParam == OMX_ALL) {
       /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          omx_base_component_Private->ports[i]->bIsPortFlushed = OMX_TRUE;
        }
      }
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
        i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
          omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          pPort=omx_base_component_Private->ports[i];
          err = pPort->FlushProcessingBuffers(pPort);
        }
      }
    }
    else {
      pPort=omx_base_component_Private->ports[message->messageParam];
      err = pPort->FlushProcessingBuffers(pPort);
    }
    if (err != OMX_ErrorNone) {
      (*(omx_base_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_base_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      err, /* The commands was a OMX_CommandStateSet */
      0, /* The state has been changed in message->messageParam */
      NULL);
    } else {
      if(message->messageParam == OMX_ALL){ /*Flush all port*/
        /* for all ports */
        for(j = 0; j < NUM_DOMAINS; j++) {
          for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
            omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
            (*(omx_base_component_Private->callbacks->EventHandler))
            (openmaxStandComp,
            omx_base_component_Private->callbackData,
            OMX_EventCmdComplete, /* The command was completed */
            OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
            i, /* The state has been changed in message->messageParam */
            NULL);

            pPort=omx_base_component_Private->ports[i];
            /* Signal the buffer Semaphore and the buffer managment semaphore, to restart the exchange of buffers after flush */
            if (PORT_IS_TUNNELED(pPort) && PORT_IS_BUFFER_SUPPLIER(pPort)) {
              for(k=0;k<pPort->nNumTunnelBuffer;k++) {
                tsem_up(pPort->pBufferSem);
                /*signal buffer management thread availability of buffers*/
                tsem_up(omx_base_component_Private->bMgmtSem);
              }
            }
          }		
        }
      } else {/*Flush input/output port*/
        (*(omx_base_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_base_component_Private->callbackData,
        OMX_EventCmdComplete, /* The command was completed */
        OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
        message->messageParam, /* The state has been changed in message->messageParam */
        NULL);
        /* Signal the buffer Semaphore and the buffer managment semaphore, to restart the exchange of buffers after flush */
        if (PORT_IS_TUNNELED(omx_base_component_Private->ports[message->messageParam]) 
             && PORT_IS_BUFFER_SUPPLIER(omx_base_component_Private->ports[message->messageParam])) {
            for(j=0;j<omx_base_component_Private->ports[message->messageParam]->nNumTunnelBuffer;j++) {
              tsem_up(omx_base_component_Private->ports[message->messageParam]->pBufferSem);
              /*signal buffer management thread availability of buffers*/
              tsem_up(omx_base_component_Private->bMgmtSem);
            }
          }
      }
    }
  }
  break;
  case OMX_CommandPortDisable: {
    /*Flush port/s*/
    if(message->messageParam == OMX_ALL) {
      /*If Component is not in loaded state,then First Flush all buffers then disable the port*/
      if(omx_base_component_Private->state!=OMX_StateLoaded) {
        /* for all ports */
        for(j = 0; j < NUM_DOMAINS; j++) {
          for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
            omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          pPort=omx_base_component_Private->ports[i];
          err = pPort->FlushProcessingBuffers(pPort);
          }
        }
      }
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
        i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
          omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          pPort=omx_base_component_Private->ports[i];
          err = pPort->Port_DisablePort(pPort);
        }
      }
    }
    else {
      pPort=omx_base_component_Private->ports[message->messageParam];
      if(omx_base_component_Private->state!=OMX_StateLoaded) {
        err = pPort->FlushProcessingBuffers(pPort);
        DEBUG(DEB_LEV_FULL_SEQ, "In %s: Port Flush completed for Comp %s\n",__func__,omx_base_component_Private->name);
      }
      err = pPort->Port_DisablePort(pPort);
    }
    /** This condition is added to pass the tests, it is not significant for the environment */
    if (err != OMX_ErrorNone) {
      (*(omx_base_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_base_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      err, /* The commands was a OMX_CommandStateSet */
      0, /* The state has been changed in message->messageParam */
      NULL);
    } else {
      if(message->messageParam == OMX_ALL){ /*Disable all ports*/
        /* for all ports */
        for(j = 0; j < NUM_DOMAINS; j++) {
          for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
            (*(omx_base_component_Private->callbacks->EventHandler))
            (openmaxStandComp,
            omx_base_component_Private->callbackData,
            OMX_EventCmdComplete, /* The command was completed */
            OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
            i, /* The state has been changed in message->messageParam */
            NULL);
          }
        }
      } else {
        (*(omx_base_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_base_component_Private->callbackData,
        OMX_EventCmdComplete, /* The command was completed */
        OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
        message->messageParam, /* The state has been changed in message->messageParam */
        NULL);
      }           
    }
  }
  break;
  case OMX_CommandPortEnable:{
    /*Flush port/s*/
    if(message->messageParam == OMX_ALL) {
      /* for all ports */
      for(j = 0; j < NUM_DOMAINS; j++) {
        for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
            omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          pPort=omx_base_component_Private->ports[i];
          err = pPort->Port_EnablePort(pPort);
          }
        }
    } else {
      pPort=omx_base_component_Private->ports[message->messageParam];
      err = pPort->Port_EnablePort(pPort);
    }
    if (err != OMX_ErrorNone) {
      (*(omx_base_component_Private->callbacks->EventHandler))
      (openmaxStandComp,
      omx_base_component_Private->callbackData,
      OMX_EventError, /* The command was completed */
      err, /* The commands was a OMX_CommandStateSet */
      0, /* The state has been changed in message->messageParam */
      NULL);
    } else {
      if(message->messageParam != OMX_ALL) {
        (*(omx_base_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
        omx_base_component_Private->callbackData,
        OMX_EventCmdComplete, /* The command was completed */
        OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
        message->messageParam, /* The state has been changed in message->messageParam */
        NULL);

        if (omx_base_component_Private->state==OMX_StateExecuting) {
          pPort=omx_base_component_Private->ports[message->messageParam];
          if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
            for(i=0; i < pPort->sPortParam.nBufferCountActual;i++) {
              tsem_up(pPort->pBufferSem);
              tsem_up(omx_base_component_Private->bMgmtSem);
            }
          }
        }

      } else {
        /* for all ports */
        for(j = 0; j < NUM_DOMAINS; j++) {
          for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
          i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
            omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
          (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventCmdComplete, /* The command was completed */
          OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
          i, /* The state has been changed in message->messageParam */
          NULL);
          }
        }

        if (omx_base_component_Private->state==OMX_StateExecuting) {
          /* for all ports */
          for(j = 0; j < NUM_DOMAINS; j++) {
            for(i = omx_base_component_Private->sPortTypesParam[j].nStartPortNumber;
            i < omx_base_component_Private->sPortTypesParam[j].nStartPortNumber +
              omx_base_component_Private->sPortTypesParam[j].nPorts; i++) {
              pPort=omx_base_component_Private->ports[i];
              if (PORT_IS_BUFFER_SUPPLIER(pPort)) {
                for(k=0; k < pPort->sPortParam.nBufferCountActual;k++) {
                  tsem_up(pPort->pBufferSem);
                  tsem_up(omx_base_component_Private->bMgmtSem);
                }
              }
            }
          }
        }
      }
    }
  }
  break;
  case OMX_CommandMarkBuffer: {
    omx_base_component_Private->pMark=(OMX_MARKTYPE *)message->pCmdData;
  }
  break;
  default:
    DEBUG(DEB_LEV_ERR, "In %s: Unrecognized command %i\n", __func__, message->messageType);
  break;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Returning from %s: \n", __func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_base_component_AllocateBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort;

  if (nPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  pPort = omx_base_component_Private->ports[nPortIndex];

  return pPort->Port_AllocateBuffer(pPort,
                                    ppBuffer,
                                    nPortIndex,
                                    pAppPrivate,
                                    nSizeBytes);
}

OMX_ERRORTYPE omx_base_component_UseBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort;

  if (nPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  pPort = omx_base_component_Private->ports[nPortIndex];

  return  pPort->Port_UseBuffer(pPort,
                                ppBufferHdr,
                                nPortIndex,
                                pAppPrivate,
                                nSizeBytes,
                                pBuffer);
}

OMX_ERRORTYPE omx_base_component_UseEGLImage (
        OMX_HANDLETYPE hComponent,
        OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
        void* eglImage) {
  return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE omx_base_component_FreeBuffer(   
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort;

  if (nPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                     omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }

  pPort = omx_base_component_Private->ports[nPortIndex];

  return pPort->Port_FreeBuffer(pPort,
                                nPortIndex,
                                pBuffer);
}
  
OMX_ERRORTYPE omx_base_component_EmptyThisBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (pBuffer->nInputPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                                   omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  pPort = omx_base_component_Private->ports[pBuffer->nInputPortIndex];
  if (pPort->sPortParam.eDir != OMX_DirInput) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port direction in Component %s\n", __func__,omx_base_component_Private->name);
    return OMX_ErrorBadPortIndex;
  }
  return pPort->Port_SendBufferFunction(pPort, pBuffer);
}

OMX_ERRORTYPE omx_base_component_FillThisBuffer(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {

  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  if (pBuffer->nOutputPortIndex >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                                    omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                    omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                                    omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  pPort = omx_base_component_Private->ports[pBuffer->nOutputPortIndex];
  if (pPort->sPortParam.eDir != OMX_DirOutput) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port direction in Component %s\n", __func__,omx_base_component_Private->name);
    return OMX_ErrorBadPortIndex;
  }
  return pPort->Port_SendBufferFunction(pPort,  pBuffer);
}

OMX_ERRORTYPE omx_base_component_ComponentTunnelRequest(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_U32 nPort,
  OMX_IN  OMX_HANDLETYPE hTunneledComp,
  OMX_IN  OMX_U32 nTunneledPort,
  OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup) {

  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
  omx_base_PortType *pPort; 
  
  if (nPort >= (omx_base_component_Private->sPortTypesParam[OMX_PortDomainAudio].nPorts +
                omx_base_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                omx_base_component_Private->sPortTypesParam[OMX_PortDomainImage].nPorts +
                omx_base_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
    return OMX_ErrorBadPortIndex;
  }
  
  pPort = omx_base_component_Private->ports[nPort];

  return pPort->ComponentTunnelRequest(pPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}
#ifdef __cplusplus
}
#endif

