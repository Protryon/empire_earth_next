// C:\EEX\Empire Earth\Calamities\EECCVolcano.cpp (file name inferred: no __FILE__; the class name). The volcano
// calamity class: 0xb4634..0xb5b7d. Unit flags: the exe's module set. Reset is ICF-folded (0x157269). The std::sort
// COMDATs of ClearArea's vector<EETileCoordinate> close the unit (0xb538d..0xb5b7d).
// Data (.rdata, after the vtable 0x43cefc): 3.5f 0x43cf4c, -10000.0f 0x43cf50.
#include <windows.h>

#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EEEffect.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECCVolcano.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEEffectManager.h"

const float kTwoPi = 6.2831855f;
const float kPi = 3.1415927f;

// FUNCTION: EXE 0x4b4634
// Creates the volcano calamity class with no effects
EECCVolcano::EECCVolcano()
    : mGraphicEffect1(NULL), mGraphicEffect2(NULL), mGraphicEffect3(NULL), mGraphicEffect4(NULL)
{
}

// SYNTHETIC: EXE 0x4b4654
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b4670
// Destroys the calamity class, unloading its sound sample and releasing its effects
EECCVolcano::~EECCVolcano()
{
    gSoundManager->Unload(this->mSound->GetSampleID());
    this->mGraphicEffect1->Dereference();
    this->mGraphicEffect2->Dereference();
    this->mGraphicEffect3->Dereference();
    if (this->mGraphicEffect4 != NULL)
        this->mGraphicEffect4->Dereference();
}

// FUNCTION: EXE 0x4b46bc
// Moves movable units off the volcano's footprint: to their next waypoint outside it, else onto the first free ring
// tile (nearest the sort corner), dropping its path
void EECCVolcano::ClearArea(EEUnitClass* inClass, long inX, long inY)
{
    GE3DPoint theLocation;
    GE3DPoint theWaypoint;
    GE2DPoint theWaypoint2D;
    GERectangle theArea;
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    GETransformation theTransformation(false);
    long theSize = inClass->mTileSize;
    long theHalfSize = theSize / 2;
    long theLeft = inX - theHalfSize;
    long theTop = inY - theHalfSize;
    long theExtent = theHalfSize - (theSize % 2 == 0 ? 1 : 0);
    theArea.SetBottom(inY + theExtent);
    theArea.SetTop(theTop);
    theArea.SetLeft(theLeft);
    theArea.SetRight(inX + theExtent);
    EEGetUnitsInRectangle(theArea, theUnits);
    unsigned long theCount = theUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
        if (theUnit == NULL || !EEIsUnitMovable(NULL, theUnit))
            continue;
        bool theMoved = false;
        long theWaypointCount = theUnit->mWaypoints.size();
        if (theWaypointCount > 0) {
            float theArg;
            while (theUnit->NextWaypoint() == 0) {
                if (theUnit->GetPathStart(theWaypoint, theArg) != 0)
                    continue;
                theWaypoint2D = GE2DPoint(theWaypoint.GetX(), theWaypoint.GetY());
                if (theArea.Contains(theWaypoint2D))
                    continue;
                theTransformation = theUnit->mPhysicalModel.GetLocation();
                theTransformation.SetPosition(theWaypoint.GetX(), theWaypoint.GetY(), theWaypoint.GetZ());
                theUnit->SetTransformation(theTransformation, true);
                theMoved = true;
                break;
            }
        }
        if (theMoved)
            continue;
        EEGetSquareBlockRing(0, theLeft, theTop, theSize, theTiles, 0);
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theLeft + theSize - 1, theTop));
        unsigned long theTileCount = theTiles.size();
        for (unsigned long j = 0; j < theTileCount; j++) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[j].mX, theTiles[j].GetY());
            if (theTile == NULL || theTile->mUnits != NULL || !EECanStandOn(theTiles[j], theUnit))
                continue;
            theTransformation = theUnit->mPhysicalModel.GetLocation();
            theLocation.SetX(theTransformation.GetX());
            theLocation.SetY(theTransformation.GetY());
            theLocation.SetZ(theTransformation.GetZ());
            theLocation.SetX(theTiles[j].mX + 0.5f);
            theLocation.SetY(theTiles[j].GetY() + 0.5f);
            if (theUnit->mUnitClass->mDomains & kMovementAir)
                theLocation.SetZ(theUnit->mUnitClass->GetHeightAt(theUnit, theLocation));
            else if (::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theTiles[j].mX, theTiles[j].GetY()))
                theLocation.SetZ(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
            theTransformation.SetX(theLocation.GetX());
            theTransformation.SetY(theLocation.GetY());
            theTransformation.SetZ(theLocation.GetZ());
            theUnit->SetTransformation(theTransformation, true);
            theUnit->DropPath();
            theUnit->StartAction(kActionIdle);
            theUnit->mUnitClass->ActionChanged(theUnit);
            break;
        }
    }
}

// FUNCTION: EXE 0x4b4af3
// Starts a volcano: places the eruption effect at the target and sets the calamity height above the highest terrain
// nearby; always returns true
bool EECCVolcano::Start(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    theLocation.SetX(theLocation.GetX() - 0.5f);
    theLocation.SetY(theLocation.GetY() - 1.5f);
    gEffectManager->CreatePlacedEffect(theLocation, this->mGraphicEffect1, NULL);
    inCalamity->mHeight = this->GetMaximumHeight(theLocation) + this->mGraphicEffect1->GetSize();
    this->PlaySound(inCalamity);
    return true;
}

// FUNCTION: EXE 0x4b4b62
// Returns the highest terrain mesh height in the 7 by 7 tile block around the location
float EECCVolcano::GetMaximumHeight(const GE3DPoint& inLocation)
{
    EEMap* theMap = ::gWorld.mMap;
    float theMaximum = -3.4028235e38f;
    long theRadius = UMFloor(3.5f);
    long theEndX = UMFloor(inLocation.GetX()) + theRadius;
    long theStartY = UMFloor(inLocation.GetY()) - theRadius;
    long theEndY = UMFloor(inLocation.GetY()) + theRadius;
    for (long y = theEndY; y > theStartY; y--) {
        for (long x = UMFloor(inLocation.GetX()) - theRadius; x < theEndX; x++) {
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x, y + 1));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x + 1, y + 1));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x + 1, y));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x, y));
        }
    }
    return theMaximum;
}

// FUNCTION: EXE 0x4b4d19
// Creates the volcano unit for the calamity's player centred on the target, clears units off its footprint and makes it
// the calamity's target; returns false if the unit could not be created
bool EECCVolcano::CreateVolcano(EECalamity* inCalamity)
{
    GE3DPoint theCorner;
    GETransformation theTransformation;
    GE3DPoint theLocation;
    bool theResult = false;
    inCalamity->GetTargetLocation(theLocation);
    EEUnitClass* theClass = gUnitClassTable[this->mUnitClassIndex];
    float theHalfSize = theClass->mTileSize * 0.5f;
    GE3DPoint thePosition(theLocation.GetX() - theHalfSize, theLocation.GetY() - theHalfSize, inCalamity->mHeight - 0.9f);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    EEUnit* theUnit = EECreateCalamityUnit(inCalamity, this->mUnitClassIndex, theTransformation, inCalamity->mPlayerNumber);
    if (theUnit != NULL) {
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
        theCorner.SetX(theLocation.GetX() - 0.5f);
        theCorner.SetY(theLocation.GetY() - 0.5f);
        EECCVolcano::ClearArea(theClass, UMFloor(theCorner.GetX()), UMFloor(theCorner.GetY()));
        inCalamity->SetTargetUnit(theUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b4e2d
// Stops the volcano's sound and kills and deselects its volcano unit
void EECCVolcano::Stop(EECalamity* inCalamity)
{
    EEUnit* theVolcano = NULL;
    inCalamity->StopSound();
    inCalamity->GetTargetUnit(theVolcano);
    if (theVolcano == NULL)
        return;
    theVolcano->Kill(0, 0, 0);
    theVolcano->mPlayer->DeselectUnit(theVolcano);
    inCalamity->SetTargetUnit(NULL);
}

// FUNCTION: EXE 0x4b4e74
// Picks a random point on the map between the minimum and maximum radius around the centre, at the tile's height
void EECCVolcano::GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation)
{
    EEMap* theMap = ::gWorld.mMap;
    EEMapTile* theTile;
    do {
        float theAngle = gPhysics->mRandom.Generate() * kTwoPi - kPi;
        float theFraction = gPhysics->mRandom.Generate();
        float theCos = UMCos(theAngle);
        float theSin = UMCos(1.5707964f - theAngle);
        float theDistance = (1.0f - theFraction) * this->mMinimumRadius + theFraction * this->mMaximumRadius;
        outLocation.SetX(theCos * theDistance);
        outLocation.SetY(theSin * theDistance);
        outLocation += inCenter;
        theTile = theMap->GetTile(UMFloor(outLocation.GetX()), UMFloor(outLocation.GetY()));
    } while (theTile == NULL);
    outLocation.SetZ(theTile->mHeight);
}

// FUNCTION: EXE 0x4b4f72
// Reads the calamity data: three effects, sound, volcano unit class and radii; the lifetime is the eruption effect's
// duration plus its delay
void EECCVolcano::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
    this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
    this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
    this->mGraphicEffect1->Reference();
    this->mGraphicEffect2->Reference();
    this->mGraphicEffect3->Reference();
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mLifeTime = (float)(this->mGraphicEffect1->mDuration + this->mGraphicEffect1->mTime1);
    this->mL48 = inData->mIntParam6;
    this->mUnitClassIndex = inData->mUnitClassIndex;
    this->mL3C = inData->mIntParam2;
    this->mL40 = inData->mIntParam3;
    this->mMinimumRadius = (float)inData->mIntParam4;
    this->mMaximumRadius = (float)inData->mIntParam5;
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCVolcano::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b5023
// Loads the effects, the sound and the volcano unit class's media and buttons
void EECCVolcano::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mGraphicEffect1->Load(inIsOnDemand);
    this->mGraphicEffect2->Load(inIsOnDemand);
    this->mGraphicEffect3->Load(inIsOnDemand);
    if (this->mGraphicEffect4 != NULL)
        this->mGraphicEffect4->Load(inIsOnDemand);
    this->mSound->LoadMedia();
    if (inIsOnDemand == true)
        inPlayer->LoadUnitClassMedia(this->mUnitClassIndex);
    inPlayer->SetUnitClassMedia(this->mUnitClassIndex, 0x7f);
    inPlayer->LoadUnitClassButtons(this->mUnitClassIndex);
}

// FUNCTION: EXE 0x4b508b
// Starts the volcano's sound as the calamity's sound
void EECCVolcano::PlaySound(EECalamity* inCalamity)
{
    inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(), 0.0f,
                                                                    1.0f, this->mSound->GetLooping(), NULL));
    if (inCalamity->mSound != NULL)
        inCalamity->mSound->Dereference();
}

// FUNCTION: EXE 0x4b50ec
// Unloads the effects, the sound and the volcano unit class's media and buttons
void EECCVolcano::UnloadMedia(EEPlayer* inPlayer)
{
    this->mGraphicEffect1->Unload();
    this->mGraphicEffect2->Unload();
    this->mGraphicEffect3->Unload();
    if (this->mGraphicEffect4 != NULL)
        this->mGraphicEffect4->Unload();
    this->mSound->UnloadMedia();
    inPlayer->UnloadUnitClassMedia(this->mUnitClassIndex);
    inPlayer->UnloadUnitClassButtons(this->mUnitClassIndex);
}

// FUNCTION: EXE 0x4b5137
// Makes units step aside from the volcano's site at the tile
void EECCVolcano::Place(EEUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EETileCoordinate theTile;
    theTile.mX = inX;
    theTile.mY = inY;
    EEStepAsideFromSite(NULL, gUnitClassTable[this->mUnitClassIndex], inUnit->mPlayer, theTile, 1);
}

// FUNCTION: EXE 0x4b516c
// Ages the volcano; after the eruption delay creates the volcano unit with its two effects (returning the last effect's
// result); returns 7 at the end of its lifetime
long EECCVolcano::Update(EECalamity* inCalamity, float inElapsedTime)
{
    float theTimeAlive = inCalamity->GetTimeAlive() + inElapsedTime;
    long theResult = 0;
    this->UpdateVolcano(inCalamity, inElapsedTime);
    this->UpdateSound(inCalamity, this->mSound, -1.0f, 4.0f, true);
    inCalamity->SetTimeAlive(theTimeAlive);
    if (this->mGraphicEffect1->mTime1 < theTimeAlive) {
        EEUnit* theVolcano;
        if (!inCalamity->GetTargetUnit(theVolcano)) {
            this->CreateVolcano(inCalamity);
            inCalamity->GetTargetUnit(theVolcano);
            gEffectManager->CreateUnitEffect(theVolcano, this->mGraphicEffect3, NULL, 0, 0, 0);
            theResult = gEffectManager->CreateUnitEffect(theVolcano, this->mGraphicEffect2, NULL, 0, 0, 0);
        }
    }
    if (theTimeAlive > this->mLifeTime)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4b523e
// Accepts a site whose whole footprint is on the map, off the map edge and unblocked for the volcano unit class
bool EECCVolcano::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    std::vector<long> theUnused;
    bool theResult = true;
    long theSize = gUnitClassTable[this->mUnitClassIndex]->mTileSize;
    long theStartX = inX - theSize / 2;
    long theStartY = inY - theSize / 2;
    EEPlayer* thePlayer = inUnit != NULL ? inUnit->mPlayer : ::gWorld.mPlayers[0];
    for (long x = theStartX; x < theStartX + theSize && theResult; x++) {
        for (long y = theStartY; y < theStartY + theSize && theResult; y++) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(x, y);
            theResult = theTile != NULL;
            if (!theResult)
                continue;
            theResult = !::gWorld.GetMap()->IsEdgeTile(x, y);
            if (theResult)
                theResult = EEIsTileUnblocked(thePlayer, this->mUnitClassIndex, theTile);
        }
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x563d6f
// (empty: folded into a shared empty function)
void EECCVolcano::UpdateVolcano(EECalamity* inCalamity, float inElapsedTime)
{
}

// Compiler-generated for this unit (std::sort of ClearArea's tiles with EETileDistanceLess; Dinkum 2.33):
// TEMPLATE: EXE 0x4b538d
// TEMPLATE: EXE 0x4b5473
// TEMPLATE: EXE 0x4b54a6
// TEMPLATE: EXE 0x4b55b3
// TEMPLATE: EXE 0x4b55fd
// TEMPLATE: EXE 0x4b56cb
// TEMPLATE: EXE 0x4b570f
// TEMPLATE: EXE 0x4b575c
// TEMPLATE: EXE 0x4b58b1
// TEMPLATE: EXE 0x4b58f9
// TEMPLATE: EXE 0x4b595e
// TEMPLATE: EXE 0x4b5991
// TEMPLATE: EXE 0x4b5a3c
// TEMPLATE: EXE 0x4b5ab4
// TEMPLATE: EXE 0x4b5b29
