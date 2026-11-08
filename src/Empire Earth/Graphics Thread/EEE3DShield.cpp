// C:\EEX\Empire Earth\Graphics Thread\EEE3DShield.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DShield-Data"). 0xf923f..0xf9871: the key chain, both ctors .. Virtual4C; EEE3DSmokeGenerator.cpp
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
#include "ee/EEE3DShield.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4f923f
// SYNTHETIC: EXE 0x4f9249
// SYNTHETIC: EXE 0x4f925a
// SYNTHETIC: EXE 0x4f9266
const UString EEE3DShield::sData("EEE3DShield-Data");

// FUNCTION: EXE 0x4f9282
// Creates a shield effect around a unit, with no graphic object yet
EEE3DShield::EEE3DShield(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DShield), mGraphicObject(NULL)
{
}

// SYNTHETIC: EXE 0x4f92c6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f92e2
// Creates an empty shield effect (for loading)
EEE3DShield::EEE3DShield()
    : EEEffect3D(kEffect3DShield)
{
}

// FUNCTION: EXE 0x4f9305
// Destroys the shield, unloading its graphic object's media
EEE3DShield::~EEE3DShield()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DShield::DrawPass2()
{
    return 0;
}

long EEE3DShield::DrawPass4()
{
    return 0;
}

long EEE3DShield::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4f932c
// Snapshots the shield's age, opacity, texture scroll and location for drawing and queues it in render queue 3
long EEE3DShield::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawOpacity = this->mOpacity;
        this->mDrawScroll = this->mScroll;
        this->mTransformation.GetPosition(this->mLocation);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DShield::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f93bd
// Loads the shield's media, records the unit's hit points, starts at the resting opacity (param 5) with no scroll and
// moves it onto the unit; returns the media load result
long EEE3DShield::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mAge = 0.0f;
    this->mHitPoints = this->mUnit->mHitPoints;
    this->mDrawOpacity = this->mGraphicEffect->GetParam5();
    this->mOpacity = this->mGraphicEffect->GetParam5();
    this->mScrollingDown = false;
    this->mScroll = 0.0f;
    this->mDrawScroll = 0.0f;
    this->FollowUnit();
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4f942a
// Draws the shield model with its texture scrolled vertically, alpha blended at its opacity, fading out over
// the last tenth of its life; restores the model's texture coordinates and opacity
// the inline SetPosition(point)'s inner SetPosition(float x3) is refused here (the imported call)
long EEE3DShield::DrawPass5()
{
    std::vector<float> theCoordinates;
    GETransformation theTransformation;
    GETextureCoordinate theCoordinate;
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetUnitLighting();
    theTransformation.SetPosition(this->mLocation);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    unsigned long i;
    float theV;
    for (i = 0; i < theModel->GetVertexArray(0)->GetSize(); i++) {
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, i);
        theV = theCoordinate.GetV();
        theCoordinates.push_back(theV);
        theV += this->mDrawScroll;
        theCoordinate.SetV(theV);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, i);
    }
    theModel->PrepareTransforms();
    theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    long theResult;
    if (this->mDrawAge >= 0.9f) {
        theModel->SetOpacity((1.0f - this->mDrawAge) * 10.0f);
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    } else {
        theModel->SetOpacity(this->mDrawOpacity);
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    theModel->SetOpacity(1.0f);
    for (unsigned long j = 0; j < theModel->GetVertexArray(0)->GetSize(); j++) {
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, j);
        theCoordinate.SetV(theCoordinates[j]);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, j);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f966d
// Reads the shield's age, opacity, hit points, scroll and (from version 0.180) scroll direction; returns the read error
long EEE3DShield::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mAge);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOpacity);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHitPoints);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mScroll);
    if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0xb4))
        theResult = inFile.Read(this->mScrollingDown);
    return theResult;
}

// FUNCTION: EXE 0x4f9701
// Writes the shield's age, opacity, hit points, scroll and scroll direction; returns the first write error
long EEE3DShield::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mOpacity);
    if (theResult == 0)
        theResult = outFile.Write(this->mHitPoints);
    if (theResult == 0)
        theResult = outFile.Write(this->mScroll);
    if (theResult == 0)
        theResult = outFile.Write(this->mScrollingDown);
    return theResult;
}

// FUNCTION: EXE 0x4f9775
// Ages the shield and scrolls its texture back and forth between params 2 and 3; flashes to param 6 opacity when the
// unit loses hit points, else fades towards param 5; returns 7 when the unit dies or the shield's time is up
long EEE3DShield::Step()
{
    float theElapsed = gPhysics->GetTickTime();
    long theResult = 0;
    if (this->mUnit->mHitPoints > 0) {
        this->mAge += theElapsed / this->mGraphicEffect->mDuration;
        if (this->mScrollingDown) {
            if (this->mGraphicEffect->GetParam2() > this->mScroll)
                this->mScrollingDown = false;
        } else {
            if (this->mGraphicEffect->GetParam3() < this->mScroll)
                this->mScrollingDown = true;
        }
        if (this->mScrollingDown)
            this->mScroll -= this->mGraphicEffect->GetParam1() * theElapsed;
        else
            this->mScroll += this->mGraphicEffect->GetParam1() * theElapsed;
        if (this->mAge <= 1.0f) {
            this->FollowUnit();
            if (this->mHitPoints > this->mUnit->mHitPoints)
                this->mOpacity = this->mGraphicEffect->GetParam6();
            else
                this->mOpacity = max(this->mGraphicEffect->GetParam5(), this->mOpacity - this->mGraphicEffect->GetParam4());
            this->mHitPoints = this->mUnit->mHitPoints;
        } else {
            theResult = 7;
        }
    } else {
        theResult = 7;
    }
    return theResult;
}

// ICF-folded into EEE3DBlastWave's 0xe904a
void EEE3DShield::FollowUnit()
{
    GE3DPoint thePoint;
    GETransformation theLocation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    float theScale = this->mUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = this->mUnit->mComplexUnit->mPhysicalModel.GetFrame();
    EETagPointList* theList = this->mUnit->mComplexUnit->GetModel()->GetTagPoints(this->mUnit->mComplexUnit->GetAnimation(), 0x12);
    if (theList != NULL && !theList->mPoints.empty()) {
        EETagPointList::Scale(theLocation, theScale);
        theList->mPoints.front()->GetPoint(theFrame, theLocation, thePoint);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        this->SetTransformation(theLocation);
    }
}
