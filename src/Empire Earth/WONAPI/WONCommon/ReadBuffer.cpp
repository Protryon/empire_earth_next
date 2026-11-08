// ReadBuffer: sequential reads from a byte buffer, throwing ReadBufferException at the end
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
// FUNCTION: WON 0x006a39f0
// Creates a reader over the given bytes
ReadBuffer::ReadBuffer(const void* data,unsigned long length)
{ this->SetData(data,length); }
// FUNCTION: WON 0x006a3a10
// Points the reader at the given bytes and rewinds it
void ReadBuffer::SetData(const void* data,unsigned long length)
{
    this->mData=static_cast<const char*>(data);
    this->mLength=length;
    this->Rewind();
}
// FUNCTION: WON 0x006a41c0
// Moves the read position back to the start
void ReadBuffer::Rewind() {this->mPos=0;}
// FUNCTION: WON 0x006a3a30
// Returns a pointer to the next bytes and skips them; throws ReadBufferException past the end
const void* ReadBuffer::ReadBytes(unsigned long amount)
{
    if(this->length()-this->pos()<amount)
        throw ReadBufferException("Attempt to read past end of buffer.");
    const void* result=static_cast<const char*>(this->data())+this->mPos;
    this->mPos+=amount;
    return result;
}
// FUNCTION: WON 0x006a3b40
// (the implicit destructor of ReadBufferException is emitted here, after the throw in ReadBytes)
// FUNCTION: WON 0x006a3d30
// Reads a byte as a bool
bool ReadBuffer::ReadBool() {return this->ReadByte()!=0;}
// FUNCTION: WON 0x006a3d40
// Reads one byte
unsigned char ReadBuffer::ReadByte() {return *static_cast<const unsigned char*>(this->ReadBytes(1));}
// FUNCTION: WON 0x006a3d50
// Reads a 16-bit value
short ReadBuffer::ReadShort() {return *static_cast<const short*>(this->ReadBytes(2));}
// FUNCTION: WON 0x006a3d60
// Reads a 32-bit value
long ReadBuffer::ReadLong() {return *static_cast<const long*>(this->ReadBytes(4));}
// FUNCTION: WON 0x006a3d70
// Reads a sign-extended 1, 2 or 4 byte length field; returns 0 for any other size
unsigned long ReadBuffer::ReadLength(unsigned char fieldSize)
{
    switch(fieldSize) {
    case 1:return (signed char)this->ReadByte();
    case 2:return this->ReadShort();
    case 4:return this->ReadLong();
    default:return 0;
    }
}
// FUNCTION: WON 0x006a3db0
// Reads the given number of characters into the string
void ReadBuffer::ReadRawString(std::string& text,unsigned long length)
{
    text.erase();
    const char* data=static_cast<const char*>(this->ReadBytes(length));
    text.append(data,length);
}
// FUNCTION: WON 0x006a3f40
// Reads a length-prefixed string with the given length field size
void ReadBuffer::ReadString(std::string& text,unsigned char fieldSize)
{this->ReadRawString(text,this->ReadLength(fieldSize));}
// FUNCTION: WON 0x006a3f60
// Reads the given number of 16-bit characters into the wide string
void ReadBuffer::ReadRawWString(std::wstring& text,unsigned long length)
{
    text.erase();
    const wchar_t* data=static_cast<const wchar_t*>(this->ReadBytes(length*2));
    text.append(data,length);
}
// The field size is passed as int
// FUNCTION: WON 0x006a40f0
// Reads a length-prefixed wide string with the given length field size
void ReadBuffer::ReadWString(std::wstring& text,int fieldSize)
{this->ReadRawWString(text,this->ReadLength((unsigned char)fieldSize));}
// FUNCTION: WON 0x006a4110
// Reads a length-prefixed block into a new byte buffer
ByteBufferPtr ReadBuffer::ReadBuf(unsigned char fieldSize)
{
    unsigned long length=this->ReadLength(fieldSize);
    return new ByteBuffer(this->ReadBytes(length),length,false);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a3b20
// (public: virtual void * __thiscall WONAPI::ReadBufferException::`scalar deleting destructor'(unsigned int)    [??_GReadBufferException@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x006a40c0
// (_wmemcpy    [_wmemcpy])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a3bb0
// (public: __thiscall WONAPI::ReadBufferException::ReadBufferException(class WONAPI::ReadBufferException const &)    [??0ReadBufferException@WONAPI@@QAE@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a1d40
// (_wmemmove)
