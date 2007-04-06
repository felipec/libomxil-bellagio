/**
 * @file src/components/ffmpeg/omx_audiodec_component.c
 *
 * This component implements and mp3 decoder. The Mp3/WMA decoder is based on ffmpeg
 * software library.
 *
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Pankaj SEN,Ukri NIEMIMUUKKO, Diego MELPIGNANO, , David SIORPAES, Giulio URLINI
 * 
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 * 2006/05/11:  ffmpeg mp3 decoder component version 0.2
 *
 */


#include <omxcore.h>
#include <omx_audiodec_component.h>
/** modification to include audio formats */
#include<OMX_Audio.h>

#define MAX_COMPONENT_AUDIODEC 4
/** Maximum Number of Audio Component Instance*/
OMX_U32 noAudioDecInstance=0;


//global variable specifically for vorbis format
                                                                                                                             
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
                                                                                                                             
static int second_header_buffer_processed;



/** The Constructor 
 */

OMX_ERRORTYPE omx_audiodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE err = OMX_ErrorNone;	
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_audiodec_component_PortType *outPort;
  OMX_S32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, allocating component\n",__func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_audiodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)
      return OMX_ErrorInsufficientResources;
  }
  else 
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n",__func__, (int)openmaxStandComp->pComponentPrivate);

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;

  // we could create our own port structures here
  // fixme maybe the base class could use a "port factory" function pointer?	
  err = omx_base_filter_Constructor(openmaxStandComp,cComponentName);

  /* here we can override whatever defaults the base_component constructor set
  * e.g. we can override the function pointers in the private struct  */
  omx_audiodec_component_Private = (omx_audiodec_component_PrivateType *)openmaxStandComp->pComponentPrivate;

  //debug statement
  DEBUG(DEB_LEV_SIMPLE_SEQ,"constructor of audiodecoder component is called\n");

  /** Allocate Ports and Call base port constructor. */	
  if (omx_audiodec_component_Private->sPortTypesParam.nPorts && !omx_audiodec_component_Private->ports) {
    omx_audiodec_component_Private->ports = calloc(omx_audiodec_component_Private->sPortTypesParam.nPorts,sizeof (omx_base_PortType *));
    if (!omx_audiodec_component_Private->ports) return OMX_ErrorInsufficientResources;

    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam.nPorts; i++) {
      // this is the important thing separating this from the base class; size of the struct is for derived class port type
      // this could be refactored as a smarter factory function instead?
      omx_audiodec_component_Private->ports[i] = calloc(1, sizeof(omx_audiodec_component_PortType));
      if (!omx_audiodec_component_Private->ports[i]) return OMX_ErrorInsufficientResources;
    }
  }

  base_port_Constructor(openmaxStandComp,&omx_audiodec_component_Private->ports[0],0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp,&omx_audiodec_component_Private->ports[1],1, OMX_FALSE);

  /** Domain specific section for the ports. */	
  // first we set the parameter common to both formats
  //common parameters related to input port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

  //common parameters related to output port
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  // now it's time to know the audio coding type of the component
  if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))   // mp3 format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

  else if(!strcmp(cComponentName, AUDIO_DEC_VORBIS_NAME))   // VORBIS format decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
		
  else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME))// general audio decoder
    omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;

  else  // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;

  if(!omx_audiodec_component_Private->avCodecSyncSem) {
    omx_audiodec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_audiodec_component_Private->avCodecSyncSem == NULL) return OMX_ErrorInsufficientResources;
    tsem_init(omx_audiodec_component_Private->avCodecSyncSem, 0);
  }

  SetInternalParameters(openmaxStandComp);

  outPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  outPort->sAudioParam.nPortIndex = 1;
  outPort->sAudioParam.nIndex = 0;
  outPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;


  //settings of output port
  //output is pcm mode for all decoders - so generalise it 
  setHeader(&omx_audiodec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_audiodec_component_Private->pAudioPcmMode.nPortIndex = 1;
  omx_audiodec_component_Private->pAudioPcmMode.nChannels = 2;
  omx_audiodec_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_audiodec_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_audiodec_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = 44100;
  omx_audiodec_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_audiodec_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_audiodec_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  //general configuration irrespective of any audio formats
  //setting other parameters of omx_audiodec_component_private	
  omx_audiodec_component_Private->avCodec = NULL;
  omx_audiodec_component_Private->avCodecContext= NULL;
  omx_audiodec_component_Private->avcodecReady = OMX_FALSE;

	//selection of two different callback functions for two different audio decoding
  if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) //for vorbis decoding
    omx_audiodec_component_Private->BufferMgmtCallback = omx_audiodec_component_BufferMgmtCallbackVorbis;
  else //for mp3 decoding
    omx_audiodec_component_Private->BufferMgmtCallback = omx_audiodec_component_BufferMgmtCallback;
                                                                                                                             
  omx_audiodec_component_Private->messageHandler = omx_audio_decoder_MessageHandler;
  omx_audiodec_component_Private->destructor = omx_audiodec_component_Destructor;
  //omx_audiodec_component_Private->DomainCheck	 = &omx_audiodec_component_DomainCheck;

  openmaxStandComp->SetParameter = omx_audiodec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_audiodec_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_audiodec_component_ComponentRoleEnum;

  noAudioDecInstance++;

  if(noAudioDecInstance>MAX_COMPONENT_AUDIODEC)
    return OMX_ErrorInsufficientResources;

  return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_audiodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) 
{
  int i;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;

  if(omx_audiodec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_audiodec_component_Private->avCodecSyncSem);
    free(omx_audiodec_component_Private->avCodecSyncSem);
    omx_audiodec_component_Private->avCodecSyncSem=NULL;
  }

  //frees the locally dynamic allocated memory
  if (omx_audiodec_component_Private->sPortTypesParam.nPorts && omx_audiodec_component_Private->ports) {
    for (i=0; i < omx_audiodec_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_audiodec_component_Private->ports[i])
        base_port_Destructor(omx_audiodec_component_Private->ports[i]);
    }
    free(omx_audiodec_component_Private->ports);
    omx_audiodec_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of audiodecoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  noAudioDecInstance--;

  return OMX_ErrorNone;
}

/** 
	It initializates the ffmpeg framework, and opens an ffmpeg audiodecoder of type specified by IL client - currently only used for mp3 decoding
*/ 
OMX_ERRORTYPE omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {

  OMX_U32 target_codecID;  // id of ffmpeg codec to be used for different audio formats 
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFMpeg Library/codec iniited\n");

  switch(omx_audiodec_component_Private->audio_coding_type){
  case OMX_AUDIO_CodingMP3 :
    target_codecID = CODEC_ID_MP2;
    break;
  default :
    DEBUG(DEB_LEV_ERR, "Audio format other than mp3 not supported\nCodec not found\n");
    return OMX_ErrorComponentNotFound;
  }
	
  /*Find the  decoder corresponding to the audio type specified by IL client*/
  omx_audiodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_audiodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec Not found\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_audiodec_component_Private->avCodecContext = avcodec_alloc_context();

  /*open the avcodec if mp3 format selected */
  if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3)
  {
	  if (avcodec_open(omx_audiodec_component_Private->avCodecContext, omx_audiodec_component_Private->avCodec) < 0) 
		{
  	  DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    	return OMX_ErrorInsufficientResources;
  	}
	}
	
	tsem_up(omx_audiodec_component_Private->avCodecSyncSem);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");
	return OMX_ErrorNone;
}

/** 
	It Deinitializates the ffmpeg framework, and close the ffmpeg mp3 decoder
*/
void omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_PrivateType* omx_audiodec_component_Private) {
	
  avcodec_close(omx_audiodec_component_Private->avCodecContext);

  if (omx_audiodec_component_Private->avCodecContext->priv_data)
    avcodec_close (omx_audiodec_component_Private->avCodecContext);

  if (omx_audiodec_component_Private->avCodecContext->extradata) {
    av_free (omx_audiodec_component_Private->avCodecContext->extradata);
    omx_audiodec_component_Private->avCodecContext->extradata = NULL;
  }
  /*Free Codec Context*/
  av_free (omx_audiodec_component_Private->avCodecContext);
    
}

void SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private;
  omx_audiodec_component_PortType *pPort;

  omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
	
  if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) 
	{
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/mpeg";
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

    setHeader(&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
    omx_audiodec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
    omx_audiodec_component_Private->pAudioMp3.nChannels = 2;                                                                    
    omx_audiodec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
    omx_audiodec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
    omx_audiodec_component_Private->pAudioMp3.nAudioBandWidth = 0;
    omx_audiodec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    setHeader(&omx_audiodec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    omx_audiodec_component_Private->pAudioMp3.nPortIndex=0;
    omx_audiodec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

    pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = 0;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;
  } 

	else if(omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS)
  {
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/vorbis";
    omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
                                                                                                                             
    setHeader(&omx_audiodec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    omx_audiodec_component_Private->pAudioVorbis.nPortIndex = 0;
    omx_audiodec_component_Private->pAudioVorbis.nChannels = 2;                                                                                                                          
    omx_audiodec_component_Private->pAudioVorbis.nBitRate = 28000;
    omx_audiodec_component_Private->pAudioVorbis.nSampleRate = 44100;
    omx_audiodec_component_Private->pAudioVorbis.nAudioBandWidth = 0; //encoder decides the needed bandwidth
    omx_audiodec_component_Private->pAudioVorbis.nQuality = 3; //default quality
		
		pPort = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

    setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pPort->sAudioParam.nPortIndex = 0;
    pPort->sAudioParam.nIndex = 0;
    pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingVORBIS;

  }

}

/** The Initialization function 
 */
OMX_ERRORTYPE omx_audiodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 nBufferSize;

  /*Temporary First Output buffer size*/
  omx_audiodec_component_Private->inputCurrBuffer=NULL;
  omx_audiodec_component_Private->inputCurrLength=0;
  nBufferSize=omx_audiodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
  omx_audiodec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(nBufferSize);
  memset(omx_audiodec_component_Private->internalOutputBuffer, 0, nBufferSize);
  omx_audiodec_component_Private->isFirstBuffer=1;
  omx_audiodec_component_Private->positionInOutBuf = 0;
  omx_audiodec_component_Private->isNewBuffer=1;
	
	
  //for vorbis decoder
  ogg_sync_init(&oy);
  second_header_buffer_processed = 0;
                                                                                                                             
  return err;
	
};

/** The Deinitialization function 
 */
OMX_ERRORTYPE omx_audiodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (omx_audiodec_component_Private->avcodecReady) {
    omx_audiodec_component_ffmpegLibDeInit(omx_audiodec_component_Private);
    omx_audiodec_component_Private->avcodecReady = OMX_FALSE;
  }

  free(omx_audiodec_component_Private->internalOutputBuffer);
	omx_audiodec_component_Private->internalOutputBuffer = NULL;

  //for vorbis decoder
  second_header_buffer_processed = 0;
  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
  ogg_sync_clear(&oy);

  return err;
}

/*Deprecated function. May or may not be used in future version*/
/**Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_audiodec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef) {
  if(pDef.eDomain!=OMX_PortDomainAudio)
    return OMX_ErrorPortsNotCompatible;
  else if(pDef.format.audio.eEncoding == OMX_AUDIO_CodingMax)
    return OMX_ErrorPortsNotCompatible;

  return OMX_ErrorNone;
}

/** This function is used to process the input buffer and provide one output buffer
 */
void omx_audiodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_S32 outputfilled = 0;
  OMX_U8* outputCurrBuffer;
  OMX_U32 outputLength;
  OMX_U32 len = 0;
  int internalOutputFilled=0;

  /**Fill up the current input buffer when a new buffer has arrived*/
  if(omx_audiodec_component_Private->isNewBuffer) {
    omx_audiodec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
    omx_audiodec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
    omx_audiodec_component_Private->positionInOutBuf = 0;
    omx_audiodec_component_Private->isNewBuffer=0;
  }
  outputCurrBuffer = outputbuffer->pBuffer;
  outputLength = outputbuffer->nAllocLen;
  outputbuffer->nFilledLen = 0;
  outputbuffer->nOffset=0;
	
  while (!outputfilled) 
	{
   	if (omx_audiodec_component_Private->isFirstBuffer) 
		{
     	tsem_down(omx_audiodec_component_Private->avCodecSyncSem);
     	len = avcodec_decode_audio(omx_audiodec_component_Private->avCodecContext, 
              (short*)omx_audiodec_component_Private->internalOutputBuffer, 
              &internalOutputFilled,
              omx_audiodec_component_Private->inputCurrBuffer, 
              omx_audiodec_component_Private->inputCurrLength);
							
							
      DEBUG(DEB_LEV_FULL_SEQ, "Frequency = %i channels = %i\n", omx_audiodec_component_Private->avCodecContext->sample_rate, omx_audiodec_component_Private->avCodecContext->channels);
   	  //omx_audiodec_component_Private->minBufferLength = internalOutputFilled;
      

     	if((omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate != omx_audiodec_component_Private->avCodecContext->sample_rate) ||
       	( omx_audiodec_component_Private->pAudioPcmMode.nChannels!=omx_audiodec_component_Private->avCodecContext->channels)) 
			{
	      DEBUG(DEB_LEV_FULL_SEQ, "---->Sending Port Settings Change Event\n");
				/* has mp3 dependency--requires modification */
			  //switch for different audio formats---parameter settings accordingly
        switch(omx_audiodec_component_Private->audio_coding_type)	
				{
       		case OMX_AUDIO_CodingMP3 :
						/*Update Parameter which has changed from avCodecContext*/
  	        /*pAudioMp3 is for input port Mp3 data*/
    	      omx_audiodec_component_Private->pAudioMp3.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
      	    omx_audiodec_component_Private->pAudioMp3.nBitRate = omx_audiodec_component_Private->avCodecContext->bit_rate;
        	  omx_audiodec_component_Private->pAudioMp3.nSampleRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          	/*pAudioPcmMode is for output port PCM data*/
          	omx_audiodec_component_Private->pAudioPcmMode.nChannels = omx_audiodec_component_Private->avCodecContext->channels;
	          if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S16)
  	          omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample = 16;
    	      else if(omx_audiodec_component_Private->avCodecContext->sample_fmt==SAMPLE_FMT_S32)
      	      omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = 32;
        	  omx_audiodec_component_Private->pAudioPcmMode.nSamplingRate = omx_audiodec_component_Private->avCodecContext->sample_rate;
          	break;
						
	       default :
          DEBUG(DEB_LEV_ERR, "Audio format other than mp3 not supported\nCodec not found\n");
          break;                       
  	    }//end of switch
					
					
				/*Send Port Settings changed call back*/
      	(*(omx_audiodec_component_Private->callbacks->EventHandler))
        	  (openmaxStandComp,
          	omx_audiodec_component_Private->callbackData,
	          OMX_EventPortSettingsChanged, /* The command was completed */
  	        0, 
    	      1, /* This is the output port index */
      	    NULL);
			}
			omx_audiodec_component_Private->minBufferLength = 1152*(omx_audiodec_component_Private->pAudioPcmMode.nBitPerSample/8)*
													omx_audiodec_component_Private->avCodecContext->channels;
      DEBUG(DEB_LEV_FULL_SEQ, "buffer length %i buffer given %i len=%d\n", internalOutputFilled, (int)outputLength, (int)len);
		
      if (internalOutputFilled > outputLength) {
       	DEBUG(DEB_LEV_ERR, "---> Ouch! the output buffer is too small!!!! iof=%d,ol=%d\n", internalOutputFilled, (int)outputLength);
	      inputbuffer->nFilledLen=0;
  	    /*Simply return the output buffer without writing anything*/
    	  internalOutputFilled = 0;
        omx_audiodec_component_Private->positionInOutBuf = 0;
       	outputfilled = 1;
       	break;
      }
    } 
		else  //not first buffer
		{
      len = avcodec_decode_audio(omx_audiodec_component_Private->avCodecContext,
              (short*)(outputCurrBuffer + (omx_audiodec_component_Private->positionInOutBuf * omx_audiodec_component_Private->minBufferLength)), 
              &internalOutputFilled,
              omx_audiodec_component_Private->inputCurrBuffer, 
              omx_audiodec_component_Private->inputCurrLength);
		}
    if (len < 0){
      	DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
    }
				
  	if (internalOutputFilled > 0) 
		{
      omx_audiodec_component_Private->inputCurrBuffer += len;
     	omx_audiodec_component_Private->inputCurrLength -= len;
	    inputbuffer->nFilledLen -= len;
				
      DEBUG(DEB_LEV_FULL_SEQ, "Buf Consumed IbLen=%d Len=%d minlen=%d\n", 
	    (int)inputbuffer->nFilledLen, (int)len, omx_audiodec_component_Private->minBufferLength);
  	  /*Buffer is fully consumed. Request for new Input Buffer*/
    	if(inputbuffer->nFilledLen==0)
        omx_audiodec_component_Private->isNewBuffer=1;
	  } 
		else 
		{
  	  /**  This condition becomes true when the input buffer has completely be consumed.
      *  In this case is immediately switched because there is no real buffer consumption */
     	DEBUG(DEB_LEV_FULL_SEQ, "New Buf Reqd IbLen=%d Len=%d  iof=%d\n", (int)inputbuffer->nFilledLen, (int)len, internalOutputFilled);
	    inputbuffer->nFilledLen=0;
  	  /*Few bytes may be left in the input buffer but can't generate one output frame. 
      Request for new Input Buffer*/
     	omx_audiodec_component_Private->isNewBuffer=1;
	  }

  	if (omx_audiodec_component_Private->isFirstBuffer) 
		{
      memcpy(outputCurrBuffer, omx_audiodec_component_Private->internalOutputBuffer, internalOutputFilled);
     	omx_audiodec_component_Private->isFirstBuffer = 0;
    }
	  if (internalOutputFilled > 0) 
		{
  	  outputbuffer->nFilledLen += internalOutputFilled;
    }

    /* We are done with output buffer */
    //omx_audiodec_component_Private->positionInOutBuf++;

    //if ((omx_audiodec_component_Private->minBufferLength > 
    //  (outputLength - (omx_audiodec_component_Private->positionInOutBuf * omx_audiodec_component_Private->minBufferLength))) || (internalOutputFilled <= 0)) {
    //  internalOutputFilled = 0;
      omx_audiodec_component_Private->positionInOutBuf = 0;
      outputfilled = 1;
      /*Send the output buffer*/
      //}
  }
 
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n", (int)outputbuffer->pBuffer, (int)outputbuffer->nFilledLen);
}

/** buffer management calback function for vorbis decoder
	*/
void omx_audiodec_component_BufferMgmtCallbackVorbis(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
	omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
	OMX_U8* outputCurrBuffer;
	OMX_U32 outputLength;
	OMX_S32 result;
	float **pcm;
	OMX_S32 samples;
  OMX_S32 i,j;
	OMX_S32 bout;
  OMX_S32 clipflag=0;
	OMX_S32 val;
			
	//Fill up the current input buffer when a new buffer has arrived
	if(omx_audiodec_component_Private->isNewBuffer) {
		omx_audiodec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
		omx_audiodec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
		omx_audiodec_component_Private->positionInOutBuf = 0;
		//omx_audiodec_component_Private->isNewBuffer=0; //-- 	done later
		DEBUG(DEB_LEV_SIMPLE_SEQ,"new -- input buf %x filled len : %d \n", (int)inputbuffer->pBuffer, (int)inputbuffer->nFilledLen);
		
		//for each new input buffer --- copy buffer content into into ogg sunc state structure data
		vorbis_buffer = ogg_sync_buffer(&oy,inputbuffer->nAllocLen);
		memcpy(vorbis_buffer,inputbuffer->pBuffer,inputbuffer->nFilledLen);
		ogg_sync_wrote(&oy,inputbuffer->nFilledLen);
	}

	outputCurrBuffer = outputbuffer->pBuffer;
	outputLength = outputbuffer->nAllocLen;
	outputbuffer->nFilledLen = 0;
  outputbuffer->nOffset=0;

  if(omx_audiodec_component_Private->isFirstBuffer) {
		DEBUG(DEB_LEV_SIMPLE_SEQ,"in processing the first header buffer\n");
		
		omx_audiodec_component_Private->isNewBuffer=0;
		omx_audiodec_component_Private->isFirstBuffer = 0;
		//tsem_down(omx_audiodec_component_Private->avCodecSyncSem);
		if(ogg_sync_pageout(&oy,&og)!=1) {
			DEBUG(DEB_LEV_ERR,"this input stream is not a vorbis stream\n");
		}
			
		ogg_stream_init(&os,ogg_page_serialno(&og));
		vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
			
		if(ogg_stream_pagein(&os,&og)<0) {
     	DEBUG(DEB_LEV_ERR,"Error reading first page of Ogg bitstream data.\n");
		}
		if(ogg_stream_packetout(&os,&op)!=1) {
			DEBUG(DEB_LEV_ERR,"Error reading initial header packet.\n");
		}
      
		if(vorbis_synthesis_headerin(&vi,&vc,&op)<0) {
			DEBUG(DEB_LEV_ERR,"This Ogg bitstream does not contain Vorbis audio data\n");
		}

		result=ogg_sync_pageout(&oy,&og);
		if(result == 0) {
			omx_audiodec_component_Private->isNewBuffer=1;
			second_header_buffer_processed = 1;
			inputbuffer->nFilledLen = 0;
		} else {
			DEBUG(DEB_LEV_ERR,"error in reading first header buffer of vorbis stream\n");
		}
	} else if(!omx_audiodec_component_Private->isFirstBuffer && second_header_buffer_processed && omx_audiodec_component_Private->isNewBuffer) {
		DEBUG(DEB_LEV_SIMPLE_SEQ,"in processing the second header buffer\n");
		omx_audiodec_component_Private->isNewBuffer=0;
		result=ogg_sync_pageout(&oy,&og);
		if(result == 0) {
			DEBUG(DEB_LEV_ERR,"error in processing second header buffer\n");
			omx_audiodec_component_Private->isNewBuffer=1;
			second_header_buffer_processed = 0;
			inputbuffer->nFilledLen = 0;
		} else if(result == 1) {
			ogg_stream_pagein(&os,&og);
			result=ogg_stream_packetout(&os,&op);
			if(result == 0) {
				DEBUG(DEB_LEV_ERR,"error n processing 2nd header buffer -- packet --- 1\n");
			} else if(result < 0) {
				DEBUG(DEB_LEV_ERR,"corrupted secondary header ---- 1\n");
			}
			vorbis_synthesis_headerin(&vi,&vc,&op);
			result=ogg_stream_packetout(&os,&op);
			if(result != 0) {
				DEBUG(DEB_LEV_ERR,"2nd header buffer error\n");
			} else {
				result=ogg_sync_pageout(&oy,&og);
				if(result != 1) {
					DEBUG(DEB_LEV_ERR,"2nd header buffer has error after pageout\n");
				} else {
					ogg_stream_pagein(&os,&og);
					result=ogg_stream_packetout(&os,&op);
					if(result == 0) {
						DEBUG(DEB_LEV_ERR,"error n processing 2nd header buffer -- packet ---- 2\n");
					} else if(result < 0) {
						DEBUG(DEB_LEV_ERR,"corrupted secondary header ---- 2\n");	
					}

					vorbis_synthesis_headerin(&vi,&vc,&op);
					omx_audiodec_component_Private->isNewBuffer=1;
					second_header_buffer_processed = 0;
					inputbuffer->nFilledLen = 0;
					
					convsize = inputbuffer->nAllocLen/vi.channels ;
					vorbis_synthesis_init(&vd,&vi);
					vorbis_block_init(&vd,&vb);
				}
			}
		}
	} else {// all headers are parsed --now bitstream decoding
			//if new buffer for decoding then read the page boundary
			if(omx_audiodec_component_Private->isNewBuffer) {
				omx_audiodec_component_Private->isNewBuffer=0;
				result=ogg_sync_pageout(&oy,&og);
				DEBUG(DEB_LEV_SIMPLE_SEQ,"--->  page (read in decoding) - header len :  %ld body len : %ld \n",og.header_len,og.body_len);
				if(result == 0 || result < 0) {
					omx_audiodec_component_Private->isNewBuffer=1;
					inputbuffer->nFilledLen = 0;
				} else {
					ogg_stream_pagein(&os,&og);
				}
			}
			//this condition is useful becz if it is false, then instead of decoding errorneous packet, go to get next buffer
			if(!omx_audiodec_component_Private->isNewBuffer) {
				//now read packet by packet from already estimated page boundary
				result=ogg_stream_packetout(&os,&op);
				DEBUG(DEB_LEV_SIMPLE_SEQ,"packet length (read in decoding a particular page): %ld  ",op.bytes);
				if(result == 0 || result < 0) {
					result = ogg_sync_pageout(&oy,&og);
					if(result == 0) {
						omx_audiodec_component_Private->isNewBuffer=1;
						inputbuffer->nFilledLen = 0;
					}
				} else {
					if(vorbis_synthesis(&vb,&op)==0) { // test for success! 
						vorbis_synthesis_blockin(&vd,&vb);
					}
					// **pcm is a multichannel float vector.  In stereo, for example, pcm[0] is left, and pcm[1] is right.  
					// samples is the size of each channel.  Convert the float values (-1.<=range<=1.) to whatever PCM format and write it out 
					while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0) {
						bout=(samples<convsize?samples:convsize);
						// convert floats to 16 bit signed ints (host order) and interleave 
						for(i=0;i<vi.channels;i++) {
							float  *mono=pcm[i];
							outputCurrBuffer = outputbuffer->pBuffer + 2*i;

							for(j=0;j<bout;j++) {
								val=mono[j]*32767.f;
								// might as well guard against clipping 
								if(val>32767) {
									val=32767;clipflag=1;
								}
								if(val<-32768) {
									val=-32768;clipflag=1;
								}
								//putting the output
								*outputCurrBuffer = (val >> 0) & 0xff;
								outputCurrBuffer++; outputbuffer->nFilledLen++;
								*outputCurrBuffer = (val >> 8) & 0xff;
								outputCurrBuffer++; outputbuffer->nFilledLen++;
								//changing pointer position to make room for interleaved futuredata for the other channel
								outputCurrBuffer += 2; 
							}
						}
						if(clipflag) {
							DEBUG(DEB_LEV_SIMPLE_SEQ,"Clipping in frame %ld\n",(long)(vd.sequence));
						}
						vorbis_synthesis_read(&vd,bout); // tell libvorbis how many samples we actually consumed 
					}
				}
			}
		}
	DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x len=%d is full returning\n", (int)outputbuffer->pBuffer, (int)outputbuffer->nFilledLen);
}

OMX_ERRORTYPE omx_audiodec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) 
{

  switch (nIndex) {
  default: // delegate to superclass
    return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
  switch (nIndex) {
  default: // delegate to superclass
    return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; //support for Vorbis format
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_audiodec_component_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    /*Check Structure Header*/
    err = checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
    CHECK_ERROR(err,"Check Header");
    //if (err != OMX_ErrorNone)
    //  return err;
    memcpy(&omx_audiodec_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
    break;	
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    CHECK_ERROR(err,"Parameter Check");
    //if (err != OMX_ErrorNone)
      //return err;
    if (portIndex <= 1) {
      port = (omx_audiodec_component_PortType *) omx_audiodec_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;	
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    portIndex = pAudioPcmMode->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    CHECK_ERROR(err,"Parameter Check");
    //if (err != OMX_ErrorNone)
    //  return err;
    memcpy(&omx_audiodec_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
    break;

	case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    portIndex = pAudioVorbis->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
		CHECK_ERROR(err,"Parameter Check");
    //if(err!=OMX_ErrorNone)
    //  return err;
    if(pAudioVorbis->nPortIndex == 0)
      memcpy(&omx_audiodec_component_Private->pAudioVorbis,pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    else
      return OMX_ErrorBadPortIndex;
    break;

	
	
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    } else if (!strcmp((char*)pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE)) {
      omx_audiodec_component_Private->audio_coding_type = OMX_AUDIO_CodingVORBIS;
    } else {
      return OMX_ErrorBadParameter;
    }
    SetInternalParameters(openmaxStandComp);
    break;
		
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
    portIndex = pAudioMp3->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    CHECK_ERROR(err,"Parameter Check");
    //if(err!=OMX_ErrorNone)
    //  return err;
    if (pAudioMp3->nPortIndex == 0) {
      memcpy(&omx_audiodec_component_Private->pAudioMp3,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audiodec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_AUDIO_PARAM_VORBISTYPE *pAudioVorbis; //support for Vorbis format	
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
  omx_audiodec_component_PortType *port;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
    memcpy(ComponentParameterStructure, &omx_audiodec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;		
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_audiodec_component_PortType *)omx_audiodec_component_Private->ports[pAudioPortFormat->nPortIndex];
      memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;		
  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    setHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    if (pAudioPcmMode->nPortIndex > 1) {
      return OMX_ErrorBadPortIndex;
    }
    memcpy(pAudioPcmMode,&omx_audiodec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;
  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    if (pAudioMp3->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    setHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    memcpy(pAudioMp3,&omx_audiodec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    break;
		
	case OMX_IndexParamAudioVorbis:
    pAudioVorbis = (OMX_AUDIO_PARAM_VORBISTYPE*)ComponentParameterStructure;
    if (pAudioVorbis->nPortIndex != 0) {
       return OMX_ErrorBadPortIndex;
    }
    setHeader(pAudioVorbis, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    memcpy(pAudioVorbis,&omx_audiodec_component_Private->pAudioVorbis,sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
    break;
	
		
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    setHeader(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));

    if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
    }	else if (omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingVORBIS) {
      strcpy((char*)pComponentRole->cRole, AUDIO_DEC_VORBIS_ROLE);
    } else {
      strcpy((char*)pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_audio_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message)
{
  omx_audiodec_component_PrivateType* omx_audiodec_component_Private = (omx_audiodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;

  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_audiodec_component_Private->state == OMX_StateLoaded)) {
			//this if "logical AND" condition -- for initializing ffmpeg library only in case of mp3 decoding
      if (!omx_audiodec_component_Private->avcodecReady  &&  omx_audiodec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
        err = omx_audiodec_component_ffmpegLibInit(omx_audiodec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_audiodec_component_Private->avcodecReady = OMX_TRUE;
      }
      err = omx_audiodec_component_Init(openmaxStandComp);
      CHECK_ERROR(err,"Audio Decoder Init Failed");
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_audiodec_component_Private->state == OMX_StateIdle)) {
      err = omx_audiodec_component_Deinit(openmaxStandComp);
      CHECK_ERROR(err,"Audio Decoder Deinit Failed");
    }
  }
  // Execute the base message handling
  return omx_base_component_MessageHandler(openmaxStandComp,message);
}

OMX_ERRORTYPE omx_audiodec_component_ComponentRoleEnum(
    OMX_IN OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_U8 *cRole,
	OMX_IN OMX_U32 nIndex)
{
	if (nIndex == 0) {
		strcpy((char*)cRole, AUDIO_DEC_MP3_ROLE);
	} else if (nIndex == 1) {
    strcpy((char*)cRole, AUDIO_DEC_VORBIS_ROLE);
  }else {
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}



