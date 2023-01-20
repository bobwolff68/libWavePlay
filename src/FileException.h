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

#include <exception>
#include <string>

/*! @class FileException
 *  @brief helper for supporting try/catch in file-based exceptions.
 *  @details Allows tracking partially completed file operations and being able to query
 *           how many bytes were completed at the time of the exception. This is the typical
 *           situation where files are read in 'blocks' or 'chunks' of a fixed size and at
 *           the end of the file, there is a 'remainder'.
*/
class FileException : public std::exception {
    private:
    std::string message;
    size_t partComplete;
    bool bIsEOF;

    public:
    //! @brief When the caller /throws/, they can give a message and also note how many bytes were
    //!        processed and whether or not EOF was reached. This is very informative for the catch.
    FileException(const char * msg, size_t partialBytes, bool bEOF) : message(msg), partComplete(partialBytes), bIsEOF(bEOF) {};
    char * what () {
        return (char*)message.c_str();
    }
    size_t getPartial() { return partComplete; };
    bool isEOF() { return bIsEOF; };
};
