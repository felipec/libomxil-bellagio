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
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

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
	
	2006/05/11:  IL component register version 0.2

*/

#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>

#include "omxcore.h"

int main(int argc, char** argv)
{
	char* componentspath;
	DIR *dirp = NULL;
	struct dirent *dp;

   void* handle;
	int i = 0;
	int ncomponents;
	int err;
	
	if(argc == 1){
		componentspath = OMXILCOMPONENTSPATH;
	}else if(argc == 2){
		componentspath = argv[1];
	}else{
		fprintf(stderr, "Usage: %s [componentspath]\n", argv[0]);
		exit(-EINVAL);
	}

	/* Call the build function in libomxil */
	err = buildComponentsList(componentspath, &ncomponents);
	if(err)
		fprintf(stderr, "%s\n", strerror(err));
	else
		fprintf(stdout, "%i OpenMAX IL components succesfully scanned\n", ncomponents);
	
	return 0;
}

