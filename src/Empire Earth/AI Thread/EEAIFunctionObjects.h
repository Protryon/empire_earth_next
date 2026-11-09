// \eex\Empire Earth\AI Thread\EEAIFunctionObjects.h (RECONSTRUCTION, PARTIAL). The AI's
// small function objects whose members live in EEAIFunctionObjects.cpp (0x29fcc..0x2bb72) and that no other header
// declares yet (the original header records __FILE__ for an assert first emitted at 0x2ddfb: found through the
// \eex\Empire Earth\AI Thread\ include path: lives in src/Empire Earth/AI Thread/, included as
// <EEAIFunctionObjects.h> so __FILE__ is the drive-less /I form). Class names from RTTI; slot and
// member names PROVISIONAL.
// Requires ee/EEAIFOUnitsToKillScoringBase.h, ee/EECPUMGatherUnitsScreenBase.h.
#ifndef EE_EEAIFUNCTIONOBJECTS_H
#define EE_EEAIFUNCTIONOBJECTS_H

class EEComplexUnit;
class EEComputerPlayer;
class EEUnitClass;

// RTTI EEAIFOContinentScreenBase (vtable 0x43bce0: ??_G 0x58608, +0x04 0x58554, +0x08 0x58584): the continent filter base;
// its inline slots are COMDATs after their first user EECPAttackGroup::FindLandTransport. Provisional names
class EEAIFOContinentScreenBase {
public:
    virtual ~EEAIFOContinentScreenBase() {}   // +0x00 (??_G 0x58608: the derived screens' dtors fold into it)
    virtual bool Virtual04(long inID) { return inID != 0; }   // +0x04 0x58554
    virtual bool Test(unsigned short inID) { return inID != 0; }        // +0x08 0x58584
};

// RTTI EEAIFOContinentScreenContinentsShareAdjacentID > EEAIFOContinentScreenBase (vtable 0x43bcd0: ??_G 0x58608,
// +0x04 0x58554, +0x08 0x29fcc): whether a continent is one of two or adjacent to both (gContinentManager's adjacency
// flags). Ctor 0x58560 (ret 0xc; inline, refused once in FindLandTransport)
class EEAIFOContinentScreenContinentsShareAdjacentID : public EEAIFOContinentScreenBase {
public:
    EEAIFOContinentScreenContinentsShareAdjacentID(unsigned short inContinent1, unsigned short inContinent2, bool inB08)
        : mContinent1(inContinent1), mContinent2(inContinent2), mB08(inB08) {}
    virtual bool Test(unsigned short inID);        // +0x08 0x29fcc

    unsigned short mContinent1;             // +0x04
    unsigned short mContinent2;             // +0x06
    bool mB08;                              // +0x08
};

// RTTI EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID > ...ShareAdjacentID (vtable 0x43bcc0: ??_G
// 0x58608, +0x04 0x58554, +0x08 0x58591): the same over continents of kind 4 (mOcean) or 2
class EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID : public EEAIFOContinentScreenContinentsShareAdjacentID {
public:
    EEAIFOContinentScreenContinentsShareAdjacentOceanContinentID(unsigned short inContinent1, unsigned short inContinent2,
                                                                 bool inOcean)
        : EEAIFOContinentScreenContinentsShareAdjacentID(inContinent1, inContinent2, false), mOcean(inOcean) {}
    virtual bool Test(unsigned short inID);        // +0x08 0x58591 (inline in the original: a COMDAT after FindLandTransport)

    bool mOcean;                            // +0x0c
};

// RTTI EEAIFOUnitsToKillForMilitary > EEAIFOUnitsToKillScoringBase (vtable 0x43a460: ??_G 0x7b906, the base's three
// scores, +0x10 0x2a271, +0x14 0x2a2d7); ctor 0x2a246 (ret 0x10; called from 0x8740b, 0x9ad76)
class EEAIFOUnitsToKillForMilitary : public EEAIFOUnitsToKillScoringBase {
public:
    EEAIFOUnitsToKillForMilitary(EEComputerPlayer* inPlayer, long inCitizensToKill, long inTransportsToKill, long inMilitaryToKill);  // 0x2a246
    virtual bool CanKill(EEComplexUnit* inUnit);     // +0x10 0x2a271
    virtual long GetKillScore(EEComplexUnit* inUnit);  // +0x14 0x2a2d7

    EEComputerPlayer* mPlayer;              // +0x14 (0x8740b, 0x9ad76 pass their computer player)
};

// RTTI EECPUGatherCitizenUnitsCount > EECPUMGatherUnitsScreenBase (vtable 0x43a498: ??_G 0x7895f, 0x2a31f, 0x2a432,
// 0x2a457): the citizens (categories 6 and 0x16) and their requests; a local of
// EEAIFOUnitsToKillScoringBase::GetCitizenSurplus (members before the vptr: the base's init list)
class EECPUGatherCitizenUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherCitizenUnitsCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, true, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2a31f
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2a432
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2a457

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EECPUGatheAircraftCarriersCount (sic) > EECPUMGatherUnitsScreenBase (vtable 0x43a4c8: ??_G 0x7895f, 0x29f57,
// 0x29f7e, 0x29fa1): the aircraft carriers (category 0x15 with +0x28 set) and their requests; a local of
// EEAIFOCurrentlyBuildableUnitScreener::Screen 0x2ae09 (members before the vptr: the base's init list)
class EECPUGatheAircraftCarriersCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatheAircraftCarriersCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, true, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x29f57
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x29f7e
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x29fa1

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EECPUGatherMilitaryUnitsCount > EECPUMGatherUnitsScreenBase (vtable 0x43a504: ??_G 0x7895f, 0x2b239, 0x2b2b2,
// 0x2b323): the military classes the combat manager accepts (EECPCombatManager::IsMilitaryClass), not type 0x2e5 nor
// categories 0x20/0x15; mB14 weighs them by the class's AddedToParent. Ctor 0x2b207 (ret 0x10).
class EECPUGatherMilitaryUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherMilitaryUnitsCount(EEComputerPlayer* inPlayer, bool inByClass, bool inRequests, bool inCountPopulation);  // 0x2b207
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2b239
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2b2b2
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2b323

    EEComputerPlayer* mPlayer;              // +0x10
    bool mCountPopulation;                  // +0x14
};

// RTTI EECPUGatherTransportUnitsCount > EECPUMGatherUnitsScreenBase (vtable 0x43a518: ??_G 0x7895f, 0x2b3d2, 0x2b3fc,
// 0x2b422): the transport classes (EEUnitClass::IsTransport). Ctor 0x2b3a7 (ret 0xc).
class EECPUGatherTransportUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherTransportUnitsCount(EEComputerPlayer* inPlayer, bool inByClass, bool inRequests);   // 0x2b3a7
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2b3d2
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2b3fc
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2b422

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EECPUGatherHeroesCount > EECPUMGatherUnitsScreenBase (vtable 0x43a678: ??_G 0x7895f, 0x2b503, 0x2b528,
// 0x2b549): the heroes (EEUnitClass +0x1d0 == 0x1b). Its ctor is not in this unit.
class EECPUGatherHeroesCount : public EECPUMGatherUnitsScreenBase {
public:
    // inline (EEComputerPlayer::GetHeroCount 0x2e248 builds it in its frame: the base's stores, the vptr, then
    // mPlayer)
    EECPUGatherHeroesCount(EEComputerPlayer* inPlayer, bool inByClass, bool inRequests)
        : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2b503
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2b528
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2b549

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EEAIFOFavoriteUnitScreener > EEAIFOUnitSelectionScreener (vtable 0x43a660; ctor 0x2ddfb in EEComputerPlayer.cpp,
// ret 0x1c: the init list before the vptr, mEpoch clamped to 0..14 in the body). Needs ee/EECPUnitSelector.h.
// Member names PROVISIONAL.
class EEAIFOFavoriteUnitScreener : public EEAIFOUnitSelectionScreener {
public:
    EEAIFOFavoriteUnitScreener(EEComputerPlayer* inPlayer, bool inAllowAir, bool inAllowLand, bool inAllowSea, bool inAllowSpace, long inEpoch,
        long inEpochSpan);                  // 0x2ddfb
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x2a7d2 (EEAIFunctionObjects.cpp)

    EEComputerPlayer* mPlayer;              // +0x04
    long mEpoch;                            // +0x08
    long mEpochSpan;                        // +0x0c
    bool mAllowAir;                         // +0x10
    bool mAllowSea;                         // +0x11
    bool mAllowLand;                        // +0x12
    bool mAllowSpace;                       // +0x13
};

// 0x2a5de (ret 8): the unit classes (gUnitClassTable indices) by +0x20c, the larger first (EECPUnitSelector's sort
// 0x49c229) PROVISIONAL name
struct EEAIFOUnitClassGreater {
    bool operator()(const long& inA, const long& inB);
};

class EECPAttackSubGroup;
class EECPILocation;

// RTTI EEAIFOBestRegionToAttack (vtable 0x43a4dc: ??_G 0x2b1a4, +0x04 _purecall): a sub group's strike-location
// scorer; sizeof 0x10 (EECPAttackGroup::FindLandTarget's frame); slot/member names provisional
class EEAIFOBestRegionToAttack {
public:
    EEAIFOBestRegionToAttack(EECPAttackSubGroup* inSubGroup, const EETileLocation& inTile);   // 0x2b100
    virtual ~EEAIFOBestRegionToAttack() {}
    virtual float operator()(const EECPILocation& inLocation) = 0;   // +0x04

    EETileLocation mTile;                   // +0x04 (the attack's start tile: EECPILocation +0x04)
    EECPAttackSubGroup* mSubGroup;          // +0x0c
};

// RTTI EEAIFOBestInfluenceRegionToAttack (vtable 0x43a4e8): the land scorer of EECPAttackGroup::FindLandTarget
class EEAIFOBestInfluenceRegionToAttack : public EEAIFOBestRegionToAttack {
public:
    EEAIFOBestInfluenceRegionToAttack(EECPAttackSubGroup* inSubGroup, const EETileLocation& inTile);   // 0x2b129
    virtual float operator()(const EECPILocation& inLocation);   // +0x04 0x2b145 (0.0f)
};

// RTTI EEAIFOBestInfluenceRegionToAirStrike (vtable 0x43a4f8; bases: EEAIFOBestRegionToAttack only): the air scorer of
// EECPAttackGroup::FindAirTarget (ctor 0x2b14e with a gPhysics draw); sizeof 0x20 (FindAirTarget's frame)
class EEAIFOBestInfluenceRegionToAirStrike : public EEAIFOBestRegionToAttack {
public:
    EEAIFOBestInfluenceRegionToAirStrike(EECPAttackSubGroup* inSubGroup, const EETileLocation& inTile, float inChance);
    virtual float operator()(const EECPILocation& inLocation);   // +0x04 (ICF-folded into 0x2b145: 0.0f)

    // +0x10: 8 bytes the ctor leaves unset (sizeof 0x20; not a tile: a tile's ctor zeroes, r24 prep) PROVISIONAL
    long mL10;
    long mL14;
    float mChance;                          // +0x18 (0, or the draw when positive)
    long mCount;                            // +0x1c (1, or (long)(draw * 10) when that is not 0)
};

// The screeners' base slot +0x04 (COMDAT 0x2de43, first emitted by EEComputerPlayer.cpp): not to be called. The class
// is declared in ee/EECPUnitSelector.h; the ESVERIFY line is the original's (0x1d8 = 472).
inline bool EEAIFOUnitSelectionScreener::Screen(EEUnitClass* inClass)
{
    ESVERIFY_AT(false, 472);
    return false;
}

#endif
