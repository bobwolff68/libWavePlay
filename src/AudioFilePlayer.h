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

#ifdef ESP_PLATFORM
#include <Arduino.h>
#include "WaveFileLittleFSReader.h"
#include "WaveFileSPIFFSReader.h"
#else
#include <thread>
#include <chrono>
#include "WaveFileStdioReader.h"
#endif

#include "utils.h"
#include "robotask.h"

/*! \class   AudioFilePlayer
 *  \brief   Support for playing and pausing a single file.
 *  \details ESP32-centric class for playout and managment of playout for a single audio file.
 *           Timer and DAC pin are required inputs for ESP32 usage. When running in native compile
 *           mode, class will 'go through the motions' but will not actually play.
 *           Based on RoboTask for its independent threaded runtime.
*/
class AudioFilePlayer : RoboTask
{
public:
    /*! @brief Instantiates and sets up the hardware. Kicks off a thread for native playout as well.
     * @param esp32Timer - there are 4 timers in the ESP32 [0-3]
     * @param esp32Pin - there are two DAC pins on the ESP32 - 25 and 26
     */
    AudioFilePlayer(uint8_t esp32Timer, uint8_t esp32Pin);
    ~AudioFilePlayer();
    //! @brief This method causes the file header to be parsed and processed to be ready for playout.
    //! @return true on success or false if file could not be loaded/found/parsed.
    bool LoadFile(const char* fname);
    //! @brief Kick off the playout of the file.
    void PlayFile();
    //! @brief Pause playback. @todo this needs further testing
    void PauseFile();
    /*! @brief Attenuation of the signal in software. This does not control hardware.
     *  @param _vol is a range of 0-100 where 100 is a fully unmodified playout.
     */
    void SetVolume(uint8_t _vol);
    //! @brief Status of when the file is done being played fully.
    bool isDonePlaying();
    //! @brief RoboTask's thread-based worker for native mode. In ESP32 mode, the ISR handles DAC writing.
    void Run();
    //! @brief Utility for inspecting what values will be used given a volume set via SetVolume()
    //! @see SetVolume
    void printDataTable();
#ifdef ESP_PLATFORM
    //! @brief Interrupt service routine for ESP32 to control precise writing of DAC values.
    static void IRAM_ATTR timerISRCallback();
#endif
    //! @brief Base class instance for the loaded wave file to be processed/played
    static WaveFileType* pWave;
    //! @brief Holding place for the pin used as the DAC output.
    static uint8_t pinDAC;

protected:
    //! Used in native mode to simulate processing at semi-realtime of the data (slowing things down)
    uint32_t taskSleepTimeTarget;
    //! Holding place for the timer number to be used in the ESP32 device
    uint8_t timerNumber;
    //! Volume translation table at a given volume.
    //! Since volume isn't changed often, we pre-calculate all 256 8-bit output values after the volume
    //! is changed. This is more efficient than making the calculation on every value to be output.
    static uint8_t pDataTable[256];
    //! Holder for the current volume value.
    static uint8_t curVolume;
    //! Worker for calculating `pDataTable[]` values once the volume is changed in SetVolume
    void calcDataTableBasedOnVolume();
#ifdef ESP_PLATFORM
    //! Manage the hardware resource.
    void killTimer();
    //! Manage the hardware resource.
    void pauseTimer();
    //! Timer resource from ESP32.
    hw_timer_t *HWTimer;
#endif
};
