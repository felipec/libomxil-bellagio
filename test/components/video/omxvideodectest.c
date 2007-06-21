/**
  @file test/components/video/omxvideodectest.c
	
  Test application that uses a OpenMAX component, a generic video decoder. 
  The application receives an video stream (.m4v or .264) decoded by a multiple format decoder component.
  The decoded output is seen by a yuv viewer.
	
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
	
	$Date$
	Revision $Rev$
	Author $Author$
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
#include <ctype.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>

#include "omxvideodectest.h"
#include "tsemaphore.h"

/** defining global declarations */
#define MPEG4_TYPE_SEL 1
#define AVC_TYPE_SEL 2
#define COMPONENT_NAME_BASE "OMX.st.video_decoder"
#define BASE_ROLE "video_decoder.avc"
#define COMPONENT_NAME_BASE_LEN 20

/** global variables */
OMX_COLOR_FORMATTYPE COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format24bitRGB888;

appPrivateType* appPriv;

unsigned int nextBuffer = 0;

/** used with video decoder */
OMX_BUFFERHEADERTYPE *pInBuffer1, *pInBuffer2, *pOutBuffer1, *pOutBuffer2;
/** used with color converter if selected */
OMX_BUFFERHEADERTYPE *pInBufferColorConv1, *pInBufferColorConv2,*pOutBufferColorConv1, *pOutBufferColorConv2;
OMX_BUFFERHEADERTYPE *pInBufferSink1, *pInBufferSink2;

int buffer_in_size = BUFFER_IN_SIZE;
OMX_U32 buffer_out_size;
OMX_U32 outbuf_colorconv_size;

OMX_PARAM_PORTDEFINITIONTYPE paramPort;
OMX_PARAM_PORTDEFINITIONTYPE omx_colorconvPortDefinition;
OMX_PARAM_COMPONENTROLETYPE paramRole;
OMX_VIDEO_PARAM_PORTFORMATTYPE omxVideoParam;

OMX_CALLBACKTYPE videodeccallbacks = { .EventHandler = videodecEventHandler,
    .EmptyBufferDone = videodecEmptyBufferDone,
    .FillBufferDone = videodecFillBufferDone
    };

OMX_CALLBACKTYPE colorconv_callbacks = { .EventHandler = colorconvEventHandler,
	  .EmptyBufferDone = colorconvEmptyBufferDone,
	  .FillBufferDone = colorconvFillBufferDone
    };

OMX_CALLBACKTYPE fbdev_sink_callbacks = { .EventHandler = fb_sinkEventHandler,
	  .EmptyBufferDone = fb_sinkEmptyBufferDone,
	  .FillBufferDone = NULL
    };


OMX_U32 out_width = 0, new_out_width = 0;
OMX_U32 out_height = 0, new_out_height = 0;

FILE *fd;

appPrivateType* appPriv;

char *input_file, *output_file;

int selectedType = 0;
FILE *outfile;

int flagIsOutputEspected;
int flagDecodedOutputReceived;
int flagInputReceived;
int flagIsColorConvRequested;
int flagIsSinkRequested;
int flagIsFormatRequested;
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


/** this function sets the color converter and video sink port characteristics 
  * based on the video decoder output port settings 
  */
int setPortParameters() {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  paramPort.nPortIndex = 1;
  setHeader(&paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videodechandle, OMX_IndexParamPortDefinition, &paramPort);
  new_out_width = paramPort.format.video.nFrameWidth;
  new_out_height = paramPort.format.video.nFrameHeight;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "input picture width : %d height : %d \n", (int)new_out_width, (int)new_out_height);

  /** setting the color converter and sink component chararacteristics, if selected - 
    * both in tunneled as well as non tunneled case 
    */
  if(flagIsColorConvRequested == 1) {
    /** setting the color conv input port width, height 
      * it will be same as the video decoder output port width, height 
      */
    omx_colorconvPortDefinition.nPortIndex = 0;
    setHeader(&omx_colorconvPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);	
    omx_colorconvPortDefinition.format.video.nFrameWidth = new_out_width;
    omx_colorconvPortDefinition.format.video.nFrameHeight = new_out_height;
    err = OMX_SetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
      return err; 
    } 
    /** setting the color converter output width height 
      * it will be same as input dimensions 
      */
    omx_colorconvPortDefinition.nPortIndex = 1;
    err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
    omx_colorconvPortDefinition.format.video.nFrameWidth = new_out_width;
    omx_colorconvPortDefinition.format.video.nFrameHeight = new_out_height;
    err = OMX_SetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Setting Output Port Definition Error=%x\n",__func__,err); 
      return err; 
    } 
    /** setting the input color format of color converter component 
      * it will be same as output yuv color format of the decoder component 
      */
    paramPort.nPortIndex = 1;
    setHeader(&paramPort, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    err = OMX_GetParameter(appPriv->videodechandle, OMX_IndexParamPortDefinition, &paramPort);
    omxVideoParam.nPortIndex = 0;
    setHeader(&omxVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);		 
    omxVideoParam.eColorFormat = paramPort.format.video.eColorFormat;	
    err = OMX_SetParameter(appPriv->colorconv_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);
    if(err==OMX_ErrorBadParameter) {	
      DEBUG(DEB_LEV_ERR,"\n bad parameter of input color format - exiting\n");
      exit(1);
    }
    /** setting output RGB color format of the color converter component */
    omxVideoParam.nPortIndex = 1;
    err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);
    omxVideoParam.eColorFormat = COLOR_CONV_OUT_RGB_FORMAT;
    err = OMX_SetParameter(appPriv->colorconv_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);
    if(err==OMX_ErrorBadParameter) {	
      DEBUG(DEB_LEV_ERR,"\n bad parameter of output color format setting- exiting\n");
      exit(1);
    }
    /** if video sink component is selected then set its input port settings 
      *  accroding to the output port settings of the color converter component  
      */
    if(flagIsSinkRequested == 1) {
      omx_colorconvPortDefinition.nPortIndex = 1; //color converter output port index
      err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
      omx_colorconvPortDefinition.nPortIndex = 0; //sink input port index
      err = OMX_SetParameter(appPriv->fbdev_sink_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
      if(err != OMX_ErrorNone) {	
        DEBUG(DEB_LEV_ERR,"\n error in setting the inputport param of the sink component- exiting\n");
        exit(1);
      }
      omxVideoParam.nPortIndex = 1; //color converter output port index
      err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);
      omxVideoParam.nPortIndex = 0; //sink input port index
      err = OMX_SetParameter(appPriv->fbdev_sink_handle, OMX_IndexParamVideoPortFormat, &omxVideoParam);
      if(err != OMX_ErrorNone) {	
        DEBUG(DEB_LEV_ERR,"\n error in setting the input video param of the sink component- exiting\n");
        exit(1);
      }
    }
  }

  return err;
}

/**	Try to determine resolution based on filename */
int find_resolution(char* searchname) {
  char filename_lower[255];
  char* ptr = filename_lower;
  
  strcpy(filename_lower, searchname);
  while (*ptr != '\0' && ptr - filename_lower < 255) {
    *ptr = tolower(*ptr);
    ++ptr;
  }
  if (strstr(filename_lower, "qcif") != NULL) {
    out_width = 176;
    out_height = 144;
    return 1;
  } else if (strstr(filename_lower, "qvga") != NULL) {
    out_width = 320;
    out_height = 240;
    return 2;
  } else if (strstr(filename_lower, "cif") != NULL) {
    out_width = 352;
    out_height = 288;
    return 3;
  } else if (strstr(filename_lower, "vga") != NULL) {
    out_width = 640;
    out_height = 480;
    return 4;
  }
  return 0;
}

/** help display */
void display_help() {
  printf("\n");
  printf("Usage: omxvideodectest -o outfile [-t] [-c] [-h] [-f input_fmt] [-s] input_filename\n");
  printf("\n");
  printf("       -o outfile: If this option is specified, the output is written to user specified outfile\n");
  printf("                   Else, the output is written in the same directory of input file\n");
  printf("                     the file name looks like input_filename_app.yuv/rgb depending on input option\n");
  printf("                   If the color conv option (-c) is specified then outfile will be .rgb file\n");
  printf("                   Else outfile will be in .yuv format \n");
  printf("                   N.B : This option is not needed if you use the sink component\n");
  printf("\n");
  printf("       -c : Color conv option - input file is decoded and color converted in outfile(.rgb file)\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  printf("       -f : input format specification in case of color conv comp usage \n");
  printf("            The available input formats are - \n");
  printf("              - OMX_COLOR_Format24bitRGB888  (default format) \n");
  printf("              - OMX_COLOR_Format24bitBGR888  \n");
  printf("              - OMX_COLOR_Format32bitBGRA8888  \n");
  printf("              - OMX_COLOR_Format32bitARGB8888  \n");
  printf("              - OMX_COLOR_Format16bitARGB1555  \n");
  printf("              - OMX_COLOR_Format16bitRGB565  \n");
  printf("              - OMX_COLOR_Format16bitBGR565  \n");
  printf("\n");
  printf("       -s: Uses the video sink component to display the output of the color converter(.rgb file)\n");
  printf("       input_filename is the user specified input file name\n");
  printf("\n");
  printf("       -t: Tunneling option - if this option is selected then by default the color converter and \n");
  printf("           video sink components are selected even if those two options are not specified - \n");
  printf("           the components are tunneled between themselves\n");
  printf("\n");
  exit(1);
}


OMX_ERRORTYPE test_OMX_ComponentNameEnum() {
  char * name;
  int index;

  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n", __func__);
  name = malloc(128 * sizeof(char));
  index = 0;
  while(1) {
    err = OMX_ComponentNameEnum (name, 128, index);
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
  }	else {
    string_of_roles = (OMX_U8**)malloc(no_of_roles * sizeof(OMX_STRING));
    for (index = 0; index < no_of_roles; index++) {
      *(string_of_roles + index) = (OMX_U8 *)malloc(no_of_roles * 128);
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
    string_of_comp_per_role[index] = malloc(128 * sizeof(char));
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

  if(string_of_comp_per_role)	{
    free(string_of_comp_per_role);
    string_of_comp_per_role = NULL;
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result OMX_ErrorNone\n", __func__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE test_OpenClose(OMX_STRING component_name) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
  err = OMX_GetHandle(&appPriv->videodechandle, component_name, NULL /*appPriv */, &videodeccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No component found\n");
  } else {
    err = OMX_FreeHandle(appPriv->videodechandle);
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
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
    flagIsOutputEspected = 0;
    flagDecodedOutputReceived = 0;
    flagInputReceived = 0;
    flagIsColorConvRequested = 0;
    flagSetupTunnel = 0;
    flagIsSinkRequested = 0;
    flagIsFormatRequested = 0;

    argn_dec = 1;
    while (argn_dec < argc) {
      if (*(argv[argn_dec]) == '-') {
        if (flagIsOutputEspected) {
          display_help();
        }
        switch (*(argv[argn_dec] + 1)) {
          case 'h' :
            display_help();
            break;
          case 't' :
            flagSetupTunnel = 1;
            flagIsSinkRequested = 1;
            flagIsColorConvRequested = 1;
            break;
          case 's':
            flagIsSinkRequested = 1;
            break;
          case 'o':
            flagIsOutputEspected = 1;
            break;
          case 'c':
            flagIsColorConvRequested = 1;
            break;
          case 'f' :
            flagIsFormatRequested = 1;
            break;
          default:
            display_help();
        }
      } else {
        if (flagIsOutputEspected) {
          if(strstr(argv[argn_dec], ".yuv") == NULL && strstr(argv[argn_dec], ".rgb") == NULL) {
            output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 5);
            strcpy(output_file,argv[argn_dec]);
            strcat(output_file, ".yuv");
          } else {
            output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
            strcpy(output_file,argv[argn_dec]);
          }          
          flagIsOutputEspected = 0;
          flagDecodedOutputReceived = 1;
        } else if(flagIsFormatRequested) {
          if(strstr(argv[argn_dec], "OMX_COLOR_Format24bitRGB888") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format24bitRGB888;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format24bitBGR888") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format24bitBGR888;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format32bitBGRA8888") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format32bitBGRA8888;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format32bitARGB8888") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format32bitARGB8888;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format16bitARGB1555") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format16bitARGB1555;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format16bitRGB565") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format16bitRGB565;
          } else if(strstr(argv[argn_dec], "OMX_COLOR_Format16bitBGR565") != NULL) {
            COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format16bitBGR565;
          } else {
            DEBUG(DEB_LEV_ERR, "\n specified color converter format is unknown - keeping default format\n");
          }
          flagIsFormatRequested = 0;
        } else {
          input_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
          strcpy(input_file,argv[argn_dec]);
          flagInputReceived = 1;
        }
      }
      argn_dec++;
    }

    /** input file name check */
    if ((!flagInputReceived) || ((strstr(input_file, ".m4v") == NULL) && (strstr(input_file, ".264") == NULL))) {
      DEBUG(DEB_LEV_ERR, "\n you must specify appropriate input file of .m4v or .264 format\n");
      display_help();
    }

    /** output file name check */
    //case 1 - user did not specify any output file
    if(!flagIsOutputEspected && !flagDecodedOutputReceived) {
      if(flagIsColorConvRequested) {
        if(!flagIsSinkRequested) {
          DEBUG(DEB_LEV_ERR,"\n you did not enter any output file name and entered color converter option");
          output_file = malloc( (strlen(input_file) - strlen(strstr(input_file,".")) + 8) * sizeof(char) + 1);                                                                  
          strncpy(output_file,input_file, (strlen(input_file) - strlen(strstr(input_file,"."))) );
          strcat(output_file,"_app.rgb");
          DEBUG(DEB_LEV_ERR,"\n the output file name will be %s ", output_file);
        }
      } else {
        DEBUG(DEB_LEV_ERR,"\n you did not enter any output file name");
        output_file = malloc( (strlen(input_file) - strlen(strstr(input_file,".")) + 8) * sizeof(char) + 1);                                                                  
        strncpy(output_file,input_file, (strlen(input_file) - strlen(strstr(input_file,"."))) );
        strcat(output_file,"_app.yuv");
        DEBUG(DEB_LEV_ERR,"\n the decoded output file name will be %s ", output_file);
      }
    } else if(flagDecodedOutputReceived) {
      //case 2 - user has given wrong format
      if(flagIsColorConvRequested && strstr(output_file, ".rgb") == NULL) {
        output_file[strlen(output_file) - strlen(strstr(output_file, "."))] = '\0';
        strcat(output_file, ".rgb");
        DEBUG(DEB_LEV_ERR,"\n color conv option is selected - so the output file is %s \n", output_file);
      }else if(!flagIsColorConvRequested && strstr(output_file, ".yuv") == NULL) {
        output_file[strlen(output_file) - strlen(strstr(output_file, "."))] = '\0';
        strcat(output_file, ".yuv");
        DEBUG(DEB_LEV_ERR,"\n color conv option is not selected - so the output file is %s \n", output_file);
      }
    }

    /** if color converter component is not selected then sink component will not work, even if specified */
    if(!flagIsColorConvRequested && flagIsSinkRequested) {
      DEBUG(DEB_LEV_ERR, "\n you are not using color converter component -  so sink component will not be used\n");
      flagIsSinkRequested = 0;
    }

    /** determine the input file format in case of decoding or color converting */
    if((strstr(input_file, ".m4v") != NULL)) {
      selectedType = MPEG4_TYPE_SEL;
    } else if((strstr(input_file, ".264") != NULL)) {
      selectedType = AVC_TYPE_SEL;
    } else {
      DEBUG(DEB_LEV_ERR, "\n input file format and operation not supported \n");
      display_help();
    }


    DEBUG(DEFAULT_MESSAGES, "Options selected:\n");
    if(flagIsColorConvRequested == 0) {
      DEBUG(DEFAULT_MESSAGES, "Decode file %s to produce file %s\n", input_file, output_file);
    } else if(!flagIsSinkRequested){
      DEBUG(DEFAULT_MESSAGES, "Decode file followed by Color convert file %s to produce file %s\n", input_file, output_file);
    } else {
      DEBUG(DEFAULT_MESSAGES, "As sink component works, so there is no output file storage even if you specified\n");
    }
    if(flagIsSinkRequested) {
      DEBUG(DEFAULT_MESSAGES, "The color converter output will be displayed in the video sink\n");
    }
    if(flagSetupTunnel) {
      DEBUG(DEFAULT_MESSAGES,"The components are tunneled between themselves\n");
    }
  }

  fd = fopen(input_file, "rb");
  if(fd == NULL) {
    DEBUG(DEB_LEV_ERR, "Error in opening input file %s\n", input_file);
    exit(1);
  }

  if(!flagIsSinkRequested) {
    outfile = fopen(output_file, "wb");
    if(outfile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error in opening output file %s\n", output_file);
      exit(1);
    }
  }

  /** setting input picture width to a default value (vga format) for allocation of video decoder buffers */
  out_width = 640;    
  out_height = 480;   

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));	
  appPriv->decoderEventSem = malloc(sizeof(tsem_t));
  if(flagIsColorConvRequested == 1) {
    if(flagIsSinkRequested == 1) {
      appPriv->fbdevSinkEventSem = malloc(sizeof(tsem_t));
    }
 	  appPriv->colorconvEventSem = malloc(sizeof(tsem_t));
  }
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->decoderEventSem, 0);
  if(flagIsColorConvRequested == 1) {
    if(flagIsSinkRequested == 1) {
      tsem_init(appPriv->fbdevSinkEventSem, 0);
    }
 	  tsem_init(appPriv->colorconvEventSem, 0);
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
  

  full_component_name = (OMX_STRING) malloc(sizeof(char*) * OMX_MAX_STRINGNAME_SIZE);
  strcpy(full_component_name, COMPONENT_NAME_BASE);
  if(selectedType == MPEG4_TYPE_SEL) {
    strcpy(full_component_name + COMPONENT_NAME_BASE_LEN, ".mpeg4");
  } else if (selectedType == AVC_TYPE_SEL) {
    strcpy(full_component_name + COMPONENT_NAME_BASE_LEN, ".avc");
  } 

  DEBUG(DEFAULT_MESSAGES, "The component selected for decoding is %s\n", full_component_name);

  /** getting video decoder handle */
  err = OMX_GetHandle(&appPriv->videodechandle, full_component_name, NULL, &videodeccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No video decoder component found. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "Found The component for decoding is %s\n", full_component_name);
  }

  /** getting color converter component handle, if specified */
  if(flagIsColorConvRequested == 1) {
    err = OMX_GetHandle(&appPriv->colorconv_handle, "OMX.st.video_colorconv", NULL, &colorconv_callbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No color converter component found. Exiting...\n");
      exit(1);
    } else {
      DEBUG(DEFAULT_MESSAGES, "Found The component for color converter \n");
    }

    /** getting sink component handle - if reqd' */
    if(flagIsSinkRequested == 1) {
      err = OMX_GetHandle(&appPriv->fbdev_sink_handle, "OMX.st.fbdev.fbdev_sink", NULL, &fbdev_sink_callbacks);
      if(err != OMX_ErrorNone){
        DEBUG(DEB_LEV_ERR, "No video sink component component found. Exiting...\n");
        exit(1);
      } else {
        DEBUG(DEFAULT_MESSAGES, "Found The video sink component for color converter \n");
      }
    }
  }

  setHeader(&paramRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
  err = OMX_GetParameter(appPriv->videodechandle, OMX_IndexParamStandardComponentRole, &paramRole);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The role set for this component can not be retrieved err = %i\n", err);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "The role currently set is %s\n", paramRole.cRole);

  /** output buffer size calculation based on input dimension speculation */
  buffer_out_size = out_width * out_height * 1.5; //yuv420 format -- bpp = 12
  DEBUG(DEB_LEV_SIMPLE_SEQ, "\n buffer_out_size : %d \n", (int)buffer_out_size);

  /** if tunneling option is given then set up the tunnel between the components */
  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    err = OMX_SetupTunnel(appPriv->videodechandle, 1, appPriv->colorconv_handle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between video dec & color conv Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->colorconv_handle, 1, appPriv->fbdev_sink_handle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between color conv & video sink Failed\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES, "Set up Tunnel Completed\n");
  }  


  /** sending command to video decoder component to go to idle state */
  pInBuffer1 = pInBuffer2 = NULL;
  err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  /** in tunnel case, change the color conv and sink comp state to idle */
  if(flagIsColorConvRequested && flagSetupTunnel) {
    err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(flagIsSinkRequested && flagSetupTunnel) {
      err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
  }

  err = OMX_AllocateBuffer(appPriv->videodechandle, &pInBuffer1, 0, NULL, buffer_in_size);
  err = OMX_AllocateBuffer(appPriv->videodechandle, &pInBuffer2, 0, NULL, buffer_in_size);

  if(flagSetupTunnel) {
    if(flagIsSinkRequested) {
      tsem_down(appPriv->fbdevSinkEventSem);
    }
    if(flagIsColorConvRequested) {
      tsem_down(appPriv->colorconvEventSem);
    }
  }

  /** if tunneling option is not given then allocate buffers on video decoder output port */
  if (!flagSetupTunnel) {
    pOutBuffer1 = pOutBuffer2 = NULL;
    err = OMX_AllocateBuffer(appPriv->videodechandle, &pOutBuffer1, 1, NULL, buffer_out_size);
    err = OMX_AllocateBuffer(appPriv->videodechandle, &pOutBuffer2, 1, NULL, buffer_out_size);
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
  tsem_down(appPriv->decoderEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "decoder Sem free\n");
  
  /** sending command to video decoder component to go to executing state */
  err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  tsem_down(appPriv->decoderEventSem);

  if (!flagSetupTunnel) {
    err = OMX_FillThisBuffer(appPriv->videodechandle, pOutBuffer1);
    err = OMX_FillThisBuffer(appPriv->videodechandle, pOutBuffer2);
  }


  DEBUG(DEB_LEV_SIMPLE_SEQ, "---> Before locking on condition and decoderMutex\n");

  data_read = fread(pInBuffer1->pBuffer, sizeof(char), buffer_in_size, fd);
  pInBuffer1->nFilledLen = data_read;
  pInBuffer1->nOffset = 0;

  /** in non tunneled case use the 2nd input buffer for input read and procesing
    * in tunneled case, it will be used afterwards
    */
  if(!flagSetupTunnel) {
    data_read = fread(pInBuffer2->pBuffer, sizeof(char), buffer_in_size, fd);
    pInBuffer2->nFilledLen = data_read;
    pInBuffer2->nOffset = 0;
  }
  
  DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)pInBuffer1->pBuffer);
  err = OMX_EmptyThisBuffer(appPriv->videodechandle, pInBuffer1);

  if(!flagSetupTunnel) {
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)pInBuffer2->pBuffer);
    err = OMX_EmptyThisBuffer(appPriv->videodechandle, pInBuffer2);
  }

  DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS\n");

  /** in tunneled case, disable the video components ports and then set parameters 
    * after that, ebnable those ports
    */
  if(flagSetupTunnel) {
    /** this is the signal from port settings change event - 
      * wait for the dummy up signal from port settings change event of the video decoder
      * the signal is called in every execution
      */
    tsem_down(appPriv->decoderEventSem);
    /** disable the ports */
    if(flagIsColorConvRequested) {
      err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandPortDisable, -1, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"color conv input port disable failed\n");
			  exit(1);
      }
      if(flagIsSinkRequested) {
        err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandPortDisable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"video sink input port disable failed\n");
			    exit(1);
        }
      }
    }

    /** wait for port disable events to be finished */
    if(flagIsColorConvRequested) {
      if(flagIsSinkRequested) {
        tsem_down(appPriv->fbdevSinkEventSem);
      }
      tsem_down(appPriv->colorconvEventSem);
    }
    tsem_down(appPriv->decoderEventSem); // for video decoder output port disable to complete
    DEBUG(DEB_LEV_SIMPLE_SEQ,"All ports are disabled in %s\n", __func__);

    /** now set the port characteristics */
    setPortParameters();

    /** enable the previously disabled ports */
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Enable Command\n");
    err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandPortEnable, 1, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"video decoder port enable failed\n");
			exit(1);
    }
    if(flagIsColorConvRequested) {
      err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandPortEnable, -1, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"color conv input port enable failed\n");
			  exit(1);
      }
      if(flagIsSinkRequested) {
        err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandPortEnable, 0, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"video sink input port enable failed\n");
			    exit(1);
        }
      }
    }
    /** wait for port disable events to be finished */
    if(flagIsColorConvRequested) {
      if(flagIsSinkRequested) {
        tsem_down(appPriv->fbdevSinkEventSem);
      }
      tsem_down(appPriv->colorconvEventSem);
    }
    tsem_down(appPriv->decoderEventSem);
    DEBUG(DEB_LEV_SIMPLE_SEQ,"All ports are enabled in %s\n", __func__);

    /** send command to change color onv and sink comp in executing state */
    if(flagIsColorConvRequested == 1) {
      err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
      tsem_down(appPriv->colorconvEventSem);
      if(flagIsSinkRequested == 1) {    
        err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        tsem_down(appPriv->fbdevSinkEventSem);
      }
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ, "in %s in tunneled case, after all components in executing state\n", __func__);

    /** activate 2nd input buffer now */
   data_read = fread(pInBuffer2->pBuffer, sizeof(char), buffer_in_size, fd);
    pInBuffer2->nFilledLen = data_read;
    pInBuffer2->nOffset = 0;
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)pInBuffer2->pBuffer);
    err = OMX_EmptyThisBuffer(appPriv->videodechandle, pInBuffer2);
  }
  tsem_down(appPriv->eofSem);

  DEBUG(DEFAULT_MESSAGES, "The execution of the video decoding process is terminated\n");

  /** state change of all components from executing to idle */
  err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if(flagIsColorConvRequested == 1) {
    err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(flagIsSinkRequested == 1) {
      err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
  }

  tsem_down(appPriv->decoderEventSem);
  if(flagIsColorConvRequested == 1) {
    tsem_down(appPriv->colorconvEventSem);
    if(flagIsSinkRequested == 1) {
      tsem_down(appPriv->fbdevSinkEventSem);
    }
  }

  DEBUG(DEFAULT_MESSAGES, "All video components Transitioned to Idle\n");

  /** sending command to all components to go to loaded state */
  err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if(flagIsColorConvRequested == 1) {
    err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if(flagIsSinkRequested == 1) {
      err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    }
  }

  /** freeing buffers of color conv and sink component */
  if(flagIsColorConvRequested == 1 && !flagSetupTunnel) {
    if(flagIsSinkRequested == 1 && !flagSetupTunnel) {
      DEBUG(DEB_LEV_PARAMS, "Video sink to loaded\n");
      err = OMX_FreeBuffer(appPriv->fbdev_sink_handle, 0, pOutBufferColorConv1);
      err = OMX_FreeBuffer(appPriv->fbdev_sink_handle, 0, pOutBufferColorConv1);
    }
    DEBUG(DEB_LEV_PARAMS, "Color conv to loaded\n");
    err = OMX_FreeBuffer(appPriv->colorconv_handle, 0, pOutBuffer1);
    err = OMX_FreeBuffer(appPriv->colorconv_handle, 0, pOutBuffer2);
  
    err = OMX_FreeBuffer(appPriv->colorconv_handle, 1, pOutBufferColorConv1);
    err = OMX_FreeBuffer(appPriv->colorconv_handle, 1, pOutBufferColorConv2);
  }

  /** freeing buffers of video decoder input ports */
  DEBUG(DEB_LEV_PARAMS, "Video dec to loaded\n");
  err = OMX_FreeBuffer(appPriv->videodechandle, 0, pInBuffer1);
  err = OMX_FreeBuffer(appPriv->videodechandle, 0, pInBuffer2);

  if(!flagSetupTunnel) {
    DEBUG(DEB_LEV_PARAMS, "Free Video dec output ports\n");
    err = OMX_FreeBuffer(appPriv->videodechandle, 1, pOutBuffer1);
    err = OMX_FreeBuffer(appPriv->videodechandle, 1, pOutBuffer2);
  }

  if(flagIsColorConvRequested == 1) {
    if(flagIsSinkRequested == 1) {
      tsem_down(appPriv->fbdevSinkEventSem);
    }
    tsem_down(appPriv->colorconvEventSem);
  }
  tsem_down(appPriv->decoderEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");

  OMX_FreeHandle(appPriv->videodechandle);
  if(flagIsColorConvRequested == 1) {
    if(flagIsSinkRequested == 1) {
      OMX_FreeHandle(appPriv->fbdev_sink_handle);
    }
    OMX_FreeHandle(appPriv->colorconv_handle);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "video dec freed\n");

  OMX_Deinit();

  DEBUG(DEFAULT_MESSAGES, "All components freed. Closing...\n");
  free(appPriv->decoderEventSem);
  if(flagIsColorConvRequested == 1) {
    if(flagIsSinkRequested == 1) {
      free(appPriv->fbdevSinkEventSem);
    }
    free(appPriv->colorconvEventSem);
  }
  free(appPriv->eofSem);
  free(appPriv);

  /** closing the output file */
  if(!flagIsSinkRequested) {
    fclose(outfile);
  }
  /** closing the input file */
  fclose(fd);

  return 0;
}


/** callbacks implementation of video sink component */
OMX_ERRORTYPE fb_sinkEventHandler(
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
      tsem_up(appPriv->fbdevSinkEventSem);
    }		  
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->fbdevSinkEventSem);
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


OMX_ERRORTYPE fb_sinkEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int inputBufferDropped = 0;
  if(pBuffer != NULL) {
    if(!bEOS) {
      if(pOutBufferColorConv1->pBuffer == pBuffer->pBuffer) {
        pOutBufferColorConv1->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->colorconv_handle, pOutBufferColorConv1);
      } else {
        pOutBufferColorConv2->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->colorconv_handle, pOutBufferColorConv2);
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


/* Callbacks implementation of the color conv component */
OMX_ERRORTYPE colorconvEventHandler(
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
      tsem_up(appPriv->colorconvEventSem);
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->colorconvEventSem);
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


OMX_ERRORTYPE colorconvEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int iBufferDropped = 0;

  if(pBuffer != NULL) {
    if(!bEOS) {
      if(pOutBuffer1->pBuffer == pBuffer->pBuffer) {
        pOutBuffer1->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->videodechandle, pOutBuffer1);
      } else {
        pOutBuffer2->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->videodechandle, pOutBuffer2);
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


OMX_ERRORTYPE colorconvFillBufferDone(
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
        if(pInBufferSink1->pBuffer == pBuffer->pBuffer) {
          pInBufferSink1->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->fbdev_sink_handle, pInBufferSink1);
        } else {
          pInBufferSink2->nFilledLen = pBuffer->nFilledLen;
          err = OMX_EmptyThisBuffer(appPriv->fbdev_sink_handle, pInBufferSink2);
        }
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
        }
      } else if((pBuffer->nFilledLen > 0) && (!flagSetupTunnel)) {
          fwrite(pBuffer->pBuffer, sizeof(char),  pBuffer->nFilledLen, outfile);		
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



/** Callbacks implementation of the video decoder component*/
OMX_ERRORTYPE videodecEventHandler(
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
      tsem_up(appPriv->decoderEventSem);
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->decoderEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "\n port settings change event handler in %s \n", __func__);
    if(Data2 == 1) {

      /** before setting port parameters , first check if tunneled case 
        * if so, then disable the output port of video decoder
        * and generate dummy up signal, so that in main thread
        * the color conv and sink comp ports are disabled and 
        * port parameter settings changes are done properly
        */
      if(flagSetupTunnel) {
        DEBUG(DEB_LEV_SIMPLE_SEQ,"Sending Port Disable Command\n");
        err = OMX_SendCommand(appPriv->videodechandle, OMX_CommandPortDisable, 1, NULL);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"video decoder port disable failed\n");
			    exit(1);
        }
        // dummy up signal - caught in main thread to resume processing from there
        tsem_up(appPriv->decoderEventSem);
      }

      /** in non tunneled case, if color converter and sink component are selected 
        * then set their port parameters according to input tream characteristics and 
        * send command to them to go to idle state and executing state
        */
      if(!flagSetupTunnel) {
        setPortParameters();
        if(flagIsColorConvRequested == 1) {
          pOutBufferColorConv1 = pOutBufferColorConv2 = NULL;
          err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

          /** in non tunneled case, using buffers in color conv input port, allocated by video dec component output port */
          err = OMX_UseBuffer(appPriv->colorconv_handle, &pInBufferColorConv1, 0, NULL, buffer_out_size, pOutBuffer1->pBuffer);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to use the video dec comp allocate buffer\n");
            exit(1);
          }
          err = OMX_UseBuffer(appPriv->colorconv_handle, &pInBufferColorConv2, 0, NULL, buffer_out_size, pOutBuffer2->pBuffer);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to use the video dec comp allocate buffer\n");
            exit(1);
          }

          /** allocating buffers in the color converter compoennt output port */
          omx_colorconvPortDefinition.nPortIndex = 1;
          setHeader(&omx_colorconvPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
          err = OMX_GetParameter(appPriv->colorconv_handle, OMX_IndexParamPortDefinition, &omx_colorconvPortDefinition);
          outbuf_colorconv_size = omx_colorconvPortDefinition.nBufferSize;
          DEBUG(DEB_LEV_SIMPLE_SEQ, " outbuf_colorconv_size : %d \n", (int)outbuf_colorconv_size);

          err = OMX_AllocateBuffer(appPriv->colorconv_handle, &pOutBufferColorConv1, 1, NULL, outbuf_colorconv_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in color conv\n");
            exit(1);
          }
          err = OMX_AllocateBuffer(appPriv->colorconv_handle, &pOutBufferColorConv2, 1, NULL, outbuf_colorconv_size);
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in colro conv\n");
            exit(1);
          }

          DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
          tsem_down(appPriv->colorconvEventSem);
          DEBUG(DEB_LEV_SIMPLE_SEQ, "color conv Sem free\n");

          if(flagIsSinkRequested == 1) {
            err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
            err = OMX_UseBuffer(appPriv->fbdev_sink_handle, &pInBufferSink1, 0, NULL, outbuf_colorconv_size, pOutBufferColorConv1->pBuffer);
            if(err != OMX_ErrorNone) {
              DEBUG(DEB_LEV_ERR, "Unable to use the color conv comp allocate buffer\n");
              exit(1);
            }
            err = OMX_UseBuffer(appPriv->fbdev_sink_handle, &pInBufferSink2, 0, NULL, outbuf_colorconv_size, pOutBufferColorConv2->pBuffer);
            if(err != OMX_ErrorNone) {
              DEBUG(DEB_LEV_ERR, "Unable to use the color conv comp allocate buffer\n");
              exit(1);
            }

            DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
            tsem_down(appPriv->fbdevSinkEventSem);
            DEBUG(DEB_LEV_SIMPLE_SEQ, "video sink comp Sem free\n");
          }
        }

        if(flagIsColorConvRequested == 1) {
          err = OMX_SendCommand(appPriv->colorconv_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
          tsem_down(appPriv->colorconvEventSem);
          if(flagIsSinkRequested == 1) {    
            err = OMX_SendCommand(appPriv->fbdev_sink_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            tsem_down(appPriv->fbdevSinkEventSem);
          }
        }

        if(flagIsColorConvRequested == 1 ) { 
          err = OMX_FillThisBuffer(appPriv->colorconv_handle, pOutBufferColorConv1);
          err = OMX_FillThisBuffer(appPriv->colorconv_handle, pOutBufferColorConv2);
          DEBUG(DEB_LEV_SIMPLE_SEQ, "---> After fill this buffer function calls to the color conv output buffers\n");
        }
      }
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
  return err; 
}

OMX_ERRORTYPE videodecEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  int data_read;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  data_read = fread(pBuffer->pBuffer, sizeof(char), buffer_in_size, fd);
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
  return OMX_ErrorNone;
}


OMX_ERRORTYPE videodecFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  OMX_STATETYPE eState;

  if(pBuffer != NULL){
    if(!bEOS) {
      /** if there is color conv component in processing state then send this buffer, in non tunneled case 
        * else in non tunneled case, write the output buffer contents in the specified output file
        */
      if(flagIsColorConvRequested && (!flagSetupTunnel)) {
        OMX_GetState(appPriv->colorconv_handle,&eState);
        if(eState == OMX_StateExecuting || eState == OMX_StatePause) {
          if(pInBufferColorConv1->pBuffer == pBuffer->pBuffer) {
            pInBufferColorConv1->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->colorconv_handle, pInBufferColorConv1);
          } else {
            pInBufferColorConv2->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->colorconv_handle, pInBufferColorConv2);
          }
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
          }
        } else {
          err = OMX_FillThisBuffer(hComponent, pBuffer);
        }
      } else if((pBuffer->nFilledLen > 0) && (!flagSetupTunnel)) {
          fwrite(pBuffer->pBuffer, sizeof(char),  pBuffer->nFilledLen, outfile);		
          pBuffer->nFilledLen = 0;
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      if(!bEOS && !flagIsColorConvRequested && (!flagSetupTunnel)) {
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


