/**
	@file src/omxregister.c
	
	Register OpenMAX components. This application registers the installed OpenMAX
	components and stores the list in the file:
	$HOME/.omxregistry
	
	It must be run before using components.
	The components are searched in the default directory:
	/usr/local/lib/omxcomponents/
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

#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

#include "st_static_component_loader.h"

/** String element to be put in the .omxregistry file to indicate  an
 * OpenMAX component and its roles
 */
#define ARROW " ==> "

/** @brief Creates a list of components on a registry file
 * 
 * This function 
 *  - reads for the given directory any library contained
 *  - check if the library belong to OpenMAX ST static component loader
 *    (it must contain the function omx_component_library_Setup for the initialization)
 *  - write the openmax names and related libraries to the registry file
 */
int buildComponentsList(char* componentspath, int* ncomponents,int *nroles,int verbose) {
	DIR *dirp = NULL;
	struct dirent *dp;
	
  void* handle;
	int i = 0,num_of_comp;
	int j;
	FILE* omxregistryfp;
	char omxregistryfile[200];
  char *buffer; // TODO :Verify what should be buffer type
  int (*fptr)(void *);
	stLoaderComponentType **stComponents;
	*ncomponents = 0;
	*nroles=0;

	memset(omxregistryfile, 0, sizeof(omxregistryfile));
	strcat(omxregistryfile, getenv("HOME"));
	strcat(omxregistryfile, "/.omxregistry");

  buffer = (char*)malloc(sizeof(char)*256);
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
	
	while((dp = readdir(dirp)) != NULL) {
		int len;
		char ext[4];

		len = strlen(dp->d_name);

		if(len >= 3){
			strncpy(ext, &(dp->d_name[len-3]), 3);
			ext[4]='\0';
    
			if(strncmp(ext, ".so", 3) == 0){
				char lib_absolute_path[200];

				strcpy(lib_absolute_path, componentspath);
				strcat(lib_absolute_path, dp->d_name);

				if((handle = dlopen(lib_absolute_path, RTLD_NOW)) == NULL) {
					DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", lib_absolute_path, dlerror());
				} else {
					if ((*(void **)(&fptr) = dlsym(handle, "omx_component_library_Setup")) == NULL) {
						DEBUG(DEB_LEV_ERR, "the library %s is not compatible with ST static component loader - %s\n", lib_absolute_path, dlerror());
					} else {
						num_of_comp = (int)(*fptr)(NULL);
						stComponents = malloc(num_of_comp * sizeof(stLoaderComponentType*));
						for (i = 0; i<num_of_comp; i++) {
							stComponents[i] = malloc(sizeof(stLoaderComponentType));
						}
						(*fptr)(stComponents);

						memset(buffer, 0, sizeof(buffer));
						// insert first of all the name of the library
						strcat(buffer, lib_absolute_path);
						for (i = 0; i<num_of_comp; i++) {
							DEBUG(DEB_LEV_PARAMS, "Found component %s version=%d.%d.%d.%d in shared object %s\n",
									stComponents[i]-> name,
									stComponents[i]->componentVersion.s.nVersionMajor,
									stComponents[i]->componentVersion.s.nVersionMinor,
									stComponents[i]->componentVersion.s.nRevision,
									stComponents[i]->componentVersion.s.nStep, 
									lib_absolute_path);
							strcat(buffer, "\n");
							// insert any name of component
							strcat(buffer, ARROW);
							if (verbose) {printf("Component %s registered\n", stComponents[i]->name);}
							strcat(buffer, stComponents[i]->name);
							if (stComponents[i]->name_specific_length>0) {
								*nroles += stComponents[i]->name_specific_length;
								strcat(buffer, ARROW);
								for(j=0;j<stComponents[i]->name_specific_length;j++){
									if (verbose) {printf("Specific role %s registered\n", stComponents[i]->name_specific[j]);}
									strcat(buffer, stComponents[i]->name_specific[j]);
									strcat(buffer, ":");
								}
							}
							strcat(buffer, "\n");
							fwrite(buffer, 1, strlen(buffer), omxregistryfp);
							(*ncomponents)++;
						}
					}
				}
			}
		}
	}

  free(buffer);
	buffer = NULL;
	fclose(omxregistryfp);
	return 0;
}

/** @brief execution of registration function
 * 
 * This register by default searches for openmax libraries in OMXILCOMPONENTSPATH
 * If specified it can search in a different directory
 */
int main(int argc, char** argv) {
	char* componentspath;
	int ncomponents,nroles;
	int err;
  int verbose=0;
	
	if(argc == 1){
		componentspath = OMXILCOMPONENTSPATH;
	}else if(argc >= 2) {
		if(*(argv[1]) == '-') {
			if (*(argv[1]+1) == 'v') {
				verbose = 1;
				if (argc > 2) {
					componentspath = argv[2];
				} else {
					componentspath = OMXILCOMPONENTSPATH;
				}
			} else if (*(argv[1]+1) == 'h') {
				DEBUG(DEB_LEV_ERR, "Usage: %s [-v] [-h] [componentspath]\n", argv[0]);
				exit(0);
			} else {
				DEBUG(DEB_LEV_ERR, "Usage: %s [-v] [-h] [componentspath]\n", argv[0]);
				exit(-EINVAL);
			}
		} else {
			componentspath = argv[1];
		}

	} else {
		DEBUG(DEB_LEV_ERR, "Usage: %s [-v] [-h] [componentspath]\n", argv[0]);
		exit(-EINVAL);
	}

	err = buildComponentsList(componentspath, &ncomponents,&nroles,verbose);
	if(err)
		DEBUG(DEB_LEV_ERR, "Error registering OpenMAX components with ST static component loader %s\n", strerror(err));
	else {
		if (verbose) {
			printf("%i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
		} else {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "%i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
		}
	}
	return 0;
}
