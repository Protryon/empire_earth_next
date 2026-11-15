// C:\EEX\Empire Earth\Graphics Thread\EEE3DParticleEmitter.cpp (no __FILE__; name from the RTTI class and its key).
// 0xf5d94..0xf69e3: the key chain, four ctors, the dtor, the sound helpers, Move, the virtuals. Unit flags: the exe's
// module set.
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
#include "ee/EEE3DParticleEmitter.h"


// SYNTHETIC: EXE 0x4f5d94
// SYNTHETIC: EXE 0x4f5d9e
// SYNTHETIC: EXE 0x4f5daf
// SYNTHETIC: EXE 0x4f5dbb
const UString EEE3DParticleEmitter::sData("EEE3DParticleEmitter");

// FUNCTION: EXE 0x4f5dd7
// Creates a particle emitter for a graphic effect at a fixed point, with no sound
EEE3DParticleEmitter::EEE3DParticleEmitter(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    GETransformation theTransformation;
    this->mSound = NULL;
    theTransformation.SetPosition(inLocation.GetX(), inLocation.GetY(), inLocation.GetZ());
    this->SetTransformation(theTransformation);
    this->mTagPoint = 0;
    this->mIsFirstStep = true;
    // (the original leaves the times to SetUp: an effect saved before its set-up wrote whatever the memory held)
    this->mTime = 0.0f;
    this->mEmitTime = 0.0f;
}

// SYNTHETIC: EXE 0x4f5e66
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f5e82
// Creates a particle emitter for a graphic effect at the line's start, moving along the line's direction
EEE3DParticleEmitter::EEE3DParticleEmitter(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    GETransformation theTransformation;
    this->mSound = NULL;
    theTransformation.SetPosition(inLine.GetStart().GetX(), inLine.GetStart().GetY(), inLine.GetStart().GetZ());
    this->SetTransformation(theTransformation);
    this->mVelocity = inLine.GetDirection();
    this->mTagPoint = 0;
    this->mIsFirstStep = true;
    this->mTime = 0.0f;   // (as above)
    this->mEmitTime = 0.0f;
}

// FUNCTION: EXE 0x4f5f23
// Creates a particle emitter attached to a unit's tag point: at the unit's model location (unless the effect
// follows the tag point), or at a non-complex unit's tile centre on the ground
EEE3DParticleEmitter::EEE3DParticleEmitter(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind,
                                           unsigned long inTagPoint)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    // (the original's is uninitialized, and a tag point follower placed itself at whatever it held until its first
    // step: here the identity)
    GETransformation theTransformation;
    EEComplexUnit* theUnit = inUnit->GetComplexUnit();
    this->mSound = NULL;
    this->mTagPoint = inTagPoint;
    if (theUnit) {
        if (!this->mGraphicEffect->mFlag1)
            theTransformation = theUnit->mPhysicalModel.GetLocation();
    } else {
        GE3DPoint theLocation;
        float theHalf = inUnit->mUnitClass->mTileSize * 0.5f;
        theLocation.SetX(inUnit->mTileX + theHalf);
        theLocation.SetY(inUnit->mTileY + theHalf);
        if (::gWorld.mMap->mTerrainMesh.GetIsValidTile(inUnit->mTileX, inUnit->mTileY))
            theLocation.SetZ(::gWorld.mMap->GetHeight(theLocation));
        else
            theLocation.SetZ(0.0f);
        theTransformation.MakeIdentity();
        theTransformation.SetPosition(theLocation);
    }
    this->SetTransformation(theTransformation);
    this->mIsFirstStep = true;
    this->mTime = 0.0f;   // (as above)
    this->mEmitTime = 0.0f;
}

// FUNCTION: EXE 0x4f604d
// Creates an empty particle emitter of the given kind (for loading), with no sound
EEE3DParticleEmitter::EEE3DParticleEmitter(long inKind)
    : EEEffect3D(inKind)
{
    this->mSound = NULL;
    this->mTagPoint = 0;
    this->mIsFirstStep = true;
}

// FUNCTION: EXE 0x4f6089
// Destroys the emitter, stopping its sound
EEE3DParticleEmitter::~EEE3DParticleEmitter()
{
    this->StopSound();
}

// FUNCTION: EXE 0x4f60a6
// Stops the emitter's sound, if any, and releases it
void EEE3DParticleEmitter::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4f60cb
// Drops the emitter's reference to its sound
void EEE3DParticleEmitter::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// FUNCTION: EXE 0x4f60f5
// Applies drag, gravity, wind and rise to the emitter's velocity and returns its next position; bounces off
// the terrain when enabled and returns 7 if that position leaves the map
long EEE3DParticleEmitter::Move(float inSeconds, GE3DPoint& outPosition)
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    const GEVector& theWind = gWorld.mWindDirection;
    EeGfxEmitterMove theMove = {{theWind.GetX(), theWind.GetY(), theWind.GetZ()}, theEffect->GetParam4(), theEffect->GetParam8(),
                                theEffect->GetParam9(), theEffect->GetParam10(), theEffect->mParam11, theEffect->mFlag2};
    EeGfxParticleGround theGround = EEParticleGround();
    return ee_gfx_effect_emitter_move(this->mRecord, inSeconds, &theMove, &theGround,
                                      reinterpret_cast<EeGe3DPoint*>(&outPosition));
}

// FUNCTION: EXE 0x4f634b
// Resets the emitter's running time and emit countdown
long EEE3DParticleEmitter::SetUp()
{
    this->mTime = 0.0f;
    this->mEmitTime = 0.0f;
    return 0;
}

// FUNCTION: EXE 0x4f635e
// Reads the emitter's velocity, running time, emit countdown and tag point; returns the first read error
long EEE3DParticleEmitter::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_effect_load_emitter(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4f63b9
// Writes the emitter's velocity, running time, emit countdown and tag point (ee_gfx effect_state.rs); returns the
// first write error
long EEE3DParticleEmitter::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_emitter(this->mRecord, &theFile);
}

// The emitter's line particles (CreateLineEffect of mLinkedEffect1) (provisional name)
struct EEEmitterHost {
    static int32_t Line(void* inContext, const EeGe3DPoint* inStart, const EeGe3DPoint* inDirection)
    {
        EEE3DParticleEmitter* theEmitter = static_cast<EEE3DParticleEmitter*>(inContext);
        GE3DLine theLine;
        theLine.SetStart(*reinterpret_cast<const GE3DPoint*>(inStart));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        return (int32_t)gEffectManager->CreateLineEffect(theLine, theEmitter->mGraphicEffect->mLinkedEffect1, NULL);
    }
};

// FUNCTION: EXE 0x4f6417
// Moves the emitter with its unit's tag point (or by its own motion), updates its sound and, when the countdown
// runs out, emits a burst of line particles; returns 7 once the unit dies or the duration ends
long EEE3DParticleEmitter::Step()
{
    GE3DPoint theLocation;
    GETransformation theTransformation(false);
    GETransformation thePart(false);
    long theResult = 0;
    float theDelta = ee_gfx_effect_emitter_begin(this->mRecord, gPhysics->GetTickSeconds());
    float theSeconds = theDelta / 1000.0f;
    if (this->mUnit) {
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        if (theUnit) {
            if (this->mGraphicEffect->mFlag1) {
                theTransformation = theUnit->mPhysicalModel.GetLocation();
                EEGraphicObject* theGraphic = this->mUnit->GetModel();
                EETagPointList* theTags = theGraphic->GetTagPoints(theUnit->GetAnimation(), this->mGraphicEffect->mTagPointKind);
                if (this->mGraphicEffect->mTagPointKind != 0x15 && theTags) {
                    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
                    theTags->Scale(theTransformation, theUnit->mPhysicalModel.GetScale());
                    if (theGraphic->GetNumParts() > 0) {
                        unsigned long thePartIndex;
                        thePart.MakeIdentity();
                        theResult = theGraphic->FindTurretPart(theTags->GetTagPoint(this->mTagPoint)->mModel, thePartIndex);
                        if (theResult == 0)
                            thePart.SetOrientationYPR(theUnit->mTurretAngles[thePartIndex], 0.0f, 0.0f);
                        theTags->GetTagPoint(this->mTagPoint)->GetPoint(theFrame, theTransformation, thePart, theLocation);
                    } else
                        theTags->GetTagPoint(this->mTagPoint)->GetPoint(theFrame, theTransformation, theLocation);
                } else
                    theTransformation.GetPosition(theLocation);
            } else {
                theResult = this->Move(theSeconds, theLocation);
                theTransformation = this->mTransformation;
            }
            if (this->mGraphicEffect->mFlag2) {
                if (theUnit->GetCommandType() == kActionDead)
                    theResult = 7;
            } else if (this->mGraphicEffect->mEndsWithUnitOrDuration) {
                if (theUnit->GetHitPoints() <= 0 || this->mGraphicEffect->mDuration < this->mTime)
                    theResult = 7;
            } else if (this->mGraphicEffect->mDuration < this->mTime)
                theResult = 7;
        } else {
            theTransformation.MakeIdentity();
            if (this->mGraphicEffect->mFlag1)
                this->mTransformation.GetPosition(theLocation);
            else
                theResult = this->Move(theSeconds, theLocation);
            if (this->mGraphicEffect->mFlag2) {
                if (this->mUnit->GetHitPoints() <= 0)
                    theResult = 7;
            } else if (this->mGraphicEffect->mEndsWithUnitOrDuration) {
                if (this->mUnit->GetHitPoints() <= 0 || this->mGraphicEffect->mDuration < this->mTime)
                    theResult = 7;
            } else if (this->mGraphicEffect->mDuration < this->mTime)
                theResult = 7;
        }
    } else {
        theTransformation.MakeIdentity();
        theResult = this->Move(theSeconds, theLocation);
        if (this->mGraphicEffect->mDuration < this->mTime)
            theResult = 7;
    }
    if (theResult != 7) {
        theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        this->SetTransformation(theTransformation);
        if (this->mGraphicEffect->mGraphicObject) {
            this->SetSound(PlayAnimationSound(theLocation, this->mGraphicEffect->mGraphicObject, 0, this->mSound, 0, this->mIsFirstStep));
            if (this->mSound)
                this->mSound->Dereference();
        }
        EEGraphicEffect* theEffect = this->mGraphicEffect;
        EeGfxEmitterBurst theBurst = {(uint32_t)theEffect->mIntParam3, theEffect->mEmitPositionSpread, theEffect->mIsEmitFlat,
                                      theEffect->GetParam1(), theEffect->GetParam2(), theEffect->GetParam3(),
                                      theEffect->GetParam4(), theEffect->GetParam5(), theEffect->GetParam6(),
                                      (int32_t)theEffect->mIntParam1, (int32_t)theEffect->mIntParam2};
        EeGfxEmitterHost theHost = {this, EEEmitterHost::Line};
        theResult = ee_gfx_effect_emitter_burst(this->mRecord, theDelta, reinterpret_cast<const EeGe3DPoint*>(&theLocation),
                                                reinterpret_cast<const EeGeTransformation*>(&theTransformation), &theBurst,
                                                (int32_t)theResult, &theHost);
    }
    this->mIsFirstStep = false;
    return theResult;
}

long EEE3DParticleEmitter::DrawPass2()
{
    return 0;
}

long EEE3DParticleEmitter::DrawPass4()
{
    return 0;
}

long EEE3DParticleEmitter::DrawPass5()
{
    return 0;
}

long EEE3DParticleEmitter::DrawPass6()
{
    return 0;
}

// ICF-folded into EEEDamagedEffect's 0x1020dc
long EEE3DParticleEmitter::Queue(EEEffectQueue* inQueues)
{
    return 10;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DParticleEmitter::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f69bc
// Replaces the emitter's sound, referencing the new one and releasing the old
void EEE3DParticleEmitter::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (this->mSound)
        this->mSound->Reference();
}
