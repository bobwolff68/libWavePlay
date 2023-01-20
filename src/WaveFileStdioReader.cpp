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
#ifndef ESP_PLATFORM
#include "WaveFileStdioReader.h"

WaveFileStdioReader::WaveFileStdioReader(const char* fname) : WaveFileBufferReader(fname)
{
    totalWavBytesReadSoFar=0;
    pFile = nullptr;

    if (!open(fname))
        throw "WaveFileStdioReader::File did not open.";

    readAndProcessWavHeader();
}

WaveFileStdioReader::~WaveFileStdioReader()
{
    close();
}

bool WaveFileStdioReader::open(const char* fname)
{
    totalWavBytesReadSoFar=0;
    pFile = fopen(fname, "rb");
    return (pFile != nullptr);
}

bool WaveFileStdioReader::read(uint8_t* pDest, size_t numBytes)
{
    int bytesRead;

    if (numBytes==0)
        return true;

    bytesRead = fread(pDest, 1, numBytes, pFile);
    totalWavBytesReadSoFar += bytesRead;

    if (bytesRead != numBytes) {
        if(feof(pFile)) {
            throw FileException("EOF reached.", bytesRead, true);
        }
        else if (ferror(pFile)) {
            printf("Inside read(), FERROR detected, bytesread=%d\n", bytesRead);
            throw FileException("ERROR found.", bytesRead, false);
        }
        else {
            printf("Inside read(), UNKNOWN error, bytesread=%d\n", bytesRead);
            perror("Inside read(), system error says:");
            throw FileException("ERROR found.", bytesRead, false);
        }
    }

    // With 'throw' above, this will ALWAYS be 'true'    
    return (bytesRead == numBytes);
}

bool WaveFileStdioReader::seekRel(long offset) {
    totalWavBytesReadSoFar += offset; // May be plus or minus but is always relative.
    
    return (fseek(pFile, offset, SEEK_CUR)==0);
}

void WaveFileStdioReader::close(void)
{
    if (pFile) {
        fclose(pFile);
        pFile = nullptr;
    }
}
#endif
