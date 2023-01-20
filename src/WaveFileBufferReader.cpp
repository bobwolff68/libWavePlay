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
#include "WaveFileBufferReader.h"
#include <iostream>

#ifdef ESP_PLATFORM
#define SleepMS(x)  (vTaskDelay( (x) / portTICK_PERIOD_MS ))
#define PrintLN(x)  (Serial.println((x)))
#else
#define SleepMS(x)  (std::this_thread::sleep_for(std::chrono::milliseconds((x))))
#define PrintLN(x)  { printf("%s\n",(x)); }
#endif

// Utility
void printHex(uint8_t* ptr, u_int16_t len) {
    assert(ptr);
    assert(len);
    char ascii[8];

#ifdef ESP_PLATFORM
    Serial.print("HEXDUMP: ");
#else
    printf("HEXDUMP: ");
#endif
    for(int i=0; i<len; i++) {
#ifdef ESP_PLATFORM
        Serial.printf("0x%02x ", ptr[i]);
#else
        printf("0x%02x ", ptr[i]);
#endif
        if (i % 16 == 15)
#ifdef ESP_PLATFORM
            Serial.print("\n         ");
#else
            printf("\n         ");
#endif
    }

#ifdef ESP_PLATFORM
    Serial.println();
#else
    printf("\n");
#endif
}

WaveFileBufferReader::WaveFileBufferReader(const char* fname, uint16_t rampTiming) : rampTime(rampTiming)
{
    fileName="";
    totalWavBytesReadSoFar=0;
    percentComplete=0;
    lengthWavBuffer = 0;
    lastByteValueOfFile = 0x7f;
    pWavBuffer = nullptr;
    bIsRampOutComplete = false;
    pBufferRead = pBufferWrite = nullptr;
    bIsFirstFill=true;
    bIsBufferReady=false;
    bIsDoneReadingFile=false;
    pHeader=nullptr;

    numChannels=0;
    bitsPerSample=0;
    sampleRate=0;
    byteRate=0;
    totalWaveBytes = 0;

    if (!fname)
        throw "WaveFileBufferReader::Filename must be non-null.";

    fileName = fname;

    pHeader = new unsigned char[WAV_HEADER];
    assert(pHeader);

    bufferByteRates[0] = 8000; // 8kHz 8-bit mono and below
    bufferByteRates[1] = 16000;
    bufferByteRates[2] = 32000;
    bufferByteRates[3] = 64000;
    bufferByteRates[4] = 128000;
    bufferByteRates[5] = 200000;
    // Calculate bands of memory allocation sizes
    // Starting with a buffer that is half the size of the per-second rate.
    // This would imply it runs out in 500ms so the refill would need to be 250ms
    // These calculations need to go hand in hand.
//    fillSleepTime = 250;
    fillSleepTime = 250;
    for (int i=0; i<MAX_SIZES_COUNT; i++)
        bufferSizes[i] = bufferByteRates[i] / 2;

// Shall we start the process here or wait until the buffer is alloc'd and wave file is known?
//    Start();
}

WaveFileBufferReader::~WaveFileBufferReader()
{
    if (pHeader) {
        delete pHeader;
        pHeader = nullptr;
    }

    if (pWavBuffer) {
        delete pWavBuffer;
        pWavBuffer = nullptr;
    }
}

void WaveFileBufferReader::readAndProcessWavHeader(void)
{
    unsigned char* ptr = pHeader;
    uint32_t tmp32;
    uint16_t tmp16;
    uint16_t blockAlign;
    uint32_t subChunkSize;

    if (!read((uint8_t*)pHeader, WAV_HEADER_TO_CHUNKLEN)) {
        assert("Early file header read failed."==nullptr);
        throw "WaveFileBufferReader::Early File header read failed.";
    }

//    printHex((uint8_t*)ptr, WAV_HEADER_TO_CHUNKLEN);

    assert(strncmp((char*)ptr, "RIFF", 4)==0);

    // chunksize - skip
    ptr = pHeader + 4;
    tmp32 = ptr[3]<<24 | ptr[2]<<16 | ptr[1]<<8 | ptr[0];
    // This value represents FILESIZE-8 for the two entries at +0 and +4
//    printf("Chunksize at RIFF header says: %d\n", tmp32);

    ptr = pHeader + 8;
    assert(strncmp((char*)ptr, "WAVE", 4)==0);

    ptr = pHeader + 12;
    assert(strncmp((char*)ptr, "fmt ", 4)==0);

    ptr = pHeader + 16;
    subChunkSize = ptr[3]<<24 | ptr[2]<<16 | ptr[1]<<8 | ptr[0];
    if (subChunkSize !=16 && subChunkSize !=18) {
#ifdef ESP_PLATFORM
        Serial.printf("WARN: May not be PCM. After 'fmt ', Subchunk size was not 16 or 18. Instead it is %d.\n", subChunkSize);
#else
        printf("WARN: May not be PCM. After 'fmt ', Subchunk size was not 16 or 18. Instead it is %d.\n", subChunkSize);
#endif
    }

    // Now we have a value for the number of bytes remaining. It should be 16 or 18.
    // And we're going to read an extra 8 bytes to get the ID and Size from the next chunk header.
    if (!read((uint8_t*)pHeader+WAV_HEADER_TO_CHUNKLEN, subChunkSize+8))
        throw "WaveFileBufferReader::Later File header read failed.";

    ptr = pHeader + 20;
    tmp16 = ptr[1]<<8 | ptr[0];
    if (tmp16 != 1) {
        printHex(pHeader, WAV_HEADER);
        assert(tmp16==1 && "@20:PCM==1");
    }

    ptr = pHeader + 22;
    numChannels = ptr[1]<<8 | ptr[0];
    assert(numChannels<=2);

    ptr = pHeader + 24;
    sampleRate = ptr[3]<<24 | ptr[2]<<16 | ptr[1]<<8 | ptr[0];
    assert(sampleRate <= 48000);

    ptr = pHeader + 28;
    byteRate = ptr[3]<<24 | ptr[2]<<16 | ptr[1]<<8 | ptr[0];
    // Wait for next reads as we need them for validation.

    ptr = pHeader + 32;
    blockAlign = ptr[1]<<8 | ptr[0];
    // Should be '1' for mono 8-bit ; 2 for mono 16-bit ; 4 for 2-channel 16-bit

    ptr = pHeader + 34;
    bitsPerSample = ptr[1]<<8 | ptr[0];
    assert(bitsPerSample==8 || bitsPerSample==16);

    // Validate a few things now that we have all the data.
    // Byterate was given to us but it should be chan*bitspersample/8*samplerate
//    printf("byteRate read is:%lu. numch=%d, bitspersample=%d, samplerate=%lu\n", 
//        byteRate, numChannels, bitsPerSample, sampleRate);
    assert(byteRate == numChannels*bitsPerSample/8*sampleRate);
    // Blockalign is in stack var blockAlign and should be numchannels*bitspersample/8;
    assert(blockAlign == numChannels*bitsPerSample/8);
    assert(sampleRate <= 48000);

// Now we need to iterate through chunks until we encounter 'data' as the chunk ID
// Then read the final data chunk size and leave the seek position in the file 'ready to read data'
    ptr = pHeader + WAV_HEADER_TO_CHUNKLEN + subChunkSize;
    totalWaveBytes = ptr[7]<<24 | ptr[6]<<16 | ptr[5]<<8 | ptr[4];
    if (strncmp((char*)ptr, "data", 4)) {
        long payloadSize = totalWaveBytes;
        unsigned char head[8];

//        printf("Chunk isn't 'data' but instead is '%c%c%c%c'\n", ptr[0], ptr[1], ptr[2], ptr[3]);
        // Now we need to iteratively skip the payload and find the next chunk iD + length
        // by doing a seek(payloadlength) followed by a read(8) and re-processing.
        do {
//            printf("Scanning for next CHUNK %ld bytes forward.\n", payloadSize);
            // if (payloadSize==26) {
            //     seek(26);
            //     read((uint8_t*)head,8);
            //     printHex(head, 8);
            //     exit(200);
            // }
//            else if (!seek(payloadSize)) {
            if (!seekRel(payloadSize)) {
                assert("SEEK failed for CHUNK."==nullptr);
                throw "Failed to seek while iterating through CHUNKs";
            }
            if (!read((uint8_t*)head, 8)) {
                assert("READ failed for CHUNK."==nullptr);
                throw "Failed to read next 8 bytes in seeking through CHUNKs";
            }
//            printf("NEXT CHUNK HEAD: ");
//            printHex((uint8_t*)head, 8);

            payloadSize = head[7]<<24 | head[6]<<16 | head[5]<<8 | head[4];
            if (!payloadSize) {
                assert("SEEK failed for CHUNK."==nullptr);
                throw "Payload failure.";
            }

            if (strncmp((char*)head, "data", 4)) {
//                printf("Chunk isn't 'data'. Skipping '%c%c%c%c'\n", head[0], head[1], head[2], head[3]);
            }
            else {
                // Got the data chunk. Time to step out.
//                printf("Total byte size of the chunk payload is: %lu\n", payloadSize);
                totalWaveBytes = payloadSize;
                break;
            }

        } while(true) ;
    }

//    printf("Total byte size of the 'data' chunk payload is: %lu\n", totalWaveBytes);
    bIsBufferReady = true;
    bufferAlloc();

    // Now that we're sitting on "byte 1", let's read that byte so we can prep our ramp in data.
    // Goal: Read the byte - keep it on hand - then calculate our ramp in data and lay it in the buffer.
    //       Finally lay down byte one and continue.
    readFirstByteAndPrepRampIn();

    Start();
}

//
// TODO: Needs to take into consideration 16-bit samples & 2-channel also
//       Currently ONLY lays down ramp-in for 8-bit mono
//
void WaveFileBufferReader::readFirstByteAndPrepRampIn() {
    uint8_t firstByte;

    try {
        read(&firstByte, 1);
    } catch (FileException& fex) {
        PrintLN("RampIn: EXIT - failed to read ONE BYTE.");
        return;
    }

    uint8_t rampInDelta = firstByte / (rampTime / (1000000 / sampleRate) );
#ifdef ESP_PLATFORM
    Serial.printf("RampIn: Target: %u, step delta: %u\n", firstByte, rampInDelta);
#else
    printf("RampIn: Target: %u, step delta: %u\n", firstByte, rampInDelta);
#endif
    assert(pWavBuffer);
    for(uint8_t rampValue=0; rampValue<firstByte; pBufferWrite++, rampValue += rampInDelta)
        *pBufferWrite = rampValue;

    *pBufferWrite = firstByte;
}

void WaveFileBufferReader::prepRampOut() {
    assert(pWavBuffer);
    assert(isFileReadComplete());

    // No matter what happens, our job here is "done".
    // Even if there's no room in the buffer, we'll just wind up with a 'pop' in ramp out
    bIsRampOutComplete = true;

    // Final byte of the file is sitting at pBufferWrite-1
    uint8_t startingValue;
    if (pBufferWrite==pWavBuffer)
        startingValue = *(pWavBuffer + lengthWavBuffer -1);
    else
        startingValue = *(pBufferWrite-1);
    
    assert(rampTime);
    assert(sampleRate);

    uint8_t rampOutDelta = startingValue / (rampTime / (1000000 / sampleRate) );

    if (!rampOutDelta) {
        PrintLN("RampOut: EXIT - rampOutDelta is ZERO - likely because final value is zero.");
        return;
    }
        
    uint8_t numIterations = startingValue / rampOutDelta;

#ifdef ESP_PLATFORM
    Serial.printf("RampOut: Start-At: %u, step delta: %u, iterations: %u\n", startingValue, rampOutDelta, numIterations);
#else
    printf("RampOut: Start-At: %u, step delta: %u\n", startingValue, rampOutDelta);
#endif

    // How much space do we need to put the ramp-out data into the buffer?
    // If there's no space for it, then we could be in trouble.
    // The "Cheap" way out here is to NOT allow the corner cases. Only allow cases where
    // pBufferRead > pBufferWrite+numIterations. This means no 'wrap' cases and no optimizing
    // situations where there might be room to add some of the rampOut data and not all of it.
    if (pBufferRead > pBufferWrite+numIterations) {
        // We've got space. Lay down the values and move the write pointer.
        for(uint8_t rampValue=startingValue; rampValue>=rampOutDelta; pBufferWrite++, rampValue -= rampOutDelta) {
            *pBufferWrite = rampValue;
        }
    }
    else
        PrintLN("RampOut: EXIT - did not have room to write RampOut data.");

}

void WaveFileBufferReader::printFileInfo() {
    int seconds = totalWaveBytes / byteRate;
#ifdef ESP_PLATFORM
    Serial.printf("File: %s - %d-Channel, %d-Bits, %lu Hz, Byterate:%lu, Total Wave Bytes:%lu, Total Playout Time:%d seconds\n", 
        fileName.c_str(), numChannels, bitsPerSample, sampleRate, byteRate, totalWaveBytes, seconds);
#else
    printf("File: %s - %d-Channel, %d-Bits, %lu Hz, Byterate:%lu, Total Wave Bytes:%lu, Total Playout Time:%d seconds\n", 
        fileName.c_str(), numChannels, bitsPerSample, sampleRate, byteRate, totalWaveBytes, seconds);
#endif
}

////////////////////////////////////
//
// B U F F E R   M E T H O D S
//
////////////////////////////////////

void WaveFileBufferReader::bufferAlloc() {
    if (pWavBuffer) {
        //TODO: Might we need to halt the thread (if we're not being called BY the thread)
        //      or otherwise signal the DAC-WRITE-ISR so it doesn't start writing from bogus memory?
        delete pWavBuffer;
        pWavBuffer = nullptr;
    }

    // Need to calculate how big our buffer shoudl be.
    // Goals: 
    //      Don't allocate "too much" memory
    //      Minimize the frequency with which the 'fill' task has to wake up and process
    //      Have enough added 'slop' in our timing so that the fill can catch up if it gets a bit behind
    //      Possibly allow for the task Run() to adjust its own sleep period based on success/failure
    // Technique/Plan:
    //      byteRate will dictate a lot about size of buffer and timing of wakeup
    //      If byteRate is bytes that must be processed in a second, and if we know some sample 'rates':
    //          8-bit mono, the byte rate is the sample rate. So 8kHz is 8000 bytes per second. Likely a minimum
    //              32kHz is 32kbytes in a second
    //              48kHz is 48kbytes in a second (this is really the max and would be surprising for high sample rate of an 8-bit mono signal)
    //          16-bit mono, byterate is double the sample rate
    //              Likely 8kHz(16kB/s) to possibly 44100Hz(88kB/s)
    //          8-bit 2-channel is unlikely to find in nature - 2 channels likely implies 16-bit samples at least
    //          16-bit 2-channel is 4x of the sample rate (2 bytes per sample and 2 samples for the 2 channels)
    //              Likely 22050Hz(88kB/s) to 48kHz(192kB/s)
    //
    assert(byteRate);
    assert(numChannels);
    assert(bitsPerSample);
    assert(sampleRate);

    uint8_t curIndex=0;
    for (curIndex=0; curIndex<MAX_SIZES_COUNT; curIndex++) {
        if (byteRate <= bufferByteRates[curIndex]) {
            lengthWavBuffer = bufferSizes[curIndex];
            break;
        }
    }
#ifdef ESP_PLATFORM
//    Serial.printf("WAVE Buffer allocation size: %u\n", lengthWavBuffer);
#else
//    printf("WAVE Buffer allocation size: %u\n", lengthWavBuffer);
#endif
    pWavBuffer = new uint8_t[lengthWavBuffer];
    assert(pWavBuffer);

    pBufferRead = pBufferWrite = pWavBuffer;
    bIsBufferReady = true;
}

bool WaveFileBufferReader::isPlaybackComplete() {

    if (bIsDoneReadingFile && pBufferRead==pBufferWrite) {
//        printf("Pausing the thread to avoid any issues.\n");
#ifndef ESP_PLATFORM
        Pause();
#endif
        return true;
    }
    else
        return false;
}

void WaveFileBufferReader::bufferFill() {
    uint32_t bytesToFill;
    uint32_t rate;
#ifdef ESP_PLATFORM
    unsigned long readTimeStart, readTimeEnd, readTimeElapsed;
#else
    std::chrono::high_resolution_clock::time_point startPT;
    std::chrono::high_resolution_clock::time_point endPT;
    std::chrono::duration<double> span;  // Span is in SECONDS as a double
#endif 

    // Circular buffer with a pBufferRead and a pBufferWrite
    // Make local copies of the READ pointer as the ISR will modify it when it writes data.
    uint8_t* pLocalRead = pBufferRead;

    if (bIsDoneReadingFile || !bIsBufferReady)
        return;

    assert(totalWaveBytes);
    assert(lengthWavBuffer);
    assert(pBufferRead);
    assert(pBufferWrite);

    if (100*totalWavBytesReadSoFar/totalWaveBytes >= percentComplete) {
#ifdef ESP_PLATFORM
        Serial.printf("%d%%  ", percentComplete);
#else
        printf("%d%%  ", percentComplete);
        fflush(stdout);
#endif
        percentComplete += 10;  // Don't print again until we get to another 10% completed.
    }

    //
    // If pBufferRead is greater than pBufferWrite, then the difference is all we want to add.
    //
    // If, however, pBufferWrite > pBufferRead, then we want to fill to the end and then fill from
    // the front of the buffer to where pBufferRead is at. Two separate memcpy() events.
    //
    if (pLocalRead > pBufferWrite || bIsFirstFill) {
        if (bIsFirstFill) {
            bIsFirstFill=false;
            bytesToFill = 2 * lengthWavBuffer / 5;      // Don't want to do a HUGE fill initially
        }
        else
            bytesToFill = pLocalRead - pBufferWrite;

#ifdef ESP_PLATFORM
//        Serial.printf("Read > Write - need to fill %u%% (%u bytes)\n", 100*bytesToFill/lengthWavBuffer, bytesToFill);
        readTimeStart = micros();
#else
//        printf("Read > Write - need to fill %u%% (%u bytes)\n", 100*bytesToFill/lengthWavBuffer, bytesToFill);
        startPT = std::chrono::high_resolution_clock::now();
#endif
        try {
            read(pBufferWrite, bytesToFill);
        } catch (FileException& fex) {
            if (fex.isEOF()) {
                bIsDoneReadingFile=true;
                pBufferWrite += fex.getPartial();
                // Guaranteed to be ok address math since Read > Write (above)
                //   which implies there's no chance of an address wrap situation.
                lastByteValueOfFile = *(pBufferWrite - 1);
                return;
            }
        }
#ifdef ESP_PLATFORM
        readTimeEnd = micros();
        readTimeElapsed = readTimeEnd - readTimeStart;
        rate = (uint32_t)((float)bytesToFill / (float)((float)readTimeElapsed / (float)1000000));
        Serial.printf("READING (%d%%) One Part:  %5u bytes in %4u uS is a rate of %d kB/s\n", getFileReadPercentage(), bytesToFill, readTimeElapsed, rate/1024);
#else
        endPT = std::chrono::high_resolution_clock::now();
        span = endPT - startPT;
        rate = bytesToFill / span.count();
//        printf("READING (%lu%%) One Part:  %5u bytes in %4u uS is a rate of %d kB/s\n", getFileReadPercentage(), bytesToFill, (uint32_t)(span.count()*1000000), rate/1024);
#endif

        pBufferWrite += bytesToFill;
        lastByteValueOfFile = *(pBufferWrite - 1);
        assert(pBufferWrite < pWavBuffer+lengthWavBuffer);
    }
    else if (pBufferWrite > pLocalRead) {
        // In this case, we have to fill from 'write' to the end of the buffer
        // and then fill from the front of the buffer to 'read'
        uint32_t bytesToEndOfBuffer = (pWavBuffer+lengthWavBuffer) - pBufferWrite;
        uint32_t bytesAtFrontOfBuffer = pLocalRead - pWavBuffer;
        bytesToFill = bytesToEndOfBuffer + bytesAtFrontOfBuffer;

#ifdef ESP_PLATFORM
//        Serial.printf("Write > Read - need to fill %u%% (%u bytes - 1st:%u 2nd:%u)\n", 100*bytesToFill/lengthWavBuffer, bytesToFill, bytesToEndOfBuffer, bytesAtFrontOfBuffer);
        readTimeStart = micros();
#else
//        printf("Write > Read - need to fill %u%% (%u bytes - 1st:%u 2nd:%u)\n", 100*bytesToFill/lengthWavBuffer, bytesToFill, bytesToEndOfBuffer, bytesAtFrontOfBuffer);
        startPT = std::chrono::high_resolution_clock::now();
#endif

    // 
    // Read from 'write' to end of buffer.
    // If we get EOF here, then the end of the read is by definition BEFORE the end of buffer
    // And so the EOF would find the last byte at the write pointer minus one.
    //
        try {
            read(pBufferWrite, bytesToEndOfBuffer);
        } catch (FileException& fex) {
            if (fex.isEOF()) {
                bIsDoneReadingFile=true;
                pBufferWrite += fex.getPartial();
                // Guaranteed to be ok address math since Read > Write (above)
                //   which implies there's no chance of an address wrap situation.
                lastByteValueOfFile = *(pBufferWrite - 1);

                return;
            }
        }

        // Second part of the read is at the FRONT of the buffer, by definition.
        // And whether at the end of a read or an EOF, the last byte will be
        // at the write pointer minus one.
        try {
            read(pWavBuffer, bytesAtFrontOfBuffer);
        } catch (FileException& fex) {
            if (fex.isEOF()) {
                bIsDoneReadingFile=true;
                pBufferWrite = pWavBuffer + fex.getPartial();
                // Guaranteed to be ok address math since Read > Write (above)
                //   which implies there's no chance of an address wrap situation.
                lastByteValueOfFile = *(pBufferWrite - 1);

                return;
            }
        }

#ifdef ESP_PLATFORM
        readTimeEnd = micros();
        readTimeElapsed = readTimeEnd - readTimeStart;
        rate = (uint32_t)((float)bytesToFill / (float)((float)readTimeElapsed / (float)1000000));
//        Serial.printf("READING (%d%%) Two Parts: %5u bytes in %4u uS is a rate of %d kB/s\n", getFileReadPercentage(), bytesToFill, readTimeElapsed, rate/1024);
#else
        endPT = std::chrono::high_resolution_clock::now();
        span = endPT - startPT;
        rate = bytesToFill / span.count();
//        printf("READING (%lu%%) Two Parts: %5u bytes in %4u uS is a rate of %d kB/s\n", getFileReadPercentage(), bytesToFill, (uint32_t)(span.count()*1000000), rate/1024);
#endif

        pBufferWrite = pLocalRead;      // Skip ahead to where 'read' was when we entered.
        lastByteValueOfFile = *(pBufferWrite - 1);
    }
    else {
#ifdef ESP_PLATFORM
//        Serial.println("WARN: Write and Read buffers are in the same location. Nothing to do.");
#else
//        printf("WARN: Write and Read buffers are in the same location. Nothing to do.\n");
#endif
    }
}

uint8_t WaveFileBufferReader::getFileReadPercentage() {
    return 100*totalWavBytesReadSoFar/totalWaveBytes;
}

uint8_t WaveFileBufferReader::getBufferFullPercentage() {
    if (pBufferRead <= pBufferWrite)
        return 100 - ((pBufferWrite - pBufferRead) / lengthWavBuffer);
    else {
        // length to end of buffer + read-pWaveBuffer
        uint32_t lenToEnd = (pWavBuffer + lengthWavBuffer) - pBufferRead;
        return 100 - (((pBufferWrite-pWavBuffer)+lenToEnd) / lengthWavBuffer);
    }
}

uint8_t* WaveFileBufferReader::getReadPointer() {
    return pBufferRead;
}

//
// Advances the read pointer by one.
//
void WaveFileBufferReader::advanceReadPointer() {
// Lock down variables?
    if (pBufferRead+1 >= pWavBuffer+lengthWavBuffer)
        pBufferRead = pWavBuffer;
    else
        pBufferRead++;
}

void WaveFileBufferReader::Run()
{
    if (bIsBufferReady && !bIsDoneReadingFile && hasElapsed(fillSleepTime)) {
        // Do update stuff.
        resetElapsedTimer();
        bufferFill();
        if (bIsDoneReadingFile && !bIsRampOutComplete) {
            // Give a little time to allow the reader to drain the buffer.
            // This gives us space to put the ramp-out data into the buffer.
            SleepMS(10);
            prepRampOut();
        }

#ifdef SIMULATE_READING
        // Artificially drain 33%
        if (bIsDoneReadingFile) {
            pBufferRead = pBufferWrite;
        }
        else {
            pBufferRead += lengthWavBuffer / 3;
            if (pBufferRead > pWavBuffer + lengthWavBuffer)
                pBufferRead = pWavBuffer + (pBufferRead - (pWavBuffer + lengthWavBuffer));
        }
#endif
    }

#ifdef ESP_PLATFORM
      vTaskDelay( 10 / portTICK_PERIOD_MS ); // 50 ms wait period while task is 'paused'
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
}
