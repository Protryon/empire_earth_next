// EEInfluenceTable.h (RECONSTRUCTION; file name inferred). Per map region (9x9 tiles) and player, the
// summed +0x244 value of the units there (RTTI EEInfluenceTable > RTSRegionTable<std::pair<unsigned long, long> >;
// vtable 0x43c41c, one slot ??_G 0xa55b1; sizeof 0x1c). Each region holds mNumPlayers + 1 (player, value) pairs, the last
// the total. EEWorld::CreateInfluenceTable 0x13a255 news it with (map, mNumPlayers). Names are provisional.
// Requires ee/RTSRegionTable.h, lle/U2DRectangle.h.
#ifndef EE_EEINFLUENCETABLE_H
#define EE_EEINFLUENCETABLE_H

class EEMap;
class EEUnit;

class EEInfluenceTable : public RTSRegionTable<std::pair<unsigned long, long> > {
public:
    EEInfluenceTable(EEMap* inMap, unsigned long inNumPlayers);   // 0xa5589
    virtual ~EEInfluenceTable();            // +0x00 0xa55cd (??_G 0xa55b1)
    void Init();                            // 0xa55d8: every region gets (i, 0) for i = 0..mNumPlayers
    // 0xa5685 (ret 8): the sum of GetValue over the rectangle (x from its left to its right, y from its top to its bottom)
    long SumValue(unsigned long inPlayer, const U2DRectangle<long>& inRect);
    long SumOthersValue(unsigned long inPlayer, const U2DRectangle<long>& inRect);  // 0xa56f6: the sum of GetOthersValue, clipped at 0
    // 0x24495a (ret 8; out of line right after its first user 0x24485b, EEFOFindNewTargetNearOldTarget.cpp's range):
    // SumOthersValue over the regions under inTiles (provisional name)
    long SumOthersValueOnTiles(unsigned long inPlayer, const U2DRectangle<long>& inTiles);
    long GetOthersValue(unsigned long inPlayer, unsigned long inX, unsigned long inY);  // 0xa57f9: the total minus inPlayer's
    // 0x42e47 (ret 8; outside this unit's range: ICF-folded with a twin) the value for inPlayer at the region of inTile
    // (EECPCombatManager 0x473fc6 adds it to each unit's weight) (provisional name)
    long GetTileValue(unsigned long inPlayer, const U2DPoint<long>& inTile);
    void AddUnit(EEUnit* inUnit, long inX, long inY);      // 0xa5839: + the unit's class +0x244 for its owner and the total
    void RemoveUnit(EEUnit* inUnit, long inX, long inY);   // 0xa58a7: the same subtracted, clamped at 0
    // 0xa5920: the values at inTile of the players inPlayer is at war with (player 0 only if inWithFirst)
    long GetEnemyValue(unsigned long inPlayer, const U2DPoint<long>& inTile, bool inWithFirst);
    long SumEnemyValue(unsigned long inPlayer, const U2DRectangle<long>& inRect, bool inWithFirst);  // 0xa598d: over a rectangle
    // inline, never expanded (COMDAT 0x81a71, first emitted by EECPIntelligence.cpp): inPlayer's value in region (inX, inY)
    long GetValue(unsigned long inPlayer, unsigned long inX, unsigned long inY)
    {
        std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(inX, inY);
        // an invalid player (the route planner passes -1 for a task without a player) has no entry: value 0
        // instead of reading the heap header in front of the region's table
        return theRegion != NULL && inPlayer < theRegion->size() ? (*theRegion)[inPlayer].second : 0;
    }
    // inline (COMDAT 0xff19 after its first user, EEAIGPGModiferUseInfluenceCostsBase::Virtual0C 0xfec1; ret 8):
    // inPlayer's value in region inRegion, through the out-of-line GetRegion (provisional name)
    long GetValue(unsigned long inPlayer, const U2DPoint<long>& inRegion)
    {
        std::vector<std::pair<unsigned long, long> >* theRegion = this->GetRegion(inRegion);
        // an invalid player (the route planner passes -1 for a task without a player) has no entry: value 0
        // instead of reading the heap header in front of the region's table
        return theRegion != NULL && inPlayer < theRegion->size() ? (*theRegion)[inPlayer].second : 0;
    }

    unsigned long mNumPlayers;              // +0x18 (ctor argument 2, stored after the vptr)
};

#endif
