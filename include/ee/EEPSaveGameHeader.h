// C:\EEX\Empire Earth\Persistent\EEPSaveGameHeader.h (RECONSTRUCTION; provisional names). The header of a
// saved game: name, description, save version, the game options, the scenario's media strings. EEPSaveGameHeader.cpp
// is 0x14b7a5..0x14bd64 (vtable 0x4422a8: FSRestorable's four slots). sizeof 0x500. Names are provisional (the keys
// and the default name strings give some).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, ee/EEGameOptions.h, ee/EESMMediaStrings.h, <vector>.
#ifndef EE_EEPSAVEGAMEHEADER_H
#define EE_EEPSAVEGAMEHEADER_H

class EEPSaveGameHeader : public FSRestorable {
public:
    EEPSaveGameHeader();                                                        // 0x14b8f4
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // +0x00 (empty: ICF-folded 0x157269)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x14b993
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x14bb9a

    long LoadHeader(FSFile& inFile);                                            // 0x14b9b2
    long LoadBody(FSFile& inFile);                                              // 0x14ba08
    long LoadFromFile(FSFile& inFile);                                          // 0x14bb26
    long SaveHeader(FSFile& outFile);                                           // 0x14bbb9
    long SaveBody(FSFile& outFile);                                             // 0x14bc02

    static const UString sMajorVersion;     // "EEPSaveGameHeader-MajorVersion" (guard bit 1)
    static const UString sMinorVersion;     // bit 2
    static const UString sNumChunks;        // bit 4
    static const UString sChunkID;          // bit 8
    static const UString sCompressionType;  // bit 0x10

    // inline accessors (the SCL load event's inline budget needs these five expansions; provisional names)
    void SetMajorVersion(unsigned long inVersion) { this->mMajorVersion = inVersion; }
    void SetMinorVersion(unsigned long inVersion) { this->mMinorVersion = inVersion; }
    const EESMMediaStrings& GetMediaStrings() const { return this->mMediaStrings; }

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
