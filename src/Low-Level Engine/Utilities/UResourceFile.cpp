// C:\EEX\Low-Level Engine\Utilities\UResourceFile.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1008268d..0x10082871 in the original: two resource enumeration callbacks, the ctors, the dtor and GetString
// (the callbacks went with the resource reader, see below). The inline members and implicit copy members are
// emitted by ISKey.cpp (0x10039c02..). User Interface.cpp's init chains start at 0x10082871.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "rust/base/util.h"

// The strings come from a Rust table generated from Language.rc (tools/lang_strings.py; ee_util strings.rs), on
// every system: the original enumerated the module's RT_STRING resources for their language (URFEnumNames/
// URFEnumLanguages: the first table's, else UGetOSLanguage) and read each string with LoadStringA. The table holds
// what LoadStringA returned, its language is the script's; an id it lacks fails as LoadStringA did
// (ERROR_RESOURCE_NAME_NOT_FOUND).
static const uint32_t kErrorResourceNameNotFound = 1814;

// Records the string table's language (the module is kept for GetModule; nothing is read from it)
UResourceFile::UResourceFile(HINSTANCE inModule)
    : mOwnsModule(false)
{
    this->mModule = inModule;
    uint16_t theLanguage = ee_util_resource_strings_language();
    this->mLanguage = theLanguage != 0 ? theLanguage : UGetOSLanguage();
}

// Destroys the resource file (the module is the game's own: mOwnsModule is always false)
UResourceFile::~UResourceFile()
{
}

// Looks a string up by id (up to 4500 bytes, as LoadStringA's buffer); native error when it is missing
long UResourceFile::GetString(long inID, UWideString& outString)
{
    const char* theText = (const char*)ee_util_resource_string((uint16_t)inID);
    if (theText != NULL) {
        if (strlen(theText) < 4499) {
            outString = theText;
        } else {
            char theString[4500];
            memcpy(theString, theText, 4499);
            theString[4499] = 0;
            outString = theString;
        }
        return kErrorOK;
    }
    ESRecordNativeError(kErrorResourceNameNotFound);
    return kErrorNativeError;
}
