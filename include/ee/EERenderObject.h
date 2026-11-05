// EERenderObject.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EERenderObject.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EERENDEROBJECT_H
#define EE_EERENDEROBJECT_H

#include "ee/RTSGraphics.h"

class RTSRenderObject;
class GE3DPoint;
class GETransformation;
class EEUnit;
class EETile;
class EEDoppelganger;

// free functions of the render units (cdecl; provisional names; types inferred)
void Fn10F7C8();                            // 0x10f7c8: ambient colour 1, 1, 1 and intensity (the effects' Virtual18)
void Fn11A130(const GE3DPoint& inLocation, unsigned long inFog, float& outValue);   // 0x11a130
void Fn10F71C();                            // 0x10f71c: the effects' lighting set-up (EEE3DLinearProjectile::DrawPass5)
void Fn10F833();                            // 0x10f833: the effects' lighting set-up (EEE3DEarthquake::DrawPass2)
void Fn10FA01();                            // 0x10fa01: the effects' lighting set-up before a model draw (EEE3DDisease::DrawPass5)
void Fn10F6DE();                            // 0x10f6de: lighting set-up (EEROWater::RenderScene)
void Fn10F60F();                            // 0x10f60f: lighting set-up (EEROWater::RenderScene)
void Fn10F772();                            // 0x10f772: lighting set-up (EEROTerrain::RenderScene's clouds)
void Fn10F6C7();                            // 0x10f6c7 (EEROUnits::DrawPlacement)
void Fn10F8DE();                            // 0x10f8de (EEROUnits::DrawObjectModels)
void Fn10FAE8();                            // 0x10fae8 (EEROUnits::DrawObjectModels)
void Fn10FB74();                            // 0x10fb74 (EEROUnits::DrawObjectModels)


// EEEffect.cpp's batch functions (declared here for the render objects)
class GETexture;
void Fn102724(GETexture* inTexture);        // 0x102724: starts both effect batches with inTexture
void Fn102F20();                            // 0x102f20: drops Fn102724's references
void Fn103F64();                            // 0x103f64: draws what the batches hold
// the camera plane the fog distance is measured from (0x4e9008; declared in EEEffect.cpp, read by Fn11A130)
// (provisional name)
class GEQuaternion;
extern long& gHiddenUnitsMode;
extern bool& gDrawGroundShadows;             // 0x5308d4 (the ground shadows are drawn; provisional name)

                       // 0x5308c8 (EEUnitPicker::Reset copies it; provisional name; types inferred)

// vtable 0x441378; bases EERenderObject > RTSRenderObject; sizeof ?; ctor ?; ??_G 0x10ae51; dtor 0x10ae6d
class EERenderObject : public RTSRenderObject {
public:
    EERenderObject();                       // inline
    // implicit dtor: ??_G 0x10ae51 and EEROEffects's dtor call ~RTSRenderObject 0x10ae6d (it stores RTSRenderObject's vptr 0x441390)
    void QueueEffects();                    // 0x10bdd2 (EEROEffects::FrameStarting tail-calls it; provisional name)
    void Fn10C036();                        // 0x10c036 (EEROGroundShadows::FrameStarting; provisional name)
    void FillGhostRecords();                // 0x10c952 (FillRenderLists: the ghost units)
    void FillRenderLists();                 // 0x10eabe (EEROGroundShadows::FrameStarting; provisional name)
    void Fn10E8D1();                        // 0x10e8d1 (EEROTerrain::FrameStarting; provisional name)
#ifdef _SET_
    // FillRenderLists's per-unit helpers (thiscall, `this` unused; provisional names; types inferred)
    bool IsUnitDrawn(EEUnit* inUnit, EETile* inTile, std::set<EEUnit*>& ioDrawn);  // 0x10b3b3: the unit is drawn this frame
#endif
    void DrawGroundCircle(const GETransformation& inLocation, float inRadius, long inMode);  // 0x10bba4: a ground circle
#ifdef EE_EERENDERLISTS_H
    void FillObject(EEUnit* inUnit, EERenderShadow* outObject, EERenderShadowMap& ioMap, EERenderFrameInfo& ioInfo);
    void FillDoppelgangerObject(EEDoppelganger* inDoppelganger, EERenderShadow* outObject, unsigned long& ioCount,
                  EERenderShadowMap& ioMap, unsigned long inMaximum, long inX, long inY, EERenderFrameInfo& ioInfo);
    void FillUnit(EEUnit* inUnit, EERenderUnit* outUnit, EERenderUnitMap& ioMap, EERenderFrameInfo& ioInfo);
    void FillDoppelgangerUnit(EEDoppelganger* inDoppelganger, EERenderUnit* outUnit, unsigned long& ioCount,
                  EERenderUnitMap& ioMap, unsigned long inMaximum, long inX, long inY, EERenderFrameInfo& ioInfo);
    bool GetUnitOpacity(EEUnit* inUnit, float& outOpacity);  // 0x10f412: the unit's opacity this frame
#endif
};

// the effects' render queues, one map per pass (0x4e9418, EERenderObject.cpp's init chain 0x10ac30; [4] holds the water
// effects: EEROWater::RenderScene) (provisional name); declared where ee/EEEffect.h (the typedef) came first
#ifdef EE_EEEFFECT_H
extern EEEffectQueue gEffectQueues[5];
#endif

#endif

