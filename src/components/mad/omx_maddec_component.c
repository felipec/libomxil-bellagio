/**
 * @file src/components/mad/omx_maddec_component.c
 *
 * This component implements an mp3 decoder. The Mp3 decoder is based on mad
 * software library.
 *
 * Copyright (C) 2006  Nokia and STMicroelectronics
 * @author Pankaj SEN,Ukri NIEMIMUUKKO, Diego MELPIGNANO, , David SIORPAES, Giulio URLINI , SOURYA BHATTACHARYYA
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
 * 2006/10/17:  mad mp3 decoder component version 0.2
 *
 */
 
 
#include <omxcore.h>
#include <omx_maddec_component.h>


/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
  unsigned char const *start;
  unsigned long length;
};


/** this function registers template of the decoder component in the template list which is used to store 
  * the registered components */
void __attribute__ ((constructor)) omx_maddec_component_register_template() 
{
	stComponentType *component;
	int i;
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Registering component's template in %s...\n", __func__);	
	  
	component = base_component_CreateComponentStruct();
	/** here you can override the base component defaults */
	
	//modified component name according to standard component definition
	strcpy(component->name, AUDIO_DEC_BASE_NAME);	 

	//setting the no of specific components
	component->name_specific_length = 1;
       /** allocating memory and assigning the names of specific components */

	component->name_specific = (char **)malloc(component->name_specific_length);	
	component->role_specific = (char **)malloc(component->name_specific_length);	

	for(i=0;i<component->name_specific_length;i++)
		component->name_specific[i] = (char* )malloc(128);
	for(i=0;i<component->name_specific_length;i++)
		component->role_specific[i] = (char* )malloc(128);

	//modified role or specific names of standard component according to standard definition
	strcpy(component->name_specific[0],AUDIO_DEC_MP3_NAME);
	strcpy(component->role_specific[0],AUDIO_DEC_MP3_ROLE);
	
	component->constructor = omx_maddec_component_Constructor;
	component->destructor = omx_maddec_component_Destructor;
	component->omx_component.SetConfig = omx_maddec_component_SetConfig;
	component->omx_component.GetConfig = omx_maddec_component_GetConfig;
	component->omx_component.SetParameter = omx_maddec_component_SetParameter;
	component->omx_component.GetParameter = omx_maddec_component_GetParameter;
	component->omx_component.ComponentRoleEnum = omx_maddec_component_ComponentRoleEnum;

	  
	// port count must be set for the base class constructor (if we call it, and we will)
	component->nports = 2;
	
 	register_template(component);
 	
}//end of register template






/** this function initializates the mad framework, and opens an mad decoder of type specified by IL client */ 
OMX_ERRORTYPE omx_maddec_component_madLibInit(omx_maddec_component_PrivateType* omx_maddec_component_Private) 
{
	struct buffer buffer;
	//problem is to synchronize between decoder_init parameter & standard file command line input in test application 
	/* initialize our private message structure */
	//buffer.start  = start;
	//buffer.length = length;
	/* configure input, output, and error functions  which are implemented in private func area*/
    //mad_decoder_init(omx_maddec_component_Private->decoder, &buffer, input, 0 /* header */, 0 /* filter */, output, error, 0 /* message */);
	
}//end of lib init





/** this function Deinitializates the mad framework, and close the mad decoder */
void omx_maddec_component_madLibDeInit(omx_maddec_component_PrivateType* omx_maddec_component_Private) 
{
	
}//end of lib de init







/** The Constructor */
OMX_ERRORTYPE omx_maddec_component_Constructor(stComponentType* stComponent) 
{
	OMX_ERRORTYPE err = OMX_ErrorNone;	
	omx_maddec_component_PrivateType* omx_maddec_component_Private;
	omx_maddec_component_PortType *inPort,*outPort;
	OMX_S32 i;
	
	if (!stComponent->omx_component.pComponentPrivate) 
	{
		stComponent->omx_component.pComponentPrivate = calloc(1, sizeof(omx_maddec_component_PrivateType));
		if(stComponent->omx_component.pComponentPrivate==NULL)
			return OMX_ErrorInsufficientResources;
	}//end if
	
	omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	stComponent->messageHandler = omx_mad_decoder_MessageHandler;
	
	//port allocation
	// fixme maybe the base class could use a "port factory" function pointer?	
	if (stComponent->nports && !omx_maddec_component_Private->ports) 
	{
		omx_maddec_component_Private->ports = calloc(stComponent->nports,sizeof (base_component_PortType *));

		if (!omx_maddec_component_Private->ports) 
			return OMX_ErrorInsufficientResources;
		
		for (i=0; i < stComponent->nports; i++) 
		{
			// this is the important thing separating this from the base class; size of the struct is for derived class port type
			// this could be refactored as a smarter factory function instead?
			omx_maddec_component_Private->ports[i] = calloc(1, sizeof(omx_maddec_component_PortType));
			if (!omx_maddec_component_Private->ports[i]) 
				return OMX_ErrorInsufficientResources;
		}//end for
	}//end if
	
	// we could create our own port structures here
	// fixme maybe the base class could use a "port factory" function pointer?	
	err = base_filter_component_Constructor(stComponent);

	/* here we can override whatever defaults the base_component constructor set
	 * e.g. we can override the function pointers in the private struct  */
	omx_maddec_component_Private = (omx_maddec_component_PrivateType *)stComponent->omx_component.pComponentPrivate;

	//debug statement
	DEBUG(DEB_LEV_SIMPLE_SEQ,"constructor of mad decoder component is called\n");
	// oh well, for the time being, set the port params, now that the ports exist	
	/** Domain specific section for the ports. */	

	//parameters related to input port which does not depend upon input audio format
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_IN_BUFFER_SIZE;

	//parameters related to output port 
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.eDomain = OMX_PortDomainAudio;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "raw";
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.pNativeRender = 0;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;

	/** now it's time to know the audio coding type of the component */
	if(!strcmp(stComponent->name_requested, AUDIO_DEC_MP3_NAME))   // mp3 format decoder
		omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;

	else if (!strcmp(stComponent->name_requested, AUDIO_DEC_BASE_NAME))// general audio decoder
		omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingUnused;

	else  // IL client specified an invalid component name
		return OMX_ErrorInvalidComponentName;
		
	/** initialise the semaphore to be used for mad decoder access synchronization */
	if(!omx_maddec_component_Private->madDecSyncSem) 
	{
		omx_maddec_component_Private->madDecSyncSem = malloc(sizeof(tsem_t));
		if(omx_maddec_component_Private->madDecSyncSem == NULL) return OMX_ErrorInsufficientResources;
		tsem_init(omx_maddec_component_Private->madDecSyncSem, 0);
	}//end if
	
	/** setting of internal port parameters related to audio format */
	SetInternalParameters(stComponent);

	outPort = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
	setHeader(&outPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	outPort->sAudioParam.nPortIndex = 1;
	outPort->sAudioParam.nIndex = 0;
	outPort->sAudioParam.eEncoding = OMX_AUDIO_CodingPCM;
	
	/** settings of output port audio format - pcm */
	setHeader(&omx_maddec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
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
	 *  setting values of other fields of omx_maddec_component_Private structure	*/ 
	omx_maddec_component_Private->decoder = NULL;
	//omx_maddec_component_Private->stream = NULL;
	//omx_maddec_component_Private->frame = NULL;  // within it header also null
	//omx_maddec_component_Private->synth = NULL;  // within it pcm is also null
	omx_maddec_component_Private->maddecReady = OMX_FALSE;
	omx_maddec_component_Private->BufferMgmtCallback = omx_maddec_component_BufferMgmtCallback;
	omx_maddec_component_Private->Init = omx_maddec_component_Init;
	omx_maddec_component_Private->Deinit = omx_maddec_component_Deinit;
	omx_maddec_component_Private->DomainCheck = &omx_maddec_component_DomainCheck;

	return err;
	
}//end of constructor





/** this function sets some inetrnal parameters to the input port depending upon the input audio format */
SetInternalParameters(stComponentType* stComponent) 
{
	omx_maddec_component_PrivateType* omx_maddec_component_Private;
	omx_maddec_component_PortType *inPort,*outPort;
	
	omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	
	/** setting port & private fields according to mp3 audio format values */
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.cMIMEType = "audio/mpeg";
	omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

	setHeader(&omx_maddec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));    
	omx_maddec_component_Private->pAudioMp3.nPortIndex = 0;                                                                    
	omx_maddec_component_Private->pAudioMp3.nChannels = 2;                                                                    
	omx_maddec_component_Private->pAudioMp3.nBitRate = 28000;                                                                  
	omx_maddec_component_Private->pAudioMp3.nSampleRate = 44100;                                                               
	omx_maddec_component_Private->pAudioMp3.nAudioBandWidth = 0;
	omx_maddec_component_Private->pAudioMp3.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    omx_maddec_component_Private->pAudioMp3.eFormat=OMX_AUDIO_MP3StreamFormatMP1Layer3;

	inPort = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

	setHeader(&inPort->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
	inPort->sAudioParam.nPortIndex = 0;
	inPort->sAudioParam.nIndex = 0;
	inPort->sAudioParam.eEncoding = OMX_AUDIO_CodingMP3;

}//end of set internal parameters





/** The destructor */
OMX_ERRORTYPE omx_maddec_component_Destructor(stComponentType* stComponent) 
{
	int i;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	omx_maddec_component_PortType* port = (omx_maddec_component_PortType *)omx_maddec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	
	//frees the locally dynamic allocated memory
	for(i=0;i<stComponent->name_specific_length;i++) {
		free(stComponent->name_specific[i]); //free memory or holding specific names
		free(stComponent->role_specific[i]); //free memory or holding specific names
	}
	free(stComponent->name_specific); //freeing storage of char pointers for holding name
	free(stComponent->role_specific); //freeing storage of char pointers for holding name
	
	DEBUG(DEB_LEV_SIMPLE_SEQ, "Destructor of mad decoder component is called\n");
	
	return base_component_Destructor(stComponent);
}//end of destructor






/** The Initialization function  */
OMX_ERRORTYPE omx_maddec_component_Init(stComponentType* stComponent)
{
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_U32 nBufferSize;
	
	base_component_Init(stComponent);


	/*Temporary First Output buffer size*/
	omx_maddec_component_Private->inputCurrBuffer=NULL;
	omx_maddec_component_Private->inputCurrLength=0;
	nBufferSize=omx_maddec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.nBufferSize * 2;
	omx_maddec_component_Private->internalOutputBuffer = (OMX_U8 *)malloc(nBufferSize);
	memset(omx_maddec_component_Private->internalOutputBuffer, 0, nBufferSize);
	omx_maddec_component_Private->isFirstBuffer=1;
	omx_maddec_component_Private->positionInOutBuf = 0;
	omx_maddec_component_Private->isNewBuffer=1;

	return err;
}//end of init







/** The Deinitialization function  */
OMX_ERRORTYPE omx_maddec_component_Deinit(stComponentType* stComponent) 
{
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	if (omx_maddec_component_Private->maddecReady) {
		omx_maddec_component_madLibDeInit(omx_maddec_component_Private);
		omx_maddec_component_Private->maddecReady = OMX_FALSE;
	}

	free(omx_maddec_component_Private->internalOutputBuffer);
	base_component_Deinit(stComponent);

	return err;
	
}//end of deinit






/** Check Domain of the Tunneled Component*/
OMX_ERRORTYPE omx_maddec_component_DomainCheck(OMX_PARAM_PORTDEFINITIONTYPE pDef)
{
	if(pDef.eDomain!=OMX_PortDomainAudio)
		return OMX_ErrorPortsNotCompatible;
	else if(pDef.format.audio.eEncoding == OMX_AUDIO_CodingMax)
		return OMX_ErrorPortsNotCompatible;

	return OMX_ErrorNone;
}//end of domain check





/** This function is used to process the input buffer and provide one output buffer */
void omx_maddec_component_BufferMgmtCallback(stComponentType* stComponent, OMX_BUFFERHEADERTYPE* inputbuffer, OMX_BUFFERHEADERTYPE* outputbuffer) 
{
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_S32 outputfilled = 0;
	OMX_U8* outputCurrBuffer;
	OMX_U32 outputLength;
	OMX_U32 len = 0;
	OMX_S32 internalOutputFilled=0;
			
	/**Fill up the current input buffer when a new buffer has arrived*/
	if(omx_maddec_component_Private->isNewBuffer) 
	{
		omx_maddec_component_Private->inputCurrBuffer = inputbuffer->pBuffer;
		omx_maddec_component_Private->inputCurrLength = inputbuffer->nFilledLen;
		omx_maddec_component_Private->positionInOutBuf = 0;
		omx_maddec_component_Private->isNewBuffer=0;
	}//end if
	
	outputCurrBuffer = outputbuffer->pBuffer;
	outputLength = outputbuffer->nAllocLen;
	outputbuffer->nFilledLen = 0;
	
}//end of buffer mgmt callback





/** this function sets the configuration structure */
OMX_ERRORTYPE omx_maddec_component_SetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_IN  OMX_PTR pComponentConfigStructure) 
{
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
		
	switch (nIndex) 
	{
		default: // delegate to superclass
			return base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
	}//end switch
	return OMX_ErrorNone;
}//end of set config






/** this function gets the configuartion structure */
OMX_ERRORTYPE omx_maddec_component_GetConfig(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nIndex,
	OMX_INOUT OMX_PTR pComponentConfigStructure)
{
	switch (nIndex) 
	{
		default: // delegate to superclass
			return base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
	}//end switch
	return OMX_ErrorNone;
}//end of get config





/** this function sets the parameter values regarding audio format & index */
OMX_ERRORTYPE omx_maddec_component_SetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_IN  OMX_PTR ComponentParameterStructure)
{

	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;
	OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef ;
	OMX_AUDIO_PARAM_MP3TYPE * pAudioMp3;
	OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and make control its header */
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	omx_maddec_component_PortType *port;
	if (ComponentParameterStructure == NULL) 
	{
		return OMX_ErrorBadParameter;
	}//end if

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
	switch(nParamIndex) 
	{
		case OMX_IndexParamAudioInit:
			/*Check Structure Header*/
			err = checkHeader(ComponentParameterStructure , sizeof(OMX_PORT_PARAM_TYPE));
			if (err != OMX_ErrorNone)
				return err;
			memcpy(&omx_maddec_component_Private->sPortTypesParam,ComponentParameterStructure,sizeof(OMX_PORT_PARAM_TYPE));
		break;	
			
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			portIndex = pAudioPortFormat->nPortIndex;
			/*Check Structure Header and verify component state*/
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (err != OMX_ErrorNone)
					return err;
			if (portIndex <= 1) 
			{
				port = (omx_maddec_component_PortType *) omx_maddec_component_Private->ports[portIndex];
				memcpy(&port->sAudioParam,pAudioPortFormat,sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} 
			else 
			{
					return OMX_ErrorBadPortIndex;
			}
		break;
				
		case OMX_IndexParamAudioPcm:
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			portIndex = pAudioPcmMode->nPortIndex;
			/*Check Structure Header and verify component state*/
			err = base_component_ParameterSanityCheck(hComponent, portIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			if (err != OMX_ErrorNone)
				return err;
			memcpy(&omx_maddec_component_Private->pAudioPcmMode,pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));					
		break;
		case OMX_IndexParamStandardComponentRole:
			pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
			if (!strcmp(pComponentRole->cRole, AUDIO_DEC_MP3_ROLE)) 
			{
				omx_maddec_component_Private->audio_coding_type = OMX_AUDIO_CodingMP3;
			} 
			else 
			{
				return OMX_ErrorBadParameter;
			}
			SetInternalParameters(stComponent);
		break;
            
        case OMX_IndexParamAudioMp3:
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*) ComponentParameterStructure;
            portIndex = pAudioMp3->nPortIndex;
            err = base_component_ParameterSanityCheck(hComponent,portIndex,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
            if(err!=OMX_ErrorNone)
			    return err;
			if (pAudioMp3->nPortIndex == 1) 
			{
               memcpy(&omx_maddec_component_Private->pAudioMp3,pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			} 
			else 
			{
			     return OMX_ErrorBadPortIndex;
			}
		break;

		default: /*Call the base component function*/
			return base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}//end of switch
	return OMX_ErrorNone;
	
}//end of set parameter






/** this function gets the parameters regarding audio formats and index */
OMX_ERRORTYPE omx_maddec_component_GetParameter(
	OMX_IN  OMX_HANDLETYPE hComponent,
	OMX_IN  OMX_INDEXTYPE nParamIndex,
	OMX_INOUT OMX_PTR ComponentParameterStructure)
{
	
	OMX_PRIORITYMGMTTYPE* pPrioMgmt;
	OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat;	
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_AUDIO_PARAM_PCMMODETYPE *pAudioPcmMode;
	OMX_PORT_PARAM_TYPE* pPortDomains;
	OMX_U32 portIndex;
	OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
	OMX_AUDIO_PARAM_MP3TYPE *pAudioMp3;
	omx_maddec_component_PortType *port;
	
	stComponentType* stComponent = (stComponentType*)hComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	if (ComponentParameterStructure == NULL) 
	{
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) 
	{
		case OMX_IndexParamAudioInit:
			setHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
			memcpy(ComponentParameterStructure, &omx_maddec_component_Private->sPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
		break;		
		
		case OMX_IndexParamAudioPortFormat:
			pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
			setHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			if (pAudioPortFormat->nPortIndex <= 1) 
			{
				port = (omx_maddec_component_PortType *)omx_maddec_component_Private->ports[pAudioPortFormat->nPortIndex];
				memcpy(pAudioPortFormat, &port->sAudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
			} 
			else 
			{
					return OMX_ErrorBadPortIndex;
			}
		break;		
		
		case OMX_IndexParamAudioPcm:
			pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)ComponentParameterStructure;
			setHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			if (pAudioPcmMode->nPortIndex > 1) 
			{
				return OMX_ErrorBadPortIndex;
			}
			memcpy(pAudioPcmMode,&omx_maddec_component_Private->pAudioPcmMode,sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
		break;
		
		case OMX_IndexParamAudioMp3:
			pAudioMp3 = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParameterStructure;
			if (pAudioMp3->nPortIndex != 1) 
			{
				return OMX_ErrorBadPortIndex;
			}
			setHeader(pAudioMp3, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
			memcpy(pAudioMp3,&omx_maddec_component_Private->pAudioMp3,sizeof(OMX_AUDIO_PARAM_MP3TYPE));
		break;
		
		case OMX_IndexParamStandardComponentRole:
			pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
			setHeader(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));

			if (omx_maddec_component_Private->audio_coding_type == OMX_AUDIO_CodingMP3) 
			{
				strcpy(pComponentRole->cRole, AUDIO_DEC_MP3_ROLE);
			}
			else 
			{
				strcpy(pComponentRole->cRole,"\0");;
			}
		break;
		default: /*Call the base component function*/
			return base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return OMX_ErrorNone;
	
}//end of get parameter







/** message handling of audio decoder */
OMX_ERRORTYPE omx_mad_decoder_MessageHandler(coreMessage_t* message)
{
	stComponentType* stComponent = message->stComponent;
	omx_maddec_component_PrivateType* omx_maddec_component_Private = stComponent->omx_component.pComponentPrivate;
	OMX_ERRORTYPE err;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	if(message->messageType == SENDCOMMAND_MSG_TYPE){
		if (message->messageParam1 == OMX_CommandStateSet){
			if ((message->messageParam2 == OMX_StateIdle) && (stComponent->state == OMX_StateLoaded)) {
				if (!omx_maddec_component_Private->maddecReady) {
					err = omx_maddec_component_madLibInit(omx_maddec_component_Private);
					if (err != OMX_ErrorNone) {
						return OMX_ErrorNotReady;
					}
					omx_maddec_component_Private->maddecReady = OMX_TRUE;
				}
			}
		}
	}
	// Execute the base message handling
	return base_component_MessageHandler(message);
	
}//end of message handler



/** componnt role enumeration */
OMX_ERRORTYPE omx_maddec_component_ComponentRoleEnum(
	OMX_IN OMX_HANDLETYPE hComponent,
	OMX_OUT OMX_STRING cRole,
	OMX_IN OMX_U32 nNameLength,
	OMX_IN OMX_U32 nIndex) {
	if (nIndex == 0) 
	{
		strcpy(cRole, AUDIO_DEC_MP3_ROLE);
	}
	else 
	{
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}

//------------------------------------------------------------------------------------------------------------------
/** some private function field related to mad decoder --- init, run and finish supporting functions */
//------------------------------------------------------------------------------------------------------------------

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *data,
		    struct mad_stream *stream)
{
  struct buffer *buffer = data;

  if (!buffer->length)
    return MAD_FLOW_STOP;

  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
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

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

static
enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  /* pcm->samplerate contains the sampling frequency */

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  while (nsamples--) {
    signed int sample;

    /* output sample(s) in 16-bit signed little-endian PCM */

    sample = scale(*left_ch++);
    putchar((sample >> 0) & 0xff);
    putchar((sample >> 8) & 0xff);

    if (nchannels == 2) {
      sample = scale(*right_ch++);
      putchar((sample >> 0) & 0xff);
      putchar((sample >> 8) & 0xff);
    }
  }

  return MAD_FLOW_CONTINUE;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static
enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{
  struct buffer *buffer = data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
}


  
