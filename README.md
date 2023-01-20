# ESP32 PCM Audio Classes

Want to play audio on your ESP32? You can hook up an I2S device but it'll cost quite a bit more than simply having an audio amplifier on the DAC pin. I've used the LM386 pre-built boards which are quite inexpensive. It is also possible to simply hook up a 4Ohm or 8Ohm speaker to the output of the DAC pin (25 or 26) but the volume will be very low even with the volume set to 100. I've found the LM386 at 5 volts works quite well for a nearby single speaker. When using 12 volts with the amplifier, I found too much clipping despite a generally louder final output.

The classes utilize c++11 and this is called out in the .ini file profile. I've also taken my first shot at utilizing *std::unique_ptr* and *make_ptr* (which is truly available in C++14 but this version came from recommendations on Stackoverflow). Lastly, I've utliized exceptions in a few places. It's a first for me as well so some criticism might be welcoem. :)

PlatformIO .ini config is using aarch64 for ARM M1 processors on the Mac. Taking this out would be necessary for an x86 based computer.

Native compilation is supported to a degree, but without a true set of timers and DACs, the hardware doesn't actually play audio. But it is helpful at times to have a native compilation and run of new features without a USB cable and dev board etc.


## Features

* Configurable to use any of the 4 timers for feeding the DAC output
* Output to DAC pin with 8-bit resolution
* Volume settings for 8-bit samples done through a pre-calculated lookup table
  * Table takes ~15 microseconds to calculate upon changing the audio volume
* Task-based controls and processing
* Multiple filesystems supported: LittleFS, SPIFFS, and regular old stdio
* Reads and decodes WAV RIFF/fmt header and skips unknowns
* Buffer management both in terms of sizing the buffer based on sample-rate/bitspersample/etc as well as reading the file based on buffer fullness.
* Manages a "ramp in" and a "ramp out" to avoid *clicks* and *pops* in the speaker all based on the first sample value and last sample value.
* Integration class allows for enabling and disabling the audio amplifier through supporting hardware (BJT, MOSFET, or relay)
* Separate classes for
  * WaveFileBufferReader:: File reading/processing/buffer
  * AudioFilePlayer:: handles loading of WAV files & managing the playout including hardware
  * AudioPlaylistManager:: Integration layer which handles knowing all files available, having a lead-in attention-gaining sound and the stateful transition of playout including volume settings.


## Limitations

At the moment, there are several general limitations, however if you are looking to store audio files on your ESP32 flash memory, it would generally make sense to store it in 8-bit mono if you have one speaker. And if you have 2 speakers on two DAC pins, you could easily add support for 2-channel audio going directly to two outputs. This is a long way of saying multi-channel down-mixing isn't supported.

* Original Microsoft Linear PCM (type=1) only. No compression.
* No multichannel downmixing
* 16-bit to 8-bit isn't implemented but is relateively easy to do in the timer ISR
* AudioFilePlayer utilizes several *static* items which makes it important to only instantiate *one* of these classes. It should probably be handled more elegantly as a factory / singleton to be a bit more elegant.
* There is a desire to make this into a PlatformIO library and make it part of the registry. This will come in time along with breaking up 'main.cpp' into separate example files for how the library can be used.

## Build environment

The build / development environment assumed at the moment is using MS Visual Code with PlatformIO plugin. The platform.ini file should allow compilation for native or an ESP32 "Devkit" device as is. 

The intention is to make this into a library which can be discovered and used from the PlatformIO library 'search' functions.

## Class Descriptions

Please note there are Doxygen docs comments in the code and a Doxyfile in the root. The output of the doxygen run is also found in ./docs/index.html for those not familiar with using the doxygen tool.

### WaveFileBufferReader & WaveFile<filesystem>Reader

WaveFile<filesystem>Reader - Each of these classes simply implement open/read/seek/close for the <filesystem> specifically.

*WaveFileBufferReader* is the pure virtual base class which is the workhorse. It is responsible for processing the WAV header and settings of the file. It also reads the file in a task-based manner to constantly keep the buffer 'fed'. It also offers up access and modifiers to the read buffer pointer for use by an external class such as *AudioFilePlayer*.

### AudioFilePlayer

This is the first integration layer which manages loading of wave files, timer ISR rates, and actual DAC writing of the WAV buffer. In the case of the native build, the task simulates the reading of the buffer to allow testing of the mechanics natively. This layer also handles the re-calculation of DAC PCM values based upon a table lookup of volume values. 

The volume feature works quite well and yet is quite simply a linear dividing of the values based upon them being signed values from -127 to 128. If this isn't clear, the unsigned values are between 0 and 255 which represent 0 volts and 3.3 volts respectively. Effectively, this means that the "zero point" of the speaker itself is when the unsigned values are 128. All lower values are 'negative' speaker movement while the values above 128 are 'positive' speaker movements. So, the volume isn't a matter of multiplying the unsigned value by a 'percentage'. By shifting the unsigned value down by 127, we make 'zero' for the speaker and 'zero' for the value make sense.

### AudioPlaylistManager

Final integration layer is found here. The primary purpose of this class is to manage the loading and playout of a file. In order to do this, it reads all the files available and allows the user of the class to set an intro sound as well as a the primary sound to play (or random sound). Volume setting is possible as well as having the class manage turning on and off of the amplifier to save power and avoid any static crackle while *not* playing a file.

### main.cpp

Both native and esp32 (setup/loop) builds are supported. Additionally, this file contains several ways of instantiating or testing the base classes in doActions() and playlistAction() and wavefileTest().