// C:\EEX\Empire Earth\AI Thread\EETown.h (RECONSTRUCTION; file name inferred). A player's
// town: an AI map area (RTTI EETown > EEAIArea @0; vtable 0x43c57c, 11 slots: EEAIArea's with its own ??_G 0xabbf4 and
// slot +0x20 0xac203, which EEHerd shares by ICF). EETown.cpp 0xabbaf..0xac7b9: ctor 0xabbaf (EEAIArea ctor, then the
// members, then 0xabf8f), dtor 0xabc10. The owner keeps its towns in EEPlayer +0x7cc (EECPILocation::Update 0x80a57
// walks them). Only the members the ctor and 0xabc27 prove; names are provisional.
// Requires ee/EEAIArea.h.
#ifndef EE_EETOWN_H
#define EE_EETOWN_H

class EEPlayer;
class EEPPatrolGroup;
class EEUnit;

class EETown : public EEAIArea {
public:
    // 0xabbaf: EEAIArea::mAreaType = 5 (the town kind); its only caller, EEPlayer 0x163138, news 0x9c bytes and passes
    // (this, ++DAT_5318ac: a town counter)
    EETown(EEPlayer* inPlayer, long inTownID);
    virtual ~EETown();                          // 0xabc10 (??_G 0xabbf4): Reset, then ~EEAIArea
    // +0x20 0xac203 (ret 4, hidden result): 16 tries of a random tile inside the hull's bounds (+0x34..+0x40) that
    // is on the map (EEMap::GetTileAtLocation) and in the hull; else EEAIArea::GetRandomPoint
    virtual U2DPoint<long> GetRandomPoint();
    // 0xabc27 (ret 4, bool): adds a unit: mL78 += its +0x2c object's +0x244 value, the kind counters; makes mPatrolGroup
    bool AddUnit(EEUnit* inUnit);
    void Reset();                               // 0xabf8f (ctor and dtor): counters 0, mL94 = 0x100, mPatrolGroup released
    // 0xabdf3 (ret 4, true): inserts a tile (sets mContinent from the first one); the hull is rebuilt unless the tile is inside
    // it and not one of its vertices
    bool AddTile(const U2DPoint<long>& inTile);
    void Merge(EETown* inOther);                // 0xabede: inOther's tiles and counters added
    bool UpdateNearWater();                     // 0xabfda (EEPlayer 0x168115; mB98/mNearMapEdge recomputed; returns mB98)
    void FindPatrolGroup();                     // 0xac116: adopts a patrol group of an own unit (kind 0xdf) on a hull vertex
    bool RemoveUnit(EEUnit* inUnit);            // 0xac351 (ret 4): the mirror of AddUnit
    // 0xac573 (ret 4): removes a tile (not EEAIArea::RemoveTile: the vtable keeps the base slot); hull rebuilt if it was a vertex
    bool RemoveHullTile(const U2DPoint<long>& inTile);
    bool CanTake(EEUnit* inUnit);               // 0xac6f7 (ret 4): whether inUnit belongs to this town
    void RefreshPatrolGroup();                  // 0xac4b6 (EEPlayer 0x1646e7/0x169509/0x16be34; mPatrolGroup rebuilt)
    static bool IsTownUnit(EEUnit* inUnit);     // 0xac2b6 (cdecl, 1 argument; EEPlayer 0x16316e; type inferred)

    EEPlayer* GetPlayer() const { return this->mPlayer; }
    EEPlayer* mPlayer;                      // +0x68 (ctor argument 1)
    EEPPatrolGroup* GetPatrolGroup() const { return this->mPatrolGroup; }
    EEPPatrolGroup* mPatrolGroup;           // +0x6c (0: 0xabc27 creates one and registers it with mPlayer; Reset Dereferences it)
    unsigned short GetContinent() const { return this->mContinent; }
    unsigned short mContinent;              // +0x70 (ctor 0; 0xabfda passes it to gContinentManager 0x21e22 as a sector id)
    long GetTownID() const { return this->mTownID; }
    long mTownID;                           // +0x74 (ctor argument 2)
    unsigned long mValue;                   // +0x78 (0xabc27 adds the unit's value; unsigned: sorts 0x47db9c ja / 0x45a8ad jb)
    long mNumWalls;                         // +0x7c
    long mL80;                              // +0x80
    long mNumWonders;                       // +0x84
    long mNumDocks;                         // +0x88
    long mNumAirports;                      // +0x8c
    long mNumUnits;                         // +0x90
    long mReach;                            // +0x94 (Reset 0x100; 0xac6f7 compares a count with it)
    bool mNearWater;                        // +0x98 (ctor false; 0xabfda: a unit within distance^2 100)
    bool mNearMapEdge;                      // +0x99 (ctor false)
    // sizeof 0x9c (the only new, EEPlayer 0x16320c). The counters +0x78..+0x90 are signed (jle/dec in 0xac351).
};

#endif
