// C:\EEX\Low-Level Engine\Utilities\UConfig.cpp (the original's URegistry.cpp, 0x10081d93..0x1008268d; UResourceFile.cpp
// follows it). The game's settings go to config.yaml through the platform layer (platform/PlatConfig.h) instead of the
// registry: lle/UConfig.h.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/UConfig.h"
#include "platform/PlatConfig.h"

// Reads an integer value
long UConfig::GetValue(const char* inBlock, const char* inKey, long& outValue)
{
    int32_t theValue;
    if (!PlatConfigGetInt(inBlock, inKey, &theValue))
        return kErrorNotFound;
    outValue = theValue;
    return kErrorOK;
}

// Reads a string value (at most 1023 bytes)
long UConfig::GetValue(const char* inBlock, const char* inKey, UWideString& outValue)
{
    char theData[1024];
    if (!PlatConfigGetString(inBlock, inKey, theData, sizeof(theData)))
        return kErrorNotFound;
    outValue = theData;
    return kErrorOK;
}

// Writes an integer value
long UConfig::SetValue(const char* inBlock, const char* inKey, long inValue)
{
    return PlatConfigSetInt(inBlock, inKey, (int32_t)inValue) ? kErrorOK : kErrorNotFound;
}

// Writes a string value
long UConfig::SetValue(const char* inBlock, const char* inKey, const UWideString& inValue)
{
    return PlatConfigSetString(inBlock, inKey, (const char*)inValue) ? kErrorOK : kErrorNotFound;
}

// Reads a boolean value
long UConfig::GetFlag(const char* inBlock, const char* inKey, bool& outValue)
{
    int32_t theValue;
    if (!PlatConfigGetInt(inBlock, inKey, &theValue))
        return kErrorNotFound;
    outValue = theValue != 0;
    return kErrorOK;
}

// Writes a boolean value
long UConfig::SetFlag(const char* inBlock, const char* inKey, bool inValue)
{
    return PlatConfigSetBool(inBlock, inKey, inValue ? 1 : 0) ? kErrorOK : kErrorNotFound;
}

// Removes a value
long UConfig::DeleteValue(const char* inBlock, const char* inKey)
{
    return PlatConfigDelete(inBlock, inKey) ? kErrorOK : kErrorNotFound;
}
