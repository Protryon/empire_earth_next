// New (port), TEST ONLY: the world views' check (ee/EEWorldLayout.h, docs/progress/world-views.md). Every physics tick
// while the capture runs, a fixed list of the world's, every player's, every listed unit's and every map tile's fields,
// read through the C++ members, folded into one FNV-1a 64 of 32-bit words: the VWCK record.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>

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
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitAction.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "ee/EESimCapture.h"
#include "ee/EEWorldLayout.h"
#include "rust/sim/units.h"

namespace {

const unsigned int kTag = 0x4b435756u;   // "VWCK"

struct Digest {
    uint64_t mHash = 0xcbf29ce484222325ull;
    void Word(uint32_t inValue)
    {
        for (int i = 0; i < 4; i++) {
            this->mHash ^= (inValue >> (8 * i)) & 0xff;
            this->mHash *= 0x100000001b3ull;
        }
    }
    void Float(float inValue)
    {
        uint32_t theBits;
        memcpy(&theBits, &inValue, 4);
        this->Word(theBits);
    }
};

void UnitFields(Digest& ioDigest, RTSUnit* inUnit)
{
    ioDigest.Word((uint32_t)inUnit->mUnitID);
    ioDigest.Word((uint32_t)inUnit->mTileX);
    ioDigest.Word((uint32_t)inUnit->mTileY);
    ioDigest.Word(inUnit->mIsValid ? 1 : 0);
    ioDigest.Word(inUnit->mIsQueuedForDelete ? 1 : 0);
    ioDigest.Word(inUnit->mIsNodeFreed ? 1 : 0);
    ioDigest.Word(inUnit->mObjectKind.mValue);
    ioDigest.Word(inUnit->mTile != NULL ? 1 : 0);
    ioDigest.Word(inUnit->mTile != NULL ? (uint32_t)inUnit->mTile->mTerrainType : 0);
    ioDigest.Word(inUnit->mPlayer != NULL ? (uint32_t)inUnit->mPlayer->mPlayerID : 0xffffffffu);
    if (inUnit->mObjectKind.mValue < EE_OBJECT_UNIT)
        return;
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    ioDigest.Word((uint32_t)theUnit->mResourceType);
    ioDigest.Word((uint32_t)theUnit->mHitPoints);
    ioDigest.Word((uint32_t)theUnit->mResourceAmount);
    ioDigest.Word(theUnit->mHiddenFlags);
    EEUnitClass* theClass = theUnit->mUnitClass;
    if (theClass != NULL) {
        ioDigest.Word((uint32_t)theClass->mCategory);
        ioDigest.Word((uint32_t)theClass->mUnitType);
        ioDigest.Word((uint32_t)theClass->mFamily);
        ioDigest.Float(theClass->mRange);
    } else {
        ioDigest.Word(0xffffffffu);
    }
    EEUnitClassUpgrade* theStats = theUnit->mStats;
    if (theStats != NULL) {
        ioDigest.Float(theStats->mRange);
        ioDigest.Float(theStats->mScale);
        ioDigest.Word((uint32_t)theStats->mMaxHitPoints);
    } else {
        ioDigest.Word(0xffffffffu);
    }
    ioDigest.Word(theUnit->mComplexUnit == NULL ? 0 : theUnit->mComplexUnit == theUnit ? 1 : 2);
    if (inUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
        return;
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(inUnit);
    ioDigest.Float(theComplex->mHeight);
    ioDigest.Float(theComplex->mSight);
    ioDigest.Word((uint32_t)theComplex->mFacing);
    ioDigest.Word(theComplex->mIsBuilt ? 1 : 0);
    ioDigest.Word(theComplex->mIsCommandable ? 1 : 0);
    ioDigest.Word(theComplex->mIsCloaked ? 1 : 0);
    ioDigest.Word(theComplex->mExperience);
    ioDigest.Word((uint32_t)theComplex->mFormationSlot);
    ioDigest.Word((uint32_t)theComplex->mPower);
    const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
    ioDigest.Float(theLocation.GetX());
    ioDigest.Float(theLocation.GetY());
    ioDigest.Float(theLocation.GetZ());
    ioDigest.Word(theComplex->mCommand != NULL ? theComplex->mCommand->mType : 0);
    if (theComplex->mGoalState != NULL) {
        ioDigest.Word(theComplex->mGoalState->mKind);
        ioDigest.Word((uint32_t)theComplex->mGoalState->mID);
    } else {
        ioDigest.Word(0xffffffffu);
    }
    ioDigest.Word(theComplex->mActiveGoal != NULL ? theComplex->mActiveGoal->mKind : 0xffffffffu);
    ioDigest.Word((uint32_t)theComplex->mCargo.size());
    for (size_t i = 0; i < theComplex->mCargo.size(); i++)
        ioDigest.Word(theComplex->mCargo[i] != NULL ? (uint32_t)theComplex->mCargo[i]->mUnitID : 0);
}

uint32_t PlayerFields(Digest& ioDigest, EEPlayer* inPlayer)
{
    ioDigest.Word((uint32_t)inPlayer->mPlayerID);
    ioDigest.Word((uint32_t)inPlayer->mNextUniqueID);
    ioDigest.Word((uint32_t)inPlayer->mUnitCount);
    ioDigest.Word(inPlayer->mSlot);
    ioDigest.Word(inPlayer->mSeesAll ? 1 : 0);
    ioDigest.Word(inPlayer->mSaveVisMap ? 1 : 0);
    ioDigest.Word(inPlayer->mObserverMode ? 1 : 0);
    ioDigest.Float(inPlayer->mColor.mRed);
    ioDigest.Float(inPlayer->mColor.mGreen);
    ioDigest.Float(inPlayer->mColor.mBlue);
    ioDigest.Word(inPlayer->mSelectedGroup != NULL ? 1 : 0);
    for (int i = 0; i < 17; i++)
        ioDigest.Word((uint32_t)inPlayer->mEnemies[i]);
    for (int i = 0; i < 7; i++)
        ioDigest.Word((uint32_t)inPlayer->mResourceCaps[i]);
    ioDigest.Word((uint32_t)inPlayer->mPopLimit);
    ioDigest.Word((uint32_t)inPlayer->mPopulationCount);
    ioDigest.Word((uint32_t)inPlayer->mGameResult);
    ioDigest.Word(inPlayer->mSharedLOS ? 1 : 0);
    ioDigest.Float(inPlayer->mTributeFee);
    ioDigest.Word((uint32_t)inPlayer->mHeroLimit);
    ioDigest.Word((uint32_t)inPlayer->mNumHeroes);
    uint32_t theUnits = 0;
    for (RTSUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
        UnitFields(ioDigest, theUnit);
        theUnits++;
    }
    ioDigest.Word(theUnits);
    return theUnits;
}

uint32_t MapFields(Digest& ioDigest, EEMap* inMap)
{
    ioDigest.Word((uint32_t)inMap->mSize);
    ioDigest.Word((uint32_t)inMap->mU19561C);
    ioDigest.Word((uint32_t)inMap->mL195610);
    ioDigest.Word((uint32_t)inMap->mL195614);
    ioDigest.Word((uint32_t)inMap->mMapCharacteristics);
    unsigned long theWords = (inMap->mU19561C >> 1) + (inMap->mU19561C & 1);
    uint32_t theTiles = 0;
    for (long y = 0; y < inMap->mSize; y++) {
        for (long x = 0; x < inMap->mSize; x++) {
            EETile* theTile = inMap->GetTile(x, y);
            if (theTile == NULL) {
                ioDigest.Word(0);
                continue;
            }
            theTiles++;
            ioDigest.Word(1);
            ioDigest.Word((uint32_t)theTile->mTerrainType);
            ioDigest.Float(theTile->mHeight);
            ioDigest.Word((uint32_t)(int32_t)theTile->mAIValue);
            if (theTile->mVisibility != NULL)
                for (unsigned long w = 0; w < theWords; w++)
                    ioDigest.Word(theTile->mVisibility[w]);
            uint32_t theCount = 0;
            for (RTSTUnitList* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                ioDigest.Word(theNode->mUnit != NULL ? (uint32_t)theNode->mUnit->mUnitID : 0);
                theCount++;
            }
            ioDigest.Word(theCount);
        }
    }
    return theTiles;
}

}  // namespace

void EEWorldViewCheck()
{
    if (!EESimCaptureRecord::Active() || !EESimCaptureRecord::Keeps(kTag))
        return;
    Digest theDigest;
    uint32_t thePlayers = 0, theUnits = 0, theTiles = 0;
    theDigest.Word((uint32_t)gWorld.mNumPlayers);
    theDigest.Word((uint32_t)gWorld.mCurrentPlayer);
    for (int i = 0; i < 17; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        theDigest.Word(thePlayer != NULL ? 1 : 0);
        if (thePlayer == NULL)
            continue;
        thePlayers++;
        theUnits += PlayerFields(theDigest, thePlayer);
    }
    theDigest.Word(gWorld.mMap != NULL ? 1 : 0);
    if (gWorld.mMap != NULL)
        theTiles = MapFields(theDigest, gWorld.mMap);
    EESimCaptureRecord theRecord(kTag);
    theRecord.U32(gPhysics != NULL ? (unsigned int)gPhysics->mNumTicksDone : 0);
    theRecord.U32(thePlayers);
    theRecord.U32(theUnits);
    theRecord.U32(theTiles);
    theRecord.U32((unsigned int)(theDigest.mHash & 0xffffffffu));
    theRecord.U32((unsigned int)(theDigest.mHash >> 32));
    theRecord.Commit();
}
