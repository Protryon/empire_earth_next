// The 3D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs, effect_follow.rs, effect_shot.rs,
// effect_beam.rs, effect_aged.rs, effect_additive.rs,
// effect_orbit.rs, effect_hurricane.rs, effect_nuclear.rs, effect_multi.rs, effect_wall.rs, effect_vent.rs): EEE3DRust's virtuals call Rust by the record's kind and answer its questions about the world here.
// Replaces EEE3DTeleport.cpp, EEE3DTimeWarp.cpp, EEE3DBlastWave.cpp, EEE3DShield.cpp, EEE3DPulseCannonBall.cpp,
// EEE3DTracers.cpp, EEE3DShells.cpp, EEE3DCitizenBeam.cpp, EEE3DWaterSplash.cpp, EEE3DScreenFlash.cpp,
// EEE3DAdditivePass.cpp, EEE3DLinearProjectile.cpp, EEE3DDisease.cpp, EEE3DVirus.cpp, EEE3DHurricane.cpp,
// EEE3DNuclearExplosion.cpp, EEE3DEnergyWave.cpp, EEE3DMultiEffect.cpp, EEE3DEnergyWall.cpp and
// EEE3DDamageVent.cpp.
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
#include "ee/EEE3DRust.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEUnitUtilities.h"

namespace {
// the world's answers for one call
struct Ask {
    EEE3DRust* mEffect;
    EEEffectQueue* mQueues;
    EETagPointList* mSpawn;   // the hurricane's spawn list, read once by SpawnCount for SpawnAt
    EeGfxEffect3DOp mOp;
    void* mArg;               // a constructor's unit (Create), point (CreateAt) or line (CreateLine)
    EETagPointList* mVentFrom;   // the damage vent's tag point lists, read once a step by VentLists
    EETagPointList* mVentTo;
    static EEE3DRust* E(void* inAsk) { return static_cast<Ask*>(inAsk)->mEffect; }
    static EeGe3DPoint UnitPosition(void* inAsk)
    {
        GE3DPoint thePoint;
        E(inAsk)->mUnit->GetPosition(thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static float ModelHeight(void* inAsk)
    {
        return E(inAsk)->mGraphicEffect->mGraphicObject->mModels[0]->GetBoundingBox(0).GetHeight();
    }
    static void MoveTo(void* inAsk)
    {
        GETransformation theTransformation(E(inAsk)->mTransformation);
        E(inAsk)->SetTransformation(theTransformation);
    }
    static void Line(void* inAsk, const EeGe3DPoint* inStart, const EeGeVector* inDirection, uint32_t inLinked)
    {
        GE3DLine theLine;
        theLine.SetStart(GE3DPoint(inStart->x, inStart->y, inStart->z));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        EEGraphicEffect* theEffect = E(inAsk)->mGraphicEffect;
        gEffectManager->CreateLineEffect(theLine, inLinked == 1 ? theEffect->mLinkedEffect1 : theEffect->mLinkedEffect2, NULL);
    }
    static int32_t BaseQueue(void* inAsk) { return E(inAsk)->EEEffect3D::Queue(static_cast<Ask*>(inAsk)->mQueues); }
    static void Push(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        GETexture* theTexture = theObject ? theObject->mTextures[gScreenIndex] : NULL;
        static_cast<Ask*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    static int32_t Media(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->GetGraphicObject();
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void* Model(void* inAsk) { return E(inAsk)->mGraphicObject->mModel; }
    static void* Texture(void* inAsk) { return E(inAsk)->mGraphicObject->mTextures[0]; }
    // to the unit's first tag point of type 0x12 (keeping the unit's orientation), else to the unit (EEE3DBlastWave's
    // and EEE3DShield's FollowUnit, 0xe904a)
    static void Follow(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        GE3DPoint thePoint;
        GETransformation theLocation(theUnit->mPhysicalModel.GetLocation());
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0x12);
        if (theList != NULL && !theList->mPoints.empty()) {
            EETagPointList::Scale(theLocation, theScale);
            theList->mPoints.front()->GetPoint(theFrame, theLocation, thePoint);
            GETransformation theTransformation(false);
            theTransformation = theLocation;
            theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
            theEffect->SetTransformation(theTransformation);
        } else {
            theEffect->SetTransformation(theLocation);
        }
    }
    static void* ModelAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mModels[inIndex]; }
    // a tracer's end: its impact effect at its position, then the target's hit (EEE3DTracers::Step, 0xfdad4)
    static void ShotHit(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicEffect* theImpact = theEffect->mGraphicEffect->mLinkedEffect1;
        if (theImpact) {
            GE3DPoint theLocation(theEffect->mTransformation);
            gEffectManager->CreatePlacedEffect(theLocation, theImpact, NULL);
        }
        if (theEffect->mUnit)
            theEffect->mUnit->mUnitClass->UnitDamaged(theEffect->mUnit->mComplexUnit, NULL, 0);
    }
    // the citizen beam's start: the unit's first tag point (EEE3DCitizenBeam::Aim, 0xe9606)
    static EeGe3DPoint BeamStart(void* inAsk)
    {
        GE3DPoint theStart;
        EEUnit* theOwner = E(inAsk)->mUnit;
        EEModel* theModel = theOwner->GetModel();
        EEComplexUnit* theUnit = theOwner->mComplexUnit;
        GETransformation theUnitTransformation(theUnit->mPhysicalModel.GetLocation());
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        EETagPointList* theList = theModel->mTagPoints[theUnit->mAnimation];
        EETagPointList::Scale(theUnitTransformation, theScale);
        theList->GetTagPoint(0)->GetPoint(theFrame, theUnitTransformation, theStart);
        return EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    }
    // the beam's unit as a complex unit (EEE3DCitizenBeam::Step, 0xe9aec)
    static EeGfxBeamUnit BeamUnit(void* inAsk)
    {
        EeGfxBeamUnit theFacts = {};
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(E(inAsk)->mUnit);
        if (theUnit) {
            theFacts.complex = true;
            theFacts.animation = (int32_t)theUnit->mAnimation;
            theFacts.frame = (int32_t)(long)theUnit->mPhysicalModel.GetFrame();
        }
        return theFacts;
    }
    static bool BeamHidden(void* inAsk)
    {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(E(inAsk)->mUnit);
        bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
        EEPlayer* thePlayer;
        EEPlayer* theCurrent;
        return theCloaked && (thePlayer = theUnit->mPlayer) != (theCurrent = gWorld.GetCurrentPlayer())
            && theCurrent->mEnemies[thePlayer->mPlayerID] != 0;
    }
    static void PlaceLinked(void* inAsk, const EeGe3DPoint* inPoint)
    {
        EEGraphicEffect* theLinked = E(inAsk)->mGraphicEffect->mLinkedEffect1;
        if (theLinked) {
            GE3DPoint thePoint(inPoint->x, inPoint->y, inPoint->z);
            gEffectManager->CreatePlacedEffect(thePoint, theLinked, NULL);
        }
    }
    // the screen flash's place: its unit's complex unit's position (EEE3DScreenFlash::SetUp, ICF-folded into 0xee3a4)
    static EeGe3DPoint ComplexPosition(void* inAsk)
    {
        GE3DPoint thePoint;
        E(inAsk)->mUnit->mComplexUnit->GetPosition(thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    // the additive pass's unit model (EEE3DAdditivePass::SetUp, 0xe8b78, and RepairPointers, 0xe8dcc)
    static int32_t UnitModelMedia(void* inAsk)
    {
        E(inAsk)->mUnitModel = E(inAsk)->mUnit->GetModel();
        return E(inAsk)->mUnitModel->LoadMedia();
    }
    static float UnitScale(void* inAsk) { return E(inAsk)->mUnit->mStats->GetScale(); }
    static EeGeTransformation UnitLocation(void* inAsk)
    {
        const GETransformation& theLocation = E(inAsk)->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        return *reinterpret_cast<const EeGeTransformation*>(&theLocation);
    }
    static float UnitModelRadius(void* inAsk) { return E(inAsk)->mUnitModel->mModels[0]->GetBoundingRadius(0); }
    static EeGfxUnitFrame UnitFrame(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        return EeGfxUnitFrame{(uint32_t)theUnit->mPhysicalModel.GetFrame(), (uint32_t)theUnit->mAnimation};
    }
    static void* UnitModelAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mUnitModel->mModels[inIndex]; }
    // the graphic object's orbitals (EEE3DLinearProjectile::SetUp, 0xf30ba, copies them)
    static uint32_t OrbitalCount(void* inAsk) { return (uint32_t)E(inAsk)->mGraphicObject->GetOrbitalCount(); }
    static EeGfxOrbital Orbital(void* inAsk, uint32_t inIndex)
    {
        EEOrbital* theOrbital = E(inAsk)->mGraphicObject->GetOrbital(inIndex);
        return EeGfxOrbital{theOrbital->mModel, theOrbital->mSpeed, theOrbital->mAngle};
    }
    // the disease's and virus's animation 13 (EEE3DDisease::Step, 0xec4d7)
    static EeGfxAnimation Animation(void* inAsk)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxAnimation{(uint32_t)theObject->mStartFrames[13], (uint32_t)theObject->mFrameTimes[13],
                              (uint32_t)theObject->mFrameCounts[13]};
    }
    // the frame's sound at the unit, kept as the effect's (EEE3DDisease::Step and SetSound, 0xec6bd)
    static void AnimationSound(void* inAsk, uint32_t inFrame, bool inChanged)
    {
        EEE3DRust* theEffect = E(inAsk);
        GE3DPoint theLocation;
        theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        SESMInstanceNode* theSound =
            PlayAnimationSound(theLocation, theEffect->mGraphicObject, 0, theEffect->mSound, inFrame, inChanged);
        if (theSound != theEffect->mSound) {
            if (theEffect->mSound)
                theEffect->mSound->Dereference();
            theEffect->mSound = theSound;
            if (theSound)
                theSound->Reference();
        }
        if (theEffect->mSound)
            theEffect->mSound->Dereference();
    }
    // the hurricane's spawn points: its graphic's tag points of kind 6 (animation 0) (EEE3DHurricane::GetSpawnPoint,
    // 0xf188a); none without a list
    static uint32_t SpawnCount(void* inAsk)
    {
        EETagPointList* theList = E(inAsk)->mGraphicEffect->mGraphicObject->GetTagPoints(0, 6);
        static_cast<Ask*>(inAsk)->mSpawn = theList;
        return theList == NULL ? 0 : (uint32_t)theList->mPoints.size();
    }
    // tag point inIndex at inFrame around the unit, scaled by the graphic effect's size (after SpawnCount)
    static EeGe3DPoint SpawnAt(void* inAsk, uint32_t inIndex, uint32_t inFrame)
    {
        EEE3DRust* theEffect = E(inAsk);
        GETransformation theLocation(theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
        EETagPointList* theList = static_cast<Ask*>(inAsk)->mSpawn;
        GE3DPoint thePoint;
        EETagPointList::Scale(theLocation, theEffect->mGraphicEffect->mSize);
        theList->mPoints[inIndex]->GetPoint((unsigned long)inFrame, theLocation, thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    // the linked effect 1 placed at inPoint; the placement's result
    static int32_t Spawn(void* inAsk, const EeGe3DPoint* inPoint)
    {
        GE3DPoint thePoint(inPoint->x, inPoint->y, inPoint->z);
        return gEffectManager->CreatePlacedEffect(thePoint, E(inAsk)->mGraphicEffect->mLinkedEffect1, NULL);
    }
    static void* TextureAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mTextures[inIndex]; }
    // the nuclear explosion's place off the map: its tile's (EEE3DNuclearExplosion::SetUp, 0xf4524)
    static EeGfxUnitTile UnitTile(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        return EeGfxUnitTile{theUnit->mTileNode != NULL, (float)theUnit->GetClass()->GetTileSize(),
                             (float)(long)theUnit->mTileX, (float)(long)theUnit->mTileY};
    }
    static float TerrainZ(void* inAsk, float inX, float inY) { return gWorld.mMap->mTerrainMesh.GetMeshZ(inX, inY); }
    // the energy wave's unit is a cloaked enemy's (EEE3DEnergyWave::Queue, 0xee287; the port's R5a.3 test)
    static bool UnitHidden(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
        if (!theCloaked)
            return false;
        EEPlayer* thePlayer = theUnit->mPlayer;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        return thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0;
    }
    static bool SpaceTile(void*, uint32_t inX, uint32_t inY)
    {
        return gWorld.mMap->mTerrainMesh.GetIsSpaceTile((unsigned long)inX, (unsigned long)inY);
    }
    // the multi-effect's linked effect inIndex through its constructor's entry point (EEE3DMultiEffect's three
    // constructors, 0xf4247, 0xf42c3, 0xf4339)
    static int32_t CreateLinked(void* inAsk, uint32_t inIndex)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEGraphicEffect* theEffect = theAsk->mEffect->mGraphicEffect;
        EEGraphicEffect* theLinked = inIndex == 1 ? theEffect->mLinkedEffect1
                                     : inIndex == 2 ? theEffect->mLinkedEffect2 : theEffect->mLinkedEffect3;
        if (theAsk->mOp == EeGfxEffect3DOp::CreateLine)
            return gEffectManager->CreateLineEffect(*static_cast<GE3DLine*>(theAsk->mArg), theLinked, NULL);
        if (theAsk->mOp == EeGfxEffect3DOp::CreateAt)
            return gEffectManager->CreatePlacedEffect(*static_cast<GE3DPoint*>(theAsk->mArg), theLinked, NULL);
        return gEffectManager->CreateUnitEffect(static_cast<EEUnit*>(theAsk->mArg), theLinked, NULL, 0, 0, 0);
    }
    // the energy wall's posts on a gate: its two end parts (EEE3DEnergyWall::Place, 0xed784)
    static bool WallGatePosts(void* inAsk, EeGe3DPoint* outFirst, EeGe3DPoint* outSecond)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        if (theUnit->GetClass()->mCategory != kUnitCategoryGate)
            return false;
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetClass());
        EEComplexUnit* theFirst = theGate->GetPart(theUnit, 0);
        EEComplexUnit* theSecond = theGate->GetPart(theUnit, 4);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theFirst->GetPosition(theFrom);
        theSecond->GetPosition(theTo);
        *outFirst = EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ};
        *outSecond = EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ};
        return true;
    }
    // the height of the last living, finished wall piece or gate side part of the wall's owner on a tile (0xee00c)
    static bool WallPiece(void* inAsk, int32_t inX, int32_t inY, float* outZ)
    {
        unsigned long theOwner = E(inAsk)->mUnit->mComplexUnit->mPlayer->mPlayerID;
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
    // a gate's opening (EEE3DEnergyWall::Queue, 0xed9d0)
    static bool GateOpen(void* inAsk, float* outOpen)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        if (theUnit->GetClass()->mCategory != kUnitCategoryGate)
            return false;
        *outOpen = theUnit->GetOpenFraction();
        return true;
    }
    static EeGfxColor PlayerColor(void* inAsk)
    {
        const GEColor& theColor = E(inAsk)->mUnit->mComplexUnit->mPlayer->mColor;
        return EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
    }
    // the damage vent's vents (kind 5) and aims (0x38) on the unit's model at its animation (EEE3DDamageVent::Step,
    // 0xeae33)
    static void VentLists(void* inAsk)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEUnit* theUnit = theAsk->mEffect->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        theAsk->mVentFrom = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 5);
        theAsk->mVentTo = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0x38);
    }
    static EeGfxVentUnit VentUnit(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        return EeGfxVentUnit{(theComplex->mCommand ? theComplex->mCommand->mType : 0) == kActionDead,
                             (float)theUnit->mStats->mMaxHitPoints};
    }
    static uint32_t VentCount(void* inAsk) { return (uint32_t)static_cast<Ask*>(inAsk)->mVentFrom->GetNumTagPoints(); }
    // a jet's vent's and aim's offsets (EEE3DDamageVentPart::Aim, 0xeb4e9)
    static void VentOffsets(void* inAsk, int32_t inTag, EeGe3DPoint* outPoints)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theAsk->mVentFrom->mPoints[inTag]->GetOffset(0, theFrom);
        theAsk->mVentTo->mPoints[inTag]->GetOffset(0, theTo);
        outPoints[0] = EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ};
        outPoints[1] = EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ};
    }
    // a jet's vent and aim at frame 0 around the unit's location scaled by its stats, and the vent's offset from the
    // model's centre turned by that location (EEE3DDamageVent::Step)
    static EeGfxVentPoints VentPoints(void* inAsk, int32_t inTag)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEComplexUnit* theComplex = theAsk->mEffect->mUnit->mComplexUnit;
        GETransformation theUnitTransformation(theComplex->mPhysicalModel.GetLocation());
        theAsk->mVentFrom->Scale(theUnitTransformation, theComplex->mStats->GetScale());
        GE3DPoint theFrom;
        GE3DPoint theTo;
        (theAsk->mVentFrom->GetTagPoint(inTag))->GetPoint(0, theUnitTransformation, theFrom);
        (theAsk->mVentTo->GetTagPoint(inTag))->GetPoint(0, theUnitTransformation, theTo);
        GEModel* theModel = theComplex->GetModel()->mModels[0];
        GE3DPoint theOffset;
        theAsk->mVentFrom->mPoints[inTag]->GetOffset(0, theOffset);
        GE3DPoint theCenter = theModel->GetCenterPoint();
        GEVector theTranslation;
        theUnitTransformation.Apply(GEVector(theOffset.GetX() - theCenter.GetX(), theOffset.GetY() - theCenter.GetY(),
                                             theOffset.GetZ() - theCenter.GetZ()), theTranslation);
        return EeGfxVentPoints{EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ}, EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ},
                               EeGeVector{theTranslation.mX, theTranslation.mY, theTranslation.mZ}};
    }
    static EeGfxFrames Frames(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxFrames{(uint32_t)theObject->mStartFrames[inIndex], (uint32_t)theObject->mFrameTimes[inIndex],
                           (uint32_t)theObject->mFrameCounts[inIndex], (uint32_t)theObject->mLastFrames[inIndex]};
    }
    static float UnitClassScale(void* inAsk) { return E(inAsk)->mUnit->GetClass()->GetScale(); }
};

// a shot's start: the unit's weapon point (ee_combat's weapon_point; inMode 6 the pulse cannon's, 7 the tracers');
// whether the unit's model has tag points at its animation
bool ShotStart(long inMode, EEUnit* inUnit, EeGe3DPoint& outStart)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->GetAnimation()];
    bool theTagged = theList != NULL && theList->GetNumTagPoints() != 0;
    EECombatEffectWeaponPoint(inMode, theUnit, theModel, theList, theTagged, theStart);
    outStart = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    return theTagged;
}
}

// on a unit (the multi-effect's own unit is none: its linked effects are made on inUnit)
EEE3DRust::EEE3DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inKind == 0x3b ? NULL : inUnit, inGraphicEffect, inKind)
{
    this->Call(EeGfxEffect3DOp::Create, NULL, NULL, 0, 0, 0, inUnit);
}

EEE3DRust::EEE3DRust(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->mRecord->aged.location = EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
    this->Call(EeGfxEffect3DOp::CreateAt, NULL, NULL, 0, 0, 0, const_cast<GE3DPoint*>(&inLocation));
}

// the multi-effect (0x3b) along a line (EEE3DMultiEffect, 0xf42c3)
EEE3DRust::EEE3DRust(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->Call(EeGfxEffect3DOp::CreateLine, NULL, NULL, 0, 0, 0, &inLine);
}

// the pulse cannon ball (0x43): from the unit's weapon point to the target (EEE3DPulseCannonBall, 0xf6a26); the
// citizen beam (0x33): towards the target (EEE3DCitizenBeam, 0xe94b7)
EEE3DRust::EEE3DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    EeGe3DPoint theTarget = {inTarget.mX, inTarget.mY, inTarget.mZ};
    if (inKind == 0x33) {   // the citizen beam
        this->mRecord->shot.end = theTarget;
    } else {
        ShotStart(6, inUnit, this->mRecord->shot.path.start);
        this->mRecord->shot.location = theTarget;
    }
    this->Call(EeGfxEffect3DOp::Create);
}

// the tracers (0x21) and shells (0x26) on inTo: from inFrom's weapon point to a point of inTo's model, in inFrom's
// player colour (EEE3DTracers, 0xfd373)
EEE3DRust::EEE3DRust(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inTo, inGraphicEffect, inKind)
{
    GE3DPoint theEnd;
    GETransformation theToTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    float theToScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    if (ShotStart(7, inFrom, this->mRecord->shot.path.start))
        EETagPointList::Scale(theToTransformation, theToScale);
    GE3DPoint theOffset = inTo->GetModel()->PickPoint();
    theToTransformation.Apply(theOffset, theEnd);
    const GEColor& theColor = inFrom->mPlayer->mColor;
    this->mRecord->shot.location = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->mRecord->shot.color = EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
    this->Call(EeGfxEffect3DOp::Create);
}

// the linear projectile (0x25): from inFrom to inTo (EEE3DLinearProjectile, 0xf2dd6)
EEE3DRust::EEE3DRust(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->mRecord->shot.path.start = EeGe3DPoint{inFrom.mX, inFrom.mY, inFrom.mZ};
    this->mRecord->shot.location = EeGe3DPoint{inTo.mX, inTo.mY, inTo.mZ};
    this->Call(EeGfxEffect3DOp::Create);
}

EEE3DRust::EEE3DRust(long inKind)
    : EEEffect3D(inKind)
{
}

// stops the sound (the disease's and virus's), unloads the graphic object's media and the unit model's (the
// additive pass's)
EEE3DRust::~EEE3DRust()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->mSound->Dereference();
        this->mSound = NULL;
    }
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    if (this->mUnitModel) {
        this->mUnitModel->UnloadMedia();
        this->mUnitModel = NULL;
    }
}

long EEE3DRust::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile, long inBase,
                     unsigned long inMajorVersion, unsigned long inMinorVersion, void* inArg)
{
    Ask theAsk = {this, inQueues, NULL, inOp, inArg, NULL, NULL};
    EeGfxEffect3DHost theHost = {&theAsk, Ask::UnitPosition, Ask::ModelHeight, Ask::MoveTo, Ask::Line, Ask::BaseQueue,
                                 Ask::Push, Ask::Media, Ask::Model, Ask::Texture, Ask::Follow, Ask::ModelAt,
                                 Ask::ShotHit, Ask::BeamStart, Ask::BeamUnit, Ask::BeamHidden, Ask::PlaceLinked,
                                 Ask::ComplexPosition, Ask::UnitModelMedia, Ask::UnitScale, Ask::UnitLocation,
                                 Ask::UnitModelRadius, Ask::UnitFrame, Ask::UnitModelAt, Ask::OrbitalCount,
                                 Ask::Orbital, Ask::Animation, Ask::AnimationSound, Ask::SpawnCount, Ask::SpawnAt,
                                 Ask::Spawn, Ask::TextureAt, Ask::UnitTile, Ask::TerrainZ, Ask::UnitHidden,
                                 Ask::SpaceTile, Ask::CreateLinked, Ask::WallGatePosts, Ask::WallPiece,
                                 Ask::GateOpen, Ask::PlayerColor, Ask::VentLists, Ask::VentUnit, Ask::VentCount,
                                 Ask::VentOffsets, Ask::VentPoints, Ask::Frames, Ask::UnitClassScale};
    // Load and Save read no facts: during a load the unit is still its saved id (RepairPointers resolves it)
    if (inFile != NULL)
        return ee_gfx_effect3d_file(inOp, inBase, this->mRecord, inFile, inMajorVersion, inMinorVersion, &theHost);
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    EEGraphicObject* theObject = theEffect ? theEffect->mGraphicObject : NULL;
    EeGfxGraphicFacts theFacts = {};
    if (theEffect != NULL) {
        theFacts.size = theEffect->mSize;
        theFacts.start_size = theEffect->mStartSize;
        theFacts.param1 = theEffect->mParam1;
        theFacts.param2 = theEffect->mParam2;
        theFacts.param3 = theEffect->mParam3;
        theFacts.param4 = theEffect->mParam4;
        theFacts.param5 = theEffect->mParam5;
        theFacts.param6 = theEffect->mParam6;
        theFacts.flag1 = theEffect->mFlag1;
        theFacts.flag2 = theEffect->mFlag2;
        theFacts.param7 = theEffect->mParam7;
        theFacts.param8 = theEffect->mParam8;
        theFacts.duration = (uint32_t)theEffect->mDuration;
        theFacts.has_object = theObject != NULL;
        theFacts.alpha_ref = theObject != NULL ? theObject->mAlphaReference : 0.0f;
    }
    theFacts.tick_seconds = gPhysics->GetTickSeconds();
    theFacts.tick_time = gPhysics->GetTickTime();
    theFacts.has_unit = this->mUnit != NULL;
    theFacts.unit_hit_points = this->mUnit != NULL ? (int32_t)this->mUnit->mHitPoints : 0;
    if (theEffect != NULL) {
        theFacts.int_param1 = (int32_t)theEffect->mIntParam1;
        theFacts.int_param2 = (int32_t)theEffect->mIntParam2;
        theFacts.int_param3 = (int32_t)theEffect->mIntParam3;
        theFacts.has_linked1 = theEffect->mLinkedEffect1 != NULL;
        theFacts.has_linked3 = theEffect->mLinkedEffect3 != NULL;
    }
    theFacts.game_time_bits = (uint32_t)(unsigned long)gPhysics->mGameTime;
    theFacts.free_camera = gFreeCamera;
    return ee_gfx_effect3d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// the base 3D effect, then the kind's own (ee_gfx); the first error, as each original
long EEE3DRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theBase = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}

// after loading: the base effect's pointers, then the kind's own (the media of the teleport and time warp)
long EEE3DRust::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    return theResult != 0 ? theResult : this->Call(EeGfxEffect3DOp::Repair);
}

// the energy wall (0x34): the current player sees the tile or holds a doppelganger of the unit there (then the wall
// is marked seen: its texture frame restarts) (EEE3DEnergyWall::IsTileVisible, 0xed98d); the others: the base's
bool EEE3DRust::IsTileVisible(EEMapTile* inTile)
{
    bool theResult = inTile->IsSeenBy(gWorld.GetCurrentPlayer());
    if (theResult || this->mKind != 0x34)
        return theResult;
    long thePlayer = gWorld.mCurrentPlayer;
    EEUnit* theUnit = this->mUnit;
    theResult = gDoppelgangerManager.HasDoppelganger(inTile, theUnit, thePlayer);
    if (theResult)
        this->mRecord->wall.seen = true;
    return theResult;
}

long EEE3DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theBase = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}
