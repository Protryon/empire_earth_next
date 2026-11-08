// WriteBuffer (0x18 bytes): a growable message buffer with room for a length prefix
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
// FUNCTION: WON 0x006a8140
// Creates an empty buffer with the given length-prefix size and whether the length counts the prefix
WriteBuffer::WriteBuffer(unsigned char lengthSize,bool includesLength)
{
    this->mAllocation=NULL;
    this->mData=NULL;
    this->mLengthFieldSize=lengthSize;
    this->mLengthIncludesField=includesLength;
    this->Reset();
}

// FUNCTION: WON 0x006a8190
// Destroys the buffer, freeing its allocation
WriteBuffer::~WriteBuffer()
{
    delete [] this->mAllocation;
}
// FUNCTION: WON 0x006a8110
// Frees the allocation and empties the buffer
void WriteBuffer::Reset()
{
    delete [] this->mAllocation;
    this->mAllocation=NULL;
    this->mData=NULL;
    this->mLength=0;
    this->mCapacity=0;
}
// FUNCTION: WON 0x006a8130
// Forgets the allocation without freeing it (ownership passed on) and empties the buffer
void WriteBuffer::Release()
{
    this->mAllocation=NULL;
    this->mData=NULL;
    this->Reset();
}
// The first capacity check is unsigned; the growth loop compares signed 32-bit values
// FUNCTION: WON 0x006a81b0
// Grows the capacity (from 32, doubling) so the given number of bytes fits
void WriteBuffer::CheckWrite(unsigned long length)
{
    unsigned long needed=this->mLength+length;
    if(!(needed>this->mCapacity))
        return;
    long capacity=static_cast<long>(this->mCapacity);
    if(capacity<32)
        capacity=32;
    while(capacity<static_cast<long>(needed))
        capacity <<= 1;
    this->Reserve(static_cast<unsigned long>(capacity));
}
// FUNCTION: WON 0x006a81e0
// Reallocates to at least the given capacity plus 6 prefix bytes, keeping the written data
void WriteBuffer::Reserve(unsigned long capacity)
{
    if(this->mCapacity>=capacity)
        return;
    char* allocation=new char[capacity+6];
    char* data=allocation+6;
    memcpy(data,this->mData,this->mLength);
    delete [] this->mAllocation;
    this->mAllocation=allocation;
    this->mData=data;
    this->mCapacity=capacity;
}
// FUNCTION: WON 0x006a8240
// Sets the written length, growing the buffer as needed
void WriteBuffer::SetSize(unsigned long size)
{
    this->Reserve(size);
    this->mLength=size;
}
// FUNCTION: WON 0x006a8260
// Reserves the given number of bytes at the end without writing them
void WriteBuffer::SkipBytes(unsigned long length)
{
    this->CheckWrite(length);
    this->mLength+=length;
}
// No shortcut for zero bytes; the source must not alias the buffer
// FUNCTION: WON 0x006a8280
// Appends the given bytes
void WriteBuffer::AppendBytes(const void* bytes,unsigned long length)
{
    this->CheckWrite(length);
    memcpy(this->mData+this->mLength,bytes,length);
    this->mLength+=length;
}
// FUNCTION: WON 0x006a82c0
// Appends one byte
void WriteBuffer::AppendByte(unsigned char value) { this->AppendBytes(&value,1); }
// FUNCTION: WON 0x006a82d0
// Appends a bool as one byte (0 or 1)
void WriteBuffer::AppendBool(bool value)
{
    unsigned char byte=value?1:0;
    this->AppendBytes(&byte,1);
}
// FUNCTION: WON 0x006a82f0
// Appends a 16-bit value
void WriteBuffer::AppendShort(short value) { this->AppendBytes(&value,2); }
// FUNCTION: WON 0x006a8300
// Appends a 32-bit value
void WriteBuffer::AppendLong(long value) { this->AppendBytes(&value,4); }
// FUNCTION: WON 0x006a8310
// Appends a length as a 1, 2 or 4 byte field; appends nothing for any other size
void WriteBuffer::AppendLength(unsigned long length,unsigned char size)
{
    switch(size) {
    case 1: this->AppendByte(static_cast<unsigned char>(length)); break;
    case 2: this->AppendShort(static_cast<short>(length)); break;
    case 4: this->AppendLong(static_cast<long>(length)); break;
    }
}
// FUNCTION: WON 0x006a8350
// Appends a string with a length field of the given size
void WriteBuffer::AppendString(const std::string& text,unsigned char size)
{
    this->AppendLength(text.length(),size);
    this->AppendBytes(text.c_str(),text.length());
}
// FUNCTION: WON 0x006a8390
// Appends a wide string: a character-count field, then the raw 16-bit characters
void WriteBuffer::AppendWString(const std::wstring& text,int size)
{
    this->AppendLength(text.length(),static_cast<unsigned char>(size));
    this->AppendBytes(text.c_str(),text.length()*2);
}
// FUNCTION: WON 0x006a83d0
// Appends a byte buffer with a length field; a NULL buffer writes only a zero length
void WriteBuffer::AppendBuffer(const ByteBuffer* buffer,unsigned char size)
{
    unsigned long length=buffer!=NULL?buffer->length():0;
    this->AppendLength(length,size);
    if(buffer!=NULL && length>0)
        this->AppendBytes(buffer->data(),length);
}
// FUNCTION: WON 0x006a8410
// Overwrites bytes at the given position (no bounds check)
void WriteBuffer::SetBytes(unsigned long position,const void* bytes,unsigned long length)
{
    memcpy(this->mData+position,bytes,length);
}
// FUNCTION: WON 0x006a8440
// Overwrites one byte at the given position
void WriteBuffer::SetByte(unsigned long position,unsigned char value) { this->SetBytes(position,&value,1); }
// FUNCTION: WON 0x006a8460
// Overwrites a 16-bit value at the given position
void WriteBuffer::SetShort(unsigned long position,short value) { this->SetBytes(position,&value,2); }
// With release the buffer hands its allocation to the result; otherwise the result is a copy
// FUNCTION: WON 0x006a8480
// Writes the length prefix in front of the data (0xffff + 32-bit for long 2-byte lengths); returns it as a byte buffer
ConstSmartPtr<ByteBuffer> WriteBuffer::ToByteBuffer(bool release)
{
    unsigned long value=this->mLength;
    unsigned long length=this->mLength;
    char* data=this->mData;
    switch(this->mLengthFieldSize) {
    case 1:
        if(this->mLengthIncludesField) ++value;
        --data;
        ++length;
        *data=static_cast<char>(value);
        break;
    case 2:
        if(value<0xffff) {
            if(this->mLengthIncludesField) value+=2;
            data=this->mData-2;
            length+=2;
            unsigned short word=static_cast<unsigned short>(value);
            memcpy(data,&word,2);
        } else {
            data=this->mData-6;
            length+=6;
            unsigned short marker=0xffff;
            memcpy(data,&marker,2);
            if(this->mLengthIncludesField) value+=4;
            memcpy(data+2,&value,4);
        }
        break;
    case 4:
        if(this->mLengthIncludesField) value+=4;
        data=this->mData-4;
        length+=4;
        memcpy(data,&value,4);
        break;
    }
    if(release) {
        char* allocation=this->mAllocation;
        this->Release();
        return new OffsetByteBuffer(allocation,static_cast<unsigned long>(data-allocation),length);
    }
    return new ByteBuffer(data,length,false);
}
}

namespace WONAPI {
// No original address identified: WON SDK chunk growth
void WriteBuffer::ReserveChunk(unsigned long theNumBytesToReserve, unsigned long theMaxSize)
{
	int aTotalLen = theNumBytesToReserve;
	if(!(aTotalLen>this->mCapacity))
		return;
	int aSize = this->mCapacity;
	if(aSize<32)
		aSize = 32;

	while(aSize<aTotalLen)
		aSize<<=1;

	if(theMaxSize>=theNumBytesToReserve && aSize>theMaxSize)
		aSize = theMaxSize;

	this->Reserve(aSize);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a8170
// (public: virtual void * __thiscall WONAPI::WriteBuffer::`scalar deleting destructor'(unsigned int)    [??_GWriteBuffer@WONAPI@@UAEPAXI@Z])
