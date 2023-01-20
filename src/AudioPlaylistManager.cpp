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
#include "AudioPlaylistManager.h"

AudioPlaylistManager::AudioPlaylistManager(uint8_t esp32Timer, uint8_t esp32Pin, const char* _initLoc, uint8_t _ampControlPin, bool _onPinHigh)
{
    assert(esp32Timer < 4);
    assert(esp32Pin < 40 && esp32Pin > 0);
    
    ClearFileList();
    if (_initLoc) {
        AddFilesFrom(_initLoc);
    }

    pAFP = make_unique<AudioFilePlayer>(esp32Timer, esp32Pin);
    assert(pAFP);

    ampPowerPin = _ampControlPin;
    ampPowerPinHigh = _onPinHigh;
#ifdef ESP_PLATFORM
    if (ampPowerPin) {
        pinMode(ampPowerPin, OUTPUT);
    }

    SetAmpPower(false);
#endif

    entryNumberForIntro = -1;
    entryNumberToPlay = -1;

    curState = Idle;
    Start();
}

void AudioPlaylistManager::PlayRandomEntry()
{
    assert(pAFP);

    if (filenames.size()) {
        int16_t entry = getrand(0, filenames.size()-1);
        PlayEntryIndex(entry);
    }
}

void AudioPlaylistManager::SetIntroSoundIndex(uint16_t entryNum)
{
    if (entryNum < filenames.size()) {
        entryNumberToPlay = entryNum;
    }
}

void AudioPlaylistManager::SetIntroSoundName(const char* fname)
{
    if (!fname)
        return;

    auto it = std::find(begin(filenames), end(filenames), fname );

    if (it != std::end(filenames))
        entryNumberForIntro = it - begin(filenames); 
    else
        entryNumberForIntro = -1;
}

void AudioPlaylistManager::PlayEntryIndex(uint16_t entryNum)
{
    if (entryNum >= filenames.size())
        return;

    entryNumberToPlay = entryNum;

    Play();
}

void AudioPlaylistManager::PlayEntryName(const char* fname)
{
    if (!fname)
        return;

    auto it = std::find(begin(filenames), end(filenames), fname );

    if (it != std::end(filenames))
        entryNumberToPlay = it - begin(filenames); 
    else
        entryNumberToPlay = -1;

    Play();
}

void AudioPlaylistManager::Play()
{
    if (entryNumberToPlay == -1)
        return;

    if (curState==Idle) {
        NextState(PlayingIntro);
    }
    else {
        PrintLN("Play() - just saying 'play' as we're not in idle.");
        pAFP->PlayFile();
    }
}

void AudioPlaylistManager::Pause()
{
    if (curState == PlayingIntro || curState == PlayingSound)
        pAFP->PauseFile();
}

void AudioPlaylistManager::SetVolume(uint8_t _vol)
{
    if (_vol > 100)
        return;

    assert(pAFP);
    pAFP->SetVolume(_vol);
}

void AudioPlaylistManager::ClearFileList()
{
    filenames.clear();
    entryNumberForIntro = -1;
}

void AudioPlaylistManager::AddFilesFrom(const char* _dirname)
{
    std::vector<std::string> myFiles;

    if (!GetFiles(_dirname, myFiles)) {
        PrintLN("Unable to get list of files");
        exit(1);
    }

    for (auto it=myFiles.begin() ; it!=myFiles.end(); it++)
        filenames.push_back(*it);

}

void AudioPlaylistManager::GetFileList(std::vector<std::string>& fList)
{
    fList = filenames;
}

void AudioPlaylistManager::SetAmpPower(bool _on)
{
#ifdef ESP_PLATFORM
    if (ampPowerPin) {
        if (ampPowerPinHigh) {
            digitalWrite(ampPowerPin, _on);
        }
        else {
            digitalWrite(ampPowerPin, !_on);
        }
    }
#endif
}

#ifdef ESP_PLATFORM
bool AudioPlaylistManager::GetFiles(const char* dirLocation, std::vector<std::string>& fileList) {
    assert(dirLocation==nullptr || dirLocation=="");

    File root = FSTYPE.open("/");
    File file = root.openNextFile();
    std::string fn;

    while(file) {
        fn = "/";
        fn += file.name();
        fileList.push_back(fn);

        file = root.openNextFile();
    }
    return true;
}
#else
bool AudioPlaylistManager::GetFiles(const char* dirLocation, std::vector<std::string>& fileList) {
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

void AudioPlaylistManager::NextState(State nextState) {
    if (nextState == Idle) {
        PrintLN("NextState: Going back to Idle.");
        curState = Idle;
        return;
    }

    if (curState == Idle) {
        if (nextState == PlayingIntro) {
            if (entryNumberForIntro != -1) {
                SetAmpPower(true);
                SleepMS(100);
                pAFP->LoadFile(filenames[entryNumberForIntro].c_str());
                pAFP->PlayFile();
                curState = PlayingIntro;
                return;
            }
            else {
                // There's no intro to play.
                // Let's jump directly to the play sound
                NextState(PlayingSound);
                return;
            }
        }

        if (nextState == PlayingSound) {
            if (entryNumberToPlay != -1) {
                SetAmpPower(true);
                SleepMS(100);
                pAFP->LoadFile(filenames[entryNumberToPlay].c_str());
                pAFP->pWave->printFileInfo();
                pAFP->PlayFile();
                curState = PlayingSound;
                return;
            }
        }
    }
    else if (curState == PlayingIntro) {
        if (nextState == PlayingSound) {
            if (entryNumberToPlay != -1) {
                SetAmpPower(true);
                SleepMS(100);
                pAFP->LoadFile(filenames[entryNumberToPlay].c_str());
                pAFP->PlayFile();
                curState = PlayingSound;
                return;
            }
        }
    }
    else if (curState == PlayingSound) {

    }
    else if (curState == Paused) {

    }

}

void AudioPlaylistManager::Run()
{
    if (hasElapsed(500)) {
        resetElapsedTimer();

        if (curState == Idle) {

        }
        else if (curState == PlayingIntro) {
            if (pAFP->isDonePlaying()) {
                SetAmpPower(false);
                NextState(PlayingSound);
                return;
            }

        }
        else if (curState == PlayingSound) {
            if (pAFP->isDonePlaying()) {
                SetAmpPower(false);
                pAFP->PauseFile();
                NextState(Idle);
                return;
            }

        }
        else if (curState == Paused) {

        }
    }
}
