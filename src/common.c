/**
  @file src/common.c

  OpenMax Integration Layer Core. This library implements the OpenMAX core
  responsible for environment setup, components tunneling and communication.

  Copyright (C) 2007, 2008  STMicroelectronics and Nokia

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
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"

#define REGISTRY_FILENAME "/registry"
#define REGISTRY_DIR PACKAGE
#define DATA_HOME_DIR ".local/share"

/** @brief Get registry filename
 *
 */
char* registryGetFilename(void) {
  char *omxregistryfile = NULL;
  char *buffer;

  buffer=getenv("OMX_BELLAGIO_REGISTRY");
  if(buffer!=NULL&&*buffer!='\0') {
    omxregistryfile = strdup(buffer);
    return omxregistryfile;
  }

  buffer=getenv("XDG_DATA_HOME");
  if(buffer!=NULL&&*buffer!='\0') {
    omxregistryfile = malloc(strlen(buffer) + strlen(REGISTRY_DIR) + strlen(REGISTRY_FILENAME) + 2);
    strcpy(omxregistryfile, buffer);
    strcat(omxregistryfile, "/");
    strcat(omxregistryfile, REGISTRY_DIR);
    goto addfilename;
  }

  buffer=getenv("HOME");
  if(buffer!=NULL&&*buffer!='\0') {
    omxregistryfile  = malloc(strlen(buffer) + strlen(DATA_HOME_DIR) + strlen(REGISTRY_DIR) + strlen(REGISTRY_FILENAME) + 3);
    strcpy(omxregistryfile, buffer);
    strcat(omxregistryfile, "/");
    strcat(omxregistryfile, DATA_HOME_DIR);
    strcat(omxregistryfile, "/");
    strcat(omxregistryfile, REGISTRY_DIR);
  } else {
    omxregistryfile  = malloc(strlen(REGISTRY_DIR) + strlen(REGISTRY_FILENAME) + 2);
    strcpy(omxregistryfile, ".");
    strcat(omxregistryfile, REGISTRY_DIR);
  }

 addfilename:
  strcat(omxregistryfile, REGISTRY_FILENAME);

  return omxregistryfile;
}



#ifdef COMMON_TEST
int main (int argc, char*argv[]) {
  printf (registryGetFilename ());
}
#endif
