/**
	@file src/components/filereader/library_entry_point.c
	
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
	
	$Date: 2007-05-22 14:43:04 +0200 (Tue, 22 May 2007) $
	Revision $Rev: 875 $
	Author $Author: giulio_urlini $

*/

#include <st_static_component_loader.h>
#include <omx_filereader_component.h>

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
 */
int omx_component_library_Setup(stLoaderComponentType **stComponents) {
  
  int i;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if (stComponents == NULL) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s \n",__func__);
    return 1; // Return Number of Component/s
  }

  stComponents[0]->componentVersion.s.nVersionMajor = 1; 
  stComponents[0]->componentVersion.s.nVersionMinor = 1; 
  stComponents[0]->componentVersion.s.nRevision = 1;
  stComponents[0]->componentVersion.s.nStep = 1;

  stComponents[0]->name = (char* )calloc(1,OMX_MAX_STRINGNAME_SIZE);
  if (stComponents[0]->name == NULL) {
    return OMX_ErrorInsufficientResources;
  }

  strcpy(stComponents[0]->name, "OMX.st.audio_filereader");
  stComponents[0]->name_specific_length = 0; 
  stComponents[0]->constructor = omx_filereader_component_Constructor;	
  stComponents[0]->destructor = omx_filereader_component_Destructor;	

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s \n",__func__);
  return 1;
}

