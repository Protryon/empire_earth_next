// C:\EEX\Low-Level Engine\Utilities\URegistry.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10081d93..0x1008268d: the header-inline ctor/dtor, the implicit copy ctor and ??_E first (at the point
// lle/URegistry.h is included), then the members (declaration order). UResourceFile.cpp starts at 0x1008268d.
// The registry calls themselves are the platform preferences store's (platform/PlatPreferences.h): on Win32 it opens
// the root and the key with KEY_ALL_ACCESS, as this code did, and a failure leaves the same last error.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/URegistry.h"

static const long kInvalidRootKey = 3;

inline bool URIsRootKey(PlatPrefRoot inRootKey)
{
    return inRootKey >= kPlatPrefClassesRoot && inRootKey < kPlatPrefRootCount;
}

// A failed store call: records the native error
static long URNativeError()
{
    ESRecordNativeError(PlatGetLastError());
    ESGetNativeError();
    return kErrorNativeError;
}

// Header-inline members emitted here
// FUNCTION: LLE 0x10081d93
// FUNCTION: LLE 0x10081d9c
// FUNCTION: LLE 0x10081da3
// SYNTHETIC: LLE 0x10081dae

// FUNCTION: LLE 0x10081dfd
// Creates a registry key under a root key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryKey(PlatPrefRoot inRootKey, UWideString& inKey)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    return PlatPrefCreateKey(inRootKey, inKey) ? kErrorOK : URNativeError();
}

// FUNCTION: LLE 0x10081ebf
// Deletes a registry key under a root key; 3 for an invalid root, native error on failure
long URegistry::DeleteRegistryKey(PlatPrefRoot inRootKey, UWideString& inKey)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    return PlatPrefDeleteKey(inRootKey, inKey) ? kErrorOK : URNativeError();
}

// FUNCTION: LLE 0x10081f5a
// Deletes a value of a registry key; 3 for an invalid root, native error on failure
long URegistry::DeleteRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    return PlatPrefDeleteValue(inRootKey, inKey, inValueName) ? kErrorOK : URNativeError();
}

// FUNCTION: LLE 0x10082035
// Writes a string value into an existing registry key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName,
                                    UWideString& inValue)
{
    char theData[1024];
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    strcpy(theData, inValue);
    return PlatPrefSetString(inRootKey, inKey, inValueName, theData, inValue.GetTCHARLength()) ? kErrorOK
                                                                                               : URNativeError();
}

// FUNCTION: LLE 0x1008213c
// Writes a DWORD value into an existing registry key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName,
                                    long& inValue)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    return PlatPrefSetInt32(inRootKey, inKey, inValueName, (int32_t)inValue) ? kErrorOK : URNativeError();
}

// FUNCTION: LLE 0x10082229
// Reads a DWORD registry value; 3 for an invalid root, native error on failure
long URegistry::GetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName,
                                 long& outValue)
{
    int32_t theValue;
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    if (!PlatPrefGetInt32(inRootKey, inKey, inValueName, &theValue))
        return URNativeError();
    outValue = theValue;
    return kErrorOK;
}

// FUNCTION: LLE 0x10082327
// Reads a string registry value (at most 500 bytes); 3 for an invalid root, native error on failure
long URegistry::GetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName,
                                 UWideString& outValue)
{
    char theData[500];
    uint32_t theSize = 500;
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    if (!PlatPrefGetString(inRootKey, inKey, inValueName, theData, &theSize))
        return URNativeError();
    outValue = theSize == 0 ? "" : theData;
    return kErrorOK;
}

// FUNCTION: LLE 0x10082443
// Sets a string registry value in an existing key
// (Fixed: the key was opened and the old value queried in one test; when the query failed (value missing, or a
// string over 500 bytes) the call failed and the opened key was never closed)
long URegistry::SetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName,
                                 UWideString& inValue)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    const char* theData = inValue;
    return PlatPrefSetString(inRootKey, inKey, inValueName, theData, inValue.GetTCHARLength()) ? kErrorOK
                                                                                               : URNativeError();
}

// FUNCTION: LLE 0x1008257a
// Sets a DWORD registry value in an existing key
// (Fixed: the key was opened and the old value queried in one test; when the value was missing the call failed and
// the opened key was never closed)
long URegistry::SetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, long inValue)
{
    if (!URIsRootKey(inRootKey))
        return kInvalidRootKey;
    return PlatPrefSetInt32(inRootKey, inKey, inValueName, (int32_t)inValue) ? kErrorOK : URNativeError();
}
