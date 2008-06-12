/**
  @file src/components/symbianoutputstream/omx_symbian_output_stream_wrapper.cpp
    
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

#include "omx_symbian_output_stream_wrapper.h"

#include "omx_symbian_output_stream.h"

#include "omx_comp_debug_levels.h"

extern "C" int create_output_stream(void **output)
{
    OmxSymbianOutputStream* temp = new OmxSymbianOutputStream();

    DEBUG(DEB_LEV_PARAMS, "output stream wrapper create pointer is %p\n", temp);
    
    if (temp == 0)
    {
        return -1;
    }

    *output = temp;

    return 0;
}

extern "C" int open_output_stream(void *output, int sampleRate, int channels)
{
    DEBUG(DEB_LEV_PARAMS, "output stream wrapper open pointer is %p\n", output);
    return ((OmxSymbianOutputStream*)output)->Open(sampleRate, channels);
}

extern "C" int close_output_stream(void *output)
{
    ((OmxSymbianOutputStream*)output)->Close();

    return 0;
}

extern "C" int write_audio_data(void *output, unsigned char *buffer, int length)
{
    return ((OmxSymbianOutputStream*)output)->WriteAudioData(buffer, length);
}
