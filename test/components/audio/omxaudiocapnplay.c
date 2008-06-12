/**
  @file test/components/audio/omxaudiocapnplay.c
  
  Test application that uses a OpenMAX component, a generic audio source. 
  The application receives an audio stream (.m4v or .264) decoded by a multiple format source component.
  The decoded output is seen by a yuv viewer.
  
  Copyright (C) 2007  STMicroelectronics
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

#include "omxaudiocapnplay.h"

/** defining global declarations */
#define COMPONENT_NAME_BASE "OMX.st.alsa.alsasrc"
#define BASE_ROLE "alsa.alsasrc"
#define COMPONENT_NAME_BASE_LEN 20

OMX_CALLBACKTYPE audiosrccallbacks = { .EventHandler = audiosrcEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = audiosrcFillBufferDone
    };

OMX_CALLBACKTYPE volume_callbacks = { .EventHandler = volumeEventHandler,
    .EmptyBufferDone = volumeEmptyBufferDone,
    .FillBufferDone = volumeFillBufferDone
    };

OMX_CALLBACKTYPE alsasink_callbacks = { .EventHandler = alsasinkEventHandler,
    .EmptyBufferDone = alsasinkEmptyBufferDone,
    .FillBufferDone = NULL
    };


/** global variables */
appPrivateType* appPriv;

/** used with audio source */
OMX_BUFFERHEADERTYPE *pOutBuffer[2];
/** used with volume componenterter if selected */
OMX_BUFFERHEADERTYPE *pInBufferVolc[2], *pOutBufferVolc[2];
OMX_BUFFERHEADERTYPE *pInBufferSink[2];

OMX_U32 buffer_out_size = 16384;

int rate = 0,channel=0;
char *output_file;
FILE *outfile;

int flagIsOutputExpected;
int flagDecodedOutputReceived;
int flagIsVolCompRequested;
int flagIsSinkRequested;
int flagSetupTunnel;

static OMX_BOOL bEOS = OMX_FALSE;

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}


/** help display */
void display_help() {
  printf("\n");
  printf("Usage: omxaudiocapnplay -o outputfile.pcm [-t] [-h] [-s]\n");
  printf("\n");
  printf("       -o outfile.pcm: If this option is specified, the output is written to user specified outfile\n");
  printf("                   If the volume component option (-c) is specified then outfile will be .rgb file\n");
  printf("                   Else outfile will be in .yuv format \n");
  printf("                   N.B : This option is not needed if you use the sink component\n");
  printf("\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  printf("\n");
  printf("       -r 8000 : sample rate[range 8000....48000]\n");
  printf("       -n 2     : number of channel\n");
  printf("       -s       : Uses the audio sink component to play\n");
  printf("       -v       : Volume Component Requested\n");
  printf("\n");
  printf("       -t       : Use Tunneling \n");
  printf("\n");
  exit(1);
}


OMX_ERRORTYPE test_OMX_ComponentNameEnum() {
  char * name;
  int index;

  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n", __func__);
  name = malloc(OMX_MAX_STRINGNAME_SIZE);
  index = 0;
  while(1) {
    err = OMX_ComponentNameEnum (name, OMX_MAX_STRINGNAME_SIZE, index);
    if ((name != NULL) && (err == OMX_ErrorNone)) {
      DEBUG(DEFAULT_MESSAGES, "component %i is %s\n", index, name);
    } else break;
    if (err != OMX_ErrorNone) break;
    index++;
  }
  free(name);
  name = NULL;
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
  return err;
}

OMX_ERRORTYPE test_OMX_RoleEnum(OMX_STRING component_name) {
  OMX_U32 no_of_roles;
  OMX_U8 **string_of_roles;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  int index;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n", __func__);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Getting roles of %s. Passing Null first...\n", component_name);
  err = OMX_GetRolesOfComponent(component_name, &no_of_roles, NULL);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Not able to retrieve the number of roles of the given component\n");
    DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
    return err;
  }
  DEBUG(DEFAULT_MESSAGES, "The number of roles for the component %s is: %i\n", component_name, (int)no_of_roles);

  if(no_of_roles == 0) {
    DEBUG(DEB_LEV_ERR, "The Number or roles is 0.\nThe component selected is not correct for the purpose of this test.\nExiting...\n");    
    err = OMX_ErrorInvalidComponentName;
  }  else {
    string_of_roles = malloc(no_of_roles * sizeof(OMX_STRING));
    for (index = 0; index < no_of_roles; index++) {
      *(string_of_roles + index) = malloc(no_of_roles * OMX_MAX_STRINGNAME_SIZE);
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
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
  return err;
}

OMX_ERRORTYPE test_OMX_ComponentEnumByRole(OMX_STRING role_name) {
  OMX_U32 no_of_comp_per_role;
  OMX_U8 **string_of_comp_per_role;
  OMX_ERRORTYPE err;
  int index;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n", __func__);
  string_of_comp_per_role = malloc (10 * sizeof(OMX_STRING));
  for (index = 0; index < 10; index++) {
    string_of_comp_per_role[index] = malloc(OMX_MAX_STRINGNAME_SIZE);
  }

  DEBUG(DEFAULT_MESSAGES, "Getting number of components per role for %s\n", role_name);

  err = OMX_GetComponentsOfRole(role_name, &no_of_comp_per_role, NULL);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Not able to retrieve the number of components of a given role\n");
    DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
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

  if(string_of_comp_per_role)  {
    free(string_of_comp_per_role);
    string_of_comp_per_role = NULL;
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result OMX_ErrorNone\n", __func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE test_OpenClose(OMX_STRING component_name) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
  err = OMX_GetHandle(&appPriv->audiosrchandle, component_name, NULL, &audiosrccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No component found\n");
  } else {
    err = OMX_FreeHandle(appPriv->audiosrchandle);
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
  return err;
}


int main(int argc, char** argv) {

  OMX_ERRORTYPE err;
  int argn_dec;
  OMX_STRING full_component_name;
  int isRate=0,isChannel=0;
  OMX_AUDIO_PARAM_PCMMODETYPE sPCMModeParam;
  if(argc < 2){
    display_help();
  } else {
    flagIsOutputExpected = 0;
    flagDecodedOutputReceived = 0;
    flagIsVolCompRequested = 0;
    flagSetupTunnel = 0;
    flagIsSinkRequested = 0;

    argn_dec = 1;
    while (argn_dec < argc) {
      if (*(argv[argn_dec]) == '-') {
        if (flagIsOutputExpected) {
          display_help();
        }
        switch (*(argv[argn_dec] + 1)) {
          case 'h' :
            display_help();
            break;
          case 't' :
            flagSetupTunnel = 1;
            flagIsSinkRequested = 1;
            flagIsVolCompRequested = 1;
            break;
          case 's':
            flagIsSinkRequested = 1;
            break;
          case 'o':
            flagIsOutputExpected = 1;
            break;
          case 'v':
            flagIsVolCompRequested = 1;
            break;
          case 'r' :
            isRate = 1;
            break;
          case 'n' :
            isChannel = 1;
            break;
          default:
            display_help();
        }
      } else {
        if (flagIsOutputExpected) {
          if(strstr(argv[argn_dec], ".pcm") == NULL) {
            output_file = malloc(strlen(argv[argn_dec]) + 5);
            strcpy(output_file,argv[argn_dec]);
            strcat(output_file, ".pcm");
          } else {
            output_file = malloc(strlen(argv[argn_dec]) + 1);
            strcpy(output_file,argv[argn_dec]);
          }          
          flagIsOutputExpected = 0;
          flagDecodedOutputReceived = 1;
        } else if(isRate) {
          rate=atoi(argv[argn_dec]);
          isRate=0;
          if(rate <0 || rate >48000) {
            DEBUG(DEB_LEV_ERR, "Bad Parameter rate\n");
            display_help();
          }
        } else if(isChannel) {
          channel=atoi(argv[argn_dec]);
          isChannel = 0;
          if(channel <0 || channel >6) {
            DEBUG(DEB_LEV_ERR, "Bad Parameter channel\n");
            display_help();
          }
        }
      }
      argn_dec++;
    }

    /** if volume componenterter component is not selected then sink component will not work, even if specified */
    if(!flagIsVolCompRequested && flagIsSinkRequested) {
      DEBUG(DEB_LEV_ERR, "You requested for sink - not producing any output file\n");
      flagIsVolCompRequested = 1;
      flagDecodedOutputReceived = 0;
    }

    /** output file name check */
    //case 1 - user did not specify any output file
    if(!flagIsOutputExpected && !flagDecodedOutputReceived && !flagIsSinkRequested) {
      DEBUG(DEB_LEV_ERR,"\n you did not enter any output file name");
      output_file = malloc(20);
      strcpy(output_file,"output.pcm");
      DEBUG(DEB_LEV_ERR,"\n the decoded output file name will be %s \n", output_file);
    } else if(flagDecodedOutputReceived) {
      if(flagIsSinkRequested || flagSetupTunnel) {
        flagDecodedOutputReceived = 0;
        DEBUG(DEB_LEV_ERR, "Sink Requested or Components are tunneled. No FILE Output will be produced\n");
      } else {
        //case 2 - user has given wrong format
        if(flagIsVolCompRequested && strstr(output_file, ".pcm") == NULL) {
          output_file[strlen(output_file) - strlen(strstr(output_file, "."))] = '\0';
          strcat(output_file, ".rgb");
          DEBUG(DEB_LEV_ERR,"\n volume component option is selected - so the output file is %s \n", output_file);
        }
      }
    }
    if(flagSetupTunnel) {
      DEBUG(DEFAULT_MESSAGES,"The components are tunneled between themselves\n");
    }
  }

  if(!flagIsSinkRequested) {
    outfile = fopen(output_file, "wb");
    if(outfile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error in opening output file %s\n", output_file);
      exit(1);
    }
  }

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));  
  appPriv->sourceEventSem = malloc(sizeof(tsem_t));
  if(flagIsVolCompRequested == 1) {
    if(flagIsSinkRequested == 1) {
      appPriv->alsasinkEventSem = malloc(sizeof(tsem_t));
    }
     appPriv->volumeEventSem = malloc(sizeof(tsem_t));
  }
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->sourceEventSem, 0);
  if(flagIsVolCompRequested == 1) {
    if(flagIsSinkRequested == 1) {
      tsem_init(appPriv->alsasinkEventSem, 0);
    }
     tsem_init(appPriv->volumeEventSem, 0);
  } 
  tsem_init(appPriv->eofSem, 0);
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Init the Omx core\n");
  err = OMX_Init();
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Omx core is initialized \n");
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
  

  full_component_name = malloc(OMX_MAX_STRINGNAME_SIZE);
  strcpy(full_component_name, "OMX.st.alsa.alsasrc");

  DEBUG(DEFAULT_MESSAGES, "The component selected for decoding is %s\n", full_component_name);

  /** getting audio source handle */
  err = OMX_GetHandle(&appPriv->audiosrchandle, full_component_name, NULL, &audiosrccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No audio source component found. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "Found The component for capturing is %s\n", full_component_name);
  }

  /** getting volume componenterter component handle, if specified */
  if(flagIsVolCompRequested == 1) {
    err = OMX_GetHandle(&appPriv->volume_handle, "OMX.st.volume.component", NULL, &volume_callbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No volume componenterter component found. Exiting...\n");
      exit(1);
    } else {
      DEBUG(DEFAULT_MESSAGES, "Found The component for volume componenterter \n");
    }

    /** getting sink component handle - if reqd' */
    if(flagIsSinkRequested == 1) {
      err = OMX_GetHandle(&appPriv->alsasink_handle, "OMX.st.alsa.alsasink", NULL, &alsasink_callbacks);
      if(err != OMX_ErrorNone){
        DEBUG(DEB_LEV_ERR, "No audio sink component component found. Exiting...\n");
        exit(1);
      } else {
        DEBUG(DEFAULT_MESSAGES, "Found The audio sink component for volume componenterter \n");
      }
    }
  }

  if(rate >0 || channel >0) {
    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    err = OMX_GetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }
    sPCMModeParam.nChannels = (channel >0 ) ? channel:sPCMModeParam.nChannels;
    sPCMModeParam.nSamplingRate = (rate >0 ) ? rate:sPCMModeParam.nSamplingRate;
    err = OMX_SetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }
    if(flagIsSinkRequested) {
      err = OMX_SetParameter(appPriv->alsasink_handle,OMX_IndexParamAudioPcm,&sPCMModeParam);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSink. Exiting...\n");
        exit(1);
      }
    }
  } else if(flagIsSinkRequested) {
    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    err = OMX_GetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }
    err = OMX_SetParameter(appPriv->alsasink_handle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSink. Exiting...\n");
      exit(1);
    }
  }

  /** output buffer size calculation based on input dimension speculation */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "\n buffer_out_size : %d \n", (int)buffer_out_size);

  /** if tunneling option is given then set up the tunnel between the components */
  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    err = OMX_SetupTunnel(appPriv->audiosrchandle, 0, appPriv->volume_handle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between audio src & volume component Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->volume_handle, 1, appPriv->alsasink_handle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between volume component & audio sink Failed\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES, "Set up Tunnel Completed\n");
  }  

  /** sending command to audio source component to go to idle state */
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  /** in tunnel case, change the volume component and sink comp state to idle */
  if(flagIsVolCompRequested && flagSetupTunnel) {
    err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(flagIsSinkRequested && flagSetupTunnel) {
      err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
  }

  if(flagSetupTunnel) {
    if(flagIsSinkRequested) {
      tsem_down(appPriv->alsasinkEventSem);
    }
    if(flagIsVolCompRequested) {
      tsem_down(appPriv->volumeEventSem);
    }
  }

  /** if tunneling option is not given then allocate buffers on audio source output port */
  if (!flagSetupTunnel) {
    pOutBuffer[0] = pOutBuffer[1] = NULL;
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &pOutBuffer[0], 0, NULL, buffer_out_size);
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &pOutBuffer[1], 0, NULL, buffer_out_size);
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
  tsem_down(appPriv->sourceEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "source Sem free\n");

  if(!flagSetupTunnel) {
    if(flagIsVolCompRequested == 1) {
      pOutBufferVolc[0] = pOutBufferVolc[1] = NULL;
      err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

      /** in non tunneled case, using buffers in volume component input port, allocated by audio dec component output port */
      err = OMX_UseBuffer(appPriv->volume_handle, &pInBufferVolc[0], 0, NULL, buffer_out_size, pOutBuffer[0]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the volume component comp allocate buffer\n");
        exit(1);
      }
      err = OMX_UseBuffer(appPriv->volume_handle, &pInBufferVolc[1], 0, NULL, buffer_out_size, pOutBuffer[1]->pBuffer);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to use the volume component comp allocate buffer\n");
        exit(1);
      }

      /** allocating buffers in the volume componenterter compoennt output port */
      
      err = OMX_AllocateBuffer(appPriv->volume_handle, &pOutBufferVolc[0], 1, NULL, buffer_out_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in volume component\n");
        exit(1);
      }
      err = OMX_AllocateBuffer(appPriv->volume_handle, &pOutBufferVolc[1], 1, NULL, buffer_out_size);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in colro conv\n");
        exit(1);
      }

      DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
      tsem_down(appPriv->volumeEventSem);
      DEBUG(DEFAULT_MESSAGES, "volume Event Sem free\n");

      if(flagIsSinkRequested == 1) {
        err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
        err = OMX_UseBuffer(appPriv->alsasink_handle, &pInBufferSink[0], 0, NULL, buffer_out_size, pOutBufferVolc[0]->pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Unable to use the alsasink_handle comp allocate buffer\n");
          exit(1);
        }
        err = OMX_UseBuffer(appPriv->alsasink_handle, &pInBufferSink[1], 0, NULL, buffer_out_size, pOutBufferVolc[1]->pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Unable to use the alsasink_handle comp allocate buffer\n");
          exit(1);
        }

        DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
        tsem_down(appPriv->alsasinkEventSem);
        DEBUG(DEB_LEV_SIMPLE_SEQ, "audio sink comp Sem free\n");
      }
    }

    if(flagIsVolCompRequested == 1) {
      err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
      tsem_down(appPriv->volumeEventSem);
      if(flagIsSinkRequested == 1) {    
        err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        tsem_down(appPriv->alsasinkEventSem);
      }
    }
  }

  /** send command to change color onv and sink comp in executing state */
  if(flagIsVolCompRequested == 1 && flagSetupTunnel) {
    err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    tsem_down(appPriv->volumeEventSem);
    if(flagIsSinkRequested == 1) {    
      err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
      tsem_down(appPriv->alsasinkEventSem);
    }
  }
  
  /** sending command to audio source component to go to executing state */
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  tsem_down(appPriv->sourceEventSem);

  if(flagIsVolCompRequested == 1 && !flagSetupTunnel) { 
    err = OMX_FillThisBuffer(appPriv->volume_handle, pOutBufferVolc[0]);
    err = OMX_FillThisBuffer(appPriv->volume_handle, pOutBufferVolc[1]);
    DEBUG(DEFAULT_MESSAGES, "---> After fill this buffer function calls to the volume component output buffers\n");
  }
  if (!flagSetupTunnel) {
    err = OMX_FillThisBuffer(appPriv->audiosrchandle, pOutBuffer[0]);
    err = OMX_FillThisBuffer(appPriv->audiosrchandle, pOutBuffer[1]);
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and sourceMutex\n");

  DEBUG(DEFAULT_MESSAGES,"Enter 'q' or 'Q' to exit\n");

  while(1) {
    if('Q' == toupper(getchar())) {
      DEBUG(DEFAULT_MESSAGES,"Stoping capture\n");
      bEOS = OMX_TRUE;
      usleep(10000);
      break;
    }
  }

  DEBUG(DEFAULT_MESSAGES, "The execution of the audio decoding process is terminated\n");

  /** state change of all components from executing to idle */
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if(flagIsVolCompRequested == 1) {
    err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(flagIsSinkRequested == 1) {
      err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
  }

  tsem_down(appPriv->sourceEventSem);
  if(flagIsVolCompRequested == 1) {
    tsem_down(appPriv->volumeEventSem);
    if(flagIsSinkRequested == 1) {
      tsem_down(appPriv->alsasinkEventSem);
    }
  }

  DEBUG(DEFAULT_MESSAGES, "All audio components Transitioned to Idle\n");

  /** sending command to all components to go to loaded state */
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if(flagIsVolCompRequested == 1) {
    err = OMX_SendCommand(appPriv->volume_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if(flagIsSinkRequested == 1) {
      err = OMX_SendCommand(appPriv->alsasink_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    }
  }

  /** freeing buffers of volume component and sink component */
  if(flagIsVolCompRequested == 1 && !flagSetupTunnel) {
    
    DEBUG(DEB_LEV_SIMPLE_SEQ, "volume component to loaded\n");
    err = OMX_FreeBuffer(appPriv->volume_handle, 0, pInBufferVolc[0]);
    err = OMX_FreeBuffer(appPriv->volume_handle, 0, pInBufferVolc[1]);

    err = OMX_FreeBuffer(appPriv->volume_handle, 1, pOutBufferVolc[0]);
    err = OMX_FreeBuffer(appPriv->volume_handle, 1, pOutBufferVolc[1]);

    if(flagIsSinkRequested == 1) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Audio sink to loaded\n");
      err = OMX_FreeBuffer(appPriv->alsasink_handle, 0, pInBufferSink[0]);
      err = OMX_FreeBuffer(appPriv->alsasink_handle, 0, pInBufferSink[1]);
    }
  }

  /** freeing buffers of audio source input ports */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Audio dec to loaded\n");
  if(!flagSetupTunnel) {
    DEBUG(DEB_LEV_PARAMS, "Free Audio dec output ports\n");
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, pOutBuffer[0]);
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, pOutBuffer[1]);
  }

  if(flagIsVolCompRequested == 1) {
    if(flagIsSinkRequested == 1) {
      tsem_down(appPriv->alsasinkEventSem);
    }
    tsem_down(appPriv->volumeEventSem);
  }
  tsem_down(appPriv->sourceEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");

  OMX_FreeHandle(appPriv->audiosrchandle);
  if(flagIsVolCompRequested == 1) {
    if(flagIsSinkRequested == 1) {
      OMX_FreeHandle(appPriv->alsasink_handle);
    }
    OMX_FreeHandle(appPriv->volume_handle);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "audio dec freed\n");

  OMX_Deinit();

  DEBUG(DEFAULT_MESSAGES, "All components freed. Closing...\n");
  free(appPriv->sourceEventSem);
  if(flagIsVolCompRequested == 1) {
    if(flagIsSinkRequested == 1) {
      free(appPriv->alsasinkEventSem);
    }
    free(appPriv->volumeEventSem);
  }
  free(appPriv->eofSem);
  free(appPriv);
  
  free(full_component_name);

  /** closing the output file */
  if(!flagIsSinkRequested) {
    fclose(outfile);
  }
  if(output_file) {
    free(output_file);
  }

  return 0;
}

/** callbacks implementation of audio sink component */
OMX_ERRORTYPE alsasinkEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
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
          DEBUG(DEB_LEV_SIMPLE_SEQ, "OMX_StateIdle ---- fbsink\n");
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
      tsem_up(appPriv->alsasinkEventSem);
    }      
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->alsasinkEventSem);
    } 
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_ERR, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return OMX_ErrorNone;
}


OMX_ERRORTYPE alsasinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int inputBufferDropped = 0;
  if(pBuffer != NULL) {
    if(!bEOS) {
      if(pOutBufferVolc[0]->pBuffer == pBuffer->pBuffer) {
        pOutBufferVolc[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->volume_handle, pOutBufferVolc[0]);
      } else {
        pOutBufferVolc[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->volume_handle, pOutBufferVolc[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
      inputBufferDropped++;
      if(inputBufferDropped == 2) {
        tsem_up(appPriv->eofSem);
      }
    }
  } else {
    if(!bEOS) {
      tsem_up(appPriv->eofSem);
    }
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}


/* Callbacks implementation of the volume component component */
OMX_ERRORTYPE volumeEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  DEBUG(DEB_LEV_SIMPLE_SEQ, "\nHi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "\nState changed in ");
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
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->volumeEventSem);
    } 
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_ERR, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
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
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int iBufferDropped = 0;

  if(pBuffer != NULL) {
    if(!bEOS) {
      if(pOutBuffer[0]->pBuffer == pBuffer->pBuffer) {
        pOutBuffer[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, pOutBuffer[0]);
      } else {
        pOutBuffer[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, pOutBuffer[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    } else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
      iBufferDropped++;
      if(iBufferDropped == 2) {
        tsem_up(appPriv->eofSem);
      }
    }
  } else {
    if(!bEOS) {
      tsem_up(appPriv->eofSem);
    }
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}  


OMX_ERRORTYPE volumeFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  if(pBuffer != NULL) {
    if(!bEOS) {
      /** if there is no sink component then write buffer content in output file, in non tunneled case 
        * else in non tunneled case, call the sink comp handle to process this buffer as its input buffer
        */
      if(flagIsSinkRequested && (!flagSetupTunnel)) {
        if(pInBufferSink[0]->pBuffer == pBuffer->pBuffer) {
          pInBufferSink[0]->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->alsasink_handle, pInBufferSink[0]);
        } else {
          pInBufferSink[1]->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->alsasink_handle, pInBufferSink[1]);
        }
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
        }
      } else if((pBuffer->nFilledLen > 0) && (!flagSetupTunnel)) {
        fwrite(pBuffer->pBuffer, 1,  pBuffer->nFilledLen, outfile);    
        pBuffer->nFilledLen = 0;
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      if(!bEOS && !flagIsSinkRequested && (!flagSetupTunnel)) {
        err = OMX_FillThisBuffer(hComponent, pBuffer);
      }
    } else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;  
}

/** Callbacks implementation of the audio source component*/
OMX_ERRORTYPE audiosrcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData) {

  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
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
      tsem_up(appPriv->sourceEventSem);
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->sourceEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "\n port settings change event handler in %s \n", __func__);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_ERR, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return err; 
}

OMX_ERRORTYPE audiosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  OMX_STATETYPE eState;
  
  if(pBuffer != NULL){
    if(!bEOS) {
      /** if there is volume component component in processing state then send this buffer, in non tunneled case 
        * else in non tunneled case, write the output buffer contents in the specified output file
        */
      if(flagIsVolCompRequested && (!flagSetupTunnel)) {
        OMX_GetState(appPriv->volume_handle,&eState);
        if(eState == OMX_StateExecuting || eState == OMX_StatePause) {
          if(pInBufferVolc[0]->pBuffer == pBuffer->pBuffer) {
            pInBufferVolc[0]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->volume_handle, pInBufferVolc[0]);
          } else {
            pInBufferVolc[1]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->volume_handle, pInBufferVolc[1]);
          }
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
          }
        } else {
          err = OMX_FillThisBuffer(hComponent, pBuffer);
        }
      } else if((pBuffer->nFilledLen > 0) && (!flagSetupTunnel)) {
        fwrite(pBuffer->pBuffer, 1,  pBuffer->nFilledLen, outfile);    
        pBuffer->nFilledLen = 0;
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      if(!bEOS && !flagIsVolCompRequested && (!flagSetupTunnel)) {
        err = OMX_FillThisBuffer(hComponent, pBuffer);
      }
    } else {
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;  
}


