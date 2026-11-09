// C:\EEX\Empire Earth\Graphics Thread\EEE2DExplosion.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xdfd03..0xe0389: both ctors .. SetSound; EEE2DFire.cpp's key chain follows. StopSound 0xdfd80 is the ICF-kept copy
// named EECalamity::StopSecondarySound (its marker sits after the inline stubs).
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
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEE2DExplosion.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEGamePreferences.h"
#include "ee/EERenderSnapshot.h"

// unit-local .rdata before the vtable (0x43fb24), loaded from memory: a named const
const float kExplosionHeightOffset = 0.4f;

// FUNCTION: EXE 0x4dfd03
// Creates an explosion effect for a unit, with no sound playing
EEE2DExplosion::EEE2DExplosion(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DExplosion, false), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4dfd2d
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4dfd49
// Creates an empty explosion for loading from a saved game
EEE2DExplosion::EEE2DExplosion()
    : EEEffect2D(gEffect2DModel, kEffect2DExplosion, false), mSound(NULL)
{
}

// FUNCTION: EXE 0x4dfd69
// Destroys the explosion, stopping and releasing its sound
EEE2DExplosion::~EEE2DExplosion()
{
    this->StopSound();
}

// 0xdfd80 (named EECalamity::StopSecondarySound: the ICF-kept copy)
void EEE2DExplosion::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

void EEE2DExplosion::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE2DExplosion::DrawPass2()
{
    return 0;
}

long EEE2DExplosion::DrawPass4()
{
    return 0;
}

long EEE2DExplosion::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4dfd80
// Stops the explosion's sound and releases it (StopSound above; the ICF-kept copy named EECalamity::StopSecondarySound)

// NEAR: the original calls the part iterator operator+ (0xdfcee) out of line; ours expands it (inline budget)
// FUNCTION: EXE 0x4dfda2
// Queues the explosion in render queue 4 under its texture for this screen, snapshotting each part's frame
long EEE2DExplosion::Queue(EEEffectQueue* inQueues)
{
    unsigned long theCount = this->mParts.size();
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        for (unsigned long i = 0; i < theCount; i++)
            this->mParts[i].SetDrawFrame((this->mParts.begin() + i)->mFrame);
        GETexture* theTexture;
        if (this->GetGraphicEffect()->GetGraphicObject())
            theTexture = this->GetGraphicEffect()->GetGraphicObject()->GetTexture(gGamePreferences.mSettings.GetScreenID());
        else
            theTexture = NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DExplosion::PrepareQueue()
{
}

// FUNCTION: EXE 0x4dfe55
// Creates the explosion's sprite parts 0.4 above the target and sets its position and bounding radius; returns 0
long EEE2DExplosion::SetUp()
{
    GE3DPoint thePosition;
    float theHeight;
    this->CreateParts(thePosition, theHeight, kExplosionHeightOffset);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(theHeight);
    return 0;
}

// NEAR: as Queue (operator+ out of line)
// FUNCTION: EXE 0x4dfe9a
// Draws each part as an additive sprite lit by the fog at its position, restoring the fog; returns the first draw error
long EEE2DExplosion::DrawPass6()
{
    GETransformation theTransformation(gRenderSnapshot.mCamera);
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    long theResult = 0;
    unsigned long theScreen = gGamePreferences.mSettings.GetScreenID();
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theCount = this->mParts.size();
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        GETexture* theTexture = this->mParts[i].mGraphicObject->mTextures[0];
        theTexture->SetFrame(this->mParts[i].mDrawFrame);
        theMaterial->SetTexture(theTexture, theScreen);
        float theValue;
        GetFogFactor(this->mParts[i].mPosition, theFog, theValue);
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
        const GE3DPoint& thePosition = (this->mParts.begin() + i)->mPosition;
        theTransformation.SetPosition(thePosition);
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->SetGraphicsScale(theScale);
        theModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e00ac
// Reads the base 2D effect state; returns its result
long EEE2DExplosion::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x4e00c0
// Writes the base 2D effect state; returns its result
long EEE2DExplosion::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
}

// FUNCTION: EXE 0x4e00d4
// Animates the unfinished parts; returns 7 (remove) once all parts are done
long EEE2DExplosion::Step()
{
    long theResult = 0;
    unsigned long theCount = this->mParts.size();
    unsigned long theDone = 0;
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

// FUNCTION: EXE 0x4e0145
// Advances one part's animation by the tick, marking it done past its last frame; the first part also
// starts or updates the explosion sound for its frame
void EEE2DExplosion::UpdatePart(unsigned long inIndex)
{
    GE3DPoint theLocation = this->mPosition;
    float theTime = this->mParts[inIndex].mTime;
    unsigned long theOldFrame = this->mParts[inIndex].mFrame;
    unsigned long theFrameTime = this->mParts[inIndex].mGraphicObject->mFrameTimes[0];
    unsigned long theStartFrame = this->mParts[inIndex].mGraphicObject->mStartFrames[0];
    unsigned long theLastFrame = this->mParts[inIndex].mGraphicObject->mLastFrames[0];
    unsigned long theFrame = theOldFrame;
    bool theFirstPart = inIndex == 0;
    bool theStart;
    if (this->mParts[inIndex].mFrame == theStartFrame && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    bool theDone = false;
    theTime = gPhysics->GetTickTime() + theTime;
    if (theTime >= (float)theFrameTime) {
        theFrame = UMFloatToLong(theTime / (float)theFrameTime);
        theTime -= (float)(theFrame * theFrameTime);
        theFrame += theOldFrame;
        theDone = theFrame > theLastFrame;
        this->mParts[inIndex].mIsDone = theDone;
    }
    // Single exit (an early return's dtor site would shift the inline budget shares).
    if (!theDone) {
        this->mParts[inIndex].mFrame = theFrame;
        this->mParts[inIndex].mTime = theTime;
        if (theFirstPart) {
            bool theChanged;
            if (theStart || theFrame != theOldFrame)
                theChanged = true;
            else
                theChanged = false;
            this->SetSound(PlayAnimationSound(theLocation, this->mParts[0].mGraphicObject, 0, this->mSound, theFrame, theChanged));
            if (this->GetSound())
                this->GetSound()->Dereference();
        }
    }
}

// FUNCTION: EXE 0x4e0368
// Replaces the explosion's sound, releasing the old one and referencing the new one
void EEE2DExplosion::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
