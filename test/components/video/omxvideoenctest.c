/**
  @file test/components/video/omxvideoenctest.c
  
  Test application that uses a OpenMAX component, a generic video encoder. 
  The application receives an video stream (.yuv) encode in Mpeg4 Video format(.m4v).
  
  Copyright (C) 2008  STMicroelectronics and Nokia

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

#include "omxvideoenctest.h"

/** defining global declarations */
#define MPEG4_TYPE_SEL 1
#define COMPONENT_NAME_BASE "OMX.st.video_encoder"
#define BASE_ROLE "video_encoder.mpeg4"
#define COMPONENT_NAME_BASE_LEN 20

OMX_CALLBACKTYPE videoenccallbacks = { 
    .EventHandler = videoencEventHandler,
    .EmptyBufferDone = videoencEmptyBufferDone,
    .FillBufferDone = videoencFillBufferDone
  };

OMX_CALLBACKTYPE videosrccallbacks = { 
    .EventHandler = videosrcEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = videosrcFillBufferDone
  };

/** global variables */
appPrivateType* appPriv;

char *input_file, *output_file;
FILE *infile,*outfile;
int selectedType = 0;
static OMX_BOOL bEOS = OMX_FALSE;
OMX_U32 in_width = 176;
OMX_U32 in_height = 144;
OMX_U32 frame_rate = 25;
int buffer_in_size = BUFFER_IN_SIZE;
OMX_U32 buffer_out_size = 32768;
OMX_U32 outbuf_colorconv_size;
char codecName[10];

OMX_COLOR_FORMATTYPE eColorFormat = OMX_COLOR_FormatYUV420Planar;

/** used with video encoder */
OMX_BUFFERHEADERTYPE *pInBuffer[2], *pOutBuffer[2], *pSrcOutBuffer[2];

OMX_PARAM_PORTDEFINITIONTYPE paramPort;
OMX_PARAM_COMPONENTROLETYPE paramRole;
OMX_VIDEO_PARAM_PORTFORMATTYPE omxVideoParam;

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
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %08x\n", __func__, err);
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
    string_of_roles = (OMX_U8**)malloc(no_of_roles * sizeof(OMX_STRING));
    for (index = 0; index < no_of_roles; index++) {
      *(string_of_roles + index) = (OMX_U8 *)malloc(no_of_roles * OMX_MAX_STRINGNAME_SIZE);
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
  string_of_comp_per_role = (OMX_U8**) malloc (10 * sizeof(OMX_STRING));
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
  err = OMX_GetHandle(&appPriv->videoenchandle, component_name, NULL, &videoenccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No component found\n");
  } else {
    err = OMX_FreeHandle(appPriv->videoenchandle);
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
  return err;
}



/**  Try to determine encoder type based on filename */
int find_encoder(char* searchname) {
  char filename_lower[255];
  char* ptr = filename_lower;
  
  strcpy(filename_lower, searchname);
  while (*ptr != '\0' && ptr - filename_lower < 255) {
    *ptr = tolower(*ptr);
    ++ptr;
  }
  if (strstr(filename_lower, "m4v") != NULL) {
    strcpy(&codecName[0],"m4v");
    return 0;
  } 
  return 1;
}

/** help display */
void display_help() {
  printf("\n");
  printf("Usage: omxvideoenctest -o outfile [-W 320] [-H 240] [-t] [-C] [-h] [-f input_fmt] -i input_filename\n");
  printf("\n");
  printf("       -i infile : Input yuv file name\n");
  printf("       -o outfile: If this option is specified, the output is written to user specified outfile\n");
  printf("                   Else, the output is written in the same directory of input file\n");
  printf("                     the file name looks like input_filename_app.m4v depending on input option\n");
  printf("\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  printf("       -f : output codec format \n");
  printf("            The available output formats are - \n");
  printf("              - m4v \n");
  printf("       -W width\n");
  printf("       -H Height\n");
  printf("       -C use camera as input source\n");
  printf("       -r frame per second\n");
  printf("       -t use tunnel between video source and encoder\n");
  printf("\n");
  exit(1);
}

int flagIsOutputExpected;
int flagIsInputExpected;
int flagOutputReceived;
int flagInputReceived;
int flagIsFormatRequested;
int flagIsWidth;
int flagIsHeight;
int flagIsCameraRequested;
int flagSetupTunnel;
int flagIsFPS;

/** this function sets the video source and encoder port characteristics  */
int setPortParameters() {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  paramPort.nPortIndex = 0;
  setHeader(&paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &paramPort);
  paramPort.format.video.nFrameWidth = in_width;
  paramPort.format.video.nFrameHeight = in_height;
  paramPort.format.video.xFramerate = frame_rate;
  err = OMX_SetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &paramPort);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
    return err; 
  } 
  DEBUG(DEB_LEV_SIMPLE_SEQ, "input picture width : %d height : %d \n", (int)in_width, (int)in_height);

  if(flagIsCameraRequested) {
    paramPort.nPortIndex = 0;
    setHeader(&paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    err = OMX_GetParameter(appPriv->videosrchandle, OMX_IndexParamPortDefinition, &paramPort);
    paramPort.format.video.nFrameWidth  = in_width;
    paramPort.format.video.nFrameHeight = in_height;
    err = OMX_SetParameter(appPriv->videosrchandle, OMX_IndexParamPortDefinition, &paramPort);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
      return err; 
    } 
  }

  return err;
}
int main(int argc, char** argv) {

  OMX_ERRORTYPE err;
  int data_read;
  int argn_dec;
  OMX_STRING full_component_name;

  if(argc < 2){
    display_help();
  } else {
    flagIsInputExpected = 0;
    flagIsOutputExpected = 0;
    flagOutputReceived = 0;
    flagInputReceived = 0;
    flagIsFormatRequested = 0;
    flagIsWidth = 0;
    flagIsHeight = 0;
    flagIsCameraRequested = 0;
    flagIsFPS = 0;

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
          case 'i':
            flagIsInputExpected = 1;
            break;
          case 'o':
            flagIsOutputExpected = 1;
            break;
          case 'f' :
            flagIsFormatRequested = 1;
            break;
          case 't' :
            flagSetupTunnel = 1;
            break;
          case 'W' :
            flagIsWidth = 1;
            break;
          case 'H' :
            flagIsHeight = 1;
            break;
          case 'C' :
            flagIsCameraRequested = 1;
            break;
          case 'r' :
            flagIsFPS = 1;
            break;
          default:
            display_help();
        }
      } else {
        if (flagIsOutputExpected) {
          if(strstr(argv[argn_dec], ".m4v") == NULL) {
            int k=0;
            if(strstr(argv[argn_dec],".")) {
              k = strlen(strstr(argv[argn_dec],"."));
            }
            output_file = malloc(strlen(argv[argn_dec]) - k + 6);
            strncpy(output_file,argv[argn_dec], (strlen(argv[argn_dec]) - k));
            strcat(output_file,".m4v");
            DEBUG(DEFAULT_MESSAGES, "New output File %s \n", output_file);
          } else {
            output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
            strcpy(output_file,argv[argn_dec]);
          }          
          flagIsOutputExpected = 0;
          flagOutputReceived = 1;
        } else if(flagIsFormatRequested) {
          if(strstr(argv[argn_dec], "m4v") != NULL) {
            strcpy(&codecName[0],"m4v");
          } else {
            DEBUG(DEB_LEV_ERR, "Specified output format is unknown - keeping default m4v format\n");
          }
          flagIsFormatRequested = 0;
        } else if(flagIsWidth) {
          in_width = atoi(argv[argn_dec]);
          flagIsWidth = 0; 
        } else if(flagIsHeight) {
          in_height = atoi(argv[argn_dec]);
          flagIsHeight = 0; 
        } else if(flagIsInputExpected){
          input_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
          strcpy(input_file,argv[argn_dec]);
          flagIsInputExpected = 0;
          flagInputReceived = 1;
        } else if(flagIsFPS) {
          frame_rate = atoi(argv[argn_dec]);
          flagIsFPS = 0;
        }
      }
      argn_dec++;
    }

    /*Camera source is given higher priority over input file*/
    if(flagIsCameraRequested) {
      if(flagInputReceived) {
        DEBUG(DEFAULT_MESSAGES, "Ignoring Input File %s, taking input from camera\n", input_file);
        flagInputReceived = 0;
        if(input_file) {
          free(input_file);
        }
      }
    } else {
      if(flagSetupTunnel) {
        DEBUG(DEFAULT_MESSAGES, "Camera Not Selected. Tunneling Can't be used\n");
        DEBUG(DEFAULT_MESSAGES, "Use option '-C' to select camera\n");
        exit(1);
      }
    }

    /** output file name check */
    if ((!flagOutputReceived) || (strstr(output_file, ".m4v") == NULL)) {
      DEBUG(DEB_LEV_ERR, "You must specify appropriate input file of .m4v format\n");
      display_help();
    }

    if (flagInputReceived != 1 && !flagIsCameraRequested) {
      DEBUG(DEB_LEV_ERR, "You must specify appropriate input file \n");
      display_help();
    }

    /** determine the output file format in case of encoding */
    if((strstr(output_file, ".m4v") != NULL)) {
      selectedType = MPEG4_TYPE_SEL;
    } else {
      DEBUG(DEB_LEV_ERR, "\n input file format and operation not supported \n");
      display_help();
    }
  }

  if(!flagIsCameraRequested) {
    infile = fopen(input_file, "rb");
    if(infile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error in opening input file %s\n", input_file);
      exit(1);
    }
  }

  outfile = fopen(output_file, "wb");
  if(outfile == NULL) {
    DEBUG(DEB_LEV_ERR, "Error in opening output file %s\n", output_file);
    exit(1);
  }

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));  
  appPriv->encoderEventSem = malloc(sizeof(tsem_t));
  appPriv->eofSem = malloc(sizeof(tsem_t));
  appPriv->sourceEventSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->encoderEventSem, 0);
  tsem_init(appPriv->eofSem, 0);
  tsem_init(appPriv->sourceEventSem, 0);
  
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
  
  full_component_name = (OMX_STRING) malloc(sizeof(char*) * OMX_MAX_STRINGNAME_SIZE);
  strcpy(full_component_name, COMPONENT_NAME_BASE);
  if(selectedType == MPEG4_TYPE_SEL) {
    strcpy(full_component_name + COMPONENT_NAME_BASE_LEN, ".mpeg4");
  } 

  DEBUG(DEFAULT_MESSAGES, "The component selected for encoding is %s\n", full_component_name);

  if(flagIsCameraRequested) {
    /** getting video source handle */
    err = OMX_GetHandle(&appPriv->videosrchandle, "OMX.st.video_src", NULL, &videosrccallbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No video source component found. Exiting...\n");
      exit(1);
    } else {
      DEBUG(DEFAULT_MESSAGES, "Found The component for capturing is %s\n", full_component_name);
    }
  }

  /** getting video encoder handle */
  err = OMX_GetHandle(&appPriv->videoenchandle, full_component_name, NULL, &videoenccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No video encoder component found. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "Found The component for encoding is %s\n", full_component_name);
  }

  setHeader(&paramRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
  err = OMX_GetParameter(appPriv->videoenchandle, OMX_IndexParamStandardComponentRole, &paramRole);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The role set for this component can not be retrieved err = %i\n", err);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "The role currently set is %s\n", paramRole.cRole);

  /** if tunneling option is given then set up the tunnel between the components */
  if(flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    err = OMX_SetupTunnel(appPriv->videosrchandle, 0, appPriv->videoenchandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between video source and encoder Failed\n");
      exit(1);
    }
    
    DEBUG(DEFAULT_MESSAGES, "Set up Tunnel Completed\n");
  }  

  /** output buffer size calculation based on input dimension speculation */
  buffer_in_size = in_width * in_height * 3/2; //yuv420 format -- bpp = 12
  DEBUG(DEB_LEV_SIMPLE_SEQ, "buffer_in_size : %d \n", (int)buffer_in_size);

  setPortParameters() ;


  /** sending command to video encoder component to go to idle state */
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  if(flagIsCameraRequested) {
    /** sending command to video source component to go to idle state */
    err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  
  if (!flagSetupTunnel) {
    pInBuffer[0] = pInBuffer[1] = NULL;
    if (flagIsCameraRequested) {
      pOutBuffer[0] = pOutBuffer[1] = NULL;
      err = OMX_AllocateBuffer(appPriv->videosrchandle, &pSrcOutBuffer[0], 0, NULL, buffer_in_size);
      err = OMX_AllocateBuffer(appPriv->videosrchandle, &pSrcOutBuffer[1], 0, NULL, buffer_in_size);

      err = OMX_UseBuffer(appPriv->videoenchandle, &pInBuffer[0], 0, NULL, buffer_in_size, pSrcOutBuffer[0]->pBuffer);
      err = OMX_UseBuffer(appPriv->videoenchandle, &pInBuffer[1], 0, NULL, buffer_in_size, pSrcOutBuffer[1]->pBuffer);
    } else {
      err = OMX_AllocateBuffer(appPriv->videoenchandle, &pInBuffer[0], 0, NULL, buffer_in_size);
      err = OMX_AllocateBuffer(appPriv->videoenchandle, &pInBuffer[1], 0, NULL, buffer_in_size);
    }
  }

  pOutBuffer[0] = pOutBuffer[1] = NULL;
  err = OMX_AllocateBuffer(appPriv->videoenchandle, &pOutBuffer[0], 1, NULL, buffer_out_size);
  err = OMX_AllocateBuffer(appPriv->videoenchandle, &pOutBuffer[1], 1, NULL, buffer_out_size);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
  tsem_down(appPriv->encoderEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "encoder Sem free\n");
  if (flagIsCameraRequested) {
    tsem_down(appPriv->sourceEventSem);
  }
  
  /** sending command to video encoder component to go to executing state */
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  tsem_down(appPriv->encoderEventSem);

  if (flagIsCameraRequested) {
    /** sending command to video source component to go to executing state */
    err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    tsem_down(appPriv->sourceEventSem);
    if (!flagSetupTunnel) {
      err = OMX_FillThisBuffer(appPriv->videosrchandle, pSrcOutBuffer[0]);
      err = OMX_FillThisBuffer(appPriv->videosrchandle, pSrcOutBuffer[1]);
    }
    
  }

  err = OMX_FillThisBuffer(appPriv->videoenchandle, pOutBuffer[0]);
  err = OMX_FillThisBuffer(appPriv->videoenchandle, pOutBuffer[1]);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and encoderMutex\n");

  if(!flagIsCameraRequested) {
    data_read = fread(pInBuffer[0]->pBuffer, sizeof(char), buffer_in_size, infile);
    pInBuffer[0]->nFilledLen = data_read;
    pInBuffer[0]->nOffset = 0;

    data_read = fread(pInBuffer[1]->pBuffer, sizeof(char), buffer_in_size, infile);
    pInBuffer[1]->nFilledLen = data_read;
    pInBuffer[1]->nOffset = 0;
  
    DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)pInBuffer[0]->pBuffer);
    err = OMX_EmptyThisBuffer(appPriv->videoenchandle, pInBuffer[0]);
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)pInBuffer[1]->pBuffer);
    err = OMX_EmptyThisBuffer(appPriv->videoenchandle, pInBuffer[1]);
  
    DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS\n");

    /** in tunneled case, disable the video components ports and then set parameters 
      * after that, ebnable those ports
      */
  
    tsem_down(appPriv->eofSem);
  } else {
    DEBUG(DEFAULT_MESSAGES,"Enter 'q' or 'Q' to exit\n");

    while(1) {
      if('Q' == toupper(getchar())) {
        DEBUG(DEFAULT_MESSAGES,"Stoping capture\n");
        bEOS = OMX_TRUE;
        break;
      }
    }
  }

  DEBUG(DEFAULT_MESSAGES, "The execution of the video encoding process is terminated\n");

  /** state change of all components from executing to idle */
  if (flagIsCameraRequested) {
    err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  tsem_down(appPriv->encoderEventSem);
  
  if (flagIsCameraRequested) {
    tsem_down(appPriv->sourceEventSem);
  }

  DEBUG(DEFAULT_MESSAGES, "All video components Transitioned to Idle\n");

  /** sending command to all components to go to loaded state */
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

  if (flagIsCameraRequested) {
    err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }

  /** freeing buffers of color conv and sink component */

  /** freeing buffers of video encoder input ports */
  DEBUG(DEB_LEV_PARAMS, "Video enc to loaded\n");
  if (!flagSetupTunnel) {
    err = OMX_FreeBuffer(appPriv->videoenchandle, 0, pInBuffer[0]);
    err = OMX_FreeBuffer(appPriv->videoenchandle, 0, pInBuffer[1]);

    if (flagIsCameraRequested) {
      err = OMX_FreeBuffer(appPriv->videosrchandle, 0, pSrcOutBuffer[0]);
      err = OMX_FreeBuffer(appPriv->videosrchandle, 0, pSrcOutBuffer[1]);
    }
  }
  

  DEBUG(DEB_LEV_PARAMS, "Free Video enc output ports\n");
  err = OMX_FreeBuffer(appPriv->videoenchandle, 1, pOutBuffer[0]);
  err = OMX_FreeBuffer(appPriv->videoenchandle, 1, pOutBuffer[1]);

  tsem_down(appPriv->encoderEventSem);

  if (flagIsCameraRequested) {
    tsem_down(appPriv->sourceEventSem);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");

  OMX_FreeHandle(appPriv->videoenchandle);

  if (flagIsCameraRequested) {
    OMX_FreeHandle(appPriv->videosrchandle);
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "video encoder and source freed\n");

  OMX_Deinit();

  DEBUG(DEFAULT_MESSAGES, "All components freed. Closing...\n");
  free(appPriv->encoderEventSem);
  free(appPriv->sourceEventSem);
  free(appPriv->eofSem);
  free(appPriv);

  /** closing the output file */
  fclose(outfile);
  if(!flagIsCameraRequested) {
    /** closing the input file */
    fclose(infile);
  }

  return 0;
}


/** Callbacks implementation of the video source component*/
OMX_ERRORTYPE videosrcEventHandler(
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
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return err; 
}

OMX_ERRORTYPE videosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  OMX_STATETYPE eState;

  if(pBuffer != NULL){
    if(!bEOS) {
      if(!flagSetupTunnel) {
        OMX_GetState(appPriv->videoenchandle,&eState);
        if(eState == OMX_StateExecuting || eState == OMX_StatePause) {
          if(pInBuffer[0]->pBuffer == pBuffer->pBuffer) {
            pInBuffer[0]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->videoenchandle, pInBuffer[0]);
          } else {
            pInBuffer[1]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->videoenchandle, pInBuffer[1]);
          }
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
          }
        } else {
          err = OMX_FillThisBuffer(hComponent, pBuffer);
        }
      } 
    } else {
      DEBUG(DEFAULT_MESSAGES, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;  
}
/** Callbacks implementation of the video encoder component*/
OMX_ERRORTYPE videoencEventHandler(
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
      tsem_up(appPriv->encoderEventSem);
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->encoderEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "\n port settings change event handler in %s \n", __func__);
    
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEFAULT_MESSAGES, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return err; 
}

OMX_ERRORTYPE videoencEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  int data_read;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  if(!flagIsCameraRequested) {
    data_read = fread(pBuffer->pBuffer, sizeof(char), buffer_in_size, infile);
    if (data_read <= 0) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
      pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
      pBuffer->nFilledLen = 0;
      tsem_up(appPriv->eofSem);
      return OMX_ErrorNone;
    }
    pBuffer->nFilledLen = data_read;
    DEBUG(DEB_LEV_PARAMS, "Empty buffer %x\n", (int)pBuffer);
    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
  } else if(!bEOS){
    if(pSrcOutBuffer[0]->pBuffer == pBuffer->pBuffer) {
      err = OMX_FillThisBuffer(appPriv->videosrchandle, pSrcOutBuffer[0]);
    } else {
      err = OMX_FillThisBuffer(appPriv->videosrchandle, pSrcOutBuffer[1]);
    }
  } else {
    DEBUG(DEFAULT_MESSAGES, "In %s: Dropping Fill This Buffer\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE videoencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;

  if(pBuffer != NULL){
    if(!bEOS) {
      if((pBuffer->nFilledLen > 0)) {
          fwrite(pBuffer->pBuffer, sizeof(char),  pBuffer->nFilledLen, outfile);    
          pBuffer->nFilledLen = 0;
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEFAULT_MESSAGES, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      if(!bEOS ) {
        err = OMX_FillThisBuffer(hComponent, pBuffer);
      }
    } else {
      DEBUG(DEFAULT_MESSAGES, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;  
}


