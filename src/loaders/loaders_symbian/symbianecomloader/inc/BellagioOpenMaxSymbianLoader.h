/**
  @file src/loaders/loaders_symbian/symbianecomloader/inc/BellagioOpenMaxSymbianLoader.h
    
  Component loader header for Symbian. 
    
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

#ifndef __BELLAGIO_OPENMAX_SYMBIANLOADER_H__
#define __BELLAGIO_OPENMAX_SYMBIANLOADER_H__

#include <e32def.h>  //TBool
#include <e32std.h>  //RLibrary

#include "BellagioOpenMaxLoader.h"

#include "BellagioOpenMaxSymbianLoaderImplUid.hrh"

#include "BellagioOpenMaxComponent.h"

// The actual implementation
#include "nrc_symbian_component_loader.h"

// Forward declaration
struct BOSA_COMPONENTLOADER;

class CBellagioOpenMaxSymbianLoader :  public CBellagioOpenMaxLoader
{
public:
    static CBellagioOpenMaxSymbianLoader* NewL(TAny* aInitParams);
	virtual ~CBellagioOpenMaxSymbianLoader();

	OMX_COMPONENTTYPE* CreateComponent(OMX_STRING cComponentName, BOSA_COMPONENTLOADER* loader);
    OMX_ERRORTYPE DestroyComponent(OMX_COMPONENTTYPE *hComponent);

    OMX_ERRORTYPE ComponentNameEnum(OMX_STRING cComponentName,
                                OMX_U32 nNameLength,
                                OMX_U32 nIndex);

    OMX_ERRORTYPE GetRolesOfComponent(OMX_STRING compName,
                                  OMX_U32 *pNumRoles,
                                  OMX_U8 **roles);

    OMX_ERRORTYPE GetComponentsOfRole(OMX_STRING role,
                                  OMX_U32 *pNumComps,
                                  OMX_U8  **compNames);

    CBellagioOpenMaxComponent* GetComponentPrivate(OMX_COMPONENTTYPE *hComponent);
	TInt MakeComponentList();

private:
	CBellagioOpenMaxSymbianLoader();

    TInt GetInfoIndex(OMX_STRING componentName);

private:
	/* a array to hold all components created by this loader */
	RPointerArray<CBellagioOpenMaxComponent> iComponents;

	/* a array to hold component infos */
	RImplInfoPtrArray componentInfos;
};

#endif  // __BELLAGIO_OPENMAX_SYMBIANLOADER_H__
