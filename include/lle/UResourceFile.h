// C:\EEX\Low-Level Engine\Utilities\UResourceFile.h (RECONSTRUCTION;
// UResourceFile.cpp 0x8268d..0x82871, all exact). A string-table resource module. sizeof 0xc
// (??_E stride 0x39c47). Inline members and the implicit copy members are emitted by ISKey.cpp (0x39c02..0x39ca1),
// the first unit to include this header.
#ifndef LLE_URESOURCEFILE_H
#define LLE_URESOURCEFILE_H

#include <string.h>
#include "lle/UWideString.h"
#include "rust/base/util.h"

// The strings come from a Rust table generated from Language.rc (tools/lang_strings.py; ee_util strings.rs), on
// every system: the original enumerated the module's RT_STRING resources for their language (the first table's,
// else UGetOSLanguage) and read each string with LoadStringA. The table holds what LoadStringA returned, its
// language is the script's; an id it lacks fails as LoadStringA did (ERROR_RESOURCE_NAME_NOT_FOUND, 1814).
class LLE_API UResourceFile {
public:
    // records the table's language (the module is kept for GetModule; nothing is read from it)
    UResourceFile(HINSTANCE inModule) : mModule(inModule), mOwnsModule(false)
    {
        uint16_t theLanguage = ee_util_resource_strings_language();
        this->mLanguage = theLanguage != 0 ? theLanguage : UGetOSLanguage();
    }
    virtual ~UResourceFile() {}
    unsigned short GetLanguage() { return this->mLanguage; }
    // a string by id (up to 4499 bytes, as LoadStringA's buffer); a native error when it is missing
    long GetString(long inID, UWideString& outString) { return ee_util_resource_string_get((uint16_t)inID, Assign, &outString); }
    static void Assign(void* inString, const char* inText) { *static_cast<UWideString*>(inString) = inText; }
    HINSTANCE mModule;                      // +0x04
    unsigned short mLanguage;               // +0x08
    bool mOwnsModule;                       // +0x0a (provisional name; always false: the module is the game's)
};

#endif
