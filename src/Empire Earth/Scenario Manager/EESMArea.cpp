// C:\EEX\Empire Earth\Scenario Manager\EESMArea.cpp (file name inferred: no __FILE__): 0x1ba02f..0x1ba89f,
// one scenario area. Unit flags: the exe module set.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GERectangle.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"

static const long kNoArea = -10000;

// FUNCTION: EXE 0x5ba02f
// Creates an unnamed rectangle-mode area with no rectangle or anchor and no cached centre tile
EESMArea::EESMArea()
{
    this->mName = "";
    this->mHasRectangle = false;
    this->mHasAnchor = false;
    this->mSelectMode = 0;
    this->mID = 0;
    this->mCenterTile.SetX(kNoArea);
    this->mCenterTile.SetY(kNoArea);
    this->mTilesTime = 0;
}

// FUNCTION: EXE 0x5ba0ac
// Destroys the area (members only)
EESMArea::~EESMArea()
{
}

// FUNCTION: EXE 0x5ba0d1
// Returns whether the unit is in the area (no anchor: the unit is off the map; negative anchor: it has no complex
// unit or that stands on the unit's tile; else by tile); false for no unit
bool EESMArea::ContainsUnit(EEUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL) {
        // The tile is read only after the NULL test (it was read first).
        U2DPoint<long> theTile(inUnit->GetTileX(), inUnit->GetTileY());
        if (this->IsUnset()) {
            if (inUnit->GetTileX() == kNoArea)
                theResult = true;
        } else if (this->IsOffMap()) {
            EEComplexUnit* theUnit = inUnit->GetComplexUnit();
            if (theUnit == NULL)
                theResult = true;
            else if (theUnit->mU0DC.GetX() == theTile.mX && theUnit->mU0DC.GetY() == theTile.mY)
                theResult = true;
            else
                theResult = false;
        } else
            theResult = this->ContainsTile(theTile);
    }
    return theResult;
}

// FUNCTION: EXE 0x5ba154
// Returns whether a tile is in the area: inside the rectangle, on the anchor's continent, or inside the hull of a
// non-Gaia town that contains the anchor
bool EESMArea::ContainsTile(const U2DPoint<long>& inTile)
{
    std::vector<EETown*> theTowns;
    GE2DPoint thePoint;
    bool theResult = false;
    EETown* theTown;
    switch (this->mSelectMode) {
    case 0:
        thePoint.SetX((float)inTile.GetX());
        thePoint.SetY((float)inTile.GetY());
        theResult = this->mRect.Contains(thePoint);
        break;
    case 1:
        if (inTile.GetX() >= 0 && inTile.GetY() >= 0) {
            unsigned short theContinent = gContinentManager.GetContinentID(this->mAnchor.GetX(), this->mAnchor.GetY());
            if (gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY()) == theContinent)
                theResult = true;
        }
        break;
    case 2:
        {
            for (long i = gWorld.GetNumPlayers() - 1; i > 0; i--) {
                EEPlayer* thePlayer = gWorld.GetPlayer(i);
                for (long j = thePlayer->GetNumTowns() - 1; j >= 0; j--) {
                    theTown = thePlayer->mTowns[j];
                    if (theTown->mHull.BoundedContains(this->mAnchor))
                        theTowns.push_back(theTown);
                }
            }
            for (long k = theTowns.size() - 1; k >= 0 && !theResult; k--) {
                theTown = theTowns[k];
                theResult = theTown->mHull.BoundedContains(inTile);
            }
        }
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ba2b2
// Copies another area's name, text, rectangle, anchor, select mode and ID (not its tile cache)
void EESMArea::Copy(const EESMArea& inArea)
{
    this->mName = inArea.mName;
    this->mText = inArea.mText;
    this->mRect = inArea.mRect;
    this->mHasRectangle = inArea.mHasRectangle;
    this->mAnchor = inArea.mAnchor;
    this->mHasAnchor = inArea.mHasAnchor;
    this->mSelectMode = inArea.mSelectMode;
    this->mID = inArea.mID;
}

#include "ee/EEPhysics.h"
#include "ee/EEMap.h"

// FUNCTION: EXE 0x5ba315
// Returns the area's centre tile, computing and caching it once: rectangle centre, continent centre, or the hull
// centre of the town holding the anchor (else the anchor)
U2DPoint<long> EESMArea::GetCenterTile()
{
    U2DPoint<long> theTile;
    GE2DPoint thePoint;
    bool theFound = false;
    if (this->mCenterTile.mX != kNoArea)
        return this->mCenterTile;
    if (this->mSelectMode == 0) {
        this->mRect.GetCenter(thePoint);
        this->mCenterTile.mX = (long)thePoint.GetX();
        this->mCenterTile.mY = (long)thePoint.GetY();
    } else if (this->mSelectMode == 1) {
        unsigned short theContinent = gContinentManager.GetContinentID(this->mAnchor.GetX(), this->mAnchor.GetY());
        gContinentManager.GetContinentCenter(theContinent, theTile);
        this->mCenterTile = theTile;
    } else {
        for (long i = gWorld.GetNumPlayers() - 1; i > 0 && !theFound; --i) {
            EEPlayer* thePlayer = gWorld.GetPlayer(i);
            for (long j = thePlayer->GetNumTowns() - 1; j >= 0 && !theFound; --j) {
                EETown* theTown = thePlayer->mTowns[j];
                if (theTown->mHull.BoundedContains(this->mAnchor)) {
                    this->mCenterTile = theTown->mHull.GetCenter();
                    theFound = true;
                }
            }
        }
        if (!theFound)
            this->mCenterTile = this->mAnchor;
    }
    return this->mCenterTile;
}

// FUNCTION: EXE 0x5ba45e
// Returns true when the area has no anchor (an anchor coordinate is the -10000 no-area value)
bool EESMArea::IsUnset() const
{
    if (this->mAnchor.mX != kNoArea && this->mAnchor.mY != kNoArea) {
        return false;
    }
    return true;
}

// FUNCTION: EXE 0x5ba473
// Returns whether the anchor has a negative coordinate (an off-map area)
bool EESMArea::IsOffMap() const
{
    if (this->mAnchor.mX < 0.0f || this->mAnchor.mY < 0.0f)
        return true;
    return false;
}

// FUNCTION: EXE 0x5ba495
// Copies the area's tiles out, rebuilding the cache when empty or unused for 3000 ticks (the rebuild appends to the
// old list); every cached read restarts the timer
void EESMArea::GetTiles(std::vector<U2DPoint<long> >& outTiles)
{
    std::vector<EETown*> theTowns;
    if (this->mTiles.size() <= 0 || (unsigned long)(gPhysics->mCurrentTick - this->mTilesTime) >= 3000) {
        this->mTilesTime = gPhysics->mCurrentTick;
        switch (this->mSelectMode) {
        case 0: {
            for (long x = (long)this->mRect.GetLeft(); (float)x <= this->mRect.GetRight(); ++x) {
                for (long y = (long)this->mRect.GetTop(); (float)y <= this->mRect.GetBottom(); ++y) {
                    if (gWorld.mMap->GetTile(x,y) != NULL) {
                        U2DPoint<long> theTile(x,y);
                        this->mTiles.push_back(theTile);
                    }
                }
            }
            break;
        }
        case 1:
            gContinentManager.GetContinentTiles(gContinentManager.GetContinentID(this->mAnchor.GetX(),this->mAnchor.GetY()),this->mTiles);
            break;
        case 2:
            for (long i = gWorld.GetNumPlayers() - 1; i > 0; --i) {
                EEPlayer* thePlayer = gWorld.GetPlayer(i);
                for (long j = thePlayer->GetNumTowns() - 1; j >= 0; --j) {
                    EETown* theTown = *(thePlayer->mTowns.begin() + j);
                    if (theTown->mHull.BoundedContains(this->mAnchor))
                        theTowns.push_back(theTown);
                }
            }
            for (long k = (long)theTowns.size() - 1; k >= 0; --k)
                theTowns[k]->GetTiles(this->mTiles);
            if (this->mTiles.size() == 0)
                this->mTiles.push_back(this->mAnchor);
            break;
        }
        outTiles = this->mTiles;
    } else {
        outTiles = this->mTiles;
        this->mTilesTime = gPhysics->mCurrentTick;
    }
}

// FUNCTION: EXE 0x5ba670
// Reads the area's name, text, anchor, rectangle, flags, select mode, tile time and ID; returns the first error
long EESMArea::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Load(inFile);
    if (theResult == kErrorOK) {
        float theX = this->mAnchor.mX;
        float theY;
        theResult = inFile.Read(theX);
        if (theResult == kErrorOK) {
            theY = this->mAnchor.mY;
            theResult = inFile.Read(theY);
        }
        if (theResult == kErrorOK) {
            this->mAnchor.mX = theX;
            this->mAnchor.mY = theY;
        }
    }
    if (theResult == kErrorOK) {
        float theLeft;
        float theTop;
        float theRight;
        float theBottom;
        theResult = inFile.Read(theLeft);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theTop);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theRight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theBottom);
        if (theResult == kErrorOK) {
            this->mRect.SetLeft(theLeft);
            this->mRect.SetTop(theTop);
            this->mRect.SetRight(theRight);
            this->mRect.SetBottom(theBottom);
        }
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasRectangle);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasAnchor);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSelectMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTilesTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mID);
    return theResult;
}

// FUNCTION: EXE 0x5ba78e
// Writes the area's name, text, anchor, rectangle, flags, select mode, tile time and ID; returns the first error
long EESMArea::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Save(outFile);
    if (theResult == kErrorOK) {
        float theX = this->mAnchor.mX;
        theResult = outFile.Write(theX);
        if (theResult == kErrorOK) {
            float theY = this->mAnchor.mY;
            theResult = outFile.Write(theY);
        }
    }
    if (theResult == kErrorOK) {
        float theLeft = this->mRect.GetLeft();
        float theTop;
        float theRight;
        float theBottom;
        theResult = outFile.Write(theLeft);
        if (theResult == kErrorOK) {
            theTop = this->mRect.GetTop();
            theResult = outFile.Write(theTop);
        }
        if (theResult == kErrorOK) {
            theRight = this->mRect.GetRight();
            theResult = outFile.Write(theRight);
        }
        if (theResult == kErrorOK) {
            theBottom = this->mRect.GetBottom();
            theResult = outFile.Write(theBottom);
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasRectangle);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasAnchor);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSelectMode);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTilesTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mID);
    return theResult;
}
