// C:\EEX\Empire Earth\Graphics Thread\EEEffect.h (RECONSTRUCTION; file name inferred). The
// base of every graphics-thread effect (RTTI EEEffect > UReferenceCounter, vtable 0x43f9fc; EEEffect.cpp
// 0x10251d..0x10428c). Layout gap-free from the ctor 0x10258e, Init 0x102ffa and the dtor 0x102600; sizeof 0x30
// (EEEffect2D's members start at +0x30, EEEffect3D's GETransformation too). Slot signatures from the bodies (ret N,
// argument reads) and the overriding classes; provisional names unless stated. Requires lle/UReferenceCounter.h.
#ifndef EE_EEEFFECT_H
#define EE_EEEFFECT_H

#include <map>
#include <deque>

class EEUnit;
class EEGraphicEffect;
class FSFile;
class GE3DPoint;
class GETransformation;
class EEGraphicObject;
class GETexture;

// An effect's handle (provisional name). A struct, not an integer: the "no effect" constant is a real .rdata object
// in every unit that uses it (EECCAEExtraDamage 0x43c728, EECCAEMindControl 0x43c78c, EEEffect.cpp 0x4411a8),
// compared and copied through memory (an integral const would fold to an immediate).
struct EEEffectHandle {
    bool operator==(const EEEffectHandle& inHandle) const { return this->mValue == inHandle.mValue; }
    bool operator!=(const EEEffectHandle& inHandle) const { return this->mValue != inHandle.mValue; }

    unsigned long mValue;                   // (EECalamity Load/Save use FSFile Read/Write(unsigned long) on it)
};

static const EEEffectHandle kNoEffectHandle = {0};

class EEEffect;

// The effects' render queues, one map per pass: slot +0x24's argument is the 5-element global array 0x4e9418 (its
// init chain 0x10ac3a builds 5 maps of 12 bytes; EERenderObject::QueueEffects 0x10bff5 passes it to every visible effect).
// The 2D effects push themselves onto [4] (+0x30), others onto [2]/[3] (+0x18/+0x24), keyed by their texture
// (map::operator[] 0xdee70, deque::push_back 0xa96fd); the 3D effects push too, so the element is the base class.
// Typedef (provisional name)
typedef std::map<GETexture*, std::deque<EEEffect*> > EEEffectQueue;

class GEColor;
// 0x103634 (cdecl, in EEEffect.cpp's range): draws a ground ring of inSize at inCenter with the four corner heights
// inHeights (EEE3DEnergyWave::DrawPass6; arguments from its pushes) (provisional name; types inferred)
void AddGroundQuad(const GE3DPoint& inCenter, const GEColor& inColor, float inYaw, float inSize, float* inHeights,
              float inBrightness, float inAlphaReference);
// 0x10317d (cdecl, in EEEffect.cpp's range): adds a camera-facing quad of inSize at inPosition, turned by inRotation,
// to the 2D batch 0x4e8db8 (colour scaled by inAlpha * inArg6; inAlphaReference keeps the batch's minimum); the 2D
// effects' Virtual18 (EEE2DFireTrail) (provisional name; types inferred)
void AddGlowSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference);
// 0x1033f1 / 0x10387b (cdecl, in EEEffect.cpp's range): the same quad as AddGlowSprite, flat on the ground / another batch
// (EEE2DWaterRipple::DrawPass5, EEE2DSmokePlume::DrawPass6) (provisional names; types inferred)
void AddGroundSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference);
void AddBlendedSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference);
// 0x102f3d (cdecl, in EEEffect.cpp's range): a random value in [inMinimum, inMaximum] (also declared locally by
// EECPCombatManager.cpp; EEE2DImpact::DrawPass6's random roll) (provisional name)
float GetEffectRandomFloat(float inMinimum, float inMaximum);
// 0x102f57 (cdecl, in EEEffect.cpp's range): a random integer in [inMinimum, inMaximum] (EEE2DBattlefieldAmbient's
// effect variant pick) (provisional name)
long GetEffectRandomLong(long inMinimum, long inMaximum);

class EEEffect : public UReferenceCounter {
public:
    static const UString sData;             // "EEEffect-Data" (chain 0x10251d)
    // 0x10258e: vptr, Init(), then the arguments (both pointers referenced), mKind, mB2C, and the media load
    // `Virtual04(gU5318D8 == false)`
    EEEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit);
    // inline (the effects' load-time ctors: EEEffect3D(long) 0xe8e76 expands it; mKind before the count and the vptr,
    // then the flags; nothing else is set). VC6 refuses to expand this exact body in a probe (any 4 stores plus
    // UReferenceCounter's inline ctor) where the original expands it: the real spelling is cheaper)
    EEEffect(long inKind) : mKind(inKind) { this->mKeepsUnit = false; this->mIsSaved = true; this->mIsLocalOnly = false; this->mPlayer = 0; }
    // inline (EEEffect2D's load-time ctor 0xdf4a2 expands it: mB2D/mL20/mKind/mB2C before the vptr, mIsSaved after)
    EEEffect(long inKind, bool inKeepsUnit) : mKind(inKind), mKeepsUnit(inKeepsUnit) { this->mIsSaved = true; this->mIsLocalOnly = false; this->mPlayer = 0; }
    virtual ~EEEffect();                    // +0x00 0x102600 (??_G 0xdf50a): unloads, leaves its tile, dereferences
    virtual void LoadMedia(bool inIsOnDemand);  // +0x04 0x1030b5: mGraphicEffect->Load(inArg) when set
    virtual long DrawPass2();               // +0x08 0x1ad67e (`xor eax, eax`, ICF-shared)
    virtual long DrawPass3();               // +0x0c 0x1ad67e
    virtual long DrawPass4();               // +0x10 0x1ad67e
    virtual long DrawPass5();               // +0x14 0x1ad67e
    virtual long DrawPass6();               // +0x18 0x1ad67e
    virtual void UnloadMedia();             // +0x1c 0x10427f: mGraphicEffect->Unload() when set
    // +0x20 0x104250: replaces mUnit when it is inOldUnit and !mB2C (dereference the old, reference the new)
    virtual void ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);
    void SetIsLocalOnly(bool inValue) { this->mIsLocalOnly = inValue; }  // inline (EEAMouseClick::Virtual0C 0x403350: the original refuses its early message ctors)
    void SetPlayer(long inValue) { this->mPlayer = inValue; }  // inline (EEAMouseClick::Virtual0C 0x403350: the original refuses its early message ctors)
    void SetIsSaved(bool inValue) { this->mIsSaved = inValue; }  // inline (EEAMouseClick::Virtual0C 0x403350: the original refuses its early message ctors)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x102807: 10 when mB2D and mL20 != gU5318C4, else 0
    virtual void ReleaseResources();        // +0x28 0x102e61: Virtual1C, drops mGraphicEffect, leaves the tile, mUnit
    virtual void PrepareQueue() = 0;        // +0x2c
    virtual long SetUp() = 0;               // +0x30
    // +0x34: `jmp __imp_?Dereference@UReferenceCounter@@QAEXXZ` (ICF-identical to the import thunk, like RTSUnit's
    // LastDereference 0x23db49)
    virtual void LastDereference() { this->Dereference(); }
    // inline (EEEMStorm::Release 0x12e9eb: its out-of-line COMDAT is ICF-folded into RTSUnit::Dereference 0x1db4;
    // provisional name: not `Dereference`, which would hide UReferenceCounter's for every caller)
    void Release() { if (this->GetReferenceCount() > 1) UReferenceCounter::Dereference(); else this->LastDereference(); }
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x103027
    virtual long RepairPointers();          // +0x3c 0x103f81: mUnit's saved id -> gWorld.GetUnit + dynamic_cast (7: none)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x10400a
    // +0x44 0x104175 / +0x48 0x10409b: move to the tile under the translation (+0x0c/+0x1c) or the point (+0/+4),
    // leaving the old tile's effect list (0x239ee2) and joining the new one (0xf40ac)
    virtual void MoveToTransformation(const GETransformation& inTransformation);
    virtual void MoveToPoint(const GE3DPoint& inLocation);
    virtual long Step() = 0;                // +0x4c
    // +0x50 0xdf42d (`ret 0x10`, ICF-shared): inline (its COMDAT follows EEE2DBattlefieldAmbient.cpp, the first unit
    // that emits EEEffect's vtable through the inline EEEffect2D ctors)
    virtual void ElevationChanged(long, long, long, long) {}

    // 0x102f8b: the effects' random number (`mov ecx, 0x4e8dc8; jmp URandom::Generate`: EEEffect.cpp's URandom object;
    // EEE3DLinearProjectile's speed and orbital angles) (provisional name)
    static float GetRandom();
    // a value around inCenter, up to half of inSpread either way: the effects' spread (EEE3DHurricane,
    // EEE3DSmokeGenerator, EEE3DParticleEmitter, EEE2DBattlefieldAmbient copy both by-value arguments, rightmost first,
    // before GetRandom; one shared inline) (provisional name)
    static float GetRandomSpread(float inSpread, float inCenter) { return (0.5f - EEEffect::GetRandom()) * inSpread + inCenter; }
    static float GetRandomSpread(long inSpread, long inCenter) { return (0.5f - EEEffect::GetRandom()) * inSpread + inCenter; }
    // 0x102759: the unit's location (its complex unit's position, else its tile centre and +0x14's +0x10 height, else
    // 0), in a function-local static (guard 0x4e8da4; EEEffect2D::CreateParts) (provisional name)
    const GE3DPoint& GetUnitLocation();
    EEUnit* GetUnit() const { return this->mUnit; }  // inline: EEEDamagedEffect::Step 0x50225c
    EEGraphicEffect* GetGraphicEffect() const { return this->mGraphicEffect; }  // inline: the effects' Queue tail (0x4ebfd0)
    EEGraphicObject* GetGraphicObject();    // 0x102f96: mGraphicEffect's object (0 without one) (provisional name)
    void Init();                            // 0x102ffa (the ctor's: pointers 0, tile -1/-1, mHandle = none, mIsSaved = true)
    // 0x102fa6 / 0x102fe7: Init(), then the ctors' argument stores (and the media load); EEEffect3D's 0x105441/0x10545e
    // call them (provisional names)
    void Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, bool inKeepsUnit);
    void Reinit(bool inKeepsUnit);

    EEUnit* mUnit;                          // +0x08 (referenced; RTSUnit::Dereference in the dtor; saved as its id)
    EEGraphicEffect* mGraphicEffect;        // +0x0c (referenced; Load/Unload)
    EEEffect* mNext;                        // +0x10 (the next effect of EEEffectManager +0x20's list)
    EEEffect* mPrevious;                    // +0x14 (the previous effect of that list: Add 0x5055d0 stores it, Remove 0x5072d0 writes its mNext)
    long mKind;                             // +0x18 (ctor argument; the fire functor tests 0x3e = EEEDamagedEffect)
    EEEffectHandle mHandle;                 // +0x1c (= kNoEffectHandle; provisional name)
    long mPlayer;                           // +0x20 (compared with gU5318C4)
    long mTileX;                            // +0x24 (-1: on no tile; provisional name)
    long mTileY;                            // +0x28 (-1)
    bool mKeepsUnit;                        // +0x2c (ctor argument; set: Virtual20 keeps the unit)
    bool mIsLocalOnly;                      // +0x2d
    bool mIsSaved;                          // +0x2e (Init: true)
};

#endif
