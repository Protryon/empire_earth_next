// C:\EEX\Low-Level Engine\Utilities\UDataCompression.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10056369..0x1005662f: ctor, dtor, the members alphabetical, then the two PKWARE stream callbacks. The PKWARE
// Data Compression Library itself (implode 0x10085bf0, explode 0x100866d0) is library code (analysis/lle/library.txt).
// UEncryption.cpp's helpers start at 0x1005662f.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/UDataCompression.h"

// PKWARE Data Compression Library for Win32 (implode.h)
extern "C" {
unsigned int implode(unsigned int (*read_buf)(char* buf, unsigned int* size, void* param),
                     void (*write_buf)(char* buf, unsigned int* size, void* param), char* work_buf, void* param,
                     unsigned int* type, unsigned int* dsize);
unsigned int explode(unsigned int (*read_buf)(char* buf, unsigned int* size, void* param),
                     void (*write_buf)(char* buf, unsigned int* size, void* param), char* work_buf, void* param);
}

// The stream state handed to implode()/explode() as `param`.
struct UDCStream {
    const char* mInput;
    unsigned long mInputLength;
    char* mOutput;
    unsigned long mOutputCapacity;
    unsigned long mInputPosition;
    unsigned long mOutputPosition;
};

// "PK01" + uncompressed length + reserved dword
static const char kCompressionSignature[4] = { 'P', 'K', '0', '1' };
static const unsigned long kHeaderSize = 12;

static unsigned int UDCReadBuffer(char* outBuffer, unsigned int* inSize, void* inStream);
static void UDCWriteBuffer(char* inBuffer, unsigned int* inSize, void* inStream);

// FUNCTION: LLE 0x10056369
// Creates a compressor with its PKWARE implode and explode work buffers
UDataCompression::UDataCompression()
{
    this->mWorkBuffers[0] = new char[0x8dd8];
    this->mWorkBuffers[1] = new char[0x3134];
}

// FUNCTION: LLE 0x10056393
// Destroys the compressor's work buffers
UDataCompression::~UDataCompression()
{
    delete[] this->mWorkBuffers[0];
    delete[] this->mWorkBuffers[1];
}

// FUNCTION: LLE 0x100563b0
// Compresses a block behind a "PK01" + size header; 0x14 already compressed, 0x15 failure, 0x16 output too small
long UDataCompression::Compress(const char* inData, unsigned long inLength, char* outData, unsigned long inCapacity,
                                unsigned long& outLength, unsigned long inLevel)
{
    unsigned int theDictionarySize = inLevel;
    unsigned int theType;
    UDCStream theStream;
    long theResult;
    if (this->GetIsCompressed(inData, inLength) == true) {
        outLength = 0;
        theResult = 0x14;
    } else {
        memcpy(outData, kCompressionSignature, 4);
        unsigned long theLength = inLength;
        memcpy(outData + 4, &theLength, 4);
        unsigned long theReserved = 0;
        memcpy(outData + 8, &theReserved, 4);
        theStream.mInput = inData;
        theStream.mInputLength = inLength;
        theStream.mOutput = outData + kHeaderSize;
        theStream.mOutputCapacity = inCapacity - kHeaderSize;
        theStream.mInputPosition = 0;
        theStream.mOutputPosition = 0;
        theType = 0;
        memset(this->mWorkBuffers[0], 0, 0x8dd8);
        if (implode(UDCReadBuffer, UDCWriteBuffer, this->mWorkBuffers[0], &theStream, &theType, &theDictionarySize) == 0) {
            if (theStream.mOutputPosition <= theStream.mOutputCapacity) {
                theResult = 0;
                outLength = theStream.mOutputPosition + kHeaderSize;
            } else {
                theResult = 0x16;
                outLength = 0;
            }
        } else {
            outLength = 0;
            theResult = 0x15;
            ESVERIFY_AT(NULL == "PKWARE implode() error!", 140);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x100564b9
// Decompresses a "PK01" block; 0x13 not compressed, 0x15 failure, 0x16 output too small
long UDataCompression::Decompress(const char* inData, unsigned long inLength, char* outData,
                                  unsigned long inCapacity, unsigned long& outLength)
{
    long theResult;
    UDCStream theStream;
    if (!this->GetIsCompressed(inData, inLength)) {
        theResult = 0x13;
        outLength = 0;
    } else {
        theStream.mInput = inData + kHeaderSize;
        theStream.mInputLength = inLength - kHeaderSize;
        theStream.mOutput = outData;
        theStream.mOutputCapacity = inCapacity;
        theStream.mInputPosition = 0;
        theStream.mOutputPosition = 0;
        memset(this->mWorkBuffers[1], 0, 0x3134);
        if (explode(UDCReadBuffer, UDCWriteBuffer, this->mWorkBuffers[1], &theStream) == 0) {
            // output that did not fit was dropped: report it as Compress does
            if (theStream.mOutputPosition <= theStream.mOutputCapacity) {
                theResult = 0;
                outLength = theStream.mOutputPosition;
            } else {
                theResult = 0x16;
                outLength = 0;
            }
        } else {
            theResult = 0x15;
            outLength = 0;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10056544
// Reads the uncompressed size from a "PK01" header; 0x13 when the data is not compressed
long UDataCompression::GetDecompressedSize(const char* inData, unsigned long inLength, unsigned long& outSize)
{
    long theResult;
    if (!this->GetIsCompressed(inData, inLength)) {
        outSize = 0;
        theResult = 0x13;
    } else {
        theResult = 0;
        memcpy(&outSize, inData + 4, 4);
    }
    return theResult;
}

// FUNCTION: LLE 0x1005657f
// Returns whether the data is longer than the header and starts with "PK01"
bool UDataCompression::GetIsCompressed(const char* inData, unsigned long inLength)
{
    bool theResult = false;
    if (inLength > kHeaderSize && memcmp(inData, kCompressionSignature, 4) == 0)
        theResult = true;
    return theResult;
}

// FUNCTION: LLE 0x100565a8
// PKWARE read callback: hands out the next piece of the input; returns its size (0 at the end)
static unsigned int UDCReadBuffer(char* outBuffer, unsigned int* inSize, void* inStream)
{
    UDCStream* theStream = static_cast<UDCStream*>(inStream);
    unsigned int theRead;
    if (theStream->mInputPosition >= theStream->mInputLength)
        theRead = 0;
    else if (theStream->mInputLength - theStream->mInputPosition < *inSize) {
        memcpy(outBuffer, theStream->mInput + theStream->mInputPosition,
               theStream->mInputLength - theStream->mInputPosition);
        theRead = theStream->mInputLength - theStream->mInputPosition;
        theStream->mInputPosition = theStream->mInputLength;
    } else {
        memcpy(outBuffer, theStream->mInput + theStream->mInputPosition, *inSize);
        theRead = *inSize;
        theStream->mInputPosition += *inSize;
    }
    return theRead;
}

// FUNCTION: LLE 0x100565fd
// PKWARE write callback: appends to the output while it fits, but always advances the output position
static void UDCWriteBuffer(char* inBuffer, unsigned int* inSize, void* inStream)
{
    UDCStream* theStream = static_cast<UDCStream*>(inStream);
    if (theStream->mOutputPosition + *inSize <= theStream->mOutputCapacity)
        memcpy(theStream->mOutput + theStream->mOutputPosition, inBuffer, *inSize);
    theStream->mOutputPosition += *inSize;
}
