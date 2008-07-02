/**
  @file src/loaders/loaders_symbian/symbianecomloader/inc/nrc_symbian_component_loader.h

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

#ifndef __NRC_SYMBIAN_COMPONENT_LOADER_H__
#define __NRC_SYMBIAN_COMPONENT_LOADER_H__

#include <OMX_Component.h>
#include <OMX_Types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "omxcore.h"

void
setup_component_loader(BOSA_COMPONENTLOADER* loader);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_InitComponentLoader(BOSA_COMPONENTLOADER *loader);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_DeInitComponentLoader(BOSA_COMPONENTLOADER *loader);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_CreateComponent(BOSA_COMPONENTLOADER *loader,
							  OMX_OUT OMX_HANDLETYPE* pHandle,
							  OMX_IN  OMX_STRING cComponentName,
							  OMX_IN  OMX_PTR pAppData,
							  OMX_IN  OMX_CALLBACKTYPE* pCallBacks);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_DestroyComponent(BOSA_COMPONENTLOADER *loader, 
							   OMX_HANDLETYPE hComponent);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_ComponentNameEnum(BOSA_COMPONENTLOADER *loader,
								OMX_STRING cComponentName,
								OMX_U32 nNameLength,
								OMX_U32 nIndex);

OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_GetRolesOfComponent(BOSA_COMPONENTLOADER *loader,
								  OMX_STRING compName,
								  OMX_U32 *pNumRoles,
								  OMX_U8 **roles);

OMX_API OMX_ERRORTYPE 
BOSA_NRC_SYMBIAN_GetComponentsOfRole(BOSA_COMPONENTLOADER *loader,
								  OMX_STRING role,
								  OMX_U32 *pNumComps,
								  OMX_U8  **compNames);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NRC_SYMBIAN_COMPONENT_LOADER_H__ */
