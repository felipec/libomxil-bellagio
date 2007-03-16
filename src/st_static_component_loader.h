/**
	@file src/st_omx_component_loader.h
	
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
	
	$Date:2007-01-17 13:28:20 +0100 (Wed, 17 Jan 2007) $
	Revision $Rev:606 $
	Author $Author:giulio_urlini $
*/

#ifndef __ST_OMX_COMPONENT_LOADER_H__
#define __ST_OMX_COMPONENT_LOADER_H__

#include "omxcore.h"

/** Define the maximum number of component that can handled by the ST loader
 * and the size of the full lisdt of component available for this loader
 */
#define MAXCOMPONENTS 128

/** @brief the private data structure handled by the ST static loader that described
 * an openMAX component
 * 
 * This structure contains all the fields that the loader must use to support 
 * the loading  unloading functions of the component, that are not described by the
 * standard.
 */
typedef struct stLoaderComponentType{
	OMX_VERSIONTYPE componentVersion; /**< the verison of the component in the OpenMAX standard format */
	char* name; /**< String that represents the name of the component, ruled by the standard */
	int name_specific_length;/**< this field contains the number of roles of the component */
	char** name_specific; /**< Strings those represent the names of the specifc format components */
	char** role_specific; /**< Strings those represent the names of the specifc format components */
	char* name_requested; /**< This parameter is used to send to the component the string requested by the IL Client */
	OMX_ERRORTYPE (*constructor)(OMX_COMPONENTTYPE*,OMX_STRING cComponentName); /**< constructor function pointer for each Linux ST OpenMAX component */
	OMX_ERRORTYPE (*destructor)(OMX_COMPONENTTYPE*); /**< constructor function pointer for each Linux ST OpenMAX component */
} stLoaderComponentType;

/** @brief The initialization of the ST specific component loader. 
 * 
 * This function allocates memory for the component loader and initialize other function pointer
 */
OMX_ERRORTYPE BOSA_ST_InitComponentLoader(BOSA_COMPONENTLOADER** componentLoader);

/** @brief The destructor of the ST specific component loader. 
 * 
 * This function frees component loader
 */
OMX_ERRORTYPE BOSA_ST_DeinitComponentLoader(BOSA_COMPONENTLOADER *componentLoader);

/** @brief The constructor of the ST specific component loader. 
 * 
 * It is the component loader developed under linux by ST, for local libraries.
 * It is based on a registry file, like in the case of Gstreamer. It reads the 
 * registry file, and allows the components to register themself to the 
 * main list templateList.
 */
OMX_ERRORTYPE BOSA_ST_CreateComponentLoader(BOSA_ComponentLoaderHandle *loaderHandle);

/** @brief The destructor of the ST specific component loader. 
 * 
 * This function deallocates the list of available components.
 */
OMX_ERRORTYPE BOSA_ST_DestroyComponentLoader(BOSA_ComponentLoaderHandle loaderHandle);

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
		OMX_IN  OMX_CALLBACKTYPE* pCallBacks);

/** @brief This function simply de-allocates the memory 
 * allocated by the CreteComponent function
 */
OMX_ERRORTYPE BOSA_ST_DestroyComponent(
		BOSA_ComponentLoaderHandle loaderHandle,
		OMX_HANDLETYPE pHandle);

/** @brief This function search for the index from 0 to end of the list
 * 
 * This function searches in the list of ST static components and enumerates
 * both the class names and the role specific components.
 */ 
OMX_ERRORTYPE BOSA_ST_ComponentNameEnum(
		BOSA_ComponentLoaderHandle loaderHandle,
		OMX_STRING cComponentName,
		OMX_U32 nNameLength,
		OMX_U32 nIndex);

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
		OMX_U8 **roles);

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
		OMX_U8  **compNames);

/** @brief The library entry point. It must have the same name for each
 * library fo the components loaded by th ST static component loader.
 * 
 * This function fills the version, the component name and if existing also the roles
 * and the specific names for each role. This base function is only an explanation.
 * For each library it must be implemented, and it must fill data of any component
 * in the library
 * 
 * @param stComponents pointer to an array of components descriptors.If NULL, the 
 * function will return only the number of components contained in the library
 * 
 * @return number of components contained in the library 

int omx_component_library_Setup(stLoaderComponentType **stComponents) {
	//int i;
	if (stComponents == NULL) {
		return  2;// number of components in this library 
	}
	*stComponents[0]->componentVersion.s.nVersionMajor = 1; 
	*stComponents[0]->componentVersion.s.nVersionMinor = 1; 
	*stComponents[0]->componentVersion.s.nRevision = 1;
	*stComponents[0]->componentVersion.s.nStep = 1;

	*stComponent[0]->name = (char* )malloc(OMX_MAX_STRINGNAME_SIZE);
	if (*stComponent[0]->name == NULL) {
		return OMX_ErrorInsufficientResources;
	}
	strcpy(*stComponent[0]->name, "OMX.st.first_component");
	*stComponents[0]->name_specific_length = 0;
	*stComponents[0]->constructor = omx_first_component_constructor;	
	*stComponents[0]->destructor = omx_first_component_destructor;	
	

	*stComponent[1]->componentVersion.s.nVersionMajor = 1; 
	*stComponent[1]->componentVersion.s.nVersionMinor = 1; 
	*stComponent[1]->componentVersion.s.nRevision = 1;
	*stComponent[1]->componentVersion.s.nStep = 1;

	*stComponent[1]->name = (char* )malloc(OMX_MAX_STRINGNAME_SIZE);
	if (*stComponent[1]->name == NULL) {
		return OMX_ErrorInsufficientResources;
	}
	strcpy(*stComponent[1]->name, "OMX.st.second_component");
	*stComponent[1]->name_specific_length = 2;
	*stComponent[1]->name_specific = (char **)malloc(sizeof(char *)**stComponent[1]->name_specific_length);	
	*stComponent[1]->role_specific = (char **)malloc(sizeof(char *)**stComponent[1]->name_specific_length);	

	for(i=0;i<*stComponent[1]->name_specific_length;i++) {
		*stComponent[1]->name_specific[i] = (char* )malloc(OMX_MAX_STRINGNAME_SIZE);
		if (*stComponent[1]->name_specific[i] == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	}
	for(i=0;i<*stComponent[1]->name_specific_length;i++) {
		*stComponent[1]->role_specific[i] = (char* )malloc(OMX_MAX_STRINGNAME_SIZE);
		if (*stComponent[1]->role_specific[i] == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	}

	strcpy(*stComponent[1]->name_specific[0], "OMX.st.second_component.first_specific");
	strcpy(*stComponent[1]->name_specific[1], "OMX.st.second_component.second_specific");
	strcpy(*stComponent[1]->role_specific[0], "first_role");
	strcpy(*stComponent[1]->role_specific[1], "second_role");

	*stComponent[1]->constructor = omx_second_component_constructor;	
	*stComponent[1]->destructor = omx_second_component_destructor;
	return 2;
} */

#endif
