// EERenderObject.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EERenderObject.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EERENDEROBJECT_H
#define EE_EERENDEROBJECT_H

#include <set>                          // (its includers' std::set)
#include "ee/RTSGraphics.h"

class RTSRenderObject;
class GE3DPoint;
class GETransformation;
class EEUnit;
class EETile;
class EEDoppelganger;

void GetFogFactor(const GE3DPoint& inLocation, unsigned long inFog, float& outValue);
void SetTerrainLighting();                  // 0x10f833: the effects' lighting set-up (EEE3DEarthquake::DrawPass2)
void SetLightingOff();                      // 0x10f6de: lighting set-up (EEROWater::RenderScene)
void SetSceneFog();                         // 0x10f60f: lighting set-up (EEROWater::RenderScene)

// the camera plane the fog distance is measured from (0x4e9008; declared in EEEffect.cpp, read by GetFogFactor)
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
    void FillRenderLists();                 // 0x10eabe (EEROGroundShadows::FrameStarting; provisional name)
    void Fn10E8D1();                        // 0x10e8d1 (EEROTerrain::FrameStarting; provisional name)
    // FillRenderLists' per-unit helpers (IsUnitDrawn 0x10b3b3, DrawGroundCircle 0x10bba4, FillGhostRecords 0x10c952,
    // FillObject, FillUnit, the doppelgangers' fills, GetUnitOpacity 0x10f412): gamecpp render_object.rs
};

#endif

