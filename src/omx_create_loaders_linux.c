/**
  @file src/omx_create_loaders_linux.c

  In this file is implemented the entry point for the construction
  of every component loader in linux. In the current implementation
  only the st static loader is called.

  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

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

#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include "component_loader.h"
#include "omx_create_loaders.h"
#include "omxcore.h"
#include "st_static_component_loader.h"
#include "common.h"

#define OMX_LOADERS_FILENAME ".omxloaders"

int createComponentLoaders() {
	// load component loaders
	BOSA_COMPONENTLOADER *loader;
	void *handle;
	void *functionPointer;
	void (*fptr)(BOSA_COMPONENTLOADER *loader);
	char *libraryFileName = NULL;
	FILE *loaderFP;
	size_t len = 0;
	int read;
	char *omxloader_registry_filename;
	char *dir, *dirp;
	int onlyDefault = 0;
	int isNewFile = 0;

	omxloader_registry_filename = allRegistryGetFilename(OMX_LOADERS_FILENAME);

	/* make sure the registry directory exists */
	dir = strdup(omxloader_registry_filename);
	if (dir == NULL) {
		DEBUG(DEB_LEV_ERR, "The directory has not been specified\n");
		onlyDefault = 1;
	}
	dirp = strrchr(dir, '/');
	if (dirp != NULL) {
		*dirp = '\0';
		if (makedir(dir)) {
			DEBUG(DEB_LEV_ERR, "Cannot create OpenMAX registry directory %s\n", dir);
			onlyDefault = 1;
		}
	}
	free(dir);

	/* test the existence of the file */
	loaderFP = fopen(omxloader_registry_filename, "r");
	if (loaderFP == NULL){
		DEBUG(DEB_LEV_SIMPLE_SEQ, "The registry file for loaders does not exists. Create it with the default loader\n");
		loaderFP = fopen(omxloader_registry_filename, "w");
		if (loaderFP == NULL){
			DEBUG(DEB_LEV_ERR, "Cannot create OpenMAX registry file %s\n", omxloader_registry_filename);
			onlyDefault = 1;
		}
	}
	if (onlyDefault) {
		loader = calloc(1, sizeof(BOSA_COMPONENTLOADER));
		if (loader == NULL) {
				DEBUG(DEB_LEV_ERR, "not enough memory for this loader\n");
				return OMX_ErrorInsufficientResources;
		}
		st_static_setup_component_loader(loader);
		BOSA_AddComponentLoader(loader);
		return 0;
	}
	free(omxloader_registry_filename);
	/* add the ST static component loader */
	loader = calloc(1, sizeof(BOSA_COMPONENTLOADER));
	if (loader == NULL) {
			DEBUG(DEB_LEV_ERR, "not enough memory for this loader\n");
			return OMX_ErrorInsufficientResources;
	}
	st_static_setup_component_loader(loader);
	BOSA_AddComponentLoader(loader);

	// dlopen all loaders defined in .omxloaders file
	while((read = getline(&libraryFileName, &len, loaderFP)) != -1)
	{
		// strip delimeter, the dlopen doesn't like it
		if(libraryFileName[read-1] == '\n')
			libraryFileName[read-1] = 0;

		handle = dlopen(libraryFileName, RTLD_NOW);

	    if (!handle)
		{
			DEBUG(DEB_LEV_ERR, "library %s dlopen error: %s\n", libraryFileName, dlerror());
			return OMX_ErrorUndefined;
	    }

	    if ((functionPointer = dlsym(handle, "setup_component_loader")) == NULL)
		{
				DEBUG(DEB_LEV_ERR, "the library %s is not compatible - %s\n", libraryFileName, dlerror());
				return OMX_ErrorUndefined;
		}
	    fptr = functionPointer;

		loader = calloc(1, sizeof(BOSA_COMPONENTLOADER));

		if (loader == NULL)
		{
				DEBUG(DEB_LEV_ERR, "not enough memory for this loader\n");
				return OMX_ErrorInsufficientResources;
		}

		/* setup the function pointers */
		(*fptr)(loader);

		/* add loader to core */
		BOSA_AddComponentLoader(loader);
	}

	if (libraryFileName)
	{
		free(libraryFileName);
	}

	fclose(loaderFP);

	return 0;
}
