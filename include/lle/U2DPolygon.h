// Low-Level Engine U2DPolygon header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOLYGON_H
#define LLE_U2DPOLYGON_H

#include "lle/U2DRectangle.h"

#ifndef LLE_EXE_HEADERS   // the exe has one U2DPolygon-PointCount key chain (0x1160, its first unit), not one per
                          // includer (EECPILocation.cpp has only its own chain)
const UString kU2DPolygonPointCountKey("U2DPolygon-PointCount");
#endif

// Engine templates in the original (GETerrainMesh.cpp instantiated U2DPolygon<float>/U2DConvexHull<float>: vtables
// 0x10098010, 0x1009804c; the exe U2DPolygon<long>/U2DConvexHull<long>: EEAIArea::mHull, U2DPolygon<long> ctor
// 0x11251, Contains 0xd16e).
//
// Port: both are gone. The float instantiation went with the terrain volume's hull (ee_ge hull.rs /
// terrain_volume.rs); the long one's only user was EEAIArea, whose hull is now its Rust record's (ee_astar area.rs,
// ee/EEAIArea.h; the algorithms hull.rs over ee_ge hull.rs; docs/RUST_PLAN.md R5b.1, branch rust-astar-areas1).
// What is left is the seam: the point type and the geometry capture records.
#ifdef LLE_EXE_HEADERS
#include <vector>
#include "rust/ee_astar.h"

// New (port), TEST ONLY (EE_SIM_CAPTURE, tools/sim_capture.py; defined in EEAIContinentCapture.cpp): the records of
// the hulls' geometry (HULL: kind 1 SetPoints, 2 GetClosestOutlinePoint, 4 GetClosestPoint, 6 Contains; ee/EEAIArea.h
// now records them; kinds 3 GetClosestPoints and 7 GetScanLines had no caller) and of the areas' (AREA). Only the
// outermost captured call of a thread is recorded: a scope made while another is alive is not outer, so the calls an
// algorithm makes inside another (and that leave the C++ with it) never reach the digests.
enum {
    kGeomCaptureHull = 0x4c4c5548,          // "HULL"
    kGeomCaptureArea = 0x41455241,          // "AREA"
};
class EEGeomCaptureScope {
public:
    EEGeomCaptureScope();                   // mOuter: the capture is on and no other scope is alive on this thread
    ~EEGeomCaptureScope();
    bool mOuter;
};
void EEGeomCaptureBegin(unsigned int inTag, unsigned int inKind);
void EEGeomCaptureValue(long inValue);
void EEGeomCaptureFloat(float inValue);
void EEGeomCapturePoint(const U2DPoint<long>& inPoint);
// inWhole: the count and every point; else the count and an FNV-1a hash of the points (a hull's identity)
void EEGeomCapturePoints(const U2DPoint<long>* inPoints, size_t inCount, bool inWhole);
inline void EEGeomCapturePoints(const std::vector<U2DPoint<long> >& inPoints, bool inWhole)
{
    EEGeomCapturePoints(inPoints.empty() ? NULL : &inPoints[0], inPoints.size(), inWhole);
}
void EEGeomCaptureCommit();

// New (port): the seam's point type is U2DPoint<long>'s layout, so a point vector passes in place
static_assert(sizeof(U2DPoint<long>) == sizeof(EeHullPoint), "EeHullPoint");
inline const EeHullPoint* U2DHullPoints(const std::vector<U2DPoint<long> >& inPoints)
{
    return inPoints.empty() ? NULL : reinterpret_cast<const EeHullPoint*>(&inPoints[0]);
}
inline EeHullPoint U2DHullPointOf(const U2DPoint<long>& inPoint)
{
    EeHullPoint thePoint;
    thePoint.x = inPoint.mX;
    thePoint.y = inPoint.mY;
    return thePoint;
}
inline U2DPoint<long> U2DHullPoint(EeHullPoint inPoint)
{
    return U2DPoint<long>(inPoint.x, inPoint.y);
}
#endif

#endif
