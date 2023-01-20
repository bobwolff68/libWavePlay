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
#ifdef ESP_PLATFORM

#include "WaveFileLittleFSReader.h"

WaveFileLittleFSReader::WaveFileLittleFSReader(const char* fname) : WaveFileBufferReader(fname)
{
    bIsOpen = false;

      if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
        throw "LittleFS did not initialize.";
        return;
      }

    if (!open(fname))
        throw "WaveFileLittleFSReader::File did not open.";

    bIsOpen = true;

    readAndProcessWavHeader();
}

WaveFileLittleFSReader::~WaveFileLittleFSReader()
{
    if (bIsOpen)
        close();
}

bool WaveFileLittleFSReader::open(const char* fname)
{
    f = LittleFS.open(fname, FILE_READ);

    if (!f) {
        Serial.println("There was an error opening the LittleFS file");
        return false;
    }

    totalWavBytesReadSoFar=0;
    return true;
}

bool WaveFileLittleFSReader::read(uint8_t* pDest, size_t numBytes)
{
    int bytesRead;

    if (numBytes==0)
        return true;

//    Serial.printf("Inside read() - available():%lu\n", f.available());

    bytesRead =  f.read(pDest, numBytes);
    totalWavBytesReadSoFar += bytesRead;

//    Serial.printf("Inside read() - req:%d, actual:%d\n", numBytes, bytesRead);

    if (bytesRead != numBytes) {
        if(!f.available()) {
//            Serial.printf("Inside read(), !available() detected, bytesread=%d\n", bytesRead);
            throw FileException("EOF reached.", bytesRead, true);
        }
    }

    // With 'throw' above, this will ALWAYS be 'true'    
    return (bytesRead == numBytes);
}

bool WaveFileLittleFSReader::seekRel(long offset)
{
    totalWavBytesReadSoFar += offset;   // May be positive or negative but it is relative to here.
    return f.seek(offset, fs::SeekMode::SeekCur);
}

void WaveFileLittleFSReader::close(void)
{
    f.close();
}

#endif
