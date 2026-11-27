// C:\EEX\Empire Earth\Persistent\EEPMultiplayerHelper.h (RECONSTRUCTION; layout proven: EEPMultiplayerHelper sizeof 0x8
// = its new-site size, ctor 0x14ae4d exact in EEPMultiplayerHelper.cpp; member names are provisional). The saved
// multiplayer state (RTTI EEPMultiplayerHelper > FSPersistent; vtable 0x44225c, tag 8; no members: sizeof 8).
// EEPMultiplayerHelper.cpp is 0x14ac35..0x14b22d. Load/Save save the synchronous communications' tick counters
// (gCommunications, dynamic_cast to the server/client); provisional names (the keys).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h.
#ifndef EE_EEPMULTIPLAYERHELPER_H
#define EE_EEPMULTIPLAYERHELPER_H

#include "rust/game/gamecpp.h"
#include "ee/EEPersistentTag.h"
#include "lle/FSPersistent.h"
class EEPMultiplayerHelper : public FSPersistent {
public:
    // Creates the saved multiplayer-state chunk
    EEPMultiplayerHelper() : FSPersistent(kPersistentMultiplayer) {  }
    // Destroys the saved multiplayer-state chunk
    // +0x10 0x14ae79 (??_G 0x14ae5d)
    virtual ~EEPMultiplayerHelper() {  }
    // Reads the physics state and the eight network tick counters; restores only the server's or client's physics tick
    // from them (the other counters are read and dropped); returns the first read error
    // +0x04 0x14ae9e
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_pmh_load(inMajorVersion, inMinorVersion, &inFile); }
    // Writes the physics state and the server's or client's network tick counters to a save (nothing for versions up to
    // 0.12); returns the first write error
    // +0x0c 0x14aff1
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_pmh_save(inMajorVersion, inMinorVersion, &outFile); }
    // Creates an empty saved multiplayer-state chunk (persistent factory)
    // +0x14 0x14ae87
    virtual FSPersistent* CreateDefault() const { return new EEPMultiplayerHelper; }

    // class static keys (guard byte 0x531ab8, bits 1..0x80)
};

#endif
