// Platform layer, Win32: the preferences store on the registry (include/platform/PlatPreferences.h). The same calls
// in the same order as the game's URegistry made them: open the root with KEY_ALL_ACCESS, open (or create) the key,
// read or write, close. On a failure the thread's last error is the one GetLastError gave right after the failing
// call (the URegistry code recorded it before closing the keys).
#include <windows.h>

#include "platform/PlatPreferences.h"

namespace {

HKEY RootKey(PlatPrefRoot inRoot)
{
    switch (inRoot) {
    case kPlatPrefClassesRoot: return HKEY_CLASSES_ROOT;
    case kPlatPrefCurrentUser: return HKEY_CURRENT_USER;
    case kPlatPrefLocalMachine: return HKEY_LOCAL_MACHINE;
    case kPlatPrefUsers: return HKEY_USERS;
    case kPlatPrefCurrentConfig: return HKEY_CURRENT_CONFIG;
    case kPlatPrefDynData: return HKEY_DYN_DATA;
    default: return NULL;
    }
}

// The root opened for all access, NULL on failure (the error is kept).
HKEY OpenRoot(PlatPrefRoot inRoot, DWORD& outError)
{
    HKEY theRoot;
    HKEY theKey = RootKey(inRoot);
    if (theKey == NULL) {
        outError = ERROR_INVALID_HANDLE;
        return NULL;
    }
    if (RegOpenKeyExA(theKey, NULL, 0, KEY_ALL_ACCESS, &theRoot) == ERROR_SUCCESS)
        return theRoot;
    outError = GetLastError();
    return NULL;
}

int Fail(DWORD inError)
{
    SetLastError(inError);
    return 0;
}

// Opens inKey under the root, runs inOperation(key) -> its result; closes both keys.
template <class Operation> int WithKey(PlatPrefRoot inRoot, const char* inKey, Operation inOperation)
{
    DWORD theError = 0;
    HKEY theRoot = OpenRoot(inRoot, theError);
    if (theRoot == NULL)
        return Fail(theError);
    int theResult = 0;
    HKEY theKey;
    if (RegOpenKeyExA(theRoot, inKey, 0, KEY_ALL_ACCESS, &theKey) == ERROR_SUCCESS) {
        if (inOperation(theKey))
            theResult = 1;
        else
            theError = GetLastError();
        RegCloseKey(theKey);
    } else
        theError = GetLastError();
    RegCloseKey(theRoot);
    return theResult ? 1 : Fail(theError);
}

}   // namespace

extern "C" int PlatPrefCreateKey(PlatPrefRoot inRoot, const char* inKey)
{
    DWORD theError = 0;
    HKEY theRoot = OpenRoot(inRoot, theError);
    if (theRoot == NULL)
        return Fail(theError);
    char theClass[100];
    theClass[0] = 0;
    HKEY theNewKey;
    DWORD theDisposition;
    int theResult = 0;
    if (RegCreateKeyExA(theRoot, inKey, 0, theClass, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &theNewKey,
                        &theDisposition) == ERROR_SUCCESS) {
        theResult = 1;
        RegCloseKey(theNewKey);
    } else
        theError = GetLastError();
    RegCloseKey(theRoot);
    return theResult ? 1 : Fail(theError);
}

extern "C" int PlatPrefDeleteKey(PlatPrefRoot inRoot, const char* inKey)
{
    DWORD theError = 0;
    HKEY theRoot = OpenRoot(inRoot, theError);
    if (theRoot == NULL)
        return Fail(theError);
    int theResult = 1;
    if (RegDeleteKeyA(theRoot, inKey) != ERROR_SUCCESS) {
        theError = GetLastError();
        theResult = 0;
    }
    RegCloseKey(theRoot);
    return theResult ? 1 : Fail(theError);
}

extern "C" int PlatPrefDeleteValue(PlatPrefRoot inRoot, const char* inKey, const char* inName)
{
    return WithKey(inRoot, inKey, [&](HKEY inOpened) {
        return RegDeleteValueA(inOpened, inName) == ERROR_SUCCESS;
    });
}

extern "C" int PlatPrefSetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, const char* inData,
                                 uint32_t inSize)
{
    return WithKey(inRoot, inKey, [&](HKEY inOpened) {
        return RegSetValueExA(inOpened, inName, 0, REG_SZ, reinterpret_cast<const BYTE*>(inData), inSize)
            == ERROR_SUCCESS;
    });
}

extern "C" int PlatPrefSetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t inValue)
{
    return WithKey(inRoot, inKey, [&](HKEY inOpened) {
        return RegSetValueExA(inOpened, inName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&inValue),
                              sizeof(inValue)) == ERROR_SUCCESS;
    });
}

extern "C" int PlatPrefGetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t* outValue)
{
    return WithKey(inRoot, inKey, [&](HKEY inOpened) {
        DWORD theType = REG_DWORD;
        DWORD theSize = sizeof(int32_t);
        int32_t theValue;
        if (RegQueryValueExA(inOpened, inName, NULL, &theType, reinterpret_cast<BYTE*>(&theValue), &theSize)
            != ERROR_SUCCESS)
            return false;
        *outValue = theValue;
        return true;
    });
}

extern "C" int PlatPrefGetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, char* outData,
                                 uint32_t* ioSize)
{
    return WithKey(inRoot, inKey, [&](HKEY inOpened) {
        DWORD theType = 0;
        DWORD theSize = *ioSize;
        if (RegQueryValueExA(inOpened, inName, NULL, &theType, reinterpret_cast<BYTE*>(outData), &theSize)
            != ERROR_SUCCESS)
            return false;
        *ioSize = theSize;
        return true;
    });
}
