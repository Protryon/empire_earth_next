// C:\EEX\Low-Level Engine\Utilities\UResourceFile.h (RECONSTRUCTION;
// UResourceFile.cpp 0x8268d..0x82871, all exact). A string-table resource module. sizeof 0xc
// (??_E stride 0x39c47). Inline members and the implicit copy members are emitted by ISKey.cpp (0x39c02..0x39ca1),
// the first unit to include this header.
#ifndef LLE_URESOURCEFILE_H
#define LLE_URESOURCEFILE_H

class FSFileSpec;
class UWideString;

class LLE_API UResourceFile {
public:
    UResourceFile(HINSTANCE inModule);
    virtual ~UResourceFile();
    unsigned short GetLanguage() { return this->mLanguage; }
    HINSTANCE GetModule() const { return this->mModule; }
    long GetString(long inID, UWideString& outString);
private:
    HINSTANCE mModule;                      // +0x04
    unsigned short mLanguage;               // +0x08
    bool mOwnsModule;                       // +0x0a (provisional name; always false: the module is the game's)
};

#endif
