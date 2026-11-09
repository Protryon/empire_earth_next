// C:\EEX\Empire Earth\Graphics Thread\EEE3DEnergyWave.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DEnergyWave-Data"). 0xee1b4..0xee5c0: the key chain, both ctors .. Virtual4C.
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
#include "ee/EEE3DEnergyWave.h"
#include "ee/EEEffectKindIDs.h"

// 0x4bd8c8 / 0x4bd8d8 (.data, before the key's string): the corners' directions (provisional names)
static float sEnergyWaveCornerX[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
static float sEnergyWaveCornerY[4] = {1.0f, 1.0f, -1.0f, -1.0f};

// SYNTHETIC: EXE 0x4ee1b4
// SYNTHETIC: EXE 0x4ee1be
// SYNTHETIC: EXE 0x4ee1cf
// SYNTHETIC: EXE 0x4ee1db
const UString EEE3DEnergyWave::sData("EEE3DEnergyWave-Data");

// FUNCTION: EXE 0x4ee1f7
// Creates an energy wave on a unit, at age and phase 0
EEE3DEnergyWave::EEE3DEnergyWave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DEnergyWave), mGraphicObject(NULL)
{
    this->mAge = 0.0f;
    this->mPhase = 0.0f;
}

// SYNTHETIC: EXE 0x4ee233
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ee24f
// Creates an empty energy wave for loading from a saved game
EEE3DEnergyWave::EEE3DEnergyWave()
    : EEEffect3D(kEffect3DEnergyWave)
{
}

// FUNCTION: EXE 0x4ee263
// Destroys the wave, unloading its graphic's media
EEE3DEnergyWave::~EEE3DEnergyWave()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DEnergyWave::DrawPass2()
{
    return 0;
}

long EEE3DEnergyWave::DrawPass4()
{
    return 0;
}

long EEE3DEnergyWave::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4ee287
// Queues the wave in render queue 4, snapshotting its phase and each corner's height (0 over a space tile)
long EEE3DEnergyWave::Queue(EEEffectQueue* inQueues)
{
    long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawPhase = this->mPhase;
        GE3DPoint thePosition(this->mTransformation.GetX(), this->mTransformation.GetY(), this->mTransformation.GetZ());
        float* theHeight = this->mHeights;
        EEGraphicEffect* theEffect = this->mGraphicEffect;
        for (i = 0; i < 4; i++) {
            float theDistance = theEffect->GetSize() * this->mDrawPhase;
            unsigned long theTileX = UMFloorUnsigned(theDistance * sEnergyWaveCornerX[i] + thePosition.GetX());
            unsigned long theTileY = UMFloorUnsigned(theDistance * sEnergyWaveCornerY[i] + thePosition.GetY());
            *theHeight++ = gWorld.mMap->mTerrainMesh.GetIsSpaceTile(theTileX, theTileY) ? 0.0f : theEffect->mParam3;
        }
        GETexture* theTexture;
        theTexture = theEffect->mGraphicObject ? theEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DEnergyWave::PrepareQueue()
{
}

// FUNCTION: EXE 0x4ee3a4
// Moves the wave to its unit, sizes it by the effect and loads its graphic; returns the load result
long EEE3DEnergyWave::SetUp()
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    this->mUnit->mComplexUnit->GetPosition(thePoint);
    theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4ee41b
// Draws the wave around its raised centre, sized by its phase, at its corner heights (not if a cloaked enemy)
// NEAR: gWorld.mPlayers[] (gWorld+0x74) is compared as an absolute address (verifier field map?)
long EEE3DEnergyWave::DrawPass6()
{
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
    if (theCloaked) {
        EEPlayer* thePlayer = theUnit->mPlayer;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        if (thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0)
            return 0;
    }
    GE3DPoint theCenter(this->mTransformation.GetX(), this->mTransformation.GetY(), this->mTransformation.GetZ());
    theCenter.SetZ(this->mGraphicEffect->GetParam5() + theCenter.GetZ());
    float theReference;
    if (this->mGraphicEffect->mGraphicObject)
        theReference = this->mGraphicEffect->mGraphicObject->mAlphaReference;
    else
        theReference = 0.0f;
    GEColor theColor;
    AddGroundQuad(theCenter, theColor, 0.0f, this->mGraphicEffect->mSize * this->mDrawPhase * 2.0f, this->mHeights,
             this->mGraphicEffect->GetParam4(), theReference);
    return 0;
}

// FUNCTION: EXE 0x4ee4dd
// Reloads the graphic and reads age and phase; returns only the phase read's result
long EEE3DEnergyWave::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    inFile.Read(this->mAge);
    return inFile.Read(this->mPhase);
}

// RepairPointers (relinks the effect's pointers): ICF-folded into EEE3DBlastWave::RepairPointers 0xe93e0
long EEE3DEnergyWave::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4ee51f
// Writes the base 3D effect, age and phase; returns only the phase write's result
long EEE3DEnergyWave::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    outFile.Write(this->mAge);
    return outFile.Write(this->mPhase);
}

// FUNCTION: EXE 0x4ee550
// Cycles the wave's phase over the effect's first parameter and ages it; returns 7 (remove) once its duration is up
long EEE3DEnergyWave::Step()
{
    float theElapsed = gPhysics->GetTickSeconds() * 1000.0f;
    this->mPhase += theElapsed / this->mGraphicEffect->GetParam1();
    while (this->mPhase >= 1.0f)
        this->mPhase -= 1.0f;
    this->mAge += theElapsed / this->mGraphicEffect->mDuration;
    if (this->mAge >= 1.0f)
        return 7;
    return 0;
}
