// C:\EEX\Empire Earth\AI Thread\EETown.h (RECONSTRUCTION; file name inferred). A player's town: an AI map area (RTTI
// EETown > EEAIArea @0; vtable 0x43c57c, 11 slots: EEAIArea's with its own ??_G 0xabbf4 and slot +0x20 0xac203, which
// EEHerd shares by ICF). EETown.cpp 0xabbaf..0xac7b9: ctor 0xabbaf, dtor 0xabc10. The owner keeps its towns in EEPlayer
// +0x7cc.
// Port (the AI Thread cluster, round 2): the bodies are Rust's (ee_sim::cp town.rs); the counters, continent, reach and
// flags are the record EeTownFields the class derives from (the original's names), the owner and the patrol group stay
// typed members here. The members forward to the Rust; EETown.cpp is the town's host (ee_cp_town_host).
// Requires ee/EEAIArea.h.
#ifndef EE_EETOWN_H
#define EE_EETOWN_H
#include "rust/sim/units.h"

#include "rust/sim/cp.h"             // EeTownFields

class EEPlayer;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class EEUnit;

class EETown : public EEAIArea, public EeTownFields {
public:
    // 0xabbaf: EEAIArea::mAreaType = 5 (the town kind); its only caller, EEPlayer 0x163138, passes (this, ++a town counter)
    EETown(EEPlayer* inPlayer, long inTownID) : EeTownFields(), mPlayer(inPlayer), mPatrolGroup(NULL) { ee_cp_town_construct(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this), inTownID); }
    // 0xabc10 (??_G 0xabbf4): Reset, then ~EEAIArea
    ~EETown() { this->Reset(); }
    // +0x20 0xac203: 16 tries of a random tile in the hull's bounds on the map and in the hull; else EEAIArea's
    U2DPoint<long> GetRandomPoint() { EeHerdPoint p = ee_cp_town_random_point(this, static_cast<EEAIArea*>(this), this); return U2DPoint<long>(p.mX, p.mY); }
    // 0xabc27: a building joins (its tile, the counters, the patrol group)
    bool AddUnit(EEUnit* inUnit) { return ee_cp_town_add_unit(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this), (uint64_t)(uintptr_t)inUnit); }
    // 0xabf8f: counters 0, mReach = 0x100, the patrol group released
    void Reset() { ee_cp_town_reset(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this)); }
    // 0xabede: inOther's tiles and counters added
    void Merge(EETown* inOther) { ee_cp_town_merge(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this), (uint64_t)(uintptr_t)inOther, static_cast<EeTownFields*>(inOther)); }
    // 0xabfda: mNearWater/mNearMapEdge worked out; returns mNearWater
    bool UpdateNearWater() { return ee_cp_town_update_near_water(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this)); }
    // 0xac351: the mirror of AddUnit
    bool RemoveUnit(EEUnit* inUnit) { return ee_cp_town_remove_unit(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this), (uint64_t)(uintptr_t)inUnit); }
    // 0xac6f7: whether inUnit belongs to this town
    bool CanTake(EEUnit* inUnit) { return ee_cp_town_can_take(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this), (uint64_t)(uintptr_t)inUnit); }
    // 0xac4b6: mPatrolGroup found or made, its points the hull's
    void RefreshPatrolGroup() { ee_cp_town_refresh_patrol_group(static_cast<void*>(this), static_cast<void*>(static_cast<EEAIArea*>(this)), static_cast<EeTownFields*>(this)); }
    static bool IsTownUnit(EEUnit* inUnit) { return ee_cp_town_is_town_unit((uint64_t)(uintptr_t)inUnit); }

    EEPlayer* GetPlayer() const { return this->mPlayer; }
    EEPlayer* mPlayer;                      // +0x68 (ctor argument 1)
    EEPPatrolGroup* GetPatrolGroup() const { return this->mPatrolGroup; }
    EEPPatrolGroup* mPatrolGroup;           // +0x6c (AddUnit makes one and registers it with mPlayer; Reset releases it)
    unsigned short GetContinent() const { return this->mContinent; }
};

#endif
