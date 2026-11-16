// C:\EEX\Empire Earth\Graphics Thread\EEGraphicalGoalManager.cpp (file name inferred: no __FILE__; the class pair
// EEGraphicalGoal (vtable 0x4412a0) / EEGraphicalGoalManager (vtable 0x4412a8), held by EEEffectManager +0x24).
// 0x107616..0x10972b: the goal's members, the manager's members, then the map/vector template COMDATs of mGoals;
// EEGraphics.cpp's init chain follows at 0x10972b. Unit flags: the exe's module set.
// port (R4.4, twenty-sixth round): the markers (the original's EEGraphicalGoal) and the manager's algorithms
// (Rebuild, AddGoal, TrimGoals, Update, the destructor's walk) are ee_gfx goal_markers.rs's; this unit answers its
// questions: the selection, the goals, the map's ground and the markers' effects (a marching line, type 0, or an icon,
// types 1..6, made through gEffectManager, shown only to their player and not saved).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "rust/ee_gfx.h"
#include "ee/EEGraphicalGoalManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"

static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint), "GE3DPoint mirror");

// The world goal_markers.rs reads and the effects it makes (EeGfxGoalHost; the selection is the last player asked)
struct EEGoalOps {
    static EESelectedGroup* sGroup;
    static EeGe3DPoint R(const GE3DPoint& inPoint) { return EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ}; }
    static EEComplexUnit* Unit(uint32_t inUnit) { return sGroup->GetUnit(inUnit); }
    static uint32_t CurrentPlayer(void*) { return (uint32_t)::gWorld.mCurrentPlayer; }
    static uint32_t PlayerCount(void*) { return (uint32_t)::gWorld.mNumPlayers; }
    static EeGfxGoalPlayer Player(void*, uint32_t inPlayer)
    {
        EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
        sGroup = thePlayer->mSelectedGroup;
        return EeGfxGoalPlayer{thePlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicWW2, thePlayer->mShiftMode,
                               (uint32_t)sGroup->GetNumUnits()};
    }
    static EeGfxGoalUnit Facts(void*, uint32_t inUnit)
    {
        EEComplexUnit* theUnit = Unit(inUnit);
        EEUnitClass* theClass = theUnit->mUnitClass;
        return EeGfxGoalUnit{(uint32_t)theClass->mCategory, (uint32_t)theClass->mUnitType, (int32_t)theClass->mAirTrafficType,
                             theUnit->mTile != NULL, theUnit->mContainer != NULL, (uint32_t)theUnit->GetNumGoalQueues()};
    }
    static uint32_t GoalCount(void*, uint32_t inUnit, uint32_t inQueue)
    {
        return (uint32_t)Unit(inUnit)->mGoalQueues[inQueue].mGoals.size();
    }
    // a goal's kind, automatic waypoint, garrison container or attack target, and location
    static EeGfxGoalStep Goal(void*, uint32_t inUnit, uint32_t inQueue, uint32_t inGoal)
    {
        EEUnitGoal* theGoal = Unit(inUnit)->mGoalQueues[inQueue].mGoals[inGoal];
        EeGfxGoalStep theStep = {};
        theStep.kind = theGoal->mKind;
        if (theGoal->mKind == kGoalLocation) {
            theStep.auto_waypoint = EELocationGoalCast(theGoal)->GetIsAutoWaypoint();
            if (theStep.auto_waypoint)
                return theStep;
        }
        GE3DPoint theTo;
        theGoal->GetGoalLocation(theTo);
        theStep.to = R(theTo);
        EEUGRust* theCast = theGoal->mKind == kGoalLoadGarrison ? EELoadGarrisonGoalCast(theGoal)
                            : theGoal->mKind == kGoalAttackUnit  ? EEAttackUnitCast(theGoal) : NULL;
        if (theCast != NULL) {
            theStep.has_target = true;
            theStep.target = theGoal->mKind == kGoalLoadGarrison ? (void*)theCast->GetLoadContainer()
                                                                 : (void*)theCast->GetAttackTarget();
        }
        return theStep;
    }
    static bool GoalLocation(void*, uint32_t inUnit, uint32_t inQueue, uint32_t inGoal, EeGe3DPoint* outPoint)
    {
        EEUnitGoal* thePrevious = Unit(inUnit)->mGoalQueues[inQueue].GetGoalAt(inGoal);
        if (thePrevious == NULL)
            return false;
        GE3DPoint theFrom;
        thePrevious->GetGoalLocation(theFrom);
        *outPoint = R(theFrom);
        return true;
    }
    static EeGe3DPoint UnitLocation(void*, uint32_t inUnit, bool inOnTile)
    {
        EEComplexUnit* theUnit = Unit(inUnit);
        return R(GE3DPoint((inOnTile ? theUnit : theUnit->mContainer)->mPhysicalModel.GetLocation()));
    }
    // a placed target's outer part; a land building's: the goal's point on its centre's tile corner
    static void* Target(void*, void* inTarget, EeGe3DPoint* ioTo)
    {
        EEUnit* theTarget = static_cast<EEUnit*>(inTarget);
        if (theTarget->mTileNode != NULL) {
            EEComplexUnit* theOuter = theTarget->mComplexUnit;
            while (theOuter->mPreviousPart != NULL)
                theOuter = theOuter->mPreviousPart;
            theTarget = theOuter;
        }
        if (theTarget->mUnitClass->IsLandBuilding()) {
            if (theTarget->mTileNode != NULL) {
                ioTo->x = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX();
                ioTo->y = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY();
            }
            ioTo->x = ioTo->x + theTarget->mUnitClass->mTileSize * 0.5f - 0.25f;
            ioTo->y = ioTo->y - theTarget->mUnitClass->mTileSize * 0.5f + 0.25f;
        }
        return theTarget;
    }
    static bool Ground(void*, const EeGe3DPoint* inPoint, EeGe3DPoint* outPoint)
    {
        if (::gWorld.mMap->GetTile(UMFloor(inPoint->x), UMFloor(inPoint->y)) == NULL)
            return false;
        GE3DPoint theGround;
        ::gWorld.mMap->GetGroundPoint(GE3DPoint(inPoint->x, inPoint->y, inPoint->z), theGround);
        *outPoint = R(theGround);
        return true;
    }
    // EEGraphicalGoal's constructor: type 0 a marching line, 1 a waypoint icon, 2..6 the rally flags, at the origin
    static uint32_t Create(void*, uint32_t inType, uint32_t inPlayer)
    {
        static const long kEffects[7] = {kGraphicEffectBasicMarchingLine, kGraphicEffectWaypointIndicator,
                                         kGraphicEffectRallyFlagBuilding, kGraphicEffectRallyFlagFighter,
                                         kGraphicEffectRallyFlagBomber, kGraphicEffectRallyFlagAtomicBomber,
                                         kGraphicEffectRallyFlagParatrooper};
        GE3DPoint theFrom;
        GE3DPoint theTo;
        EEEffectHandle theHandle = kNoEffectHandle;
        if (inType == 0)
            gEffectManager->CreateEffectBetween(theFrom, theTo, EEGraphicEffect::Get(kEffects[0]), &theHandle);
        else if (inType < 7)
            gEffectManager->CreatePlacedEffect(theFrom, EEGraphicEffect::Get(kEffects[inType]), &theHandle);
        EEEffect* theEffect = gEffectManager->GetEffect(theHandle);
        if (theEffect != NULL) {
            theEffect->mIsSaved = false;
            theEffect->mIsLocalOnly = true;
            theEffect->mPlayer = (long)inPlayer;
        }
        return theHandle.mValue;
    }
    static void Destroy(void*, uint32_t inEffect) { gEffectManager->DestroyEffect(EEEffectHandle{inEffect}); }
    static void MoveLine(void*, uint32_t inEffect, const EeGe3DPoint* inFrom, const EeGe3DPoint* inTo)
    {
        EEEffect* theLine = gEffectManager->GetEffect(EEEffectHandle{inEffect});
        if (theLine == NULL)
            return;
        theLine->SetFrom(GE3DPoint(inFrom->x, inFrom->y, inFrom->z));
        theLine->SetTo(GE3DPoint(inTo->x, inTo->y, inTo->z));
        GETransformation theTransformation;
        theTransformation.mMatrix[0][3] = inFrom->x;
        theTransformation.mMatrix[1][3] = inFrom->y;
        theTransformation.mMatrix[2][3] = inFrom->z;
        theLine->mTransformation = theTransformation;
        theLine->MoveToTransformation(theTransformation);
    }
    static void MoveIcon(void*, uint32_t inEffect, const EeGe3DPoint* inTo)
    {
        EEEffect* theIcon = gEffectManager->GetEffect(EEEffectHandle{inEffect});
        if (theIcon != NULL)
            theIcon->SetLocation(GE3DPoint(inTo->x, inTo->y, inTo->z));
    }
};
EESelectedGroup* EEGoalOps::sGroup;

static const EeGfxGoalHost kGoalHost = {NULL, EEGoalOps::CurrentPlayer, EEGoalOps::PlayerCount, EEGoalOps::Player,
                                        EEGoalOps::Facts, EEGoalOps::GoalCount, EEGoalOps::Goal, EEGoalOps::GoalLocation,
                                        EEGoalOps::UnitLocation, EEGoalOps::Target, EEGoalOps::Ground, EEGoalOps::Create,
                                        EEGoalOps::Destroy, EEGoalOps::MoveLine, EEGoalOps::MoveIcon};

// Creates a goal marker manager with no markers
EEGraphicalGoalManager::EEGraphicalGoalManager()
    : mMarkers(ee_gfx_goals_new())
{
}

// Destroys the manager, queueing every marker's effect for destruction
EEGraphicalGoalManager::~EEGraphicalGoalManager()
{
    ee_gfx_goals_free(this->mMarkers, &kGoalHost);
}

// Rebuilds the current player's goal markers, then moves every changed marker's effect
void EEGraphicalGoalManager::Update()
{
    ee_gfx_goals_update(this->mMarkers, &kGoalHost);
}

// Returns the goal at an index of the queue
// EEUnitVectorItem's member, out of line here after its first user Rebuild (the header declares it out of line)
EEUnitGoal* EEUnitVectorItem::GetGoalAt(unsigned long inIndex)
{
    return this->mGoals[inIndex];
}

long EEGraphicalGoalManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return 0;
}

long EEGraphicalGoalManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return 0;
}
