/**
	@file src/omxcore.h
	
	OpenMax Integration Layer Core. This library implements the OpenMAX core
	responsible for environment setup and components tunneling and communication.
	
	Copyright (C) 2006  STMicroelectronics

	@author Diego MELPIGNANO, Pankaj SEN, David SIORPAES, Giulio URLINI

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
	
	2006/05/11:  IL Core version 0.2

*/

#ifndef __ST_CORE_H
#define __ST_CORE_H

#include <OMX_Component.h>
#include <OMX_Types.h>

#include <pthread.h>
#include <sys/msg.h>

#include "omx_comp_debug_levels.h"
#include "tsemaphore.h"
#include "queue.h"


/** Defines the major version of the core */
#define SPECVERSIONMAJOR  1
/** Defines the minor version of the core */
#define SPECVERSIONMINOR  0
/** Defines the revision of the core */
#define SPECREVISION      0
/** Defines the step version of the core */
#define SPECSTEP          0

/** the TUNNEL_ESTABLISHED specifies if a port is tunneled.
	* It is assigned to a private field of the port if it is tunneled
	*/
#define TUNNEL_ESTABLISHED 0x0001
/** the TUNNEL_IS_SUPPLIER specifies if a tunneled port is the supplier.
	* It is assigned to a private field of the port if it is tunneled and also it is the buffer supplier for the tunnel.
	*/
#define TUNNEL_IS_SUPPLIER 0x0002

/* The following define values are used to characterize each buffer 
	* allocated or assigned to the component. A buffer list is 
	* created for each port of the component. The buffer can be assigned
	* to the port, or owned by the port. The buffer flag are applied for each buffer
	* in each port buffer list. The following use cases are currently implemented:
	* - When the IL Client asks the component to allocate a buffer 
	*   for a given port, with the call to AllocateBuffer, the 
	*   buffer created is characterizeed by the flag BUFFER_ALLOCATED
	* - When the IL Client asks the component to use a buffer allocated
	*   by the client itself, the buffer flag is BUFFER_ASSIGNED
	* - When the component is tunneled by another component, and the first
	*   is supplier of the buffer, the buffer is marked with the 
	*   BUFFER_ALLOCATED flag.
	* - When the component is tunneled by another component, and the second
	*   is supplier of the buffer, the buffer is marked with the 
	*   BUFFER_ASSIGNED flag.
	* - The case of a buffer supplied by the first component but allocated by another
	*   component or another port inside the same component, as in the case
	*   of shared buffers, is not yet implemented in these components
	* - During hte deallocation phase each buffer is marked with the BUFFER_FREE
	*   flag, so that the component can check if all the buffers have been deallocated 
	*   before switch the component state to Loaded, as specified by
	*   the OpenMAX specs
	*/

/** This flag is applied to a buffer when it is allocated
	* by the given port of the component 
	*/
#define BUFFER_ALLOCATED (1 << 0)
/** This flag is applied to a buffer when it is assigned 
	* from another port or by the IL client 
	*/
#define BUFFER_ASSIGNED (1 << 1)
/** This flag is applied to a buffer if its header has been allocated
	*/
#define HEADER_ALLOCATED (1 << 2)
/** This flag is applied to a buffer just deallocated 
	*/
#define BUFFER_FREE 0

/* Contains core related data structures
 * message handler thread descriptors, queue and semaphore.
 */

/** Core internal structure used for cummunication with components.
	* This structure is used to define the thread that handles the messages queue
	* THere exists only an instance of this structure, that is used only in the
	* OpenMAX core
	* It contains also the exit mutex used to synchronize the disposal of 
	* the OMX components with the message queue
	*/
typedef struct coreDescriptor_t{
	pthread_t messageHandlerThread; /// This field contains the reference to the thread that receives messages for the components
	int messageHandlerThreadID; /// The ID of the pthread that handles the messages for the components
	queue_t* messageQueue; /// The output queue for the messages to be send to the components
	tsem_t* messageSem; /// The queue access semahpore used for message read synchronization
	int exit_messageThread; /// boolean variable for the exit condition from the message thread
	pthread_mutex_t exit_mutex; /// mutex for access synchronization to the internal core fields during exiting the core
} coreDescriptor_t;


/** This structure contains all the fields of a message handled by the core */
typedef struct coreMessage_t coreMessage_t;
/** This field contains all the private data common to each component, both private and related to OpenMAX standard */
typedef struct stComponentType stComponentType;

struct stComponentType{
	OMX_COMPONENTTYPE omx_component; /// The OpenMAX standard data structure describing a component
	char* name; /// String that represents the name of the component, ruled by the standard
	OMX_STATETYPE state; /// Private field that contains the current state of the component
	OMX_CALLBACKTYPE* callbacks; /// pointer to every client callback function, as specified by the standard
	OMX_PTR callbackData; /// Private data that can be send with the client callbacks. Not specified by the standard
	OMX_U32 nports; /// Number of ports for the component.
	coreDescriptor_t* coreDescriptor; /// reference to the message queue and related info handled by the core
	OMX_ERRORTYPE (*constructor)(stComponentType*); /// constructor function pointer for each Linux ST OpenMAX component
	OMX_ERRORTYPE (*destructor)(stComponentType*);/// destructor function pointer for each Linux ST OpenMAX component
	OMX_ERRORTYPE (*messageHandler)(coreMessage_t*);/// This function receives messages from the message queue. It is needed for each Linux ST OpenMAX component

	int needsRegistation; /// This field specify if the store component structure has already been registered by the omxregister application
};

/** this flag specifies that the message send is a command */
#define SENDCOMMAND_MSG_TYPE 1
/** this flag specifies that the message send is an error message */
#define ERROR_MSG_TYPE       2
/** this flag specifies that the message send is a warning message */
#define WARNING_MSG_TYPE     3

struct coreMessage_t{
	stComponentType* stComponent; /// A reference to the main structure that defines a component. It represents the target of the message
	int messageType; /// the flag that specifies if the message is a command, a warning or an error
	int messageParam1; /// the first field of the message. Its use is the same as specified for the command in OpenMAX spec
	int messageParam2; /// the second field of the message. Its use is the same as specified for the command in OpenMAX spec
	OMX_PTR pCmdData; /// This pointer could contain some proprietary data not covered by the standard
};

/**
 * \brief Templates are stored in the templateList[] array for factoring
 * \param template The pointer to an existing template.
 * \return Whether the template was successfully registered (0) or not (-1)
 */
int register_template(stComponentType* template);

/** \brief Core's message handler thread function
 * Handles all messages coming from components and
 * processes them by dispatching them back to the
 * triggering component.
 */
void* messageHandlerFunction(void*);

/** \brief Fills in the header of a given structure with proper size and spec version
 * \param header Pointer to the structure
 * \param size Size of the structure
 */
void setHeader(OMX_PTR header, OMX_U32 size);

/** \brief Checks the header of a structure for consistency with size and spec version
 * \param header Pointer to the structure to be checked
 * \param size Size of the structure
 * \return OMX error code. If the header has failed the check, OMX_ErrorVersionMismatch
 * is returned. If the header fails the size check OMX_ErrorBadParameter is returned
 */
OMX_ERRORTYPE checkHeader(OMX_PTR header, OMX_U32 size);

/** Builds the component registry file given the components directory.
 * The registry file is placed in $HOME.
 * Meant to be used by the omxregister application.
 * \param componentspath Path where to search the components
 * \param ncomponents The number of components found
 * \return Error code
 */
int buildComponentsList(char* componentspath, int* ncomponents);

/** Loads the shared object that contains the implementation of the required
 * component.
 * The location of the shared object to load is obtained from the OMX registry file
 * \param component The component to load
 * \return Error code
 */
int loadComponentSO(char* component);

/** Reads the .omxregistry file and returns the name of the component at the specificed position.
	* \param position the specified position in the list of component 
	* \param size the max size of returned string
	* \param componentname the returned component name
	* \return it returns OMX_ErrorNone in case of successfull request, and OMX_ErrorNoMore in case of error
	*/
OMX_ERRORTYPE readOMXRegistry(int position, int size, char* componentname);

/** Checks whether the template of a given component exists
 * \cComponentName The name of the component
 * \return Returns 0 if the component template cannot be found
 */
static int isTemplateAvailable(char* cComponentName);
		 
#endif
