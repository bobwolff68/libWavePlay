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
#include <Arduino.h>
#else
#include <stdint.h>
#include <dirent.h>
#endif
#include "WaveFileStdioReader.h"
#include "AudioFilePlayer.h"
#include <vector>
#include <string>
#include <iostream>

#include "AudioPlaylistManager.h"
#include "utils.h"

/*! \mainpage Support classes for (limited) processing of WAVE/PCM files on an ESP32 device.
 *
 * \section intro_sec Introduction
 *
 * There are a few wave file processing and DAC-output libraries available for ESP32, but none
 * of them were structured in a nice C++ oriented manner or multi-threaded manner as would be
 * desired, so I wrote my own. Please see the README.md file as well.
 * 
 * Want to play audio on your ESP32? You can hook up an I2S device but it'll cost quite a bit 
 * more than simply having an audio amplifier on the DAC pin. I've used the LM386 pre-built 
 * boards which are quite inexpensive. It is also possible to simply hook up a 4Ohm or 8Ohm 
 * speaker to the output of the DAC pin (25 or 26) but the volume will be very low even with 
 * the volume set to 100. I've found the LM386 at 5 volts works quite well for a nearby single 
 * speaker. When using 12 volts with the amplifier, I found too much clipping despite a generally
 * louder final output.
 *
 * \section limitations_sec Limitations
 *
 * The WAVE specification allows for many features and options which have grown over the years
 * since it was published in the 1990's. This library specifically concentrates on the use of
 * non-compressed PCM formats. And while it is intended to support a variety of sampling rates,
 * channels, and resolutions, it has been primarily tested against 8-bit resolution mono files.
 * Ultimately it is possible to support much more, but the reality is that these little ESP32
 * devices have limited space to hold large files and often times the speaker output is limited
 * enough to make high resolution or sampling rates ineffective in the real world.
 * 
 * So, watch out for pitfalls:
 * - Volume features are very specific to 8-bit values today. The pDataTable calculation is
 *   hard coded to 256 values. Supporting 16-bit values would require 32k*16-bit = 64kBytes
 *   of calculations. This could be limited to a smaller figure or samples could be volume-corrected
 *   on the fly for non-8-bit values as an alternative.
 * - Another option with 16-bit files is to convert them to 8-bit on the fly, but this suffers
 *   from the fact that the file is large in memory/flash and yet there's no audio clarity gained
 *   by that higher resolution file. So, typically a utility like Audacity or ffmpeg is used to convert
 *   files down to 8-bits prior to loading them into the SPIFFS or LittleFS filesystem.
 * - No multi-channel downmixing yet.
 * - Since this is targeting an ESP32, there is an expectation that only a single instance
 *   is used. There are some static items in the classes which would be problematic for use
 *   if more than one instance is created. An advancement would be to use a factory or singleton
 *   pattern in such a case, but the added complication doesn't seem to make sense given the target.
 *
 */

//! @file main.cpp

//#define FORCE_FORMAT

#ifdef ESP_PLATFORM
//! @brief in linux-style or ESP32-style - retrieve the list of files in a folder.
//! @return true for success
//! @param dirLocation is the relative directory/folder.
//! @param fileList is a vector of strings which is a reference so it is passed back/returned.
bool getFiles(const char* dirLocation, std::vector<std::string>& fileList) {
  assert(dirLocation==nullptr || dirLocation==""); 

  File root = FSTYPE.open("/");

  File file = root.openNextFile();
 
  if (!file)
    PrintLN("NO FILES.");

  while(file){ 
//      Serial.print("FILE: ");
//      Serial.println(file.name());
      fileList.push_back(file.name());
 
      file = root.openNextFile();
  }
  return true;
}
#else
bool getFiles(const char* dirLocation, std::vector<std::string>& fileList) {
  DIR *dir;
  struct dirent *ent;
  std::string fullName;
  assert(dirLocation);
  if ((dir = opendir (dirLocation)) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
//      printf ("%s\n", ent->d_name);
      if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
        fullName = dirLocation;
        fullName += "/";
        fullName += ent->d_name;
        fileList.push_back(fullName);
      }
    }
    closedir (dir);
  } else {
    /* could not open directory */
    return false;
  }

  return true;
}
#endif

void wavFileTest() {
#if 0
  pWav = new WaveFileType("/cartman-pissed-off.wav");
    pWav->printFileInfo();

  // wait for reading/playing complete.
    while (!pWav->isPlaybackComplete()) {
        SleepMS(100);
    }

    delete pWav;

    PrintLN("done with file.");

    pWav = new WaveFileType("/cartman-going-home.wav");
    pWav->printFileInfo();

  // wait for reading/playing complete.
    while (!pWav->isPlaybackComplete()) {
        SleepMS(100);
    }

    delete pWav;

    PrintLN("done with file.");
#ifdef ESP_PLATFORM
    return;
#endif
    exit(2);
#endif  
}

void doActions() {
  std::vector<std::string> myFiles;
  WaveFileType* pWav;
#ifdef ESP_PLATFORM
  const char* dirName=nullptr;
#else
//  const char* dirName="./test";
  const char* dirName="./waveFilesForPlaya";
#endif

  if (!getFiles(dirName, myFiles)) {
    PrintLN("Unable to get list of files");
    exit(1);
  }

#if 0
AudioFilePlayer* pAFP = new AudioFilePlayer(0, 25);

#ifdef ESP_PLATFORM
//pAFP->LoadFile("/cartman-pissed-off.wav");
pAFP->LoadFile("/cartman-going-home.wav");
#else
pAFP->LoadFile("waveFilesForPlaya/monty-python-peril.wav");
#endif

  pAFP->pWave->printFileInfo();

pAFP->PlayFile();
while (!pAFP->isDonePlaying()) {
  SleepMS(1000);
}
pAFP->PauseFile();

printf("And now I recognize we're complete.\n");

exit(1);
#endif

#if 1
  PrintLN("Files found:");
  for (auto ent: myFiles) {
    PrintLN(ent.c_str());
  }
  SleepMS(1500);
#endif

#if 1
  AudioFilePlayer* pAFP = new AudioFilePlayer(0, 25);
  std::string fn;

  for (auto ent: myFiles) {
#ifdef ESP_PLATFORM
    fn = "/" + ent;
    pAFP->LoadFile(fn.c_str());
#else
    pAFP->LoadFile(ent.c_str());
#endif

    pAFP->pWave->printFileInfo();

    SleepMS(1500);

    pAFP->PlayFile();
    while (!pAFP->isDonePlaying()) {
      SleepMS(1000);
    }
    pAFP->PauseFile();

    printf("File playout complete.\n");
  }
#endif

  SleepMS(500);
}

void playlistAction() {
  std::unique_ptr<AudioPlaylistManager> pAPM = make_unique<AudioPlaylistManager>(0, 25, "");

  std::vector<std::string> filelist;
  pAPM->GetFileList(filelist);

  PrintLN("Getting file list from playlist manager.");
  for (auto x: filelist)
#ifdef ESP_PLATFORM
    Serial.printf("  %s\n", x.c_str());
#else
    printf("  %s\n", x.c_str());
#endif

  pAPM->SetVolume(35);
//  pAPM->PlayEntryName("/King.wav");
  pAPM->PlayRandomEntry();

// Not allowing an exit as the unique_ptr would be deleted
  while(1)
    SleepMS(1000);
}

#ifdef ESP_PLATFORM
/*! @fn setup
 *  @brief   ESP32/Arduino style entry point. This is ran ONCE by the 'system'
 *  @details Setup serial port, format the flash if FORCE_FORMAT is defined. 
 *           Then either call doActions or playlistAction to exercise the library.
 *           @note This is a bit unusual as loop is where Arduino-style systems usuall
 *                 do all their 'work'. But for this test-jig we only do these things once.
*/
void setup() {	
  Serial.begin(115200);

  if (!FSTYPE.begin(true)) {
      PrintLN("An Error has occurred while mounting filesystem");
      return;
  }
#ifdef FORCE_FORMAT
  PrintLN("Forcing filesystem format. Please wait.");
  SleepMS(1000);
  bool ok = FSTYPE.format();
  if (ok)
    PrintLN("Filesystem formatted. Please reboot after changing FORCE_FORMAT");
  else
    PrintLN("Filesystem format failed.");

  while(1)
    SleepMS(1000);
#endif

  Serial.println("Hello World.");

  try {
//    doActions();
    playlistAction();
  }
  catch(std::exception& e) {
    Serial.printf("setup() - caught exception: %s", e.what());
    while(1)
      ;
  }
  catch(...) {
    Serial.println("Other exception type found in setup().");
    while(1)
      ;
  }
}

//! @brief ESP32/Arduino loop is called again and again, but we're not using this in
//!        the test-jig programming style here. setup will have done all the actions.
void loop() {
  // put your main code here, to run repeatedly:
}
#else
int main(int argc, char** argv) {
  doActions();
//  playlistAction();
}
#endif
