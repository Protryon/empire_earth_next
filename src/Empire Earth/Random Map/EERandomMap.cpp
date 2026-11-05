// C:\EEX\Empire Earth\Random Map\EERandomMap.cpp (__FILE__ at 0x28655a). The game's random map: the generator list
// (EERandomMap::Initialize over the "rmap" PIMs and Data\Random Map Scripts\*.rmv), and one EERandomMap per try:
// the generator PIM steps on the shared state (ee/EERandomMap.h, seen here as EERandomMapData), then the game-side
// passes that turn the state into the EEMap's tiles, terrain and units. 0x283b2e..0x28befc: the two key chains, the
// class statics' chains, the members in address order, then the template COMDATs (0x288bd4..). Unit flags: the exe's
// module set. Header: ee/EERandomMapClass.h.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/PIMModule.h"
#include "lle/U2DPoint.h"
#include "lle/U2DFractal.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUCLandCitizen.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGameOptions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EERules.h"

// EETerrainTable::sCliffTable[inIndex] (its COMDAT 0x28aaf7 follows the unit's code; provisional name)
inline EECliffTerrainTable* EEGetCliffTerrainTable(unsigned long inIndex)
{
    return EETerrainTable::sCliffTable[inIndex];
}

// the generator list's keys (unguarded file-scope globals, 0x5655e0 / 0x5655d0)
const UString kUseMapGeneratorKey("UseMapGenerator");
const UString kStringTableIDKey("StringTableID");

FSFileSpec EERandomMap::sGeneratorDirectory;
std::vector<EERMGenerator> EERandomMap::sRandomMapGenerators;

// unit-local constants (.rdata 0x44c00c, 0x44c040, 0x44c048; provisional names)
static const float kRMMinimumPlayerDistance = 6.0f;
static const float kRMRingScale = 0.35355338f;
static const float kRMElevationOffset = -10.0f;

// the grid helpers (their COMDATs follow the unit's code at 0x28bccb..0x28be87: inline, refused at every call)
inline unsigned long EERMGetElevation(float inHeight)
{
    unsigned long theIndex = UMFloorUnsigned(UMFloor(inHeight) - kRMElevationOffset);
    if (theIndex > 30)
        theIndex = 30;
    return theIndex;
}

inline bool EERMIsAreaFree(const std::vector<std::vector<unsigned char> >& inValid,
                           const std::vector<std::vector<unsigned char> >& inUsed, unsigned long inX, unsigned long inY,
                           unsigned long inRadius)
{
    unsigned long theSize = inValid.size();
    unsigned long theLeft = inX - inRadius;
    unsigned long theRight = inX + inRadius;
    unsigned long theBottom = inY - inRadius;
    unsigned long theTop = inY + inRadius;
    bool theResult = true;
    if (theLeft >= theSize)
        theLeft = 0;
    if (theBottom >= theSize)
        theBottom = 0;
    if (theRight >= theSize)
        theRight = theSize - 1;
    if (theTop >= theSize)
        theTop = theSize - 1;
    for (unsigned long x = theLeft; x <= theRight && theResult; x++) {
        for (unsigned long y = theBottom; y <= theTop && theResult; y++) {
            if (inValid[x][y] == 0 || inUsed[x][y] != 0)
                theResult = false;
            else
                theResult = true;
        }
    }
    return theResult;
}

inline bool EERMIsNearEdge(const std::vector<std::vector<unsigned char> >& inValid, unsigned long inX, unsigned long inY,
                           unsigned long inRadius)
{
    unsigned long theSize = inValid.size();
    unsigned long theLeft = inX - inRadius;
    unsigned long theRight = inX + 1 + inRadius;
    unsigned long theBottom = inY - inRadius;
    unsigned long theTop = inY + 1 + inRadius;
    return !(theLeft < theSize && theRight < theSize && theBottom < theSize && theTop < theSize &&
             inValid[theLeft][theBottom] && inValid[theLeft][theTop] && inValid[theRight][theBottom] &&
             inValid[theRight][theTop]);
}

inline void EERMClaimArea(std::vector<std::vector<unsigned char> >& ioUsed, unsigned long inX, unsigned long inY,
                          unsigned long inSize)
{
    unsigned long theSize = ioUsed.size();
    for (unsigned long i = 0; i < inSize; i++) {
        for (unsigned long j = 0; j < inSize; j++) {
            if (inX + i < theSize && j + inY < theSize)
                ioUsed[inX + i][j + inY] = true;
        }
    }
}

static bool EERMIsCliff(const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY);

// the generator list's string ids: the terrain named "Space" (0x5655f0; provisional name)
unsigned long gSpaceTerrain;

// FUNCTION: EXE 0x683c18
// Creates a random map for a generator PIM, the game options and the players, binding the generator's entry points
EERandomMap::EERandomMap(PIMModule* inModule, EEGameOptions* inOptions, std::vector<EEPlayer*>* inPlayers)
    : mGamePlayers(inPlayers), mOptions(inOptions)
{
    this->mModule = inModule;
    this->mCreateFlatMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateFlatMap");
    this->mCreateHeightMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateHeightMap");
    this->mCreateNeutralLandWaterMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateNeutralLandWaterMap");
    this->mCreatePlayerLandWaterMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreatePlayerLandWaterMap");
    this->mCreateTrees = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateTrees");
    this->mFilterHeightMapPIM = (EERMPIMFunction)inModule->GetEntry("EERMPIMFilterHeightMap");
    this->mGenerateNeutralPositions = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateNeutralPositions");
    this->mGeneratePlayerPositions = (EERMPIMFunction)inModule->GetEntry("EERMPIMGeneratePlayerPositions");
    this->mGenerateResources = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateResources");
    this->mGenerateShallows = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateShallows");
    this->mSetRandomMapValues = (EERMPIMSetValuesFunction)inModule->GetEntry("EERMPIMSetRandomMapValues");
}

// SYNTHETIC: EXE 0x683dea
// (the scalar deleting destructor, emitted with the vtable)

// SYNTHETIC: EXE 0x683e06
// (EERandomMapData's implicit default constructor, out of line after its first user)

// FUNCTION: EXE 0x684025
// Destroys the random map (members only)
EERandomMap::~EERandomMap()
{
}

// SYNTHETIC: EXE 0x684033
// (EERandomMapData's implicit destructor)

// FUNCTION: EXE 0x68443b
// Returns whether a cell of a byte grid (the valid tiles) lies inside the grid and is set
static bool EERMIsCellSet(const std::vector<std::vector<unsigned char> >& inGrid, unsigned long inX, unsigned long inY)
{
    return inX < inGrid.size() && inY < inGrid.size() && inGrid[inX][inY] != 0;
}

// FUNCTION: EXE 0x684477
// Returns whether a grid cell has no land value (its low 15 bits are clear)
static bool EERMIsTileFree(const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY)
{
    return (inTiles[inX][inY] & 0x7fff) == 0;
}

// FUNCTION: EXE 0x684149
// On space maps fills empty cells (a disc on Earth and Mars maps, else cells with 5+ land neighbours), then lists the
// empty valid map cells as the water points
void EERandomMap::Fn284149()
{
    GE2DPoint thePoint;
    unsigned long theSize = this->mData.mGridSize;
    unsigned long x;
    unsigned long y;
    if (this->mData.mIsSpaceMap) {
        if ((this->mData.mMapCharacteristics & kRMMapEarthOrMarsMap) != 0) {
            float theRadius = (this->mData.mHalfMapSize - this->mData.mWaterBorder) * 0.75f;
            for (x = 0; x < theSize; x++) {
                for (y = 0; y < theSize; y++) {
                    if (this->mData.mValidTiles[x][y] && this->mData.mRing.mCenter.DistanceTo(GE2DPoint((float)x, (float)y)) <= theRadius &&
                        EERMIsTileFree(this->mData.mTiles, x, y))
                        this->mData.mTiles[x][y] = 0x400;
                }
            }
        } else {
            for (x = 0; x < theSize; x++) {
                for (y = 0; y < theSize; y++) {
                    if (!this->mData.mValidTiles[x][y] || !EERMIsTileFree(this->mData.mTiles, x, y))
                        continue;
                    unsigned long theValue = 0;
                    unsigned long theCount = 0;
                    for (unsigned long i = x - 1; i <= x + 1; i++) {
                        for (unsigned long j = y - 1; j <= y + 1; j++) {
                            if (!EERMIsTileFree(this->mData.mTiles, i, j)) {
                                theValue = this->mData.mTiles[i][j];
                                theCount++;
                            }
                        }
                    }
                    if (theCount >= 5)
                        this->mData.mTiles[x][y] = theValue;
                }
            }
        }
    }
    this->mData.mWaterPoints.clear();
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (!this->mMap->GetTile(x, y) || !EERMIsCellSet(this->mData.mValidTiles, x, y) || (this->mData.mTiles[x][y] & 0x7fff) != 0)
                continue;
            thePoint.SetX((float)x);
            thePoint.SetY((float)y);
            this->mData.mWaterPoints.push_back(thePoint);
        }
    }
}

// FUNCTION: EXE 0x68449e
// Runs the generator's steps in order (positions, land and water, heights, terrain, resources, trees, features) with
// load progress; returns the first step error, or 9 when the players end up cut off
long EERandomMap::Generate()
{
    long theResult;
    if ((this->mData.mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
        this->mData.mNumTerrainBands = (unsigned long)(this->mData.mTerrainRegions[this->mData.mTerrainRegions.size() - 1].mTo + 1) / 100;
        this->mData.mTerrainBandOffset = this->mData.mRandomIntegers.Generate(0, this->mData.mNumTerrainBands);
    }
    gUsingCarbon = (this->mData.mMapCharacteristics & kRMMapMarsMap) == kRMMapMarsMap;
    theResult = this->mGeneratePlayerPositions(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.1f);
    theResult = this->mGenerateNeutralPositions(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.14f);
    theResult = this->mCreatePlayerLandWaterMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.18f);
    theResult = this->mCreateNeutralLandWaterMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.22f);
    this->Fn284149();
    theResult = this->mCreateFlatMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.26f);
    theResult = this->mCreateHeightMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.3f);
    theResult = this->mGenerateShallows(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.34f);
    theResult = this->FilterHeightMap();
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.38f);
    this->SmoothHeights();
    this->mMap->SetCliffTexture(this->mData.mCliffTexture);
    this->Fn288839();
    if (!EERMArePlayersConnected(this->mData, this->mMap))
        return 9;
    this->PaintTiles();
    this->mMap->UpdateCliffs(0, this->mMap->mSize, 0, this->mMap->mSize);
    this->MarkFeatures();
    theResult = this->mGenerateResources(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.4f + 0.02f);
    theResult = this->mCreateTrees(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.46f);
    this->PlaceFeatures();
    this->Fn288426();
    this->mMap->mMapCharacteristics = this->mData.mMapCharacteristics;
    return 0;
}

// FUNCTION: EXE 0x684701
// Checks that every player's start area is dry, non-space land and that no two players stand closer than
// kRMMinimumPlayerDistance
bool EERMArePlayersConnected(EERandomMapData& inMap, EEMap* inGameMap)
{
    bool theResult = true;
    unsigned long i;
    for (i = 0; i < inMap.mPlayers.size() && theResult; i++) {
        if (inMap.mPlayers[i].mIsNeutral)
            continue;
        unsigned long x = UMFloor(inMap.mPlayers[i].mPosition.GetX() + inMap.mFlatArea.GetLeft());
        unsigned long theBottom = UMFloor(inMap.mPlayers[i].mPosition.GetY() + inMap.mFlatArea.GetBottom());
        unsigned long theRight = UMFloor(inMap.mPlayers[i].mPosition.GetX() + inMap.mFlatArea.GetRight()) + 1;
        unsigned long theTop = UMFloor(inMap.mPlayers[i].mPosition.GetY() + inMap.mFlatArea.GetTop()) + 1;
        // every column of the flat area runs the full y range
        for (; x < theRight && theResult; x++) {
            for (unsigned long y = theBottom; y < theTop && theResult; y++) {
                EEMapTile* theTile = inGameMap->GetTile(x, y);
                theResult = theTile && theTile->GetHeight() >= 0.0f && theTile->mTerrainType != kTerrainSpace;
            }
        }
    }
    for (i = 0; i < inMap.mPlayers.size() - 1 && theResult; i++) {
        if (inMap.mPlayers[i].mIsNeutral)
            continue;
        for (unsigned long j = i + 1; j < inMap.mPlayers.size() && theResult; j++) {
            if (!inMap.mPlayers[j].mIsNeutral) {
                float theDistance = inMap.mPlayers[i].mPosition.DistanceToSquared(inMap.mPlayers[j].mPosition);
                theResult = theDistance >= kRMMinimumPlayerDistance * kRMMinimumPlayerDistance;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x6849f2
// Places each player's start units (citizens up to the player's limit, ring units on random free tiles) and walls,
// then the herds as nature units; resource units paint their resource terrain under them
void EERandomMap::PlaceFeatures()
{
    GE2DPoint theStart;
    GE2DPoint theEnd;
    EERMNamedEntry theEntry;
    EERMFeature theFeature;
    unsigned long theNumPlayers = this->mData.mPlayers.size();
    unsigned long theNumFeatures = this->mData.mFeatures.size();
    unsigned long theNumWalls = this->mData.mWalls.size();
    EEUnitClass* theClass;
    EEUnit* theUnit;
    unsigned long j;
    for (unsigned long i = 0; i < theNumPlayers; i++) {
        if (this->mData.mPlayers[i].mIsNeutral)
            continue;
        this->mData.mPlayers[i];
        float thePlayerX = this->mData.mPlayers[i].mPosition.GetX();
        float thePlayerY = this->mData.mPlayers[i].mPosition.mY;
        long thePlayer = this->mData.mPlayers[i].mIndex;
        unsigned long theCitizenLimit = (*this->mGamePlayers)[thePlayer]->mMaxStartingCitizens;
        unsigned long theCitizens = 0;
        for (j = 0; j < theNumFeatures; j++) {
            theFeature = this->mData.mFeatures[j];
            theEntry = this->mData.mUnitNames[theFeature.mUnitType];
            if (theFeature.mIsHerd || theEntry.mUnitClass >= EEUnitClassCount())
                continue;
            theClass = gUnitClassTable[theEntry.mUnitClass];
            if (dynamic_cast<EEUCLandCitizen*>(theClass)) {
                theCitizens++;
                if (theCitizens > theCitizenLimit)
                    theClass = NULL;
            }
            if (theClass == NULL)
                continue;
            float theX;
            float theY;
            if (!this->mData.mFeatures[j].mInRing) {
                theX = theFeature.mPosition.GetX() + thePlayerX;
                theY = theFeature.mPosition.GetY() + thePlayerY;
            } else {
                theX = 0.0f;
                unsigned long theTry = 0;
                theY = 0.0f;
                if (theTry < this->mData.mMaxLoopCount) {
                    float theRange = theFeature.mPosition.GetY() - theFeature.mPosition.GetX();
                    do {
                        float theAngle = this->mData.mRandom.GenerateUnit() * 6.2831855f - 3.1415927f;
                        float theRadius = this->mData.mRandom.GenerateUnit() * theRange + theFeature.mPosition.GetX();
                        theX = UMCos(theAngle) * theRadius + thePlayerX;
                        theY = UMSin(theAngle) * theRadius + thePlayerY;
                        long theTileX = UMFloor(theX);
                        long theTileY = UMFloor(theY);
                        EEMapTile* theTile = this->mMap->GetTile(theTileX, theTileY);
                        if (theTile && !this->mMap->IsEdgeTile(theTileX, theTileY) &&
                            theClass->mTerrainTypes[theTile->mTerrainType] &&
                            EERMIsAreaFree(this->mData.mValidTiles, this->mData.mClaimedTiles, theTileX, theTileY, theClass->GetTileSize()) &&
                            !EERMIsNearEdge(this->mData.mValidTiles, theTileX, theTileY, 1)) {
                            EERMClaimArea(this->mData.mClaimedTiles, theTileX, theTileY, theClass->GetTileSize());
                            theX = theTileX + 0.5f;
                            theY = theTileY + 0.5f;
                            break;
                        }
                        theTry++;
                    } while (theTry < this->mData.mMaxLoopCount);
                }
                if (theTry == this->mData.mMaxLoopCount)
                    theX = theY = -1.0f;
            }
            if (this->mMap->GetTile(UMFloor(theX), UMFloor(theY)) == NULL ||
                this->mMap->IsEdgeTile(UMFloor(theX), UMFloor(theY)))
                continue;
            EEPlayer* theOwner = this->mData.mFeatures[j].mIsWorldUnit ? (*this->mGamePlayers)[0] : (*this->mGamePlayers)[thePlayer];
            theUnit = theClass->CreateUnitAtPoint(theOwner, theX, theY, this->mMap, theFeature.mAngle);
            if (theUnit == NULL)
                continue;
            if (theUnit->mComplexUnit) {
                theUnit->mComplexUnit->SetBuilt(1, 0, 0);
                EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
                if (theComplexUnit->mIsPaidFor != true)
                    theComplexUnit->mIsPaidFor = true;
            } else if (theUnit->mResourceType >= kResourceFood && theUnit->mResourceType < 7 && theUnit->mResourceAmount > 0) {
                long theTexture;
                switch (theUnit->mResourceType) {
                case kResourceFood: theTexture = this->mData.mResourceTerrainTexture[3]; break;
                case kResourceWood: theTexture = this->mData.mResourceTerrainTexture[2]; break;
                case kResourceStone: theTexture = this->mData.mResourceTerrainTexture[0]; break;
                case kResourceGold: theTexture = this->mData.mResourceTerrainTexture[1]; break;
                case kResourceIron: theTexture = this->mData.mResourceTerrainTexture[6]; break;
                default: continue;
                }
                if (theTexture != -1)
                    this->mMap->PaintTerrain(UMFloor(theX), UMFloor(theY), theTexture);
            }
        }
        for (j = 0; j < theNumWalls; j++) {
            theEntry = this->mData.mUnitNames[this->mData.mWalls[j].mUnitType];
            if (theEntry.mUnitClass >= EEUnitClassCount())
                continue;
            theClass = gUnitClassTable[theEntry.mUnitClass];
            if (theClass == NULL || !theClass->mIsWall)
                continue;
            theStart.SetX((float)(UMFloor(thePlayerX) + this->mData.mWalls[j].mStartX) + 0.5f);
            theStart.SetY((float)(UMFloor(thePlayerY) + this->mData.mWalls[j].mStartY) + 0.5f);
            theEnd.SetX((float)(UMFloor(thePlayerX) + this->mData.mWalls[j].mEndX) + 0.5f);
            theEnd.SetY((float)(UMFloor(thePlayerY) + this->mData.mWalls[j].mEndY) + 0.5f);
            this->PlaceWall(theClass, theStart, theEnd, (*this->mGamePlayers)[thePlayer]);
        }
    }
    for (j = 0; j < theNumFeatures; j++) {
        theFeature = this->mData.mFeatures[j];
        theEntry = this->mData.mUnitNames[theFeature.mUnitType];
        if (theFeature.mIsHerd != true || theEntry.mUnitClass >= EEUnitClassCount())
            continue;
        theClass = gUnitClassTable[theEntry.mUnitClass];
        if (theClass == NULL)
            continue;
        if (this->mMap->GetTile(UMFloor(theFeature.mPosition.GetX()), UMFloor(theFeature.mPosition.GetY())) == NULL)
            continue;
        if (this->mMap->IsEdgeTile(UMFloor(theFeature.mPosition.GetX()), UMFloor(theFeature.mPosition.GetY())))
            continue;
        theUnit = theClass->CreateUnitAtPoint(this->mGamePlayers->front(), theFeature.mPosition.GetX(),
                                      theFeature.mPosition.GetY(), this->mMap, theFeature.mAngle);
        if (theUnit == NULL)
            continue;
        if (theUnit->mComplexUnit) {
            theUnit->mComplexUnit->SetBuilt(1, 0, 0);
            EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
            if (theComplexUnit->mIsPaidFor != true)
                theComplexUnit->mIsPaidFor = true;
        } else if (theUnit->mResourceType >= kResourceFood && theUnit->mResourceType < 7 && theUnit->mResourceAmount > 0) {
            long theTexture;
            switch (theUnit->mResourceType) {
            case kResourceFood: theTexture = this->mData.mResourceTerrainTexture[3]; break;
            case kResourceWood: theTexture = this->mData.mResourceTerrainTexture[2]; break;
            case kResourceStone: theTexture = this->mData.mResourceTerrainTexture[0]; break;
            case kResourceGold: theTexture = this->mData.mResourceTerrainTexture[1]; break;
            case kResourceIron: theTexture = this->mData.mResourceTerrainTexture[6]; break;
            default: continue;
            }
            if (theTexture != -1)
                this->mMap->PaintTerrain(UMFloor(theFeature.mPosition.GetX()), UMFloor(theFeature.mPosition.GetY()),
                               theTexture);
        }
    }
}

// FUNCTION: EXE 0x685560
// Sets up and generates the map from a generator and seed, and returns the player's start point (the centre for 0)
void EERandomMap::CreateRandomMap(unsigned long inPlayer, unsigned long inGenerator, unsigned long inSeed,
                                  EEMap* inMap, GE3DPoint& outLocation)
{
    unsigned long theStart = TMGetTime();
    this->SetUp(inGenerator, inMap, inSeed);
    this->Generate();
    if (inPlayer > 0) {
        outLocation.SetX(this->mData.mPlayers[inPlayer - 1].mPosition.GetX() + 0.5f);
        outLocation.SetY(this->mData.mPlayers[inPlayer - 1].mPosition.GetY() + 0.5f);
        outLocation.SetZ(this->mMap->GetTile(UMFloorUnsigned(outLocation.GetX()), UMFloorUnsigned(outLocation.GetY()))->GetHeight());
    } else {
        outLocation.SetX((inMap->mSize + 1.0f) * 0.5f);
        outLocation.SetY((inMap->mSize + 1.0f) * 0.5f);
        outLocation.SetZ(this->mMap->GetTile(inMap->mSize / 2, inMap->mSize / 2)->GetHeight());
    }
    ESDebugOut("EERandomMap::CreateRandomMap processing completed in %6.2f s\n", (TMGetTime() - theStart) * 0.001f);
}

// FUNCTION: EXE 0x6856e7
// Groups the players into teams: each player not yet placed starts a team with the later players it is not at war with
void EERandomMap::CreateTeams()
{
    std::vector<unsigned char> theAssigned;
    EERMTeam theTeam;
    this->mData.mTeams.resize(0);
    theAssigned.resize(this->mGamePlayers->size(), 0);
    std::fill(theAssigned.begin(), theAssigned.end(), (unsigned char)false);
    for (unsigned long i = 1; i < this->mGamePlayers->size(); i++) {
        if (theAssigned[i])
            continue;
        theTeam.mPlayers.clear();
        theTeam.mPositions.clear();
        theTeam.mPlayers.push_back(i);
        theTeam.mPositions.push_back(GE2DPoint());
        theAssigned[i] = true;
        for (unsigned long j = i + 1; j < this->mGamePlayers->size(); j++) {
            if (theAssigned[j])
                continue;
            unsigned long theID = (*this->mGamePlayers)[j]->GetPlayerID();
            if ((*this->mGamePlayers)[i]->mEnemies[theID] == 0) {
                theTeam.AddPlayer(j);
                theAssigned[j] = true;
            }
        }
        this->mData.mTeams.push_back(theTeam);
    }
}

// FUNCTION: EXE 0x685907
// Adds a player to the team, with an unset position
void EERMTeam::AddPlayer(long inPlayer)
{
    this->mPlayers.push_back(inPlayer);
    this->mPositions.push_back(GE2DPoint());
}

// FUNCTION: EXE 0x685933
// Marks which grid points are valid terrain-mesh points, logging the count
void EERandomMap::CreateValidMap()
{
    unsigned long theCount = 0;
    GETerrainMesh& theMesh = this->mMap->mTerrainMesh;
    for (unsigned long x = 0; x < this->mData.mGridSize + 1; x++) {
        for (unsigned long y = 0; y < this->mData.mGridSize + 1; y++) {
            this->mData.mValidTiles[x][y] = theMesh.GetIsValidPoint(x, y);
            if (this->mData.mValidTiles[x][y])
                theCount++;
        }
    }
    ESDebugOut("EERandomMap::CreateValidMap %ld valid tiles found\n", theCount);
}

// FUNCTION: EXE 0x6859e7
// Builds a player's wall along a line: drops cliff and edge tiles, places shaped pieces on free dry tiles and joins
// the player's fortifications already on the line
void EERandomMap::PlaceWall(EEUnitClass* inClass, const GE2DPoint& inStart, const GE2DPoint& inEnd,
                            EEPlayer* inPlayer)
{
    std::vector<GE2DPoint> thePoints;
    std::vector<EETileCoordinate> theTiles;
    std::vector<long> theCorners;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EETileCoordinate> thePlaced;
    unsigned long i = 0;
    EETileCoordinate theTile;
    if (inStart != inEnd) {
        inStart.InterpolateTo(inEnd, 1.0f, thePoints);
        for (; i < thePoints.size(); i++) {
            const GE2DPoint& thePoint = thePoints[i];
            theTile.mX = (long)thePoint.GetX(); theTile.mY = (long)thePoint.GetY();
            theTiles.push_back(theTile);
        }
        EEDropDiagonalCorners(theTiles);
        i = 0;
    } else {
        theTile.mX = (long)inStart.GetX();
        theTile.mY = (long)inStart.mY;
        theTiles.push_back(theTile);
    }
    EEDropRepeatedTiles(theTiles);
    for (unsigned long j = i; j < theTiles.size(); j++) {
        long theX = theTiles[j].GetX();
        long theY = theTiles[j].mY;
        EEMapTile* theMapTile = this->mMap->GetTile(theX, theY);
        if (theMapTile == NULL || theMapTile->mTerrainType == kTerrainCliff || this->mMap->IsEdgeTile(theX, theY)) {
            theTiles.erase(theTiles.begin() + j);
            j--;
        }
        i = 0;
    }
    EEPickWallPieces(theTiles, theCorners, this->mMap);
    for (unsigned long k = i; k < theTiles.size(); k++) {
        long theX = theTiles[k].GetX();
        long theY = theTiles[k].GetY();
        EEMapTile* theMapTile = this->mMap->GetTile(theX, theY);
        if (theMapTile == NULL)
            continue;
        if (theMapTile->mUnits == NULL) {
            if (theMapTile->GetTerrainType() == kTerrainDeepWater || theMapTile->GetTerrainType() == kTerrainShallowWater)
                continue;
            EEComplexUnit* theUnit = inClass->CreateUnitAtTile(inPlayer, theX, theY, this->mMap, 3.1415927f)->GetComplexUnit();
            theUnit->SetBuilt(1, 0, 0);
            this->mMap->GetTile(theX, theY);
            theTile.SetX(theX);
            theTile.SetY(theY);
            thePlaced.push_back(theTile);
            long theRaised = this->mMap->GetNumRaisedCorners(theX, theY);
            EEShapeWall(theUnit, theCorners[k], theRaised);
        } else {
            EEUnit* theOther = theMapTile->mUnits->mUnit;
            EEComplexUnit* theComplexUnit = theOther->GetComplexUnit();
            if (theOther->GetPlayer() == inPlayer && theOther->GetUnitClass()->mAIKind == kAIKindFortification && theComplexUnit &&
                theComplexUnit->GetAnimation() != 0xb)
                theUnits.push_back(theComplexUnit);
        }
    }
    EEMakeWallJunctions(theUnits, thePlaced, inPlayer, this->mMap);
}

// FUNCTION: EXE 0x685d89
// Runs the generator's height-map filter when the script asks for it; returns its result
long EERandomMap::FilterHeightMap()
{
    long theResult = 0;
    if (this->mData.mFilterHeightMap)
        theResult = this->mFilterHeightMapPIM(&this->mData);
    return theResult;
}

// FUNCTION: EXE 0x685d9f
// Smooths each height toward its neighbours' mean and keeps edge (space maps: empty) cells at sea level, then
// raises the low corners of tiles straddling the sea level
void EERandomMap::SmoothHeights()
{
    float theRange = (float)(this->mData.mMaxIntElevation - this->mData.mMinIntElevation);
    float theScale = this->mMap->GetF1955F4();
    float theMinimum = (float)this->mData.mMinIntElevation;
    float theSeaLevel = -(theMinimum / theRange);
    long x;
    long y;
    for (x = 0; x < (long)this->mData.mGridSize; x++) {
        for (y = 0; y < (long)this->mData.mGridSize; y++) {
            if (!this->mData.mValidTiles[x][y])
                continue;
            float theHeight = this->mData.mHeights[x][y];
            float theSum = 0.0f;
            long theCount = 0;
            float theLow;
            float theHigh;
            this->AddNeighbourHeight(x - 1, y - 1, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x - 1, y, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x - 1, y + 1, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x, y - 1, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x, y + 1, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x + 1, y - 1, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x + 1, y, theSum, theCount, theLow, theHigh);
            this->AddNeighbourHeight(x + 1, y + 1, theSum, theCount, theLow, theHigh);
            theSum = theSum / theCount;
            theHeight = (theHeight * theRange + theMinimum) * theScale;
            if (UMAbs(theHeight - (theSum * theRange + theMinimum) * theScale) >= theScale * this->mData.mHeightMapSmoothness)
                this->mData.mHeights[x][y] = theSum;
            if (this->mData.mIsSpaceMap) {
                // the normalised height, like the sea level (theHeight is in world units)
                if (this->mData.mHeights[x][y] < theSeaLevel && EERMIsTileFree(this->mData.mTiles, x, y))
                    this->mData.mHeights[x][y] = theSeaLevel;
            } else {
                if (this->mMap->GetTile(x, y) && this->mMap->GetTile(x - 1, y) && this->mMap->GetTile(x - 1, y - 1) &&
                    this->mMap->GetTile(x, y - 1))
                    continue;
                if (this->mData.mHeights[x][y] < theSeaLevel)
                    this->mData.mHeights[x][y] = theSeaLevel;
            }
        }
    }
    for (x = 0; x < (long)this->mData.mGridSize; x++) {
        for (y = 0; y < (long)this->mData.mGridSize; y++) {
            if (!this->mData.mValidTiles[x][y] || this->mMap->GetTile(x, y) == NULL)
                continue;
            bool theLow = false;
            bool theHigh = false;
            float theHeight = this->mData.mHeights[x][y];
            if (theHeight > theSeaLevel) theHigh = true;
            else if (theHeight < theSeaLevel + 0.0001f)
                theLow = true;
            theHeight = this->mData.mHeights[x][y + 1];
            if (theHeight > theSeaLevel) theHigh = true;
            else if (theHeight < theSeaLevel + 0.0001f)
                theLow = true;
            theHeight = this->mData.mHeights[x + 1][y];
            if (theHeight > theSeaLevel) theHigh = true;
            else if (theHeight < theSeaLevel + 0.0001f)
                theLow = true;
            theHeight = this->mData.mHeights[x + 1][y + 1];
            if (theHeight > theSeaLevel)
                theHigh = true;
            else { if (theHeight < theSeaLevel + 0.0001f)
                    theLow = true;
                if (!theHigh)
                    continue;
            }
            if (!theLow)
                continue;
            if (this->mData.mHeights[x][y] < theSeaLevel)
                this->mData.mHeights[x][y] = theSeaLevel;
            if (this->mData.mHeights[x][y + 1] < theSeaLevel)
                this->mData.mHeights[x][y + 1] = theSeaLevel;
            if (this->mData.mHeights[x + 1][y] < theSeaLevel)
                this->mData.mHeights[x + 1][y] = theSeaLevel;
            if (this->mData.mHeights[x + 1][y + 1] < theSeaLevel)
                this->mData.mHeights[x + 1][y + 1] = theSeaLevel;
        }
    }
}

// FUNCTION: EXE 0x68655a
// Finds the named generator's PIM and generates the map, retrying with a new seed (11 tries at most) until the players
// are connected; returns the PIM search status
long EERandomMap::CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                  const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                  GE3DPoint& outLocation)
{
    PIMModule* theModule;
    unsigned long theAttempts = 0;
    long theWidth;
    long theHeight;
    inOptions->GetMapDimensions(theWidth, theHeight);
    unsigned long theGeneratorIndex = 0;
    while (theGeneratorIndex < EERandomMap::sRandomMapGenerators.size()) {
        if (PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)EERandomMap::sRandomMapGenerators[theGeneratorIndex].mName),
                     reinterpret_cast<const unsigned char*>((const char*)inGenerator)) == 0)
            break;
        theGeneratorIndex++;
    }
    ESVERIFY_AT(theGeneratorIndex < EERandomMap::sRandomMapGenerators.size(), 1340);
    long theResult = PIMModule::GetFirstModule(EERandomMap::sGeneratorDirectory, 'rmap', theModule);
    while (theResult == 0) {
        EERMGenerator& theGenerator = EERandomMap::sRandomMapGenerators.begin()[theGeneratorIndex];
        if (PlatStrICmp(theGenerator.mModuleName, theModule->GetName()) == 0) {
            bool theConnected;
            do {
                EERandomMap* theMap = new EERandomMap(theModule, inOptions, &inPlayers);
                theMap->CreateRandomMap(inPlayer, theGeneratorIndex, inSeed, inMap, outLocation);
                theConnected = EERMArePlayersConnected(theMap->mData, inMap);
                inSeed = theMap->mData.mRandomIntegers.Generate(0, 2000000);
                theAttempts++;
                delete theMap;
            } while (!theConnected && theAttempts <= 10);
            ESDebugOut("!Playable random map created after %ld attempts.\n", theAttempts);
            theModule->Unload();
            break;
        }
        theResult = PIMModule::GetNextModule(theModule);
    }
    return theResult;
}

// FUNCTION: EXE 0x686714
// Returns the number of random-map generators
unsigned long EERMGetNumberOfGenerators()
{
    return EERandomMap::sRandomMapGenerators.size();
}

// FUNCTION: EXE 0x68672f
// Returns a generator's display name by index
const UWideString& EERMGetGeneratorDisplayName(unsigned long inIndex)
{
    return EERandomMap::sRandomMapGenerators[inIndex].mDisplayName;
}

// FUNCTION: EXE 0x686741
// Returns the display name of the generator with the given name, or the name itself when none matches
const UWideString& EERMGetGeneratorDisplayName(const UWideString& inName)
{
    unsigned long i; for (i = 0; i < EERandomMap::sRandomMapGenerators.size(); i++) {
        if (inName == EERandomMap::sRandomMapGenerators[i].mName)
            break;
    }
    if (i < EERandomMap::sRandomMapGenerators.size())
        return EERandomMap::sRandomMapGenerators[i].mDisplayName;
    return inName;
}

// FUNCTION: EXE 0x6867a8
// Returns a generator's name by index
const UWideString& EERMGetGeneratorName(unsigned long inIndex)
{
    return EERandomMap::sRandomMapGenerators[inIndex].mName;
}

// FUNCTION: EXE 0x6867b6
// Finds a generator by name and returns its index; returns 3 when there is none
long EERMFindGenerator(const UWideString& inName, unsigned long& outIndex)
{
    long theResult = 3;
    for (unsigned long i = 0; i < EERandomMap::sRandomMapGenerators.size(); i++) {
        if (inName == EERandomMap::sRandomMapGenerators[i].mName) {
            theResult = 0;
            outIndex = i;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x686804
// Lists the random-map generators: each .rmv script naming an installed rmap PIM, with its string-table display name,
// sorted; asserts that both exist
long EERandomMap::Initialize(const FSFileSpec& inDirectory)
{
    std::vector<UString> theValidPlugIns;
    FSFileSpec theDirectory;
    FSFileSpec theFile;
    std::vector<UString> theTokens;
    UString theQuote("\"");
    UString theSeparators(" \t\n,");
    UString theBrackets("()");
    UString theLine(0x400);
    UString theName;
    EERMGenerator theGenerator;
    PIMModule* theModule;
    EERandomMap::sRandomMapGenerators.clear();
    EERandomMap::sGeneratorDirectory = inDirectory;
    for (long theResult = PIMModule::GetFirstModule(inDirectory, 'rmap', theModule); theResult == 0;
         theResult = PIMModule::GetNextModule(theModule)) {
        theValidPlugIns.push_back(theModule->GetName());
        ESDebugOut("EERandomMap::Initialize: %s random map generator detected.\n", (const char*)theModule->GetName());
        theModule->Unload();
    }
    ESVERIFY_AT(theValidPlugIns.size() > 0, 1571);

    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\\Random Map Scripts\\");
    if (theDirectory.GetFirstFile("*.rmv", theFile) == 0) {
        do {
            if (theFile.FOpen("r") != 0)
                continue;
            FILE* theStream = theFile;
            bool theHasStringID = false;
            bool theHasGenerator = false;
            long theStringID = 0;
            char theBuffer[1024];
            while (fgets(theBuffer, 0x400, theStream) != NULL) {
                theLine = theBuffer;
                theLine.Tokenize(theQuote, theTokens);
                if (theTokens.size() >= 2 &&
                    theTokens[0].GetSubstring(0, kUseMapGeneratorKey.GetLength()) == kUseMapGeneratorKey) {
                    theName = theFile.GetFile();
                    theName.ToLower();
                    theGenerator.mName = theFile.GetFile().GetSubstring(0, theName.FindBackwards(".rmv", -1));
                    theGenerator.mModuleName = theTokens[1];
                    theHasGenerator = true;
                }
                theLine.NestedTokenize(theSeparators, theBrackets, theTokens);
                if (theTokens.size() >= 2 &&
                    theTokens[0].GetSubstring(0, kStringTableIDKey.GetLength()) == kStringTableIDKey) {
                    sscanf(theTokens[1], "%ld", &theStringID);
                    theHasStringID = true;
                }
            }
            theFile.Close();
            if (theHasGenerator &&
                std::find(theValidPlugIns.begin(), theValidPlugIns.end(), theGenerator.mModuleName) !=
                    theValidPlugIns.end()) {
                if (!theHasStringID || gResource->GetString(theStringID, theGenerator.mDisplayName) != 0)
                    theGenerator.mDisplayName = theGenerator.mName;
                EERandomMap::sRandomMapGenerators.push_back(theGenerator);
            }
        } while (theDirectory.GetNextFile(theFile) == 0);
    }
    ESVERIFY_AT(EERandomMap::sRandomMapGenerators.size() > 0, 1652);
    // a predicate: the sort COMDATs pass a 4-byte P (0x68b733 _Unguarded_insert, _Adjust_heap); provisional name
    std::sort(EERandomMap::sRandomMapGenerators.begin(), EERandomMap::sRandomMapGenerators.end(), std::less<EERMGenerator>());
    return 0;
}

// SYNTHETIC: EXE 0x686d4f
// (EERMGenerator's implicit default constructor)

// SYNTHETIC: EXE 0x686d6e
// (EERMGenerator's implicit destructor)

// FUNCTION: EXE 0x686d8d
// Fills the generator state from the game: seeds, map dimensions and size class, players, start epoch, and the unit,
// terrain and cliff names (spaces removed) the scripts refer to
void EERandomMap::InitializeValues(unsigned long inSeed)
{
    std::vector<UString> theTokens;
    UString theSeparator(" ");
    UString theUnitName;
    UString theName;
    EERMNamedValue theCliff;
    EERMNamedValue theTerrain;
    EERMNamedEntry theUnit;
    unsigned long i;
    unsigned long j;
    this->mData.mSeed = inSeed;
    this->mData.mRandom.mSeed = inSeed;
    this->mData.mRandomIntegers.mSeed = inSeed;
    this->mData.mMaximumY = this->mMap->mL195610 << 1;
    this->mData.mMaximumX = this->mMap->mL195614 << 1;
    FSFileSpec& theDirectory = this->mData.mScriptDirectory;
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\\Random Map Scripts\\");
    this->mData.mTileCount = this->mMap->GetTileCount();
    this->mData.mClumpCircles.resize(0);
    this->mData.mPlayerCount = this->mGamePlayers->size() - 1;
    this->mData.mStartEpoch = this->mOptions->mEpochStart;
    unsigned long theHeight = this->mMap->mL195610;
    {   // the scripts' size class: that of the smallest map size (gameoptions.yaml) at least this high
        long theBest = -1;
        for (const EEMapSizeRule& theSize : EEGetRules().mGameOptions.mMapSizes)
            if ((unsigned long)theSize.mTiles >= theHeight &&
                (theBest < 0 || theSize.mTiles < EEGetRules().mGameOptions.mMapSizes[theBest].mTiles))
                theBest = &theSize - &EEGetRules().mGameOptions.mMapSizes[0];
        if (theBest >= 0)
            this->mData.mMapSize = EEGetRules().mGameOptions.mMapSizes[theBest].mScriptSize;
        else
            ESVERIFY_AT(false, 1735);
    }
    for (i = 0; i < EEUnitClassCount(); i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass == NULL || !theClass->mIsEnabled)
            continue;
        theClass->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theUnitName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theUnitName += theTokens[j];
        theUnit.mName = theUnitName;
        theUnit.mUnitClass = i;
        theUnit.mSize = theClass->mTileSize <= 1 ? 1 : theClass->mTileSize;
        this->mData.mUnitNames.push_back(theUnit);
    }
    this->mData.mFeatures.clear();
    this->mData.mTerrainNames.clear();
    for (i = 0; i < EETerrainTable::GetNumTables(); i++) {
        EETerrainTable* theTable = EETerrainTable::Get(i);
        if (theTable == NULL)
            continue;
        theTable->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theName += theTokens[j];
        theTerrain.mName = theName;
        theTerrain.mValue = i;
        this->mData.mTerrainNames.push_back(theTerrain);
        if (PlatStrICmp(theName, "Space") == 0)
            gSpaceTerrain = i;
    }
    std::vector<EERMNamedValue>& theCliffs = this->mData.mCliffNames;
    theCliffs.clear();
    for (i = 0; i < EETerrainTable::GetNumCliffTerrains(); i++) {
        EECliffTerrainTable* theTable = EEGetCliffTerrainTable(i);
        if (theTable == NULL)
            continue;
        theTable->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theName += theTokens[j];
        theCliff.mName = theName;
        theCliff.mValue = i;
        theCliffs.push_back(theCliff);
    }
}

// FUNCTION: EXE 0x6871fa
// Sizes the generator grids to the power of two above the map (64 to 1024), resets them to the initial elevation and
// sets up the player, neutral and moon rings
void EERandomMap::CreateGrids()
{
    unsigned long theSize = this->mMap->mL195610 * 2;
    unsigned long theOther = this->mMap->mL195614 * 2;
    if (theOther > theSize)
        theSize = theOther;
    if (theSize < 0x40)
        this->mData.mGridSize = 0x40;
    else if (theSize < 0x80)
        this->mData.mGridSize = 0x80;
    else if (theSize < 0x100)
        this->mData.mGridSize = 0x100;
    else if (theSize < 0x200)
        this->mData.mGridSize = 0x200;
    else if (theSize < 0x400)
        this->mData.mGridSize = 0x400;
    this->mData.mClaimedTiles.resize(this->mData.mGridSize + 1);
    this->mData.mTiles.resize(this->mData.mGridSize + 1);
    this->mData.mHeights.resize(this->mData.mGridSize + 1);
    this->mData.mFlatTiles.resize(this->mData.mGridSize + 1);
    this->mData.mValidTiles.resize(this->mData.mGridSize + 1);
    unsigned long x;
    for (x = 0; x < this->mData.mGridSize + 1; x++) {
        this->mData.mClaimedTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mHeights[x].resize(this->mData.mGridSize + 1, 0.0f);
        this->mData.mFlatTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mValidTiles[x].resize(this->mData.mGridSize + 1, 0);
    }
    for (x = 0; x < this->mData.mGridSize + 1; x++) {
        for (unsigned long y = 0; y < this->mData.mGridSize + 1; y++) {
            this->mData.mClaimedTiles[x][y] = 0;
            this->mData.mTiles[x][y] = 0;
            this->mData.mHeights[x][y] = this->mData.mInitialElevation;
            this->mData.mFlatTiles[x][y] = 0;
            this->mData.mValidTiles[x][y] = 0;
        }
    }
    float theExtent = UMAbs(this->mData.mFlatArea.GetLeft());
    if (UMAbs(this->mData.mFlatArea.GetRight()) > theExtent)
        theExtent = UMAbs(this->mData.mFlatArea.GetRight());
    if (UMAbs(this->mData.mFlatArea.GetBottom()) > theExtent)
        theExtent = UMAbs(this->mData.mFlatArea.GetBottom());
    if (UMAbs(this->mData.mFlatArea.GetTop()) > theExtent)
        theExtent = UMAbs(this->mData.mFlatArea.GetTop());
    float theRadius = ((float)this->mMap->mSize - theExtent) * kRMRingScale;
    this->mData.mHalfMapSize = (float)this->mMap->mSize * 0.5f;
    float theCenter = 0.5f * (float)this->mMap->mSize;
    this->mData.mRing.mCenter = GE2DPoint(theCenter, theCenter);
    this->mData.mRing.mMinRadius = theRadius * this->mData.mPlayerInnerRadius;
    this->mData.mRing.mMaxRadius = theRadius * this->mData.mPlayerOuterRadius;
    this->mData.mNeutralRing.mCenter = this->mData.mRing.mCenter;
    this->mData.mNeutralRing.mMinRadius = theRadius * this->mData.mNeutralInnerRadius;
    this->mData.mNeutralRing.mMaxRadius = theRadius * this->mData.mNeutralOuterRadius;
    this->mData.mMoonRing.mCenter = this->mData.mRing.mCenter;
    this->mData.mMoonRing.mMaxRadius = this->mData.mHalfMapSize;
    this->mData.mMoonRing.mMinRadius = (this->mData.mHalfMapSize - this->mData.mRing.mMaxRadius) * 0.5f + this->mData.mRing.mMaxRadius;
}

// FUNCTION: EXE 0x687636
// Claims the grid cells of every player's start units and walls, and of every cliff tile
void EERandomMap::MarkFeatures()
{
    GE2DPoint theStart;
    GE2DPoint theEnd;
    EERMNamedEntry theEntry;
    unsigned long theNumPlayers = this->mData.mPlayers.size();
    unsigned long theNumFeatures = this->mData.mFeatures.size();
    unsigned long theNumWalls = this->mData.mWalls.size();
    for (unsigned long i = 0; i < theNumPlayers; i++) {
        if (this->mData.mPlayers[i].mIsNeutral)
            continue;
        float thePlayerX = this->mData.mPlayers[i].mPosition.GetX();
        float thePlayerY = this->mData.mPlayers[i].mPosition.mY;
        unsigned long j;
        for (j = 0; j < theNumFeatures; j++) {
            EERMFeature theFeature = this->mData.mFeatures[j];
            theEntry = this->mData.mUnitNames[theFeature.mUnitType];
            unsigned long theY = UMFloor(theFeature.mPosition.GetY() + thePlayerY);
            unsigned long theX = UMFloor(theFeature.mPosition.GetX() + thePlayerX);
            EERMClaimArea(this->mData.mClaimedTiles, theX, theY, theEntry.mSize);
        }
        for (j = 0; j < theNumWalls; j++) {
            theStart.SetX((float)this->mData.mWalls[j].mStartX + thePlayerX + 0.5f);
            theStart.SetY((float)this->mData.mWalls[j].mStartY + thePlayerY + 0.5f);
            theEnd.SetX((float)this->mData.mWalls[j].mEndX + thePlayerX + 0.5f);
            theEnd.SetY((float)this->mData.mWalls[j].mEndY + thePlayerY + 0.5f);
            this->MarkLine(theStart, theEnd);
        }
    }
    for (float x = 0.0f; x < (float)this->mData.mGridSize; x += 1.0f) {
        for (float y = 0.0f; y < (float)this->mData.mGridSize; y += 1.0f) {
            EEMapTile* theTile = this->mMap->GetTile(UMFloor(x), UMFloor(y));
            if (theTile && theTile->mTerrainType == kTerrainCliff)
                this->mData.mClaimedTiles[UMFloor(x)][UMFloor(y)] = true;
        }
    }
}

// FUNCTION: EXE 0x687a3f
// Marks a wall line's tiles as land and claimed on the grid
void EERandomMap::MarkLine(const GE2DPoint& inStart, const GE2DPoint& inEnd)
{
    std::vector<GE2DPoint> thePoints;
    std::vector<EETileCoordinate> theTiles;
    EETileCoordinate theTile;
    unsigned long i = 0;
    if (inStart != inEnd) {
        inStart.InterpolateTo(inEnd, 1.0f, thePoints);
        for (; i < thePoints.size(); i++) {
            const GE2DPoint& thePoint = thePoints[i];
            theTile.mX = (long)thePoint.GetX();
            theTile.mY = (long)thePoint.GetY();
            theTiles.push_back(theTile);
        }
        EEDropDiagonalCorners(theTiles);
    } else {
        theTile.mX = (long)inStart.GetX();
        theTile.mY = (long)inStart.GetY();
        theTiles.push_back(theTile);
    }
    EEDropRepeatedTiles(theTiles);
    for (unsigned long j = 0; j < theTiles.size(); j++) {
        long theX = theTiles[j].GetX();
        long theY = theTiles[j].GetY();
        if (this->mMap->GetTile(theX, theY)) {
            this->mData.mTiles[theX][theY] = 0x400;
            this->mData.mClaimedTiles[theX][theY] = true;
        }
    }
}

// FUNCTION: EXE 0x687c18
// May place one of the terrain's ambient units (by its chance) on an unclaimed tile away from the edge, owned by nature
void EERandomMap::PlaceTerrainUnit(unsigned long inX, unsigned long inY, EETerrainTable* inTerrain)
{
    if (!(inTerrain->GetNumUnitClasses() > 0) || this->mData.mClaimedTiles[inX][inY] ||
        !(this->mData.mRandom.Generate() <= inTerrain->mAmbientUnitChance * kHeightEpsilon) ||
        EERMIsNearEdge(this->mData.mValidTiles, inX, inY, this->mData.mIsSpaceMap ? 3 : 1))
        return;
    long theIndex = this->mData.mRandomIntegers.Generate(0, inTerrain->GetNumUnitClasses() - 1);
    EEUnitClass* theClass = gUnitClassTable[inTerrain->GetUnitClass(theIndex)];
    if (theClass) {
        theClass->CreateUnitAtTile(this->mGamePlayers->front(), inX, inY, this->mMap, 0.0f);
        this->mData.mClaimedTiles[inX][inY] = true;
    }
}

// FUNCTION: EXE 0x687d37
// Paints every land tile, then every water tile, with the terrain its fractal value (plus cliff, Mars or band offset)
// selects by elevation, and may add that terrain's ambient unit
void EERandomMap::PaintTiles()
{
    U2DFractal theFractal(this->mData.mGridSize, 0.0f, this->mData.mTerrainChaos, this->mData.mSeed);
    GETerrainMesh* theMesh = &this->mMap->mTerrainMesh;
    unsigned long theStart = TMGetTime();
    ESDebugOut("EERandomMap::PaintTiles begin of function\n");
    unsigned long x;
    unsigned long y;
    unsigned long i;
    unsigned long theTerrain;
    unsigned long theValue;
    for (x = 0; x < this->mData.mGridSize; x++) {
        for (y = 0; y < this->mData.mGridSize; y++) {
            EEMapTile* theTile = this->mMap->GetTile(x, y);
            if (theTile == NULL || theTile->GetHeight() < 0.0f || theTile->mTerrainType == kTerrainSpace)
                continue;
            theTerrain = this->mData.mTerrains.size();
            theValue = min(99, max(0, UMFloatToLong(theFractal.GetValue(x, y) * 100.0f)));
            if (EERMIsCliff(this->mData.mTiles, x, y)) {
                theValue += (((this->mData.mTiles[x][y] + 1) & 3) + 10) * 100;
            } else if ((this->mData.mMapCharacteristics & kRMMapMarsMap) == kRMMapMarsMap) {
                theValue += 100;
            } else if ((this->mData.mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
                theValue += ((this->mData.mTerrainBandOffset + this->mData.mTiles[x][y]) % this->mData.mNumTerrainBands) * 100;
            }
            for (i = 0; i < this->mData.mTerrainRegions.size(); i++) {
                if (theValue >= this->mData.mTerrainRegions[i].mFrom && theValue <= this->mData.mTerrainRegions[i].mTo) {
                    theTerrain = this->mData.mTerrainRegions[i].mTerrain;
                    break;
                }
            }
            if (theTerrain == this->mData.mTerrains.size() && this->mData.mTerrainRegions.size() != 0)
                theTerrain = this->mData.mTerrainRegions[0].mTerrain;
            if (theTerrain >= this->mData.mTerrains.size())
                continue;
            float theHeight = theTile->GetHeight();
            float theUnit = this->mMap->GetF1955F4();
            long theTexture = this->mData.mTerrains[theTerrain].mTextures[EERMGetElevation(theHeight / theUnit)];
            if (EETerrainTable::Get(theTexture)) {
                this->mMap->PaintTerrain(x, y, theTexture);
                this->PlaceTerrainUnit(x, y, EETerrainTable::Get(theTexture));
            }
        }
    }
    for (x = 0; x < this->mData.mGridSize; x++) {
        for (y = 0; y < this->mData.mGridSize; y++) {
            EEMapTile* theTile = this->mMap->GetTile(x, y);
            if (theTile == NULL || theTile->GetHeight() >= 0.0f)
                continue;
            theTerrain = this->mData.mTerrains.size();
            theValue = min(99, max(0, UMFloatToLong(theFractal.GetValue(x, y) * 100.0f)));
            for (i = 0; i < this->mData.mTerrainRegions.size(); i++) {
                if (theValue >= this->mData.mTerrainRegions[i].mFrom && theValue <= this->mData.mTerrainRegions[i].mTo) {
                    theTerrain = this->mData.mTerrainRegions[i].mTerrain;
                    break;
                }
            }
            if (theTerrain == this->mData.mTerrains.size() && this->mData.mTerrainRegions.size() != 0)
                theTerrain = this->mData.mTerrainRegions[0].mTerrain;
            if (theTerrain >= this->mData.mTerrains.size())
                continue;
            // the water tile's highest corner; a corner at or above the water line counts as one unit deep
            float theHighest = theMesh->GetZ(x, y);
            if (theMesh->GetZ(x + 1, y) > theHighest)
                theHighest = theMesh->GetZ(x + 1, y);
            if (theMesh->GetZ(x, y + 1) > theHighest)
                theHighest = theMesh->GetZ(x, y + 1);
            if (theMesh->GetZ(x + 1, y + 1) > theHighest)
                theHighest = theMesh->GetZ(x + 1, y + 1);
            if (theHighest >= 0.0f)
                theHighest = -this->mMap->GetF1955F4();
            long theTexture = this->mData.mTerrains[theTerrain].mTextures[EERMGetElevation(theHighest /
                                                                              this->mMap->GetF1955F4())];
            if (EETerrainTable::Get(theTexture)) {
                this->mMap->PaintTerrain(x, y, theTexture);
                this->PlaceTerrainUnit(x, y, EETerrainTable::Get(theTexture));
            }
        }
    }
    ESDebugOut("EERandomMap::PaintTiles Processing completed in %6.2f s\n", (TMGetTime() - theStart) * 0.001f);
}

// FUNCTION: EXE 0x688401
// Returns whether a grid cell is a cliff (value 0x8000 or more)
static bool EERMIsCliff(const std::vector<std::vector<unsigned long> >& inTiles, unsigned long inX, unsigned long inY)
{
    return inTiles[inX][inY] >= 0x8000;
}

// FUNCTION: EXE 0x688426
// Creates the generator's resource objects (stone, gold, trees, food, animals, kind 5, iron) as nature units and paints
// their resource terrain under them
void EERandomMap::Fn288426()
{
    unsigned long theStart = TMGetTime();
    for (std::vector<EERMObject>::iterator theObject = this->mData.mObjects.begin(); theObject < this->mData.mObjects.end(); theObject++) {
        long theTexture = this->mData.mResourceTerrainTexture[theObject->mKind];
        bool theRandomHeading = false;
        unsigned long theClass;
        switch (theObject->mKind) {
        case 0:
            theClass = 0x26;
            break;
        case 1:
            theClass = 0x32;
            break;
        case 2: {
            unsigned long theIndex = this->mData.mRandomIntegers.Generate(0, this->mData.mForests[theObject->mType].mTreeTypes.size() - 1);
            theClass = this->mData.mUnitNames[this->mData.mForests[theObject->mType].mTreeTypes[theIndex]].mUnitClass;
            theTexture = this->mData.mForests[theObject->mType].mTerrainTexture;
            break;
        }
        case 3:
            theClass = 0x37;
            break;
        case 4:
            if (this->mData.mAvailableAnimals.size() != 0) {
                unsigned long theIndex = this->mData.mRandomIntegers.Generate(0, this->mData.mAvailableAnimals.size() - 1);
                theClass = this->mData.mUnitNames[this->mData.mAvailableAnimals[theIndex]].mUnitClass;
                theRandomHeading = true;
            } else {
                theClass = 0;
            }
            theTexture = -1;
            break;
        case 5:
            theTexture = -1;
            theClass = 0x13b;
            break;
        case 6:
            theClass = 0xe0;
            break;
        default:
            continue;
        }
        if (theClass == 0 || theClass >= EEUnitClassCount())
            continue;
        unsigned long theX = UMFloor(theObject->mLocation.GetX());
        unsigned long theY = UMFloor(theObject->mLocation.GetY());
        EEUnitClass* theUnitClass = gUnitClassTable[theClass];
        EEMapTile* theTile = this->mMap->GetTile(theX, theY);
        if (theUnitClass == NULL || theTile == NULL || theTile->mTerrainType == kTerrainCliff)
            continue;
        float theHeading;
        if (theRandomHeading)
            theHeading = this->mData.mRandom.Generate() * 6.2831855f - 3.1415927f;
        else
            theHeading = 3.1415927f;
        theUnitClass->CreateUnitAtTile((*this->mGamePlayers)[0], theX, theY, this->mMap, theHeading);
        if (theTexture != -1 && theTile->mTerrainType != kTerrainSpace && this->mData.mTiles[theX][theY] < 0x8000)
            this->mMap->PaintTerrain(theX, theY, theTexture);
    }
    TMGetTime();
}

// FUNCTION: EXE 0x688742
// Prepares a generation: fills the state, applies the generator script's values, then builds the grids, valid map and
// teams; returns the script status
long EERandomMap::SetUp(unsigned long inGenerator, EEMap* inMap, unsigned long inSeed)
{
    UString theScript;
    this->mMap = inMap;
    this->InitializeValues(inSeed);
    theScript = EERandomMap::sRandomMapGenerators[inGenerator].mName;
    long theResult = this->mSetRandomMapValues(&this->mData, theScript);
    if (theResult == 0) {
        this->CreateGrids();
        this->CreateValidMap();
        this->CreateTeams();
    }
    return theResult;
}

// FUNCTION: EXE 0x6887bd
// Adds a valid grid cell's height to a running sum, count, minimum and maximum (cells outside the grid are skipped)
void EERandomMap::AddNeighbourHeight(unsigned long inX, unsigned long inY, float& ioSum, long& ioCount,
                                     float& ioMinimum, float& ioMaximum)
{
    if (!(inX < this->mData.mGridSize) || !(inY < this->mData.mGridSize) || !this->mData.mValidTiles[inX][inY])
        return;
    float theHeight = this->mData.mHeights[inX][inY];
    ioSum += theHeight;
    ioCount++;
    if (ioCount == 1) {
        ioMinimum = theHeight;
        ioMaximum = theHeight;
    } else {
        if (theHeight < ioMinimum)
            ioMinimum = theHeight;
        if (theHeight > ioMaximum)
            ioMaximum = theHeight;
    }
}

// FUNCTION: EXE 0x688839
// Applies the height grid to the terrain mesh (clamped, shallow dips flattened, empty space-map cells painted Space),
// then sets water, normals, tile heights and shorelines
void EERandomMap::Fn288839()
{
    GETerrainMesh* theMesh = &this->mMap->mTerrainMesh;

    float theRange = (float)(this->mData.mMaxIntElevation - this->mData.mMinIntElevation);
    float theScale = this->mMap->GetF1955F4();
    unsigned long theSize = this->mData.mMaximumY + 1;
    float theMinimum = (float)this->mData.mMinIntElevation;
    EETerrainTable* theTerrain = EETerrainTable::Get(theMesh->GetGlobalWaterSetID());
    EETerrainTypeTable::Get(theTerrain->mColorType);
    unsigned long x;
    unsigned long y;
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            float theZ = theRange * this->mData.mHeights[x][y] + theMinimum;
            if (theZ > (float)this->mData.mMaxIntElevation)
                theZ = (float)this->mData.mMaxIntElevation;
            else if (theZ < (float)this->mData.mMinIntElevation)
                theZ = (float)this->mData.mMinIntElevation;
            if (theZ < 0.0f && (theZ > -0.25f || this->mData.mIsSpaceMap))
                theZ = 0.0f;
            theZ = theZ * theScale;
            if (this->mData.mIsSpaceMap && (this->mData.mTiles[x][y] & 0x7fff) == 0) {
                if (theMesh->GetIsValidPoint(x, y))
                    theMesh->SetZ(x, y, 0.0f);
                this->mMap->PaintTerrain(x, y, gSpaceTerrain);
            } else if (theMesh->GetIsValidPoint(x, y)) {
                theMesh->SetZ(x, y, theZ);
                if (theZ < 0.0f)
                    theMesh->SetWaterColorSet(x, y);
            }
        }
    }
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (!theMesh->GetIsValidPoint(x, y))
                continue;
            theMesh->ComputeVertexNormal(x, y);
            if (!theMesh->GetIsValidTile(x, y))
                continue;
            float theLowest = min(theMesh->GetZ(x, y), theMesh->GetZ(x + 1, y));
            theLowest = min(theLowest, theMesh->GetZ(x, y + 1));
            theLowest = min(theLowest, theMesh->GetZ(x + 1, y + 1));
            theMesh->SetIsWater(x, y, theLowest < 0.0f);
            this->mMap->GetTile(x, y)->mHeight = theLowest;
        }
    }
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            if (theMesh->GetIsValidTile(x, y))
                theMesh->ComputeShorelines(x, y);
        }
    }
}

// FUNCTION: EXE 0x688bd4
// Drops the registered random-map generators (at Empire Earth.cpp's shutdown); returns 0
// (cdecl; provisional name)
long EEClearRandomMapGenerators()
{
    EERandomMap::sRandomMapGenerators.clear();
    return 0;
}

// TEMPLATE: EXE 0x68bccb
// (EERMGetElevation, defined above; its COMDAT follows the unit's code)

// TEMPLATE: EXE 0x68bd18
// (EERMIsAreaFree)

// TEMPLATE: EXE 0x68bdbe
// (EERMIsNearEdge)

// TEMPLATE: EXE 0x68be87
// (EERMClaimArea)

// SYNTHETIC: EXE 0x683b2e
// SYNTHETIC: EXE 0x683b38
// SYNTHETIC: EXE 0x683b49
// SYNTHETIC: EXE 0x683b55
// SYNTHETIC: EXE 0x683b60
// SYNTHETIC: EXE 0x683b6a
// SYNTHETIC: EXE 0x683b7b
// SYNTHETIC: EXE 0x683b87
// SYNTHETIC: EXE 0x683b92
// SYNTHETIC: EXE 0x683b9c
// SYNTHETIC: EXE 0x683ba7
// SYNTHETIC: EXE 0x683bb3
// SYNTHETIC: EXE 0x683bcf
// SYNTHETIC: EXE 0x683bd9
// SYNTHETIC: EXE 0x683bf1
// SYNTHETIC: EXE 0x683bfd

// the template COMDATs (0x288bd4..0x28befc)
// TEMPLATE: EXE 0x688be1
// TEMPLATE: EXE 0x688c16
// TEMPLATE: EXE 0x688c67
// TEMPLATE: EXE 0x688c8d
// TEMPLATE: EXE 0x688cb9
// TEMPLATE: EXE 0x688cd5
// TEMPLATE: EXE 0x688d80
// TEMPLATE: EXE 0x688e48
// TEMPLATE: EXE 0x688efb
// TEMPLATE: EXE 0x688ff8
// TEMPLATE: EXE 0x6890dd
// TEMPLATE: EXE 0x689114
// TEMPLATE: EXE 0x68914c
// TEMPLATE: EXE 0x68915e
// TEMPLATE: EXE 0x6891b8
// TEMPLATE: EXE 0x6891f1
// TEMPLATE: EXE 0x6892ac
// TEMPLATE: EXE 0x6892fc
// TEMPLATE: EXE 0x689335
// TEMPLATE: EXE 0x6893c7
// TEMPLATE: EXE 0x689431
// TEMPLATE: EXE 0x68946a
// TEMPLATE: EXE 0x6894ae
// TEMPLATE: EXE 0x6894e7
// TEMPLATE: EXE 0x6895ac
// TEMPLATE: EXE 0x689603
// TEMPLATE: EXE 0x68966c
// TEMPLATE: EXE 0x6896a3
// TEMPLATE: EXE 0x6896db
// TEMPLATE: EXE 0x6896ed
// TEMPLATE: EXE 0x689756
// TEMPLATE: EXE 0x689ab7
// TEMPLATE: EXE 0x689da0
// TEMPLATE: EXE 0x68a0b9
// TEMPLATE: EXE 0x68a0cd
// TEMPLATE: EXE 0x68a137
// TEMPLATE: EXE 0x68a749
// TEMPLATE: EXE 0x68aa22
// TEMPLATE: EXE 0x68aa52
// TEMPLATE: EXE 0x68aa7d
// TEMPLATE: EXE 0x68aad0
// TEMPLATE: EXE 0x68abdf
// TEMPLATE: EXE 0x68ac0a
// TEMPLATE: EXE 0x68ac1e
// TEMPLATE: EXE 0x68ad2c
// TEMPLATE: EXE 0x68ad50
// TEMPLATE: EXE 0x68ad7c
// TEMPLATE: EXE 0x68ad99
// TEMPLATE: EXE 0x68adc5
// TEMPLATE: EXE 0x68ade9
// TEMPLATE: EXE 0x68ae0e
// TEMPLATE: EXE 0x68ae2b
// TEMPLATE: EXE 0x68ae53
// TEMPLATE: EXE 0x68ae84
// TEMPLATE: EXE 0x68aee9
// TEMPLATE: EXE 0x68af20
// TEMPLATE: EXE 0x68af6b
// TEMPLATE: EXE 0x68afb1
// TEMPLATE: EXE 0x68b0ab
// TEMPLATE: EXE 0x68b0d7
// TEMPLATE: EXE 0x68b0f4
// TEMPLATE: EXE 0x68b120
// TEMPLATE: EXE 0x68b136
// TEMPLATE: EXE 0x68b14c
// TEMPLATE: EXE 0x68b15e
// TEMPLATE: EXE 0x68b16f
// TEMPLATE: EXE 0x68b1eb
// TEMPLATE: EXE 0x68b21b
// TEMPLATE: EXE 0x68b237
// TEMPLATE: EXE 0x68b390
// TEMPLATE: EXE 0x68b5dc
// TEMPLATE: EXE 0x68b6f4
// TEMPLATE: EXE 0x68b733
// TEMPLATE: EXE 0x68b7fc
// TEMPLATE: EXE 0x68b802
// TEMPLATE: EXE 0x68b87e
// TEMPLATE: EXE 0x68b9af
// TEMPLATE: EXE 0x68ba0f
// TEMPLATE: EXE 0x68ba95
// TEMPLATE: EXE 0x68baba
// TEMPLATE: EXE 0x68bb5d
// TEMPLATE: EXE 0x68bbcb
// TEMPLATE: EXE 0x68bbfd
// TEMPLATE: EXE 0x68bc6e
// TEMPLATE: EXE 0x68bcba
// TEMPLATE: EXE 0x68beea
// TEMPLATE: EXE 0x68aaf7
// TEMPLATE: EXE 0x688c2c
// TEMPLATE: EXE 0x6890ce
// TEMPLATE: EXE 0x68936d
// TEMPLATE: EXE 0x6894f9
// TEMPLATE: EXE 0x68a149
// TEMPLATE: EXE 0x68a42c
// TEMPLATE: EXE 0x68ab30
// TEMPLATE: EXE 0x68ab6a
// TEMPLATE: EXE 0x68ab95
// TEMPLATE: EXE 0x68ac47
// TEMPLATE: EXE 0x68afe7
// TEMPLATE: EXE 0x68b012
// TEMPLATE: EXE 0x68b03e
// TEMPLATE: EXE 0x68b05b
// TEMPLATE: EXE 0x68b087
// TEMPLATE: EXE 0x68b195
// TEMPLATE: EXE 0x68b1a7
// TEMPLATE: EXE 0x68b474
// TEMPLATE: EXE 0x68909a
// TEMPLATE: EXE 0x689a73
// TEMPLATE: EXE 0x68aac1
// more template COMDATs this unit compiles
// TEMPLATE: EXE 0x688cfc
// TEMPLATE: EXE 0x688d28
// TEMPLATE: EXE 0x688d54
// TEMPLATE: EXE 0x688dbb
// TEMPLATE: EXE 0x688de1
// TEMPLATE: EXE 0x688e0d
// TEMPLATE: EXE 0x688f45
// TEMPLATE: EXE 0x689a0a
// TEMPLATE: EXE 0x68a6e0
// TEMPLATE: EXE 0x68b1c7
// TEMPLATE: EXE 0x68b1d9
// TEMPLATE: EXE 0x68b5f2
// TEMPLATE: EXE 0x68b61b
// TEMPLATE: EXE 0x68b83f
// TEMPLATE: EXE 0x68ba6c
// TEMPLATE: EXE 0x68a05a
// TEMPLATE: EXE 0x68ab05
// TEMPLATE: EXE 0x68aebd
