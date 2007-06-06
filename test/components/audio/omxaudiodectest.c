/**
  @file test/components/testaudio/omxaudiodectest.c

  Test application that uses three OpenMAX components, a file reader,an audio decoder 
  and an alsa sink. The application receives an compressed audio stream on input port
  from a file, decodes it and sends it to the alsa sink, or to a file or standard output.
  The audio formats supported are:
  mp3 (ffmpeg)

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

  $Date: 2007-06-05 06:20:07 +0200 (Tue, 05 Jun 2007) $
  Revision $Rev: 918 $
  Author $Author: pankaj_sen $
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
#define FILE_READER "OMX.st.audio_filereader"
#define AUDIO_EFFECT "OMX.st.volume.component"

appPrivateType* appPriv;

unsigned int nextBuffer = 0;

OMX_BUFFERHEADERTYPE *outBufferFileRead1, *outBufferFileRead2;
OMX_BUFFERHEADERTYPE *inBufferAudioDec1, *inBufferAudioDec2,*outBufferAudioDec1, *outBufferAudioDec2;
OMX_BUFFERHEADERTYPE *inBufferVolume1, *inBufferVolume2,*outBufferVolume1, *outBufferVolume2;
OMX_BUFFERHEADERTYPE *inBufferSink1, *inBufferSink2;
int buffer_in_size = BUFFER_IN_SIZE; 
int buffer_out_size = BUFFER_OUT_SIZE;
static OMX_BOOL bEOS=OMX_FALSE;

OMX_PARAM_PORTDEFINITIONTYPE paramPort,decparamPort;
OMX_PARAM_COMPONENTROLETYPE paramRole;
OMX_AUDIO_PARAM_MP3TYPE paramMp3;
OMX_AUDIO_PARAM_VORBISTYPE paramOgg;
OMX_AUDIO_PARAM_PCMMODETYPE paramAudioPCM;

OMX_CALLBACKTYPE audiodeccallbacks = { 
  .EventHandler    = audiodecEventHandler,
  .EmptyBufferDone = audiodecEmptyBufferDone,
  .FillBufferDone  = audiodecFillBufferDone
};

OMX_CALLBACKTYPE audiosinkcallbacks = { 
  .EventHandler    = audiosinkEventHandler,
  .EmptyBufferDone = audiosinkEmptyBufferDone,
  .FillBufferDone  = NULL
};

OMX_CALLBACKTYPE filereadercallbacks = { 
  .EventHandler    = filereaderEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = filereaderFillBufferDone
};

OMX_CALLBACKTYPE volumecallbacks = { 
  .EventHandler    = volumeEventHandler,
  .EmptyBufferDone = volumeEmptyBufferDone,
  .FillBufferDone  = volumeFillBufferDone
};



int fd = 0;
appPrivateType* appPriv;

char *input_file, *output_file;

int selectedType = 0;
FILE *outfile;

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}


void display_help() {
  printf("\n");
  printf("Usage: omxaudiodectest [-o outfile] [-stmdgh] filename\n");
  printf("\n");
  printf("       -o outfile: If this option is specified, the decoded stream is written to outfile\n");
  printf("                   This option can't be used with '-t' \n");
  printf("       -s single_ogg: Use the single role ogg decoder instead of the default one. Can't be used with -m or .mp3 file\n");
  printf("       -t: The audio decoder is tunneled with the alsa sink\n");
  printf("       -m: For mp3 decoding use the mad library. Can't be used with -s or .ogg file\n");
  printf("       -d: If no output is specified, and no playback is specified,\n");
  printf("       	   this flag activated the print of the stream directly on std out\n");
  printf("       -g: Gain of the audio sink[0...100]\n");
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
    for (index = 0; index<no_of_roles; index++) {
      free(*(string_of_roles + index));
    }
    free(string_of_roles);
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
  
  DEBUG(DEFAULT_MESSAGES, "Getting number of components per role for %s\n", role_name);

  err = OMX_GetComponentsOfRole(role_name, &no_of_comp_per_role, NULL);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Not able to retrieve the number of components of a given role\n");
    DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
    return err;
  }
  DEBUG(DEFAULT_MESSAGES, "Number of components per role for %s is %i\n", role_name, (int)no_of_comp_per_role);

  string_of_comp_per_role = (OMX_U8**)malloc(no_of_comp_per_role * sizeof(OMX_STRING));
  for (index = 0; index<no_of_comp_per_role; index++) {
    string_of_comp_per_role[index] = malloc(128 * sizeof(char));
  }

  err = OMX_GetComponentsOfRole(role_name, &no_of_comp_per_role, string_of_comp_per_role);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Not able to retrieve the components of a given role\n");
    DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
    for (index = 0; index<no_of_comp_per_role; index++) {
      if(string_of_comp_per_role[index]) {
        free(string_of_comp_per_role[index]);
        string_of_comp_per_role[index] = NULL;
      }
    }

    if(string_of_comp_per_role)	{
      free(string_of_comp_per_role);
      string_of_comp_per_role = NULL;
    }
    return err;
  }

  DEBUG(DEFAULT_MESSAGES, " The components are:\n");
  for (index = 0; index < no_of_comp_per_role; index++) {
    DEBUG(DEFAULT_MESSAGES, "%s\n", string_of_comp_per_role[index]);
  }
  for (index = 0; index<no_of_comp_per_role; index++) {
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
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s err %08x in Free Handle\n",__func__,err);
    }
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n",__func__, err);
  return err;
}

int flagIsOutputExpected;
int flagSetupTunnel;
int flagPlaybackOn;
int flagOutputReceived;
int flagInputReceived;
int flagIsMadRequested;
int flagDirect;
int flagSingleOGGSelected;
int flagUsingFFMpeg;
int flagIsGain;


int main(int argc, char** argv) {
  int argn_dec;
  char *temp = NULL;
  OMX_ERRORTYPE err;
  OMX_INDEXTYPE eIndexParamFilename;
  OMX_STRING full_component_name;
  int index;
  OMX_PTR pExtraData;
  OMX_INDEXTYPE eIndexExtraData;
  int data_read;
  int gain=-1;
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume;

  if(argc < 2){
    display_help();
  } else {
    flagIsOutputExpected = 0;
    flagSetupTunnel = 0;
    flagPlaybackOn = 1;
    flagOutputReceived = 0;
    flagInputReceived = 0;
    flagIsMadRequested = 0;
    flagDirect = 0;
    flagSingleOGGSelected = 0;
    flagUsingFFMpeg = 1;
    flagIsGain = 0;

    argn_dec = 1;
    while (argn_dec<argc) {
      if (*(argv[argn_dec]) =='-') {
        if (flagIsOutputExpected) {
          display_help();
        }
        switch (*(argv[argn_dec]+1)) {
        case 'h':
          display_help();
          break;
        case 's':
          flagSingleOGGSelected = 1;
          flagUsingFFMpeg = 0;
          break;
        case 't':
          flagSetupTunnel = 1;
          break;
        case 'o':
          flagIsOutputExpected = 1;
          flagPlaybackOn = 0;
          break;
        case 'm':
          flagIsMadRequested = 1;
          flagUsingFFMpeg = 0;
          break;
        case 'd':
          flagDirect = 1;
          flagPlaybackOn = 0;
          break;
        case 'g':
          flagIsGain = 1;
          break;
        default:
          display_help();
        }
      } else {
        if (flagIsGain) {
          gain = (int)atoi(argv[argn_dec]);
          flagIsGain = 0;
          if(gain > 100) {
            DEBUG(DEFAULT_MESSAGES, "Gain should be between [0..100]\n");
            gain = 100; 
          }
        } else if (flagIsOutputExpected) {
          output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
          strcpy(output_file,argv[argn_dec]);
          flagIsOutputExpected = 0;
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
	

  if (flagOutputReceived) {
    outfile = fopen(output_file,"wb");
    if(outfile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error at opening the output file");
      exit(1);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Success at opening the output file");
    }
  }

  if(!flagUsingFFMpeg) {
    fd = open(input_file, O_RDONLY);
	  if(fd == -1) {
		  DEBUG(DEB_LEV_ERR,"Error in opening input file %s\n", input_file);
		  exit(1);
	  }
  }

  /** initializing appPriv structure */
  appPriv = malloc(sizeof(appPrivateType));
  appPriv->filereaderEventSem = malloc(sizeof(tsem_t));
  appPriv->decoderEventSem = malloc(sizeof(tsem_t));
  appPriv->eofSem = malloc(sizeof(tsem_t));
  if (flagPlaybackOn) {
    appPriv->sinkEventSem = malloc(sizeof(tsem_t));
    tsem_init(appPriv->sinkEventSem, 0);
    appPriv->volumeEventSem = malloc(sizeof(tsem_t));
    tsem_init(appPriv->volumeEventSem, 0);
  }
  tsem_init(appPriv->filereaderEventSem, 0);
  tsem_init(appPriv->decoderEventSem, 0);
  tsem_init(appPriv->eofSem, 0);

  /** initialising openmax */
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

  if(flagUsingFFMpeg) {
    /** file reader component name -- gethandle*/
    err = OMX_GetHandle(&appPriv->filereaderhandle, FILE_READER, NULL /*appPriv */, &filereadercallbacks);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "FileReader Component Not Found\n");
      exit(1);
    }  
  }
	
  /** getting the handle of audio decoder */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Getting Audio %s Decoder Handle\n",full_component_name);
  err = OMX_GetHandle(&appPriv->audiodechandle, full_component_name, NULL /*appPriv */, &audiodeccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Audio Decoder Component Not Found\n");
		exit(1);
  } 
  DEBUG(DEFAULT_MESSAGES, "Component %s opened\n", full_component_name);
  if (flagPlaybackOn) {
    err = OMX_GetHandle(&appPriv->audiosinkhandle, SINK_NAME, NULL , &audiosinkcallbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No sink found. Exiting...\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES, "Getting Handle for Component %s\n", AUDIO_EFFECT);
    err = OMX_GetHandle(&appPriv->volumehandle, AUDIO_EFFECT, NULL , &volumecallbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No sink found. Exiting...\n");
      exit(1);
    }

    if((gain >= 0) && (gain <100)) {
      err = OMX_GetConfig(appPriv->volumehandle, OMX_IndexConfigAudioVolume, &sVolume);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
      }
      sVolume.sVolume.nValue = gain;
      DEBUG(DEFAULT_MESSAGES, "Setting Gain %d \n", gain);
      err = OMX_SetConfig(appPriv->volumehandle, OMX_IndexConfigAudioVolume, &sVolume);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
      }
    }
  }

  if(flagUsingFFMpeg) {
    /** setting the input audio format in file reader */
    err = OMX_GetExtensionIndex(appPriv->filereaderhandle,"OMX.ST.index.param.filereader.inputfilename",&eIndexParamFilename);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
			exit(1);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"FileName Param index : %x \n",eIndexParamFilename);
      temp = (char *)malloc(25);
      OMX_GetParameter(appPriv->filereaderhandle,eIndexParamFilename,temp);
      err = OMX_SetParameter(appPriv->filereaderhandle,eIndexParamFilename,input_file);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n error in input audio format - exiting\n");
        exit(1);
      }
    }
  }

  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    if(flagUsingFFMpeg) {
      err = OMX_SetupTunnel(appPriv->filereaderhandle, 0, appPriv->audiodechandle, 0);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Set up Tunnel Failed\n");
        exit(1);
      }
    }
    err = OMX_SetupTunnel(appPriv->audiodechandle, 1, appPriv->volumehandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->volumehandle, 1, appPriv->audiosinkhandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel Failed\n");
      exit(1);
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Set up Tunnel Completed\n");
  }
	
  if(flagUsingFFMpeg) {
    /** now set the filereader component to idle and executing state */
    OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Dec\n");
    /*Send State Change Idle command to Audio Decoder*/
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (flagPlaybackOn) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Sink\n");
      err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
      err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
    if( !flagUsingFFMpeg) {
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec1, 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec2, 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
    }
  }

  if (!flagSetupTunnel && flagUsingFFMpeg) {
    outBufferFileRead1 = outBufferFileRead2 = NULL;
    /** allocation of file reader component's output buffers
    * these two will be used as input buffers of the audio decoder component 
    */
    err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead1, 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 1 in file read\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead2, 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 2 in file read\n");
      exit(1);
    }
  }
	if(flagUsingFFMpeg) {
    /*Wait for File reader state change to */
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader idle state \n");
  }

  if (flagSetupTunnel) {
    tsem_down(appPriv->decoderEventSem);
    if (flagPlaybackOn) {
      tsem_down(appPriv->volumeEventSem);
      tsem_down(appPriv->sinkEventSem);
    }
  }

  if(flagUsingFFMpeg) {
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"file reader state executing failed\n");
			exit(1);
    }
    /*Wait for File reader state change to executing*/
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader executing state \n");

    /*Wait for File Reader Ports Setting Changed Event. Since File Reader Always detect the stream
    Always ports setting change event will be received*/
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader Port Settings Changed event \n");
  }

  /*If Ports Are Tunneled the Disable Output port of File Reader and Input port of Audio Decoder*/
  if (flagSetupTunnel && flagUsingFFMpeg) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Disable Command\n");
    /*Port Disable for filereader is sent from Port Settings Changed event of FileReader*/
    /*
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandPortDisable, 0, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"file reader port disable failed\n");
			exit(1);
    }*/
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortDisable, 0, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"file reader port disable failed\n");
			exit(1);
    }
    /*Wait for File Reader Ports Disable Event*/
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader Port Disable\n");
    /*Wait for Audio Decoder Ports Disable Event*/
    tsem_down(appPriv->decoderEventSem);
  }

  if(flagUsingFFMpeg) {
    err = OMX_GetExtensionIndex(appPriv->audiodechandle,"OMX.ST.index.param.extradata",&eIndexExtraData);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
			exit(1);
    } else {
      pExtraData = malloc(128);
      err = OMX_GetParameter(appPriv->filereaderhandle, eIndexExtraData, pExtraData);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n file reader Get Param Failed error =%08x index=%08x\n",err,eIndexExtraData);
				exit(1);
      }
      err = OMX_SetParameter(appPriv->audiodechandle, eIndexExtraData, pExtraData);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n audio decoder Set Param Failed error=%08x\n",err);
				exit(1);
      }
      free(pExtraData);
    }
    // getting port parameters from file reader component //
    paramPort.nPortIndex = 0;
    setHeader(&paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    err = OMX_GetParameter(appPriv->filereaderhandle, OMX_IndexParamPortDefinition, &paramPort);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in file reader port settings get\n");	
			exit(1);
    }
    decparamPort.nPortIndex = 0;
    setHeader(&decparamPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &decparamPort);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in audiodechandle settings get\n");	
			exit(1);
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ,"mime type=%s --- \n",paramPort.format.audio.cMIMEType);	
    decparamPort.format.audio.cMIMEType=paramPort.format.audio.cMIMEType;
    DEBUG(DEB_LEV_SIMPLE_SEQ,"mime type=%s --- \n",decparamPort.format.audio.cMIMEType);	
    decparamPort.format.audio.eEncoding=paramPort.format.audio.eEncoding;
    err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamPortDefinition, &decparamPort);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x setting audiodec port param --- \n",err);
			exit(1);
    } 
  }

  if (flagSetupTunnel && flagUsingFFMpeg) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Enable Command\n");

    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandPortEnable, 0, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"file reader port enable failed\n");
			exit(1);
    }
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortEnable, 0, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"file reader port enable failed\n");
			exit(1);
    }
    /*Wait for File Reader Ports Disable Event*/
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader Port Enable \n");
    /*Wait for Audio Decoder Ports Disable Event*/
    tsem_down(appPriv->decoderEventSem);

    DEBUG(DEB_LEV_SIMPLE_SEQ,"Ports are Enabled Now\n");
  }

  /*Send State Change Idle command to Audio Decoder*/
  if (!flagSetupTunnel) {
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

    /** the output buffers of file reader component will be used 
    *  in the audio decoder component as input buffers 
    */ 
    if(flagUsingFFMpeg) {
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec1, 0, NULL, buffer_out_size, outBufferFileRead1->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec2, 0, NULL, buffer_out_size, outBufferFileRead2->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
        exit(1);
      }
    }
    else {
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec1, 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec2, 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
    }

    err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBufferAudioDec1, 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio dec\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBufferAudioDec2, 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio dec\n");
      exit(1);
    }
    /*Wait for decoder state change to idle*/
    tsem_down(appPriv->decoderEventSem);
  }


  if ((flagPlaybackOn) && (!flagSetupTunnel)) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

    err = OMX_UseBuffer(appPriv->volumehandle, &inBufferVolume1, 0, NULL, buffer_out_size, outBufferAudioDec1->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->volumehandle, &inBufferVolume2, 0, NULL, buffer_out_size, outBufferAudioDec2->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }

    err = OMX_AllocateBuffer(appPriv->volumehandle, &outBufferVolume1, 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in volume 1\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->volumehandle, &outBufferVolume2, 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in volume 2\n");
      exit(1);
    }

    if (flagPlaybackOn) {
      tsem_down(appPriv->volumeEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"volume state idle\n");
    }

    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

    err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink1, 0, NULL, buffer_out_size, outBufferVolume1->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink2, 0, NULL, buffer_out_size, outBufferVolume2->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }
    if (flagPlaybackOn) {
      tsem_down(appPriv->sinkEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"audio sink state idle\n");
    }
  }

  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"audio decoder state executing failed\n");
    exit(1);
  }
  /*Wait for decoder state change to executing*/
  tsem_down(appPriv->decoderEventSem);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"volume state executing failed\n");
			exit(1);
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ,"waiting for  volume state executing\n");
    tsem_down(appPriv->volumeEventSem);

    DEBUG(DEB_LEV_SIMPLE_SEQ,"sending audio sink state executing\n");
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"audio sink state executing failed\n");
			exit(1);
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ,"waiting for  audio sink state executing\n");
    tsem_down(appPriv->sinkEventSem);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "audio sink state executing successful\n");
  }
	
  DEBUG(DEB_LEV_ERR,"All Component state changed to Executing\n");
  if (!flagSetupTunnel && flagUsingFFMpeg) {
    err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead1);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer File Reader\n", __func__,err);
			exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill reader second buffer %x\n", (int)outBufferFileRead1);
    err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead2);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer File Reader\n", __func__,err);
			exit(1);
    }
  }

  if (!flagSetupTunnel) {
    err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec1);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
			exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill decoder second buffer %x\n", (int)outBufferAudioDec2);
    err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec2);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
			exit(1);
    }
  }

  if (!flagSetupTunnel && flagPlaybackOn) {
    err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume1);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
			exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill decoder second buffer %x\n", (int)outBufferVolume2);
    err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume2);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
			exit(1);
    }
  }

  if(!flagUsingFFMpeg) {
    data_read = read(fd, inBufferAudioDec1->pBuffer, buffer_in_size);
    inBufferAudioDec1->nFilledLen = data_read;

    data_read = read(fd, inBufferAudioDec2->pBuffer, buffer_in_size);
    inBufferAudioDec2->nFilledLen = data_read;

    DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBufferAudioDec1);
    err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec1);
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBufferAudioDec2);
    err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec2);
  }

  DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS = %d\n",appPriv->eofSem->semval);

  tsem_down(appPriv->eofSem);

  DEBUG(DEFAULT_MESSAGES,"Received EOS \n");
  /*Send Idle Command to all components*/
  DEBUG(DEFAULT_MESSAGES, "The execution of the decoding process is terminated\n");
  if(flagUsingFFMpeg) {
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }	
  if(flagUsingFFMpeg) {
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader idle state \n");
  }
  tsem_down(appPriv->decoderEventSem);
  if (flagPlaybackOn) {
    tsem_down(appPriv->volumeEventSem);
    tsem_down(appPriv->sinkEventSem);
  }

  DEBUG(DEFAULT_MESSAGES, "All componnet Transitioned to Idle\n");
  /*Send Loaded Command to all components*/
  if(flagUsingFFMpeg) {
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }

  DEBUG(DEFAULT_MESSAGES, "Audio dec to loaded\n");

  /*Free buffers is components are not tunnelled*/
  if (!flagSetupTunnel || !flagUsingFFMpeg ) {
    err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec1);
    err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec2);
  }
  DEBUG(DEB_LEV_PARAMS, "Free Audio dec output ports\n");
  if (!flagSetupTunnel) {
    err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBufferAudioDec1);
    err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBufferAudioDec2);
  }

  if (!flagSetupTunnel && flagUsingFFMpeg) {
    err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead1);
    err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead2);
  }

  if ((flagPlaybackOn) && (!flagSetupTunnel)) {
    err = OMX_FreeBuffer(appPriv->volumehandle, 0, inBufferVolume1);
    err = OMX_FreeBuffer(appPriv->volumehandle, 0, inBufferVolume2);
    err = OMX_FreeBuffer(appPriv->volumehandle, 1, outBufferVolume1);
    err = OMX_FreeBuffer(appPriv->volumehandle, 1, outBufferVolume2);

    err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink1);
    err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink2);
  }

  if(flagUsingFFMpeg) {
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader loaded state \n");
  }
  tsem_down(appPriv->decoderEventSem);
  if (flagPlaybackOn) {
    tsem_down(appPriv->volumeEventSem);
    tsem_down(appPriv->sinkEventSem);
  }


  DEBUG(DEFAULT_MESSAGES, "All components released\n");

  /** freeing all handles and deinit omx */
  OMX_FreeHandle(appPriv->audiodechandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "audiodec dec freed\n");
  
  if(flagUsingFFMpeg) {
    OMX_FreeHandle(appPriv->filereaderhandle);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "filereader freed\n");
  }

  if (flagPlaybackOn) {
    OMX_FreeHandle(appPriv->volumehandle);
    OMX_FreeHandle(appPriv->audiosinkhandle);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "audiosink freed\n");
  }

  OMX_Deinit();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components freed. Closing...\n");

  free(appPriv->filereaderEventSem);
  appPriv->filereaderEventSem = NULL;

  free(appPriv->decoderEventSem);
  appPriv->decoderEventSem = NULL;
  if (flagPlaybackOn) {
    free(appPriv->volumeEventSem);
    appPriv->volumeEventSem = NULL;

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

  if(!flagUsingFFMpeg) {
    if(close(fd) == -1) {
      DEBUG(DEB_LEV_ERR,"Error in closing input file stream\n");
      exit(1);
    }
    else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Succees in closing input file stream\n");
    }
  }

  free(full_component_name);
  free(input_file);
  free(output_file);
  free(temp);

  return 0;
}	

/* Callbacks implementation */

OMX_ERRORTYPE filereaderEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  OMX_ERRORTYPE err;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);

  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_ERR/*SIMPLE_SEQ*/, "File Reader State changed in ");
      switch ((int)Data2) {
      case OMX_StateInvalid:
        DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateInvalid\n");
        break;
      case OMX_StateLoaded:
        DEBUG(DEB_LEV_ERR, "OMX_StateLoaded\n");
        break;
      case OMX_StateIdle:
        DEBUG(DEB_LEV_ERR/*SIMPLE_SEQ*/, "OMX_StateIdle\n");
        break;
      case OMX_StateExecuting:
        DEBUG(DEB_LEV_ERR/*SIMPLE_SEQ*/, "OMX_StateExecuting\n");
        break;
      case OMX_StatePause:
        DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StatePause\n");
        break;
      case OMX_StateWaitForResources:
        DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateWaitForResources\n");
        break;
      }
      tsem_up(appPriv->filereaderEventSem);
    } else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->filereaderEventSem);
    }
  }else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Setting Changed event\n");
    if (flagSetupTunnel && flagUsingFFMpeg) {
      /*Sent Port Disable command. So that Audio Decoder Port which willl be disabled don't
        receive further buffers*/
      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandPortDisable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"file reader port disable failed\n");
			  exit(1);
      }
    }
    /*Signal Port Setting Changed*/
    tsem_up(appPriv->filereaderEventSem);
  }else if(eEvent == OMX_EventPortFormatDetected) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port Format Detected %x\n", __func__,(int)Data1);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
	return OMX_ErrorNone;
}

OMX_ERRORTYPE filereaderFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  /* Output data to audio decoder */

  if(pBuffer != NULL){
    if(!bEOS) {
      if(inBufferAudioDec1->pBuffer == pBuffer->pBuffer) {
        inBufferAudioDec1->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec1);
      } else {
        inBufferAudioDec2->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec2);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
      if(pBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
        bEOS=OMX_TRUE;
      }
    }
    else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  }	else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiodecEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  OMX_ERRORTYPE err;
  OMX_PARAM_PORTDEFINITIONTYPE param;
  OMX_AUDIO_PARAM_PCMMODETYPE pcmParam;
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ/*SIMPLE_SEQ*/, "Audio Decoder State changed in ");
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
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->decoderEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_ERR, "In %s Received Port Settings Changed Event\n", __func__);
    if (Data2 == 1) {
      param.nPortIndex = 1;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamPortDefinition, &param);
      /*Get Port parameters*/
      pcmParam.nPortIndex=1;
      setHeader(&pcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamAudioPcm, &pcmParam);
      pcmParam.nPortIndex=0;
      err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &pcmParam);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
      }
    } else if (Data2 == 0) {
      /*Get Port parameters*/
      param.nPortIndex = 0;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamPortDefinition, &param);
    }
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiodecEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  int data_read;
  static int iBufferDropped=0;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  if(flagUsingFFMpeg) {
    if(pBuffer != NULL){
      if(!bEOS) {
        if(outBufferFileRead1->pBuffer == pBuffer->pBuffer) {
          err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead1);
        } else {
          err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead2);
        }
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
        }
      }
      else {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
        iBufferDropped++;
        if(iBufferDropped==2) {
          tsem_up(appPriv->eofSem);
        }
      }
    } else {
      if(!bEOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ,"It is here EOS = %d\n",appPriv->eofSem->semval);
        tsem_up(appPriv->eofSem);
      }
      DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
    }
  } else {

 	  data_read = read(fd, pBuffer->pBuffer, buffer_in_size);
	  if (data_read <= 0) {
		  DEBUG(DEB_LEV_ERR, "In the %s no more input data available\n", __func__);
      iBufferDropped++;
      if(iBufferDropped==2) {
		    tsem_up(appPriv->eofSem);
        return OMX_ErrorNone;
      }
      pBuffer->nFilledLen=0;
      pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
      err = OMX_EmptyThisBuffer(hComponent, pBuffer);
		  return OMX_ErrorNone;
	  }
	  pBuffer->nFilledLen = data_read;
	  if(!bEOS) {
	    DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
	    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
    }else {
      DEBUG(DEB_LEV_ERR, "In %s Dropping Empty This buffer to Audio Dec\n", __func__);
    }
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiodecFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  int i;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
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
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else if ((flagOutputReceived) && (!flagPlaybackOn)) {
      if(pBuffer->nFilledLen > 0) {
        fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, outfile);
      }
      pBuffer->nFilledLen = 0;
      err = OMX_FillThisBuffer(hComponent, pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else if ((!flagSetupTunnel) && (flagPlaybackOn))  { //playback on, redirect to alsa sink, if it is not tunneled
      if(inBufferVolume1->pBuffer == pBuffer->pBuffer) {
        inBufferVolume1->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->volumehandle, inBufferVolume1);
      } else {
        inBufferVolume2->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->volumehandle, inBufferVolume2);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
      }
    }
  }	else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE volumeEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ/*SIMPLE_SEQ*/, "Audio Decoder State changed in ");
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
      tsem_up(appPriv->volumeEventSem);
    }
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE volumeEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  static int iBufferDropped=0;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  if(pBuffer != NULL){
    if(!bEOS) {
      if(outBufferAudioDec1->pBuffer == pBuffer->pBuffer) {
        err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec1);
      } else {
        err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec2);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    }
    else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
      iBufferDropped++;
      if(iBufferDropped==2) {
        tsem_up(appPriv->eofSem);
      }
    }
  } else {
    if(!bEOS) {
      DEBUG(DEFAULT_MESSAGES,"It is here EOS = %d\n",appPriv->eofSem->semval);
      tsem_up(appPriv->eofSem);
    }
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE volumeFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  int i;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
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
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else if ((flagOutputReceived) && (!flagPlaybackOn)) {
      if(pBuffer->nFilledLen > 0) {
        fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, outfile);
      }
      pBuffer->nFilledLen = 0;
      err = OMX_FillThisBuffer(hComponent, pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else if ((!flagSetupTunnel) && (flagPlaybackOn))  { //playback on, redirect to alsa sink, if it is not tunneled
      if(inBufferSink1->pBuffer == pBuffer->pBuffer) {
        inBufferSink1->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiosinkhandle, inBufferSink1);
      } else {
        inBufferSink2->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiosinkhandle, inBufferSink2);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
      }
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
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Audio Sink State changed in ");
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
  if(outBufferVolume1->pBuffer == pBuffer->pBuffer) {
    err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume1);
  } else {
    err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume2);
  }
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
  }

  return OMX_ErrorNone;
}

