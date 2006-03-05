#ifndef _OMX_TWOPORT_COMPONENT_H_
#define _OMX_TWOPORT_COMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <omx_base_component.h>

#define OMX_TWOPORT_INPUTPORT_INDEX 0
#define OMX_TWOPORT_OUTPUTPORT_INDEX 1


/** Twoport component private structure.
 */
DERIVEDCLASS(omx_twoport_component_PrivateType, base_component_PrivateType)
#define omx_twoport_component_PrivateType_FIELDS base_component_PrivateType_FIELDS \
	/** @param BufferMgmtCallback function pointer for algorithm callback */ \
	void (*BufferMgmtCallback)(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer);
ENDCLASS(omx_twoport_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_twoport_component_Constructor(stComponentType*);

void* omx_twoport_component_BufferMgmtFunction(void* param);

#endif //_OMX_TWOPORT_COMPONENT_H_
