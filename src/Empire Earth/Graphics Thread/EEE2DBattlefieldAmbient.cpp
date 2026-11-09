// C:\EEX\Empire Earth\Graphics Thread\EEE2DBattlefieldAmbient.cpp (no __FILE__; name from the RTTI class and the key).
// 0xde331..0xdf57c: the key chain, the two decal helpers the physics models call (EEPlaceImpactDecal, EEPlaceTrackDecal) with their
// static helpers, the ctors .. Virtual4C, then the first copies of the effect render-queue templates and of
// EEEffect/EEEffect2D's inline members (Virtual34, Virtual50, SetPosition, the EEEffect2D ctors, both ??_G, the
// vector<EEEffect2DPart> _Buy); EEE2DCircularMarchingIcons.cpp follows.
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
#include "ee/EEEffectManager.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4de331
// SYNTHETIC: EXE 0x4de33b
// SYNTHETIC: EXE 0x4de34c
// SYNTHETIC: EXE 0x4de358
const UString EEE2DBattlefieldAmbient::sData("EEE2DBattlefieldAmbient-Data");

// Floats 0x43f924/0x43f928 are shared pooled literals; outside-TU readers proven.
// 0x43f924: pooled 0.005f, not an independently bound unit-local symbol.
// 0x43f928: pooled 0.7853982f; the near-threshold doubles below are inferred.
// Near thresholds are literal doubles: original words0x43f930/0x43f938, payloads independently checked.
// Positive threshold also has an outside-TU reader; do not give the shared word a private name.
// the slope limits 0x43f940/0x43f944 are pooled __real literals (0x43f944 = 0.03f is read by four units:
// CitizenBeam, EEPMBuilding, 0x53ac1d), first contributed by this object

static EEGraphicEffect* AmbientPickVariant(EEGraphicEffect* inEffect);
static bool AmbientRollChance(EEGraphicEffect* inEffect);
EEE2DBattlefieldAmbient* AmbientFindNear(unsigned long inType, GE3DPoint& inLocation);
static bool AmbientScatter(EEGraphicEffect* inEffect, GE3DPoint& ioLocation);

// NEAR: register choice (the original keeps the effect in edi, &mL68 in esi and the ambient in ebx, reloading
// inLocation; ours gives the variant a parameter slot)
// FUNCTION: EXE 0x4de374
// Places up to N shell-hole decals of the unit's class around a point (ambient effects on); stops on a failed roll
// The first one renews a decal already on that spot instead, unless its scatter roll moves it
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation)
{
    long i;
    EEGraphicEffect* theVariant;
    GE3DPoint theLocation = inLocation;
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || (theEffect = inUnit->mUnitClass->mShellHoleEffect) == 0)
        return;
    theLocation.SetZ(theLocation.GetZ() + 0.005f);
    for (i = 0; i < theEffect->mIntParam2; i++) {
        theVariant = AmbientPickVariant(theEffect);
        if (!AmbientRollChance(theVariant))
            return;
        if (i == 0) {
            EEE2DBattlefieldAmbient* theAmbient = AmbientFindNear(0, theLocation);
            if (theAmbient && !AmbientScatter(theVariant, theLocation)) {
                theAmbient->mAge = 0.0f;
                continue;
            }
        } else {
            theLocation.SetX(EEEffect::GetRandomSpread(theEffect->GetParam5(), inLocation.GetX()));
            theLocation.SetY(EEEffect::GetRandomSpread(theEffect->GetParam5(), inLocation.GetY()));
            theLocation.SetZ(gWorld.mMap->GetHeight(theLocation) + 0.005f);
        }
        gEffectManager->CreatePlacedEffect(theLocation, theVariant, NULL);
    }
}

// FUNCTION: EXE 0x4de4a9
// Places the unit class's blood-stain decal under the unit if ambient effects and blood are on and its roll passes
// (moved forward along the heading for giraffes and horses)
void EEPlaceTrackDecal(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || !gDrawBlood || (theEffect = inUnit->mUnitClass->mBloodStainEffect) == 0)
        return;
    EEGraphicEffect* theVariant = AmbientPickVariant(theEffect);
    if (!AmbientRollChance(theVariant))
        return;
    inUnit->GetPosition(theLocation);
    unsigned long theType = inUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypeResGiraffe || theType == kUnitTypeResHorse) {
        const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
        float theAngle = UMArcTan2(theTransformation.mMatrix[1][0], theTransformation.mMatrix[0][0]);
        float theDistance = 0.4f;
        if (theType == kUnitTypeResHorse) {
            theDistance = 0.3f;
            theAngle -= 0.7853982f;
        }
        theLocation.SetX(theLocation.GetX() + UMCos(theAngle) * theDistance);
        theLocation.SetY(theLocation.GetY() + UMCos(1.5707964f - theAngle) * theDistance);
    }
    theLocation.SetZ(theLocation.GetZ() + 0.005f);
    gEffectManager->CreatePlacedEffect(theLocation, theVariant, NULL);
}

// FUNCTION: EXE 0x4de5ca
// Returns the first battlefield decal of the given type within 0.05 tiles of a point, or NULL
// NEAR: registers only
static EEE2DBattlefieldAmbient* AmbientFindNear(unsigned long inType, GE3DPoint& inLocation)
{
    EEE2DBattlefieldAmbient* theAmbient;
    EEEffect* theEffect = gEffectManager->mEffects;
    while (theEffect) {
        if (theEffect->mKind == kEffect2DBattlefieldAmbient) {
            theAmbient = dynamic_cast<EEE2DBattlefieldAmbient*>(theEffect);
            if (theAmbient->mType == inType) {
                GE3DPoint theOffset = inLocation;
                theOffset -= theAmbient->mCenter;
                if (theOffset.GetX() < 0.05 && theOffset.GetX() > -0.05 &&
                    theOffset.GetY() < 0.05 && theOffset.GetY() > -0.05)
                    return theAmbient;
            }
        }
        theEffect = theEffect->mNext;
    }
    return NULL;
}

// FUNCTION: EXE 0x4de66d
// Rolls the effect's chance (parameter 2); always succeeds when it is 1 or more
static bool AmbientRollChance(EEGraphicEffect* inEffect)
{
    if (inEffect->GetParam2() >= 1.0f)
        return true;
    float theChance = inEffect->GetParam2();
    return EEEffect::GetRandom() <= theChance;
}

// FUNCTION: EXE 0x4de6a1
// Picks the effect or one of its up to three linked variants, each equally likely
static EEGraphicEffect* AmbientPickVariant(EEGraphicEffect* inEffect)
{
    long theCount;
    if (inEffect->mLinkedEffect3)
        theCount = 4;
    else if (inEffect->mLinkedEffect2)
        theCount = 3;
    else if (inEffect->mLinkedEffect1)
        theCount = 2;
    else
        return inEffect;
    switch (GetEffectRandomLong(1, theCount)) {
    case 2:
        return inEffect->mLinkedEffect1;
    case 3:
        return inEffect->mLinkedEffect2;
    case 4:
        return inEffect->mLinkedEffect3;
    }
    return inEffect;
}

// FUNCTION: EXE 0x4de6e9
// With the effect's scatter chance (parameter 3), moves the point randomly within parameter 4; returns whether it moved
static bool AmbientScatter(EEGraphicEffect* inEffect, GE3DPoint& ioLocation)
{
    if (EEEffect::GetRandom() >= inEffect->GetParam3())
        return false;
    ioLocation.SetX(ioLocation.GetX() + inEffect->GetParam4() * (0.5f - EEEffect::GetRandom()));
    ioLocation.SetY(ioLocation.GetY() + inEffect->GetParam4() * (0.5f - EEEffect::GetRandom()));
    return true;
}

// NEAR: the `theLevel = true` store is scheduled after the mesh pointer in ours
// FUNCTION: EXE 0x4de74f
// Creates a ground decal at a point with a random size and optional random rotation; it expires at once (age 1)
// when a corner is off the map or the corners are not level, so the on-slope branch never runs
EEE2DBattlefieldAmbient::EEE2DBattlefieldAmbient(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gU4E8DF4, kEffect2DBattlefieldAmbient, false), mAge(0.0f)
{
    this->mDrawAge = 0.0f;
    this->mHidden = 0.0f;
    this->mType = inGraphicEffect->mIntParam1;
    this->mGraphicObject = NULL;
    this->mSize = inGraphicEffect->GetStartSize() + EEEffect::GetRandom() * (inGraphicEffect->GetSize() - inGraphicEffect->GetStartSize());
    this->mCenter = inLocation;
    this->mCorners[0].SetX(inLocation.GetX() - this->mSize * 0.5f);
    this->mCorners[0].SetY(inLocation.GetY() - this->mSize * 0.5f);
    this->mCorners[1].SetX(inLocation.GetX() + this->mSize * 0.5f);
    this->mCorners[1].SetY(inLocation.GetY() - this->mSize * 0.5f);
    this->mCorners[2].SetX(inLocation.GetX() + this->mSize * 0.5f);
    this->mCorners[2].SetY(inLocation.GetY() + this->mSize * 0.5f);
    this->mCorners[3].SetX(inLocation.GetX() - this->mSize * 0.5f);
    this->mCorners[3].SetY(inLocation.GetY() + this->mSize * 0.5f);
    bool theLevel = true;
    GETerrainMesh& theMesh = gWorld.mMap->mTerrainMesh;
    float theFirstZ = 0.0f;
    for (long i = 0; i < 4; i++) {
        unsigned long theX = UMFloorUnsigned(this->mCorners[i].GetX());
        unsigned long theY = UMFloorUnsigned(this->mCorners[i].GetY());
        if (!theMesh.GetIsValidTile(theX, theY)) {
            this->mHidden = 1.0f;
            break;
        }
        this->mCorners[i].SetZ(theMesh.GetMeshZ(this->mCorners[i].GetX(), this->mCorners[i].GetY()));
        float theZ = this->mCorners[i].GetZ();
        if (i == 0) {
            theFirstZ = this->mCorners[0].GetZ();
        } else if (theLevel) {
            float theRise = theZ - theFirstZ;
            if (theRise > 0.03f || theRise < -0.03f) {
                theLevel = false;
                this->mHidden = 1.0f;
            }
        }
    }
    if (this->mHidden != 0.0f) {
        this->mAge = 1.0f;
    } else {
        if (theLevel) {
            this->mOnSlope = 0;
            if (inGraphicEffect->mFlag1)
                this->mRotation = EEEffect::GetRandom() * 6.2831855f;
            else
                this->mRotation = 0.0f;
        } else {
            this->mOnSlope = 1;
            this->mRotation = 0.0f;
        }
        this->mTransformation.SetPosition(inLocation);
    }
}

// SYNTHETIC: EXE 0x4dea0b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4dea27
// Creates an empty battlefield decal for loading from a saved game
EEE2DBattlefieldAmbient::EEE2DBattlefieldAmbient()
    : EEEffect2D(gU4E8DF4, kEffect2DBattlefieldAmbient, false)
{
}

// FUNCTION: EXE 0x4dea76
// Destroys the decal, unloading its graphic's media
EEE2DBattlefieldAmbient::~EEE2DBattlefieldAmbient()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DBattlefieldAmbient::DrawPass4()
{
    return 0;
}

long EEE2DBattlefieldAmbient::DrawPass5()
{
    return 0;
}

long EEE2DBattlefieldAmbient::DrawPass6()
{
    return 0;
}

void EEE2DBattlefieldAmbient::PrepareQueue()
{
}

// NEAR: the original reads mAge through a pointer (lea) and keeps it on the FP stack (fcom) for the product
// FUNCTION: EXE 0x4deab2
// Queues the decal in render queue 0 under its texture for this screen; flagged effects grow in over 10% of life
long EEE2DBattlefieldAmbient::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        if (this->mGraphicEffect->mFlag2) {
            float theAge = this->mAge;
            if (theAge < 0.1f)
                this->mDrawSize = (theAge * 10.0f) * this->mSize;
            else
                this->mDrawSize = this->mSize;
        } else
            this->mDrawSize = this->mSize;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[0][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4deb58
// Loads the decal's graphic and sets its position and bounding radius (the effect's size); returns the load result
long EEE2DBattlefieldAmbient::SetUp()
{
    GE3DPoint thePosition;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mTransformation.GetPosition(thePosition);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4debba
// Draws the decal flat on the ground, fading it out over its last 20% of life; draws nothing when hidden
long EEE2DBattlefieldAmbient::DrawPass2()
{
    if (this->mHidden != 0.0f)
        return 0;
    float theFade;
    if (this->mDrawAge >= 0.8f)
        theFade = (1.0f - this->mDrawAge) * 5.0f;
    else
        theFade = 1.0f;
    float theReference;
    if (this->mGraphicEffect->mGraphicObject)
        theReference = this->mGraphicEffect->mGraphicObject->mAlphaReference;
    else
        theReference = 0.0f;
    GE3DPoint thePosition(this->mTransformation);
    float theScale = this->mGraphicEffect->mParam1;
    float theAlpha = this->mGraphicEffect->mAlpha;
    GEColor theColor;
    AddGroundSprite(thePosition, theColor, this->mRotation, this->mDrawSize, theAlpha * theFade, theScale, theReference);
    return 0;
}

// FUNCTION: EXE 0x4dec7f
// Reloads the decal's graphic and reads its placement, age, type, size, slope flag and corners (hidden when on a slope)
// Returns the first read error; the base class result is ignored
long EEE2DBattlefieldAmbient::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long i;
    long theType;
    long theOnSlope;
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mTransformation.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mAge);
    if (theResult == 0)
        theResult = this->mCenter.Load(inFile);
    if (theResult == 0) {
        theResult = inFile.Read(theType);
        this->mType = theType;
    }
    if (theResult == 0)
        theResult = inFile.Read(this->mRotation);
    if (theResult == 0)
        theResult = inFile.Read(this->mSize);
    if (theResult == 0) {
        theResult = inFile.Read(theOnSlope);
        this->mOnSlope = theOnSlope;
    }
    if (theResult == 0) {
        for (i = 0; i < 4; i++) {
            theResult = this->mCorners[i].Load(inFile);
            if (theResult != 0)
                break;
        }
    }
    this->mHidden = this->mOnSlope != 0;
    return theResult;
}

// FUNCTION: EXE 0x4ded67
// Writes the decal's placement, age, type, rotation, size, slope flag and corners; returns the first write error
// (the base class result is ignored)
long EEE2DBattlefieldAmbient::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long i;
    long theType;
    long theOnSlope;
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mTransformation.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = this->mCenter.Save(outFile);
    if (theResult == 0)
    {
        theType = this->mType;
        theResult = outFile.Write(theType);
    }
    if (theResult == 0)
        theResult = outFile.Write(this->mRotation);
    if (theResult == 0)
        theResult = outFile.Write(this->mSize);
    if (theResult == 0)
    {
        theOnSlope = this->mOnSlope;
        theResult = outFile.Write(theOnSlope);
    }
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 4; i++) {
        theResult = this->mCorners[i].Save(outFile);
        if (theResult != 0)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4dee2b
// Ages the decal by the tick over the effect's duration; returns 7 (remove) once its life is over
long EEE2DBattlefieldAmbient::Step()
{
    long theResult = 0;
    this->mAge += (gPhysics->GetTickSeconds() * 1000.0f) / this->mGraphicEffect->mDuration;
    if (this->mAge > 1.0f)
        theResult = 7;
    return theResult;
}

// The first copies of the effect render-queue templates (Virtual24's inQueues[0][theTexture].push_back) and of
// EEEffect/EEEffect2D's inline members (their vtables are first needed here, through the inline EEEffect2D ctors).

// FUNCTION: EXE 0x4dee70
// Returns a render queue's effect list for a texture, adding an empty one first (std::map<...>::operator[])

// FUNCTION: EXE 0x4deeca
// Inserts a texture's effect list into a render queue map unless the key is there (std::_Tree<...>::insert)

// FUNCTION: EXE 0x4def80
// Links a new node into the render queue map's red-black tree and rebalances it (std::_Tree<...>::_Insert)

// FUNCTION: EXE 0x4df114
// Copies a list of queued effects (std::deque<EEEffect*> copy constructor)

// FUNCTION: EXE 0x4df150
// Steps a render queue map iterator back to the previous node (std::_Tree<...>::const_iterator::_Dec, ICF-named)

// FUNCTION: EXE 0x4df191
// Copy-constructs a texture/effect-list pair in place (std::_Construct<pair<GETexture* const, deque<EEEffect*> > >)

// FUNCTION: EXE 0x4df1ae
// Inserts a range of effects into a queued-effect list at a position (std::deque<EEEffect*>::_Insert, ICF-named)

// FUNCTION: EXE 0x4df412
// Returns a queued-effect list iterator moved forward by n (std::deque<EEEffect*>::iterator::operator+)

// FUNCTION: EXE 0x4df427
// Drops the effect's last reference (EEEffect::LastDereference, inline in ee/EEEffect.h)

// FUNCTION: EXE 0x4df42d
// Ignores terrain elevation changes (EEEffect::ElevationChanged, inline in ee/EEEffect.h)

// FUNCTION: EXE 0x4df430
// Moves the 2D effect's transformation to a point (EEEffect2D::SetPosition, inline in ee/EEEffect2D.h)

// FUNCTION: EXE 0x4df44e
// Creates a 2D effect for a unit and graphic effect with a model and kind (inline EEEffect2D ctor, ee/EEEffect2D.h)

// FUNCTION: EXE 0x4df4a2
// Creates an empty 2D effect with a model and kind, for loading (inline EEEffect2D ctor, ee/EEEffect2D.h)

// FUNCTION: EXE 0x4df50a
// Destroys an effect and optionally frees it (EEEffect's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x4df526
// Destroys a 2D effect and optionally frees it (EEEffect2D's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x4df542
// Allocates room for n effect parts (std::vector<EEEffect2DPart>::_Buy, ICF-named); returns whether n was nonzero
