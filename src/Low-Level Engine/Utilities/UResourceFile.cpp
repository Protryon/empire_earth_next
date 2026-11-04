// C:\EEX\Low-Level Engine\Utilities\UResourceFile.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1008268d..0x10082871: the two resource enumeration callbacks (cdecl, returning bool, cast to the Win32
// callback types), the ctors, the dtor and GetString. The inline members and implicit copy members are emitted by
// ISKey.cpp (0x10039c02..). User Interface.cpp's init chains start at 0x10082871.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"

// FUNCTION: LLE 0x1008268d
// Resource language callback: stores the first language found and stops the enumeration
// (VC6: a cdecl bool function cast to the stdcall callback type; Windows' callback thunk restored the stack)
static BOOL CALLBACK URFEnumLanguages(HMODULE inModule, LPCSTR inType, LPCSTR inName, WORD inLanguage, LONG_PTR inParam)
{
    *reinterpret_cast<unsigned short*>(inParam) = inLanguage;
    return FALSE;
}

// FUNCTION: LLE 0x1008269c
// Resource name callback: stores a string table's language, falling back to the OS language
static BOOL CALLBACK URFEnumNames(HMODULE inModule, LPCSTR inType, LPSTR inName, LONG_PTR inParam)
{
    if (!EnumResourceLanguagesA(inModule, RT_STRING, inName, URFEnumLanguages, inParam)) {
        *reinterpret_cast<unsigned short*>(inParam) = UGetOSLanguage();
        ESRecordNativeError(GetLastError());
    }
    return TRUE;
}

// Reads the resources of a loaded module (the game's own: the original loaded Language.dll from a file) and records
// its string table language (the OS language if none)
UResourceFile::UResourceFile(HINSTANCE inModule)
    : mOwnsModule(false)
{
    this->mModule = inModule;
    if (!EnumResourceNamesA(this->mModule, RT_STRING, URFEnumNames, reinterpret_cast<LONG_PTR>(&this->mLanguage))) {
        this->mLanguage = UGetOSLanguage();
        ESRecordNativeError(GetLastError());
    }
}

// FUNCTION: LLE 0x1008280b
// Destroys the resource file, unloading the DLL when it loaded it
UResourceFile::~UResourceFile()
{
    if (this->mOwnsModule)
        FreeLibrary(this->mModule);
}

// FUNCTION: LLE 0x10082821
// Loads a string resource by id (up to 4500 bytes); native error when it is missing
long UResourceFile::GetString(long inID, UWideString& outString)
{
    char theString[4500];
    if (LoadString(this->mModule, inID, theString, 4500) > 0) {
        outString = theString;
        return kErrorOK;
    }
    ESRecordNativeError(GetLastError());
    return kErrorNativeError;
}
