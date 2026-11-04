// C:\EEX\Empire Earth\Graphics Thread\EEEDamagedEffect.cpp (no __FILE__; name from the RTTI class and its key
// "EEEDamagedEffect-Data"). 0x101fce..0x10251d: the key chain, both ctors, the dtor, Virtual24, Load, Save, Virtual4C.
// Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEWorld.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEDamagedEffect.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x501fce
// SYNTHETIC: EXE 0x501fd8
// SYNTHETIC: EXE 0x501fe9
// SYNTHETIC: EXE 0x501ff5
const UString EEEDamagedEffect::sData("EEEDamagedEffect-Data");

// FUNCTION: EXE 0x502011
// Creates the damage controller effect for a unit, not yet finished
EEEDamagedEffect::EEEDamagedEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffectDamaged, false)
{
    this->mIsFinished = false;
}

// SYNTHETIC: EXE 0x50205b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x502077
// Creates an empty damage controller effect for loading from a saved game
EEEDamagedEffect::EEEDamagedEffect()
    : EEEffect2D(gEffect2DModel, kEffectDamaged, false)
{
    this->mIsFinished = false;
}

// FUNCTION: EXE 0x5020ba
// Destroys the damage controller (members only; the child effects are not destroyed here)
EEEDamagedEffect::~EEEDamagedEffect()
{
}

long EEEDamagedEffect::DrawPass2()
{
    return 0;
}

long EEEDamagedEffect::DrawPass4()
{
    return 0;
}

long EEEDamagedEffect::DrawPass5()
{
    return 0;
}

long EEEDamagedEffect::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x5020dc
// Queues nothing: the controller draws nothing itself and returns 10
long EEEDamagedEffect::Queue(EEEffectQueue* inQueues)
{
    return 10;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEEDamagedEffect::PrepareQueue()
{
}

long EEEDamagedEffect::SetUp()
{
    return 0;
}

// FUNCTION: EXE 0x5020e2
// Reads the handles of the child damage effects (and their secondary partners when saved) and, from save
// version 0.249 on, the finished flag; returns the first read error (the base class result is ignored)
long EEEDamagedEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    bool theHasSecond;
    unsigned long theCount;
    unsigned long i;
    EEEffectHandle theHandle;
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    long theResult = inFile.Read(theHasSecond);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                theResult = inFile.Read(theHandle.mValue);
                if (theResult != kErrorOK)
                    continue;
                this->mPrimaryEffects.push_back(theHandle);
                if (!theHasSecond)
                    continue;
                theResult = inFile.Read(theHandle.mValue);
                if (theResult == kErrorOK)
                    this->mSecondaryEffects.push_back(theHandle);
            }
            if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0xf9))
                theResult = inFile.Read(this->mIsFinished);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5021a2
// Writes whether a secondary effect is linked, the child effect handles (paired with their secondaries) and
// the finished flag; returns the first write error (the base class result is ignored)
long EEEDamagedEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mGraphicEffect->mLinkedEffect2 != NULL);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write((unsigned long)this->mPrimaryEffects.size());
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < this->mPrimaryEffects.size(); i++) {
        theResult = outFile.Write(this->mPrimaryEffects[i].mValue);
        if (theResult != 0)
            break;
        if (!this->mGraphicEffect->mLinkedEffect2)
            continue;
        theResult = outFile.Write(this->mSecondaryEffects[i].mValue);
        if (theResult != 0)
            break;
    }
    if (theResult == 0)
        theResult = outFile.Write(this->mIsFinished);
    return theResult;
}

// FUNCTION: EXE 0x50225c
// Keeps one child damage effect (plus its secondary) per damage tag point, more as the unit's health drops;
// hides them all on a cloaked enemy unit, and once finished destroys them and returns 7 (remove)
long EEEDamagedEffect::Step()
{
    EEUnit* theUnit = this->mUnit;
    EETagPointList* theList;
    unsigned long theCount;
    unsigned long theWanted;
    long theDelta;
    long i;
    EEEffectHandle theHandle;
    if (this->mIsFinished) {
        for (i = this->mPrimaryEffects.size(); i > 0; i--) {
            gWorld.GetEffectManager()->DestroyEffect(this->mPrimaryEffects.back());
            this->mPrimaryEffects.pop_back();
            if (this->mGraphicEffect->mLinkedEffect2) {
                gWorld.GetEffectManager()->DestroyEffect(this->mSecondaryEffects.back());
                this->mSecondaryEffects.pop_back();
            }
        }
        return 7;
    }
    if (!theUnit)
        return 0;
    if (theUnit->mTileNode) {
        while (theUnit->mComplexUnit->mPreviousPart)
            theUnit = theUnit->mComplexUnit->mPreviousPart;
    }
    if (theUnit->mTileNode) {
        long theTag = this->mGraphicEffect->mLinkedEffect1->mTagPointKind;
        theList = theUnit->GetModel()->GetTagPoints(theUnit->mComplexUnit->GetAnimation(), theTag);
    } else
        theList = theUnit->GetModel()->GetTagPoints(0, this->mGraphicEffect->mLinkedEffect1->mTagPointKind);
    if (theList)
        theCount = theList->GetNumTagPoints();
    else
        theCount = 1;
    float theRatio = (float)theUnit->mHitPoints / theUnit->mStats->mMaxHitPoints;
    if (theRatio <= this->mGraphicEffect->GetParam2() && theUnit->mHitPoints > 0) {
        float theAmount = (1.0f - theRatio * (1.0f / this->mGraphicEffect->GetParam2())) * theCount;
        theWanted = UMFloatToLong(theAmount);
        if (theWanted < theAmount)
            theWanted++;
    } else
        theWanted = 0;
    if (theWanted == 0 && this->mGraphicEffect->GetParam2() < 1.0f)
        this->mIsFinished = true;
    theDelta = theWanted - this->mPrimaryEffects.size();
    if (theUnit->mTileNode) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        bool theHidden = theComplexUnit->mIsCloakable ? theComplexUnit->mIsCloaked : false;
        if (theHidden) {
            EEPlayer* thePlayer = theComplexUnit->mPlayer;
            if (thePlayer != gWorld.GetCurrentPlayer()
                && gWorld.GetCurrentPlayer()->mEnemies[thePlayer->mPlayerID] != 0)
                theDelta = -(long)this->mPrimaryEffects.size();
        }
    }
    if (theDelta == 0)
        return 0;
    if (theDelta > 0) {
        do {
            gWorld.GetEffectManager()->CreateUnitEffect(this->mUnit, this->mGraphicEffect->mLinkedEffect1, &theHandle, 0, this->mPrimaryEffects.size(), 0);
            this->mPrimaryEffects.push_back(theHandle);
            if (this->mGraphicEffect->mLinkedEffect2) {
                gWorld.GetEffectManager()->CreateUnitEffect(this->mUnit, this->mGraphicEffect->mLinkedEffect2, &theHandle, 0, this->mSecondaryEffects.size(), 0);
                this->mSecondaryEffects.push_back(theHandle);
            }
            theDelta--;
        } while (theDelta > 0);
    } else {
        do {
            gWorld.GetEffectManager()->DestroyEffect(this->mPrimaryEffects.back());
            this->mPrimaryEffects.pop_back();
            if (this->mGraphicEffect->mLinkedEffect2) {
                gWorld.GetEffectManager()->DestroyEffect(this->mSecondaryEffects.back());
                this->mSecondaryEffects.pop_back();
            }
            theDelta++;
        } while (theDelta < 0);
    }
    return 0;
}
