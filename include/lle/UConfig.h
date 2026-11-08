// The game's settings file: keyed values in the nested blocks of config.yaml (platform/PlatConfig.h; inBlock is the
// block's path, "display.resolution"). The original's URegistry (C:\EEX\Low-Level Engine\Utilities\URegistry.cpp,
// 0x10081d93..0x1008268d) wrote its values into the Windows registry under HKEY_* root keys; this port keeps them in
// the file instead (no registry is used).
// Results: 0 (kErrorOK) when the value was read or written, kErrorNotFound otherwise (a missing value, a value of
// the other kind, or a file that cannot be written).
#ifndef LLE_UCONFIG_H
#define LLE_UCONFIG_H

class UWideString;

class LLE_API UConfig {                     // no data
public:
    long GetValue(const char* inBlock, const char* inKey, long& outValue);
    long GetValue(const char* inBlock, const char* inKey, UWideString& outValue);
    long SetValue(const char* inBlock, const char* inKey, long inValue);
    long SetValue(const char* inBlock, const char* inKey, const UWideString& inValue);
    // port: a boolean (true/false; an integer reads as nonzero)
    long GetFlag(const char* inBlock, const char* inKey, bool& outValue);
    long SetFlag(const char* inBlock, const char* inKey, bool inValue);
    long DeleteValue(const char* inBlock, const char* inKey);
};

#endif
