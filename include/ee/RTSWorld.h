// C:\EEX\RTS Engine\RTS Engine\RTSWorld.h (RECONSTRUCTION; file name inferred). Base of the game world (vtable
// 0x4421bc; RTSWorld.cpp, __FILE__ 0x4c1304: ctor 0x147dfd, dtor 0x147eb6, slots 0x147e9a..0x148202). sizeof 0xd8.
// Layout from the ctor/dtor; every member name is provisional unless an assert names it. Virtual
// slots are named by vtable offset (signatures from their `ret N`; inferred).
// Requires lle/LowLevelEngine.h (GETransformation, GE3DLine, TSReadWriteLock), <vector>.
#ifndef EE_RTSWORLD_H
#define EE_RTSWORLD_H

#include <vector>
#include "lle/GETransformation.h"
#include "lle/GE3DLine.h"
#include "ee/RTSDummyHeader.h"
#include "rust/game/rts.h"

class ESChecksum;
class EEMap;
class EEPlayer;
class RTSUnit;
class TSReadWriteLock;


class RTSWorld {
public:
    RTSWorld() { ee_rts_world_construct(this); }
    // RTSWorld.cpp's non-virtual members (provisional names unless stated)
    // 0x148024: index in mPlayers[0..mNumPlayers), -1 if none
    long GetPlayerIndex(EEPlayer* inPlayer) { return ee_rts_world_player_index(this, inPlayer); }
    unsigned long GetNumPlayers() const { return this->mNumPlayers; }  // (the diplomacy form reads it through a temporary, EEUIEDSOpen 0x1d4165)
    EEPlayer* GetPlayer(unsigned long inIndex) const { return this->mPlayers[inIndex]; }  // (likewise)
    // 0x14804f: the top byte of the id is the player
    RTSUnit* GetUnit(unsigned long inUnitID) { return static_cast<RTSUnit*>(ee_rts_world_unit(this, inUnitID)); }
    // 0x148072 (no arguments; EESELoadThread Handle403 under the write lock; provisional name)
    void FlushQueuedPlacements() { ee_rts_world_flush_queued_placements(this); }
    // 0x1480ad: references the unit, push_back into mU004
    void AddUnitEntry(RTSUnit* inUnit, RTSUnit* inInsertAfter)
        { ee_rts_world_add_unit_entry(this, inUnit, inInsertAfter); }
    // 0x1481c9: deletes the old player unless it is the same
    void SetPlayer(long inIndex, EEPlayer* inPlayer) { ee_rts_world_set_player(this, inIndex, inPlayer); }
    // +0x00 (??_G 0x147e9a; dtor 0x147eb6)
    virtual ~RTSWorld() { ee_rts_world_destruct(this); }
    // +0x04 0x147eee (EEWorld inherits it; the per-player limit mPlayerPopulationLimit)
    virtual void UpdatePlayerPopulationLimit(bool inRecount) { ee_rts_world_update_population_limit(this, inRecount); }
    // +0x08 0x147f71 (deletes the players and the map)
    virtual void Clear() { ee_rts_world_clear(this); }
    // +0x0c 0x147faf ("RTSWorld", mNumPlayers)
    virtual void GetChecksum(ESChecksum& ioChecksum) { ee_rts_world_checksum(this, &ioChecksum); }
    // +0x10 0x1480d1
    virtual void Reset() { ee_rts_world_reset(this); }
    virtual void ResortUnit(RTSUnit* inUnit) = 0;   // +0x14 (EEWorld 0x13c105: a placed unit changed column)
    virtual void SetCameraTransformation(const GETransformation& inTransformation);  // +0x18 0x14810b (the camera: mTransformation, mLine)
    // +0x1c 0x1481ee (then Virtual04(true))
    virtual void SetNumPlayers(unsigned long inNumPlayers) { ee_rts_world_set_num_players(this, inNumPlayers); }
    // inline (its refused copy, COMDAT 0x11428f `mov eax, [ecx+0xbc]`, closes EEROTerrain.cpp: the coloured
    // rectangles' erase; provisional name)
    EEMap* GetMap() { return this->mMap; }
    // +0x20 0x148202: mPopulationLimit = inLimit
    virtual void SetPopulationLimit(unsigned long inLimit) { this->mPopulationLimit = inLimit; }
    virtual void CameraChanged(const GETransformation& inTransformation) = 0;  // +0x24 (0x14810b passes its argument)
    virtual void PlaceQueuedUnits() = 0;    // +0x28
    virtual void GameSpeedChanged() = 0;    // +0x2c

    // +0x04 the placement queue (std::vector<RTSWorldUnitEntry>, 8-byte unit/insert-after pairs): Rust's (ee_game
    // rts world.rs QUEUED)
    RTSDummyUnit mUnits;                    // +0x14 (provisional name)
    GETransformation mTransformation;       // +0x1c
    // inline: EEPWorldHelper's world ctor copies it through the FPU (fld/fstp)
    float GetGameSpeed() const { return this->mGameSpeed; }

    GE3DLine mLine;                         // +0x5c
    EEPlayer* mPlayers[17];                 // +0x74 (all zeroed by one `rep stosd` of 17 dwords; indexed by
                                            // EEWorld::mCurrentPlayer; element type inferred: probably RTSPlayer*)
    TSReadWriteLock* mLock;                 // +0xb8 (named by the ctor's CreateReadWriteLock assert)
    EEMap* mMap;                            // +0xbc (deleted through its vtable slot +0x10 by the dtor)
    float mGameSpeed;                       // +0xc0 (= 1.0; EEPWorldHelper's "TheGameSpeed")
    float mCameraYaw;                       // +0xc4 (= 0)
    float GetCameraYaw() const { return this->mCameraYaw; }  // inline (camera yaw read as a value before the arithmetic: EEE3DCommandConfirmation 0x4ea201) (provisional name)
    float mCameraPitch;                     // +0xc8 (= 0)
    float GetCameraPitch() const { return this->mCameraPitch; }  // inline (with GetCameraYaw) (provisional name)
    unsigned long mNumPlayers;              // +0xcc (= 0)
    long mPopulationLimit;                  // +0xd0 (= 800; EEPWorldHelper's "ThePopulationLimit")
    long mPlayerPopulationLimit;            // +0xd4 (not set by the ctor; read with fimul)
};

#endif
