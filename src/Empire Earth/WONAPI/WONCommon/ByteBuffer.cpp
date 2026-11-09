// ByteBuffer and OffsetByteBuffer: ref-counted owned byte arrays
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
// FUNCTION: WON 0x006a0a20
// Creates a buffer holding a copy of the string including its terminator; NULL gives an empty buffer
ByteBuffer::ByteBuffer(const char* text)
{
    if(text==NULL) {
        this->mData=NULL;
        this->mDataLen=0;
    } else {
        this->mDataLen=static_cast<unsigned long>(strlen(text))+1;
        this->mData=new char[this->mDataLen];
        memcpy(this->mData,text,this->mDataLen);
    }
}
// Copying zero bytes still allocates new char[0]
// FUNCTION: WON 0x006a0ac0
// Creates a buffer that adopts the given bytes or holds a copy of them
ByteBuffer::ByteBuffer(const void* data,unsigned long length,bool takeOwnership)
{
    if(takeOwnership) {
        this->mData=const_cast<char*>(static_cast<const char*>(data));
        this->mDataLen=length;
    } else {
        this->mData=new char[length];
        this->mDataLen=length;
        if(length>0)
            memcpy(this->mData,data,length);
    }
}
// FUNCTION: WON 0x006a0b70
// Destroys the buffer, freeing its bytes
ByteBuffer::~ByteBuffer()
{
    delete [] this->mData;
}
// 0x6a0bc0 (emitted from the list below): creates a view of allocation+offset that remembers the allocation
OffsetByteBuffer::OffsetByteBuffer(char* allocation,unsigned long offset,unsigned long length)
    :ByteBuffer(allocation+offset,length,true),mAllocation(allocation)
{
}
// FUNCTION: WON 0x006a0c10
// Destroys the view, freeing the whole underlying allocation instead of the view's bytes
OffsetByteBuffer::~OffsetByteBuffer()
{
    delete [] this->mAllocation;
    this->mData=NULL;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a0a00
// (public: virtual void * __thiscall WONAPI::ByteBuffer::`scalar deleting destructor'(unsigned int)    [??_GByteBuffer@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006a0bc0
// (public: __thiscall WONAPI::OffsetByteBuffer::OffsetByteBuffer(char *,unsigned long,unsigned long)    [??0OffsetByteBuffer@WONAPI@@QAE@PADKK@Z])
// TEMPLATE: WON 0x006a0bf0
// (public: virtual void * __thiscall WONAPI::OffsetByteBuffer::`scalar deleting destructor'(unsigned int)    [??_GOffsetByteBuffer@WONAPI@@UAEPAXI@Z])
