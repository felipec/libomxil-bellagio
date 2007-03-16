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

#include <st_static_component_loader.h>
#include <omx_audiodec_component.h>

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
	strcpy(stComponents[0]->name, "OMX.st.audio_decoder");
	stComponents[0]->name_specific_length = 2;
	stComponents[0]->constructor = omx_audiodec_component_Constructor;	
	stComponents[0]->destructor = omx_audiodec_component_Destructor;	
	
  	stComponents[0]->name_specific = (char **)calloc(stComponents[0]->name_specific_length,sizeof(char *));	
	stComponents[0]->role_specific = (char **)calloc(stComponents[0]->name_specific_length,sizeof(char *));	

	for(i=0;i<stComponents[0]->name_specific_length;i++) {
		stComponents[0]->name_specific[i] = (char* )calloc(1,OMX_MAX_STRINGNAME_SIZE);
		if (stComponents[0]->name_specific[i] == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	}
	for(i=0;i<stComponents[0]->name_specific_length;i++) {
		stComponents[0]->role_specific[i] = (char* )calloc(1,OMX_MAX_STRINGNAME_SIZE);
		if (stComponents[0]->role_specific[i] == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	}

	strcpy(stComponents[0]->name_specific[0], "OMX.st.audio_decoder.mp3");
	strcpy(stComponents[0]->name_specific[1], "OMX.st.audio_decoder.wma");
	strcpy(stComponents[0]->role_specific[0], "audio_decoder.mp3");
	strcpy(stComponents[0]->role_specific[1], "audio_decoder.wma");

  	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s \n",__func__);
	return 1;
}
