// The 2D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_2d.rs, effect_confirm.rs): EEE2DRust's
// virtuals call Rust by the record's kind and answer its questions about the world here. Replaces
// EEE3DCommandConfirmation.cpp.
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
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE3DRust.h"
#include "ee/EEE2DRust.h"

namespace {
// the world's answers for one call
struct Ask2D {
    EEE2DRust* mEffect;
    EEEffectQueue* mQueues;
    static EEE2DRust* E(void* inAsk) { return static_cast<Ask2D*>(inAsk)->mEffect; }
    static int32_t BaseQueue(void* inAsk)
    {
        EEE2DRust* theEffect = E(inAsk);
        EEEffectQueue* theQueues = static_cast<Ask2D*>(inAsk)->mQueues;
        return theEffect->mKind == 0x41 ? theEffect->EEEffect::Queue(theQueues) : theEffect->EEEffect2D::Queue(theQueues);
    }
    static void Push(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        GETexture* theTexture = theObject ? theObject->mTextures[gScreenIndex] : NULL;
        static_cast<Ask2D*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    // the graphic effect's own object (not GetGraphicObject: EEE3DCommandConfirmation::SetUp, 0xe9eb5)
    static int32_t Media(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static EeGfxFrames Frames(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxFrames{(uint32_t)theObject->mStartFrames[inIndex], (uint32_t)theObject->mFrameTimes[inIndex],
                           (uint32_t)theObject->mFrameCounts[inIndex], (uint32_t)theObject->mLastFrames[inIndex]};
    }
    static void MoveToPoint(void* inAsk)
    {
        GE3DPoint thePosition(E(inAsk)->mPosition);
        E(inAsk)->MoveToPoint(thePosition);
    }
    static void* Model(void* inAsk) { return E(inAsk)->mModel; }
    static float SelectionSize(void* inAsk) { return E(inAsk)->mUnit->mUnitClass->mSelectionSize; }
    // an attack's target belongs to another player, not player 0, the current player's enemy
    static bool ConfirmEnemy(void* inAsk)
    {
        EEPlayer* thePlayer = E(inAsk)->mUnit->mPlayer;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        return thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0 && thePlayer->mPlayerID != 0;
    }
    // the goal a state-0 confirmation shows: the last goal of the unit's goal queue as a location goal (on the tile
    // at the effect: blocked when the unit cannot stand there and its player sees it; forced; a move), else whether
    // its active goal unloads a transport (EEE3DCommandConfirmation::SetUp)
    static EeGfxConfirmGoal ConfirmGoal(void* inAsk, uint32_t inGoalIndex, int32_t inTileX, int32_t inTileY)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        if (!theUnit || !theUnit->mComplexUnit)
            return EeGfxConfirmGoal::NoUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EEUGRust* theGoal = theComplex->mGoalQueues[inGoalIndex].mGoals.size() == 0
                                ? NULL : EELocationGoalCast(theComplex->mGoalQueues[inGoalIndex].mGoals.back());
        if (theGoal) {
            EETileCoordinate theTile(inTileX, inTileY);
            EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
            if (EECanStandOn(theTile, theComplex) == false && theMapTile && theMapTile->IsVisibleTo(theUnit->mPlayer))
                return EeGfxConfirmGoal::Blocked;
            return theGoal->GetIsForcedMove() ? EeGfxConfirmGoal::Forced : EeGfxConfirmGoal::Move;
        }
        EEUnitGoal* theLoadGoal = theComplex->mActiveGoal;
        return theLoadGoal && theLoadGoal->mKind == kGoalUnloadTransport ? EeGfxConfirmGoal::Unloading
                                                                          : EeGfxConfirmGoal::Other;
    }
    // where the unit stands for its ring: its position; a tree's id, class tile size and tile; a unit without hit
    // points with a complex unit and a model: its model's top point and scale (EEE3DCommandConfirmation::Step,
    // 0xea47e)
    static EeGfxConfirmUnit ConfirmUnit(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EeGfxConfirmUnit theFacts = {};
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theFacts.position = EeGe3DPoint{theLocation.mX, theLocation.mY, theLocation.mZ};
        if (theUnit->mUnitClass->mIsTree) {
            theFacts.tree = true;
            theFacts.id = (int32_t)(long)theUnit->mUnitID;
            theFacts.tile_size = (int32_t)theUnit->mUnitClass->mTileSize;
            theFacts.tile_x = (int32_t)(long)theUnit->mTileX;
            theFacts.tile_y = (int32_t)(long)theUnit->mTileY;
        } else if (theUnit->mHitPoints <= 0 && theUnit->mComplexUnit && theUnit->GetModel()->mModel) {
            theFacts.dead_top = true;
            const GE3DPoint& theTop = theUnit->GetModel()->mTopPoint;
            theFacts.top = EeGe3DPoint{theTop.mX, theTop.mY, theTop.mZ};
            theFacts.scale = theUnit->mComplexUnit->mPhysicalModel.GetScale();
        }
        return theFacts;
    }
    static EeGeTransformation UnitLocation(void* inAsk)
    {
        const GETransformation& theLocation = E(inAsk)->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        return *reinterpret_cast<const EeGeTransformation*>(&theLocation);
    }
    static float TerrainZ(void*, float inX, float inY) { return gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY); }
    static void* CommandTexture(void*, uint32_t inIndex)
    {
        return inIndex == 0 ? sTexture4E0 : inIndex == 1 ? sTexture4E4 : sTexture4E8;
    }
    // a random one of the unit's tag points of inKind (the unit itself without any) (EEE2DShockwave::SetUp, 0xe6259)
    static EeGe3DPoint SpawnPoint(void* inAsk, uint32_t inKind)
    {
        EEE2DRust* theEffect = E(inAsk);
        GE3DPoint thePosition;
        GETransformation theTransformation(false);
        theTransformation = theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), (long)inKind);
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        if (theList && theList->mPoints.size() != 0) {
            long theIndex = UMFloatToLong(EEEffect::GetRandom() * theList->mPoints.size());
            if (theIndex == theList->mPoints.size())
                theIndex = theList->mPoints.size() - 1;
            EETagPointList::Scale(theTransformation, theScale);
            theList->mPoints[theIndex]->GetPoint(theFrame, theTransformation, thePosition);
        } else {
            // no tag points of the kind: at the unit itself, not at the map origin
            thePosition = GE3DPoint(theTransformation);
        }
        return EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
    }
    static int32_t ObjectMedia(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->GetGraphicObject();
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void* ObjectTexture(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mTextures[inIndex]; }
};
}

// the command confirmation (0x41) on a unit (EEE3DCommandConfirmation, 0xe9d1e)
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, unsigned long inGoalIndex, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    this->Call(EeGfxEffect3DOp::Create);
}

// at a point (0xe9d91)
EEE2DRust::EEE2DRust(EEUnit* inUnit, const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect,
                     unsigned long inGoalIndex, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->mPosition = inLocation;
    this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    this->Call(EeGfxEffect3DOp::CreateAt);
}

// the shockwave (0x12) on a unit (EEE2DShockwave, 0xe6192)
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->Call(EeGfxEffect3DOp::Create);
}

// at a point (0xe61e2)
EEE2DRust::EEE2DRust(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->Call(EeGfxEffect3DOp::CreateAt);
    this->SetPosition(inPosition);
}

// for loading (0xe621f)
EEE2DRust::EEE2DRust(long inKind)
    : EEEffect2D(gU4E8DF4, inKind, false)
{
}

// unloads the kept graphic object's media
EEE2DRust::~EEE2DRust()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

long EEE2DRust::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile)
{
    Ask2D theAsk = {this, inQueues};
    EeGfxEffect2DHost theHost = {&theAsk, Ask2D::BaseQueue, Ask2D::Push, Ask2D::Media, Ask2D::Frames, Ask2D::MoveToPoint,
                                 Ask2D::Model, Ask2D::SelectionSize, Ask2D::ConfirmEnemy, Ask2D::ConfirmGoal,
                                 Ask2D::ConfirmUnit, Ask2D::UnitLocation, Ask2D::TerrainZ, Ask2D::CommandTexture,
                                 Ask2D::SpawnPoint, Ask2D::ObjectMedia, Ask2D::ObjectTexture};
    // Load and Save read no facts: during a load the unit is still its saved id (RepairPointers resolves it)
    if (inFile != NULL)
        return ee_gfx_effect2d_file(inOp, this->mRecord, inFile, &theHost);
    EeGfxGraphicFacts theFacts = EEEffectFacts(this->mGraphicEffect, this->mUnit);
    return ee_gfx_effect2d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// the confirmation (0x41): 0 without the base's (it is never saved); the others: EEEffect2D's, then the kind's (the
// base's result ignored, as the shockwave's)
long EEE2DRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Load);
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile);
}

long EEE2DRust::RepairPointers()
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Repair);
    return EEEffect2D::RepairPointers();
}

long EEE2DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Save);
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile);
}
