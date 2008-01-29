/**
  @file src/components/mad/omx_maddec_component.c

  This component implements and mp3 decoder based on mad
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

  $Date$
  Revision $Rev$
  Author $Author$

*/
 
#include <omxcore.h>
#include <omx_base_audio_port.h>
#include <omx_maddec_component.h>
#include <id3tag.h>

#define MIN(X,Y)    ((X) < (Y) ?  (X) : (Y))

/** Maximum Number of Audio Mad Decoder Component Instance*/
#define MAX_COMPONENT_MADDEC 4

/** This is used when the temporary buffer takes data of this length specified
  from input buffer before its processing */
#define TEMP_BUF_COPY_SPACE 1024

/** This is the temporary buffer size used for last portion of input buffer storage */
#define TEMP_BUFFER_SIZE DEFAULT_IN_BUFFER_SIZE * 2

/** this function initializates the mad framework, and opens an mad decoder of type specified by IL client */ 
OMX_ERRORTYPE omx_maddec_component_madLibInit(omx_maddec_component_PrivateType* omx_maddec_component_Private) {
  
  mad_stream_init (omx_maddec_component_Private->stream);
  mad_frame_init (omx_maddec_component_Private->frame);
  mad_synth_init (omx_maddec_component_Private->synth);
  tsem_up (omx_maddec_component_Private->madDecSyncSem);
  return OMX_ErrorNone;  
}


/** this function Deinitializates the mad framework, and close the mad decoder */
void omx_maddec_component_madLibDeInit(omx_maddec_component_PrivateType* omx_maddec_component_Private) {
  
  mad_synth_finish (omx_maddec_component_Private->synth);
  mad_frame_finish (omx_maddec_component_Private->frame);
  mad_stream_finish (omx_maddec_component_Private->stream);
}

/** The Constructor 
  *
  * @param cComponentName name of the component to be constructed
  */
OMX_ERRORTYPE omx_maddec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp, OMX_STRING cComponentName) {
  
  OMX_ERRORTYPE err = OMX_ErrorNone;  
  omx_maddec_component_PrivateType* omx_maddec_component_Private;
  omx_base_audio_PortType *inPort,*outPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_maddec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate==NULL)  {
      return OMX_ErrorInsufficientResources;
    }
  }  else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", 
              __func__, (int)openmaxStandComp->pComponentPrivate);
  }
  
  omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_maddec_component_Private->ports = NULL;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?  
    */
  err = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "constructor of mad decoder component is called\n");

  /** Domain specific section for the ports. */  
  /** first we set the parameter common to both formats
    * parameters related to input port which does not depend upon input audio format
    */

  /** Allocate Ports and call port constructor. */  
  if (omx_maddec_component_Private->sPortTypesParam.nPorts && !omx_maddec_component_Private->ports) {
    omx_maddec_component_Private->ports = calloc(omx_maddec_component_Private->sPortTypesParam.nPorts, sizeof(omx_base_PortType *));
    if (!omx_maddec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_maddec_component_Private->sPortTypesParam.nPorts; i++) {
      omx_maddec_component_Private->ports[i] = calloc(1, sizeof(omx_base_audio_PortType));
      if (!omx_maddec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_audio_port_Constructor(openmaxStandComp, &omx_maddec_component_Private->ports[0], 0, OMX_TRUE);
  base_audio_port_Constructor(openmaxStandComp, &omx_maddec_component_Private->ports[1], 1, OMX_FALSE);
  
  
  /** parameters related to input port */
  inPort = (omx_base_audio_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  
  inPort->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;
  strcpy(inPort->sPortParam.format.audio.cMIMEType, "audio/mpeg");
  inPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

  inPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

  setHeader(&omx_maddec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
  omx_maddec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
  omx_maddec_component_Private->pAudioMp3.nChannels = 2;                                                                    
  omx_maddec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
  omx_maddec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
  omx_maddec_component_Private->pAudioMp3.nAudioBandWidth = 0;
  omx_maddec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;
  omx_maddec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

  /** parameters related to output port */
  outPort = (omx_base_audio_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  outPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

  outPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;

  /** settings of output port audio format - pcm */
  setHeader(&omx_maddec_component_Private->pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
  omx_maddec_component_Private->pAudioPcmMode.nPortIndex = 1;
  omx_maddec_component_Private->pAudioPcmMode.nChannels = 2;
  omx_maddec_component_Private->pAudioPcmMode.eNumData = OMX_NumericalDataSigned;
  omx_maddec_component_Private->pAudioPcmMode.eEndian = OMX_EndianLittle;
  omx_maddec_component_Private->pAudioPcmMode.bInterleaved = OMX_TRUE;
  omx_maddec_component_Private->pAudioPcmMode.nBitPerSample = 16;
  omx_maddec_component_Private->pAudioPcmMode.nSamplingRate = 44100;
  omx_maddec_component_Private->pAudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
  omx_maddec_component_Private->pAudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
  omx_maddec_component_Private->pAudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

  /** now it's time to know the audio coding type of the component */
  if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))  {   
    omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
  }  else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME)) {
    omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;
  }  else  {
    // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;
  }
  /** initialise the semaphore to be used for mad decoder access synchronization */
  if(!omx_maddec_component_Private->madDecSyncSem) {
    omx_maddec_component_Private->madDecSyncSem = malloc(sizeof(tsem_t));
    if(omx_maddec_component_Private->madDecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_maddec_component_Private->madDecSyncSem, 0);
  }

  /** general configuration irrespective of any audio formats
    *  setting values of other fields of omx_maddec_component_Private structure  
    */ 
  omx_maddec_component_Private->maddecReady = OMX_FALSE;
  omx_maddec_component_Private->BufferMgmtCallback = omx_maddec_component_BufferMgmtCallback;
  omx_maddec_component_Private->messageHandler = omx_mad_decoder_MessageHandler;
  omx_maddec_component_Private->destructor = omx_maddec_component_Destructor;
  openmaxStandComp->SetParameter = omx_maddec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_maddec_component_GetParameter;

  /** initialising mad structures */
  omx_maddec_component_Private->stream = malloc (sizeof(struct mad_stream));
  omx_maddec_component_Private->synth = malloc (sizeof(struct mad_synth));
  omx_maddec_component_Private->frame = malloc (sizeof(struct mad_frame));

  return err;
}

/** this function sets some inetrnal parameters to the input port depending upon the input audio format */
void omx_maddec_component_SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_maddec_component_PrivateType* omx_maddec_component_Private;
  omx_base_audio_PortType *pPort;;

  omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;

  /** setting port & private fields according to mp3 audio format values */
  strcpy(omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType, "audio/mpeg");
  omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

  setHeader(&omx_maddec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
  omx_maddec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
  omx_maddec_component_Private->pAudioMp3.nChannels = 2;                                                                    
  omx_maddec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
  omx_maddec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
  omx_maddec_component_Private->pAudioMp3.nAudioBandWidth = 0;
  omx_maddec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;
  omx_maddec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

  pPort = (omx_base_audio_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
  pPort->sAudioParam.nPortIndex = 0;
  pPort->sAudioParam.nIndex = 0;
  pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

}


/** The destructor */
OMX_ERRORTYPE omx_maddec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_maddec_component_Private->madDecSyncSem) {
    tsem_deinit(omx_maddec_component_Private->madDecSyncSem);
    free(omx_maddec_component_Private->madDecSyncSem);
    omx_maddec_component_Private->madDecSyncSem = NULL;
  }

  /** freeing mad decoder structures */
  free(omx_maddec_component_Private->stream);
  omx_maddec_component_Private->stream = NULL;
  free(omx_maddec_component_Private->synth);
  omx_maddec_component_Private->synth = NULL;
  free(omx_maddec_component_Private->frame);
  omx_maddec_component_Private->frame = NULL;

  /* frees port/s */
  if (omx_maddec_component_Private->ports) {
    for (i=0; i < omx_maddec_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_maddec_component_Private->ports[i])
        omx_maddec_component_Private->ports[i]->PortDestructor(omx_maddec_component_Private->ports[i]);
    }
    free(omx_maddec_component_Private->ports);
    omx_maddec_component_Private->ports=NULL;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of mad decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);

  return OMX_ErrorNone;

}

/** The Initialization function  */
OMX_ERRORTYPE omx_maddec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)  {

  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  /** initializing omx_maddec_component_Private->temporary_buffer with 2k memory space*/
  omx_maddec_component_Private->temporary_buffer = malloc(sizeof(OMX_BUFFERHEADERTYPE));
  omx_maddec_component_Private->temporary_buffer->pBuffer = malloc(DEFAULT_IN_BUFFER_SIZE*2);
  memset(omx_maddec_component_Private->temporary_buffer->pBuffer, 0, DEFAULT_IN_BUFFER_SIZE*2);

  omx_maddec_component_Private->temp_input_buffer = omx_maddec_component_Private->temporary_buffer->pBuffer;
  omx_maddec_component_Private->temporary_buffer->nFilledLen=0;
  omx_maddec_component_Private->temporary_buffer->nOffset=0;

  omx_maddec_component_Private->isFirstBuffer = 1;
  omx_maddec_component_Private->isNewBuffer = 1;

  return err;
}

/** The Deinitialization function  */
OMX_ERRORTYPE omx_maddec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (omx_maddec_component_Private->maddecReady) {
    omx_maddec_component_madLibDeInit(omx_maddec_component_Private);
    omx_maddec_component_Private->maddecReady = OMX_FALSE;
  }

  /*Restore temporary input buffer pointer*/
  omx_maddec_component_Private->temporary_buffer->pBuffer = omx_maddec_component_Private->temp_input_buffer;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "Freeing Temporary Buffer\n");
  /* freeing temporary memory allocation */
  if(omx_maddec_component_Private->temporary_buffer->pBuffer) {
    free(omx_maddec_component_Private->temporary_buffer->pBuffer);
    omx_maddec_component_Private->temporary_buffer->pBuffer = NULL;
  }
  if(omx_maddec_component_Private->temporary_buffer) {
    free(omx_maddec_component_Private->temporary_buffer);
    omx_maddec_component_Private->temporary_buffer = NULL;
  }
  
  return err;
}

/** The following utility routine performs simple rounding, clipping, and
  * scaling of MAD's high-resolution samples down to 16 bits. It does not
  * perform any dithering or noise shaping, which would be recommended to
  * obtain any exceptional audio quality. 
  */
static inline int scale_int (mad_fixed_t sample) {

  #if MAD_F_FRACBITS < 28
    /* round */
    sample += (1L << (28 - MAD_F_FRACBITS - 1));
  #endif

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  #if MAD_F_FRACBITS < 28
    /* quantize */
    sample >>= (28 - MAD_F_FRACBITS);
  #endif

  /* convert from 29 bits to 32 bits */
  return (int) (sample << 3);
}

/** This function is the buffer management callback function for mp3 decoding
  * is used to process the input buffer and provide one output buffer 
  * @param inputbuffer is the input buffer containing the input mp3 content
  * @param outputbuffer is the output buffer on which the output pcm content will be written
  */
void omx_maddec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {
  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;  
  OMX_U32 nchannels;
  int count;
  int consumed = 0;
  int nsamples;
  unsigned char const *before_sync, *after_sync;
  mad_fixed_t const *left_ch, *right_ch;
  unsigned short *outdata;
  int tocopy;
 
  outputbuffer->nFilledLen = 0;
  outputbuffer->nOffset=0;

  if(omx_maddec_component_Private->isNewBuffer==1 || omx_maddec_component_Private->need_mad_stream == 1) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s New Buffer len=%d\n", __func__,(int)inputbuffer->nFilledLen);

    /** first copy TEMP_BUF_COPY_SPACE bytes of new input buffer to add with temporary buffer content  */
    tocopy = MIN (MAD_BUFFER_MDLEN, MIN (inputbuffer->nFilledLen,
              MAD_BUFFER_MDLEN * 3 - omx_maddec_component_Private->temporary_buffer->nFilledLen));

    if (tocopy == 0) {
      DEBUG(DEB_LEV_ERR,"mad claims to need more data than %u bytes, we don't have that much", MAD_BUFFER_MDLEN * 3);
      inputbuffer->nFilledLen=0;
      omx_maddec_component_Private->isNewBuffer = 1;
      return;
    }

    if(omx_maddec_component_Private->need_mad_stream == 1) {
      DEBUG(DEB_LEV_SIMPLE_SEQ,"In %s memmove temp buf len=%d\n", __func__,(int)omx_maddec_component_Private->temporary_buffer->nFilledLen);
      memmove (omx_maddec_component_Private->temp_input_buffer, omx_maddec_component_Private->temporary_buffer->pBuffer, omx_maddec_component_Private->temporary_buffer->nFilledLen);
      omx_maddec_component_Private->temporary_buffer->pBuffer = omx_maddec_component_Private->temp_input_buffer;
      omx_maddec_component_Private->need_mad_stream = 0;
      memcpy(omx_maddec_component_Private->temporary_buffer->pBuffer+omx_maddec_component_Private->temporary_buffer->nFilledLen, inputbuffer->pBuffer + inputbuffer->nOffset, tocopy);
      omx_maddec_component_Private->temporary_buffer->nFilledLen += tocopy;
      inputbuffer->nFilledLen -= tocopy;
      inputbuffer->nOffset += tocopy;

      DEBUG(DEB_LEV_SIMPLE_SEQ, "Input buffer filled len : %d temp buf len = %d tocopy=%d\n", (int)inputbuffer->nFilledLen, (int)omx_maddec_component_Private->temporary_buffer->nFilledLen,tocopy);
      omx_maddec_component_Private->isNewBuffer = 0;

      mad_stream_buffer(omx_maddec_component_Private->stream, omx_maddec_component_Private->temporary_buffer->pBuffer, omx_maddec_component_Private->temporary_buffer->nFilledLen);
    }
    if(inputbuffer->nFilledLen == 0) {
      omx_maddec_component_Private->isNewBuffer = 1;
      inputbuffer->nOffset=0;
    }
  }

  /* added separate header decoding to catch errors earlier, also fixes
   * some weird decoding errors... */
  DEBUG(DEB_LEV_SIMPLE_SEQ,"decoding the header now\n");
  
  if (mad_header_decode (&(omx_maddec_component_Private->frame->header), omx_maddec_component_Private->stream) == -1) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"mad_header_decode had an error: %s\n",
        mad_stream_errorstr (omx_maddec_component_Private->stream));
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ,"decoding one frame now\n");

  /** this flag setting disables the CRC check */
  omx_maddec_component_Private->frame->header.flags &= ~MAD_FLAG_PROTECTION;

  if (mad_frame_decode (omx_maddec_component_Private->frame, omx_maddec_component_Private->stream) == -1) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"got error %d\n", omx_maddec_component_Private->stream->error);

    /* not enough data, need to wait for next buffer? */
    if (omx_maddec_component_Private->stream->error == MAD_ERROR_BUFLEN) {
      if (omx_maddec_component_Private->stream->next_frame == omx_maddec_component_Private->temporary_buffer->pBuffer) {
        DEBUG(DEB_LEV_SIMPLE_SEQ,"not enough data in tempbuffer  breaking to get more\n");
        omx_maddec_component_Private->need_mad_stream=1;
        return;
      } else {
        DEBUG(DEB_LEV_SIMPLE_SEQ,"sync error, flushing unneeded data\n");
        /* figure out how many bytes mad consumed */
        /** if consumed is already set, it's from the resync higher up, so
          * we need to use that value instead.  Otherwise, recalculate from
          * mad's consumption */
        if (consumed == 0) {
          consumed = omx_maddec_component_Private->stream->next_frame - omx_maddec_component_Private->temporary_buffer->pBuffer;
        }
        DEBUG(DEB_LEV_SIMPLE_SEQ,"consumed %d bytes\n", consumed);
        /* move out pointer to where mad want the next data */
        omx_maddec_component_Private->temporary_buffer->pBuffer += consumed;
        omx_maddec_component_Private->temporary_buffer->nFilledLen -= consumed;
        return;
      }
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ,"mad_frame_decode had an error: %s\n",
        mad_stream_errorstr (omx_maddec_component_Private->stream));
    if (!MAD_RECOVERABLE (omx_maddec_component_Private->stream->error)) {
     DEBUG(DEB_LEV_ERR,"non recoverable error");
    } else if (omx_maddec_component_Private->stream->error == MAD_ERROR_LOSTSYNC) {
      /* lost sync, force a resync */
      signed long tagsize;
      tagsize = id3_tag_query(omx_maddec_component_Private->stream->this_frame, omx_maddec_component_Private->stream->bufend - omx_maddec_component_Private->stream->this_frame);
      mad_stream_skip(omx_maddec_component_Private->stream, tagsize);
      DEBUG(DEB_LEV_SIMPLE_SEQ,"recoverable lost sync error\n");
    }

    mad_frame_mute (omx_maddec_component_Private->frame);
    mad_synth_mute (omx_maddec_component_Private->synth);
    before_sync = omx_maddec_component_Private->stream->ptr.byte;
    if (mad_stream_sync (omx_maddec_component_Private->stream) != 0)
      DEBUG(DEB_LEV_ERR,"mad_stream_sync failed\n");
    after_sync = omx_maddec_component_Private->stream->ptr.byte;
    /* a succesful resync should make us drop bytes as consumed, so
       calculate from the byte pointers before and after resync */
    consumed = after_sync - before_sync;
    DEBUG(DEB_LEV_SIMPLE_SEQ,"resynchronization consumes %d bytes\n", consumed);
    DEBUG(DEB_LEV_SIMPLE_SEQ,"synced to data: 0x%0x 0x%0x\n", *omx_maddec_component_Private->stream->ptr.byte,
        *(omx_maddec_component_Private->stream->ptr.byte + 1));

    mad_stream_sync (omx_maddec_component_Private->stream);
    /* recoverable errors pass */
    /* figure out how many bytes mad consumed */
    /** if consumed is already set, it's from the resync higher up, so
      * we need to use that value instead.  Otherwise, recalculate from
      * mad's consumption */
    if (consumed == 0) {
      consumed = omx_maddec_component_Private->stream->next_frame - omx_maddec_component_Private->temporary_buffer->pBuffer;
    }
    DEBUG(DEB_LEV_SIMPLE_SEQ,"consumed %d bytes\n", consumed);
    /* move out pointer to where mad want the next data */
    omx_maddec_component_Private->temporary_buffer->pBuffer += consumed;
    omx_maddec_component_Private->temporary_buffer->nFilledLen -= consumed;
    return;
  } 

  /* if we're not resyncing/in error, check if caps need to be set again */
  nsamples = MAD_NSBSAMPLES (&omx_maddec_component_Private->frame->header) *
      (omx_maddec_component_Private->stream->options & MAD_OPTION_HALFSAMPLERATE ? 16 : 32);
  nchannels = MAD_NCHANNELS (&omx_maddec_component_Private->frame->header);

  if((omx_maddec_component_Private->pAudioPcmMode.nSamplingRate != omx_maddec_component_Private->frame->header.samplerate) ||
    ( omx_maddec_component_Private->pAudioPcmMode.nChannels!=nchannels)) {
    DEBUG(DEB_LEV_FULL_SEQ, "---->Sending Port Settings Change Event\n");

    switch(omx_maddec_component_Private->audio_coding_type)  {
    case OMX_AUDIO_CodingMP3 :
      /*Update Parameter which has changed from avCodecContext*/
      /*pAudioMp3 is for input port Mp3 data*/
      omx_maddec_component_Private->pAudioMp3.nChannels = nchannels;
      omx_maddec_component_Private->pAudioMp3.nBitRate = omx_maddec_component_Private->frame->header.bitrate;
      omx_maddec_component_Private->pAudioMp3.nSampleRate = omx_maddec_component_Private->frame->header.samplerate;
      /*pAudioPcmMode is for output port PCM data*/
      omx_maddec_component_Private->pAudioPcmMode.nChannels = nchannels;
      omx_maddec_component_Private->pAudioPcmMode.nSamplingRate = 32;
      omx_maddec_component_Private->pAudioPcmMode.nSamplingRate = omx_maddec_component_Private->frame->header.samplerate;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "Audio format other than mp3 not supported\nCodec not found\n");
      break;                       
    }

    /*Send Port Settings changed call back*/
    (*(omx_maddec_component_Private->callbacks->EventHandler))
    (openmaxStandComp,
    omx_maddec_component_Private->callbackData,
    OMX_EventPortSettingsChanged, /* The command was completed */
    0, 
    1, /* This is the output port index */
    NULL);
  }


  mad_synth_frame (omx_maddec_component_Private->synth, omx_maddec_component_Private->frame);
  left_ch = omx_maddec_component_Private->synth->pcm.samples[0];
  right_ch = omx_maddec_component_Private->synth->pcm.samples[1];

  outdata = (unsigned short *)outputbuffer->pBuffer;
  outputbuffer->nFilledLen=nsamples * nchannels * 2;

  // output sample(s) in 16-bit signed native-endian PCM //
  if (nchannels == 1) {
    count = nsamples;

    while (count--) {
      *outdata++ = (scale_int (*left_ch++) >>16) & 0xffff;
    }
  } else {
    count = nsamples;
    while (count--) {
      *outdata++ = (scale_int (*left_ch++) >>16) & 0xffff;
      *outdata++ = (scale_int (*right_ch++)>>16) & 0xffff;
    }
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ,"Returning output buffer size=%d \n", (int)outputbuffer->nFilledLen);

  /* figure out how many bytes mad consumed */
  /** if consumed is already set, it's from the resync higher up, so
    * we need to use that value instead.  Otherwise, recalculate from
    * mad's consumption */
  if (consumed == 0)
    consumed = omx_maddec_component_Private->stream->next_frame - omx_maddec_component_Private->temporary_buffer->pBuffer;

  DEBUG(DEB_LEV_SIMPLE_SEQ,"consumed %d bytes\n", consumed);
  /* move out pointer to where mad want the next data */
  omx_maddec_component_Private->temporary_buffer->pBuffer += consumed;
  omx_maddec_component_Private->temporary_buffer->nFilledLen -= consumed;
}

/** this function sets the parameter values regarding audio format & index */
OMX_ERRORTYPE omx_maddec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure)  {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
  OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
  OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_audio_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    portIndex = pAudioPortFormat->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    if (portIndex <= 1) {
      port = (omx_base_audio_PortType *) omx_maddec_component_Private->ports[portIndex];
      memcpy(&port->sAudioParam, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    portIndex = pAudioPcmMode->nPortIndex;
    /*Check Structure Header and verify component state*/
    err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    memcpy(&omx_maddec_component_Private->pAudioPcmMode, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));          
    break;
    
  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if (!strcmp( (char*) pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) {
      omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
    }  else {
      return OMX_ErrorBadParameter;
    }
    omx_maddec_component_SetInternalParameters(openmaxStandComp);
    break;

  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
    portIndex = pAudioMp3->nPortIndex;
    err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    if(err!=OMX_ErrorNone) { 
      DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err); 
      break;
    }
    if (pAudioMp3->nPortIndex == 0) {
      memcpy(&omx_maddec_component_Private->pAudioMp3, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE)); 
    }  else {
      return OMX_ErrorBadPortIndex;
    }
    break;

  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
  
}

/** this function gets the parameters regarding audio formats and index */
OMX_ERRORTYPE omx_maddec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure)  {

  OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;  
  OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
  OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
  OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
  omx_base_audio_PortType *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamAudioInit:
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(ComponentParameterStructure, &omx_maddec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
    break;    

  case OMX_IndexParamAudioPortFormat:
    pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPortFormat->nPortIndex <= 1) {
      port = (omx_base_audio_PortType *)omx_maddec_component_Private->ports[pAudioPortFormat->nPortIndex];
      memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    } else {
      return OMX_ErrorBadPortIndex;
    }
    break;    

  case OMX_IndexParamAudioPcm:
    pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (pAudioPcmMode->nPortIndex > 1) {
      return OMX_ErrorBadPortIndex;
    }
    memcpy(pAudioPcmMode, &omx_maddec_component_Private->pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    break;

  case OMX_IndexParamAudioMp3:
    pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
    if (pAudioMp3->nPortIndex != 0) {
      return OMX_ErrorBadPortIndex;
    }
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_MP3TYPE))) != OMX_ErrorNone) { 
      break;
    }
    memcpy(pAudioMp3, &omx_maddec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
    break;

  case OMX_IndexParamStandardComponentRole:
    pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
    if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
      break;
    }
    if (omx_maddec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
      strcpy( (char*) pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
    }  else {
      strcpy( (char*) pComponentRole->cRole,"\0");;
    }
    break;
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
  
}

/** message handling of mad decoder */
OMX_ERRORTYPE omx_mad_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp, internalRequestMessageType *message)  {

  omx_maddec_component_PrivateType* omx_maddec_component_Private = (omx_maddec_component_PrivateType*)openmaxStandComp->pComponentPrivate;  
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_maddec_component_Private->state;
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle) && (omx_maddec_component_Private->state == OMX_StateLoaded)) {
      err = omx_maddec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s MAD Decoder Init Failed Error=%x\n",__func__,err); 
        return err;
      }
    } else if ((message->messageParam == OMX_StateExecuting) && (omx_maddec_component_Private->state == OMX_StateIdle)) {
      DEBUG(DEB_LEV_FULL_SEQ, "State Changing from Idle to Exec\n");
      omx_maddec_component_Private->temporary_buffer->nFilledLen=0;
      omx_maddec_component_Private->temporary_buffer->nOffset=0;
      omx_maddec_component_Private->need_mad_stream = 1;
      if (!omx_maddec_component_Private->maddecReady) {
        err = omx_maddec_component_madLibInit(omx_maddec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_maddec_component_Private->maddecReady = OMX_TRUE;
      }
    }
  }
  /** Execute the base message handling */
  err = omx_base_component_MessageHandler(openmaxStandComp, message);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateLoaded) && (eCurrentState == OMX_StateIdle)) {
      err = omx_maddec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s MAD Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      }
    }else if ((message->messageParam == OMX_StateIdle) && (eCurrentState == OMX_StateExecuting)) {
      omx_maddec_component_madLibDeInit(omx_maddec_component_Private);
      omx_maddec_component_Private->maddecReady = OMX_FALSE;
    }
  }

  return err;  
}
 
