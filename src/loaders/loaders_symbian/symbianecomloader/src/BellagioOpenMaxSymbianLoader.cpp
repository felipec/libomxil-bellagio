/**
  @file src/loaders/symbianloader/src/BellagioOpenMaxSymbianLoader.cpp
        
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Utf.h>

#include "BellagioOpenMaxComponent.h"
#include "BellagioOpenMaxComponentUid.hrh"

#include "component_loader.h"

CBellagioOpenMaxSymbianLoader::CBellagioOpenMaxSymbianLoader()
{
}

CBellagioOpenMaxSymbianLoader::~CBellagioOpenMaxSymbianLoader()
{
	iComponents.ResetAndDestroy();
	componentInfos.ResetAndDestroy();
}

CBellagioOpenMaxSymbianLoader* 
CBellagioOpenMaxSymbianLoader::NewL(TAny* /*aInitParams*/)
{    
	BOSA_COMPONENTLOADER *aLoader = new BOSA_COMPONENTLOADER;
	
	CBellagioOpenMaxSymbianLoader *self = new(ELeave) CBellagioOpenMaxSymbianLoader;

	setup_component_loader(aLoader);

	aLoader->loaderPrivate = (void*)self;

    self->loader = aLoader;

    return self;
}

TInt
CBellagioOpenMaxSymbianLoader::MakeComponentList()
{
	/* We are looking for Bellagio components */
    TUid InterfaceUid = {KUidBellagioOpenMaxComponent};

	TRAPD(r, REComSession::ListImplementationsL(InterfaceUid, componentInfos));
    if (r != KErrNone)
    {
        return 0;
    }

	return componentInfos.Count();
}

OMX_COMPONENTTYPE*
CBellagioOpenMaxSymbianLoader::CreateComponent(OMX_STRING cComponentName, BOSA_COMPONENTLOADER* /*loader*/)
{
	TInt index = 0;
    TUid id;

    index = GetInfoIndex(cComponentName);

	if (index < 0)
    {
        return 0;
    }

    id = componentInfos[index]->ImplementationUid();

    CBellagioOpenMaxComponent *nrcComponent = 0; 
    TRAPD(r, (nrcComponent = CBellagioOpenMaxComponent::NewL(id, cComponentName))); 

    if (r != KErrNone)
    {
        return 0;
    }

	r = iComponents.Append(nrcComponent);
	if (r != KErrNone)
    {
		delete nrcComponent;
        return 0;
    }

	return nrcComponent->GetComponent();
}

CBellagioOpenMaxComponent *
CBellagioOpenMaxSymbianLoader::GetComponentPrivate(OMX_COMPONENTTYPE *hComponent)
{
	TInt count = iComponents.Count();
    TInt index = 0;

    if (count < 1)
    {
        return 0;
    }
   
    /* Search all the components for the correct one */ 
    for (index = 0; index < count; index++)
    {
        if(iComponents[index]->GetComponent() == hComponent)
        {
            break; // stop the loop
        }
    }

	if (index == count)
    {
        return 0;
    }

	return iComponents[index];
}

OMX_ERRORTYPE
CBellagioOpenMaxSymbianLoader::DestroyComponent(OMX_COMPONENTTYPE *hComponent)
{
    /* This will also tell if the component was created from this loader */
    CBellagioOpenMaxComponent *nrcComponent = GetComponentPrivate( (OMX_COMPONENTTYPE *) hComponent);

	if (nrcComponent == 0)
	{
		return OMX_ErrorComponentNotFound;
	}

    iComponents.Remove(iComponents.Find(nrcComponent));

    delete (nrcComponent);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
CBellagioOpenMaxSymbianLoader::ComponentNameEnum(OMX_STRING cComponentName,
                                                 OMX_U32 nNameLength,
                                                 OMX_U32 nIndex)
{
    TInt count = 0;

    count = componentInfos.Count();
    
    if (count < 1)
    {
        return OMX_ErrorComponentNotFound;
    }

    if (nIndex > (count - 1))
    {
        return OMX_ErrorNoMore;
    }

    TBuf8<257> name;
    name.FillZ();
    name.Zero();
    CnvUtfConverter::ConvertFromUnicodeToUtf8(name, (componentInfos[nIndex])->DisplayName());
    name.ZeroTerminate();
    
    if (nNameLength < name.Length() - 1)
    {
        return OMX_ErrorUndefined;
    }

    strcpy((char*)cComponentName, (char*)name.Ptr());

    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
CBellagioOpenMaxSymbianLoader::GetRolesOfComponent(OMX_STRING compName,
                                                   OMX_U32 *pNumRoles,
                                                   OMX_U8 **roles)
{
    TInt index = 0;

    index = GetInfoIndex(compName);

    if (index < 0)
    {
        return OMX_ErrorComponentNotFound;
    }

    TLex8 lexer = TLex8((componentInfos[index])->OpaqueData());
    lexer.Val((TUint32 &)(*pNumRoles), EDecimal);

    if((*pNumRoles) == 0) 
    {
        return OMX_ErrorNone;
    }

    // check if client is asking only for the number of roles
    if (roles == NULL)
    {
        return OMX_ErrorNone;
    }

    // TODO We copy only one role here and there might be several of those
    TBuf8<257> role;
    TBufC8<1> endOfString((TText8*)"\0");

    role.Append((componentInfos[index])->DataType());
    role.Append(endOfString);

    strcpy((char*)roles[0], (char*)role.Ptr());

    return OMX_ErrorNone;
}

OMX_ERRORTYPE 
CBellagioOpenMaxSymbianLoader::GetComponentsOfRole(OMX_STRING role,
                                                   OMX_U32 *pNumComps,
                                                   OMX_U8  **compNames)
{
    TInt index = 0;
    TInt count = 0;
    TInt length = 0;
    TInt roles = 0;
    TInt nameIndex = 0;

    count = componentInfos.Count();
    
    if (count < 1)
    {
        return OMX_ErrorComponentNotFound;
    }

    /* Turn the role into Symbian descriptor */
    length = strlen(role);
    TPtrC8 role8(reinterpret_cast<const TUint8 *>(role), length);
   
    /* Search all the components for the roles count */ 
    for (index = 0; index < count; index++)
    {
        if (role8.Compare((componentInfos[index])->DataType()) == 0)
        {
            roles++;
        }
    }

    *pNumComps = roles;

    // check if client is asking only for the number of components
    if (compNames == NULL)
    {
        return OMX_ErrorNone;
    }

    TBuf8<257> component;
    TBufC8<1> endOfString((TText8*)"\0");

    /* Search all the components for the component names */ 
    for (index = 0; index < count; index++)
    {
        if (role8.Compare((componentInfos[index])->DataType()) == 0)
        {
            CnvUtfConverter::ConvertFromUnicodeToUtf8(component, (componentInfos[index])->DisplayName());
            component.Append(endOfString);
            strcpy((char*)compNames[nameIndex], (char*)component.Ptr());
            nameIndex++;
        }
    }

    return OMX_ErrorNone;
}

TInt
CBellagioOpenMaxSymbianLoader::GetInfoIndex(OMX_STRING componentName)
{
    TInt index = 0;
    TInt count = 0;
    TInt length = 0;

    count = componentInfos.Count();
    
    if (count < 1)
    {
        return -1;
    }

    /* Turn the name into Symbian descriptor */
    length = strlen(componentName);
    TPtrC8 name8(reinterpret_cast<const TUint8 *>(componentName), length);
   
    TBuf<256> name16;
    CnvUtfConverter::ConvertToUnicodeFromUtf8(name16, name8);

    /* Search all the components for the correct one */ 
    for (index = 0; index < count; index++)
    {
        if (name16.Compare((componentInfos[index])->DisplayName()) == 0)
        { 
            break; // stop the loop
        }
    }

    if (index == count)
    {
        return -1;
    }

    return index;
}
