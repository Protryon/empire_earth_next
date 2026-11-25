//! The game's named integer constants (the effect kinds of ee/EEEffectKindIDs.h, the graphic effect and sound table
//! indices of ee/EEGraphicEffect.h and ee/EESoundObject.h, ee/EEImprovementIDs.h, ee/EEPersistentTag.h,
//! ee/EEResourceType.h, ee/EEStormType.h, ee/EEAIKindIDs.h, the terrain types of ee/EEMap.h,
//! the calamities of ee/EECalamityManager.h and the damage forms of ee/EERules.h), as plain (unscoped) C enums for the
//! C++ headers; each comment gives the evidence for its name, "(provisional name)" marks an inferred one.
#![allow(non_camel_case_types, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// Effect class kinds (EEEffect::mKind, the ctor argument; EEGraphicEffect::mKind = the kind a graphic-effect record
/// creates): EEEffectManager.cpp creates each class by kind (load, create, copy switches); names from the RTTI class
/// created (EEE prefix dropped).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEEffectKind {
    /// RTTI EEE3DIcon: EEEffectManager.cpp case 0x2 creates it
    kEffect3DIcon = 0x2,
    /// RTTI EEE2DExhaustTrail: EEEffectManager.cpp case 0x3 creates it
    kEffect2DExhaustTrail = 0x3,
    /// RTTI EEE2DExplosion: EEEffectManager.cpp case 0x4 creates it
    kEffect2DExplosion = 0x4,
    /// RTTI EEE2DFire: EEEffectManager.cpp case 0x5 creates it
    kEffect2DFire = 0x5,
    /// RTTI EEE2DImpact: EEEffectManager.cpp case 0x6 creates it
    kEffect2DImpact = 0x6,
    /// RTTI EEE2DImpactBlood: EEEffectManager.cpp case 0x7 creates it
    kEffect2DImpactBlood = 0x7,
    /// RTTI EEE2DLightning: EEEffectManager.cpp case 0x8 creates it
    kEffect2DLightning = 0x8,
    /// RTTI EEE2DMuzzleFlash: EEEffectManager.cpp case 0x9 creates it
    kEffect2DMuzzleFlash = 0x9,
    /// RTTI EEE2DPlasmaPulse: EEEffectManager.cpp case 0xa creates it
    kEffect2DPlasmaPulse = 0xa,
    /// RTTI EEE2DSmokePlume: EEEffectManager.cpp case 0xb creates it
    kEffect2DSmokePlume = 0xb,
    /// RTTI EEE2DVelocityParticle: EEEffectManager.cpp case 0xc creates it
    kEffect2DVelocityParticle = 0xc,
    /// RTTI EEE2DLightGlow: EEEffectManager.cpp case 0xd creates it
    kEffect2DLightGlow = 0xd,
    /// RTTI EEE2DFireTrail: EEEffectManager.cpp case 0xe creates it
    kEffect2DFireTrail = 0xe,
    /// RTTI EEE2DSquareMarchingIcons: EEEffectManager.cpp case 0xf creates it
    kEffect2DSquareMarchingIcons = 0xf,
    /// RTTI EEE2DCircularMarchingIcons: EEEffectManager.cpp case 0x10 creates it
    kEffect2DCircularMarchingIcons = 0x10,
    /// RTTI EEE2DShockwave: EEEffectManager.cpp case 0x12 creates it
    kEffect2DShockwave = 0x12,
    /// RTTI EEE2DSmokeMuzzleBlast: EEEffectManager.cpp case 0x14 creates it
    kEffect2DSmokeMuzzleBlast = 0x14,
    /// RTTI EEE2DWinterBreath: EEEffectManager.cpp case 0x16 creates it
    kEffect2DWinterBreath = 0x16,
    /// RTTI EEE3DBlastWave: EEEffectManager.cpp case 0x18 creates it
    kEffect3DBlastWave = 0x18,
    /// RTTI EEE3DEarthquake: EEEffectManager.cpp case 0x1a creates it; its ctor passes the kind
    kEffect3DEarthquake = 0x1a,
    /// RTTI EEE3DWaterSplash: EEEffectManager.cpp case 0x1b creates it; its ctor passes the kind
    kEffect3DWaterSplash = 0x1b,
    /// RTTI EEE3DDestructionSmoke: EEEffectManager.cpp case 0x1e creates it
    kEffect3DDestructionSmoke = 0x1e,
    /// RTTI EEE3DVolcano: EEEffectManager.cpp case 0x20 creates it; its ctor passes the kind
    kEffect3DVolcano = 0x20,
    /// RTTI EEE3DTracers: EEEffectManager.cpp case 0x21 creates it
    kEffect3DTracers = 0x21,
    /// RTTI EEE2DWaterRipple: EEEffectManager.cpp case 0x22 creates it
    kEffect2DWaterRipple = 0x22,
    /// RTTI EEE3DDisease: EEEffectManager.cpp case 0x23 creates it; its ctor passes the kind
    kEffect3DDisease = 0x23,
    /// RTTI EEE3DHurricane: EEEffectManager.cpp case 0x24 creates it; its ctor passes the kind
    kEffect3DHurricane = 0x24,
    /// RTTI EEE3DLinearProjectile: EEEffectManager.cpp case 0x25 creates it; its ctor passes the kind
    kEffect3DLinearProjectile = 0x25,
    /// RTTI EEE3DShells: EEEffectManager.cpp case 0x26 creates it
    kEffect3DShells = 0x26,
    /// RTTI EEE3DAdditivePass: EEEffectManager.cpp case 0x27 creates it; its ctor passes the kind
    kEffect3DAdditivePass = 0x27,
    /// RTTI EEE3DNuclearExplosion: EEEffectManager.cpp case 0x28 creates it; its ctor passes the kind
    kEffect3DNuclearExplosion = 0x28,
    /// RTTI EEE3DFarm: EEEffectManager.cpp case 0x29 creates it; its ctor passes the kind
    kEffect3DFarm = 0x29,
    /// RTTI EEE3DRain: EEEffectManager.cpp case 0x2a creates it
    kEffect3DRain = 0x2a,
    /// RTTI EEE3DSmokeGenerator: EEEffectManager.cpp case 0x2b creates it
    kEffect3DSmokeGenerator = 0x2b,
    /// RTTI EEE3DDamageSmoke: EEEffectManager.cpp case 0x2c creates it
    kEffect3DDamageSmoke = 0x2c,
    /// RTTI EEE3DMarchingLine: EEEffectManager.cpp case 0x2d creates it; its ctor passes the kind
    kEffect3DMarchingLine = 0x2d,
    /// RTTI EEE2DParticle: EEEffectManager.cpp case 0x2e creates it
    kEffect2DParticle = 0x2e,
    /// RTTI EEE2DPulseWave: EEEffectManager.cpp case 0x2f creates it
    kEffect2DPulseWave = 0x2f,
    /// RTTI EEE2DLinearProjectile: EEEffectManager.cpp case 0x30 creates it
    kEffect2DLinearProjectile = 0x30,
    /// RTTI EEE2DVortex: EEEffectManager.cpp case 0x31 creates it
    kEffect2DVortex = 0x31,
    /// RTTI EEE2DBattlefieldAmbient: EEEffectManager.cpp case 0x32 creates it
    kEffect2DBattlefieldAmbient = 0x32,
    /// RTTI EEE3DCitizenBeam: EEEffectManager.cpp case 0x33 creates it
    kEffect3DCitizenBeam = 0x33,
    /// RTTI EEE3DEnergyWall: EEEffectManager.cpp case 0x34 creates it
    kEffect3DEnergyWall = 0x34,
    /// RTTI EEE3DDamageVent: EEEffectManager.cpp case 0x35 creates it; its ctor passes the kind
    kEffect3DDamageVent = 0x35,
    /// RTTI EEE3DParticle: EEEffectManager.cpp case 0x39 creates it
    kEffect3DParticle = 0x39,
    /// RTTI EEE3DParticleEmitter: EEEffectManager.cpp case 0x3a creates it
    kEffect3DParticleEmitter = 0x3a,
    /// RTTI EEE3DMultiEffect: EEEffectManager.cpp case 0x3b creates it
    kEffect3DMultiEffect = 0x3b,
    /// RTTI EEE3DGoal: EEEffectManager.cpp case 0x3c creates it
    kEffect3DGoal = 0x3c,
    /// RTTI EEE3DSnow: EEEffectManager.cpp case 0x3d creates it
    kEffect3DSnow = 0x3d,
    /// RTTI EEEDamagedEffect: EEEffectManager.cpp case 0x3e creates it
    kEffectDamaged = 0x3e,
    /// RTTI EEE3DTeleport: EEEffectManager.cpp case 0x3f creates it
    kEffect3DTeleport = 0x3f,
    /// RTTI EEE3DTracerBeam: EEEffectManager.cpp case 0x40 creates it
    kEffect3DTracerBeam = 0x40,
    /// RTTI EEE3DCommandConfirmation: EEEffectManager.cpp case 0x41 creates it
    kEffect3DCommandConfirmation = 0x41,
    /// RTTI EEE3DVirus: EEEffectManager.cpp case 0x42 creates it; its ctor passes the kind
    kEffect3DVirus = 0x42,
    /// RTTI EEE3DPulseCannonBall: EEEffectManager.cpp case 0x43 creates it
    kEffect3DPulseCannonBall = 0x43,
    /// RTTI EEE3DTimeWarp: EEEffectManager.cpp case 0x44 creates it
    kEffect3DTimeWarp = 0x44,
    /// RTTI EEE3DShield: EEEffectManager.cpp case 0x45 creates it; its ctor passes the kind
    kEffect3DShield = 0x45,
    /// RTTI EEE3DScreenFlash: EEEffectManager.cpp case 0x46 creates it; its ctor passes the kind
    kEffect3DScreenFlash = 0x46,
    /// RTTI EEE3DEnergyWave: EEEffectManager.cpp case 0x47 creates it; its ctor passes the kind
    kEffect3DEnergyWave = 0x47,
}

/// dbgfxeffects.dat effects by table index (EEGraphicEffect::Get; LoadTable 0x4d6e9e fills sEffects in file order)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEGraphicEffectID {
    /// dbgfxeffects.dat record 13 (file order = sEffects index; +0x68 = 13) name 'Land Ripple'; its sites call EEGraphicEffect::Get
    kGraphicEffectLandRipple = 0xd,
    /// dbgfxeffects.dat record 15 (file order = sEffects index; +0x68 = 15) name 'Impact Sparks'; its sites call EEGraphicEffect::Get
    kGraphicEffectImpactSparks = 0xf,
    /// dbgfxeffects.dat record 21 (file order = sEffects index; +0x68 = 21) name 'Lightning (unit)'; its sites call EEGraphicEffect::Get
    kGraphicEffectLightningUnit = 0x15,
    /// dbgfxeffects.dat record 84 (file order = sEffects index; +0x68 = 84) name 'Rain'; its sites call EEGraphicEffect::Get
    kGraphicEffectRain = 0x54,
    /// dbgfxeffects.dat record 86 (file order = sEffects index; +0x68 = 86) name 'Horse Breath (units)'; its sites call EEGraphicEffect::Get
    kGraphicEffectHorseBreathUnits = 0x56,
    /// dbgfxeffects.dat record 94 (file order = sEffects index; +0x68 = 94) name 'Priest Conversion Indiator'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestConversionIndiator = 0x5e,
    /// dbgfxeffects.dat record 96 (file order = sEffects index; +0x68 = 96) name 'Basic marching line'; its sites call EEGraphicEffect::Get
    kGraphicEffectBasicMarchingLine = 0x60,
    /// dbgfxeffects.dat record 98 (file order = sEffects index; +0x68 = 98) name 'Waypoint Indicator'; its sites call EEGraphicEffect::Get
    kGraphicEffectWaypointIndicator = 0x62,
    /// dbgfxeffects.dat record 171 (file order = sEffects index; +0x68 = 171) name 'Dust - Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectDustEffect = 0xab,
    /// dbgfxeffects.dat record 172 (file order = sEffects index; +0x68 = 172) name 'Snow'; its sites call EEGraphicEffect::Get
    kGraphicEffectSnow = 0xac,
    /// dbgfxeffects.dat record 180 (file order = sEffects index; +0x68 = 180) name 'Calamity - Teleport In'; its sites call EEGraphicEffect::Get
    kGraphicEffectCalamityTeleportIn = 0xb4,
    /// dbgfxeffects.dat record 184 (file order = sEffects index; +0x68 = 184) name 'Command Confirmation - Move'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMove = 0xb8,
    /// dbgfxeffects.dat record 185 (file order = sEffects index; +0x68 = 185) name 'Command Confirmation - Attack'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationAttack = 0xb9,
    /// dbgfxeffects.dat record 186 (file order = sEffects index; +0x68 = 186) name 'Command Confirmation - Gather / Other'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationGatherOther = 0xba,
    /// dbgfxeffects.dat record 188 (file order = sEffects index; +0x68 = 188) name 'Disband Unit (emitter)'; its sites call EEGraphicEffect::Get
    kGraphicEffectDisbandUnitEmitter = 0xbc,
    /// dbgfxeffects.dat record 207 (file order = sEffects index; +0x68 = 207) name 'Aircraft Explosion'; its sites call EEGraphicEffect::Get
    kGraphicEffectAircraftExplosion = 0xcf,
    /// dbgfxeffects.dat record 209 (file order = sEffects index; +0x68 = 209) name 'Command Confirmation - Mouse Click No Unit'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMouseClickNoUnit = 0xd1,
    /// dbgfxeffects.dat record 210 (file order = sEffects index; +0x68 = 210) name 'Rally Flag - Building'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagBuilding = 0xd2,
    /// dbgfxeffects.dat record 211 (file order = sEffects index; +0x68 = 211) name 'Command Confirmation - Mouse Click Unit'; its sites call EEGraphicEffect::Get
    kGraphicEffectCommandConfirmationMouseClickUnit = 0xd3,
    /// dbgfxeffects.dat record 212 (file order = sEffects index; +0x68 = 212) name 'Rally Flag - Fighter'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagFighter = 0xd4,
    /// dbgfxeffects.dat record 213 (file order = sEffects index; +0x68 = 213) name 'Rally Flag - Bomber'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagBomber = 0xd5,
    /// dbgfxeffects.dat record 214 (file order = sEffects index; +0x68 = 214) name 'Rally Flag - Atomic Bomber'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagAtomicBomber = 0xd6,
    /// dbgfxeffects.dat record 236 (file order = sEffects index; +0x68 = 236) name 'Disintegration (area effect -intermediate)'; its sites call EEGraphicEffect::Get
    kGraphicEffectDisintegrationAreaEffectIntermediate = 0xec,
    /// dbgfxeffects.dat record 245 (file order = sEffects index; +0x68 = 245) name 'Rally Flag - Paratrooper'; its sites call EEGraphicEffect::Get
    kGraphicEffectRallyFlagParatrooper = 0xf5,
    /// dbgfxeffects.dat record 247 (file order = sEffects index; +0x68 = 247) name 'Priest Tower Swirl Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestTowerSwirlEffect = 0xf7,
    /// dbgfxeffects.dat record 253 (file order = sEffects index; +0x68 = 253) name 'Fanaticism Effect'; its sites call EEGraphicEffect::Get
    kGraphicEffectFanaticismEffect = 0xfd,
    /// dbgfxeffects.dat record 256 (file order = sEffects index; +0x68 = 256) name 'Priest Tower Smoke Generator'; its sites call EEGraphicEffect::Get
    kGraphicEffectPriestTowerSmokeGenerator = 0x100,
    /// dbgfxeffects.dat record 271 (file order = sEffects index; +0x68 = 271) name 'Space Citizen Mining Beam'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceCitizenMiningBeam = 0x10f,
    /// dbgfxeffects.dat record 274 (file order = sEffects index; +0x68 = 274) name 'Space Gate Energy Wall'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceGateEnergyWall = 0x112,
    /// dbgfxeffects.dat record 275 (file order = sEffects index; +0x68 = 275) name 'Space Wall Energy Wall'; its sites call EEGraphicEffect::Get
    kGraphicEffectSpaceWallEnergyWall = 0x113,
    /// dbgfxeffects.dat record 284 (file order = sEffects index; +0x68 = 284) name 'Market Ticket Tape'; its sites call EEGraphicEffect::Get
    kGraphicEffectMarketTicketTape = 0x11c,
}

/// dbsounds.dat sounds by table index (EESoundObject::Get; LoadTable 0x4da680 fills gSoundObjects in file order)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EESoundID {
    /// dbsounds.dat record 8 (file order = gSoundObjects index; +0x68 = 8) name 'Flare' (sounds\flare-8.wav); its sites call EESoundObject::Get
    kSoundFlare = 0x8,
    /// dbsounds.dat record 14 (file order = gSoundObjects index; +0x68 = 14) name 'Cash Register' (sounds\register-8.wav); its sites call EESoundObject::Get
    kSoundCashRegister = 0xe,
    /// dbsounds.dat record 134 (file order = gSoundObjects index; +0x68 = 134) name 'Garrison' (sounds\garrison2.wav); its sites call EESoundObject::Get
    kSoundGarrison = 0x86,
    /// dbsounds.dat record 374 (file order = gSoundObjects index; +0x68 = 374) name 'Lightning' (sounds\Slightning2-8.wav); its sites call EESoundObject::Get
    kSoundLightning = 0x176,
    /// dbsounds.dat record 387 (file order = gSoundObjects index; +0x68 = 387) name 'Transport Load' (sounds\Transport-8.wav); its sites call EESoundObject::Get
    kSoundTransportLoad = 0x183,
    /// dbsounds.dat record 397 (file order = gSoundObjects index; +0x68 = 397) name 'Under Attack' (sounds\dunderattack1.wav); its sites call EESoundObject::Get
    kSoundUnderAttack = 0x18d,
    /// dbsounds.dat record 439 (file order = gSoundObjects index; +0x68 = 439) name 'Trumpets' (sounds\fanfare-8.wav); its sites call EESoundObject::Get
    kSoundTrumpets = 0x1b7,
    /// dbsounds.dat record 442 (file order = gSoundObjects index; +0x68 = 442) name 'Thunder 2' (sounds\thunder-8.wav); its sites call EESoundObject::Get
    kSoundThunder2 = 0x1ba,
    /// dbsounds.dat record 481 (file order = gSoundObjects index; +0x68 = 481) name 'Gate' (sounds\gate-8.wav); its sites call EESoundObject::Get
    kSoundGate = 0x1e1,
    /// dbsounds.dat record 519 (file order = gSoundObjects index; +0x68 = 519) name 'Button 1' (sounds\button1.wav); its sites call EESoundObject::Get
    kSoundButton1 = 0x207,
    /// dbsounds.dat record 520 (file order = gSoundObjects index; +0x68 = 520) name 'Button 2' (sounds\button2.wav); its sites call EESoundObject::Get
    kSoundButton2 = 0x208,
    /// dbsounds.dat record 936 (file order = gSoundObjects index; +0x68 = 936) name 'Clock Tick' (sounds\clock-8.wav); its sites call EESoundObject::Get
    kSoundClockTick = 0x3a8,
    /// dbsounds.dat record 942 (file order = gSoundObjects index; +0x68 = 942) name 'Incoming Chat Message' (sounds\message-8.wav); its sites call EESoundObject::Get
    kSoundIncomingChatMessage = 0x3ae,
    /// dbsounds.dat record 957 (file order = gSoundObjects index; +0x68 = 957) name 'Computer Attack' (sounds\compattack.wav); its sites call EESoundObject::Get
    kSoundComputerAttack = 0x3bd,
    /// dbsounds.dat record 958 (file order = gSoundObjects index; +0x68 = 958) name 'Wonder Clock' (sounds\wonderclock.wav); its sites call EESoundObject::Get
    kSoundWonderClock = 0x3be,
    /// dbsounds.dat record 962 (file order = gSoundObjects index; +0x68 = 962) name 'Hot Key Alarm' (sounds\hotkeyalarm.wav); its sites call EESoundObject::Get
    kSoundHotKeyAlarm = 0x3c2,
    /// dbsounds.dat record 1049 (file order = gSoundObjects index; +0x68 = 1049) name 'Insurance collected' (sounds\insurance-8.wav); its sites call EESoundObject::Get
    kSoundInsuranceCollected = 0x419,
    /// dbsounds.dat record 1052 (file order = gSoundObjects index; +0x68 = 1052) name 'Priest Tower Convert' (sounds\ptower_16.wav); its sites call EESoundObject::Get
    kSoundPriestTowerConvert = 0x41c,
    /// dbsounds.dat record 1058 (file order = gSoundObjects index; +0x68 = 1058) name 'Slavery' (sounds\slavery.wav); its sites call EESoundObject::Get
    kSoundSlavery = 0x422,
}

/// Unit-class improvement kinds (the EEUnitClassImprovement switches, EEUIBUpgrade::mAttribute): names from
/// EEUnitClassImprovement::GetName (Language.rc strings); the armour kinds as setrilo unit_class_upgrade.h UpgradeKind.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEImprovementKind {
    /// EEUnitClassImprovement::GetName case 0: IDS_AREA_EFFECT
    kImprovementAreaEffect = 0,
    /// EEUnitClassImprovement::GetName case 1: IDS_IMPROVEMENT_ATTACK
    kImprovementAttack = 1,
    /// EEUnitClassImprovement::GetName case 2: IDS_ARROW
    kImprovementArrowArmor = 2,
    /// EEUnitClassImprovement::GetName case 3: IDS_GUN
    kImprovementGunArmor = 3,
    /// EEUnitClassImprovement::GetName case 4: IDS_LASER
    kImprovementLaserArmor = 4,
    /// EEUnitClassImprovement::GetName case 5: IDS_MISSILE
    kImprovementMissileArmor = 5,
    /// EEUnitClassImprovement::GetName case 6: IDS_PIERCE
    kImprovementPierceArmor = 6,
    /// EEUnitClassImprovement::GetName case 7: IDS_SHOCK
    kImprovementShockArmor = 7,
    /// EEUnitClassImprovement::GetName case 8: IDS_IMPROVEMENT_FLIGHT_TIME or IDS_IMPROVEMENT_POWER by mFlightTime
    kImprovementFlightTimeOrPower = 8,
    /// EEUnitClassImprovement::GetName case 9: IDS_IMPROVEMENT_HIT_POINTS
    kImprovementHitPoints = 9,
    /// EEUnitClassImprovement::GetName case 10: IDS_LEADERSHIP
    kImprovementLeadership = 10,
    /// EEUnitClassImprovement::GetName case 11: IDS_IMPROVEMENT_RANGE
    kImprovementRange = 11,
    /// EEUnitClassImprovement::GetName case 12: IDS_IMPROVEMENT_REPAIR
    kImprovementRepair = 12,
    /// EEUnitClassImprovement::GetName case 14: IDS_IMPROVEMENT_SPEED
    kImprovementSpeed = 14,
    /// EEUnitClassImprovement::GetName case 15: IDS_TRANSPORT
    kImprovementTransport = 15,
}

/// FSPersistent::mTagID of the saved-game chunks (EEGameStateLoader::RestorePersistents switches on it; each
/// FSPersistent subclass passes its tag): names from the class (ctor, dynamic_cast); setrilo game_load.h SavedChunkType
/// agrees.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEPersistentTag {
    /// RTTI EEPChecksum: tag 1
    kPersistentChecksum = 1,
    /// EEPTGeneric<unsigned long> (setrilo kChunkSeed): tag 2
    kPersistentSeed = 2,
    /// EEPTRestorable<GE3DPoint> (setrilo kChunkCamera): tag 3
    kPersistentCamera = 3,
    /// EEPTPRestorable<EEMap>: tag 4
    kPersistentMap = 4,
    /// RTTI EEPPlayerHelper: tag 5
    kPersistentPlayers = 5,
    /// EEPTPRestorable<EEScenarioManager>: tag 6
    kPersistentScenario = 6,
    /// RTTI EEPWorldHelper: tag 7
    kPersistentWorld = 7,
    /// RTTI EEPMultiplayerHelper (restored as nothing): tag 8
    kPersistentMultiplayer = 8,
    /// EEPTPRestorable<EEEffectManager>: tag 10
    kPersistentEffects = 10,
    /// RTTI EEPCalamityHelper: tag 11
    kPersistentCalamities = 11,
    /// EEPTPRestorable<EEEnvironmentManager>: tag 12
    kPersistentEnvironment = 12,
}

/// EEUnit/EEUnitClass mResourceType (also the player's resource index, EEPlayer::AddResource).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEResourceType {
    /// EEUnit::Init/ResetEEUnit store 0; `!= 0` tests 'is a resource' (EEUCResource::SimpleUnitCreated sets the amount only then) (provisional name)
    kResourceNone = 0,
    /// EEComplexUnit::CanTake 0x5f6a1a: a storage of type 1 takes every resource (mResourceType == 1 || == inValue); CanGather/status box exclude 0 and 1 (provisional name)
    kResourceAny = 1,
    /// the player's resource index 2..6 (mResourceCaps[7]; EEWorld::RecordStatistics pairs [2]/[3]/[4]/[5]/[6] with the starting food/wood/stone/gold/iron amounts); same ids as EEUnit::mResourceType (provisional name)
    kResourceFood = 2,
    /// the player's resource index 2..6 (mResourceCaps[7]; EEWorld::RecordStatistics pairs [2]/[3]/[4]/[5]/[6] with the starting food/wood/stone/gold/iron amounts); same ids as EEUnit::mResourceType (provisional name)
    kResourceWood = 3,
    /// the player's resource index 2..6 (mResourceCaps[7]; EEWorld::RecordStatistics pairs [2]/[3]/[4]/[5]/[6] with the starting food/wood/stone/gold/iron amounts); same ids as EEUnit::mResourceType (provisional name)
    kResourceStone = 4,
    /// the player's resource index 2..6 (mResourceCaps[7]; EEWorld::RecordStatistics pairs [2]/[3]/[4]/[5]/[6] with the starting food/wood/stone/gold/iron amounts); same ids as EEUnit::mResourceType (provisional name)
    kResourceGold = 5,
    /// the player's resource index 2..6 (mResourceCaps[7]; EEWorld::RecordStatistics pairs [2]/[3]/[4]/[5]/[6] with the starting food/wood/stone/gold/iron amounts); same ids as EEUnit::mResourceType (provisional name)
    kResourceIron = 6,
}

/// EEEMStorm::mType: EEEMStorm::GetTypeName names each type (names from its strings; setrilo environment_manager.h
/// StormType agrees for 0..4).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEStormType {
    /// EEEMStorm::GetTypeName case 0: "Unknown"
    kStormUnknown = 0,
    /// EEEMStorm::GetTypeName case 1: "Rain"
    kStormRain = 1,
    /// EEEMStorm::GetTypeName case 2: "Snow"
    kStormSnow = 2,
    /// EEEMStorm::GetTypeName case 3: "Hurricane"
    kStormHurricane = 3,
    /// EEEMStorm::GetTypeName case 4: "Blizzard"
    kStormBlizzard = 4,
}

/// EEAIArea::mAreaType (+0x64): the area class each creator stores; EECPILocation::mType uses the same ids (Update 0x480a57 re-finds the area of that class at the tile)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAIAreaKind {
    /// the EEAIArea ctor 0x4110cb stores 0; EECPILocation::Update 0x480a57 stores 0 when no area is found (provisional name)
    kAIAreaNone = 0,
    /// RTTI EETown: the EETown ctor stores 5; EECPILocation::Update: the player's town containing the tile
    kAIAreaTown = 5,
    /// RTTI EEAIHill: EEAIAStarHillGoal::CreateHill stores 7; EECPILocation::Update: GetHillAt; EEEMStorm::Begin dynamic_casts GetRandomArea(7) to EEAIHill
    kAIAreaHill = 7,
    /// RTTI EEAIRectangleArea: EEAIContinentManager::CreateScenarioAreas stores 8; EECPILocation::Update: GetRectangleAreaAt
    kAIAreaRectangleArea = 8,
    /// RTTI EEHerd: EEHerd::Clear stores 9
    kAIAreaHerd = 9,
}

/// EEAIContinent::mCategory (+0x11a): the terrain class of a continent; EEAIAStarContinentGoal::AcceptTile 0x41575c takes it from the fill's first tile (also the path-area/sector goals' copies and the manager's category filters)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAIContinentCategory {
    /// unset (EEAIAStarContinentGoal ctor 0x4155c0 / CreateContinent 0x4156b2 reset; AcceptTile 0x41575c classifies the first tile while 0); as a filter any category (GetLargestContinent 0x422952; GetRandomContinent 0x422dc8: any but the map edge) (provisional name)
    kContinentCategoryNone = 0,
    /// setrilo ai_continents.h:215 '1 land'; AcceptTile 0x41575c: any terrain but 1, 9, 0xc
    kContinentCategoryLand = 1,
    /// setrilo ai_continents.h:215 '2 deep water'; AcceptTile 0x41575c: terrain 1; CreateContinents counts it into mNumWaterTiles
    kContinentCategoryWater = 2,
    /// setrilo ai_continents.h:215 '3 cliff'; AcceptTile 0x41575c: terrain 9 (the cliff terrain EEMap::UpdateCliffs sets)
    kContinentCategoryCliff = 3,
    /// AcceptTile 0x41575c: EEMap::IsEdgeTile tiles (setrilo's base-game rim); EEHerd 0x4a4aa2 leaves the map through a random one (provisional name)
    kContinentCategoryMapEdge = 5,
}

/// The region ids a path area carries (EEAIPathArea +0x84..+0x8a): EEAIContinentManager::CreatePathAreaKind 0x4204ea grows one kind per pass (EEAIAStarPathAreaGoal::mMode); GetContinent 0x422b86, GetAdjacentAreaID 0x421a18 and EEAIContinent::GetShoreInRegions select the id by it
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAIPathKind {
    /// setrilo ai_continents.h:153; GetContinent 0x422b86: mLandMassID; CreatePathAreaKind 0x4204ea: not terrain 1 / 0xc
    kAIPathLandMass = 2,
    /// GetContinent 0x422b86: mW84; CreatePathAreaKind 0x4204ea and GetAdjacentAreaID 0x421a18: tiles not terrain 0xc (and not on the map's diagonal edges) (provisional name)
    kAIPathNonSpace = 4,
}

/// The AI's kind of a tile and of the path area / sub-sector grown from it (EEAIGetTileKind 0x41ed2c, EEAIAStarPathAreaGoal::AcceptTile, EEAIAStarPathSectorGoal::ClassifyTile; EEAIPathArea::mKind, EEAIPathSubSector::mKind); shore / near-shore by the manager's tile value (UpdateTileValues 0x4231d7: < 0 a shore tile, > 0 next to shore tiles)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAITileKind {
    /// setrilo ai_continents.h:149 '4 forest'; EEAIAStarPathAreaGoal::CreateAreas 0x417510 makes an EEAIForest for it; EEAIPathSubSector::GetBlockingUnit 0x42bf45: a tree
    kAITileForest = 4,
}

/// Tile terrain types (RTSTile::mTerrainType = dbterrain.dat record +0x78, plus the water kinds EETile::SetTerrainType derives from the height)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EETerrainType {
    /// EETile::SetTerrainType 0x5214bf: a tile below -0.75 elevation unit becomes 1; setrilo ai_astar.h '2 deep water for type 1'
    kTerrainDeepWater = 1,
    /// dbterrain.dat type 5 (+0x78) = Ice, Snow, Snow - Tufts/Grass Patches/Dirt Patches, Mars glaciers/polar cap; EEUnitClass 0x638ce7 shows the horse breath effect at night or on type 5 (provisional name)
    kTerrainSnow = 5,
    /// dbterrain.dat: the records of type 9 (+0x78) are exactly the cliff textures (Cliff, Cliffwinter, CliffRock); EEMap::UpdateCliffs sets 9 (setrilo rts_map.h)
    kTerrainCliff = 9,
    /// EETile::SetTerrainType 0x5214bf: a tile under water with fewer than 3 dry corners becomes 10 (3+ dry corners keep the land type); setrilo ai_continents.h 'a water tile (types 1, 10)', shallows join land continents (provisional name)
    kTerrainShallowWater = 10,
    /// dbterrain.dat: 'Space' is the only record of type 12 (+0x78); EETile::SetTerrainType 0x5214bf SetIsSpace(type == 12)
    kTerrainSpace = 12,
}

/// A calamity class's enum (dbcalamity.dat +0x84; EECalamityClass::mClassEnum). Enum and enumerator names are
/// provisional: the
/// game's callers of GetCalamityClass(const&) pass converted temporaries (`x - 6`, the literal 7), so the parameter
/// is a distinct type, and EECalamity::Load's long picks the unsigned long overload. Values from the class switch in
/// LoadCalamityClasses (1, 2, 5, 6, 11, 12 have no class).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EECalamityEnum {
    kCalamityEarthquake = 3,
    kCalamityHurricane = 4,
    kCalamityPlague = 7,
    kCalamityLeprosy = 8,
    kCalamityVolcano = 9,
    kCalamityFire = 10,
    kCalamityMorale = 13,
    kCalamityTimeWarp = 14,
    kCalamityTeleport = 15,
    kCalamityAreaEffect = 16,
    kCalamityAEExtraDamage = 17,
    kCalamitySpecialAttack = 18,
    kCalamityAEMindControl = 19,
    kCalamityConscription = 20,
    kCalamityLogicBomb = 21,
    kCalamityDropObject = 22,
    kCalamityCloaking = 23,
    kCalamityMeteorStorm = 24,
    kCalamityLineOfSight = 25,
    kCalamitySpecialLocation = 26,
}

/// How a hit's damage is formed (EEUnitClass::ComputeDamage)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEDamageForm {
    /// a unit's or projectile's hit (EEUnitClass::ApplyHit)
    kDamageDirect = 0,
    /// splash damage around it (EEUnitClass::ApplyAreaDamage)
    kDamageSplash = 1,
    /// a moving unit pushing a stopped enemy (EEPhysicsModel)
    kDamageTrample = 2,
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The values the data and the saved games fix (dbgfxeffects and dbsounds rows, the save tags, the resources).
    #[test]
    fn values() {
        assert_eq!(EEEffectKind::kEffect3DIcon as i32, 2);
        assert_eq!(EEEffectKind::kEffect3DEnergyWave as i32, 0x47);
        assert_eq!(EEGraphicEffectID::kGraphicEffectMarketTicketTape as i32, 284);
        assert_eq!(EESoundID::kSoundSlavery as i32, 1058);
        assert_eq!(EEImprovementKind::kImprovementTransport as i32, 15);
        assert_eq!(EEPersistentTag::kPersistentEnvironment as i32, 12);
        assert_eq!(EEResourceType::kResourceIron as i32, 6);
        assert_eq!(EEStormType::kStormBlizzard as i32, 4);
        assert_eq!(EEAIAreaKind::kAIAreaHerd as i32, 9);
        assert_eq!(EEAIContinentCategory::kContinentCategoryMapEdge as i32, 5);
        assert_eq!(EEAIPathKind::kAIPathNonSpace as i32, 4);
        assert_eq!(EEAITileKind::kAITileForest as i32, 4);
        assert_eq!(EETerrainType::kTerrainSpace as i32, 12);
        assert_eq!(EECalamityEnum::kCalamitySpecialLocation as i32, 26);
        assert_eq!(EEDamageForm::kDamageTrample as i32, 2);
    }
}
