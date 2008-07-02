/**
  @file test/components/components_symbian/symbiancompnameenum/src/omxcompnameenumtest.c
    
  Copyright (C) 2008  STMicroelectronics
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>

#include <user_debug_levels.h>

int main(int argc, char** argv) {

  int index;
  char name[256];
  OMX_ERRORTYPE err = OMX_ErrorNone;

  err = OMX_Init();
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
    exit(1);
  }

  index = 0;
  while(err == OMX_ErrorNone)
  {
    err = OMX_ComponentNameEnum(name, 256, index);
    if (err == OMX_ErrorNone)
    {
        printf("Component %d is %s\n", index, name);
    }
    index++;
  }

  printf("Hit any key\n");
  getchar();

  OMX_Deinit();

  return 0;
}
