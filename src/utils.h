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

//#define PREF_SPIFFS

#ifdef ESP_PLATFORM
#include <Arduino.h>
#else
#include <thread>
#include <chrono>
#endif

#include <vector>
#include <string>

#ifdef ESP_PLATFORM
//! @brief Macro to allow sleeping for specified period in ms - works in native and ESP32
#define SleepMS(x)  (vTaskDelay( (x) / portTICK_PERIOD_MS ))
//! @brief Macro to allow simple print of text - works in native and ESP32
#define PrintLN(x)  (Serial.println((x)))
#ifdef PREF_SPIFFS
#include "SPIFFS.h"
#define WaveFileType WaveFileSPIFFSReader
#define FSTYPE SPIFFS
#else
#include "LittleFS.h"
//! @brief WaveFileType allows us to easily use Stdio or LittleFS (or SPIFFS - deprecated)
#define WaveFileType WaveFileLittleFSReader
#define FSTYPE LittleFS
#endif
#else
#define SleepMS(x)  (std::this_thread::sleep_for(std::chrono::milliseconds((x))))
#define PrintLN(x)  { printf("%s\n",(x)); }
#define WaveFileType WaveFileStdioReader
#endif

//! @brief gets a random number between two values (inclusive)
int16_t getrand(int16_t low, int16_t high);

#ifndef ESP_PLATFORM
//! @brief returns a value based on the input 'v' and boundaries low and high
int16_t constrain(int16_t v, int16_t low, int16_t high);
#endif

template<typename T, typename ...Args>
//! @brief Utilize C++11 to make a make_unique() for unique_ptr<>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

