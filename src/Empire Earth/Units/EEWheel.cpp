// C:\EEX\Empire Earth\Units\EEWheel.cpp (file name inferred: no __FILE__; the class name, as the codebase names its files).
// A unit model's wheels and some cdecl unit helpers: 0x23c83a..0x23ceca. Unit flags: the exe's module set.
// Data: the name key "wheel" 0x564488 (a plain global: its dtor is unguarded); vtable 0x449af0; the unit's named
// floats 0x449ae8.. (.rdata).
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"
#include "lle/GELOD.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEWheel.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAIKindIDs.h"

// A cdecl helper defined in another unit (also declared in EEUnitUtilities.h)
float EEJitterHeading(float inAngle, float inMin, float inMax);


const UString kWheelName("wheel");
const float kMinimumWheelDistance = 0.1f;   // 0x449ae8
// the trade price curve (EEGetTradePrice; provisional names)
const float kTradeCurve = 0.666666687f;     // 0x449af8
const float kTradeScale = 5000.0f;          // 0x449afc
// SYNTHETIC: EXE 0x63c83a   (wrapper)
// SYNTHETIC: EXE 0x63c844   (ctor)
// SYNTHETIC: EXE 0x63c855   (registrar)
// SYNTHETIC: EXE 0x63c861   (dtor)

// FUNCTION: EXE 0x63c86c
// Creates a wheel for a model part, its radius half the part's bounding-box height, unturned
EEWheel::EEWheel(GEModel* inModel)
{
    this->mModel = inModel;
    this->mRadius = inModel->GetBoundingBox(0).GetHeight() * 0.5f;
    this->mAngle = 0.0f;
}

// SYNTHETIC: EXE 0x63c895
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x63c8b1
// Creates a copy of a wheel (model part, radius and angle)
EEWheel::EEWheel(const EEWheel& inWheel)
{
    this->mModel = inWheel.mModel;
    this->mRadius = inWheel.mRadius;
    this->mAngle = inWheel.mAngle;
}

// FUNCTION: EXE 0x63c8d2
// Destroys the wheel (nothing to release)
EEWheel::~EEWheel()
{
}

// FUNCTION: EXE 0x63c8d9
// Returns whether a model part is a wheel (its name starts with "wheel", any case)
bool EEIsWheelModel(GEModel* inModel)
{
    return inModel->GetName().EqualsIgnoreCase(kWheelName, kWheelName.GetLength());
}

// FUNCTION: EXE 0x63c8ff
// Rolls the wheel back by the unit's travel this tick (speed or turn rate, whichever is the larger share of its top)
// over its scaled radius, in physics actions 0, 4 and 5 above a tenth of the top; wraps the angle by a full turn
void EEWheel::Update(EEComplexUnit* inUnit)
{
    float theCircumference = inUnit->mPhysicalModel.GetScale() * this->mRadius;
    EEUnitStats* theStats = inUnit->mStats;
    if (!(theCircumference < -0.0001f) && !(theCircumference > 0.0001f))
        return;
    unsigned long theMode = inUnit->mCurrentPhysicsAction;
    if (theMode != 0 && theMode != 4 && theMode != 5)
        return;
    float theSpeed = inUnit->mVelocity;
    float theDistance = theStats->GetInverseMaxVelocity() * (theSpeed < 0.0f ? -theSpeed : theSpeed);
    float theYaw = inUnit->mAngularVelocity.mYaw;
    float theTurn = theStats->GetInverseMaxAngularVelocity() * (theYaw < 0.0f ? -theYaw : theYaw);
    if (theDistance > theTurn)
        theTurn = theDistance;
    if (!(theTurn >= kMinimumWheelDistance))
        return;
    this->mAngle -= gPhysics->GetTickSeconds() * theStats->GetMaxVelocity() * theTurn / theCircumference;
    // wrap by a full turn (adding pi made the wheel jump half a turn at each wrap)
    while (this->mAngle < -3.1415927f)
        this->mAngle += 2.0f * 3.1415927f;
}

// FUNCTION: EXE 0x63c9d9
// Returns the unit's attack animation set: 0xb for the special attack of Roman Legionnaires and capital ships,
// 8 when it fires high over walls, else 0
long EEGetAttackFrameSet(EEComplexUnit* inUnit)
{
    if (inUnit->mIsSpecialAttack) {
        unsigned long theType = inUnit->mUnitClass->mUnitType;
        if (theType == kUnitTypeX04RomanLegionnaire || theType == kUnitTypeSp15CapitalShip || theType == kUnitTypeCapitalShipYamato)
            return 0xb;
    }
    if (inUnit->mStats->mCanFireOverWalls && inUnit->mIsFiringHigh)
        return 8;
    return 0;
}

// FUNCTION: EXE 0x63ca26
// Replaces a target the unit can't reach: with its position when that is reachable, else with a reachable spot up to
// 9 tiles away, halved toward the target to within half a tile (the unit's position when none is found)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget)
{
    if (gContinentManager.CanGetTo(inUnit, (long)ioTarget->GetX(), (long)ioTarget->GetY()))
        return;
    float theX = inUnit->mPhysicalModel.GetLocation().GetX();
    bool theFound = false;
    float theY = inUnit->mPhysicalModel.GetLocation().GetY();
    float theTargetX = ioTarget->GetX();
    float theTargetY = ioTarget->GetY();
    if (!gContinentManager.CanGetTo(inUnit, UMFloor(theX), UMFloor(theY))) {
        for (float theRadius = 1.0f; theRadius < 10.0f && !theFound; theRadius += 1.0f) {
            for (long theDX = -1; theDX <= 1 && !theFound; theDX++) {
                for (long theDY = -1; theDY <= 1 && !theFound; theDY++) {
                    if (theDX == 0 && theDY == 0)
                        continue;
                    float theCandidateY = (float)theDY * theRadius + theY;
                    float theCandidateX = (float)theDX * theRadius + theX;
                    if (!gContinentManager.CanGetTo(inUnit, UMFloor(theCandidateX), UMFloor(theCandidateY)))
                        continue;
                    theFound = true;
                    theX = theCandidateX;
                    theY = theCandidateY;
                }
            }
        }
        if (theFound) {
            while (!(UMAbs(theTargetY - theY) < 0.5f && UMAbs(theTargetX - theX) < 0.5f)) {
                float theMidX = (theTargetX + theX) * 0.5f;
                float theMidY = (theTargetY + theY) * 0.5f;
                if (gContinentManager.CanGetTo(inUnit, (long)theMidX, (long)theMidY)) {
                    theX = theMidX;
                    theY = theMidY;
                } else {
                    theTargetX = theMidX;
                    theTargetY = theMidY;
                }
            }
        }
    }
    ioTarget->SetX(theX);
    ioTarget->SetY(theY);
}

// FUNCTION: EXE 0x63cc42
// Returns a resource's market price (1..1000) from the world table's rate and the player's trade balance in it;
// 1 lower when inBuy is set, else 1 higher
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy)
{
    float theRate = 1.0f;
    long theTrade = inPlayer->mResourceTrade[inResource - 2];
    switch (inResource) {
    case 2: theRate = gWorldTable.GetFloatValue(92); break;
    case 3: theRate = gWorldTable.GetFloatValue2(92); break;
    case 4: theRate = gWorldTable.GetFloatValue(91); break;
    case 6: theRate = gWorldTable.GetFloatValue2(91); break;
    }
    float theLimit = theTrade >= 0 ? kHeightEpsilon : 1000.0f;
    float thePrice = 100.0f / (((theRate - theLimit) * (kTradeCurve * theRate - theLimit) * kTradeScale) /
        ((kTradeCurve * theRate - theLimit) * kTradeScale + (theRate - kTradeCurve * theRate) * (float)theTrade) + theLimit);
    if (inBuy != 0)
        thePrice -= 1.0f;
    else
        thePrice += 1.0f;
    if (thePrice < 1.0f)
        thePrice = 1.0f;
    if (thePrice > 1000.0f)
        thePrice = 1000.0f;
    return UMFloatToLong(thePrice);
}

// FUNCTION: EXE 0x63cd32
// Picks a point 4..12 tiles from the unit along its goal-to-unit heading (+-20 degrees) on its land continent, up to
// 13 tries; returns whether found, else writes its position ((0, 0) when it has no goal)
// cdecl with a real GE2DPoint output: EEComplexUnit::FleeAfterCalamity and 0xbaab pass one (provisional name)
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint)
{
    EETileCoordinate theCandidate(0, 0);
    GE3DPoint theGoal;
    GE3DPoint thePosition;
    bool theFound = false;
    if (inUnit->mGoalState != NULL) {
        inUnit->mGoalState->GetGoalLocation(theGoal);
        inUnit->GetPosition(thePosition);
        float theAngle = EEJitterHeading(EEGetHeadingBetween(&theGoal, &thePosition), -20.0f, 20.0f);
        EETileCoordinate theTile(inUnit->mTileX, inUnit->mTileY);
        unsigned short theContinent = gContinentManager.GetContinent(theTile, kAIPathLandMass);
        for (long i = 12; i >= 0; i--) {
            if (theFound)
                break;
            float theRadius = gPhysics->GetRandom(4.0f, 12.0f);
            outPoint->SetX(UMCos(theAngle) * theRadius + thePosition.GetX());
            outPoint->SetY(UMCos(1.57079637f - theAngle) * theRadius + thePosition.GetY());
            theCandidate.mX = (long)outPoint->GetX();
            theCandidate.mY = (long)outPoint->GetY();
            if ((unsigned long)theCandidate.GetX() < gMapWidth && (unsigned long)theCandidate.mY < gMapHeight)
                theFound = gContinentManager.GetContinent(theCandidate, kAIPathLandMass) == theContinent;
        }
    }
    if (!theFound) {
        outPoint->SetX(thePosition.GetX());
        outPoint->SetY(thePosition.GetY());
    }
    return theFound;
}

// FUNCTION: EXE 0x63cea6
// Returns the unit type of the player's farms: 0x33a once the robotic farm upgrade is researched, else 0x2a
long EEGetFarmType(EEPlayer* inPlayer)
{
    return inPlayer->mTechnologies->Get(kTechRoboticFarmUpgrade)->mIsResearched ? 0x33a : 0x2a;
}
