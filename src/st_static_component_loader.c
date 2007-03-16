/**
	@file src/st_static_component_loader.c
	
	ST specific component loader for local components.
	
	Copyright (C) 2007  STMicroelectronics

	@author Giulio URLINI, Pankaj SEN

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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <errno.h>

#include "st_static_component_loader.h"

/** @brief The initialization of the ST specific component loader. 
 * 
 * This function allocates memory for the component loader and initialize other function pointer
 */
OMX_ERRORTYPE BOSA_ST_InitComponentLoader(BOSA_COMPONENTLOADER** componentLoader)
{
  *componentLoader=(BOSA_COMPONENTLOADER *)calloc(1,sizeof(BOSA_COMPONENTLOADER));

  DEBUG(DEB_LEV_ERR, "In %s componentLoader %x \n",__func__,(int)*componentLoader);

	(*componentLoader)->BOSA_CreateComponentLoader = &BOSA_ST_CreateComponentLoader;
	(*componentLoader)->BOSA_DestroyComponentLoader = &BOSA_ST_DestroyComponentLoader;
	(*componentLoader)->BOSA_CreateComponent = &BOSA_ST_CreateComponent;
	(*componentLoader)->BOSA_DestroyComponent = &BOSA_ST_DestroyComponent;
	(*componentLoader)->BOSA_ComponentNameEnum = &BOSA_ST_ComponentNameEnum;
	(*componentLoader)->BOSA_GetRolesOfComponent = &BOSA_ST_GetRolesOfComponent;
	(*componentLoader)->BOSA_GetComponentsOfRole = &BOSA_ST_GetComponentsOfRole;

  return OMX_ErrorNone;

}

/** @brief The destructor of the ST specific component loader. 
 * 
 * This function frees component loader
 */
OMX_ERRORTYPE BOSA_ST_DeinitComponentLoader(BOSA_COMPONENTLOADER *componentLoader)
{
  free(componentLoader);

  return OMX_ErrorNone;
}

int listindex=0;

/** @brief the ST static loader contructor
 * 
 * This function creates the ST static component loader, and creates
 * the list of available components, based on a registry file
 * created by a separate appication. It is called omxregister, 
 * and must be called before the use of this loader
 */
OMX_ERRORTYPE BOSA_ST_CreateComponentLoader(BOSA_ComponentLoaderHandle *loaderHandle) {
	FILE* omxregistryfp;
	char* line = NULL;
	char omxregistryfile[200];
	char *libname;
	int num_of_comp=0;
 	int read;
	stLoaderComponentType** templateList;
	stLoaderComponentType** stComponentsTemp;
	void* handle;
	size_t len;
  int (*fptr)(void *);
	int i;
	//int listindex=0;
	int index;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	memset(omxregistryfile, 0, sizeof(omxregistryfile));
	strcat(omxregistryfile, getenv("HOME"));
	strcat(omxregistryfile, "/.omxregistry");

	omxregistryfp = fopen(omxregistryfile, "r");
	if (omxregistryfp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file%s\n", omxregistryfile);
		return ENOENT;
	}
	num_of_comp = 0;
	libname = malloc(OMX_MAX_STRINGNAME_SIZE * 2 * sizeof(char));
	while((read = getline(&line, &len, omxregistryfp)) != -1) {
		if ((*line == ' ') && (*(line+1) == '=')) {
			num_of_comp++;
			// not a library line. skip
			continue;
		}
	}
  templateList = calloc(num_of_comp,sizeof (stLoaderComponentType*));
	for (i = 0; i<num_of_comp; i++) {
		templateList[i] = NULL;
	}
	fseek(omxregistryfp, 0, 0);
	listindex = 0;
	while((read = getline(&line, &len, omxregistryfp)) != -1) {
		if ((*line == ' ') && (*(line+1) == '=')) {
			// not a library line. skip
			continue;
		}
		index = 0;
		while (*(line+index)!= '\n') index++;
		*(line+index) = 0;
		strcpy(libname, line);
    DEBUG(DEB_LEV_ERR, "libname: %s\n",libname);
		if((handle = dlopen(libname, RTLD_NOW)) == NULL) {
			DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", libname, dlerror());
		} else {
			if ((*(void **)(&fptr) = dlsym(handle, "omx_component_library_Setup")) == NULL) {
				DEBUG(DEB_LEV_ERR, "the library %s is not compatible with ST static component loader - %s\n", libname, dlerror());
			} else {
        num_of_comp = (int)(*fptr)(NULL);
				stComponentsTemp = calloc(num_of_comp,sizeof(stLoaderComponentType*));
				for (i = 0; i<num_of_comp; i++) {
					stComponentsTemp[i] = calloc(1,sizeof(stLoaderComponentType));
				}
				(*fptr)(stComponentsTemp);
				for (i = 0; i<num_of_comp; i++) {
					templateList[listindex + i] = stComponentsTemp[i];
          DEBUG(DEB_LEV_ERR, "In %s comp name[%d]=%s\n",__func__,listindex + i,templateList[listindex + i]->name);
				}
        free(stComponentsTemp);
        listindex+= i;
				if (listindex >= MAXCOMPONENTS) {
					DEBUG(DEB_LEV_ERR, "Reached the maximum number of component allowed. No more components are registered");
					break;
				}
			}
		}
	}
	if(line)
		free(line);
	free(libname);
	fclose(omxregistryfp);
  *loaderHandle = templateList;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief The destructor of the ST specific component loader. 
 * 
 * This function deallocates the list of available components.
 */
OMX_ERRORTYPE BOSA_ST_DestroyComponentLoader(BOSA_ComponentLoaderHandle loaderHandle) {
	int i,j;
	stLoaderComponentType** templateList;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	templateList = (stLoaderComponentType**)loaderHandle;
 
  DEBUG(DEB_LEV_ERR, "In %s listindex=%d\n", __func__,listindex);

	//for(i = 0; i<MAXCOMPONENTS; i++) {
  for(i = 0; i<listindex; i++) {
		if (templateList[i]) {
      DEBUG(DEB_LEV_ERR, "In %s i=%d\n", __func__,i);

      //TODO: Giving segmentation fault.
      
      if(templateList[i]->name_requested){
        free(templateList[i]->name_requested);
        templateList[i]->name_requested=NULL;
      }
      
      for(j=0;j<templateList[i]->name_specific_length;j++){
        if(templateList[i]->name_specific[j]) {
          free(templateList[i]->name_specific[j]);
          templateList[i]->name_specific[j]=NULL;
        }
        if(templateList[i]->role_specific[j]){
          free(templateList[i]->role_specific[j]);
          templateList[i]->role_specific[j]=NULL;
        }
      }

      if(templateList[i]->name_specific){
        free(templateList[i]->name_specific);	
        templateList[i]->name_specific=NULL;
      }
      if(templateList[i]->role_specific){
        free(templateList[i]->role_specific);
        templateList[i]->role_specific=NULL;
      }
      if(templateList[i]->name){
        free(templateList[i]->name);
        templateList[i]->name=NULL;
      }
      
      
      free(templateList[i]);
			templateList[i] = NULL;
		}
	}
  
  if(templateList) {
    free(templateList);
    templateList=NULL;
  }
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief creator of the requested openmax component
 * 
 * This function searches for the requested component in the internal list.
 * If the copmponent is found, its constructor is called,
 * and the standard callback are assigned.
 * A pointer to a standard openmax component is returned.
 */
OMX_ERRORTYPE BOSA_ST_CreateComponent(
	BOSA_ComponentLoaderHandle loaderHandle,
	OMX_OUT OMX_HANDLETYPE* pHandle,
	OMX_IN  OMX_STRING cComponentName,
	OMX_IN  OMX_PTR pAppData,
	OMX_IN  OMX_CALLBACKTYPE* pCallBacks) {
	
	int i, j, err = OMX_ErrorNone;
	int componentPosition = -1;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	stLoaderComponentType** templateList;
	OMX_COMPONENTTYPE *openmaxStandComp;
		
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	templateList = (stLoaderComponentType**)loaderHandle;
	for(i=0;i<MAXCOMPONENTS;i++) {
		if(templateList[i]) {
			if(!strcmp(templateList[i]->name, cComponentName)) {
			//given component name matches with the general component names
				componentPosition = i;
				break;
			} else {
				for(j=0;j<templateList[i]->name_specific_length;j++) {
					if(!strcmp(templateList[i]->name_specific[j], cComponentName)) {
						//given component name matches with specific component names
            componentPosition = i;
						break;
					}
				}
        if(componentPosition != -1)
						break;
			}
		}
    else {
      DEBUG(DEB_LEV_ERR, "In %s Template List Null\n", __func__);
      exit(1);
    }
	}
	if (componentPosition == MAXCOMPONENTS) {
		DEBUG(DEB_LEV_ERR, "Component not fount with current ST static component loader.\n");
		return OMX_ErrorComponentNotFound;
	}

	//component name matches with general component name field
	DEBUG(DEB_LEV_PARAMS, "Found base requested template %s\n", cComponentName);
	/* Build ST component from template and fill fields */
	if (templateList[componentPosition]->name_requested == NULL) {
		templateList[componentPosition]->name_requested = calloc(1,sizeof(char) * OMX_MAX_STRINGNAME_SIZE);
	}
	memcpy(templateList[componentPosition]->name_requested, cComponentName, OMX_MAX_STRINGNAME_SIZE * sizeof(char));

	openmaxStandComp = (OMX_COMPONENTTYPE*)calloc(1,sizeof(OMX_COMPONENTTYPE));
	if (!openmaxStandComp) {
		return OMX_ErrorInsufficientResources;
	}
	eError=templateList[componentPosition]->constructor(openmaxStandComp,cComponentName);
	*pHandle = openmaxStandComp;
	((OMX_COMPONENTTYPE*)*pHandle)->SetCallbacks(*pHandle, pCallBacks, pAppData);
	DEBUG(DEB_LEV_FULL_SEQ, "Template %s found returning from OMX_GetHandle\n", cComponentName);
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return eError;
}

/** @brief This function simply de-allocates the memory 
 * allocated by the CreteComponent function
 */
OMX_ERRORTYPE BOSA_ST_DestroyComponent(
		BOSA_ComponentLoaderHandle loaderHandle,
		OMX_HANDLETYPE pHandle) {
	int i, j;
  
  ((OMX_COMPONENTTYPE*)pHandle)->ComponentDeInit(pHandle);

  //TODO: Giving segmentation fault with test harness;
  //free((OMX_COMPONENTTYPE*)pHandle);
  /*
	stLoaderComponentType** templateList;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	templateList = (stLoaderComponentType**)loaderHandle;
	componentPrivateData = (base_component_PrivateType*)pHandle->pComponentPrivate;
	if ((componentPrivateData == NULL) || (componentPrivateData->uniqueID != ST_STATIC_COMP_CODE)) {
		//the component is not handled by this component loader
		return OMX_ErrorComponentNotFound;
	}
	for (i = 0; i<MAXCOMPONENTS; i++) {
		if (templateList[i]) {
			if (!strcmp(componentPrivateData->name, templateList[i]->name)) {
				templateList[i]->destructor(pHandle);
			} else {
				for(j=0;j<templateList[i]->name_specific_length;j++) {
					if(!strcmp(templateList[i]->name_specific[j], componentPrivateData->name)) {
						templateList[i]->destructor(pHandle);
					}
				}
			}
		}
	}
	free(pHandle);
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
  */
	return OMX_ErrorNone;
}

/** @brief This function search for the index from 0 to end of the list
 * 
 * This function searches in the list of ST static components and enumerates
 * both the class names and the role specific components.
 */ 
OMX_ERRORTYPE BOSA_ST_ComponentNameEnum(
	BOSA_ComponentLoaderHandle loaderHandle,
	OMX_STRING cComponentName,
	OMX_U32 nNameLength,
	OMX_U32 nIndex) {
	
	stLoaderComponentType** templateList;
	int i, j, index = 0;
	int found = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
		
	templateList = (stLoaderComponentType**)loaderHandle;
	i = 0;
	while(templateList[i]) {
		DEBUG(DEB_LEV_FUNCTION_NAME, "Inside while loop %s %i %i\n", __func__, index, nIndex);
		if (index == nIndex) {
			strncpy(cComponentName, templateList[i]->name, nNameLength);
			found = 1;
			break;
		}
		index++;
		for (j = 0; j<templateList[i]->name_specific_length;j++) {
      DEBUG(DEB_LEV_FUNCTION_NAME, "Inside for loop %s %i %i\n", __func__, index, nIndex);
			if (index == nIndex) {
				strncpy(cComponentName,templateList[i]->name_specific[j], nNameLength);
				found = 1;
				break;
			}
			index++;
		}
		i++;
	}
	if (!found) {
		DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s not found\n", __func__);
		return OMX_ErrorNoMore;
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief The specific version of OMX_GetRolesOfComponent 
 * 
 * This function replicates exactly the behavior of the 
 * standard OMX_GetRolesOfComponent function for the ST static
 * component loader 
 */
OMX_ERRORTYPE BOSA_ST_GetRolesOfComponent( 
	BOSA_ComponentLoaderHandle loaderHandle,
	OMX_STRING compName,
	OMX_U32 *pNumRoles,
	OMX_U8 **roles) {
	
	stLoaderComponentType** templateList;
	int i, j,index;
	int max_roles = *pNumRoles;
	int found = 0;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	templateList = (stLoaderComponentType**)loaderHandle;
	
	for(i = 0; i<MAXCOMPONENTS; i++) {
		if (templateList[i]) {
			if(!strcmp(templateList[i]->name, compName)) {
				DEBUG(DEB_LEV_SIMPLE_SEQ, "Found requested template %s IN GENERAL COMPONENT\n", compName);
				// set the no of roles field
				*pNumRoles = templateList[i]->name_specific_length;
				if(roles == NULL) {
					return OMX_ErrorNone;
				}
				//append the roles
				for (index = 0; index < templateList[i]->name_specific_length; index++) {
					if (index < max_roles) {
						strcpy (*(roles+index), templateList[i]->role_specific[index]);
					}
				}
				found = 1;
			} else {
				for(j=0;j<templateList[i]->name_specific_length;j++) {
					if(!strcmp(templateList[i]-> name_specific[j], compName)) {
						DEBUG(DEB_LEV_SIMPLE_SEQ, "Found requested component %s IN SPECIFIC COMPONENT \n", compName);
						*pNumRoles = 1;
						found = 1;
						if(roles == NULL) {
							return OMX_ErrorNone;
						}
						if (max_roles > 0) {
							strcpy (*roles , templateList[i]->role_specific[j]);
						}
					}
				}
			}
		}
		if(found) {
			break;
		}
	}
	if(i == MAXCOMPONENTS) {
		DEBUG(DEB_LEV_ERR, "no component match in whole template list has been found\n");
		*pNumRoles = 0;
		return OMX_ErrorComponentNotFound;
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}

/** @brief The specific version of OMX_GetComponentsOfRole 
 * 
 * This function replicates exactly the behavior of the 
 * standard OMX_GetComponentsOfRole function for the ST static
 * component loader 
 */
OMX_API OMX_ERRORTYPE BOSA_ST_GetComponentsOfRole ( 
	BOSA_ComponentLoaderHandle loaderHandle,
	OMX_STRING role,
	OMX_U32 *pNumComps,
	OMX_U8  **compNames) {
	
	stLoaderComponentType** templateList;
	int i,j;
	int num_comp = 0;
	int max_entries = *pNumComps;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	templateList = (stLoaderComponentType**)loaderHandle;
  for(i=0;i<MAXCOMPONENTS;i++) {
		if(templateList[i]) {
			for (j = 0; j<templateList[i]->name_specific_length; j++) {
			  if (!strcmp(templateList[i]->role_specific[j], role)) {
					if (compNames != NULL) {
						if (num_comp < max_entries) {
							strcpy(((*compNames)+num_comp), templateList[i]->name);
						}
					}
					num_comp++;
				}
			}
		}
	}
	*pNumComps = num_comp;
	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);
	return OMX_ErrorNone;
}
