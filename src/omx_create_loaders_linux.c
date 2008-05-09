#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include "component_loader.h"
#include "omx_create_loaders.h"
#include "omxcore.h"
#include "st_static_component_loader.h"

int 
createComponentLoaders()
{
	// load component loaders
	BOSA_COMPONENTLOADER *loader;
	void *handle;
	void *functionPointer;
	void (*fptr)(BOSA_COMPONENTLOADER *loader);
	char loaderFile[200];
	char *libraryFileName = NULL;
	FILE *loaderFP;
	size_t len = 0;
	int read;
	
	/* add the ST static component loader */
	st_static_InitComponentLoader();
	BOSA_AddComponentLoader(st_static_loader);
	
	// open file containing loader library names ($HOME/.omxloaders)
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	memset(loaderFile, 0, sizeof(loaderFile));
	strcat(loaderFile, getenv("HOME"));
	strcat(loaderFile, "/.omxloaders");

	loaderFP = fopen(loaderFile, "r");
	if (loaderFP == NULL)
	{
		//DEBUG(DEB_LEV_ERR, "Could not open .omxloaders file\n");
		return -1;
	}
	
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
	        exit(1);
	    }

	    if ((functionPointer = dlsym(handle, "setup_component_loader")) == NULL)
		{
				DEBUG(DEB_LEV_ERR, "the library %s is not compatible - %s\n", libraryFileName, dlerror());
				exit(1);
		}
	    fptr = functionPointer;

		loader = (BOSA_COMPONENTLOADER *) calloc(1, sizeof(BOSA_COMPONENTLOADER));

		if (loader == NULL) 
		{
				DEBUG(DEB_LEV_ERR, "not enough memory for this loader\n");
				exit(1);
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
