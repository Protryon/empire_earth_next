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

#include "rust/sim/cp.h"             // EeTownFields

class EEPlayer;
typedef struct EeUnitsPatrolGroup EEPPatrolGroup;
class EEUnit;

class EETown : public EEAIArea, public EeTownFields {
public:
    // 0xabbaf: EEAIArea::mAreaType = 5 (the town kind); its only caller, EEPlayer 0x163138, passes (this, ++a town counter)
    EETown(EEPlayer* inPlayer, long inTownID);
    virtual ~EETown();                          // 0xabc10 (??_G 0xabbf4): Reset, then ~EEAIArea
    // +0x20 0xac203: 16 tries of a random tile in the hull's bounds on the map and in the hull; else EEAIArea's
    virtual U2DPoint<long> GetRandomPoint();
    bool AddUnit(EEUnit* inUnit);               // 0xabc27: a building joins (its tile, the counters, the patrol group)
    void Reset();                               // 0xabf8f: counters 0, mReach = 0x100, the patrol group released
    bool AddTile(const U2DPoint<long>& inTile); // 0xabdf3: a tile joins (the hull made again unless inside it)
    void Merge(EETown* inOther);                // 0xabede: inOther's tiles and counters added
    bool UpdateNearWater();                     // 0xabfda: mNearWater/mNearMapEdge worked out; returns mNearWater
    bool RemoveUnit(EEUnit* inUnit);            // 0xac351: the mirror of AddUnit
    bool RemoveHullTile(const U2DPoint<long>& inTile);   // 0xac573: a tile leaves (the hull made again if a vertex)
    bool CanTake(EEUnit* inUnit);               // 0xac6f7: whether inUnit belongs to this town
    void RefreshPatrolGroup();                  // 0xac4b6: mPatrolGroup found or made, its points the hull's
    static bool IsTownUnit(EEUnit* inUnit);

    EEPlayer* GetPlayer() const { return this->mPlayer; }
    EEPlayer* mPlayer;                      // +0x68 (ctor argument 1)
    EEPPatrolGroup* GetPatrolGroup() const { return this->mPatrolGroup; }
    EEPPatrolGroup* mPatrolGroup;           // +0x6c (AddUnit makes one and registers it with mPlayer; Reset releases it)
    unsigned short GetContinent() const { return this->mContinent; }
};

#endif
