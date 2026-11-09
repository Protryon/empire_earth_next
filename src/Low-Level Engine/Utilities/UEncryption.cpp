// C:\EEX\Low-Level Engine\Utilities\UEncryption.cpp
// The encryption itself is Rust (rust/crates/ee_codec, encryption.rs): these forwarders only convert UStrings. The
// exported signatures stay, so nothing above this file changed.
#include <windows.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/UEncryption.h"
#include "rust/ee_rust.h"

static const uint8_t* Bytes(const UString& inString)
{
    return reinterpret_cast<const uint8_t*>(static_cast<const char*>(inString));
}

// sink: the bytes become the UString's text (no embedded NULs: the Rust side drops them as the C++ did)
static void ToUString(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}

void UEEncryptAuthorizationCode(UString& inCode, UString& outCode)
{
    ee_ue_encrypt_authorization_code(Bytes(inCode), inCode.GetLength(), ToUString, &outCode);
}

void UEDecryptAuthorizationCode(UString& inCode, unsigned long& outFirst, unsigned long& outSecond)
{
    uint32_t theFirst;
    uint32_t theSecond;
    ee_ue_decrypt_authorization_code(Bytes(inCode), inCode.GetLength(), &theFirst, &theSecond);
    outFirst = theFirst;
    outSecond = theSecond;
}

void UEEncryptExpiration(unsigned long inFirst, unsigned long inSecond, UString& outString)
{
    ee_ue_encrypt_expiration((uint32_t)inFirst, (uint32_t)inSecond, ToUString, &outString);
}

bool UEDecryptExpiration(const UString& inString, unsigned long& outFirst, unsigned long& outSecond)
{
    uint32_t theFirst;
    uint32_t theSecond;
    bool theResult = ee_ue_decrypt_expiration(Bytes(inString), inString.GetLength(), &theFirst, &theSecond);
    outFirst = theFirst;
    outSecond = theSecond;
    return theResult;
}

void UEEncryptGUID(unsigned long inValue, UString& inGUID, unsigned long inKey, UString& outString)
{
    ee_ue_encrypt_guid((uint32_t)inValue, Bytes(inGUID), inGUID.GetLength(), inGUID.GetAllocatedLength(),
                       (uint32_t)inKey, ToUString, &inGUID, ToUString, &outString);
}

bool UEDecryptGUID(const UString& inString, unsigned long inKey, unsigned long& outValue, UString& outGUID)
{
    uint32_t theValue;
    bool theResult = ee_ue_decrypt_guid(Bytes(inString), inString.GetLength(), (uint32_t)inKey, &theValue,
                                        Bytes(outGUID), outGUID.GetLength(), outGUID.GetAllocatedLength(), ToUString,
                                        &outGUID);
    outValue = theValue;
    return theResult;
}

void UEEncryptUString(const UString& inString, UString& outString)
{
    ee_ue_encrypt_ustring(Bytes(inString), inString.GetLength(), inString.GetAllocatedLength(), ToUString, &outString);
}

bool UEDecryptUString(const UString& inString, UString& outString)
{
    return ee_ue_decrypt_ustring(Bytes(inString), inString.GetLength(), ToUString, &outString);
}
