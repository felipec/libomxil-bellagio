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
	
	$Date: 2007-04-05 16:39:38 +0200 (Thu, 05 Apr 2007) $
	Revision $Rev: 787 $
	Author $Author: giulio_urlini $

*/
 
#include <omxcore.h>
#include <omx_maddec_component.h>
#include <id3tag.h>


#define MAX_COMPONENT_MADDEC 4
/** Maximum Number of Audio Component Instance*/
OMX_U32 noMadDecInstance=0;


/** This is used when the temporary buffer takes data of this length specified
  from input buffer before its processing */
#define TEMP_BUF_COPY_SPACE 1024

/** This is the temporary buffer size used for last portion of input buffer storage */
#define TEMP_BUFFER_SIZE 2048

/** private function prototype */
static inline signed int scale(mad_fixed_t sample);

/** global variable declaration */
OMX_BUFFERHEADERTYPE *temporary_buffer;

static int input_buffer_still_processed = 0;
static int consumed_length;
static int total_consumed_length_before;
static int total_consumed_length_after;


/** this function initializates the mad framework, and opens an mad decoder of type specified by IL client */ 
OMX_ERRORTYPE omx_maddec_component_madLibInit(omx_maddec_component_PrivateType* omx_maddec_component_Private) {
  
	mad_stream_init (omx_maddec_component_Private->stream);
  mad_frame_init (omx_maddec_component_Private->frame);
  mad_synth_init (omx_maddec_component_Private->synth);
  tsem_up(omx_maddec_component_Private->madDecSyncSem);
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
	omx_maddec_component_PortType *outPort;
	OMX_S32 i;
	
	if (!openmaxStandComp->pComponentPrivate) {
		openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_maddec_component_PrivateType));
		if(openmaxStandComp->pComponentPrivate==NULL)	{
			return OMX_ErrorInsufficientResources;
		}
	}	else {
    DEBUG(DEB_LEV_FUNCTION_NAME,"In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
	}
	
	omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	
	/** we could create our own port structures here
		* fixme maybe the base class could use a "port factory" function pointer?	
		*/
	err = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

	/** here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  
	 */
	omx_maddec_component_Private = (omx_maddec_component_PrivateType *)openmaxStandComp->pComponentPrivate;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "constructor of mad decoder component is called\n");
	
  /** Allocate Ports and Call base port constructor. */	
  if (omx_maddec_component_Private->sPortTypesParam.nPorts && !omx_maddec_component_Private->ports) {
    omx_maddec_component_Private->ports = calloc(omx_maddec_component_Private->sPortTypesParam.nPorts, sizeof (omx_base_PortType *));
    if (!omx_maddec_component_Private->ports) {
			return OMX_ErrorInsufficientResources;
		}
    for (i=0; i < omx_maddec_component_Private->sPortTypesParam.nPorts; i++) {
      /** this is the important thing separating this from the base class; size of the struct is for derived class port type
      	* this could be refactored as a smarter factory function instead?
				*/
      omx_maddec_component_Private->ports[i] = calloc(1, sizeof(omx_maddec_component_PortType));
      if (!omx_maddec_component_Private->ports[i]) {
				return OMX_ErrorInsufficientResources;
			}
    }
  }

  base_port_Constructor(openmaxStandComp,&omx_maddec_component_Private->ports[0], 0, OMX_TRUE);
  base_port_Constructor(openmaxStandComp,&omx_maddec_component_Private->ports[1], 1, OMX_FALSE);

	/** Domain specific section for the ports. */	
	/** first we set the parameter common to both formats
		* parameters related to input port which does not depend upon input audio format
		*/
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

	/** parameters related to output port */
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

	/** now it's time to know the audio coding type of the component */
	if(!strcmp(cComponentName, AUDIO_DEC_MP3_NAME))	{   
		omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
	}	else if (!strcmp(cComponentName, AUDIO_DEC_BASE_NAME)) {
		omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;
	}	else  {
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
	
	/** setting of internal port parameters related to audio format */
	SetInternalParameters(openmaxStandComp);

	outPort = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	outPort->sAudioParam.nPortIndex = 1;
	outPort->sAudioParam.nIndex = 0;
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
	
	/** general configuration irrespective of any audio formats
	 *  setting values of other fields of omx_maddec_component_Private structure	
	 */ 
	omx_maddec_component_Private->maddecReady = OMX_FALSE;
	omx_maddec_component_Private->BufferMgmtCallback = omx_maddec_component_BufferMgmtCallback;
  omx_maddec_component_Private->messageHandler = omx_mad_decoder_MessageHandler;
  omx_maddec_component_Private->destructor = omx_maddec_component_Destructor;
	//omx_maddec_component_Private->DomainCheck = &omx_maddec_component_DomainCheck;
  openmaxStandComp->SetParameter = omx_maddec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_maddec_component_GetParameter;
  //openmaxStandComp->ComponentRoleEnum = omx_maddec_component_ComponentRoleEnum;
	
	
  /** initialising mad structures */
  omx_maddec_component_Private->stream = malloc (sizeof(struct mad_stream));
  omx_maddec_component_Private->synth = malloc (sizeof(struct mad_synth));
  omx_maddec_component_Private->frame = malloc (sizeof(struct mad_frame));

	/** increamenting component counter and checking against max allowed components */
  noMadDecInstance++;

  if(noMadDecInstance>MAX_COMPONENT_MADDEC)	{
    return OMX_ErrorInsufficientResources;
	}
	return err;	
}

/** this function sets some inetrnal parameters to the input port depending upon the input audio format */
void SetInternalParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

	omx_maddec_component_PrivateType* omx_maddec_component_Private;
	omx_maddec_component_PortType *pPort;;
	
	omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	
	/** setting port & private fields according to mp3 audio format values */
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/mpeg";
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

	setHeader(&omx_maddec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
	omx_maddec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
	omx_maddec_component_Private->pAudioMp3.nChannels = 2;                                                                    
	omx_maddec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
	omx_maddec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
	omx_maddec_component_Private->pAudioMp3.nAudioBandWidth = 0;
	omx_maddec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;
  omx_maddec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

	pPort = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	setHeader(&pPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	pPort->sAudioParam.nPortIndex = 0;
	pPort->sAudioParam.nIndex = 0;
	pPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

}


/** The destructor */
OMX_ERRORTYPE omx_maddec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {

	int i;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	
	if(omx_maddec_component_Private->madDecSyncSem) {
		tsem_deinit(omx_maddec_component_Private->madDecSyncSem);
		free(omx_maddec_component_Private->madDecSyncSem);
		omx_maddec_component_Private->madDecSyncSem = NULL;
	}
	
  if (omx_maddec_component_Private->sPortTypesParam.nPorts && omx_maddec_component_Private->ports) {
    for (i=0; i < omx_maddec_component_Private->sPortTypesParam.nPorts; i++) {
      if(omx_maddec_component_Private->ports[i])	{
        base_port_Destructor(omx_maddec_component_Private->ports[i]);
			}
    }
    free(omx_maddec_component_Private->ports);
    omx_maddec_component_Private->ports = NULL;
  }
	
	/** freeing mad decoder structures */
  free(omx_maddec_component_Private->stream);
	omx_maddec_component_Private->stream = NULL;
  free(omx_maddec_component_Private->synth);
	omx_maddec_component_Private->synth = NULL;
  free(omx_maddec_component_Private->frame);
	omx_maddec_component_Private->frame = NULL;

	DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of mad decoder component is called\n");
		
  omx_base_filter_Destructor(openmaxStandComp);
	
  noMadDecInstance--;

  return OMX_ErrorNone;

}

/** The Initialization function  */
OMX_ERRORTYPE omx_maddec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp)	{

	omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	
  /** initializing temporary_buffer with 2k memory space*/
  temporary_buffer = (OMX_BUFFERHEADERTYPE*) malloc(sizeof(OMX_BUFFERHEADERTYPE));
  temporary_buffer->pBuffer = (OMX_U8*) malloc(TEMP_BUFFER_SIZE);
  memset(temporary_buffer->pBuffer, 0, TEMP_BUFFER_SIZE);

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

  /* freeing temporary memory allocation */
  free(temporary_buffer->pBuffer);
	temporary_buffer->pBuffer = NULL;
  free(temporary_buffer);
	temporary_buffer = NULL;
	return err;
	
}

/** Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_maddec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef)	{

	if(pDef.eDomain != OMX_PortDomainAudio)	{
		return OMX_ErrorPortsNotCompatible;
	}	else if(pDef.format.audio.eEncoding == OMX_AUDIO_CodingMax)	{
		return OMX_ErrorPortsNotCompatible;
	}
	return OMX_ErrorNone;
}

/** This function generates the mad output from decoded data 
	* @param outputbuffer is the output buffer on which the output pcm content will be written
	*/
void generate_output(omx_maddec_component_PrivateType* omx_maddec_component_Private,OMX_BUFFERHEADERTYPE* outputbuffer)	{

	mad_fixed_t const *left_ch, *right_ch;
	OMX_U8* outputCurrBuffer;
	int count;
	signed int scale_result;
	
	mad_synth_frame(omx_maddec_component_Private->synth, omx_maddec_component_Private->frame);
	left_ch = omx_maddec_component_Private->synth->pcm.samples[0];
	right_ch = omx_maddec_component_Private->synth->pcm.samples[1];
			
	outputCurrBuffer = (OMX_U8*)outputbuffer->pBuffer;	
	if(omx_maddec_component_Private->synth->pcm.channels == 1) {  
		count = omx_maddec_component_Private->synth->pcm.length;  		
		outputbuffer->nFilledLen += count*2;
	  DEBUG(DEB_LEV_SIMPLE_SEQ, "no of pcm channel : 1  output length increament : %d ",count*2);
		while (count--) {            
			scale_result = scale (*left_ch++);
			*outputCurrBuffer = (scale_result >> 0) & 0xff;
			outputCurrBuffer++; 
			*outputCurrBuffer = (scale_result >> 8) & 0xff;
			outputCurrBuffer++; 
		}		
	} else {   
		count = omx_maddec_component_Private->synth->pcm.length; 
		outputbuffer->nFilledLen += count*4;
		DEBUG(DEB_LEV_SIMPLE_SEQ, "no of pcm channel : 2  output length increament : %d ",count*4);
		while (count--) {
			scale_result = scale (*left_ch++);
			*outputCurrBuffer = (scale_result >> 0) & 0xff;
			outputCurrBuffer++; 
			*outputCurrBuffer = (scale_result >> 8) & 0xff;
			outputCurrBuffer++; 
				
			scale_result = scale (*right_ch++);
			*outputCurrBuffer = (scale_result >> 0) & 0xff;
			outputCurrBuffer++; 
			*outputCurrBuffer = (scale_result >> 8) & 0xff;
			outputCurrBuffer++; 
		} 		
	}

}


/** This function is the buffer management callback function for mp3 decoding
	* is used to process the input buffer and provide one output buffer 
	* @param inputbuffer is the input buffer containing the input mp3 content
	* @param outputbuffer is the output buffer on which the output pcm content will be written
	*/
void omx_maddec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) {

	omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;	
	signed long tagsize;
	int decode_result;
	
	static int bProcTempBuffer = 0;
	
   /** case where 1) new input buffer has come -- not the first buffer or 2) if the temporary buffer is to be processed
   	 * this temporary buffer consists of residual portion of prev buffer 
		 */
   if((omx_maddec_component_Private->isNewBuffer && !omx_maddec_component_Private->isFirstBuffer) || bProcTempBuffer == 1 )	{
	 
		if(bProcTempBuffer == 0 && omx_maddec_component_Private->isNewBuffer) {
		
	    /** first copy TEMP_BUF_COPY_SPACE bytes of new input buffer to add with temporary buffer content  */
	    memcpy(temporary_buffer->pBuffer+temporary_buffer->nFilledLen, inputbuffer->pBuffer, TEMP_BUF_COPY_SPACE);
	    temporary_buffer->nFilledLen += TEMP_BUF_COPY_SPACE;
	    inputbuffer->nFilledLen -= TEMP_BUF_COPY_SPACE;
	    inputbuffer->nOffset += TEMP_BUF_COPY_SPACE;
	    
	    /** now this temp buffer contains residual portion of prev buffer plus the TEMP_BUF_COPY_SPACE bytes of new input buffer
			  * it is to be processed - so set the corersponding integer variable bProcTempBuffer to 1 
				*/
	    bProcTempBuffer = 1;
	    
	    DEBUG(DEB_LEV_SIMPLE_SEQ, "Input buffer filled len : %d temp buf len = %d", (int)inputbuffer->nFilledLen, (int)temporary_buffer->nFilledLen);			
	    omx_maddec_component_Private->isNewBuffer = 0;

	    mad_stream_buffer(omx_maddec_component_Private->stream, temporary_buffer->pBuffer, temporary_buffer->nFilledLen);

	    total_consumed_length_before = 0;
	    total_consumed_length_after  = 0;
	    consumed_length = 0;
		}

    decode_result = mad_frame_decode(omx_maddec_component_Private->frame, omx_maddec_component_Private->stream);
						
		if(decode_result == -1) {
			if (!MAD_RECOVERABLE(omx_maddec_component_Private->stream->error))	{
				if(omx_maddec_component_Private->stream->error == MAD_ERROR_BUFLEN)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Stream error bufflen residual buffer case ");
				}	else if(omx_maddec_component_Private->stream->error == MAD_ERROR_BUFPTR)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Stream error buffer pointer residual buffer case ");
				}	else if(omx_maddec_component_Private->stream->error == MAD_ERROR_NOMEM)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "Stream error no memory residual buffer case ");
				}	else	{
					DEBUG(DEB_LEV_ERR, "Unknown non recoverable error residual buffer case ");
				}
			} else if(omx_maddec_component_Private->stream->error == MAD_ERROR_LOSTSYNC)	{
				tagsize = id3_tag_query(omx_maddec_component_Private->stream->this_frame, omx_maddec_component_Private->stream->bufend - omx_maddec_component_Private->stream->this_frame);
				mad_stream_skip(omx_maddec_component_Private->stream, tagsize);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "Decoding error LOSTSYNC - residual buffer case -  at frame %u - skipping %u bytes", (int)omx_maddec_component_Private->stream->this_frame, (int)tagsize);
			} else {
				generate_output(omx_maddec_component_Private,outputbuffer);
			}
		} else {
    	/** placing the decoded data onto output buffer */
	    generate_output(omx_maddec_component_Private,outputbuffer);
		}


    total_consumed_length_after = (omx_maddec_component_Private->stream->next_frame - omx_maddec_component_Private->stream->buffer);		
    consumed_length = total_consumed_length_after - total_consumed_length_before;
    total_consumed_length_before = total_consumed_length_after;				
    temporary_buffer->nFilledLen -= consumed_length;
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Temp buffer filled len : %d consumed length=%d", (int)temporary_buffer->nFilledLen, consumed_length);
    
    
    if(temporary_buffer->nFilledLen <= consumed_length + 100)	{
			 /** Temporary buffer is fully processed */
    	 bProcTempBuffer = 0;
    }
    
    if(temporary_buffer->nFilledLen != 0 && bProcTempBuffer == 0)	{
			/** A bit portion is left in temporary buffer
				* it is the content of new input buffer
				* better to adjust the offset of new input buffer
				*/
      inputbuffer->nOffset -= temporary_buffer->nFilledLen;
      inputbuffer->nFilledLen += temporary_buffer->nFilledLen;
      DEBUG(DEB_LEV_SIMPLE_SEQ,"Input buffer offset : %d ",(int)inputbuffer->nOffset);
    }		
  }	else {
		/**  not new input buffer (except first buffer) */
		if(omx_maddec_component_Private->isFirstBuffer)	{
			DEBUG(DEB_LEV_SIMPLE_SEQ, "This is the first input buffer");
			omx_maddec_component_Private->isFirstBuffer = 0;
			omx_maddec_component_Private->isNewBuffer = 0;
		}
		/** initialization  */
		if(!input_buffer_still_processed) {
			mad_stream_buffer(omx_maddec_component_Private->stream, (inputbuffer->pBuffer+inputbuffer->nOffset), inputbuffer->nFilledLen);		
			total_consumed_length_before = 0;
			total_consumed_length_after  = 0;
			consumed_length = 0;	
			DEBUG(DEB_LEV_SIMPLE_SEQ,"Inputbuffer filledlen : %i inputbuffer pointer : %x", (int)inputbuffer->nFilledLen, (int)(inputbuffer->pBuffer+inputbuffer->nOffset));
		}
	
		input_buffer_still_processed = 1;
	
		decode_result = mad_frame_decode(omx_maddec_component_Private->frame, omx_maddec_component_Private->stream);
		if(decode_result == -1) {
			if (!MAD_RECOVERABLE(omx_maddec_component_Private->stream->error))	{
				input_buffer_still_processed = 0;
			
				if(omx_maddec_component_Private->stream->error == MAD_ERROR_BUFLEN)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "stream error bufflen normal input buffer ");
				}	else if(omx_maddec_component_Private->stream->error == MAD_ERROR_BUFPTR)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "stream error buffer pointer normal input buffer ");
				}	else if(omx_maddec_component_Private->stream->error == MAD_ERROR_NOMEM)	{
					DEBUG(DEB_LEV_SIMPLE_SEQ, "stream error no memory normal input buffer ");
				}	else	{
					DEBUG(DEB_LEV_ERR, "unknown non recoverable error normal input buffer ");
				}
			}	else if(omx_maddec_component_Private->stream->error == MAD_ERROR_LOSTSYNC)	{
				tagsize = id3_tag_query(omx_maddec_component_Private->stream->this_frame, omx_maddec_component_Private->stream->bufend - omx_maddec_component_Private->stream->this_frame);
				mad_stream_skip(omx_maddec_component_Private->stream, tagsize);
				DEBUG(DEB_LEV_SIMPLE_SEQ, "decoding error LOSTSYNC - normal buffer case - at frame %u - skipping %u bytes", (int)omx_maddec_component_Private->stream->this_frame, (int)tagsize);
			}	else	{
				generate_output(omx_maddec_component_Private, outputbuffer);
			}
		}	else 	{
			generate_output(omx_maddec_component_Private, outputbuffer);
		}

		total_consumed_length_after = (omx_maddec_component_Private->stream->next_frame - omx_maddec_component_Private->stream->buffer);
		consumed_length = total_consumed_length_after - total_consumed_length_before;
		total_consumed_length_before = total_consumed_length_after;
		DEBUG(DEB_LEV_SIMPLE_SEQ, "consumed length : %d ", consumed_length);	
		inputbuffer->nFilledLen -= consumed_length ;
		inputbuffer->nOffset += consumed_length ;
		
		if(inputbuffer->nFilledLen <= (consumed_length + 100) ){ 
			input_buffer_still_processed = 0;
		}
		/** here almost whole input buffer is processed....only remaining few bytes are left. */
		if(!input_buffer_still_processed) {
			omx_maddec_component_Private->isNewBuffer = 1;
			/** copy the remaining portion of input buffer to the temporary_buffer allocated  */
			// TODO fix this check. It is only a patch for some SEG FAULT
			if (((int)inputbuffer->nFilledLen) > 0) {
				memcpy(temporary_buffer->pBuffer, inputbuffer->pBuffer+inputbuffer->nOffset, inputbuffer->nFilledLen);
				temporary_buffer->nFilledLen = inputbuffer->nFilledLen;
			}
			DEBUG(DEB_LEV_SIMPLE_SEQ,"temp buffer filled len : %d ", (int)temporary_buffer->nFilledLen);
			temporary_buffer->nOffset = 0;
			inputbuffer->nFilledLen = 0;
			inputbuffer->nOffset = 0;
		}
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "--> --> One output buffer %x len=%d is full returning", (int)outputbuffer->pBuffer, (int)outputbuffer->nFilledLen);
}

/** this function sets the configuration structure */
OMX_ERRORTYPE omx_maddec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) {
	
	switch (nIndex) {
		default: // delegate to superclass
			return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}

/** this function gets the configuartion structure */
OMX_ERRORTYPE omx_maddec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)	{
	
	switch (nIndex) {
		default: // delegate to superclass
			return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}


/** this function sets the parameter values regarding audio format & index */
OMX_ERRORTYPE omx_maddec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)	{

	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and make control its header */
	OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	omx_maddec_component_PortType *port;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			/*Check Structure Header*/
			err = checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
			CHECK_ERROR(err, "Check Header");
			memcpy(&omx_maddec_component_Private->sPortTypesParam, ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
		break;	
			
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			/*Check Structure Header and verify component state*/
			err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			CHECK_ERROR(err, "Parameter Check");
			if (portIndex <= 1) {
				port = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[portIndex];
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
			CHECK_ERROR(err, "Parameter Check");
			memcpy(&omx_maddec_component_Private->pAudioPcmMode, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
		break;
		
		case OMX_IndexParamStandardComponentRole:
			pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
			if (!strcmp( (char*) pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) {
				omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
			}	else {
				return OMX_ErrorBadParameter;
			}
			SetInternalParameters(openmaxStandComp);
		break;
            
    case OMX_IndexParamAudioMp3:
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
      portIndex = pAudioMp3->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			CHECK_ERROR(err,"Parameter Check");
			if (pAudioMp3->nPortIndex == 1) {
         memcpy(&omx_maddec_component_Private->pAudioMp3, pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE)); 
			}	else {
			  return OMX_ErrorBadPortIndex;
			}
		break;

		default: /*Call the base component function*/
			return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
	
}

/** this function gets the parameters regarding audio formats and index */
OMX_ERRORTYPE omx_maddec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)	{
	
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
	OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
	omx_maddec_component_PortType *port;
	
	OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = openmaxStandComp->pComponentPrivate;
	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
		case OMX_IndexParamAudioInit:
			setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
			memcpy(ComponentParameterStructure, &omx_maddec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
		break;		
		
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (pAudioPortFormat->nPortIndex <= 1) {
				port = (omx_maddec_component_PortType *)omx_maddec_component_Private->ports[pAudioPortFormat->nPortIndex];
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
			memcpy(pAudioPcmMode, &omx_maddec_component_Private->pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
		break;
		
		case OMX_IndexParamAudioMp3:
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			if (pAudioMp3->nPortIndex != 1) {
				return OMX_ErrorBadPortIndex;
			}
			setHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			memcpy(pAudioMp3, &omx_maddec_component_Private->pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
		break;
		
		case OMX_IndexParamStandardComponentRole:
			pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
			setHeader(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
			if (omx_maddec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) {
				strcpy( (char*) pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
			}	else {
				strcpy( (char*) pComponentRole->cRole,"\0");;
			}
		break;
		default: /*Call the base component function*/
			return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
	
}

/** message handling of mad decoder */
OMX_ERRORTYPE omx_mad_decoder_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp, internalRequestMessageType *message)	{

	omx_maddec_component_PrivateType* omx_maddec_component_Private = (omx_maddec_component_PrivateType*)openmaxStandComp->pComponentPrivate;	
	OMX_ERRORTYPE err;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if (message->messageType == OMX_CommandStateSet){
		if ((message->messageParam == OMX_StateIdle) && (omx_maddec_component_Private->state == OMX_StateLoaded)) {
			if (!omx_maddec_component_Private->maddecReady) {
				err = omx_maddec_component_madLibInit(omx_maddec_component_Private);
				if (err != OMX_ErrorNone) {
					return OMX_ErrorNotReady;
				}
				omx_maddec_component_Private->maddecReady = OMX_TRUE;
			}
			err = omx_maddec_component_Init(openmaxStandComp);
      CHECK_ERROR(err, "MAD Decoder Init Failed");
		}	else if ((message->messageParam == OMX_StateLoaded) && (omx_maddec_component_Private->state == OMX_StateIdle)) {
    	err = omx_maddec_component_Deinit(openmaxStandComp);
    	CHECK_ERROR(err, "MAD Decoder Deinit Failed");
  	}
	}
  /** Execute the base message handling */
  return omx_base_component_MessageHandler(openmaxStandComp, message);
	
}

/** The following utility routine performs simple rounding, clipping, and
 	* scaling of MAD's high-resolution samples down to 16 bits. It does not
 	* perform any dithering or noise shaping, which would be recommended to
 	* obtain any exceptional audio quality. It is therefore not recommended to
 	* use this routine if high-quality output is desired.
 	*/

static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}


  
