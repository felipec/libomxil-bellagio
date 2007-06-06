/**
  @file src/components/vorbis/omx_vorbisdec_component.c

  This component implements a ogg-vorbis decoder. The vorbis decoder is based on libvorbis
  software library.

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

  $Date: 2007-06-05 13:33:56 +0200 (Tue, 05 Jun 2007) $
  Revision $Rev: 921 $
  Author $Author: giulio_urlini $

*/

#include <omxcore.h>
#include <omx_vorbisdec_component.h>
/** modification to include audio formats */
#include <OMX_Audio.h>

#define MAX_COMPONENT_VORBISDEC 4
/** Maximum Number of Audio Vorbis Component Instance*/
OMX_U32 noVorbisDecInstance = 0;
ogg_int16_t convbuffer[4096];

/** global variable specifically for vorbis format */
                                                                                                                          
int convsize;
ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
ogg_packet       op; /* one raw packet of data for decode */
                                                                                                                             
vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                          settings */
vorbis_comment   vc; /* struct that stores all the bitstream user comments */
vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
vorbis_block     vb; /* local working space for packet->PCM decode */
                                                                                                                             
char *vorbis_buffer;
int  bytes;
                                                                                                                             
static int second_header_buffer_processed=0;


/** The Constructor
  * @param cComponentName is the name of the component to be initialized
  */

OMX_ERRORTYPE omx_vorbisdec_component_Constructor( OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private;
  omx_vorbisdec_component_PortType *outPort;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_vorbisdec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL)	{
      return OMX_ErrorInsufficientResources;
    }
  }	else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }
  omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?	
    */
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */
  omx_vorbisdec_component_Private = (omx_vorbisdec_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  strcpy(omx_vorbisdec_component_Private->name,cComponentName);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "constructor of vorbisdecoder component is called\n");

  /** Allocate Ports and Call base port constructor. */	
  if (omx_vorbisdec_component_Private->sPortTypesParam.nPorts && !omx_vorbisdec_component_Private->ports) {
    omx_vorbisdec_component_Private->ports = calloc(omx_vorbisdec_component_Private->sPortTypesParam.nPorts, sizeof(omx_base_PortType *));
    if (!omx_vorbisdec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_vorbisdec_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; size of the struct is for derived class port type
      * this could be refactored as a smarter factory function instead?
      */
      omx_vorbisdec_component_Private->ports[i] = calloc(1, sizeof(omx_vorbisdec_component_PortType));
      if (!omx_vorbisdec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_port_Constructor(openmaxStandComp, &omx_vorbisdec_component_Private->ports[0], 0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp, &omx_vorbisdec_component_Private->ports[1], 1, OMX_FALSE);

  /** Domain specific section for the ports. */	
  /** first we set the parameter common to both formats
  	* common parameters related to input port
		*/
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  /**	common parameters related to output port */
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  /** now it's time to know the audio coding type of the component 
    * if audio coding type is set other than vorbis then error returned
    */
  if(!strcmp(cComponentName, AUDIO_DEC_VORBIS_NAME)) {
    omx_vorbisdec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
  }	else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME)) {
    omx_vorbisdec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;
  }	else  {
    /** IL client specified an invalid component name */
    return OMX_ErrorInvalidComponentName;
  }

  if(!omx_vorbisdec_component_Private->avCodecSyncSem) {
    omx_vorbisdec_component_Private->avCodecSyncSem = calloc(1, sizeof(tsem_t));
    if(omx_vorbisdec_component_Private->avCodecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_vorbisdec_component_Private->avCodecSyncSem, 0);
  }

  omx_vorbisdec_component_SetInternalParameters(openmaxStandComp);

  outPort = (omx_vorbisdec_component_PortType *) omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  outPort->sAudioParam.nPortIndex = 1;
  outPort->sAudioParam.nIndex = 0;
  outPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;


  /** settings of output port 
    * output is pcm audo format - so set the pcm mode settings
    */ 
  setHeader(&omx_vorbisdec_component_Private->pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_vorbisdec_component_Private->pAudioPcmMode.nPortIndex = 1;
  omx_vorbisdec_component_Private->pAudioPcmMode.nChannels = 2;
  omx_vorbisdec_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_vorbisdec_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_vorbisdec_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_vorbisdec_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_vorbisdec_component_Private->pAudioPcmMode.nSamplingRate = 44100;
  omx_vorbisdec_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_vorbisdec_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_vorbisdec_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  /** some more component private structure initialization */
  omx_vorbisdec_component_Private->BufferMgmtCallback = omx_vorbisdec_component_BufferMgmtCallbackVorbis;	
  omx_vorbisdec_component_Private->messageHandler = omx_vorbis_decoder_MessageHandler;
  omx_vorbisdec_component_Private->destructor = omx_vorbisdec_component_Destructor;
  openmaxStandComp->SetParameter = omx_vorbisdec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_vorbisdec_component_GetParameter;

  /** increase the counter of initialized components and check against the maximum limit */
  noVorbisDecInstance++;

  if(noVorbisDecInstance > MAX_COMPONENT_VORBISDEC) {
    return OMX_ErrorInsufficientResources;
  }

  return err;
}


/** The destructor
 */
OMX_ERRORTYPE omx_vorbisdec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  int i;
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;

  if(omx_vorbisdec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_vorbisdec_component_Private->avCodecSyncSem);
    free(omx_vorbisdec_component_Private->avCodecSyncSem);
    omx_vorbisdec_component_Private->avCodecSyncSem = NULL;
  }

  /** frees the locally dynamic allocated memory */
  if (omx_vorbisdec_component_Private->sPortTypesParam.nPorts && omx_vorbisdec_component_Private->ports) {
    for (i=0; i < omx_vorbisdec_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_vorbisdec_component_Private->ports[i])
        base_port_Destructor(omx_vorbisdec_component_Private->ports[i]);
    }
    free(omx_vorbisdec_component_Private->ports);
    omx_vorbisdec_component_Private->ports = NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of vorbisdecoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noVorbisDecInstance--;

  return OMX_ErrorNone;
}

/** sets some parameters of the private structure for decoding */

void omx_vorbisdec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private;
  omx_vorbisdec_component_PortType *pPort;

  omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;
	
  if(omx_vorbisdec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS)	{
    omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/vorbis";
    omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
                                                                                                                             
    setHeader(&omx_vorbisdec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    omx_vorbisdec_component_Private->pAudioVorbis.nPortIndex = 0;
    omx_vorbisdec_component_Private->pAudioVorbis.nChannels = 2;                                                                                                                          
    omx_vorbisdec_component_Private->pAudioVorbis.nBitRate = 28000;
    omx_vorbisdec_component_Private->pAudioVorbis.nSampleRate = 44100;
    omx_vorbisdec_component_Private->pAudioVorbis.nAudioBandWidth = 0; 
    omx_vorbisdec_component_Private->pAudioVorbis.nQuality = 3; 
		
    pPort = (omx_vorbisdec_component_PortType *) omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = 0;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingVORBIS;
  }
}

/** The Initialization function 
  */
OMX_ERRORTYPE omx_vorbisdec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)	{
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /** Temporary First Output buffer size*/
  omx_vorbisdec_component_Private->inputCurrBuffer = NULL;
  omx_vorbisdec_component_Private->inputCurrLength = 0;
  nBufferSize = omx_vorbisdec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_vorbisdec_component_Private->internalOutputBuffer = (OMX_U8 *) malloc(nBufferSize);
  memset(omx_vorbisdec_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_vorbisdec_component_Private->isFirstBuffer = 1;
  omx_vorbisdec_component_Private->positionInOutBuf = 0;
  omx_vorbisdec_component_Private->isNewBuffer = 1;
	
  /** initializing vorbis decoder parameters */
  ogg_sync_init(&oy);
  second_header_buffer_processed = 0;
                                                                                                                             
  return err;
};

/** The Deinitialization function 
  */
OMX_ERRORTYPE omx_vorbisdec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  free(omx_vorbisdec_component_Private->internalOutputBuffer);
  omx_vorbisdec_component_Private->internalOutputBuffer = NULL;
	
  /** reset te vorbis decoder related parameters */
  second_header_buffer_processed = 0;
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
  ogg_sync_clear(&oy);
                                                                                                                             
  return err;
}


/** central buffer management function 
  * @param inputbuffer contains the input ogg file content
  * @param outputbuffer is returned along with its output pcm file content that is produced as a result of this function execution
  */
void omx_vorbisdec_component_BufferMgmtCallbackVorbis(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {

  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U8* outputCurrBuffer;
  OMX_U32 outputLength;
  OMX_S32 result;	
  float **pcm;
  OMX_S32 samples;
  OMX_S32 i, j;
  OMX_S32 bout;
  OMX_S32 clipflag=0;
  int val;
  float  *mono;
  static OMX_S32 index=0;
  int eos=0;

 
	DEBUG(DEB_LEV_FULL_SEQ, "input buf %x filled len : %d \n", (int)inputbuffer->pBuffer, (int)inputbuffer->nFilledLen);	
  /** Fill up the current input buffer when a new buffer has arrived */
  if(omx_vorbisdec_component_Private->isNewBuffer) {
    omx_vorbisdec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
    omx_vorbisdec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
    omx_vorbisdec_component_Private->positionInOutBuf = 0;

    DEBUG(DEB_LEV_SIMPLE_SEQ, "new -- input buf %x filled len : %d \n", (int)inputbuffer->pBuffer, (int)inputbuffer->nFilledLen);	

    /** for each new input buffer --- copy buffer content into into ogg sync state structure data */
    vorbis_buffer = ogg_sync_buffer(&oy, inputbuffer->nAllocLen);
    memcpy(vorbis_buffer, inputbuffer->pBuffer, inputbuffer->nFilledLen);
    ogg_sync_wrote(&oy, inputbuffer->nFilledLen);
    DEBUG(DEB_LEV_FULL_SEQ,"***** bytes read to buffer (of first header): %d \n",(int)inputbuffer->nFilledLen);
  }
  outputCurrBuffer = outputbuffer->pBuffer;
  outputLength = outputbuffer->nAllocLen;
  outputbuffer->nFilledLen = 0;
  outputbuffer->nOffset = 0;
	
  if(omx_vorbisdec_component_Private->isFirstBuffer || second_header_buffer_processed == 1) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "in processing the first header buffer\n");

    omx_vorbisdec_component_Private->isNewBuffer = 0;
    if(omx_vorbisdec_component_Private->isFirstBuffer) {
      omx_vorbisdec_component_Private->isFirstBuffer = 0;
      if(ogg_sync_pageout(&oy, &og) != 1)	{
        DEBUG(DEB_LEV_ERR, "this input stream is not a vorbis stream\n");
        exit(1);
      }	
      ogg_stream_init(&os, ogg_page_serialno(&og));		
      vorbis_info_init(&vi);
      vorbis_comment_init(&vc);

      if(ogg_stream_pagein(&os, &og) < 0)	{
        DEBUG(DEB_LEV_ERR, "Error reading first page of Ogg bitstream data.\n");
        exit(1);
      }
      if(ogg_stream_packetout(&os, &op) != 1)	{
        DEBUG(DEB_LEV_ERR, "Error reading initial header packet.\n");
        exit(1);
      }
      if(vorbis_synthesis_headerin(&vi, &vc, &op) < 0)	{
        DEBUG(DEB_LEV_ERR, "This Ogg bitstream does not contain Vorbis audio data\n");
        exit(1);
      }  
    }

    index=0;
    while(index<2)
    {
      while(index<2)
      {
        int result=ogg_sync_pageout(&oy,&og);
        if(result==0) { //break; /* Need more data */
          omx_vorbisdec_component_Private->isNewBuffer = 1;
          second_header_buffer_processed = 1;	
          inputbuffer->nFilledLen = 0;
          return;
        }
        /* Don't complain about missing or corrupt data yet.  We'll
        catch it at the packet output phase */
        if(result==1)
        {
          ogg_stream_pagein(&os,&og); /* we can ignore any errors here as they'll also become apparent at packetout */
          while(index<2){
            result=ogg_stream_packetout(&os,&op);
            if(result==0)break;
            if(result<0) {
              /* Uh oh; data at some point was corrupted or missing!
              We can't tolerate that in a header.  Die. */
              DEBUG(DEB_LEV_ERR,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }//end if
            vorbis_synthesis_headerin(&vi,&vc,&op);
            index++;
          }//end while
        }//end if
      }//end while

      omx_vorbisdec_component_Private->isNewBuffer = 1;
      if(index == 2) {
        second_header_buffer_processed = 0;	
      } else {
        second_header_buffer_processed = 1;	
      }
      inputbuffer->nFilledLen = 0;
      return;

    }//end while
  }

  if(index==2){
    index=0;
    /* Throw the comments plus a few lines about the bitstream we're decoding */
    {
      // ptr should be declared earlier//
      char **ptr=vc.user_comments;
      while(*ptr){
        DEBUG(DEB_LEV_ERR,"%s\n",*ptr);
        ++ptr;
      }
      DEBUG(DEB_LEV_ERR,"Bitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
      DEBUG(DEB_LEV_ERR,"Encoded by: %s\n\n",vc.vendor);
    }

    convsize=inputbuffer->nFilledLen/vi.channels;
    /* OK, got and parsed all three headers. Initialize the Vorbis
    packet->PCM decoder. */
    vorbis_synthesis_init(&vd,&vi); /* central decode state */
    vorbis_block_init(&vd,&vb);/* local state for most of the decode
			                         so multiple block decodes can
			                         proceed in parallel.  We could init
			                         multiple vorbis_block structures
			                         for vd here */
  }
  DEBUG(DEB_LEV_FULL_SEQ,"***** now the decoding will start *****\n");

  if(omx_vorbisdec_component_Private->isNewBuffer)  {
    omx_vorbisdec_component_Private->isNewBuffer=0;
    int result=ogg_sync_pageout(&oy,&og);
    DEBUG(DEB_LEV_FULL_SEQ," --->  page (read in decoding) - header len :  %ld body len : %ld \n",og.header_len,og.body_len);
    if(result == 0)	{
      omx_vorbisdec_component_Private->isNewBuffer = 1;
      inputbuffer->nFilledLen = 0;
      return;
    }
    if(result<0)
    { 
      /* missing or corrupt data at this page position */
      DEBUG(DEB_LEV_ERR,"Corrupt or missing data in bitstream; continuing...\n");
    }
    else {
      ogg_stream_pagein(&os,&og); /* can safely ignore errors at */
    }
  }
  
  result=ogg_stream_packetout(&os,&op);
  DEBUG(DEB_LEV_FULL_SEQ," packet length (read in decoding a particular page): %ld \n",op.bytes);
  if(result == 0)	{
    omx_vorbisdec_component_Private->isNewBuffer = 1;
    inputbuffer->nFilledLen = 0;
    return;
  }
  if(result<0)
  {
    /* missing or corrupt data at this page position */
    /* no reason to complain; already complained above */
    DEBUG(DEB_LEV_ERR,"Corrupt or missing data in bitstream; continuing...\n");
  } else {
    /* we have a packet.  Decode it */

    if(vorbis_synthesis(&vb,&op)==0) /* test for success! */
    vorbis_synthesis_blockin(&vd,&vb);
    /**pcm is a multichannel float vector.  In stereo, for
      example, pcm[0] is left, and pcm[1] is right.  samples is
      the size of each channel.  Convert the float values
      (-1.<=range<=1.) to whatever PCM format and write it out */

    while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0)
    {
      bout=(samples<convsize?samples:convsize);

      /* convert floats to 16 bit signed ints (host order) and interleave */
      for(i=0;i<vi.channels;i++)
      {
        ogg_int16_t *ptr=convbuffer+i;
        mono=pcm[i];
        for(j=0;j<bout;j++)
        {
#if 1
          val=mono[j]*32767.f;
#else     /* optional dither */
          val=mono[j]*32767.f+drand48()-0.5f;
#endif
          /* might as well guard against clipping */
          if(val>32767)
          {
            val=32767;
            clipflag=1;
          }
          if(val<-32768)
          {
            val=-32768;
            clipflag=1;
          }
          *ptr=val;
          ptr+=vi.channels;
        }
      }

      outputbuffer->nFilledLen=2*vi.channels*bout;
      memcpy(outputCurrBuffer,(char *)convbuffer,outputbuffer->nFilledLen);

      if(clipflag) {
        DEBUG(DEB_LEV_FULL_SEQ,"Clipping in frame %ld\n",(long)(vd.sequence));
      }
      vorbis_synthesis_read(&vd,bout); /* tell libvorbis how many samples we actually consumed */
    }
  }
  if(ogg_page_eos(&og)) {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s EOS Detected\n",__func__);
    eos=1;
  }
 
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n", (int)outputbuffer->pBuffer, (int)outputbuffer->nFilledLen);	
}

/** setting parameter values
	* @param hComponent is handle of component
	* @param nParamIndex is the indextype of the parameter
	* @param ComponentParameterStructure is the input structure containing parameter setings
	*/
OMX_ERRORTYPE omx_vorbisdec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)	{
	
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; 
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /** Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_vorbisdec_component_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch (nParamIndex) {

  case OMX_IndexParamAudioInit:
    /** Check Structure Header */
    CHECK_HEADER(err,ComponentParameterStructure,OMX_PORT_PARAM_TYPE);
    memcpy(&omx_vorbisdec_component_Private->sPortTypesParam, ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
    break;	
			
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /** Check Structure Header and verify component state */
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    CHECK_ERROR(err, "Parameter Check");
    if (portIndex <= 1) {
      port = (omx_vorbisdec_component_PortType *) omx_vorbisdec_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam,pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;	
			
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    portIndex = pAudioPcmMode->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    CHECK_ERROR(err, "Parameter Check");
    memcpy(&omx_vorbisdec_component_Private->pAudioPcmMode, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
    break;

  case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    portIndex = pAudioVorbis->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioVorbis, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    CHECK_ERROR(err, "Parameter Check");
    if(pAudioVorbis->nPortIndex == 0)	{
      memcpy(&omx_vorbisdec_component_Private->pAudioVorbis, pAudioVorbis, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    } else	{
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp( (char*) pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE)) {
      omx_vorbisdec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
    } else {
      return OMX_ErrorBadParameter;
    }
    omx_vorbisdec_component_SetInternalParameters(openmaxStandComp);
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

/** getting parameter values
	* @param hComponent is handle of component
	* @param nParamIndex is the indextype of the parameter
	* @param ComponentParameterStructure is the structure to contain obtained parameter setings
	*/
OMX_ERRORTYPE omx_vorbisdec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)	{
	
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; 
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  omx_vorbisdec_component_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = (omx_vorbisdec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
	
  case OMX_IndexParamAudioInit:
    CHECK_HEADER(err,ComponentParameterStructure,OMX_PORT_PARAM_TYPE);
    memcpy(ComponentParameterStructure, &omx_vorbisdec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		

  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    CHECK_HEADER(err,ComponentParameterStructure,OMX_AUDIO_PARAM_PORTFORMATTYPE);
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_vorbisdec_component_PortType *)omx_vorbisdec_component_Private->ports[pAudioPortFormat->nPortIndex];
      memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;		

  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    CHECK_HEADER(err,ComponentParameterStructure,OMX_AUDIO_PARAM_PCMMODETYPE);
    if (pAudioPcmMode->nPortIndex > 1) {
      return OMX_ErrorBadPortIndex;
    }
    memcpy(pAudioPcmMode, &omx_vorbisdec_component_Private->pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;

  case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    if(pAudioVorbis->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    CHECK_HEADER(err,ComponentParameterStructure,OMX_AUDIO_PARAM_VORBISTYPE);
    memcpy(pAudioVorbis, &omx_vorbisdec_component_Private->pAudioVorbis, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    CHECK_HEADER(err,ComponentParameterStructure,OMX_PARAM_COMPONENTROLETYPE);
    if (omx_vorbisdec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
      strcpy( (char*) pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE);
    } else {
      strcpy( (char*) pComponentRole->cRole, "\0");;
    }
    break;

  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

/** handles the message generated by the IL client 
	* @param message is the message type
	*/
OMX_ERRORTYPE omx_vorbis_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)	{
  omx_vorbisdec_component_PrivateType* omx_vorbisdec_component_Private = (omx_vorbisdec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_vorbisdec_component_Private->state == OMX_StateLoaded)) {
      err = omx_vorbisdec_component_Init(openmaxStandComp);
      CHECK_ERROR(err, "vorbis Decoder Init Failed");
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_vorbisdec_component_Private->state == OMX_StateIdle)) {
      err = omx_vorbisdec_component_Deinit(openmaxStandComp);
      CHECK_ERROR(err, "vorbis Decoder Deinit Failed");
    }
  }
  // Execute the base message handling
  return omx_base_component_MessageHandler(openmaxStandComp, message);
}
