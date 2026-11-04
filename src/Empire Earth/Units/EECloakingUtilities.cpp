// C:\EEX\Empire Earth\Units\EECloakingUtilities.cpp (file name inferred: no __FILE__, no RTTI). Unit helpers
// between EEWheel.cpp and RTSUnit.cpp: 0x23ceca..0x23d7c0 (unit map [C]; edge: a globals chain of one function). The
// cloaked units' opacity (Fn23CEE2, the render lists' and the farm's), the cloaking effect (EEAddCloakEffect/EERemoveCloakEffect), the
// space docks' yaws and sites (EEGetSpaceDockYaw, EEIsSpaceDockSiteOpen) and the space units' planets (EEGetLandContinentOf
// and its mPlanetID pass). Unit flags: the exe's module set (provisional names).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEAIKindIDs.h"

// 0x2035ef (EEGhostUnit.cpp's spelling): the terrain kind blocks a wall piece's ground
bool EEIsLandTerrain(long inTerrainType);
unsigned short EECountTilesAround(long inX, long inY, bool inAny);  // 0x23d4ba (below; provisional name)

// the opacity range of a cloaked unit seen by an enemy (0x449b00 / 0x449b04; 0x564498 = their difference, set by the
// unit's dynamic initializer: the two are not compile-time constants)
static float kCloakedFullOpacity = 1.0f;
static float kCloakedOpacity = 0.6f;
// SYNTHETIC: EXE 0x63ceca   (the init-chain wrapper: jmp to the initializer)
// SYNTHETIC: EXE 0x63cecf   (the dynamic initializer of kCloakedOpacityRange)
static float kCloakedOpacityRange = kCloakedFullOpacity - kCloakedOpacity;
// the wall pieces' yaws (0x446dc0, one object shared with EEGhostUnit.cpp/EEUCBuilding.cpp/EEUnitClass.cpp; EEGetSpaceDockYaw)
static const float kWallRotations[4] = {0.0f, 3.1415927f, -1.5707964f, 1.5707964f};

// FUNCTION: EXE 0x63cee2
// Returns whether a unit is drawn translucent and sets its opacity: the rotting fade, or its cloak opacity (dimmed and
// clamped for an enemy viewer); a translucent unit that is not cloaked is left as it is
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity)
{
    bool theResult = inTranslucent;
    if (inTranslucent && !(inUnit->mIsCloakable && inUnit->mIsCloaked))
        return theResult;
    if (inUnit->GetCommandType() == kActionUnitRotting) {
        long theMaximum = inUnit->mUnitClass->mRottingTime;
        if (theMaximum != 0)
            ioOpacity = (float)inUnit->mComplexUnit->mRottingAwayTime / theMaximum;
        else
            ioOpacity = 0.0f;
    } else if (!inUnit->mIsCloakable || gFreeCamera) {
        ioOpacity = 1.0f;
    } else {
        float theOpacity = inUnit->mOpacity;
        ioOpacity = theOpacity;
        if (theOpacity < 1.0f)
            theResult = true;
        EEPlayer* theViewer = gWorld.GetCurrentPlayer();
        if (theViewer != inUnit->mPlayer && inUnit->mPlayer->mEnemies[theViewer->mPlayerID] != 0 &&
            !gWorld.GetCurrentPlayer()->mObserverMode) {
            ioOpacity = (theOpacity - kCloakedOpacity) / kCloakedOpacityRange * kCloakedOpacity;
            if (ioOpacity < 0.0f)
                ioOpacity = 0.0f;
            else if (ioOpacity > kCloakedOpacity)
                ioOpacity = kCloakedOpacity;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x63d008
// Returns a space dock's yaw from which of the 2 x 2 tiles at (inX, inY) are space (one or two space tiles), else the
// yaw of EEGetDockFacing
float EEGetSpaceDockYaw(long inX, long inY, long inSize)
{
    unsigned short theCount = 0;
    long theLeft = (long)(inX - 1.5f) + 1;
    long theBottom = (long)(inY - 1.5f) + 1;
    long theRight = (long)(inX + 1.5f);
    long theTop = (long)(inY + 1.5f);
    float theYaw = 0.0f;
    EEMap* theMap = gWorld.mMap;
    long x;
    long y;
    for (x = theLeft; x < theRight; x++) {
        for (y = theBottom; y < theTop; y++) {
            EETile* theTile = theMap->GetTile(x, y);
            if (theTile != NULL && theTile->mTerrainType != kTerrainSpace)
                theCount++;
        }
    }
    if (theCount == 3) {
        unsigned long theWater = 0;
        unsigned long theIndex = 0;
        for (x = theLeft; x < theRight; x++) {
            for (y = theBottom; y < theTop; y++) {
                // an off-map tile counts as space, as in the count above (it used to be read through NULL)
                EETile* theTile = theMap->GetTile(x, y);
                if (theTile == NULL || theTile->mTerrainType == kTerrainSpace)
                    theWater = theIndex;
                theIndex++;
            }
        }
        switch ((unsigned short)theWater) {
        case 3:
            theYaw = -0.78539819f;
            break;
        case 2:
            theYaw = -2.3561945f;
            break;
        case 1:
            theYaw = 0.78539819f;
            break;
        case 0:
            theYaw = 2.3561945f;
            break;
        }
    } else if (theCount == 2) {
        long theKinds[4];
        unsigned long theIndex = 0;
        for (x = theLeft; x < theRight; x++) {
            for (y = theBottom; y < theTop; y++) {
                // an off-map tile counts as space, as in the count above (it used to be read through NULL)
                EETile* theTile = theMap->GetTile(x, y);
                theKinds[(unsigned short)theIndex++] = theTile != NULL ? theTile->mTerrainType : kTerrainSpace;
            }
        }
        long thePiece;
        if (theKinds[1] != 0xc && theKinds[3] != 0xc)
            thePiece = 1;
        else if (theKinds[0] != 0xc && theKinds[1] != 0xc)
            thePiece = 2;
        else if (theKinds[2] != 0xc && theKinds[3] != 0xc)
            thePiece = 3;
        else
            thePiece = 0;
        theYaw = kWallRotations[thePiece];
    } else
        theYaw = kWallRotations[EEGetDockFacing(inX, inY)];
    return theYaw;
}

// FUNCTION: EXE 0x63d1f1
// Returns whether a space dock (unit type 0x2d8) fits at (inX, inY): its inner 2 x 2 tiles placeable, its border on the
// map with at least two land tiles, and its inner land/space mix fitting a dock
bool EEIsSpaceDockSiteOpen(long inX, long inY, EEPlayer* inPlayer)
{
    bool theOK = true;
    long theBlocking = 0;
    unsigned short theInner = 0;
    long theWater = 0;
    long theLeft = (long)(inX - 1.5f);
    long theBottom = (long)(inY - 1.5f);
    long theRight = (long)(inX + 1.5f);
    long theTop = (long)(inY + 1.5f);
    GE3DPoint thePoint((float)inX, (float)inY, 0.0f);
    EEMap* theMap = gWorld.mMap;
    long x;
    long y;
    for (x = theLeft; x <= theRight && theOK; x++) {
        for (y = theBottom; y <= theTop && theOK; y++) {
            EETile* theTile = theMap->GetTile(x, y);
            if (x == theLeft || y == theBottom || x == theRight || y == theTop) {
                if (theTile == NULL || theMap->IsEdgeTile(x, y)) {
                    theOK = false;
                } else {
                    long theKind = theTile->mTerrainType;
                    if (EEIsLandTerrain(theKind))
                        theBlocking++;
                    else if (theKind == kTerrainSpace)
                        theWater++;
                }
            } else {
                bool thePlaceable;
                if (theTile != NULL) {
                    thePlaceable = EECanPlace(inPlayer, 0x2d8, &thePoint, x, y, 0, 0);
                    if (EEIsLandTerrain(theTile->mTerrainType))
                        theInner++;
                } else {
                    thePlaceable = false;
                }
                if (!thePlaceable)
                    theOK = false;
            }
        }
    }
    if (theOK) {
        if (theInner == 3) {
            bool theFound = false;
            for (x = theLeft + 1; x < theRight && !theFound; x++) {
                for (y = theBottom + 1; y < theTop && !theFound; y++) {
                    EETile* theTile = theMap->GetTile(x, y);
                    if (theTile != NULL && theTile->mTerrainType == kTerrainSpace)
                        theFound = true;
                }
            }
            if (EECountTilesAround(x - 1, y - 1, false) < 5)
                theOK = false;
        } else if (theInner == 2) {
            // two blocking tiles inside: accepted as is
        } else if (theInner == 1) {
            unsigned short theMost = 0;
            for (x = theLeft + 1; x < theRight; x++) {
                for (y = theBottom + 1; y < theTop; y++) {
                    EETile* theTile = theMap->GetTile(x, y);
                    if (theTile == NULL || theTile->mTerrainType != kTerrainSpace)
                        continue;
                    unsigned short theCount = EECountTilesAround(x, y, true);
                    if (theCount > theMost)
                        theMost = theCount;
                }
            }
            if (theMost < 5)
                theOK = false;
        } else {
            theOK = false;
        }
    }
    if (theBlocking < 2)
        theOK = false;
    return theOK;
}

// FUNCTION: EXE 0x63d4ba
// Counts the 3 x 3 tiles around (inX, inY) that are land (inAny) or space (otherwise); 0 when the centre is off the map
unsigned short EECountTilesAround(long inX, long inY, bool inAny)
{
    if (gWorld.mMap->GetTile(inX, inY) == NULL)
        return 0;
    long theCount = 0;
    for (long x = inX - 1; x <= inX + 1; x++) {
        for (long y = inY - 1; y <= inY + 1; y++) {
            EETile* theTile = gWorld.mMap->GetTile(x, y);
            if (theTile == NULL)
                continue;
            if (inAny) {
                if (EEIsLandTerrain(theTile->mTerrainType))
                    theCount++;
            } else if (theTile->mTerrainType == kTerrainSpace) {
                theCount++;
            }
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x63d570
// Attaches the cloaking graphic effect (0x11c) to the unit and keeps its handle in the complex unit's mCloaked
void EEAddCloakEffect(EEUnit* inUnit)
{
    EEEffectHandle theHandle;
    if (gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectMarketTicketTape), &theHandle, false, 0, 0) == 0)
        dynamic_cast<EEComplexUnit*>(inUnit)->mCloaked = theHandle.mValue;
}

// FUNCTION: EXE 0x63d5c3
// Destroys the unit's cloaking effect, if it has one, and clears its handle
void EERemoveCloakEffect(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(inUnit);
    if ((long)theUnit->mCloaked <= 0)
        return;
    EEEffectHandle theHandle;
    theHandle.mValue = theUnit->mCloaked;
    gWorld.GetEffectManager()->DestroyEffect(theHandle);
    theUnit->mCloaked = 0;
}

// FUNCTION: EXE 0x63d5fb
// Returns the planet (land continent) under a unit or nearest it within 6 tiles; 1 on a non-space map, 0 when none
unsigned short EEGetLandContinentOf(EEComplexUnit* inUnit)
{
    if (!gWorld.mMap->mTerrainMesh.GetIsSpaceMap())
        return 1;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    bool theFound = false;
    EETile* theTile = gWorld.mMap->GetTile(theX, theY);
    EEMap* theMap = gWorld.mMap;
    if (theTile != NULL && theTile->mTerrainType != kTerrainSpace && !theMap->IsEdgeTile(theX, theY))
        theFound = true;
    for (long theStep = 1; theStep <= 6 && !theFound; theStep++) {
        for (long i = -1; i <= 1 && !theFound; i++) {
            for (long j = -1; j <= 1 && !theFound; j++) {
                if (i == 0 && j == 0)
                    continue;
                theX = i * theStep + inUnit->GetTileX();
                theY = j * theStep + inUnit->GetTileY();
                theTile = theMap->GetTile(theX, theY);
                if (theTile != NULL && theTile->mTerrainType != kTerrainSpace && !theMap->IsEdgeTile(theX, theY))
                    theFound = true;
            }
        }
    }
    unsigned short theResult = 0;
    if (theFound) {
        EETileCoordinate theTileCoordinate;
        theTileCoordinate.mX = theX;
        theTileCoordinate.mY = theY;
        theResult = gContinentManager.GetContinent(theTileCoordinate, kAIPathNonSpace);
    }
    return theResult;
}

// FUNCTION: EXE 0x63d76c
// Sets the planet of every complex air unit, space dock, space turret and orbital space station on the map
// (cdecl; called by EEWorld.cpp and EEUserInterface.cpp)
void EEUpdateLandContinents()
{
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (theComplexUnit == NULL)
            continue;
        EEUnitClass* theClass = theComplexUnit->mUnitClass;
        if ((theClass->mDomains & kMovementAir) || theClass->mUnitType == kUnitTypeSpaceDock || theClass->mUnitType == kUnitTypeSpaceTurret
            || theClass->mUnitType == kUnitTypeOrbitalSpaceStation)
            theComplexUnit->mPlanetID = EEGetLandContinentOf(theComplexUnit);
    }
}
