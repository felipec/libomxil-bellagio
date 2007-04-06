/**
	@file src/omxcore.c
	
	OpenMax Integration Layer Core. This library implements the OpenMAX core
	responsible for environment setup, components tunneling and communication.
	
	Copyright (C) 2007  STMicroelectronics and Nokia

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
/** The following include file contains the st staic component loader st_static_loader
 */
#include "st_static_component_loader.h"

/** The static field initialized is equal to 0 if the core is not initialized. 
 * It is equal to 1 whern the OMX_Init has been called
 */
int initialized=0;

/** The macro NUM_LOADERS contains the number of loaders available in the system.
 */
#define NUM_LOADERS 1

/** The pointer to the loaders list. This list contains the all the different component loaders 
 * present in the system or added by the IL Client with the BOSA_AddComponentLoader function.
 * The component loader is a implementation specific way to handle a set of components. The implementation
 * of the IL core accesses to the loaders in a standard way, but the different loaders can handle
 * different types of components, or handle in different ways the same components. It can be used also
 * to create a multi-OS support
 */
BOSA_COMPONENTLOADER *loadersList[NUM_LOADERS];

/** @brief The OMX_Init standard function
 * 
 * This function calls the init function of each componente loader added. If there 
 * is no component loaders present, the ST default component loader (static libraries)
 * is loaded as default component loader.
 * 
 * @return OMX_ErrorNone
 */
OMX_ERRORTYPE OMX_Init() {
	int i = 0;
	OMX_ERRORTYPE err;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);
	if(initialized == 0) {
		initialized = 1;
		st_static_InitComponentLoader();
		loadersList[0] = &st_static_loader;
		
		for (i = 0; i<NUM_LOADERS; i++) {
			err = loadersList[i]->BOSA_CreateComponentLoader(&loadersList[i]->componentLoaderHandler);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "A Component loader constructor fails. Exiting\n");
				return OMX_ErrorInsufficientResources;
			}
		}
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief The OMX_Deinit standard function
 * 
 * In this function the Deinit function for each component loader is performed
 */
OMX_ERRORTYPE OMX_Deinit() {
	int i = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	if(initialized == 1) {
		for (i = 0; i<NUM_LOADERS; i++) {
			loadersList[i]->BOSA_DestroyComponentLoader(loadersList[i]->componentLoaderHandler);
		}
	}
	initialized = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief the OMX_GetHandle standard function
 * 
 * This function will scan inside any component loader to search for
 * the requested component. If there are more components with the same name 
 * the first component is returned. The existence of multiple components with
 * the same name is not contemplated in OpenMAX specification. The assumption is
 * that ithis behavior is NOT allowed
 * 
 * @return OMX_ErrorNone if a component has been found
 *         OMX_ErrorComponentNotFound if the requested component has not been found 
 *                                    in any loader
 */
OMX_ERRORTYPE OMX_GetHandle(OMX_OUT OMX_HANDLETYPE* pHandle,
	OMX_IN  OMX_STRING cComponentName,
	OMX_IN  OMX_PTR pAppData,
	OMX_IN  OMX_CALLBACKTYPE* pCallBacks) {
	
	int i;
	OMX_ERRORTYPE err;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

	for (i = 0; i<NUM_LOADERS; i++) {
		err = loadersList[i]->BOSA_CreateComponent(
					loadersList[i]->componentLoaderHandler,
					pHandle,
					cComponentName,
					pAppData,
					pCallBacks);
		if (err == OMX_ErrorNone) {
			// the component has been found
			return OMX_ErrorNone;
		}
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorComponentNotFound;
}

/** @brief the OMX_FreeHandle standard function
 * 
 * This function calls the DeInit standard function of the component, and frees the handle.
 * There is non need ofd a componente loader specific function, because the behavior of the
 * release of resources is completely described on the spec and can be done only in this way.
 */
OMX_ERRORTYPE OMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent) {
  OMX_ERRORTYPE err;
  err = ((OMX_COMPONENTTYPE*)hComponent)->ComponentDeInit(hComponent);
  free((OMX_COMPONENTTYPE*)hComponent);
	hComponent = NULL;
	return err;
}

/** @brief the OMX_ComponentNameEnum standard function
 * 
 * This function build a complete list of names from all the loaders. 
 * FOr each loaader the index is from 0 to max, but this function must provide a single 
 * list, with a common index. This implementation orders the loaders and the 
 * related list of components
 */
OMX_ERRORTYPE OMX_ComponentNameEnum(OMX_OUT OMX_STRING cComponentName,
	OMX_IN OMX_U32 nNameLength,
	OMX_IN OMX_U32 nIndex) {
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int i, index, offset = 0;
	int end_index = 0;
	
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	for (i = 0; i<NUM_LOADERS; i++) {
		err = loadersList[i]->BOSA_ComponentNameEnum(
					loadersList[i]->componentLoaderHandler,
					cComponentName,
					nNameLength,
					nIndex - offset);
		if (err != OMX_ErrorNone) {
			/** The component has been not found with the current loader.
			 * the first step is to find the curent number of component for the
			 * current loader, and use it as offset for the next loader
			 */
			end_index = 0; index = 0;
			while (!end_index) {
				err = loadersList[i]->BOSA_ComponentNameEnum(
							loadersList[i]->componentLoaderHandler,
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
			DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s with OMX_ErrorNone\n", __func__);
			return OMX_ErrorNone;
		}
	}
	cComponentName = NULL;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s with OMX_ErrorNoMore\n", __func__);
	return OMX_ErrorNoMore;
}

/** @brief the OMX_SetupTunnel standard function
 * 
 * The implementation of this function is described in the OpenMAX spec
 */
OMX_ERRORTYPE OMX_SetupTunnel(
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
		tunnelSetup = NULL;
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
				tunnelSetup = NULL;
				DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s with OMX_ErrorUndefined\n", __func__);
				return OMX_ErrorUndefined;
			}
			free(tunnelSetup);
			tunnelSetup = NULL;
			DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s with OMX_ErrorPortsNotCompatible\n", __func__);
			return OMX_ErrorPortsNotCompatible;
		}
	}
	DEBUG(DEB_LEV_PARAMS, "Second stage of tunneling acheived:\n");
	DEBUG(DEB_LEV_PARAMS, "       - supplier proposed = %i\n", (int)tunnelSetup->eSupplier);
	DEBUG(DEB_LEV_PARAMS, "       - flags             = %i\n", (int)tunnelSetup->nTunnelFlags);
	free(tunnelSetup);
	tunnelSetup = NULL;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief the OMX_GetRolesOfComponent standard function
 */ 
OMX_ERRORTYPE OMX_GetRolesOfComponent ( 
  OMX_IN      OMX_STRING CompName, 
  OMX_INOUT   OMX_U32 *pNumRoles,
  OMX_OUT     OMX_U8 **roles) {
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int i;
	
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	for (i = 0; i<NUM_LOADERS; i++) {
		err = loadersList[i]->BOSA_GetRolesOfComponent(
					loadersList[i]->componentLoaderHandler,
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

/** @brief the OMX_GetComponentsOfRole standard function
 * 
 * This function searches in all the component loaders any component
 * supporting the requested role
 */
OMX_ERRORTYPE OMX_GetComponentsOfRole ( 
  OMX_IN      OMX_STRING role,
  OMX_INOUT   OMX_U32 *pNumComps,
  OMX_INOUT   OMX_U8  **compNames) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
	int i,j;
	int only_number_requested = 0, full_number=0;
	OMX_U32 temp_num_comp = 0;
	
	OMX_U8 **tempCompNames;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	if (compNames == NULL) {
		only_number_requested = 1;
	} else {
		only_number_requested = 0;
	}
	for (i = 0; i<NUM_LOADERS; i++) {
		temp_num_comp = *pNumComps;
		err = loadersList[i]->BOSA_GetComponentsOfRole(
					loadersList[i]->componentLoaderHandler,
					role,
					&temp_num_comp,
					NULL);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
			return OMX_ErrorComponentNotFound;
		}
		if (only_number_requested == 0) {
			tempCompNames = malloc(temp_num_comp * sizeof(OMX_STRING));
			for (j=0; j<temp_num_comp; j++) {
				tempCompNames[j] = malloc(OMX_MAX_STRINGNAME_SIZE * sizeof(char));
			}
			err = loadersList[i]->BOSA_GetComponentsOfRole(
					loadersList[i]->componentLoaderHandler,
					role,
					pNumComps,
					tempCompNames);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
				return OMX_ErrorComponentNotFound;
			}
			
			for (j = 0; j<temp_num_comp; j++) {
				if (full_number+j<*pNumComps) {
					compNames[full_number+j] =  tempCompNames[j];
				}
			}
		}
		full_number += temp_num_comp;
	}
	*pNumComps = full_number;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}
