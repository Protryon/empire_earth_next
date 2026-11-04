// C:\EEX\Empire Earth\AI Thread\EECPIArmy.cpp (file name inferred: no __FILE__; RTTI class EECPIArmy > EEAIArea,
// vtable 0x43c1dc). 0x8013d..0x80816: EECPExplorationManager.cpp's code ends at 0x8013d, EECPILocation's key chain starts at
// 0x80816. After the class's own functions: the inline U2DRectangle<long>/U2DPoint<long> geometry COMDATs it uses first
// (0x80347 .. 0x80804).
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
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
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPIArmy.h"

// The original stores the zero members in the order 68, 6c, 78, 70, 7c, 84, 74 (not declaration order)
// FUNCTION: EXE 0x48013d
// Creates an empty army area with zero strength and a join distance of 12 tiles
EECPIArmy::EECPIArmy()
{
    this->mL68 = 0;
    this->mL6C = 0;
    this->mStrength = 0;
    this->mContinentID = 0;
    this->mL7C = 0;
    this->mB84 = false;
    this->mL74 = 0;
    this->mAreaType = kAIAreaArmy;
    this->mJoinDistanceSq = 0x90;
}

// SYNTHETIC: EXE 0x48017b
// (the scalar deleting destructor)

// FUNCTION: EXE 0x48019d
// Adds the unit's tile (its container's when it is inside one) to the army, growing the bounds and the strength
// (the unit's own)
void EECPIArmy::AddUnit(EEComplexUnit* inUnit)
{
    U2DPoint<long> theTile;
    EEComplexUnit* theUnit = inUnit->mContainer != NULL ? inUnit->mContainer : inUnit;
    if (theUnit == NULL || theUnit->mTileX == -10000)   // the tile used (was the contained unit's own, -10000)
        return;
    theTile.SetXY(theUnit->GetTileX(), theUnit->GetTileY());
    if (this->mTiles.Size() > 0)
        this->mBounds.Extend(theTile);
    else {
        this->mBounds.mTop = theTile.mY;
        this->mBounds.mLeft = theTile.mX;
        this->mBounds.mBottom = theTile.mY;
        this->mBounds.mRight = theTile.mX;
    }
    this->mTiles.Insert(U2DPoint<long>(theUnit->mTileX, theUnit->mTileY));
    this->mStrength += inUnit->mUnitClass->mInfluence;   // the unit's own strength (the container only lends its tile)
}

// FUNCTION: EXE 0x480221
// Merges another army into this one: its tiles, strength and second counter
void EECPIArmy::Merge(EECPIArmy* inOther)
{
    std::vector<U2DPoint<long> > thePoints;
    inOther->mTiles.BuildPointList(thePoints, 1);
    for (long i = thePoints.size() - 1; i >= 0; i--)
        this->mTiles.Insert(thePoints[i]);
    this->mStrength += inOther->mStrength;
    this->mL7C += inOther->mL7C;
}

// NEAR: the original materialises the land flag (`shr al,1; and al,1`: a bool accessor?) and keeps the result
// in another register
// FUNCTION: EXE 0x480296
// Returns true if the unit (or its container) is a land unit on the army's continent (or the army has none) and
// lies inside the army's bounds or within the join distance of them; any land unit joins an empty army
bool EECPIArmy::IsNear(EEComplexUnit* inUnit)
{
    U2DPoint<long> theTile;
    U2DPoint<long> theClosest;
    if (inUnit->mContainer != NULL)
        inUnit = inUnit->mContainer;
    bool theResult = (inUnit->mUnitClass->mDomains >> 1) & 1;
    if (theResult) {
        unsigned short theID = gContinentManager.GetContinentID(inUnit);
        if (theID != this->mContinentID && this->mContinentID != 0)
            theResult = false;
        else if (this->mTiles.Size() > 0) {
            theTile.SetXY(inUnit->GetTileX(), inUnit->GetTileY());
            theResult = this->mBounds.Contains(theTile);
            if (!theResult) {
                theClosest = this->mBounds.GetClosestPoint(theTile);
                theResult = theClosest.DistanceToSquared(theTile) <= this->mJoinDistanceSq;
            }
        } else
            theResult = true;
    }
    return theResult;
}

// The inline geometry COMDATs first emitted here (exe-view U2DRectangle<long> / U2DPoint<long> members in
// lle/U2DRectangle.h and lle/U2DPoint.h): GetClosestPoint 0x80347, GetClosestCorner 0x803f4,
// GetClosestPointOnSegment 0x805ff, GetBottomLeft 0x8070d, GetTopRight 0x8071e, ProjectOnSegment 0x80730,
// GetBottomRight 0x807f3, GetTopLeft 0x80804
// TEMPLATE: EXE 0x480347
// TEMPLATE: EXE 0x4803f4
// TEMPLATE: EXE 0x4805ff
// TEMPLATE: EXE 0x48070d
// TEMPLATE: EXE 0x48071e
// TEMPLATE: EXE 0x480730
// TEMPLATE: EXE 0x4807f3
// TEMPLATE: EXE 0x480804
