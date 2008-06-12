/**
  @file src/omx_create_loaders_symbian.cpp
    
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

#include "omx_create_loaders.h"

extern "C"
{
#include "omxcore.h"
}

#include "BellagioOpenMaxLoader.h"

#include <e32std.h>
#include <ImplementationInformation.h>

int 
createComponentLoaders()
{
    RImplInfoPtrArray pluginInfos;
	CBellagioOpenMaxLoader *nrcLoader = NULL;

    /* We are looking for Bellagio loaders */
    TUid InterfaceUid = {KUidBellagioOpenMaxLoader};

    TRAPD(r, REComSession::ListImplementationsL(InterfaceUid, pluginInfos));
    if (r != KErrNone)
    {
        return OMX_ErrorComponentNotFound;
    }

    TInt count = pluginInfos.Count();
    
    if (count < 1)
    {
        return OMX_ErrorComponentNotFound;
    }
   
    /* Create all possible loaders */ 
    for (TInt index = 0; index < count; index++)
    {
        /* Create the actual ecom plugin loader and set up the function pointers*/
        TRAP(r, nrcLoader = CBellagioOpenMaxLoader::NewL(pluginInfos[index]->ImplementationUid()));
        
        /* add loader to core */
        BOSA_AddComponentLoader(nrcLoader->GetLoader());
    }

    return 0;
}
