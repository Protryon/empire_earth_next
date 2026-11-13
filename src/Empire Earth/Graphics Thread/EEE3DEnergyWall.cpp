// C:\EEX\Empire Earth\Graphics Thread\EEE3DEnergyWall.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DEnergyWall-Data"). 0xed606..0xee1b4: the key chain, both ctors .. the segment search; EEE3DEnergyWave.cpp
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
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEE3DEnergyWall.h"

// 0x4bd7fc .. 0x4bd868 (.data): the segment ends' offsets and the tiles searched: ee_gfx effect_wall.rs

// SYNTHETIC: EXE 0x4ed606
// SYNTHETIC: EXE 0x4ed610
// SYNTHETIC: EXE 0x4ed621
// SYNTHETIC: EXE 0x4ed62d
const UString EEE3DEnergyWall::sData("EEE3DEnergyWall-Data");

// FUNCTION: EXE 0x4ed649
// Creates the energy wall effect on a wall post or gate and finds its connected segments
EEE3DEnergyWall::EEE3DEnergyWall(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    this->mGraphicObject = NULL;
    this->Place();
    this->FindSegments();
}

// SYNTHETIC: EXE 0x4ed6cf
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ed6eb
// Creates an empty energy wall of a kind for loading from a saved game
EEE3DEnergyWall::EEE3DEnergyWall(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4ed745
// Destroys the energy wall, unloading its graphic's media
EEE3DEnergyWall::~EEE3DEnergyWall()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DEnergyWall::DrawPass2()
{
    return 0;
}

long EEE3DEnergyWall::DrawPass4()
{
    return 0;
}

long EEE3DEnergyWall::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4ed784
// Places the wall's posts (a gate's two end parts, else the unit) raised by the effect's height, and the four
// segment ends around each post (ee_gfx effect_wall.rs); clears the segments and marks the wall placed
void EEE3DEnergyWall::Place()
{
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    GE3DPoint thePosition;
    theUnit->GetPosition(thePosition);
    this->mGate = false;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    if (theUnit->GetClass()->mCategory == kUnitCategoryGate) {
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetClass());
        EEComplexUnit* theFirst = theGate->GetPart(theUnit, 0);
        EEComplexUnit* theSecond = theGate->GetPart(theUnit, 4);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theFirst->GetPosition(theFrom);
        theSecond->GetPosition(theTo);
        this->mPoints[0] = theFrom;
        this->mPoints[9] = theTo;
        this->mPoints[0].SetZ(theEffect->GetParam6() + theFrom.GetZ());
        this->mPoints[9].SetZ(theEffect->GetParam6() + theTo.GetZ());
        this->mGate = true;
    } else {
        this->mPoints[0].SetX(thePosition.GetX());
        this->mPoints[0].SetY(thePosition.GetY());
        this->mPoints[0].SetZ(theEffect->GetParam6() + thePosition.GetZ());
    }
    ee_gfx_wall_place(this->mRecord);
    GETransformation theTransformation(theUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation;
    theUnit->GetPosition(theLocation);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = 2.828f;
}

// FUNCTION: EXE 0x4ed98d
// Returns whether the current player sees the tile or holds a doppelganger of the unit there (then marks the wall seen)
bool EEE3DEnergyWall::IsTileVisible(EEMapTile* inTile)
{
    bool theResult = inTile->IsSeenBy(gWorld.GetCurrentPlayer());
    if (theResult)
        return theResult;
    long thePlayer = gWorld.mCurrentPlayer;
    EEUnit* theUnit = this->mUnit;
    theResult = gDoppelgangerManager.HasDoppelganger(inTile, theUnit, thePlayer);
    if (theResult)
        this->mSeen = true;
    return theResult;
}

// FUNCTION: EXE 0x4ed9d0
// Queues a placed wall in render queue 4: sets its alpha (a gate's by how open it is; 0 when cloaked from an enemy),
// the owner's colour and an animated texture offset
long EEE3DEnergyWall::Queue(EEEffectQueue* inQueues)
{
    if (!this->mPlaced)
        return 0;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        if (gFreeCamera)
            this->FindSegments();
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        EEGraphicEffect* theEffect = this->mGraphicEffect;
        if (theUnit->GetClass()->mCategory == kUnitCategoryGate)
            this->mGateAlpha = (1.0f - theUnit->GetOpenFraction()) * (theEffect->GetParam3() - theEffect->GetParam2()) + theEffect->GetParam2();
        this->mAlpha = theEffect->GetParam3();
        bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
        if (theCloaked) {
            EEPlayer* thePlayer = theUnit->mPlayer;
            EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
            if (thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0) {
                this->mAlpha = 0.0f;
                this->mGateAlpha = 0.0f;
            }
        }
        this->mColor = theUnit->mPlayer->mColor;
        long theFrame;
        if (gFreeCamera)
            theFrame = GetEffectRandomLong(0, 0x20);
        else if (this->mSeen) {
            theFrame = 0;
            this->mSeen = false;
        } else
            theFrame = gPhysics->mGameTime;
        this->mTextureOffset.SetU((theFrame & 7) * 0.14f);
        this->mTextureOffset.SetV((theFrame >> 2 & 7) * 0.14f);
        GETexture* theTexture;
        theTexture = theEffect->mGraphicObject ? theEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DEnergyWall::PrepareQueue()
{
}

// FUNCTION: EXE 0x4edb58
// Loads the wall's graphic; returns the load result
long EEE3DEnergyWall::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4edb6c
// Draws an energy beam from each post to each connected segment end, plus the beam across a gate; returns 0
long EEE3DEnergyWall::DrawPass6()
{
    long i;
    GETextureCoordinate theSize;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    theSize.SetU(theEffect->GetParam4());
    theSize.SetV(theEffect->GetParam5());
    for (i = 1; i < 5; i++) {
        if (this->mSegments & (1 << i))
            AddGroundStrip(this->mPoints[0], this->mPoints[i], theEffect->GetParam1(), this->mColor, this->mTextureOffset, theSize, this->mAlpha, 1.0f,
                     theEffect->mGraphicObject ? theEffect->mGraphicObject->mAlphaReference : 0.0f);
    }
    if (!this->mGate)
        return 0;
    AddGroundStrip(this->mPoints[0], this->mPoints[9], theEffect->GetParam1(), this->mColor, this->mTextureOffset, theSize, this->mGateAlpha, 1.0f,
             theEffect->mGraphicObject ? theEffect->mGraphicObject->mAlphaReference : 0.0f);
    for (i = 5; i < 9; i++) {
        if (this->mSegments & (1 << i))
            AddGroundStrip(this->mPoints[9], this->mPoints[i], theEffect->GetParam1(), this->mColor, this->mTextureOffset, theSize, this->mAlpha, 1.0f,
                     theEffect->mGraphicObject ? theEffect->mGraphicObject->mAlphaReference : 0.0f);
    }
    return 0;
}

// FUNCTION: EXE 0x4edce0
// Reads the wall (versions before 0.250 through LoadOldVersion): gate flag, ten points, segments, timer, placed flag
// (ee_gfx effect_wall.rs)
long EEE3DEnergyWall::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    if (inMajorVersion == 0 && inMinorVersion < 0xfa)
        return this->LoadOldVersion(inMajorVersion, inMinorVersion, inFile);
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_wall_load(this->mRecord, &theFile, (int32_t)theResult, false);
}

// FUNCTION: EXE 0x4edd9c
// Reads and discards an old-format wall's points, keeps its gate flag, segments and timer, and leaves it to be
// placed again (ee_gfx effect_wall.rs)
long EEE3DEnergyWall::LoadOldVersion(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_wall_load(this->mRecord, &theFile, (int32_t)theResult, true);
}

// FUNCTION: EXE 0x4ede8b
// Writes the base 3D effect, gate flag, ten points, segments, timer and placed flag (ee_gfx effect_wall.rs); returns
// the first error
long EEE3DEnergyWall::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_wall_save(this->mRecord, &theFile);
}

// RepairPointers (relinks the effect's pointers): ICF-folded into EEE3DBlastWave's 0xe93e0
long EEE3DEnergyWall::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4edf19
// Places the wall if needed and re-finds its segments every 500 ms (ee_gfx effect_wall.rs); always returns 0
long EEE3DEnergyWall::Step()
{
    if (!this->mPlaced)
        this->Place();
    if (ee_gfx_wall_step(this->mRecord, gPhysics->mTickTime))
        this->FindSegments();
    return 0;
}

// FUNCTION: EXE 0x4ee00c
// The height of the last living, finished wall piece or gate side part of the wall's owner on a tile (the segment
// search's world; provisional name)
static bool EEWallPiece(void* inContext, int32_t inX, int32_t inY, float* outZ)
{
    EEE3DEnergyWall* theWall = static_cast<EEE3DEnergyWall*>(inContext);
    unsigned long theOwner = theWall->mUnit->mComplexUnit->mPlayer->mPlayerID;
    bool theFound = false;
    std::vector<EEUnit*> theUnits;
    EEGetUnitsOnTile(inX, inY, theUnits);
    for (unsigned long j = 0; j < theUnits.size(); j++) {
        EEComplexUnit* theOther = dynamic_cast<EEComplexUnit*>(theUnits[j]);
        if (!theOther || theOther->mPlayer->mPlayerID != theOwner || !(theOther->mHitPoints > 0) ||
            theOther->IsFoundation())
            continue;
        long theType = theOther->GetClass()->mUnitType;
        if (theType == kUnitTypeWallSpace
            || (theType == kUnitTypeGateSpace && static_cast<EEUCGate*>(theOther->GetClass())->IsSidePart(theOther))) {
            GE3DPoint thePosition;
            theOther->GetPosition(thePosition);
            *outZ = thePosition.GetZ();
            theFound = true;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x4edf5a
// Re-finds the segments around the first post and, for a gate, the second post, each one reaching a friendly wall
// piece (ee_gfx effect_wall.rs)
void EEE3DEnergyWall::FindSegments()
{
    EeGfxWallHost theHost = {this, EEWallPiece};
    ee_gfx_wall_find_segments(this->mRecord, this->mGraphicEffect->GetParam6(), &theHost);
}
