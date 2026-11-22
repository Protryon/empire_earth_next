// DX7Rasterizer (RECONSTRUCTION): the GERasterizer of the DX7 renderer plug-ins, plus the headers every DX7 unit
// includes. Slot order from the vtable 0x10010324 and the exports. The original's sizeof 0x7c (copy ctor 0x10001271):
// GERasterizer 0x68, the per-screen DX7 state vector +0x68, the debug color +0x78.
//
// Whole in Rust (rust/crates/ee_render/src/dx7 rastobj.rs, R4.3 part 13): the per-screen DX7 records, the debug colour, the light
// slots and every method; DX7TnLRasterizer is the object's T&L flavour (inTnL). The class keeps the vtable the game
// calls through GERasterizer and one member; DX7Rasterizer's own four virtuals (GetIsScreenValid, CreateVertexBuffers,
// DrawTerrainMaterial, GetVideoMemory) and the protected LockVB/Set2DMode/Set3DMode/UnlockVB are Rust's.
#ifndef DX7_DX7RASTERIZER_H
#define DX7_DX7RASTERIZER_H

#include <vector>
#include <utility>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/U2DPolygon.h"
#include "lle/GERasterizer.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/GELight.h"
#include "lle/U2DSparseArray.h"

#include "dx7/DX7Common.h"
#include "lle/GEVertexArray.h"

struct EeDx7Rasterizer;                     // rust/ee_dx7.h

class DX7_API DX7Rasterizer : public GERasterizer {
public:
    // port (LLE round 12): the methods are GERasterizer's (one rasterizer: each calls this object's mDX7)
    DX7Rasterizer(const UString& inName, bool inTnL = false);
    virtual ~DX7Rasterizer();

private:
    DX7Rasterizer(const DX7Rasterizer&) = delete;
    DX7Rasterizer& operator=(const DX7Rasterizer&) = delete;
};

#endif
