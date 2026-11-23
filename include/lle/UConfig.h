// The game's settings file: keyed values in the nested blocks of config.yaml (platform/PlatConfig.h; inBlock is the
// block's path, "display.resolution"). The original's URegistry (C:\EEX\Low-Level Engine\Utilities\URegistry.cpp,
// 0x10081d93..0x1008268d) wrote its values into the Windows registry under HKEY_* root keys; this port keeps them in
// the file instead (no registry is used).
// Results: 0 (kErrorOK) when the value was read or written, kErrorNotFound otherwise (a missing value, a value of
// the other kind, or a file that cannot be written).
#ifndef LLE_UCONFIG_H
#define LLE_UCONFIG_H

#include "platform/PlatConfig.h"
#include "lle/UWideString.h"
#include "rust/base/util.h"   // the reads (ee_base util config.rs)

class LLE_API UConfig {                     // no data
public:
    long GetValue(const char* inBlock, const char* inKey, long& outValue) { return ee_uconfig_get_long(inBlock, inKey, &outValue); }
    // a string of at most 1023 bytes
    long GetValue(const char* inBlock, const char* inKey, UWideString& outValue)
    {
        char theData[1024];
        if (!PlatConfigGetString(inBlock, inKey, theData, sizeof(theData)))
            return kErrorNotFound;
        outValue = theData;
        return kErrorOK;
    }
    long SetValue(const char* inBlock, const char* inKey, long inValue)
        { return PlatConfigSetInt(inBlock, inKey, (int32_t)inValue) ? kErrorOK : kErrorNotFound; }
    long SetValue(const char* inBlock, const char* inKey, const UWideString& inValue)
        { return PlatConfigSetString(inBlock, inKey, (const char*)inValue) ? kErrorOK : kErrorNotFound; }
    // port: a boolean (true/false; an integer reads as nonzero)
    long GetFlag(const char* inBlock, const char* inKey, bool& outValue) { return ee_uconfig_get_flag(inBlock, inKey, &outValue); }
    long SetFlag(const char* inBlock, const char* inKey, bool inValue)
        { return PlatConfigSetBool(inBlock, inKey, inValue ? 1 : 0) ? kErrorOK : kErrorNotFound; }
    long DeleteValue(const char* inBlock, const char* inKey)
        { return PlatConfigDelete(inBlock, inKey) ? kErrorOK : kErrorNotFound; }
};

#endif
