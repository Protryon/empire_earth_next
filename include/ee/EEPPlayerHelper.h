// C:\EEX\Empire Earth\Persistent\EEPPlayerHelper.h (RECONSTRUCTION; layout proven: EEPPlayerHelper sizeof 0x24 = its
// new-site size, ctor 0x14b339 exact in EEPPlayerHelper.cpp; member names are provisional). The saved players
// (RTTI EEPPlayerHelper > FSPersistent; vtable 0x442288, tag 5). EEPPlayerHelper.cpp is 0x14b22d..0x14b7a5
// (__FILE__ 0x4c17c0). sizeof 0x24 (CreateDefault's `new`). Names are provisional (the checksum key
// names mLocalPlayerIndex).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h, <vector>.
#ifndef EE_EEPPLAYERHELPER_H
#define EE_EEPPLAYERHELPER_H

#include "rust/game/gamecpp.h"
#include "lle/FSPersistent.h"
#include "rust/sim/game_ids.h"
class EEPlayer;
class EEWorld;
class EEGameState;

class EEPPlayerHelper : public FSPersistent {
public:
    EEPPlayerHelper() : FSPersistent(kPersistentPlayers) { ee_gamecpp_pph_init(this); }
    EEPPlayerHelper(EEWorld& inWorld) : FSPersistent(kPersistentPlayers) { ee_gamecpp_pph_from_world(this, &inWorld); }
    // implicit virtual dtor: ??_G +0x10 0x14b380
    // +0x00 0x14b446
    virtual void GetChecksum(ESChecksum& ioChecksum) const { ee_gamecpp_pph_checksum(this, &ioChecksum); }
    // +0x04 0x14b4e1
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_pph_load(this, inMajorVersion, inMinorVersion, &inFile); }
    // +0x0c 0x14b6d0
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_pph_save(this, inMajorVersion, inMinorVersion, &outFile); }
    // +0x14 0x14b42f
    virtual FSPersistent* CreateDefault() const { return new EEPPlayerHelper; }
    // 0x14b4a3 (13 until loaded)
    long GetLocalPlayerIndex(long& outIndex) { return ee_gamecpp_pph_local_index(this, &outIndex); }

    // class static keys (guard byte 0x531b00, bits 1..8)

    std::vector<EEPlayer*> mPlayers;        // +0x08
    unsigned long mLocalPlayerIndex;        // +0x18 (EEWorld +0xb84 mCurrentPlayer; Read/Write(unsigned long&))
    unsigned long mMaxPlayers;              // +0x1c (3; EEWorld +0xb8c; Read/Write(unsigned long&))
    bool mIsForLoading;                     // +0x20 (true; false for the world ctor: Load only reads when set)
    bool mLoaded;                           // +0x21 (Load sets it per player read)
};

#endif
