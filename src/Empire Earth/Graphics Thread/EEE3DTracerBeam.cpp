// C:\EEX\Empire Earth\Graphics Thread\EEE3DTracerBeam.cpp (file name inferred: no __FILE__).
// Original TU RVA0xfc382..0xfd330: guarded class key chain,19-function RTTI class family, then
// registered EEE3DTracers key chain at 0xfd330.
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
#include "lle/SESoundManager.h"

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
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEEffectManager.h"
#include "ee/EEE3DTracerBeam.h"


// SYNTHETIC: EXE 0x4fc382
// SYNTHETIC: EXE 0x4fc38c
// SYNTHETIC: EXE 0x4fc39d
// SYNTHETIC: EXE 0x4fc3a9
const UString EEE3DTracerBeam::sData("EEE3DTracerBeam-Data");

// FUNCTION: EXE 0x4fc3c5
// Creates a beam from a unit's weapon tag point to a random point on the target unit (referenced) and aims it;
// tests the target for null before referencing it but then uses it unconditionally
EEE3DTracerBeam::EEE3DTracerBeam(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inFrom, inGraphicEffect, inKind)
{
    this->mTarget = inTo;
    if (this->mTarget)
        this->mTarget->Reference();
    this->mGraphicObject = NULL;
    this->mAge = 0.0f;
    this->mIsTimed = false;
    this->mIsImpactCreated = false;
    this->mDrawAge = 0.0f;
    this->mScroll = 0.0f;
    // Pick a point on the target model only when there is a target (the null test above already allows none).
    if (this->mTarget)
        this->mTargetPoint = this->mTarget->GetModel()->PickPoint();
    this->mWeaponTagPoint = inFrom->mComplexUnit->mWeaponTagPoint;
    this->Aim();
}

// SYNTHETIC: EXE 0x4fc481
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fc49d
// Creates a beam from a unit's weapon tag point to a fixed point and aims it
EEE3DTracerBeam::EEE3DTracerBeam(EEUnit* inFrom, GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inFrom, inGraphicEffect, inKind)
{
    this->mAge = 0.0f;
    this->mTarget = NULL;
    this->mGraphicObject = NULL;
    this->mIsTimed = false;
    this->mDrawAge = 0.0f;
    this->mIsImpactCreated = false;
    this->mScroll = 0.0f;
    this->mTargetPoint = inTo;
    this->mWeaponTagPoint = inFrom->mComplexUnit->mWeaponTagPoint;
    this->Aim();
}

// FUNCTION: EXE 0x4fc543
// Creates an empty beam of the given kind (for loading)
EEE3DTracerBeam::EEE3DTracerBeam(long inKind)
    : EEEffect3D(inKind)
{
    this->mIsImpactCreated = false;
}

// FUNCTION: EXE 0x4fc574
// Destroys the beam, unloading its media and releasing the target unit
EEE3DTracerBeam::~EEE3DTracerBeam()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    if (this->mTarget)
        this->mTarget->Dereference();
}

// FUNCTION: EXE 0x4fc5a4
// Places the beam between the shooter's weapon tag point (or tag kind 6, or the default point) and its target point,
// growing from the start until age param 5 and shrinking towards the end after age param 6
void EEE3DTracerBeam::Aim()
{
    GE3DPoint theEnd;
    GE3DPoint theStart;
    GETransformation theFrom(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GETransformation theTo(false);
    GETransformation thePart(false);
    GETransformation theBeam(false);
    GEVector theDirection;
    EEGraphicObject* theGraphic = this->mUnit->GetModel();
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    float theTargetScale = 1.0f;
    EETagPointList* theTags = theGraphic->mTagPoints[theUnit->GetAnimation()];
    float theScale = theUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long thePoint = this->mWeaponTagPoint;
    if (this->mTarget) {
        theTo = this->mTarget->mComplexUnit->mPhysicalModel.GetLocation();
        theTargetScale = this->mTarget->mComplexUnit->mPhysicalModel.GetScale();
    } else
        theTo.MakeIdentity();
    if (this->mGraphicEffect->mIntParam1 == 1) {
        EETagPointList* theOtherTags = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 6);
        if (theOtherTags && theOtherTags->GetNumTagPoints() != 0) {
            theOtherTags->Scale(theFrom, theScale);
            theOtherTags->GetTagPoint(0)->GetPoint(theFrame, theFrom, theStart);
        }
    } else if (this->mGraphicEffect->mIntParam1 == 0 && theTags && theTags->GetNumTagPoints() != 0) {
        theTags->Scale(theFrom, theScale);
        if (theGraphic->GetNumParts() > 0) {
            unsigned long thePartIndex;
            thePart.MakeIdentity();
            if (theGraphic->FindTurretPart(theTags->GetTagPoint(thePoint)->mModel, thePartIndex) == 0)
                thePart.SetOrientationYPR(theUnit->mTurretAngles[thePartIndex], 0.0f, 0.0f);
            theTags->GetTagPoint(thePoint)->GetPoint(theFrame, theFrom, thePart, theStart);
        } else
            theTags->GetTagPoint(thePoint)->GetPoint(theFrame, theFrom, theStart);
        theTags->Scale(theTo, theTargetScale);
    } else
        theFrom.Apply(this->mUnit->mStats->mModel->mDefaultPoint, theStart);
    if (this->mTarget)
        theTo.Apply(this->mTargetPoint, theEnd);
    else
        theEnd = this->mTargetPoint;
    if (this->mGraphicEffect->GetParam5() > 0.0f && this->mAge < this->mGraphicEffect->GetParam5()) {
        float theFraction = this->mAge / this->mGraphicEffect->GetParam5();
        theEnd.SetX((theEnd.GetX() - theStart.GetX()) * theFraction + theStart.GetX());
        theEnd.SetY((theEnd.GetY() - theStart.GetY()) * theFraction + theStart.GetY());
        theEnd.SetZ((theEnd.GetZ() - theStart.GetZ()) * theFraction + theStart.GetZ());
    }
    if (this->mGraphicEffect->GetParam6() > 0.0f && this->mGraphicEffect->GetParam6() < this->mAge) {
        float theFraction = (this->mAge - this->mGraphicEffect->GetParam6()) / (1.0f - this->mGraphicEffect->GetParam6());
        theStart.SetX((theEnd.GetX() - theStart.GetX()) * theFraction + theStart.GetX());
        theStart.SetY((theEnd.GetY() - theStart.GetY()) * theFraction + theStart.GetY());
        theStart.SetZ((theEnd.GetZ() - theStart.GetZ()) * theFraction + theStart.GetZ());
    }
    theDirection.SetX(theEnd.GetX() - theStart.GetX());
    theDirection.SetY(theEnd.GetY() - theStart.GetY());
    theDirection.SetZ(theEnd.GetZ() - theStart.GetZ());
    this->mLength = theDirection.GetMagnitude();
    theDirection.Scale(0.5f, theDirection);
    theStart += theDirection;
    theDirection.Normalize();
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    theBeam.SetX(theStart.GetX());
    theBeam.SetY(theStart.GetY());
    theBeam.SetZ(theStart.GetZ());
    theBeam.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theBeam.SetElement(3, 0, 0.0f);
    theBeam.SetElement(3, 1, 0.0f);
    theBeam.SetElement(3, 2, 0.0f);
    theBeam.SetElement(3, 3, 1.0f);
    this->SetTransformation(theBeam);
}
// FUNCTION: EXE 0x4fc9d7
// Snapshots the beam's age, length, scroll and width for drawing and queues it in render queue 4 under its texture
long EEE3DTracerBeam::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawLength = this->mLength;
        this->mDrawScroll = this->mScroll;
        this->mDrawWidth = this->mWidth;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}
// FUNCTION: EXE 0x4fca62
// Loads the beam's media, makes it timed unless the effect's flag 1 is set, and starts at width param 4
long EEE3DTracerBeam::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mIsTimed = !this->mGraphicEffect->mFlag1;
    this->mWidth = this->mGraphicEffect->GetParam4();
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4fca98
// Draws nothing in this pass (returns 0)
long EEE3DTracerBeam::DrawPass2()
{
    return 0;
}

long EEE3DTracerBeam::DrawPass4()
{
    return 0;
}

long EEE3DTracerBeam::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4fca9b
// Draws the beam model stretched to its length and width with its texture scrolled, additive when the effect's flag 2
// is set; restores the model's vertices, texture coordinates and sub-model offsets
long EEE3DTracerBeam::DrawPass6()
{
    std::vector<float> theOldYs;
    std::vector<float> theOldXs;
    std::vector<float> theOldUs;
    std::vector<float> theOldModelYs;
    GETransformation theScale;
    GETransformation theTransformation(this->mTransformation);
    GETransformation theCombined(false);
    GETransformation theModelTransform(false);
    GETextureCoordinate theCoordinate;
    GE3DPoint thePoint;
    float theValue;
    GEModel* theModel = this->mGraphicObject->mModel;
    unsigned long theScreen = gScreenIndex;
    float theSize = this->mGraphicEffect->GetSize();
    bool theColoured = this->mGraphicEffect->mFlag2;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    if (theColoured)
        gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    else
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    Fn10F7C8();
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    if (theColoured) {
        GE3DPoint theLocation = theTransformation.GetPosition();
        Fn11A130(theLocation, theFog, theValue);
    }
    theScale.SetScale(theSize, theSize, theSize);
    theTransformation.Apply(theScale, theCombined);
    theModel->SetGraphicsPostModelTransform(theCombined);
    unsigned long i;
    for (i = 0; i < theModel->GetVertexArray(0)->GetSize(); i++) {
        theModel->GetVertexArray(0)->GetPoint(thePoint, i);
        float theY = thePoint.GetY();
        theOldYs.push_back(theY);
        float theX = thePoint.GetX();
        theOldXs.push_back(theX);
        thePoint.SetY(thePoint.GetY() * this->mDrawLength * 1.15f);
        thePoint.SetX(thePoint.GetX() * this->mDrawWidth);
        theModel->GetVertexArray(0)->SetPoint(thePoint, i);
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, i);
        float theU = theCoordinate.GetU();
        theOldUs.push_back(theU);
        theU += this->mDrawScroll;
        theCoordinate.SetU(theU);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, i);
    }
    long j;
    for (j = 0; j < (long)theModel->GetSubModelCount(); j++) {
        GEModel* theSubModel = theModel->GetSubModel(j);
        theModelTransform = theSubModel->GetInternalTransform(0);
        float theY = theModelTransform.GetY();
        theOldModelYs.push_back(theY);
        theModelTransform.SetY(theModelTransform.GetY() * this->mDrawLength * 1.15f);
        theSubModel->SetInternalTransform(0, theModelTransform);
    }
    theModel->ComputeBoundingVolumes();
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    for (i = 0; i < theModel->GetVertexArray(0)->GetSize(); i++) {
        theModel->GetVertexArray(0)->GetPoint(thePoint, i);
        thePoint.SetY(theOldYs[i]);
        thePoint.SetX(theOldXs[i]);
        theModel->GetVertexArray(0)->SetPoint(thePoint, i);
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, i);
        theCoordinate.SetU(theOldUs[i]);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, i);
    }
    for (j = 0; j < (long)theModel->GetSubModelCount(); j++) {
        GEModel* theSubModel = theModel->GetSubModel(j);
        theModelTransform = theSubModel->GetInternalTransform(0);
        theModelTransform.SetY(theOldModelYs[j]);
        theSubModel->SetInternalTransform(0, theModelTransform);
    }
    return theResult;
}
// FUNCTION: EXE 0x4fcf21
// Reads the target unit's id, age, timing, length, scroll, width, target point and (from version 0.250) weapon tag
// point; returns the first read error
long EEE3DTracerBeam::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mTarget = (EEUnit*)-1;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    unsigned long theID;
    if (theResult == 0 && (theResult = inFile.Read(theID)) == 0) {
        this->mTarget = reinterpret_cast<EEUnit*>(theID);  // serialized ID; RepairPointers resolves it through gWorld.GetUnit
        if ((theResult = inFile.Read(this->mAge)) == 0 && (theResult = inFile.Read(this->mIsTimed)) == 0
            && (theResult = inFile.Read(this->mLength)) == 0 && (theResult = inFile.Read(this->mScroll)) == 0
            && (theResult = inFile.Read(this->mWidth)) == 0 && (theResult = this->mTargetPoint.Load(inFile)) == 0
            && (inMajorVersion > 0 || inMinorVersion >= 0xfa))
            return inFile.Read(this->mWeaponTagPoint);
    }
    this->mWeaponTagPoint = 0;
    return theResult;
}

// FUNCTION: EXE 0x4fd000
// After loading, turns the saved target id back into a referenced unit; returns 7 if that unit no longer exists
long EEE3DTracerBeam::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0 && this->mTarget != (EEUnit*)-1) {
        this->mTarget = dynamic_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget)));
        if (this->mTarget == NULL)
            theResult = 7;
        else
            this->mTarget->Reference();
    } else
        this->mTarget = NULL;
    return theResult;
}

// FUNCTION: EXE 0x4fd04e
// Writes the target unit's id (-1 for none), age, timing, length, scroll, width, target point and weapon tag point;
// returns the first write error
long EEE3DTracerBeam::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theID = (unsigned long)-1;
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    if (this->mTarget)
        theID = this->mTarget->GetUnitID();
    if ((theResult = outFile.Write(theID)) == 0 && (theResult = outFile.Write(this->mAge)) == 0
        && (theResult = outFile.Write(this->mIsTimed)) == 0 && (theResult = outFile.Write(this->mLength)) == 0
        && (theResult = outFile.Write(this->mScroll)) == 0 && (theResult = outFile.Write(this->mWidth)) == 0
        && (theResult = this->mTargetPoint.Save(outFile)) == 0)
        theResult = outFile.Write(this->mWeaponTagPoint);
    return theResult;
}

// FUNCTION: EXE 0x4fd10f
// Replaces the shooter as the base effect does, and the target too unless the effect keeps its unit
void EEE3DTracerBeam::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    EEEffect::ReplaceUnit(inOldUnit, inNewUnit);
    if (this->mTarget != inOldUnit || this->mKeepsUnit)
        return;
    this->mTarget->Dereference();
    this->mTarget = inNewUnit;
    this->mTarget->Reference();
}

// FUNCTION: EXE 0x4fd140
// Re-aims the beam, scrolls its texture and pulses its width; a timed beam ages, places its impact effect after age
// param 7, reports a hit to the target at age param 5 and returns 7 past age 1
long EEE3DTracerBeam::Step()
{
    long theResult = 0;
    this->Aim();
    float theLength = this->mLength;
    if (theLength < 1.0f)
        theLength = 1.0f;
    this->mScroll = (gPhysics->GetTickTime() * this->mGraphicEffect->GetParam1()) / theLength + this->mScroll;
    if (this->mScroll > 1.0f)
        this->mScroll -= 1.0f;
    if (this->mScroll < 0.0f)
        this->mScroll += 1.0f;
    this->mWidth += gPhysics->GetTickTime() * this->mGraphicEffect->GetParam2();
    if (this->mWidth > this->mGraphicEffect->GetParam4())
        this->mWidth = this->mGraphicEffect->GetParam3();
    if (this->mIsTimed) {
        float thePreviousTime = this->mAge;
        this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
        if (this->mGraphicEffect->mLinkedEffect1 && !this->mIsImpactCreated && this->mAge > this->mGraphicEffect->GetParam7()) {
            GETransformation theTransformation;
            GE3DPoint theLocation;
            if (this->mTarget) {
                theTransformation = this->mTarget->mComplexUnit->mPhysicalModel.GetLocation();
                theTransformation.Apply(this->mTargetPoint, theLocation);
            } else {
                theTransformation.MakeIdentity();
                theLocation = this->mTargetPoint;
            }
            EEGraphicEffect* theSubEffect = this->mGraphicEffect->mLinkedEffect1;
            theResult = gEffectManager->CreatePlacedEffect(theLocation, theSubEffect, NULL);
            this->mIsImpactCreated = true;
        }
        if (this->mTarget && thePreviousTime <= this->mGraphicEffect->GetParam5() && this->mGraphicEffect->GetParam5() <= this->mAge)
            this->mTarget->mUnitClass->UnitDamaged(this->mTarget->mComplexUnit, NULL, 0);
        if (this->mAge > 1.0f)
            theResult = 7;
    }
    return theResult;
}

// ICF-shared empty slot at RVAe123d.
void EEE3DTracerBeam::PrepareQueue()
{
}
