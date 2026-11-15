// C:\EEX\Empire Earth\Persistent\EEPChecksum.h (RECONSTRUCTION; layout proven: EEPChecksum sizeof 0x20 = its new-site
// size, ctor 0x149d05 exact in EEPChecksum.cpp; member names are provisional). A saved checksum table
// (RTTI EEPChecksum > ESChecksum, FSPersistent@0x18; vtables 0x442254 (ESChecksum: the dtor) and 0x442234
// (FSPersistent, tag 1)). EEPChecksum.cpp starts at 0x149c3c (three class static keys, guard byte 0x531a28).
// sizeof 0x20 (CreateDefault's `new`). Names are provisional.
// Requires lle/LowLevelEngine.h (ESChecksum), lle/FSRestorable.h, lle/FSPersistent.h.
#ifndef EE_EEPCHECKSUM_H
#define EE_EEPCHECKSUM_H

class EEPChecksum : public ESChecksum, public FSPersistent {
public:
    EEPChecksum(bool inVerbose);                                                // 0x149d05
    virtual ~EEPChecksum() {}                                                   // 0x149fdc (??_G 0x149fc0; the
                                                                                // FSPersistent slot's adjustor 0x149fb8)
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // FSPersistent +0x00 0x149d5f
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x149d9b
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x149e5c
    virtual FSPersistent* CreateDefault() const;                                // +0x14 0x149d3c

    static const UString sNumChecksums;     // "EEPChecksum-NumChecksums" (guard bit 1)
    static const UString sChecksum;         // bit 2
    static const UString sVerboseMode;      // bit 4
};

#endif
