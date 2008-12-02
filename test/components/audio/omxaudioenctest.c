/**
  @file test/components/audio/omxaudioenctest.c
  
  This test application can encode audio pcm stream into mp3/aac/amr format.
  
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
  
  $Date: 2008-02-13 16:26:20 +0530 (Wed, 13 Feb 2008) $
  Revision $Rev: 1301 $
  Author $Author: pankaj_sen $
*/

#include "omxaudioenctest.h"

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;
int fd = 0;
unsigned int filesize;
OMX_ERRORTYPE err;

OMX_CALLBACKTYPE callbacks = { .EventHandler = audioencEventHandler,
                               .EmptyBufferDone = audioencEmptyBufferDone,
                               .FillBufferDone = audioencFillBufferDone,
};

OMX_CALLBACKTYPE audiosrccallbacks = { 
  .EventHandler = audiosrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone = audiosrcFillBufferDone,
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
  printf("Usage: omxaudioenctest [-o outfile.mp3] -r 44100 -n 2 -i filename.pcm\n");
  printf("\n");
  printf("       -o outfile.mp3 \n");
  printf("       -i filename.pcm \n");
  printf("       -s : Grab input from audio source [Can't be used with '-i' option]\n");
  printf("       -t : Use tunnel between audio source and encoder\n");
  printf("       -r 44100 : Sample Rate\n");
  printf("       -n 2     : Number of channel\n");
  printf("       -f G726  : Format Requested\n\n");
  printf("          SupportedFormat\n");
  printf("            G726 \n");
  printf("            MP3 \n");
  printf("            AAC \n");
  printf("            AMR \n");
  printf("       -m [NB0..NB7,WB0..WB8]: AMR Band \n");
  printf("          [OMX_AUDIO_AMRBandModeNB0,..,OMX_AUDIO_AMRBandModeNB7, OMX_AUDIO_AMRBandModeWB0,..,OMX_AUDIO_AMRBandModeWB8]\n");
  printf("           \n\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  exit(1);
}

int flagIsInputExpected;
int flagIsOutputExpected;
int flagOutputReceived;
int flagInputReceived;
int flagIsUseAudioSource;
int flagIsRate;
int flagIsChannel;
int flagIsFormat;
int flagIsFormatSpecified;
int flagSetupTunnel;
int flagBandMode;

char *input_file=NULL, *output_file=NULL;
static OMX_BOOL bEOS=OMX_FALSE;
FILE *outfile;
OMX_BUFFERHEADERTYPE *outSrcBuffer[2],*inBuffer[2], *outBuffer[2];

int main(int argc, char** argv) {
  int                             data_read1;
  int                             data_read2;
  OMX_AUDIO_PARAM_MP3TYPE         sAudioMp3;
  OMX_AUDIO_PARAM_AACPROFILETYPE  sAudioAac;
  OMX_AUDIO_PARAM_G726TYPE        sAudioG726;
  OMX_AUDIO_PARAM_AMRTYPE         sAudioAmr;
  OMX_AUDIO_PARAM_PCMMODETYPE     sPCMModeParam;
  int                             rate=0,channel=0,bandmode = OMX_AUDIO_AMRBandModeNB7;
  int                             argn_dec;
  OMX_AUDIO_CODINGTYPE            eCodingType;
  char                            cComponentName[OMX_MAX_STRINGNAME_SIZE];

  /*Default encoding is in Mp3 format*/
  eCodingType = OMX_AUDIO_CodingMP3;
  strcpy(&cComponentName[0],"OMX.st.audio_encoder.mp3");

  /* Obtain file descriptor */
  if(argc < 2){
    display_help();
  } else {
    flagIsInputExpected = 0;
    flagIsOutputExpected = 0;
    flagOutputReceived = 0;
    flagInputReceived = 0;
    flagIsRate = 0;
    flagIsChannel = 0;
    flagIsFormat = 0;
    flagIsUseAudioSource = 0;
    flagSetupTunnel = 0;
    flagIsFormatSpecified = 0;
    flagBandMode = 0;

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
        case 'i':
          flagIsInputExpected = 1;
          break;
        case 'o':
          flagIsOutputExpected = 1;
          break;
        case 'r':
          flagIsRate = 1;
          break;
        case 'n':
          flagIsChannel = 1;
          break;
        case 'f':
          flagIsFormat = 1;
          break;
        case 's':
          flagIsUseAudioSource = 1;
          break;
        case 't':
          flagSetupTunnel = 1;
          break;
        case 'm':
          flagBandMode = 1;
          break;
        default:
          display_help();
        }
      } else {
        if (flagIsRate) {
          rate = (int)atoi(argv[argn_dec]);
          flagIsRate = 0;
          if(rate < 8000) {
            DEBUG(DEFAULT_MESSAGES, "Rate should be between [8000..48000]\n");
            rate = 48000; 
          } else if(rate > 48000) {
            DEBUG(DEFAULT_MESSAGES, "Rate should be between [8000..48000]\n");
            rate = 48000; 
          }
        } if (flagIsChannel) {
          channel = (int)atoi(argv[argn_dec]);
          flagIsChannel = 0;
          if(channel > 6 && channel < 1) {
            DEBUG(DEFAULT_MESSAGES, "Channel should be between [1..6]\n");
            channel = 1;
          }
        } else if (flagIsOutputExpected) {
          output_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(output_file,argv[argn_dec]);
          flagIsOutputExpected = 0;
          flagOutputReceived = 1;
        } else if (flagIsInputExpected) {
          input_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(input_file,argv[argn_dec]);
          flagInputReceived = 1;
          flagIsInputExpected = 0;
        } else if(flagIsFormat) {
          flagIsFormat = 0;
          if(strncmp(argv[argn_dec], "G726", 4) == 0) {
            eCodingType = OMX_AUDIO_CodingG726;
            strcpy(&cComponentName[0],"OMX.st.audio_encoder.g726");
          } else if(strncmp(argv[argn_dec], "MP3", 3) == 0) {
            eCodingType = OMX_AUDIO_CodingMP3;
            strcpy(&cComponentName[0],"OMX.st.audio_encoder.mp3");
          } else if(strncmp(argv[argn_dec], "AAC", 3) == 0) {
            eCodingType = OMX_AUDIO_CodingAAC;
            strcpy(&cComponentName[0],"OMX.st.audio_encoder.aac");
          } else if(strncmp(argv[argn_dec], "AMR", 3) == 0) {
            eCodingType = OMX_AUDIO_CodingAMR;
            strcpy(&cComponentName[0],"OMX.st.audio_encoder.amr");
          } else {
            DEBUG(DEFAULT_MESSAGES, "Format Not Detected\n");
            display_help();
          }
          flagIsFormatSpecified = 1;
        } else if(flagBandMode) {
          flagBandMode = 0;
          char *str = strdup(argv[argn_dec]);
          DEBUG(DEFAULT_MESSAGES, "Band Mode %s %c\n", str,str[2]);
          bandmode = (int)atoi(&str[2]);
          if(strncmp(str,"NB",2)==0) {
            bandmode += 1;
          } else if(strncmp(str,"WB",2)==0) {
            strcpy(&cComponentName[0],"OMX.st.audio_encoder.amr");
            bandmode += 9;
          }
          DEBUG(DEFAULT_MESSAGES, "Band Mode %d\n", bandmode);
        }
      }
      argn_dec++;
    }
    if (!flagInputReceived && !flagSetupTunnel && !flagIsUseAudioSource) {
      display_help();
    }
    DEBUG(DEFAULT_MESSAGES, "Input file %s", input_file);
    DEBUG(DEFAULT_MESSAGES, " to ");
    if (flagOutputReceived) {
      DEBUG(DEFAULT_MESSAGES, " %s\n", output_file);
    }
  }
 
  

  if(flagOutputReceived && !flagIsFormatSpecified) {
    if(strstr(output_file,".aac") != NULL) {
      eCodingType = OMX_AUDIO_CodingAAC;
      strcpy(&cComponentName[0],"OMX.st.audio_encoder.aac");
    } else if(strstr(output_file,".g726") != NULL) {
      eCodingType = OMX_AUDIO_CodingG726;
      strcpy(&cComponentName[0],"OMX.st.audio_encoder.g726");
    } else if(strstr(output_file,".mp3") != NULL) {
      eCodingType = OMX_AUDIO_CodingMP3;
      strcpy(&cComponentName[0],"OMX.st.audio_encoder.mp3");
    } else if(strstr(output_file,".amr") != NULL) {

      if((bandmode <= OMX_AUDIO_AMRBandModeNB7) && ((rate !=0 && rate!=8000) || (channel!=0 && channel!=1))) {
        DEBUG(DEFAULT_MESSAGES, "AMR-NB Support only 8000Hz Mono\n");
        exit(1);
      } else if((bandmode > OMX_AUDIO_AMRBandModeNB7) && ((rate !=0 && rate!=16000) || (channel!=0 && channel!=1))) {
        DEBUG(DEFAULT_MESSAGES, "AMR-WB Support only 16000Hz Mono\n");
        exit(1);
      }

      eCodingType = OMX_AUDIO_CodingAMR;
      strcpy(&cComponentName[0],"OMX.st.audio_encoder.amr");
    } else {
      DEBUG(DEFAULT_MESSAGES, "Bad Output File Extension %s\n", output_file);
      exit(1);
    }
  }
  if(flagSetupTunnel) {
    flagIsUseAudioSource = 1;
  }
  if(!flagIsUseAudioSource) {
    fd = open(input_file, O_RDONLY);
    if(fd < 0){
      perror("Error opening input file\n");
      exit(1);
    }
  } else {
    DEBUG(DEFAULT_MESSAGES, "Using captured data from Audio Source");
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
  if(flagIsUseAudioSource) {
    appPriv->audiosrcEventSem = malloc(sizeof(tsem_t));
    tsem_init(appPriv->audiosrcEventSem, 0);
  }

  err = OMX_Init();
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_Init() failed\n");
    exit(1);
  }
  /** Ask the core for a handle to the volume control component
    */
  DEBUG(DEFAULT_MESSAGES, "Getting Handle for Component %s \n",cComponentName);
  err = OMX_GetHandle(&appPriv->audioenchandle, cComponentName, NULL , &callbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "OMX_GetHandle failed\n");
    exit(1);
  }

  if(flagIsUseAudioSource) {
    err = OMX_GetHandle(&appPriv->audiosrchandle, "OMX.st.alsa.alsasrc", NULL , &audiosrccallbacks);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "OMX_GetHandle failed\n");
      exit(1);
    }
  }

  if((rate > 0 || channel >0) && flagIsUseAudioSource)  {
    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    sPCMModeParam.nPortIndex = 0;
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
    rate = sPCMModeParam.nSamplingRate;
    channel = sPCMModeParam.nChannels;
  } else if(flagIsUseAudioSource) { /*If sample rate and channel is not specified then use audio src rate and channel as default*/
    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    sPCMModeParam.nPortIndex = 0;
    err = OMX_GetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }
    rate = sPCMModeParam.nSamplingRate;
    channel = sPCMModeParam.nChannels;
  }
  

  if(rate > 0 || channel >0)  {
    DEBUG(DEFAULT_MESSAGES, "Setting Sample Rate =%d and nChannel=%d\n",rate,channel);

    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    sPCMModeParam.nPortIndex = 0;
    err = OMX_GetParameter(appPriv->audioenchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in Audio Encoder. Exiting...\n");
      exit(1);
    }
    
    sPCMModeParam.nChannels = (channel >0 ) ? channel:sPCMModeParam.nChannels;
    sPCMModeParam.nSamplingRate = (rate >0 ) ? rate:sPCMModeParam.nSamplingRate;

    err = OMX_SetParameter(appPriv->audioenchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in Audio Encoder. Exiting...\n");
      exit(1);
    }
    
    switch(eCodingType) {
    case OMX_AUDIO_CodingMP3 : {
        setHeader(&sAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
        sAudioMp3.nPortIndex=1;
        err = OMX_GetParameter(appPriv->audioenchandle, OMX_IndexParamAudioMp3, &sAudioMp3);
        sAudioMp3.nChannels = channel;
        sAudioMp3.nSampleRate = rate;
        err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamAudioMp3, &sAudioMp3);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }
                               }
      break;
    case OMX_AUDIO_CodingAAC :  {
        setHeader(&sAudioAac, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
        sAudioAac.nPortIndex=1;
        err = OMX_GetParameter(appPriv->audioenchandle, OMX_IndexParamAudioAac, &sAudioAac);
        sAudioAac.nChannels = channel;
        sAudioAac.nSampleRate = rate;
        err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamAudioAac, &sAudioAac);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }
                                }
      break;
    case OMX_AUDIO_CodingG726 :   {
        setHeader(&sAudioG726, sizeof(OMX_AUDIO_PARAM_G726TYPE));
        sAudioG726.nPortIndex=1;
        err = OMX_GetParameter(appPriv->audioenchandle, OMX_IndexParamAudioG726, &sAudioG726);
        //sAudioG726.nChannels = channel;
        err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamAudioG726, &sAudioG726);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }
                                  }
      break;
    case OMX_AUDIO_CodingAMR : {
        setHeader(&sAudioAmr, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
        sAudioAmr.nPortIndex=1;
        err = OMX_GetParameter(appPriv->audioenchandle, OMX_IndexParamAudioAmr, &sAudioAmr);
        sAudioAmr.nChannels = (channel >0 ) ? channel:sAudioAmr.nChannels;

        switch(bandmode) {
        case OMX_AUDIO_AMRBandModeNB0:                 /**< AMRNB Mode 0 =  4750 bps */
          sAudioAmr.nBitRate = 4750; 
          break;
        case OMX_AUDIO_AMRBandModeNB1:                 /**< AMRNB Mode 1 =  5150 bps */
          sAudioAmr.nBitRate = 5150;
          break;
        case OMX_AUDIO_AMRBandModeNB2:                 /**< AMRNB Mode 2 =  5900 bps */
          sAudioAmr.nBitRate = 5900;
          break;
        case OMX_AUDIO_AMRBandModeNB3:                 /**< AMRNB Mode 3 =  6700 bps */
          sAudioAmr.nBitRate = 6700;
          break;
        case OMX_AUDIO_AMRBandModeNB4:                 /**< AMRNB Mode 4 =  7400 bps */
          sAudioAmr.nBitRate = 7400;
          break;
        case OMX_AUDIO_AMRBandModeNB5:                 /**< AMRNB Mode 5 =  7950 bps */
          sAudioAmr.nBitRate = 7950;
          break;
        case OMX_AUDIO_AMRBandModeNB6:                 /**< AMRNB Mode 6 = 10200 bps */
          sAudioAmr.nBitRate = 10200;
          break;
        case OMX_AUDIO_AMRBandModeNB7:                /**< AMRNB Mode 7 = 12200 bps */
          sAudioAmr.nBitRate = 12200;
          break;
        case OMX_AUDIO_AMRBandModeWB0:                 /**< AMRWB Mode 0 =  6600 bps */
          sAudioAmr.nBitRate = 6600; 
          break;
        case OMX_AUDIO_AMRBandModeWB1:                 /**< AMRWB Mode 1 =  8840 bps */
          sAudioAmr.nBitRate = 8840;
          break;
        case OMX_AUDIO_AMRBandModeWB2:                 /**< AMRWB Mode 2 =  12650 bps */
          sAudioAmr.nBitRate = 12650;
          break;
        case OMX_AUDIO_AMRBandModeWB3:                 /**< AMRWB Mode 3 =  14250 bps */
          sAudioAmr.nBitRate = 14250;
          break;
        case OMX_AUDIO_AMRBandModeWB4:                 /**< AMRWB Mode 4 =  15850 bps */
          sAudioAmr.nBitRate = 15850;
          break;
        case OMX_AUDIO_AMRBandModeWB5:                 /**< AMRWB Mode 5 =  18250 bps */
          sAudioAmr.nBitRate = 18250;
          break;
        case OMX_AUDIO_AMRBandModeWB6:                 /**< AMRWB Mode 6 = 19850 bps */
          sAudioAmr.nBitRate = 19850;
          break;
        case OMX_AUDIO_AMRBandModeWB7:                /**< AMRWB Mode 7 = 23050 bps */
          sAudioAmr.nBitRate = 23050;
          break;
        case OMX_AUDIO_AMRBandModeWB8:                /**< AMRWB Mode 8 = 23850 bps */
          sAudioAmr.nBitRate = 23850;
          break;
        default:
          DEBUG(DEB_LEV_ERR, "In %s AMR Band Mode %x Not Supported\n",__func__,sAudioAmr.eAMRBandMode); 
          display_help();
        }
        sAudioAmr.eAMRBandMode = bandmode;
        DEBUG(DEFAULT_MESSAGES, "Setting OMX_IndexParamAudioAmr Band Mode=%x\n",(int)sAudioAmr.eAMRBandMode);

        err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamAudioAmr, &sAudioAmr);
        if(err!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
        }
                               }
      break;
    default :
      DEBUG(DEB_LEV_ERR, "Audio format not supported\n");
      break;
    }
  }

  /** if tunneling option is given then set up the tunnel between the components */
  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    err = OMX_SetupTunnel(appPriv->audiosrchandle, 0, appPriv->audioenchandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel between audio source & encoder component Failed\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES, "Set up Tunnel Completed\n");
  }  

  if(flagIsUseAudioSource) {
    err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  if(!flagSetupTunnel && flagIsUseAudioSource) {
    inBuffer[0] = inBuffer[1] = outBuffer[0] = outBuffer[1] = outSrcBuffer[0]= outSrcBuffer[1]=NULL;
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &outSrcBuffer[0], 0, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 1%i\n", err);
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &outSrcBuffer[1], 0, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 2 %i\n", err);
      exit(1);
    } 
    err = OMX_UseBuffer(appPriv->audioenchandle, &inBuffer[0], 0, NULL, BUFFER_IN_SIZE, outSrcBuffer[0]->pBuffer);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on UseBuffer in 1%i\n", err);
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->audioenchandle, &inBuffer[1], 0, NULL, BUFFER_IN_SIZE, outSrcBuffer[1]->pBuffer);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on UseBuffer in 2 %i\n", err);
      exit(1);
    } 
  } else {
    err = OMX_AllocateBuffer(appPriv->audioenchandle, &inBuffer[0], 0, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 1%i\n", err);
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audioenchandle, &inBuffer[1], 0, NULL, BUFFER_IN_SIZE);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer in 2 %i\n", err);
      exit(1);
    } 
  }

  err = OMX_AllocateBuffer(appPriv->audioenchandle, &outBuffer[0], 1, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 1 %i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(appPriv->audioenchandle, &outBuffer[1], 1, NULL, BUFFER_IN_SIZE);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on AllocateBuffer out 2 %i\n", err);
    exit(1);
  }

  if(flagIsUseAudioSource) {
    tsem_down(appPriv->audiosrcEventSem);
  }

  tsem_down(appPriv->eventSem);

  if(flagIsUseAudioSource) {
    err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  }
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);
  if(flagIsUseAudioSource) {
    tsem_down(appPriv->audiosrcEventSem);
  }

  if(!flagSetupTunnel && !flagIsUseAudioSource) {
    DEBUG(DEB_LEV_PARAMS, "Had buffers at:\n0x%08x\n0x%08x\n0x%08x\n0x%08x\n", 
                  (int)inBuffer[0]->pBuffer, (int)inBuffer[1]->pBuffer, (int)outBuffer[0]->pBuffer, (int)outBuffer[1]->pBuffer);
    DEBUG(DEB_LEV_PARAMS, "After switch to executing\n");

    data_read1 = read(fd, inBuffer[0]->pBuffer, FRAME_SIZE);
    inBuffer[0]->nFilledLen = data_read1;
    filesize -= data_read1;

    data_read2 = read(fd, inBuffer[1]->pBuffer, FRAME_SIZE);
    inBuffer[1]->nFilledLen = data_read2;
    filesize -= data_read2;

    DEBUG(DEB_LEV_PARAMS, "Empty first  buffer %x\n", (int)inBuffer[0]);
    err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[0]);
    DEBUG(DEB_LEV_PARAMS, "Empty second buffer %x\n", (int)inBuffer[1]);
    err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[1]);
  }
  
  /** Schedule a couple of buffers to be filled on the output port
    * The callback itself will re-schedule them.
    */
  err = OMX_FillThisBuffer(appPriv->audioenchandle, outBuffer[0]);
  err = OMX_FillThisBuffer(appPriv->audioenchandle, outBuffer[1]);

  if(flagIsUseAudioSource) {
    err = OMX_FillThisBuffer(appPriv->audiosrchandle, outSrcBuffer[0]);
    err = OMX_FillThisBuffer(appPriv->audiosrchandle, outSrcBuffer[1]);

    DEBUG(DEFAULT_MESSAGES,"Enter 'q' or 'Q' to exit\n");

    while(1) {
      if('Q' == toupper(getchar())) {
        DEBUG(DEFAULT_MESSAGES,"Stoping capture\n");
        bEOS = OMX_TRUE;
        usleep(10000);
        break;
      }
    }
  } 

  DEBUG(DEFAULT_MESSAGES,"Waiting for EOS\n");
  tsem_down(appPriv->eofSem);
  DEBUG(DEFAULT_MESSAGES,"Received for EOS\n");
  

  if(flagIsUseAudioSource) {
    err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);
  if(flagIsUseAudioSource) {
    tsem_down(appPriv->audiosrcEventSem);
  }

  DEBUG(DEFAULT_MESSAGES,"All component transitioned to Idle\n");

  if(flagIsUseAudioSource) {
    err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if(!flagSetupTunnel && flagIsUseAudioSource) {
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, outSrcBuffer[0]);
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, outSrcBuffer[1]);
  } 
  
  if(!flagSetupTunnel) {
    err = OMX_FreeBuffer(appPriv->audioenchandle, 0, inBuffer[0]);
    err = OMX_FreeBuffer(appPriv->audioenchandle, 0, inBuffer[1]);
  }
  
  err = OMX_FreeBuffer(appPriv->audioenchandle, 1, outBuffer[0]);
  err = OMX_FreeBuffer(appPriv->audioenchandle, 1, outBuffer[1]);

  /* Wait for commands to complete */
  tsem_down(appPriv->eventSem);
  if(flagIsUseAudioSource) {
    tsem_down(appPriv->audiosrcEventSem);
  }
  DEBUG(DEFAULT_MESSAGES,"All component transitioned to Loaded\n");

  
  OMX_FreeHandle(appPriv->audioenchandle);

  if(flagIsUseAudioSource) {
    OMX_FreeHandle(appPriv->audiosrchandle);
    free(appPriv->audiosrcEventSem);
  }

  free(appPriv->eventSem);
  free(appPriv);

  if(!flagIsUseAudioSource) {
    DEBUG(DEFAULT_MESSAGES,"Closing infile\n");
    close(fd);
  }
  if (flagOutputReceived) {
    DEBUG(DEFAULT_MESSAGES,"Closing outfile\n");
    if(fclose(outfile) != 0) {
      DEBUG(DEB_LEV_ERR,"Error in closing output file\n");
      exit(1);
    }
    DEBUG(DEFAULT_MESSAGES,"Freeing output_file\n");
    if(output_file) {
      free(output_file);
    }
  }
  
  if(input_file) {
    DEBUG(DEFAULT_MESSAGES,"Freeing input_file\n");
    free(input_file);
    DEBUG(DEFAULT_MESSAGES,"Freed input_file\n");
  }

  return 0;
}

/* Callbacks implementation */
OMX_ERRORTYPE audioencEventHandler(
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

OMX_ERRORTYPE audioencEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  int data_read;
  static int iBufferDropped=0;

  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);
  if(!flagIsUseAudioSource) {
    data_read = read(fd, pBuffer->pBuffer, FRAME_SIZE);
    pBuffer->nFilledLen = data_read;
    pBuffer->nOffset = 0;
    filesize -= data_read;
    if (data_read <= 0) {
      DEBUG(DEB_LEV_ERR, "In the %s no more input data available\n", __func__);
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
      DEBUG(DEB_LEV_ERR, "In %s Dropping Empty This buffer to Audio Encoder\n", __func__);
    }
  } else {
    if(!bEOS) {
      if(outSrcBuffer[0]->pBuffer == pBuffer->pBuffer) {
        outSrcBuffer[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, outSrcBuffer[0]);
      } else {
        outSrcBuffer[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, outSrcBuffer[1]);
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
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE audioencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  int i;  

  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback. Got buflen %i for buffer at 0x%08x\n",
                          __func__, (int)pBuffer->nFilledLen, (int)pBuffer);

  /* Output data to standard output */
  if(pBuffer->nFlags != OMX_BUFFERFLAG_EOS) {
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
  }
  /* Reschedule the fill buffer request */
  if(pBuffer->nFlags != OMX_BUFFERFLAG_EOS) {
    err = OMX_FillThisBuffer(hComponent, pBuffer);
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
      tsem_up(appPriv->audiosrcEventSem);
    }
    else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->audiosrcEventSem);
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
  static int iBufferDropped=0;
  
  if(pBuffer != NULL) {
    if(!bEOS) {
      if(!flagSetupTunnel) {
          if(inBuffer[0]->pBuffer == pBuffer->pBuffer) {
            inBuffer[0]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[0]);
          } else {
            inBuffer[1]->nFilledLen = pBuffer->nFilledLen;
            err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[1]);
          }
          if(err != OMX_ErrorNone) {
            DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
          }
      } else if((pBuffer->nFilledLen > 0) && (!flagSetupTunnel)) {
        fwrite(pBuffer->pBuffer, 1,  pBuffer->nFilledLen, outfile);    
        pBuffer->nFilledLen = 0;

        if(!bEOS && !flagSetupTunnel) {
          err = OMX_FillThisBuffer(hComponent, pBuffer);
        }
      }
      if(pBuffer->nFlags == OMX_BUFFERFLAG_EOS) {
        DEBUG(DEB_LEV_ERR, "In %s: eos=%x Calling Empty This Buffer\n", __func__, (int)pBuffer->nFlags);
        bEOS = OMX_TRUE;
      }
      
    } else {
      iBufferDropped++;
      if(iBufferDropped>=2) {
        //tsem_up(appPriv->eofSem);
        return OMX_ErrorNone;
      }
      pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
      if(inBuffer[0]->pBuffer == pBuffer->pBuffer) {
        inBuffer[0]->nFilledLen = pBuffer->nFilledLen;
        inBuffer[0]->nFlags = OMX_BUFFERFLAG_EOS;
        err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[0]);
      } else {
        inBuffer[1]->nFilledLen = pBuffer->nFilledLen;
        inBuffer[1]->nFlags = OMX_BUFFERFLAG_EOS;
        err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBuffer[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
      return OMX_ErrorNone;
      DEBUG(DEB_LEV_ERR, "In %s: eos=%x Dropping Empty This Buffer\n", __func__,(int)pBuffer->nFlags);
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
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
