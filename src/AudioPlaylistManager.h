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
#else
#include <thread>
#include <chrono>
#include <dirent.h>
#endif

#include "utils.h"
#include "AudioFilePlayer.h"

#include <vector>
#include <string>
#include <memory>
#include "robotask.h"

/*! @class   AudioPlaylistManager 
 *  @brief   Organizer/Manager class for enabling playback of audio files from a list
 *  @details Features include:
 *           - Scanning for files in an _initLoc location to gather the full set of files.
 *           - Features the ability to set one of the audio files as an /Intro/. When there is an
 *             intro file, playout of any audio file will be preceeded by the playout of the intro.
 *           - Handles the statefulness of playing audio files including the Intro audio 
 *           - Thread-based so that it can handle stateful transitions and playout without any
 *             external polling or 'babysitting' of the class. 
 *           - Volume controls via software - not controlling an amplifier
 *           - Allows for a pin to be utilized for controlling an amplifier as an on/off mechnism.
 *             The purpose here is about power savings. If the audio is primarily unused, this allows
 *             the amplifier to be turned off using a transitor or MOSFET to do so in hardware.
 *           - Amplifier control pin can be active high or low via _onPinHigh
 *           - PlayRandomEntry() simply picks an entry from the list at random
 *           - Audio file list can be managed via ClearFileList() and AddFilesFrom()
 */
class AudioPlaylistManager : public RoboTask
{
public:
    /*! @brief instantiate the audio manager
     *  @param esp32Timer - there are 4 timers in the ESP32 [0-3]
     *  @param esp32Pin - there are two DAC pins on the ESP32 - 25 and 26
     *  @param _initLoc - if non-null, this is the folder location which should populate the file list.
     *  @param _ampControlPin - used for hardware control of the audio amplifier for playout
     *  @param _onPinHigh - active high hardware control when true. Active low when false.
     */
    AudioPlaylistManager(uint8_t esp32Timer, uint8_t esp32Pin, const char* _initLoc, uint8_t _ampControlPin=0, bool _onPinHigh=true);
    //! @brief Handles stateful playout of intro and desired audio clip in a thread.
    void Run();
    //! @brief Simply allows a random entry from the list to be played.
    void PlayRandomEntry();
    //! @brief Setting the intro sound file via the index
    void SetIntroSoundIndex(uint16_t entryNum);
    //! @brief Setting the intro sound file via the name of the file sans folder name
    void SetIntroSoundName(const char* fname);
    //! @brief Setting the file to play out via the index
    void PlayEntryIndex(uint16_t entryNum);
    //! @brief Setting the file to playout via the name of the file sans folder name
    void PlayEntryName(const char* fname);
    //! @brief Play/pause control
    void Play();
    //! @brief Play/pause control
    void Pause();

    //! @brief Sets the volume level (in software) from 0-100
    void SetVolume(uint8_t _vol);
    //! @brief Utility/debug routine for printing the modified values given the volume setting.
    void printDataTable() { if (pAFP) pAFP->printDataTable(); };

    //! @brief the audio file list contents.
    void ClearFileList();
    //! @brief Add (more) files to the current file list from a given _dirname
    void AddFilesFrom(const char* _dirname);
    //! @brief Obtain the current audio list as a vector of strings fList
    void GetFileList(std::vector<std::string>& fList);

    //! @enum Statefulness is handled by this group of enums.
    enum State { Idle, PlayingIntro, PlayingSound, Paused };

protected:
    State curState;
    uint8_t ampPowerPin;
    bool ampPowerPinHigh;
    //! @brief List of audio file names ready to be played.
    std::vector<std::string> filenames;
    //! Single instance of the AudioFilePlayer which is re-used for each playout.
    std::unique_ptr<AudioFilePlayer> pAFP;
    int16_t entryNumberForIntro;
    int16_t entryNumberToPlay;

    //! @brief Turns on or off the amplifier power. This is managed by the thread internally.
    void SetAmpPower(bool _on);
    //! @brief Worker function for iterating through the files in a folder
    bool GetFiles(const char* dirLocation, std::vector<std::string>& fileList);
    //! @brief Stateful transition utility for the thread.
    void NextState(State nextState);
};
