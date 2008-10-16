/**
  @file src/common.h

  OpenMAX Integration Layer Core. This library implements the OpenMAX core
  responsible for environment setup, component tunneling and communication.

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

#ifndef __COMMON_H__
#define __COMMON_H__

int makedir(const char *newdir);
char* allRegistryGetFilename(char* registry_name);
char *registryGetFilename(void);

typedef struct nameList {
	char* name;
	struct nameList *next;
} nameList;


#endif

