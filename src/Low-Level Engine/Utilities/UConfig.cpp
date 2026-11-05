// C:\EEX\Low-Level Engine\Utilities\UConfig.cpp (the original's URegistry.cpp, 0x10081d93..0x1008268d; UResourceFile.cpp
// follows it). The game's settings go to config.ini through the platform layer (platform/PlatConfig.h) instead of the
// registry: lle/UConfig.h.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/UConfig.h"
#include "platform/PlatConfig.h"

// Reads an integer value
long UConfig::GetValue(const char* inSection, const char* inName, long& outValue)
{
    int32_t theValue;
    if (!PlatConfigGetInt(inSection, inName, &theValue))
        return kErrorNotFound;
    outValue = theValue;
    return kErrorOK;
}

// Reads a string value (at most 1023 bytes)
long UConfig::GetValue(const char* inSection, const char* inName, UWideString& outValue)
{
    char theData[1024];
    if (!PlatConfigGetString(inSection, inName, theData, sizeof(theData)))
        return kErrorNotFound;
    outValue = theData;
    return kErrorOK;
}

// Writes an integer value
long UConfig::SetValue(const char* inSection, const char* inName, long inValue)
{
    return PlatConfigSetInt(inSection, inName, (int32_t)inValue) ? kErrorOK : kErrorNotFound;
}

// Writes a string value
long UConfig::SetValue(const char* inSection, const char* inName, const UWideString& inValue)
{
    return PlatConfigSetString(inSection, inName, (const char*)inValue) ? kErrorOK : kErrorNotFound;
}

// Removes a value
long UConfig::DeleteValue(const char* inSection, const char* inName)
{
    return PlatConfigDelete(inSection, inName) ? kErrorOK : kErrorNotFound;
}
