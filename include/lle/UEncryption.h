// C:\EEX\Low-Level Engine\Utilities\UEncryption.h (RECONSTRUCTION; UEncryption.cpp 0x1005662f..0x10057218). Free
// functions that turn authorization codes, expirations, GUIDs and strings into 8-digit hex blocks: each 32-bit value
// is rotated left and xor'ed with one of four keys, and every record but the authorization code ends in a byte-sum
// checksum block. Signatures from the exports (names of the parameters provisional name).
#ifndef LLE_UENCRYPTION_H
#define LLE_UENCRYPTION_H

class UString;

LLE_API void UEEncryptUString(const UString& inString, UString& outString);
LLE_API bool UEDecryptUString(const UString& inString, UString& outString);

#endif
