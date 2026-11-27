// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.cpp, GEModel.cpp, GERasterizer.cpp, GETexture.cpp and the DX7
// plug-in's DX7 Display Plug-ins\Common\DX7Rasterizer.cpp (built with the engine's flags since LLE/UI round 3): the
// Graphics Engine's one unit. Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS.
// Each original unit is a section, headed by its path and keeping its comment (GEBitmap's names its original order).
#include <windows.h>
#include <stddef.h>   // (used directly: kept for the POSIX build)
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GETextureSet.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEPhysicalModel.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/GETextureCoordinate.h"
#include "rust/render/openformats.h"
#include "lle/GERhombus.h"
#include "platform/PlatFont.h"
#include "lle/U2DPoint.h"
#include "platform/PlatSim.h"

// ======== Graphics Engine/GEBitmap.cpp ========
// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.cpp (file name inferred: no __FILE__; alphabetically
// GE3DPoint < GEBitmap < GEBoundingBox, and GEBitmap's own code is the only non-inline code in the range)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1000dd9e..0x10010550. The unit opens with the header-inline members of every class it is the first to include,
// in include order (URandom, GEColor, GESurface, GETextureManager, GETexture, GEWindow, GEScreen, GERectangle,
// GEViewport, GEVertexArray, the U2DPoint/U2DRectangle key statics, GERasterizer, the gone GE2DVertex), then GEBitmap's own
// code (0x1000f3a4) and the map<UString, GETexture*> / vector<GERScreenState*> COMDATs.

// The RNG trace hook (the draws are Rust: ee_base random)
extern "C" void (*gURandomTraceHook)(URandom* inRandom) = NULL;   // port, TEST ONLY (lle/URandom.h)

// ---- GEBitmap (0x1000f3a4..). Vertex order: 0 top left, 1 top right, 2 bottom left, 3 bottom right.
//
// port (R4.3, ninth part): the bitmap is a whole Rust object (ee_ge bmpobj.rs): the constructor's quad, the
// destructor's releases, the corners, the stretched coordinates, the flips, the per-vertex values and SetTexture.
// This section is its shell: the forwarders and the host, which makes each call the original made on another
// object (the rasterizer, the vertex array, the textures). Floats cross the host as their bits.

// ---- the vertex host: the calls GEBitmap, GELine and GERhombus (ee_ge bmpobj.rs, prim.rs) make on other objects

static intptr_t GEBitmapFloatBits(float inValue) { return UFloatBits(inValue); }

// (C linkage: ee_ge game_hosts.rs names it)
extern "C" intptr_t GEVertexHost(void*, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    // (the active rasterizer, its screen count, a texture's Reference and dropped MIP levels: Rust's own, ee_ge
    // bmpobj.rs direct_op)
    case EeGeBmpOp::RasterVertexArray:
        return (intptr_t)((GERasterizer*)inObj)->CreateVertexArray((unsigned long)a, (unsigned long)b);
    // (the vertex arrays' other calls are on their Rust arrays: ee_ge bmpobj.rs vertex_op)
    case EeGeBmpOp::VertexDelete:
        delete ((GEVertexArray*)inObj);
        return 0;
    case EeGeBmpOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    case EeGeBmpOp::TexFrameWidth:
        return GEBitmapFloatBits(((GETexture*)inObj)->GetFrameWidth());
    case EeGeBmpOp::TexFrameHeight:
        return GEBitmapFloatBits(((GETexture*)inObj)->GetFrameHeight());
    case EeGeBmpOp::TexWidth:
        return (intptr_t)((GETexture*)inObj)->GetWidth();
    case EeGeBmpOp::TexHeight:
        return (intptr_t)((GETexture*)inObj)->GetHeight();
    }
    return 0;
}
// ---- end of the vertex host

// ---- the layouts Rust reads in place (ee_ge cppobj.rs, R4.3 part 17): a reference-counted object is its vtable
// pointer, its count and its Rust object; a text, a window and a primitive their Rust object alone; a UString and a
// UWideString their vtable pointer and ee_ustring's record
static_assert(sizeof(GEMaterial) == 3 * sizeof(void*) && sizeof(GEFont) == 3 * sizeof(void*) &&
              sizeof(GETexture) == 3 * sizeof(void*) &&
              sizeof(GEBitmap) == 3 * sizeof(void*) && sizeof(GEFontBitmap) == 3 * sizeof(void*), "cppobj.rs CppCounted");
static_assert(sizeof(GEText) == sizeof(void*) && sizeof(GEWindow) == sizeof(void*) && sizeof(GETextureSet) == sizeof(void*) &&
              offsetof(GERasterizer, mRust) == 0 && offsetof(GERasterizer, mDX7) == sizeof(void*), "cppobj.rs plain");
static_assert(sizeof(UString) == sizeof(void*) + sizeof(EeUString) && sizeof(UWideString) == sizeof(void*) + sizeof(EeUWideString),
              "cppobj.rs CppUString");

// ---- the object host: the calls GEMaterial, GEFont, GEText and GEWindow (ee_ge smallobj.rs) make on other objects
extern "C" intptr_t GEObjectHost(void*, EeGeObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeGeObjOp::StrNew:
        return (intptr_t)new UString;
    case EeGeObjOp::StrCopy:
        return (intptr_t)new UString(*(const UString*)a);
    case EeGeObjOp::StrDelete:
        delete ((UString*)inObj);
        return 0;
    case EeGeObjOp::WStrNew:
        return (intptr_t)new UWideString((unsigned long)a);
    case EeGeObjOp::WStrDelete:
        delete ((UWideString*)inObj);
        return 0;
    case EeGeObjOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    case EeGeObjOp::RefDereference:
        ((UReferenceCounter*)inObj)->Dereference();
        return 0;
    // (the platform fonts, the text functions and the platform window's calls: ee_base's, called by the Rust objects
    // themselves)
    }
    return 0;
}

GEWindow::GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
                   bool inIsFullScreen)
    : mRust(ee_ge_win_new(&GEObjectHost, this, inWindow, inWidth, inHeight, inDepth, inIsFullScreen)) {}
// ---- end of the object host

// (GEBitmap's members forward to its Rust object inline: lle/GEBitmap.h)

// ---- GEFontBitmap (GEFontBitmap.cpp, folded here in R4.3 part 16)

// ---- GEFontBitmap.
//
// port (R4.3, tenth part): the font bitmap is a whole Rust object (ee_ge fbobj.rs): the constructor's glyph rendering,
// the destructor, CreateNewFontTexture, CreateSurfaceAlpha and the two draws. This unit is its shell: the forwarders
// and the host, which makes each call the original made on another object (the rasterizer, the font, the working
// text, the drawing bitmap, the textures, surfaces and screens). Floats cross the host as their bits.

// (the working text's and the drawing bitmap's members and the two draws are the Rust object's own: fbobj.rs
// direct_op, on the 2D draw list)
intptr_t GEFontBitmap::Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeGeFbOp::TextNew:
        return (intptr_t)new GEText((GEFont*)inObj);
    case EeGeFbOp::TextDelete:
        delete ((GEText*)inObj);
        return 0;
    // (TextSetByte, FontRange, TextOps, RasterScreen, RasterGetBlending, TexSurface, FontSetBitmap, the surfaces' and
    // screen's calls, RasterSetBlending: the Rust object's own, fbobj.rs)
    case EeGeFbOp::BmpNew:
        return (intptr_t)new GEBitmap;
    case EeGeFbOp::BmpDereference:
        ((GEBitmap*)inObj)->Dereference();
        return 0;
    case EeGeFbOp::VerifySurface: ESVerify(__FILE__, 228, "theResult == keseOK"); return 0;
    case EeGeFbOp::VerifyTexture: ESVerify(__FILE__, 235, "((GETexture*)inObj) != NULL"); return 0;
    default: return 0;
    }
}

// ======== Graphics Engine/GEModel.cpp ========
// C:\EEX\Low-Level Engine\Graphics Engine\GEModel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001eb9f..0x100206c7: GEModel's ctors, dtor and core members, then its map COMDAT and the UList<GELOD*> pool init
// pair (lle/GELOD.h via GEModel.h).
//
// port (R4.3, ninth part): the model is a whole Rust object (ee_ge modobj.rs): the constructors' fields and
// AllocateInternalData, the destructor's releases, the bounding volumes, normals, transforms and scale removal (over
// model_geom.rs), the frame setters over the sub-models, SetSubModel, SetTexture, SizeOf, SortTagPoints and Verify.
// This unit is its shell: the forwarders and the host, which makes each call the original made on another object
// (the rasterizer, the vertex and polygon arrays, the materials, the texture manager and textures, the manager, the
// sub-models' reference counts, the UStrings).

// the vertex arrays' values as floats (ee_ge_va_get/set; lle/GEVertexArray.h)
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPoint) == 3 * sizeof(float) &&
              sizeof(GEVector) == 3 * sizeof(float) && sizeof(GETextureCoordinate) == 2 * sizeof(float), "vertarr.rs");

// The calls the model makes on other objects (EeGeModOp), in the original's statements; the materials, the
// strings' records, the references taken and the sub-models' objects are read in place (ee_ge cppobj.rs)
static_assert(sizeof(GEModel) == 3 * sizeof(void*), "ee_ge cppobj.rs CppCounted (the others: GEBitmap.cpp)");
intptr_t GEModel::Host(void*, EeGeModOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    // (the active rasterizer, its screen id and the textures' stage filters: Rust's own, ee_ge modobj.rs direct_op)
    case EeGeModOp::RasterVertexArray:
        return (intptr_t)((GERasterizer*)inObj)->CreateVertexArray((unsigned long)a, (unsigned long)b);
    case EeGeModOp::RasterPolygonArray:
        return (intptr_t)((GERasterizer*)inObj)->CreatePolygonArray((unsigned long)a);
    case EeGeModOp::RasterTextureManager:
        return (intptr_t)((GERasterizer*)inObj)->GetScreen((unsigned long)a)->GetTextureManager();
    // (the arrays' other calls are on their Rust arrays: ee_ge modobj.rs array_op)
    case EeGeModOp::VertexDelete:
        delete ((GEVertexArray*)inObj);
        return 0;
    case EeGeModOp::PolygonDelete:
        delete ((GEPolygonArray*)inObj);
        return 0;
    case EeGeModOp::MaterialNew:
        return (intptr_t)new GEMaterial((unsigned long)a, (unsigned long)b);
    case EeGeModOp::MaterialDereference:
        ((GEMaterial*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::MaterialSizeOf:
        return (intptr_t)((GEMaterial*)inObj)->SizeOf();
    case EeGeModOp::ModelNew:
        return (intptr_t)new GEModel((GEModelManager*)inObj);
    case EeGeModOp::ModelBaseDereference:
        ((GEModel*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::ManagerRemove:
        ((GEModelManager*)inObj)->RemoveModel((GEModel*)a);
        return 0;
    case EeGeModOp::StrNew:
        return (intptr_t)new UString;
    case EeGeModOp::StrDelete:
        delete ((UString*)inObj);
        return 0;
    case EeGeModOp::StrArrayNew:
        return (intptr_t)new UString[(size_t)a];
    case EeGeModOp::StrArrayDelete:
        delete[] ((UString*)inObj);
        return 0;
    case EeGeModOp::TexLoad:
        return (intptr_t)((GETextureManager*)inObj)->LoadTexture(*(const UString*)a, *(const GETextureDescription*)b);
    }
    return 0;
}

// ---- GEModel Load (GEModel Load.cpp, folded here in R4.3 part 16)

// (StoreLoaded: ee_ge_mod_store_loaded, from the manager's host below: the arrays allocated from the counts, the model
// stored where the original's chunk readers stored it, the sub-models made and filled)

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)

// ---- GEModelManager (GEModelManager.cpp, folded here in R4.3 part 16)

// ---- the open-format lookups' host. The game's models and textures are glTF and PNG files only (docs/RUST_PLAN.md
// D9, docs/MODEL_FORMATS.md): Rust finds them on the data path and decodes them into what the engine stores
// (ee_openformats find.rs: ee_of_find_model, a model's .glb/.gltf; ee_of_find_texture, GETexture.cpp: a texture's PNG
// frames); the path and its files are C++, reached through this host
intptr_t GEOpenFormatsHost(EeOfFileOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    unsigned long theRead = 0;
    switch (inOp) {
    case EeOfFileOp::PathFind: return ((FSPath*)inObj)->FindFile(UString((const char*)a), *(FSFileSpec**)b);
    case EeOfFileOp::FileOpen: return ((FSFileSpec*)inObj)->Open((unsigned long)a);
    case EeOfFileOp::FileLength: return ((FSFileSpec*)inObj)->GetLength(*(unsigned long*)a);
    case EeOfFileOp::FileRead:
        return ((FSFileSpec*)inObj)->Read((unsigned char*)a, (unsigned long)b, theRead) == kErrorOK ? (intptr_t)theRead : -1;
    case EeOfFileOp::FileClose: return ((FSFileSpec*)inObj)->Close();
    case EeOfFileOp::FileDelete: delete ((FSFileSpec*)inObj); return 0;
    case EeOfFileOp::Verify: ESVerify(__FILE__, __LINE__, (const char*)a); return 0;   // a file that does not decode
    }
    return 0;
}
// ---- end of the open-format lookups' host

// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GEModelManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    // (the lock's calls, the path's Reference, a model's reference count, name, sub-model, size and verification,
    // ModelStoreLoaded and ModelFreeFile: Rust's own, ee_ge manobj.rs direct_op)
    case EeGeManOp::PathDereference:
        ((FSPath*)inObj)->Dereference();
        return 0;
    case EeGeManOp::BaseDereference:
        ((GEModel*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpText: fprintf(*(FSFileSpec*)inObj, "%s", (const char*)a); return 0;
    case EeGeManOp::ModelNew: return (intptr_t)new GEModel(*(const EeGeModelCreate*)a, UString((const char*)b), (GEModelManager*)inOwner);
    case EeGeManOp::ModelNewSub: return (intptr_t)new GEModel(*(const EeGeModelCreate*)a, *(const UString*)b, (GEModelManager*)inOwner);
    case EeGeManOp::ModelFindFile:
        return ee_of_find_model(&GEOpenFormatsHost, inObj, (const char*)a, (const EeGeSharedFrames*)b, (EeGeCemModel**)c);
    case EeGeManOp::ModelNewEmpty:
        return (intptr_t)new GEModel(((GEModelManager*)inOwner));
    default:
        return 0;
    }
}

// ---- GEPhysicalModel (GEPhysicalModel.cpp, folded here in R4.3 part 16)

// kLODFraction (0x10097b78) and ComputeTriangleBox (0x10021ebc), the model against model walk's, are Rust
// (ee_ge physical_walk.rs, physical.rs)

// The walks read the models through the game's table (ee_ge physical_walk.rs, ee_ge_physical_table): the handles are
// the GEModel* the walks were given, read in place (their Rust objects; the exact retry a placed model of its own)
const EeGePhysicalModels* GEPhysicalModelTable() { return ee_ge_physical_table(); }   // the unit picker's walks (ee_gfx pick_walk.rs)

// Returns whether the placed model can be visible in a viewport: bounding sphere against the four planes and the
// clip distances, or its box against the orthographic rectangle
// (the test is Rust: ee_ge physical.rs, model_visible)
bool GEPhysicalModel::Intersects(const GEViewport& inViewport, bool inOrthographic) const
{
    return ee_ge_physical_visible(GERust(this->mLocation), this->mScale,
                                  GERust(this->mModel->GetInternalTransform(this->mFrame)),
                                  GERust(this->mModel->GetCenterPoint()), this->mModel->GetBoundingRadius(this->mFrame),
                                  GERust(this->mModel->GetBoundingBox(this->mFrame)), GERust(inViewport),
                                  inOrthographic);
}

// vector<vector<GEBoundingBox> > / vector<GEBoundingBox> COMDATs (Intersects of two models)

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)

// ---- GETerrainMesh (GETerrainMesh.cpp, folded here in R4.3 part 16)

// (the records the renderer reads through the mesh are the Rust object's: GETerrainMesh.h's typedefs)

// The calls the mesh makes on other objects (EeGeTmOp), in the original's statements
static intptr_t TerrainMeshHost(void*, EeGeTmOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    // (the clock, the fractals, the texture sets' reads and a texture's Reference: Rust's own, ee_ge tmobj.rs direct_op)
    case EeGeTmOp::Quantize:
        return (intptr_t)((const GEVector*)a)->GetQuantizedVersion();
    case EeGeTmOp::SetDelete:
        delete ((GETextureSet*)inObj);
        return 0;
    case EeGeTmOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    }
    return 0;
}

GETerrainMesh::GETerrainMesh() { this->mRust = ee_ge_tm_new(TerrainMeshHost, this); }

// ======== Graphics Engine/GERasterizer.cpp ========
// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// GERasterizer's base state is whole in Rust (rust/crates/ee_render/src/ge rastobj.rs, R4.3 part 12): the members, the screen
// records, the statics and every method below. The class keeps its vtable (the plug-in's base) and one member,
// mRust; the methods forward, and GERasterizerHost makes the calls on other objects Rust asks for.

static_assert(sizeof(EeGeRastFog) == 0x20 && sizeof(GEColor) == sizeof(EeGeRastColor), "fog");
// a placed model as Rust reads it (ee_ge modobj.rs PlacedModel: the location, the model, the scale, the frame)
static_assert(offsetof(GEPhysicalModel, mModel) == sizeof(GETransformation) &&
              offsetof(GEPhysicalModel, mScale) == sizeof(GETransformation) + sizeof(void*), "PlacedModel");

// The calls the Rust rasterizer makes on other objects (EeGeRastOp), in the original's order.
struct GERasterizerHost {
    static intptr_t Call(void* inOwner, EeGeRastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        switch (inOp) {
        case EeGeRastOp::NameNew: return (intptr_t) new UString(*reinterpret_cast<const UString*>(a));
        case EeGeRastOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
        // (the drawing lock's creation and destruction: Rust's own, ee_ge rastobj.rs)
        case EeGeRastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        case EeGeRastOp::FontNew: return (intptr_t) new GEFont("Arial Bold", 9, 0, kFontQualityAntialiased);
        case EeGeRastOp::FontDereference: static_cast<GEFont*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextNew: return (intptr_t) new GEText(reinterpret_cast<GEFont*>(a));
        case EeGeRastOp::TextDelete: delete static_cast<GEText*>(inObj); return 0;
        case EeGeRastOp::ActivateRasterizer: return static_cast<GERasterizer*>(inOwner)->ActivateRasterizer();
        case EeGeRastOp::FontBitmapNew:
            new GEFontBitmap(static_cast<GERasterizer*>(inOwner), reinterpret_cast<GEFont*>(a), (unsigned long)b);
            return 0;
        case EeGeRastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
        case EeGeRastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
        // (the debug text's setters, the models' and placed models' reads and SetClipToPlane: Rust's own, ee_ge
        // rastobj.rs direct_op)
        default: return 0;
        }
    }
};

// The 2D draw list's executor (ee_ge list2d.rs): the texts, the draws the DX7 rasterizer's Rust does not make itself
// (its bitmaps, rhombi and lines it draws itself; a rasterizer without one, the store gate's, draws nothing)
static long GERasterizerDraw2D(void* inRasterizer, unsigned long inScreen, const EeGeDraw2D* inCmd)
{
    switch (inCmd->kind) {
    case EeGeDraw2DKind::TextClipped:
        return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEText*>(inCmd->object), *reinterpret_cast<const GERectangle*>(&inCmd->clip),
                                   inCmd->surface ? static_cast<GESurface*>(inCmd->surface)
                                                  : static_cast<GERasterizer*>(inRasterizer)->GetSecondarySurface(inScreen));
    case EeGeDraw2DKind::Text:
        return static_cast<GERasterizer*>(inRasterizer)->Draw(static_cast<GEText*>(inCmd->object), static_cast<GESurface*>(inCmd->surface));
    default: return 0;
    }
}

extern "C" intptr_t GEVertexHost(void* inOwner, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b);   // GEBitmap.cpp

intptr_t DX7RasterizerHost(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b);   // DX7Rasterizer.cpp

// Creates a rasterizer with its drawing lock and a white-on-black Arial 9 debug text, not yet activated, and no device
// (the model store gate's: loads, no draws)
GERasterizer::GERasterizer(const UString& inName)
    : mRust(ee_ge_rast_new_drawing(&GERasterizerHost::Call, this, &inName, &GERasterizerDraw2D, &GEVertexHost)) {}

// The game's: also its DX7 rasterizer with a dark blue debug colour (inTnL: the T&L rasterizer), which the 2D draw list
// draws on in Rust
GERasterizer::GERasterizer(const UString& inName, bool inTnL)
    : GERasterizer(inName)
{
    this->mDX7 = ee_dx7r_new_direct(&DX7RasterizerHost, this, this->mRust, inTnL);
}

// 0x1002398a: GEText's scalar deleting dtor (see ~GERasterizer). A `delete p` compiles to a ??_G call whenever the
//   dtor body is inline-visible (even when the caller's budget is spent), and to ??1 + ??3 only when it is not;
//   UIForm.cpp has both shapes for the same field (~UIForm ??1+??3, RelayoutForm ??_G).
// 0x10024665..0x100246a7: the template-static init wrapper and three atexit registrars of
//   UList<GELODMetaVertex*>/<GELODPolygon*>/<GELODVertex*>::sNodePool (their $D ctors are GEBoundingBox.cpp's
//   0x10014ed4/0x10014efb/0x10014f22).

// ---- the engine's start-up and pixel formats (Graphics Engine.cpp, folded in R4.3 part 15)

HINSTANCE__* GEScreen::sInstance;

// The renderer linked into the game: the T&L DX7 rasterizer (DX7Rasterizer.cpp; the plug-in boundary went in part 14)
GERasterizer* DX7TnLRasterizer();

// The start-up (GEInitialize, GEShutdown) is Rust's (ee_render ge startup.rs); its calls on the C++ objects
extern "C" {
bool ee_gecpp_fonts_register_builtin() { return PlatFontRegisterBuiltIn() != 0; }
void ee_gecpp_fonts_missing() { ESVerify(__FILE__, __LINE__, "PlatFontRegisterBuiltIn() (the fonts built into the program: PlatFontData.c)"); }
void ee_gecpp_set_instance(void* inInstance) { GEScreen::sInstance = static_cast<HINSTANCE__*>(inInstance); }
void* ee_gecpp_tnl_rasterizer() { return DX7TnLRasterizer(); }
long ee_gecpp_rast_activate(void* inRasterizer) { return static_cast<GERasterizer*>(inRasterizer)->Activate(); }
unsigned long ee_gecpp_rast_screen_count(void* inRasterizer) { return static_cast<GERasterizer*>(inRasterizer)->GetScreenCount(); }
void ee_gecpp_rast_deactivate(void* inRasterizer) { static_cast<GERasterizer*>(inRasterizer)->Deactivate(); }
}

// ======== Graphics Engine/GETexture.cpp ========
// C:\EEX\Low-Level Engine\Graphics Engine\GETexture.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10032bda..0x10033b1f: the U2D key init chains (U2DPoint.h, U2DRectangle.h; no guard byte), then the GETexture
// members: the two private ctors, the dtor, then alphabetical (Demote .. Unlock; SetFrame/SetSurface/Unlock after
// the SetStage* family). The accessors are header-inline (GEBitmap.cpp emits them).
//
// port (R4.3, eighth part): the texture is a whole Rust object (ee_ge texobj_tex.rs): the constructors' fields, the
// destructor's releases, Load's two frame formats, Recreate, the stage setters over every frame, SetFrame,
// SetSurface and the texture pack's logical shift. This unit is its shell: the forwarders and the host, which makes
// each call the original made on another object (its surfaces, screen, frames, manager, the rasterizer, the file).

// the data path's host for ee_of_find_texture (a texture's PNG frames; GEModel.cpp)
intptr_t GEOpenFormatsHost(EeOfFileOp inOp, void* inObj, intptr_t a, intptr_t b);
// the Rust mirrors (texobj_tex.rs), field for field
static_assert(sizeof(EeGeTexCoord) == sizeof(GETextureCoordinate), "GETextureCoordinate");
static_assert(offsetof(EeGeTexCoord, v) == offsetof(GETextureCoordinate, mV), "GETextureCoordinate");

// The calls the texture makes on other objects (EeGeTexOp), in the original's statements
intptr_t GETexture::Host(void* inOwner, EeGeTexOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeGeTexOp::SurfReference:
        ((GESurface*)inObj)->Reference();
        return 0;
    case EeGeTexOp::SurfDereference:
        ((GESurface*)inObj)->Dereference();
        return 0;
    case EeGeTexOp::SurfParentScreen:
        return (intptr_t)((GESurface*)inObj)->GetParentScreen();
    case EeGeTexOp::SurfAllocatedSize:
        return (intptr_t)((GESurface*)inObj)->GetAllocatedSize();
    case EeGeTexOp::SurfLogicalShift:
        return (intptr_t)((GESurface*)inObj)->GetLogicalShift();
    case EeGeTexOp::SurfSetLogicalShift:
        ((GESurface*)inObj)->SetLogicalShift((unsigned long)a);
        return 0;
    case EeGeTexOp::SurfLock:
        return ((GESurface*)inObj)->Lock();
    case EeGeTexOp::SurfUnlock:
        return ((GESurface*)inObj)->Unlock();
    case EeGeTexOp::SurfMemory:
        return (intptr_t)((GESurface*)inObj)->GetMemoryAddress();
    case EeGeTexOp::SurfBytesPerRow:
        return (intptr_t)((GESurface*)inObj)->GetBytesPerRow();
    case EeGeTexOp::SurfDescription:
        *(GESurfaceDescription*)a = ((GESurface*)inObj)->GetSurfaceDescription();
        return 0;
    case EeGeTexOp::SurfAttached:
        return (intptr_t)((GESurface*)inObj)->GetAttachedSurface(0);
    case EeGeTexOp::SurfAttachedCount:
        return (intptr_t)((GESurface*)inObj)->GetAttachedSurfaceCount();
    case EeGeTexOp::SurfWidth:
        return (intptr_t)((GESurface*)inObj)->GetWidth();
    case EeGeTexOp::SurfHeight:
        return (intptr_t)((GESurface*)inObj)->GetHeight();
    case EeGeTexOp::SurfCopyWithFilter:   // b: the source and destination (EeGeTexCopy)
        return ((GESurface*)inObj)->CopyWithFilter((GESurface*)a, ((GERectangle*)b)[0], ((GERectangle*)b)[1]);
    case EeGeTexOp::SurfCopyTo:
        return ((GESurface*)inObj)->CopyTo((GESurface*)a, ((GERectangle*)b)[0], ((GERectangle*)b)[1], c != 0);
    case EeGeTexOp::ScreenCreateSurface:
        return ((GEScreen*)inObj)->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b);
    // (FileInfo, FileFrame, RasterCreate, RasterDestroy, ManagerRemove, ManagerPromote: the Rust object's own, ee_ge
    // texobj_tex.rs)
    case EeGeTexOp::SurfPixelFormat: return (intptr_t)((GESurface*)inObj)->GetPixelFormat();
    case EeGeTexOp::SurfPixelDepth: return (intptr_t)((GESurface*)inObj)->GetPixelDepth();
    case EeGeTexOp::SurfVolatility: return (intptr_t)((GESurface*)inObj)->GetSurfaceVolatility();
    case EeGeTexOp::SurfIs3DRenderable: return ((GESurface*)inObj)->GetIs3DRenderable();
    case EeGeTexOp::SurfIsTransparencyEnabled: return ((GESurface*)inObj)->GetIsTransparencyEnabled();
    case EeGeTexOp::TexNew:
        return (intptr_t)new GETexture((GETextureManager*)inObj);
    case EeGeTexOp::TexBaseDereference:
        ((GETexture*)inObj)->UReferenceCounter::Dereference();
        return 0;
    default:
        return 0;
    }
}

// (the SetStage* setters: inline in lle/GETexture.h)

// ---- GETextureManager (GETextureManager.cpp, folded here in R4.3 part 16)

// the Rust mirrors of the descriptions (texobj.rs), field for field
static_assert(sizeof(EeGeTextureDescription) == sizeof(GETextureDescription), "GETextureDescription is its record");

// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GETextureManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    // (the lock's calls, the path's Reference and TexFreeFile: Rust's own, ee_ge manobj.rs direct_op)
    case EeGeManOp::PathDereference:
        ((FSPath*)inObj)->Dereference();
        return 0;
    case EeGeManOp::BaseDereference:
        ((GETexture*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpText: fprintf(*(FSFileSpec*)inObj, "%s", (const char*)a); return 0;
    case EeGeManOp::TexFindFile:
        return ee_of_find_texture(&GEOpenFormatsHost, inObj, (const char*)a, (EeOfTexture**)c);
    case EeGeManOp::TexNew:
        return (intptr_t)new GETexture(((GETextureManager*)inOwner));
    case EeGeManOp::TexNewFromSurface:
        return (intptr_t)new GETexture((GESurface*)inObj, ((GETextureManager*)inOwner));
    default: return 0;
    }
}

// ---- GETextureSet (GETextureSet.cpp, folded here in R4.3 part 16)

void GETextureSet::Dereference(void* inTexture) { static_cast<GETexture*>(inTexture)->Dereference(); }

// ---- GESurface (GESurface.cpp, folded here in R4.3 part 16)

// The calls the image members make on other objects (EeOfSurfOp), in the original's statements
static intptr_t GESurfaceHost(EeOfSurfOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    unsigned long theWritten;
    switch (inOp) {
    case EeOfSurfOp::Format: return (intptr_t)((GESurface*)inObj)->GetPixelFormat();
    case EeOfSurfOp::Depth: return (intptr_t)((GESurface*)inObj)->GetPixelDepth();
    case EeOfSurfOp::BytesPerRow: return (intptr_t)((GESurface*)inObj)->GetBytesPerRow();
    case EeOfSurfOp::Memory: return (intptr_t)((GESurface*)inObj)->GetMemoryAddress();
    case EeOfSurfOp::PixelWidth: return (intptr_t)((GESurface*)inObj)->GetPixelWidth();
    case EeOfSurfOp::PixelHeight: return (intptr_t)((GESurface*)inObj)->GetPixelHeight();
    case EeOfSurfOp::NoAlpha: return ((GESurface*)inObj)->GetSurfaceDescription().mNoAlpha;
    case EeOfSurfOp::Lock: return ((GESurface*)inObj)->Lock();
    case EeOfSurfOp::Unlock: return ((GESurface*)inObj)->Unlock();
    case EeOfSurfOp::FileIsOpen: return ((FSFileSpec*)inObj)->GetIsOpen();
    case EeOfSurfOp::FileAccess: return (intptr_t)((FSFileSpec*)inObj)->GetFileAccess();
    case EeOfSurfOp::FileOpen: return ((FSFileSpec*)inObj)->Open((unsigned long)a);
    case EeOfSurfOp::FileClose: return ((FSFileSpec*)inObj)->Close();
    case EeOfSurfOp::FileFOpen: return ((FSFileSpec*)inObj)->FOpen((char*)a);
    case EeOfSurfOp::FileStream: return (intptr_t)(FILE*)*((FSFileSpec*)inObj);
    case EeOfSurfOp::FilePosition: return ((FSFileSpec*)inObj)->GetPosition(*(long*)a);
    case EeOfSurfOp::FileSetPosition: return ((FSFileSpec*)inObj)->SetPosition((long)a, (unsigned long)b, c != 0);
    case EeOfSurfOp::FileWrite: return ((FSFileSpec*)inObj)->Write((const unsigned char*)a, (unsigned long)b, theWritten);
    case EeOfSurfOp::FileWriteU16: return ((FSFileSpec*)inObj)->Write((unsigned short)a);
    case EeOfSurfOp::FileWriteLong: return ((FSFileSpec*)inObj)->Write((long)a);
    case EeOfSurfOp::FileRead: return ((FSFileSpec*)inObj)->Read((unsigned char*)a, (unsigned long)b, *(unsigned long*)c);
    case EeOfSurfOp::FileLength: return ((FSFileSpec*)inObj)->GetLength(*(unsigned long*)a);
    }
    return 0;
}

// Reads an image file's width, height and depth by file type (BMP: unsupported, JPG, JPG2, PNG)
long GESurface::GetImageFileAttributes(FSFileSpec& inFile, unsigned char inType, unsigned long& outWidth,
                                       unsigned long& outHeight, unsigned long& outDepth)
{
    return ee_of_surf_attributes(&GESurfaceHost, &inFile, inType, &outWidth, &outHeight, &outDepth);
}

// Loads an image file into the surface by file type (BMP: unsupported, JPG, JPG2, PNG; TGA: no longer read, D9)
long GESurface::Load(FSFileSpec& inFile, unsigned char inType) { return ee_of_surf_load(&GESurfaceHost, this, &inFile, inType); }

// Saves the surface as an image file of the given type (BMP, JPG, JPG2)
long GESurface::Save(FSFileSpec& inFile, unsigned char inType) { return ee_of_surf_save(&GESurfaceHost, this, &inFile, inType); }

// ======== Graphics Engine/DX7 Display Plug-ins/Common/DX7Rasterizer.cpp ========
// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x100091aa..0x1000d0fc: the U2D key init chains, the DirectDraw/Direct3D enumeration callbacks, DX7Rasterizer's
// ctor and dtor, then its members in alphabetical order, then the vector<DX7RScreenState*> COMDATs and the
// template-static init chain head. The enumeration callbacks and ActivateRasterizer were built without global
// optimisation (see the pragmas below); everything else is -O1.

// DX7Rasterizer is whole in Rust (rust/crates/ee_render/src/dx7/rastobj.rs, R4.3 part 13): the per-screen DX7 records, the
// debug colour, the light slots and every method; DX7TnLRasterizer is its T&L flavour. Since LLE round 12 the C++ class
// is GERasterizer itself (its member mDX7; lle/GERasterizer.h), whose members call the Rust object directly; the ones
// needing a complete GE type are below, and DX7RasterizerHost makes the calls on other objects Rust asks for.

// The calls the Rust rasterizer makes on other objects (EeDx7RastOp), in the original's order.
intptr_t DX7RasterizerHost(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeDx7RastOp::ScreenNew:
        return (intptr_t) new GEScreen(NULL, static_cast<GXDirectDraw*>(inObj), UString(reinterpret_cast<const char*>(a)),
                                       (b & 1) != 0, (b & 2) != 0);
    case EeDx7RastOp::ScreenDelete: delete static_cast<GEScreen*>(inObj); return 0;
    // (the screens' calls, the texture manager's SetScreen, the surfaces' attachments, GX surfaces, flips and logical
    // sizes, a stage texture's GX surface and the window's origin: Rust's own, ee_dx7 rastobj.rs object_op)
    case EeDx7RastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
    case EeDx7RastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
    // (the drawing lock's Enter and Leave, the clock, the references, a texture's change flag, promotion and stage:
    // Rust's own, ee_dx7 rastobj.rs)
    case EeDx7RastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
    default: return 0;
    }
}

// the primitives are their Rust objects (one pointer each: the draws walk the C++ arrays, ee_dx7 draw_ffi.rs)
static_assert(sizeof(GELine) == sizeof(void*) && sizeof(GERhombus) == sizeof(void*),
              "ee_ge prim.rs");

static_assert(offsetof(GEScreen, mTextureManager) == 0 && offsetof(GEScreen, mDX7) == 2 * sizeof(void*) &&
              offsetof(GESurface, mDX7) == 2 * sizeof(void*) && sizeof(GETextureManager) == sizeof(void*) &&
              offsetof(GESurface, mLogicalShift) == 3 * sizeof(void*) && offsetof(GESurface, mLogicalHeight) == 3 * sizeof(void*) + 2 * sizeof(long),
              "ee_ge cppobj.rs CppScreen, CppCounted, CppSurface, plain");
static_assert(sizeof(std::pair<GE3DPoint, float>) == 16, "ee_dx7/src/draw_model.rs draw_3d_line: (x, y, z, advance)");
static_assert(sizeof(std::pair<U2DPoint<long>, U2DPoint<long> >) == 4 * sizeof(long), "ee_dx7 draw_terrain grid lines");

// ---- the screens, surfaces and movies (DX7Trampolines.cpp, folded here in R4.3 part 17): their objects' hosts and
// constructors, and the layouts Rust fills

extern "C" void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }

// --- GEScreen and GESurface (DX7Screen, DX7Surface): whole in Rust (ee_dx7 scrobj.rs, surfobj.rs); the calls they make on C++ objects ------
static_assert(offsetof(GERectangle, mBottom) == 0 && offsetof(GERectangle, mLeft) == 4 && offsetof(GERectangle, mRight) == 8 &&
              offsetof(GERectangle, mTop) == 12 && sizeof(GEColor) == 12, "ee_dx7/src/surfobj.rs ge_rect");

static intptr_t DX7ObjectHost(EeDx7ObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeDx7ObjOp::NameNew: return (intptr_t) new UString(*static_cast<const UString*>(inObj));
    case EeDx7ObjOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
    case EeDx7ObjOp::SurfaceNew:
        return (intptr_t) new GESurface(static_cast<GXSurface*>(inObj), *reinterpret_cast<const GESurfaceDescription*>(a),
                                        reinterpret_cast<GEScreen*>(b));
    // (a surface's object, reference, allocated size and logical metrics: Rust's own, ee_dx7 surfobj.rs direct_op)
    case EeDx7ObjOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
    case EeDx7ObjOp::ProbeWindow: {   // Test32BitSupport's hidden 16 x 16 window
        UString theClassName("TestWindowClass"), theTitle("");
        return static_cast<GEScreen*>(inObj)->CreateAWindow(GE2DPoint(0.0f, 0.0f), 16, 16, theClassName, theTitle,
                                                             *reinterpret_cast<GEWindow**>(a), 2, NULL);
    }
    case EeDx7ObjOp::WindowDelete: delete static_cast<GEWindow*>(inObj); return 0;   // (WindowOs, DispatchEvents: direct_op)
    default: return 0;
    }
}

// Creates a screen for a DirectDraw device: lists its display modes, finds the current one, probes 32-bit support and
// registers it (the original's private ctor 0x1000d271; the rasterizer makes the screens while it enumerates)
GEScreen::GEScreen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
                   bool inSharesPrimary)
{
    this->mDX7 = ee_dx7sc_new(DX7ObjectHost, this, inMonitor, inDirectDraw, &inName, inAttachedToDesktop, inSharesPrimary);
    ee_dx7sc_start(this->mDX7);
}

// CreateAWindow's C++ (ee_dx7 scrobj.rs create_window): the window object and the failure's debug line
static void* DX7NewWindow(void* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inStyle)
    { return new GEWindow(static_cast<PlatWindow*>(inWindow), inWidth, inHeight, inStyle, true); }
static void DX7WindowError(const char* inError) { ESDebugOut("CreateAWindow: %s\n", inError); }

// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the full
// screen (borderless, no mode change) when it covers the display. The class name and the parent are unused. (0x1000d622)
long GEScreen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              const UString& inClassName, const UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    return ee_dx7sc_create_window(gPlatSimMode != 0, inOrigin.GetX(), inOrigin.GetY(), inWidth, inHeight, inTitle, inStyle,
                                  reinterpret_cast<void**>(&outWindow), DX7NewWindow, DX7WindowError);
}

// Wraps a GX surface with its description and parent screen, unlocked and with no attachments (0x1000eced)
GESurface::GESurface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen)
{
    this->mDX7 = ee_dx7sf_new(DX7ObjectHost, static_cast<GESurface*>(this), inSurface,
                              reinterpret_cast<const EeDx7SurfaceDescription*>(&inDescription), inParentScreen);
}

// ---- the renderer the game links (DX7HRTnLDisplay.cpp's globals; its PIM entry points went with the plug-in boundary)
static UString gName("Direct3D Hardware TnL");
static GERasterizer gTnLRasterizer(gName, true);   // the T&L flavour (rastobj.rs)

// The game's rasterizer (GEInitialize adds it when it finds a screen)
GERasterizer* DX7TnLRasterizer() { return &gTnLRasterizer; }
