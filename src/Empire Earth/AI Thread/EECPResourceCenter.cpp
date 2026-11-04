// C:\EEX\Empire Earth\AI Thread\EECPResourceCenter.cpp (file name inferred: no __FILE__; the class-static key
// "EECPResourceCenter-Data"). A computer player's resource center: 0x8bbbf..0x8e702 (the key chain first; the vector /
// sort / find_if COMDATs close the unit).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
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
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEMorale.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAIRandom.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EEAIKindIDs.h"

const UString EECPResourceCenter::sData("EECPResourceCenter-Data");
// SYNTHETIC: EXE 0x48bbbf   (wrapper)
// SYNTHETIC: EXE 0x48bbc9   (ctor)
// SYNTHETIC: EXE 0x48bbda   (registrar)
// SYNTHETIC: EXE 0x48bbe6   (dtor)

// The tiles by their distance from mCenter, the nearer first: the unit's own predicate (its sort COMDATs 0x48d45c..
// call operator() out of line). Unit-local (provisional name)
struct EECPRCTileNearer {
    EECPRCTileNearer(const EETileCoordinate& inCenter) : mCenter(inCenter) {}
    EECPRCTileNearer(const EECPRCTileNearer& inOther) : mCenter(inOther.mCenter) {}
    ~EECPRCTileNearer() {}
    bool operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const;

    EETileCoordinate mCenter;
};

// FUNCTION: EXE 0x48bc02
// Orders two tiles by their squared distance from mCenter, the nearer first
// built without /Og (frame locals, leave; like EECPUnitTileDistanceLess 0x42cf1e): the pragma alone makes it exact
#pragma optimize("g", off)
bool EECPRCTileNearer::operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const
{
    long theLeft = inFirst.DistanceToSquared(this->mCenter);
    long theRight = inSecond.DistanceToSquared(this->mCenter);
    bool theResult = theLeft < theRight;
    return theResult;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x48bc3c
// Creates a resource center for the computer player, holding the given resource unit if any, with no main building
EECPResourceCenter::EECPResourceCenter(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    this->mUnit = NULL;
    this->mPlayer = inPlayer;
    if (inUnit != NULL)
        this->AddUnit(inUnit);
}

// FUNCTION: EXE 0x48bc71
// Destroys the resource center, releasing its resource units
EECPResourceCenter::~EECPResourceCenter()
{
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        this->mUnits[i]->Dereference();
    this->mUnits.clear();
}

// FUNCTION: EXE 0x48bcae
// Adds a resource unit to the center, taking a reference on it
void EECPResourceCenter::AddUnit(EEUnit* inUnit)
{
    inUnit->Reference();
    this->mUnits.push_back(inUnit);
}

// find's result in a local iterator, compared with a freshly loaded end()
// FUNCTION: EXE 0x48bcc5
// Returns whether the center holds the unit
bool EECPResourceCenter::Contains(EEUnit* inUnit)
{
    bool theResult = false;
    if (this->mUnits.size() > 0) {
        std::vector<EEUnit*>::iterator theFound = std::find(this->mUnits.begin(), this->mUnits.end(), inUnit);
        theResult = theFound != this->mUnits.end();
    }
    return theResult;
}

// FUNCTION: EXE 0x48bd2b
// Makes the player's drop site (not a granary) the center's main building if the center has none and the site is
// within drop-off distance of one of its resource units
void EECPResourceCenter::AdoptDropSite(EEComplexUnit* inUnit)
{
    long theRadius = this->mPlayer->mDropOffDistance;
    long theArea = theRadius * theRadius;
    if (this->mUnit != NULL || inUnit->mPlayer != this->mPlayer || !inUnit->GetUnitClass()->IsDropSite() || inUnit->GetUnitClass()->mUnitType == kUnitTypeGranary)
        return;
    unsigned long i, n;
    for (i = 0, n = this->mUnits.size(); i < n; i++) {
        if (!(inUnit->DistanceSquaredToUnit(this->mUnits[i]) < (float)theArea))
            continue;
        this->mUnit = inUnit;
        inUnit->Reference();
        break;
    }
}

// FUNCTION: EXE 0x48bdb4
// Returns the squared distance from this unit's position to another unit's position (its tile if not a complex unit)
float EEComplexUnit::DistanceSquaredToUnit(EEUnit* inOther)
{
    float theDeltaY;
    float theDeltaX;
    EEComplexUnit* theOther = inOther->mComplexUnit;
    if (theOther != NULL) {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - theOther->mPhysicalModel.GetLocation().GetY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - theOther->mPhysicalModel.GetLocation().GetX();
    } else {
        theDeltaY = this->mPhysicalModel.GetLocation().GetY() - inOther->GetTileY();
        theDeltaX = this->mPhysicalModel.GetLocation().GetX() - inOther->GetTileX();
    }
    return theDeltaX * theDeltaX + theDeltaY * theDeltaY;
}

// The owner's living, unfinished houses (unit type 0x25, +0x34c clear) whose tile lies in mRect (0x8da4d, out of
// line) (provisional name; layout inferred: 0x14 bytes on GetMoraleFill's frame)
struct EECPRCBuildingInRectangle {
    EECPRCBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && !inUnit->mIsBuilt &&
               inUnit->mHitPoints > 0 && this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// The owner's living houses (unit type 0x25) whose tile lies in mRect, finished or not (0x8d6e0, out of line;
// EECPRCBuildingInRectangle without the +0x34c test) (provisional name; layout inferred: 0x14 bytes on FindBuildSite's
// frame)
struct EECPRCLiveBuildingInRectangle {
    EECPRCLiveBuildingInRectangle(EEPlayer* inPlayer, const U2DRectangle<long>& inRect) : mPlayer(inPlayer), mRect(inRect) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && inUnit->GetUnitClass()->mUnitType == kUnitTypeHouse && inUnit->mHitPoints > 0 &&
               this->mRect.Contains(U2DPoint<long>(inUnit->GetTileX(), inUnit->GetTileY()));
    }

    EEPlayer* mPlayer;                      // +0x00
    U2DRectangle<long> mRect;               // +0x04
};

// A unit whose tile is one of mTiles (the find_if predicate 0x8d75b, out of line; 0x10 bytes: the vector copy is built
// straight into find_if's argument slot) (provisional name)
struct EECPRCUnitOnTiles {
    EECPRCUnitOnTiles(const std::vector<U2DPoint<long> >& inTiles) : mTiles(inTiles) {}
    bool operator()(EEUnit* inUnit) const
    {
        U2DPoint<long> theTile(inUnit->GetTileX(), inUnit->GetTileY());
        return std::find(this->mTiles.begin(), this->mTiles.end(), theTile) != this->mTiles.end();
    }

    std::vector<U2DPoint<long> > mTiles;    // +0x00
};

// Orders the units without an area effect of mKind for mPlayer on their tile first (the sort predicate 0x8e0ab, out
// of line, ret 8; 8 bytes by value) (provisional name)
struct EECPRCAreaEffectLess {
    EECPRCAreaEffectLess(long inKind, long inPlayer) : mKind(inKind), mPlayer(inPlayer) {}
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const
    {
        unsigned long theFirst = 0;
        if (gAEManager.HasAreaEffect(this->mKind, inFirst->GetTileX(), inFirst->GetTileY(), this->mPlayer))
            theFirst = 1;
        unsigned long theSecond = 0;
        if (gAEManager.HasAreaEffect(this->mKind, inSecond->GetTileX(), inSecond->GetTileY(), this->mPlayer))
            theSecond = 1;
        return theFirst < theSecond;
    }

    long mKind;                             // +0x00
    long mPlayer;                           // +0x04
};

// The tile vector is vector<U2DPoint<long> > (the zeroed U2DPoint locals pushed into it; Fn2492C2 /
// EEGetSquareRing / find 0x8d7bc / copy ctor 0x26f46 by their bound names)
// FUNCTION: EXE 0x48bdfa
// Finds a site near the resource center for a building: towers by gold or iron, houses in the main building's
// morale area, temples/universities/hospitals clear of their effects, others in rings round the center
bool EECPResourceCenter::FindBuildSite(long inUnitClass, EETileCoordinate& outTile)
{
    std::vector<EEComplexUnit*> theBuildings;
    U2DRectangle<long> theRect;
    U2DPoint<long> theSite;
    U2DPoint<long> theUnitCenter;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    EECPBuildManager* theBuildManager = this->mPlayer->mCivilizationManager->mBuildManager;
    long theRange = this->mPlayer->mDropOffDistance;
    long theSize = gUnitClassTable[inUnitClass]->mTileSize;
    bool theFound = false;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    long theKind2 = 0;
    unsigned long theCount = 0;
    long theEffectRadius = 0;
    unsigned short theContinent;
    std::vector<U2DPoint<long> >::iterator it;
    unsigned long i;
    if ((inUnitClass == this->mPlayer->mCivilizationManager->GetTowerClass() || inUnitClass == kUnitTypePriestTower) && this->mUnit != NULL) {
        float theFactor = this->mPlayer->mUnitClassUpgrades[inUnitClass]->mRange;
        float theRadius = this->mPlayer->mTowerSpacing * theFactor;
        float theRadiusSquared = theRadius * theRadius;
        theContinent = gContinentManager.GetContinent(U2DPoint<long>(this->mUnit->GetTileX(), this->mUnit->GetTileY()), kAIPathLandMass);
        for (i = 0, theCount = this->mUnits.size(); i < theCount && !theFound; i++) {
            EEUnit* theUnit = this->mUnits[i];
            if (theUnit->mResourceType != kResourceGold && theUnit->mResourceType != kResourceIron)
                continue;
            for (long theRing = 2; theRing < UMFloatToLong(theFactor) && !theFound; theRing++) {
                Fn2492C2(theUnit->GetTileX(), theUnit->GetTileY(), theRing, theTiles);
                theCenter.mX = this->mUnit->GetTileX() + this->mUnit->GetUnitClass()->mTileSize / 2;
                theCenter.mY = this->mUnit->GetTileY() + this->mUnit->GetUnitClass()->mTileSize / 2;
                std::sort(theTiles.begin(), theTiles.end(), EECPRCTileNearer(theCenter));
                for (it = theTiles.begin() + (theTiles.size() > 3 ? 3 : 0); it != theTiles.end() && !theFound;
                     it++) {
                    if (theBuildManager->HasTowerNear(it->mX, it->mY, theRadiusSquared))
                        continue;
                    theFound = theBuildManager->CanBuildAt(theContinent, inUnitClass, *it, theSize, true, true,
                                                        this->mUnit->GetTileX(), this->mUnit->GetTileY());
                    if (theFound) {
                        outTile.mX = it->mX;
                        outTile.mY = it->mY;
                    }
                }
            }
        }
    } else {
        switch (inUnitClass) {
        case kUnitTypeSettlement: {
            this->GetCenter(theCenter);
            unsigned short theCenterContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
            for (long theRing = 2; theRing < theRange && !theFound; theRing++) {
                Fn2492C2(theCenter.mX, theCenter.mY, theRing, theTiles);
                for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++) {
                    theFound = theBuildManager->CanBuildAt(theCenterContinent, kUnitTypeSettlement, *it, theSize, true, true, theCenter.mX,
                                                        theCenter.mY);
                    if (theFound) {
                        outTile.mX = it->mX;
                        outTile.mY = it->mY;
                    }
                }
            }
            break;
        }
        case kUnitTypeHouse: {
            EEUnit* theUnit;
            if (this->mUnits.size() > 0) {
                theContinent = gContinentManager.GetContinent(this->mUnits[0], kAIPathLandMass);
                theUnit = this->mUnits[0];
            } else {
                theContinent = gContinentManager.GetContinent(this->mUnit, kAIPathLandMass);
                theUnit = this->mUnit;
            }
            EETileCoordinate theUnitTile;
            theUnitTile.mX = theUnit->GetTileX();
            theUnitTile.mY = theUnit->GetTileY();
            long theUnitSize = this->mUnit->GetUnitClass()->mTileSize;
            long theX = this->mUnit->GetTileX() + theUnitSize / 2;
            long theY = this->mUnit->GetTileY() + theUnitSize / 2;
            for (std::vector<EEAreaEffect*>::iterator theIterator = this->mUnit->mAreaEffects.mItems.begin();
                 theIterator != this->mUnit->mAreaEffects.mItems.end(); ++theIterator) {
                EEAEMoraleFromBuildings* theEffect = dynamic_cast<EEAEMoraleFromBuildings*>(*theIterator);
                if (theEffect == NULL)
                    continue;
                theEffectRadius = UMFloatToLong(theEffect->GetRadius());
                break;
            }
            if (this->GetMoraleFill() > 0.0f) {
                theRect.mTop = theY - theEffectRadius;
                theRect.mBottom = theEffectRadius + theY - 1;
                theRect.mLeft = theX - theEffectRadius;
                theRect.mRight = theEffectRadius + theX - 1;
                EEGetUnitsInRectangle(theRect, theBuildings, EECPRCLiveBuildingInRectangle(this->mPlayer, theRect), true);
                for (i = 0, theCount = theBuildings.size(); i < theCount; i++) {
                    EEComplexUnit* theBuilding = theBuildings[i];
                    theSite = U2DPoint<long>(theBuilding->GetTileX() - theSize, theBuilding->GetTileY());
                    if (this->IsInAreaEffect(theSite.mX, theSite.mY, 0x25, 5))
                        theTiles.push_back(theSite);
                    theSite = U2DPoint<long>(theBuilding->GetTileX() + theSize, theBuilding->GetTileY());
                    if (this->IsInAreaEffect(theSite.mX, theSite.mY, 0x25, 5))
                        theTiles.push_back(theSite);
                    theSite = U2DPoint<long>(theBuilding->GetTileX(), theBuilding->GetTileY() - theSize);
                    if (this->IsInAreaEffect(theSite.mX, theSite.mY, 0x25, 5))
                        theTiles.push_back(theSite);
                    theSite = U2DPoint<long>(theBuilding->GetTileX(), theBuilding->GetTileY() + theSize);
                    if (this->IsInAreaEffect(theSite.mX, theSite.mY, 0x25, 5))
                        theTiles.push_back(theSite);
                }
            }
            if (theTiles.size() > 0)
                EEAIRandomShuffle(theTiles.begin(), theTiles.end());
            for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++) {
                U2DPoint<long> theTile = *it;
                theFound = theBuildManager->CanBuildAt(theContinent, kUnitTypeHouse, *it, theSize, false, true, -1, -1);
                if (!theFound)
                    continue;
                unsigned long j, n;
                for (j = 0, n = this->mUnits.size(); j < n && theFound; j++) {
                    EEUnit* theOther = this->mUnits[j];
                    long theOtherSize = theOther->GetUnitClass()->mTileSize;
                    float theHalf = theOtherSize * 0.5f;
                    theUnitCenter = U2DPoint<long>(UMFloatToLong(theOther->GetTileX() + theHalf),
                                                   UMFloatToLong(theOther->GetTileY() + theHalf));
                    theHalf = theSize * 0.5f;
                    theSite = U2DPoint<long>(UMFloatToLong(theTile.mX + theHalf), UMFloatToLong(theTile.mY + theHalf));
                    theFound = theSite.DistanceTo(theUnitCenter) >= theOtherSize + 2;
                }
                if (theFound) {
                    outTile.mX = it->mX;
                    outTile.mY = it->mY;
                }
            }
            if (!theFound) {
                for (long theRing = theEffectRadius - 2; theRing > theUnitSize && !theFound; theRing--) {
                    Fn2492C2(theX, theY, theRing, theTiles);
                    for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++) {
                        theFound = theBuildManager->CanBuildAt(theContinent, kUnitTypeHouse, *it, theSize, false, true, theUnitTile.mX,
                                                            theUnitTile.mY);
                        if (!theFound)
                            continue;
                        unsigned long j, n;
                        for (j = 0, n = this->mUnits.size(); j < n && theFound; j++) {
                            EEUnit* theOther = this->mUnits[j];
                            long theOtherSize = theOther->GetUnitClass()->mTileSize;
                            float theHalf = theOtherSize * 0.5f;
                            theUnitCenter = U2DPoint<long>(UMFloatToLong(theOther->GetTileX() + theHalf),
                                                           UMFloatToLong(theOther->GetTileY() + theHalf));
                            theHalf = theSize * 0.5f;
                            theSite = U2DPoint<long>(UMFloatToLong(it->mX + theHalf),
                                                     UMFloatToLong(it->mY + theHalf));
                            theFound = theSite.DistanceTo(theUnitCenter) >= theOtherSize + 2;
                        }
                        if (theFound) {
                            outTile.mX = it->mX;
                            outTile.mY = it->mY;
                        }
                    }
                }
            }
            break;
        }
        case kUnitTypeBarracks:
        case kUnitTypeArcheryRange:
        case kUnitTypeSiegeFactory:
        case kUnitTypeStable:
        case kUnitTypeMissileBase:
        case kUnitTypeTankFactory:
        case kUnitTypeCyberFactory:
        case kUnitTypeCyberLaboratory:
        case kUnitTypeCpMissileBase:
            if (this->mUnit != NULL) {
                if (this->mUnits.size() == 0) {
                    theCenter.mX = theBuildManager->mTownCenterTile.mX;
                    theCenter.mY = theBuildManager->mTownCenterTile.mY;
                } else
                    this->GetCenter(theCenter);
                theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
                if (theCenter.mX != -10000 && theCenter.mY != -10000) {
                    for (long theRing = theRange + 5; theRing > 1 && !theFound; theRing--) {
                        Fn2492C2(theCenter.mX, theCenter.mY, theRing, theTiles);
                        for (it = theTiles.begin(); it != theTiles.end() && !theFound; it++) {
                            theFound = theBuildManager->CanBuildAt(theContinent, inUnitClass, *it, theSize, true, true,
                                                                theCenter.mX, theCenter.mY);
                            if (theFound) {
                                outTile.mX = it->mX;
                                outTile.mY = it->mY;
                            }
                        }
                    }
                }
            }
            break;
        case kUnitTypeTemple:
        case kUnitTypeUniversity:
        case kUnitTypeHospital: {
            this->GetCenter(theCenter);
            theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
            long theKind;
            long theOtherClass = 0;
            if (inUnitClass == kUnitTypeHospital)
                theKind = 1;
            else if (inUnitClass == kUnitTypeTemple) {
                theKind = 3;
                theKind2 = 4;
                theOtherClass = 0x3c;
            } else {
                theKind = 4;
                theKind2 = 3;
                theOtherClass = 0x27;
            }
            EEGetPlayerUnitsOfType(inUnitClass, this->mPlayer->mPlayerID, theUnits);
            EEGetPlayerUnitsOfType(theOtherClass, this->mPlayer->mPlayerID, theUnits);
            std::vector<EEUnit*>::iterator theUnit = theUnits.begin();
            while (theUnit != theUnits.end()) {
                if ((*theUnit)->mTileNode == NULL || (*theUnit)->mComplexUnit->mIsBuilt)
                    theUnit = theUnits.erase(theUnit);
                else
                    theUnit++;
            }
            long theRadius = this->GetMinEffectRadius(inUnitClass);
            if (inUnitClass == kUnitTypeHospital)
                theRadius *= 3;
            EEGetTilesInRadius(theCenter.mX, theCenter.mY, theRadius, true, theTiles);
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPRCUnitOnTiles(theTiles)) == theUnits.end()) {
                std::sort(this->mUnits.begin(), this->mUnits.end(), EECPRCAreaEffectLess(theKind, this->mPlayer->mPlayerID));
                for (i = 0, theCount = this->mUnits.size(); i < theCount && !theFound; i++) {
                    EEUnit* theOther = this->mUnits[i];
                    long theX = theOther->GetTileX();
                    long theY = theOther->GetTileY();
                    long theOtherSize = theOther->GetUnitClass()->mTileSize;
                    bool theBlocked = false;
                    bool theBlocked2 = false;
                    for (long k = 0; k < theRadius && !theFound && !theBlocked && !theBlocked2; k++) {
                        long theSide = theOtherSize + theSize + 2 * k;
                        EEGetSquareRing(theX - k - theSize, theY - k - theSize, theSide, theSide, theTiles);
                        EEAIRandomShuffle(theTiles.begin(), theTiles.end());
                        for (it = theTiles.begin(); it != theTiles.end() && !theFound && !theBlocked && !theBlocked2;
                             it++) {
                            theBlocked = this->IsInAreaEffect(it->mX, it->mY, inUnitClass, theKind);
                            theBlocked2 = this->IsInAreaEffect(it->mX, it->mY, inUnitClass, theKind2);
                            if (!theBlocked && !theBlocked2 &&
                                theBuildManager->CanBuildAt(theContinent, inUnitClass, *it, theSize, true, true,
                                                         theCenter.mX, theCenter.mY)) {
                                outTile.mX = it->mX;
                                outTile.mY = it->mY;
                                theFound = true;
                            } else
                                theFound = false;
                        }
                    }
                }
            }
            break;
        }
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x48cbec
// Returns the smallest radius among the area effect templates of a unit class's upgrade (0 when it has none)
long EECPResourceCenter::GetMinEffectRadius(long inUnitClass)
{
    long theRadius = 0;
    EEUnitClassUpgrade* theUpgrade = this->mPlayer->mUnitClassUpgrades[inUnitClass];
    for (unsigned long i = 0; i < 4; i++) {
        unsigned long theTemplate = i < 4 ? theUpgrade->mAreaEffectTemplates[i] : 0;
        if (theTemplate <= 0)
            continue;
        float theTemplateRadius = gAEManager.mTemplates[theTemplate]->mRadius;
        if (theRadius == 0 || theTemplateRadius < theRadius)
            theRadius = UMFloatToLong(theTemplateRadius);
    }
    return theRadius;
}

// FUNCTION: EXE 0x48cc5f
// Returns how full the main building's morale effect is: its level plus the unfinished houses in range, over
// its maximum level (1 without a main building or effect)
float EECPResourceCenter::GetMoraleFill()
{
    std::vector<EEComplexUnit*> theUnits;
    float theResult = 1.0f;
    EEComplexUnit* theUnit = this->mUnit;
    U2DRectangle<long> theRect;
    if (theUnit != NULL) {
        long theHalf = theUnit->GetUnitClass()->mTileSize / 2;
        long theX = theUnit->GetTileX() + theHalf;
        long theY = theUnit->GetTileY() + theHalf;
        for (std::vector<EEAreaEffect*>::iterator theIterator = theUnit->mAreaEffects.mItems.begin();
             theIterator != theUnit->mAreaEffects.mItems.end(); ++theIterator) {
            EEAEMoraleFromBuildings* theEffect = dynamic_cast<EEAEMoraleFromBuildings*>(*theIterator);
            if (theEffect == NULL)
                continue;
            theEffect->MarkDirty();
            theEffect->Recount(0);
            long theCount = theEffect->GetLevel();
            long theMax = theEffect->mTemplate->mMaxLevel;
            long theRadius = UMFloatToLong(theEffect->GetRadius());
            theRect.mTop = theY - theRadius;
            theRect.mLeft = theX - theRadius;
            theRect.mBottom = theRadius + theY - 1;
            theRect.mRight = theRadius + theX - 1;
            EEGetUnitsInRectangle(theRect, theUnits, EECPRCBuildingInRectangle(this->mPlayer, theRect), true);
            if (theMax == 0)
                theResult = 1.0f;
            else
                theResult = (float)(theUnits.size() + theCount) / theMax;
            break;
        }
    }
    return theResult;
}

// NEAR: the original reads mUnits[i] through out-of-line begin()/operator+ and keeps the box in memory
// FUNCTION: EXE 0x48cdbf
// Returns the free tile nearest the middle of the resource units (or next to the single unit or main building),
// else the first candidate tile; leaves outCenter unchanged when there is nothing to measure
void EECPResourceCenter::GetCenter(EETileCoordinate& outCenter)
{
    std::vector<EETileCoordinate> theTiles;
    EEAIAStarBounds theBounds;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    bool theFound = false;
    if (this->mUnits.size() > 1) {
        EETileCoordinate theTile;
        theTile.mX = this->mUnits[0]->GetTileX();
        theTile.mY = this->mUnits[0]->mTileY;
        theBounds.Set(theTile);
        for (unsigned long i = 1; i < this->mUnits.size(); i++) {
            theTile.mX = this->mUnits[i]->GetTileX();
            theTile.mY = this->mUnits[i]->mTileY;
            theBounds.Extend(theTile);
        }
        theBounds.GetCenter(theCenter);
        long theRadius = (theBounds.mRight - theBounds.mLeft) / 2;
        if (theRadius < 5)
            theRadius = 5;
        EEGetTilesInRadius(theCenter.mX, theCenter.mY, theRadius, true, theTiles);
        std::sort(theTiles.begin(), theTiles.end(), EECPRCTileNearer(theCenter));
    } else {
        EEUnit* theUnit;
        long theY;
        long theSize;
        if (this->mUnits.size() <= 0) {
            theUnit = this->mUnit;
            if (theUnit == NULL)
                goto search;
            theSize = theUnit->GetUnitClass()->mTileSize;
            theY = theUnit->GetTileY();
        } else {
            theSize = this->mUnits[0]->mUnitClass->mTileSize;
            theY = this->mUnits[0]->mTileY;
            theUnit = this->mUnits[0];
        }
        EEGetSquareBlockRing(1, theUnit->GetTileX(), theY, theSize, theTiles, 0);
    }
search:
    std::vector<EETileCoordinate>::iterator it;
    for (it = theTiles.begin(); it != theTiles.end(); ++it) {
        if (theFound)
            break;
        EETile* theTile = gWorld.mMap->GetTileAtLocation(U2DPoint<long>(it->mX, it->mY));
        if (theTile != NULL && theTile->mUnits == NULL) {
            outCenter = *it;
            theFound = true;
        } else
            theFound = false;
    }
    if (!theFound && theTiles.size() > 0)
        outCenter = theTiles[0];
}
// FUNCTION: EXE 0x48d065
// Returns the first resource unit of the given resource type that is not exhausted, or NULL
EEUnit* EECPResourceCenter::FindResourceUnit(unsigned long inUnitType)
{
    EEUnit* theResult = NULL;
    unsigned long i, n;
    for (i = 0, n = this->mUnits.size(); i < n && theResult == NULL; i++) {
        EEUnit* theUnit = this->mUnits[i];
        if (theUnit->mResourceType == inUnitType && theUnit->mResourceAmount > 0)
            theResult = theUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x48d0a2
// Returns whether any tile under a unit class's footprint at (inX, inY) has an area effect of the kind for the player
bool EECPResourceCenter::IsInAreaEffect(long inX, long inY, long inUnitClass, long inKind)
{
    long thePlayerID = this->mPlayer->mPlayerID;
    bool theResult = false;
    if (inKind != 0) {
        EEUnitClass* theClass = gUnitClassTable[inUnitClass];
        if (theClass != NULL) {
            for (long x = inX; x < inX + theClass->mTileSize && !theResult; x++) {
                for (long y = inY; y < inY + theClass->mTileSize && !theResult; y++)
                    theResult = gAEManager.HasAreaEffect(inKind, x, y, thePlayerID);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x48d126
// Reads the resource unit ids and the main building's id (resolved by Restore) and, from version 0.141 on, the
// active flag; returns the first read error
long EECPResourceCenter::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount;
    unsigned long theID;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        this->mUnits.clear();
        for (i = 0; i < theCount; i++) {
            theResult = inFile.Read(theID);
            if (theResult == kErrorOK) {
                EEUnit* theUnit = reinterpret_cast<EEUnit*>(theID);  // the ids sit in the list until Restore
                this->mUnits.push_back(theUnit);
            }
        }
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theID);
            if (theResult == kErrorOK) {
                this->mUnit = reinterpret_cast<EEComplexUnit*>(theID);
                if (inMinorVersion >= 0x8d)
                    theResult = inFile.Read(this->mIsActive);
            }
        }
    }
    return theResult;
}

// NEAR: the original reads the elements as *(begin() + i) through the out-of-line Ptrit operator+ and pushes the
// element's address as a const long&
// FUNCTION: EXE 0x48d1bd
// Turns the loaded ids back into units (dropping missing and duplicate ones) and references the main building;
// returns 3 if the main building no longer exists
long EECPResourceCenter::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<long> theIDs;
    long theResult = 0;
    unsigned long i;
    for (i = 0; i < this->mUnits.size(); i++)
        theIDs.push_back(reinterpret_cast<long>(this->mUnits[i]));
    this->mUnits.clear();
    for (i = 0; i < theIDs.size(); i++) {
        if (gWorld.mPlayers[0] == NULL)
            continue;
        EEUnit* theUnit = gWorld.GetUnit(theIDs[i]);
        if (theUnit != NULL && !this->Contains(theUnit))
            this->AddUnit(theUnit);
    }
    if (reinterpret_cast<long>(this->mUnit) != -1) {
        this->mUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mUnit)));
        if (this->mUnit != NULL)
            this->mUnit->Reference();
        else
            theResult = 3;
    } else
        this->mUnit = NULL;
    return theResult;
}

// FUNCTION: EXE 0x48d2bc
// Returns whether a unit's tile is within the player's drop-off distance of the center
bool EECPResourceCenter::IsWithinRadius(EEUnit* inUnit)
{
    EETileDistanceLess theCenter(0, 0);
    long theRadius = this->mPlayer->mDropOffDistance;
    this->GetCenter(theCenter.mCenter);
    EETileCoordinate theTile;
    theTile.mX = inUnit->GetTileX();
    theTile.mY = inUnit->GetTileY();
    return theCenter.DistanceSquared(theTile) < theRadius * theRadius;
}

// FUNCTION: EXE 0x48d306
// Writes the ids of the valid resource units, the main building's id (-1 for none) and the active flag; returns
// the first write error
long EECPResourceCenter::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long i;
    unsigned long theCount;
    for (i = 0, theCount = 0; i < this->mUnits.size(); i++) {
        if (this->mUnits[i]->mIsValid)
            theCount++;
    }
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    // stop at the first failed id write (each result used to overwrite the previous one)
    for (i = 0; i < this->mUnits.size() && theResult == 0; i++) {
        EEUnit* theUnit = this->mUnits[i];
        if (theUnit->mIsValid) {
            unsigned long theID = theUnit->GetUnitID();
            theResult = outFile.Write(theID);
        }
    }
    if (theResult == 0) {
        unsigned long theMainID;
        theMainID = (this->mUnit != NULL && this->mUnit->mIsValid) ? this->mUnit->GetUnitID() : -1;
        theResult = outFile.Write(theMainID);
        if (theResult == 0)
            theResult = outFile.Write(this->mIsActive);
    }
    return theResult;
}

// FUNCTION: EXE 0x48d3c3
// Sets the center's main building, moving the reference from the old one
void EECPResourceCenter::SetUnit(EEComplexUnit* inUnit)
{
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->mUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x48d408
// Releases and clears the main building if it is the given unit
void EECPResourceCenter::ReleaseUnit(EEComplexUnit* inUnit)
{
    if (this->mUnit == inUnit) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
}

// FUNCTION: EXE 0x48d3e4
// Sets whether the center is active; activating it also computes its center (the result is discarded)
void EECPResourceCenter::SetActive(bool inFlag, long inArg2)
{
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    this->mIsActive = inFlag;
    if (inFlag)
        this->GetCenter(theCenter);
}

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x48d421
// TEMPLATE: EXE 0x48d45c
// TEMPLATE: EXE 0x48d542
// TEMPLATE: EXE 0x48d6e0
// TEMPLATE: EXE 0x48d728
// TEMPLATE: EXE 0x48d75b
// TEMPLATE: EXE 0x48d7e9
// TEMPLATE: EXE 0x48d8af
// TEMPLATE: EXE 0x48da4d
// TEMPLATE: EXE 0x48da9e
// TEMPLATE: EXE 0x48dad1
// TEMPLATE: EXE 0x48de27
// TEMPLATE: EXE 0x48de6b
// TEMPLATE: EXE 0x48e0ab
// TEMPLATE: EXE 0x48e345

// Template and inline COMDATs this unit already emits (compared by name)
// TEMPLATE: EXE 0x48dbde
// TEMPLATE: EXE 0x48dc28
// TEMPLATE: EXE 0x48dc55
// TEMPLATE: EXE 0x48dd31
// TEMPLATE: EXE 0x48dd65
// TEMPLATE: EXE 0x48deb8
// TEMPLATE: EXE 0x48dfcf
// TEMPLATE: EXE 0x48e00b
// TEMPLATE: EXE 0x48e10c
// TEMPLATE: EXE 0x48e149
// TEMPLATE: EXE 0x48e18d
// TEMPLATE: EXE 0x48e2ad
// TEMPLATE: EXE 0x48e2e0
// TEMPLATE: EXE 0x48e378
// TEMPLATE: EXE 0x48e3bf
// TEMPLATE: EXE 0x48e3ec
// TEMPLATE: EXE 0x48e486
// TEMPLATE: EXE 0x48e4fe
// TEMPLATE: EXE 0x48e56f
// TEMPLATE: EXE 0x48e5c5
// TEMPLATE: EXE 0x48e628
// TEMPLATE: EXE 0x48e67c
// TEMPLATE: EXE 0x48e6cb
