// C:\EEX\Low-Level Engine\Utilities\URegistry.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10081d93..0x1008268d: the header-inline ctor/dtor, the implicit copy ctor and ??_E first (at the point
// lle/URegistry.h is included), then the members (declaration order). UResourceFile.cpp starts at 0x1008268d.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/URegistry.h"

static const long kInvalidRootKey = 3;

inline bool URIsRootKey(long inRootKey)
{
    return inRootKey == (long)HKEY_CLASSES_ROOT || inRootKey == (long)HKEY_CURRENT_CONFIG
        || inRootKey == (long)HKEY_LOCAL_MACHINE || inRootKey == (long)HKEY_USERS
        || inRootKey == (long)HKEY_DYN_DATA || inRootKey == (long)HKEY_CURRENT_USER;
}

// Header-inline members emitted here
// FUNCTION: LLE 0x10081d93
// FUNCTION: LLE 0x10081d9c
// FUNCTION: LLE 0x10081da3
// SYNTHETIC: LLE 0x10081dae

// FUNCTION: LLE 0x10081dfd
// Creates a registry key under a root key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryKey(long inRootKey, UWideString& inKey)
{
    HKEY theNewKey;
    DWORD theDisposition;
    char theClass[100];
    long theResult = kInvalidRootKey;
    theClass[0] = 0;
    if (URIsRootKey(inRootKey)) {
        HKEY theRoot;
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            if (RegCreateKeyEx(theRoot, inKey, 0, theClass, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &theNewKey,
                               &theDisposition) == ERROR_SUCCESS) {
                theResult = kErrorOK;
                RegCloseKey(theNewKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10081ebf
// Deletes a registry key under a root key; 3 for an invalid root, native error on failure
long URegistry::DeleteRegistryKey(long inRootKey, UWideString& inKey)
{
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        HKEY theRoot;
        theResult = kErrorOK;
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            if (RegDeleteKey(theRoot, inKey) != ERROR_SUCCESS) {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10081f5a
// Deletes a value of a registry key; 3 for an invalid root, native error on failure
long URegistry::DeleteRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName)
{
    HKEY theRoot;
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                if (RegDeleteValue(theKey, inValueName) == ERROR_SUCCESS)
                    theResult = kErrorOK;
                else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10082035
// Writes a string value into an existing registry key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName,
                                    UWideString& inValue)
{
    HKEY theRoot;
    char theData[1024];
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                strcpy(theData, inValue);
                if (RegSetValueEx(theKey, inValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(theData), inValue.GetTCHARLength()) == ERROR_SUCCESS)
                    theResult = kErrorOK;
                else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1008213c
// Writes a DWORD value into an existing registry key; 3 for an invalid root, native error on failure
long URegistry::CreateRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long& inValue)
{
    long theValue = inValue;
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        HKEY theRoot;
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                if (RegSetValueEx(theKey, inValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&theValue), sizeof(theValue)) == ERROR_SUCCESS)
                    theResult = kErrorOK;
                else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10082229
// Reads a DWORD registry value; 3 for an invalid root, native error on failure
long URegistry::GetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long& outValue)
{
    HKEY theRoot;
    DWORD theSize;
    DWORD theType;
    long theValue;
    long theResult = kInvalidRootKey;
    theType = REG_DWORD;
    theSize = sizeof(long);
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                if (RegQueryValueEx(theKey, inValueName, NULL, &theType, reinterpret_cast<BYTE*>(&theValue), &theSize)
                    == ERROR_SUCCESS) {
                    outValue = theValue;
                    theResult = kErrorOK;
                } else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10082327
// Reads a string registry value (at most 500 bytes); 3 for an invalid root, native error on failure
long URegistry::GetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName,
                                 UWideString& outValue)
{
    HKEY theRoot;
    DWORD theSize;
    DWORD theType;
    char theData[500];
    long theResult = kInvalidRootKey;
    theType = 0;
    theSize = 500;
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                if (RegQueryValueEx(theKey, inValueName, NULL, &theType, reinterpret_cast<BYTE*>(theData), &theSize)
                    == ERROR_SUCCESS) {
                    outValue = theSize == 0 ? "" : theData;
                    theResult = kErrorOK;
                } else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10082443
// Sets a string registry value in an existing key
long URegistry::SetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& inValue)
{
    HKEY theRoot;
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            // Fixed: the key was opened and the old value queried in one test; when the query failed (value missing,
            // or a string over 500 bytes) the call failed and the opened key was never closed
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                const char* theData = inValue;
                if (RegSetValueEx(theKey, inValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(theData), inValue.GetTCHARLength())
                    == ERROR_SUCCESS)
                    theResult = kErrorOK;
                else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1008257a
// Sets a DWORD registry value in an existing key
long URegistry::SetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long inValue)
{
    DWORD theSize = sizeof(long);
    HKEY theRoot;
    long theResult = kInvalidRootKey;
    if (URIsRootKey(inRootKey)) {
        if (RegOpenKeyEx((HKEY)inRootKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS) {
            HKEY theKey;
            // Fixed: the key was opened and the old value queried in one test; when the value was missing the call
            // failed and the opened key was never closed
            if (RegOpenKeyEx(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
                if (RegSetValueEx(theKey, inValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&inValue), theSize) == ERROR_SUCCESS)
                    theResult = kErrorOK;
                else {
                    ESRecordNativeError(GetLastError());
                    ESGetNativeError();
                    theResult = kErrorNativeError;
                }
                RegCloseKey(theKey);
            } else {
                ESRecordNativeError(GetLastError());
                ESGetNativeError();
                theResult = kErrorNativeError;
            }
            RegCloseKey(theRoot);
        } else {
            ESRecordNativeError(GetLastError());
            ESGetNativeError();
            theResult = kErrorNativeError;
        }
    }
    return theResult;
}
