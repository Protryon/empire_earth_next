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
#include "lle/UResourceStrings.h"

// The strings come from a table generated from Language.rc (tools/lang_strings.py), on every system: the original
// enumerated the module's RT_STRING resources for their language (URFEnumNames/URFEnumLanguages: the first table's,
// else UGetOSLanguage) and read each string with LoadStringA. The table holds what LoadStringA returned, its
// language is the script's; an id it lacks fails as LoadStringA did (ERROR_RESOURCE_NAME_NOT_FOUND).
static const uint32_t kErrorResourceNameNotFound = 1814;

// Records the string table's language (the module is kept for GetModule; nothing is read from it)
UResourceFile::UResourceFile(HINSTANCE inModule)
    : mOwnsModule(false)
{
    this->mModule = inModule;
    this->mLanguage = gResourceStringCount != 0 ? gResourceStringsLanguage : UGetOSLanguage();
}

// FUNCTION: LLE 0x1008280b
// Destroys the resource file (the module is the game's own: mOwnsModule is always false)
UResourceFile::~UResourceFile()
{
}

// FUNCTION: LLE 0x10082821
// Looks a string up by id (up to 4500 bytes, as LoadStringA's buffer); native error when it is missing
long UResourceFile::GetString(long inID, UWideString& outString)
{
    unsigned long theLow = 0, theHigh = gResourceStringCount;
    unsigned short theID = (unsigned short)inID;
    while (theLow < theHigh) {
        unsigned long theMiddle = (theLow + theHigh) / 2;
        if (gResourceStrings[theMiddle].mID < theID)
            theLow = theMiddle + 1;
        else
            theHigh = theMiddle;
    }
    if (theLow < gResourceStringCount && gResourceStrings[theLow].mID == theID) {
        const char* theText = gResourceStrings[theLow].mText;
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
