/**
	@file src/omxcore.c
	
	OpenMax Integration Layer Core. This library implements the OpenMAX core
	responsible for environment setup, components tunneling and communication.
	
	Copyright (C) 2007  STMicroelectronics

	@author Pankaj SEN, Giulio URLINI

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

/** The static field initialized is equal to 0 if the core is not initialized. 
 * It is equal to 1 whern the OMX_Init has been called
 */
int initialized=0;
/** This static fields contain the numebr of loaders already added to the system. 
 * The use of a static variable can be changed in the future when a different
 * way to expose loaders will be proposed.
 */ 
int loadersAdded = 0;

OMX_BOOL isDefaultLoader=OMX_FALSE;
BOSA_COMPONENTLOADER* defaultLoader=NULL;

#define MAXLOADER 3

/** The pointer to the loaders list. This filed, tike the loadersAdded, is
 * used by the current loaders list handlig, and can be changed in the future
 */
BOSA_COMPONENTLOADER *loadersList[MAXLOADER]={NULL,NULL,NULL};

/** @brief The registration function for loaders. 
 * 
 * This function is used with 
 * loadersAdded and loadersList. It could be changed in the future
 */ 
OMX_ERRORTYPE BOSA_AddComponentLoader(BOSA_COMPONENTLOADER* componentLoader) {
	BOSA_COMPONENTLOADER *currentloader;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	//currentloader = loadersList + loadersAdded;
	//currentloader = componentLoader;
  loadersList[loadersAdded]=componentLoader;
  DEBUG(DEB_LEV_ERR, "In %s loadersList=%x,componentLoader=%x,\n",
    __func__,(int)loadersList[loadersAdded],(int)componentLoader);
	loadersAdded++;
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief The OMX_Init standard function
 */
OMX_ERRORTYPE OMX_Init() {
	/// TODO add a check for multiple components with the same name
	/// in different loaders
	int i = 0;
  //void *test;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);
	if(initialized == 0) {
		initialized = 1;

    /*If no loader added then add default component loader*/
    if(loadersAdded==0) {
      DEBUG(DEB_LEV_ERR, "In %s no of loader=%d Adding Default Loader\n", __func__,loadersAdded);
      BOSA_ST_InitComponentLoader(&defaultLoader);
      BOSA_AddComponentLoader(defaultLoader);
      isDefaultLoader=OMX_TRUE;
    }
		/// TODO check if the list is empty and in case provide a default component loader
    for (i = 0; i<loadersAdded; i++) {
      if(loadersList) 
        if(loadersList[i]->BOSA_CreateComponentLoader)
          loadersList[i]->BOSA_CreateComponentLoader(&loadersList[i]->storeHandler);
		}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
  }
}

/** @brief The OMX_Deinit standard function
 */
OMX_ERRORTYPE OMX_Deinit() {
	int i = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	if(initialized == 1) {
		for (i = 0; i<loadersAdded; i++) {
			loadersList[i]->BOSA_DestroyComponentLoader(loadersList[i]->storeHandler);
		}
	}

  /*If Default Loader Added Then Free the Default Loader*/
  if(isDefaultLoader==OMX_TRUE){
    BOSA_ST_DeinitComponentLoader(defaultLoader);
    isDefaultLoader=OMX_FALSE;
    loadersAdded--;
  }
  initialized = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(OMX_OUT OMX_HANDLETYPE* pHandle,
	OMX_IN  OMX_STRING cComponentName,
	OMX_IN  OMX_PTR pAppData,
	OMX_IN  OMX_CALLBACKTYPE* pCallBacks) {
	int i;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	for (i = 0; i<loadersAdded; i++) {
		err = loadersList[i]->BOSA_CreateComponent(
					loadersList[i]->storeHandler,
					pHandle,
					cComponentName,
					pAppData,
					pCallBacks);
		if (err == OMX_ErrorNone) {
			// the component has been found
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int i;
	for (i = 0; i<loadersAdded; i++) {
		err = loadersList[i]->BOSA_DestroyComponent(
					loadersList[i]->storeHandler,
					hComponent);
		if (err == OMX_ErrorNone) {
			// the component has been found
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(OMX_OUT OMX_STRING cComponentName,
	OMX_IN OMX_U32 nNameLength,
	OMX_IN OMX_U32 nIndex) {
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int i, index, offset = 0;
	int end_index = 0;
	for (i = 0; i<loadersAdded; i++) {
		err = loadersList[i]->BOSA_ComponentNameEnum(
					loadersList[i]->storeHandler,
					cComponentName,
					nNameLength,
					nIndex - offset);
		if (err != OMX_ErrorNone) {
			// The component has been not found with the current loader.
			// the first step is to find the curent number of component for the 
			// current loader, and use it as offset for the next loader
			end_index = 0; index = 0;
			while (!end_index) {
				err = loadersList[i]->BOSA_ComponentNameEnum(
							loadersList[i]->storeHandler,
							cComponentName,
							nNameLength,
							index);
				if (err == OMX_ErrorNone) {
					index++;
				} else {
					end_index = 1;
					offset+=index;
				}
			}
		} else {
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorNoMore;
}

/** @brief The setup tunnel openMAX standard function
 * 
 * The implementation of this function is described in the OpenMAX spec
 */
OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
	OMX_IN  OMX_HANDLETYPE hOutput,
	OMX_IN  OMX_U32 nPortOutput,
	OMX_IN  OMX_HANDLETYPE hInput,
	OMX_IN  OMX_U32 nPortInput) {
	
	OMX_ERRORTYPE err;
	OMX_COMPONENTTYPE* component;
	OMX_TUNNELSETUPTYPE* tunnelSetup;
	
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
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
	DEBUG(DEB_LEV_PARAMS, "First stage of tunneling acheived:\n");
	DEBUG(DEB_LEV_PARAMS, "       - supplier proposed = %i\n", (int)tunnelSetup->eSupplier);
	DEBUG(DEB_LEV_PARAMS, "       - flags             = %i\n", (int)tunnelSetup->nTunnelFlags);
	
	component = (OMX_COMPONENTTYPE*)hInput;
	if (hInput) {
		err = (component->ComponentTunnelRequest)(hInput, nPortInput, hOutput, nPortOutput, tunnelSetup);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Tunneling failed: input port rejects it - err = %08x\n", err);
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
	DEBUG(DEB_LEV_PARAMS, "Second stage of tunneling acheived:\n");
	DEBUG(DEB_LEV_PARAMS, "       - supplier proposed = %i\n", (int)tunnelSetup->eSupplier);
	DEBUG(DEB_LEV_PARAMS, "       - flags             = %i\n", (int)tunnelSetup->nTunnelFlags);
	free(tunnelSetup);
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetRolesOfComponent ( 
  OMX_IN      OMX_STRING CompName, 
  OMX_INOUT   OMX_U32 *pNumRoles,
  OMX_OUT     OMX_U8 **roles) {
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int i;
	
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	for (i = 0; i<loadersAdded; i++) {
		err = loadersList[i]->BOSA_GetRolesOfComponent(
					loadersList[i]->storeHandler,
					CompName,
					pNumRoles,
					roles);
		if (err == OMX_ErrorNone) {
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorComponentNotFound;
}

OMX_ERRORTYPE OMX_GetComponentsOfRole ( 
  OMX_IN      OMX_STRING role,
  OMX_INOUT   OMX_U32 *pNumComps,
  OMX_INOUT   OMX_U8  **compNames) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
	int i,j;
	int only_number_requested = 0,full_number=0;
	int total_num_comp = 0;
	int temp_num_comp = 0;
	int index_current_name = 0;
	
	OMX_U8 **tempCompNames;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	if (compNames == NULL) {
		only_number_requested = 1;
	} else {
		only_number_requested = 0;
	}
	for (i = 0; i<loadersAdded; i++) {
		temp_num_comp = *pNumComps;
		err = loadersList[i]->BOSA_GetComponentsOfRole(
					&loadersList[i]->storeHandler,
					role,
					pNumComps,
					NULL);
		full_number += temp_num_comp;
		if (only_number_requested == 0) {
			tempCompNames = malloc(temp_num_comp * sizeof(OMX_STRING));
			for (j=0; j<temp_num_comp; j++) {
				tempCompNames[j] = malloc(OMX_MAX_STRINGNAME_SIZE * sizeof(char));
			}
			err = loadersList[i]->BOSA_GetComponentsOfRole(
					loadersList[i]->storeHandler,
					role,
					pNumComps,
					tempCompNames);
			
		}
		
		if (err == OMX_ErrorNone) {
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorComponentNotFound;
}

