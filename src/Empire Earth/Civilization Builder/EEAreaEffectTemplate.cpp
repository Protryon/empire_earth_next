// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffectTemplate.cpp (name from RTTI EEAreaEffectTemplate; folder
// inferred: no __FILE__). 0x27c0f3..0x27c8b9: the area-effect template (vtable 0x44be78; EEAEManager::Load 0x27b079
// makes one per record of the area-effect file): ctor, ??_G, dtor, the player-relation and range tests, Init,
// Load/Unload; then a unit's area-effect list (EEUnitAreaEffectList) members and the UReferenceContainer<EEAreaEffect*>
// template COMDATs they instantiate (Remove 0x27c7f2, Contains 0x27c820 (ICF-shared with <EEComplexUnit*>), 0x27c873
// (its end()), Erase 0x27c87f). EECBCivilizationCreator.cpp starts at 0x27c8b9. Unit flags: the exe's module set.
#include <windows.h>
#include <math.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEColor.h"

#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAreaEffectKindIDs.h"

// UReferenceContainer<T>::Contains (its COMDAT 0x27c820 closes this unit) is defined in ee/UReferenceContainer.h

// FUNCTION: EXE 0x67c0f3
// Creates an empty template: square shape, radius 1, allied players, no unit set or graphics, white tint
EEAreaEffectTemplate::EEAreaEffectTemplate()
{
    this->mKind = 0;
    this->mShape = 1;
    this->mRadius = 1.0f;
    this->mColor.SetRed(1.0f);
    this->mMode = 2;
    this->mUnitSet = -1;
    this->mGraphicEffect = NULL;
    this->mGraphicEffect18 = NULL;
    this->mMedia = NULL;
    this->mMaxLevel = -1;
    this->mLevel = -1;
    this->mL44 = -1;
    this->mL48 = -1;
    this->mL4C = -1;
    this->mColor.SetGreen(1.0f);
    this->mColor.SetBlue(1.0f);
    this->mColorAlpha = 0.15f;
    this->mRed = -1.0f;
    this->mGreen = -1.0f;
    this->mBlue = -1.0f;
    this->mAlpha = -1.0f;
    this->mF60 = -1.0f;
    this->mB64 = false;
    this->mB65 = false;
    this->mB66 = false;
    this->mB67 = false;
    this->mB68 = false;
}

// SYNTHETIC: EXE 0x67c17d
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x67c199
// Destroys the template, releasing its two graphic effects
EEAreaEffectTemplate::~EEAreaEffectTemplate()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Dereference();
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Dereference();
}

// FUNCTION: EXE 0x67c1c5
// Tells whether the template's player mode (1 self, 2 allies, 3 enemies, 4 everyone) affects the other player
bool EEAreaEffectTemplate::AffectsPlayer(long inPlayer, long inOther)
{
    bool theResult = false;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    EEPlayer* theOther = gWorld.mPlayers[inOther];
    bool theAllied = thePlayer->IsAlliedWith(theOther);
    long theMode = this->mMode;
    if (theMode == 4 || theMode == 1 && inPlayer == inOther || theMode == 2 && theAllied ||
        theMode == 3 && theOther != thePlayer && thePlayer->mEnemies[theOther->mPlayerID] != 0)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x67c233
// Tells whether the other player falls outside the template's mode (1 others, 2 enemies, 3 friends)
bool EEAreaEffectTemplate::AffectsPlayerReversed(long inPlayer, long inOther)
{
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    EEPlayer* theOther = gWorld.mPlayers[inOther];
    bool theResult = false;
    bool theFriendly = theOther == thePlayer || thePlayer->mEnemies[theOther->mPlayerID] == 0;
    long theMode = this->mMode;
    if (theMode == 1 && inPlayer != inOther || theMode == 2 && !theFriendly || theMode == 3 && theFriendly)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x67c28e
// Tells whether a built unit of the template's unit set, owned by an affected player, is inside the scaled area
bool EEAreaEffectTemplate::CoversUnit(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint,
                                    float inScale)
{
    GE2DPoint theLocation(0.0f, 0.0f);
    unsigned long theOwner = inUnit->mPlayer->mPlayerID;
    bool theResult = false;
    if (gUnitSetManager.Contains(inUnit->mUnitClass->GetUnitType(), this->mUnitSet) && this->AffectsPlayer(inPlayer, theOwner) &&
        inUnit->mIsBuilt) {
        if (this->mRadius > 0.0f) {
            theLocation = GE2DPoint(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY());
            if (this->IsWithin(theLocation, inPoint, inScale, inUnit))
                theResult = true;
        } else {
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67c334
// Tells whether a point lies in the square or circular area around a center on the same continent; a space
// dock or turret carried by another unit also passes when the point is on its owner's planet
bool EEAreaEffectTemplate::IsWithin(const GE2DPoint& inCenter, const GE2DPoint& inPoint, float inScale,
                                    EEComplexUnit* inUnit)
{
    bool theResult = false;
    float theRange = inScale * this->mRadius;
    if (this->mRadius <= 0.0f)
        return true;
    if (this->mShape == 1) {
        float theDX = (float)fabs(inCenter.GetX() - inPoint.GetX());
        float theDY = (float)fabs(inCenter.GetY() - inPoint.GetY());
        if (theDX > theRange || theDY > theRange)
            return theResult;
    } else if (this->mShape == 2) {
        if (inPoint.DistanceTo(inCenter) > theRange)
            return theResult;
    } else {
        return theResult;
    }
    theResult = true;
    if (this->mUnitSet != 4) {
        EETileCoordinate theTile;
        theTile.mX = (long)inPoint.GetX();
        theTile.mY = (long)inPoint.GetY();
        unsigned short theContinent = gContinentManager.GetContinent(theTile, kAIPathNonSpace);
        if (theContinent != 0) {
            theTile.mX = (long)inCenter.GetX();
            theTile.mY = (long)inCenter.GetY();
            if (theContinent != gContinentManager.GetContinent(theTile, kAIPathNonSpace)) {
                theResult = false;
                if (inUnit != NULL) {
                    EEComplexUnit* theOwner = inUnit->mComplexUnit;
                    if (theOwner != NULL) {
                        unsigned long theType = inUnit->mUnitClass->GetUnitType();
                        if ((theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation) && theType != kUnitTypeOrbitalSpaceStation)
                            theResult = theContinent == theOwner->mPlanetID;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67c489
// Fills the template from an area-effect file record, resolving its graphic effects and media
bool EEAreaEffectTemplate::Init(const EEAreaEffectData& inData)
{
    this->mKind = inData.mKind;
    this->mID = inData.mID;
    this->mRadius = inData.mRadius;
    this->mShape = inData.mShape;
    this->mMode = inData.mMode;
    this->mUnitSet = inData.mUnitSet;
    if (inData.mGraphicEffect > 0) {
        this->mGraphicEffect = EEGraphicEffect::Get(inData.mGraphicEffect);
        if (this->mGraphicEffect != NULL)
            this->mGraphicEffect->Reference();
    }
    if (inData.mGraphicEffect18 > 0) {
        this->mGraphicEffect18 = EEGraphicEffect::Get(inData.mGraphicEffect18);
        if (this->mGraphicEffect18 != NULL)
            this->mGraphicEffect18->Reference();
    }
    if (inData.mMedia > 0)
        this->mMedia = EEMedia::Get(inData.mMedia);
    else
        this->mMedia = NULL;
    this->mMaxLevel = inData.mMaxLevel;
    this->mLevel = inData.mLevel;
    this->mL44 = inData.mL90;
    this->mL48 = inData.mL94;
    this->mL4C = inData.mL98;
    this->mRed = inData.mRed;
    this->mGreen = inData.mGreen;
    this->mBlue = inData.mBlue;
    this->mAlpha = inData.mAlpha;
    this->mF60 = inData.mFB0;
    this->mB64 = inData.mBB4;
    this->mB65 = inData.mBB5;
    this->mB66 = inData.mBB6;
    this->mB67 = inData.mBB7;
    this->mB68 = inData.mBB8;
    this->mColor.SetRed(this->mRed);
    this->mColor.SetGreen(this->mGreen);
    this->mColor.SetBlue(this->mBlue);
    this->mColorAlpha = this->mAlpha;
    return true;
}

// FUNCTION: EXE 0x67c5ab
// Loads the template's graphic effects and sound media
void EEAreaEffectTemplate::Load(bool inIsOnDemand)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Load(inIsOnDemand);
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Load(inIsOnDemand);
    if (this->mMedia != NULL)
        this->mMedia->LoadMedia();
}

// FUNCTION: EXE 0x67c5de
// Unloads the template's graphic effects and sound media
void EEAreaEffectTemplate::Unload()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Unload();
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Unload();
    if (this->mMedia != NULL)
        this->mMedia->UnloadMedia();
}

// FUNCTION: EXE 0x67c607
// Adds an area effect to the unit's list (taking a reference)
void EEUnitAreaEffectList::Add(EEAreaEffect* inEffect)
{
    UReferenceContainer<EEAreaEffect*>::Add(inEffect);
}

// FUNCTION: EXE 0x67c623
// Adds an area effect unless the unit already has a stronger or equal one of the same kind (all morale kinds
// count as one), replacing a weaker one
void EEUnitAreaEffectList::AddAreaEffect(EEAreaEffect* inEffect)
{
    long theKind = inEffect->mTemplate->mKind;
    std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin();
    for (; theIterator != this->mItems.end(); ++theIterator) {
        long theOther = (*theIterator)->mTemplate->mKind;
        if (theKind == theOther)
            break;
        if ((theKind == kAreaEffectMorale || theKind == kAreaEffectMoraleFromBuildings || theKind == kAreaEffectMoraleFromMilitaryHeroes) && (theOther == kAreaEffectMorale || theOther == kAreaEffectMoraleFromBuildings || theOther == kAreaEffectMoraleFromMilitaryHeroes))
            break;
    }
    if (theIterator != this->mItems.end()) {
        EEAreaEffect* theOld = *theIterator;
        if ((unsigned long)inEffect->GetStrength() <= (unsigned long)theOld->GetStrength())
            return;
        UReferenceContainer<EEAreaEffect*>::Remove(*theIterator);
    }
    if (inEffect != NULL)
        UReferenceContainer<EEAreaEffect*>::Add(inEffect);
}

// FUNCTION: EXE 0x67c6b9
// Lets every effect modify a value of the given kind, showing each effect that applied
void EEUnitAreaEffectList::ApplyModifiers(long inKind, const long& inDamage, long& outDamage, EEComplexUnit* inUnit)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        if ((*theIterator)->Modify(inKind, inDamage, outDamage))
            (*theIterator)->ShowOn(inUnit);
    }
}

// FUNCTION: EXE 0x67c6f3
// Removes the first effect of the given kind; returns whether one was found
bool EEUnitAreaEffectList::Remove(long inKind)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        if ((*theIterator)->mTemplate->mKind == inKind) {
            UReferenceContainer<EEAreaEffect*>::Remove(*theIterator);
            return true;
        }
    }
    return false;
}

// FUNCTION: EXE 0x67c721
// Rebuilds the list keeping only the strongest effect of each kind
void EEUnitAreaEffectList::KeepStrongest()
{
    EEUnitAreaEffectList theList;
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        theList.AddAreaEffect(*theIterator);
    *this = theList;
}

// FUNCTION: EXE 0x67c768
// Marks every effect in the list for a recount
void EEUnitAreaEffectList::MarkDirty()
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        (*theIterator)->MarkDirty();
}

// FUNCTION: EXE 0x67c783
// Moves every effect in the list to a new location and recounts it
void EEUnitAreaEffectList::MoveAndUpdate(const GE2DPoint& inPoint, float inValue)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        (*theIterator)->mLocation = inPoint;
        (*theIterator)->Recount((long)inValue);
    }
}

// FUNCTION: EXE 0x67c7c9
// Ticks every effect in the list on the unit
void EEUnitAreaEffectList::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        (*theIterator)->Tick(inUnit, inElapsed);
}

// TEMPLATE: EXE 0x67c7f2
// TEMPLATE: EXE 0x67c820
// TEMPLATE: EXE 0x67c873
// TEMPLATE: EXE 0x67c87f
