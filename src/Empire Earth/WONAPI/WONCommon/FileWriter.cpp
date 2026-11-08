// C:\EEX\Empire Earth\WONAPI\WONCommon\FileWriter.cpp
// FileWriter 0x6a1e10..0x6a26a5: binary writes to a file (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {

FileWriter::FileWriter() : mFile(NULL) {}

FileWriter::~FileWriter() { this->Close(); }
// FUNCTION: WON 0x006a1e30
// Opens the given file for binary writing, clearing its read-only flag and retrying when asked
bool FileWriter::Open(const char* inPath, bool inOpenIfReadOnly)
{
    this->Close();
    if (inPath[0] == 0)
        return false;
    this->mFile = fopen(inPath, "wb");
    if (this->mFile == NULL && inOpenIfReadOnly) {
        WONFile file(inPath);
        file.SetReadOnly(false);
        this->mFile = fopen(inPath, "wb");
    }
    return this->mFile != NULL;
}
// FUNCTION: WON 0x006a2060
// Closes the file
void FileWriter::Close()
{
    if (this->mFile != NULL) {
        fclose(this->mFile);
        this->mFile = NULL;
    }
}

// FUNCTION: WON 0x006a2080
// Returns the current write position, 0 with no file open
unsigned long FileWriter::pos()
{
    return this->mFile != NULL ? ftell(this->mFile) : 0;
}
// FUNCTION: WON 0x006a20a0
// Moves the write position; throws FileWriterException when the seek fails
void FileWriter::SetPos(unsigned long inPos)
{
    if (this->mFile != NULL && fseek(this->mFile, inPos, SEEK_SET) != 0)
        throw FileWriterException("Attempt to setpos past end of file.");
}
// FUNCTION: WON 0x006a2400
// Writes the given bytes; throws FileWriterException with no file or on a short write
void FileWriter::WriteBytes(const void* inData, unsigned long inLength)
{
    if (this->mFile == NULL)
        throw FileWriterException("NULL File.");
    if (fwrite(inData, 1, inLength, this->mFile) != inLength)
        throw FileWriterException("fwrite failed (disk full)");
}

// FUNCTION: WON 0x006a25f0
// Writes one byte
void FileWriter::WriteByte(unsigned char inValue) { this->WriteBytes(&inValue, 1); }

// FUNCTION: WON 0x006a2600
// Writes a 16-bit value
void FileWriter::WriteShort(unsigned short inValue) { this->WriteBytes(&inValue, 2); }

// FUNCTION: WON 0x006a2610
// Writes a 32-bit value
void FileWriter::WriteLong(long inValue) { this->WriteBytes(&inValue, 4); }
// FUNCTION: WON 0x006a2620
// Writes a length as a 1, 2 or 4 byte field; writes nothing for any other size
void FileWriter::WriteLength(unsigned long inLength, unsigned char inFieldSize)
{
    switch (inFieldSize) {
    case 1: this->WriteByte((unsigned char)inLength); break;
    case 2: this->WriteShort((unsigned short)inLength); break;
    case 4: this->WriteLong((long)inLength); break;
    }
}

// FUNCTION: WON 0x006a2660
// Writes the string's characters without a length or terminator
void FileWriter::WriteRawString(const std::string& inString)
{
    this->WriteBytes(inString.c_str(), inString.length());
}
// FUNCTION: WON 0x006a2680
// Writes a length-prefixed string with the given length field size
void FileWriter::WriteString(const std::string& inString, unsigned char inFieldSize)
{
    this->WriteLength(inString.length(), inFieldSize);
    this->WriteRawString(inString);
}
}

namespace WONAPI {
// WON SDK byte-order helper
inline bool IsLittleEndian()
{
	short aTest = 1;
	return ((*reinterpret_cast<char*>(&aTest))&1)?true:false;
}

// No original address identified: WON SDK wide string serialization
void FileWriter::WriteRawWString(const std::wstring &theWStr)
{
	if(IsLittleEndian() && sizeof(wchar_t)==2)
		this->WriteBytes(theWStr.data(),theWStr.length()*2);
	else
	{
		for(int i=0; i<theWStr.length(); i++)
			this->WriteShort(theWStr[i]);
	}
}

void FileWriter::WriteWString(const std::wstring &theWStr, unsigned char theLengthFieldSize)
{
	this->WriteLength(theWStr.length(),theLengthFieldSize);
	this->WriteRawWString(theWStr);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a1e20
// (public: __thiscall WONAPI::FileWriter::~FileWriter(void)    [??1FileWriter@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006a21f0
// (public: virtual void * __thiscall WONAPI::FileWriterException::`scalar deleting destructor'(unsigned int)    [??_GFileWriterException@WONAPI@@UAEPAXI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a1e10
// (public: __thiscall WONAPI::FileWriter::FileWriter(void)    [??0FileWriter@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006a2210
// (public: virtual __thiscall WONAPI::FileWriterException::~FileWriterException(void)    [??1FileWriterException@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a2280
// (public: __thiscall WONAPI::FileWriterException::FileWriterException(class WONAPI::FileWriterException const &)    [??0FileWriterException@WONAPI@@QAE@)
