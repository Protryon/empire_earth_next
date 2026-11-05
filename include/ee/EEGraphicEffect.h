// C:\EEX\Empire Earth\Database\EEGraphicEffect.h (RECONSTRUCTION; file name inferred). One effect of
// db\dbgfxeffects.dat (RTTI bases UReferenceCounter +0, EEMedia +8; vtables 0x43e534/0x43e524). Field names
// are provisional; offsets proven by the calamity units. Requires lle/LowLevelEngine.h, ee/EEMedia.h.
#ifndef EE_EEGRAPHICEFFECT_H
#define EE_EEGRAPHICEFFECT_H

#include <set>
#include <vector>

class EEGraphicObject;
class FSFile;
class FSFileSpec;

class EEGraphicEffect : public UReferenceCounter, public EEMedia {
public:
    // the table of every effect by index (out-of-line COMDAT 0x432f: 0 past the end; vector 0x4e8424) (provisional
    // name)
    static EEGraphicEffect* Get(unsigned long inIndex);
    // inline (out-of-line COMDAT 0x185a71 after its first user, EESETSETGame.cpp's GraphicEffect::Virtual14 loop)
    // (provisional name)
    static unsigned long GetCount() { return EEGraphicEffect::sEffects.size(); }
    float GetSize() const { return this->mSize; }  // inline (the hurricane copies it through the FP stack)
    // inline: the 3D effects load the parameters as values (EEE3DShield: `fld [+0x38]` twice for two copies, `fld
    // [+0x28]; fmul st1`) (provisional names)
    float GetStartSize() const { return this->mStartSize; }
    float GetAlpha() const { return this->mAlpha; }
    float GetParam1() const { return this->mParam1; }
    float GetParam2() const { return this->mParam2; }
    float GetParam3() const { return this->mParam3; }
    float GetParam4() const { return this->mParam4; }
    float GetParam5() const { return this->mParam5; }
    float GetParam6() const { return this->mParam6; }
    float GetParam7() const { return this->mParam7; }  // inline (EEE2DParticle::DrawPass6 loads the colour as values)
    // the unit EEGraphicEffect.cpp (0xd6bed..0xd72c1); the ctor copies a database record
    EEGraphicEffect(struct EEGraphicEffectRecord* inRecord);   // 0xd6ceb
    virtual ~EEGraphicEffect();             // ??_G 0xd6e5d (EEMedia's adjusting thunk 0xd726c), dtor 0xd6e79
    virtual long LoadMedia();               // EEMedia +0x04 0xd72a9
    virtual long UnloadMedia();             // EEMedia +0x08 0xd72b6
    static long LoadTable(FSFile* inFile);  // 0xd6e9e (db\dbgfxeffects.dat)
    static void DumpIDs(FSFileSpec& inFile);    // 0xd6fe6 (fprintf to the spec's stream; EEWorld::DumpMedia)
    static void UnloadTable();              // 0xd7048
    EEMedia* GetMedia();                    // 0xd7074 (the graphic object, or a random one of this effect's kind)
    static void SortMediaByKind();          // 0xd70b7
    void RecordNotPreloaded();              // 0xd723c (records the effect's id once)
    void Load(bool inIsOnDemand);           // 0xd7100 (counts a use at +0x80) (provisional name)
    void Unload();                          // 0xd71ac (provisional name)

    // members (the ctor 0xd6ceb's stores, a database record copied field by field; types by width only,
    // provisional names; sizeof 0x90)
    EEGraphicObject* mGraphicObject;        // +0x0c
    EEGraphicEffect* mLinkedEffect1;       // +0x10 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    EEGraphicEffect* mLinkedEffect2;       // +0x14 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    EEGraphicEffect* mLinkedEffect3;       // +0x18 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    float mSize;                            // +0x1c (provisional name; a float: the hurricane fld/fstp's it)
    float mStartSize;                       // +0x20 (fld/fmul in the EEE3D effects)
    float mAlpha;                           // +0x24 (the 2D effects' V18 fld/fchs it; db record +0x80 holds 1.0/0.9)
    float mParam1;                          // +0x28
    float mParam2;                          // +0x2c (fld/fmul in the EEE3D effects)
    float mParam3;                          // +0x30 (fld/fmul in the EEE3D effects)
    float mParam4;                          // +0x34 (fld/fmul in the EEE3D effects)
    float mParam5;                          // +0x38 (fld/fmul in the EEE3D effects)
    float mParam6;                          // +0x3c (fld/fmul in the EEE3D effects)
    float mParam7;                          // +0x40 (fld/fmul in the EEE3D effects)
    float GetParam8() const { return this->mParam8; }  // inline value getters
    float GetParam9() const { return this->mParam9; }
    float GetParam10() const { return this->mParam10; }
    float mParam8;                          // +0x44 (fld/fmul in the EEE3D effects)
    float mParam9;                          // +0x48 (fld/fmul in the EEE3D effects)
    float mParam10;                         // +0x4c (fld/fmul in the EEE3D effects)
    float mParam11;                         // +0x50 (fld/fmul in the EEE3D effects)
    float mEmitPositionSpread;                          // +0x54
    long mKind;                             // +0x58 (the plague draws 0x23 between two points, 0x40 unit to unit)
    long mTagPointKind;                     // +0x5c
    unsigned long mID;                      // +0x60 (unsigned: EESETSETGameGraphicEffect::Show 0x185790 pushes a converted temporary)
    long mIntParam1;                        // +0x64
    long mIntParam2;                        // +0x68
    long mIntParam3;                        // +0x6c
    unsigned long mTime1;                   // +0x70 (unsigned: the volcano converts it with fild qword)
    long mTime2;                            // +0x74
    unsigned long mDuration;                // +0x78 (provisional name)
    long mNameStringId;                     // +0x7c
    long mNumLoads;                         // +0x80
    long mDetailLevel;                      // +0x84
    bool mFlag1;                            // +0x88
    bool mFlag2;                            // +0x89
    bool mEndsWithUnitOrDuration;                            // +0x8a
    bool mIsScenarioListed;                 // +0x8b
    bool mIsEmitFlat;                            // +0x8c

    // the table Get/GetCount read (0x4e8420: allocator byte, _First 0x4e8424, _Last 0x4e8428; a class static like
    // Get) (provisional name)
    static std::vector<EEGraphicEffect*> sEffects;
    // the media of each kind (0x4e7f98, 0x48 kinds): a class static, its atexit dtor shares sEffects' guard
    // byte 0x4e8418 (provisional name)
    static std::vector<EEMedia*> sMediaByKind[0x48];
    // the effect ids (0x4e7f88): a class static, its atexit dtor shares sEffects' guard byte 0x4e8418
    // (provisional name)
    static std::set<unsigned long> sIDs;
};

// dbgfxeffects.dat effects by table index (EEGraphicEffect::Get; LoadTable 0x4d6e9e fills sEffects in file order)
enum EEGraphicEffectID {
    kGraphicEffectLandRipple = 0xd,                           // dbgfxeffects.dat record 13 (file order = sEffects index; +0x68 = 13) name 'Land Ripple'; its sites call EEGraphicEffect::Get
    kGraphicEffectImpactSparks = 0xf,                         // dbgfxeffects.dat record 15 (file order = sEffects index; +0x68 = 15) name 'Impact Sparks'; its sites call EEGraphicEffect::Get
    kGraphicEffectLightningUnit = 0x15,                       // dbgfxeffects.dat record 21 (file order = sEffects index; +0x68 = 21) name 'Lightning (unit)'; its sites call EEGraphicEffect::Get
    kGraphicEffectRain = 0x54,                                // dbgfxeffects.dat record 84 (file order = sEffects index; +0x68 = 84) name 'Rain'; its sites call EEGraphicEffect::Get
    kGraphicEffectHorseBreathUnits = 0x56,                    // dbgfxeffects.dat record 86 (file order = sEffects index; +0x68 = 86) name 'Horse Breath (units)'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestConversionIndiator = 0x5e,            // dbgfxeffects.dat record 94 (file order = sEffects index; +0x68 = 94) name 'Priest Conversion Indiator'; its sites call EEGraphicEffect::Get
    kGraphicEffectBasicMarchingLine = 0x60,                   // dbgfxeffects.dat record 96 (file order = sEffects index; +0x68 = 96) name 'Basic marching line'; its sites call EEGraphicEffect::Get
    kGraphicEffectWaypointIndicator = 0x62,                   // dbgfxeffects.dat record 98 (file order = sEffects index; +0x68 = 98) name 'Waypoint Indicator'; its sites call EEGraphicEffect::Get
    kGraphicEffectDustEffect = 0xab,                          // dbgfxeffects.dat record 171 (file order = sEffects index; +0x68 = 171) name 'Dust - Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectSnow = 0xac,                                // dbgfxeffects.dat record 172 (file order = sEffects index; +0x68 = 172) name 'Snow'; its sites call EEGraphicEffect::Get
    kGraphicEffectCalamityTeleportIn = 0xb4,                  // dbgfxeffects.dat record 180 (file order = sEffects index; +0x68 = 180) name 'Calamity - Teleport In'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMove = 0xb8,             // dbgfxeffects.dat record 184 (file order = sEffects index; +0x68 = 184) name 'Command Confirmation - Move'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationAttack = 0xb9,           // dbgfxeffects.dat record 185 (file order = sEffects index; +0x68 = 185) name 'Command Confirmation - Attack'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationGatherOther = 0xba,      // dbgfxeffects.dat record 186 (file order = sEffects index; +0x68 = 186) name 'Command Confirmation - Gather / Other'; its sites call EEGraphicEffect::Get
    kGraphicEffectDisbandUnitEmitter = 0xbc,                  // dbgfxeffects.dat record 188 (file order = sEffects index; +0x68 = 188) name 'Disband Unit (emitter)'; its sites call EEGraphicEffect::Get
    kGraphicEffectAircraftExplosion = 0xcf,                   // dbgfxeffects.dat record 207 (file order = sEffects index; +0x68 = 207) name 'Aircraft Explosion'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMouseClickNoUnit = 0xd1,  // dbgfxeffects.dat record 209 (file order = sEffects index; +0x68 = 209) name 'Command Confirmation - Mouse Click No Unit'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagBuilding = 0xd2,                   // dbgfxeffects.dat record 210 (file order = sEffects index; +0x68 = 210) name 'Rally Flag - Building'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMouseClickUnit = 0xd3,   // dbgfxeffects.dat record 211 (file order = sEffects index; +0x68 = 211) name 'Command Confirmation - Mouse Click Unit'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagFighter = 0xd4,                    // dbgfxeffects.dat record 212 (file order = sEffects index; +0x68 = 212) name 'Rally Flag - Fighter'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagBomber = 0xd5,                     // dbgfxeffects.dat record 213 (file order = sEffects index; +0x68 = 213) name 'Rally Flag - Bomber'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagAtomicBomber = 0xd6,               // dbgfxeffects.dat record 214 (file order = sEffects index; +0x68 = 214) name 'Rally Flag - Atomic Bomber'; its sites call EEGraphicEffect::Get
    kGraphicEffectDisintegrationAreaEffectIntermediate = 0xec,  // dbgfxeffects.dat record 236 (file order = sEffects index; +0x68 = 236) name 'Disintegration (area effect -intermediate)'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagParatrooper = 0xf5,                // dbgfxeffects.dat record 245 (file order = sEffects index; +0x68 = 245) name 'Rally Flag - Paratrooper'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestTowerSwirlEffect = 0xf7,              // dbgfxeffects.dat record 247 (file order = sEffects index; +0x68 = 247) name 'Priest Tower Swirl Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectFanaticismEffect = 0xfd,                    // dbgfxeffects.dat record 253 (file order = sEffects index; +0x68 = 253) name 'Fanaticism Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestTowerSmokeGenerator = 0x100,          // dbgfxeffects.dat record 256 (file order = sEffects index; +0x68 = 256) name 'Priest Tower Smoke Generator'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceCitizenMiningBeam = 0x10f,             // dbgfxeffects.dat record 271 (file order = sEffects index; +0x68 = 271) name 'Space Citizen Mining Beam'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceGateEnergyWall = 0x112,                // dbgfxeffects.dat record 274 (file order = sEffects index; +0x68 = 274) name 'Space Gate Energy Wall'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceWallEnergyWall = 0x113,                // dbgfxeffects.dat record 275 (file order = sEffects index; +0x68 = 275) name 'Space Wall Energy Wall'; its sites call EEGraphicEffect::Get
    kGraphicEffectMarketTicketTape = 0x11c,                   // dbgfxeffects.dat record 284 (file order = sEffects index; +0x68 = 284) name 'Market Ticket Tape'; its sites call EEGraphicEffect::Get
};

#endif
