// The battlefield decals' placement helpers (the class EEE2DBattlefieldAmbient whole in Rust on EEE2DRust since R4.4's
// twenty-first round: ee_gfx effect_decal.rs; the helpers are EEE2DRust.cpp's). Requires lle/GE3DPoint.h.
#ifndef EE_EEE2DBATTLEFIELDAMBIENT_H
#define EE_EEE2DBATTLEFIELDAMBIENT_H

class EEComplexUnit;

// 0xde374 (cdecl): the class +0xcc effect's decals around inLocation (EEPMAirplane's crash; also declared by
// ee/EEPhysicsModel.h) (provisional name)
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation);
// 0xde4a9 (cdecl): one decal of the class +0xd4 effect under the unit (offset for unit types 0x124/0x175)
// Provisional name
void EEPlaceTrackDecal(EEComplexUnit* inUnit);

#endif
