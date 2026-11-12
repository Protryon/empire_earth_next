// WON byte/read/write buffers (shims over ee_won's buffer module). Requires the LLE/windows headers included
// before it. No GUI/Core/Root header dependency.
#ifndef EE_WONAPI_WONBUFFER_H
#define EE_WONAPI_WONBUFFER_H
#include <exception>
#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <string>
#include "ee/EEWonNet.h"
#include "rust/ee_rust.h"
namespace WONAPI {

// WON's wide text is UTF-16 on the wire and in its files (wchar_t of the original Windows build). wchar_t is UTF-32
// on Linux and macOS, so every byte boundary converts per 16-bit unit, little-endian (characters past U+FFFF as
// surrogate pairs). On Windows each wchar_t is one unit and the bytes are unchanged. The conversions are Rust's
// (rust/crates/ee_won/src/text.rs, the one place WON's text is converted: docs/RUST_PLAN.md D6).

// the number of UTF-16 units (the wire's character count) of a wide string
inline unsigned long WideUtf16Length(const std::wstring& inText)
{
    return static_cast<unsigned long>(ee_won_utf16le_from_wide(inText.data(),sizeof(wchar_t),inText.length(),NULL,0)/2);
}
// the UTF-16 little-endian bytes of a wide string, appended to outBytes
inline void WideAppendUtf16LE(std::string& outBytes, const std::wstring& inText)
{
    size_t at=outBytes.length();
    size_t count=ee_won_utf16le_from_wide(inText.data(),sizeof(wchar_t),inText.length(),NULL,0);
    outBytes.resize(at+count);
    if(count>0)
        ee_won_utf16le_from_wide(inText.data(),sizeof(wchar_t),inText.length(),
                                 reinterpret_cast<uint8_t*>(&outBytes[at]),count);
}
// appends one UTF-16 unit; on UTF-32 systems a low surrogate after a high one joins it into one character
inline void WideAppendUtf16Unit(std::wstring& ioText, uint16_t inUnit)
{
#if WCHAR_MAX > 0xffff
    if(!ioText.empty()) {
        uint32_t joined=ee_won_utf16_join(static_cast<uint32_t>(ioText[ioText.length()-1]),inUnit);
        if(joined!=0) {
            ioText[ioText.length()-1]=static_cast<wchar_t>(joined);
            return;
        }
    }
#endif
    ioText+=static_cast<wchar_t>(inUnit);
}
// a wide string of count UTF-16 little-endian units (surrogate pairs joined on UTF-32 systems)
inline void WideAssignUtf16LE(std::wstring& outText, const void* inBytes, unsigned long inCount)
{
    outText.resize(inCount);
    size_t n=inCount>0 ? ee_won_wide_from_utf16le(static_cast<const uint8_t*>(inBytes),inCount,sizeof(wchar_t),
                                                    &outText[0],inCount) : 0;
    outText.resize(n);
}

// WONCommon's buffers are Rust objects (rust/crates/ee_won/src/buffer.rs: the state, growth, length prefixes,
// bounds and field codings); the classes below are the shims the C++ callers use. ByteBuffer keeps the reference
// count and holds an EeWonByteBuffer; WriteBuffer holds an EeWonWriteBuffer; ReadBuffer embeds its EeWonReadBuffer
// by value (a reader over bytes it does not own) and throws ReadBufferException where Rust reports a read past the
// end. The original units (ByteBuffer.cpp with OffsetByteBuffer, ReadBuffer.cpp, WriteBuffer.cpp) are deleted;
// their answers to tests/won/won_buffer_test.cpp's script are rust/crates/ee_won/src/golden_buffer.txt.

// a reference-counted byte buffer
class ByteBuffer : public RefCount {
public:
    // a copy of the text including its terminator; NULL gives a buffer with no bytes (a NULL data())
    explicit ByteBuffer(const char* inText) : mBytes(ee_won_bytes_text(inText)) {}
    // a copy of the bytes; with inTakeOwnership the caller's new[] array is handed over (and freed here at once)
    ByteBuffer(const void* inData, unsigned long inLength, bool inTakeOwnership)
        : mBytes(ee_won_bytes_copy(static_cast<const uint8_t*>(inData), inLength))
    {
        if (inTakeOwnership)
            delete[] const_cast<char*>(static_cast<const char*>(inData));
    }
    // adopts Rust's bytes (ToByteBuffer)
    explicit ByteBuffer(EeWonByteBuffer* inBytes) : mBytes(inBytes) {}
    virtual ~ByteBuffer() { ee_won_bytes_free(this->mBytes); }
    ByteBuffer(const ByteBuffer&) = delete;
    ByteBuffer& operator=(const ByteBuffer&) = delete;
    const char* data() const { return this ? reinterpret_cast<const char*>(ee_won_bytes_data(this->mBytes)) : NULL; }
    unsigned long length() const { return this ? ee_won_bytes_length(this->mBytes) : 0; }

    EeWonByteBuffer* mBytes;
};

inline AuthLoginCommunityData::AuthLoginCommunityData() {}
typedef ConstSmartPtr<ByteBuffer> ByteBufferPtr;

// ee_won's sinks: bytes into a new ByteBuffer held by *(ByteBufferPtr*)inContext; UTF-16 units into the std::wstring
// at inContext
inline void WonByteBufferSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<ConstSmartPtr<ByteBuffer>*>(inContext)=new ByteBuffer(inBytes,static_cast<unsigned long>(inLength),false);
}
inline void WonWideSink(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring& text=*static_cast<std::wstring*>(inContext);
    text.resize(inCount);
    size_t n=inCount>0 ? ee_won_wide_from_utf16(inUnits,inCount,sizeof(wchar_t),&text[0],inCount) : 0;
    text.resize(n);
}

// RTTI: ReadBufferException : std::exception. Original dtor 0x2a3b40 destroys string +8.
class ReadBufferException : public std::exception {
public:
    ReadBufferException() {}
    ReadBufferException(const std::string& text) {this->mMessage=text;} //2a3a30, throw site, size 24
    std::string mMessage;                   // +0x08; total size 0x18
};


class ReadBuffer {
public:
    ReadBuffer(const void* inData, unsigned long inLength) { this->SetData(inData,inLength); }
    // points the reader at the bytes and rewinds it
    void SetData(const void* inData, unsigned long inLength)
    { ee_won_reader_set(&this->mState,static_cast<const uint8_t*>(inData),inLength); }
    const char* data() const { return reinterpret_cast<const char*>(this->mState.data); }
    unsigned long length() const { return static_cast<unsigned long>(this->mState.length); }
    int pos() { return static_cast<int>(this->mState.pos); }
    void SetPos(int thePos) { this->mState.pos=thePos; }
    int Available() { return static_cast<int>(this->length())-this->pos(); }
    void Rewind() { this->mState.pos=0; }
    // the next bytes, skipped; throws ReadBufferException past the end
    const void* ReadBytes(unsigned long inLength) { return this->Take(inLength); }
    bool ReadBool() { return this->ReadByte()!=0; }
    unsigned char ReadByte() { return *static_cast<const unsigned char*>(this->Take(1)); }
    short ReadShort() { short aValue; memcpy(&aValue,this->Take(2),2); return aValue; }
    int32_t ReadLong() { int32_t aValue; memcpy(&aValue,this->Take(4),4); return aValue; } // 32-bit on every OS
    // a sign-extended 1, 2 or 4-byte length field (32 bits on every OS); 0 for another size
    unsigned long ReadLength(unsigned char inSize)
    {
        uint32_t aLength=0;
        if(!ee_won_reader_length(&this->mState,inSize,&aLength))
            throw ReadBufferException("Attempt to read past end of buffer.");
        return aLength;
    }
    void ReadRawString(std::string& outText, unsigned long inLength)
    {
        outText.erase();
        const char* aData=static_cast<const char*>(this->Take(inLength));
        outText.append(aData,inLength);
    }
    void ReadString(std::string& outText, unsigned char inLengthSize = 2)
    { this->ReadRawString(outText,this->ReadLength(inLengthSize)); }
    // inLength UTF-16 little-endian units (wchar_t is 4 bytes outside Windows; the byte count does not wrap)
    void ReadRawWString(std::wstring& outText, unsigned long inLength)
    {
        outText.erase();
        const void* aData=this->Take(static_cast<uint64_t>(inLength)*2);
        WideAssignUtf16LE(outText,aData,inLength);
    }
    void ReadWString(std::wstring& outText, int inLengthSize)
    { this->ReadRawWString(outText,this->ReadLength(static_cast<unsigned char>(inLengthSize))); }
    // a length-prefixed block as a new byte buffer
    ByteBufferPtr ReadBuf(unsigned char inLengthSize)
    {
        unsigned long aLength=this->ReadLength(inLengthSize);
        return new ByteBuffer(this->Take(aLength),aLength,false);
    }

    EeWonReadBuffer mState;

private:
    const void* Take(uint64_t inLength)
    {
        const uint8_t* aBytes=NULL;
        if(!ee_won_reader_take(&this->mState,inLength,&aBytes))
            throw ReadBufferException("Attempt to read past end of buffer.");
        return aBytes;
    }
};

class WriteBuffer {
public:
    WriteBuffer(unsigned char inLengthFieldSize, bool inLengthIncludesField)
        : mBuffer(ee_won_writer_new(inLengthFieldSize,inLengthIncludesField)) {}
    virtual ~WriteBuffer() { ee_won_writer_free(this->mBuffer); }
    WriteBuffer(const WriteBuffer&) = delete;
    WriteBuffer& operator=(const WriteBuffer&) = delete;
    // the data's start (NULL before the first growth): capacity() bytes may be written there until the next growth
    char* data() { return reinterpret_cast<char*>(ee_won_writer_data(this->mBuffer)); }
    unsigned long length() const { return ee_won_writer_length(this->mBuffer); }
    unsigned long capacity() const { return ee_won_writer_capacity(this->mBuffer); }
    void Reset() { ee_won_writer_reset(this->mBuffer); }
    // empties the buffer (the original forgot an allocation another owner had taken; ToByteBuffer hands it over now)
    void Release() { ee_won_writer_reset(this->mBuffer); }
    void CheckWrite(unsigned long inLength) { ee_won_writer_check_write(this->mBuffer,inLength); }
    void Reserve(unsigned long inNumBytes) { ee_won_writer_reserve(this->mBuffer,inNumBytes); }
    void ReserveChunk(unsigned long theNumBytesToReserve,unsigned long theMaxSize=0)
    { ee_won_writer_reserve_chunk(this->mBuffer,theNumBytesToReserve,theMaxSize); }
    void SetSize(unsigned long inSize) { ee_won_writer_set_size(this->mBuffer,inSize); }
    void SkipBytes(unsigned long inLength) { ee_won_writer_skip(this->mBuffer,inLength); }
    void AppendBytes(const void* inData, unsigned long inLength)
    { ee_won_writer_append(this->mBuffer,static_cast<const uint8_t*>(inData),inLength); }
    void AppendByte(unsigned char inByte) { this->AppendBytes(&inByte,1); }
    void AppendBool(bool inValue) { unsigned char aByte=inValue?1:0; this->AppendBytes(&aByte,1); }
    void AppendShort(short inValue) { this->AppendBytes(&inValue,2); }
    void AppendLong(int32_t inValue) { this->AppendBytes(&inValue,4); } // 32-bit on every OS
    void AppendLength(unsigned long inLength, unsigned char inSize)
    { ee_won_writer_append_length(this->mBuffer,inLength,inSize); }
    void AppendString(const std::string& inText, unsigned char inSize = 2)
    {
        ee_won_writer_append_field(this->mBuffer,reinterpret_cast<const uint8_t*>(inText.data()),
                                   static_cast<unsigned long>(inText.length()),inSize);
    }
    void AppendRawString(const std::string& theStr) { this->AppendString(theStr,0); }
    // a unit count field, then the UTF-16 little-endian units
    void AppendWString(const std::wstring& inText, int inLengthSize)
    {
        std::string aBytes;
        WideAppendUtf16LE(aBytes,inText);
        ee_won_writer_append_utf16le(this->mBuffer,reinterpret_cast<const uint8_t*>(aBytes.data()),
                                     static_cast<unsigned long>(aBytes.length()),
                                     static_cast<unsigned char>(inLengthSize));
    }
    // a length field, then the bytes; a NULL buffer writes only a zero length
    void AppendBuffer(const ByteBuffer* inBuffer, unsigned char inLengthFieldSize = 0)
    {
        const uint8_t* aData=inBuffer!=NULL ? reinterpret_cast<const uint8_t*>(inBuffer->data()) : NULL;
        ee_won_writer_append_field(this->mBuffer,aData,inBuffer!=NULL ? inBuffer->length() : 0,inLengthFieldSize);
    }
    void SetBytes(unsigned long inPos, const void* inData, unsigned long inLength)
    { ee_won_writer_set(this->mBuffer,inPos,static_cast<const uint8_t*>(inData),inLength); }
    void SetByte(unsigned long inPos, unsigned char inByte) { this->SetBytes(inPos,&inByte,1); }
    void SetShort(unsigned long inPos, short inValue) { this->SetBytes(inPos,&inValue,2); }
    // the length prefix in front of the data, as a byte buffer; inRelease hands the bytes over and empties this
    ConstSmartPtr<ByteBuffer> ToByteBuffer(bool inRelease)
    { return new ByteBuffer(ee_won_writer_to_bytes(this->mBuffer,inRelease)); }

    EeWonWriteBuffer* mBuffer;
};

}
#endif
