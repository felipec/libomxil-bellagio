/**
  @file src/loaders/symbianloader/src/nrc_symbian_component_loader.cpp
    
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

#include "BellagioOpenMaxSymbianLoader.h"
#include "BellagioOpenMaxComponent.h"

#include "nrc_symbian_component_loader.h"
#include "omxcore.h"

/* entry point to fill the function pointers*/
void
setup_component_loader(BOSA_COMPONENTLOADER* loader)
{
    loader->BOSA_InitComponentLoader = &BOSA_NRC_SYMBIAN_InitComponentLoader;
    loader->BOSA_DeInitComponentLoader = &BOSA_NRC_SYMBIAN_DeInitComponentLoader;
    loader->BOSA_CreateComponent = &BOSA_NRC_SYMBIAN_CreateComponent;
    loader->BOSA_DestroyComponent = &BOSA_NRC_SYMBIAN_DestroyComponent;
    loader->BOSA_ComponentNameEnum = &BOSA_NRC_SYMBIAN_ComponentNameEnum;
    loader->BOSA_GetRolesOfComponent = &BOSA_NRC_SYMBIAN_GetRolesOfComponent;
    loader->BOSA_GetComponentsOfRole = &BOSA_NRC_SYMBIAN_GetComponentsOfRole;
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_InitComponentLoader(BOSA_COMPONENTLOADER *loader)
{
    ASSERT(loader != NULL);

    CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

	if (sLoader->MakeComponentList() <= 0)
    {
        return OMX_ErrorComponentNotFound;
    }
    /* At this point we could set up the callback thread for listening ECOM additions! */

    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_DeInitComponentLoader(BOSA_COMPONENTLOADER *loader)
{
    ASSERT (loader != NULL);

	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

	ASSERT (sLoader != NULL);

    delete (sLoader);

    sLoader = 0;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_CreateComponent(BOSA_COMPONENTLOADER *loader,
                              OMX_OUT OMX_HANDLETYPE* pHandle,
                              OMX_IN  OMX_STRING cComponentName,
                              OMX_IN  OMX_PTR pAppData,
                              OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{
    OMX_COMPONENTTYPE *component = 0;
    
    ASSERT (loader != NULL);

	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

    ASSERT (sLoader != NULL);

	component = sLoader->CreateComponent(cComponentName, loader);

    if (component == 0)
	{
		return OMX_ErrorComponentNotFound;
	}
    
    component->SetCallbacks(component, pCallBacks, pAppData);   

	*pHandle = REINTERPRET_CAST(OMX_HANDLETYPE*, component);
    
	return OMX_ErrorNone;
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_DestroyComponent(BOSA_COMPONENTLOADER *loader, 
                               OMX_HANDLETYPE hComponent)
{
	ASSERT (loader != NULL);
	
	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

    ASSERT (sLoader != NULL);

    return sLoader->DestroyComponent( (OMX_COMPONENTTYPE *) hComponent);
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_ComponentNameEnum(BOSA_COMPONENTLOADER * loader,
                                OMX_STRING cComponentName,
                                OMX_U32 nNameLength,
                                OMX_U32 nIndex)
{
    ASSERT (loader != NULL);
	
	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

    ASSERT (sLoader != NULL);

    return sLoader->ComponentNameEnum(cComponentName, nNameLength, nIndex);
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_GetRolesOfComponent(BOSA_COMPONENTLOADER *loader,
                                  OMX_STRING compName,
                                  OMX_U32 *pNumRoles,
                                  OMX_U8 **roles)
{
    ASSERT (loader != NULL);
	
	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

    return sLoader->GetRolesOfComponent(compName, pNumRoles, roles);
}

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_GetComponentsOfRole(BOSA_COMPONENTLOADER *loader,
                                  OMX_STRING role,
                                  OMX_U32 *pNumComps,
                                  OMX_U8  **compNames)
{
    ASSERT (loader != NULL);
	
	CBellagioOpenMaxSymbianLoader* sLoader = (CBellagioOpenMaxSymbianLoader *) loader->loaderPrivate;

    return sLoader->GetComponentsOfRole(role, pNumComps, compNames);
}
