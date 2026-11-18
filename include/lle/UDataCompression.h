// C:\EEX\Low-Level Engine\Utilities\UDataCompression.h (file name inferred): the "PK01" block format over a PKWARE
// DCL stream. Forwards to Rust (rust/crates/ee_base/src/codec); the work buffers are gone with the C implementation.
#ifndef LLE_UDATACOMPRESSION_H
#define LLE_UDATACOMPRESSION_H

class LLE_API UDataCompression {
public:
    UDataCompression();
    virtual ~UDataCompression();
    long Compress(const char* inData, unsigned long inLength, char* outData, unsigned long inCapacity,
                  unsigned long& outLength, unsigned long inLevel);
};

#endif
