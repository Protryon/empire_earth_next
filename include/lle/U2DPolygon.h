// Low-Level Engine U2DPolygon header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOLYGON_H
#define LLE_U2DPOLYGON_H

#include "lle/U2DRectangle.h"

// Engine templates in the original (GETerrainMesh.cpp instantiated U2DPolygon<float>/U2DConvexHull<float>: vtables
// 0x10098010, 0x1009804c; the exe U2DPolygon<long>/U2DConvexHull<long>: EEAIArea::mHull, U2DPolygon<long> ctor
// 0x11251, Contains 0xd16e).
//
// Port: both are gone. The float instantiation went with the terrain volume's hull (ee_ge hull.rs /
// terrain_volume.rs); the long one's only user was EEAIArea, whose hull is now its Rust record's (ee_astar area.rs,
// ee/EEAIArea.h; the algorithms hull.rs over ee_ge hull.rs; docs/RUST_PLAN.md R5b.1, branch rust-astar-areas1).
// What is left is the geometry capture's scope.
#ifdef LLE_EXE_HEADERS
#include <vector>
#include "rust/sim/app.h"

// New (port), TEST ONLY (EE_SIM_CAPTURE, tools/sim_capture.py; the records are Rust's: ee_sim app capture_record.rs): the
// records of the hulls' geometry (HULL: kind 1 SetPoints, 2 GetClosestOutlinePoint, 4 GetClosestPoint, 6 Contains;
// ee/EEAIArea.h now records them; kinds 3 GetClosestPoints and 7 GetScanLines had no caller) and of the areas' (AREA).
// Only the outermost captured call of a thread is recorded: a scope made while another is alive is not outer, so the
// calls an algorithm makes inside another (and that leave the C++ with it) never reach the digests.
class EEGeomCaptureScope {
public:
    // mOuter: the capture is on and no other scope is alive on this thread
    EEGeomCaptureScope() : mOuter(ee_sim_capture_geom_enter()) {}
    ~EEGeomCaptureScope() { ee_sim_capture_geom_leave(); }
    bool mOuter;
};
#endif

#endif
