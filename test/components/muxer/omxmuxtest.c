/**
  @file test/components/parser/omxparsertest.c

  Test application uses following OpenMAX components, 
    mux  -- 3gp file muxer ,
    audio encoder pipeline  - alsa src and audio encoder
    video encoder pipeline  - video src and video encoder
  
  Test application that uses five OpenMAX components, a mux, an audio encoder,
  a video deocder, an alsa src and a video src. The application capture audio and video stream
  using a mic and camera then encode it using audio and video encoders.
  And sends the encoded data to mux component to write into a file. Only components
  based on ffmpeg library are supported.
  The video formats supported are:
    MPEG4 (ffmpeg)
  The audio formats supported are:
    amr (ffmpeg)

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

#include "omxmuxtest.h"

#define MPEG4_TYPE_SEL            1
#define VIDEO_COMPONENT_NAME_BASE "OMX.st.video_encoder"
#define AUDIO_COMPONENT_NAME_BASE "OMX.st.audio_encoder" 
#define VIDEO_ENC_MPEG4_ROLE      "video_encoder.mpeg4"
#define AUDIO_ENC_AMR_ROLE        "audio_encoder.amr"
#define VIDEO_SRC                 "OMX.st.video_src"
#define AUDIO_SRC                 "OMX.st.alsa.alsasrc"
#define MUX_3GP                   "OMX.st.mux.3gp"
#define CLOCK_SRC                 "OMX.st.clocksrc"
#define COMPONENT_NAME_BASE_LEN   20
#define extradata_size            1024
#define VIDEO_PORT_INDEX          0  /* video port index on clock component */
#define AUDIO_PORT_INDEX          1  /* audio port index on clock component */
#define MUX_PORT_INDEX            2  /* parser port index on clock component */
#define CLIENT_CLOCK_PORT_INDEX   1  /* clock port index on src (Audio/Video) component */
#define MUX_CLOCK_PORT_INDEX      2  /* clock port index on parser component */

OMX_BUFFERHEADERTYPE *inBufferMuxVideo[2], *inBufferMuxAudio[2];
OMX_BUFFERHEADERTYPE *inBufferVideoEnc[2], *outBufferVideoEnc[2];
OMX_BUFFERHEADERTYPE *inBufferAudioEnc[2],*outBufferAudioEnc[2];
OMX_BUFFERHEADERTYPE *outBufferSrcAudio[2],*outBufferSrcVideo[2];

int buffer_in_size = BUFFER_IN_SIZE; 
int buffer_out_size = BUFFER_OUT_SIZE;
static OMX_BOOL bEOS=OMX_FALSE;

static OMX_PARAM_PORTDEFINITIONTYPE paramPortVideo, paramPortAudio;
OMX_PARAM_PORTDEFINITIONTYPE encparamPortVideo;
OMX_PARAM_PORTDEFINITIONTYPE encparamPortAudio;

OMX_CALLBACKTYPE videoenccallbacks = { 
  .EventHandler = videoencEventHandler,
  .EmptyBufferDone = videoencEmptyBufferDone,
  .FillBufferDone = videoencFillBufferDone
};

OMX_CALLBACKTYPE videosrc_callbacks = { 
  .EventHandler = videosrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone = videosrcFillBufferDone
};

OMX_CALLBACKTYPE muxcallbacks = { 
  .EventHandler    = muxEventHandler,
  .EmptyBufferDone = muxEmptyBufferDone,
  .FillBufferDone  = NULL
};

OMX_CALLBACKTYPE clocksrccallbacks = {
  .EventHandler    = clocksrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = clocksrcFillBufferDone
};

OMX_CALLBACKTYPE audioenccallbacks = {
  .EventHandler    = audioencEventHandler,
  .EmptyBufferDone = audioencEmptyBufferDone,
  .FillBufferDone  = audioencFillBufferDone
};

OMX_CALLBACKTYPE audiosrccallbacks = {
  .EventHandler    = audiosrcEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = audiosrcFillBufferDone,
};

OMX_U32 out_width  = 0;  
OMX_U32 out_height = 0;
OMX_U32 frame_rate = 10, channel = 1, rate = 8000;
OMX_VIDEO_CODINGTYPE eCompressionFormat = OMX_VIDEO_CodingMPEG4;
int bandmode = OMX_AUDIO_AMRBandModeNB7;

appPrivateType* appPriv;

char *output_file, *output_format_audio, *output_format_video;

FILE *outfileAudio, *outfileVideo, *outfile3Gp;

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

/** this function sets the audio deocder and audio src port characteristics 
  * based on the mux output port settings 
  */
int SetPortParametersAudio() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_AMRTYPE sAudioAmr;
  OMX_AUDIO_PARAM_PCMMODETYPE sPCMModeParam;

  setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  sPCMModeParam.nPortIndex = 0;
  err = OMX_GetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
    return err; 
  }
  sPCMModeParam.nChannels = (channel >0 ) ? channel:sPCMModeParam.nChannels;
  sPCMModeParam.nSamplingRate = (rate >0 ) ? rate:sPCMModeParam.nSamplingRate;
  err = OMX_SetParameter(appPriv->audiosrchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
    return err; 
  }

  sPCMModeParam.nPortIndex = 0;
  err = OMX_GetParameter(appPriv->audioenchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
    return err; 
  }
  sPCMModeParam.nChannels = (channel >0 ) ? channel:sPCMModeParam.nChannels;
  sPCMModeParam.nSamplingRate = (rate >0 ) ? rate:sPCMModeParam.nSamplingRate;
  err = OMX_SetParameter(appPriv->audioenchandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
    return err; 
  }

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
    sAudioAmr.nBitRate = 7900;
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
    return err; 
  }
  sAudioAmr.eAMRBandMode = bandmode;
  err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamAudioAmr, &sAudioAmr);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 0 \n",err);
  }

  paramPortAudio.nPortIndex = 0;
  setHeader(&paramPortAudio, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->muxhandle, OMX_IndexParamPortDefinition, &paramPortAudio);
  paramPortAudio.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
  err = OMX_SetParameter(appPriv->muxhandle, OMX_IndexParamPortDefinition, &paramPortAudio);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
    return err; 
  } 

  sAudioAmr.nPortIndex=1;
  err = OMX_SetParameter(appPriv->muxhandle, OMX_IndexParamAudioAmr, &sAudioAmr);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetParameter 1 \n",err);
  }

  return err;
}

int SetPortParametersVideo() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  
  DEBUG(DEB_ALL_MESS,"In %s Compression format=%x\n",__func__,eCompressionFormat);

  paramPortVideo.nPortIndex = 0;
  setHeader(&paramPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videosrchandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  paramPortVideo.format.video.nFrameWidth  = out_width;
  paramPortVideo.format.video.nFrameHeight = out_height;
  err = OMX_SetParameter(appPriv->videosrchandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
    return err; 
  } 
  
  paramPortVideo.nPortIndex = 0;
  setHeader(&paramPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  paramPortVideo.format.video.eCompressionFormat = eCompressionFormat;
  paramPortVideo.format.video.nFrameWidth  = out_width;
  paramPortVideo.format.video.nFrameHeight = out_height;
  paramPortVideo.format.video.xFramerate   = frame_rate;
  err = OMX_SetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
    return err; 
  } 

  paramPortVideo.nPortIndex = 0;
  setHeader(&paramPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->muxhandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  paramPortVideo.format.video.eCompressionFormat = eCompressionFormat;
  paramPortVideo.format.video.nFrameWidth  = out_width;
  paramPortVideo.format.video.nFrameHeight = out_height;
  paramPortVideo.format.video.xFramerate   = frame_rate;
  err = OMX_SetParameter(appPriv->muxhandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  if(err!=OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Setting Input Port Definition Error=%x\n",__func__,err); 
    return err; 
  } 
  DEBUG(DEB_LEV_SIMPLE_SEQ, "input picture width : %d height : %d \n", (int)out_width, (int)out_height);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "input picture width : %d height : %d \n", (int)out_width, (int)out_height);

  return err;
}

/** this function sets the video deocder and video src port characteristics 
  * based on the mux output port settings 
  */
int SetPortParametersVideo_old() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  
  // getting port parameters from mux component //
  paramPortVideo.nPortIndex = VIDEO_PORT_INDEX; //video port of the mux
  setHeader(&paramPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->muxhandle, OMX_IndexParamPortDefinition, &paramPortVideo);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"\n error in mux port settings get\n");  
    exit(1);
  }

 // setting the port parameters of video encoder
 // input port settings
  encparamPortVideo.nPortIndex = 0; /* input port of the video encoder */
  setHeader(&encparamPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &encparamPortVideo);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"\n error in videoenchandle settings get\n");  
    exit(1);
  }

  encparamPortVideo.format.video.eCompressionFormat=paramPortVideo.format.video.eCompressionFormat;
  encparamPortVideo.format.video.nFrameWidth=paramPortVideo.format.video.nFrameWidth;
  encparamPortVideo.format.video.nFrameHeight=paramPortVideo.format.video.nFrameHeight; 
  err = OMX_SetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &encparamPortVideo);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x setting videoenc input port param --- \n",err);
    exit(1);
  } 

 // output port settings
  encparamPortVideo.nPortIndex = 1;
  setHeader(&encparamPortVideo, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  err = OMX_GetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &encparamPortVideo);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"\n error in videoenchandle settings get\n");
    exit(1);
  }

  encparamPortVideo.format.video.eCompressionFormat=paramPortVideo.format.video.eCompressionFormat;
  encparamPortVideo.format.video.nFrameWidth=paramPortVideo.format.video.nFrameWidth;
  encparamPortVideo.format.video.nFrameHeight=paramPortVideo.format.video.nFrameHeight; 
  err = OMX_SetParameter(appPriv->videoenchandle, OMX_IndexParamPortDefinition, &encparamPortVideo);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Error %08x setting videoenc output port param --- \n",err);
    exit(1);
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "input picture width : %d height : %d \n", (int)out_width, (int)out_height);

  return err;
}

void display_help() {
  printf("\n");
  printf("Usage: omxmuxtest -vf MPEG4 -af AMRNB -o outfile.3gp\n");
  printf("\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  //printf("       -c: clock component selected AVsync ON\n");
  printf("\n");
  printf("       -r 8000  : Sample Rate\n");
  printf("       -vf MPEG4: Use Video format\n");
  printf("       -af AMR  : Use Audio format\n");
  printf("       -m [NB0..NB7,WB0..WB8]: AMR Band [Default OMX_AUDIO_AMRBandModeNB7]\n");
  printf("          [OMX_AUDIO_AMRBandModeNB0,..,OMX_AUDIO_AMRBandModeNB7, OMX_AUDIO_AMRBandModeWB0,..,OMX_AUDIO_AMRBandModeWB8]\n");
  printf("           \n\n");
  printf("       -t: Tunneling option - if this option is selected then by default \n");
  printf("                              the components are tunneled between themselves\n");
  printf("\n");
  exit(1);
} 

int flagIsAudioFormatExpected;       /* to write the audio output to a file */
int flagIsVideoFormatExpected;       /* to write the video output to a file */
int flagEncodedOutputReceived;  
int flagEncodedOutputExpected;  
int flagSetupTunnel;
int flagAVsync;                 /* to select the AVsync option 1 = AV sync ON, clock component selected, 0 = no clock component selected*/
int flagBandMode;
int flagIsRate;

int main(int argc, char** argv) {
  int argn_enc;
  OMX_ERRORTYPE                       err;
  OMX_INDEXTYPE                       eIndexParamFilename;
  OMX_PARAM_COMPONENTROLETYPE         sComponentRole; 
  OMX_TIME_CONFIG_CLOCKSTATETYPE      sClockState;
  OMX_TIME_CONFIG_SCALETYPE           sConfigScale;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  sRefClock;
  char keyin;
  OMX_S32  newscale=0;
  char cAudioComponentName[OMX_MAX_STRINGNAME_SIZE];


  strcpy(&cAudioComponentName[0],"OMX.st.audio_encoder.amrnb");

  if(argc < 2){
    display_help();
  } else {
    flagIsVideoFormatExpected = 0;
    flagIsAudioFormatExpected = 0;
    flagEncodedOutputReceived = 0;
    flagEncodedOutputExpected = 0;
    flagSetupTunnel           = 0;
    flagAVsync                = 0;
    flagBandMode              = 0;
    flagIsRate                = 0;

    argn_enc = 1;
    while (argn_enc < argc) {
      if (*(argv[argn_enc]) == '-') {
        if (flagIsVideoFormatExpected || flagIsAudioFormatExpected) {
          display_help();
        }
        switch (*(argv[argn_enc] + 1)) {
        case 'h' :
          display_help();
          break;
        case 't' :
          flagSetupTunnel = 1;
          break;
        case 'c':
          flagAVsync = 1;
          break;
        case 'a' :
          if(*(argv[argn_enc] + 2)=='f'){
            flagIsAudioFormatExpected = 1;
          }
          break;
        case 'v' :
          if(*(argv[argn_enc] + 2)=='f'){
            flagIsVideoFormatExpected = 1;
          }
          break;
        case 'o':
          flagEncodedOutputExpected = 1;
          break;
        case 'm':
          flagBandMode = 1;
          break;
        case 'r':
          flagIsRate = 1;
          break;
        default:
          display_help();
        }
      } else {
        if (flagIsVideoFormatExpected) {
          if(strstr(argv[argn_enc], "MPEG4") != NULL) {
            eCompressionFormat = OMX_VIDEO_CodingMPEG4;
          } else {
            DEBUG(DEB_LEV_ERR, "\n Output Video Format Not Supported\n");
            display_help();
          }
          flagIsVideoFormatExpected = 0;
        } else if (flagIsAudioFormatExpected){
          if (strstr(argv[argn_enc], "AMR") == NULL) {
            DEBUG(DEB_LEV_ERR, "\n Output Audio Format Not Supported\n");
            display_help();
          }
          flagIsAudioFormatExpected = 0;
        } else if(flagEncodedOutputExpected) {
          output_file = malloc(strlen(argv[argn_enc]) * sizeof(char) + 1);
          strcpy(output_file,argv[argn_enc]);
          flagEncodedOutputExpected = 0;
        } else if(flagBandMode) {
          flagBandMode = 0;
          char *str = strdup(argv[argn_enc]);
          DEBUG(DEFAULT_MESSAGES, "Band Mode %s %c\n", str,str[2]);
          bandmode = (int)atoi(&str[2]);
          if(strncmp(str,"NB",2)==0) {
            bandmode += 1;
          } else if(strncmp(str,"WB",2)==0) {
            strcpy(&cAudioComponentName[0],"OMX.st.audio_encoder.amrwb");
            bandmode += 9;
          }
          DEBUG(DEFAULT_MESSAGES, "Band Mode %d\n", bandmode);
        } else if (flagIsRate) {
          rate = (int)atoi(argv[argn_enc]);
          flagIsRate = 0;
          if(rate < 8000) {
            DEBUG(DEFAULT_MESSAGES, "Rate should be between [8000..16000]\n");
            rate = 8000; 
          } else if(rate > 16000) {
            DEBUG(DEFAULT_MESSAGES, "Rate should be between [8000..16000]\n");
            rate = 16000; 
          }
        } 
      }
      argn_enc++;
    }

    /** input file name check */
    if ((output_file == NULL) || (strstr(output_file, ".3gp") == NULL)) {
      DEBUG(DEB_LEV_ERR, "\n you must specify appropriate output file of .3gp format\n");
      display_help();
    }

    /** output file name check */
    //case 1 - user did not specify output file names
    if(flagIsAudioFormatExpected || flagIsVideoFormatExpected) {
       DEBUG(DEB_LEV_ERR, "\n you must specify appropriate output audio and video file names \n");
       display_help();
    }

    DEBUG(DEFAULT_MESSAGES, "Options selected:\n");
    if(flagEncodedOutputReceived) {
      DEBUG(DEFAULT_MESSAGES, "Encode file %s to produce audio file %s and video file %s\n", output_file, output_format_audio, output_format_video);
    } else {
      DEBUG(DEFAULT_MESSAGES, "As audio and video src components are chosen, no output file is generated even if specified\n");
    }
    if(flagSetupTunnel) {
      DEBUG(DEFAULT_MESSAGES,"The components are tunneled between themselves\n");
    }
  }

  /** setting input picture width to a default value (vga format) for allocation of video encoder buffers */
  out_width = 320;
  out_height = 240;

  /** initializing appPriv structure */
  appPriv = malloc(sizeof(appPrivateType));
  appPriv->muxEventSem = malloc(sizeof(tsem_t));
  appPriv->videoencEventSem = malloc(sizeof(tsem_t));
  appPriv->audioencEventSem = malloc(sizeof(tsem_t));
  appPriv->videosrcEventSem = malloc(sizeof(tsem_t));
  appPriv->audiosrcEventSem = malloc(sizeof(tsem_t));
  appPriv->eofSem = malloc(sizeof(tsem_t));
  
  if(flagAVsync) {
    appPriv->clockEventSem = malloc(sizeof(tsem_t));
  } 

  tsem_init(appPriv->muxEventSem, 0);
  tsem_init(appPriv->videoencEventSem, 0);
  tsem_init(appPriv->audioencEventSem, 0);
  tsem_init(appPriv->videosrcEventSem, 0);
  tsem_init(appPriv->audiosrcEventSem, 0);
  tsem_init(appPriv->eofSem, 0); 
  if(flagAVsync) {
    tsem_init(appPriv->clockEventSem, 0); 
  }

  /** initialising openmax */
  err = OMX_Init();
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
    exit(1);
  }else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Omx core is initialized \n");
  }

  DEBUG(DEFAULT_MESSAGES, "------------------------------------\n");

  DEBUG(DEFAULT_MESSAGES, "The component selected for video encoding is %s\n Role is encided by the encoder\n", VIDEO_COMPONENT_NAME_BASE);
  DEBUG(DEFAULT_MESSAGES, "The component selected for audio encoding is %s\n Role is encided by the encoder\n", AUDIO_COMPONENT_NAME_BASE);

  /** mux component --  gethandle*/
  err = OMX_GetHandle(&appPriv->muxhandle, MUX_3GP, NULL /*appPriv */, &muxcallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Mux Component %s Not Found\n",MUX_3GP);
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "Mux Component %s Found\n",MUX_3GP);
  } 

  /* getting the handle to the clock src component */
  if(flagAVsync){
    err = OMX_GetHandle(&appPriv->clocksrchandle, CLOCK_SRC, NULL, &clocksrccallbacks);
    if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "Clocksrc Component Not Found\n");
         exit(1);
    } else {
        DEBUG(DEFAULT_MESSAGES, "Clocksrc Component Found\n");
    }
  }

  /* getting the handles for the components in video  and audio piepline*/
  /** getting video encoder handle */
  err = OMX_GetHandle(&appPriv->videoenchandle, VIDEO_COMPONENT_NAME_BASE, NULL, &videoenccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No video encoder component found. Exiting...\n");
    exit(1);
  } else {
    DEBUG(DEFAULT_MESSAGES, "The component for video encoding is %s\n", VIDEO_COMPONENT_NAME_BASE);
  }

  /** getting audio encoder handle */
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Getting Audio %s Encoder Handle\n",AUDIO_COMPONENT_NAME_BASE);
  err = OMX_GetHandle(&appPriv->audioenchandle, cAudioComponentName, NULL, &audioenccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Audio Encoder Component Not Found\n");
    exit(1);
  }
  DEBUG(DEFAULT_MESSAGES, "The Component for audio deoding is %s \n", AUDIO_COMPONENT_NAME_BASE);

  /** getting handle of other components in audio and video pipeline, if specified */
  /** getting video src component handle */
  err = OMX_GetHandle(&appPriv->videosrchandle, VIDEO_SRC, NULL, &videosrc_callbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No video src component component found. Exiting...\n");
     exit(1);
  } else {
     DEBUG(DEFAULT_MESSAGES, "Found The video src component for video src \n");
  }

  /** getting audio src component handle */
  err = OMX_GetHandle(&appPriv->audiosrchandle, AUDIO_SRC, NULL , &audiosrccallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No audio src found. Exiting...\n");
    exit(1);
  } else {
     DEBUG(DEFAULT_MESSAGES, "Found The alsa src component for audio \n");
  }


  /* disable the clock ports of the clients (audio, video and mux), if AVsync not enabled*/
  if(flagAVsync) {
    err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandPortEnable, 2, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"mux clock port Enable failed\n");
      exit(1);
    }
    tsem_down(appPriv->muxEventSem); /* mux clock port Enabled */
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s mux Clock Port Enabled\n", __func__);

    //if(flagIsDisplayRequested){
      err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandPortEnable, 1, NULL);
      if(err != OMX_ErrorNone) {
         DEBUG(DEB_LEV_ERR,"audiosrc clock port Enable failed\n");
         exit(1);
       }
       tsem_down(appPriv->audiosrcEventSem); /* audio src clock port Enabled */
       DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Audio Sink Clock Port Enabled\n", __func__);
   
      err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandPortEnable, 1, NULL);
      if(err != OMX_ErrorNone) {
         DEBUG(DEB_LEV_ERR,"videosrc clock port Enable failed\n");
         exit(1);
       }
       tsem_down(appPriv->videosrcEventSem); /* video src clock port Enabled */
       DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Video Sink Clock Port Enabled\n", __func__);
    //}
  }

  /** setting the input format in mux */
  err = OMX_GetExtensionIndex(appPriv->muxhandle,"OMX.ST.index.param.outputfilename",&eIndexParamFilename);
  if(err != OMX_ErrorNone) {
     DEBUG(DEB_LEV_ERR,"\n error in get extension index\n");
    exit(1);
  } else {
    char *temp = malloc(128);
    DEBUG(DEB_LEV_SIMPLE_SEQ,"FileName Param index : %x \n",eIndexParamFilename);
    OMX_GetParameter(appPriv->muxhandle,eIndexParamFilename,temp);
    err = OMX_SetParameter(appPriv->muxhandle,eIndexParamFilename,output_file);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"\n error in input format - exiting\n");
      exit(1);
    }
    free(temp);
  }

  /* Set up the tunnel between the clock component ports and the client clock ports and disable the rest of the ports */
  if(flagAVsync) {
    err = OMX_SetupTunnel(appPriv->clocksrchandle, AUDIO_PORT_INDEX, appPriv->audiosrchandle, CLIENT_CLOCK_PORT_INDEX);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel btwn clock and audio src Failed Error=%x\n",err);
      exit(1);
    } else{
      DEBUG(DEB_LEV_ERR, "Setup Tunnel between clock and audio src successful\n");
    }

    err = OMX_SetupTunnel(appPriv->clocksrchandle, VIDEO_PORT_INDEX, appPriv->videosrchandle, CLIENT_CLOCK_PORT_INDEX);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel btwn clock and video src Failed\n");
      exit(1);
    } else{
      DEBUG(DEB_LEV_ERR, "Setup Tunnel between clock and video src successful\n");
    }

    err = OMX_SetupTunnel(appPriv->clocksrchandle, MUX_PORT_INDEX, appPriv->muxhandle, MUX_CLOCK_PORT_INDEX);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel btwn clock and mux Failed\n");
      exit(1);
    } else{
      DEBUG(DEB_LEV_ERR, "Setup Tunnel between clock and mux successful\n");
    }

    if(!flagSetupTunnel){
      /* disable the clock port on parser and the clock port on the clock component tunneled to the parser, till clock is put in Idle state*/
      err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandPortDisable, 2, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"parser clock port disable failed\n");
        exit(1);
      }
      tsem_down(appPriv->muxEventSem); /* parser clock port disabled */
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s parser Clock Port Disabled\n", __func__);

      err = OMX_SendCommand(appPriv->clocksrchandle, OMX_CommandPortDisable, 2, NULL);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"clocksrc component's clock  port (tunneled to parser's clock port) disable failed\n");
        exit(1);
      }
      tsem_down(appPriv->clockEventSem); /* clocksrc clock port disabled */
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s clocksrc  Clock Port (connected to parser) Disabled\n", __func__);
    }
  }

  if (flagSetupTunnel) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Setting up Tunnel\n");
    err = OMX_SetupTunnel(appPriv->videosrchandle, 0, appPriv->videoenchandle, 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel video src to video encoder Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->audiosrchandle , 0, appPriv->audioenchandle , 0);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel audio src to encoder Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->videoenchandle, 1, appPriv->muxhandle, VIDEO_PORT_INDEX);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel video encoder to mux Failed\n");
      exit(1);
    }
    err = OMX_SetupTunnel(appPriv->audioenchandle, 1, appPriv->muxhandle, AUDIO_PORT_INDEX);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Set up Tunnel audio encoder to mux Failed\n");
      exit(1);
    }
    
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Set up Tunnel Completed\n");

    /* The encoder is the buffer supplier and parser is the consumer,
     not the other way round. This ensures that the first buffer is available when the encoder is in idle state. 
     This prevents the loss of the first frame buffer */
    /*
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;
    sBufferSupplier.nPortIndex = 0;
    sBufferSupplier.eBufferSupplier = OMX_BufferSupplyInput;
    setHeader(&sBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));

    err = OMX_SetParameter(appPriv->videoenchandle, OMX_IndexParamCompBufferSupplier, &sBufferSupplier);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Setting Video Encoder Input Port Buffer Supplier Error=%x\n",__func__,err);
      return err;
    }
    err = OMX_SetParameter(appPriv->audioenchandle, OMX_IndexParamCompBufferSupplier, &sBufferSupplier);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Setting Audio Encoder Input Port Buffer Supplier Error=%x\n",__func__,err);
      return err;
    }
    */
  }

  /*  setting the port parameters and component role */
  if(OMX_ErrorNone != SetPortParametersVideo()) {
    exit(1);
  }
  setHeader(&sComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
  err = OMX_GetParameter(appPriv->videoenchandle,OMX_IndexParamStandardComponentRole,&sComponentRole); 
  switch(eCompressionFormat){
  case OMX_VIDEO_CodingMPEG4:
    strcpy((char*)&sComponentRole.cRole[0], VIDEO_ENC_MPEG4_ROLE);  
    break;
  default :
    DEBUG(DEB_LEV_ERR,"Error only MPEG4 is supported %08x\n", eCompressionFormat); 
  }
  err = OMX_SetParameter(appPriv->videoenchandle,OMX_IndexParamStandardComponentRole,&sComponentRole);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"\n error in input video format - exiting\n");
    exit(1);
  }

  /* setting the component role for the audio component */
  if(OMX_ErrorNone != SetPortParametersAudio()) {
    exit(1);
  }
  /*
  setHeader(&sComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
  err = OMX_GetParameter(appPriv->audioenchandle, OMX_IndexParamStandardComponentRole,&sComponentRole); 
  switch(paramPortAudio.format.audio.eEncoding) {
  case OMX_AUDIO_CodingMP3:
    strcpy((char*)&sComponentRole.cRole[0], AUDIO_ENC_MP3_ROLE);  
    break;
  case OMX_AUDIO_CodingAAC:
    strcpy((char*)&sComponentRole.cRole[0],  AUDIO_ENC_AAC_ROLE);
    break;
  default :
    DEBUG(DEB_LEV_ERR,"Error only MP3 and AAC role are supported %08x\n",paramPortAudio.format.audio.eEncoding); 
  }
  err = OMX_SetParameter(appPriv->audioenchandle,OMX_IndexParamStandardComponentRole,&sComponentRole);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"\n error in input audio format - exiting\n");
    exit(1);
  }
  */

  /** set the mux component to idle state */
  OMX_SendCommand(appPriv->muxhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  //if (flagSetupTunnel) {
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Video Enc\n");
  /*Send State Change Idle command to Video and Audio Encoder*/
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if(flagAVsync){
    err = OMX_SendCommand(appPriv->clocksrchandle,  OMX_CommandStateSet, OMX_StateIdle, NULL);
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio and Video Sink\n");
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);  /* put in the idle state after the clock is put in idle state */
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  //}

  /* In case tunnel is not set up then allocate the output buffers of the parser  
     two buffers for video port and two buffers for audio port */
  if (!flagSetupTunnel) {

    /* video src  */
    err = OMX_AllocateBuffer(appPriv->videosrchandle, &outBufferSrcVideo[0], 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->videosrchandle, &outBufferSrcVideo[1], 0, NULL, buffer_out_size);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }

    /** the output buffers of video src component will be used 
      *  in the video encoder component as input buffers 
      */ 
    err = OMX_UseBuffer(appPriv->videoenchandle, &inBufferVideoEnc[0], 0, NULL, buffer_out_size, outBufferSrcVideo[0]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the mux comp allocate buffer\n");
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->videoenchandle, &inBufferVideoEnc[1], 0, NULL, buffer_out_size, outBufferSrcVideo[1]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the mux comp allocate buffer\n");
      exit(1);
    }

    /* Allocate the output buffers of the video encoder */
    err = OMX_AllocateBuffer(appPriv->videoenchandle, &outBufferVideoEnc[0], 1, NULL, BUFFER_OUT_SIZE_VIDEO);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in video enc\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->videoenchandle, &outBufferVideoEnc[1], 1, NULL, BUFFER_OUT_SIZE_VIDEO);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in video enc\n");
      exit(1);
    }

    /* audio src  */
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &outBufferSrcAudio[0], 0, NULL, BUFFER_OUT_SIZE_AUDIO);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audiosrchandle, &outBufferSrcAudio[1], 0, NULL, BUFFER_OUT_SIZE_AUDIO);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the allocated buffer\n");
      exit(1);
    }

    /** the output buffers of audio src component will be used 
       in the audio encoder component as input buffers 
    */ 
    err = OMX_UseBuffer(appPriv->audioenchandle, &inBufferAudioEnc[0], 0, NULL, BUFFER_OUT_SIZE_AUDIO, outBufferSrcAudio[0]->pBuffer);
    if(err != OMX_ErrorNone) {
     DEBUG(DEB_LEV_ERR, "Unable to use the mux allocate buffer\n");
     exit(1);
    }    
    err = OMX_UseBuffer(appPriv->audioenchandle, &inBufferAudioEnc[1], 0, NULL, BUFFER_OUT_SIZE_AUDIO, outBufferSrcAudio[1]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to use the mux allocate buffer\n");
      exit(1);
    }    

    /* Allocate the output buffers of the audio encoder */
    err = OMX_AllocateBuffer(appPriv->audioenchandle, &outBufferAudioEnc[0], 1, NULL, BUFFER_OUT_SIZE_AUDIO);
    if(err != OMX_ErrorNone) {
     DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio enc\n");
     exit(1);
    }
    err = OMX_AllocateBuffer(appPriv->audioenchandle, &outBufferAudioEnc[1], 1, NULL, BUFFER_OUT_SIZE_AUDIO);
    if(err != OMX_ErrorNone) {
     DEBUG(DEB_LEV_ERR, "Unable to allocate buffer in audio enc\n");
     exit(1);
    }
    
  
    inBufferMuxVideo[0] = inBufferMuxVideo[1] = NULL;
    inBufferMuxAudio[0] = inBufferMuxAudio[1] = NULL;
    /** allocation of mux component's output buffers for video encoder component */
    err = OMX_UseBuffer(appPriv->muxhandle, &inBufferMuxVideo[0], VIDEO_PORT_INDEX, NULL, BUFFER_OUT_SIZE_VIDEO, outBufferVideoEnc[0]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate Video buffer 1 in mux \n");
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->muxhandle, &inBufferMuxVideo[1], VIDEO_PORT_INDEX, NULL, BUFFER_OUT_SIZE_VIDEO, outBufferVideoEnc[1]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate Video buffer 2 in mux \n");
      exit(1);
    }

    /** allocation of mux component's output buffers for audio encoder component */
    err = OMX_UseBuffer(appPriv->muxhandle, &inBufferMuxAudio[0], AUDIO_PORT_INDEX, NULL, BUFFER_OUT_SIZE_AUDIO, outBufferAudioEnc[0]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate Audio buffer 1 in mux \n");
      exit(1);
    }
    err = OMX_UseBuffer(appPriv->muxhandle, &inBufferMuxAudio[1], AUDIO_PORT_INDEX, NULL, BUFFER_OUT_SIZE_AUDIO, outBufferAudioEnc[1]->pBuffer);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Unable to allocate Audio buffer 2 in mux \n");
      exit(1);
    }
  }

  /* Wait for all components to change to idle state*/
  tsem_down(appPriv->videoencEventSem);
  tsem_down(appPriv->audioencEventSem);
  DEBUG(DEFAULT_MESSAGES," audio and video encoder transitioned to  idle state \n");
  if(flagAVsync){
    tsem_down(appPriv->clockEventSem);
    DEBUG(DEFAULT_MESSAGES,"clock src  state idle\n");
  }
  tsem_down(appPriv->videosrcEventSem);
  DEBUG(DEFAULT_MESSAGES,"video src state idle\n");
  tsem_down(appPriv->audiosrcEventSem);
  DEBUG(DEFAULT_MESSAGES,"audio src state idle\n");

  /*Wait for Mux  and clocksrc state change to Idle*/
  tsem_down(appPriv->muxEventSem);
  DEBUG(DEFAULT_MESSAGES,"Mux in idle state \n");

  err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"Parser 3gp state executing failed\n");
    exit(1);
  }
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"video encoder state executing failed\n");
    exit(1);
  }
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"audio encoder state executing failed\n");
    exit(1);
  }
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"video src state executing failed\n");
    exit(1);
  }
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR,"audio src state executing failed\n");
    exit(1);
  }
  
  /*Wait for encoder state change to executing*/
  tsem_down(appPriv->videoencEventSem);
  tsem_down(appPriv->audioencEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ,"video and audio encoder state executing successful\n");
  tsem_down(appPriv->videosrcEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "video src state executing successful\n");
  tsem_down(appPriv->audiosrcEventSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "audio src state executing successful\n");
  /*Wait for Mux state change to executing*/
  tsem_down(appPriv->muxEventSem);
  DEBUG(DEFAULT_MESSAGES,"All component are in executing state \n");
  
  if(flagAVsync && !flagSetupTunnel){
    /* enabling the clock port of the clocksrc connected to the parser and parser's clock port */
    err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandPortEnable, 2, NULL);
    err = OMX_SendCommand(appPriv->clocksrchandle,  OMX_CommandPortEnable, 2, NULL);
    /*Wait for Ports Enable Events*/
    tsem_down(appPriv->muxEventSem);
    tsem_down(appPriv->clockEventSem);
  }
  
  /* putting the clock src in Executing state */
  if(flagAVsync){
    err = OMX_SendCommand(appPriv->clocksrchandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"clock src state executing failed\n");
      exit(1);
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ,"waiting for  clock src state executing\n");
    tsem_down(appPriv->clockEventSem);
    DEBUG(DEFAULT_MESSAGES, "clock src in executing state\n");

    /* set the audio as the master */ 
    setHeader(&sRefClock, sizeof(OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
    sRefClock.eClock = OMX_TIME_RefClockAudio;
    OMX_SetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeActiveRefClock,&sRefClock);

    /* set the clock state to OMX_TIME_ClockStateWaitingForStartTime */
    setHeader(&sClockState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
    err = OMX_GetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeClockState, &sClockState);
    sClockState.nWaitMask = OMX_CLOCKPORT1 || OMX_CLOCKPORT0;  /* wait for audio and video start time */
    sClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;
    err = OMX_SetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeClockState, &sClockState);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig \n",err);
      exit(1);
    }
    err = OMX_GetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeClockState, &sClockState);
  }

  
  DEBUG(DEFAULT_MESSAGES,"All Component state changed to Executing\n");

  if (!flagSetupTunnel)  {
    err = OMX_FillThisBuffer(appPriv->videosrchandle, outBufferSrcVideo[0]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Video Src \n", __func__,err);
      exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill parser second buffer %x\n", (int)outBufferSrcVideo[1]);
    err = OMX_FillThisBuffer(appPriv->videosrchandle, outBufferSrcVideo[1]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Video Src\n", __func__,err);
      exit(1);
    }

    err = OMX_FillThisBuffer(appPriv->audiosrchandle, outBufferSrcAudio[0]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Src \n", __func__,err);
      exit(1);
    }    
    DEBUG(DEB_LEV_PARAMS, "Fill parser second buffer %x\n", (int)outBufferSrcAudio[1]);
    err = OMX_FillThisBuffer(appPriv->audiosrchandle, outBufferSrcAudio[1]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Src\n", __func__,err);
      exit(1);
    }    

    err = OMX_FillThisBuffer(appPriv->videoenchandle, outBufferVideoEnc[0]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Video Enc\n", __func__,err);
      exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill encoder second buffer %x\n", (int)outBufferVideoEnc[1]);
    err = OMX_FillThisBuffer(appPriv->videoenchandle, outBufferVideoEnc[1]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Video Enc\n", __func__,err);
      exit(1);
    }

    err = OMX_FillThisBuffer(appPriv->audioenchandle, outBufferAudioEnc[0]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Enc\n", __func__,err);
      exit(1);
    }
    DEBUG(DEB_LEV_PARAMS, "Fill encoder second buffer %x\n", (int)outBufferAudioEnc[1]);
    err = OMX_FillThisBuffer(appPriv->audioenchandle, outBufferAudioEnc[1]);
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer Audio Enc\n", __func__,err);
      exit(1);
    }
  }

  setHeader(&sConfigScale, sizeof(OMX_TIME_CONFIG_SCALETYPE));
  if(flagAVsync){
    DEBUG(DEFAULT_MESSAGES,"--------------------------\n");
    DEBUG(DEFAULT_MESSAGES,"Enter F : fastforward \n");
    DEBUG(DEFAULT_MESSAGES,"Enter R : Rewind      \n");
    DEBUG(DEFAULT_MESSAGES,"Enter P : Pause      \n");
    DEBUG(DEFAULT_MESSAGES,"Enter N : Normal Play      \n");
    DEBUG(DEFAULT_MESSAGES,"Enter Q : Quit       \n");
    DEBUG(DEFAULT_MESSAGES,"--------------------------\n");
    while(1) {
      keyin = toupper(getchar());
      if(keyin == 'Q'|| keyin == 'q'){
        DEBUG(DEFAULT_MESSAGES,"Quitting \n");
        //bEOS = OMX_TRUE;
        break;
      }else{
        switch(keyin){
          case 'F':
          case 'f':
            newscale = 2<<16;    // Q16 format is used
            DEBUG(DEFAULT_MESSAGES,"Fast forwarding .......\n");
            break;
          case 'R':
          case 'r':
            newscale = -2<<16;    // Q16 format is used
            DEBUG(DEFAULT_MESSAGES,"Rewinding ........\n");
            break;
          case 'P':
          case 'p':
            newscale = 0;
            DEBUG(DEFAULT_MESSAGES,"Pause ......\n");
            break;
          case 'N':
          case 'n':
            newscale = 1<<16;    // Q16 format is used
            DEBUG(DEFAULT_MESSAGES,"Normal Play ......\n");
            break;
          default:
            break;        
        }
        sConfigScale.xScale = newscale;
       /* send the scale change notification to the clock */
        err = OMX_SetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeScale, &sConfigScale);
      }
    }
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

  DEBUG(DEFAULT_MESSAGES,"Received EOS \n");

  /* putting the clock src in Executing state */
  /*
  if(flagAVsync){
    // set the clock state to OMX_TIME_ClockStateWaitingForStartTime //
    setHeader(&sClockState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
    err = OMX_GetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeClockState, &sClockState);
    //sClockState.nWaitMask = OMX_CLOCKPORT1 || OMX_CLOCKPORT0;  // wait for audio and video start time //
    sClockState.eState = OMX_TIME_ClockStateStopped;
    err = OMX_SetConfig(appPriv->clocksrchandle, OMX_IndexConfigTimeClockState, &sClockState);
    if(err!=OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR,"Error %08x In OMX_SetConfig \n",err);
      exit(1);
    }
  }
  */
  /*Send Idle Command to all components*/

  if (flagAVsync) {
    err = OMX_SendCommand(appPriv->clocksrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  } 

  DEBUG(DEFAULT_MESSAGES, "The execution of the encoding process is terminated\n");
  err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  
  tsem_down(appPriv->muxEventSem);
  DEBUG(DEFAULT_MESSAGES,"Mux idle state \n");
  tsem_down(appPriv->videoencEventSem);
  tsem_down(appPriv->audioencEventSem);

  tsem_down(appPriv->videosrcEventSem);
  tsem_down(appPriv->audiosrcEventSem);
  
  if (flagAVsync) {
    tsem_down(appPriv->clockEventSem);
  }  

  DEBUG(DEFAULT_MESSAGES, "All component Transitioned to Idle\n");
  /*Send Loaded Command to all components*/
  err = OMX_SendCommand(appPriv->muxhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->videoenchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->audioenchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->videosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->audiosrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  
  if (flagAVsync) {
    err = OMX_SendCommand(appPriv->clocksrchandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }  

  DEBUG(DEFAULT_MESSAGES, "All components to loaded\n");

  /*Free input buffers if components are not tunnelled*/
  if (!flagSetupTunnel) {
    err = OMX_FreeBuffer(appPriv->videoenchandle, 0, inBufferVideoEnc[0]);
    err = OMX_FreeBuffer(appPriv->videoenchandle, 0, inBufferVideoEnc[1]);
    err = OMX_FreeBuffer(appPriv->audioenchandle, 0, inBufferAudioEnc[0]);
    err = OMX_FreeBuffer(appPriv->audioenchandle, 0, inBufferAudioEnc[1]);

    DEBUG(DEB_LEV_PARAMS, "Freeing encoder output ports\n");
    err = OMX_FreeBuffer(appPriv->videoenchandle, 1, outBufferVideoEnc[0]);
    err = OMX_FreeBuffer(appPriv->videoenchandle, 1, outBufferVideoEnc[1]);
    err = OMX_FreeBuffer(appPriv->audioenchandle, 1, outBufferAudioEnc[0]);
    err = OMX_FreeBuffer(appPriv->audioenchandle, 1, outBufferAudioEnc[1]);
    err = OMX_FreeBuffer(appPriv->muxhandle, VIDEO_PORT_INDEX, inBufferMuxVideo[0]);
    err = OMX_FreeBuffer(appPriv->muxhandle, VIDEO_PORT_INDEX, inBufferMuxVideo[1]);
    err = OMX_FreeBuffer(appPriv->muxhandle, AUDIO_PORT_INDEX, inBufferMuxAudio[0]);
    err = OMX_FreeBuffer(appPriv->muxhandle, AUDIO_PORT_INDEX, inBufferMuxAudio[1]);

    err = OMX_FreeBuffer(appPriv->videosrchandle, 0, outBufferSrcVideo[0]);
    err = OMX_FreeBuffer(appPriv->videosrchandle, 0, outBufferSrcVideo[1]);
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, outBufferSrcAudio[0]);
    err = OMX_FreeBuffer(appPriv->audiosrchandle, 0, outBufferSrcAudio[1]);
  }

  
  tsem_down(appPriv->muxEventSem);
  DEBUG(DEFAULT_MESSAGES,"Mux loaded state \n");
  tsem_down(appPriv->videoencEventSem); 
  tsem_down(appPriv->audioencEventSem); 

  tsem_down(appPriv->videosrcEventSem);
  tsem_down(appPriv->audiosrcEventSem);
  
  if (flagAVsync) {
    tsem_down(appPriv->clockEventSem);
  }  

  DEBUG(DEFAULT_MESSAGES, "All components released\n");

  /** freeing all handles and deinit omx */
  OMX_FreeHandle(appPriv->videoenchandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "videoenc freed\n");
  OMX_FreeHandle(appPriv->audioenchandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "audioenc enc freed\n");
  OMX_FreeHandle(appPriv->muxhandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "mux freed\n");
  OMX_FreeHandle(appPriv->videosrchandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "videosrc freed\n");
  OMX_FreeHandle(appPriv->audiosrchandle);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "audiosrc freed\n");
  
  if (flagAVsync) {
    OMX_FreeHandle(appPriv->clocksrchandle);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "clock src freed\n");
  }

  OMX_Deinit();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components freed. Closing...\n");

  free(appPriv->muxEventSem);
  appPriv->muxEventSem = NULL;

  free(appPriv->videoencEventSem);
  appPriv->videoencEventSem = NULL;
  free(appPriv->audioencEventSem);
  appPriv->audioencEventSem = NULL;
  free(appPriv->videosrcEventSem);
  appPriv->videosrcEventSem = NULL;
  free(appPriv->audiosrcEventSem);
  appPriv->audiosrcEventSem = NULL;
  
  if(flagAVsync) {
    free(appPriv->clockEventSem);
  }

  free(appPriv->eofSem);
  appPriv->eofSem = NULL;
  free(appPriv);
  appPriv = NULL;

  free(output_file);
  
  return 0;
}  

/* Callbacks implementation */
OMX_ERRORTYPE muxEventHandler(
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
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Parser 3gp State changed in ");
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
      tsem_up(appPriv->muxEventSem);
    } else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->muxEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->muxEventSem);
    } else if (Data1 == OMX_CommandFlush){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Flush Event\n",__func__);
      tsem_up(appPriv->muxEventSem);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Received Event Event=%d Data1=%d,Data2=%d\n",__func__,eEvent,(int)Data1,(int)Data2);
    }
  }else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Mux Port Setting Changed event\n");
  }else if(eEvent == OMX_EventPortFormatDetected) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port Format Detected %x\n", __func__,(int)Data1);
  }else if (eEvent ==OMX_EventError){
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Error in %x Detection for port %d\n",__func__,(int)Data1,(int)Data2);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE muxEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  /* Output data to video & audio encoder */
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if(pBuffer != NULL){
    switch(pBuffer->nInputPortIndex) {
    case VIDEO_PORT_INDEX:
      if(!bEOS) {
        if(outBufferVideoEnc[0]->pBuffer == pBuffer->pBuffer) {
          outBufferVideoEnc[0]->nFilledLen = 0;
          err = OMX_FillThisBuffer(appPriv->videoenchandle, outBufferVideoEnc[0]);
        } else {
          outBufferVideoEnc[1]->nFilledLen = 0;
          err = OMX_FillThisBuffer(appPriv->videoenchandle, outBufferVideoEnc[1]);
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
      break;
     case AUDIO_PORT_INDEX:
       if(!bEOS) {
         if(outBufferAudioEnc[0]->pBuffer == pBuffer->pBuffer) {
           outBufferAudioEnc[0]->nFilledLen = 0;
           err = OMX_FillThisBuffer(appPriv->audioenchandle, outBufferAudioEnc[0]);
         } else {
           outBufferAudioEnc[1]->nFilledLen = 0;
           err = OMX_FillThisBuffer(appPriv->audioenchandle, outBufferAudioEnc[1]);
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
       break;
     }
   }  else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
   }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE videoencEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  OMX_ERRORTYPE err;
  OMX_PARAM_PORTDEFINITIONTYPE param;
  
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ/*SIMPLE_SEQ*/, "Video Encoder State changed in ");
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
      tsem_up(appPriv->videoencEventSem);
    }
    else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->videoencEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->videoencEventSem);
    } else if (Data1 == OMX_CommandFlush){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Flush Event\n",__func__);
      tsem_up(appPriv->videoencEventSem);
    } 
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Settings Changed Event\n", __func__);
    if (Data2 == 1) {
      param.nPortIndex = 1;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->videoenchandle,OMX_IndexParamPortDefinition, &param);
      /*Get Port parameters*/
    } else if (Data2 == 0) {
      /*Get Port parameters*/
      param.nPortIndex = 0;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->videoenchandle,OMX_IndexParamPortDefinition, &param);
    }
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  
  return OMX_ErrorNone;
}

OMX_ERRORTYPE videoencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  static int iBufferDropped=0;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if(pBuffer != NULL){
    if(!bEOS) {
      if(inBufferMuxVideo[0]->pBuffer == pBuffer->pBuffer) {
       inBufferMuxVideo[0]->nFilledLen = pBuffer->nFilledLen;
        inBufferMuxVideo[0]->nFlags     = pBuffer->nFlags;
        err = OMX_EmptyThisBuffer(appPriv->muxhandle, inBufferMuxVideo[0]);
      } else if(inBufferMuxVideo[1]->pBuffer == pBuffer->pBuffer) {
        inBufferMuxVideo[1]->nFilledLen = pBuffer->nFilledLen;
        inBufferMuxVideo[1]->nFlags     = pBuffer->nFlags;
        err = OMX_EmptyThisBuffer(appPriv->muxhandle, inBufferMuxVideo[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    }
    else {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s: eos=%x Dropping Fill This Buffer\n", __func__,(int)pBuffer->nFlags);
      iBufferDropped++;
      if(iBufferDropped==2) {
        tsem_up(appPriv->eofSem);
      }
    }
  } else {
    if(!bEOS) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"It is here EOS = %d\n",appPriv->eofSem->semval);
    }
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE videoencEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);
  
  /* Output data to alsa src */
  if(pBuffer != NULL){
   if(!bEOS) {
    if(outBufferSrcVideo[0]->pBuffer == pBuffer->pBuffer) {
      outBufferSrcVideo[0]->nFilledLen = 0;
      err = OMX_FillThisBuffer(appPriv->videosrchandle, outBufferSrcVideo[0]);
    } else {
      outBufferSrcVideo[1]->nFilledLen = 0;
      err = OMX_FillThisBuffer(appPriv->videosrchandle, outBufferSrcVideo[1]);
    }
    if(err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
    }
   } else {
        DEBUG(DEB_LEV_ERR, " Buffer Dropping ...\n");
   }
  }  else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

/** callbacks implementation of video src component */
OMX_ERRORTYPE videosrcEventHandler(
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
      tsem_up(appPriv->videosrcEventSem);
    } else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable/Disable Event\n",__func__);
      tsem_up(appPriv->videosrcEventSem);
    } else if (OMX_CommandFlush){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Flush Event\n",__func__);
      tsem_up(appPriv->videosrcEventSem);
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

OMX_ERRORTYPE videosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int inputBufferDropped = 0;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if(pBuffer != NULL) {
    if(!bEOS) {
      if(inBufferVideoEnc[0]->pBuffer == pBuffer->pBuffer) {
        inBufferVideoEnc[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->videoenchandle, inBufferVideoEnc[0]);
      } else {
        inBufferVideoEnc[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->videoenchandle, inBufferVideoEnc[1]);
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

OMX_ERRORTYPE audioencEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  OMX_ERRORTYPE err;
  OMX_PARAM_PORTDEFINITIONTYPE param;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "Hi there, I am in the %s callback\n", __func__);
  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ/*SIMPLE_SEQ*/, "Audio Encoder State changed in ");
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
      tsem_up(appPriv->audioencEventSem);
    }
    else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->audioencEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->audioencEventSem);
    } else if (Data1 == OMX_CommandFlush){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Flush Event\n",__func__);
      tsem_up(appPriv->audioencEventSem);
    }
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Settings Changed Event\n", __func__);
    if (Data2 == 1) {
      param.nPortIndex = 1;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audioenchandle,OMX_IndexParamPortDefinition, &param);
    } else if (Data2 == 0) {
      /*Get Port parameters*/
      param.nPortIndex = 0;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      err = OMX_GetParameter(appPriv->audioenchandle,OMX_IndexParamPortDefinition, &param);
    }
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
//      tsem_up(appPriv->eofSem);
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
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  static int iBufferDropped=0;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  if(pBuffer != NULL){
    if(!bEOS) {
      if(outBufferSrcAudio[0]->pBuffer == pBuffer->pBuffer) {
        outBufferSrcAudio[0]->nFilledLen = 0;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, outBufferSrcAudio[0]);
      } else {
        outBufferSrcAudio[1]->nFilledLen = 0;
        err = OMX_FillThisBuffer(appPriv->audiosrchandle, outBufferSrcAudio[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
      }
    }
    else {
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
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audioencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n",__func__);

  /* Output data to alsa src */
  if(pBuffer != NULL){
    if(!bEOS) {
      if (pBuffer->nFilledLen == 0) {
        DEBUG(DEB_LEV_ERR, "Ouch! In %s: had 0 data size in output buffer...\n", __func__);
        return OMX_ErrorNone;
      }
      if(inBufferMuxAudio[0]->pBuffer == pBuffer->pBuffer) {
        inBufferMuxAudio[0]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->muxhandle, inBufferMuxAudio[0]);
      } else {
        inBufferMuxAudio[1]->nFilledLen = pBuffer->nFilledLen;
        err = OMX_EmptyThisBuffer(appPriv->muxhandle, inBufferMuxAudio[1]);
      }
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling EmptyThisBuffer\n", __func__,err);
      }
    } else {
      DEBUG(DEB_LEV_ERR, " Buffer Dropping ...\n");
    }
  } else {
    DEBUG(DEB_LEV_ERR, "Ouch! In %s: had NULL buffer to output...\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE audiosrcEventHandler(
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
    tsem_up(appPriv->audiosrcEventSem);
  } else if (Data1 == OMX_CommandPortEnable){
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
    tsem_up(appPriv->audiosrcEventSem);
  } else if (Data1 == OMX_CommandPortDisable){
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
    tsem_up(appPriv->audiosrcEventSem);
  } else if (Data1 == OMX_CommandFlush){
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Flush Event\n",__func__);
    tsem_up(appPriv->audiosrcEventSem);
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

OMX_ERRORTYPE audiosrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  static int alsaSinkBufferDropped=0;
  
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);

  if(!bEOS) {
    if(inBufferAudioEnc[0]->pBuffer == pBuffer->pBuffer) {
      inBufferAudioEnc[0]->nFilledLen = pBuffer->nFilledLen;
      err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBufferAudioEnc[0]);
    } else {
      inBufferAudioEnc[1]->nFilledLen = pBuffer->nFilledLen;
      err = OMX_EmptyThisBuffer(appPriv->audioenchandle, inBufferAudioEnc[1]);
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

OMX_ERRORTYPE clocksrcEventHandler(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_EVENTTYPE eEvent,
  OMX_OUT OMX_U32 Data1,
  OMX_OUT OMX_U32 Data2,
  OMX_OUT OMX_PTR pEventData)
{
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback\n", __func__);

  if(eEvent == OMX_EventCmdComplete) {
    if (Data1 == OMX_CommandStateSet) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Clock Component State changed in ");
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
      tsem_up(appPriv->clockEventSem);
    } else if (Data1 == OMX_CommandPortEnable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Enable  Event\n",__func__);
      tsem_up(appPriv->clockEventSem);
    } else if (Data1 == OMX_CommandPortDisable){
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Received Port Disable Event\n",__func__);
      tsem_up(appPriv->clockEventSem);
    } else {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s Received Event Event=%d Data1=%d,Data2=%d\n",__func__,eEvent,(int)Data1,(int)Data2);
    }
  } else if(eEvent == OMX_EventPortSettingsChanged) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Clock src  Port Setting Changed event\n");
    /*Signal Port Setting Changed*/
    tsem_up(appPriv->clockEventSem);
  } else if(eEvent == OMX_EventPortFormatDetected) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Port Format Detected %x\n", __func__,(int)Data1);
  } else if(eEvent == OMX_EventBufferFlag) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s OMX_BUFFERFLAG_EOS\n", __func__);
    if((int)Data2 == OMX_BUFFERFLAG_EOS) {
//      tsem_up(appPriv->eofSem);
    }
  } else {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param1 is %i\n", (int)Data1);
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Param2 is %i\n", (int)Data2);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE clocksrcFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_ERRORTYPE err;
  /* Output data to audio encoder */

  if(pBuffer != NULL){
    if(!bEOS) {
        err = OMX_EmptyThisBuffer(appPriv->clocksrchandle, pBuffer);
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

