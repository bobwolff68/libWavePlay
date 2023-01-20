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
#include "AudioFilePlayer.h"

#ifdef ESP_PLATFORM
#define SleepMS(x)  (vTaskDelay( (x) / portTICK_PERIOD_MS ))
#define PrintLN(x)  (Serial.println((x)))
#else
#define SleepMS(x)  (std::this_thread::sleep_for(std::chrono::milliseconds((x))))
#define PrintLN(x)  { printf("%s\n",(x)); }
#endif

// 80MHz is the timer raw frequency
// 80MHz / 1MHz = 80 for the scaler ==> 1uS per tick
// 80Mhz / 40 prescaler = 2million ==> 0.5uS per tick
#define PRESCALER 80

uint8_t AudioFilePlayer::pinDAC = 0;
WaveFileType* AudioFilePlayer::pWave = nullptr;
uint8_t AudioFilePlayer::curVolume = 100;
// Just init with dont-care data - 256 entries.
uint8_t AudioFilePlayer::pDataTable[256] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};

AudioFilePlayer::AudioFilePlayer(uint8_t esp32Timer, uint8_t esp32Pin)
{
    pWave = nullptr;
    taskSleepTimeTarget=0;
    SetVolume(100);

    assert(esp32Timer < 4);
    timerNumber = esp32Timer;
    pinDAC = esp32Pin;

#ifdef ESP_PLATFORM
    pinMode(esp32Pin, ANALOG);
    // Setup the timer callback but don't enable it yet.
    HWTimer = timerBegin(timerNumber, PRESCALER, true);
    timerAttachInterrupt(HWTimer, &timerISRCallback, true);
#else
    taskSleepTimeTarget=10000;  // 10ms to start
    setBaseRunDelay(0);
    Start();
#endif
}

AudioFilePlayer::~AudioFilePlayer()
{
#ifdef ESP_PLATFORM
    killTimer();
#endif

    if (pWave) {
        delete pWave;
        pWave = nullptr;
    }
}

bool AudioFilePlayer::LoadFile(const char* fname)
{
#ifdef ESP_PLATFORM
    pauseTimer();
//    killTimer();
#else
    Pause();
#endif

    if (pWave) {
        PrintLN("AFP::LoadFile - pWave already exists. Deleting for re-load.");
        delete pWave;
        pWave = nullptr;
    }

    try {
        pWave = new WaveFileType(fname);
        assert(pWave);
    } catch(std::exception& e) {
#ifdef ESP_PLATFORM
        Serial.println("Unable to load file.");
#else
        printf("Unable to load file.\n");
#endif
        return false;
    }

    // Once we're loaded and we know the file parameters/settings, use that.
    taskSleepTimeTarget = 1000000 / pWave->getSampleRate();

#ifdef ESP_PLATFORM
    // // Setup the timer callback but don't enable it yet.
    // HWTimer = timerBegin(timerNumber, PRESCALER, true);
    // timerAttachInterrupt(HWTimer, &timerISRCallback, true);
    timerAlarmWrite(HWTimer, taskSleepTimeTarget, true);
#endif

    SleepMS(250);       // Need a little time for buffers to get set or we get static/noise
    return true;
}

void AudioFilePlayer::SetVolume(uint8_t _vol) {
    if (_vol > 100)
        curVolume = 100;
    else
        curVolume = _vol;

    calcDataTableBasedOnVolume();
}

void AudioFilePlayer::printDataTable() {
#ifdef ESP_PLATFORM
    Serial.printf("\n\nDataTable Lookup for Volume=%u\n", curVolume);
#else
    printf("\n\nDataTable Lookup for Volume=%u\n", curVolume);
#endif

    for (auto i=0; i<256; i++) {
        if (i % 16 == 0)
#ifdef ESP_PLATFORM
            Serial.printf("\n%3d: ", i);
#else
            printf("\n%3d: ", i);
#endif
        
#ifdef ESP_PLATFORM
        Serial.printf("0x%02x ", pDataTable[i]);
#else
        printf("0x%02x ", pDataTable[i]);
#endif
    }
}

void AudioFilePlayer::calcDataTableBasedOnVolume() {
    int8_t sig1;

//    unsigned long st = micros();

    for (uint16_t i=0; i<=255; i++) {
        sig1 = ((i-127) * curVolume / 100);
        pDataTable[i] = sig1+128;
//        Serial.printf("  At i=%3u, v.sig=%4d, output=%3u\n", i, v.sig, pDataTable[i]);
    }

//    unsigned long en = micros();
//    Serial.printf("calcDataTable: Time Required: %lu microSeconds\n", en-st);
}

#ifdef ESP_PLATFORM
void AudioFilePlayer::killTimer()
{
    if (HWTimer) {
        timerAlarmDisable(HWTimer);
        timerDetachInterrupt(HWTimer);
        timerEnd(HWTimer);
        HWTimer=nullptr;
    }
}

void AudioFilePlayer::pauseTimer()
{
    if (HWTimer) {
        timerAlarmDisable(HWTimer);
//        timerDetachInterrupt(HWTimer);
//        timerEnd(HWTimer);
//        HWTimer=nullptr;
    }
}
#endif

bool AudioFilePlayer::isDonePlaying() {
    if (!pWave) {
        PrintLN("AFP::isDonePlaying - YES - but due to NO pWAVE anymore. NOT GOOD?");
        return true;    // Must be "done" if there's nothing to play, right?
    }

    return pWave->isPlaybackComplete();
}

void AudioFilePlayer::PlayFile()
{
    assert(pWave);

#ifdef ESP_PLATFORM
    assert(HWTimer);
    timerAlarmEnable(HWTimer);
#else
    Start();
#endif
}

void AudioFilePlayer::PauseFile()
{
    assert(pWave);

#ifdef ESP_PLATFORM
    Serial.println("Pausing file playback.");
    assert(HWTimer);
    pauseTimer();
#else
    Pause();
#endif
}

void AudioFilePlayer::Run()
{
    static int32_t offsetSleep=0;

    if (!pWave || isDonePlaying())
        return;

#ifdef ESP_PLATFORM
    assert("AudioFilePlayer::Run() - should not be here on ESP32."==nullptr);
#else
    static std::chrono::high_resolution_clock::time_point lastPT = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point curPT;
    std::chrono::duration<double, std::micro> span;
 
    curPT = std::chrono::high_resolution_clock::now();
    span = curPT - lastPT;

    pWave->advanceReadPointer();

#if 0
    if (span.count() > taskSleepTimeTarget*1.2 || span.count() < taskSleepTimeTarget*0.8) {
        // Possibly correct the target time to compensate for the system
        printf("Buffer:%u%% File:%u%%: Missed target time of: %u uS. Actual was: %u.\n", 
                pWave->getBufferFullPercentage(), pWave->getFileReadPercentage(), taskSleepTimeTarget+offsetSleep, (uint32_t)span.count());
        
        // if (span.count() > taskSleepTimeTarget) {
        //     // We were too late - give a negative offset
        //     offsetSleep = -1 * (span.count() - taskSleepTimeTarget) / 2;
        // }
        // else
            offsetSleep = offsetSleep + (taskSleepTimeTarget - span.count()) / 3;
            if (offsetSleep > taskSleepTimeTarget * 0.9)
                offsetSleep = taskSleepTimeTarget * 0.9;
            else if (offsetSleep < -taskSleepTimeTarget * 0.9)
                offsetSleep = -taskSleepTimeTarget * 0.9;
    }
#endif

    if (hasElapsed(500)) {
        resetElapsedTimer();
        printf("Buffer:%u%% File:%u%%\n", pWave->getBufferFullPercentage(), pWave->getFileReadPercentage());
    }

    lastPT = curPT;
    if (taskSleepTimeTarget+offsetSleep > 20)
        std::this_thread::sleep_for(std::chrono::microseconds(taskSleepTimeTarget+offsetSleep));
    else {
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        offsetSleep = 20 - taskSleepTimeTarget;
    }
#endif
}

#ifdef ESP_PLATFORM
void IRAM_ATTR AudioFilePlayer::timerISRCallback() {
  static uint8_t lastValue=0x7f;
  static uint8_t* pDataLoc = nullptr;
  static uint8_t dataVal;

    // Shouldn't happen that pWave is null if timers are all handled well when changing files.
    // However, should it happen, let's just exit.
    if (!pWave || pWave->isPlaybackComplete())
        return;

    pDataLoc = pWave->getReadPointer();

    if (!pDataLoc)
        return;

    dataVal = *pDataLoc;
    dataVal = pDataTable[dataVal];
//    dataVal = DataBasedOnVolume(dataVal);

    // Don't send the same value to the DAC twice in a row.
    if (dataVal != lastValue) {
        dacWrite(pinDAC, dataVal);
        lastValue = dataVal;
    }

    pWave->advanceReadPointer();

}

#endif
