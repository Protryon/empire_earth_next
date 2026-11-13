// C:\EEX\Empire Earth\Graphics Thread\EEE2DBattlefieldAmbient.h (RECONSTRUCTION; file name inferred). A ground decal
// (crater, scorch) left by a battle: a flat square of random size at a point, fading out
// over its duration (RTTI EEE2DBattlefieldAmbient > EEEffect2D; vtable 0x43f94c; EEE2DBattlefieldAmbient.cpp
// 0xde331..0xdf57c; sizeof 0xf4). Layout from the ctors 0xde74f/0xdea27, the dtor 0xdea76, Virtual24 and Load/Save;
// names are provisional. Requires ee/EEEffect2D.h, lle/GE3DPoint.h, lle/GETransformation.h.
#ifndef EE_EEE2DBATTLEFIELDAMBIENT_H
#define EE_EEE2DBATTLEFIELDAMBIENT_H

#include "ee/EEEffect2D.h"

class EEGraphicObject;
class EEComplexUnit;

// 0xde374 (cdecl): the class +0xcc effect's decals around inLocation (EEPMAirplane's crash; also declared by
// ee/EEPhysicsModel.h) (provisional name)
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation);
// 0xde4a9 (cdecl): one decal of the class +0xd4 effect under the unit (offset for unit types 0x124/0x175)
// Provisional name
void EEPlaceTrackDecal(EEComplexUnit* inUnit);

class EEE2DBattlefieldAmbient : public EEEffect2D {
public:
    static const UString sData;             // "EEE2DBattlefieldAmbient-Data" (0x4e87e8; chain 0xde331, guard 0x4e87e0)

    // 0xde74f (kind 0x32, no unit): the decal's square of random size at inLocation, laid on the terrain; flat
    // decals get a random rotation
    EEE2DBattlefieldAmbient(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect);
    EEE2DBattlefieldAmbient();              // 0xdea27 (the load-time ctor)
    virtual ~EEE2DBattlefieldAmbient();     // +0x00 0xdea76 (??_G 0xdea0b)
    virtual long DrawPass2();               // +0x08 0xdebba (draws the decal)
    virtual long DrawPass4();               // +0x10 (`return 0`: ICF 0xfca98)
    virtual long DrawPass5();               // +0x14 (0xfca98)
    virtual long DrawPass6();               // +0x18 (0xfca98)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xdeab2
    virtual void PrepareQueue();            // +0x2c (empty: ICF 0xe123d)
    virtual long SetUp();                   // +0x30 0xdeb58 (loads the graphic object, places the effect)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x38 0xdec7f
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x40 0xded67
    virtual long Step();                    // +0x4c 0xdee2b (ages; 7 when done)

    // the record's (ee_gfx effect_state.rs `EeGfxEffectAmbient`); the constructor's draws and corners, the tick,
    // load and save are Rust's
    uint32_t& mType = this->mRecord->ambient.kind;   // +0x54 (the effect's int param 1; saved)
    uint32_t& mOnSlope = this->mRecord->ambient.on_slope;   // +0x58 (1: the corners are not level; saved)
    GE3DPoint& mCenter = *reinterpret_cast<GE3DPoint*>(&this->mRecord->ambient.center);   // +0x5c (saved)
    GE3DPoint (&mCorners)[4] = *reinterpret_cast<GE3DPoint(*)[4]>(&this->mRecord->ambient.corners);   // +0x68
    GETransformation& mTransformation = *reinterpret_cast<GETransformation*>(&this->mRecord->ambient.transformation);
    EEGraphicObject* mGraphicObject;        // +0xd8 (UnloadMedia in the dtor)
    float& mAge = this->mRecord->ambient.age;   // +0xdc (0..1 over the effect's duration; saved)
    float mDrawAge;                         // +0xe0 (Virtual24's copy of mAge)
    float& mRotation = this->mRecord->ambient.rotation;   // +0xe4 (saved)
    float& mSize = this->mRecord->ambient.size;   // +0xe8 (saved)
    float mDrawSize;                        // +0xec (Virtual24: grows during the first tenth of the age)
    float& mHidden = this->mRecord->ambient.hidden;   // +0xf0 (1.0: off the map or on a slope)
};

#endif
