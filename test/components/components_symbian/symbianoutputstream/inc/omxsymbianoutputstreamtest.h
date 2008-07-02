/**
	@file test/components/components_symbian/symbianoutputstream/inc/omxsymbianoutputstreamtest.h

	Simple application that uses an OpenMAX Symbian output stream component. The application receives
	a pcm stream from a file or, if not specified, from standard input.
	The audio stream is inteded as stereo, at 44100 Hz.
	
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

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <omx_comp_debug_levels.h>

#include <pthread.h>
#include <sys/stat.h>

#include "tsemaphore.h"

/* Application's private data */
typedef struct appPrivateType{
	pthread_cond_t condition;
	pthread_mutex_t mutex;
	void* input_data;
	OMX_BUFFERHEADERTYPE* currentInputBuffer;
	tsem_t* eventSem;
}appPrivateType;

/* Size of the buffers requested to the component */
#define BUFFER_SIZE (16*1024)

/* Callback prototypes */
    /** The EventHandler method is used to notify the application when an
        event of interest occurs.  Events are defined in the OMX_EVENTTYPE
        enumeration.  Please see that enumeration for details of what will
        be returned for each type of event. Callbacks should not return
        an error to the component, so if an error occurs, the application 
        shall handle it internally.  This is a blocking call.

        The application should return from this call within 5 msec to avoid
        blocking the component for an excessively long period of time.

        @param hComponent
            handle of the component to access.  This is the component
            handle returned by the call to the GetHandle function.
        @param pAppData
            pointer to an application defined value that was provided in the 
            pAppData parameter to the OMX_GetHandle method for the component.
            This application defined value is provided so that the application 
            can have a component specific context when receiving the callback.
        @param eEvent
            Event that the component wants to notify the application about.
        @param nData1
            nData will be the OMX_ERRORTYPE for an error event and will be 
            an OMX_COMMANDTYPE for a command complete event and OMX_INDEXTYPE for a OMX_PortSettingsChanged event.
         @param nData2
            nData2 will hold further information related to the event. Can be OMX_STATETYPE for
            a OMX_StateChange command or port index for a OMX_PortSettingsCHanged event.
            Default value is 0 if not used. )
        @param pEventData
            Pointer to additional event-specific data (see spec for meaning).
      */
OMX_ERRORTYPE symbianoutputstreamEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 nData1,
	OMX_OUT OMX_U32 nData2,
	OMX_OUT OMX_PTR pEventData);

    /** The EmptyBufferDone method is used to return emptied buffers from an
        input port back to the application for reuse.  This is a blocking call 
        so the application should not attempt to refill the buffers during this
        call, but should queue them and refill them in another thread.  There
        is no error return, so the application shall handle any errors generated
        internally.  
        
        The application should return from this call within 5 msec.
        
        @param hComponent
            handle of the component to access.  This is the component
            handle returned by the call to the GetHandle function.
        @param pAppData
            pointer to an application defined value that was provided in the 
            pAppData parameter to the OMX_GetHandle method for the component.
            This application defined value is provided so that the application 
            can have a component specific context when receiving the callback.
        @param pBuffer
            pointer to an OMX_BUFFERHEADERTYPE structure allocated with UseBuffer
            or AllocateBuffer indicating the buffer that was emptied.
     */
OMX_ERRORTYPE symbianoutputstreamEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

    /** The FillBufferDone method is used to return filled buffers from an
        output port back to the application for emptying and then reuse.  
        This is a blocking call so the application should not attempt to 
        empty the buffers during this call, but should queue the buffers 
        and empty them in another thread.  There is no error return, so 
        the application shall handle any errors generated internally.  The 
        application shall also update the buffer header to indicate the
        number of bytes placed into the buffer.  

        The application should return from this call within 5 msec.
        
        @param hComponent
            handle of the component to access.  This is the component
            handle returned by the call to the GetHandle function.
        @param pAppData
            pointer to an application defined value that was provided in the 
            pAppData parameter to the OMX_GetHandle method for the component.
            This application defined value is provided so that the application 
            can have a component specific context when receiving the callback.
        @param pBuffer
            pointer to an OMX_BUFFERHEADERTYPE structure allocated with UseBuffer
            or AllocateBuffer indicating the buffer that was filled.
     */
OMX_ERRORTYPE symbianoutputstreamFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

/** Gets the file descriptor's size
 *
 * @param fd the file descriptor
 * @return the size of the file. If size cannot be computed
 * (e.g. stdin) zero is returned)
 */
static int getFileSize(int fd);
