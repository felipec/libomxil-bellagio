/**
  @file src/components/symbianoutputstream/omx_symbian_output_stream.cpp
    
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
*/

#include "omx_symbian_output_stream.h"

#include "omx_comp_debug_levels.h"

OmxSymbianOutputStream::OmxSymbianOutputStream() : 
iCleanup(NULL),
iOutputStream(NULL),
iPlayBuffer(NULL, 0),
iOpenComplete(EFalse),
iPlayComplete(EFalse)
{
}

OmxSymbianOutputStream::~OmxSymbianOutputStream()
{
    Close();
}

int OmxSymbianOutputStream::Open(int sampleRate, int channels)
{
    DEBUG(DEB_LEV_PARAMS, "output stream open beginning\n");

    if (iCleanup != NULL)
    {
        // DEBUG(DEB_LEV_FULL_SEQ, "output stream open cleanup stack exists allready\n");
	    return 0;
    }

    DEBUG(DEB_LEV_PARAMS, "output stream open before cleanup stack creation\n");

    iCleanup = CTrapCleanup::New();
    if (iCleanup == 0)
    {
        return -1;
    }

    DEBUG(DEB_LEV_PARAMS, "output stream open before scheduler install\n");

    // install active scheduler for this thread
    CActiveScheduler::Install(this);

    DEBUG(DEB_LEV_PARAMS, "output stream open after scheduler install\n");

    TRAPD(err, iOutputStream = CMdaAudioOutputStream::NewL(*this));

    DEBUG(DEB_LEV_PARAMS, "output stream open after stream creation\n");

    if (err != KErrNone)
    {
        return -1;
    }

    TInt sR;
    TInt cC;

    switch ((TInt)sampleRate)
    {
    case 8000:
        sR = TMdaAudioDataSettings::ESampleRate8000Hz;
        break;
    case 16000:
        sR = TMdaAudioDataSettings::ESampleRate16000Hz;
        break;
    case 44100:
        sR = TMdaAudioDataSettings::ESampleRate44100Hz;
        break;
    case 48000:
    default:
        sR = TMdaAudioDataSettings::ESampleRate48000Hz;
        break;
    }

    switch (channels)
    {
    case 1:
        cC = TMdaAudioDataSettings::EChannelsMono;
        break;
    case 2:
    default:
        cC = TMdaAudioDataSettings::EChannelsStereo;
        break;
    }

    TMdaAudioDataSettings settings;
    
    settings.iSampleRate = sR;
    settings.iChannels = cC;
    settings.iVolume = 0;

    DEBUG(DEB_LEV_FULL_SEQ, "output stream before stream open\n");
    
    iOutputStream->Open(&settings);

    DEBUG(DEB_LEV_FULL_SEQ, "output stream before scheduler start\n");
    // wait until open returns with status
    CActiveScheduler::Start();

    if (iOpenComplete == EFalse)
    {
        delete iOutputStream;
        iOutputStream = NULL;
        return -1;
    }

    iOutputStream->SetVolume(iOutputStream->MaxVolume());

    return 0;
}

int OmxSymbianOutputStream::WriteAudioData(unsigned char* buffer, int length)
{

    iPlayBuffer.Set((TUint8*)buffer, length, length);

    TRAPD(err, iOutputStream->WriteL(iPlayBuffer));

    if(err != KErrNone)
    {                            
        return -1;
    }

    // wait until write returns with status
    CActiveScheduler::Start();

    if (iPlayComplete == EFalse)
    {
        return -1;
    }

    return 0;
}

void OmxSymbianOutputStream::Close()
{
/*
    iOutputStream->Stop();
    
    // wait until stop returns with status
    CActiveScheduler::Start();

    if (iOutputStream)
    {
        delete iOutputStream;
        iOutputStream = NULL;
    }

    if (iCleanup)
    {
        delete iCleanup;
        iCleanup = NULL;
    }
    */
}

void OmxSymbianOutputStream::MaoscOpenComplete(TInt aError)
{
    iOpenComplete = EFalse;

    if (aError == KErrNone)
    {     
        /*iOutputStream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate44100Hz, TMdaAudioDataSettings::EChannelsStereo);*/
        iOpenComplete = ETrue;
    }

    DEBUG(DEB_LEV_FULL_SEQ, "output stream open complete callback\n");

    CActiveScheduler::Stop();
}
   
void OmxSymbianOutputStream::MaoscBufferCopied(TInt aError, const TDesC8 & /*aBuffer*/)
{
    iPlayComplete = EFalse;

    if (aError == KErrNone)
    {     
        iPlayComplete = ETrue;
    }

    CActiveScheduler::Stop();
}
    
void OmxSymbianOutputStream::MaoscPlayComplete(TInt /*aError*/)
{
    CActiveScheduler::Stop();
}
