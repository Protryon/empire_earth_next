// The C++ seam of the rasterizer's draw paths moved to Rust (rust/crates/ee_dx7/src/draw*.rs): the per-screen pinned
// record and the access DX7Rasterizer's own forwarders and the ee_dx7cpp_* functions need.
#ifndef DX7_DX7DRAWSEAM_H
#define DX7_DX7DRAWSEAM_H

#include "rust/ee_rust.h"
#include "dx7/DX7Rasterizer.h"

// The rasterizer's protected members, for the forwarders and the ee_dx7cpp_* seam (a view of the same object: no data
// of its own).
struct DX7DrawAccess : public DX7Rasterizer {
    using DX7Rasterizer::mDX7ScreenStates;
    using DX7Rasterizer::Set2DMode;
    using DX7Rasterizer::Set3DMode;
    using DX7Rasterizer::LockVB;
    using DX7Rasterizer::UnlockVB;
    using GERasterizer::mDrawingAccess;
    using GERasterizer::mScreenStates;
    using GERasterizer::mComputedAmbientColor;
    using GERasterizer::mWaterRipple0;
    using GERasterizer::mWaterRipple1;
    using GERasterizer::mWaterRipple2;
};

// The screen's pinned DX7 state record (DX7Trampolines.cpp pins EeDx7ScreenState to DX7RScreenState).
inline EeDx7ScreenState* DX7DrawState(DX7Rasterizer* inRasterizer, unsigned long inScreen)
{
    return reinterpret_cast<EeDx7ScreenState*>(static_cast<DX7DrawAccess*>(inRasterizer)->mDX7ScreenStates[inScreen]);
}

#endif
