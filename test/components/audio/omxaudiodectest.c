/**
	@file test/components/audio/omxaudiodectest.c
	
	Test application that uses two OpenMAX components, an audio decoder and 
	an alsa sink. The application receives an compressed audio stream on input port
	from a file, decodes it and sends it to the alsa sink, or to a file or standard output.
	The audio formats supported are:
	mp3 (ffmpeg and mad)
	ogg (libvorbis)
	
	Copyright (C) 2007  STMicroelectronics and Nokia

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
	
	$Date: 2007-04-05 16:39:38 +0200 (Thu, 05 Apr 2007) $
	Revision $Rev: 787 $
	Author $Author: giulio_urlini $
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

#include "omxaudiodectest.h"
#include "tsemaphore.h"


#define MP3_TYPE_SEL 1
#define VORBIS_TYPE_SEL 2
#define COMPONENT_NAME_BASE "OMX.st.audio_decoder"
#define BASE_ROLE "audio_decoder.ogg"
#define COMPONENT_NAME_BASE_LEN 20
#define SINK_NAME "OMX.st.alsa.alsasink"

appPrivateType* appPriv;

unsigned int nextBuffer = 0;

OMX_BUFFERHEADERTYPE *inBuffer1, *inBuffer2, *outBuffer1, *outBuffer2;
int buffer_in_size = BUFFER_IN_SIZE;
int buffer_out_size = BUFFER_OUT_SIZE;

OMX_PARAM_PORTDEFINITIONTYPE paramPort;
OMX_PARAM_COMPONENTROLETYPE paramRole;
OMX_AUDIO_PARAM_MP3TYPE paramMp3;
OMX_AUDIO_PARAM_VORBISTYPE paramOgg;
OMX_AUDIO_PARAM_PCMMODETYPE paramAudioPCM;

OMX_CALLBACKTYPE audiodeccallbacks = { 
		.EventHandler = audiodecEventHandler,
		.EmptyBufferDone = audiodecEmptyBufferDone,
		.FillBufferDone = audiodecFillBufferDone
};

OMX_CALLBACKTYPE audiosinkcallbacks = { 
		.EventHandler = audiosinkEventHandler,
		.EmptyBufferDone = audiosinkEmptyBufferDone,
		.FillBufferDone = NULL
};

int fd = 0;
appPrivateType* appPriv;

char *input_file, *output_file;

int selectedType = 0;
FILE *outfile;

void display_help() {
	printf("\n");
	printf("Usage: omx11audiodectest [-o outfile] [-tmdh] filename\n");
	printf("\n");
	printf("       -o outfile: If this option is specified, the decoded stream is written to outfile\n");
	printf("       -s single_ogg: Use the single role ogg decoder instead of the default one\n");
	printf("       -t: The audio decoder is tunneled with the alsa sink\n");
	printf("       -m: For mp3 decoding use the mad library\n");
	printf("       -d: If no output is specified, and no playback is specified,\n");
	printf("       	   this flag activated the print of the stream directly on std out\n");
	printf("       -h: Displays this help\n");
	printf("\n");
	exit(1);
}

OMX_ERRORTYPE test_OMX_ComponentNameEnum() {
	char * name;
	int index;

	OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
	name = malloc(128 * sizeof(char));
	index = 0;
	while(1) {
		err = OMX_ComponentNameEnum (name, 128, index);
		if ((name != NULL) && (err == OMX_ErrorNone)) {
		  DEBUG(DEFAULT_MESSAGES, "component %i is %s\n",index, name);
		} else break;
		if (err != OMX_ErrorNone) break;
		index++;
	}
	free(name);
	name = NULL;
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
	return err;
}

OMX_ERRORTYPE test_OMX_RoleEnum(OMX_STRING component_name) {
	OMX_U32 no_of_roles;
	OMX_U8 **string_of_roles;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int index;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Getting roles of %s. Passing Null first...\n", component_name);
	err = OMX_GetRolesOfComponent(component_name, &no_of_roles, NULL);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Not able to retrieve the number of roles of the given component\n");
	  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
		return err;
	}
  DEBUG(DEFAULT_MESSAGES, "The number of roles for the component %s is: %i\n", component_name, (int)no_of_roles);

	if(no_of_roles == 0) {
	  DEBUG(DEB_LEV_ERR, "The Number or roles is 0.\nThe component selected is not correct for the purpose of this test.\nExiting...\n");		
		err = OMX_ErrorInvalidComponentName;
	}	else {
		string_of_roles = (OMX_U8**)malloc(no_of_roles * sizeof(OMX_STRING));
		for (index = 0; index<no_of_roles; index++) {
			*(string_of_roles + index) = (OMX_U8 *)malloc(no_of_roles*128);
		}
	  DEBUG(DEB_LEV_SIMPLE_SEQ, "...then buffers\n");

		err = OMX_GetRolesOfComponent(component_name, &no_of_roles, string_of_roles);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Not able to retrieve the roles of the given component\n");
		} else if(string_of_roles != NULL) {
			for (index = 0; index < no_of_roles; index++) {
		  	DEBUG(DEFAULT_MESSAGES, "The role %i for the component:  %s \n", (index + 1), *(string_of_roles+index));
			}
		} else {
			DEBUG(DEB_LEV_ERR, "role string is NULL!!! Exiting...\n");
			err = OMX_ErrorInvalidComponentName;
		}
	}
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
	return err;
}

OMX_ERRORTYPE test_OMX_ComponentEnumByRole(OMX_STRING role_name) {
	OMX_U32 no_of_comp_per_role;
	OMX_U8 **string_of_comp_per_role;
	OMX_ERRORTYPE err;
	int index;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
	string_of_comp_per_role = (OMX_U8**)malloc(10 * sizeof(OMX_STRING));
	for (index = 0; index<10; index++) {
		string_of_comp_per_role[index] = malloc(128 * sizeof(char));
	}

 	DEBUG(DEFAULT_MESSAGES, "Getting number of components per role for %s\n", role_name);
		
	err = OMX_GetComponentsOfRole(role_name, &no_of_comp_per_role, NULL);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Not able to retrieve the number of components of a given role\n");
	  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
		return err;
	}
 	DEBUG(DEFAULT_MESSAGES, "Number of components per role for %s is %i\n", role_name, (int)no_of_comp_per_role);
		
	err = OMX_GetComponentsOfRole(role_name, &no_of_comp_per_role, string_of_comp_per_role);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Not able to retrieve the components of a given role\n");
	  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
		return err;
	}

 	DEBUG(DEFAULT_MESSAGES, " The components are:\n");
	for (index = 0; index < no_of_comp_per_role; index++) {
	 	DEBUG(DEFAULT_MESSAGES, "%s\n", string_of_comp_per_role[index]);
	}
	for (index = 0; index<10; index++) {
		if(string_of_comp_per_role[index]) {
			free(string_of_comp_per_role[index]);
			string_of_comp_per_role[index] = NULL;
		}
	}

	if(string_of_comp_per_role)	{
		free(string_of_comp_per_role);
		string_of_comp_per_role = NULL;
	}
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result OMX_ErrorNone\n",__func__);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE test_OpenClose(OMX_STRING component_name) {
	OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
	err = OMX_GetHandle(&appPriv->audiodechandle, component_name, NULL /*appPriv */, &audiodeccallbacks);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "No component found\n");
	} else {
		err = OMX_FreeHandle(appPriv->audiodechandle);
	}
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
	return err;
}

int flagIsOutputEspected;
int flagSetupTunnel;
int flagPlaybackOn;
int flagOutputReceived;
int flagInputReceived;
int flagIsMadRequested;
int flagDirect;
int flagSingleOGGSelected;

int main(int argc, char** argv) {
	
	OMX_ERRORTYPE err;
	int data_read;
	int index;
	int argn_dec;
	OMX_STRING full_component_name;

	if(argc < 2){
		display_help();
	} else {
		flagIsOutputEspected = 0;
		flagSetupTunnel = 0;
		flagPlaybackOn = 1;
		flagOutputReceived = 0;
		flagInputReceived = 0;
		flagIsMadRequested = 0;
		flagDirect = 0;
		flagSingleOGGSelected = 0;

		argn_dec = 1;
		while (argn_dec<argc) {
			if (*(argv[argn_dec]) =='-') {
				if (flagIsOutputEspected) {
					display_help();
				}
				switch (*(argv[argn_dec]+1)) {
					case 'h':
						display_help();
					break;
					case 's':
						flagSingleOGGSelected = 1;
					break;
					case 't':
						flagSetupTunnel = 1;
					break;
					case 'o':
						flagIsOutputEspected = 1;
						flagPlaybackOn = 0;
					break;
					case 'm':
						flagIsMadRequested = 1;
					break;
					case 'd':
						flagDirect = 1;
						flagPlaybackOn = 0;
					break;
					default:
						display_help();
				}
			} else {
				if (flagIsOutputEspected) {
					output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
			    strcpy(output_file,argv[argn_dec]);
					flagIsOutputEspected = 0;
					flagOutputReceived = 1;
				} else {
					input_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
			    strcpy(input_file,argv[argn_dec]);
					flagInputReceived = 1;
				}
			}
			argn_dec++;
		}
		if (flagSetupTunnel) {
			flagOutputReceived = 0;
			flagPlaybackOn = 1;
		}
		if (!flagInputReceived) {
			display_help();
		}
		DEBUG(DEFAULT_MESSAGES, "Options selected:\n");
		DEBUG(DEFAULT_MESSAGES, "Decode file %s", input_file);
		DEBUG(DEFAULT_MESSAGES, " to ");
		if (flagPlaybackOn) {
			DEBUG(DEFAULT_MESSAGES, " alsa sink");
			if (flagSetupTunnel) {
				DEBUG(DEFAULT_MESSAGES, " with tunneling\n");
			} else {
				DEBUG(DEFAULT_MESSAGES, " without tunneling\n");
			}
		} else {
			if (flagOutputReceived) {
				DEBUG(DEFAULT_MESSAGES, " %s\n", output_file);
			}
		}
	}
	index = 0;
	while(*(input_file+index) != '\0') {
		index++;
	}
	DEBUG(DEFAULT_MESSAGES, "Format selected ");
	if (*(input_file+index -1) == '3') {
		selectedType = MP3_TYPE_SEL;
		DEBUG(DEFAULT_MESSAGES, "MP3\n");
	} else if(*(input_file+index -1) == 'g') {
		selectedType = VORBIS_TYPE_SEL;
		DEBUG(DEFAULT_MESSAGES, "VORBIS\n");
	} else {
		DEBUG(DEB_LEV_ERR, "The input audio format is not supported - exiting\n");
		exit(1);
	}

	fd = open(input_file, O_RDONLY);
	if(fd == -1) {
		DEBUG(DEB_LEV_ERR,"Error in opening input file %s\n", input_file);
		exit(1);
	}

	/* Initialize application private data */
	appPriv = malloc(sizeof(appPrivateType));
	
	appPriv->decoderEventSem = malloc(sizeof(tsem_t));
	if (flagPlaybackOn) {
		appPriv->sinkEventSem = malloc(sizeof(tsem_t));
	}
	appPriv->eofSem = malloc(sizeof(tsem_t));
	
	tsem_init(appPriv->decoderEventSem, 0);
	if (flagPlaybackOn) {
		tsem_init(appPriv->sinkEventSem, 0);
	}
	tsem_init(appPriv->eofSem, 0);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Init the Omx core\n");
	err = OMX_Init();
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
		exit(1);
	}

  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
	test_OMX_ComponentNameEnum();
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
	test_OMX_RoleEnum(COMPONENT_NAME_BASE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
	test_OMX_ComponentEnumByRole(BASE_ROLE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
	test_OpenClose(COMPONENT_NAME_BASE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");


	full_component_name = (OMX_STRING) malloc(sizeof(char*) * OMX_MAX_STRINGNAME_SIZE);
	strcpy(full_component_name, COMPONENT_NAME_BASE);
	if(selectedType == MP3_TYPE_SEL) {
		strcpy(full_component_name+COMPONENT_NAME_BASE_LEN, ".mp3");
		if (flagIsMadRequested) {
			strcpy(full_component_name+COMPONENT_NAME_BASE_LEN+4, ".mad");
		}
	} else if (selectedType == VORBIS_TYPE_SEL) {
		strcpy(full_component_name+COMPONENT_NAME_BASE_LEN, ".ogg");
		if (flagSingleOGGSelected) {
			strcpy(full_component_name+COMPONENT_NAME_BASE_LEN+4, ".single");
		}
	}

	DEBUG(DEFAULT_MESSAGES, "The component selected for decoding is %s\n", full_component_name);
	err = OMX_GetHandle(&appPriv->audiodechandle, full_component_name, NULL, &audiodeccallbacks);
	if(err != OMX_ErrorNone){
		DEBUG(DEB_LEV_ERR, "No component found. Exiting...\n");
		exit(1);
	}
	if (flagPlaybackOn) {
		err = OMX_GetHandle(&appPriv->audiosinkhandle, SINK_NAME, NULL /*appPriv */, &audiosinkcallbacks);
		if(err != OMX_ErrorNone){
			DEBUG(DEB_LEV_ERR, "No sink found. Exiting...\n");
			exit(1);
		}
	}

	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamStandardComponentRole, &paramRole);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "The role set for this component can not be retrieved err = %i\n", err);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "The role currently set is %s\n", paramRole.cRole);

	paramPort.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	paramPort.nPortIndex = 0;
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}
	
	paramPort.nPortIndex = 1;
	err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	paramPort.nBufferCountActual = 2;
	err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &paramPort);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
		exit(1);
	}

	if (flagPlaybackOn) {
		paramPort.nPortIndex = 0;
		err = OMX_GetParameter(appPriv->audiosinkhandle, OMX_IndexParamPortDefinition, &paramPort);
		paramPort.nBufferCountActual = 2;
		err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamPortDefinition, &paramPort);
		if(err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Error in setting OMX_PORT_PARAM_TYPE parameter\n");
			exit(1);
		}
	}
	
	if (flagSetupTunnel) {
		OMX_SetupTunnel(appPriv->audiodechandle, 1, appPriv->audiosinkhandle, 0);
	}

	inBuffer1 = inBuffer2 = outBuffer1 = outBuffer2 = NULL;

	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

	if (flagPlaybackOn) {
		err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	}
	
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBuffer1, 0, NULL, buffer_in_size);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
		exit(1);
	}
	err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBuffer2, 0, NULL, buffer_in_size);
	if(err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
		exit(1);
	}
	
	if (!flagSetupTunnel) {
		err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBuffer1, 1, NULL, buffer_out_size);
		if(err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
			exit(1);
		}
		err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBuffer2, 1, NULL, buffer_out_size);
		if(err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
			exit(1);
		}
	}

	if ((flagPlaybackOn) && (!flagSetupTunnel)) {
		err = OMX_UseBuffer(appPriv->audiosinkhandle, &outBuffer1, 0, NULL, buffer_out_size, outBuffer1->pBuffer);
		if(err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
			exit(1);
		}
		err = OMX_UseBuffer(appPriv->audiosinkhandle, &outBuffer2, 0, NULL, buffer_out_size, outBuffer2->pBuffer);
		if(err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
			exit(1);
		}
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
	tsem_down(appPriv->decoderEventSem);
	if (flagPlaybackOn) {
		tsem_down(appPriv->sinkEventSem);
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "decoder Sem free\n");
	
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	tsem_down(appPriv->decoderEventSem);
	if (flagPlaybackOn) {
		err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
		tsem_down(appPriv->sinkEventSem);
	}
	
	if (!flagSetupTunnel) {
		outBuffer1->nOutputPortIndex = 1;
		outBuffer1->nInputPortIndex = 0;
		outBuffer2->nOutputPortIndex = 1;
		outBuffer2->nInputPortIndex = 0;

		err = OMX_FillThisBuffer(appPriv->audiodechandle, outBuffer1);
		err = OMX_FillThisBuffer(appPriv->audiodechandle, outBuffer2);
	}
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and decoderMutex\n");

	if (flagOutputReceived) {
	  outfile = fopen(output_file,"wb");
  	if(outfile == NULL) {
	    DEBUG(DEB_LEV_ERR, "Error at opening the output file");
			exit(1);
		} else {
  	  DEBUG(DEB_LEV_SIMPLE_SEQ, "Success at opening the output file");
		}
	}

	data_read = read(fd, inBuffer1->pBuffer, buffer_in_size);
	inBuffer1->nFilledLen = data_read;

	data_read = read(fd, inBuffer2->pBuffer, buffer_in_size);
	inBuffer2->nFilledLen = data_read;

	DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBuffer1);
	err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBuffer1);
	DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBuffer2);
	err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBuffer2);

	tsem_down(appPriv->eofSem);

	DEBUG(DEFAULT_MESSAGES, "The execution of the decoding process is terminated\n");
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (flagPlaybackOn) {
		err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	}	
	tsem_down(appPriv->decoderEventSem);
	if (flagPlaybackOn) {
		tsem_down(appPriv->sinkEventSem);
	}
	
	err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if (flagPlaybackOn) {
		err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	}
	
	DEBUG(DEB_LEV_PARAMS, "Audio dec to loaded\n");
	err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBuffer1);
	err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBuffer2);
	DEBUG(DEB_LEV_PARAMS, "Free Audio dec output ports\n");
	err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBuffer1);
	err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBuffer2);
	if (flagPlaybackOn) {
		err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, outBuffer1);
		err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, outBuffer2);
	}

	tsem_down(appPriv->decoderEventSem);
	if (flagPlaybackOn) {
		tsem_down(appPriv->sinkEventSem);
	}
	DEBUG(DEFAULT_MESSAGES, "All components released\n");
	
	OMX_FreeHandle(appPriv->audiodechandle);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "audiodec dec freed\n");
	OMX_Deinit();
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "All components freed. Closing...\n");
	free(appPriv->decoderEventSem);
	if (flagPlaybackOn) {
		free(appPriv->sinkEventSem);
		appPriv->sinkEventSem = NULL;
	}
	free(appPriv->eofSem);
	appPriv->eofSem = NULL;
	free(appPriv);
	appPriv = NULL;
	if (flagOutputReceived) {
	  fclose(outfile);
	}
	if(close(fd) == -1) {
		DEBUG(DEB_LEV_ERR,"Error in closing input file stream\n");
		exit(1);
	}
	else {
		DEBUG(DEB_LEV_SIMPLE_SEQ,"Succees in closing input file stream\n");
	}
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
		
	} else {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
	}
	tsem_up(appPriv->decoderEventSem);
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
		DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
		tsem_up(appPriv->eofSem);
		return OMX_ErrorNone;
	}
	pBuffer->nFilledLen = data_read;
	
	DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
	err = OMX_EmptyThisBuffer(hComponent, pBuffer);

	return OMX_ErrorNone;
}

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
			DEBUG(DEB_LEV_ERR, "Ouch! In %s: had 0 data size in output buffer...\n", __func__);
			return OMX_ErrorNone;
		}
		if ((!flagOutputReceived) && (!flagPlaybackOn) && (flagDirect)) {
			for(i = 0; i<pBuffer->nFilledLen; i++){
				putchar(*(char*)(pBuffer->pBuffer + i));
			}
			pBuffer->nFilledLen = 0;
			err = OMX_FillThisBuffer(hComponent, pBuffer);
		} else if ((flagOutputReceived) && (!flagPlaybackOn)) {
			if(pBuffer->nFilledLen > 0) {
  	    fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, outfile);
			}
			pBuffer->nFilledLen = 0;
			err = OMX_FillThisBuffer(hComponent, pBuffer);
		} else if ((!flagSetupTunnel) && (flagPlaybackOn))  { //playback on, redirect to alsa sink, if it is not tunneled
			OMX_EmptyThisBuffer(appPriv->audiosinkhandle, pBuffer);
		}
	}	else {
		DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audiosinkEventHandler(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_EVENTTYPE eEvent,
	OMX_OUT OMX_U32 Data1,
	OMX_OUT OMX_U32 Data2,
	OMX_OUT OMX_PTR pEventData) {
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
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
		
	} else {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
	}
	tsem_up(appPriv->sinkEventSem);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE audiosinkEmptyBufferDone(
	OMX_OUT OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_PTR pAppData,
	OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_ERRORTYPE err;
	DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
	
	DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", (int)pBuffer);
	err = OMX_FillThisBuffer(appPriv->audiodechandle, pBuffer);

	return OMX_ErrorNone;
}

