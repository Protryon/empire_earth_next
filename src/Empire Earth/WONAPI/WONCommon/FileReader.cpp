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

// FUNCTION: WON 0x006a0ce0
// Resets the reader to no file and no buffer
void FileReader::Init()
{
    this->mFile = NULL;
    this->mBuffer = NULL;
    this->mLength = 0;
    this->mPosition = 0;
    this->mOwnBuffer = false;
    this->mMemoryMode = false;
}

FileReader::FileReader() { this->Init(); }
// FUNCTION: WON 0x006a0d10
// Creates a reader and opens the given file in binary mode
FileReader::FileReader(const char* inPath) { this->Init(); this->Open(inPath); }
// FUNCTION: WON 0x006a0d30
// Destroys the reader, closing its file
FileReader::~FileReader()
{
    if (this->mFile != NULL)
        fclose(this->mFile);
}
// FUNCTION: WON 0x006a0d40
// Opens the given file for binary reading; returns false for an empty path or a failed open
bool FileReader::Open(const char* inPath)
{
    this->Close();
    if (inPath[0] == 0) return false;
    this->mFile = fopen(inPath, "rb");
    this->mMemoryMode = false;
    return this->mFile != NULL;
}
// FUNCTION: WON 0x006a0d80
// Opens the given file for text reading; returns false for an empty path or a failed open
bool FileReader::OpenText(const char* inPath)
{
    this->Close();
    if (inPath[0] == 0) return false;
    this->mFile = fopen(inPath, "r");
    this->mMemoryMode = false;
    return this->mFile != NULL;
}
// FUNCTION: WON 0x006a0dc0
// Switches the reader to the given memory buffer (not owned)
void FileReader::Open(const void* inData, unsigned long inLength)
{
    this->Close();
    this->mBuffer = static_cast<const char*>(inData);
    this->mLength = inLength;
    this->mMemoryMode = true;
}
// FUNCTION: WON 0x006a0de0
// Releases the buffer (freeing it when owned) and closes the file
void FileReader::Close()
{
    if (this->mBuffer != NULL) {
        if (this->mOwnBuffer) delete[] const_cast<char*>(this->mBuffer);
        this->mBuffer = NULL;
        this->mLength = 0;
        this->mPosition = 0;
        this->mOwnBuffer = false;
    }
    if (this->mFile != NULL) {
        fclose(this->mFile);
        this->mFile = NULL;
    }
    this->mMemoryMode = false;
}

// FUNCTION: WON 0x006a0e20
// Returns the current read position in the buffer or file
unsigned long FileReader::pos()
{
    if (this->mBuffer != NULL) return this->mPosition;
    if (this->mFile != NULL) return ftell(this->mFile);
    return 0;
}
// FUNCTION: WON 0x006a0e40
// Returns the data length, measuring the file once by seeking to its end
unsigned long FileReader::length()
{
    if (this->mMemoryMode || this->mFile == NULL)
        return this->mLength;
    unsigned long oldPos = this->pos();
    fseek(this->mFile, 0, SEEK_END);
    this->mLength = this->pos();
    this->mMemoryMode = true;
    this->SetPos(oldPos);
    return this->mLength;
}
// FUNCTION: WON 0x006a0e90
// Moves the read position; throws FileReaderException past the end of a buffer or on a failed seek
void FileReader::SetPos(unsigned long inPos)
{
    if (this->mBuffer != NULL) {
        if (inPos >= this->mLength)
            throw FileReaderException("Attempt to setpos past end of mem buffer.");
        this->mPosition = inPos;
    } else if (this->mFile != NULL && fseek(this->mFile, inPos, SEEK_SET) != 0) {
        throw FileReaderException("Attempt to setpos past end of file.");
    }
}
// FUNCTION: WON 0x006a12d0
// Reads up to the given number of bytes; returns the count read (throws with no source)
int FileReader::ReadMaxBytes(void* outData, int inMaxBytes)
{
    if (this->mBuffer != NULL) {
        int count = this->mLength - this->mPosition;
        if (count > inMaxBytes) count = inMaxBytes;
        memcpy(outData, this->mBuffer + this->mPosition, count);
        this->mPosition += count;
        return count;
    }
    if (this->mFile == NULL) throw FileReaderException("NULL File.");
    return fread(outData, 1, inMaxBytes, this->mFile);
}
// FUNCTION: WON 0x006a1460
// Reads exactly the given number of bytes; throws FileReaderException when the data runs out
void FileReader::ReadBytes(void* outData, unsigned long inLength)
{
    if (this->mBuffer != NULL) {
        if (this->mPosition + inLength > this->mLength)
            throw FileReaderException("Attempt to read past end of mem buffer.");
        memcpy(outData, this->mBuffer + this->mPosition, inLength);
        this->mPosition += inLength;
    } else {
        if (this->mFile == NULL) throw FileReaderException("NULL File.");
        if (fread(outData, 1, inLength, this->mFile) != inLength)
            throw FileReaderException("Attempt to read past end of file.");
    }
}
// FUNCTION: WON 0x006a1780
// Reads one byte; returns -1 at the end of the data
int FileReader::TryReadByte()
{
    if (this->mBuffer != NULL) {
        if (this->mPosition < this->mLength)
            return (unsigned char)this->mBuffer[this->mPosition++];
    } else if (this->mFile != NULL) {
        return fgetc(this->mFile);
    }
    return -1;
}
// FUNCTION: WON 0x006a17c0
// Skips the given number of bytes; throws FileReaderException past the end
void FileReader::SkipBytes(unsigned long inLength)
{
    if (this->mBuffer != NULL) {
        if (this->mPosition + inLength > this->mLength)
            throw FileReaderException("Attempt to read past end of mem buffer.");
        this->mPosition += inLength;
    } else {
        if (this->mFile == NULL) throw FileReaderException("NULL File.");
        if (fseek(this->mFile, inLength, SEEK_CUR) != 0)
            throw FileReaderException("Attempt to read past end of file.");
    }
}
// FUNCTION: WON 0x006a1ac0
// Loads the open file into an owned memory buffer, keeping the read position
void FileReader::ReadIntoMemory()
{
    if (this->mBuffer != NULL || this->mFile == NULL)
        return;
    unsigned long size = this->length();
    if (size <= 0)
        return;
    unsigned long oldPos = this->pos();
    this->SetPos(0);
    char* data = new char[size];
    int count = this->ReadMaxBytes(data, size);
    this->Open(data, count);
    this->mOwnBuffer = true;
    this->SetPos(oldPos);
}

// FUNCTION: WON 0x006a1b20
// Reads one byte
unsigned char FileReader::ReadByte() { unsigned char value; this->ReadBytes(&value, 1); return value; }

// FUNCTION: WON 0x006a1b40
// Reads a 16-bit value
unsigned short FileReader::ReadShort() { unsigned short value; this->ReadBytes(&value, 2); return value; }

// FUNCTION: WON 0x006a1b60
// Reads a 32-bit value
long FileReader::ReadLong() { long value; this->ReadBytes(&value, 4); return value; }
// FUNCTION: WON 0x006a1b80
// Reads a length field of 1, 2 or 4 bytes; returns 0 for any other field size
int FileReader::ReadLength(int inFieldSize)
{
    switch (inFieldSize) {
    case 1: return this->ReadByte();
    case 2: return this->ReadShort();
    case 4: return this->ReadLong();
    default: return 0;
    }
}
// FUNCTION: WON 0x006a1bc0
// Reads the given number of characters into the string
void FileReader::ReadRawString(std::string& outString, int inNumChars)
{
    outString.erase();
    outString.reserve(inNumChars);
    for (int i = 0; i < inNumChars; ++i)
        outString += (char)this->ReadByte();
}
// FUNCTION: WON 0x006a1d90
// Reads a length-prefixed string with the given length field size
void FileReader::ReadString(std::string& outString, unsigned char inFieldSize)
{
    this->ReadRawString(outString, this->ReadLength(inFieldSize));
}
// FUNCTION: WON 0x006a1dc0
// Moves the read position back to the start
void FileReader::Rewind()
{
    if (this->mBuffer != NULL) this->mPosition = 0;
    else if (this->mFile != NULL) rewind(this->mFile);
}

// FUNCTION: WON 0x006a1de0
// Returns true at the end of the data or when nothing is open
bool FileReader::EndOfFile()
{
    if (this->mBuffer != NULL) return this->mPosition >= this->mLength;
    if (this->mFile != NULL) return feof(this->mFile) ? true : false;
    return true;
}
}

namespace WONAPI {
// No original address identified: WON SDK memory-reader constructor
FileReader::FileReader(const void *theMemDataP, unsigned long theMemDataLen)
{
	this->Init();
	this->Open(theMemDataP,theMemDataLen);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a0d00
// (public: __thiscall WONAPI::FileReader::FileReader(void)    [??0FileReader@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006a10b0
// (public: virtual __thiscall std::exception::~exception(void)    [??1exception@std@@UAE@XZ])
// TEMPLATE: WON 0x006a10c0
// (public: virtual void * __thiscall WONAPI::FileReaderException::`scalar deleting destructor'(unsigned int)    [??_GFileReaderException@WONAPI@@UAEPAXI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a10e0
// (public: virtual __thiscall WONAPI::FileReaderException::~FileReaderException(void)    [??1FileReaderException@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a1150
// (public: __thiscall WONAPI::FileReaderException::FileReaderException(class WONAPI::FileReaderException const &)    [??0FileReaderException@WONAPI@@QAE@)
