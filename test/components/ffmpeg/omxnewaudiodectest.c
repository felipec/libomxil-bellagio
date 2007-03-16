/**
	@file test/components/ffmpeg/omx11audiodectest.c
	
	Test application that uses two OpenMAX components, an mp3 decoder and 
	an alsa sink. The application receives an mp3 stream in inupt, from a file
	or from standard input, decodes it and sends it to the alsa sink. 
	It is possible to specify the option -t that requests a tunnel between 
	OpenMAX components.
	
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
	
	2006/02/08:  Mp3 decoder and tunnel test version 0.1

*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <component_loader.h>

#include <st_static_component_loader.h>

#include "omxnewaudiodectest.h"
#include "tsemaphore.h"


#define MP3_TYPE_SEL 1
#define WMA_TYPE_SEL 2

appPrivateType* appPriv;

unsigned int nextBuffer = 0;
int stateTransitionExecToIdle=0;

OMX_BUFFERHEADERTYPE *inBuffer1, *inBuffer2, *outBuffer1, *outBuffer2;
int buffer_in_size = BUFFER_IN_SIZE;
int buffer_out_size = BUFFER_OUT_SIZE;

OMX_PARAM_PORTDEFINITIONTYPE paramPort;
OMX_PARAM_COMPONENTROLETYPE paramRole;
OMX_AUDIO_PARAM_WMATYPE paramWma;
OMX_AUDIO_PARAM_MP3TYPE paramMp3;
int fd = 0;
OMX_AUDIO_PARAM_PCMMODETYPE paramAudioPCM;

appPrivateType* appPriv;

int main(int argc, char** argv)
{
	
	OMX_ERRORTYPE err;
	OMX_U32 no_of_roles;
	OMX_U8* *string_of_roles;
	
	OMX_U32 no_of_comp_per_role;
	OMX_U8* *string_of_comp_per_role;
	
	OMX_STRING single_role;
	
	int isTunneled = 0;
	int data_read;
	int index = 0;
	int innerindex = 0;
	int selectedType = 0;
	char * name;
	int string_index = 0;

  BOSA_COMPONENTLOADER* componentLoader;

	OMX_CALLBACKTYPE audiodeccallbacks = { .EventHandler = audiodecEventHandler,
											 .EmptyBufferDone = audiodecEmptyBufferDone,
											 .FillBufferDone = audiodecFillBufferDone
	};
	
	if(argc < 2){
		DEBUG(DEB_LEV_ERR, "Usage: omx11audiodectest filename.mp3/filename.wma\n");
		exit(1);
	} else {
		fd = open(argv[1], O_RDONLY);
		while(*(argv[1]+index) != '\0') {
			index++;
		}
		if (*(argv[1]+index -1) == '3') {
			selectedType = MP3_TYPE_SEL;
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Format selected MP3\n");
		} else {
			selectedType = WMA_TYPE_SEL;
			DEBUG(DEB_LEV_SIMPLE_SEQ, "Format selected WMA\n");
		}
	}
	/* Initialize application private data */
	appPriv = malloc(sizeof(appPrivateType));
	
	appPriv->decoderEventSem = malloc(sizeof(tsem_t));
	appPriv->eofSem = malloc(sizeof(tsem_t));
	
	tsem_init(appPriv->decoderEventSem, 0);
	tsem_init(appPriv->eofSem, 0);

  /** this function should be used by the omx core, and not by the application to create the set of function pointers.
		It creates an instance of component loader.
	*/
  /*
  componentLoader=(BOSA_COMPONENTLOADER*)malloc(sizeof(BOSA_COMPONENTLOADER));

	componentLoader->BOSA_CreateComponentLoader = BOSA_ST_CreateComponentLoader;
	componentLoader->BOSA_DestroyComponentLoader = BOSA_ST_DestroyComponentLoader;
	componentLoader->BOSA_CreateComponent = BOSA_ST_CreateComponent;
	componentLoader->BOSA_DestroyComponent = BOSA_ST_DestroyComponent;
	componentLoader->BOSA_ComponentNameEnum = BOSA_ST_ComponentNameEnum;
	componentLoader->BOSA_GetRolesOfComponent = BOSA_ST_GetRolesOfComponent;
	componentLoader->BOSA_GetComponentsOfRole = BOSA_ST_GetComponentsOfRole;
  */

  /*

  BOSA_ST_InitComponentLoader(&componentLoader);
  //BOSA_ST_DeinitComponentLoader(componentLoader);
  
  if (componentLoader==NULL)
    DEBUG(DEB_LEV_ERR, "componentLoader is NULL\n");
  else {
    DEBUG(DEB_LEV_ERR, "componentLoader %x is Not null\n",componentLoader);

    if (componentLoader->BOSA_CreateComponentLoader)
      DEBUG(DEB_LEV_ERR, "componentLoader BOSA_CreateComponentLoader is NULL\n");
    else
      DEBUG(DEB_LEV_ERR, "componentLoader %x is BOSA_CreateComponentLoader Not null\n",componentLoader->BOSA_CreateComponentLoader);
  }

  BOSA_AddComponentLoader(componentLoader);
  */

	err = OMX_Init();

	name = malloc(128 * sizeof(char));
	index = 0;
	while(1) {
		err = OMX_ComponentNameEnum (name, 128, index);
		if (name != NULL) {
		  DEBUG(DEB_LEV_SIMPLE_SEQ, "component %i is %s\n",index, name);
		} else break;
		if (err != OMX_ErrorNone) break;
		index++;
	}

  free(name);
	
	//getting roles of component
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Null first...\n");
	OMX_GetRolesOfComponent("OMX.st.audio_decoder", &no_of_roles, NULL);
  DEBUG(DEB_LEV_ERR, "THE NO OF ROLES FOR COMPONENT :  %lu \n",no_of_roles);
	
	//check if the component whose roles were queried actually exists---print accordingly
	if(no_of_roles == 0)
	{
	  DEBUG(DEB_LEV_ERR, "NO COMPONENT EXISTS--CHECK COMPONENT NAME--SO NO ROLE CAN BE SPECIFIED\n");		
		exit(1);
	}	
	else
	{
		string_of_roles = (OMX_U8**)malloc(no_of_roles * sizeof(OMX_STRING));
		for (index = 0; index<no_of_roles; index++) {
			*(string_of_roles + index) = (char *)malloc(no_of_roles*128);
		}
	  DEBUG(DEB_LEV_SIMPLE_SEQ, "...then buffers\n");

		OMX_GetRolesOfComponent("OMX.st.audio_decoder", &no_of_roles, string_of_roles);
	  //if string_of_roles field is null then no role can be printed
	  if(string_of_roles != NULL) {
			for (index = 0; index < no_of_roles; index++) {
		  	DEBUG(DEB_LEV_ERR, "THE ROLE %i FOR COMPONENT :  %s \n", (index + 1), *(string_of_roles+index));
        //free(*(string_of_roles+index));
			}
		} else {
			DEBUG(DEB_LEV_ERR, "role string is NULL!!! Exiting...");
			exit(1);
		}
    //free(string_of_roles);
	}
	
	// getting components of a role
  /*
	string_of_comp_per_role = (OMX_U8**)malloc(10 * sizeof(OMX_STRING));
	for (index = 0; index<10; index++) {
		string_of_comp_per_role[index] = malloc(128 * sizeof(char));
	}
	for (index = 0; index < no_of_roles; index++) {
		no_of_comp_per_role = 0;
	 	DEBUG(DEB_LEV_SIMPLE_SEQ, "Getting number of components per role for %s\n", string_of_roles[index]);
		err = OMX_GetComponentsOfRole (string_of_roles[index], &no_of_comp_per_role, NULL);
	 	DEBUG(DEB_LEV_SIMPLE_SEQ, "Number of components per role for %s is %i\n", string_of_roles[index], no_of_comp_per_role);
		err = OMX_GetComponentsOfRole (string_of_roles[index], &no_of_comp_per_role, string_of_comp_per_role);
	 	DEBUG(DEB_LEV_SIMPLE_SEQ, " The components are:\n");
		for (innerindex = 0; innerindex < no_of_comp_per_role; innerindex++) {
		 	DEBUG(DEB_LEV_SIMPLE_SEQ, " - %s\n", string_of_comp_per_role[innerindex]);
		}
	}  
  */

	DEBUG(DEB_LEV_SIMPLE_SEQ, " Getting the handle...\n");
	/** Ask the core for a handle to the audio decoder component
	 */
  /*
	err = OMX_GetHandle(&appPriv->audiodechandle, "OMX.st.audio_decoder.mp3", NULL , &audiodeccallbacks);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "No component found. Exiting...\n");
		exit(1);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, " Got the handle... %x\n", &appPriv->audiodechandle);
 	OMX_FreeHandle(appPriv->audiodechandle);
	DEBUG(DEB_LEV_SIMPLE_SEQ, " Freed the handle...%x\n", &appPriv->audiodechandle);
  */
	err = OMX_GetHandle(&appPriv->audiodechandle, "OMX.st.audio_decoder.mp3", NULL /*appPriv */, &audiodeccallbacks);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "No component found. Exiting...\n");
		exit(1);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, " Got the handle...\n");

	single_role = (char*)malloc(128 * sizeof(char));
	index = 0;
	/** The following test calls a functionality that is not intended to be used directly by the IL client. 
		* It is executed here only for debugging purposes.
		*/
  /*
	while(1) {
		err = ((OMX_COMPONENTTYPE*)appPriv->audiodechandle)->ComponentRoleEnum(&appPriv->audiodechandle, single_role, index);
		if ((err != OMX_ErrorNone) || (single_role == NULL)) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "End of roles for this component\n");
			break;
		}
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Role %i for component is %s\n", index, single_role);
		index++;
	}
  */

	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamStandardComponentRole, &paramRole);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "The role set for this component can not be retrieved err = %i\n", err);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "The role currently set is %s\n", paramRole.cRole);
	// Inspect the list of roles and select the desired one
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Inspect roles of the component\n");
	for (index = 0; index < no_of_roles; index++) {
		string_index = 0;
		while( *((*(string_of_roles+index))+string_index) != '\0') {string_index++;}
		if ( *((*(string_of_roles+index))+string_index-1) == '3' ) {
			if (selectedType == MP3_TYPE_SEL) {
				strcpy(single_role, (*(string_of_roles+index)));
			}
		} else {
			if (selectedType == WMA_TYPE_SEL) {
				strcpy(single_role, (*(string_of_roles+index)));
			}
		}
	}
	DEBUG(DEB_LEV_ERR, "Assign the role %s\n", single_role);
	strcpy(paramRole.cRole, single_role);

  

	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamStandardComponentRole, &paramRole);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "The new role can not be set for this component err = %08x\n", err);
		exit(1);
	}
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamStandardComponentRole, &paramRole);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "The role set for this component can not be retrieved err = %08x\n", err);
		exit(1);
	}

   if(string_of_roles != NULL) {
			for (index = 0; index < no_of_roles; index++) {
        free(*(string_of_roles+index));
			}
      free(string_of_roles);
   } 
    
  free(single_role);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "The role currently set is %s\n", paramRole.cRole);
	//--------------------------------------------------------------------------------------------------------
	/* following code is to check the set & get parameter calls for wma decoders */
	
	//getting the parameters of wma decoders	
	//if selected type is wma


  if( selectedType == WMA_TYPE_SEL) 
  {
    err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamAudioWma,&paramWma);
    if(err!=OMX_ErrorNone)
    {
      DEBUG(DEB_LEV_ERR,"could not GET the parameters related to audio format  WMA for 1st time\n");
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "The parameters obtained from 1st getparameter call :\n");
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Port Index : %d No of channels : %d Bit Rate : %d Audio Format : %d\n", paramWma.nPortIndex, paramWma.nChannels,paramWma.nBitRate, paramWma.eFormat);

    paramWma.nSize = sizeof(OMX_AUDIO_PARAM_WMATYPE);
    paramWma.nBitRate = 44000;
    paramWma.eFormat = OMX_AUDIO_WMAFormat7;
    err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamAudioWma, &paramWma);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "could not SET the parameters related to audio format WMA\n");
    }

    //getting parameters for 2nd time
    err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamAudioWma,&paramWma);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"could not GET the parameters related to audio format  WMA for 2nd time\n");
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "The parameters obtained from 2nd getparameter call :\n");
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Port Index : %d No of channels : %d Bit Rate : %d Audio Format : %d\n", paramWma.nPortIndex, paramWma.nChannels, paramWma.nBitRate, paramWma.eFormat);

  } //end if based on selected type wma

    //------------------------------------------------------------------------------------------------------------

	
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamAudioPcm, &paramAudioPCM);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Getting WRONG----------------------- %i\n", err);
	}
	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamAudioPcm, &paramAudioPCM);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Setting WRONG----------------------- %i\n", err);
	}
	

   //--------------------------------------------------------------------------------------------------------------
/* following code is to check the set & get parameter calls for mp3 decoders */
	

	//getting the parameters of mp3 decoders 
	//if selected type is mp3	

  if( selectedType == MP3_TYPE_SEL)
  {
    err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamAudioMp3,&paramMp3);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"could not GET the parameters related to audio format MP3 for 1st time\n");
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "The parameters obtained from 1st getparameter call :\n");
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Port Index : %d No of channels : %d Bit Rate : %d Sample rate : %d  Audio band width : %d  Channel mode : %d\n", paramMp3.nPortIndex, paramMp3.nChannels,paramMp3.nBitRate, paramMp3.nSampleRate,paramMp3.nAudioBandWidth,paramMp3.eChannelMode);

    //setting some parameters
    paramMp3.nSize = sizeof(OMX_AUDIO_PARAM_MP3TYPE);
    paramMp3.nBitRate = 44000;
    paramMp3.nSampleRate=10000;
    paramMp3.nAudioBandWidth=5;
    paramMp3.eChannelMode = OMX_AUDIO_ChannelModeMono;
    err = OMX_SetParameter(appPriv->audiodechandle,OMX_IndexParamAudioMp3,&paramMp3);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"could not SET the parameters related to audio format MP3 err = %i\n", err);
    }

    //getting parameters for 2nd time
    err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamAudioMp3,&paramMp3);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"could not GET the parameters related to audio format MP3 for 2nd time\n");
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "The parameters obtained from 2nd getparameter call :\n");
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Port Index : %d No of channels : %d Bit Rate : %d Sample rate : %d  Audio band width : %d  Channel mode : %d\n", paramMp3.nPortIndex, paramMp3.nChannels,paramMp3.nBitRate, paramMp3.nSampleRate,paramMp3.nAudioBandWidth,paramMp3.eChannelMode);

  } // end if based on selected type mp3



//---------------------------------------------------------------------------------------------------------------------	
	/** Set the number of ports for the dummy component
	 */
	
	paramPort.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	paramPort.nPortIndex = 0;
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parametererr = %08x\n", err);
		exit(1);
	}
	
	paramPort.nPortIndex = 1;
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter = %08x\n", err);
		exit(1);
	}
	
	inBuffer1 = inBuffer2 = outBuffer1 = outBuffer2 = NULL;

	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBuffer1, 0, NULL, buffer_in_size);
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBuffer2, 0, NULL, buffer_in_size);
	
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBuffer1, 1, NULL, buffer_out_size);
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBuffer2, 1, NULL, buffer_out_size);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
	tsem_down(appPriv->decoderEventSem);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "decoder Sem free\n");
	
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	tsem_down(appPriv->decoderEventSem);
		
	err = OMX_FillThisBuffer(appPriv->audiodechandle, outBuffer1);
	err = OMX_FillThisBuffer(appPriv->audiodechandle, outBuffer2);
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and decoderMutex\n");
	
	data_read = read(fd, inBuffer1->pBuffer, buffer_in_size);
	inBuffer1->nFilledLen = data_read;
		
	data_read = read(fd, inBuffer2->pBuffer, buffer_in_size);
	inBuffer2->nFilledLen = data_read;

	DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", inBuffer1);
	err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBuffer1);
	DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", inBuffer2);
	err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBuffer2);

	tsem_down(appPriv->eofSem);

  stateTransitionExecToIdle=1;

	DEBUG(DEB_LEV_ERR, "Stop audiodec dec\n");
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	
	tsem_down(appPriv->decoderEventSem);
	
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	
	DEBUG(DEB_LEV_PARAMS, "Mp3 dec to loaded\n");
	err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBuffer1);
	err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBuffer2);
	DEBUG(DEB_LEV_PARAMS, "Free Mp3 dec output ports\n");
	err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBuffer1);
	err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBuffer2);

	tsem_down(appPriv->decoderEventSem);
	DEBUG(DEB_LEV_ERR, "All components released\n");
	
	OMX_FreeHandle(appPriv->audiodechandle);
	DEBUG(DEB_LEV_ERR, "audiodec dec freed\n");
	OMX_Deinit();
	
	DEBUG(DEB_LEV_ERR, "All components freed. Closing...\n");
	free(appPriv->decoderEventSem);
	free(appPriv->eofSem);
	free(appPriv);
	
	return 0;
}

/* Callbacks implementation */

OMX_ERRORTYPE audiodecEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent==OMX_EventCmdComplete){
	  if (Data1 == OMX_CommandStateSet) {
		  DEBUG(DEB_LEV_SIMPLE_SEQ, "State changed in ");
		  switch ((int)Data2) {
			  case OMX_StateInvalid:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
				  break;
			  case OMX_StateLoaded:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateLoaded\n");
				  break;
			  case OMX_StateIdle:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle\n");
				  break;
			  case OMX_StateExecuting:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateExecuting\n");
				  break;
			  case OMX_StatePause:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
				  break;
			  case OMX_StateWaitForResources:
				  DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
				  break;
		  }
      
      tsem_up(appPriv->decoderEventSem);
		  
	  } else {
		  DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
		  DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
	  }
  }
	//tsem_up(appPriv->decoderEventSem);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiodecEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	int data_read;
	DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
	data_read = read(fd, pBuffer->pBuffer, buffer_in_size);
	if (data_read <= 0) {
		DEBUG(DEB_LEV_ERR, "In the %s no more input data available\n", __func__);
		tsem_up(appPriv->eofSem);
		return OMX_ErrorNone;
	}
	pBuffer->nFilledLen = data_read;
	
	DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", pBuffer);
	err = OMX_EmptyThisBuffer(hComponent, pBuffer);

	return OMX_ErrorNone;
}

int total_size = 0;

OMX_ERRORTYPE audiodecFillBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	int i;	
	/* Output data to alsa sink */
	if(pBuffer != NULL){
		if (pBuffer->nFilledLen == 0) {
			DEBUG(DEB_LEV_ERR, "Ouch! In %s: no data in the output buffer!!!\n", __func__);
      return OMX_ErrorNone;
		} else {
			total_size += pBuffer->nFilledLen;
		}

    //DEBUG(DEB_LEV_ERR, "In %s: has buffer (size=%d)to output...\n", __func__,pBuffer->nFilledLen);
    /*
		for(i=0;i<pBuffer->nFilledLen;i++){
			putchar(*(char*)(pBuffer->pBuffer + i));
		}
    */
		pBuffer->nFilledLen = 0;
	}
	else {
		DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
	}
  if(stateTransitionExecToIdle==0)
	  err = OMX_FillThisBuffer(hComponent, pBuffer);
  else
    DEBUG(DEB_LEV_ERR, "stateTransitionExecToIdle Not Sending OMX_FillThisBuffer\n");
  
  return OMX_ErrorNone;
}

