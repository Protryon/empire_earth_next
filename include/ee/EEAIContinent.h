// C:\EEX\Empire Earth\...\EEAIContinent.h (RECONSTRUCTION; file path inferred). One continent
// of the AI's continent map (RTTI EEAIContinent > EEAIArea @0, AStarGraphableObjectBase @0x68; vtables 0x43a1c8
// (EEAIArea part, 14 slots) and 0x43a1c0 (graph part: ??_G thunk 0x1e602); sizeof 0x120). Ctor 0x1d846 (new site
// 0x156b2), dtor 0x1d94d (??_G 0x1d931 calls ??1). Owned by EEAIContinentManager::mContinents (index = mID).
// Slot signatures from the bodies (ret N, argument reads). Slot and member names are provisional. Ctor 0x1d846 (init
// list mCategory(false), mID(0) + the mCounts loop) and dtor 0x1d94d (empty body) prove the layout and the MI
// (bind_only 100).
// Requires ee/EEAIArea.h's engine headers and ee/EEAreaUtilities.h (EETileCoordinate).
#ifndef EE_EEAICONTINENT_H
#define EE_EEAICONTINENT_H

#include <vector>

#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"

class EEAIPathArea;

class EEAIContinent : public EEAIArea, public AStarGraphableObjectBase {
public:
    EEAIContinent();                        // 0x1d846
    virtual ~EEAIContinent();               // +0x00 dtor 0x1d94d
    // +0x1c 0x1da61: the EEAIArea slot (closest points)
    virtual bool GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther);
    // +0x2c 0x1dc54 (ret 8): reserves outTiles to mShoreTiles's size, then walks mShoreTiles (inID 0: every tile)
    virtual void GetShoreNextTo(unsigned short inID, std::vector<U2DPoint<long> >& outTiles);
    // +0x30 0x1e45e (ret 0xc): Virtual34 with the one-element id list { inID }
    virtual void GetShoreInRegion(unsigned short inID, std::vector<U2DPoint<long> >& outTiles, long inKind);
    // +0x34 0x1dccf (ret 0xc): outTiles is cleared first (0x288c2c); walks inIDs from the back
    // with inIDs empty or holding a 0, every tile; else the tiles whose path area's id (+0x8a for inArg3 3,
    // else +0x86) is listed, or, without one, a kind-1 continent's tiles next to a listed continent
    virtual void GetShoreInRegions(const std::vector<unsigned short>& inIDs, std::vector<U2DPoint<long> >& outTiles,
                           long inKind);

    void AddPathAreaID(unsigned short inID);   // 0x1d9e0: unless present (0x1e5bd), push_back + sort (0x1e6ac)
    EEAIPathArea* GetPathAreaAt(const U2DPoint<long>& inPoint);   // 0x1dbf4: as EEAIPathSector's
    void RemovePathAreaID(unsigned short inID);   // 0x1e4a1: lower_bound + erase when equal
    void AddChokePoint(long inIndex);       // 0x26d84: push_back + sort (EEAIContinentManager::AddChokePoint)
    void AddForestID(unsigned short inID);  // 0x26dca: push_back + sort (0x208cc, the forest's +0x82)
    void AddHillID(unsigned short inID);    // 0x26e13: push_back + sort (0x2033f, the hill's +0x6a)
    void RemoveForestID(unsigned short inID);   // 0x26ea4 (EEAIContinentManager 0x25325) (provisional name)
    // inline (COMDATs after their first user EEAIAStarPathAreaGoal::AssignKindIDs 0x17a4b; bodies in
    // EEAIAStarPathAreaGoal.cpp): push_back unless std::find finds it (provisional names)
    void AddIDBC(unsigned short inID);      // 0x188e0
    void AddIDCC(unsigned short inID);      // 0x1887f
    void AddIDDC(unsigned short inID);      // 0x189a2
    void AddIDEC(unsigned short inID);      // 0x18941
    // inline (COMDAT 0x26e7b in EEAIContinentManager.cpp's range): inContinent is in mNeighbors
    bool HasNeighbor(EEAIContinent* inContinent)
    {
        std::vector<EEAIContinent*>::iterator theEnd = this->mNeighbors.end();
        std::vector<EEAIContinent*>::iterator theIt;
        for (theIt = this->mNeighbors.begin(); theIt != theEnd && *theIt != inContinent; ++theIt) {
        }
        return theIt != theEnd;
    }
    // the graph machine's view (EEAIAStarGraphMachine<.., EEAIContinent>): out-of-line COMDAT 0x275eb
    // (`jbe`: unsigned) and the hull's center as the node tile (0x28aa7 reads +0x44/+0x48) (provisional names)
    unsigned long GetNumNeighbors() const { return this->mNeighbors.size(); }
    const U2DPoint<long>& GetGraphLocation() const { return this->GetHullCenter(); }

    // New (port): the members behind accessors (R5b.1 blocker (b))
    const std::vector<U2DPoint<long> >& GetShoreTiles() const { return this->mShoreTiles; }
    void AddShoreTile(const U2DPoint<long>& inTile) { this->mShoreTiles.push_back(inTile); }
    void ClearShoreTiles() { this->mShoreTiles.clear(); }
    const std::vector<long>& GetChokePoints() const { return this->mChokePoints; }
    long GetChokePointsAt(unsigned long inIndex) const { return this->mChokePoints[inIndex]; }
    const std::vector<unsigned short>& GetForestIDs() const { return this->mForestIDs; }
    const std::vector<unsigned short>& GetPathAreaIDs() const { return this->mPathAreaIDs; }
    const std::vector<unsigned short>& GetHillIDs() const { return this->mHillIDs; }
    const std::vector<unsigned short>& GetIDsBC() const { return this->mIDsBC; }
    unsigned short GetIDsBCAt(unsigned long inIndex) const { return this->mIDsBC[inIndex]; }
    const std::vector<unsigned short>& GetIDsCC() const { return this->mIDsCC; }
    const std::vector<unsigned short>& GetIDsDC() const { return this->mIDsDC; }
    unsigned short GetIDsDCAt(unsigned long inIndex) const { return this->mIDsDC[inIndex]; }
    const std::vector<unsigned short>& GetIDsEC() const { return this->mIDsEC; }
    const std::vector<EEAIContinent*>& GetNeighbors() const { return this->mNeighbors; }
    void AddNeighbor(EEAIContinent* inContinent) { this->mNeighbors.push_back(inContinent); }
    void ClearNeighbors() { this->mNeighbors.clear(); }
    unsigned short GetCount(long inKind) const { return this->mCounts[inKind]; }
    void SetCount(long inKind, unsigned short inCount) { this->mCounts[inKind] = inCount; }
    unsigned char GetCategory() const { return this->mCategory; }  // inline (GetRandomContinent 0x422dc8) (provisional name)
    void SetCategory(unsigned char inCategory) { this->mCategory = inCategory; }
    unsigned short GetID() const { return this->mID; }
    void SetID(unsigned short inID) { this->mID = inID; }
    // the lists in a saved game (EEAIContinentSave.cpp's writer and reader: Points, Longs, Shorts), in this order
    template <class W> void WriteLists(W& ioWriter) const
    {
        ioWriter.Points(this->mShoreTiles);
        ioWriter.Longs(this->mChokePoints);
        ioWriter.Shorts(this->mForestIDs);
        ioWriter.Shorts(this->mPathAreaIDs);
        ioWriter.Shorts(this->mHillIDs);
        ioWriter.Shorts(this->mIDsBC);
        ioWriter.Shorts(this->mIDsCC);
        ioWriter.Shorts(this->mIDsDC);
        ioWriter.Shorts(this->mIDsEC);
    }
    template <class R> bool ReadLists(R& ioReader)
    {
        return ioReader.Points(this->mShoreTiles) && ioReader.Longs(this->mChokePoints)
            && ioReader.Shorts(this->mForestIDs) && ioReader.Shorts(this->mPathAreaIDs) && ioReader.Shorts(this->mHillIDs)
            && ioReader.Shorts(this->mIDsBC) && ioReader.Shorts(this->mIDsCC) && ioReader.Shorts(this->mIDsDC)
            && ioReader.Shorts(this->mIDsEC);
    }

private:
    // +0x6c (0x208cc/0x1fa5c push_back tiles; 8-byte elements, sar 3). U2DPoint<long> (GetClosestPoints 0x1da61 is
    // EEAIArea's 0xd8fe over it, the manager's 0x22ad9 takes its elements as EETileLocation)
    std::vector<U2DPoint<long> > mShoreTiles;
    std::vector<long> mChokePoints;         // +0x7c EEAIContinentManager::mChokePoints indices (push_back<long>)
    std::vector<unsigned short> mForestIDs; // +0x8c
    std::vector<unsigned short> mPathAreaIDs;  // +0x9c
    std::vector<unsigned short> mHillIDs;   // +0xac
    std::vector<unsigned short> mIDsBC;     // +0xbc (0x188e0, from the path-area unit's 0x17a4b, as are the next three)
    std::vector<unsigned short> mIDsCC;     // +0xcc (0x1887f)
    std::vector<unsigned short> mIDsDC;     // +0xdc (0x189a2)
    std::vector<unsigned short> mIDsEC;     // +0xec (0x18941)
    std::vector<EEAIContinent*> mNeighbors; // +0xfc the adjacent continents (the chain machine's graph edges)
    unsigned short mCounts[7];              // +0x10c (ctor body `for (i = 6; i >= 0; i--) mCounts[i] = 0;`, proven)
    unsigned char mCategory;                // +0x11a kind (byte; the factory copies its +0x50 byte; compared with 1, 2, 4 at 0xabfda/0xac2b6)
    unsigned short mID;                     // +0x11c the id (EEAIContinentManager::FindContinentID returns it)
};

#endif
