// C:\EEX\Empire Earth\Graphics Thread\EEE3DCommandConfirmation.cpp (no __FILE__, no key chain; name from the RTTI
// class). 0xe9bee..0xea703: six colour chains, the ctors .. Virtual4C; EEE3DDamageSmoke.cpp follows.
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEE3DCommandConfirmation.h"
#include "ee/EEAreaUtilities.h"

// SYNTHETIC: EXE 0x4e9bee
// SYNTHETIC: EXE 0x4e9bf8
// SYNTHETIC: EXE 0x4e9c13
// SYNTHETIC: EXE 0x4e9c1f
static GEColor sConfirmGreen(0.37f, 1.0f, 0.37f);        // 0x4e8ae0
// SYNTHETIC: EXE 0x4e9c20
// SYNTHETIC: EXE 0x4e9c2a
// SYNTHETIC: EXE 0x4e9c49
// SYNTHETIC: EXE 0x4e9c55
static GEColor sConfirmOrange(0.94f, 0.61f, 0.26f);      // 0x4e8ad0
// SYNTHETIC: EXE 0x4e9c56
// SYNTHETIC: EXE 0x4e9c60
// SYNTHETIC: EXE 0x4e9c7b
// SYNTHETIC: EXE 0x4e9c87
static GEColor sConfirmRed(1.0f, 0.15f, 0.15f);          // 0x4e8ac0
// SYNTHETIC: EXE 0x4e9c88
// SYNTHETIC: EXE 0x4e9c92
// SYNTHETIC: EXE 0x4e9cad
// SYNTHETIC: EXE 0x4e9cb9
static GEColor sConfirmYellow(1.0f, 1.0f, 0.1f);         // 0x4e8ab0
// SYNTHETIC: EXE 0x4e9cba
// SYNTHETIC: EXE 0x4e9cc4
// SYNTHETIC: EXE 0x4e9cdf
// SYNTHETIC: EXE 0x4e9ceb
static GEColor sConfirmBlue(0.05f, 0.05f, 1.0f);         // 0x4e8aa0 (not used)
// SYNTHETIC: EXE 0x4e9cec
// SYNTHETIC: EXE 0x4e9cf6
// SYNTHETIC: EXE 0x4e9d11
// SYNTHETIC: EXE 0x4e9d1d
static GEColor sConfirmPink(1.0f, 0.5f, 1.0f);           // 0x4e8a90

// FUNCTION: EXE 0x4e9d1e
// Creates a command confirmation marker on a unit for one of its goal queues, in state 2 with zero sizes
EEE3DCommandConfirmation::EEE3DCommandConfirmation(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect,
                                                   unsigned long inGoalIndex, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->mGoalIndex = inGoalIndex;
    this->mAge = 0.0f;
    this->mState = 2;
    this->mStartSize = 0.0f;
    this->mEndSize = 0.0f;
}

// SYNTHETIC: EXE 0x4e9d75
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e9d91
// Creates a command confirmation marker at a ground point for a unit's goal queue, in state 2 with zero sizes
EEE3DCommandConfirmation::EEE3DCommandConfirmation(EEUnit* inUnit, const GE3DPoint& inLocation,
                                                   EEGraphicEffect* inGraphicEffect, unsigned long inGoalIndex,
                                                   long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    GETransformation theTransformation;
    theTransformation.SetPosition(inLocation.GetX(), inLocation.GetY(), inLocation.GetZ());
    GE3DPoint thePosition(theTransformation);
    this->SetPosition(thePosition);
    this->mAge = 0.0f;
    this->mGoalIndex = inGoalIndex;
    this->mStartSize = 0.0f;
    this->mState = 2;
    this->mEndSize = 0.0f;
}

// FUNCTION: EXE 0x4e9e20
// Destroys the marker, unloading its graphic's media
EEE3DCommandConfirmation::~EEE3DCommandConfirmation()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

long EEE3DCommandConfirmation::DrawPass2()
{
    return 0;
}

long EEE3DCommandConfirmation::DrawPass5()
{
    return 0;
}

long EEE3DCommandConfirmation::DrawPass6()
{
    return 0;
}

// NEAR: queue/result registers swapped (ebx/edi)
// FUNCTION: EXE 0x4e9e42
// Queues the marker in render queue 2 under its texture, snapshotting its age, frame and state
long EEE3DCommandConfirmation::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawFrame = this->mFrame;
        this->mDrawState = this->mState;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[2][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DCommandConfirmation::PrepareQueue()
{
}

// NEAR: vector<EEUnitVectorItem>::operator[] (0xa4f48) and RTSMap::GetTile (0x2f85) out of line in the original;
// 1.25f is the memory word 0x43bc74 reloaded per use (EECPAttackFormation's kAFDepthRanged: an extern const
// float?); frame slots
// FUNCTION: EXE 0x4e9eb5
// Picks the marker's colour by state (attack: yellow, red on enemies; move: red if unreachable, green forced,
// else orange; pink otherwise) and sizes it to the unit's selection circle; loads the graphic
long EEE3DCommandConfirmation::SetUp()
{
    UString theUnused;
    EETileCoordinate theTile(0, 0);
    EEUGLocation* theGoal;
    this->mStartSize = this->mGraphicEffect->GetStartSize();
    float theHeight = this->mGraphicEffect->mSize;
    long theState = this->mGraphicEffect->mIntParam1;
    this->mEndSize = this->mGraphicEffect->GetSize();
    this->mGraphicObject = this->mGraphicEffect->mGraphicObject;
    this->mGraphicObject->LoadMedia();
    this->mState = theState;
    this->mBoundingRadius = theHeight;
    switch (theState) {
    case 4:
        this->mColor = sConfirmYellow;
        if (this->mUnit) {
            this->mStartSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f * this->mStartSize;
            this->mEndSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f;
            EEPlayer* thePlayer = this->mUnit->mPlayer;
            EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
            if (thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0
                && thePlayer->mPlayerID != 0)
                this->mColor = sConfirmRed;
        }
        break;
    case 3:
        this->mColor = sConfirmGreen;
        if (this->mUnit) {
            this->mStartSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f * this->mStartSize;
            this->mEndSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f;
        }
        break;
    case 2:
        this->mColor = sConfirmYellow;
        if (this->mUnit) {
            this->mStartSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f * this->mStartSize;
            this->mEndSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f;
        }
        break;
    case 1:
        this->mColor = sConfirmRed;
        if (this->mUnit) {
            this->mStartSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f * this->mStartSize;
            this->mEndSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f;
        }
        break;
    case 0:
        if (this->mUnit && this->mUnit->mComplexUnit) {
            theGoal = this->mUnit->mComplexUnit->mGoalQueues[this->mGoalIndex].mGoals.size() == 0 ? NULL : dynamic_cast<EEUGLocation*>(this->mUnit->mComplexUnit->mGoalQueues[this->mGoalIndex].mGoals.back());
            if (this->mGoalIndex == 0) {
                this->mStartSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f * this->mStartSize;
                this->mEndSize = this->mUnit->mUnitClass->mSelectionSize * 1.25f;
            }
            if (theGoal) {
                theTile.mY = UMFloor(this->mPosition.GetY());
                theTile.mX = UMFloor(this->mPosition.GetX());
                EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
                if (EECanStandOn(theTile, this->mUnit->mComplexUnit) == false && theMapTile
                    && theMapTile->IsVisibleTo(this->mUnit->mPlayer))
                    this->mColor = sConfirmRed;
                else if (theGoal->mIsForcedMove)
                    this->mColor = sConfirmGreen;
                else
                    this->mColor = sConfirmOrange;
            } else {
                EEUnitGoal* theLoadGoal = this->mUnit->mComplexUnit->mActiveGoal;
                if (theLoadGoal && theLoadGoal->mKind == kGoalUnloadTransport)
                    this->mColor = sConfirmYellow;
                else
                    this->mColor = sConfirmPink;
            }
        } else {
            this->mColor = sConfirmPink;
        }
        break;
    }
    this->mTime = 0.0f;
    this->mFrame = this->mGraphicObject->mStartFrames[0];
    this->mDrawFrame = 0;
    return 0;
}

// FUNCTION: EXE 0x4ea1fc
// Does nothing and returns 0 (markers are not saved)
long EEE3DCommandConfirmation::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return 0;
}

long EEE3DCommandConfirmation::RepairPointers()
{
    return 0;
}

// Save (does nothing): ICF-folded into Load 0xea1fc
long EEE3DCommandConfirmation::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return 0;
}

// FUNCTION: EXE 0x4ea201
// Draws the marker facing the camera, swelling and fading on a half sine over its life, without fog, with the
// texture for its state; restores the fog and z state and returns the draw result
long EEE3DCommandConfirmation::DrawPass4()
{
    unsigned long theScreen = gScreenIndex;
    long theResult = 0;
    GEModel* theModel = this->mModel;
    GETransformation theTransformation(false);
    float theValue = UMCos(1.5707964f - this->mDrawAge / this->mGraphicEffect->mDuration * 3.1415927f);
    if (theValue < 0.0f)
        theValue = 0.0f;
    else if (theValue > 1.0f)
        theValue = 1.0f;
    float theSize = (this->mEndSize - this->mStartSize) * theValue + this->mStartSize;
    if (theSize > 0.0f) {
        if (this->mGraphicEffect->mGraphicObject)
            gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
        else
            gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
        unsigned long theFog = gRasterizer->GetFog(theScreen);
        unsigned long theZBuffer = gRasterizer->GetZBuffer(theScreen);
        unsigned long theZCompare = gRasterizer->GetZCompare(theScreen);
        gRasterizer->SetFog(0, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetZCompare(kCompareLess, theScreen);
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        gRasterizer->SetShadingColorSource(0, theScreen);
        SetWhiteLighting();
        theTransformation.SetPosition(this->mPosition.GetX(), this->mPosition.GetY(), this->mPosition.GetZ());
        theTransformation.SetOrientationYPR(gWorld.GetCameraYaw() - 0.7853982f, 0.0f, 0.0f);
        theTransformation.SetElement(3, 0, 0.0f);
        theTransformation.SetElement(3, 1, 0.0f);
        theTransformation.SetElement(3, 2, 0.0f);
        theTransformation.SetElement(3, 3, 1.0f);
        theModel->GetMaterial(0)->SetColor(this->mColor);
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->SetGraphicsScale(theSize);
        theModel->SetOpacity(theValue);
        theModel->PrepareTransforms();
        if (this->mDrawState == 0) {
            theModel->GetMaterial(0)->SetTexture(sTexture4E4, theScreen);
        } else if (this->mDrawState == 3) {
            sTexture4E8->SetFrame(this->mDrawFrame);
            theModel->GetMaterial(0)->SetTexture(sTexture4E8, theScreen);
        } else {
            theModel->GetMaterial(0)->SetTexture(sTexture4E0, theScreen);
        }
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
        gRasterizer->SetFog(theFog, theScreen);
        gRasterizer->SetZBuffer(theZBuffer, theScreen);
        gRasterizer->SetZCompare(theZCompare, theScreen);
    }
    return theResult;
}

// NEAR: frame slots, the per-unit offset's register choice
// FUNCTION: EXE 0x4ea47e
// Ages the marker, keeps a unit marker on its unit (trees: an id-based spot; units with no hit points: the model's
// top point), advances its animation frames; returns 7 (remove) when expired or a non-looping animation ends
long EEE3DCommandConfirmation::Step()
{
    GE3DPoint theOffset;
    GE3DPoint theLocation;
    long theResult;
    unsigned long theFrame = this->mFrame;
    unsigned long theCurrentFrame = this->mFrame;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[0];
    unsigned long theLastFrame = this->mGraphicObject->mLastFrames[0];
    unsigned long theStartFrame = this->mGraphicObject->mStartFrames[0];
    this->mAge += gPhysics->mSpeed * 1000.0f;
    float theTime = gPhysics->GetTickTime() + this->GetTime();
    if (this->mAge <= (float)this->mGraphicEffect->mDuration) {
        theResult = 0;
        if (this->mState != 0 && this->mUnit) {
            this->mUnit->GetPosition(theLocation);
            if (this->mUnit->mUnitClass->mIsTree) {
                long theID = this->mUnit->mUnitID;
                long theSize = this->mUnit->mUnitClass->mTileSize;
                theLocation.SetX((theID % 3 - 1) * theSize * 0.125f + theSize * 0.5f + this->mUnit->mTileX);
                theLocation.SetY((theID / 3 % 3 - 1) * theSize * 0.125f + this->mUnit->mUnitClass->mTileSize * 0.5f
                                 + this->mUnit->mTileY);
                theLocation.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
            } else if (this->mUnit->mHitPoints <= 0 && this->mUnit->mComplexUnit && this->mUnit->GetModel()->mModel) {
                theOffset = this->mUnit->GetModel()->mTopPoint;
                float theScale = this->mUnit->mComplexUnit->mPhysicalModel.GetScale();
                theOffset.SetX(theOffset.GetX() * theScale);
                theOffset.SetY(theOffset.GetY() * theScale);
                this->mUnit->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, theLocation);
            }
            GETransformation theTransformation(false);
            theTransformation.MakeIdentity();
            theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
            this->SetPosition(GE3DPoint(theTransformation));
        }
        if (theTime >= (float)theFrameTime) {
            long theFrames = UMFloatToLong(theTime / (float)theFrameTime);
            theTime -= (float)(theFrames * theFrameTime);
            theFrame = theFrames + theCurrentFrame;
            if (theFrame > theLastFrame) {
                if (this->mGraphicEffect->mFlag1)
                    theFrame = theStartFrame;
                else
                    theResult = 7;
            }
        }
        if (theResult == 0) {
            this->mFrame = theFrame;
            this->mTime = theTime;
        }
    } else {
        theResult = 7;
    }
    return theResult;
}
