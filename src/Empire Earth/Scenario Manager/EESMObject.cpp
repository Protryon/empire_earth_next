// C:\EEX\Empire Earth\Scenario Manager\EESMObject.cpp (file name inferred: no __FILE__): 0x1c5f73..0x1ca4ee,
// scenario objects and triggers (with EESMTConditionWrapper.cpp and EESMTEffectWrapper.cpp folded in).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "lle/FSFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMObject.h"
#include "ee/EESMTConditionWrapper.h"
#include "ee/EESMTEffectWrapper.h"
#include <algorithm>
#include "ee/EESMTrigger.h"
#include "ee/EESMCondition.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// 0x246ea2 (cdecl, 5 arguments; EEFOFindNewTargetNearOldTarget.cpp): the squared distance from inUnit to tile
// (inX, inY), the unit's nearest tile out (provisional name)
float Fn246EA2(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "LostDebug.h"
#include "ee/EEPlayer.h"
#include "ee/EESMArea.h"
#include "ee/EEStringIDs.h"
// 0x246ea2 again, as IsNearTile uses it (provisional name)
float Fn246EA2(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);

// FUNCTION: EXE 0x5c5f73
// Creates an object named None that picks units on the map, owned by and selected by player 7, with no filters
EESMObject::EESMObject()
{
    gResource->GetString(IDS_NONE, this->mName);
    this->mUnits.clear();
    this->mOwner = 7;
    this->mSelectedByPlayer = 7;
    this->mFamily = 0;
    this->mClass = 0;
    this->mInAreaCheck = 0;
    this->mNearCheck = 0;
    this->mHasAttributeCheck = 0;
    this->mVisibleByCheck = 0;
    this->mHasStateCheck = 0;
    this->mSelectedByCheck = 0;
    this->mArea = 0;
    this->mNearMode = 0;
    this->mParent = 0;
    this->mNearObject = 0;
    this->mAttribute = kSMUnitAttributeHitPoints;
    this->mAttributeMin = 0;
    this->mAttributeMax = 0;
    this->mVisibleByPlayer = 0;
    this->mState = kActionInvalid;
    this->mIsAttributePercent = false;
    this->mL64 = 0;
    this->mIsSelectOnMap = true;
    this->mMinCount = 1;
    this->mMaxCount = 1;
    this->mIsDynamic = true;
}

// SYNTHETIC: EXE 0x5c6019
// (the scalar deleting destructor, emitted with the vtable; slot EESMObject+0x0)

// FUNCTION: EXE 0x5c6035
// Destroys the object (members only)
EESMObject::~EESMObject()
{
}

// FUNCTION: EXE 0x5c6051
// Returns whether a unit class passes the object's filter: its class (or any scenario-selectable one), no projectiles
// or set 0x79 members, narrowed by mFamily (epoch 1..0xf, family 0x10..0x4b, AI kind 0x4c..0x74, then a unit set)
// (provisional name)
bool EESMObject::AcceptsClass(unsigned long inClass)
{
    bool theResult;
    if (this->mClass != 0)
        theResult = this->mClass == (long)inClass;
    else
        theResult = gUnitClassTable[inClass]->mIsScenarioSelectable;
    if (theResult) {
        EEUnitClass* theClass = gUnitClassTable[inClass];
        if (theClass != NULL) {
            long theType = theClass->mCategory;
            if (theType == kUnitCategoryProjectile || theType == kUnitCategoryBomb || theType == kUnitCategoryMissile || theType == kUnitCategoryTorpedo
                || gUnitSetManager.Contains(theType, 0x79)) {
                theResult = false;
            } else if (this->mFamily == 0) {
                theResult = true;
            } else if ((unsigned long)this->mFamily <= 0xf) {
                theResult = this->mFamily == theClass->mEpoch + 1;
            } else if ((unsigned long)this->mFamily <= 0x4b) {
                theResult = this->mFamily - 0x10 == theClass->mFamily;
            } else if ((unsigned long)this->mFamily <= 0x74) {
                theResult = this->mFamily - 0x4c == theClass->mAIKind;
            } else if ((unsigned long)this->mFamily <= gUnitSetManager.GetSize() + 0x74) {
                EEUnitSet* theSet = gUnitSetManager.GetSet(this->mFamily - 0x75);
                if (theSet != NULL)
                    theResult = theSet->Contains(inClass);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5c6148
// Returns whether one of the object's units is near the unit (mode 0: within the sight range of either side's unit,
// 1: within reach, 2: adjacent); refreshes the object's units first
// (provisional name)
bool EESMObject::IsNearUnit(EEUnit* inUnit, long inMode, bool inFromUnit)
{
    bool theResult = false;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    if (theX != -10000) {
        this->Evaluate(0);
        long theCount = this->mUnits.size();
        for (long i = 0; i < theCount && !theResult; i++) {
            EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(gWorld.GetUnit(this->mUnits[i]));
            if (theUnit != NULL && theUnit != inUnit && theUnit->mTileX != -10000) {
                long theUnitX;
                long theUnitY;
                inUnit->GetNearestFootprintTile(theUnit, &theUnitX, &theUnitY);
                theUnit->GetNearestFootprintTile(inUnit, &theX, &theY);
                long theDY = theUnitY - theY;
                long theDX = theUnitX - theX;
                float theDistance = (float)(theDX * theDX + theDY * theDY);
                switch (inMode) {
                case 0: {
                    float theRange = (inFromUnit ? inUnit->mComplexUnit : theUnit->mComplexUnit)->GetLineOfSight();
                    theResult = theDistance <= theRange * theRange;
                    break;
                }
                case 1:
                    if (inFromUnit)
                        theResult = inUnit->mComplexUnit != NULL ? inUnit->mComplexUnit->IsInReach(theUnit) : false;
                    else
                        theResult = theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->IsInReach(inUnit) : false;
                    break;
                case 2:
                    theResult = theDistance <= 2.0f;
                    break;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5c629f
// Returns whether one of the object's units is near a tile (mode 0: within its sight range, 1: can reach it,
// 2: adjacent); optionally refreshes the units first, after reading their count
// (provisional name)
bool EESMObject::IsNearTile(long inX, long inY, long inMode, bool inEvaluate)
{
    long theTileX = 0;
    long theTileY = 0;
    long theCount = this->mUnits.size();
    if (inEvaluate)
        this->Evaluate(0);
    bool theResult = false;
    for (long i = 0; i < theCount && !theResult; i++) {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(gWorld.GetUnit(this->mUnits[i]));
        if (theUnit == NULL)
            continue;
        float theDistance = Fn246EA2(theUnit, inX, inY, &theTileX, &theTileY);
        switch (inMode) {
        case 0: {
            float theRange = theUnit->mComplexUnit->GetLineOfSight();
            theResult = theDistance <= theRange * theRange;
            break;
        }
        case 1:
            theResult = theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->CanReachTile(inX, inY) : false;
            break;
        case 2:
            theResult = theDistance <= 2.0f;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5c6386
// Copies another object's name and filter settings, and its unit list when it picks units on the map
void EESMObject::Copy(const EESMObject& inObject)
{
    this->mName = inObject.mName;
    this->mIsSelectOnMap = inObject.mIsSelectOnMap;
    if (this->mIsSelectOnMap)
        this->mUnits = inObject.mUnits;
    this->mOwner = inObject.mOwner;
    this->mFamily = inObject.mFamily;
    this->mClass = inObject.mClass;
    this->mMinCount = inObject.mMinCount;
    this->mMaxCount = inObject.mMaxCount;
    this->mIsDynamic = inObject.mIsDynamic;
    this->mInAreaCheck = inObject.mInAreaCheck;
    this->mNearCheck = inObject.mNearCheck;
    this->mHasAttributeCheck = inObject.mHasAttributeCheck;
    this->mVisibleByCheck = inObject.mVisibleByCheck;
    this->mHasStateCheck = inObject.mHasStateCheck;
    this->mSelectedByCheck = inObject.mSelectedByCheck;
    this->mArea = inObject.mArea;
    this->mNearMode = inObject.mNearMode;
    this->mNearObject = inObject.mNearObject;
    this->mParent = inObject.mParent;
    this->mAttribute = inObject.mAttribute;
    this->mAttributeMin = inObject.mAttributeMin;
    this->mAttributeMax = inObject.mAttributeMax;
    this->mVisibleByPlayer = inObject.mVisibleByPlayer;
    this->mState = inObject.mState;
    this->mSelectedByPlayer = inObject.mSelectedByPlayer;
    this->mL64 = inObject.mL64;
    this->mIsAttributePercent = inObject.mIsAttributePercent;
}

// FUNCTION: EXE 0x5c6458
// Rebuilds a dynamic object's units (parent or owners), keeps those passing the state/attribute/selected/visible/area/
// near filters, shuffles and caps them; returns whether any pass (visibility uses the selected-by player)
bool EESMObject::Evaluate(bool inIncludeDead)
{
    std::vector<long> thePlayers;
    EESMObject* theParent = gWorld.mScenarioManager->GetScenarioObject(this->mParent);
    EESMObject* theTarget = gWorld.mScenarioManager->GetScenarioObject(this->mNearObject);
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mArea);
    bool theRebuilt = false;
    bool theIncludeDead = inIncludeDead != 0 || this->mState == kActionResourcesRotting || this->mState == kActionUnitRotting || this->mState == kActionDead || this->mState == kActionResourcesDepleting;
    if (!this->mIsSelectOnMap) {
        if (this->mIsDynamic == true)
            this->mUnits.clear();
        if (this->mUnits.size() == 0) {
            theRebuilt = true;
            if (this->mParent != 0) {
                theParent->Evaluate(theIncludeDead);
                theParent->GetUnits(this->mUnits);
                gWorld.mScenarioManager->ChoosePlayer(this->mOwner, &thePlayers, false);
                for (long i = 0; i < (long)this->GetUnitCount(); i++) {
                    EEUnit* theUnit = gWorld.GetUnit(this->mUnits[i]);
                    bool theKeep = theUnit != NULL && this->AcceptsClass(theUnit->mUnitClass->mUnitType);
                    if (theKeep) {
                        long thePlayerID = theUnit->mPlayer->GetPlayerID();
                        theKeep = std::find(thePlayers.begin(), thePlayers.end(), thePlayerID) != thePlayers.end();
                    }
                    if (!theKeep) {
                        this->mUnits.erase(this->mUnits.begin() + i);
                        i--;
                    }
                }
            } else {
                gWorld.mScenarioManager->ChoosePlayer(this->mOwner, &thePlayers, false);
                for (unsigned long i = 0; i < thePlayers.size(); i++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[thePlayers[i]];
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL;
                         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (this->AcceptsClass(theUnit->mUnitClass->mUnitType)) {
                            unsigned long theID = theUnit->GetUnitID();
                            this->mUnits.push_back(theID);
                        }
                    }
                }
            }
            if (this->mUnits.size() < this->mMinCount)
                this->mUnits.clear();
        }
    }
    bool theResult = false;
    for (long i = 0; i < (long)this->mUnits.size(); i++) {
        EEUnit* theUnit = gWorld.GetUnit(this->GetUnitAt(i));
        bool theKeep = false;
        if (theUnit != NULL && (theIncludeDead || theUnit->GetHitPoints() > 0 || theUnit->mUnitClass->mIsTree)
            && (theUnit->mComplexUnit == NULL || theUnit->mComplexUnit->mPreviousPart == NULL)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (this->mHasStateCheck == 0)
                theKeep = true;
            else if (theComplex != NULL) {
                long theCommand = theComplex->GetCommandType();
                theKeep = this->mHasStateCheck == 1 ? theCommand == this->mState : theCommand != this->mState;
            }
            if (theKeep && this->mHasAttributeCheck != 0) {
                long theValue = this->GetUnitAttribute(this->mAttribute, this->mIsAttributePercent, theUnit, 0, 0);
                if (this->mHasAttributeCheck == 1)
                    theKeep = theValue >= (long)this->mAttributeMin && theValue <= (long)this->mAttributeMax;
                else
                    theKeep = theValue < (long)this->mAttributeMin || theValue > (long)this->mAttributeMax;
            }
            if (this->mSelectedByCheck != 0) {
                gWorld.mScenarioManager->ChoosePlayer(this->mSelectedByPlayer, &thePlayers, false);
                for (unsigned long j = 0; j < thePlayers.size(); j++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[thePlayers[j]];
                    if (this->mSelectedByCheck == 1)
                        theKeep = theKeep && (thePlayer->mActiveGroup->Contains(theComplex) || thePlayer->mViewedUnit == theUnit);
                    else
                        theKeep = theKeep && !thePlayer->mActiveGroup->Contains(theComplex) && thePlayer->mViewedUnit != theUnit;
                }
            }
            if (theKeep && this->mVisibleByCheck != 0) {
                gWorld.mScenarioManager->ChoosePlayer(this->mVisibleByPlayer, &thePlayers, false);
                for (unsigned long j = 0; j < thePlayers.size(); j++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[thePlayers[j]];
                    if (this->mVisibleByCheck == 1)
                        theKeep = theKeep && EEIsSeenBy(theUnit, thePlayer, false);
                    else
                        theKeep = theKeep && !EEIsSeenBy(theUnit, thePlayer, false);
                }
            }
            if (theKeep && this->mInAreaCheck != 0) {
                if (this->mInAreaCheck == 1)
                    theKeep = theArea != NULL && theArea->ContainsUnit(theUnit);
                else
                    theKeep = theArea != NULL && !theArea->ContainsUnit(theUnit);
            }
            if (theKeep && this->mNearCheck != 0) {
                if (this->mNearCheck == 1)
                    theKeep = theTarget != NULL && theTarget->IsNearUnit(theUnit, this->mNearMode, false);
                else
                    theKeep = theTarget != NULL && !theTarget->IsNearUnit(theUnit, this->mNearMode, false);
            }
        }
        if (!theKeep && theRebuilt == true) {
            this->mUnits.erase(this->mUnits.begin() + i);
            i--;
        } else {
            theResult = theResult || theKeep;
            if (theResult && !theRebuilt)
                break;
        }
    }
    if (theRebuilt == true) {
        if (this->mUnits.size() < this->mMinCount)
            this->mUnits.clear();
        else {
            EEAIRandomShuffle(this->mUnits.begin(), this->mUnits.end());
            if (this->mUnits.size() > this->mMaxCount)
                this->mUnits.erase(this->mUnits.begin() + this->mMaxCount, this->mUnits.end());
        }
        theResult = this->mUnits.size() > 0;
        if (theResult && !this->mIsDynamic)
            this->mIsSelectOnMap = true;
    }
    return theResult;
}


// FUNCTION: EXE 0x5c6b14
// Picks the unit type to create for the object: the parent's first unit's type, the object's class, or a random
// class matching its epoch/family/AI kind/unit set filter; -1 if none
long EESMObject::PickUnitType()
{
    std::vector<long> theClasses;
    std::vector<unsigned long> theUnits;
    EESMObject* theParent = gWorld.GetScenarioManager()->GetScenarioObject(this->GetParent());
    long theResult = -1;
    if (theParent != NULL && this->GetParent() != 0) {
        if (theParent->GetIsSelectOnMap()) {
            theParent->GetUnits(theUnits);
            if (theUnits.size() > 0) {
                EEUnit* theUnit = gWorld.GetUnit(theUnits[0]);
                if (theUnit != NULL)
                    theResult = theUnit->mUnitClass->GetUnitType();
            }
        } else {
            theResult = theParent->PickUnitType();
        }
    } else {
        if (this->GetClass() != 0)
            theResult = this->GetClass();
        else {
            for (long i = 0; i < 0x367; i++) {
                EEUnitClass* theClass = gUnitClassTable[i];
                if (theClass == NULL)
                    continue;
                long theCategory = theClass->mCategory;
                if (theCategory != kUnitCategoryProjectile && theCategory != kUnitCategoryBomb && theCategory != kUnitCategoryMissile && theCategory != kUnitCategoryTorpedo) {
                    if (this->GetFamily() == 0) {
                        long theType = theClass->GetUnitType();
                        theClasses.push_back(theType);
                    } else if (this->GetFamily() <= 0xf) {
                        if (this->GetFamily() == theClass->mEpoch + 1) {
                            long theType = theClass->GetUnitType();
                            theClasses.push_back(theType);
                        }
                    } else if (this->GetFamily() <= 0x4b) {
                        if (this->GetFamily() - 0x10 == theClass->mFamily) {
                            long theType = theClass->GetUnitType();
                            theClasses.push_back(theType);
                        }
                    } else if (this->GetFamily() <= 0x74) {
                        if (this->GetFamily() - 0x4c == theClass->mAIKind) {
                            long theType = theClass->GetUnitType();
                            theClasses.push_back(theType);
                        }
                    } else if (this->mFamily <= gUnitSetManager.GetSize() + 0x74) {
                        EEUnitSet* theSet = gUnitSetManager.GetSet(this->GetFamily() - 0x75);
                        if (theSet != NULL && theSet->Contains(theClass->GetUnitType())) {
                            long theType = theClass->GetUnitType();
                            theClasses.push_back(theType);
                        }
                    }
                }
            }
            if (theClasses.size() > 0)
                theResult = theClasses[gPhysics->GetRandom(0L, (long)theClasses.size() - 1)];
        }
    }
    return theResult;
}

#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"

// FUNCTION: EXE 0x5c6d4f
// Finds a free spot for a new unit of the type that meets the object's area and nearness filters (random tries for
// 'not in/near' filters, then one retry without them); returns whether one was found
bool EESMObject::FindPlacement(unsigned long inUnitType, GE2DPoint& outLocation, bool inTryRandom)
{
    std::vector<U2DPoint<long> > theCandidates;
    std::vector<U2DPoint<long> > theAreaTiles;
    std::vector<U2DPoint<long> > theObjectTiles;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOtherTiles;
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(this->mNearObject);
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mArea);
    unsigned long theHeight = gWorld.mMap->mL195610;
    unsigned long theWidth = gWorld.mMap->mL195614;
    bool theFound = false;
    bool theNoArea = false;
    bool theTriedRandom = false;
    if ((inTryRandom && this->mInAreaCheck == 2 && theArea != NULL) || (this->mNearCheck == 2 && theObject != NULL)) {
        theTriedRandom = true;
        if (this->mNearCheck == 2 && theObject != NULL)
            theObject->Evaluate(false);
        for (long i = 0; i < 25; ++i) {
            long theA = gPhysics->GetRandom(0L,theWidth - 1);
            long theB = gPhysics->GetRandom(0L,theHeight - 1);
            long theX = theB + theA;
            long theY = theHeight + theB - theA;
            bool theAllowed = true;
            if (this->mInAreaCheck == 2 && theArea != NULL) {
                U2DPoint<long> theTile((long)(float)theX,(long)(float)theY);
                if (theArea->ContainsTile(theTile))
                    theAllowed = false;
            }
            if (this->mNearCheck == 2 && theObject != NULL && theObject->IsNearTile(theX,theY,this->mNearMode,false))
                theAllowed = false;
            if (theAllowed) {
                U2DPoint<long> theTile(theX,theY);
                theCandidates.push_back(theTile);
            }
        }
    } else {
        if (this->mInAreaCheck == 1 && theArea != NULL) {
            if (theArea->IsUnset()) {
                theNoArea = true;
                outLocation.mX = -10000.0f;
                theFound = true;
                outLocation.mY = -10000.0f;
            } else {
                if (!theArea->IsOffMap())
                    theArea->GetTiles(theAreaTiles);
            }
        } else if (this->mInAreaCheck == 2 && theArea != NULL) {
            // the area's own tiles, to leave out below
            if (!theArea->IsUnset() && theArea->mSelectMode != 0)
                theArea->GetTiles(theTiles);
            for (long row = 0; row < theWidth; ++row) {
                long x = row;
                long y = row + theHeight;
                for (long count = theHeight; count != 0; --count, ++x, --y) {
                    U2DPoint<long> theTile(x,y);
                    if (theArea->IsUnset())
                        theAreaTiles.push_back(theTile);
                    else if (theArea->mSelectMode == 0) {
                        if (!theArea->ContainsTile(theTile))
                            theAreaTiles.push_back(theTile);
                    } else if (std::find(theTiles.begin(),theTiles.end(),theTile) == theTiles.end())
                        theAreaTiles.push_back(theTile);
                    U2DPoint<long> theNextTile(x + 1,y);
                    if (theArea->IsUnset())
                        theAreaTiles.push_back(theNextTile);
                    else if (theArea->mSelectMode == 0) {
                        if (!theArea->ContainsTile(theNextTile))
                            theAreaTiles.push_back(theNextTile);
                    } else if (std::find(theTiles.begin(),theTiles.end(),theNextTile) == theTiles.end())
                        theAreaTiles.push_back(theNextTile);
                }
            }
        }
        if (this->mNearCheck != 0 && theObject != NULL
            && ((theObject->mSelectedByCheck != 0 && theObject->mUnits.size() > 0) || theObject->Evaluate(false))) {
            for (unsigned long i = 0; i < theObject->mUnits.size(); ++i) {
                EEUnit* theUnit = gWorld.GetUnit(theObject->mUnits[i]);
                if (theUnit != NULL && theUnit->mHitPoints > 0) {
                    theTiles.clear();
                    theOtherTiles.clear();
                    switch (this->mNearMode) {
                    case 0:
                        if (theUnit->mComplexUnit->GetLineOfSight() > 0.0f) {
                            GE3DPoint thePoint((float)theUnit->GetTileX(),(float)theUnit->GetTileY(),0.0f);
                            Fn24B212(thePoint,theUnit->mComplexUnit->GetLineOfSight(),theTiles);
                        }
                        break;
                    case 1:
                        if (theUnit->mStats->mRange > 0.0f) {
                            GE3DPoint thePoint((float)theUnit->GetTileX(),(float)theUnit->GetTileY(),0.0f);
                            Fn24B212(thePoint,theUnit->mStats->mRange,theTiles);
                        }
                        break;
                    case 2:
                        EEGetSideTiles(theUnit->GetTileX(),theUnit->GetTileY(),theUnit->mUnitClass->mTileSize,theTiles);
                        EEGetCornerTiles(0,theUnit->GetTileX(),theUnit->GetTileY(),theUnit->mUnitClass->mTileSize,theOtherTiles);
                        break;
                    }
                }
                for (unsigned long j = 0; j < theTiles.size(); ++j)
                    theObjectTiles.push_back(theTiles[j]);
                for (unsigned long k = 0; k < theOtherTiles.size(); ++k)
                    theObjectTiles.push_back(theOtherTiles[k]);
            }
            if (this->mNearCheck == 2) {
                theTiles = theObjectTiles;
                theObjectTiles.clear();
                for (long row = 0; row < theWidth; ++row) {
                    long x = row;
                    long y = row + theHeight;
                    for (long count = theHeight; count != 0; --count, ++x, --y) {
                        U2DPoint<long> theTile(x,y);
                        if (std::find(theTiles.begin(),theTiles.end(),theTile) == theTiles.end())
                            theObjectTiles.push_back(theTile);
                    }
                }
            }
        }
        if (this->mInAreaCheck == 0) {
            if (this->mNearCheck != 0)
                theCandidates = theObjectTiles;
        } else if (this->mNearCheck == 0)
            theCandidates = theAreaTiles;
        else {
            for (unsigned long i = 0; i < theAreaTiles.size(); ++i) {
                if (std::find(theObjectTiles.begin(),theObjectTiles.end(),theAreaTiles[i]) != theObjectTiles.end())
                    theCandidates.push_back(theAreaTiles[i]);
            }
        }
        if (theNoArea)
            return theFound;
    }
    long theSize = gUnitClassTable[inUnitType]->mTileSize;
    EEAIRandomShuffle(theCandidates.begin(),theCandidates.end());
    for (std::vector<U2DPoint<long> >::iterator i = theCandidates.begin(); !theFound && i != theCandidates.end(); ++i) {
        long theX = i->mX;
        long theY = i->mY;
        if (EEPlayerBlockTakes(NULL,inUnitType,theX,theY,theSize,theSize,0)) {
            if (theSize == 1) {
                theFound = true;
                outLocation.mX = (float)theX;
                outLocation.mY = (float)theY;
            } else {
                for (unsigned long j = 0; j < theCandidates.size(); ++j) {
                    if (theCandidates[j].mX == theSize - 1 + theX && theCandidates[j].mY == theSize - 1 + theY) {
                        theFound = true;
                        outLocation.mX = (float)theX;
                        outLocation.mY = (float)theY;
                        break;
                    }
                }
                // The original type 0x41 test is independent of whether the far footprint corner was found.
                if ((theFound && inUnitType == 0x24) || inUnitType == 0x41) {
                    theFound = false;
                    for (long x = theX - 1; !theFound && x <= theX + theSize; ++x) {
                        for (long y = theY - 1; !theFound && y <= theY + theSize; ++y) {
                            EEMapTile* theMapTile = gWorld.mMap->GetTile(x,y);
                            if (theMapTile != NULL && theMapTile->mTerrainType != kTerrainDeepWater && theMapTile->mTerrainType != kTerrainShallowWater)
                                theFound = true;
                        }
                    }
                }
            }
        }
    }
    if (!theFound && inTryRandom && theTriedRandom)
        theFound = this->FindPlacement(inUnitType,outLocation,false);
    return theFound;
}

// FUNCTION: EXE 0x5c7749
// Returns a unit's attribute for an object filter: current values, upgrade bonuses over the class, or totals,
// optionally as a percentage of the class or upgrade value; 0 for a NULL unit
long EESMObject::GetUnitAttribute(unsigned long inAttribute, bool inPercent, EEUnit* inUnit,
    long inArg4, long inArg5)
{
    long theResult = 0;
    if (inUnit == NULL)
        return theResult;
    EEComplexUnit* theComplex = inUnit->GetComplexUnit();
    EEUnitClass* theClass = inUnit->GetUnitClass();
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    switch (inAttribute) {
    case kSMUnitAttributeHitPoints:
        theResult = inUnit->GetHitPoints();
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mMaxHitPoints;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mMaxHitPoints)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeStone:
        if (inUnit->mResourceType != kResourceStone) break;
        goto check_carrying;
    case kSMUnitAttributeGold:
        if (inUnit->mResourceType != kResourceGold) break;
        goto check_carrying;
    case kSMUnitAttributeWood:
        if (inUnit->mResourceType != kResourceWood) break;
        goto check_carrying;
    case kSMUnitAttributeFood:
        if (inUnit->mResourceType != kResourceFood) break;
        goto check_carrying;
    case kSMUnitAttributeIron:
        if (inUnit->mResourceType != kResourceIron) break;
    case kSMUnitAttributeAnyResource:
    check_carrying:
        theResult = inUnit->mResourceAmount;
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mCarryAmount;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mCarryAmount)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeFlightTime:
        if (theComplex == NULL) break;
        theResult = (long)(theComplex->mFlightTime / 1000.0f);
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mFlightTime;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mFlightTime)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeCargoCount:
        if (theComplex == NULL) break;
        theResult = theComplex->GetNumCargoUnits();
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mMaxObjectCarry;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mMaxObjectCarry)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeGarrisonLevel:
        if (theComplex == NULL) break;
        theResult = theComplex->mGarrisonCount;
        if (inPercent && theClass != NULL) {
            float theCapacity = (float)theClass->mGarrisonCapacity;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theClass->mGarrisonCapacity)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeAmmunition:
        if (theComplex == NULL) break;
        theResult = theComplex->mAmmunition;
        if (inPercent && theClass != NULL) {
            // Original 1c78d1..de zero-extends capacity +0x29c before FILD; the signed field spelling remains provisional.
            float theCapacity = (float)(unsigned long)theClass->mAmmunition;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theClass->mAmmunition)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeAnimationIsPaused:
        if (theComplex != NULL) theResult = theComplex->mIsAnimationPaused;
        break;
    case kSMUnitAttributeReloadTime:
        if (theComplex == NULL) break;
        theResult = theComplex->mReloadTimer;
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mReloadTime;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mReloadTime)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeGraphicScale:
        if (theComplex != NULL) {
            theResult = (long)theComplex->mPhysicalModel.GetScale();
            if (inPercent && theStats != NULL) {
                if (theStats->mScale > 0.0f) {
                    if (theResult == 0) break;
                    if (theResult != (long)theStats->mScale)
                        { theResult = (long)(theResult * 100.0f / theStats->mScale); break; }
                }
                { theResult = 100; break; }
            }
        } else {
            if (theStats != NULL) return UMFloatToLong(theStats->mScale);
            { theResult = 1; break; }
        }
        break;
    case kSMUnitAttributePower:
        if (theComplex == NULL) break;
        theResult = theComplex->GetPower();
        if (inPercent && theStats != NULL) {
            float theCapacity = (float)theStats->mPower;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theStats->mPower)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeRottingTime:
        if (theComplex == NULL) break;
        theResult = theComplex->mRottingAwayTime;
        if (inPercent && theClass != NULL) {
            float theCapacity = (float)theClass->mRottingTime;
            if (theCapacity > 0.0f) {
                if (theResult == 0) break;
                if (theResult != theClass->mRottingTime)
                    { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeConstructionComplete:
        if (theComplex != NULL) theResult = theComplex->mIsBuilt;
        break;
    case kSMUnitAttributeUnitVariable1:
        if (theComplex != NULL) theResult = theComplex->mUnitVariables[0];
        break;
    case kSMUnitAttributeUnitVariable2:
        if (theComplex != NULL) theResult = theComplex->mUnitVariables[1];
        break;
    case kSMUnitAttributeUnitVariable3:
        if (theComplex != NULL) theResult = theComplex->mUnitVariables[2];
        break;
    case kSMUnitAttributeShockArmorPlus:
        theResult = theStats->mArmor[0] - theStats->mUnitClass->mArmor[0];
        goto attribute_42;
    case kSMUnitAttributeArrowArmorPlus:
        theResult = theStats->mArmor[1] - theStats->mUnitClass->mArmor[1];
        goto attribute_44;
    case kSMUnitAttributePierceArmorPlus:   // armor slot 2, as the attribute effect uses (shared the arrow body)
        theResult = theStats->mArmor[2] - theStats->mUnitClass->mArmor[2];
        goto attribute_pierce;
    case kSMUnitAttributeGunArmorPlus:
        theResult = theStats->mArmor[3] - theStats->mUnitClass->mArmor[3];
        goto attribute_45;
    case kSMUnitAttributeLaserArmorPlus:
        theResult = theStats->mArmor[4] - theStats->mUnitClass->mArmor[4];
        goto attribute_46;
    case kSMUnitAttributeMissileArmorPlus:
        theResult = theStats->mArmor[5] - theStats->mUnitClass->mArmor[5];
        goto attribute_47;
    case kSMUnitAttributeReloadTimePlus:
        theResult = theStats->mReloadTime - theStats->mUnitClass->mReloadTime;
        goto attribute_48;
    case kSMUnitAttributeAttackPlus:
        theResult = (long)(theStats->mAttack - theStats->mUnitClass->mAttack);
        goto attribute_49;
    case kSMUnitAttributeWorkRatePlus:
        theResult = theStats->mWorkRate - theStats->mUnitClass->mWorkRate;
        goto attribute_50;
    case kSMUnitAttributeFlightTimePlus:
        theResult = (long)((theStats->mFlightTime - theStats->mUnitClass->mFlightTime) / 1000.0f);
        goto attribute_51;
    case kSMUnitAttributeLeadershipPlus:
        theResult = (long)(theStats->mLeadership - theStats->mUnitClass->mLeadership);
        goto attribute_52;
    case kSMUnitAttributeMaxObjectsCarriedPlus:
        theResult = theStats->mMaxObjectCarry - theStats->mUnitClass->mMaxObjectCarry;
        goto attribute_53;
    case kSMUnitAttributeRepairPlus:
        theResult = theStats->mRepairRate - theStats->mUnitClass->mRepairRate;
        goto attribute_54;
    case kSMUnitAttributeHitPointsPlus:
        theResult = theStats->mMaxHitPoints - theStats->mUnitClass->mHitPoints;
        goto attribute_55;
    case kSMUnitAttributeCarryAmountPlus:
        theResult = theStats->mCarryAmount - theStats->mUnitClass->mCarryAmount;
        goto attribute_56;
    case kSMUnitAttributeContainerHealingHPPerSecPlus:
        theResult = theStats->mHealingPoints - theStats->mUnitClass->mHealingPoints;
        goto attribute_57;
    case kSMUnitAttributeRangePlus:
        theResult = (long)(theStats->mRange - theStats->mUnitClass->mRange);
        goto attribute_58;
    case kSMUnitAttributeLineOfSightPlus:
        theResult = (long)(theStats->mLineOfSight - theStats->mUnitClass->mLineOfSight);
        goto attribute_59;
    case kSMUnitAttributeMaxVelocityPlus:
        theResult = (long)((theStats->mMaxVelocity - theStats->mUnitClass->mMaxVelocity) * 20.0f);
        goto attribute_60;
    case kSMUnitAttributePowerPlus:
        theResult = theStats->mPower - theStats->mUnitClass->mPower;
        if (inPercent) {
            if (theClass->mPower > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mPower); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeAnimalAIActivePlus:
        theResult = theStats->mIsInitialized;
        break;
    case kSMUnitAttributeCameraFocusPlus:
        theResult = (long)(theStats->mCameraFocus * 100.0f);
        break;
    case kSMUnitAttributeTotalShockArmor:
        theResult = theStats->mArmor[0];
    attribute_42:
        if (inPercent) {
            if (theClass->mArmor[0] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[0]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalArrowArmor:
        theResult = theStats->mArmor[1];
    attribute_44:
        if (inPercent) {
            if (theClass->mArmor[1] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[1]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalPierceArmor:   // armor slot 2 (shared the arrow body)
        theResult = theStats->mArmor[2];
    attribute_pierce:
        if (inPercent) {
            if (theClass->mArmor[2] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[2]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalGunArmor:
        theResult = theStats->mArmor[3];
    attribute_45:
        if (inPercent) {
            if (theClass->mArmor[3] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[3]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalLaserArmor:
        theResult = theStats->mArmor[4];
    attribute_46:
        if (inPercent) {
            if (theClass->mArmor[4] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[4]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalMissileArmor:
        theResult = theStats->mArmor[5];
    attribute_47:
        if (inPercent) {
            if (theClass->mArmor[5] > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mArmor[5]); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalReloadTime:
        theResult = theStats->mReloadTime;
    attribute_48:
        if (inPercent) {
            if (theClass->mReloadTime > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mReloadTime); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalAttack:
        theResult = (long)(theStats->mAttack);
    attribute_49:
        if (inPercent) {
            if (theClass->mAttack > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mAttack); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalWorkRate:
        theResult = theStats->mWorkRate;
    attribute_50:
        if (inPercent) {
            if (theClass->mWorkRate > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mWorkRate); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalFlightTime:
        theResult = (long)(theStats->mFlightTime / 1000.0f);
    attribute_51:
        if (inPercent) {
            if (theClass->mFlightTime > 0)
                { theResult = (long)(theResult * 100.0f / (theClass->mFlightTime / 1000.0f)); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalLeadership:
        theResult = (long)(theStats->mLeadership);
    attribute_52:
        if (inPercent) {
            if (theClass->mLeadership > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mLeadership); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalMaxObjectsCarried:
        theResult = theStats->mMaxObjectCarry;
        if (theResult == 0) {
            theResult = theClass->mGarrisonCapacity;
            if (inPercent) { theResult = 100; break; }
            break;
        }
    attribute_53:
        if (inPercent) {
            if (theClass->mMaxObjectCarry > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mMaxObjectCarry); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalRepair:
        theResult = theStats->mRepairRate;
    attribute_54:
        if (inPercent) {
            if (theClass->mRepairRate > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mRepairRate); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalHitPoints:
        theResult = theStats->mMaxHitPoints;
    attribute_55:
        if (inPercent) {
            if (theClass->mHitPoints > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mHitPoints); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalCarryAmount:
        theResult = theStats->mCarryAmount;
    attribute_56:
        if (inPercent) {
            if (theClass->mCarryAmount > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mCarryAmount); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalHealingHPPerSec:
        theResult = theStats->mHealingPoints;
    attribute_57:
        if (inPercent) {
            if (theClass->mHealingPoints > 0)
                { theResult = (long)(theResult * 100.0f / theClass->mHealingPoints); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalRange:
        theResult = (long)(theStats->mRange);
    attribute_58:
        if (inPercent) {
            if (theClass->mRange > 0.0f)
                { theResult = (long)(theResult * 100.0f / theClass->mRange); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalLineOfSight:
        theResult = (long)(theStats->mLineOfSight);
    attribute_59:
        if (inPercent) {
            if (theClass->mLineOfSight > 0.0f)
                { theResult = (long)(theResult * 100.0f / theClass->mLineOfSight); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalMaxVelocity:
        theResult = (long)(theStats->mMaxVelocity * 20.0f);
    attribute_60:
        if (inPercent) {
            if (theClass->mMaxVelocity > 0.0001f)
                { theResult = (long)(theResult * 100.0f / (theClass->mMaxVelocity * 20.0f)); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalPower:
        theResult = theStats->mPower;
        if (inPercent) {
            float theCapacity = (float)theClass->mPower;
            if (theCapacity > 0.0f)
                { theResult = (long)(theResult * 100.0f / theCapacity); break; }
            { theResult = 100; break; }
        }
        break;
    case kSMUnitAttributeTotalPlaceHolder2:
    case kSMUnitAttributeTotalPlaceHolder3:
        break;
    case kSMUnitAttributeTotalTransportWeight:
        theResult = theClass->mTransportCapacity;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalWeight:
        theResult = theClass->mWeight;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalTileSize:
        theResult = theClass->mTileSize;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalCruiseAltitude:
        theResult = (long)theClass->GetCruiseAltitude();
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalMinimumRange:
        theResult = (long)theClass->mMinRange;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalCanEnterAirTransports:
        theResult = theClass->mCanEnterAirTransports != 0;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalCanEnterLandTransports:
        theResult = theClass->mCanEnterLandTransports != 0;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalCanEnterSeaTransports:
        theResult = theClass->mCanEnterSeaTransports != 0;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalPlaceHolder1:
        theResult = theClass->mCanEnterFortresses != 0;
        if (inPercent) { theResult = 100; break; }
        break;
    case kSMUnitAttributeTotalPlaceHolder2B:
    case kSMUnitAttributeTotalPlaceHolder3B:
        if (inPercent) { theResult = 100; break; }
        break;
    }
    return theResult;
}


// FUNCTION: EXE 0x5c8274
// Copies the object's unit ids out
void EESMObject::GetUnits(std::vector<unsigned long>& outUnits) const
{
    outUnits = this->mUnits;
}

// FUNCTION: EXE 0x5c8284
// Reads the object's units and filter settings (files before 0.0xcd: attribute ids above Construction Complete
// move up by 3); returns the first error
long EESMObject::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long theUnit = 0;
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile.Read(theUnit);
            if (theResult != kErrorOK)
                break;
            this->mUnits.push_back(theUnit);
        }
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsSelectOnMap);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOwner);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mFamily);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mClass);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mParent);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMinCount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMaxCount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsDynamic);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mInAreaCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNearCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasAttributeCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mVisibleByCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasStateCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSelectedByCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mArea);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNearMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNearObject);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttribute);
    if (theResult == kErrorOK) {
        if (inMajorVersion <= 0 && inMinorVersion < 0xcd && this->mAttribute > kSMUnitAttributeConstructionComplete)
            this->mAttribute += 3;
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeMin);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttributeMax);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mVisibleByPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSelectedByPlayer);
    if (theResult == kErrorOK && inMinorVersion >= 0x15)
        theResult = inFile.Read(this->mIsAttributePercent);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mL64);
    return theResult;
}

// FUNCTION: EXE 0x5c84db
// Updates the object's area index after areas inOld/inNew swap, or after inOld is deleted (inNew -1)
void EESMObject::RenumberAreas(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mArea;
    if (theIndex == inOldIndex) {
        this->mArea = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mArea = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mArea = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5c8511
// Updates the parent and near-object indices after objects inOld/inNew swap, or after inOld is deleted
void EESMObject::RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mParent;
    if (theIndex == inOldIndex) {
        this->mParent = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mParent = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mParent = theIndex - 1;
    }
    theIndex = this->mNearObject;
    if (theIndex == inOldIndex) {
        this->mNearObject = (inNewIndex != 0xffffffff) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mNearObject = inOldIndex;
    } else if (inNewIndex == 0xffffffff && theIndex > inOldIndex) {
        this->mNearObject = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5c856e
// Maps the object's area index through the ID map
void EESMObject::RemapAreas(EESMIDMap& inMap)
{
    EERemapID(this->mArea, inMap);
}

// FUNCTION: EXE 0x5c8580
// Maps the object's parent and near-object indices through the ID map
void EESMObject::RemapObjects(EESMIDMap& inMap)
{
    EERemapID(this->mParent, inMap);
    EERemapID(this->mNearObject, inMap);
}

// FUNCTION: EXE 0x5c85a4
// Replaces every occurrence of a unit id in the object's unit list by a new id
void EESMObject::ReplaceUnit(long inOldUnit, long inNewUnit)
{
    long theCount = this->mUnits.size();
    for (long i = 0; i < theCount; i++) {
        unsigned long& theUnit = this->mUnits[i];
        if (theUnit == inOldUnit)
            theUnit = inNewUnit;
    }
}

// FUNCTION: EXE 0x5c85de
// Writes the object's units and filter settings (the parent only for minor version >= 0x2a); returns the first error
long EESMObject::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    unsigned long theCount = this->mUnits.size();
    if (theResult == kErrorOK)
        theResult = outFile.Write(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            unsigned long theUnit = this->GetUnitAt(i);
            theResult = outFile.Write(theUnit);
            if (theResult != kErrorOK)
                break;
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsSelectOnMap);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOwner);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFamily);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mClass);
    if (theResult == kErrorOK && inMinorVersion >= 0x2a)
        theResult = outFile.Write(this->mParent);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinCount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxCount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsDynamic);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mInAreaCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNearCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasAttributeCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mVisibleByCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasStateCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSelectedByCheck);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mArea);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNearMode);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNearObject);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttributeMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mVisibleByPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mState);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSelectedByPlayer);
    if (theResult == kErrorOK && inMinorVersion >= 0x15)
        theResult = outFile.Write(this->mIsAttributePercent);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mL64);
    return theResult;
}

#include "ee/EESMEffect.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"

// FUNCTION: EXE 0x5c8871
// Raises or lowers a player's unit type stat by an amount or percent of the class value (armor: the bonus over the
// class base changes and is clamped at 0), clamped, and queues the change as a pending effect
// Original cdecl helper. Parameter types follow the dispatcher's pushes and the actual player/upgrade readers.
// Every case is written out in full (the original reads inUpgrade->mUnitClass per case; the int cases' shared
// add/clamp/store tail is VC6 tail merging); theAdd is the original's sete (negate when it is false).
void EEModifyUnitAttribute(unsigned long inAttribute, bool inPercentage, EEPlayer* inPlayer,
                EEUnitClassUpgrade* inUpgrade, long inAmount, long inMode)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEHerd*> theHerds;
    EESMEffect theEffect;
    bool theAdd = inMode == 0;
    long theAmount = inAmount;
    float theFloat;
    EEUnit* theNode;
    long theArmor = 0;
    theEffect.mAttributeUnit = (unsigned long)-1;
    theEffect.mType = kSMEffectObject;
    theEffect.mObjectAction = kSMObjectActionUnitAttribute;
    theEffect.mAttributeUnitType = inUpgrade != NULL ? inUpgrade->mUnitClass->GetUnitType() : 0;
    theEffect.mAttributePlayer = inPlayer != NULL ? inPlayer->GetPlayerID() : -1;
    theEffect.mObject = 0;
    theEffect.mAttribute = inAttribute;
    theEffect.mUsesPercent = inPercentage;
    theEffect.mAttributeAmount = inAmount;
    theEffect.mAttributeMode = inMode;
    if (inUpgrade == NULL)
        return;
    switch (inAttribute) {
    case kSMUnitAttributeShockArmorPlus: case kSMUnitAttributeArrowArmorPlus: case kSMUnitAttributePierceArmorPlus: case kSMUnitAttributeGunArmorPlus: case kSMUnitAttributeLaserArmorPlus: case kSMUnitAttributeMissileArmorPlus: {
        switch (inAttribute) {
        case kSMUnitAttributeShockArmorPlus: theArmor = 0; break;
        case kSMUnitAttributeArrowArmorPlus: theArmor = 1; break;
        case kSMUnitAttributePierceArmorPlus: theArmor = 2; break;
        case kSMUnitAttributeGunArmorPlus: theArmor = 3; break;
        case kSMUnitAttributeLaserArmorPlus: theArmor = 4; break;
        case kSMUnitAttributeMissileArmorPlus: theArmor = 5; break;
        }
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mArmor[theArmor] * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = (inUpgrade->mArmor[theArmor] - (float)inUpgrade->mUnitClass->mArmor[theArmor]) + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        // the clamped bonus goes back on top of the class base armor (the upgrade holds the total armor)
        inUpgrade->mArmor[theArmor] = theFloat + (float)inUpgrade->mUnitClass->mArmor[theArmor];
        break;
    }
    case kSMUnitAttributeReloadTimePlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mReloadTime * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mReloadTime;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mReloadTime = inAmount;
        break;
    case kSMUnitAttributeAttackPlus:
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mAttack * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetAttack() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->mAttack = theFloat;
        break;
    case kSMUnitAttributeWorkRatePlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mWorkRate * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mWorkRate;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mWorkRate = inAmount;
        break;
    case kSMUnitAttributeFlightTimePlus: {
        long theChange;
        if (inPercentage)
            theChange = UMFloatToLong((float)inUpgrade->mUnitClass->mFlightTime * inAmount * 0.01f);
        else
            theChange = (long)(inAmount / 0.001f + 0.5f);
        if (!theAdd)
            theChange = -theChange;
        theChange += inUpgrade->mFlightTime;
        if (theChange < 0)
            theChange = 0;
        inUpgrade->mFlightTime = theChange;
        break;
    }
    case kSMUnitAttributeLeadershipPlus:
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mLeadership * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetLeadership() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->mLeadership = theFloat;
        break;
    case kSMUnitAttributeMaxObjectsCarriedPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mMaxObjectCarry * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mMaxObjectCarry;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mMaxObjectCarry = inAmount;
        break;
    case kSMUnitAttributeRepairPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mRepairRate * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mRepairRate;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mRepairRate = inAmount;
        break;
    case kSMUnitAttributeHitPointsPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mHitPoints * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mMaxHitPoints;
        if (inAmount < 0)
            inAmount = 0;
        if (inAmount <= 1)
            inAmount = 1;
        inUpgrade->mMaxHitPoints = inAmount;
        break;
    case kSMUnitAttributeCarryAmountPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mCarryAmount * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mCarryAmount;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mCarryAmount = inAmount;
        break;
    case kSMUnitAttributeContainerHealingHPPerSecPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mHealingPoints * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mHealingPoints;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mHealingPoints = inAmount;
        break;
    case kSMUnitAttributeRangePlus:
        if (inPercentage)
            theAmount = UMFloatToLong(inUpgrade->mUnitClass->GetRange() * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetRange() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->SetRange(theFloat);
        break;
    case kSMUnitAttributeLineOfSightPlus: {
        if (inPercentage)
            theAmount = UMFloatToLong(inUpgrade->mUnitClass->GetLineOfSight() * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        float theSpeed = inUpgrade->GetLineOfSight() + theFloat;
        if (theSpeed < 0.0f)
            theSpeed = 0.0f;
        for (theNode = inPlayer->mFirstUnit; theNode != NULL;
             theNode = static_cast<EEUnit*>(theNode->mNext)) {
            if (theNode->mComplexUnit != NULL && theNode->mUnitClass == inUpgrade->mUnitClass) {
                theNode->mComplexUnit->SightOffHere();
                theUnits.push_back(theNode);
            }
        }
        inUpgrade->mLineOfSight = theSpeed;
        for (unsigned long i = 0; i < theUnits.size(); i++)
            theUnits[i]->SightOnHere();
        break;
    }
    case kSMUnitAttributeMaxVelocityPlus:
        if (inPercentage)
            theFloat = inUpgrade->mUnitClass->GetMaxVelocity() * inAmount * 0.01f;
        else
            theFloat = (float)inAmount / 20.0f;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetMaxVelocity() + theFloat;
        if (theFloat < 0.0001f)
            theFloat = 0.0001f;
        inUpgrade->SetMaxVelocity(theFloat);
        break;
    case kSMUnitAttributePowerPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mPower * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mPower;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mPower = inAmount;
        break;
    case kSMUnitAttributeAnimalAIActivePlus: {
        if (inPercentage)
            inAmount = inAmount > 0;
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mIsInitialized != false;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mIsInitialized = inAmount > 0;
        inPlayer->GetHerdsOfType(theHerds, inUpgrade->mUnitClass->mUnitType);
        for (long j = (long)theHerds.size() - 1; j >= 0; --j)
            theHerds[j]->mIsActive = inUpgrade->mIsInitialized;
        break;
    }
    case kSMUnitAttributeCameraFocusPlus:
        if (!theAdd)
            inAmount = -inAmount;
        theAmount = inAmount + (long)(inUpgrade->mCameraFocus * 100.0f);
        if (theAmount < 0)
            theAmount = 0;
        inUpgrade->mCameraFocus = (float)theAmount * 0.01f;
        break;
    }
    gWorld.mScenarioManager->AddV134Effect(theEffect);
}

#include "ee/EEUnitAction.h"
#include "ee/EEUGLoadGarrison.h"

// FUNCTION: EXE 0x5c8f28
// Adds to, subtracts from or sets one unit's attribute (hit points, cargo, flight time, garrison, ammo, scale, power,
// rot time, built state, variables) by a fixed, random or percentage amount
// Original cdecl helper; random range and the percentage flag are independent of add/subtract/assign mode.
void EEChangeUnit(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode)
{
    std::vector<unsigned long> theUnused;
    std::vector<EEComplexUnit*> theUnits;
    EESMEffect theEffect;
    EEUnitClass* theClass = inUnit != NULL ? inUnit->GetUnitClass() : NULL;
    EEComplexUnit* theComplex = inUnit != NULL ? inUnit->GetComplexUnit() : NULL;
    EEUnitClassUpgrade* theStats = inUnit != NULL ? inUnit->GetStats() : NULL;
    theEffect.mType = kSMEffectObject;
    theEffect.mObjectAction = kSMObjectActionUnitAttribute;
    theEffect.mAttributeUnit = inUnit != NULL ? inUnit->GetUnitID() : (unsigned long)-1;
    theEffect.mAttributeUnitType = theClass != NULL ? theClass->GetUnitType() : 0;
    theEffect.mAttributePlayer = inUnit != NULL ? inUnit->GetPlayer()->GetPlayerID() : -1;
    theEffect.mUsesPercent = false;
    theEffect.mObject = 0;
    theEffect.mAttribute = inAttribute;
    theEffect.mAttributeMode = inMode;
    long theAmount = inMaximum != 0 ? gPhysics->GetRandom(inMinimum, (long)inMaximum) : inMinimum;
    if (inRandom) {
        if (!inPercentage)
            theAmount = gPhysics->GetRandom(0, inMinimum);
        else {
            theAmount = gPhysics->GetRandom(0L, inMinimum + (long)(inMaximum > 100 ? 0 : 100 - inMaximum));
            if (theAmount >= inMinimum)
                theAmount += 1 + (inMaximum - inMinimum);
        }
    }
    long* theValue = NULL;
    long theBase = 0;
    switch (inAttribute) {
    case kSMUnitAttributeHitPoints:
        if (inUnit != NULL && inUnit->GetHitPoints() > 0) {
            if (inPercentage) theAmount = UMFloatToLong((float)theStats->mMaxHitPoints * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += inUnit->GetHitPoints();
            else if (inMode == kSMAttributeModeDecrease) theAmount = inUnit->GetHitPoints() - theAmount;
            if (theAmount < 1) {
                if (inUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource) {
                    inUnit->SetResourceAmount(0);
                    inUnit->GetPlayer()->UnitTaken(inUnit, false);
                    inUnit->GetPlayer()->KillUnit(inUnit, false);
                } else
                    inUnit->Kill(0, 0, 0);
            } else
                inUnit->SetHitPoints(theAmount);
        }
        break;
    case kSMUnitAttributeAnyResource: case kSMUnitAttributeStone: case kSMUnitAttributeGold: case kSMUnitAttributeWood: case kSMUnitAttributeFood: case kSMUnitAttributeIron: {
        if (inPercentage) theAmount = UMFloatToLong((float)theStats->mCarryAmount * theAmount * 0.01f);
        if (inMode == kSMAttributeModeIncrease) theAmount += inUnit->mResourceAmount;
        else if (inMode == kSMAttributeModeDecrease) theAmount = inUnit->mResourceAmount - theAmount;
        if (theAmount < 0) theAmount = 0;
        unsigned long theState = 6;
        if (inAttribute == kSMUnitAttributeAnyResource) {
            if (theComplex == NULL || theComplex->mCommand == NULL || theComplex->mCommand->mType != kActionGatherResource) {
                inUnit->SetResourceAmount(theAmount);
                inUnit->SetResourceType(gPhysics->GetRandom(2L, 6L));
            }
            theState = 4;
        } else if (inAttribute == kSMUnitAttributeStone) theState = 4;
        else if (inAttribute == kSMUnitAttributeGold) theState = 5;
        else if (inAttribute == kSMUnitAttributeWood) theState = 3;
        else if (inAttribute == kSMUnitAttributeFood) theState = 2;
        if (theComplex != NULL && theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionGatherResource
            && theComplex->mResourceType != theState)
            break;
        inUnit->SetResourceAmount(theAmount);
        inUnit->SetResourceType(theState);
        break;
    }
    case kSMUnitAttributeFlightTime:
        if (theComplex != NULL) {
            long theTime = inPercentage ? UMFloatToLong((float)theStats->mFlightTime * theAmount * 0.01f)
                                       : (long)(theAmount / 0.001f + 0.5f);
            if (inMode == kSMAttributeModeIncrease) theTime += theComplex->mFlightTime;
            else if (inMode == kSMAttributeModeDecrease) theTime = theComplex->mFlightTime - theTime;
            if (theTime < 0) theTime = 0;
            theComplex->SetFlightTime(theTime);
        }
        break;
    case kSMUnitAttributeGarrisonLevel:
        if (theComplex != NULL && theComplex->GetUnitClass()->mGarrisonCapacity > 0) {
            if (inPercentage) theAmount = UMFloatToLong((float)theClass->mGarrisonCapacity * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += theComplex->mGarrisonCount;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theComplex->mGarrisonCount - theAmount;
            if (theAmount < 0) theAmount = 0;
            long theDelta = theAmount - theComplex->mGarrisonCount;
            if (theComplex->GetUnitClass()->mGarrisonCapacity < theComplex->mGarrisonCount + theDelta)
                theDelta = theComplex->GetUnitClass()->mGarrisonCapacity - theComplex->mGarrisonCount;
            theComplex->AddGarrison(theDelta, theComplex->mGarrisonPopulation);
            if (theComplex->mGarrisonCount == theComplex->GetUnitClass()->mGarrisonCapacity) {
                EEUnitGoal* theGoal = new EEUGLoadGarrison(theComplex, theUnits, false);
                theComplex->QueueGoal(theGoal, 0);
                theComplex->StartAction(kActionWaitForGarrisonCost);
                theComplex->GetUnitClass()->ActionChanged(theComplex);
            }
        }
        break;
    case kSMUnitAttributeAmmunition:
        if (theComplex != NULL) {
            if (inPercentage) theAmount = UMFloatToLong((float)(unsigned long)theClass->mAmmunition * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += theComplex->mAmmunition;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theComplex->mAmmunition - theAmount;
            if (theAmount < 0) theAmount = 0;
            if (theComplex->mAmmunition != (unsigned long)theAmount)
                theComplex->mAmmunition = theAmount;
        }
        break;
    case kSMUnitAttributeAnimationIsPaused:
        if (theComplex != NULL)
            theComplex->SetAnimation(theComplex->mAnimation, theComplex->mPhysicalModel.GetFrame(), theAmount != 0);
        break;
    case kSMUnitAttributeReloadTime:
        if (theComplex != NULL) {
            theBase = theStats->mReloadTime;
            theValue = &theComplex->mReloadTimer;
        }
        break;
    case kSMUnitAttributeGraphicScale:
        if (theComplex != NULL) {
            float theScale = inPercentage ? theClass->GetScale() * theAmount * 0.01f : (float)theAmount;
            if (inMode == kSMAttributeModeIncrease) theScale = theComplex->mPhysicalModel.GetScale() + theScale;
            else if (inMode == kSMAttributeModeDecrease) theScale = theComplex->mPhysicalModel.GetScale() - theScale;
            if (theScale <= 0.0f) theScale = 0.001f;
            theComplex->mPhysicalModel.SetScale(theScale);
        }
        break;
    case kSMUnitAttributePower:
        if (theComplex != NULL) {
            theBase = theStats->mPower;
            theValue = &theComplex->mPower;
        }
        break;
    case kSMUnitAttributeRottingTime:
        if (theComplex != NULL) {
            theBase = theClass->mRottingTime;
            theValue = &theComplex->mRottingAwayTime;
        }
        break;
    case kSMUnitAttributeConstructionComplete:
        if (theComplex != NULL)
            theComplex->SetBuilt(theAmount != 0, 1, 1);
        break;
    case kSMUnitAttributeUnitVariable1: case kSMUnitAttributeUnitVariable2: case kSMUnitAttributeUnitVariable3:
        if (theComplex != NULL) {
            long& theCounter = theComplex->mUnitVariables[inAttribute - 17];
            if (inMode == kSMAttributeModeIncrease) theAmount += theCounter;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theCounter - theAmount;
            if (theAmount < 0) theAmount = 0;
            theCounter = theAmount;
        }
        break;
    }
    if (theValue == NULL)
        return;
    if (inPercentage) theAmount = UMFloatToLong((float)theBase * theAmount * 0.01f);
    if (inMode == kSMAttributeModeIncrease) theAmount += *theValue;
    else if (inMode == kSMAttributeModeDecrease) theAmount = *theValue - theAmount;
    if (theAmount < 0) theAmount = 0;
    if (*theValue != theAmount) *theValue = theAmount;
}

// FUNCTION: EXE 0x5c9628
// Renames the object and renames its object conditions to "<name> Exists"
void EESMObject::SetName(const UWideString& inName)
{
    UWideString theConditionName = inName + " Exists";
    this->mName = inName;
    if (gWorld.mScenarioManager != NULL) {
        long theIndex = gWorld.mScenarioManager->GetObjectIndex(this);
        std::vector<EESMCondition*>& theConditions = gWorld.mScenarioManager->mConditions;
        for (unsigned long i = 1; i < theConditions.size(); i++) {
            EESMCondition* theCondition = theConditions[i];
            if (theCondition->mType == kSMConditionObject && theCondition->mObjectIndex == theIndex)
                theCondition->mName = theConditionName;
        }
    }
}

// FUNCTION: EXE 0x5c96b7
// Sets the parent object; a parent chain that loops back to this object resets the parent to 0
void EESMObject::SetParent(long inParent)
{
    std::vector<EESMObject*> theVisited;
    EESMObject* theObject = gWorld.GetScenarioManager()->GetScenarioObject(this->mParent);
    this->mParent = inParent;
    if (gWorld.mScenarioManager != NULL) {
        EESMObject* theThis = this;
        theVisited.push_back(theThis);
        long theParent = inParent;
        EESMObject* theRoot = gWorld.mScenarioManager->GetScenarioObject(0);
        while (true) {
            theObject = gWorld.mScenarioManager->GetScenarioObject(theParent);
            if (theObject == NULL || theObject == theRoot)
                break;
            if (std::find(theVisited.begin(), theVisited.end(), theObject) != theVisited.end()) {
                this->mParent = 0;
                theObject = NULL;
                return;
            }
            theVisited.push_back(theObject);
            theParent = theObject->mParent;
        }
        this->mParent = inParent;
    }
}

// FUNCTION: EXE 0x5c97a8
// Replaces the object's unit list
void EESMObject::SetUnits(const std::vector<unsigned long>& inUnits)
{
    this->mUnits = inUnits;
}

// FUNCTION: EXE 0x5c97b7
// Creates an enabled, non-looping trigger with one empty condition entry and one empty effect entry
EESMTrigger::EESMTrigger()
{
    EESMTConditionWrapper* theCondition;
    EESMTEffectWrapper* theEffect;
    this->SetName("");
    this->SetText("");
    this->mIsOn = true;
    this->mHasFired = false;
    this->mIsInCinematic = false;
    this->mIsLooping = false;
    this->mConditionDuration = 0;
    this->mConditionStartTime = 0;
    theCondition = new EESMTConditionWrapper;
    this->mConditions.push_back(theCondition);
    this->SetCondition(0, false, 0);
    this->SetConditionOperator(0, 0);
    theEffect = new EESMTEffectWrapper;
    this->mEffects.push_back(theEffect);
    this->SetEffect(0, 0, 0);
    this->SetEffectOperator(0, 0);
    this->mID = 0;
}

// FUNCTION: EXE 0x5c98dd
// Destroys the trigger and its condition and effect entries
EESMTrigger::~EESMTrigger()
{
    while (this->mConditions.size() > 0) {
        if (this->mConditions.back() != NULL)
            delete this->mConditions.back();
        this->mConditions.pop_back();
    }
    while (this->mEffects.size() > 0) {
        if (this->mEffects.back() != NULL)
            delete this->mEffects.back();
        this->mEffects.pop_back();
    }
}

// FUNCTION: EXE 0x5c9966
// Copies another trigger's name, text, conditions, effects and flags (not its condition start time)
void EESMTrigger::Copy(const EESMTrigger& inTrigger)
{
    this->mName = inTrigger.mName;
    this->mText = inTrigger.mText;
    this->CopyConditions(inTrigger.mConditions);
    this->CopyEffects(inTrigger.mEffects);
    this->mIsOn = inTrigger.mIsOn;
    this->mHasFired = inTrigger.mHasFired;
    this->mIsLooping = inTrigger.mIsLooping;
    this->mConditionDuration = inTrigger.mConditionDuration;
    this->mID = inTrigger.mID;
    this->mIsInCinematic = inTrigger.mIsInCinematic;
}

// FUNCTION: EXE 0x5c99c7
// Deletes a condition entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteCondition(unsigned long inIndex)
{
    delete this->mConditions[inIndex];
    this->mConditions.erase(this->mConditions.begin() + inIndex);
    if (inIndex == this->mConditions.size())
        this->mConditions[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
}

// FUNCTION: EXE 0x5c9a3e
// Deletes an effect entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteEffect(unsigned long inIndex)
{
    delete this->mEffects[inIndex];
    this->mEffects.erase(this->mEffects.begin() + inIndex);
    if (inIndex == this->mEffects.size())
        this->mEffects[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
}

// FUNCTION: EXE 0x5c9ab5
// Fires the trigger when its AND/OR condition chain has held for the set duration: queues all effects, or one random
// OR-separated group, at their delays, and turns a non-looping trigger off
// (EEScenarioManager::Update 0x1b20e6)
void EESMTrigger::Evaluate(float inTime)
{
    long theChosen = 0;
    long theRandomCount = 0;
    bool theContinue = true;
    bool theResult = false;
    if (!this->mIsOn || (gWorld.mInCinematic && !this->mIsInCinematic))
        return;
    long theCount = this->mConditions.size();
    long i;
    for (i = 0; i < theCount && theContinue == true; i++) {
        theResult = this->GetCondition(i)->Evaluate(theContinue);
        if (theContinue || this->GetCondition(i)->mOperator != kSMTriggerOperatorAnd)
            continue;
        for (long j = i + 1; j < theCount; j++) {
            if (this->GetCondition(j)->mOperator != kSMTriggerOperatorOr)
                continue;
            i = j;
            j = theCount;
            theContinue = true;
        }
    }
    if (theResult == true) {
        if (this->mConditionDuration != 0) {
            if (this->mConditionStartTime == 0) {
                this->mConditionStartTime = (long)inTime;
                return;
            }
            if ((float)this->mConditionDuration > (inTime - this->mConditionStartTime) * 0.001f)
                return;
        }
        this->mHasFired = true;
        this->mConditionStartTime = 0;
        if (!this->mIsLooping)
            this->mIsOn = false;
        long theEffects = this->mEffects.size();
        long e;
        for (e = 0; e < theEffects; e++) {
            if (this->mEffects[e]->mOperator == kSMTriggerOperatorOr)
                theRandomCount++;
        }
        if (theRandomCount > 0)
            theChosen = gPhysics->GetRandom(0, theRandomCount);
        theRandomCount = 0;
        for (long k = 0; k < theEffects; k++) {
            EESMTEffectWrapper* theWrapper = this->mEffects[k];
            if (theRandomCount == theChosen) {
                unsigned long theTime = (long)(theWrapper->mDelay * 1000.0f + inTime);
                // LOST-DEBUG: the original fetches the effect and discards it (a trace argument)
                // (the original also calls GetEffect(mL0C) here and discards it, 0x1c9c6d; no stand-in: the row is not exact, r32 close)
                gWorld.mScenarioManager->AddTimedEffect(theWrapper->mEffectIndex, theTime, this->mIsInCinematic);
            }
            if (theWrapper->mOperator == kSMTriggerOperatorOr)
                theRandomCount++;
            if (theRandomCount > theChosen)
                break;
        }
        return;
    }
    this->mConditionStartTime = 0;
}

// FUNCTION: EXE 0x5c9caf
// Returns the entry's condition result (negated if set); ioResult says whether the chain goes on (AND: result true,
// OR: result false, end: never)
bool EESMTConditionWrapper::Evaluate(bool& ioResult)
{
    bool theResult;
    EESMCondition* theCondition = gWorld.mScenarioManager->GetCondition(this->mValue);
    if (theCondition != NULL && theCondition->Evaluate())
        theResult = true;
    else
        theResult = false;
    if (this->mIsNot == true)
        theResult = !theResult;
    switch (this->mOperator) {
    case kSMTriggerOperatorOr:
        ioResult = !theResult;
        break;
    case kSMTriggerOperatorAnd:
        ioResult = theResult;
        break;
    case kSMTriggerOperatorEnd:
        ioResult = false;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x5c9d0e
// Returns the condition entry at an index, or NULL
EESMTConditionWrapper* EESMTrigger::GetCondition(unsigned long inIndex)
{
    EESMTConditionWrapper* theWrapper = NULL;
    if (inIndex < this->mConditions.size())
        theWrapper = this->mConditions[inIndex];
    return theWrapper;
}

// FUNCTION: EXE 0x5c9d32
// Returns the effect entry at an index, or NULL
EESMTEffectWrapper* EESMTrigger::GetEffect(unsigned long inIndex)
{
    EESMTEffectWrapper* theWrapper = NULL;
    if (inIndex < this->mEffects.size())
        theWrapper = this->mEffects[inIndex];
    return theWrapper;
}

// FUNCTION: EXE 0x5c9d56
// Reads the trigger's settings, condition entries and effect entries (id -1 becomes 0); returns the first error
long EESMTrigger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    bool theNot = false;
    unsigned long theCount = 0;
    unsigned long theOperator = 0;
    unsigned long theValue = 0;
    unsigned long theDelay = 0;
    long theResult = this->mName.Load(inFile);
    if (theResult != 0 || (theResult = this->mText.Load(inFile)) != 0 || (theResult = inFile.Read(this->mIsOn)) != 0
        || (theResult = inFile.Read(this->mIsLooping)) != 0 || (theResult = inFile.Read(this->mHasFired)) != 0
        || (theResult = inFile.Read(this->mIsInCinematic)) != 0 || (theResult = inFile.Read(this->mConditionDuration)) != 0
        || (theResult = inFile.Read(this->mConditionStartTime)) != 0 || (theResult = inFile.Read(this->mID)) != 0
        || (theResult = inFile.Read(theCount)) != 0)
        return theResult;
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        if ((theResult = inFile.Read(theNot)) == 0 && (theResult = inFile.Read(theOperator)) == 0)
            theResult = inFile.Read(theValue);
        if (theValue == (unsigned long)-1)
            theValue = 0;
        if (theResult != 0)
            return theResult;
        this->SetCondition(i, theNot, theValue);
        this->SetConditionOperator(i, theOperator);
    }
    theResult = inFile.Read(theCount);
    if (theResult == 0) {
        for (i = 0; i < theCount; i++) {
            if ((theResult = inFile.Read(theDelay)) == 0 && (theResult = inFile.Read(theOperator)) == 0)
                theResult = inFile.Read(theValue);
            if (theValue == (unsigned long)-1)
                theValue = 0;
            if (theResult != 0)
                return theResult;
            this->SetEffect(i, theDelay, theValue);
            this->SetEffectOperator(i, theOperator);
            theResult = 0;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5c9f3d
// Updates the entry's condition index after conditions inOld/inNew swap, or after inOld is deleted
void EESMTConditionWrapper::Virtual4(unsigned long inOld, unsigned long inNew)
{
    unsigned long theValue = this->mValue;
    if (theValue == inOld) {
        inNew = (inNew != 0xffffffff) ? inNew : 0;
    } else {
        if (theValue == inNew) {
            this->mValue = inOld;
            return;
        }
        if (inNew != 0xffffffff) {
            return;
        }
        if (theValue <= inOld) {
            return;
        }
        inNew = theValue - 1;
    }
    this->mValue = inNew;
}

// FUNCTION: EXE 0x5c9f73
// Maps the entry's condition index through the ID map
void EESMTConditionWrapper::Virtual0(EESMIDMap& inMap)
{
    EERemapID(this->mValue, inMap);
}

// FUNCTION: EXE 0x5c9f85
// Updates the entry's effect index after effects inOld/inNew swap, or after inOld is deleted
void EESMTEffectWrapper::Virtual4(unsigned long inOld, unsigned long inNew)
{
    unsigned long theCurrent = this->mEffectIndex;
    if (theCurrent == inOld) {
        this->mEffectIndex = (inNew != 0xffffffff) ? inNew : 0;
        return;
    }
    if (theCurrent == inNew) {
        this->mEffectIndex = inOld;
        return;
    }
    if (inNew != 0xffffffff) {
        return;
    }
    if (theCurrent > inOld) {
        this->mEffectIndex = theCurrent - 1;
    }
}

// FUNCTION: EXE 0x5c9fbb
// Maps the entry's effect index through the ID map
void EESMTEffectWrapper::Virtual0(EESMIDMap& inMap)
{
    EERemapID(this->mEffectIndex, inMap);
}

// FUNCTION: EXE 0x5c9fcd
// Updates every condition entry after conditions inOld/inNew swap or inOld is deleted
void EESMTrigger::RenumberConditions(unsigned long inOld, unsigned long inNew)
{
    long theCount = this->mConditions.size();
    for (long i = 0; i < theCount; i++)
        this->mConditions[i]->Virtual4(inOld, inNew);
}

// FUNCTION: EXE 0x5ca009
// Updates every effect entry after effects inOld/inNew swap or inOld is deleted
void EESMTrigger::RenumberEffects(unsigned long inOld, unsigned long inNew)
{
    long theCount = this->mEffects.size();
    for (long i = 0; i < theCount; i++)
        this->mEffects[i]->Virtual4(inOld, inNew);
}

// FUNCTION: EXE 0x5ca045
// Maps every condition entry's index through the ID map
void EESMTrigger::RemapConditions(EESMIDMap& inMap)
{
    long theCount = this->mConditions.size();
    for (long i = 0; i < theCount; i++)
        this->mConditions[i]->Virtual0(inMap);
}

// FUNCTION: EXE 0x5ca07c
// Maps every effect entry's index through the ID map
void EESMTrigger::RemapEffects(EESMIDMap& inMap)
{
    long theCount = this->mEffects.size();
    for (long i = 0; i < theCount; i++)
        this->mEffects[i]->Virtual0(inMap);
}

// FUNCTION: EXE 0x5ca0b3
// Writes the trigger's settings, condition entries and effect entries; returns the first error
long EESMTrigger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == 0 && (theResult = this->mText.Save(outFile)) == 0 && (theResult = outFile.Write(this->mIsOn)) == 0
        && (theResult = outFile.Write(this->mIsLooping)) == 0 && (theResult = outFile.Write(this->mHasFired)) == 0
        && (theResult = outFile.Write(this->mIsInCinematic)) == 0 && (theResult = outFile.Write(this->mConditionDuration)) == 0
        && (theResult = outFile.Write(this->mConditionStartTime)) == 0)
        theResult = outFile.Write(this->mID);
    unsigned long theCount = this->mConditions.size();
    unsigned long i;
    unsigned long theDelay;
    unsigned long theValue;
    if (theResult == 0 && (theResult = outFile.Write(theCount)) == 0) {
        for (i = 0; i < this->mConditions.size(); i++) {
            bool theNot = this->GetConditionAt(i)->mIsNot;
            theResult = outFile.Write(theNot);
            theValue = this->GetConditionAt(i)->mOperator;
            if (theResult == 0)
                theResult = outFile.Write(theValue);
            theCount = this->GetConditionAt(i)->mValue;
            if (theResult != 0 || (theResult = outFile.Write(theCount)) != 0)
                break;
        }
    }
    theCount = this->mEffects.size();
    if (theResult != 0 || (theResult = outFile.Write(theCount)) != 0)
        return theResult;
    for (i = 0; i < this->mEffects.size(); i++) {
        theDelay = this->GetEffectAt(i)->mDelay;
        theResult = outFile.Write(theDelay);
        theValue = this->GetEffectAt(i)->mOperator;
        if (theResult == 0)
            theResult = outFile.Write(theValue);
        theCount = this->GetEffectAt(i)->mEffectIndex;
        if (theResult != 0)
            return theResult;
        theResult = outFile.Write(theCount);
        if (theResult != 0)
            return theResult;
    }
    return theResult;
}

// FUNCTION: EXE 0x5ca29d
// Sets a condition entry's condition index and NOT flag
void EESMTrigger::SetCondition(unsigned long inIndex, bool inNot, unsigned long inCondition)
{
    this->mConditions[inIndex]->mIsNot = inNot;
    this->mConditions[inIndex]->mValue = inCondition;
}

// FUNCTION: EXE 0x5ca2c3
// Sets a condition entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetConditionOperator(unsigned long inIndex, long inOperator)
{
    if (inIndex == this->mConditions.size() - 1 && inOperator != 0)
        this->mConditions.push_back(new EESMTConditionWrapper);
    this->mConditions[inIndex]->mOperator = inOperator;
}

// FUNCTION: EXE 0x5ca331
// Replaces the trigger's condition entries by copies of the given ones
void EESMTrigger::CopyConditions(const std::vector<EESMTConditionWrapper*>& inConditions)
{
    while (this->mConditions.size() > 1)
        this->DeleteCondition(1);
    for (unsigned long i = 0; i < inConditions.size(); i++) {
        EESMTConditionWrapper* theWrapper = inConditions[i];
        this->SetCondition(i, theWrapper->mIsNot, theWrapper->mValue);
        this->SetConditionOperator(i, theWrapper->mOperator);
    }
}

// FUNCTION: EXE 0x5ca39b
// Sets an effect entry's effect index and delay
void EESMTrigger::SetEffect(unsigned long inIndex, long inDelay, unsigned long inEffect)
{
    this->mEffects[inIndex]->mDelay = inDelay;
    this->mEffects[inIndex]->mEffectIndex = inEffect;
}

// FUNCTION: EXE 0x5ca3c1
// Sets an effect entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetEffectOperator(unsigned long inIndex, long inType)
{
    if (inIndex == this->mEffects.size() - 1 && inType != 0)
        this->mEffects.push_back(new EESMTEffectWrapper);
    this->mEffects[inIndex]->mOperator = inType;
}

// FUNCTION: EXE 0x5ca42f
// Replaces the trigger's effect entries by copies of the given ones
void EESMTrigger::CopyEffects(const std::vector<EESMTEffectWrapper*>& inEffects)
{
    while (this->mEffects.size() > 1)
        this->DeleteEffect(1);
    for (unsigned long i = 0; i < inEffects.size(); i++) {
        EESMTEffectWrapper* theWrapper = inEffects[i];
        this->SetEffect(i, theWrapper->mDelay, theWrapper->mEffectIndex);
        this->SetEffectOperator(i, theWrapper->mOperator);
    }
}

// FUNCTION: EXE 0x5ca496
// Appends a condition entry to a trigger's list (vector<EESMTConditionWrapper*>::push_back)

// FUNCTION: EXE 0x5ca4c2
// Appends an effect entry to a trigger's list (vector<EESMTEffectWrapper*>::push_back)
