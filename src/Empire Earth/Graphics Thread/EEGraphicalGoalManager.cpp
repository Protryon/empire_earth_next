// C:\EEX\Empire Earth\Graphics Thread\EEGraphicalGoalManager.cpp (file name inferred: no __FILE__; the class pair
// EEGraphicalGoal (vtable 0x4412a0) / EEGraphicalGoalManager (vtable 0x4412a8), held by EEEffectManager +0x24).
// 0x107616..0x10972b: the goal's members, the manager's members, then the map/vector template COMDATs of mGoals;
// EEGraphics.cpp's init chain follows at 0x10972b. Unit flags: the exe's module set.
// The goal's effect is a marching line (type 0) or an icon (types 1..6): the effect manager hands it out as EEEffect*,
// whose vtable ends at +0x54, so Update reaches the line's SetFrom/SetTo (+0x58/+0x5c) and the icon's SetLocation
// (+0x58) through a static_cast to the class the graphic effect creates.
#include <windows.h>
#include <map>
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
#include "ee/EEEffect3D.h"
#include "ee/EEEffectManager.h"
#include "ee/EEE3DIcon.h"
#include "ee/EEE3DMarchingLine.h"
#include "ee/EEGraphicalGoal.h"
#include "ee/EEGraphicalGoalManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGAttackUnit.h"

// the icons' spacing across the queues (0x441294) and the markers' height (0x441298) (provisional names)
const float kGoalSpacing = 0.4f;
const float kGoalHeight = 0.02f;

// FUNCTION: EXE 0x507616
// Creates a goal marker of a type with its effect (type 0 a marching line, 1 a waypoint icon, 2..6 rally
// flags), shown only to the given player and not saved
EEGraphicalGoal::EEGraphicalGoal(long inType, long inPlayer)
    : mType(inType)
{
    switch (inType) {
    case 0:
        gEffectManager->CreateEffectBetween(this->mFrom, this->mTo, EEGraphicEffect::Get(kGraphicEffectBasicMarchingLine), &this->mEffect);
        break;
    case 1:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectWaypointIndicator), &this->mEffect);
        break;
    case 2:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectRallyFlagBuilding), &this->mEffect);
        break;
    case 3:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectRallyFlagFighter), &this->mEffect);
        break;
    case 4:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectRallyFlagBomber), &this->mEffect);
        break;
    case 5:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectRallyFlagAtomicBomber), &this->mEffect);
        break;
    case 6:
        gEffectManager->CreatePlacedEffect(this->mFrom, EEGraphicEffect::Get(kGraphicEffectRallyFlagParatrooper), &this->mEffect);
        break;
    }
    EEEffect* theEffect = gEffectManager->GetEffect(this->mEffect);
    if (theEffect == NULL)
        return;
    theEffect->mIsSaved = false;
    theEffect->mIsLocalOnly = true;
    theEffect->mPlayer = inPlayer;
}

// FUNCTION: EXE 0x5076e3
// Destroys the marker object (its effect stays; DestroyEffect removes it)
EEGraphicalGoal::~EEGraphicalGoal()
{
}

// FUNCTION: EXE 0x5076ea
// Queues the marker's effect for destruction
void EEGraphicalGoal::DestroyEffect()
{
    gEffectManager->DestroyEffect(this->mEffect);
}

// FUNCTION: EXE 0x5076f9
// Moves a changed marker's effect: a line to its from/to points, an icon to its to point; a marker whose effect
// was not created (its graphic effect is above the effect detail setting) has nothing to move
void EEGraphicalGoal::Update()
{
    if (!this->mChanged)
        return;
    switch (this->mType) {
    case 0: {
        EEE3DMarchingLine* theLine = static_cast<EEE3DMarchingLine*>(gEffectManager->GetEffect(this->mEffect));
        if (theLine == NULL)
            break;
        theLine->SetFrom(this->mFrom);
        theLine->SetTo(this->mTo);
        GETransformation theTransformation;
        theTransformation.mMatrix[0][3] = this->mFrom.GetX();
        theTransformation.mMatrix[1][3] = this->mFrom.GetY();
        theTransformation.mMatrix[2][3] = this->mFrom.GetZ();
        theLine->mTransformation = theTransformation;
        theLine->MoveToTransformation(theTransformation);
        break;
    }
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6: {
        EEE3DIcon* theIcon = static_cast<EEE3DIcon*>(gEffectManager->GetEffect(this->mEffect));
        if (theIcon != NULL)
            theIcon->SetLocation(this->mTo);
        break;
    }
    }
    this->mChanged = false;
}

// FUNCTION: EXE 0x50779d
// Creates a goal marker manager with no markers
EEGraphicalGoalManager::EEGraphicalGoalManager()
{
    this->mGoals.clear();
}

// SYNTHETIC: EXE 0x5077cd
// (the scalar deleting destructor, emitted with the vtable; slot EEGraphicalGoalManager+0x0)

// FUNCTION: EXE 0x5077e9
// Destroys the manager, queueing every marker's effect for destruction
EEGraphicalGoalManager::~EEGraphicalGoalManager()
{
    std::map<unsigned long, std::map<unsigned long, std::vector<EEGraphicalGoal> > >::iterator thePlayer;
    std::map<unsigned long, std::vector<EEGraphicalGoal> >::iterator theType;
    for (thePlayer = this->mGoals.begin(); thePlayer != this->mGoals.end(); thePlayer++) {
        for (theType = thePlayer->second.begin(); theType != thePlayer->second.end(); theType++) {
            for (std::vector<EEGraphicalGoal>::iterator theGoal = theType->second.begin(); theGoal != theType->second.end(); theGoal++)
                theGoal->DestroyEffect();
        }
    }
    this->mGoals.clear();
}

// FUNCTION: EXE 0x50786c
// Appends a new marker of a type for a player when the list has no marker at the given index yet
void EEGraphicalGoalManager::AddGoal(long inPlayer, unsigned long inCount, long inType)
{
    if (inCount >= this->GetGoalCount(inPlayer, inType)) {
        std::vector<EEGraphicalGoal>& theGoals = this->mGoals[inPlayer][inType];
        theGoals.push_back(EEGraphicalGoal(inType, inPlayer));
    }
}

// FUNCTION: EXE 0x5078c9
// Places the player's goal markers for the selected units' queued goals (lines and icons per unit kind and
// queue; units only in shift mode, buildings always), then trims the leftovers and every other player's
// For each selected unit with a tile or a holder, each queued goal (but the automatic waypoints) gives a line from
// the previous goal (or the unit) to it and an icon at it, for the goal types its kind of unit shows; the count per
// type is kept in theCounts
void EEGraphicalGoalManager::Rebuild(long inPlayer)
{
    GE3DPoint theOffset;
    GE3DPoint theFromPoint;
    GE3DPoint theToPoint;
    GE3DPoint theFrom;
    GE3DPoint theTo;
    EEUnit* theTarget = NULL;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    bool theLateEpoch = thePlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicWW2;
    std::map<unsigned long, unsigned long> theCounts;
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    theCounts.clear();
    unsigned long theNumUnits = theGroup->GetNumUnits();
    for (unsigned long u = 0; u < theNumUnits; u++) {
        EEComplexUnit* theUnit = theGroup->GetUnit(u);
        bool theOther = false;
        bool theCitizen = false;
        bool theSpecial = false;
        bool theAircraft = false;
        bool theCarrier = false;
        EEUnitClass* theClass = theUnit->mUnitClass;
        if (theClass->mCategory == kUnitCategoryAirplane || theClass->mCategory == kUnitCategoryPlanetSpaceship)
            theAircraft = true;
        else if (theClass->mUnitType == kUnitTypeAirport)
            theSpecial = true;
        else if ((theClass->mCategory == kUnitCategoryNavalBuilding || theClass->mCategory == kUnitCategorySpaceBuilding) && theClass->mAirTrafficType != 0)
            theCarrier = true;
        else if (theClass->mCategory == kUnitCategoryBuilding)
            theCitizen = true;
        else
            theOther = true;
        bool theOnTile = theUnit->mTile != NULL;
        bool theHeld = theUnit->mContainer != NULL;
        if (!theSpecial && !theCitizen && !theCarrier && !thePlayer->mShiftMode)
            continue;
        if (!theOnTile && !theHeld)
            continue;
        for (unsigned long q = 0; q < theUnit->GetNumGoalQueues(); q++) {
            long thePrevious = -1;
            for (unsigned long g = 0; g < theUnit->mGoalQueues[q].mGoals.size(); g++) {
                EEUnitGoal* theGoal = theUnit->mGoalQueues[q].mGoals[g];
                if (theGoal->mKind == kGoalLocation && dynamic_cast<EEUGLocation*>(theGoal)->mIsAutoWaypoint)
                    continue;
                EEUnitGoal* thePreviousGoal = NULL;
                if (g > 0 && thePrevious != -1)
                    thePreviousGoal = theUnit->mGoalQueues[q].GetGoalAt(thePrevious);
                if (thePreviousGoal != NULL)
                    thePreviousGoal->GetGoalLocation(theFrom);
                else if (theOnTile)
                    theFrom = GE3DPoint(theUnit->mPhysicalModel.GetLocation());
                else if (theHeld)
                    theFrom = GE3DPoint(theUnit->mContainer->mPhysicalModel.GetLocation());
                theGoal->GetGoalLocation(theTo);
                if (theGoal->mKind == kGoalLoadGarrison) {
                    EEUGLoadGarrison* theGarrison = dynamic_cast<EEUGLoadGarrison*>(theGoal);
                    if (theGarrison != NULL)
                        theTarget = theGarrison->mContainer;
                } else if (theGoal->mKind == kGoalAttackUnit) {
                    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
                    if (theAttack != NULL)
                        theTarget = theAttack->mTarget;
                } else {
                    theTarget = NULL;
                }
                if (theTarget != NULL) {
                    if (theTarget->mTileNode != NULL) {
                        EEComplexUnit* theOuter = theTarget->mComplexUnit;
                        while (theOuter->mPreviousPart != NULL)
                            theOuter = theOuter->mPreviousPart;
                        theTarget = theOuter;
                    }
                    if (theTarget->mUnitClass->IsLandBuilding()) {
                        if (theTarget != NULL && theTarget->mTileNode != NULL && theTarget->mUnitClass->IsLandBuilding()) {
                            theTo.mX = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX();
                            theTo.mY = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY();
                        }
                        theTo.mX = theTo.GetX() + theTarget->mUnitClass->mTileSize * 0.5f - 0.25f;
                        theTo.mY = theTo.mY - theTarget->mUnitClass->mTileSize * 0.5f + 0.25f;
                    }
                }
                for (unsigned long t = 0; t < 7; t++) {
                    theOffset = GE3DPoint(0.0f, 0.0f, kGoalHeight);
                    switch (t) {
                    case 0:
                        if (!(theOther && q == 0)
                            && !(theSpecial && q != 0 && ((q != 3 && q != 4) || theLateEpoch))
                            && !(theCitizen && q == 1 && this->GetGoalCount(inPlayer, 2) > q)
                            && !theAircraft && !(theCarrier && thePlayer->mShiftMode))
                            continue;
                        if (theSpecial)
                            theOffset.mX = (float)(long)(q - 2) * kGoalSpacing;
                        break;
                    case 1:
                        if (!(theOther && q == 0) && !(theCarrier && thePlayer->mShiftMode && q == 0) && !theAircraft)
                            continue;
                        break;
                    case 2:
                        if (!theCitizen || q != 1)
                            continue;
                        break;
                    case 3:
                        if (!theSpecial && !theAircraft && !theCarrier)
                            continue;
                        if (q != 1)
                            continue;
                        if (theSpecial)
                            theOffset.mX = kGoalSpacing * -1.0f;
                        break;
                    case 4:
                        if (!theSpecial || q != 2)
                            continue;
                        theOffset.mX = 0.0f;
                        break;
                    case 5:
                        if (!theSpecial || !theLateEpoch || q != 3)
                            continue;
                        theOffset.mX = kGoalSpacing;
                        break;
                    case 6:
                        if (!theSpecial || !theLateEpoch || q != 4)
                            continue;
                        theOffset.mX = kGoalSpacing + kGoalSpacing;
                        break;
                    default:
                        continue;
                    }
                    this->AddGoal(inPlayer, theCounts[t], t);
                    if (::gWorld.mMap->GetTile(UMFloor(theFrom.GetX()), UMFloor(theFrom.mY)) != NULL && g > 0)
                        ::gWorld.mMap->GetGroundPoint(theFrom, theFromPoint);
                    else
                        theFromPoint = theFrom;
                    if (g > 0)
                        theFromPoint += theOffset;
                    if (theFromPoint.mZ < 0.0f)
                        theFromPoint.mZ = 0.0f;
                    if (::gWorld.mMap->GetTile(UMFloor(theTo.GetX()), UMFloor(theTo.mY)) != NULL)
                        ::gWorld.mMap->GetGroundPoint(theTo, theToPoint);
                    else
                        theToPoint = theTo;
                    theToPoint += theOffset;
                    if (theToPoint.mZ < 0.0f)
                        theToPoint.mZ = 0.0f;
                    EEGraphicalGoal& theLine = this->mGoals[inPlayer][t][theCounts[t]];
                    theLine.mFrom = theFromPoint;
                    theLine.mChanged = true;
                    EEGraphicalGoal& theIcon = this->mGoals[inPlayer][t][theCounts[t]];
                    theIcon.mTo = theToPoint;
                    theIcon.mChanged = true;
                    theCounts[t]++;
                }
                thePrevious = g;
            }
        }
    }
    for (unsigned long p = 0; p < ::gWorld.mNumPlayers; p++) {
        for (unsigned long t = 0; t < 7; t++) {
            if (p != (unsigned long)inPlayer)
                this->TrimGoals(p, 0, t);
            else
                this->TrimGoals(inPlayer, theCounts[t], t);
        }
    }
}

// FUNCTION: EXE 0x50815e
// Returns the goal at an index of the queue
// EEUnitVectorItem's member, out of line here after its first user Rebuild (the header declares it out of line)
EEUnitGoal* EEUnitVectorItem::GetGoalAt(unsigned long inIndex)
{
    return this->mGoals[inIndex];
}

// FUNCTION: EXE 0x508185
// Destroys a player's markers of a type from the front of the list until at most the given count remain
void EEGraphicalGoalManager::TrimGoals(long inPlayer, unsigned long inCount, long inType)
{
    std::vector<EEGraphicalGoal>::iterator theGoal = this->mGoals[inPlayer][inType].begin();
    while (this->GetGoalCount(inPlayer, inType) > inCount) {
        theGoal->DestroyEffect();
        theGoal = this->GetGoals(inPlayer, inType).erase(theGoal);
    }
}

// FUNCTION: EXE 0x5081f1
// Rebuilds the current player's goal markers, then moves every changed marker's effect
void EEGraphicalGoalManager::Update()
{
    std::map<unsigned long, std::map<unsigned long, std::vector<EEGraphicalGoal> > >::iterator thePlayer;
    std::map<unsigned long, std::vector<EEGraphicalGoal> >::iterator theType;
    this->Rebuild(::gWorld.mCurrentPlayer);
    for (thePlayer = this->mGoals.begin(); thePlayer != this->mGoals.end(); thePlayer++) {
        for (theType = thePlayer->second.begin(); theType != thePlayer->second.end(); theType++) {
            for (std::vector<EEGraphicalGoal>::iterator theGoal = theType->second.begin(); theGoal != theType->second.end(); theGoal++)
                theGoal->Update();
        }
    }
}

long EEGraphicalGoalManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return 0;
}

long EEGraphicalGoalManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return 0;
}

// FUNCTION: EXE 0x508af0
// Returns a player's marker list of a type, creating it empty when missing
std::vector<EEGraphicalGoal>& EEGraphicalGoalManager::GetGoals(unsigned long inPlayer, unsigned long inType)
{
    return this->mGoals[inPlayer][inType];
}

// FUNCTION: EXE 0x508b0c
// Returns the number of a player's markers of a type (creating the empty list when missing)
unsigned long EEGraphicalGoalManager::GetGoalCount(long inPlayer, long inType)
{
    return this->mGoals[inPlayer][inType].size();
}

// TEMPLATE: EXE 0x508268
// (the player map's _Tree destructor, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508293
// (the player map's clear, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5082a7
// (vector<EEGraphicalGoal>::push_back, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5082d3
// (vector<EEGraphicalGoal>::erase(iterator), a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508347
// (the player map's erase, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5083b8
// (vector<EEGraphicalGoal>::insert(iterator, const T&), a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508412
// (the player map's erase, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5086c8
// (the player map's _Erase, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508702
// (vector<EEGraphicalGoal>::insert(iterator, n, const T&), a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5089dc
// (named set<UString>::const_iterator::_Inc, an identical-code twin, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508a14
// (vector<EEGraphicalGoal>::_Ufill, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508a3f
// (std::copy of goal markers, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508a68
// (std::fill of goal markers, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508a85
// (std::copy_backward of goal markers, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508b4c
// (the player map's operator[], a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508bba
// (the type map's operator[], a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508cc6
// (the type map's _Tree destructor, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508f3b
// (the type map's erase, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x509140
// (vector<EEGraphicalGoal>'s copy constructor, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x50918a
// (named vector<EECPUnitRequirement>::_Buy, an identical-code twin, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5091c4
// (the type map's erase, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x50947a
// (the type map's _Copy, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5094da
// (the type map's _Erase, a template COMDAT emitted by this unit)



// TEMPLATE: EXE 0x509596
// (the type map's _Copy, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x509619
// (vector<EEGraphicalGoal>::_Ucopy, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5096a9
// (the type map pair's constructor, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5096c6
// (std::_Construct of a goal marker, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508c10
// (the player map's insert, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508cf1
// (the type map's insert, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508da7
// (the player map's _Insert, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x508fac
// (the type map's _Insert, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x509645
// (std::_Construct of a player map pair, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x509657
// (std::_Construct of a type map pair, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x509674
// (the player map pair's constructor, a template COMDAT emitted by this unit)

// EEGraphicalGoal's implicit operator= and copy ctor, its ??_G, and two map _Dec COMDATs
// TEMPLATE: EXE 0x508ab1
// TEMPLATE: EXE 0x509514
// TEMPLATE: EXE 0x509555
// TEMPLATE: EXE 0x5096d8
// SYNTHETIC: EXE 0x50970f
