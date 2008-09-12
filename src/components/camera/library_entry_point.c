/**
  @file src/components/camera/library_entry_point.c

  The library entry point. It must have the same name for each
  library of the components loaded by the ST static component loader.
  This function fills the version, the component name and if existing also the roles
  and the specific names for each role. This base function is only an explanation.
  For each library it must be implemented, and it must fill data of any component
  in the library

  Copyright (C) 2007-2008  Motorola and STMicroelectronics

  This code is licensed under LGPL see README for full LGPL notice.

  Date                             Author                Comment
  Mon, 09 Jul 2007                 Motorola              File created

  This Program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  $Date$
  Revision $Rev$
  Author $Author$

*/


#include <st_static_component_loader.h>
#include <omx_camera_source_component.h>
#include <string.h>

/** @brief The library entry point. It must have the same name for each
  * library of the components loaded by the ST static component loader.
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
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for camera component\n",__func__);

  if (stComponents == NULL) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: %d\n",__func__, 1);
    return 1; /* Return Number of Components - one for camera source component */
  }

  stComponents[0]->componentVersion.s.nVersionMajor = 1;
  stComponents[0]->componentVersion.s.nVersionMinor = 1;
  stComponents[0]->componentVersion.s.nRevision = 1;
  stComponents[0]->componentVersion.s.nStep = 1;

  /* component name */
  stComponents[0]->name = calloc(1, OMX_MAX_STRINGNAME_SIZE);
  if (stComponents[0]->name == NULL) {
    err = OMX_ErrorInsufficientResources;
    goto ERR_HANDLE;
  }
  strncpy(stComponents[0]->name, "OMX.st.v4l.camera_source", OMX_MAX_STRINGNAME_SIZE);
  stComponents[0]->name[OMX_MAX_STRINGNAME_SIZE-1] = '\0';

  /* specific names */
  stComponents[0]->name_specific_length = 1;
  stComponents[0]->name_specific = calloc(stComponents[0]->name_specific_length, sizeof(char *));
  if (stComponents[0]->name_specific == NULL) {
    err = OMX_ErrorInsufficientResources;
    goto ERR_HANDLE;
  }

  for (i=0;i<stComponents[0]->name_specific_length;i++) {
    stComponents[0]->name_specific[i] = calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->name_specific[i] == NULL) {
      err = OMX_ErrorInsufficientResources;
      goto ERR_HANDLE;
    }
  }

  strncpy(stComponents[0]->name_specific[0], "OMX.st.v4l.camera_source", OMX_MAX_STRINGNAME_SIZE);
  stComponents[0]->name_specific[0][OMX_MAX_STRINGNAME_SIZE-1] = '\0';

  /* component roles */
  stComponents[0]->role_specific = calloc(stComponents[0]->name_specific_length, sizeof(char *));
  if (stComponents[0]->role_specific == NULL) {
    err = OMX_ErrorInsufficientResources;
    goto ERR_HANDLE;
  }

  for (i=0;i<stComponents[0]->name_specific_length;i++) {
    stComponents[0]->role_specific[i] = calloc(1, OMX_MAX_STRINGNAME_SIZE);
    if (stComponents[0]->role_specific[i] == NULL) {
      err = OMX_ErrorInsufficientResources;
      goto ERR_HANDLE;
    }
  }

  strncpy(stComponents[0]->role_specific[0], "v4l.camera_source", OMX_MAX_STRINGNAME_SIZE);
  stComponents[0]->role_specific[0][OMX_MAX_STRINGNAME_SIZE-1] = '\0';

  /* component constructor */
  stComponents[0]->constructor = omx_camera_source_component_Constructor;

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: %d\n",__func__, 1);
  return 1; /* Return Number of Components - one for camera source component */

ERR_HANDLE:
  if (stComponents[0]->name != NULL) {
    free(stComponents[0]->name);
    stComponents[0]->name = NULL;
  }

  if (stComponents[0]->name_specific != NULL) {
    for (i=0;i<stComponents[0]->name_specific_length;i++) {
      if (stComponents[0]->name_specific[i] != NULL) {
        free(stComponents[0]->name_specific[i]);
      }
    }
    free(stComponents[0]->name_specific);
    stComponents[0]->name_specific = NULL;
  }

  if (stComponents[0]->role_specific != NULL) {
    for (i=0;i<stComponents[0]->name_specific_length;i++) {
      if (stComponents[0]->role_specific[i] != NULL) {
        free(stComponents[0]->role_specific[i]);
      }
    }
    free(stComponents[0]->role_specific);
    stComponents[0]->role_specific = NULL;
  }


  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for camera component, return code: 0x%X\n",__func__, err);
  return err;
}
