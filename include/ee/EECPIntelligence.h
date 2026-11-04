// C:\EEX\Empire Earth\AI Thread\EECPIntelligence.h (RECONSTRUCTION; file name inferred; for the EECPCombatStrategy
// unit; for the EECPIntelligence unit 0x80c69..0x84d8d).
// EECPIntelligencePlayerFile (RTTI, no base; vtable 0x43c214: the destructor only; sizeof 0xb4: new 0xb4 in 0x81da1;
// ctor 0x80c69(inOwner -> +0x48, inPlayer -> +0x44)): what a computer player knows about one player: its armies, unit
// type and family counts, the bounds of its units, and summed strengths. Names are provisional.
// Requires <vector>, <map>.
#ifndef EE_EECPINTELLIGENCE_H
#define EE_EECPINTELLIGENCE_H

#include "ee/EEAIAStarStorage.h"     // EEAIAStarBounds (a member)

class EEPlayer;
class EEComputerPlayer;
class EEUnit;
class EEComplexUnit;
class EECPIArmy;
class EECPILocation;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
template <class T> class U2DPoint;

// 0x2492c2 (cdecl; outside this unit's range): the regions around region (inX, inY) within inRadius appended to
// outRegions (EECPIntelligencePlayerFile::GetInfluence 0x81a0d) (provisional name; signature inferred)
void Fn2492C2(long inX, long inY, long inRadius, std::vector<EETileCoordinate>& outRegions);
// the real signature (names.txt 2492c2, every AI caller's mangled call): the tiles are U2DPoint<long>;
// the EETileCoordinate form above stays for EEUCBuilding.cpp until its caller is retyped
void Fn2492C2(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles);

// A unit type's count in a player file (12 bytes; EECPIntelligencePlayerFile::AddUnitType 0x81b0e) (provisional names)
struct EECPIUnitTypeCount {
    EECPIUnitTypeCount() {}
    EECPIUnitTypeCount(const EECPIUnitTypeCount& inOther)
    {
        this->mUnitType = inOther.mUnitType;
        this->mCount = inOther.mCount;
        this->mFamily = inOther.mFamily;
    }
    EECPIUnitTypeCount& operator=(const EECPIUnitTypeCount& inOther)
    {
        this->mUnitType = inOther.mUnitType;
        this->mCount = inOther.mCount;
        this->mFamily = inOther.mFamily;
        return *this;
    }

    long mCount;                            // +0x00
    unsigned long mUnitType;                // +0x04 (the class's mUnitType)
    long mFamily;                           // +0x08 (the class's +0x1d0)
};

// A family's count in a player file (8 bytes; EECPIntelligencePlayerFile::AddFamily 0x81a6e; was pair<long, long>):
// like EECPIUnitTypeCount, its copies take the key first (copy ctor 0x83057: +0x04, then +0x00) (provisional names)
struct EECPIFamilyCount {
    EECPIFamilyCount() {}
    EECPIFamilyCount(const EECPIFamilyCount& inOther)
    {
        this->mFamily = inOther.mFamily;
        this->mCount = inOther.mCount;
    }
    EECPIFamilyCount& operator=(const EECPIFamilyCount& inOther)
    {
        this->mFamily = inOther.mFamily;
        this->mCount = inOther.mCount;
        return *this;
    }

    long mCount;                            // +0x00
    long mFamily;                           // +0x04 (the class's +0x1d0)
};

class EECPIntelligencePlayerFile {
public:
    EECPIntelligencePlayerFile(EEPlayer* inOwner, EEPlayer* inPlayer);   // 0x80c69
    virtual ~EECPIntelligencePlayerFile();  // +0x00 (??_G 0x80cd5, dtor 0x80cf1)
    void AddToArmies(EEComplexUnit* inUnit);   // 0x80d19 (ret 4): into the nearest army, merging the ones it joins
    float GetScanRange(float inValue);      // 0x814bd (ret 4): inValue times the const 0x43c074 (EECPAttackSubGroup::TakeRequestedUnits) (provisional name)
    // 0x80f9b (ret 0xc): refreshes the file (at most every 30 s unless inForce; the armies only with inArmies)
    void Update(EEComputerPlayer* inPlayer, bool inForce, bool inArmies);
    void DeleteArmies();                    // 0x81438
    void Reset();                           // 0x81443 (the ctor and Update)
    long GetFamilyCount(long inFamily);     // 0x814ca (ret 4): the count of inFamily (+0x24 searched from the back)
    // 0x81500 (ret 0xc): the living units on the map (on inContinent unless 0) whose class passes CanMoveOnTerrain
    void GetUnits(std::vector<EEComplexUnit*>& outUnits, unsigned short inContinent, bool inWater);
    // 0x815bc (ret 8): the living units on the map (on inContinent unless 0)
    void GetAllUnits(unsigned short inContinent, std::vector<EEComplexUnit*>& outUnits);
    // 0x81654 (ret 8): the player's units (its buildings first, Virtual54) as locations, best target first
    void GetTargets(std::vector<EECPILocation>& outLocations, long inArg);
    long GetInfluence(long inX, long inY, long inRadius);   // 0x819c0 (ret 0xc): our influence around tile (inX, inY)
    void AddFamily(EEUnit* inUnit);         // 0x81aaa (ret 4)
    void AddUnitType(EEUnit* inUnit);       // 0x81b0e (ret 4)
    void AddUnitClass(EEUnit* inUnit);      // 0x81c42 (ret 4)
    // mArmies.size() out of line (ICF-folded into 0x91f11; EECPDefenseCommander 0x77e8e/0x79382) (provisional name)
    unsigned long GetNumArmies() const { return this->mArmies.size(); }

    std::vector<EECPIArmy*> mArmies;        // +0x04
    std::vector<EECPIUnitTypeCount> mUnitTypes;   // +0x14
    std::vector<EECPIFamilyCount> mFamilies;   // +0x24
    EEAIAStarBounds mBounds;                // +0x34 the bounds of the player's unit tiles
    EEPlayer* mPlayer;                      // +0x44 (the ctor's second argument)
    EEPlayer* mOwner;                       // +0x48 (the ctor's first argument)
    float mStrength;                        // +0x4c
    unsigned long mSeaStrength;             // +0x50 the summed +0x244 of the flag-4 classes
    unsigned long mLandStrength;            // +0x54 ... of the others
    unsigned long mAirStrength;             // +0x58 ... of the flag-1 classes
    unsigned long mSpaceStrength;           // +0x5c ... of the flag-8 classes
    unsigned long mNextUpdate;              // +0x60 (gPhysics's time + 30000)
    long mL64;                              // +0x64 (ctor 0)
    long mL68;                              // +0x68 (ctor 0)
    long mL6C;                              // +0x6c (ctor 0)
    long mNumBuildings;                     // +0x70 the category-3 classes without +0x310
    unsigned long mNumGatherers;            // +0x74 the workers (category 6 or 0x16)
    long mNumSeaUnits;                      // +0x78 (EECPCombatStrategy 0x771be: the second share's count) flag 4
    long mNumAirUnits;                      // +0x7c (the first share's count) flag 1
    long mNumLandUnits;                     // +0x80 (the third share's count) the others
    long mNumSpaceUnits;                    // +0x84 (the fourth share's count) flag 8
    long mVictoryPercent;                   // +0x88 the best victory-condition percentage of the player's teams
    long mL8C;                              // +0x8c
    long mTotalAttack;                      // +0x90 the summed stats +0xd8
    long mResources[7];                     // +0x94 the gatherers per resource (EEUnit::mResourceType of the resource)
    long mLB0;                              // +0xb0
};

// The mUnitTypes order of EECPIntelligencePlayerFile::Update: by the family's position in mFamilies, then by count
// (RTTI EEAIFOIntelligenceAIFamilyAndUnitTypeSorter, vtable 0x43c21c: the destructor only, ??_G 0x8141b; passed by
// value to sort). operator() 0x83977 is out of line. Names are provisional.
class EEAIFOIntelligenceAIFamilyAndUnitTypeSorter {
public:
    EEAIFOIntelligenceAIFamilyAndUnitTypeSorter(EECPIntelligencePlayerFile* inFile) : mFile(inFile) {}
    // user-declared, member assigned in the body (copy ctor 0x82a1d; the sort COMDATs' predicate copies store the vptr
    // after the member load)
    EEAIFOIntelligenceAIFamilyAndUnitTypeSorter(const EEAIFOIntelligenceAIFamilyAndUnitTypeSorter& inOther)
    {
        this->mFile = inOther.mFile;
    }
    virtual ~EEAIFOIntelligenceAIFamilyAndUnitTypeSorter() {}
    bool operator()(const EECPIUnitTypeCount& inFirst, const EECPIUnitTypeCount& inSecond);

    EECPIntelligencePlayerFile* mFile;      // +0x04
};

// A computer player's intelligence (RTTI EECPIntelligence, no base; vtable 0x43c224: the destructor only; new 0x14,
// ctor 0x81cd0 in EECPCombatManager's ctor; EECPCombatManager +0x50). Names are provisional.
class EECPIntelligence {
public:
    virtual ~EECPIntelligence();            // +0x00 (??_G 0x81cfc, dtor 0x81d18)
    EECPIntelligencePlayerFile* GetFile(EEPlayer* inPlayer);   // 0x81d60 (ret 4): inPlayer's file
    // (EECPCombatManager.cpp's calls; provisional names)
    EECPIntelligence(EEComputerPlayer* inPlayer);   // 0x81cd0
    // 0x81da1 (ret 8): inPlayer's file, created on first use, then refreshed
    EECPIntelligencePlayerFile* UpdateFile(EEComputerPlayer* inOwner, EEPlayer* inPlayer);
    EEPlayer* GetRandomEnemy(EEPlayer* inPlayer);  // 0x81e30 (ret 4): a random enemy of inPlayer (player 0 if none)

    EEComputerPlayer* mPlayer;              // +0x04
    std::map<unsigned long, EECPIntelligencePlayerFile*> mFiles;   // +0x08 by player id (insert 0x482007 setb, _Lbound 0xd9988 jae)
};

// The score order of EECPIntelligence's GetTargets (unit, score) list: units of category 0x13 first, then the larger
// score. Its operator() is out of line in EEAIFunctionObjects.cpp (0x42b0ba, between EEAIFunctionObjects' functors;
// EECPIntelligence's sort COMDATs 0x4840c1.. call it), so the struct is shared through this header (was local to
// EECPIntelligence.cpp). Name is provisional.
struct EECPITargetScoreGreater {
    bool operator()(const std::pair<EEComplexUnit*, long>& inFirst, const std::pair<EEComplexUnit*, long>& inSecond);
};

#endif
