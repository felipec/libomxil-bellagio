/**
  @file test/components/audio_effects/omxaudiomixertest.c
  
  This simple test application take one or more input stream/s. passes
  these streams to an audio mixer component and stores the mixed output in another 
  output file.
  
  Copyright (C) 2008 STMicroelectronics
  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).

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

#include "omxaudiomixertest.h"
#include "ctype.h"

#define SINK_NAME "OMX.st.alsa.alsasink"
#define BUFFER_COUNT_ACTUAL 2
#define FRAME_SIZE 1152*2*2 // 1152 samples* 2 channels * 2byte/16bits per channel

OMX_CALLBACKTYPE callbacks = { .EventHandler = audiomixerEventHandler,
                               .EmptyBufferDone = audiomixerEmptyBufferDone,
                               .FillBufferDone = audiomixerFillBufferDone,
};

OMX_CALLBACKTYPE audiosinkcallbacks = { 
                              .EventHandler    = audiosinkEventHandler,
                              .EmptyBufferDone = audiosinkEmptyBufferDone,
                              .FillBufferDone  = NULL
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
  printf("Usage: omxaudiomixertest [-o outfile] [-gi gain] -t -r 44100 -n 2 filename1 filename2\n");
  printf("\n");
  printf("       -o outfile: If this option is specified, the output stream is written to outfile\n");
  printf("                   otherwise redirected to std output; Can't be used with -t\n");
  printf("       -gi       : Gain of stream i[0..3] data [0...100]\n");
  printf("       -t        : The audio mixer is tunneled with the alsa sink; Can't be used with -o\n");
  printf("       -r 44100  : Sample Rate [Default 44100]\n");
  printf("       -n 2      : Number of channel [Default 2]\n\n");
  printf("       -h        : Displays this help\n");
  printf("\n");
  exit(1);
}

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;
int fd = 0,fd1=0;
unsigned int filesize,filesize1;
int flagIsOutputExpected;
int flagOutputReceived;
int flagInputReceived;
int flagIsGain[4];
int flagPlaybackOn;
int flagSetupTunnel;
int flagSampleRate;
int flagChannel;
char *input_file[2], *output_file;
static OMX_BOOL bEOS1=OMX_FALSE,bEOS2=OMX_FALSE;
FILE *outfile;

OMX_BUFFERHEADERTYPE *inBuffer[4], *outBuffer[2],*inBufferSink[2];
static OMX_BOOL isPortDisabled[4];
static int iBufferDropped[2];

int main(int argc, char** argv) {

  OMX_PORT_PARAM_TYPE sParam;
  OMX_U32 data_read,j;
  OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume;
  OMX_AUDIO_PARAM_PCMMODETYPE sPcmModeType;
  int gain[4];
  int argn_dec;
  int i=0,fd2;
  OMX_U32 srate=0,nchannel=0;
  OMX_ERRORTYPE err;
  char c;

  gain[0]=gain[1]=gain[2]=gain[3]=100;

  /* Obtain file descriptor */
  if(argc < 2){
    display_help();
  } else {
    flagIsOutputExpected = 0;
    flagOutputReceived = 0;
    flagInputReceived = 0;
    flagIsGain[0] = 0;
    flagIsGain[1] = 0;
    flagIsGain[2] = 0;
    flagIsGain[3] = 0;
    flagPlaybackOn = 1;
    flagSetupTunnel = 0;
    flagSampleRate = 0;
    flagChannel = 0;

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
          flagPlaybackOn = 0;
          break;
        case 'g':
          i = atoi(argv[argn_dec]+2);
          if(i > 3) {
            DEBUG(DEFAULT_MESSAGES, "-g%i is not valid\n",i);
            i = 0;
          }
          flagIsGain[i] = 1;
          break;
        case 't':
          flagSetupTunnel = 1;
          break;
        case 'r':
          flagSampleRate = 1;
          break;
        case 'n':
          flagChannel = 1;
          break;
        default:
          display_help();
        }
      } else {
        if (flagIsGain[i]) {
          gain[i] = (int)atoi(argv[argn_dec]);
          DEBUG(DEFAULT_MESSAGES, "gain[%d]=%d\n",i,gain[i]);
          flagIsGain[i] = 0;
          if(gain[i] > 100) {
            DEBUG(DEFAULT_MESSAGES, "Gain of stream %i should be between [0..100]\n",i);
            gain[i] = 100; 
          }
          i = 0;
        } else if (flagIsOutputExpected) {
          output_file = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
          strcpy(output_file,argv[argn_dec]);
          flagIsOutputExpected = 0;
          flagOutputReceived = 1;
        } else if (flagSampleRate) {
          srate = (int)atoi(argv[argn_dec]);
          flagSampleRate = 0;
        } else if (flagChannel) {
          nchannel = (int)atoi(argv[argn_dec]);
          flagChannel = 0;
        } else {
          input_file[i] = malloc(strlen(argv[argn_dec]) * sizeof(char) + 1);
          strcpy(input_file[i],argv[argn_dec]);
          flagInputReceived = 1;
          i++;
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
    DEBUG(DEFAULT_MESSAGES, "Input file %s %s ", input_file[0],input_file[1]);
    DEBUG(DEFAULT_MESSAGES, " to ");
    if (flagOutputReceived) {
      DEBUG(DEFAULT_MESSAGES, " %s\n", output_file);
    } else {
      DEBUG(DEFAULT_MESSAGES, " Audio Sink\n");
    }
  }
 
  if(input_file[0]== NULL || input_file[1]==NULL)  {
    DEBUG(DEFAULT_MESSAGES, "Please Supply 2 input files\n");
    exit(1);
  }
  fd = open(input_file[0], O_RDONLY);
  if(fd < 0){
    perror("Error opening input file 1\n");
    exit(1);
  }

  fd1 = open(input_file[1], O_RDONLY);
  if(fd1 < 0){
    perror("Error opening input file 2\n");
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
  filesize1 = getFileSize(fd1);
  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));
  pthread_cond_init(&appPriv->condition, NULL);
  pthread_mutex_init(&appPriv->mutex, NULL);
  appPriv->eventSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eventSem, 0);
  appPriv->eofSem = malloc(sizeof(tsem_t));
  tsem_init(appPriv->eofSem, 0);
  if (flagPlaybackOn) {
    appPriv->sinkEventSem = malloc(sizeof(tsem_t));
    tsem_init(appPriv->sinkEventSem, 0);
  }
  iBufferDropped[0] = 0;
  iBufferDropped[1] = 0;

  err = OMX_Init();
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
    exit(1);
  }
  /** Ask the core for a handle to the audio mixer component */
  err = OMX_GetHandle(&appPriv->handle, "OMX.st.audio.mixer", NULL , &callbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Audio Mixer OMX_GetHandle failed\n");
    exit(1);
  }
  if (flagPlaybackOn) {
    err = OMX_GetHandle(&appPriv->audiosinkhandle, SINK_NAME, NULL , &audiosinkcallbacks);
    if(err != OMX_ErrorNone){
      DEBUG(DEB_LEV_ERR, "No sink found. Exiting...\n");
      exit(1);
    }
  }

  /*Max 4 input stream*/
  for(j=0;j<4;j++) {
    isPortDisabled[i] = OMX_FALSE;
    if((gain[j] >= 0) && (gain[j] <100)) {
      sVolume.nPortIndex = j;
      err = OMX_GetConfig(appPriv->handle, OMX_IndexConfigAudioVolume, &sVolume);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_GetConfig 0 \n",err);
      }
      sVolume.sVolume.nValue = gain[j];
      DEBUG(DEFAULT_MESSAGES, "Setting Gain[%i] %d \n",(int)j, gain[j]);
      err = OMX_SetConfig(appPriv->handle, OMX_IndexConfigAudioVolume, &sVolume);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig 0 \n",err);
      }
    }
  }

  /*Set sample rate and channel no to alsa sink if specified*/
  if(srate && nchannel && flagPlaybackOn) {
    DEBUG(DEFAULT_MESSAGES, "Sample Rate=%d,NChannel=%d\n",(int)srate,(int)nchannel);
    sPcmModeType.nPortIndex=0;
    setHeader(&sPcmModeType, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    err = OMX_GetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &sPcmModeType);

    sPcmModeType.nChannels = nchannel;
    sPcmModeType.nSamplingRate = srate;
    err = OMX_SetParameter(appPriv->audiosinkhandle, OMX_IndexParamAudioPcm, &sPcmModeType);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
    }
  }

  if (flagSetupTunnel) {
    err = OMX_SetupTunnel(appPriv->handle, 4, appPriv->audiosinkhandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel Failed\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES, "Set up Tunnel Completed\n");
  }

  /** Get the number of ports */
  setHeader(&sParam, sizeof(OMX_PORT_PARAM_TYPE));
  err = OMX_GetParameter(appPriv->handle, OMX_IndexParamAudioInit, &sParam);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }
  DEBUG(DEFAULT_MESSAGES, "Audio Mixer has %d ports\n",(int)sParam.nPorts);

  setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sPortDef.nPortIndex = 0;
  err = OMX_GetParameter(appPriv->handle, OMX_IndexParamPortDefinition, &sPortDef);

  sPortDef.nBufferCountActual = 2;
  err = OMX_SetParameter(appPriv->handle, OMX_IndexParamPortDefinition, &sPortDef);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }
  sPortDef.nPortIndex = 1;
  err = OMX_GetParameter(appPriv->handle, OMX_IndexParamPortDefinition, &sPortDef);

  sPortDef.nBufferCountActual = 2;
  err = OMX_SetParameter(appPriv->handle, OMX_IndexParamPortDefinition, &sPortDef);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "Error in getting OMX_PORT_PARAM_TYPE parameter\n");
    exit(1);
  }

  /*Disable 2 out of 4 ports*/
  isPortDisabled[2] = OMX_TRUE;
  isPortDisabled[3] = OMX_TRUE;
  err = OMX_SendCommand(appPriv->handle, OMX_CommandPortDisable, 2, NULL);
  err = OMX_SendCommand(appPriv->handle, OMX_CommandPortDisable, 3, NULL);
  tsem_down(appPriv->eventSem);
  tsem_down(appPriv->eventSem);


  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  inBuffer[0] = inBuffer[1] = inBuffer[2] = inBuffer[3]= outBuffer[0] = outBuffer[1] = NULL;

  for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
    err = OMX_AllocateBuffer(appPriv->handle, &inBuffer[j], 0, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in %i %i\n",(int)j, err);
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->handle, &inBuffer[j+ 2], 1, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in %i %i\n",(int)j+2, err);
      exit(1);
    }
  }

  if (!flagSetupTunnel) {
    for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
      err = OMX_AllocateBuffer(appPriv->handle, &outBuffer[j], 4, NULL, BUFFER_IN_SIZE);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out %i %i\n",(int)j, err);
        exit(1);
      }
    }
    if (flagPlaybackOn) {
      for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
        err = OMX_UseBuffer(appPriv->audiosinkhandle, &inBufferSink[j], 0, NULL, BUFFER_IN_SIZE, outBuffer[j]->pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer %i\n",(int)j);
          exit(1);
        }
      }
    }
  }

  if (flagPlaybackOn) {
    tsem_down(appPriv->sinkEventSem);
    DEBUG(DEB_LEV_SIMPLE_SEQ,"audio sink state idle\n");
  }
  tsem_down(appPriv->eventSem);

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);

  if (flagPlaybackOn) {
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
  DEBUG(DEB_LEV_PARAMS, "Had buffers at:\n0x%08x\n0x%08x\n0x%08x\n0x%08x\n", 
                (int)inBuffer[0]->pBuffer, (int)inBuffer[1]->pBuffer, (int)outBuffer[0]->pBuffer, (int)outBuffer[1]->pBuffer);
  DEBUG(DEB_LEV_PARAMS, "After switch to executing\n");
  
  for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
    data_read = read(fd, inBuffer[j]->pBuffer, FRAME_SIZE);
    inBuffer[0]->nFilledLen = data_read;
    filesize -= data_read;

    data_read = read(fd1, inBuffer[j+2]->pBuffer, FRAME_SIZE);
    inBuffer[2]->nFilledLen = data_read;
    filesize1 -= data_read;
  }

  for(j=0;j<BUFFER_COUNT_ACTUAL*2;j++) {
    DEBUG(DEB_LEV_PARAMS, "Empty %i  buffer %x\n",(int)j, (int)inBuffer[j]);
    err = OMX_EmptyThisBuffer(appPriv->handle, inBuffer[j]);
  }

  /** Schedule a couple of buffers to be filled on the output port
    * The callback itself will re-schedule them.
    */
  if (!flagSetupTunnel) {
    for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
      err = OMX_FillThisBuffer(appPriv->handle, outBuffer[j]);
    }
  }

  i=0;
  /*Port Disable option available in case of direct play out only*/
  if(!flagOutputReceived) {
    DEBUG(DEFAULT_MESSAGES, "\nIf you want to disabled port enter port number[0..1]: else Enter 'q' \n\n");
    DEBUG(DEFAULT_MESSAGES, "Entry : ");
    while(!bEOS1 && !bEOS2) {
      c = getchar();
      if(c=='\n') {
        DEBUG(DEFAULT_MESSAGES, "Entry : ");
        continue;
      } else if(toupper(c) == 'Q') {
        DEBUG(DEFAULT_MESSAGES,"No port to disable\n");
        break;
      } else {
        i= (int)atoi(&c);
        if(i==0 || i==1) {
          DEBUG(DEFAULT_MESSAGES,"Disabling Port %i\n",i);
          isPortDisabled[i] = OMX_TRUE;
          err = OMX_SendCommand(appPriv->handle, OMX_CommandPortDisable, i, NULL);
          while(iBufferDropped[i]!=2) {
            usleep(10000);
          }

          for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
            if(i==0) {
              err = OMX_FreeBuffer(appPriv->handle, 0, inBuffer[j]);
            } else {
              err = OMX_FreeBuffer(appPriv->handle, 1, inBuffer[j+2]);
            }
          }
        
          tsem_down(appPriv->eventSem);
          iBufferDropped[i] = 0;
          break;
        } else {
          DEBUG(DEFAULT_MESSAGES,"Either Port %i is already disabled or not valid\n",i);
        }
      }
    }
  }
  
  /* If user want to enable previously disabled port*/
  if(isPortDisabled[0] == OMX_TRUE || isPortDisabled[1] == OMX_TRUE) {
    DEBUG(DEFAULT_MESSAGES, "\nIf you want to re-enable port %i enter 'y' else 'n' \n\n",i);
    c = ' ';
    while(1) {
      c = getchar();
      if(c=='\n') {
        DEBUG(DEFAULT_MESSAGES, "Entry : ");
        continue;
      } else if(toupper(c) == 'N') {
        DEBUG(DEFAULT_MESSAGES,"Port to remain disable\n");
        break;
      } else if(toupper(c) == 'Y') {
        DEBUG(DEFAULT_MESSAGES,"Re-Enabling Port %i\n",i);
        err = OMX_SendCommand(appPriv->handle, OMX_CommandPortEnable, i, NULL);
      
        /*Buffer 0..1 for Port 0 and 2..3 for Port 1*/
        j = (i==0)?0:2;

        err = OMX_AllocateBuffer(appPriv->handle, &inBuffer[j], i, NULL, BUFFER_IN_SIZE);
        if (err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Error on Re-AllocateBuffer in %i %i\n",(int)j, err);
          exit(1);
        }
        err = OMX_AllocateBuffer(appPriv->handle, &inBuffer[j+ 1], i, NULL, BUFFER_IN_SIZE);
        if (err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "Error on Re-AllocateBuffer in %i %i\n",(int)j+1, err);
          exit(1);
        }
    
        tsem_down(appPriv->eventSem);
        isPortDisabled[i] = OMX_FALSE;

        fd2 = (i==0)?fd:fd1;

        data_read = read(fd2, inBuffer[j]->pBuffer, FRAME_SIZE);
        inBuffer[j]->nFilledLen = data_read;

        data_read = read(fd2, inBuffer[j+1]->pBuffer, FRAME_SIZE);
        inBuffer[j+1]->nFilledLen = data_read;

        /*Sending Empty buffer*/
        err = OMX_EmptyThisBuffer(appPriv->handle, inBuffer[j]);
        err = OMX_EmptyThisBuffer(appPriv->handle, inBuffer[j+1]);
        break;
      }
      
    }
  }


  DEBUG(DEFAULT_MESSAGES, "Waiting for EOS\n");
  if(isPortDisabled[0] == OMX_FALSE) {
    tsem_down(appPriv->eofSem);
    DEBUG(DEFAULT_MESSAGES, "Received EOS 1\n");
  }
  if(isPortDisabled[1] == OMX_FALSE) {
    tsem_down(appPriv->eofSem);
    DEBUG(DEFAULT_MESSAGES, "Received EOS 2\n");
  }

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }  
  if (flagPlaybackOn) {
    tsem_down(appPriv->sinkEventSem);
  }
  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }

  for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
    if(isPortDisabled[0] == OMX_FALSE) {
      err = OMX_FreeBuffer(appPriv->handle, 0, inBuffer[j]);
    }
    if(isPortDisabled[1] == OMX_FALSE) {
      err = OMX_FreeBuffer(appPriv->handle, 1, inBuffer[j+2]);
    }
  }

  if (!flagSetupTunnel) {
    for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
      err = OMX_FreeBuffer(appPriv->handle, 4, outBuffer[j]);
    }
  }

  if (flagPlaybackOn && !flagSetupTunnel) {
    for(j=0;j<BUFFER_COUNT_ACTUAL;j++) {
      err = OMX_FreeBuffer(appPriv->audiosinkhandle, 0, inBufferSink[j]);
    }
  }
  if (flagPlaybackOn) {
    tsem_down(appPriv->sinkEventSem);
  }
  
  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);
  
  OMX_FreeHandle(appPriv->handle);

  free(appPriv->eventSem);
  free(appPriv);
  if (flagPlaybackOn) {
    free(appPriv->sinkEventSem);
    appPriv->sinkEventSem = NULL;
  }

  if (flagOutputReceived) {
    if(fclose(outfile) != 0) {
      DEBUG(DEB_LEV_ERR,"Error in closing output file\n");
      exit(1);
    }
    free(output_file);
  }

  close(fd);
  close(fd1);
  free(input_file[0]);
  free(input_file[1]);

  return 0;
}

/* Callbacks implementation */
OMX_ERRORTYPE audiomixerEventHandler(
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

OMX_ERRORTYPE audiomixerEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  int data_read;
  

  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
  if(pBuffer->nInputPortIndex==0) {
    
    if(isPortDisabled[0] == OMX_FALSE) {
      data_read = read(fd, pBuffer->pBuffer, FRAME_SIZE);
      pBuffer->nFilledLen = data_read;
      pBuffer->nOffset = 0;
      filesize -= data_read;
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Sending from file 1 data read=%d\n",data_read);
      if (data_read <= 0) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
        ++iBufferDropped[0];
        if(iBufferDropped[0]==2) {
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Dropping Empty This buffer to Audio Mixer Stream 1\n", __func__);
          tsem_up(appPriv->eofSem);
          return OMX_ErrorNone;
        } else if(iBufferDropped[0]>2) { 
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Dropping Empty This buffer to Audio Mixer Stream 1\n", __func__);
          return OMX_ErrorNone;
        }
        pBuffer->nFilledLen=0;
        pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
        bEOS1=OMX_TRUE;
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Sending EOS for Stream 1\n", __func__);
        err = OMX_EmptyThisBuffer(hComponent, pBuffer);
        return OMX_ErrorNone;
      }
    } else {
      ++iBufferDropped[0];
      return OMX_ErrorNone;
    }
  } else if(pBuffer->nInputPortIndex==1) {
    
    if(isPortDisabled[1] == OMX_FALSE) {
      data_read = read(fd1, pBuffer->pBuffer, FRAME_SIZE);
      pBuffer->nFilledLen = data_read;
      pBuffer->nOffset = 0;
      filesize1 -= data_read;
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Sending from file 2 data read=%d\n",data_read);
      if (data_read <= 0) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
        ++iBufferDropped[1];
        if(iBufferDropped[1]==2) {
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Dropping Empty This buffer to Audio Mixer Stream 2\n", __func__);
          tsem_up(appPriv->eofSem);
          return OMX_ErrorNone;
        } else if(iBufferDropped[1]>2) { 
          DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Dropping Empty This buffer to Audio Mixer Stream 2\n", __func__);
          return OMX_ErrorNone;
        }
        pBuffer->nFilledLen=0;
        pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
        bEOS2=OMX_TRUE;
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Sending EOS for Stream 2\n", __func__);
        err = OMX_EmptyThisBuffer(hComponent, pBuffer);
        return OMX_ErrorNone;
      }
    }else {
      ++iBufferDropped[1];
      return OMX_ErrorNone;
    }
  }
  if(!bEOS1 || !bEOS2 ) {
    DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %x\n", (int)pBuffer);
    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
  }else {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Empty This buffer to Audio Mixer\n", __func__);
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiomixerFillBufferDone(
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
      pBuffer->nFilledLen = 0;
      /* Reschedule the fill buffer request */
      if(!bEOS1 || !bEOS2) {
        err = OMX_FillThisBuffer(hComponent, pBuffer);
      } else {
        DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Fill This buffer to Audio Mixer\n", __func__);
      }
    } else if (flagPlaybackOn) {
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
      for(i=0;i<pBuffer->nFilledLen;i++) {
        putchar(*(char*)(pBuffer->pBuffer + i));
      }
      pBuffer->nFilledLen = 0;
      /* Reschedule the fill buffer request */
      if(!bEOS1 || !bEOS2) {
        err = OMX_FillThisBuffer(hComponent, pBuffer);
      } else {
        DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Fill This buffer to Audio Mixer\n", __func__);
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
  if(!bEOS1 || !bEOS2) {
    if(outBuffer[0]->pBuffer == pBuffer->pBuffer) {
      outBuffer[0]->nFilledLen=0;
      err = OMX_FillThisBuffer(appPriv->handle, outBuffer[0]);
    } else {
      outBuffer[1]->nFilledLen=0;
      err = OMX_FillThisBuffer(appPriv->handle, outBuffer[1]);
    }
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
    }
  } else {
    DEBUG(DEFAULT_MESSAGES,"In %s EOS reached\n",__func__);
    alsaSinkBufferDropped++;
    tsem_up(appPriv->eofSem);
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
