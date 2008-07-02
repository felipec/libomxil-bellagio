/**
  @file src/components/components_symbian/outputstream/src/omx_symbian_output_stream.h
 
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

#ifndef __OMX_SYMBIAN_OUTPUT_STREAM_H__
#define __OMX_SYMBIAN_OUTPUT_STREAM_H__

#include <MdaAudioOutputStream.h>
#include <Mda\Common\Audio.h>

class OmxSymbianOutputStream : public CActiveScheduler, 
                                public MMdaAudioOutputStreamCallback                      
{
public:
    OmxSymbianOutputStream();
    virtual ~OmxSymbianOutputStream();

    int Open(int sampleRate, int channels);
    void Close();

    int WriteAudioData(unsigned char* buffer, int length);

    // from MMdaAudioOutputStreamCallback
    void MaoscOpenComplete(TInt aError);
    void MaoscBufferCopied(TInt aError, const TDesC8 &aBuffer);
    void MaoscPlayComplete(TInt aError);

private:
    CTrapCleanup            *iCleanup;
    CMdaAudioOutputStream   *iOutputStream;
    TPtr8                   iPlayBuffer;
    TBool                   iOpenComplete;
    TBool                   iPlayComplete;
};

#endif // __OMX_SYMBIAN_OUTPUT_STREAM_H__
