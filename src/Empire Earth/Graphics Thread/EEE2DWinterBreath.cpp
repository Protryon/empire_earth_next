// C:\EEX\Empire Earth\Graphics Thread\EEE2DWinterBreath.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DWinterBreath-Data"). 0xe8158..0xe89c6: the key chain, both ctors .. Virtual4C; EEE3DAdditivePass.cpp follows.
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
#include "ee/EEE2DWinterBreath.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e8158
// SYNTHETIC: EXE 0x4e8162
// SYNTHETIC: EXE 0x4e8173
// SYNTHETIC: EXE 0x4e817f
const UString EEE2DWinterBreath::sData("EEE2DWinterBreath-Data");

// FUNCTION: EXE 0x4e819b
// Creates a breath puff at the unit's mouth tag point (or a per-unit-type mouth offset; then the puff is inert),
// shorter and faster when the unit moves, drifting forward and down, with a random rotation
EEE2DWinterBreath::EEE2DWinterBreath(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DWinterBreath, false)
{
    GE3DPoint thePosition;
    GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GEVector theFacing;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0x16);
    float theScale = inUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    this->mDuration = inGraphicEffect->mDuration;
    this->mSpeed = inGraphicEffect->GetParam1();
    this->mSlowDownTime = inGraphicEffect->GetParam2();
    this->mFade = inGraphicEffect->GetParam3();
    if (theList) {
        EETagPointList::Scale(theTransformation, theScale);
        theList->GetTagPoint(0)->GetPoint(theFrame, theTransformation, thePosition);
    } else {
        this->mFade = 0.0f;
        this->mSpeed = 0.0f;
        this->mSlowDownTime = 0.0f;
        this->mDuration = 0;
        theTransformation.SetElement(0, 0, theTransformation.GetElement(0, 0) * theScale);
        theTransformation.SetElement(0, 1, theTransformation.GetElement(0, 1) * theScale);
        theTransformation.SetElement(0, 2, theTransformation.GetElement(0, 2) * theScale);
        theTransformation.SetElement(1, 0, theTransformation.GetElement(1, 0) * theScale);
        theTransformation.SetElement(1, 1, theTransformation.GetElement(1, 1) * theScale);
        theTransformation.SetElement(1, 2, theTransformation.GetElement(1, 2) * theScale);
        theTransformation.SetElement(2, 0, theTransformation.GetElement(2, 0) * theScale);
        theTransformation.SetElement(2, 1, theTransformation.GetElement(2, 1) * theScale);
        theTransformation.SetElement(2, 2, theTransformation.GetElement(2, 2) * theScale);
        thePosition = this->GetMouthPoint(inUnit, theFrame, theTransformation);
    }
    if (inUnit->mComplexUnit->mVelocity != 0.0f) {
        this->mDuration = UMFloatToLong(this->mDuration * 0.55f);
        this->mSpeed *= 1.05f;
        this->mSlowDownTime *= 1.15f;
    }
    theFacing.SetX(0.0f);
    theFacing.SetY(1.0f);
    theFacing.SetZ(-1.0f);
    theTransformation.Apply(theFacing, this->mVelocity);
    this->mVelocity.Normalize();
    this->SetPosition(thePosition);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mRotation = EEEffect::GetRandom() * 6.2831855f;
}

// SYNTHETIC: EXE 0x4e83d9
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e83f5
// Creates an empty breath puff for loading from a saved game
EEE2DWinterBreath::EEE2DWinterBreath()
    : EEEffect2D(gEffect2DModel, kEffect2DWinterBreath, false)
{
}

// FUNCTION: EXE 0x4e842f
// Destroys the puff, unloading its graphic's media
EEE2DWinterBreath::~EEE2DWinterBreath()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DWinterBreath::DrawPass2()
{
    return 0;
}

long EEE2DWinterBreath::DrawPass4()
{
    return 0;
}

long EEE2DWinterBreath::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e844d
// Queues the puff in render queue 4 under its effect's texture, snapshotting its age and position to draw
long EEE2DWinterBreath::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawPosition = this->mPosition;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DWinterBreath::PrepareQueue()
{
}

// SetUp (loads the puff's graphic): ICF-folded into EEE2DSmokeMuzzleBlast's 0xe6b62
long EEE2DWinterBreath::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e84c0
// Draws the puff as up to three rotated sprites trailing along its velocity (the later ones after their delays),
// growing with age and fading by the effect's fade factor
long EEE2DWinterBreath::DrawPass6()
{
    GEColor theColor;
    GE3DPoint thePosition(this->mDrawPosition);
    float theAge = this->mDrawAge / this->mDuration;
    float theSize = (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * theAge + this->mGraphicEffect->GetStartSize();
    float theSize2 = theSize * 0.95f;
    float theSize3 = theSize * 0.85f;
    float theAlpha = 1.0f - (theAge * this->mFade + (1.0f - this->mFade));
    float theDelay = this->mDrawAge - this->mGraphicEffect->mTime1;
    float theDelay2 = this->mDrawAge - this->mGraphicEffect->mTime1 * 2;
    float theOffset = -this->mGraphicEffect->mAlpha;
    Fn10387B(thePosition, theColor, this->mRotation, theSize, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    if (!(theDelay >= 0.0f))
        return 0;
    thePosition.SetX(thePosition.GetX() + this->mVelocity.GetX() * theOffset);
    thePosition.SetY(thePosition.GetY() + this->mVelocity.GetY() * theOffset);
    thePosition.SetZ(thePosition.GetZ() + this->mVelocity.GetZ() * theOffset);
    Fn10387B(thePosition, theColor, this->mRotation + 1.0471976f, theSize2, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    if (!(theDelay2 >= 0.0f))
        return 0;
    thePosition.SetX(thePosition.GetX() + this->mVelocity.GetX() * theOffset);
    thePosition.SetY(thePosition.GetY() + this->mVelocity.GetY() * theOffset);
    thePosition.SetZ(thePosition.GetZ() + this->mVelocity.GetZ() * theOffset);
    Fn10387B(thePosition, theColor, this->mRotation + 2.0943952f, theSize3, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    return 0;
}

// FUNCTION: EXE 0x4e86b0
// Reloads the puff's graphic and reads its motion, age, rotation (twice), draw state, timings; returns the first error
long EEE2DWinterBreath::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mVelocity.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mRotation);
    if (theResult != 0)
        return theResult;
    theResult = this->mDrawPosition.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mDrawAge);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mDuration);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mFade);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mSpeed);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mSlowDownTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mRotation);
    return theResult;
}

// FUNCTION: EXE 0x4e8778
// Writes the puff's motion, age, rotation (twice), draw state and timings; returns the first write error
long EEE2DWinterBreath::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mVelocity.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mRotation);
    if (theResult != 0)
        return theResult;
    theResult = this->mDrawPosition.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mDrawAge);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mDuration);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFade);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mSpeed);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mSlowDownTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mRotation);
    return theResult;
}

// FUNCTION: EXE 0x4e8832
// Returns the unit's mouth point through the transformation, with offsets for cavalry archers, chariot archers,
// bronze spear cavalry and knights
GE3DPoint EEE2DWinterBreath::GetMouthPoint(EEUnit* inUnit, unsigned long inFrame, const GETransformation& inTransformation)
{
    GE3DPoint theOffset(0.0f, 0.4f, 0.405f);
    GE3DPoint theResult;
    switch (inUnit->mUnitClass->mUnitType) {
    case kUnitTypeArch06CavalryArcher:
        theOffset.SetY(0.465f);
        theOffset.SetZ(0.385f);
        break;
    case kUnitTypeArch04ChariotArcher:
        theOffset.SetY(0.872f);
        theOffset.SetZ(0.465f);
        break;
    case kUnitTypeCav04BronzeSpearCavalry:
    case kUnitTypeCav06Knight:
        theOffset.SetY(0.272f);
        theOffset.SetZ(0.235f);
        break;
    }
    inTransformation.Apply(theOffset, theResult);
    return theResult;
}

// FUNCTION: EXE 0x4e88c9
// Moves the puff along its slowing velocity plus the world's wind (rising slightly); returns 7 (remove) once expired
long EEE2DWinterBreath::Step()
{
    GEVector theWind = gWorld.mWindDirection;
    GE3DPoint thePosition;
    long theResult = 0;
    float theElapsed = gPhysics->GetTickTime();
    float theStep = theElapsed * this->mSpeed / 1000.0f;
    this->mAge += theElapsed;
    if (this->mAge <= this->mDuration) {
        float theFade = 1.0f - this->mAge / this->mSlowDownTime;
        if (theFade < 0.0f)
            theFade = 0.0f;
        theStep = theFade * theFade * theStep;
        thePosition.SetX(this->mVelocity.GetX() * theStep + theWind.GetX() * theElapsed * 0.04f + this->mPosition.GetX());
        thePosition.SetY(this->mVelocity.GetY() * theStep + theWind.GetY() * theElapsed * 0.04f + this->mPosition.GetY());
        thePosition.SetZ((theWind.GetZ() + 0.0007f) * theElapsed * 0.04f + this->mVelocity.GetZ() * theStep + this->mPosition.GetZ());
        this->SetPosition(thePosition);
    } else {
        theResult = 7;
    }
    return theResult;
}
