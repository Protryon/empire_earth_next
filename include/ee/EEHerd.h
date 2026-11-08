// C:\EEX\Empire Earth\AI Thread\EEHerd.h (RECONSTRUCTION; file name inferred). A herd of wild animals that is hunted or
// driven as one AI area (RTTI EEHerd > EEAIArea @0; vtable 0x43c2e8, 11 slots: EEAIArea's, with its own ??_G 0xa19e3
// and Create override 0xa2775; slot +0x20 is EETown's 0xac203 by ICF).
// EEHerd.cpp 0xa1910..0xa543d: the save key "EEHerd-SaveLoadData", ctor 0xa1953 (default; Clear 0xa25b6 last) and 0xa19ff
// (player, animal index, ...), dtor 0xa1b1e. Members from the ctors, Clear, Load/Save (FSFile slots: +0x5c unsigned
// long, +0x58 long, +0x54 unsigned short, +0x44 float, +0x3c bool); names are provisional.
// Requires ee/EEAIArea.h, ee/EETileCoordinate.h, ee/EEAnimal.h, <vector>, <map>, <algorithm>.
#ifndef EE_EEHERD_H
#define EE_EEHERD_H

#include <map>
#include <vector>
#include <algorithm>
#include "ee/EETileCoordinate.h"

class EEComplexUnit;
class EEAnimal;
class EEMedia;
class EEUnit;
class EEPlayer;
class FSFile;
class EEUnitGoal;
class EEFormation;

class EEHerd : public EEAIArea {
public:
    static const UString sData;             // "EEHerd-SaveLoadData" (guarded chain 0xa191a)

    EEHerd();                               // 0xa1953
    // 0xa19ff (ret 0xc): +0xac, +0xbc (the animal index: EEAnimal::Get), +0xf6 (a word) from the arguments
    EEHerd(EEPlayer* inPlayer, long inAnimal, unsigned short inID);
    virtual ~EEHerd();                      // +0x00 0xa1b1e (??_G 0xa19e3)
    // +0x08 0xa2775 (ret 4): EEAIArea::Create, then (unless gLoadingSavedGame loading and the bounds already set) the bounds
    // grown by a random margin; then PickLeader
    virtual void Create(const std::vector<U2DPoint<long> >& inTiles);
    // +0x20: EETown's body (ICF-folded into EETown::GetRandomPoint 0xac203): a random hull point on the map
    virtual U2DPoint<long> GetRandomPoint();

    bool AddUnit(EEComplexUnit* inUnit);    // 0xa1b45 (ret 4)
    void HandleUnitHit(EEComplexUnit* inUnit);  // 0xa1d0f (ret 4): the unit was attacked (+0x1dc): flee or fight back
    void Attack(EEUnit* inTarget, const std::vector<EEComplexUnit*>& inUnits);   // 0xa1df7 (ret 8): one attack goal per unit
    bool CanStandOn(EEComplexUnit* inUnit, const U2DPoint<long>& inTile);  // 0xa1e75 (ret 8): may inUnit stand on inTile
    bool Chase();                           // 0xa1efc: pick a moving target on the continent and circle it
    bool MoveTo(const U2DPoint<long>& inTile, unsigned long inRadius);   // 0xa2275 (ret 8): circle the herd round inTile
    bool Hunt();                            // 0xa24d0
    void Clear();                           // 0xa25b6
    void PickLeader();                      // 0xa26b9: the leader is the unit with the most hit points
    void Graze(EEComplexUnit* inUnit, unsigned long inTime);    // 0xa286a (ret 8): graze: move inUnit and its young
    EEComplexUnit* FindMother();            // 0xa2d03: the parent for a new young one
    void GetYoungUnits(std::vector<EEComplexUnit*>& outUnits);  // 0xa302c (ret 4): the units whose +0x240 is below 1.0
    void GetPrey(unsigned short inContinent, std::vector<EEComplexUnit*>& outUnits);  // 0xa3075 (ret 8)
    U2DPoint<long> GetRandomTile();         // 0xa30b6 (ret 4, hidden result): a random point of mBounds2
    bool GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction);  // 0xa3110 (ret 0x10)
    void AttackBack(EEComplexUnit* inUnit, bool inAll);  // 0xa315b (ret 8)
    void GrowUnit(EEComplexUnit* inUnit);   // 0xa31fd (ret 4): a young one grows
    bool FollowNearestHerd();               // 0xa3295: attack the nearest other herd
    void Update(unsigned long inTime);      // 0xa33bd (ret 4): the per-state update
    static bool IsAnimalOnMap(EEUnit* inUnit);  // 0xa349c (cdecl)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0xa34dc (ret 0xc)
    void ClearUnits();                      // 0xa3749
    void RemoveUnitMedia(unsigned long inKey);      // 0xa37a7 (ret 4)
    void ReleaseLeader();                   // 0xa37e4
    void ReleaseTarget();                   // 0xa37fb
    bool RemoveUnit(EEComplexUnit* inUnit);   // 0xa3812 (ret 4)
    long RepairPointers(long, long);        // 0xa3967 (ret 8)
    bool GiveBirth(unsigned long inTime);   // 0xa3d9c (ret 4): a young one is born
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0xa3e7a (ret 0xc)
    void ResetGrazeTimes(long inIndex, bool inShuffle);  // 0xa412e (ret 8): the units' +0x314 timers cleared
    void SetUnitMedia(EEMedia* inMedia, unsigned long inKey);  // 0xa41fb (ret 8): attaches inMedia to unit id inKey
    bool CanTake(EEComplexUnit* inUnit, bool inCheckRange);  // 0xa4263 (ret 8): may inUnit join
    void Surround(EEComplexUnit* inTarget);  // 0xa4351 (ret 4): circle inTarget
    // the state handlers of Update (ret 4; the delay to the next update)
    unsigned long UpdateChasing(unsigned long inTime);  // 0xa45b7 (state 7)
    unsigned long UpdateHunting(unsigned long inTime);  // 0xa467b (state 9)
    unsigned long UpdateIdle(unsigned long inTime);  // 0xa4757 (state 4)
    unsigned long UpdateAttacking(unsigned long inTime);  // 0xa475f (state 3)
    unsigned long UpdateResting(unsigned long inTime);  // 0xa4854 (state 0)
    unsigned long UpdateLeaving(unsigned long inTime);  // 0xa4aa2 (state 0xa; sets 0xb once all are off the map)
    unsigned long UpdateSurrounding(unsigned long inTime);  // 0xa4e76 (state 2)

    // inline (COMDAT 0xa53e8 after the unit's templates; CanStandOn calls it) (provisional name)
    bool HasUnit(EEComplexUnit* inUnit) { return std::find(this->mUnits.begin(), this->mUnits.end(), inUnit) != this->mUnits.end(); }

    std::map<unsigned long, EEMedia*> mMedia;      // +0x68 unit id -> the media attached to it (SetUnitMedia/RemoveUnitMedia; UnloadMedia on release; the map code is ICF-shared with map<long, long*>)
    EEComplexUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEComplexUnit*> mUnits;     // +0x74
    U2DRectangle<long> mBounds2;            // +0x84
    U2DPoint<long> mP94;                    // +0x94
    U2DPoint<long> mCenter;                 // +0x9c (the target's tile)
    EEAnimal* GetAnimal() const { return this->mAnimal; }
    EEAnimal* mAnimal;                      // +0xa4 (saved as its +0x3c index; RepairPointers: EEAnimal::sTable)
    UReferenceCounter* mPathRequest;        // +0xa8 (released through gPathManager, then Dereference)
    EEPlayer* GetPlayer() const { return this->mPlayer; }
    EEPlayer* mPlayer;                      // +0xac (saved as its mPlayerID; RepairPointers: gWorld's player)
    EEComplexUnit* GetLeader() const { return this->mLeader; }
    EEComplexUnit* mLeader;                 // +0xb0 (reference counted)
    EEComplexUnit* GetTarget() const { return this->mTarget; }
    EEComplexUnit* mTarget;                 // +0xb4 (reference counted)
    float mFB8;                             // +0xb8
    unsigned long mAnimalID;                // +0xbc (the animal index: the units' class +0x2cc)
    unsigned long GetGrazeAreaID() const { return this->mGrazeAreaID; }
    unsigned long mGrazeAreaID;             // +0xc0 (an area serial: gContinentManager.GetArea)
    unsigned long mState;                   // +0xc4 (0, 2, 3, 4, 7, 9, 0xa, 0xb)
    unsigned long mLC8;                     // +0xc8
    unsigned long mBirthTimer;              // +0xcc (the next birth)
    unsigned long mFollowTime;              // +0xd0
    unsigned long mNextUpdateTime;          // +0xd4 (the next update)
    unsigned long mLD8;                     // +0xd8
    unsigned long mChaseTime;               // +0xdc
    unsigned long mHuntTime;                // +0xe0
    unsigned long mLE4;                     // +0xe4
    long mRadiusSquared;                    // +0xe8 (the animal's +0x68 squared: the joining range)
    long mNumYoung;                         // +0xec (the young ones)
    long mMaxUnits;                         // +0xf0 (the most units)
    unsigned short mContinent;              // +0xf4
    unsigned short mID;                     // +0xf6
    bool mBreeds;                           // +0xf8 (the animal's +0x85: breeds)
    bool mIsActive;                         // +0xf9
};

#endif
