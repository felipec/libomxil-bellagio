/**
	@file src/omxregister.c
	
	Register OpenMAX components. This application registers the installed OpenMAX
	components and stores the list in the file:
	$HOME/.omxregistry
	
	It must be run before using components.
	The components are searched in the default directory:
	OMXILCOMPONENTSPATH
	If the components are installed in a different location, specify:
	
	omxregister installation_path
	
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

#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "st_static_component_loader.h"

/** String element to be put in the .omxregistry file to indicate  an
 * OpenMAX component and its roles
 */
static const char arrow[] =  " ==> ";

/** registry filename
 */
static const char registry_filename[] = "/.omxregistry";

/** @brief Creates a list of components on a registry file
 * 
 * This function 
 *  - reads for the given directory any library contained
 *  - check if the library belong to OpenMAX ST static component loader
 *    (it must contain the function omx_component_library_Setup for the initialization)
 *  - write the openmax names and related libraries to the registry file
 */
static int buildComponentsList(char *componentspath, int *ncomponents, int *nroles, int verbose) {
  DIR *dirp;
	struct dirent *dp;
	void *handle;
	int i, num_of_comp;
	int j;
	FILE *omxregistryfp;
	char *omxregistryfile;
	char *buffer;
	int (*fptr)(void *);
	stLoaderComponentType **stComponents;
	*ncomponents = 0;
	*nroles=0;

	if((buffer=getenv("HOME"))) {
		omxregistryfile = malloc(strlen(buffer) + sizeof(registry_filename) + 1);
		strcpy(omxregistryfile, buffer);
		buffer = NULL;
	} else {
		omxregistryfile  = malloc(sizeof(registry_filename) + 1);
		*omxregistryfile = 0;
	}
	strcat(omxregistryfile, registry_filename);

	/* Populate the registry file */
	dirp = opendir(componentspath);
	if(dirp == NULL){
    int err = errno;
		DEBUG(DEB_LEV_ERR, "Cannot open directory %s\n", componentspath);
		return err;
	}

	omxregistryfp = fopen(omxregistryfile, "w");
	if (omxregistryfp == NULL){
    int err = errno;
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file %s\n", omxregistryfile);
    closedir(dirp);
		return err;
	}
	
	while((dp = readdir(dirp)) != NULL) {
    int len = strlen(dp->d_name);

		if(len >= 3){

      
      if(strncmp(dp->d_name+len-3, ".so", 3) == 0){
        char lib_absolute_path[strlen(componentspath) + len + 1];

				strcpy(lib_absolute_path, componentspath);
				strcat(lib_absolute_path, dp->d_name);

				if((handle = dlopen(lib_absolute_path, RTLD_NOW)) == NULL) {
					DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", lib_absolute_path, dlerror());
				} else {
					if ((fptr = dlsym(handle, "omx_component_library_Setup")) == NULL) {
						DEBUG(DEB_LEV_ERR, "the library %s is not compatible with ST static component loader - %s\n", lib_absolute_path, dlerror());
						continue;
					}
					num_of_comp = fptr(NULL);
					stComponents = malloc(num_of_comp * sizeof(stLoaderComponentType*));
					for (i = 0; i<num_of_comp; i++) {
						stComponents[i] = malloc(sizeof(stLoaderComponentType));
					}
					fptr(stComponents);
					
					fwrite(lib_absolute_path, 1, strlen(lib_absolute_path), omxregistryfp);
					fwrite("\n", 1, 1, omxregistryfp);


					for (i = 0; i<num_of_comp; i++) {
						DEBUG(DEB_LEV_PARAMS, "Found component %s version=%d.%d.%d.%d in shared object %s\n",
								stComponents[i]->name,
								stComponents[i]->componentVersion.s.nVersionMajor,
								stComponents[i]->componentVersion.s.nVersionMinor,
								stComponents[i]->componentVersion.s.nRevision,
								stComponents[i]->componentVersion.s.nStep, 
								lib_absolute_path);
						if (verbose)
              printf("Component %s registered\n", stComponents[i]->name);

						// allocate max memory
            len = sizeof(arrow)                 /* arrow */
                 +strlen(stComponents[i]->name) /* component name */
                 +sizeof(arrow)                 /* arrow */
                 +1                             /* '\n' */
                 +1                             /* '\0' */;
            buffer = realloc(buffer, len);

            // insert first of all the name of the library
						strcpy(buffer, arrow);
						strcat(buffer, stComponents[i]->name);

						if (stComponents[i]->name_specific_length>0) {
							*nroles += stComponents[i]->name_specific_length;
							strcat(buffer, arrow);
							for(j=0;j<stComponents[i]->name_specific_length;j++){
								if (verbose)
                  printf("  Specific role %s registered\n", stComponents[i]->name_specific[j]);
                len += strlen(stComponents[i]->name_specific[j]) /* specific role */
                      +1                                         /* ':' */;
                buffer = realloc(buffer, len);
								strcat(buffer, stComponents[i]->name_specific[j]);
								strcat(buffer, ":");
							}
						}
						strcat(buffer, "\n");
						fwrite(buffer, 1, strlen(buffer), omxregistryfp);
						(*ncomponents)++;
					}
					for (i = 0; i < num_of_comp; i++) {
						free(stComponents[i]);
					}
					free(stComponents);
				}
			}
		}
	}
	free(omxregistryfile);
	free(buffer);
	fclose(omxregistryfp);
	closedir(dirp);
	return 0;
}

void usage() {
	printf(
      "Usage: omxregister [-v] [-h] [componentspath]\n"
      "\n"
      "This programs scans for a given directory searching for any OpenMAX component\n"
      " compatible with the ST static component loader. The list of components is\n"
      " stored in a registry file located in the $HOME directory and named .omxregistry\n"
      "\n"
      "The following options are supported:\n"
      "\n"
      "        -v   display a verbose output, listing all the components registered\n"
      "        -h   display this message\n"
      "         componentspath: a searching path for components can be specified.\n"
      "         If this parameter is omitted, the components are searched in the\n"
      "         default %s directory\n"
      "\n"
      , OMXILCOMPONENTSPATH
      );
}

/** @brief execution of registration function
 * 
 * This register by default searches for openmax libraries in OMXILCOMPONENTSPATH
 * If specified it can search in a different directory
 */
int main(int argc, char *argv[]) {
	char *componentspath = OMXILCOMPONENTSPATH;
	int ncomponents,nroles;
	int err;
  	int verbose=0;
	
	for(err = 1; err < argc; err++) {
		if(*(argv[err]) == '-') {
			if (*(argv[err]+1) == 'v') {
				verbose = 1;
			} else {
				usage();
				exit(*(argv[err]+1) == 'h' ? 0 : -EINVAL);
			}
		} else {
			componentspath = argv[err];
    }
  }

	err = buildComponentsList(componentspath, &ncomponents, &nroles, verbose);
	if(err) {
		DEBUG(DEB_LEV_ERR, "Error registering OpenMAX components with ST static component loader %s\n", strerror(err));
	} else {
		if (verbose) {
			printf("\n %i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
		} else {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "\n %i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
		}
	}
	return 0;
}
