/**
  @file test/components/video/omxvideocapturetest.c
  
  Test application that uses a OpenMAX component, a generic video videosrc. 
  The application receives an video stream (.yuv) captured using a camera.
  The capture picture is then stored into a file, which can be seen by a yuv viewer.
  
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
  
  $Date: 2008-02-13 16:34:49 +0100 (Wed, 13 Feb 2008) $
  Revision $Rev: 1304 $
  Author $Author: giulio_urlini $
*/

#include "omxvideocapturetest.h"

/** defining global declarations */
#define COMPONENT_NAME "OMX.st.video_src"

/** global variables */
OMX_COLOR_FORMATTYPE COLOR_CONV_OUT_RGB_FORMAT = OMX_COLOR_Format24bitRGB888;

/** used with video videosrc */
OMX_BUFFERHEADERTYPE  *pOutBuffer1, *pOutBuffer2;

OMX_U32 buffer_out_size;

OMX_PARAM_PORTDEFINITIONTYPE paramPort;
OMX_VIDEO_PARAM_PORTFORMATTYPE omxVideoParam;

OMX_CALLBACKTYPE videosrccallbacks = { .EventHandler = videosrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone = videosrcFillBufferDone
  };

OMX_U32 out_width = 0;
OMX_U32 out_height = 0;

appPrivateType* appPriv;

char *output_file;
FILE *outfile;

int flagIsOutputExpected;
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
  printf("Usage: omxvideocapturetest -o outfile\n");
  printf("\n");
  printf("    -o outfile: outfile, where captured unencoded raw yuv data will be stored\n");
  printf("\n");
  exit(1);
}

OMX_ERRORTYPE test_OpenClose(OMX_STRING component_name) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s\n",__func__);
  err = OMX_GetHandle(&appPriv->videosrchandle, component_name, NULL , &videosrccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No component found\n");
  } else {
    err = OMX_FreeHandle(appPriv->videosrchandle);
  }
  DEBUG(DEFAULT_MESSAGES, "GENERAL TEST %s result %i\n", __func__, err);
  return err;
}

int main(int argc, char** argv) {

  OMX_ERRORTYPE err;
  int argn_dec;
  OMX_STRING full_component_name;

  if(argc < 2){
    display_help();
  } else {
    flagIsOutputExpected = 0;
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
          case 'o':
            flagIsOutputExpected = 1;
            break;
          default:
            display_help();
        }
      } else {
        if (flagIsOutputExpected) {
          if(strstr(argv[argn_dec], ".yuv") == NULL && strstr(argv[argn_dec], ".rgb") == NULL) {
            output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 5);
            strcpy(output_file,argv[argn_dec]);
            strcat(output_file, ".rgb");
          } else {
            output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
            strcpy(output_file,argv[argn_dec]);
          }          
          flagIsOutputExpected = 0;
        } 
      }
      argn_dec++;
    }
  }

  if(output_file==NULL) {
    output_file = malloc(30);
    strcpy(output_file,"default_camera_out.rgb");
  }
 
  outfile = fopen(output_file, "wb");
  if(outfile == NULL) {
    DEBUG(DEB_LEV_ERR, "Error in opening output file %s\n", output_file);
    exit(1);
  }
  
  /** setting input picture width to a default value (vga format) for allocation of video videosrc buffers */
  out_width = 176;    
  out_height = 144;   

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));  
  appPriv->videosrcEventSem = malloc(sizeof(tsem_t));
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->videosrcEventSem, 0);
  tsem_init(appPriv->eofSem, 0);
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Init the Omx core\n");
  err = OMX_Init();
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Omx core is initialized \n");
  }
  
  test_OpenClose(COMPONENT_NAME);
  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");
  

  full_component_name = (OMX_STRING) malloc(sizeof(char*) * OMX_MAX_STRINGNAME_SIZE);
  strcpy(full_component_name, COMPONENT_NAME);
  

  DEBUG(DEFAULT_MESSAGES, "The component selected for capturing is %s\n", full_component_name);

  /** getting video videosrc handle */
  err = OMX_GetHandle(&appPriv->videosrchandle, full_component_name, NULL, &videosrccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No video videosrc component found. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "Found The component for capturing is %s\n", full_component_name);
  }

  /** output buffer size calculation based on input dimension speculation */
  buffer_out_size = out_width * out_height * 3; //yuv420 format -- bpp = 12
  DEBUG(DEB_LEV_SIMPLE_SEQ, "\n buffer_out_size : %d \n", (int)buffer_out_size);

  /** sending command to video videosrc component to go to idle state */
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  pOutBuffer1 = pOutBuffer2 = NULL;
  err = OMX_AllocateBuffer(appPriv->videosrchandle, &pOutBuffer1, 0, NULL, buffer_out_size);
  err = OMX_AllocateBuffer(appPriv->videosrchandle, &pOutBuffer2, 0, NULL, buffer_out_size);
  

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Before locking on idle wait semaphore\n");
  tsem_down(appPriv->videosrcEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "videosrc Sem free\n");
  
  /** sending command to video videosrc component to go to executing state */
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  tsem_down(appPriv->videosrcEventSem);

  err = OMX_FillThisBuffer(appPriv->videosrchandle, pOutBuffer1);
  err = OMX_FillThisBuffer(appPriv->videosrchandle, pOutBuffer2);
 
  DEBUG(DEFAULT_MESSAGES,"Waiting for  EOS\n");

  /*Capture video for 10 seconds*/
  sleep(10);
  bEOS = OMX_TRUE;
  DEBUG(DEFAULT_MESSAGES,"Set  EOS\n");
  sleep(1);
  //tsem_down(appPriv->eofSem);

  DEBUG(DEFAULT_MESSAGES, "The execution of the video capturing process is terminated\n");

  /** state change of all components from executing to idle */
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  
  tsem_down(appPriv->videosrcEventSem);
  

  DEBUG(DEFAULT_MESSAGES, "All video components Transitioned to Idle\n");

  /** sending command to all components to go to loaded state */
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  

  /** freeing buffers of video videosrc input ports */
  DEBUG(DEB_LEV_PARAMS, "Free Video dec output ports\n");
  err = OMX_FreeBuffer(appPriv->videosrchandle, 0, pOutBuffer1);
  err = OMX_FreeBuffer(appPriv->videosrchandle, 0, pOutBuffer2);
 
  tsem_down(appPriv->videosrcEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");

  OMX_FreeHandle(appPriv->videosrchandle);
  
  DEBUG(DEB_LEV_SIMPLE_SEQ, "video dec freed\n");

  OMX_Deinit();

  DEBUG(DEFAULT_MESSAGES, "All components freed. Closing...\n");
  free(appPriv->videosrcEventSem);
  
  free(appPriv->eofSem);
  free(appPriv);

  /** closing the output file */
  fclose(outfile);
  
  return 0;
}


/** Callbacks implementation of the video videosrc component*/
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
      tsem_up(appPriv->videosrcEventSem);
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

OMX_ERRORTYPE videosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;

  if(pBuffer != NULL){
    if(!bEOS) {
      /** if there is color conv component in processing state then send this buffer, in non tunneled case 
        * else in non tunneled case, write the output buffer contents in the specified output file
        */
      if(pBuffer->nFilledLen > 0) {
          fwrite(pBuffer->pBuffer, sizeof(char),  pBuffer->nFilledLen, outfile);    
          pBuffer->nFilledLen = 0;
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      if(!bEOS ) {
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


