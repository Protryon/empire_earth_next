// C:\EEX\Low-Level Engine\Utilities\UEncryption.cpp
// The encryption itself is Rust (rust/crates/ee_base/src/codec, encryption.rs): these forwarders only convert UStrings. The
// exported signatures stay, so nothing above this file changed.
#include <windows.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/UEncryption.h"
#include "rust/base/codec.h"

static const uint8_t* Bytes(const UString& inString)
{
    return reinterpret_cast<const uint8_t*>(static_cast<const char*>(inString));
}

// sink: the bytes become the UString's text (no embedded NULs: the Rust side drops them as the C++ did)
static void ToUString(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}

void UEEncryptUString(const UString& inString, UString& outString)
{
    ee_ue_encrypt_ustring(Bytes(inString), inString.GetLength(), inString.GetAllocatedLength(), ToUString, &outString);
}

bool UEDecryptUString(const UString& inString, UString& outString)
{
    return ee_ue_decrypt_ustring(Bytes(inString), inString.GetLength(), ToUString, &outString);
}
