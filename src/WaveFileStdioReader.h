// Copyright 2022 Robert M. Wolff (bob dot wolff 68 at gmail dot com)
//
// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation and/or 
// other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors 
// may be used to endorse or promote products derived from this software without 
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#pragma once
#ifndef ESP_PLATFORM

#include "WaveFileBufferReader.h"
#include "FileException.h"

/*! @class WaveFileStdioReader
 *  @brief Support for reading using stdio (for use in native builds)
 *  @details The base class WaveFileBufferReader has 4 pure virtual functions which
 *           need to be implemented by these support classes. This allows for the main
 *           classes to not be concerned with the implementation specifics of each type
 *           of filesystem. The filesystem specifics are /hidden/ under these classes.
 */
class WaveFileStdioReader : public WaveFileBufferReader
{
public:
    //! @brief Instantiate with filename to be opened
    WaveFileStdioReader(const char* fname);
    ~WaveFileStdioReader();

protected:
    //! @brief open the fname consistent with the filesystem herein. Called by constructor.
    bool open(const char* fname);
    //! @brief read numBytes from the open file and place those bytes in pDest
    //! @return true if all is well.
    //! @note This function will throw FileException on EOF or read errors.
    bool read(uint8_t* pDest, size_t numBytes);
    //! @brief seek in a relative manner (+ or -) given the offset.
    bool seekRel(long offset);
    //! @brief close the file
    void close(void);

    FILE* pFile;
};
#endif
