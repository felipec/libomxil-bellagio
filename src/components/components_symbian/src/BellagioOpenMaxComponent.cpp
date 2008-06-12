/**
  @file src/components/components_symbian/src/BellagioOpenMaxComponent.cpp
    
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

#include "BellagioOpenMaxComponent.h"

CBellagioOpenMaxComponent* 
CBellagioOpenMaxComponent::NewL(TUid id, OMX_STRING aComponentName)
    {
	TUid id_key;

	TAny *ptr = REComSession::CreateImplementationL(id, id_key, REINTERPRET_CAST(TAny*, aComponentName));
	
	if (!ptr) 
	{
		User::Leave(KErrNoMemory);
	}

	CBellagioOpenMaxComponent* nrcComponent = REINTERPRET_CAST(CBellagioOpenMaxComponent*, ptr);
	
	nrcComponent->setId(id_key);

    return nrcComponent;
    }

CBellagioOpenMaxComponent::~CBellagioOpenMaxComponent()
	{
	REComSession::DestroyedImplementation(iDtor_ID_Key);
	}

EXPORT_C void 
CBellagioOpenMaxComponent::setId(TUid id)
	{
	iDtor_ID_Key = id;
	}

EXPORT_C BOSA_COMPONENTLOADER * 
CBellagioOpenMaxComponent::GetLoader()
{
	return loader;
}

EXPORT_C void 
CBellagioOpenMaxComponent::SetLoader(BOSA_COMPONENTLOADER *pLoader)
{
	loader = pLoader;
}

EXPORT_C OMX_COMPONENTTYPE * 
CBellagioOpenMaxComponent::GetComponent()
{
	return component;
}

EXPORT_C void 
CBellagioOpenMaxComponent::SetComponent(OMX_COMPONENTTYPE *pComponent)
{
	component = pComponent;
}
