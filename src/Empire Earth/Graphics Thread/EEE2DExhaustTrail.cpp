// C:\EEX\Empire Earth\Graphics Thread\EEE2DExhaustTrail.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DExhaustTrail-Data"). 0xdf68d..0xdfd03: the key chain, both ctors .. UpdatePart, then the part iterator's
// operator+ (its first user in link order); EEE2DExplosion.cpp follows.
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
#include "ee/EEE2DExhaustTrail.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4df68d
// SYNTHETIC: EXE 0x4df697
// SYNTHETIC: EXE 0x4df6a8
// SYNTHETIC: EXE 0x4df6b4
const UString EEE2DExhaustTrail::sData("EEE2DExhaustTrail-Data");

#include "ee/EEGamePreferences.h"
// the trail sprite grows to seven times the effect height over the animation (0x43fb18, loaded through memory; Virtual14)
static const float kExhaustTrailGrowth = 6.0f;

// FUNCTION: EXE 0x4df6d0
// Creates an exhaust trail for a unit, spinning clockwise or counter-clockwise at random
EEE2DExhaustTrail::EEE2DExhaustTrail(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DExhaustTrail, false), mAngle(0.0f)
{
    this->mDrawAngle = 0.0f;
    this->mClockwise = EEEffect::GetRandom() < 0.5f ? 1 : 0;
}

// SYNTHETIC: EXE 0x4df71a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4df736
// Creates an empty exhaust trail for loading from a saved game
EEE2DExhaustTrail::EEE2DExhaustTrail()
    : EEEffect2D(gEffect2DModel, kEffect2DExhaustTrail, false)
{
}

// FUNCTION: EXE 0x4df752
// Destroys the exhaust trail (members only)
EEE2DExhaustTrail::~EEE2DExhaustTrail()
{
}

long EEE2DExhaustTrail::DrawPass2()
{
    return 0;
}

long EEE2DExhaustTrail::DrawPass4()
{
    return 0;
}

long EEE2DExhaustTrail::DrawPass6()
{
    return 0;
}

// NEAR: the part iterator operator+ (0xdfcee) is out of line in the original, expanded in ours
// FUNCTION: EXE 0x4df75d
// Queues the trail in render queue 3 under its texture for this screen, snapshotting its spin and part frames
long EEE2DExhaustTrail::Queue(EEEffectQueue* inQueues)
{
    unsigned long theCount = this->mParts.size();
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAngle = this->mAngle;
        for (unsigned long i = 0; i < theCount; i++)
            this->mParts[i].SetDrawFrame((this->mParts.begin() + i)->mFrame);
        GETexture* theTexture;
        if (this->GetGraphicEffect()->GetGraphicObject())
            theTexture = this->GetGraphicEffect()->GetGraphicObject()->GetTexture(gGamePreferences.mSettings.GetScreenID());
        else
            theTexture = NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DExhaustTrail::PrepareQueue()
{
}

// FUNCTION: EXE 0x4df816
// Creates the trail's sprite parts and sets its position and bounding radius from them; returns 0
long EEE2DExhaustTrail::SetUp()
{
    GE3DPoint thePosition;
    float theHeight;
    this->CreateParts(thePosition, theHeight, 0.0f);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(theHeight);
    return 0;
}

// FUNCTION: EXE 0x4df857
// Draws each part as a spun alpha-blended sprite, growing from the effect size to 7x as the first part animates
// Stops at the first draw error and returns it
long EEE2DExhaustTrail::DrawPass5()
{
    GETransformation theRotation;
    GETransformation theTransformation(false);
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    float theHeight = this->mGraphicEffect->GetSize();
    EEEffect2DPart* theFirst = &this->mParts[0];
    unsigned long theStartFrame = this->mParts[0].mGraphicObject->mStartFrames[0];
    unsigned long theFrameCount = this->mParts[0].mGraphicObject->mFrameCounts[0];
    float theGrowth = ((float)theFirst->mDrawFrame - (long)theStartFrame) / (long)theFrameCount * theHeight;
    float theScale = theGrowth * kExhaustTrailGrowth + theHeight;
    unsigned long theScreen = gScreenIndex;
    long theResult = 0;
    unsigned long theCount = this->mParts.size();
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    theRotation.SetOrientationYPR(0.0f, 0.0f, this->mDrawAngle);
    gWorld.mTransformation.Apply(theRotation, theTransformation);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    SetPrelitLighting();
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        GETexture* theTexture = this->mParts[i].mGraphicObject->mTextures[0];
        theTexture->SetFrame(this->mParts[i].mDrawFrame);
        theMaterial->SetTexture(theTexture, theScreen);
        const GE3DPoint& thePosition = this->mParts[i].mPosition;
        theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->SetGraphicsScale(theScale);
        theModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    return theResult;
}

// FUNCTION: EXE 0x4dfac1
// Reads the trail's spin angle and direction; returns the first read error (the base class result is ignored)
long EEE2DExhaustTrail::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    long theResult = inFile.Read(this->mAngle);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mClockwise);
    return theResult;
}

// FUNCTION: EXE 0x4dfaf6
// Writes the trail's spin angle and direction; returns the first write error (the base class result is ignored)
long EEE2DExhaustTrail::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mAngle);
    if (theResult == 0)
        theResult = outFile.Write(this->mClockwise);
    return theResult;
}

// FUNCTION: EXE 0x4dfb2b
// Spins the trail one turn per second and animates its unfinished parts; returns 7 (remove) once all parts are done
long EEE2DExhaustTrail::Step()
{
    long theResult = 0;
    unsigned long theCount = this->mParts.size();
    float theSpin = gPhysics->GetTickSeconds() * 6.2831855f;
    unsigned long theDone = 0;
    if (this->mClockwise) {
        this->mAngle += theSpin;
        if (this->mAngle > 3.1415927f)
            this->mAngle -= 6.2831855f;
    } else {
        this->mAngle -= theSpin;
        if (this->mAngle < -3.1415927f)
            this->mAngle += 6.2831855f;
    }
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mParts[i].mIsDone == false)
            this->UpdatePart(i);
        if (this->mParts[i].mIsDone)
            theDone++;
    }
    if (theDone == theCount)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4dfbe6
// Advances one part's animation by the tick, marking it done once it passes its last frame
void EEE2DExhaustTrail::UpdatePart(unsigned long inIndex)
{
    float theTime = this->mParts[inIndex].mTime;
    unsigned long theFrame = this->mParts[inIndex].mFrame;
    unsigned long theFrameTime = this->mParts[inIndex].mGraphicObject->mFrameTimes[0];
    unsigned long theLastFrame = (this->mParts.begin() + inIndex)->mGraphicObject->mLastFrames[0];
    theTime = gPhysics->GetTickTime() + theTime;
    if (theTime >= (float)theFrameTime) {
        unsigned long theFrames = UMFloatToLong(theTime / (float)theFrameTime);
        theTime -= (float)(theFrames * theFrameTime);
        theFrame = theFrames + theFrame;
        bool theDone = theFrame > theLastFrame;
        this->mParts[inIndex].mIsDone = theDone;
        if (theDone)
            return;
    }
    this->mParts[inIndex].mFrame = theFrame;
    this->mParts[inIndex].mTime = theTime;
}

// 0xdfcee: std::_Ptrit<EEEffect2DPart>::operator+ (the template COMDAT, first used by Queue, called out of line
// by every 2D effect in the original)

// Template/inline COMDATs this unit instantiates
// TEMPLATE: EXE 0x4dfcee
