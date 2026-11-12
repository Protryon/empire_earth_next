// C:\EEX\Low-Level Engine\Utilities\UDataCompression.cpp
// The class forwards to Rust (rust/crates/ee_codec: the PKWARE DCL stream and the "PK01" block format). The stream
// code that lived in src/ThirdParty/pkdcl is gone; only the implode() error's assert stays here.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UDataCompression.h"
#include "rust/ee_codec.h"

UDataCompression::UDataCompression()
{
}

UDataCompression::~UDataCompression()
{
}

long UDataCompression::Compress(const char* inData, unsigned long inLength, char* outData, unsigned long inCapacity,
                                unsigned long& outLength, unsigned long inLevel)
{
    uint32_t theLength = 0;
    long theResult = ee_compress(reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength,
                                 reinterpret_cast<uint8_t*>(outData), (uint32_t)inCapacity, &theLength,
                                 (uint32_t)inLevel);
    outLength = theLength;
    if (theResult == 0x15)
        ESVERIFY_AT(NULL == "PKWARE implode() error!", 140);
    return theResult;
}

long UDataCompression::Decompress(const char* inData, unsigned long inLength, char* outData,
                                  unsigned long inCapacity, unsigned long& outLength)
{
    uint32_t theLength = 0;
    long theResult = ee_decompress(reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength,
                                   reinterpret_cast<uint8_t*>(outData), (uint32_t)inCapacity, &theLength);
    outLength = theLength;
    return theResult;
}

long UDataCompression::GetDecompressedSize(const char* inData, unsigned long inLength, unsigned long& outSize)
{
    uint32_t theSize = 0;
    long theResult = ee_decompressed_size(reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength, &theSize);
    outSize = theSize;
    return theResult;
}

bool UDataCompression::GetIsCompressed(const char* inData, unsigned long inLength)
{
    return ee_is_compressed(reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength);
}
