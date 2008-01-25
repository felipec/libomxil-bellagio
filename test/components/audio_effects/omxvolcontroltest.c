/**
  @file test/components/audio_effects/omxvolcontroltest.c
  
  This simple test application provides a tesnting stream for the volume control component. 
  It will be added in the more complex audio test application in the next release  
  
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

#include "omxvolcontroltest.h"

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;
int fd = 0;
unsigned int filesize;
OMX_ERRORTYPE err;
OMX_HANDLETYPE handle;

OMX_CALLBACKTYPE callbacks = { .EventHandler = volcEventHandler,
                               .EmptyBufferDone = volcEmptyBufferDone,
                               .FillBufferDone = volcFillBufferDone,
};

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
  printf("Usage: omxvolcontroltest [-o outfile] [-g gain] filename\n");
  printf("\n");
  printf("       -o outfile: If this option is specified, the output stream is written to outfile\n");
  printf("                   otherwise redirected to std output\n");
  printf("       -g: Gain of pcm data [0...100]\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  exit(1);
}

int flagIsOutputExpected;
int flagOutputReceived;
int flagInputReceived;
int flagIsGain;
char *input_file, *output_file;
static OMX_BOOL bEOS=OMX_FALSE;
FILE *outfile;

int main(int argc, char** argv) {

  OMX_PORT_PARAM_TYPE param;
  OMX_BUFFERHEADERTYPE *inBuffer1, *inBuffer2, *outBuffer1, *outBuffer2;
  int data_read1;
  int data_read2;
  OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume;
  int gain=100;
  int argn_dec;

  /* Obtain file descriptor */
  if(argc < 2){
    display_help();
  } else {
    flagIsOutputExpected = 0;
    flagOutputReceived = 0;
    flagInputReceived = 0;
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
        case 'o':
          flagIsOutputExpected = 1;
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
    if (!flagInputReceived) {
      display_help();
    }
    DEBUG(DEFAULT_MESSAGES, "Input file %s", input_file);
    DEBUG(DEFAULT_MESSAGES, " to ");
    if (flagOutputReceived) {
      DEBUG(DEFAULT_MESSAGES, " %s\n", output_file);
    }
  }

 
  fd = open(input_file, O_RDONLY);
  if(fd < 0){
    perror("Error opening input file\n");
    exit(1);
  }

  if (flagOutputReceived) {
    outfile = fopen(output_file,"wb");
    if(outfile == NULL) {
      DEBUG(DEB_LEV_ERR, "Error at opening the output file");
      exit(1);
    } 
  }
   
  filesize = getFileSize(fd);
  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));
  pthread_cond_init(&appPriv->condition, NULL);
  pthread_mutex_init(&appPriv->mutex, NULL);
  appPriv->eventSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eventSem, 0);
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eofSem, 0);

  err = OMX_Init();
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
    exit(1);
  }
  /** Ask the core for a handle to the volume control component
    */
  err = OMX_GetHandle(&handle, "OMX.st.volume.component", NULL /*appPriv */, &callbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_GetHandle failed\n");
    exit(1);
  }

  if((gain >= 0) && (gain <100)) {
    err = OMX_GetConfig(handle, OMX_IndexConfigAudioVolume, &sVolume);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_GetConfig 0 \n",err);
    }
    sVolume.sVolume.nValue = gain;
    DEBUG(DEFAULT_MESSAGES, "Setting Gain %d \n", gain);
    err = OMX_SetConfig(handle, OMX_IndexConfigAudioVolume, &sVolume);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig 0 \n",err);
    }
  }

  /** Set the number of ports for the parameter structure */
  param.nPorts = 2;
  setHeader(&param, sizeof(OMX_PORT_PARAM_TYPE));
  err = OMX_GetParameter(handle, OMX_IndexParamAudioInit, &param);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }

  setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sPortDef.nPortIndex = 0;
  err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);

  sPortDef.nBufferCountActual = 2;
  err = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }
  sPortDef.nPortIndex = 1;
  err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);

  sPortDef.nBufferCountActual = 2;
  err = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &sPortDef);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }

  err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  inBuffer1 = inBuffer2 = outBuffer1 = outBuffer2 = NULL;
  err = OMX_AllocateBuffer(handle, &inBuffer1, 0, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 1%i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(handle, &inBuffer2, 0, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 2 %i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(handle, &outBuffer1, 1, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 1 %i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(handle, &outBuffer2, 1, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 2 %i\n", err);
    exit(1);
  }

  tsem_down(appPriv->eventSem);
  err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);

  DEBUG(DEB_LEV_PARAMS, "Had buffers at:\n0x%08x\n0x%08x\n0x%08x\n0x%08x\n", 
                (int)inBuffer1->pBuffer, (int)inBuffer2->pBuffer, (int)outBuffer1->pBuffer, (int)outBuffer2->pBuffer);
  DEBUG(DEB_LEV_PARAMS, "After switch to executing\n");

  data_read1 = read(fd, inBuffer1->pBuffer, BUFFER_IN_SIZE);
  inBuffer1->nFilledLen = data_read1;
  filesize -= data_read1;

  data_read2 = read(fd, inBuffer2->pBuffer, BUFFER_IN_SIZE);
  inBuffer2->nFilledLen = data_read2;
  filesize -= data_read2;

  DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBuffer1);
  err = OMX_EmptyThisBuffer(handle, inBuffer1);
  DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBuffer2);
  err = OMX_EmptyThisBuffer(handle, inBuffer2);
  
  /** Schedule a couple of buffers to be filled on the output port
    * The callback itself will re-schedule them.
    */
  err = OMX_FillThisBuffer(handle, outBuffer1);
  err = OMX_FillThisBuffer(handle, outBuffer2);

  tsem_down(appPriv->eofSem);

  err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);

  err = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_FreeBuffer(handle, 0, inBuffer1);
  err = OMX_FreeBuffer(handle, 0, inBuffer2);
  err = OMX_FreeBuffer(handle, 1, outBuffer1);
  err = OMX_FreeBuffer(handle, 1, outBuffer2);

  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);

  OMX_FreeHandle(handle);

  free(appPriv->eventSem);
  free(appPriv);

  if (flagOutputReceived) {
    if(fclose(outfile) != 0) {
      DEBUG(DEB_LEV_ERR,"Error in closing output file\n");
      exit(1);
    }
    free(output_file);
  }

  close(fd);
  free(input_file);

  return 0;
}

/* Callbacks implementation */
OMX_ERRORTYPE volcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_IN OMX_PTR pEventData) {

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Volume Component State changed in ");
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
      tsem_up(appPriv->eventSem);
    } else  if (Data1 == OMX_CommandPortEnable){
      tsem_up(appPriv->eventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      tsem_up(appPriv->eventSem);
    } 
  } else if(eEvent == OMX_EventBufferFlag) {
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE volcEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  int data_read;
  static int iBufferDropped=0;

  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
  data_read = read(fd, pBuffer->pBuffer, BUFFER_IN_SIZE);
  pBuffer->nFilledLen = data_read;
  pBuffer->nOffset = 0;
  filesize -= data_read;
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
  if(!bEOS) {
    DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
  }else {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Empty This buffer to Audio Dec\n", __func__);
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE volcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  int i;  

  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback. Got buflen %i for buffer at 0x%08x\n",
                          __func__, (int)pBuffer->nFilledLen, (int)pBuffer);

  /* Output data to standard output */
  if(pBuffer != NULL) {
    if (pBuffer->nFilledLen == 0) {
      DEBUG(DEB_LEV_ERR, "Ouch! In %s: no data in the output buffer!\n", __func__);
      return OMX_ErrorNone;
    }
    if (flagOutputReceived) {
      if(pBuffer->nFilledLen > 0) {
        fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, outfile);
      }
    } else {
      for(i=0;i<pBuffer->nFilledLen;i++) {
        putchar(*(char*)(pBuffer->pBuffer + i));
      }
    }
    pBuffer->nFilledLen = 0;
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  /* Reschedule the fill buffer request */
  if(!bEOS) {
    err = OMX_FillThisBuffer(hComponent, pBuffer);
  }
  return OMX_ErrorNone;
}

/** Gets the file descriptor's size
  * @return the size of the file. If size cannot be computed
  * (i.e. stdin, zero is returned)
  */
static int getFileSize(int fd) {

  struct stat input_file_stat;
  int err;

  /* Obtain input file length */
  err = fstat(fd, &input_file_stat);
  if(err){
    DEBUG(DEB_LEV_ERR, "fstat failed");
    exit(-1);
  }
  return input_file_stat.st_size;
}
