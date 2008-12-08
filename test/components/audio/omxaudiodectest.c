/**
  @file test/components/audio/omxaudiodectest.c

  Test application that uses three OpenMAX components, a file reader, an audio decoder 
  and an ALSA sink. The application receives an compressed audio stream on input port
  from a file, decodes it and sends it to the ALSA sink, or to a file or standard output.
  The audio formats supported are:
  MP3 (FFmpeg)

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

  $Date$
  Revision $Rev$
  Author $Author$
*/


#include "omxaudiodectest.h"

#define MP3_TYPE_SEL    1
#define VORBIS_TYPE_SEL 2
#define AAC_TYPE_SEL    3 
#define G726_TYPE_SEL   4 
#define AMR_TYPE_SEL    5 
#define COMPONENT_NAME_BASE "OMX.st.audio_decoder"
#define BASE_ROLE "audio_decoder.ogg"
#define COMPONENT_NAME_BASE_LEN 20
#define SINK_NAME "OMX.st.alsa.alsasink"
#define FILE_READER "OMX.st.audio_filereader"
#define AUDIO_EFFECT "OMX.st.volume.component"
#define extradata_size 1024

appPrivateType* appPriv;

OMX_BUFFERHEADERTYPE *outBufferFileRead[2];
OMX_BUFFERHEADERTYPE *inBufferAudioDec[2],*outBufferAudioDec[2];
OMX_BUFFERHEADERTYPE *inBufferVolume[2],*outBufferVolume[2];
OMX_BUFFERHEADERTYPE *inBufferSink[2];
int buffer_in_size = BUFFER_IN_SIZE; 
int buffer_out_size = BUFFER_OUT_SIZE;
static OMX_BOOL bEOS=OMX_FALSE;

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

FILE *fd ,*outfile;
char *input_file, *output_file;
int selectedType = 0;

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
  printf("       -s single_ogg: Use the single role Ogg Vorbis decoder instead of the default one. Can't be used with -m or .mp3 file\n");
  printf("       -t: The audio decoder is tunneled with the ALSA sink\n");
  printf("       -m: For MP3 decoding use the mad library. Can't be used with -s or .ogg file\n");
  printf("       -d: If no output is specified, and no playback is specified,\n");
  printf("           this flag activated the print of the stream directly on stdout\n");
  printf("       -f: Use filereader with mad\n");
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
  name = malloc(OMX_MAX_STRINGNAME_SIZE);
  index = 0;
  while(1) {
    err = OMX_ComponentNameEnum (name, OMX_MAX_STRINGNAME_SIZE, index);
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
      *(string_of_roles + index) = (OMX_U8 *)malloc(no_of_roles*OMX_MAX_STRINGNAME_SIZE);
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
    string_of_comp_per_role[index] = malloc(OMX_MAX_STRINGNAME_SIZE);
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


int main(int argc, char** argv) {
  int argn_dec;
  char *temp = NULL;
  OMX_ERRORTYPE err;
  OMX_INDEXTYPE eIndexParamFilename;
  OMX_STRING full_component_name;
  int index;
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
    flagIsMadUsingFileReader = 0;
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
        case 'f':
          flagIsMadUsingFileReader = 1;
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
          output_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(output_file,argv[argn_dec]);
          flagIsOutputExpected = 0;
          flagOutputReceived = 1;
        } else {
          input_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(input_file,argv[argn_dec]);
          flagInputReceived = 1;
        }
      }
      argn_dec++;
    }
    if (flagSetupTunnel) {
      if(flagOutputReceived) {
        DEBUG(DEFAULT_MESSAGES, "-o Option Ignored. No FILE output will be produced.\n");
        flagOutputReceived = 0;
      }
      flagPlaybackOn = 1;
    }
    if (!flagInputReceived) {
      display_help();
    }
    DEBUG(DEFAULT_MESSAGES, "Options selected:\n");
    DEBUG(DEFAULT_MESSAGES, "Decode file %s", input_file);
    DEBUG(DEFAULT_MESSAGES, " to ");
    if (flagPlaybackOn) {
      DEBUG(DEFAULT_MESSAGES, " ALSA sink");
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
    if(flagSingleOGGSelected) {
      DEBUG(DEB_LEV_ERR, "ERROR:Wrong Format(OGG Single) Selected\n");
      display_help();
    }
  } else if(*(input_file+index -1) == 'g') {
    selectedType = VORBIS_TYPE_SEL;
    DEBUG(DEFAULT_MESSAGES, "VORBIS\n");
    if(flagIsMadRequested) {
      DEBUG(DEB_LEV_ERR, "ERROR:Wrong Format(MAD) Selected\n");
      display_help();
    }
  } else if(*(input_file+index -1) == 'c') {   
    selectedType = AAC_TYPE_SEL;
    DEBUG(DEFAULT_MESSAGES, "AAC\n");
    if(flagIsMadRequested || flagSingleOGGSelected) {
      DEBUG(DEB_LEV_ERR, "ERROR:Wrong Format(MAD/VORBIS) Selected\n");
      display_help();
    }
  } else if(*(input_file+index -1) == '6') {   
    selectedType = G726_TYPE_SEL;
    DEBUG(DEFAULT_MESSAGES, "G726\n");
    if(flagIsMadRequested || flagSingleOGGSelected) {
      DEBUG(DEB_LEV_ERR, "ERROR:Wrong Format(MAD/VORBIS) Selected\n");
      display_help();
    }
    flagUsingFFMpeg = 0; //Explicitly not using filereader in case of G726
  } else if(*(input_file+index -1) == 'r') {   
    selectedType = AMR_TYPE_SEL;
    DEBUG(DEFAULT_MESSAGES, "AMR\n");
    if(flagIsMadRequested || flagSingleOGGSelected) {
      DEBUG(DEB_LEV_ERR, "ERROR:Wrong Format(MAD/VORBIS) Selected\n");
      display_help();
    }
  } else {
    DEBUG(DEB_LEV_ERR, "The input audio format is not supported - exiting\n");
    exit(1);
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
  test_OMX_ComponentEnumByRole(BASE_ROLE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
  test_OpenClose(COMPONENT_NAME_BASE);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");

  full_component_name = (OMX_STRING) malloc(OMX_MAX_STRINGNAME_SIZE);
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
  } else if (selectedType == AAC_TYPE_SEL) {   
    strcpy(full_component_name+COMPONENT_NAME_BASE_LEN, ".aac");
  } else if (selectedType == G726_TYPE_SEL) {   
    strcpy(full_component_name+COMPONENT_NAME_BASE_LEN, ".g726");
  } else if (selectedType == AMR_TYPE_SEL) {   
    strcpy(full_component_name+COMPONENT_NAME_BASE_LEN, ".amr");
  }

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Using File Reader\n");
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
      DEBUG(DEB_LEV_ERR, "No Volume Component found. Exiting...\n");
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

  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
    /** setting the input audio format in file reader */
    err = OMX_GetExtensionIndex(appPriv->filereaderhandle,"OMX.ST.index.param.inputfilename",&eIndexParamFilename);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
      exit(1);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"FileName Param index : %x \n",eIndexParamFilename);
      temp = malloc(25);
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

  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Dec\n");
    /*Send State Change Idle command to Audio Decoder*/
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if (flagPlaybackOn) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Sink\n");
      err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
      err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
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
  if(flagUsingFFMpeg || flagIsMadUsingFileReader) {
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

  /*Send State Change Idle command to Audio Decoder*/
  if (!flagSetupTunnel) {
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

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
    /*Wait for decoder state change to idle*/
    tsem_down(appPriv->decoderEventSem);
  }


  if ((flagPlaybackOn) && (!flagSetupTunnel)) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

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

    if (flagPlaybackOn) {
      tsem_down(appPriv->volumeEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"volume state idle\n");
    }

    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

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
    if (flagPlaybackOn) {
      tsem_down(appPriv->sinkEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"audio sink state idle\n");
    }
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

  if (!flagSetupTunnel && flagPlaybackOn) {
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

  if(!flagUsingFFMpeg && !flagIsMadUsingFileReader) {
    data_read = fread(inBufferAudioDec[0]->pBuffer, 1, buffer_in_size, fd);
    inBufferAudioDec[0]->nFilledLen = data_read;
    inBufferAudioDec[0]->nOffset = 0;

    data_read = fread(inBufferAudioDec[1]->pBuffer, 1, buffer_in_size, fd);
    inBufferAudioDec[1]->nFilledLen = data_read;
    inBufferAudioDec[1]->nOffset = 0;

    DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBufferAudioDec[0]);
    err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[0]);
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBufferAudioDec[1]);
    err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[1]);
  }
  /* Call FillThisBuffer now, to ensure that first two input buffers has already been sent to the component*/
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
  }

  DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS = %d\n",appPriv->eofSem->semval);

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
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    OMX_AUDIO_PARAM_PORTFORMATTYPE sPortFormat;
    OMX_AUDIO_PARAM_AMRTYPE sAmrParam;
    OMX_ERRORTYPE err;

    DEBUG(DEB_ALL_MESS,"File reader Port Setting Changed event\n");

    setHeader(&sPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    sPortFormat.nPortIndex = 0;
    err = OMX_GetParameter(appPriv->filereaderhandle,OMX_IndexParamAudioPortFormat, &sPortFormat);

    if (!flagSetupTunnel && sPortFormat.eEncoding == OMX_AUDIO_CodingAMR) {

      err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortDisable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Audio Decoder port disable failed\n");
        exit(1);
      }

      err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[0]);
      err = OMX_FreeBuffer(appPriv->audiodechandle, 0, inBufferAudioDec[1]);

      /*Wait for Decoder Ports Disable Event*/
      tsem_down(appPriv->decoderEventSem);

      setHeader(&sAmrParam, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
      sAmrParam.nPortIndex = 0;
      err = OMX_GetParameter(appPriv->filereaderhandle,OMX_IndexParamAudioAmr, &sAmrParam);

      DEBUG(DEB_ALL_MESS,"In %s nChannels =%d, nBitRate = %d eAMRBandMode=%x\n",
        __func__,(int)sAmrParam.nChannels,(int)sAmrParam.nBitRate,(int)sAmrParam.eAMRBandMode);
      
      err = OMX_SetParameter(appPriv->audiodechandle, OMX_IndexParamAudioAmr, &sAmrParam);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
      }

      err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandPortEnable, 0, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"ALSA Decoder port disable failed\n");
        exit(1);
      }

      /** the output buffers of file reader component will be used 
      *  in the audio decoder component as input buffers 
      */ 
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[0], 0, NULL, buffer_out_size, outBufferFileRead[0]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Unable to use the file read comp allocate buffer 1\n",__func__);
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->audiodechandle, &inBufferAudioDec[1], 0, NULL, buffer_out_size, outBufferFileRead[1]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Unable to use the file read comp allocate buffer 2\n",__func__);
        exit(1);
      }
      tsem_down(appPriv->decoderEventSem);
    }
  } else if(eEvent == OMX_EventPortFormatDetected) {
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
      if(inBufferAudioDec[0]->pBuffer == pBuffer->pBuffer) {
        inBufferAudioDec[0]->nFilledLen = pBuffer->nFilledLen;
        inBufferAudioDec[0]->nFlags     = pBuffer->nFlags;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[0]);
      } else {
        inBufferAudioDec[1]->nFilledLen = pBuffer->nFilledLen;
        inBufferAudioDec[1]->nFlags     = pBuffer->nFlags;
        err = OMX_EmptyThisBuffer(appPriv->audiodechandle, inBufferAudioDec[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
      if(pBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Calling Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
        bEOS=OMX_TRUE;
      }
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
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
    } else if (Data1 == OMX_CommandPortEnable){
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
          DEBUG(DEB_LEV_ERR,"ALSA sink port disable failed\n");
          exit(1);
        }

        err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[0]);
        err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[1]);

        /*Wait for sink Ports Disable Event*/
        tsem_down(appPriv->sinkEventSem);

        pcmParam.nPortIndex=0;
        err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &pcmParam);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }

        err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandPortEnable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"ALSA sink port disable failed\n");
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
      } else {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
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

    data_read = fread(pBuffer->pBuffer, 1, buffer_in_size, fd);
    pBuffer->nFilledLen = data_read;
    pBuffer->nOffset = 0;
    if (data_read <= 0) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
      iBufferDropped++;
      if(iBufferDropped>=2) {
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
    } else {
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
  /* Output data to ALSA sink */
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
    } else if ((!flagSetupTunnel) && (flagPlaybackOn))  { //playback on, redirect to ALSA sink, if it is not tunneled
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
  } else {
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
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
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
  static int volCompBufferDropped=0;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
  /* Output data to ALSA sink */
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
    } else if ((!flagSetupTunnel) && (flagPlaybackOn))  { //playback on, redirect to ALSA sink, if it is not tunneled
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
  } else {
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
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiosinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  static int alsaSinkBufferDropped=0;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", (int)pBuffer);
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

