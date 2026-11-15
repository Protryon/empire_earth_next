// C:\EEX\Empire Earth\WONAPI\WONCommon\FileReader.cpp
// FileReader 0x6a0ce0..0x6a1e05: binary reads from a file or a memory buffer (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <stdio.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {

// The state and every read are ee_won's (rust/crates/ee_won/src/file.rs, on the C runtime's stdio as before); the
// string reads below stay loops over ReadByte / ReadShort.
void FileReader::Check(int inStatus)
{
    if (inStatus != 0)
        throw FileReaderException(ee_won_file_status_text(inStatus));
}

// Resets the reader to no file and no buffer
void FileReader::Init() { ee_won_file_reader_init(&this->mCore); }

FileReader::FileReader() { this->Init(); }
// Creates a reader and opens the given file in binary mode
FileReader::FileReader(const char* inPath) { this->Init(); this->Open(inPath); }
// Destroys the reader, closing its file (an owned buffer is freed too)
FileReader::~FileReader() { ee_won_file_reader_close(&this->mCore); }
// Opens the given file for binary reading; returns false for an empty path or a failed open
bool FileReader::Open(const char* inPath) { return ee_won_file_reader_open(&this->mCore, inPath); }
// Opens the given file for text reading; returns false for an empty path or a failed open
bool FileReader::OpenText(const char* inPath) { return ee_won_file_reader_open_text(&this->mCore, inPath); }
// Switches the reader to the given memory buffer (not owned)
void FileReader::Open(const void* inData, unsigned long inLength)
{
    ee_won_file_reader_open_memory(&this->mCore, inData, inLength);
}
// Releases the buffer (freeing it when owned) and closes the file
void FileReader::Close() { ee_won_file_reader_close(&this->mCore); }

// Returns the current read position in the buffer or file
unsigned long FileReader::pos() { return ee_won_file_reader_pos(&this->mCore); }
// Returns the data length, measuring the file once by seeking to its end
unsigned long FileReader::length()
{
    int32_t aStatus = 0;
    unsigned long aLength = ee_won_file_reader_length(&this->mCore, &aStatus);
    this->Check(aStatus);
    return aLength;
}
// Moves the read position; throws FileReaderException past the end of a buffer or on a failed seek
void FileReader::SetPos(unsigned long inPos) { this->Check(ee_won_file_reader_set_pos(&this->mCore, inPos)); }
// Reads up to the given number of bytes; returns the count read (throws with no source)
int FileReader::ReadMaxBytes(void* outData, int inMaxBytes)
{
    int32_t aStatus = 0;
    int aCount = ee_won_file_reader_read_max(&this->mCore, outData, inMaxBytes, &aStatus);
    this->Check(aStatus);
    return aCount;
}
// Reads exactly the given number of bytes; throws FileReaderException when the data runs out
void FileReader::ReadBytes(void* outData, unsigned long inLength)
{
    this->Check(ee_won_file_reader_read_bytes(&this->mCore, outData, inLength));
}
// Reads one byte; returns -1 at the end of the data
int FileReader::TryReadByte() { return ee_won_file_reader_try_read_byte(&this->mCore); }
// Skips the given number of bytes; throws FileReaderException past the end
void FileReader::SkipBytes(unsigned long inLength) { this->Check(ee_won_file_reader_skip(&this->mCore, inLength)); }
// Loads the open file into an owned memory buffer, keeping the read position
void FileReader::ReadIntoMemory() { this->Check(ee_won_file_reader_read_into_memory(&this->mCore)); }

// Reads one byte
unsigned char FileReader::ReadByte() { unsigned char value; this->ReadBytes(&value, 1); return value; }

// Reads a 16-bit value
unsigned short FileReader::ReadShort() { unsigned short value; this->ReadBytes(&value, 2); return value; }

// Reads a 32-bit value
int32_t FileReader::ReadLong() { int32_t value; this->ReadBytes(&value, 4); return value; }
// Reads a length field of 1, 2 or 4 bytes; returns 0 for any other field size
int FileReader::ReadLength(int inFieldSize)
{
    int aValue = 0;
    this->Check(ee_won_file_reader_read_length(&this->mCore, inFieldSize, &aValue));
    return aValue;
}
// Reads the given number of characters into the string
void FileReader::ReadRawString(std::string& outString, int inNumChars)
{
    outString.erase();
    outString.reserve(inNumChars);
    for (int i = 0; i < inNumChars; ++i)
        outString += (char)this->ReadByte();
}
// Reads a length-prefixed string with the given length field size
void FileReader::ReadString(std::string& outString, unsigned char inFieldSize)
{
    this->ReadRawString(outString, this->ReadLength(inFieldSize));
}
// Moves the read position back to the start
void FileReader::Rewind() { ee_won_file_reader_rewind(&this->mCore); }

// Returns true at the end of the data or when nothing is open
bool FileReader::EndOfFile() { return ee_won_file_reader_end_of_file(&this->mCore); }
}

namespace WONAPI {
// No original address identified: WON SDK memory-reader constructor
FileReader::FileReader(const void *theMemDataP, unsigned long theMemDataLen)
{
	this->Init();
	this->Open(theMemDataP,theMemDataLen);
}
}
