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
#include <stdint.h>
#endif

#include "FileException.h"
#include "robotask.h"

/*! @class WaveFileBufferReader
 *  @brief Workerbee class for handling a single audio file - parsing and buffer management.
 *  @details This is the base class for the other WaveFile<filesystem>Reader classes. Features:
 *           - Thread-based WAVE file processing to manage buffer fullness without external controls.
 *           - Reduce or eliminate popping sound at the speaker by use of the rampTiming which uses
 *             the 8-bit DAC value of 0x7f as the 'zero point'
 *           - Buffer filling attempts to be smart about how much it fills at any given call for
 *             bufferFill() and splits its filling/reading operations into at most 2 operations
 *             which might straddle the end of the buffeer (wrap-around case)
 *           - Buffer size allocation is based upon byteRate, number of channels, resolution and rate.
 *           - Fairly complete WAV header processing ability in order to support as wide a variety as
 *             possible of PCM-based WAV / RIF files.
 */
class WaveFileBufferReader : RoboTask
{
public:
    /*! @brief Constructor requires the filename to use
     *  @param fname - Filename to open/process/buffer
     *  @param rampTiming - optional - time in milliseconds to do ramp-in and ramp-out of the DAC/speaker.
     *                    This is in order to minimize or eliminate 'popping' at playout and finish time.
     */
    WaveFileBufferReader(const char* fname, uint16_t rampTiming=500);
    ~WaveFileBufferReader();
    //! @brief Thread-based processing automates the reading / re-filling operation
    void Run();
    //! @brief Utility method for knowing what kind of file is being processed.
    void printFileInfo();
    //! @brief Indicates when the actual file playout is complete. This is known by the fact
    //!        that the read pointer has reached the write pointer after file reading is complete.
    bool isPlaybackComplete();
    //! @brief Indicates when the reading of the WAVE file is fully complete and in memory.
    bool isFileReadComplete() { return bIsDoneReadingFile; };
    //! @brief Based upon the chosen and allocated memory buffer size, gives 0-100 result of fullness.
    uint8_t getBufferFullPercentage();
    //! @brief Returns how far into the file we are as a percentage 0-100 at any given moment.
    uint8_t getFileReadPercentage();
    //! @brief Returns the parsed sample rate in bits per second
    uint32_t getSampleRate() { return sampleRate; };
    //! @brief The last byte of the file is useful for the ramp-out calculation. Internal, primarily.
    uint8_t  getLastByteOfFile() { return lastByteValueOfFile; };
    //! @brief Return the address of the read pointer at any moment in time.
    uint8_t* getReadPointer();
    //! @brief Bump the read pointer by one address and wrap around if necessary
    void advanceReadPointer();
    const uint8_t WAV_HEADER = 46;  // Maximum header size
    const uint8_t WAV_HEADER_TO_CHUNKLEN = 20;  // Just enough to know how much left to read.
    const uint16_t rampTime;

protected:
    //! @brief Abstract function for opening the file on a variety of filesystems
    virtual bool open(const char* fname) = 0;
    //! @brief Abstract function for reading bytes and handling EOF and read errors
    virtual bool read(uint8_t* pDest, size_t numBytes) = 0;
    //! @brief Abstract function for closing the file
    virtual void close(void) = 0;
    //! @brief Abstract function for seeking, relatively, forward or backward from current location
    virtual bool seekRel(long offset) = 0;
    void readAndProcessWavHeader(void);
    void readFirstByteAndPrepRampIn();
    void prepRampOut();
    void bufferAlloc();
    void bufferFill();

    std::string fileName;

    // Buffer-specific items
    bool bIsRampOutComplete;
    uint8_t* pWavBuffer;
    uint8_t* pBufferRead;
    uint8_t* pBufferWrite;
    uint32_t lengthWavBuffer;
    uint8_t lastByteValueOfFile;
    bool bIsFirstFill;
    bool bIsBufferReady;
    bool bIsDoneReadingFile;
    uint32_t indexNotSureWhatYet;
    unsigned char* pHeader;
    uint32_t fillSleepTime;
    static const uint8_t MAX_SIZES_COUNT = 6;
    uint32_t bufferByteRates[MAX_SIZES_COUNT];
    uint32_t bufferSizes    [MAX_SIZES_COUNT];
    uint32_t totalWavBytesReadSoFar;
    int percentComplete;

    // Buffer stat items


    // Wave-specific items
    uint8_t numChannels;
    uint8_t bitsPerSample;
    unsigned long sampleRate;
    unsigned long byteRate;
    unsigned long totalWaveBytes;

};
