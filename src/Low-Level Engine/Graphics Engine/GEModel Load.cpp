// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Load.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001c294..0x1001cd3b: GEModel's current-format loader (Load, LoadCurrentVersion, Load*Chunk) (file name inferred).
// The game's models are glTF files only (docs/RUST_PLAN.md D9): Rust decodes one (rust/crates/ee_openformats,
// ee_of_gltf_to_model) into the model the .cem chunk readers stored, and StoreLoaded stores it. The .cem reader
// (Load) and the version-1 conversion ('GEModel Legacy.cpp') are retired.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/FSMemoryFile.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "GERustMath.h"
#include "rust/ee_ge.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x1001c294
// SYNTHETIC: LLE 0x1001c29e
// SYNTHETIC: LLE 0x1001c2ae
// SYNTHETIC: LLE 0x1001c2ba
// SYNTHETIC: LLE 0x1001c2c4
// SYNTHETIC: LLE 0x1001c2ce
// SYNTHETIC: LLE 0x1001c2de
// SYNTHETIC: LLE 0x1001c2ea
// SYNTHETIC: LLE 0x1001c2f4
// SYNTHETIC: LLE 0x1001c2fe
// SYNTHETIC: LLE 0x1001c30e
// SYNTHETIC: LLE 0x1001c31a
// SYNTHETIC: LLE 0x1001c324
// SYNTHETIC: LLE 0x1001c32e
// SYNTHETIC: LLE 0x1001c33e
// SYNTHETIC: LLE 0x1001c34a
// SYNTHETIC: LLE 0x1001c354
// SYNTHETIC: LLE 0x1001c35e
// SYNTHETIC: LLE 0x1001c36e
// SYNTHETIC: LLE 0x1001c37a
// SYNTHETIC: LLE 0x1001c384
// SYNTHETIC: LLE 0x1001c38e
// SYNTHETIC: LLE 0x1001c39e
// SYNTHETIC: LLE 0x1001c3aa

// Allocates the model's arrays from the counts (AllocateInternalData, as the counts chunk did) and stores the model
// where the original's chunk readers stored it, then creates and fills the sub-models; returns the model's result
// (a sub-model's is ignored, as the original's). A model whose counts the reader refused is left as constructed.
long GEModel::StoreLoaded(const EeGeCemModel* inModel, std::map<unsigned long, long>& ioTextures)
{
    std::vector<EeGeSharedFrame> theShared;
    for (std::map<unsigned long, long>::const_iterator theIterator = ioTextures.begin(); theIterator != ioTextures.end();
         theIterator++) {
        EeGeSharedFrame theEntry;
        theEntry.frame = (uint32_t)theIterator->first;
        theEntry.offset = (int32_t)theIterator->second;
        theShared.push_back(theEntry);
    }
    return ee_ge_mod_store_loaded(this->mRust, inModel, theShared.empty() ? NULL : &theShared[0], theShared.size());
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001ccf9
// SYNTHETIC: LLE 0x1001cd17
// SYNTHETIC: LLE 0x1001cd23
// SYNTHETIC: LLE 0x1001cd2f
