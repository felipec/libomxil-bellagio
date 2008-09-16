/**
  @file test/components/audio/audio_use_case_tunnel.c

  Test application that uses four OpenMAX components, a file reader,an audio decoder , an audio effect
  and an alsa sink. The application receives an compressed audio stream on input port
  from a file, decodes it and sends it to the alsa sink, or to a file or standard output.

  This test application goes through a directory and plays all mp3,ogg and aac files in a loop

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
#include <dirent.h>
#include <errno.h>

#include "omxaudiodectest.h"

#define COMPONENT_NAME_BASE "OMX.st.audio_decoder"
#define VORBIS_ROLE "audio_decoder.ogg"
#define MP3_ROLE "audio_decoder.mp3"
#define AAC_ROLE "audio_decoder.aac"
#define SINK_NAME "OMX.st.alsa.alsasink"
#define FILE_READER "OMX.st.audio_filereader"
#define AUDIO_EFFECT "OMX.st.volume.component"
#define extradata_size 1024

OMX_CALLBACKTYPE audiodeccallbacks = { 
  .EventHandler    = audiodecEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = NULL
};

OMX_CALLBACKTYPE audiosinkcallbacks = { 
  .EventHandler    = audiosinkEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = NULL
};

OMX_CALLBACKTYPE filereadercallbacks = { 
  .EventHandler    = filereaderEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = NULL
};

OMX_CALLBACKTYPE volumecallbacks = { 
  .EventHandler    = volumeEventHandler,
  .EmptyBufferDone = NULL,
  .FillBufferDone  = NULL
};

static OMX_BOOL bEOS=OMX_FALSE;
appPrivateType* appPriv;
char *input_file=NULL;


static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
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

int flagSetupTunnel;
int flagPlaybackOn;
int flagUsingFFMpeg;
int flagIsGain;

   
int main(int argc, char** argv) {
  int argn_dec;
  OMX_ERRORTYPE err;
  OMX_INDEXTYPE eIndexParamFilename;
  int gain=-1;
  OMX_AUDIO_CONFIG_VOLUMETYPE sVolume;
  OMX_TIME_CONFIG_TIMESTAMPTYPE sTimeStamp;
  OMX_PARAM_COMPONENTROLETYPE sComponentRole;
  
  char *stream_dir=NULL;
  DIR *dirp;
	struct dirent *dp;
  int seek=1;

  flagSetupTunnel = 1;
  flagPlaybackOn = 1;
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

  

  if(flagUsingFFMpeg) {
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
    if(flagUsingFFMpeg) {
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

  if(flagUsingFFMpeg) {
    /** now set the filereader component to idle and executing state */
    OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  /*Send State Change Idle command to Audio Decoder*/
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Dec\n");
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  if (flagPlaybackOn) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "Send Command Idle to Audio Sink\n");
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  if(flagUsingFFMpeg) {
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

        flagUsingFFMpeg = 1;

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
    tsem_reset(appPriv->eofSem);
    bEOS=OMX_FALSE;

    if (flagUsingFFMpeg) {
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

    if(flagUsingFFMpeg) {
      /** setting the input audio format in file reader */
      DEBUG(DEB_LEV_SIMPLE_SEQ,"FileName Param index : %x \n",eIndexParamFilename);
      err = OMX_SetParameter(appPriv->filereaderhandle,eIndexParamFilename,input_file);
      if(err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR,"\n error in input audio format - exiting\n");
        exit(1);
      }
    }
    
    if (flagUsingFFMpeg) {
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
      
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Waiting File reader Port Disable event \n");
      /*Wait for File Reader Ports Disable Event*/
      tsem_down(appPriv->filereaderEventSem);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"File reader Port Disable State Idle\n");
      /*Wait for Audio Decoder Ports Disable Event*/
      tsem_down(appPriv->decoderEventSem);
    }

    if(flagUsingFFMpeg) {
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

    DEBUG(DEB_LEV_SIMPLE_SEQ,"All Component state changed to Executing\n");
    
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
    if(flagUsingFFMpeg) {
      err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    }
    err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    
    if(flagUsingFFMpeg) {
      tsem_down(appPriv->filereaderEventSem);
      DEBUG(DEFAULT_MESSAGES,"File reader idle state \n");
    }
    tsem_down(appPriv->decoderEventSem);
    
    DEBUG(DEFAULT_MESSAGES, "All component Transitioned to Idle\n");

  } /*Loop While Play List*/

  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  }

  if (flagPlaybackOn) {
    tsem_down(appPriv->volumeEventSem);
    tsem_down(appPriv->sinkEventSem);
  }
  /*Send Loaded Command to all components*/
  if(flagUsingFFMpeg) {
    err = OMX_SendCommand(appPriv->filereaderhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }
  err = OMX_SendCommand(appPriv->audiodechandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  if (flagPlaybackOn) {
    err = OMX_SendCommand(appPriv->volumehandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    err = OMX_SendCommand(appPriv->audiosinkhandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  }

  DEBUG(DEFAULT_MESSAGES, "Audio dec to loaded\n");

  if(flagUsingFFMpeg) {
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
  
  if(flagUsingFFMpeg) {
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

  if(input_file) {
    free(input_file);
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

      if (flagPlaybackOn && flagSetupTunnel) { /*Disable Volume Component and Audio Sink Port,Set Parameter in Tunneled Case*/

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
