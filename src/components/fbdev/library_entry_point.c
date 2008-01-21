/**
	@file src/components/fbdev/library_entry_point.c
	
	The library entry point. It must have the same name for each
	library fo the components loaded by th ST static component loader.
	This function fills the version, the component name and if existing also the roles
	and the specific names for each role. This base function is only an explanation.
	For each library it must be implemented, and it must fill data of any component
	in the library
	
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


#include <st_static_component_loader.h>
#include <omx_fbdev_sink_component.h>

/** @brief The library entry point. It must have the same name for each
  * library fo the components loaded by the ST static component loader.
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
*/
int omx_component_library_Setup(stLoaderComponentType **stComponents) {
  int i;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if (stComponents == NULL) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s \n",__func__);
    return 1; // Return Number of Components - one for fbdev sink component
  }

  /** component 1 - fbdev sink component */
  stComponents[0]->componentVersion.s.nVersionMajor = 1; 
  stComponents[0]->componentVersion.s.nVersionMinor = 1; 
  stComponents[0]->componentVersion.s.nRevision = 1;
  stComponents[0]->componentVersion.s.nStep = 1;

  stComponents[0]->name = calloc(1, OMX_MAX_STRINGNAME_SIZE);
  if (stComponents[0]->name == NULL) {
    return OMX_ErrorInsufficientResources;
  }
  strcpy(stComponents[0]->name, "OMX.st.fbdev.fbdev_sink");
  stComponents[0]->name_specific_length = 1; 
  stComponents[0]->constructor = omx_fbdev_sink_component_Constructor;	

  stComponents[0]->name_specific = calloc(stComponents[0]->name_specific_length,sizeof(char *));	
  stComponents[0]->role_specific = calloc(stComponents[0]->name_specific_length,sizeof(char *));	

  for(i=0;i<stComponents[0]->name_specific_length;i++) {
    stComponents[0]->name_specific[i] = calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->name_specific[i] == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }
  for(i=0;i<stComponents[0]->name_specific_length;i++) {
    stComponents[0]->role_specific[i] = calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->role_specific[i] == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  }
  strcpy(stComponents[0]->name_specific[0], "OMX.st.fbdev.fbdev_sink");
  strcpy(stComponents[0]->role_specific[0], "fbdev.fbdev_sink");

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s \n",__func__);
  return 1; 
}
