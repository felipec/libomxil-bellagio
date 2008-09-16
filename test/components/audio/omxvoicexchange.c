/**
  @file test/components/audio/omxvoicexchange.c
  
  This test application is supposed to exchange voice over the network 
  using some client server mechanism.
  
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
  
  $Date: 2008-09-11 12:14:48 +0200 (Thu, 11 Sep 2008) $
  Revision $Rev: 1484 $
  Author $Author: pankaj_sen $
*/

#include "omxvoicexchange.h"

/** defining global declarations */
#define COMPONENT_NAME_BASE "OMX.st.alsa.alsasrc"
#define BASE_ROLE "alsa.alsasrc"
#define COMPONENT_NAME_BASE_LEN 20

OMX_CALLBACKTYPE audiosrccallbacks = { 
    .EventHandler = audiosrcEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL
    };

OMX_CALLBACKTYPE audioenccallbacks = { 
    .EventHandler = audioencEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = audioencFillBufferDone
    };

OMX_CALLBACKTYPE audiodeccallbacks = { 
    .EventHandler = audiodecEventHandler,
    .EmptyBufferDone = audiodecEmptyBufferDone,
    .FillBufferDone = NULL
    };
OMX_CALLBACKTYPE alsasinkcallbacks = { 
    .EventHandler = audiosinkEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL
    };


/** global variables */
appPrivateType* appPriv;

/** used with audio Encoder and Decoder */
OMX_BUFFERHEADERTYPE *pOutAudioEncBuffer[2],*pInAudioDecBuffer[2];

OMX_U32 buffer_out_size = 16384,read_buffer_size=240;

char temp_buffer[BUFFER_OUT_SIZE];
int temp_buffer_filled_len = 0;

int rate = 0,channel=0;
char *output_file;
FILE *outfile;

int s_sockfd, s_newsockfd, s_portno=0, s_clilen,s_n;
char s_buffer[BUFFER_OUT_SIZE];
struct sockaddr_in s_serv_addr, s_cli_addr;

int c_sockfd, c_portno=0, c_n;
struct sockaddr_in c_serv_addr;
struct hostent *c_server;
char c_buffer[BUFFER_OUT_SIZE];


int flagIsClientPort;
int flagIsServerPort;
int flagIsMaster;
int flagDecodedOutputReceived;
int flagIsIPaddress;

static OMX_BOOL bEOS = OMX_FALSE;

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

/** help display */
void display_help() {
  DEBUG(DEFAULT_MESSAGES, "\n");
  DEBUG(DEFAULT_MESSAGES, "Usage: omxvoiceexchange [-n] [-p] [-h]\n");
  DEBUG(DEFAULT_MESSAGES, "\n");
  DEBUG(DEFAULT_MESSAGES, "       -n 10.199.15.203 : Server IP Address\n");
  DEBUG(DEFAULT_MESSAGES, "       -sp 10000        : Server port\n");
  DEBUG(DEFAULT_MESSAGES, "       -cp 10001        : Server port\n");
  DEBUG(DEFAULT_MESSAGES, "       -m               : Use as master\n");
  DEBUG(DEFAULT_MESSAGES, "\n");
  DEBUG(DEFAULT_MESSAGES, "       -h               : Display help\n");
  DEBUG(DEFAULT_MESSAGES, "\n");
  exit(1);
}

int main(int argc, char** argv) {

  OMX_ERRORTYPE err;
  int argn_dec;
  OMX_STRING ipaddress=NULL;
  OMX_AUDIO_PARAM_PCMMODETYPE sPCMModeParam;
  int rate = 8000, channel=1;
  int len=0,offset=4,packet_offset=0,i;

  if(argc < 7){
    display_help();
  } else {
    flagIsClientPort = 0;
    flagIsServerPort = 0;
    flagDecodedOutputReceived = 0;
    flagIsIPaddress = 0;
    flagIsMaster = 0;

    argn_dec = 1;
    while (argn_dec < argc) {
      if (*(argv[argn_dec]) == '-') {
        switch (*(argv[argn_dec] + 1)) {
          case 'h' :
            display_help();
            break;
          case 'n':
            flagIsIPaddress = 1;
            break;
          case 's':
            flagIsServerPort = 1;
            break;
          case 'c':
            flagIsClientPort = 1;
            break;
          case 'm':
            flagIsMaster = 1;
            break;
          default:
            display_help();
        }
      } else {
        if(flagIsServerPort) {
          s_portno=atoi(argv[argn_dec]);
          flagIsServerPort = 0;
        } else if(flagIsClientPort) {
          c_portno=atoi(argv[argn_dec]);
          flagIsClientPort = 0;
        } else if(flagIsIPaddress) {
          ipaddress = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(ipaddress,argv[argn_dec]);
          flagIsIPaddress = 0;
        }
      }
      argn_dec++;
    }
  }

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType)); 
  appPriv->audiosrcEventSem  = malloc(sizeof(tsem_t));
  appPriv->audioencEventSem  = malloc(sizeof(tsem_t));
  appPriv->audiodecEventSem  = malloc(sizeof(tsem_t));
  appPriv->audiosinkEventSem = malloc(sizeof(tsem_t));
  appPriv->eofSem            = malloc(sizeof(tsem_t));

  tsem_init(appPriv->audiosrcEventSem, 0);
  tsem_init(appPriv->audioencEventSem, 0);
  tsem_init(appPriv->audiodecEventSem, 0);
  tsem_init(appPriv->audiosinkEventSem, 0);
  tsem_init(appPriv->eofSem, 0);
  
  err = OMX_Init();
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "The OpenMAX core can not be initialized. Exiting...\n");
    exit(1);
  }

  /** getting handles */
  err = OMX_GetHandle(&appPriv->audiosrcHandle, "OMX.st.alsa.alsasrc", NULL, &audiosrccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No audio source component found. Exiting...\n");
    exit(1);
  } 

  err = OMX_GetHandle(&appPriv->audioencHandle, "OMX.st.audio_encoder.g726", NULL, &audioenccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No audio encoder component found. Exiting...\n");
    exit(1);
  } 

  err = OMX_GetHandle(&appPriv->audiodecHandle, "OMX.st.audio_decoder.g726", NULL, &audiodeccallbacks);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "No audio decoder component found. Exiting...\n");
    exit(1);
  } 

  err = OMX_GetHandle(&appPriv->audiosinkHandle, "OMX.st.alsa.alsasink", NULL, &alsasinkcallbacks);
  if(err != OMX_ErrorNone){
    DEBUG(DEB_LEV_ERR, "No audio sink component component found. Exiting...\n");
    exit(1);
  } 

  /*Set Sample Rate and Channel*/
  if(rate > 0 || channel >0)  {
    DEBUG(DEFAULT_MESSAGES, "Setting Rate and Channel\n");
    setHeader(&sPCMModeParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    sPCMModeParam.nPortIndex = 0;
    err = OMX_GetParameter(appPriv->audiosrcHandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in GetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }

    sPCMModeParam.nChannels = (channel >0 ) ? channel:sPCMModeParam.nChannels;
    sPCMModeParam.nSamplingRate = (rate >0 ) ? rate:sPCMModeParam.nSamplingRate;
    err = OMX_SetParameter(appPriv->audiosrcHandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSrc. Exiting...\n");
      exit(1);
    }

    err = OMX_SetParameter(appPriv->audiosinkHandle,OMX_IndexParamAudioPcm,&sPCMModeParam);
    if (err != OMX_ErrorNone) {
      DEBUG(DEB_LEV_ERR, "Err in SetParameter OMX_AUDIO_PARAM_PCMMODETYPE in AlsaSink. Exiting...\n");
      exit(1);
    }
  } 
  
  /** if tunneling option is given then set up the tunnel between the components */
  DEBUG(DEFAULT_MESSAGES, "Setting up Tunnel\n");
  err = OMX_SetupTunnel(appPriv->audiosrcHandle, 0, appPriv->audioencHandle, 0);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set up Tunnel between audio src & encoder component Failed\n");
    exit(1);
  }
  err = OMX_SetupTunnel(appPriv->audiodecHandle, 1, appPriv->audiosinkHandle, 0);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Set up Tunnel between decoder component & audio sink Failed\n");
    exit(1);
  }
  
  /** sending commands to go to idle state */
  err = OMX_SendCommand(appPriv->audiosrcHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audioencHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_AllocateBuffer(appPriv->audioencHandle, &pOutAudioEncBuffer[0], 1, NULL, buffer_out_size);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on pOutAudioEncBuffer %i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(appPriv->audioencHandle, &pOutAudioEncBuffer[1], 1, NULL, buffer_out_size);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on pOutAudioEncBuffer %i\n", err);
    exit(1);
  }

  err = OMX_SendCommand(appPriv->audiodecHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audiosinkHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_AllocateBuffer(appPriv->audiodecHandle, &pInAudioDecBuffer[0], 0, NULL, buffer_out_size);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on pInAudioDecBuffer %i\n", err);
    exit(1);
  }
  err = OMX_AllocateBuffer(appPriv->audiodecHandle, &pInAudioDecBuffer[1], 0, NULL, buffer_out_size);
  if (err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "Error on pInAudioDecBuffer %i\n", err);
    exit(1);
  }

  tsem_down(appPriv->audiosrcEventSem);
  tsem_down(appPriv->audioencEventSem);
  tsem_down(appPriv->audiodecEventSem);
  tsem_down(appPriv->audiosinkEventSem);

  DEBUG(DEFAULT_MESSAGES, "All Components state transitioned to Idle\n");

  DEBUG(DEFAULT_MESSAGES, "Client IP=%s, Server Port=%d\n",ipaddress,s_portno);
  if(flagIsMaster) { 
    DEBUG(DEFAULT_MESSAGES, "In the Master\n");
    /*Client Code*/
    c_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c_sockfd < 0) 
        error("ERROR opening socket");
    c_server = gethostbyname(ipaddress);
    if (c_server == NULL) {
        DEBUG(DEFAULT_MESSAGES,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &c_serv_addr, sizeof(c_serv_addr));
    c_serv_addr.sin_family = AF_INET;
    bcopy((char *)c_server->h_addr, 
           (char *)&c_serv_addr.sin_addr.s_addr,
           c_server->h_length);
    c_serv_addr.sin_port = htons(c_portno);

    /*Server Code*/
    s_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sockfd < 0) 
      error("ERROR opening socket");
    bzero((char *) &s_serv_addr, sizeof(s_serv_addr));
    s_serv_addr.sin_family = AF_INET;
    s_serv_addr.sin_addr.s_addr = INADDR_ANY;
    s_serv_addr.sin_port = htons(s_portno);
    if (bind(s_sockfd, (struct sockaddr *) &s_serv_addr,
            sizeof(s_serv_addr)) < 0) 
            error("ERROR on binding");
  
    listen(s_sockfd,5);
    s_clilen = sizeof(s_cli_addr);
    s_newsockfd = accept(s_sockfd, 
               (struct sockaddr *) &s_cli_addr, 
               &s_clilen);

    sleep(1);
    /*Client Connect to server*/
    if (connect(c_sockfd,&c_serv_addr,sizeof(c_serv_addr)) < 0)  {
      error("ERROR connecting");
    }
    DEBUG(DEFAULT_MESSAGES, "Please enter the message: ");
    bzero(c_buffer,BUFFER_OUT_SIZE);

    bzero(s_buffer,256);
    strcpy(s_buffer,"I am the Master");
    s_n = write(c_sockfd,s_buffer,strlen(s_buffer));
    if (s_n < 0) 
         error("ERROR writing to socket");
    
    bzero(s_buffer,256);
    DEBUG(DEFAULT_MESSAGES, "Master waiting for read \n"); 
    s_n = read(s_newsockfd,s_buffer,255);
    if (s_n < 0) 
         error("ERROR reading from socket");
    DEBUG(DEFAULT_MESSAGES, "%s\n",s_buffer);

    DEBUG(DEFAULT_MESSAGES, "Master read write complete\n");
             
  } else {
    DEBUG(DEFAULT_MESSAGES, "In the Slave\n");
    /*Server Code*/
    s_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sockfd < 0) 
      error("ERROR opening socket");
    bzero((char *) &s_serv_addr, sizeof(s_serv_addr));
    s_serv_addr.sin_family = AF_INET;
    s_serv_addr.sin_addr.s_addr = INADDR_ANY;
    s_serv_addr.sin_port = htons(s_portno);
    if (bind(s_sockfd, (struct sockaddr *) &s_serv_addr,
            sizeof(s_serv_addr)) < 0) 
            error("ERROR on binding");

    /*Client Code*/
    c_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c_sockfd < 0) 
        error("ERROR opening socket");
    c_server = gethostbyname(ipaddress);
    if (c_server == NULL) {
        DEBUG(DEFAULT_MESSAGES,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &c_serv_addr, sizeof(c_serv_addr));
    c_serv_addr.sin_family = AF_INET;
    bcopy((char *)c_server->h_addr, 
           (char *)&c_serv_addr.sin_addr.s_addr,
           c_server->h_length);
    c_serv_addr.sin_port = htons(c_portno);
    if (connect(c_sockfd,&c_serv_addr,sizeof(c_serv_addr)) < 0) {
      error("ERROR connecting");
    }
    DEBUG(DEFAULT_MESSAGES, "Please enter the message: ");
    bzero(c_buffer,BUFFER_OUT_SIZE);

    /*Server Listen on socket Code*/
      
    listen(s_sockfd,5);
    s_clilen = sizeof(s_cli_addr);
    s_newsockfd = accept(s_sockfd, 
               (struct sockaddr *) &s_cli_addr, 
               &s_clilen);

    DEBUG(DEFAULT_MESSAGES, "Slave writing to master \n"); 

    bzero(c_buffer,256);
    strcpy(c_buffer,"I am the Slave");
    c_n = write(c_sockfd,c_buffer,strlen(c_buffer));
    if (c_n < 0) 
         error("ERROR writing to socket");
    
    bzero(c_buffer,256);
    DEBUG(DEFAULT_MESSAGES, "Slave waiting for read \n"); 
    c_n = read(s_newsockfd,c_buffer,255);
    if (c_n < 0) 
         error("ERROR reading from socket");
    DEBUG(DEFAULT_MESSAGES, "%s\n",c_buffer);
 
  }

  /*
  // Maser         Slave 
  // s_newsockfd   s_newsockfd
  //           \  /
  //            \/    
  //            /\
  //           /  \
  // c_sockfd       c_sockfd
  // Master Read from s_newsockfd Write to c_sockfd

  // Slave  Write to c_sockfd Read from s_newsockfd
  */

  err = OMX_FillThisBuffer(appPriv->audioencHandle, pOutAudioEncBuffer[0]);
  err = OMX_FillThisBuffer(appPriv->audioencHandle, pOutAudioEncBuffer[1]);

  /** sending commands to go to executing state */
  err = OMX_SendCommand(appPriv->audiosrcHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  err = OMX_SendCommand(appPriv->audioencHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  err = OMX_SendCommand(appPriv->audiodecHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  err = OMX_SendCommand(appPriv->audiosinkHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  tsem_down(appPriv->audiosrcEventSem);
  tsem_down(appPriv->audioencEventSem);
  tsem_down(appPriv->audiodecEventSem);
  tsem_down(appPriv->audiosinkEventSem);

  DEBUG(DEFAULT_MESSAGES, "All Components state transitioned to Executing\n");

  for(i=0;i<2;i++) {
    bzero(s_buffer,BUFFER_OUT_SIZE);
    s_n = read(s_newsockfd,s_buffer,read_buffer_size);
    if (s_n < 0) 
         error("ERROR reading from socket");

    len = *((int*)&s_buffer[0]);

    DEBUG(DEFAULT_MESSAGES, "In %s sn_=%d len=%d\n",__func__,s_n,len);

    if(len >0 && len <=pInAudioDecBuffer[i]->nAllocLen) {
      packet_offset = offset;
      while (s_n >= packet_offset) {
        memcpy(&pInAudioDecBuffer[i]->pBuffer[pInAudioDecBuffer[i]->nFilledLen],&s_buffer[packet_offset],len);
        pInAudioDecBuffer[i]->nFilledLen += len;
        packet_offset += len;
        len = (int)(*((int*)&s_buffer[packet_offset]));
        if(len <0 || len > pInAudioDecBuffer[i]->nAllocLen) {
          DEBUG(DEB_LEV_ERR,"In while breaking len=%d at offset=%d\n",len,packet_offset);
          break;
        }
        packet_offset += offset;
        if((s_n - packet_offset) <len) {
          break;
        }          
      }
    }
  }
  
  err = OMX_EmptyThisBuffer(appPriv->audiodecHandle, pInAudioDecBuffer[0]);
  err = OMX_EmptyThisBuffer(appPriv->audiodecHandle, pInAudioDecBuffer[1]);
  if(err != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
  }

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

  err = OMX_SendCommand(appPriv->audiosrcHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audioencHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audiodecHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
  err = OMX_SendCommand(appPriv->audiosinkHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  tsem_down(appPriv->audiosrcEventSem);
  tsem_down(appPriv->audioencEventSem);
  tsem_down(appPriv->audiodecEventSem);
  tsem_down(appPriv->audiosinkEventSem);
  
  DEBUG(DEFAULT_MESSAGES, "All audio components Transitioned to Idle\n");

  err = OMX_SendCommand(appPriv->audiosrcHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->audioencHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->audiodecHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  err = OMX_SendCommand(appPriv->audiosinkHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

  err = OMX_FreeBuffer(appPriv->audioencHandle, 1, pOutAudioEncBuffer[0]);
  err = OMX_FreeBuffer(appPriv->audioencHandle, 1, pOutAudioEncBuffer[1]);

  err = OMX_FreeBuffer(appPriv->audiodecHandle, 0, pInAudioDecBuffer[0]);
  err = OMX_FreeBuffer(appPriv->audiodecHandle, 0, pInAudioDecBuffer[1]);

  tsem_down(appPriv->audiosrcEventSem);
  tsem_down(appPriv->audioencEventSem);
  tsem_down(appPriv->audiodecEventSem);
  tsem_down(appPriv->audiosinkEventSem);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "All components released\n");

  OMX_FreeHandle(appPriv->audiosrcHandle);
  OMX_FreeHandle(appPriv->audioencHandle);
  OMX_FreeHandle(appPriv->audiodecHandle);
  OMX_FreeHandle(appPriv->audiosinkHandle);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "All Component Handle freed\n");

  OMX_Deinit();

  DEBUG(DEFAULT_MESSAGES, "All components freed. Closing...\n");
  free(appPriv->audiosrcEventSem);
  free(appPriv->audioencEventSem);
  free(appPriv->audiodecEventSem);
  free(appPriv->audiosinkEventSem);

  return 0;
}

/** callbacks implementation */
OMX_ERRORTYPE audiosrcEventHandler(
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
      tsem_up(appPriv->audiosrcEventSem);
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

OMX_ERRORTYPE audioencEventHandler(
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
      tsem_up(appPriv->audioencEventSem);
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

OMX_ERRORTYPE audioencFillBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int inputBufferDropped = 0;
  int *len;
  if(pBuffer != NULL) {
    if(!bEOS) {
      bzero(c_buffer,BUFFER_OUT_SIZE);
      memcpy(&c_buffer[4],pBuffer->pBuffer,pBuffer->nFilledLen);
      len = (int *)c_buffer;
      *len=pBuffer->nFilledLen;
      //DEBUG(DEFAULT_MESSAGES, "In %s Write packet size=%d\n",__func__,*len);
      c_n = write(c_sockfd,c_buffer,pBuffer->nFilledLen+4);
      if (c_n < 0) {
        bEOS = OMX_TRUE;
        DEBUG(DEB_LEV_ERR,"ERROR writing to socket");
        inputBufferDropped++;
        return OMX_ErrorNone;
      }
            
      err = OMX_FillThisBuffer(appPriv->audioencHandle, pBuffer);
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

OMX_ERRORTYPE audiodecEventHandler(
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
      tsem_up(appPriv->audiodecEventSem);
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


OMX_ERRORTYPE audiodecEmptyBufferDone(
  OMX_OUT OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_PTR pAppData,
  OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  static int iBufferDropped = 0;
  int len=0,offset=4,packet_offset=0,i=0;

  if(pBuffer != NULL) {
    if(!bEOS) {
      bzero(s_buffer,BUFFER_OUT_SIZE);
      s_n = read(s_newsockfd,s_buffer,read_buffer_size);
      if (s_n < 0) {
        bEOS = OMX_TRUE;
        DEBUG(DEB_LEV_ERR,"ERROR reading from socket");
        iBufferDropped++;
        return OMX_ErrorNone;
      }
      len = *((int*)&s_buffer[0]);
      //DEBUG(DEFAULT_MESSAGES, "In %s sn_=%d len=%d Read\n",__func__,s_n,len);
      pBuffer->nFilledLen = 0;
      if((len > 0) && (len <= pBuffer->nAllocLen) && (s_n > 0)) {
        packet_offset = offset;
        while (s_n >= packet_offset) {
          memcpy(&pBuffer->pBuffer[pBuffer->nFilledLen],&s_buffer[packet_offset],len);
          pBuffer->nFilledLen += len;
          packet_offset += len;
          len = (int)(*((int*)&s_buffer[packet_offset]));
          i++;
          if(len <0 || len > pBuffer->nAllocLen) {
            DEBUG(DEB_LEV_ERR,"In while breaking len=%d at offset=%d,fillen=%d,s_n=%d,i=%d\n",
              len,packet_offset,(int)pBuffer->nFilledLen,s_n,i);
            break;
          }
          packet_offset += offset;
        }
      }
      if(!bEOS) {
        err = OMX_EmptyThisBuffer(appPriv->audiodecHandle, pBuffer);
        if(err != OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Error %08x Calling FillThisBuffer\n", __func__,err);
        }
      } else {
        iBufferDropped++;
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


/** Callbacks implementation of the audio source component*/
OMX_ERRORTYPE audiosinkEventHandler(
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
      tsem_up(appPriv->audiosinkEventSem);
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

