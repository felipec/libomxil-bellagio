/**
  @file src/components/components_symbian/inc/BellagioOpenMaxComponent.h

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

#ifndef __BELLAGIO_OPENMAX_COMPONENT_H__
#define __BELLAGIO_OPENMAX_COMPONENT_H__

#include <e32def.h>  //TBool
#include <e32std.h>  //RLibrary
#include <ecom.h>  //RLibrary

#include <OMX_Core.h>
#include <OMX_Component.h>

#include "BellagioOpenMaxComponentUid.hrh"
#include "component_loader.h"

class CBellagioOpenMaxComponent : public CBase
{
public:
	static CBellagioOpenMaxComponent* NewL(TUid id, OMX_STRING aComponentName);
	virtual ~CBellagioOpenMaxComponent();

	IMPORT_C void setId(TUid id);

	IMPORT_C BOSA_COMPONENTLOADER * GetLoader();
	IMPORT_C void SetLoader(BOSA_COMPONENTLOADER *pLoader);

	IMPORT_C OMX_COMPONENTTYPE * GetComponent();
	IMPORT_C void SetComponent(OMX_COMPONENTTYPE *pComponent);

protected:
	/* handle to the standard component member of this object */
	OMX_COMPONENTTYPE *component;
	/* handle to the loader that loaded this component */
    BOSA_COMPONENTLOADER *loader;

private:
	TUid iDtor_ID_Key;
};

#endif  // __BEBOP_OPENMAX_COMPONENT_H__
