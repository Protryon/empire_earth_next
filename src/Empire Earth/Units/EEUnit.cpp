// C:\EEX\Empire Earth\Units\EEUnit.cpp (file name inferred: no __FILE__; the keys "EEUnit-HitPoints"/"EEUnit-Data").
// The unit base: 0x228c4c..0x22a509. Unit flags: the exe's module set. Data: the keys 0x963638/0x963620 (one guard
// byte 0x963630), the vtable 0x447a20.

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

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEEffectManager.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "ee/EECameraControl.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EESoundObject.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include <math.h>
#include <algorithm>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIForest.h"
#include "ee/EEEffectKindIDs.h"

const UString EEUnit::sHitPoints("EEUnit-HitPoints");
// SYNTHETIC: EXE 0x628c4c   (wrapper)
// SYNTHETIC: EXE 0x628c56   (ctor)
// SYNTHETIC: EXE 0x628c67   (registrar)
// SYNTHETIC: EXE 0x628c73   (dtor)

const UString EEUnit::sData("EEUnit-Data");
// SYNTHETIC: EXE 0x628c8f   (wrapper)
// SYNTHETIC: EXE 0x628c99   (ctor)
// SYNTHETIC: EXE 0x628caa   (registrar)
// SYNTHETIC: EXE 0x628cb6   (dtor)

// FUNCTION: EXE 0x628cd2
// Creates an empty unit with no class, player, resources or hit points and no references
EEUnit::EEUnit()
{
    this->Reset();
    this->ResetEEUnit();
    this->mReferenceCount = 0;
}

// FUNCTION: EXE 0x628cf6
// Removes the unit from the game, unloading its class media at once
void EEUnit::Invalidate()
{
    EEUnit::Remove(false);
}

// FUNCTION: EXE 0x628cfe
// Returns the unit's 3D model (from its player's stats for the class)
EEModel* EEUnit::GetModel()
{
    return this->mStats->mModel;
}

// FUNCTION: EXE 0x628d05
// Resets the unit to an empty, invalid unit
void EEUnit::Clear()
{
    this->ResetEEUnit();
    this->Reset();
    this->mIsValid = false;
}

// SYNTHETIC: EXE 0x628d1a
// (the scalar deleting destructor, emitted with the vtable; slot EEUnit+0x0)

// FUNCTION: EXE 0x628d36
// Destroys the unit, freeing its tile node
EEUnit::~EEUnit()
{
    this->FreeNode();
}

// FUNCTION: EXE 0x628d4d
// Adds inValue (negative: takes it) to the resource amount the unit holds
void EEUnit::AddResourceAmount(long inValue)
{
    this->mResourceAmount += inValue;
}

// FUNCTION: EXE 0x628d57
// Returns whether the unit may gather from inUnit (visible tile, resource left unless inIgnoreAmount): a land citizen
// takes its own fully repaired farm or a dead or enemy non-citizen off deep water; a naval citizen enemy naval units
// Reads through the inline getters: the original refuses RTSMap::GetTile under EEMap::GetTile (inline budget: the
// getter sites after it leave the wrapper too small a share).
bool EEUnit::CanGather(EEUnit* inUnit, long inX, long inY, bool inIsVisible, bool inIgnoreAmount)
{
    bool theVisible = inIsVisible;
    if (!theVisible && gWorld.mMap != NULL) {
        EEMapTile* theTile = gWorld.mMap->GetTile(inX, inY);
        if (theTile != NULL)
            theVisible = theTile->IsVisibleTo(this->GetPlayer());
    }
    bool theAlive = this->GetStats()->mCarryAmount > 0;
    if (!theAlive)
        return theAlive;
    if (!theVisible)
        return false;
    if (!inIgnoreAmount && inUnit->mResourceAmount <= 0)
        return false;
    if (inUnit->mResourceType <= kResourceAny || inUnit->mResourceType >= 7)
        return false;
    long theCategory = this->GetUnitClass()->GetCategory();
    if (theCategory == kUnitCategoryLandCitizen) {
        if (inUnit->GetUnitClass()->GetUnitType() == kUnitTypeFarm) {
            if (inUnit->GetPlayer() == this->GetPlayer() && inUnit->GetComplexUnit()->mIsBuilt
                && inUnit->GetHitPoints() == inUnit->GetStats()->mMaxHitPoints)
                return true;
            return false;
        }
        if (inUnit->mTile->mTerrainType == kTerrainDeepWater)
            return false;
        long theOtherCategory = inUnit->GetUnitClass()->GetCategory();
        if (theOtherCategory == kUnitCategoryLandCitizen || theOtherCategory == kUnitCategoryNavalCitizen)
            return false;
        if (inUnit->GetPlayer() != this->GetPlayer() && this->GetPlayer()->GetEnemy(inUnit->GetPlayer()->mPlayerID) != 0
            || inUnit->GetHitPoints() == 0)
            return true;
        return false;
    }
    if (theCategory == kUnitCategoryNavalCitizen && inUnit->GetUnitClass()->GetCategory() == kUnitCategoryNaval && inUnit->GetPlayer() != this->GetPlayer()
        && this->GetPlayer()->GetEnemy(inUnit->GetPlayer()->mPlayerID) != 0)
        return true;
    return false;
}

// FUNCTION: EXE 0x628e7f
// Removes the unit from the game: unloads its class media (queued to the preloader if asked), zeroes its hit points
// and deactivates it
void EEUnit::Remove(bool inUnloadInBackground)
{
    this->mUnitClass->UnitRemoved();
    if (inUnloadInBackground && UShutdownInProgress() != true && gWorld.mClearing != true)
        gDataPreLoading->PostUnloadUnitClass(this->mPlayer->mPlayerID, this->mUnitClass->mUnitType);
    else
        this->mPlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
    this->mHitPoints = 0;
    if (this->mComplexUnit != NULL)
        gWorld.DeactivateUnit(this->mComplexUnit);
    RTSUnit::Invalidate();
}

// FUNCTION: EXE 0x628ef5
// Copies another unit's resource type and amount and its health fraction (scaled to this unit's maximum hit points)
void EEUnit::CopyFrom(RTSUnit* inUnit)
{
    EEUnit* theOther = dynamic_cast<EEUnit*>(inUnit);
    this->mResourceType = theOther->mResourceType;
    EEUnitClassUpgrade* theOtherStats = theOther->GetPlayer()->mUnitClassUpgrades[theOther->GetUnitClass()->mUnitType];
    EEUnitClassUpgrade* theStats = this->GetPlayer()->mUnitClassUpgrades[this->GetUnitClass()->mUnitType];
    this->mHitPoints = UMFloatToLong(theStats->mMaxHitPoints * ((float)theOther->GetHitPoints() / theOtherStats->mMaxHitPoints));
    this->mResourceAmount = theOther->mResourceAmount;
    RTSUnit::CopyFrom(inUnit);
}

// FUNCTION: EXE 0x628f90
// Rechecks, staggered by game-time phase, whether a unit or moving object stands on or beside this obstacle and sets
// its hidden flag (cleared in cinematics); returns whether the obstacle is hidden
bool EEUnit::UpdateHidden()
{
    if (!gWorld.mInCinematic) {
        if ((this->mHiddenFlags & kHiddenFlagHidden) == kHiddenFlagHidden && (signed char)(this->mHiddenFlags & kHiddenFlagPhaseMask) <= (signed char)((gPhysics->mGameTime / 1000) % 3)) {
            this->mHiddenFlags &= ~(kHiddenFlagHidden | kHiddenFlagPhaseMask);
            EEMap* theMap = gWorld.mMap;
            EETile* theCurrentTile = theMap->GetTile(this->mTileX, this->mTileY);
            unsigned char thePhase = ((gPhysics->mGameTime + 2) / 1000) % 3;
            std::vector<EETileCoordinate> theTiles;
            long theSize = this->mUnitClass->mTileSize;
            if (theSize > 1) {
                theTiles.reserve(theSize * theSize + 2);
                EETileCoordinate theTile;
                theTile.mX = this->mTileX;
                theTile.mY = this->mTileY;
                theTiles.push_back(theTile);
                for (long y = this->mTileY + theSize; y >= this->mTileY; y--) {
                    theTile.mX = this->mTileX - 1;
                    theTile.mY = y;
                    theTiles.push_back(theTile);
                }
                for (long x = this->mTileX - 1 + theSize; x >= this->mTileX; x--) {
                    theTile.mY = this->mTileY + theSize;
                    theTile.mX = x;
                    theTiles.push_back(theTile);
                }
            } else {
                theTiles.resize(4);
                theTiles[0] = EETileCoordinate(this->mTileX, this->mTileY);
                theTiles[1] = EETileCoordinate(this->mTileX, this->mTileY + 1);
                theTiles[2] = EETileCoordinate(this->mTileX - 1, this->mTileY + 1);
                theTiles[3] = EETileCoordinate(this->mTileX - 1, this->mTileY);
            }
            for (long i = theTiles.size() - 1; i >= 0; i--) {
                EETileCoordinate& theCoord = theTiles[i];
                EETile* theTile = theMap->GetTile(theCoord.mX, theCoord.mY);
                if (theTile == NULL)
                    continue;
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                    EEComplexUnit* theUnit = theNode->mUnit->GetComplexUnit();
                    if (theUnit != NULL) {
                        bool theResource = theUnit->mResourceType == kResourceFood && theUnit->mResourceAmount > 0 && theUnit->GetComplexUnit() != NULL &&
                            theUnit->GetUnitClass()->mCategory != kUnitCategoryLandCitizen && theUnit->GetUnitClass()->mCategory != kUnitCategoryNavalCitizen &&
                            theUnit->GetPlayer()->GetPlayerID() == 0;
                        if (theUnit->mCanBeHidden && (theUnit->GetHitPoints() > 0 || theResource)) {
                            this->mHiddenFlags |= kHiddenFlagHidden;
                            this->mHiddenFlags ^= (this->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;  // the phase into the low 2 bits (orig: xor; and 3; xor)
                            i = -1;
                            break;
                        }
                    } else {
                        EEUnitClass* theClass = theNode->mUnit->GetUnitClass();
                        if (!theClass->mIsObstacle && !theClass->mIsProjectile && theClass->mMaxVelocity > 0.0f) {
                            this->mHiddenFlags |= kHiddenFlagHidden;
                            this->mHiddenFlags ^= (this->mHiddenFlags ^ thePhase) & kHiddenFlagPhaseMask;  // the phase into the low 2 bits (orig: xor; and 3; xor)
                            i = -1;
                            break;
                        }
                    }
                }
            }
        }
    } else {
        this->mHiddenFlags &= ~kHiddenFlagHidden;
    }
    return (this->mHiddenFlags & kHiddenFlagHidden) == kHiddenFlagHidden && !this->IsFoundation();
}

// FUNCTION: EXE 0x629278
// Creates a simple unit of inClass for inPlayer at tile (inX, inY)
EEUnit* EEUnit::Create(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer)
{
    EEUnit* theUnit = new EEUnit;
    theUnit->Init(inClass, inX, inY, inPlayer);
    return theUnit;
}

// FUNCTION: EXE 0x6292ad
// Creates an empty simple unit
EEUnit* EEUnit::Create()
{
    return new EEUnit;
}

// FUNCTION: EXE 0x6292c4
// Frees the unit's tile node (the RTSUnit part)
void EEUnit::FreeNode()
{
    RTSUnit::FreeNode();
}

// FUNCTION: EXE 0x6292c9
// Returns whether inUnit may pass the unit: non-blocking units, dead non-resources, and wood for forest-crossing units
bool EEUnit::IsPassableBy(EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = this->mUnitClass;
    return !theClass->mBlocksTile || (this->mHitPoints == 0 && theClass->mCategory != kUnitCategoryResource)
        || (theClass->mResourceType == kResourceWood && inUnit != NULL && ((inUnit->mStats->mTerrainMask >> 14) & 1));
}

// FUNCTION: EXE 0x62930f
// Returns 0 (a simple unit adds nothing to the sync checksum)
long EEUnit::GetChecksum(unsigned long)
{
    return 0;
}

// FUNCTION: EXE 0x629314
// Returns the squared tile distance to the nearest edge tile of inUnit's footprint and that tile's coordinates
// NEAR: register choice only (edi/ebx and where x is spilled)
float EEUnit::GetNearestFootprintTile(EEUnit* inUnit, long* outX, long* outY)
{
    float theBest = this->GetTileDistanceSquared(inUnit);
    long theSize = inUnit->mUnitClass->mTileSize;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    *outX = theX;
    *outY = theY;
    if (theSize <= 1)
        return theBest;
    for (long x = theX; x < theX + theSize; x++) {
        for (long y = theY; y < theY + theSize; y++) {
            float theDistance = (float)((this->mTileX - x) * (this->mTileX - x) + (this->mTileY - y) * (this->mTileY - y));
            if (theDistance < theBest) {
                theBest = theDistance;
                *outX = x;
                *outY = y;
            }
            if (x > theX && x < theX + theSize - 1)
                y += theSize - 2;
        }
    }
    return theBest;
}

// FUNCTION: EXE 0x6293d4
// Appends the unit's one-line debug text: base text, resource type and amount, class type, hit points
void EEUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    RTSUnit::Describe(theString);
    ioString += theString;
    sprintf(theString, "res %d / %d:", this->mResourceType, this->mResourceAmount);
    ioString += theString;
    sprintf(theString, "obj %d:", this->mUnitClass->mUnitType);
    ioString += theString;
    sprintf(theString, "hp %d:", this->mHitPoints);
    ioString += theString;
}

// FUNCTION: EXE 0x62946f
// Appends the unit's long debug text: base text, class (simple units), resource and health
void EEUnit::DescribeLong(UWideString& ioString)
{
    UWideString theString;
    if (this->mComplexUnit == NULL) {
        sprintf(theString, "\n======] SIMPLE UNIT [======\n");
        ioString += theString;
    }
    RTSUnit::DescribeLong(theString);
    ioString += theString;
    if (this->mComplexUnit == NULL) {
        sprintf(theString, "Object Type and Name: %d, %s\n", this->mUnitClass->mUnitType, (const char*)this->mUnitClass->mName);
        ioString += theString;
    }
    if (this->mResourceType != kResourceNone) {
        sprintf(theString, "Resource Type/Amount:%d / %d\n", this->mResourceType, this->mResourceAmount);
        ioString += theString;
    }
    sprintf(theString, "Health: %d / %d (%.2f%%)\n", this->mHitPoints, this->mStats->mMaxHitPoints,
            (float)this->mHitPoints / (float)this->mStats->mMaxHitPoints);
    ioString += theString;
}

// FUNCTION: EXE 0x629550
// Returns the unit's tile, or for a multi-tile unit off a continent of kind inFlag, the first tile within inRange
// of its footprint that lies on such a continent
EETileCoordinate EEUnit::FindAreaTile(bool inFlag, long inRange)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(this);
    EETileCoordinate theTile;
    theTile.mX = this->mTileX;
    theTile.mY = this->mTileY;
    long theSize = this->mUnitClass->mTileSize;
    if (theSize > 1 && (theContinent == NULL || theContinent->mCategory != inFlag)) {
        long theEndY = this->mTileY + theSize + inRange;
        long theEndX = this->mTileX + theSize + inRange;
        for (long theX = this->mTileX - inRange; theX < theEndX; theX++) {
            for (long theY = this->mTileY - inRange; theY < theEndY; theY++) {
                if (!((unsigned long)theX < gMapWidth) || !((unsigned long)theY < gMapHeight))
                    continue;
                unsigned short theID = gContinentManager.GetContinentID(theX, theY);
                EEAIContinent* theOther = theID ? gContinentManager.GetContinentByID(theID) : NULL;
                if (theOther == NULL || theOther->mCategory != inFlag)
                    continue;
                theTile.mX = theX;
                theTile.mY = theY;
                theX = theEndX;
                break;
            }
        }
    }
    return theTile;
}

// FUNCTION: EXE 0x629635
// Returns the centre of the unit's tile at ground height
void EEUnit::GetPosition(GE3DPoint& outPosition)
{
    outPosition.SetX(this->mTileX + 0.5f);
    outPosition.SetY(this->mTileY + 0.5f);
    outPosition.SetZ(this->mTile->mHeight);
}

// FUNCTION: EXE 0x62965c
// Initialises the unit as a simple unit of inClass for inPlayer and places it at tile (inX, inY)
void EEUnit::Init(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer)
{
    this->Initialize(0);
    this->ResetEEUnit();
    if (this->mReferenceCount == 0)
        this->mReferenceCount = 1;
    this->mPlayer = inPlayer;
    this->mUnitClass = inClass;
    if (inClass != NULL)
        this->mStats = inPlayer->mUnitClassUpgrades[inClass->mUnitType];
    this->MoveTo(inX, inY, 0);
}

// FUNCTION: EXE 0x6296b3
// Initialises the unit part of a complex unit of inClass for inPlayer, with no resources or hit points yet
void EEUnit::Init(EEUnitClass* inClass, EEPlayer* inPlayer)
{
    this->Initialize(1);
    this->ResetEEUnit();
    if (this->mReferenceCount == 0)
        this->mReferenceCount = 1;
    this->mPlayer = inPlayer;
    this->mUnitClass = inClass;
    this->mComplexUnit = static_cast<EEComplexUnit*>(this);
    if (inClass != NULL)
        this->mStats = inPlayer->mUnitClassUpgrades[inClass->mUnitType];
    this->mResourceAmount = 0;
    this->mResourceType = kResourceNone;
    this->mHitPoints = 0;
}

// FUNCTION: EXE 0x629706
// Clears the unit's class, stats, resources, hit points and hidden flags
void EEUnit::ResetEEUnit()
{
    this->mUnitClass = NULL;
    this->mComplexUnit = NULL;
    this->mResourceAmount = 0;
    this->mResourceType = kResourceNone;
    this->mHitPoints = 0;
    this->mHiddenFlags = 0;
    this->mStats = NULL;
}

// FUNCTION: EXE 0x62971e
// Loads the unit class's media for the owner: at once, or for a foundation queued to the preloader
void EEUnit::LoadMedia(bool inIsFoundation)
{
    if (!inIsFoundation)
        this->mPlayer->SetUnitClassMedia(this->mUnitClass->mUnitType, 0x7f);
    else
        gDataPreLoading->PostLoadUnitClass(this->mPlayer->mPlayerID, this->mUnitClass->mUnitType);
}

// FUNCTION: EXE 0x62975a
// Kills the unit: updates sight and doppelgangers, plays a simple unit's death effect and sound, empties farms and
// citizens, takes it off the minimap and camera and reports a simple unit lost
void EEUnit::Kill(bool inKilled, bool inKeepResearch, bool inOther)
{
    float theVolume;
    float thePan;
    float thePitch;
    RTSTile* theTile = this->mTile;
    if (this->mHitPoints == 0)
        return;
    if (this->mTileX == -10000)
        this->mPlayer->RemoveOffMapUnit(this);
    this->SightOff(this->mTileX, this->mTileY);
    if (this->IsFoundation()) {
        if (gDoppelgangerManager.GetDoppelganger(static_cast<EETile*>(theTile), this, this->mPlayer->mPlayerID) != NULL)
            gDoppelgangerManager.TileShown(static_cast<EETile*>(theTile), this->mTileX, this->mTileY, this->mPlayer);
    }
    this->mHitPoints = 0;
    if (this->mComplexUnit == NULL && inKilled) {
        if (this->mUnitClass->mDeathEffect != NULL)
            gEffectManager->CreateUnitEffect(this, this->mUnitClass->mDeathEffect, NULL, 0, 0, 0);
        EESoundObject* theSound = this->mStats->mDeathSound;
        if (theSound != NULL) {
            this->mUnitClass->GetSoundParameters(this, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
    }
    EEUnitClass* theClass = this->mUnitClass;
    if (theClass->mUnitType == kUnitTypeFarm || theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen)
        this->mResourceAmount = 0;
    this->SightOn(this->mTileX, this->mTileY);
    if (!EEIsCloakedEnemyOf(this, gWorld.GetCurrentPlayer()))
        gMiniMap->MoveUnit(this, this->mTileX, this->mTileY, -10000, -10000);
    if (gWorld.mCameraControl.mUnitA == this)
        gWorld.mCameraControl.ClearUnitA();
    if (gWorld.mCameraControl.mUnitB == this)
        gWorld.mCameraControl.ClearUnitB();
    if (this->mComplexUnit == NULL)
        this->mPlayer->UnitTaken(this, inKeepResearch | inOther);
}

// FUNCTION: EXE 0x6298e1
// Frees the unit's tile node and deletes the unit once its last reference goes
void EEUnit::LastDereference()
{
    this->FreeNode();
    this->mReferenceCount = 0;
    delete this;
}

// FUNCTION: EXE 0x6298fb
// Reads the unit from a saved game (resource type, health fraction held in mHitPoints until FinishLoad, amount);
// returns the first read error
long EEUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = RTSUnit::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        theResult = inFile.Read(this->mResourceType);
        if (theResult == 0) {
            float theFraction;
            long theUnused;
            theResult = inFile.Read(theFraction);
            memcpy(&this->mHitPoints, &theFraction, 4);
            if (theResult == 0) {
                theResult = inFile.Read(this->mResourceAmount);
                if (theResult == 0)
                    theResult = inFile.Read(theUnused);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x629973
// Turns the loaded health fraction into hit points (at least 1 if the fraction was not 0); returns 0
long EEUnit::FinishLoad(long, long)
{
    if (this->mUnitClass != NULL) {
        float theFraction;
        memcpy(&theFraction, &this->mHitPoints, 4);
        this->mHitPoints = UMFloatToLong(this->mStats->mMaxHitPoints * theFraction + 0.00001f);
        if (theFraction != 0.0f && this->mHitPoints == 0)
            this->mHitPoints = 1;
    }
    return 0;
}

// FUNCTION: EXE 0x6299d5
// Repairs the unit's pointers after loading (the RTSUnit part only)
long EEUnit::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return RTSUnit::RepairPointers(inMajorVersion, inMinorVersion);
}

// FUNCTION: EXE 0x6299e5
// Writes the unit to the saved game (base unit part, resource type, health fraction, amount, a zero); returns the
// first write error
long EEUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theZero = 0;
    // the base unit part's result counts too (it used to be ignored)
    long theResult = RTSUnit::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mResourceType);
    if (theResult != 0)
        return theResult;
    float theFraction = (float)this->mHitPoints / this->mStats->mMaxHitPoints;
    theResult = outFile.Write(theFraction);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mResourceAmount);
    if (theResult == 0)
        theResult = outFile.Write(theZero);
    return theResult;
}

// FUNCTION: EXE 0x629a56
// Sets the unit's hit points, activates a dead unit of an inactive class and refreshes the icon bar if it is selected
void EEUnit::SetHitPoints(long inHitPoints)
{
    if (this->mHitPoints != inHitPoints)
        this->mHitPoints = inHitPoints;
    if (this->mUnitClass->mIsInactive && this->mHitPoints <= 0 && this->mComplexUnit != NULL)
        gWorld.ActivateUnit(this->mComplexUnit);
    if (this->mTileNode != NULL && this->mPlayer == gWorld.GetCurrentPlayer() && this->mPlayer->mSelectedGroup->Contains(this->GetComplexUnit()))
        gUserInterface->PostMessage424Once();
}

// FUNCTION: EXE 0x629ac3
// Moves the unit to tile (inX, inY); with inUpdateTiles also updates the off-map list, minimap, sight, tile, region
// and influence tables, activation and the world's sort order
// NEAR: the original calls the first GetTile out of line (0x2f85) and expands the second (inline budget)
// and divides into reused slots; the four (EEComplexUnit*)this casts follow the EEUnitClass slot types (likely EEUnit*)
void EEUnit::MoveTo(long inX, long inY, bool inUpdateTiles)
{
    long theOldX = this->mTileX;
    long theOldY = this->mTileY;
    RTSRegionTable<EEComplexUnit*>* theRegions = gWorld.mMap ? gWorld.mMap->mUnitRegions : 0;
    RTSTile* theOldTile = this->mTile;
    bool theSameRegion = false;
    if (theOldX == inX && theOldY == inY)
        return;
    if (inUpdateTiles == true) {
        if (inX == -10000)
            this->mPlayer->AddOffMapUnit(this);
        else if (theOldX == -10000)
            this->mPlayer->RemoveOffMapUnit(this);
        if (this->mComplexUnit != NULL) {
            this->mComplexUnit->mF24C = this->mComplexUnit->mCollisionResolutionCount;
            this->mComplexUnit->mCollisionResolutionCount = 0.0f;
        }
    }
    if (inUpdateTiles) {
        if (this->mHitPoints != 0 && !EEIsCloakedEnemyOf(this, gWorld.GetCurrentPlayer())
            && (this->mPlayer == gWorld.GetCurrentPlayer() || !this->IsFoundation()))
            gMiniMap->MoveUnit(this, this->mTileX, this->mTileY, inX, inY);
        this->SightMoved(this->mTileX, this->mTileY, inX, inY);
        if (this->mTile != NULL) {
            if (this->mUnitClass->mTileSize <= 1) {
                long theSize = theRegions->mRegionSize;
                long theOldRegionX = theOldX / theSize;
                long theOldRegionY = theOldY / theSize;
                long theRegionX = inX / theSize;
                long theRegionY = inY / theSize;
                if (theOldRegionX == theRegionX && theOldRegionY == theRegionY && gWorld.mMap != NULL
                    && gWorld.mMap->GetTile(inX, inY) != NULL)
                    theSameRegion = true;
                else
                    theSameRegion = false;
            }
            if (!theSameRegion) {
                this->mUnitClass->RemoveFromRegion(this);
                if (this->mUnitClass->mInfluence != 0)
                    gWorld.mInfluenceTable->RemoveUnit(this, this->mTileX, this->mTileY);
            }
            this->mUnitClass->RemoveFromTile(this);
        }
    }
    this->mHiddenFlags |= kHiddenFlagMoved;
    this->mTileY = inY;
    this->mTileX = inX;
    EEUnitClass* theClass = this->mUnitClass;
    if (inX != -10000 && theClass->mIsObstacle && !(theClass->mDomains & kMovementAir) && !theClass->mIsProjectile && theClass->mMaxVelocity == 0.0f)
        this->UpdateHidden();
    if (gWorld.mMap == NULL)
        return;
    this->mTile = gWorld.mMap->GetTile(inX, inY);
    if (!inUpdateTiles)
        return;
    if (this->mTile != NULL) {
        this->mUnitClass->AddToTile(this);
        if (!theSameRegion) {
            this->mUnitClass->AddToRegion(this);
            if (this->mUnitClass->mInfluence != 0)
                gWorld.mInfluenceTable->AddUnit(this, inX, inY);
        }
        if (theOldTile == NULL && this->mComplexUnit != NULL && this->mComplexUnit->mPreviousPart == NULL
            && (!this->mUnitClass->mIsInactive || this->mHitPoints <= 0))
            gWorld.ActivateUnit(this->mComplexUnit);
    }
    if ((theOldTile != NULL || theOldX == -10000) && theOldX != inX)
        gWorld.ResortUnit(this);
}

// FUNCTION: EXE 0x629d73
// Takes inAmount from the resource; an exhausted resource unit leaves the minimap and is removed from the game
void EEUnit::DepleteResource(long inAmount)
{
    this->mResourceAmount -= inAmount;
    if (!(this->mResourceAmount <= 0))
        return;
    this->mResourceAmount = 0;
    if (this->mUnitClass->mCategory != kUnitCategoryResource)
        return;
    if (!EEIsCloakedEnemyOf(this, gWorld.GetCurrentPlayer()))
        gMiniMap->MoveUnit(this, this->mTileX, this->mTileY, -10000, -10000);
    this->mPlayer->UnitTaken(this, 0);
    this->mPlayer->KillUnit(this, false);
}

// FUNCTION: EXE 0x629de2
// Applies inDamage from inAttacker: area effects and morale adjust it and the attacker is remembered, then the unit
// loses hit points with damage and impact effects, an attack event and minimap ping, or dies
void EEUnit::TakeHit(EEUnit* inAttacker, long inDamage, unsigned long inPlayerNumber, long inMode)
{
    EEComplexUnit* theComplex = this->mComplexUnit;
    RTSPlayer* theBasePlayer = this->mPlayer;
    EEPlayer* thePlayer = dynamic_cast<EEPlayer*>(theBasePlayer);
    EETileLocation theAttackerTile;
    long theDamage = inDamage;
    long theAreaDamage = inDamage;
    bool theSuppressEffect = false;
    if (theComplex != NULL && theComplex->mPreviousPart != NULL) {
        theComplex->mPreviousPart->TakeHit(inAttacker, inDamage, inPlayerNumber, inMode);
        return;
    }
    if (::gWorld.mInCinematic || (theComplex != NULL && theComplex->mIsDisabled && !theComplex->IsProtected())) {
        if (this->mHitPoints > 0) {
            if (this->mUnitClass->mImpactSparksEffect != NULL)
                gEffectManager->CreateUnitEffect(this, this->mUnitClass->mImpactSparksEffect, NULL, false, 0, 0);
            else if (this->mUnitClass->mImpactBloodEffect != NULL)
                gEffectManager->CreateUnitEffect(this, this->mUnitClass->mImpactBloodEffect, NULL, false, 0, 0);
        }
        return;
    }
    long theHitPoints = this->mHitPoints;
    if (theHitPoints <= 0)
        return;
    if (theComplex != NULL) {
        if (inMode != 2) {
            long theInput = inDamage;
            theComplex->mAffectingAreaEffects.ApplyModifiers(1, theInput, theAreaDamage, theComplex);
            theComplex->ApplyMorale(theAreaDamage, theDamage);
        }
        if (inAttacker != NULL && inAttacker->mTileNode != NULL && inAttacker->mPlayer != this->mPlayer &&
            this->mPlayer->mEnemies[inAttacker->mPlayer->mPlayerID] != 0) {
            EEComplexUnit* theTarget = NULL;
            if (inAttacker->mComplexUnit->mOwner == NULL) {
                if (EEUUGetIsUnitClassTargetable(this, inAttacker->GetUnitClass()))
                    theTarget = inAttacker->mComplexUnit;
            } else if (EEUUGetIsUnitClassTargetable(this, inAttacker->mComplexUnit->mOwner->GetUnitClass()))
                theTarget = inAttacker->mComplexUnit->mOwner;
            if (theTarget != NULL) {
                theComplex->SetLastAttacker(theTarget);
                if (theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionIdle &&
                    theComplex->mUnitClass->FindAction(kActionUnderAttack) != NULL) {
                    theComplex->StartAction(kActionUnderAttack);
                    theComplex->mUnitClass->ActionChanged(theComplex);
                }
                EEGraphicEffect* theEffect = theTarget->mUnitClass->mShotEffect;
                if (theEffect != NULL && theEffect->mKind == kEffect3DTracers)
                    theSuppressEffect = true;
            }
        }
    }
    float theThreshold = (float)this->mStats->mMaxHitPoints * 0.1f;
    if ((float)theHitPoints > theThreshold && (float)(theHitPoints - theDamage) <= theThreshold)
        this->mUnitClass->BadlyHurt(this);
    theHitPoints -= theDamage;
    if (theHitPoints < 0)
        theHitPoints = 0;
    thePlayer->DamageTaken(inPlayerNumber, theDamage, this);
    if (this->mTileX != -10000) {
        if (inAttacker != NULL) {
            if (inAttacker->mTileNode != NULL && inAttacker->mComplexUnit->mOwner != NULL) {
                if (inAttacker->mComplexUnit->mOwner->mContainer == NULL)
                    theAttackerTile = EETileLocation(inAttacker->mComplexUnit->mOwner->mTileX,
                                                     inAttacker->mComplexUnit->mOwner->mTileY);
                else
                    theAttackerTile = EETileLocation(inAttacker->mComplexUnit->mOwner->mContainer->mTileX,
                                                     inAttacker->mComplexUnit->mOwner->mContainer->mTileY);
            } else
                theAttackerTile = EETileLocation(inAttacker->mTileX, inAttacker->mTileY);
        }
        if (theAttackerTile.GetX() < 0 || theAttackerTile.GetY() < 0)
            theAttackerTile = EETileLocation(this->mTileX, this->mTileY);
        if (inAttacker != NULL)
            thePlayer->mEvents.AddPlayerEvent(EETileLocation(this->mTileX, this->mTileY),
                theAttackerTile, 1, thePlayer, -1, false);
        else
            thePlayer->mEvents.AddPlayerEvent(EETileLocation(this->mTileX, this->mTileY),
                EETileLocation(this->mTileX, this->mTileY), 1, thePlayer, -1, false);
    }
    if (theHitPoints > 0) {
        EEGraphicEffect* theDamageEffect = this->mUnitClass->mDamageEffect;
        if (theDamageEffect != NULL && (this->mTileNode == NULL || this->mComplexUnit->mIsBuilt)) {
            float theInverseHealth = 1.0f / (float)this->mStats->mMaxHitPoints;
            if ((float)this->mHitPoints * theInverseHealth >= theDamageEffect->GetParam2() &&
                theDamageEffect->mParam2 > (float)theHitPoints * theInverseHealth) {
                if (this->mUnitClass->mCategory != kUnitCategoryGate)
                    gEffectManager->CreateUnitEffect(this, theDamageEffect, NULL, false, 0, 0);
                else {
                    EEUCGate* theGate = dynamic_cast<EEUCGate*>(this->mUnitClass);
                    gEffectManager->CreateUnitEffect(theGate->GetLastLinked(this->mComplexUnit),
                        this->mUnitClass->mDamageEffect, NULL, false, 0, 0);
                    gEffectManager->CreateUnitEffect(theGate->GetLastLinkedCarrier(this->mComplexUnit),
                        this->mUnitClass->mDamageEffect, NULL, false, 0, 0);
                }
            }
        }
        if (this->mUnitClass->mImpactSparksEffect != NULL && !theSuppressEffect)
            gEffectManager->CreateUnitEffect(this, this->mUnitClass->mImpactSparksEffect, NULL, false, 0, 0);
        else if (this->mUnitClass->mImpactBloodEffect != NULL)
            gEffectManager->CreateUnitEffect(this, this->mUnitClass->mImpactBloodEffect, NULL, false, 0, 0);
        this->mHitPoints = theHitPoints;
        if (theComplex != NULL) {
            if (theComplex->mTargetSearchTimer != 0)
                theComplex->mTargetSearchTimer = 0;
            long theCategory = theComplex->mUnitClass->mCategory;
            if ((theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship) && theComplex->mAmmunition > 0 &&
                theHitPoints == 1 && theComplex->mFlightTime != 0)
                theComplex->mFlightTime = 0;
        }
    } else {
        if (this->mUnitClass->mCategory == kUnitCategoryResource || this->mUnitClass->mCategory == kUnitCategoryAmbient)
            this->mHitPoints = 0;
        else {
            if (theComplex != NULL && this->mPlayer->mPlayerID != ::gWorld.mPlayers[0]->mPlayerID &&
                this->mPlayer->mPlayerID != inPlayerNumber) {
                long theCategory = theComplex->mUnitClass->mCategory;
                long theSeries = theCategory == kUnitCategoryBuilding || theCategory == kUnitCategoryBuilding19 || theCategory == kUnitCategoryGate ||
                    theCategory == kUnitCategoryMiningBuilding ? 2 : 3;
                ::gWorld.mPlayers[inPlayerNumber]->mSavedGroups.Reset(theSeries, 1);
            }
            this->Kill(true, false, false);
            this->mHitPoints = theHitPoints;
            if (this->mUnitClass->mUnitType == kUnitTypeCpICBM && theHitPoints == 0)
                gEffectManager->CreateUnitEffect(this, EEGraphicEffect::Get(kGraphicEffectAircraftExplosion), NULL, false, 0, 0);
        }
    }
    if (this->mPlayer == ::gWorld.GetCurrentPlayer() && this->mUnitClass->mCategory != kUnitCategoryResource) {
        GE2DPoint thePoint((float)this->mTileX, (float)this->mTileY);
        gMiniMap->Ping(thePoint);
    }
    if (theComplex != NULL && this->mPlayer == ::gWorld.GetCurrentPlayer() &&
        this->mPlayer->mSelectedGroup->Contains(theComplex))
        gUserInterface->PostMessage424Once();
}

// FUNCTION: EXE 0x62a3d8
// Remembers the unit's last attacker, moving the reference from the previous one
void EEComplexUnit::SetLastAttacker(EEComplexUnit* inUnit)
{
    if (this->mLastAttacker == inUnit)
        return;
    if (this->mLastAttacker)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = inUnit;
    inUnit->Reference();
}

// FUNCTION: EXE 0x62a3fd
// Regrows trees in the forest of a cut tree when its renewal is due (fewer, nearer ones when a citizen cut it), then
// schedules the next renewal 125-275 s ahead; never for carbon rock
void EEUnit::RegrowForest(EEComplexUnit* inUnit)
{
    if (EEUnit::IsCarbonRock(this->mUnitClass->mUnitType))
        return;
    EEAIForest* theForest = gContinentManager.GetForestAt(this);
    long theCount = 3;
    long theRadius = 3;
    if (theForest == NULL || !(theForest->mNextRenewal <= gPhysics->mGameTime) ||
        !(theForest->mTiles.Size() > 1))
        return;
    if (inUnit != NULL &&
        (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)) {
        theRadius = gPhysics->GetRandom(0L, 2L);
        theCount = gPhysics->GetRandom(1L, 2L);
    }
    U2DPoint<long> theTile(this->mTileX, this->mTileY);
    theForest->RegrowTrees(theTile, theRadius, theCount);
    long theTime = gPhysics->mGameTime;
    theForest->mNextRenewal = theTime + gPhysics->GetRandom(125000L, 275000L);
}

// FUNCTION: EXE 0x62a4c3
// Returns whether inUnitType is one of the carbon rock unit types
bool EEUnit::IsCarbonRock(unsigned long inUnitType)
{
    return inUnitType == 0x2fb || inUnitType == 0x306 || inUnitType == 0x307
        || (inUnitType >= 0x340 && inUnitType <= 0x345) || inUnitType == 0x348 || inUnitType == 0x24b
        || inUnitType == 0x24c;
}

// FUNCTION: EXE 0x62a506
// Does nothing (a simple unit has no sight to move)
void EEUnit::SightMoved(unsigned long, unsigned long, unsigned long, unsigned long)
{
}

// The empty slots EEUnit overrides; ICF folded each into a shared body elsewhere (no address of their own):
// ICF-FOLDED: EXE 0x5720c6
void EEUnit::SightOnHere()
{
}

// ICF-FOLDED: EXE 0x5720c6
void EEUnit::SightOffHere()
{
}

// ICF-FOLDED: EXE 0x563d6f
void EEUnit::SightOn(unsigned long, unsigned long)
{
}

// ICF-FOLDED: EXE 0x563d6f
void EEUnit::OutOfSynchDump(FILE*, unsigned long)
{
}

// ICF-FOLDED: EXE 0x4ac9f3
bool EEUnit::IsFoundation()
{
    return false;
}

// ICF-FOLDED: EXE 0x563d6f
void EEUnit::SightOff(unsigned long, unsigned long)
{
}

// ICF-FOLDED: EXE 0x557269
void EEUnit::ResearchCompleted(unsigned long)
{
}
