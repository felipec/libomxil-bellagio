/**
	@file src/omxregister.c

	Register OpenMAX components. This application registers the installed OpenMAX
	components and stores the list in the file:
	$HOME/.omxregistry

	It must be run before using components.
	The components are searched in the default directory:
	OMXILCOMPONENTSPATH
	If the components are installed in a different location, specify:

	omxregister-bellagio installation_path

        Copyright (C) 2007, 2008  STMicroelectronics
        Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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
#include <sys/stat.h>
#include <sys/types.h>

#include "st_static_component_loader.h"
#include "common.h"

/** String element to be put in the .omxregistry file to indicate  an
 * OpenMAX component and its roles
 */
static const char arrow[] =  " ==> ";

/** @brief Creates a list of components on a registry file
 *
 * This function
 *  - reads for the given directory(ies) any library contained
 *  - check if the library belongs to OpenMAX ST static component loader
 *    (it must contain the function omx_component_library_Setup for the initialization)
 *  - write the openmax names and related libraries to the registry file
 */
static int buildComponentsList(FILE* omxregistryfp, char *componentspath, int verbose) {
  DIR *dirp;
	struct dirent *dp;
	void *handle;
	int i, num_of_comp;
	unsigned int j;
	char *buffer = NULL;
	int (*fptr)(void *);
	stLoaderComponentType **stComponents;
	int ncomponents = 0, nroles=0;
	int pathconsumed = 0;
	int currentgiven;
	int index;
	char* currentpath = componentspath;
	char* actual;
	nameList *allNames = NULL;
	nameList *currentName = NULL;
	nameList *tempName = NULL;
	/* the componentpath contains a single or multiple directories
	 * and is is colon separated like env variables in Linux
	 */

	while (!pathconsumed) {
		index = 0;
		currentgiven = 0;
		while (!currentgiven) {
			if (*(currentpath + index) == '\0') {
				pathconsumed = 1;
			}		
			if ((*(currentpath + index) == ':') || (*(currentpath + index) =='\0')) {
				currentgiven = 1;
				if (*(currentpath + index - 1) != '/') {
					actual = malloc(index + 2);
					*(actual + index) = '/';
					*(actual+index + 1) = '\0';
				} else {
					actual = malloc(index + 1);
					*(actual+index) = '\0';
				}
				strncpy(actual, currentpath, index);
				currentpath = currentpath + index + 1;
			}
			index++;
		}
		/* Populate the registry file */
		dirp = opendir(actual);
		if(dirp == NULL){
			int err = errno;
			DEBUG(DEB_LEV_ERR, "Cannot open directory %s\n", actual);
			return err;
		} else {
			if (verbose) {
				printf("\n Scanning directory %s\n", actual);
			}
		}
		while((dp = readdir(dirp)) != NULL) {
			int len = strlen(dp->d_name);

			if(len >= 3){


				if(strncmp(dp->d_name+len-3, ".so", 3) == 0){
					char lib_absolute_path[strlen(actual) + len + 1];

					strcpy(lib_absolute_path, actual);
					strcat(lib_absolute_path, dp->d_name);

					if((handle = dlopen(lib_absolute_path, RTLD_NOW)) == NULL) {
						DEBUG(DEB_LEV_ERR, "could not load %s: %s\n", lib_absolute_path, dlerror());
					} else {
						if ((fptr = dlsym(handle, "omx_component_library_Setup")) == NULL) {
							DEBUG(DEB_LEV_ERR, "the library %s is not compatible with ST static component loader - %s\n", lib_absolute_path, dlerror());
							continue;
						}
						if (verbose) {
							printf("\n Scanning openMAX libary %s\n", lib_absolute_path);
						}
						num_of_comp = fptr(NULL);
						stComponents = malloc(num_of_comp * sizeof(stLoaderComponentType*));
						for (i = 0; i<num_of_comp; i++) {
							stComponents[i] = calloc(1,sizeof(stLoaderComponentType));
						}
						fptr(stComponents);
						fwrite(lib_absolute_path, 1, strlen(lib_absolute_path), omxregistryfp);
						fwrite("\n", 1, 1, omxregistryfp);


						for (i = 0; i<num_of_comp; i++) {
							tempName = allNames;
							if (tempName != NULL) {
								do  {
									if (!strcmp(tempName->name, stComponents[i]->name)) {
										DEBUG(DEB_LEV_ERR, "Component %s already registered. Skip\n", stComponents[i]->name);
										break;
									}
									tempName = tempName->next;
								} while(tempName != NULL);
								if (tempName != NULL) {
									continue;
								}
							}
							if (allNames == NULL) {
								allNames = malloc(sizeof(nameList));
								currentName = allNames;
							} else {
								currentName->next = malloc(sizeof(nameList));
								currentName = currentName->next;
							}
							currentName->next = NULL;
							currentName->name = malloc(strlen(stComponents[i]->name) + 1);
							strcpy(currentName->name, stComponents[i]->name);
							*(currentName->name + strlen(currentName->name)) = '\0';

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
								nroles += stComponents[i]->name_specific_length;
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
							ncomponents++;
						}
						for (i = 0; i < num_of_comp; i++) {
							free(stComponents[i]);
						}
						free(stComponents);
					}
				}
			}
		}
		free(actual);
		closedir(dirp);
	}
	free(buffer);
	if (verbose) {
		printf("\n %i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
	} else {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "\n %i OpenMAX IL ST static components with %i roles succesfully scanned\n", ncomponents, nroles);
	}

	return 0;
}

static void usage(const char *app) {
	char *registry_filename;
	registry_filename = registryGetFilename();

	printf(
      "Usage: %s [-v] [-h] [componentspath]...\n"
      "This programs scans for a given list of directory searching for any OpenMAX\n"
      "component compatible with the ST static component loader.\n"
			"The registry is saved under %s. (can be changed via OMX_BELLAGIO_REGISTRY\n"
			"environment variable)\n"
      "\n"
      "The following options are supported:\n"
      "\n"
      "        -v   display a verbose output, listing all the components registered\n"
      "        -h   display this message\n"
      "\n"
      "         componentspath: a searching path for components can be specified.\n"
      "         If this parameter is omitted, the components are searched in the\n"
      "         default %s directory\n"
      "\n",
			app, registry_filename, OMXILCOMPONENTSPATH);

  free(registry_filename);
}

static int makedir (const char *newdir)
{
  char *buffer;
  char *p;
  int err;
  size_t len;

  buffer = strdup(newdir);
  len = strlen(buffer);

  if (len == 0) {
    free(buffer);
    return 1;
  }
  if (buffer[len-1] == '/') {
    buffer[len-1] = '\0';
  }

	err = mkdir(buffer, 0755);
	if (err == 0 || (( err == -1) && (errno == EEXIST))) {
		free(buffer);
		return 0;
	}

  p = buffer+1;
  while (1) {
		char hold;

		while(*p && *p != '\\' && *p != '/')
			p++;
		hold = *p;
		*p = 0;
		if ((mkdir(buffer, 0755) == -1) && (errno == ENOENT)) {
			free(buffer);
			return 1;
		}
		if (hold == 0)
			break;
		*p++ = hold;
	}
  free(buffer);
  return 0;
}

/** @brief execution of registration function
 *
 * This register by default searches for openmax libraries in OMXILCOMPONENTSPATH
 * If specified it can search in a different directory
 */
int main(int argc, char *argv[]) {
	int found;
	int err, i;
	int verbose=0;
	FILE *omxregistryfp;
	char *registry_filename;
	char *dir,*dirp;

	for(i = 1; i < argc; i++) {
		if(*(argv[i]) != '-') {
			continue;
		}
		if (*(argv[i]+1) == 'v') {
			verbose = 1;
		} else {
			usage(argv[0]);
			exit(*(argv[i]+1) == 'h' ? 0 : -EINVAL);
		}
  }

	registry_filename = registryGetFilename();

	/* make sure the registry directory exists */
	dir = strdup(registry_filename);
	if (dir == NULL)
		exit(EXIT_FAILURE);
	dirp = strrchr(dir, '/');
	if (dirp != NULL) {
		*dirp = '\0';
		if (makedir(dir)) {
			DEBUG(DEB_LEV_ERR, "Cannot create OpenMAX registry directory %s\n", dir);
			exit(EXIT_FAILURE);
		}
	}
	free (dir);

	omxregistryfp = fopen(registry_filename, "w");
	if (omxregistryfp == NULL){
		DEBUG(DEB_LEV_ERR, "Cannot open OpenMAX registry file %s\n", registry_filename);
		exit(EXIT_FAILURE);
	}

	free(registry_filename);

	for(i = 1, found = 0; i < argc; i++) {
		if(*(argv[i]) == '-') {
			continue;
		}

		found = 1;
		err = buildComponentsList(omxregistryfp, argv[i], verbose);
		if(err) {
			DEBUG(DEB_LEV_ERR, "Error registering OpenMAX components with ST static component loader %s\n", strerror(err));
			continue;
		}
	}

	if (found == 0) {
		err = buildComponentsList(omxregistryfp, OMXILCOMPONENTSPATH, verbose);
		if(err) {
			DEBUG(DEB_LEV_ERR, "Error registering OpenMAX components with ST static component loader %s\n", strerror(err));
		}
	}

	fclose(omxregistryfp);

	return 0;
}
