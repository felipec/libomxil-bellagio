/**
  @file test/components/audio/audio_use_case.c

  Test application that uses three OpenMAX components, a file reader,an audio decoder 
  and an alsa sink. The application receives an compressed audio stream on input port
  from a file, decodes it and sends it to the alsa sink, or to a file or standard output.
  The audio formats supported are:
  mp3 (ffmpeg)

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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

  $Date: 2008-09-11 13:48:00 +0200 (Thu, 11 Sep 2008) $
  Revision $Rev: 1485 $
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
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>

#include "omxaudiodectest.h"

#define MP3_TYPE_SEL    1
#define VORBIS_TYPE_SEL 2
#define AAC_TYPE_SEL    3 
#define COMPONENT_NAME_BASE "OMX.st.audio_decoder"
#define VORBIS_ROLE "audio_decoder.ogg"
#define MP3_ROLE "audio_decoder.mp3"
#define AAC_ROLE "audio_decoder.aac"
#define COMPONENT_NAME_BASE_LEN 20
#define SINK_NAME "OMX.st.alsa.alsasink"
#define FILE_READER "OMX.st.audio_filereader"
#define AUDIO_EFFECT "OMX.st.volume.component"
#define extradata_size 1024

appPrivateType* appPriv;

OMX_BUFFERHEADERTYPE *outBufferFileRead[2];
OMX_BUFFERHEADERTYPE *inBufferAudioDec[2], *outBufferAudioDec[2];
OMX_BUFFERHEADERTYPE *inBufferVolume[2], *outBufferVolume[2];
OMX_BUFFERHEADERTYPE *inBufferSink[2];

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

FILE *fd;
appPrivateType* appPriv;

char *input_file=NULL, *output_file=NULL;

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
  }  else {
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

    if(string_of_comp_per_role)  {
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

  if(string_of_comp_per_role)  {
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

void display_help() {
  printf("This Application will playback all mp3,ogg,aac files in the directory mentioned or\n");
  printf("default directory $HOME/stream/audio\n");
  printf("Usage: omxaudiodecbatchtest [optional]dirname\n");
  printf("\n");
  printf("       dirname: dirname from where audio files to be played\n");
  printf("       -s: Disable Seek\n");
  printf("       -t: Disable Tunnelling\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  exit(1);
}

int flagIsOutputExpected;
int flagSetupTunnel;
int flagPlaybackOn;
int flagOutputReceived;
int flagInputReceived;
int flagIsMadRequested;
int flagIsMadUsingFileReader;
int flagDirect;
int flagSingleOGGSelected;
int flagUsingFFMpeg;
int flagIsGain;

static int alsaSinkBufferDropped=0;
static int volCompBufferDropped=0;
static int volCompInBufferDropped=0;
static int audioDecBufferDropped=0;
    
int main(int argc, char** argv) {
  int argn_dec;
  OMX_ERRORTYPE err;
  OMX_INDEXTYPE eIndexParamFilename;
  OMX_STRING full_component_name;
  int data_read;
  int gain=-1;
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume;
  OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
  OMX_PARAM_COMPONENTROLETYPE sComponentRole;
  
  char *stream_dir=NULL;
  DIR *dirp;
  struct dirent *dp;
  int seek=1;

  flagIsOutputExpected = 0;
  flagSetupTunnel = 1;
  flagPlaybackOn = 1;
  flagOutputReceived = 0;
  flagInputReceived = 0;
  flagIsMadRequested = 0;
  flagIsMadUsingFileReader = 0;
  flagDirect = 0;
  flagSingleOGGSelected = 0;
  flagUsingFFMpeg = 1;
  flagIsGain = 0;

  if(argc <= 3) {
    argn_dec = 1;
    while (argn_dec<argc) {
      if (*(argv[argn_dec]) =='-') {
        switch (*(argv[argn_dec]+1)) {
        case 's':
          seek = 0;
          break;
        case 't':
          flagSetupTunnel = 0;
          break;
        default:
          display_help();
        }
      } else {
        stream_dir = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
        strcpy(stream_dir,argv[argn_dec]);
      }
      argn_dec++;
    }
  } else if(argc > 3) {
    display_help();
  }

  if(stream_dir==NULL) {

    stream_dir = malloc(strlen(getenv("HOME")) * sizeof(char) + 20);
    memset(stream_dir, 0, sizeof(stream_dir));
    strcat(stream_dir, getenv("HOME"));
    strcat(stream_dir, "/stream/audio/");
  }

  DEBUG(DEFAULT_MESSAGES, "Directory Name=%s\n",stream_dir);

  /* Populate the registry file */
  dirp = opendir(stream_dir);
  if(dirp == NULL){
    int err = errno;
    DEBUG(DEB_LEV_ERR, "Cannot open directory %s\n", stream_dir);
    return err;
  }

  if (flagOutputReceived) {
    outfile = fopen(output_file,"wb");
    if(outfile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error at opening the output file");
      exit(1);
    } 
  }

  if(!flagUsingFFMpeg && !flagIsMadUsingFileReader) {
    fd = fopen(input_file, "rb");
    if(fd == NULL) {
      DEBUG(DEB_LEV_ERR, "Error in opening input file %s\n", input_file);
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
  test_OMX_ComponentEnumByRole(VORBIS_ROLE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
  test_OpenClose(COMPONENT_NAME_BASE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Using File Reader\n");
    /** file reader component name -- gethandle*/
    err = OMX_GetHandle(&appPriv->filereaderhandle, FILE_READER, NULL , &filereadercallbacks);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "FileReader Component Not Found\n");
      exit(1);
    }  
    err = OMX_GetExtensionIndex(appPriv->filereaderhandle,"OMX.ST.index.param.filereader.inputfilename",&eIndexParamFilename);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
      exit(1);
    } 
  }

  full_component_name = (OMX_STRING) malloc(sizeof(char*) * OMX_MAX_STRINGNAME_SIZE);
  
  strcpy(full_component_name, "OMX.st.audio_decoder.mp3");

  /** getting the handle of audio decoder */
  err = OMX_GetHandle(&appPriv->audiodechandle, COMPONENT_NAME_BASE, NULL , &audiodeccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Audio Decoder Component Not Found\n");
    exit(1);
  } 
  DEBUG(DEFAULT_MESSAGES, "Component %s opened\n", COMPONENT_NAME_BASE);

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
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_GetConfig 0 \n",err);
      }
      sVolume.sVolume.nValue = gain;
      DEBUG(DEFAULT_MESSAGES, "Setting Gain %d \n", gain);
      err = OMX_SetConfig(appPriv->volumehandle, OMX_IndexConfigAudioVolume, &sVolume);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig 0 \n",err);
      }
    }
  }

  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
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

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    /** now set the filereader component to idle and executing state */
    OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  if (!flagSetupTunnel && ( flagUsingFFMpeg || flagIsMadUsingFileReader)) {
    outBufferFileRead[0] = outBufferFileRead[1] = NULL;
    /** allocation of file reader component's output buffers
    * these two will be used as input buffers of the audio decoder component 
    */
    err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead[0], 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 1 in file read\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead[1], 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 2 in file read\n");
      exit(1);
    }
  }

  /*Send State Change Idle command to Audio Decoder*/
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Dec\n");
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if (flagPlaybackOn) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Sink\n");
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  if (flagSetupTunnel) {
    if( !flagUsingFFMpeg && !flagIsMadUsingFileReader) {
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
    }
  } else if (!flagSetupTunnel) { /*Send State Change Idle command to Audio Decoder*/
    /** the output buffers of file reader component will be used 
      *  in the audio decoder component as input buffers 
      */ 
    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_out_size, outBufferFileRead[0]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_out_size, outBufferFileRead[1]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
        exit(1);
      }
    }
    else {
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_in_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
        exit(1);
      }
    }

    err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBufferAudioDec[0], 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio dec\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audiodechandle, &outBufferAudioDec[1], 1, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio dec\n");
      exit(1);
    }
    if (flagPlaybackOn) {
      err = OMX_UseBuffer(appPriv->volumehandle, &inBufferVolume[0], 0, NULL, buffer_out_size, outBufferAudioDec[0]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->volumehandle, &inBufferVolume[1], 0, NULL, buffer_out_size, outBufferAudioDec[1]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
        exit(1);
      }

      err = OMX_AllocateBuffer(appPriv->volumehandle, &outBufferVolume[0], 1, NULL, buffer_out_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in volume 1\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->volumehandle, &outBufferVolume[1], 1, NULL, buffer_out_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in volume 2\n");
        exit(1);
      }

      err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink[0], 0, NULL, buffer_out_size, outBufferVolume[0]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink[1], 0, NULL, buffer_out_size, outBufferVolume[1]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
        exit(1);
      }
    }
  }

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
      /*Wait for File reader state change to */
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader idle state \n");
  }

  tsem_down(appPriv->decoderEventSem);
  if (flagPlaybackOn) {
    tsem_down(appPriv->volumeEventSem);
    DEBUG(DEFAULT_MESSAGES,"volume state idle\n");
    tsem_down(appPriv->sinkEventSem);
    DEBUG(DEFAULT_MESSAGES,"audio sink state idle\n");
  }
  
  setHeader(&sComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
  strcpy((char*)&sComponentRole.cRole[0], MP3_ROLE);

  while((dp = readdir(dirp)) != NULL) {
    int len = strlen(dp->d_name);

    if(len >= 3){
      
      if(strncmp(dp->d_name+len-4, ".mp3", 4) == 0){
        
        if(input_file!=NULL) {
          free(input_file);
          input_file=NULL;
        }

        input_file = malloc(strlen(stream_dir) * sizeof(char) + sizeof(dp->d_name) +1);
        strcpy(input_file,stream_dir);
        strcat(input_file, dp->d_name);

        DEBUG(DEFAULT_MESSAGES, "Input Mp3 File Name=%s\n",input_file);

        flagIsMadRequested = 0;
        flagIsMadUsingFileReader = 0;
        flagUsingFFMpeg = 1;

        selectedType = MP3_TYPE_SEL;
        strcpy((char*)&sComponentRole.cRole[0], MP3_ROLE);

      } else if(strncmp(dp->d_name+len-4, ".ogg", 4) == 0){
        
        if(input_file!=NULL) {
          free(input_file);
          input_file=NULL;
        }

        input_file = malloc(strlen(stream_dir) * sizeof(char) + sizeof(dp->d_name) +1);
        strcpy(input_file,stream_dir);
        strcat(input_file, dp->d_name);

        DEBUG(DEFAULT_MESSAGES, "Input Vorbis File Name=%s\n",input_file);

        flagSingleOGGSelected = 0;
        flagUsingFFMpeg = 1;

        strcpy((char*)&sComponentRole.cRole[0], VORBIS_ROLE);
                
      } else if(strncmp(dp->d_name+len-4, ".aac", 4) == 0){
        
        if(input_file!=NULL) {
          free(input_file);
          input_file=NULL;
        }

        input_file = malloc(strlen(stream_dir) * sizeof(char) + sizeof(dp->d_name) +1);
        strcpy(input_file,stream_dir);
        strcat(input_file, dp->d_name);

        DEBUG(DEFAULT_MESSAGES, "Input AAC File Name=%s\n",input_file);

        flagUsingFFMpeg = 1;

        strcpy((char*)&sComponentRole.cRole[0], AAC_ROLE);
                
      } else {
        continue;
      }
    } else {
      continue;
    }

    
    /*Reset Global Variables*/
    alsaSinkBufferDropped=0;
    volCompBufferDropped=0;
    volCompInBufferDropped=0;
    audioDecBufferDropped=0;
    tsem_reset(appPriv->eofSem);
    bEOS=OMX_FALSE;

    if (flagUsingFFMpeg || flagIsMadUsingFileReader) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Disable Command State Idle\n");
      /*Port Disable for filereader is sent from Port Settings Changed event of FileReader*/
      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandPortDisable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"file reader port disable failed\n");
        exit(1);
      }
      err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortDisable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"audio decoder port disable failed\n");
        exit(1);
      }
      
      if (!flagSetupTunnel) {
        err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead[0]);
        err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead[1]);
        err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[0]);
        err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[1]);
      }

      DEBUG(DEB_LEV_SIMPLE_SEQ,"Waiting File reader Port Disable event \n");
      /*Wait for File Reader Ports Disable Event*/
      tsem_down(appPriv->filereaderEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Disable State Idle\n");
      /*Wait for Audio Decoder Ports Disable Event*/
      tsem_down(appPriv->decoderEventSem);
    }


    DEBUG(DEB_LEV_SIMPLE_SEQ,"Setting Role\n");

    err = OMX_SetParameter(appPriv->audiodechandle,OMX_IndexParamStandardComponentRole,&sComponentRole);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in input audio format - exiting\n");
      exit(1);
    }

    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
      /** setting the input audio format in file reader */
      DEBUG(DEB_LEV_SIMPLE_SEQ,"FileName Param index : %x \n",eIndexParamFilename);
      err = OMX_SetParameter(appPriv->filereaderhandle,eIndexParamFilename,input_file);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n error in input audio format - exiting\n");
        exit(1);
      }
    }
    
    if (flagUsingFFMpeg || flagIsMadUsingFileReader) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Enable Command State Idle\n");

      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandPortEnable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"file reader port enable failed\n");
        exit(1);
      }
      err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortEnable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"audio decoder port enable failed\n");
        exit(1);
      }
      
      if (!flagSetupTunnel) {
        if (flagUsingFFMpeg || flagIsMadUsingFileReader) {
          err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead[0], 0, NULL, buffer_out_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 1 in file read\n");
            exit(1);
          }
          err = OMX_AllocateBuffer(appPriv->filereaderhandle, &outBufferFileRead[1], 0, NULL, buffer_out_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer 2 in file read\n");
            exit(1);
          }
        }
        if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
          err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_out_size, outBufferFileRead[0]->pBuffer);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
            exit(1);
          }
          err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_out_size, outBufferFileRead[1]->pBuffer);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to use the file read comp allocate buffer\n");
            exit(1);
          }
        }
        else {
          err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_in_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
            exit(1);
          }
          err = OMX_AllocateBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_in_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer\n");
            exit(1);
          }
        }
      }

      DEBUG(DEB_LEV_SIMPLE_SEQ,"Waiting File reader Port Disable event \n");
      /*Wait for File Reader Ports Disable Event*/
      tsem_down(appPriv->filereaderEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Disable State Idle\n");
      /*Wait for Audio Decoder Ports Disable Event*/
      tsem_down(appPriv->decoderEventSem);
    }

    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
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

    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"audio decoder state executing failed\n");
      exit(1);
    }
    
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Waiting for audio dec state exec\n");
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
    
    DEBUG(DEB_LEV_SIMPLE_SEQ,"All Component state changed to Executing\n");
    if (!flagSetupTunnel && (flagUsingFFMpeg || flagIsMadUsingFileReader)) {
      err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead[0]);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer File Reader\n", __func__,err);
        exit(1);
      }
      DEBUG(DEB_LEV_PARAMS, "Fill reader second buffer %x\n", (int)outBufferFileRead[0]);
      err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead[1]);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer File Reader\n", __func__,err);
        exit(1);
      }
    }

    if (!flagSetupTunnel) {
      err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec[0]);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
        exit(1);
      }
      DEBUG(DEB_LEV_PARAMS, "Fill decoder second buffer %x\n", (int)outBufferAudioDec[1]);
      err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec[1]);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
        exit(1);
      }
      if (flagPlaybackOn) {
        err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume[0]);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
          exit(1);
        }
        DEBUG(DEB_LEV_PARAMS, "Fill decoder second buffer %x\n", (int)outBufferVolume[1]);
        err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume[1]);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Dec\n", __func__,err);
          exit(1);
        }
      }
    }

    if(!flagUsingFFMpeg && !flagIsMadUsingFileReader) {
      data_read = fread(inBufferAudioDec[0]->pBuffer, sizeof(char), buffer_in_size, fd);
      inBufferAudioDec[0]->nFilledLen = data_read;
      inBufferAudioDec[0]->nOffset = 0;

      data_read = fread(inBufferAudioDec[1]->pBuffer, sizeof(char), buffer_in_size, fd);
      inBufferAudioDec[1]->nFilledLen = data_read;
      inBufferAudioDec[1]->nOffset = 0;

      DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBufferAudioDec[0]);
      err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[0]);
      DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBufferAudioDec[1]);
      err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[1]);
    }

    DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS = %d\n",appPriv->eofSem->semval);

    if (seek==1) {

      DEBUG(DEFAULT_MESSAGES,"Sleeping for 5 Secs \n");
      /* Play for 5 Secs */
      sleep(5);
      DEBUG(DEFAULT_MESSAGES,"Sleep for 5 Secs is over\n");

      /*Then Pause the filereader component*/
      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StatePause, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"file reader state executing failed\n");
        exit(1);
      }
      /*Wait for File reader state change to Pause*/
      tsem_down(appPriv->filereaderEventSem);

      setHeader(&sTimeStamp, sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE));
      sTimeStamp.nPortIndex=0;
      /*Seek to 30 secs and play for 10 secs*/
      sTimeStamp.nTimestamp = 2351*38*30; // 23.51ms*38fps*30secs
      //sTimeStamp.nTimestamp = 2351*38*60*3; // 23.51ms*38fps*60secs*4mins
      DEBUG(DEFAULT_MESSAGES, "nTimestamp %llx \n", sTimeStamp.nTimestamp);
      err = OMX_SetConfig(appPriv->filereaderhandle, OMX_IndexConfigTimePosition, &sTimeStamp);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
      }

      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"file reader state executing failed\n");
        exit(1);
      }
      /*Wait for File reader state change to Pause*/
      tsem_down(appPriv->filereaderEventSem);

      DEBUG(DEFAULT_MESSAGES,"Sleeping for 10 Secs \n");
      /*Play for 10 secs*/
      sleep(10);
      DEBUG(DEFAULT_MESSAGES,"Sleep for 10 Secs is over\n");

      if(!bEOS) {
        /*Then Pause the filereader component*/
        err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StatePause, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"file reader state executing failed\n");
          exit(1);
        }
        /*Wait for File reader state change to Pause*/
        tsem_down(appPriv->filereaderEventSem);

        /*Seek to 5 mins or EOF*/
        sTimeStamp.nTimestamp = 2351*38*60*5; // 23.51ms*38fps*30secs
        //sTimeStamp.nTimestamp = 2351*38*60*3; // 23.51ms*38fps*60secs*4mins
        DEBUG(DEFAULT_MESSAGES, "nTimestamp %llx \n", sTimeStamp.nTimestamp);
        err = OMX_SetConfig(appPriv->filereaderhandle, OMX_IndexConfigTimePosition, &sTimeStamp);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }

        err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"file reader state executing failed\n");
          exit(1);
        }
        /*Wait for File reader state change to Pause*/
        tsem_down(appPriv->filereaderEventSem);
      }
    }

    tsem_down(appPriv->eofSem);

    DEBUG(DEFAULT_MESSAGES,"Received EOS \n");
    /*Send Idle Command to all components*/
    DEBUG(DEFAULT_MESSAGES, "The execution of the decoding process is terminated\n");
    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (flagPlaybackOn) {
      err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
      err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }  
    if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
      tsem_down(appPriv->filereaderEventSem);
      DEBUG(DEFAULT_MESSAGES,"File reader idle state \n");
    }
    tsem_down(appPriv->decoderEventSem);
    if (flagPlaybackOn) {
      tsem_down(appPriv->volumeEventSem);
      tsem_down(appPriv->sinkEventSem);
    }

    DEBUG(DEFAULT_MESSAGES, "All component Transitioned to Idle\n");

  } /*Loop While Play List*/

  /*Send Loaded Command to all components*/
  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }

  DEBUG(DEFAULT_MESSAGES, "Audio dec to loaded\n");

  /*Free buffers is components are not tunnelled*/
  if (!flagSetupTunnel || (!flagUsingFFMpeg && !flagIsMadUsingFileReader )) {
    err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[0]);
    err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[1]);
  }
  DEBUG(DEB_LEV_PARAMS, "Free Audio dec output ports\n");
  if (!flagSetupTunnel) {
    err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBufferAudioDec[0]);
    err = OMX_FreeBuffer(appPriv->audiodechandle, 1, outBufferAudioDec[1]);
  }

  if (!flagSetupTunnel && (flagUsingFFMpeg || flagIsMadUsingFileReader)) {
    err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead[0]);
    err = OMX_FreeBuffer(appPriv->filereaderhandle, 0, outBufferFileRead[1]);
  }

  if ((flagPlaybackOn) && (!flagSetupTunnel)) {
    err = OMX_FreeBuffer(appPriv->volumehandle, 0, inBufferVolume[0]);
    err = OMX_FreeBuffer(appPriv->volumehandle, 0, inBufferVolume[1]);
    err = OMX_FreeBuffer(appPriv->volumehandle, 1, outBufferVolume[0]);
    err = OMX_FreeBuffer(appPriv->volumehandle, 1, outBufferVolume[1]);

    err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[0]);
    err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[1]);
  }

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    tsem_down(appPriv->filereaderEventSem);
    DEBUG(DEFAULT_MESSAGES,"File reader loaded state \n");
  }
  tsem_down(appPriv->decoderEventSem);
  if (flagPlaybackOn) {
    tsem_down(appPriv->volumeEventSem);
    tsem_down(appPriv->sinkEventSem);
  }

  if(input_file!=NULL) {
    free(input_file);
    input_file=NULL;
  }

  closedir(dirp);

  DEBUG(DEFAULT_MESSAGES, "All components released\n");

  /** freeing all handles and deinit omx */
  OMX_FreeHandle(appPriv->audiodechandle);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "audiodec dec freed\n");
  
  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    OMX_FreeHandle(appPriv->filereaderhandle);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "filereader freed\n");
  }

  if (flagPlaybackOn) {
    OMX_FreeHandle(appPriv->volumehandle);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "volume component freed\n");
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

  if(!flagUsingFFMpeg && !flagIsMadUsingFileReader) {
    if(fclose(fd) != 0) {
      DEBUG(DEB_LEV_ERR,"Error in closing input file stream\n");
      exit(1);
    }
    else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Succees in closing input file stream\n");
    }
  }

  if(full_component_name) {
    free(full_component_name);
  }
  if(input_file) {
    free(input_file);
  }
  if(output_file) {
    free(output_file);
  }

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
  OMX_PTR pExtraData;
  OMX_INDEXTYPE eIndexExtraData;
  OMX_ERRORTYPE err;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);

  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "File Reader State changed in ");
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
      tsem_up(appPriv->filereaderEventSem);
    } else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->filereaderEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->filereaderEventSem);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Received Event Event=%d Data1=%d,Data2=%d\n",__func__,eEvent,(int)Data1,(int)Data2);
    }
  }else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Setting Changed event\n");
    if(flagUsingFFMpeg) {
      err = OMX_GetExtensionIndex(appPriv->audiodechandle,"OMX.ST.index.config.audioextradata",&eIndexExtraData);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
        exit(1);
      } else {
        pExtraData = malloc(extradata_size);
        err = OMX_GetConfig(appPriv->filereaderhandle, eIndexExtraData, pExtraData);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"\n file reader Get Param Failed error =%08x index=%08x\n",err,eIndexExtraData);
          exit(1);
        }
        DEBUG(DEB_LEV_SIMPLE_SEQ,"Setting ExtraData\n");
        err = OMX_SetConfig(appPriv->audiodechandle, eIndexExtraData, pExtraData);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"\n audio decoder Set Config Failed error=%08x\n",err);
          exit(1);
        }
        free(pExtraData);
      }
    }
    /*Signal Port Setting Changed*/
    tsem_up(appPriv->filereaderEventSem);
  }else if(eEvent == OMX_EventPortFormatDetected) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port Format Detected %x\n", __func__,(int)Data1);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEFAULT_MESSAGES, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      bEOS=OMX_TRUE;
      //tsem_up(appPriv->eofSem);
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
      if(inBufferAudioDec[0]->pBuffer == pBuffer->pBuffer) {
        inBufferAudioDec[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[0]);
      } else {
        inBufferAudioDec[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
      if(pBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Calling Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
        bEOS=OMX_TRUE;
      }
    }
    else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  }  else {
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
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Audio Decoder State changed in ");
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
    else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->decoderEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->decoderEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Settings Changed Event\n", __func__);
    if (Data2 == 1) {
      param.nPortIndex = 1;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamPortDefinition, &param);
      /*Get Port parameters*/
      pcmParam.nPortIndex=1;
      setHeader(&pcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      err = OMX_GetParameter(appPriv->audiodechandle, OMX_IndexParamAudioPcm, &pcmParam);

      /*Disable Audio Sink Port and Set Parameter in Non-tunneled Case*/
      if ((flagPlaybackOn) && (!flagSetupTunnel)) {

        err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandPortDisable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"alsa sink port disable failed\n");
          exit(1);
        }

        err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[0]);
        err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[1]);

        /*Wait for sink Ports Disable Event*/
        tsem_down(appPriv->sinkEventSem);

        pcmParam.nPortIndex=0;
        err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &pcmParam);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter\n",err);
        }

        err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandPortEnable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"alsa sink port enable failed\n");
          exit(1);
        }

        err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink[0], 0, NULL, buffer_out_size, outBufferVolume[0]->pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
          exit(1);
        }
        err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink[1], 0, NULL, buffer_out_size, outBufferVolume[1]->pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
          exit(1);
        }

        /*Wait for sink Ports Enable Event*/
        tsem_down(appPriv->sinkEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ,"audio sink port enabled\n");
      } else if (flagPlaybackOn && flagSetupTunnel) { /*Disable Volume Component and Audio Sink Port,Set Parameter in Tunneled Case*/

        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Volume Component Port Disabling\n", __func__);

        err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandPortDisable, 1, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Volume Component port disable failed\n");
          exit(1);
        }

        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Audio Sink Port Disabling\n", __func__);
        err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandPortDisable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"alas sink port disable failed\n");
          exit(1);
        }

        /*Wait for Ports Disable Events*/
        tsem_down(appPriv->sinkEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Audio Sink Port Disabled\n", __func__);
        tsem_down(appPriv->volumeEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Volume Component Port Disabled\n", __func__);

        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Audio Sink Setting Parameters\n", __func__);

        pcmParam.nPortIndex=0;
        err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &pcmParam);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }

        err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandPortEnable, 1, NULL);
        err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandPortEnable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"audio sink port enable failed\n");
          exit(1);
        }

        /*Wait for Ports Enable Events*/
        tsem_down(appPriv->volumeEventSem);
        tsem_down(appPriv->sinkEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ,"audio sink port enabled\n");
      }
     
    } else if (Data2 == 0) {
      /*Get Port parameters*/
      param.nPortIndex = 0;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audiodechandle,OMX_IndexParamPortDefinition, &param);
    }
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEFAULT_MESSAGES, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      bEOS=OMX_TRUE;
      //tsem_up(appPriv->eofSem);
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
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    if(pBuffer != NULL){
      if(!bEOS) {
        if(outBufferFileRead[0]->pBuffer == pBuffer->pBuffer) {
          outBufferFileRead[0]->nFilledLen=0;
          err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead[0]);
        } else {
          outBufferFileRead[1]->nFilledLen=0;
          err = OMX_FillThisBuffer(appPriv->filereaderhandle, outBufferFileRead[1]);
        }
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
        }
      }
      else {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
        audioDecBufferDropped++;
        if(audioDecBufferDropped==2) {
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

    data_read = fread(pBuffer->pBuffer, sizeof(char), buffer_in_size, fd);
    pBuffer->nFilledLen = data_read;
    pBuffer->nOffset = 0;
    if (data_read <= 0) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
      audioDecBufferDropped++;
      if(audioDecBufferDropped>=2) {
        tsem_up(appPriv->eofSem);
        return OMX_ErrorNone;
      }
      pBuffer->nFilledLen=0;
      pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
      bEOS=OMX_TRUE;
      err = OMX_EmptyThisBuffer(hComponent, pBuffer);
      return OMX_ErrorNone;
    }
    pBuffer->nFilledLen = data_read;
    if(!bEOS) {
      DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
      err = OMX_EmptyThisBuffer(hComponent, pBuffer);
    }else {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Empty This buffer to Audio Dec\n", __func__);
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
      if(inBufferVolume[0]->pBuffer == pBuffer->pBuffer) {
        inBufferVolume[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->volumehandle, inBufferVolume[0]);
      } else {
        inBufferVolume[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->volumehandle, inBufferVolume[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
      }
    }
  }  else {
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
    } else  if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->volumeEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->volumeEventSem);
    } 
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEFAULT_MESSAGES, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      bEOS=OMX_TRUE;
      //tsem_up(appPriv->eofSem);
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
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  if(pBuffer != NULL){
    if(!bEOS) {
      if(outBufferAudioDec[0]->pBuffer == pBuffer->pBuffer) {
        outBufferAudioDec[0]->nFilledLen=0;
        err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec[0]);
      } else {
        outBufferAudioDec[1]->nFilledLen=0;
        err = OMX_FillThisBuffer(appPriv->audiodechandle, outBufferAudioDec[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    }
    else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
      volCompInBufferDropped++;
      if(volCompInBufferDropped==2) {
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
      if(!bEOS) {
        if(inBufferSink[0]->pBuffer == pBuffer->pBuffer) {
          inBufferSink[0]->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->audiosinkhandle, inBufferSink[0]);
        } else {
          inBufferSink[1]->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->audiosinkhandle, inBufferSink[1]);
        }
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
        }
      } else {
        DEBUG(DEFAULT_MESSAGES,"In %s EOS reached\n",__func__);
        volCompBufferDropped++;
        if(volCompBufferDropped==2) {
          tsem_up(appPriv->eofSem);
        }
      }

    }
  }  else {
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
  if(eEvent == OMX_EventCmdComplete) {
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
      tsem_up(appPriv->sinkEventSem);
    } else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->sinkEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->sinkEventSem);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
    }
  }else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEFAULT_MESSAGES, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      bEOS=OMX_TRUE;
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "eEvent = %i Param1: %i Param2: %i\n", (int)eEvent,(int)Data1,(int)Data2);
  }
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiosinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
  if(!bEOS) {
    if(outBufferVolume[0]->pBuffer == pBuffer->pBuffer) {
      outBufferVolume[0]->nFilledLen=0;
      err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume[0]);
    } else {
      outBufferVolume[1]->nFilledLen=0;
      err = OMX_FillThisBuffer(appPriv->volumehandle, outBufferVolume[1]);
    }
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
    }
  } else {
    DEBUG(DEFAULT_MESSAGES,"In %s EOS reached\n",__func__);
    alsaSinkBufferDropped++;
    if(alsaSinkBufferDropped==2) {
      tsem_up(appPriv->eofSem);
    }
  }

  return OMX_ErrorNone;
}

