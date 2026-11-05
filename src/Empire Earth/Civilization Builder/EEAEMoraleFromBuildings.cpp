// C:\EEX\Empire Earth\Civilization Builder\EEAEMoraleFromBuildings.cpp (name from RTTI EEAEMoraleFromBuildings;
// folder inferred: no __FILE__). 0x27b911..0x27bf0a: the morale from buildings area effect (vtable 0x44bd6c): ctor,
// Virtual4C, Virtual3C, Virtual44 (the count of the owner's buildings in range), then the two unit-collector
// instantiations it uses (EEGetUnitsInRectangle 0x27bae8 / 0x27bcfc) with their predicates (0x27bc86/0x27bcc1,
// 0x27be9a/0x27bed5). EEAEMoraleFromMilitaryHeroes's ctor starts at 0x27bf0a. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <functional>
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

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEMorale.h"
#include "ee/EERules.h"

// The owner's living, built morale buildings (combat.yaml morale buildings: the house, 0x25, in the original) whose footprint meets mRect (0x27bc86, the overlap 0x27bcc1:
// both out of line); layout inferred (0x14 bytes on the caller's frame) (provisional name)
struct EEAEMFBBuildingInRectangle {
    EEAEMFBBuildingInRectangle(const U2DRectangle<long>& inRect, EEPlayer* inPlayer) : mRect(inRect), mPlayer(inPlayer) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && EEGetRules().mCombat.IsMoraleBuilding(inUnit->mUnitClass->mUnitType) && inUnit->mHitPoints > 0 &&
               inUnit->mIsBuilt && this->Overlaps(inUnit);
    }
    bool Overlaps(EEComplexUnit* inUnit) const
    {
        long theX = inUnit->mTileX;
        long theY = inUnit->mTileY;
        long theSize = inUnit->mUnitClass->mTileSize;
        long theRight = theX + theSize - 1;
        long theBottom = theY + theSize - 1;
        if (this->mRect.mLeft <= theRight && this->mRect.mRight >= theX && this->mRect.mTop <= theBottom && this->mRect.mBottom >= theY)
            return true;
        return false;
    }

    U2DRectangle<long> mRect;               // +0x00
    EEPlayer* mPlayer;                      // +0x10
};

// The same buildings within mRangeSquared of mCenter (0x27be9a, the distance test 0x27bed5); layout inferred
// (provisional name)
struct EEAEMFBBuildingInRange {
    EEAEMFBBuildingInRange(const U2DPoint<long>& inCenter, EEPlayer* inPlayer, float inRangeSquared)
        : mCenter(inCenter), mPlayer(inPlayer), mRangeSquared(inRangeSquared) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && EEGetRules().mCombat.IsMoraleBuilding(inUnit->mUnitClass->mUnitType) && inUnit->mHitPoints > 0 &&
               inUnit->mIsBuilt && this->InRange(inUnit);
    }
    bool InRange(EEComplexUnit* inUnit) const
    {
        U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
        return this->mCenter.DistanceToSquaredF(theTile) <= this->mRangeSquared;
    }

    U2DPoint<long> mCenter;                 // +0x00
    EEPlayer* mPlayer;                      // +0x08
    float mRangeSquared;                    // +0x0c
};

// FUNCTION: EXE 0x67b911
// Creates a morale-from-buildings effect, its cap the template's maximum level plus the game variant's morale bonus
EEAEMoraleFromBuildings::EEAEMoraleFromBuildings(EEAreaEffectTemplate* inTemplate)
    : EEAEMorale(inTemplate), mCount(0)
{
    long theMax = inTemplate->mMaxLevel;
    this->mMax = theMax + gWorld.GetGameVariant()->mMoraleBonus;
    this->mDirty = true;
}

// FUNCTION: EXE 0x67b949
// Returns the morale level: the counted houses, capped at the maximum
long EEAEMoraleFromBuildings::GetLevel()
{
    return this->mCount < this->mMax ? this->mCount : this->mMax;
}

// FUNCTION: EXE 0x67b956
// Marks the house count for a recount
void EEAEMoraleFromBuildings::MarkDirty()
{
    this->mDirty = true;
}

// FUNCTION: EXE 0x67b95b
// Recounts, when marked dirty, the owner's built houses in the effect's square or circle, capped at the maximum
void EEAEMoraleFromBuildings::Recount(long inArg)
{
    std::vector<EEComplexUnit*> theUnits;
    GE2DPoint theCenter = this->mLocation;
    long theTileX = (long)theCenter.mX;
    long theTileY = (long)theCenter.mY;
    long theShape = this->mTemplate->mShape;
    U2DRectangle<long> theRect;
    long theRadius = UMFloatToLong(this->GetRadius());
    if (!this->mDirty)
        return;
    float theDX = theCenter.mX - theTileX;
    float theDY = theCenter.mY - theTileY;
    if (theDX > 0.25f && theDX < 0.75f && theDY > 0.25f && theDY < 0.75f) {
        theRect.mTop = theTileY - theRadius;
        theRect.mLeft = theTileX - theRadius;
        theRect.mRight = theRadius + theTileX;
        theRect.mBottom = theTileY + theRadius;
    } else {
        theRect.mTop = theTileY - theRadius;
        theRect.mLeft = theTileX - theRadius;
        theRect.mRight = theRadius + theTileX - 1;
        theRect.mBottom = theTileY + theRadius - 1;
    }
    long theCount;
    if (theShape == 1) {
        EEAEMFBBuildingInRectangle thePredicate(theRect, gWorld.mPlayers[this->mPlayer]);
        theCount = EEGetUnitsInRectangle(theRect, theUnits, thePredicate, true);
    } else {
        float theRange = this->GetRadius();
        EEAEMFBBuildingInRange thePredicate(U2DPoint<long>(theTileX, theTileY), gWorld.mPlayers[this->mPlayer],
                                            theRange * theRange);
        theCount = EEGetUnitsInRectangle(theRect, theUnits, thePredicate, true);
    }
    long theLimited = this->mMax < theCount ? this->mMax : theCount;
    if (theLimited < 0)
        theCount = 0;
    else if (this->mMax < theCount)
        theCount = this->mMax;
    this->mDirty = false;
    this->mCount = theCount;
}

// TEMPLATE: EXE 0x67bae8
// TEMPLATE: EXE 0x67bc86
// TEMPLATE: EXE 0x67bcc1
// TEMPLATE: EXE 0x67bcfc
// TEMPLATE: EXE 0x67be9a
// TEMPLATE: EXE 0x67bed5
