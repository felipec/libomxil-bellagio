/**
  @file src/loaders/BellagioOpenMaxLoader.ccp
    
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

#include "BellagioOpenMaxLoader.h"

CBellagioOpenMaxLoader* CBellagioOpenMaxLoader::NewL(TUid id)
{
    TUid id_key;

    TAny *ptr = REComSession::CreateImplementationL(id, id_key);
    
    if (!ptr) 
    {
        User::Leave(KErrNoMemory);
    }

    CBellagioOpenMaxLoader* aLoader = REINTERPRET_CAST(CBellagioOpenMaxLoader*, ptr);
    
    aLoader->setId(id_key);

    return aLoader;
}	

CBellagioOpenMaxLoader::CBellagioOpenMaxLoader()
{
}

CBellagioOpenMaxLoader::~CBellagioOpenMaxLoader()
{
    if (loader != NULL)
    {
        delete loader;
        loader = 0;
    }

    REComSession::DestroyedImplementation(iDtor_ID_Key);
}
