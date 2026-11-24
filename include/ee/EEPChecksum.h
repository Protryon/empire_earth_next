// C:\EEX\Empire Earth\Persistent\EEPChecksum.h (RECONSTRUCTION; layout proven: EEPChecksum sizeof 0x20 = its new-site
// size, ctor 0x149d05 exact in EEPChecksum.cpp; member names are provisional). A saved checksum table
// (RTTI EEPChecksum > ESChecksum, FSPersistent@0x18; vtables 0x442254 (ESChecksum: the dtor) and 0x442234
// (FSPersistent, tag 1)). EEPChecksum.cpp starts at 0x149c3c (three class static keys, guard byte 0x531a28).
// sizeof 0x20 (CreateDefault's `new`). Names are provisional.
// Requires lle/LowLevelEngine.h (ESChecksum), lle/FSRestorable.h, lle/FSPersistent.h.
#ifndef EE_EEPCHECKSUM_H
#define EE_EEPCHECKSUM_H
#include "rust/base/error.h"
#include "rust/game/gamecpp.h"
#include "ee/EEPersistentTag.h"

class EEPChecksum : public ESChecksum, public FSPersistent {
public:
    // Creates an empty saved checksum chunk, verbose or not
    EEPChecksum(bool inVerbose) : ESChecksum(inVerbose), FSPersistent(kPersistentChecksum) {  }
    virtual ~EEPChecksum() {}                                                   // 0x149fdc (??_G 0x149fc0; the
                                                                                // FSPersistent slot's adjustor 0x149fb8)
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // FSPersistent +0x00 0x149d5f
    // Reads the verbose flag and the described checksum entries from a saved game; returns the first read error
    // +0x04 0x149d9b
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_pc_load(static_cast<FSPersistent*>(this), &inFile); }
    // Writes the verbose flag and the described checksum entries to a save; returns the first write error
    // +0x0c 0x149e5c
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_pc_save(static_cast<FSPersistent*>(this), &outFile); }
    // Creates an empty verbose saved checksum chunk (persistent factory)
    // +0x14 0x149d3c
    virtual FSPersistent* CreateDefault() const { return new EEPChecksum(true); }
    // the Rust set (ee_game gamecpp persist_parts.rs reads and fills it)
    EeErrorChecksum* GetRust() const { return this->mRust; }

};

#endif
