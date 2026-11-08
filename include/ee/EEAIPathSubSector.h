// C:\EEX\Empire Earth\...\EEAIPathSubSector.h (RECONSTRUCTION; file path inferred). A path
// sub-sector of the AI's continent map (RTTI EEAIPathSubSector > AStarGraphableObjectBase @0; vtable 0x43a538, one
// slot ??_G 0x2beec). EEAIContinentManager::GetPathSubSectorAt 0x273c9 returns it. Only the members a decompiled unit
// calls are declared; layout from the ctor 0x2be88 (0x1a99b walks the pointer vector at +0x30/+0x34).
// Requires ee/AStarGraphableObjectBase.h, lle/U2DPoint.h, lle/U2DSparseArray.h.
#ifndef EE_EEAIPATHSUBSECTOR_H
#define EE_EEAIPATHSUBSECTOR_H

#include <map>
#include <vector>

class EEAIPathSector;
class EEComplexUnit;
struct EECPASGFlags;

class EEAIPathSubSector : public AStarGraphableObjectBase {
public:
    // (EEAIPathSubSector.cpp 0x2be88..0x2c79f; provisional names): ctor 0x2be88 (init list, then the vptr), dtor
    // 0x2bf08 (??_G 0x2beec; the members' dtors inline, the base's vptr store last)
    EEAIPathSubSector();
    virtual ~EEAIPathSubSector();
    // 0x2bf45: the complex unit standing on mLocation that fits mKind (4, 5, 0xc, 0xd, 0xe: gates, walls, ...), 0 if none
    EEComplexUnit* GetBlockingUnit();
    // 0x2c0b9 (ret 8): appends the links listed under inID; always returns false
    bool GetLinks(unsigned short inID, std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >& outLinks);
    // 0x2c10f (ret 4): whether the gate (unit class category 0x19) on mTiles lets gWorld's player inPlayer through
    // (setrilo GatePasses; a missing or dead gate passes)
    bool IsPassable(long inPlayer);
    // 0x2c219 (ret 8): inOther is a neighbour and none of its links to this sub-sector is open for inIndex (body
    // not written: the links' +0x54 is a pointer there)
    bool AreLinksBlocked(EEAIPathSubSector* inOther, long inIndex);
    // (EECPAttackSubGroupUnit::ChooseTarget 0x63d50 passes the address of an EECPASGFlags its unit class filled: the
    // second argument is that struct)
    bool AreLinksBlocked(EEAIPathSubSector* inOther, const EECPASGFlags& inFlags);
    // 0x2c2c5 (ret 4): drops every link with inOther and inOther from mNeighbors; true if it was a neighbour
    bool RemoveNeighbor(EEAIPathSubSector* inOther);
    void RemoveLinks(unsigned short inID);  // 0x2c379 (ret 4): erases the links listed under inID
    void CreateTiles(const std::vector<U2DPoint<long> >& inTiles);   // 0x2c3bc: mTiles.Create(inTiles)

    // 0x1a99b (ret 4): whether inOther is in the neighbour vector (+0x30.._Last +0x34); provisional name. Inline
    // (its COMDAT follows its first user EEAIAStarPathGraphGoal::IsGoalNode)
    // the graph machine's view (EEAIAStarGraphMachine<.., EEAIPathSubSector>; 0xaad6c's count is the ICF COMDAT
    // 0x196dff, 0xab6c1 takes the node tile from mSector +0x40/+0x44) (provisional names)
    unsigned long GetNumNeighbors() const { return this->mNeighbors.size(); }
    U2DPoint<long> GetGraphLocation() const;
    bool IsNeighbor(EEAIPathSubSector* inOther)
    {
        std::vector<EEAIPathSubSector*>::iterator theEnd = this->mNeighbors.end();
        std::vector<EEAIPathSubSector*>::iterator it;
        for (it = this->mNeighbors.begin(); it != theEnd && *it != inOther; it++)
            ;
        return it != theEnd;
    }
    // inline (COMDAT 0x26430 in EEAIContinentManager.cpp's range): links listed under inID
    bool HasLinks(unsigned short inID) { return this->mLinks.find(inID) != this->mLinks.end(); }
    // inline (COMDAT 0x2640c, same range): mLocation = the tiles' center (provisional name)
    void UpdateLocation() { this->mLocation = this->mTiles.ComputeCenterPoint(true); }

    // +0x04: the links to other sub-sectors by their id (node 0x1c: word key +0x0c, two pointers +0x10/+0x14;
    // GetLinks/RemoveLinks take upper_bound, then lower_bound)
    std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > mLinks;
    U2DSparseArrayPointContainer mTiles;    // +0x10 (0x1c)
    std::vector<EEAIPathSubSector*> mNeighbors;   // +0x2c (IsNeighbor walks +0x30.._Last +0x34)
    U2DPoint<long> mLocation;               // +0x3c (GetBlockingUnit's tile)
    EEAIPathSector* mSector;                // +0x44 (EEPathManager::StartGraphPath 0xa69e2 reads its +0x40/+0x44)
    long mNumTiles;                         // +0x48 (not set by the ctor)
    unsigned short mContinentID;            // +0x4c
    unsigned short mPathAreaID;             // +0x4e
    unsigned short GetID() const { return this->mID; }
    unsigned short mID;                     // +0x50 (the manager's 0x24dcf indexes +0x5c by it)
    long GetKind() const { return this->mKind; }
    long mKind;                             // +0x54 (GetBlockingUnit's switch; 0x10465 compares it with 0xd)
    long mPlayerID;                         // +0x58
    bool mIsStale;                          // +0x5c
};

#endif
