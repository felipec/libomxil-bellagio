/**
  @file src/content_pipe_inet.h

  This file implements the support for content pipes related to inet resources.

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

  $Date: 2008-09-18 13:24:20 +0200 (Thu, 18 Sep 2008) $
  Revision $Rev: 633 $
  Author $Author: gsent $
*/

#ifndef __CONTENT_PIPE_INET_H__
#define __CONTENT_PIPE_INET_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>


#include <OMX_Types.h>
#include <OMX_ContentPipe.h>

#include <bellagio/omx_comp_debug_levels.h>

#define SOCKET_ERROR        -1
#define QUEUE_SIZE          1

typedef struct {

  /* public */
  CP_PIPETYPE pipe;

  /* private */
  int sfd;
  int cfd;

} inet_ContentPipe;

#endif
