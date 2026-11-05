// The game's settings file: named values in named sections of config.ini (platform/PlatConfig.h). The original's
// URegistry (C:\EEX\Low-Level Engine\Utilities\URegistry.cpp, 0x10081d93..0x1008268d) wrote the same values into the
// Windows registry under HKEY_* root keys; this port keeps them in the file instead (no registry is used).
// Results: 0 (kErrorOK) when the value was read or written, kErrorNotFound otherwise (a missing value, a value of
// the other kind, or a file that cannot be written).
#ifndef LLE_UCONFIG_H
#define LLE_UCONFIG_H

class UWideString;

class LLE_API UConfig {                     // no data
public:
    long GetValue(const char* inSection, const char* inName, long& outValue);
    long GetValue(const char* inSection, const char* inName, UWideString& outValue);
    long SetValue(const char* inSection, const char* inName, long inValue);
    long SetValue(const char* inSection, const char* inName, const UWideString& inValue);
    long DeleteValue(const char* inSection, const char* inName);
};

#endif
