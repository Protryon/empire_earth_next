// C:\EEX\Empire Earth\AI Thread\EECPDefenseCommander.cpp (file name inferred: no __FILE__; RTTI class
// EECPDefenseCommander, vtable 0x43c0b4). A computer player's defense commander: 0x77718..0x7ed03 (EECPCombatStrategy's
// vector COMDATs end at 0x77718; the gatherer function objects defined here open the unit, their out-of-line slots
// first; the map/set/vector/sort COMDATs from 0x7c0f6 close it; EECPDiplomacyManager.cpp starts at 0x7ed03).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIRandom.h"
#include "ee/EECivPowersManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPMilitaryAidPackage.h"
#include "ee/EECPDefenseCommander.h"
#include "ee/EEMathInline.h"
#include "ee/EECPIArmy.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EEAIFOSubGroupFilter.h"
#include "ee/EECPBehavior.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)
// The gatherers this unit builds on the stack (out-of-line slots open the unit). Class names from RTTI.
// RTTI EECPUGatherBuildingUnitsCount (vtable 0x43c0d0): the buildings
class EECPUGatherBuildingUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherBuildingUnitsCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, false, true)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x77718
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x7774e
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x77799

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EECPUGatherCitizenUnits (vtable 0x43c0e4): the citizens themselves (into mCitizens)
class EECPUGatherCitizenUnits : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherCitizenUnits(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, false, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual ~EECPUGatherCitizenUnits() {}   // ??_G 0x792c1 (stores its own vptr first: user-declared)
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest) { return false; }   // +0x04 0x792bc
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x777dc
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x77812

    EEComputerPlayer* mPlayer;              // +0x10
    std::deque<EEComplexUnit*> mCitizens;   // +0x14 (??_G 0x792c1)
};

// RTTI EECPUGatherNavalUnitsCount (vtable 0x43c130): the ships (class flag 4) that are not transports or citizens
class EECPUGatherNavalUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherNavalUnitsCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, true, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x77864
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x778ab
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x778e4

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EEAIFOGatherAllDeathMatchDefenseBuildUpUnits (vtable 0x43c0bc): the screener of BuildUp
class EEAIFOGatherAllDeathMatchDefenseBuildUpUnits : public EEAIFOCurrentlyBuildableUnitScreener {
public:
    EEAIFOGatherAllDeathMatchDefenseBuildUpUnits(EEComputerPlayer* inPlayer)
        : EEAIFOCurrentlyBuildableUnitScreener(inPlayer)
    {
    }
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x77937
};

// RTTI EECPUGatherSpaceUnitsCount (vtable 0x43c174): the space units (class flag 8)
class EECPUGatherSpaceUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherSpaceUnitsCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, true, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x77ae5
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x77b31
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x77b76

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EECPUGatherCarrierFightersCount (vtable 0x43c100): the carrier fighters (unit types 0x55, 0x50, 0x2a8, 0x30c,
// 0x30d)
class EECPUGatherCarrierFightersCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherCarrierFightersCount(EEComputerPlayer* inPlayer) : EECPUMGatherUnitsScreenBase(true, true, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x7b7fc
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x7b828
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x7b84e
    bool IsFighter(EEUnitClass* inClass);   // 0x7b880 (ret 4)

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EEAIFOUnitsToKillForNavalMaintenance (vtable 0x43c114): the ships to delete
class EEAIFOUnitsToKillForNavalMaintenance : public EEAIFOUnitsToKillScoringBase {
public:
    EEAIFOUnitsToKillForNavalMaintenance(EEComputerPlayer* inPlayer, long inCount)
    {
        this->mPlayer = inPlayer;
        this->mCitizensToKill = EEAIFOUnitsToKillScoringBase::GetCitizensToKill(inPlayer);
        this->mCount = inCount;
    }
    virtual bool CanKill(EEComplexUnit* inUnit);     // +0x10 0x7b8b7
    // +0x14 0x7bc40 (shared with the space twin by ICF)
    virtual long GetKillScore(EEComplexUnit* inUnit)
    {
        if (inUnit->mIsCitizen)
            return this->ScoreCitizen(inUnit);
        else
            return this->ScoreMilitary(inUnit);
    }

    EEComputerPlayer* mPlayer;              // +0x14
};

// RTTI EEAIFOUnitsToKillForSpaceMaintenance (vtable 0x43c158): the space units to delete
class EEAIFOUnitsToKillForSpaceMaintenance : public EEAIFOUnitsToKillScoringBase {
public:
    EEAIFOUnitsToKillForSpaceMaintenance(EEComputerPlayer* inPlayer, long inCount)
    {
        this->mPlayer = inPlayer;
        this->mCitizensToKill = EEAIFOUnitsToKillScoringBase::GetCitizensToKill(inPlayer);
        this->mCount = inCount;
    }
    virtual bool CanKill(EEComplexUnit* inUnit);     // +0x10 0x7bbf1
    virtual long GetKillScore(EEComplexUnit* inUnit)  // +0x14 (ICF-folded into 0x7bc40)
    {
        if (inUnit->mIsCitizen)
            return this->ScoreCitizen(inUnit);
        else
            return this->ScoreMilitary(inUnit);
    }

    EEComputerPlayer* mPlayer;              // +0x14
};

// RTTI EEAIFOAMBToKill (vtable 0x43c188): the anti-missile batteries (unit type 0x2e5) to delete
class EEAIFOAMBToKill : public EEAIFOUnitsToKillScoringBase {
public:
    EEAIFOAMBToKill(long inCount)
    {
        this->mCount = inCount;
    }
    virtual bool CanKill(EEComplexUnit* inUnit)      // +0x10 0x7c023
    {
        return inUnit->mHitPoints > 0 && inUnit->mUnitClass->mUnitType == kUnitType14AntiMissileBattery && inUnit->mContainer == NULL;
    }
    virtual long GetKillScore(EEComplexUnit* inUnit);  // +0x14 0x7c04f
};

// GetWorstThreats's sort orders (inline in the sort COMDATs) and ComputeTownThreat's (by the summed army values).
// (provisional names)
struct EECPDCThreatGreater {
    bool operator()(const EECPDCTownThreat& inFirst, const EECPDCTownThreat& inSecond) const
        { return inFirst.mEnemy > inSecond.mEnemy; }
};
struct EECPDCThreatNetGreater {
    bool operator()(const EECPDCTownThreat& inFirst, const EECPDCTownThreat& inSecond) const
        { return inFirst.mEnemy - inFirst.mOwn > inSecond.mEnemy - inSecond.mOwn; }
};
// PlanAid's town order (the sort COMDAT 0x7d174: unsigned mL78, ja). (provisional name)
struct EECPDCTownValueGreater {
    bool operator()(EETown* inFirst, EETown* inSecond) const
        { return inFirst->mValue > inSecond->mValue; }
};
struct EECPDCEnemyGreater {
    bool operator()(const std::pair<EEPlayer*, unsigned long>& inFirst,
                    const std::pair<EEPlayer*, unsigned long>& inSecond) const
        { return inFirst.second > inSecond.second; }
};

// unit-local .rdata floats (provisional names)
static const float kDCCalamityThreat = 0.5f;   // 0x43c0ac (CheckSpecials)
static const float kDCOwnArmyFactor = 1.0f / 225.0f;   // 0x43c0f4 (ComputeTownThreat)
static const float kDCOwnArmyRange = 225.0f;   // 0x43c0f8 (ComputeTownThreat)

// unit-local globals (provisional names)
extern long gDCGameSpeed;                   // 0x530d24 (> 0x32: the naval maintenance)
#include "ee/EEUnitUtilities.h" // Fn4EEF9, the shared pointer shuffle (eight callers in the original)
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayerID, std::vector<EEUnit*>& outUnits);   // 0x64c395

// FUNCTION: EXE 0x477718
// Adds a pending request's count if it is for a land building other than a wall
bool EECPUGatherBuildingUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && inClass->IsLandBuilding() && inClass->mIsWall == false) {
        this->mRequested += inRequest->mCount;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x47774e
// Counts a living land building that is not a wall or a farm
bool EECPUGatherBuildingUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && inUnit->mUnitClass->IsLandBuilding() && inUnit->mHitPoints > 0) {
        EEUnitClass* theClass = inUnit->mUnitClass;
        if (theClass->mIsWall == false && theClass->mUnitType != kUnitTypeFarm && theClass->mUnitType != kUnitTypeRoboticFarm) {
            this->mUnits++;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477799
// Counts a class's units if it is a land building other than a wall (farms included)
bool EECPUGatherBuildingUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL && inClass->IsLandBuilding() && inClass->mIsWall == false) {
        this->mUnits += inUnits->size();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4777dc
// Counts a land or naval citizen and keeps it in the citizen list
bool EECPUGatherCitizenUnits::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL) {
        long theCategory = inUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
            this->mUnits++;
            theResult = true;
            this->mCitizens.push_back(inUnit);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477812
// Counts a citizen class's units and appends them to the citizen list
bool EECPUGatherCitizenUnits::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL) {
        long theCategory = inClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
            this->mUnits += inUnits->size();
            this->mCitizens.insert(this->mCitizens.end(), inUnits->begin(), inUnits->end());
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477864
// Adds a pending request's count if it is for a non-transport, non-citizen ship
bool EECPUGatherNavalUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && (inClass->mDomains & kMovementSea) && !inClass->IsTransport()) {
        long theCategory = inClass->mCategory;
        if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen) {
            this->mRequested += inRequest->mCount;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4778ab
// Counts a ship that is neither a transport nor a citizen
bool EECPUGatherNavalUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && (inUnit->mUnitClass->mDomains & kMovementSea) && !inUnit->mUnitClass->IsTransport() &&
        inUnit->mIsCitizen == false) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4778e4
// Counts a class's units if it is a non-transport, non-citizen ship class
bool EECPUGatherNavalUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL && (inClass->mDomains & kMovementSea) && !inClass->IsTransport()) {
        long theCategory = inClass->mCategory;
        if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen) {
            this->mUnits += inUnits->size();
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477937
// Accepts a buildable current-epoch mobile defence land combat class (no specials, siege, artillery, AA, medics,
// priests) whose upgrade line is current or already fielded, if the difficulty's DM minimum is set
bool EEAIFOGatherAllDeathMatchDefenseBuildUpUnits::Screen(EEUnitClass* inClass)
{
    bool theResult = false;
    EETechTree* theTree = this->mPlayer->GetTechTree();
    if (inClass->mEpoch > theTree->mCurrentEpoch)
        return theResult;
    if (inClass->mMaxVelocity == 0.0f)
        return theResult;
    if (inClass->mIsDefenseUnit == false)
        return theResult;
    EETechTreeNode* theNode = theTree->Get(inClass->GetTechnology());
    if (theNode == NULL || theNode->GetIsObsolete() || theNode->mScenEditorObsoleted != false)
        return theResult;
    EEUnitClass* theClass = gUnitClassTable[theNode->GetUnitClass(0)];
    if (theClass == NULL)
        return theResult;
    EETechTreeNode* theOther = this->mPlayer->GetTechTree()->Get(theClass->GetTechnology());
    if (theOther == NULL)
        return theResult;
    if (theOther->GetIsObsolete() || theOther->mScenEditorObsoleted != false) {   // the upgrade node's flag (was the first node's)
        EECPUnitManager* theManager = this->mPlayer->mCivilizationManager->mUnitManager;
        std::deque<EEComplexUnit*>* theUnits = theManager->GetUnits(theNode->GetUnitClass(0));
        if (theUnits == NULL || !(theUnits->size() > 0))
            return false;
    }
    if (inClass->mCategory != kUnitCategoryLand || inClass->mFamily == kUnitFamilyMines || inClass->mHasCalamityPowers != false || inClass->IsPriest() ||
        inClass->mIsProjectile != false || inClass->mCategory == kUnitCategoryLandCitizen || inClass->mCategory == kUnitCategoryNavalCitizen || inClass->mIsMedic != false ||
        inClass->FindAction(kActionInitialAttackState) == NULL)
        return false;
    switch (inClass->mAIKind) {
    case kAIKindSpecial:
    case kAIKindFieldWeapon:
    case kAIKindAntiAir:
    case kAIKindArtillery:
    case kAIKindSiege:
    case kAIKindAntiSubmarineHelicopter:
    case kAIKindMechZeus:
        return false;
    }
    switch (this->mPlayer->mDifficulty) {
    case kDifficultyEasy:
        theResult = this->mPlayer->mMinMilitaryBuildingsEasyDM != 0;
        break;
    case kDifficultyMedium:
        theResult = this->mPlayer->mMinMilitaryBuildingsMediumDM != 0;
        break;
    case kDifficultyHard:
        theResult = this->mPlayer->mMinMilitaryBuildingsHardDM != 0;
        break;
    default:
        return false;
    }
    return theResult;
}

// FUNCTION: EXE 0x477ae5
// Adds a pending request's count if it is for a space combat unit (no transports, citizens, space buildings)
bool EECPUGatherSpaceUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && (inClass->mDomains & kMovementSpace) && !inClass->IsTransport()) {
        long theCategory = inClass->mCategory;
        if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen && theCategory != kUnitCategorySpaceBuilding) {
            this->mRequested += inRequest->mCount;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477b31
// Counts a space combat unit (no transports, citizens or space buildings)
bool EECPUGatherSpaceUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && (inUnit->mUnitClass->mDomains & kMovementSpace) && !inUnit->mUnitClass->IsTransport() &&
        inUnit->mIsCitizen == false && inUnit->mUnitClass->mCategory != kUnitCategorySpaceBuilding) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x477b76
// Counts a class's units if it is a space combat class
bool EECPUGatherSpaceUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL && (inClass->mDomains & kMovementSpace) && !inClass->IsTransport()) {
        long theCategory = inClass->mCategory;
        if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen && theCategory != kUnitCategorySpaceBuilding) {
            this->mUnits += inUnits->size();
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x477bce
// Creates the defence commander, disabled; first update after (player id + 5) s, first aid after a random
// replenish delay on top
EECPDefenseCommander::EECPDefenseCommander(EEComputerPlayer* inPlayer) : mNextSpaceTime(0), mNextSpecialTime(0)
{
    long theDelay = gPhysics->GetRandom(inPlayer->GetReplenishDelayMin(), inPlayer->GetReplenishDelayMax()) * 60000;
    this->mPlayer = inPlayer;
    this->mNextUpdateTime = (inPlayer->mPlayerID + 5) * 1000;
    this->mNextAidTime = this->mNextUpdateTime + theDelay;
    this->mEnabled = false;
    this->mAttackCommander = NULL;
    this->mNextNavalTime = 0;
    this->mNextRaidCheckTime = 0;
}

// SYNTHETIC: EXE 0x477c64
// (the scalar deleting destructor)

// FUNCTION: EXE 0x477c80
// Destroys the defence commander, freeing its aid packages
EECPDefenseCommander::~EECPDefenseCommander()
{
    this->FreePackages();
}

// FUNCTION: EXE 0x477ca8
// If units are allowed, creates an inactive defensive attack group against a random enemy and marks its
// sub-groups as defenders
void EECPDefenseCommander::CreateDefendGroup()
{
    EECPAGTask theTask;
    EEPlayer* thePlayer = this->mAttackCommander->mIntelligence->GetRandomEnemy(this->mPlayer);
    if (thePlayer != NULL) {
        theTask.mPlayer = gWorld.mPlayers[thePlayer->mPlayerID];
        if (this->GetUnitAllowance() > 0) {
            theTask.mIsActive = false;
            theTask.mType = kAGTaskLocation;
            theTask.mMode = kAGModeDefense;
            EECPAttackGroup* theGroup = this->mAttackCommander->CreateGroup(theTask, 1);
            if (theGroup != NULL)
                for (long i = theGroup->GetNumSubGroups() - 1; i >= 0; i--)
                    theGroup->mSubGroups[i]->mMission = 1;
        }
    }
}

// FUNCTION: EXE 0x477d32
// Sends aid to the player's town: reuses its package or builds one (threat, attacker or a random enemy) and
// assigns sub-groups; keeps the new package only if that succeeds; returns whether aid was sent
bool EECPDefenseCommander::SendAid(EEPlayer* inPlayer, long inTownID, bool inCounterAttackOnly)
{
    bool theResult = false;
    long thePlayerID = 0;
    EETown* theTown = inPlayer->GetTownByID(inTownID);
    if (theTown != NULL) {
        std::map<long, EECPMilitaryAidPackage*>::iterator it = this->mPackages.find(inTownID);
        if (it == this->mPackages.end()) {
            theResult = false;
            EECPMilitaryAidPackage* thePackage = new EECPMilitaryAidPackage(this->mPlayer);
            thePackage->mTownID = inTownID;
            thePackage->mLocation.mX = theTown->mHull.GetCenter().mX;
            thePackage->mLocation.mY = theTown->mHull.GetCenter().mY;
            float theThreat = this->GetTownThreat(theTown, thePlayerID);
            thePackage->mThreat = theThreat;
            thePackage->mTarget = inPlayer;
            if (thePlayerID == 0) {
                // GetRandomEnemy takes any player here (an ally), though the header says EEComputerPlayer*
                EEPlayer* theOther = this->mAttackCommander->mIntelligence->GetRandomEnemy(inPlayer);
                if (theOther != NULL)
                    thePlayerID = theOther->mPlayerID;
            }
            if (thePlayerID != 0) {
                thePackage->mPlayerID = thePlayerID;
                theResult = this->AssignSubGroups(thePackage, inCounterAttackOnly);
            }
            if (inPlayer->mHumanPlayer != NULL)
                thePackage->mNextSendTime = 0;
            if (theResult)
                this->mPackages.insert(std::map<long, EECPMilitaryAidPackage*>::value_type(thePackage->mTownID, thePackage));
            else
                delete thePackage;
        } else
            theResult = this->AssignSubGroups(it->second, inCounterAttackOnly);
    }
    return theResult;
}

// FUNCTION: EXE 0x477e4b
// Returns the town's threat level (0..1) and the strongest enemy's id
float EECPDefenseCommander::GetTownThreat(EETown* inTown, long& outPlayerID)
{
    std::map<U2DPoint<long>, EECPDCTownThreat> theMap;
    float theThreat = this->ComputeTownThreat(inTown, outPlayerID, &theMap, false);
    return theThreat;
}

// FUNCTION: EXE 0x477e8e
// Updates the intelligence file on every player, with armies, forcing it when the file has no armies yet
void EECPDefenseCommander::RefreshIntelligence()
{
    EECPIntelligence* theIntelligence = this->mPlayer->mCombatManager->mIntelligence;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        if (gWorld.mPlayers[i] == NULL)
            continue;
        EECPIntelligencePlayerFile* theFile = theIntelligence->GetFile(gWorld.mPlayers[i]);
        if (theFile != NULL)
            theFile->Update(this->mPlayer, theFile->GetNumArmies() == 0, 1);
    }
}

// FUNCTION: EXE 0x477efd
// Picks the land sub-group best matching the town's last threat point (or the nearest when none), sets it
// to the aid mission and adds it to the package; the town task it builds is never used
bool EECPDefenseCommander::SendBestSubGroup(std::vector<EECPAttackSubGroup*>& inGroups, EECPMilitaryAidPackage* inPackage)
{
    EETown* theTown;
    long i;
    EECPAttackSubGroup* theUnit;
    long theDistance;
    long theID;
    std::map<U2DPoint<long>, EECPDCTownThreat> theThreats;
    std::map<U2DPoint<long>, EECPDCTownThreat>::iterator it;
    std::vector<EECPDCTownThreat> theList;
    EETileLocation theTile;
    EECPILocation theLocation;
    EECPAGTask theTask;
    EECPAttackSubGroup* theBest = NULL;
    long theBestDelta = 0;
    long theBestDistance = 0;
    long theNet = 0;
    bool theResult = false;
    if (inGroups.size() <= 0 || inPackage->mTarget == NULL)
        return theResult;
    theTown = inPackage->mTarget->GetTownByID(inPackage->mTownID);
    if (theTown != NULL) {
        this->GetWorstThreats(inPackage->mTarget, inPackage->mTownID, theThreats, theList);
        for (it = theThreats.begin(); it != theThreats.end(); ++it)
            theNet = it->second.mEnemy - it->second.mOwn;
        for (i = inGroups.size() - 1; i >= 0; i--) {
            theUnit = inGroups[i];
            if (theUnit->mKind != kASGKindLand)
                continue;
            if (theNet > 0) {
                if (theBest == NULL) {
                    theBest = theUnit;
                    theBestDelta = UMAbs(theNet - theUnit->mStats.mTotalInfluence);
                } else if (UMAbs(theNet - theUnit->mStats.mTotalInfluence) < theBestDelta) {
                    theBest = theUnit;
                    theBestDelta = UMAbs(theNet - theUnit->mStats.mTotalInfluence);
                }
            } else {
                theUnit->FindPosition(theTile, true, false);
                if (theBest == NULL) {
                    theBest = theUnit;
                    theBestDistance = theTile.DistanceToSquared(theTown->mHull.GetCenter());
                } else {
                    theDistance = theTile.DistanceToSquared(theTown->mHull.GetCenter());
                    if (theDistance < theBestDistance) {
                        theBest = theUnit;
                        theBestDistance = theDistance;
                    }
                }
            }
        }
        if (theBest != NULL) {
            theLocation.mSerial = theTown->mSerial;
            theLocation.mType = kAIAreaTown;
            theLocation.mPlayer = theTown->mPlayer->GetPlayerID();
            theResult = true;
            theLocation.ResetToArea();
            if (theLocation.IsOnMap()) {
                theTask.mLocation = theLocation;
                theTask.mType = kAGTaskLocation;
                theTask.mMode = kAGModeCombat;
                theTask.mIsActive = true;
            }
            theBest->mMission = 3;
            theID = theBest->mID;
            inPackage->mSubGroupIDs.push_back(theID);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x478148
// Creates an inactive attack group of up to 20 units (the allowance) of one random deathmatch defence class
// against a random enemy
void EECPDefenseCommander::BuildUp()
{
    EEAIFOGatherAllDeathMatchDefenseBuildUpUnits theScreener(this->mPlayer);
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;
    std::vector<long> theAll;
    std::vector<long> theUnitIDs;
    EECPAGTask theTask;
    if (this->mPlayer->GetPopulationLimit(0) == 0)
        this->mPlayer->WorkOutPopulation();
    this->mPlayer->GetPopulationLimit(0);
    this->mAttackCommander->mPlayer->GetScreenedTypesByLine(theClasses, theScreener);
    if (theClasses.size() > 0) {
        for (it = theClasses.begin(); it != theClasses.end(); ++it)
            theAll.insert(theAll.end(), (*it).second->begin(), (*it).second->end());
        if (theAll.size() > 0) {
            long theClass = theAll[gPhysics->Fn82D66(theAll)];
            long theCount = this->GetUnitAllowance();
            if (theCount >= 20)
                theCount = 20;
            if (theCount > 0) {
                theStrategy.Reset();
                theStrategy.mAddFavorites = false;
                theStrategy.mNumUnits = theCount;
                theStrategy.mNumRemaining = theCount;
                theStrategy.mKind = 5;
                theRequirement.mUnitClass = theClass;
                theRequirement.mKind = kRequirementAlreadyDefined;
                theRequirement.mNumLeft = theCount;
                theRequirement.mNumWanted = theCount;
                theStrategy.mRequirements.push_back(theRequirement);
                theTask.mPlayer = this->mPlayer->mCombatManager->GetIntelligence()->GetRandomEnemy(this->mPlayer);
                theTask.mIsActive = false;
                this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
            }
        }
    }
    for (it = theClasses.begin(); it != theClasses.end(); ++it)
        delete it->second;
}

// FUNCTION: EXE 0x4783e7
// With units allowed: by the town-guard chance sends aid to the most threatened own town (else to one of the
// 3 richest), otherwise creates a defend group; then schedules the next aid
void EECPDefenseCommander::PlanAid(unsigned long inTime)
{
    EECPMilitaryAidPackage thePackage(this->mPlayer);
    std::vector<EETown*> theTowns;
    float theChance;
    if (this->GetUnitAllowance() > 0) {
        theChance = gPhysics->GetRandom();
        if (theChance < this->mPlayer->GetTownGuardPercentage()) {
            long theTownID = this->FindThreatenedTown(this->mPlayer, &thePackage, 0);
            if (theTownID != 0) {
                if (thePackage.mThreat > 0.0f) {
                    this->SendAid(this->mPlayer, theTownID, true);
                } else {
                    theTowns = this->mPlayer->mTowns;
                    if (theTowns.size() > 0) {
                        std::sort(theTowns.begin(), theTowns.end(), EECPDCTownValueGreater());
                        if (theTowns.size() > 3)
                            theTowns.erase(theTowns.begin() + 3, theTowns.end());
                        if (gPhysics->mRandom.Generate() <= 0.25f)
                            EEAIRandomShuffle(theTowns.begin(), theTowns.end());
                        this->SendAid(this->mPlayer, theTowns.front()->mTownID, true);
                    }
                }
            }
        } else
            this->CreateDefendGroup();
    }
    this->SetNextAidTime(inTime);
}

// FUNCTION: EXE 0x4785cb
// Goes through the allies in shuffled order and sends one new aid package to the first threatened (or
// unguarded) allied town without one, when the threat or relation and the allowance permit
void EECPDefenseCommander::AidAllies()
{
    std::vector<EEPlayer*> theAllies(this->mPlayer->mAllies);
    EECPMilitaryAidPackage thePackage(this->mPlayer);
    Fn4EEF9(theAllies, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
    for (long i = theAllies.size() - 1; i >= 0; i--) {
        if (theAllies[i] == this->mPlayer)
            continue;
        EEPlayer* theAlly = theAllies[i];
        long theTownID = this->FindThreatenedTown(theAlly, &thePackage, 0);
        if (theTownID == 0) {
            for (long j = theAlly->mTowns.size() - 1; j >= 0; j--) {
                if (theAlly->mTowns[j] == NULL || !((theAlly->mTowns[j])->mTiles.Size() > 2))
                    continue;
                long theID = (theAlly->mTowns[j])->mTownID;
                std::map<long, EECPMilitaryAidPackage*>::iterator theFound = this->mPackages.find(theID);
                if (theFound == this->mPackages.end()) {
                    theTownID = (theAlly->mTowns[j])->mTownID;
                    break;
                }
            }
            if (theTownID == 0)
                continue;
        }
        std::map<long, EECPMilitaryAidPackage*>::iterator theIt = this->mPackages.find(theTownID);
        if (theIt != this->mPackages.end())
            continue;
        if ((!(this->mPlayer->mAidThreatThreshold < thePackage.mThreat || theAlly != this->mPlayer && this->mPlayer->mEnemies[theAlly->mPlayerID] == 0)) ||
            !(this->GetUnitAllowance() > 0))
            continue;
        EECPMilitaryAidPackage* theNew = new EECPMilitaryAidPackage(this->mPlayer);
        *theNew = thePackage;
        theNew->mTarget = theAlly;
        if (this->AssignSubGroups(theNew, false)) {
            if (theAlly->mHumanPlayer != NULL)
                theNew->mNextSendTime = 0;
            this->mPackages.insert(std::map<long, EECPMilitaryAidPackage*>::value_type(theNew->mTownID, theNew));
        } else
            delete theNew;
        break;
    }
}

// FUNCTION: EXE 0x478866
// Every 80 s after the attack starts: if the player has fewer buildings than its bonzai attack level, rushes a
// random enemy (Retaliate) and waits 300 s
void EECPDefenseCommander::CheckRaids()
{
    EECPUGatherBuildingUnitsCount theGatherer(this->mPlayer);
    EE_LOST_DEBUG(10);   // LOST-DEBUG: none: exact only (the original refuses the base ctor 0x478937)
    if (this->mNextRaidCheckTime == 0)
        this->mNextRaidCheckTime = this->mPlayer->mInitialAttackDelayMax * 60000;
    if (!this->mPlayer->mAttackStarted || !(this->mNextRaidCheckTime <= gPhysics->mGameTime))
        return;
    this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
    this->mNextRaidCheckTime = gPhysics->mGameTime + 80000;
    if (!(theGatherer.GetTotal() < this->mPlayer->mBonzaiAttackLevel))
        return;
    EEPlayer* theEnemy = this->mPlayer->mCombatManager->GetIntelligence()->GetRandomEnemy(this->mPlayer);
    if (theEnemy != NULL && theEnemy->mPlayerID != 0) {
        this->Retaliate(theEnemy);
        this->mNextRaidCheckTime = gPhysics->mGameTime + 300000;
    }
}

// FUNCTION: EXE 0x47897c
// Sends aid to each of the player's towns that has no aid package
void EECPDefenseCommander::AidOwnTowns()
{
    for (long i = this->mPlayer->mTowns.size() - 1; i >= 0; i--) {
        long theID = this->mPlayer->GetTownList()[i]->mTownID;
        std::map<long, EECPMilitaryAidPackage*>::iterator theIt = this->mPackages.find(theID);
        if (theIt == this->mPackages.end())
            this->SendAid(this->mPlayer, this->mPlayer->GetTownList()[i]->mTownID, false);
    }
}

// FUNCTION: EXE 0x4789fb
// Fills the town's threat map and lists its 3 strongest enemy threat points, ordered by net threat
void EECPDefenseCommander::GetWorstThreats(EEPlayer* inPlayer, long inTownID, std::map<U2DPoint<long>, EECPDCTownThreat>& ioMap,
                                   std::vector<EECPDCTownThreat>& outList)
{
    std::map<U2DPoint<long>, EECPDCTownThreat>::iterator it;
    EETown* theTown = inPlayer->GetTownByID(inTownID);
    long thePlayerID = 0;
    if (theTown == NULL)
        return;
    this->ComputeTownThreat(theTown, thePlayerID, &ioMap, true);
    if (ioMap.size() <= 0)
        return;
    outList.reserve(ioMap.size());
    for (it = ioMap.begin(); it != ioMap.end(); ++it)
        outList.push_back(it->second);
    std::sort(outList.begin(), outList.end(), EECPDCThreatGreater());
    if ((long)outList.size() > 3)
        outList.erase(outList.begin() + 3, outList.end());
    std::sort(outList.begin(), outList.end(), EECPDCThreatNetGreater());
}

// FUNCTION: EXE 0x478b34
// Creates a counter-attack group (special and counter units) for the package's town against its attacker
// and adds its sub-groups to the package; returns whether a group was made
bool EECPDefenseCommander::CounterAttack(EECPMilitaryAidPackage* inPackage)
{
    std::vector<long> theUnitIDs;
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    EECPAGTask theTask;
    EECPILocation theLocation;
    EETown* theTarget;
    bool theResult = false;
    EETileLocation theArea;
    if (inPackage->mPlayerID != 0) {
        theTask.mPlayer = gWorld.mPlayers[inPackage->mPlayerID];
        EETown* theTown = inPackage->mTarget->GetTownByID(inPackage->mTownID);
        if (theTown != NULL) {
            if (theTown->mPlayer != this->mPlayer)
                theTarget = this->mPlayer->GetNearestTown(theTown->mHull.GetCenter(), theArea);
            else
                theTarget = theTown;
            if (this->GetUnitAllowance() > 0) {
                theLocation.mSerial = theTown->mSerial;
                theLocation.mType = kAIAreaTown;
                theLocation.mPlayer = theTown->mPlayer->mPlayerID;
                theLocation.ResetToArea();
                if (gWorld.mMap->GetTileAtLocation(theLocation.mTile) != NULL) {
                    theTask.mLocation = theLocation;
                    theTask.mType = kAGTaskLocation;
                    theTask.mMode = kAGModeCombat;
                    theTask.mIsActive = true;
                    theStrategy.Reset();
                    theStrategy.mAddFavorites = true;
                    theStrategy.mKind = 1;
                    theRequirement.Clear();
                    theRequirement.mKind = kRequirementSpecial;
                    theStrategy.mRequirements.push_back(theRequirement);
                    theRequirement.Clear();
                    theRequirement.mKind = kRequirementCounter;
                    theStrategy.mRequirements.push_back(theRequirement);
                    if (theTarget != NULL)
                        theStrategy.mTownID = theTarget->mTownID;
                    else
                        theStrategy.mContinentID = theTown->mContinent;
                    EECPAttackGroup* theGroup = this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
                    if (theGroup != NULL) {
                        theResult = true;
                        for (long i = theGroup->GetNumSubGroups() - 1; i >= 0; i--) {
                            theGroup->mSubGroups[i]->mMission = 3;
                            long theID = theGroup->mSubGroups[i]->mID;
                            inPackage->mSubGroupIDs.push_back(theID);
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x478d7b
// Releases every aid package's sub-groups and deletes the packages
void EECPDefenseCommander::FreePackages()
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it;
    for (it = this->mPackages.begin(); it != this->mPackages.end(); ++it) {
        it->second->ReleaseSubGroups();
        delete it->second;
    }
    this->mPackages.clear();
}

// FUNCTION: EXE 0x478dc8
// Attacks the enemy's chosen target unit and sends every citizen at it (fighters attack, others move); with
// the Bundeswehr power citizens first queue its research
void EECPDefenseCommander::Retaliate(EEPlayer* inEnemy)
{
    EECPUGatherCitizenUnits theGatherer(this->mPlayer);
    EETechTreeNode* theTech = NULL;
    if (inEnemy == this->mPlayer || this->mPlayer->mEnemies[inEnemy->mPlayerID] == 0 || inEnemy == NULL ||
        inEnemy->mObserverMode != false || this->mPlayer->mObserverMode != false)
        return;
    EEComplexUnit* theTarget = this->mPlayer->mCombatManager->ChooseTargetUnit(inEnemy);
    if (theTarget == NULL)
        return;
    this->mPlayer->mCombatManager->AttackUnit(theTarget);
    this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
    if (theGatherer.mCitizens.size() <= 0)
        return;
    long i;
    if (gCivPowersManager.HasPower(this->mPlayer, kCivPowerBundeswehr)) {
        EETechTree* theTree = this->mPlayer->mTechnologies;
        for (i = theGatherer.mCitizens.size() - 1; i >= 0 && theTech == NULL; i--) {
            EEComplexUnit* theUnit = theGatherer.mCitizens[i];
            for (long j = theUnit->mUnitClass->GetNumResearchTechnologies() - 1; j >= 0; j--) {
                EETechTreeNode* theNode = theTree->Get(theUnit->mUnitClass->mResearchTechnologies[j]);
                if (theNode == NULL || theNode->mData->mKind != 3 || !theNode->GetIsAvailable())
                    continue;
                EECivPower thePower;
                if (gCivPowersManager.GetPower(kCivPowerBundeswehr, thePower) && thePower.mTechnology == theNode->mData->mPrerequisite1)
                    theTech = theNode;
            }
        }
        for (i = theGatherer.mCitizens.size() - 1; i >= 0 && theTech != NULL; i--) {
            if (!(theGatherer.mCitizens[i]->mHitPoints > 0) || theGatherer.mCitizens[i]->GetTileX() == -10000 ||
                theGatherer.mCitizens[i]->mIsDisabled != false)
                continue;
            EEComplexUnit* theUnit = theGatherer.mCitizens[i];
            long theType = theUnit->mUnitClass->mUnitType;
            if (!this->mPlayer->CanProduce(theTech->mData->mID, theType, 1, true, 3, NULL))
                continue;
            theUnit->StartAction(kActionIdle);
            theUnit->mUnitClass->ActionChanged(theUnit);
            theUnit->QueueResearch(theTech->mData->mID, true);
        }
    }
    EEUGLocation* theGoal = new EEUGLocation((float)theTarget->GetTileX() + 0.5f,
                                             (float)theTarget->GetTileY() + 0.5f, 0.0f, true, false, false,
                                             0.0f, false, false);
    for (i = theGatherer.mCitizens.size() - 1; i >= 0; i--) {
        if (!(theGatherer.mCitizens[i]->mHitPoints > 0) ||
            (*(theGatherer.mCitizens.begin() + i))->GetTileX() == -10000 ||
            (*(theGatherer.mCitizens.begin() + i))->mIsDisabled != false)
            continue;
        if ((*(theGatherer.mCitizens.begin() + i))->mStats->GetAttack() > 0.0f) {
            (*(theGatherer.mCitizens.begin() + i))->SetGoal(theGoal->Clone(), 0, 0);
            (*(theGatherer.mCitizens.begin() + i))->StartAction(kActionPrepareToMove);
            EEComplexUnit* theUnit = theGatherer.mCitizens[i];
            theGatherer.mCitizens[i]->mUnitClass->ActionChanged(theUnit);
        } else {
            theGatherer.mCitizens[i]->SetGoal(
                new EEUGLocation((float)theTarget->GetTileX() + 0.5f, (float)theTarget->GetTileY() + 0.5f,
                                 0.0f, false, true, false, 0.0f, false, false), 0, 0);
            theGatherer.mCitizens[i]->StartAction(kActionPrepareToMove);
            EEComplexUnit* theUnit = theGatherer.mCitizens[i];
            theGatherer.mCitizens[i]->mUnitClass->ActionChanged(theUnit);
        }
    }
    delete theGoal;
}

// FUNCTION: EXE 0x4792ec
// Appends the combat manager's defending units that the gatherer accepts
void EECPDefenseCommander::CollectDefendingUnits(EEAIFOGatherUnits& inGatherer, std::vector<EEUnit*>& ioUnits)
{
    std::vector<EEComplexUnit*> theUnits;
    this->mPlayer->mCombatManager->CollectDefendingUnits(theUnits);
    ioUnits.reserve(theUnits.size());
    for (long i = theUnits.size() - 1; i >= 0; i--)
        if (inGatherer(theUnits[i]))
            ioUnits.push_back(theUnits[i]);
}

// FUNCTION: EXE 0x479382
// Rates the town's threat (0..1) from enemy armies in or near it minus own armies, over the owner's strength;
// optionally fills a map of threat points; outPlayerID gets the strongest enemy
float EECPDefenseCommander::ComputeTownThreat(EETown* inTown, long& outPlayerID, std::map<U2DPoint<long>, EECPDCTownThreat>* ioMap,
                                    bool inFill)
{
    std::vector<std::pair<EEPlayer*, unsigned long> > theEnemies;
    std::map<U2DPoint<long>, EECPDCTownThreat>::iterator it;
    EECPDCTownThreat theThreat;
    U2DPoint<long> thePoint;
    float theResult = 0.0f;
    EEPlayer* theOwner = inTown->mPlayer;
    long theSum = 0;
    float theRange;
    long i;
    switch (this->mPlayer->mDifficulty) {
    case kDifficultyEasy:
        theRange = theOwner->IsHuman() == true ? 3025.0f : 225.0f;
        break;
    case kDifficultyMedium:
        theRange = 900.0f;
        break;
    default:
        theRange = 3025.0f;
        break;
    }
    outPlayerID = 0;
    theThreat.mOwner = theOwner->mPlayerID;
    for (i = gWorld.mNumPlayers - 1; i > 0; i--) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer != theOwner && theOwner->mEnemies[thePlayer->mPlayerID] != 0)
            theEnemies.push_back(std::pair<EEPlayer*, unsigned long>(thePlayer, 0));
    }
    for (long j = theEnemies.size() - 1; j >= 0; j--) {
        EECPIntelligencePlayerFile* theFile =
            this->mPlayer->mCombatManager->mIntelligence->GetFile((theEnemies[j]).first);
        if (theFile == NULL)
            continue;
        for (long k = theFile->mArmies.size() - 1; k >= 0; k--) {
            EECPIArmy* theArmy = theFile->mArmies[k];
            (theEnemies[j]).second += theArmy->mStrength;
            if (!(theArmy->mStrength > 0))
                continue;
            if (inTown->mHull.BoundedContains(theArmy->mHull.GetCenter())) {
                theSum += (long)(theArmy->mStrength * 2.0f);
                if (inFill) {
                    thePoint = inTown->mHull.GetClosestPoint(theArmy->mHull.GetCenter());   // the army's (was the stale point)
                    it = ioMap->find(thePoint);
                    if (it != ioMap->end())
                        it->second.mEnemy += (long)(theArmy->mStrength * 1.5f);
                    else {
                        theThreat.mEnemy = (long)(theArmy->mStrength * 1.5f);
                        theThreat.mLocation = thePoint;
                        ioMap->insert(std::map<U2DPoint<long>, EECPDCTownThreat>::value_type(thePoint, theThreat));
                    }
                }
            } else {
                thePoint = inTown->mHull.GetClosestOutlinePoint(theArmy->mHull.GetCenter());
                float theDistance = thePoint.DistanceToSquaredF(theArmy->mHull.GetCenter());
                if (theDistance < theRange) {
                    long theValue = (long)(theDistance / theRange * theArmy->mStrength);
                    theSum += theValue + theArmy->mStrength;
                    if (inFill) {
                        thePoint = inTown->mHull.GetClosestPoint(thePoint);
                        it = ioMap->find(thePoint);
                        if (it != ioMap->end())
                            it->second.mEnemy += theValue;
                        else {
                            theThreat.mLocation = thePoint;
                            theThreat.mEnemy = theValue;
                            ioMap->insert(std::map<U2DPoint<long>, EECPDCTownThreat>::value_type(thePoint, theThreat));
                        }
                    }
                }
            }
        }
    }
    EECPIntelligencePlayerFile* theOwn = this->mPlayer->mCombatManager->mIntelligence->GetFile(inTown->mPlayer);
    if (theOwn != NULL) {
        for (long k = theOwn->GetNumArmies() - 1; k >= 0; k--) {
            EECPIArmy* theArmy = theOwn->mArmies[k];
            if (!(theArmy->mStrength > 0))
                continue;
            if (inTown->mHull.BoundedContains(theArmy->mHull.GetCenter()))
                theSum -= theArmy->mStrength;
            else {
                thePoint = inTown->mHull.GetClosestOutlinePoint(theArmy->mHull.GetCenter());
                float theDistance = thePoint.DistanceToSquaredF(theArmy->mHull.GetCenter());
                if (theDistance < kDCOwnArmyRange) {
                    long theValue = (long)(theDistance * kDCOwnArmyFactor * theArmy->mStrength);
                    theSum -= theValue;
                    if (inFill) {
                        thePoint = inTown->mHull.GetClosestPoint(thePoint);
                        it = ioMap->find(thePoint);
                        if (it != ioMap->end())
                            it->second.mOwn += theValue;
                        else {
                            theThreat.mLocation = thePoint;
                            theThreat.mOwn = theValue;
                            ioMap->insert(std::map<U2DPoint<long>, EECPDCTownThreat>::value_type(thePoint, theThreat));
                        }
                    }
                }
            }
        }
    }
    if (theSum <= 0)
        theResult = 0.0f;
    else {
        EECPIntelligencePlayerFile* theFile = this->mPlayer->mCombatManager->mIntelligence->GetFile(inTown->mPlayer);
        if (theFile != NULL) {
            unsigned long theTotal = theFile->mSeaStrength + theFile->mLandStrength + theFile->mAirStrength + theFile->mSpaceStrength;
            if (theTotal > 0) {
                theResult = (float)theSum / theTotal;
                if (theResult >= 1.0f)
                    theResult = 1.0f;
            } else
                theResult = 1.0f;
        }
    }
    if (theEnemies.size() <= 0)
        return theResult;
    std::sort(theEnemies.begin(), theEnemies.end(), EECPDCEnemyGreater());
    outPlayerID = theEnemies.begin()->first->mPlayerID;
    if (inFill)
        for (it = ioMap->begin(); it != ioMap->end(); ++it)
            it->second.mAttacker = outPlayerID;
    return theResult;
}

// FUNCTION: EXE 0x4799fa
// Returns how many more population the defensive groups may use (defensive cap minus used share of the unit
// limit); with no unit limit it returns the population already used
long EECPDefenseCommander::GetUnitAllowance()
{
    std::vector<EECPAttackSubGroup*> theGroups;
    EEAIFOSubGroupFilter theFilter(0, true, true);
    float theShare = this->mPlayer->mDefensiveUnitCap;
    long theTotal = this->mPlayer->mCombatManager->GetUnitLimit();
    long theUsed = 0;
    long theKind = 0;
    theFilter.mKinds.insert(theKind);
    theKind = 2;
    theFilter.mKinds.insert(theKind);
    theKind = 5;
    theFilter.mKinds.insert(theKind);
    theKind = 1;
    theFilter.mKinds.insert(theKind);
    theKind = 6;
    theFilter.mKinds.insert(theKind);
    this->mAttackCommander->CollectFiltered(theGroups, theFilter);
    for (long i = theGroups.size() - 1; i >= 0; i--) {
        if ((theGroups[i])->mRequest == NULL) {
            for (long j = (theGroups[i])->GetNumUnits() - 1; j >= 0; j--)
                theUsed += (theGroups[i])->GetUnitUnit(j)->GetUnitClass()->GetPopulationCost();
        } else
            theUsed += (theGroups[i])->mRequest->GetPopulation();
    }
    if (theTotal > 0) {
        float theFraction = (float)theUsed / theTotal;
        theUsed = theFraction < theShare ? (long)((theShare - theFraction) * theTotal) : 0;
    }
    return theUsed;
}

// FUNCTION: EXE 0x479bd1
// Returns the player's (or area's) most threatened town and fills the package with it, else the first
// allied town without a package; 0 if none
long EECPDefenseCommander::FindThreatenedTown(EEPlayer* inPlayer, EECPMilitaryAidPackage* ioPackage, unsigned short inArea)
{
    std::vector<long> theUnused;
    std::vector<EETown*> theTowns;
    std::vector<EETown*> theOthers;
    EETown* theTown = NULL;
    EETown* theBest = NULL;
    long theResult = 0;
    long thePlayerID = 0;
    long i;
    float theThreat;
    float theBestThreat = 0.0f;
    ioPackage->mThreat = 0.0f;
    ioPackage->mTownID = 0;
    ioPackage->mLocation = U2DPoint<long>(0, 0);
    inPlayer->GetTowns(inArea, theTowns);
    if (theTowns.size() > 0)
        std::sort(theTowns.begin(), theTowns.end(), EECPDCTownValueGreater());
    for (i = theTowns.size() - 1; i >= 0; i--) {
        theTown = theTowns[i];
        if (theTown == NULL) {
            theBest = theTown;
            std::map<U2DPoint<long>, EECPDCTownThreat> theMap;
            theBestThreat = this->ComputeTownThreat(theBest, thePlayerID, &theMap, false);
        } else {
            long theID = theTown->mTownID;
            std::map<long, EECPMilitaryAidPackage*>::iterator it = this->mPackages.find(theID);
            if (it == this->mPackages.end()) {
                std::map<U2DPoint<long>, EECPDCTownThreat> theMap;
                theThreat = this->ComputeTownThreat(theTown, thePlayerID, &theMap, false);
                if (inPlayer != this->mPlayer && this->mPlayer->mEnemies[inPlayer->mPlayerID] == 0)
                    theOthers.push_back(theTown);
            } else
                theThreat = it->second->mThreat;
            if (theBestThreat < theThreat) {
                theBest = theTown;
                theBestThreat = theThreat;
            }
        }
    }
    if (theBest != NULL) {
        if (theBestThreat < 0.0f)
            theBestThreat = 0.0f;
        theResult = theBest->mTownID;
        ioPackage->mThreat = theBestThreat;
        ioPackage->mTownID = theResult;
        ioPackage->mLocation = theBest->mHull.GetCenter();
    } else {
        if (theOthers.size() <= 0)
            return theResult;
        theResult = theOthers.front()->mTownID;
        ioPackage->mThreat = 0.0f;
        ioPackage->mTownID = theResult;
        ioPackage->mLocation = theOthers.front()->mHull.GetCenter();
    }
    ioPackage->mPlayerID = thePlayerID;
    return theResult;
}

// FUNCTION: EXE 0x479e41
// When enabled: refreshes intelligence, updates packages, aids own and allied towns, plans aid when due and
// checks raids, ships, space units and specials; schedules the next update
void EECPDefenseCommander::Update(unsigned long inTime)
{
    unsigned long theDelay = 8000;
    if (this->mAttackCommander->mPlayer->mCanDefend && this->mEnabled) {
        this->RefreshIntelligence();
        theDelay = this->UpdatePackages(inTime);
        this->AidOwnTowns();
        this->AidAllies();
        if (inTime >= this->mNextAidTime)
            this->PlanAid(inTime);
        this->CheckRaids();
        this->CheckNaval(inTime);
        this->CheckSpace(inTime);
        this->CheckSpecials(inTime);
    }
    this->mNextUpdateTime = theDelay + inTime;
}

// FUNCTION: EXE 0x479eb8
// Reads the timers, the flag, the aid packages and loose sub-group ids from a saved game (by version);
// returns the last read's error
long EECPDefenseCommander::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    unsigned long theCount;
    unsigned long i;
    bool theValid;
    EECPMilitaryAidPackage* thePackage;
    unsigned long theID;
    this->mLoadedPackages.clear();
    this->FreePackages();
    if (inMajorVersion == 0 && inMinorVersion < 0xd0) {
        theResult = inFile.Read(this->mNextUpdateTime);
        if (theResult == 0)
            theResult = inFile.Read(this->mEnabled);
        theCount = 0;
        theResult = inFile.Read(theCount);
        for (i = 0; i < theCount; i++) {
            theValid = false;
            thePackage = NULL;
            if (theResult != 0)
                continue;
            theResult = inFile.Read(theValid);
            if (!theValid)
                continue;
            thePackage = new EECPMilitaryAidPackage(this->mPlayer);
            thePackage->Load(0, inMinorVersion, inFile);
            this->mLoadedPackages.push_back(thePackage);
        }
        if (theResult == 0)
            theResult = inFile.Read(this->mNextAidTime);
        theCount = 0;
        theResult = inFile.Read(theCount);
        this->mLoadedSubGroupIDs.resize(theCount, 0);
        for (i = 0; i < theCount; i++) {
            theID = 0;
            if (theResult == 0)
                theResult = inFile.Read(theID);
            this->mLoadedSubGroupIDs[i] = theID;
        }
    } else {
        theResult = inFile.Read(this->mNextUpdateTime);
        if (theResult == 0)
            theResult = inFile.Read(this->mEnabled);
        theCount = 0;
        theResult = inFile.Read(theCount);
        for (i = 0; i < theCount; i++) {
            thePackage = NULL;
            theValid = false;
            if (theResult != 0)
                continue;
            theResult = inFile.Read(theValid);
            if (!theValid)
                continue;
            thePackage = new EECPMilitaryAidPackage(this->mPlayer);
            thePackage->Load(inMajorVersion, inMinorVersion, inFile);
            this->mLoadedPackages.push_back(thePackage);
        }
        if (theResult == 0)
            theResult = inFile.Read(this->mNextAidTime);
        theCount = 0;
        theResult = inFile.Read(theCount);
        this->mLoadedSubGroupIDs.resize(theCount, 0);
        for (i = 0; i < theCount; i++) {
            theID = 0;
            if (theResult == 0)
                theResult = inFile.Read(theID);
            this->mLoadedSubGroupIDs[i] = theID;
        }
        if (theResult == 0)
            theResult = inFile.Read(this->mNextNavalTime);
    }
    if ((inMajorVersion > 0 || inMinorVersion >= 0xed) && theResult == 0)
        theResult = inFile.Read(this->mNextSpaceTime);
    if ((inMajorVersion > 0 || inMinorVersion >= 0xf0) && theResult == 0)
        theResult = inFile.Read(this->mNextSpecialTime);
    return theResult;
}

// FUNCTION: EXE 0x47a142
// Re-links the loaded packages by town (merging duplicates) and puts loose sub-groups back on inactive
// defence; always succeeds
long EECPDefenseCommander::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<EECPMilitaryAidPackage*> theDone;
    long i;
    for (i = this->mLoadedPackages.size() - 1; i >= 0; i--) {
        (this->mLoadedPackages[i])->Restore(inMajorVersion, inMinorVersion);
        if ((this->mLoadedPackages[i])->mTownID != 0) {
            long theID = (this->mLoadedPackages[i])->mTownID;
            std::map<long, EECPMilitaryAidPackage*>::iterator it = this->mPackages.find(theID);
            if (it == this->mPackages.end()) {
                this->mPackages.insert(std::make_pair(
                    (this->mLoadedPackages[i])->mTownID, this->mLoadedPackages[i]));
                continue;
            }
            for (long j = (this->mLoadedPackages[i])->mSubGroupIDs.size() - 1; j >= 0; j--) {
                long theUnit = (this->mLoadedPackages[i])->mSubGroupIDs[j];
                it->second->mSubGroupIDs.push_back(theUnit);
            }
        } else
            this->mLoadedSubGroupIDs.insert(this->mLoadedSubGroupIDs.end(), (this->mLoadedPackages[i])->mSubGroupIDs.begin(), (this->mLoadedPackages[i])->mSubGroupIDs.end());
        theDone.push_back(this->mLoadedPackages[i]);
    }
    for (i = this->mLoadedSubGroupIDs.size() - 1; i >= 0; i--) {
        EECPAttackSubGroup* theGroup = this->mAttackCommander->GetSubGroup(this->mLoadedSubGroupIDs[i]);
        if (theGroup != NULL) {
            theGroup->mTask.mMode = kAGModeDefense;
            theGroup->mTask.mIsActive = false;
        }
    }
    for (i = theDone.size() - 1; i >= 0; i--)
        if (theDone[i] != NULL)
            delete theDone[i];
    theDone.clear();
    this->mLoadedPackages.clear();
    this->mLoadedSubGroupIDs.clear();
    return 0;
}

// FUNCTION: EXE 0x47b088
// Returns the town's aid package, or NULL
EECPMilitaryAidPackage* EECPDefenseCommander::GetPackage(long inTownID)
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it = this->mPackages.find(inTownID);
    if (it != this->mPackages.end())
        return it->second;
    return NULL;
}

// FUNCTION: EXE 0x47b0b2
// Schedules the next aid after the difficulty's (deathmatch) defensive update delay
void EECPDefenseCommander::SetNextAidTime(unsigned long inTime)
{
    long theDelay;
    if (this->mPlayer->IsDeathmatch() == false) {
        switch (this->mPlayer->mDifficulty) {
        case kDifficultyEasy:
            theDelay = this->mPlayer->mDefensiveUnitUpdateEasy;
            break;
        case kDifficultyMedium:
            theDelay = this->mPlayer->mDefensiveUnitUpdateMedium;
            break;
        default:
            theDelay = this->mPlayer->mDefensiveUnitUpdateHard;
            break;
        }
    } else {
        switch (this->mPlayer->mDifficulty) {
        case kDifficultyEasy:
            theDelay = this->mPlayer->mDMDefensiveUnitUpdateEasy;
            break;
        case kDifficultyMedium:
            theDelay = this->mPlayer->mDMDefensiveUnitUpdateMedium;
            break;
        default:
            theDelay = this->mPlayer->mDMDefensiveUnitUpdateHard;
            break;
        }
    }
    this->mNextAidTime = theDelay + inTime;
}

// FUNCTION: EXE 0x47b115
// Writes the timers, the flag and every package (invalid towns' sub-groups saved as loose ids) to the saved
// game; returns the first write error
long EECPDefenseCommander::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it;
    unsigned long theCount = 0;
    bool theValid = true;
    long theResult = outFile.Write(this->mNextUpdateTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mEnabled);
    this->mLoadedSubGroupIDs.clear();
    theCount = this->mPackages.size();
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    for (it = this->mPackages.begin(); it != this->mPackages.end(); ++it) {
        theValid = false;
        if (it->second->mTarget != NULL) {
            EETown* theTown = it->second->mTarget->GetTownByID(it->second->mTownID);
            if (theTown != NULL)
                theValid = theTown->mTiles.Size() > 0;
        }
        if (theResult == 0)
            theResult = outFile.Write(theValid);
        if (theValid)
            it->second->Save(inMajorVersion, inMinorVersion, outFile);
        else {
            const EECPMilitaryAidPackage* thePackage = it->second;
            this->mLoadedSubGroupIDs.insert(this->mLoadedSubGroupIDs.end(), thePackage->mSubGroupIDs.begin(), thePackage->mSubGroupIDs.end());
        }
    }
    if (theResult == 0)
        theResult = outFile.Write(this->mNextAidTime);
    theCount = this->mLoadedSubGroupIDs.size();
    if (theResult == 0)
        theResult = outFile.Write(theCount);
    for (unsigned long i = 0; i < theCount; i++)
        if (theResult == 0)
            theResult = outFile.Write(this->mLoadedSubGroupIDs[i]);
    if (theResult == 0) {
        theResult = outFile.Write(this->mNextNavalTime);
        if (theResult == 0) {
            theResult = outFile.Write(this->mNextSpaceTime);
            if (theResult == 0)
                theResult = outFile.Write(this->mNextSpecialTime);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47b2d3
// For a friendly town: sends the best nearby free sub-group (unless counter-attack only), else a new
// counter-attack group; returns whether help was sent
bool EECPDefenseCommander::AssignSubGroups(EECPMilitaryAidPackage* inPackage, bool inCounterAttackOnly)
{
    EEAIFOSubGroupFilter theFilter(0, true, false);
    std::vector<EECPAttackSubGroup*> theGroups;
    std::vector<unsigned short> theAreas;
    EECPAttackCommander* theCommander = this->mAttackCommander;
    bool theResult = false;
    if (inPackage->mTarget != NULL
        && (inPackage->mTarget == this->mPlayer || this->mPlayer->mEnemies[inPackage->mTarget->mPlayerID] == 0)) {
        EETown* theTown = inPackage->mTarget->GetTownByID(inPackage->mTownID);
        if (theTown != NULL) {
            long theKind;
            theFilter.mAllowAdjacent = true;
            theFilter.mContinent = theTown->mContinent;
            theKind = 0;
            theFilter.mKinds.insert(theKind);
            theKind = 2;
            theFilter.mKinds.insert(theKind);
            theKind = 5;
            theFilter.mKinds.insert(theKind);
            if (theTown->mNearWater) {
                theKind = 1;
                theFilter.mKinds.insert(theKind);
            }
            if (theTown->mNearMapEdge) {
                theKind = 9;
                theFilter.mKinds.insert(theKind);
            }
            if (inCounterAttackOnly == false) {
                theFilter.SetPackages(this->mPackages);
                theCommander->CollectFiltered(theGroups, theFilter);
                theResult = this->SendBestSubGroup(theGroups, inPackage);
                if (theResult)
                    return theResult;
            }
            theResult = this->CounterAttack(inPackage);
        } else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x47b442
// Updates the due packages and deletes finished ones; returns the shortest next delay (at most 30 s)
unsigned long EECPDefenseCommander::UpdatePackages(unsigned long inTime)
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it;
    unsigned long theDelay = 30000;
    it = this->mPackages.begin();
    while (it != this->mPackages.end()) {
        unsigned long theNext;
        theNext = inTime < it->second->mNextUpdateTime ? 0 : it->second->Update(inTime);
        if (!it->second->IsFinished() ||
            it->second->mTarget == this->mPlayer && it->second->GetNumUnits() != 0 || it->second->mNextSendTime > inTime) {
            ++it;
            if (theNext != 0 && theNext <= theDelay)
                theDelay = theNext;
        } else {
            delete it->second;
            it->second = NULL;
            it = this->mPackages.erase(it);
        }
    }
    return theDelay;
}

// FUNCTION: EXE 0x47b502
// Every ship-maintenance period while gDCGameSpeed > 50: tops ships up to the difficulty's quota (freeing
// population if needed) and, from the WW2 epoch, the carrier fighters; returns the next check time
unsigned long EECPDefenseCommander::CheckNaval(unsigned long inTime)
{
    if (this->mNextNavalTime < inTime) {
        if (gDCGameSpeed > 0x32) {
            EECPUGatherNavalUnitsCount theNaval(this->mPlayer);
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theNaval);
            long theLimit;
            switch (this->mPlayer->mDifficulty) {
            case kDifficultyEasy:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMEasy : this->mPlayer->mShipsToMaintainEasy;
                break;
            case kDifficultyMedium:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMMedium : this->mPlayer->mShipsToMaintainMedium;
                break;
            default:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMHard : this->mPlayer->mShipsToMaintainHard;
                break;
            }
            if (theNaval.mUnits + theNaval.mRequested < theLimit) {
                EECPCombatStrategy theStrategy;
                EECPAGTask theTask;
                EECPUnitRequirement theRequirement;
                theLimit = theLimit - theNaval.mUnits - theNaval.mRequested;
                long theAvailable = this->mPlayer->mCombatManager->GetUnitsAvailable();
                if (theAvailable < theLimit) {
                    EEAIFOUnitsToKillForNavalMaintenance theScorer(this->mPlayer, theLimit - theAvailable);
                    this->mPlayer->ChooseUnitsToKill(theScorer, 0);
                }
                theTask.mPlayer = this->mPlayer->mCombatManager->GetIntelligence()->GetRandomEnemy(this->mPlayer);
                theStrategy.Reset();
                theStrategy.mAddFavorites = true;
                theStrategy.mKind = 5;
                theStrategy.mSeaShare = 100.0f;
                theStrategy.mNumUnits = theLimit;
                theStrategy.mNumRemaining = theLimit;
                theRequirement.Clear();
                theRequirement.mKind = kRequirementCounter;
                theStrategy.mRequirements.push_back(theRequirement);
                std::vector<long> theUnitIDs;
                this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
            }
            long theEpoch = this->mPlayer->mTechnologies->mCurrentEpoch;
            if (theEpoch >= kTechEpochAtomicWW2) {
                EECPUGatherCarrierFightersCount theFighters(this->mPlayer);
                this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theFighters);
                long theWanted;
                switch (this->mPlayer->mDifficulty) {
                case kDifficultyEasy:
                    theWanted = this->mPlayer->IsDeathmatch() ? gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mIntValue1 : gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mIntValue1;
                    break;
                case kDifficultyMedium:
                    theWanted = this->mPlayer->IsDeathmatch() ? gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mIntValue2 : gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mIntValue2;
                    break;
                default:
                    theWanted = this->mPlayer->IsDeathmatch() ? (long)gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mFloatValue : (long)gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mFloatValue;
                    break;
                }
                theWanted = theWanted - theFighters.mRequested - theFighters.mUnits;
                if (theWanted > 0) {
                    long theType;
                    switch (theEpoch) {
                    case kTechEpochAtomicWW2:
                        theType = 0x55;
                        break;
                    case kTechEpochAtomicModern:
                        theType = 0x50;
                        break;
                    default:
                        theType = 0x2a8;
                        break;
                    }
                    this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(theType, theWanted, 0, 0);
                }
            }
        }
        this->mNextNavalTime = this->mPlayer->mShipMaintenanceTimer * 60000 + inTime;
    }
    return this->mNextNavalTime;
}

// FUNCTION: EXE 0x47b7fc
// Adds a pending request's count if it is for a carrier fighter
bool EECPUGatherCarrierFightersCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && this->IsFighter(inClass)) {
        this->mRequested += inRequest->mCount;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x47b828
// Counts a carrier fighter
bool EECPUGatherCarrierFightersCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && this->IsFighter(inUnit->mUnitClass)) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x47b84e
// Counts a carrier-fighter class's units
bool EECPUGatherCarrierFightersCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL && this->IsFighter(inClass)) {
        this->mUnits += inUnits->size();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x47b880
// Returns true for the five carrier fighter unit types
bool EECPUGatherCarrierFightersCount::IsFighter(EEUnitClass* inClass)
{
    return inClass != NULL && (inClass->mUnitType == kUnitTypeCarrierFighter11Corsair1 || inClass->mUnitType == kUnitTypeCarrierFighter12F14 ||
                               inClass->mUnitType == kUnitTypeCarrierFighter13Avenger || inClass->mUnitType == kUnitTypeCarrierFighter11Dauntless ||
                               inClass->mUnitType == kUnitTypeCarrierFighter11Zero);
}

// FUNCTION: EXE 0x47b8b7
// Accepts living, collidable non-ship citizens that cost population, while citizens may be killed
bool EEAIFOUnitsToKillForNavalMaintenance::CanKill(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && inUnit->mIsCitizen && !(inUnit->mUnitClass->mDomains & kMovementSea) &&
           inUnit->mUnitClass->GetPopulationCost() > 0 && inUnit->mIsCollidable != 0 && (!inUnit->mIsCitizen || this->mCitizensToKill > 0);
}

// FUNCTION: EXE 0x47b923
// Every ship-maintenance period on space maps: tops space units up to the ship quota (freeing population if
// needed) and requests a space carrier while under 4 and fewer than the space docks
unsigned long EECPDefenseCommander::CheckSpace(unsigned long inTime)
{
    if (this->mNextSpaceTime < inTime) {
        if (gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
            EECPUGatherSpaceUnitsCount theSpace(this->mPlayer);
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theSpace);
            long theLimit;
            switch (this->mPlayer->mDifficulty) {
            case kDifficultyEasy:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMEasy : this->mPlayer->mShipsToMaintainEasy;
                break;
            case kDifficultyMedium:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMMedium : this->mPlayer->mShipsToMaintainMedium;
                break;
            default:
                theLimit = this->mPlayer->IsDeathmatch() ? this->mPlayer->mShipsToMaintainDMHard : this->mPlayer->mShipsToMaintainHard;
                break;
            }
            if (theSpace.mUnits + theSpace.mRequested < theLimit) {
                EECPCombatStrategy theStrategy;
                EECPAGTask theTask;
                EECPUnitRequirement theRequirement;
                theLimit = theLimit - theSpace.mUnits - theSpace.mRequested;
                long theAvailable = this->mPlayer->mCombatManager->GetUnitsAvailable();
                if (theAvailable < theLimit) {
                    EEAIFOUnitsToKillForSpaceMaintenance theScorer(this->mPlayer, theLimit - theAvailable);
                    this->mPlayer->ChooseUnitsToKill(theScorer, 0);
                }
                theTask.mPlayer = this->mPlayer->mCombatManager->GetIntelligence()->GetRandomEnemy(this->mPlayer);
                theStrategy.Reset();
                theStrategy.mAddFavorites = true;
                theStrategy.mKind = 5;
                theStrategy.mSpaceShare = 100.0f;
                theStrategy.mNumUnits = theLimit;
                theStrategy.mNumRemaining = theLimit;
                theRequirement.Clear();
                theRequirement.mKind = kRequirementCounter;
                theStrategy.mRequirements.push_back(theRequirement);
                std::vector<long> theUnitIDs;
                this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
            }
            EECPUGatherSpecialUnitsCount theCarriers(this->mPlayer, 0x2f3, 0);
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theCarriers);
            std::vector<EEUnit*> theUnits;
            EEGetPlayerUnitsOfType(kUnitTypeSpaceDock, this->mPlayer->mPlayerID, theUnits);
            unsigned long theCount = theCarriers.mUnits + theCarriers.mRequested;
            if (theCount < theUnits.size() && theCount < 4)
                this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(0x2f3, 1, 0, 0);
        }
        this->mNextSpaceTime = this->mPlayer->mShipMaintenanceTimer * 60000 + inTime;
    }
    return this->mNextSpaceTime;
}

// FUNCTION: EXE 0x47bbf1
// Accepts living, collidable non-space citizens that cost population, while citizens may be killed
bool EEAIFOUnitsToKillForSpaceMaintenance::CanKill(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && inUnit->mIsCitizen && !(inUnit->mUnitClass->mDomains & kMovementSpace) &&
           inUnit->mUnitClass->GetPopulationCost() > 0 && inUnit->mIsCollidable != 0 && (!inUnit->mIsCitizen || this->mCitizensToKill > 0);
}

// FUNCTION: EXE 0x47bc5b
// Keeps 1..10 anti-missile batteries (one more per town centre), casts cloaking from capitols near threatened
// towns, and keeps one ICBM with the missile power; returns the next check time
unsigned long EECPDefenseCommander::CheckSpecials(unsigned long inTime)
{
    if (this->mNextSpecialTime < inTime) {
    EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(gUnitClassTable[kUnitType14AntiMissileBattery]->GetTechnology());
    if (theNode != NULL && !theNode->GetIsObsolete() && this->mPlayer->GetTechTree()->mCurrentEpoch >= theNode->mEpoch &&
        theNode->GetIsAvailable()) {
        std::vector<EEUnit*> theUnits;
        EEGetPlayerUnitsOfType(kUnitTypeSettlement, this->mPlayer->mPlayerID, theUnits);
        EEGetPlayerUnitsOfType(kUnitTypeTownCenter, this->mPlayer->mPlayerID, theUnits);
        EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theUnits);
        EECPUGatherSpecialUnitsCount theAMBs(this->mPlayer, 0x2e5, 0);
        this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theAMBs);
        unsigned long theCount = theAMBs.mUnits + theAMBs.mRequested;
        if (theCount >= 1) {
            if (theCount > 10) {
                EEAIFOAMBToKill theScorer(theAMBs.mUnits + theAMBs.mRequested - 10);
                this->mPlayer->ChooseUnitsToKill(theScorer, 0);
            } else if (theCount < theUnits.size() && theCount + 1 < 10)
                this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(0x2e5, 1, 0, 0);
        } else
            this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(0x2e5, 1, 0, 0);
    }
    if (gCivPowersManager.HasPower(this->mPlayer, kCivPowerCloaking)) {
        std::vector<EEUnit*> theUnits;
        EETileLocation theTile;
        EETileLocation theOther;
        long thePlayerID = 0;
        EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theUnits);
        EECalamityClass* theCalamity = NULL;
        EECalamityManager::GetInstance()->GetCalamityClass(0x1f, theCalamity);
        for (unsigned long i = 0; i < theUnits.size(); i++) {
            EEComplexUnit* theUnit = (theUnits[i])->GetComplexUnit();
            long thePower = theUnit->mPower;
            if (!(thePower >= theCalamity->GetCost()))
                continue;
            theTile = theUnit->GetTile();
            EETown* theTown = this->mPlayer->GetNearestTown(theTile, theOther);
            float theThreat = this->GetTownThreat(theTown, thePlayerID);
            GE3DPoint theLocation;
            if (!(theThreat > kDCCalamityThreat) ||
                theCalamity->CanCastAt(theUnit, theLocation, theUnit->GetTileX(), theUnit->GetTileY(), true, NULL) != 0)
                continue;
            theUnit->SetGoal(new EEUGCastCalamity(theUnit, NULL, theUnit->GetTileX(), theUnit->GetTileY(), 0x43d, true),
                             0, 0);
            theUnit->StartAction(kActionCalamityPreCast);
            theUnit->mUnitClass->ActionChanged(theUnit);
        }
    }
    if (gCivPowersManager.HasPower(this->mPlayer, kCivPowerMissileBase)) {
        EETechTreeNode* theNode2 = this->mPlayer->GetTechTree()->Get(gUnitClassTable[kUnitTypeCpICBM]->GetTechnology());
        if (theNode2 != NULL && !theNode2->GetIsObsolete() && this->mPlayer->GetTechTree()->mCurrentEpoch >= theNode2->mEpoch &&
            theNode2->GetIsAvailable()) {
            EECPUGatherSpecialUnitsCount theOthers(this->mPlayer, 0x2ee, 0);
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theOthers);
            unsigned long theTotal = theOthers.mUnits + theOthers.mRequested;
            if (theTotal < 1)
                this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(0x2ee, 1, 0, 0);
        }
    }
    this->mNextSpecialTime = this->mPlayer->mShipMaintenanceTimer * 30000 + inTime;
    }
    return this->mNextSpecialTime;
}

// FUNCTION: EXE 0x47c04f
// Scores a battery for deletion: other batteries within 4 tiles minus town centres there
long EEAIFOAMBToKill::GetKillScore(EEComplexUnit* inUnit)
{
    GE3DPoint theCenter;
    std::vector<EEUnit*> theUnits;
    inUnit->GetPosition(theCenter);
    EEGetUnitsNear(theCenter, 4.0f, theUnits);
    unsigned short theAMBs = 0;
    unsigned short theTargets = 0;
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        if (*it == inUnit)
            continue;
        unsigned long theType = (*it)->mUnitClass->mUnitType;
        if (theType == kUnitType14AntiMissileBattery)
            theAMBs++;
        else if (theType == kUnitTypeCapitol || theType == kUnitTypeTownCenter || theType == kUnitTypeSettlement)
            theTargets++;
    }
    return theAMBs - theTargets;
}

// The combat manager's tasks are tried with a chance of at least 90%; forests need move flag 4 and adjacent
// continents flag 3; choke points go nearest first when inHasContinent and there are more than the player's
// limit. NEAR: the original also returns outTask.mPlayer in al (declared void here)
// FUNCTION: EXE 0x47a461
// Gives a sub-group its next defence task, first come: a combat-manager task, a nearby forest, adjacent
// continent, hill, choke point, threatened town, else its own continent
void EECPDefenseCommander::AssignSubGroupTask(EECPAttackSubGroup* inSubGroup, EECPAGTask& outTask, bool inHasContinent)
{
    std::multimap<long, EEAIChokePoint*> theChokeMap;
    std::vector<EEAIChokePoint*> theChokePoints;
    std::vector<unsigned short> theHillIDs;
    std::vector<unsigned short> theContinentIDs;
    std::vector<unsigned short> theForestIDs;
    std::vector<EEAIForest*> theForests;
    std::vector<EEAIContinent*> theContinents;
    std::vector<EEAIHill*> theHills;
    std::vector<long> theUnused;
    EECPMilitaryAidPackage thePackage(this->mPlayer);
    EECPILocation theLocation;
    EECPILocation theFrom;
    bool theFound = false;
    bool theCombat3 = false;
    U2DPoint<long> theClosest;
    bool theCombat1 = true;
    bool theCombat2 = true;
    bool theAnyContinent = inSubGroup->GetGuardContinentPercentage() > gPhysics->GetRandom();
    unsigned short theContinentID = inSubGroup->GetMainContinent();
    long i;
    inSubGroup->GetLocation(theFrom);
    outTask.mIsActive = false;
    if (this->mAttackCommander->GetCombatManager()->mDefendTasks.size() > 0) {
        float theChance = max(0.9f, this->mAttackCommander->GetCombatManager()->mDefendTasks.size() * 0.05f + 0.5f);
        switch (inSubGroup->mKind) {
        case kASGKindLand:
            theCombat2 = false;
            break;
        case kASGKindSea:
        case kASGKindSeaTransport:
        case kASGKindSubmarine:
            theCombat1 = false;
            break;
        case kASGKindSpaceship:
        case kASGKindSpaceTransport:
            theCombat1 = false;
            theCombat2 = false;
            theCombat3 = true;
            break;
        }
        if (gPhysics->GetRandom() < theChance) {
            theFound = this->mAttackCommander->GetCombatManager()->ChooseTask(outTask, theCombat1, theCombat2, theCombat3,
                                                                     theAnyContinent ? 0 : theContinentID);
            outTask.mIsActive = theFound;
        }
    }
    outTask.mPlayer = inSubGroup->mGroup->mTask.mPlayer;
    if (theFound)
        return;
    if (inSubGroup->mCommonMoveFlags.size() > 0) {
        std::vector<long>::iterator it;
        for (it = inSubGroup->mCommonMoveFlags.begin(); it != inSubGroup->mCommonMoveFlags.end(); it++)
            if (*it == 4)
                break;
        if (it != inSubGroup->mCommonMoveFlags.end() && gPhysics->GetRandom() < 0.8f) {
            EEAIContinent* theContinent = gContinentManager.mContinents[theContinentID];
            if (theContinent && theContinent->mForestIDs.size() > 0) {
                theForestIDs = theContinent->mForestIDs;
                for (i = theForestIDs.size() - 1; i >= 0; i--) {
                    EEAIForest* theForest = gContinentManager.mForests[theForestIDs[i]];
                    if (theForest && (theForest->mHull.BoundedContains(theFrom.mTile) ||
                                      theForest->GetClosestHullPoint(theFrom.mTile).DistanceToSquared(theFrom.mTile) <= 1225))
                        theForests.push_back(theForest);
                }
                if (theForests.size() != 0) {
                    long theIndex = gPhysics->GetRandom(0L, (long)theForests.size() - 1);
                    theLocation.mSerial = (theForests[theIndex])->mSerial;
                    theLocation.mType = kAIAreaForest;
                    theLocation.mTile = (theForests[theIndex])->GetRandomPoint();
                    outTask.mLocation = theLocation;
                    outTask.mMode = kAGModeDefense;
                    outTask.mType = kAGTaskLocation;
                    outTask.mIsActive = true;
                }
            }
        }
        if (!outTask.mIsActive) {
            for (it = inSubGroup->mCommonMoveFlags.begin(); it != inSubGroup->mCommonMoveFlags.end(); it++)
                if (*it == 3)
                    break;
            if (it != inSubGroup->mCommonMoveFlags.end() && gPhysics->GetRandom() < 0.45f) {
                EEAIContinent* theContinent = gContinentManager.mContinents[theContinentID];
                if (theContinent) {
                    gContinentManager.GetAdjacentIDs(theContinentIDs, theContinent->mID);
                    for (i = theContinentIDs.size() - 1; i >= 0; i--) {
                        EEAIContinent* theOther = gContinentManager.mContinents[theContinentIDs[i]];
                        if (theOther && (theOther->mHull.BoundedContains(theFrom.mTile) ||
                                         theOther->GetClosestHullPoint(theFrom.mTile).DistanceToSquared(theFrom.mTile) <= 1225))
                            theContinents.push_back(theOther);
                    }
                    if (theContinents.size() != 0) {
                        long theIndex = gPhysics->GetRandom(0L, (long)theContinents.size() - 1);
                        theLocation.mSerial = (theContinents[theIndex])->mSerial;
                        theLocation.mType = 10;
                        theLocation.mTile = (theContinents[theIndex])->GetRandomPoint();
                        outTask.mLocation = theLocation;
                        outTask.mMode = kAGModeDefense;
                        outTask.mType = kAGTaskLocation;
                        outTask.mIsActive = true;
                    }
                }
            }
        }
    }
    if (!outTask.mIsActive && gPhysics->GetRandom() < 0.35f) {
        EEAIContinent* theContinent = gContinentManager.mContinents[theContinentID];
        if (theContinent && theContinent->mHillIDs.size() > 0) {
            theHillIDs = theContinent->mHillIDs;
            for (i = theHillIDs.size() - 1; i >= 0; i--) {
                EEAIHill* theHill = gContinentManager.mHills[theHillIDs[i]];
                if (theHill && (theHill->mHull.BoundedContains(theFrom.mTile) ||
                                theHill->GetClosestHullPoint(theFrom.mTile).DistanceToSquared(theFrom.mTile) <= 1225))
                    theHills.push_back(theHill);
            }
            if (theHills.size() != 0) {
                long theIndex = gPhysics->GetRandom(0L, (long)theHills.size() - 1);
                theLocation.mSerial = (theHills[theIndex])->mSerial;
                theLocation.mType = kAIAreaHill;
                theLocation.mTile = (theHills[theIndex])->GetRandomPoint();
                outTask.mLocation = theLocation;
                outTask.mMode = kAGModeDefense;
                outTask.mType = kAGTaskLocation;
                outTask.mIsActive = true;
            }
        }
    }
    if (!outTask.mIsActive && inSubGroup->GetGuardChokepointPercentage() > gPhysics->GetRandom() &&
        (long)gContinentManager.mChokePoints.size() > 0) {
        gContinentManager.GetChokePoints(theChokePoints, theContinentID, false);
        if (theChokePoints.size() != 0 &&
            (!inHasContinent || theChokePoints.size() > (unsigned long)this->mPlayer->mNumChokePointsToGuard || !theAnyContinent)) {
            if (inHasContinent && theFrom.IsOnMap()) {
                long theCount = 0;
                while (theChokePoints.size() != 0) {
                    theClosest = theChokePoints.back()->GetClosestPoint(theFrom.mTile);
                    long theDistance = theClosest.DistanceToSquared(theFrom.mTile);
                    theChokeMap.insert(std::make_pair(theDistance, theChokePoints.back()));
                    theChokePoints.pop_back();
                }
                std::multimap<long, EEAIChokePoint*>::iterator theNearest;
                for (theNearest = theChokeMap.begin(); theNearest != theChokeMap.end() && theCount < this->mPlayer->mNumChokePointsToGuard;
                     theNearest++, theCount++)
                    theChokePoints.push_back(theNearest->second);
            }
            long theIndex = gPhysics->GetRandom(0L, (long)theChokePoints.size() - 1);
            theLocation.mSerial = (theChokePoints[theIndex])->mSerial;
            theLocation.mType = kAIAreaChokePoint;
            theLocation.mTile = (theChokePoints[theIndex])->GetRandomPoint();
            outTask.mLocation = theLocation;
            outTask.mMode = kAGModeDefense;
            outTask.mType = kAGTaskLocation;
            outTask.mIsActive = true;
        } else if (inHasContinent) {
            gContinentManager.GetChokePoints(theChokePoints, 0, true);
            if (theChokePoints.size() != 0) {
                long theIndex = gPhysics->GetRandom(0L, (long)theChokePoints.size() - 1);
                theLocation.mSerial = (theChokePoints[theIndex])->mSerial;
                theLocation.mType = kAIAreaChokePoint;
                theLocation.mTile = (theChokePoints[theIndex])->GetRandomPoint();
                outTask.mLocation = theLocation;
                outTask.mType = kAGTaskLocation;
                outTask.mMode = kAGModeDefense;
                outTask.mIsActive = true;
            }
        }
    }
    if (!outTask.mIsActive && inSubGroup->GetGuardTownPercentage() > gPhysics->mRandom.Generate()) {
        long theTownID = this->FindThreatenedTown(this->mPlayer, &thePackage, theAnyContinent ? 0 : inSubGroup->GetMainContinent());
        if (theTownID) {
            EETown* theTown = this->mPlayer->GetTownByID(theTownID);
            if (theTown) {
                EECPMilitaryAidPackage* thePending = this->GetPackage(theTown->mTownID);
                if (thePending)
                    thePending->mSubGroupIDs.push_back(inSubGroup->mID);
                inSubGroup->mMission = 3;
                theLocation.mSerial = theTown->mSerial;
                theLocation.mType = kAIAreaTown;
                theLocation.mTile = theTown->GetRandomPoint();
                outTask.mLocation = theLocation;
                outTask.mType = kAGTaskLocation;
                outTask.mMode = kAGModeDefense;
                outTask.mIsActive = true;
            }
        }
    }
    if (!outTask.mIsActive) {
        gPhysics->mRandom.Generate();
        inSubGroup->GetGuardHighGroundPercentage();
    }
    if (!outTask.mIsActive) {
        EEAIContinent* theContinent = gContinentManager.mContinents[theContinentID];
        if (theContinent) {
            theLocation.mSerial = theContinent->mSerial;
            theLocation.mType = kAIAreaContinent;
            theLocation.mTile = theContinent->GetRandomPoint();
            outTask.mLocation = theLocation;
            outTask.mType = kAGTaskLocation;
            outTask.mIsActive = true;
        }
    }
    if (outTask.mIsActive)
        inSubGroup->mMission = 1;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt). Not emitted by this build (NEAR): the base ctor
// 0x47b7e4 (EEAIFOUnitsToKillScoringBase), which the original calls out of line where this build refuses the derived
// ctor instead (inline budget). 0x478937 (EECPUMGatherUnitsScreenBase) is emitted and exact.
// TEMPLATE: EXE 0x4783b8
// TEMPLATE: EXE 0x4783ca
// TEMPLATE: EXE 0x4785a1
// TEMPLATE: EXE 0x478937
// TEMPLATE: EXE 0x47895f
// TEMPLATE: EXE 0x4792bc
// TEMPLATE: EXE 0x4792c1
// TEMPLATE: EXE 0x4799a9
// TEMPLATE: EXE 0x4799c2
// TEMPLATE: EXE 0x479bb5
// TEMPLATE: EXE 0x47b906
// TEMPLATE: EXE 0x47bc40
// TEMPLATE: EXE 0x47c023
// TEMPLATE: EXE 0x47c276
// TEMPLATE: EXE 0x47c2a1
// TEMPLATE: EXE 0x47c2cc
// TEMPLATE: EXE 0x47c2fc
// TEMPLATE: EXE 0x47c327
// TEMPLATE: EXE 0x47c3e7
// TEMPLATE: EXE 0x47c428
// TEMPLATE: EXE 0x47c466
// TEMPLATE: EXE 0x47c548
// TEMPLATE: EXE 0x47c600
// TEMPLATE: EXE 0x47c62c
// TEMPLATE: EXE 0x47c676
// TEMPLATE: EXE 0x47c719
// TEMPLATE: EXE 0x47c73a
// TEMPLATE: EXE 0x47c7ab
// TEMPLATE: EXE 0x47c9c8
// TEMPLATE: EXE 0x47ca38
// TEMPLATE: EXE 0x47ca6a
// TEMPLATE: EXE 0x47cd18
// TEMPLATE: EXE 0x47cd4a
// TEMPLATE: EXE 0x47cd7c
// TEMPLATE: EXE 0x47cdbe
// TEMPLATE: EXE 0x47ce02
// TEMPLATE: EXE 0x47ce34
// TEMPLATE: EXE 0x47d149
// TEMPLATE: EXE 0x47d231
// TEMPLATE: EXE 0x47d258
// TEMPLATE: EXE 0x47d313
// TEMPLATE: EXE 0x47d3ce
// TEMPLATE: EXE 0x47d48e
// TEMPLATE: EXE 0x47d549
// TEMPLATE: EXE 0x47d575
// TEMPLATE: EXE 0x47d599
// TEMPLATE: EXE 0x47d5ca
// TEMPLATE: EXE 0x47d5ed
// TEMPLATE: EXE 0x47d60a
// TEMPLATE: EXE 0x47d636
// TEMPLATE: EXE 0x47d66b
// TEMPLATE: EXE 0x47d7c1
// TEMPLATE: EXE 0x47da43
// TEMPLATE: EXE 0x47db1b
// TEMPLATE: EXE 0x47db4f
// TEMPLATE: EXE 0x47ddcf
// TEMPLATE: EXE 0x47de84
// TEMPLATE: EXE 0x47de8a
// TEMPLATE: EXE 0x47dec7
// TEMPLATE: EXE 0x47df06
// TEMPLATE: EXE 0x47dffb
// TEMPLATE: EXE 0x47e025
// TEMPLATE: EXE 0x47e15e
// TEMPLATE: EXE 0x47e284
// TEMPLATE: EXE 0x47e2b7
// TEMPLATE: EXE 0x47e353
// TEMPLATE: EXE 0x47e38d
// TEMPLATE: EXE 0x47e3ce
// TEMPLATE: EXE 0x47e4ba
// TEMPLATE: EXE 0x47e557
// TEMPLATE: EXE 0x47e5dd
// TEMPLATE: EXE 0x47e688
// TEMPLATE: EXE 0x47e7b4
// TEMPLATE: EXE 0x47e842
// TEMPLATE: EXE 0x47e8b0
// TEMPLATE: EXE 0x47e8da
// TEMPLATE: EXE 0x47e9e4
// TEMPLATE: EXE 0x47ea5f
// TEMPLATE: EXE 0x47eb28
// TEMPLATE: EXE 0x47eb80
// TEMPLATE: EXE 0x47ebc4
// TEMPLATE: EXE 0x47ebd5
// TEMPLATE: EXE 0x47ec7c
// TEMPLATE: EXE 0x47ecc6


// Original-unit COMDATs (strictly verified)
// TEMPLATE: EXE 0x47d694
// TEMPLATE: EXE 0x47d767
// TEMPLATE: EXE 0x47db9c
// TEMPLATE: EXE 0x47dc2d
// TEMPLATE: EXE 0x47dc67
// TEMPLATE: EXE 0x47dca8
// TEMPLATE: EXE 0x47dda6
// TEMPLATE: EXE 0x47e4ea
// TEMPLATE: EXE 0x47e52e

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was
// compared against the original by name)
// TEMPLATE: EXE 0x47c43b
// TEMPLATE: EXE 0x47c51c
// TEMPLATE: EXE 0x47c67c
// TEMPLATE: EXE 0x47c6a8
// TEMPLATE: EXE 0x47ce78
// TEMPLATE: EXE 0x47d174
// TEMPLATE: EXE 0x47d798
// TEMPLATE: EXE 0x47d89a
// TEMPLATE: EXE 0x47d8d9
// TEMPLATE: EXE 0x47d902
// TEMPLATE: EXE 0x47d9db
// TEMPLATE: EXE 0x47da1a
// TEMPLATE: EXE 0x47e0e2
// TEMPLATE: EXE 0x47e11f
// TEMPLATE: EXE 0x47e5b4
// TEMPLATE: EXE 0x47e65f
// TEMPLATE: EXE 0x47e6d3
// TEMPLATE: EXE 0x47e6fc
// TEMPLATE: EXE 0x47e761
// TEMPLATE: EXE 0x47eab5
// TEMPLATE: EXE 0x47eaf4
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x47e602
// TEMPLATE: EXE 0x47e976
// TEMPLATE: EXE 0x47ec38
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x47c0f6
// TEMPLATE: EXE 0x47c21e
// TEMPLATE: EXE 0x47c24a

// Template COMDATs named by their mangled names
// TEMPLATE: EXE 0x47c93f
// ?erase@?$_Tree@KKU_Kfn@?$set@KU?$less@K@std@@V?$allocator@K@2@@std@@U?$less@K@3@V?$allocator@K@3@$0A@@std@@QAE?AViterator@12@V312@0@Z
// TEMPLATE: EXE 0x47d4f3
// ?GetTowns@EEPlayer@@QAEXABGAAV?$vector@PAVEETown@@V?$allocator@PAVEETown@@@std@@@std@@@Z

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret 4` body the vtable slot points at)
void EECPDefenseCommander::UnitsSwapped(std::vector<EEPUnitSwap>&)
{
}
