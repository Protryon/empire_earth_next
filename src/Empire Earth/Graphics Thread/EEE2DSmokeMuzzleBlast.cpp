// C:\EEX\Empire Earth\Graphics Thread\EEE2DSmokeMuzzleBlast.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DSmokeMuzzleBlast-Data"). 0xe6621..0xe6ef2: the key chain, three ctors .. Step; EEE2DSmokePlume.cpp
// follows.
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
#include "ee/EEE2DSmokeMuzzleBlast.h"
#include "ee/EEEffectKindIDs.h"
// the lift over a flying or raised target (0x440014; this unit's own .rdata constant, read once; provisional name)
static const float kSMBTargetLift = 15.0f;

// SYNTHETIC: EXE 0x4e6621
// SYNTHETIC: EXE 0x4e662b
// SYNTHETIC: EXE 0x4e663c
// SYNTHETIC: EXE 0x4e6648
const UString EEE2DSmokeMuzzleBlast::sData("EEE2DSmokeMuzzleBlast-Data");

// NEAR: as the second ctor; the target transformation slots
// FUNCTION: EXE 0x4e6664
// Creates a muzzle smoke puff at the shooter's weapon tag point (turret-aware), drifting toward the target's model
// point (15 higher for high-arc shooters), with a random rotation
EEE2DSmokeMuzzleBlast::EEE2DSmokeMuzzleBlast(EEUnit* inUnit, EEUnit* inTarget, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DSmokeMuzzleBlast, false)
{
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GETransformation theTargetTransformation(inTarget->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    unsigned long thePart = theUnit->mAnimation;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[thePart];
    float theScale = theUnit->mPhysicalModel.GetScale();
    float theTargetScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    EETagPointList::Scale(theTransformation, theScale);
    if (theModel->GetNumParts() > 0) {
        GETransformation thePartTransformation(false);
        unsigned long theModelIndex;
        thePartTransformation.MakeIdentity();
        if (theModel->FindTurretPart(theList->mPoints[theIndex]->mModel, theModelIndex) == 0)
            thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
        (*(theList->mPoints.begin() + theIndex))->GetPoint(theFrame, theTransformation, thePartTransformation,
                                                            theStart);
    } else {
        (*(theList->mPoints.begin() + theIndex))->GetPoint(theFrame, theTransformation, theStart);
    }
    EETagPointList::Scale(theTargetTransformation, theTargetScale);
    theOffset = inTarget->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    if (inUnit->mUnitClass->mShootsHigh || (inUnit->mStats->mCanFireOverWalls && inUnit->mComplexUnit->mIsFiringHigh))
        theEnd.SetZ(theEnd.GetZ() + kSMBTargetLift);
    this->mVelocity.SetX(theEnd.GetX() - theStart.GetX());
    this->mVelocity.SetY(theEnd.GetY() - theStart.GetY());
    this->mVelocity.SetZ(theEnd.GetZ() - theStart.GetZ());
    this->mVelocity.Normalize();
    this->SetPosition(theStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mRotation = EEEffect::GetRandom() * 6.2831855f;
}

// SYNTHETIC: EXE 0x4e6902
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e691e
// Creates a muzzle smoke puff at the shooter's weapon tag point (turret-aware), drifting toward a target point
EEE2DSmokeMuzzleBlast::EEE2DSmokeMuzzleBlast(EEUnit* inUnit, const GE3DPoint& inTarget,
                                             EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DSmokeMuzzleBlast, false)
{
    GE3DPoint theStart;
    GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    unsigned long thePart = theUnit->mAnimation;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[thePart];
    float theScale = theUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    EETagPointList::Scale(theTransformation, theScale);
    if (theModel->GetNumParts() > 0) {
        GETransformation thePartTransformation(false);
        unsigned long theModelIndex;
        thePartTransformation.MakeIdentity();
        GEModel* theTagModel = theList->GetTagPoint(theIndex)->mModel;
        if (theModel->FindTurretPart(theTagModel, theModelIndex) == 0) {
            float theYaw = theUnit->mTurretAngles[theModelIndex];
            thePartTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        }
        theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, thePartTransformation,
                                                            theStart);
    } else {
        theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, theStart);
    }
    this->mVelocity.SetX(inTarget.GetX() - theStart.GetX());
    this->mVelocity.SetY(inTarget.GetY() - theStart.GetY());
    this->mVelocity.SetZ(inTarget.GetZ() - theStart.GetZ());
    this->mVelocity.Normalize();
    this->SetPosition(theStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mRotation = EEEffect::GetRandom() * 6.2831855f;
}

// FUNCTION: EXE 0x4e6b0a
// Creates an empty muzzle smoke puff for loading from a saved game
EEE2DSmokeMuzzleBlast::EEE2DSmokeMuzzleBlast()
    : EEEffect2D(gEffect2DModel, kEffect2DSmokeMuzzleBlast, false)
{
}

// FUNCTION: EXE 0x4e6b44
// Destroys the puff, unloading its graphic's media
EEE2DSmokeMuzzleBlast::~EEE2DSmokeMuzzleBlast()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DSmokeMuzzleBlast::DrawPass2()
{
    return 0;
}

long EEE2DSmokeMuzzleBlast::DrawPass4()
{
    return 0;
}

long EEE2DSmokeMuzzleBlast::DrawPass5()
{
    return 0;
}

// ICF-folded into EEE2DWinterBreath's 0xe844d
long EEE2DSmokeMuzzleBlast::Queue(EEEffectQueue* inQueues)
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

void EEE2DSmokeMuzzleBlast::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e6b62
// Loads the puff's graphic; returns the load result
long EEE2DSmokeMuzzleBlast::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e6b76
// Draws the puff as a growing, fading smoke sprite, then up to two smaller rotated puffs trailing back along its
// path once their delays (time 1, twice time 1) have passed; returns 0
long EEE2DSmokeMuzzleBlast::DrawPass6()
{
    GEColor theColor;
    GE3DPoint thePosition(this->mDrawPosition);
    float theAge = this->mDrawAge / this->mGraphicEffect->mDuration;
    float theSize = (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * theAge + this->mGraphicEffect->GetStartSize();
    float theSize2 = theSize * 0.7f;
    float theSize3 = theSize * 0.55f;
    float theDelay = this->mDrawAge - this->mGraphicEffect->mTime1;
    float theDelay2 = this->mDrawAge - this->mGraphicEffect->mTime1 * 2.0f;
    float theAlpha = 1.0f - (this->mGraphicEffect->GetParam3() * theAge + (1.0f - this->mGraphicEffect->GetParam3()));
    float theOffset = -this->mGraphicEffect->mAlpha;
    AddBlendedSprite(thePosition, theColor, this->mRotation, theSize, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    if (!(theDelay >= 0.0f))
        return 0;
    thePosition.SetX(thePosition.GetX() + this->mVelocity.GetX() * theOffset);
    thePosition.SetY(thePosition.GetY() + this->mVelocity.GetY() * theOffset);
    thePosition.SetZ(thePosition.GetZ() + this->mVelocity.GetZ() * theOffset);
    AddBlendedSprite(thePosition, theColor, this->mRotation + 1.0471976f, theSize2, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    if (!(theDelay2 >= 0.0f))
        return 0;
    thePosition.SetX(thePosition.GetX() + this->mVelocity.GetX() * theOffset);
    thePosition.SetY(thePosition.GetY() + this->mVelocity.GetY() * theOffset);
    thePosition.SetZ(thePosition.GetZ() + this->mVelocity.GetZ() * theOffset);
    AddBlendedSprite(thePosition, theColor, this->mRotation + 2.0943952f, theSize3, theAlpha, 1.0f,
             this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f);
    return 0;
}

// FUNCTION: EXE 0x4e6d5a
// Reloads the puff's graphic and reads its direction, age and rotation; returns the first read error (base ignored)
long EEE2DSmokeMuzzleBlast::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mVelocity.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mAge);
    if (theResult == 0)
        theResult = inFile.Read(this->mRotation);
    return theResult;
}

// FUNCTION: EXE 0x4e6dae
// Writes the puff's direction, age and rotation; returns the first write error (the base class result is ignored)
long EEE2DSmokeMuzzleBlast::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mVelocity.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mRotation);
    return theResult;
}

// FUNCTION: EXE 0x4e6df1
// Ages the puff and moves it along its direction (slowing to a stop by parameter 2's age) while drifting with
// the wind and rising; returns 7 (remove) once its duration is over
long EEE2DSmokeMuzzleBlast::Step()
{
    GEVector theWind = gWorld.mWindDirection;
    GE3DPoint thePosition;
    long theResult = 0;
    float theElapsed = gPhysics->GetTickSeconds() * 1000.0f;
    float theStep = this->mGraphicEffect->GetParam1() * gPhysics->GetTickSeconds();
    this->mAge += theElapsed;
    if (this->mAge <= this->mGraphicEffect->mDuration) {
        float theFade = 1.0f - this->mAge / this->mGraphicEffect->GetParam2();
        if (theFade < 0.0f)
            theFade = 0.0f;
        theStep = theFade * theFade * theStep;
        thePosition.SetX(this->mVelocity.GetX() * theStep + theWind.GetX() * theElapsed / 10.0f + this->mPosition.GetX());
        thePosition.SetY(this->mVelocity.GetY() * theStep + theWind.GetY() * theElapsed / 10.0f + this->mPosition.GetY());
        thePosition.SetZ((theWind.GetZ() + 0.00035f) * theElapsed / 10.0f + this->mVelocity.GetZ() * theStep + this->mPosition.GetZ());
        this->SetPosition(thePosition);
    } else {
        theResult = 7;
    }
    return theResult;
}
