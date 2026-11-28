// C:\EEX\Empire Earth\Persistent\EEPSaveGameHeader.h (RECONSTRUCTION; provisional names). The header of a
// saved game: name, description, save version, the game options, the scenario's media strings. EEPSaveGameHeader.cpp
// is 0x14b7a5..0x14bd64 (vtable 0x4422a8: FSRestorable's four slots). sizeof 0x500. Names are provisional (the keys
// and the default name strings give some).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, ee/EEGameOptions.h, ee/EESMMediaStrings.h, <vector>.
#ifndef EE_EEPSAVEGAMEHEADER_H
#define EE_EEPSAVEGAMEHEADER_H

#include "lle/FSRestorable.h"
#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "rust/game/gamecpp.h"
class EEPSaveGameHeader : public FSRestorable {
public:
    EEPSaveGameHeader();
    // Adds nothing to the checksum (empty: folded into a shared empty function)
    // +0x00 (empty: ICF-folded 0x157269)
    virtual void GetChecksum(ESChecksum& ioChecksum) const {  }
    // Reads the header's name/version part, then its body; returns the first read error
    // +0x04 0x14b993
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_hdr_load(this, &inFile); }
    // Writes the header's name/version part, then its body; returns the first write error
    // +0x0c 0x14bb9a
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_hdr_save(this, &outFile); }

    // Opens a save file, copies it into memory and reads its header from there; returns the first error
    long LoadFromFile(FSFile& inFile) { return ee_gamecpp_hdr_load_from_file(this, &inFile); }


    // inline accessors (the SCL load event's inline budget needs these five expansions; provisional names)
    void SetMajorVersion(unsigned long inVersion) { this->mMajorVersion = inVersion; }
    void SetMinorVersion(unsigned long inVersion) { this->mMinorVersion = inVersion; }

    UString mName;                          // +0x04 ("Load game name")
    UString mDescription;                   // +0x14 ("Load game description")
    unsigned long mMajorVersion;            // +0x24
    unsigned long mMinorVersion;            // +0x28 (LoadHeader: major 0 with minor < 0xc5 is too old)
    EEGameOptions mGameOptions;             // +0x2c
    EESMMediaStrings mMediaStrings;         // +0x438
    UString mPlayerNames;                   // +0x4c4
    UString mDate;                          // +0x4d4
    unsigned long mL4E4;                    // +0x4e4
    unsigned long mNumPlayers;              // +0x4e8 (3)
    unsigned long mChecksum;                // +0x4ec
    std::vector<long> mChunkTypes;          // +0x4f0
};

#endif
