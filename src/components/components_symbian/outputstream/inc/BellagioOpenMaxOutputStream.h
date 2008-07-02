/**
  @file src/components/components_symbian/outputstream/inc/BellagioOpenMaxOutputStream.h
    
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

#ifndef __BELLAGIO_OPENMAX_OUTPUTSTREAM_H__
#define __BELLAGIO_OPENMAX_OUTPUTSTREAM_H__

#include <e32def.h>  //TBool
#include <e32std.h>  //RLibrary

#include "BellagioOpenMaxComponent.h"
#include "BellagioOpenMaxOutputStreamImplUid.hrh"

class CBellagioOpenMaxOutputStream : public CBellagioOpenMaxComponent
{
public:
	static CBellagioOpenMaxOutputStream* NewL(TAny* aInitParams);
	~CBellagioOpenMaxOutputStream();
};

#endif  // __BELLAGIO_OPENMAX_OUTPUTSTREAM_H__
