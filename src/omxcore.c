/**
	@file src/omxcore.c
	
	OpenMax Integration Layer Core. This library implements the OpenMAX core
	responsible for environment setup and components tunneling and communication.
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

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
	
	2006/07/27:  IL Core version 0.2

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <errno.h>

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Component.h>

#include "omxcore.h"
#include "tsemaphore.h"
#include "queue.h"

/** \def MAXCOMPONENTS This statement defines the maximum ndefine statement should be removed when the dynamic load method will be implemented
	*/
#define MAXCOMPONENTS 128
/** The arrow string is used in the list of registered components, $HOME/.omxregistry */
#define ARROW " ==> "
/** Container for handle templates.
 */
stComponentType* templateList[MAXCOMPONENTS];

coreDescriptor_t* coreDescriptor=NULL;
int initialized=0;

OMX_ERRORTYPE OMX_Init()
{
	int i = 0;
	if(initialized!=1) {
		initialized=1;
		if(coreDescriptor==NULL) {
			coreDescriptor = malloc(sizeof(coreDescriptor_t));
			coreDescriptor->messageQueue=NULL;
			coreDescriptor->messageSem=NULL;

			coreDescriptor->messageQueue = malloc(sizeof(queue_t));
			queue_init(coreDescriptor->messageQueue);

			coreDescriptor->messageSem = malloc(sizeof(tsem_t));
			tsem_init(coreDescriptor->messageSem, 0);
		
			coreDescriptor->exit_messageThread=OMX_FALSE;
			pthread_mutex_init(&coreDescriptor->exit_mutex, NULL);
	
			coreDescriptor->messageHandlerThreadID = pthread_create(&coreDescriptor->messageHandlerThread,
				NULL,
				messageHandlerFunction,
				coreDescriptor);
			
			for (i = 0; i<MESS_HANDLER_THREADS; i++) {
				coreDescriptor->subMessHandler[i].subMessageSem = malloc(sizeof(tsem_t));
				tsem_init(coreDescriptor->subMessHandler[i].subMessageSem, 0);
				coreDescriptor->subMessHandler[i].subThreadCreationSem = malloc(sizeof(tsem_t));
				tsem_init(coreDescriptor->subMessHandler[i].subThreadCreationSem, 0);
				pthread_mutex_init(&coreDescriptor->subMessHandler[i].exit_mutex, NULL);
				
				pthread_mutex_lock(&coreDescriptor->subMessHandler[i].exit_mutex);
				coreDescriptor->subMessHandler[i].exit_messageThread=OMX_FALSE;
				pthread_mutex_unlock(&coreDescriptor->subMessHandler[i].exit_mutex);
				pthread_mutex_lock(&coreDescriptor->exit_mutex);
				
				coreDescriptor->subThreadIndex = i;
				pthread_mutex_unlock(&coreDescriptor->exit_mutex);
				coreDescriptor->subMessHandler[i].messHandlSubThreadID = pthread_create(&coreDescriptor->subMessHandler[i].messHandlSubThread,
				NULL,
				messHandlSubFunction,
				coreDescriptor);
				tsem_down(coreDescriptor->subMessHandler[i].subThreadCreationSem);
				coreDescriptor->subMessHandler[i].isAvailable = 1;
				pthread_mutex_init(&coreDescriptor->subMessHandler[i].avail_flag_mutex, NULL);
			}
		}
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_Deinit()
{
	int i = 0;
	if(initialized==1) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
/// \todo add the check for every component not yet disposed
		pthread_mutex_lock(&coreDescriptor->exit_mutex);
		coreDescriptor->exit_messageThread=OMX_TRUE;
		pthread_mutex_unlock(&coreDescriptor->exit_mutex);
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			pthread_mutex_lock(&coreDescriptor->subMessHandler[i].exit_mutex);
			coreDescriptor->subMessHandler[i].exit_messageThread=OMX_TRUE;
			pthread_mutex_unlock(&coreDescriptor->subMessHandler[i].exit_mutex);
		}
		DEBUG(DEB_LEV_SIMPLE_SEQ, "coreDescriptor->exit_mutex done\n");

		if(coreDescriptor->messageSem->semval==0) {
			tsem_up(coreDescriptor->messageSem);
		}
		DEBUG(DEB_LEV_SIMPLE_SEQ, "messagesem done\n");
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			if (coreDescriptor->subMessHandler[i].subMessageSem->semval == 0) {
				tsem_up(coreDescriptor->subMessHandler[i].subMessageSem);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "messagesem of thread %i done\n", i);
			}
		}
	
		/** With the following call, the mesage queue is not flushed, so that if something 
			* goes wrong in the queue, the execution hangs forever 
			*/
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			pthread_join(coreDescriptor->subMessHandler[i].messHandlSubThread,NULL);
			DEBUG(DEB_LEV_SIMPLE_SEQ, "messages thread %i done\n", i);
		}	
		pthread_join(coreDescriptor->messageHandlerThread,NULL);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "main thread closed\n");
	
		pthread_mutex_lock(&coreDescriptor->exit_mutex);
		coreDescriptor->exit_messageThread=OMX_FALSE;
		pthread_mutex_unlock(&coreDescriptor->exit_mutex);
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			pthread_mutex_lock(&coreDescriptor->subMessHandler[i].exit_mutex);
			coreDescriptor->subMessHandler[i].exit_messageThread=OMX_FALSE;
			pthread_mutex_unlock(&coreDescriptor->subMessHandler[i].exit_mutex);
		}
		
		DEBUG(DEB_LEV_SIMPLE_SEQ, "coreDescriptor->exit_mutex done\n");

		tsem_deinit(coreDescriptor->messageSem);
		queue_deinit(coreDescriptor->messageQueue);
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			tsem_deinit(coreDescriptor->subMessHandler[i].subMessageSem);
		}

		pthread_mutex_destroy(&coreDescriptor->exit_mutex);
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			pthread_mutex_destroy(&coreDescriptor->subMessHandler[i].avail_flag_mutex);
		}
		if(coreDescriptor->messageSem!=NULL) {
			free(coreDescriptor->messageSem);
		}
		if(coreDescriptor->messageQueue!=NULL) {
			free(coreDescriptor->messageQueue);
		}
		if(coreDescriptor!=NULL) {
			free(coreDescriptor);
		}
	
		DEBUG(DEB_LEV_SIMPLE_SEQ, "deinit done\n");
	
		initialized=0;
		coreDescriptor=NULL;
	}
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Exiting %s...\n", __func__);
	return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(OMX_OUT OMX_HANDLETYPE* pHandle,
	OMX_IN  OMX_STRING cComponentName,
	OMX_IN  OMX_PTR pAppData,
	OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{
	int i, err = OMX_ErrorNone;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	stComponentType* pStcomponent;
	
	/** If the component is not available try to load its
	 * implementation from the file system via loadComponentSO()
	 */
	if(!isTemplateAvailable(cComponentName))
		err = loadComponentSO(cComponentName);
	if(err){
		DEBUG(DEB_LEV_ERR, "Component %s not found, sorry...\n", cComponentName);
		return OMX_ErrorComponentNotFound;
	}

	/* Find the requested template and factor the component out of it */
	for(i=0;i<MAXCOMPONENTS;i++){
		if(templateList[i]){
			if(!strcmp(templateList[i]->name, cComponentName)){
				DEBUG(DEB_LEV_PARAMS, "Found requested template %s\n", cComponentName);
				/* Build ST component from template and fill fields */
				pStcomponent = malloc(sizeof(stComponentType));
				memcpy(pStcomponent, templateList[i], sizeof(stComponentType));
				pStcomponent->coreDescriptor = coreDescriptor;
				eError=pStcomponent->constructor(pStcomponent);//Startup component (will spawn the event manager thread)
				/* Fill and return the OMX thing */
				*pHandle = &pStcomponent->omx_component;
				((OMX_COMPONENTTYPE*)*pHandle)->SetCallbacks(*pHandle, pCallBacks, pAppData);	
				DEBUG(DEB_LEV_FULL_SEQ, "Template %s found returning from OMX_GetHandle\n", cComponentName);
				return eError;
			}
		}
	}
	
	/* Giving up. Something went wrong contructing the component */
	DEBUG(DEB_LEV_ERR, "Template %s not found, sorry...\n", cComponentName);
	return OMX_ErrorComponentNotFound;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent)
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	stComponent->destructor(stComponent);
	free(stComponent);
	return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(OMX_OUT OMX_STRING cComponentName,
	OMX_IN OMX_U32 nNameLength,
	OMX_IN OMX_U32 nIndex)
{
	OMX_ERRORTYPE err;
	err = readOMXRegistry(nIndex, nNameLength, (char*) cComponentName);
	return err;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
	OMX_IN  OMX_HANDLETYPE hOutput,
	OMX_IN  OMX_U32 nPortOutput,
	OMX_IN  OMX_HANDLETYPE hInput,
	OMX_IN  OMX_U32 nPortInput) {
	
	OMX_ERRORTYPE err;
	OMX_COMPONENTTYPE* component;
	OMX_TUNNELSETUPTYPE* tunnelSetup;
	tunnelSetup = malloc(sizeof(OMX_TUNNELSETUPTYPE));
	component = (OMX_COMPONENTTYPE*)hOutput;
	tunnelSetup->nTunnelFlags = 0;
	tunnelSetup->eSupplier = 0;

	if (hOutput == NULL && hInput == NULL)
        return OMX_ErrorBadParameter;
	if (hOutput){
		err = (component->ComponentTunnelRequest)(hOutput, nPortOutput, hInput, nPortInput, tunnelSetup);
		if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Tunneling failed: output port rejects it - err = %i\n", err);
		free(tunnelSetup);
		return err;
		}
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "First stage of tunneling acheived:\n");
	DEBUG(DEB_LEV_SIMPLE_SEQ, "       - supplier proposed = %i\n", (int)tunnelSetup->eSupplier);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "       - flags             = %i\n", (int)tunnelSetup->nTunnelFlags);
	
	component = (OMX_COMPONENTTYPE*)hInput;
	if (hInput) {
		err = (component->ComponentTunnelRequest)(hInput, nPortInput, hOutput, nPortOutput, tunnelSetup);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Tunneling failed: input port rejects it - err = %i\n", err);
			// the second stage fails. the tunnel on poutput port has to be removed
			component = (OMX_COMPONENTTYPE*)hOutput;
			err = (component->ComponentTunnelRequest)(hOutput, nPortOutput, NULL, 0, tunnelSetup);
			if (err != OMX_ErrorNone) {
				// This error should never happen. It is critical, and not recoverable
				free(tunnelSetup);
				return OMX_ErrorUndefined;
			}
			free(tunnelSetup);
			return OMX_ErrorPortsNotCompatible;
		}
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Second stage of tunneling acheived:\n");
	DEBUG(DEB_LEV_SIMPLE_SEQ, "       - supplier proposed = %i\n", (int)tunnelSetup->eSupplier);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "       - flags             = %i\n", (int)tunnelSetup->nTunnelFlags);
	free(tunnelSetup);
	return OMX_ErrorNone;
}
/**************************************************************
 *
 * PRIVATE: core private entry points and helper functions
 *
 **************************************************************/

int register_template(stComponentType* template)
{
  int i;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "REGISTERING TEMPLATE\n");
  for(i=0;i<MAXCOMPONENTS;i++)
    if(templateList[i] == NULL){
      templateList[i] = template;
		templateList[i]->needsRegistation = 1;
      return 0;
    }
  return OMX_ErrorInsufficientResources;
}

void setHeader(OMX_PTR header, OMX_U32 size)
{
	OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
	*((OMX_U32*)header) = size;

	ver->s.nVersionMajor = SPECVERSIONMAJOR;
	ver->s.nVersionMinor = SPECVERSIONMINOR;
	ver->s.nRevision = SPECREVISION;
	ver->s.nStep = SPECSTEP;
}

OMX_ERRORTYPE checkHeader(OMX_PTR header, OMX_U32 size)
{
	OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
	if (header == NULL) {
		return OMX_ErrorBadParameter;
	}
	if(*((OMX_U32*)header) != size){
		return OMX_ErrorBadParameter;
	}

	if(ver->s.nVersionMajor != SPECVERSIONMAJOR ||
		ver->s.nVersionMinor != SPECVERSIONMINOR ||
		ver->s.nRevision != SPECREVISION ||
		ver->s.nStep != SPECSTEP){
		return OMX_ErrorVersionMismatch;
	}

	return OMX_ErrorNone;
}

/** This function is executed in the context of a separate thread. 
	* There is an array of threads, each one of them that runs this function. For each message
	* recevied from the IL Client, a thread is executed. When all the threads available in
	* the array are busy, a new thread is created, and the function messHandldynamicFunction is
	* executed instead of this one.
	*/
void* messHandlSubFunction(void* param) {
	coreDescriptor_t* coreDescriptor = param;
	int subThreadIndex;
	int exit_thread;

	pthread_mutex_lock(&coreDescriptor->exit_mutex);
	subThreadIndex = coreDescriptor->subThreadIndex;
	pthread_mutex_unlock(&coreDescriptor->exit_mutex);
	tsem_up(coreDescriptor->subMessHandler[subThreadIndex].subThreadCreationSem);
	while(1) {
		tsem_down(coreDescriptor->subMessHandler[subThreadIndex].subMessageSem);
		pthread_mutex_lock(&coreDescriptor->subMessHandler[subThreadIndex].exit_mutex);
		exit_thread = coreDescriptor->subMessHandler[subThreadIndex].exit_messageThread;
		pthread_mutex_unlock(&coreDescriptor->subMessHandler[subThreadIndex].exit_mutex);
		if(exit_thread==OMX_TRUE) {
			break;
		}
		if (coreDescriptor->subMessHandler[subThreadIndex].subMessage == NULL) {DEBUG(DEB_LEV_ERR, "Error, message is NULL!!!!\n");	continue;}
		pthread_mutex_lock(&coreDescriptor->subMessHandler[subThreadIndex].subMessage->stComponent->pHandleMessageMutex);
		coreDescriptor->subMessHandler[subThreadIndex].subMessage->stComponent->messageHandler(coreDescriptor->subMessHandler[subThreadIndex].subMessage);
		pthread_mutex_unlock(&coreDescriptor->subMessHandler[subThreadIndex].subMessage->stComponent->pHandleMessageMutex);
		
		free(coreDescriptor->subMessHandler[subThreadIndex].subMessage);
		
		pthread_mutex_lock(&coreDescriptor->subMessHandler[subThreadIndex].avail_flag_mutex);
		coreDescriptor->subMessHandler[subThreadIndex].isAvailable = 1;
		pthread_mutex_unlock(&coreDescriptor->subMessHandler[subThreadIndex].avail_flag_mutex);
	}
	return NULL;
}

/** This function is executed in the context of a thread dynamically created. If the normal array of threads
	* is not enough to handle all the parallel messages from the IL client, the needed threads are created dynamically,
	* an this is the function executed.
	*/
void* messHandldynamicFunction(void* param) {
	coreMessage_t* coreMessage = param;
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
	pthread_mutex_lock(&coreMessage->stComponent->pHandleMessageMutex);
	coreMessage->stComponent->messageHandler(coreMessage);
	pthread_mutex_unlock(&coreMessage->stComponent->pHandleMessageMutex);
	free(coreMessage);
	return NULL;
}

/** This function receives the messages from the IL client and retrieve them from a message queue.
	* It dispatches the messages to message handle threads, for the processing.
	* In order to avoid deadlocks, the messages are handled in threads different from this one.
	*/
void* messageHandlerFunction(void* param)
{
	coreDescriptor_t* coreDescriptor = param;
	coreMessage_t* coreMessage;

	int exit_thread;
	int i;
	int dynamicThreadID;
	pthread_t dynamicThread;
	
	while(1){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);
		/* Wait for an incoming message */
		tsem_down(coreDescriptor->messageSem);
		pthread_mutex_lock(&coreDescriptor->exit_mutex);
		exit_thread = coreDescriptor->exit_messageThread;
		pthread_mutex_unlock(&coreDescriptor->exit_mutex);
		if(exit_thread==OMX_TRUE) {
			break;
		}

		/* Dequeue it */
		coreMessage = dequeue(coreDescriptor->messageQueue);
		
		if(coreMessage == NULL){
			DEBUG(DEB_LEV_ERR, "In %s: ouch!! had null message!\n", __func__);
			break;
		}

		if(coreMessage->stComponent == NULL){
			DEBUG(DEB_LEV_ERR, "In %s: ouch!! had null component!\n", __func__);
			break;
		}
		/* Process it by calling component's message handler method
		 */
		for (i = 0; i<MESS_HANDLER_THREADS; i++) {
			pthread_mutex_lock(&coreDescriptor->subMessHandler[i].avail_flag_mutex);
			if (coreDescriptor->subMessHandler[i].isAvailable) {
				coreDescriptor->subMessHandler[i].isAvailable = 0;
				pthread_mutex_unlock(&coreDescriptor->subMessHandler[i].avail_flag_mutex);
				coreDescriptor->subMessHandler[i].subMessage = coreMessage;
				tsem_up(coreDescriptor->subMessHandler[i].subMessageSem);
				break;
			} else {
				pthread_mutex_unlock(&coreDescriptor->subMessHandler[i].avail_flag_mutex);
			}
		}
		if (i == MESS_HANDLER_THREADS) {
			dynamicThreadID = pthread_create(&dynamicThread, NULL, messHandldynamicFunction, coreMessage);
		}
		
		/* Message ownership has been transferred to us
		 * so we gonna free it when finished.
		 */
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Message Handler thread\n");
	return NULL;
}

int buildComponentsList(char* componentspath, int* ncomponents)
{
	DIR *dirp = NULL;
	struct dirent *dp;
	
  void* handle;
	int i = 0;
	FILE* omxregistryfp;
	char omxregistryfile[200];
	
	*ncomponents = 0;
	memset(omxregistryfile, 0, sizeof(omxregistryfile));
	strcat(omxregistryfile, getenv("HOME"));
	strcat(omxregistryfile, "/.omxregistry");
	
	/* Populate the registry file */
	dirp = opendir(componentspath);
	if(dirp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open directory %s\n", componentspath);
		return ENOENT;
	}

	omxregistryfp = fopen(omxregistryfile, "w");
	if (omxregistryfp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file%s\n", omxregistryfile);
		return ENOENT;
	}
	
	while((dp = readdir(dirp)) != NULL){
		int len;
      char ext[4];
		
      len = strlen(dp->d_name);
		
      if(len >= 3){
			strncpy(ext, &(dp->d_name[len-3]), 3);
			ext[4]='\0';
			
			if(strncmp(ext, ".so", 3) == 0){
				char lib_absolute_path[200];
				
				strcpy(lib_absolute_path, componentspath);
				strcat(lib_absolute_path, "/");
				strcat(lib_absolute_path, dp->d_name);
				
				if((handle = dlopen(lib_absolute_path, RTLD_NOW)) == NULL){
					DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", lib_absolute_path, dlerror());
				}
				else{
					for(i=0;i<MAXCOMPONENTS;i++){
						if(templateList[i] && templateList[i]->needsRegistation == 1){
							char buffer[256];
							memset(buffer, 0, sizeof(buffer));
							templateList[i]->needsRegistation = 0;
							(*ncomponents)++;
							DEBUG(DEB_LEV_SIMPLE_SEQ, "Found component %s in shared object %s\n",
								templateList[i]->name, lib_absolute_path);
							strcat(buffer, templateList[i]->name);
							strcat(buffer, ARROW);
							strcat(buffer, lib_absolute_path);
							strcat(buffer, "\n");
							fwrite(buffer, 1, strlen(buffer), omxregistryfp);
						}
					}
				}
			}
		}
	}
	
	fclose(omxregistryfp);
	return 0;
}

int loadComponentSO(char* component)
{
	FILE* omxregistryfp;
	char* line = NULL;
	char* componentpath;
	size_t len = 0;
	ssize_t read;
	char omxregistryfile[200];
	void* handle = NULL;
	
	memset(omxregistryfile, 0, sizeof(omxregistryfile));
	strcat(omxregistryfile, getenv("HOME"));
	strcat(omxregistryfile, "/.omxregistry");

	omxregistryfp = fopen(omxregistryfile, "r");
	if (omxregistryfp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file%s\n", omxregistryfile);
		return ENOENT;
	}

	/** Scan through the registry and look for component's implementation.
	 * Load it.
	 */
	while((read = getline(&line, &len, omxregistryfp)) != -1) {
		if(!strncmp(line, component, strlen(component))){
			componentpath = line + strlen(component) + strlen(ARROW);
			componentpath[strlen(componentpath) - 1] = 0;
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Found component %s in %s\n", component, componentpath);
			if((handle = dlopen(componentpath, RTLD_NOW)) == NULL){
				DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", componentpath, dlerror());
			}
			break;
		}
	}
	if(line)
		free(line);

	if(handle != NULL)
		return 0;
	return ENOENT;
}

OMX_ERRORTYPE readOMXRegistry(int position, int size, char* componentname)
{
	FILE* omxregistryfp;
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	char omxregistryfile[200];
	int index;

	memset(omxregistryfile, 0, sizeof(omxregistryfile));
	strcat(omxregistryfile, getenv("HOME"));
	strcat(omxregistryfile, "/.omxregistry");

	omxregistryfp = fopen(omxregistryfile, "r");
	if (omxregistryfp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file%s\n", omxregistryfile);
		return ENOENT;
	}

	/** Scan through the registry and look for component's implementation.
	 * Load it.
	 */
	index = 0;
	while((read = getline(&line, &len, omxregistryfp)) != -1) {
		if (index == position) {
			break;
		}
		index++;
	}
	if (read != -1) {
		index = 0;
		while (*(line+index) != ' ') {index++;}
		*(line+index) = 0;
		
		if (size < index) {*(line+size) = 0;}
		strcpy(componentname, line);
		*(componentname + index) = 0;
	} else {
		if(line)
			free(line);
		return OMX_ErrorNoMore;
	}
	if(line)
		free(line);
	return OMX_ErrorNone;
}


static int isTemplateAvailable(char* cComponentName)
{
	int i;
	for(i=0;i<MAXCOMPONENTS;i++)
		if(templateList[i] && !strcmp(templateList[i]->name, cComponentName))
			return 1;
	return 0;
}
