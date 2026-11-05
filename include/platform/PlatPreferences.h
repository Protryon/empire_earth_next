// Platform layer: the preferences store (see platform/Platform.h).
//
// A tree of keys (paths with backslash separators, e.g. "Software\\SSSI\\Empire Earth") under a few roots, each key
// holding named values: strings and 32-bit integers. On Win32 it is the registry (the roots are the predefined
// HKEY_* keys, a string value is REG_SZ, an integer REG_DWORD), exactly as the game's URegistry used it. Another
// implementation may keep the same tree in a file per root (only kPlatPrefCurrentUser and kPlatPrefLocalMachine are
// used by the game).
//
// Every function -> nonzero on success, 0 on failure (then PlatGetLastError, platform/PlatThread.h, is what the
// engine records). Values are written into existing keys only: create the key first.
#ifndef PLATFORM_PLATPREFERENCES_H
#define PLATFORM_PLATPREFERENCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PlatPrefRoot {
    kPlatPrefClassesRoot = 0,               // HKEY_CLASSES_ROOT
    kPlatPrefCurrentUser = 1,               // HKEY_CURRENT_USER: the game's settings
    kPlatPrefLocalMachine = 2,              // HKEY_LOCAL_MACHINE
    kPlatPrefUsers = 3,                     // HKEY_USERS
    kPlatPrefCurrentConfig = 4,             // HKEY_CURRENT_CONFIG
    kPlatPrefDynData = 5,                   // HKEY_DYN_DATA (Windows 9x)
    kPlatPrefRootCount = 6
} PlatPrefRoot;

int PlatPrefCreateKey(PlatPrefRoot inRoot, const char* inKey);
int PlatPrefDeleteKey(PlatPrefRoot inRoot, const char* inKey);   // a key without subkeys
int PlatPrefDeleteValue(PlatPrefRoot inRoot, const char* inKey, const char* inName);
// a string value of inSize bytes from inData (the game stores the characters without the terminating zero)
int PlatPrefSetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, const char* inData,
                      uint32_t inSize);
int PlatPrefSetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t inValue);
// reads a value of any type into outValue (4 bytes) / outData (up to *ioSize bytes; *ioSize becomes the size read,
// the data is not terminated unless stored with its zero)
int PlatPrefGetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t* outValue);
int PlatPrefGetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, char* outData, uint32_t* ioSize);

#ifdef __cplusplus
}
#endif

#endif
