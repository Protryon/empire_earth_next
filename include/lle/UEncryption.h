// C:\EEX\Low-Level Engine\Utilities\UEncryption.h (RECONSTRUCTION; UEncryption.cpp 0x1005662f..0x10057218). Free
// functions that turn authorization codes, expirations, GUIDs and strings into 8-digit hex blocks: each 32-bit value
// is rotated left and xor'ed with one of four keys, and every record but the authorization code ends in a byte-sum
// checksum block. Signatures from the exports (names of the parameters provisional name).
#ifndef LLE_UENCRYPTION_H
#define LLE_UENCRYPTION_H

// The encryption is Rust's (rust/crates/ee_base/src/codec encryption.rs): these convert the UStrings.
#include <string>
#include "lle/UString.h"
#include "rust/base/codec.h"

// sink: the bytes become the UString's text (no embedded NULs: the Rust side drops them as the C++ did)
inline void UESink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}
inline void UEEncryptUString(const UString& inString, UString& outString)
{
    ee_ue_encrypt_ustring(reinterpret_cast<const uint8_t*>(static_cast<const char*>(inString)), inString.GetLength(),
                          inString.GetAllocatedLength(), UESink, &outString);
}
inline bool UEDecryptUString(const UString& inString, UString& outString)
{
    return ee_ue_decrypt_ustring(reinterpret_cast<const uint8_t*>(static_cast<const char*>(inString)),
                                 inString.GetLength(), UESink, &outString);
}

#endif
