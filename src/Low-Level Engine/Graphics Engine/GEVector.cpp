// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10035941..0x100367fc: the static init chains (the four string IDs, sQuantizedVectorArray, the zero vector), the
// QuantizedVectorArray ctor/dtor, then the quantization members. The basic GEVector operations are emitted in the
// GE2DPoint.cpp block (see lle/GEVector.h). GetQuantizedVersion expands the inline-asm UMFloatToLong, which turns
// off most of VC6's optimization for that function.
// The table is built and searched in Rust (rust/crates/ee_ge vector.rs; GERustMath.h); the storage, the pointers
// callers keep into it and the file I/O stay here.
#include <windows.h>

#define LLE_GEVECTOR_CPP    // GEVector's statics are visible here only (see lle/GEVector.h) (provisional name)
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/FSFile.h"
#include "GERustMath.h"
#include "GERustIO.h"

// The unit cube's six faces (-X, -Y, -Z, +X, +Y, +Z), each sampled on a 41 x 41 grid of normalized directions.
// (The original also kept the six face planes here, +0x1d8c8; the Rust search has its own.)
struct GEVector::QuantizedVectorArray {
    QuantizedVectorArray();
    GEVector mVectors[41][6 * 41];          // +0x00000
};
const UString GEVector::sVectorXStringID("GEVector-VectorX");
const UString GEVector::sVectorYStringID("GEVector-VectorY");
const UString GEVector::sVectorZStringID("GEVector-VectorZ");
const UString GEVector::sQuantizedVectorIndexStringID("GEVector-QuantizedVectorIndex");
GEVector::QuantizedVectorArray GEVector::sQuantizedVectorArray;
static GEVector sZeroVector;                // 0x1010ff60: the quantized version of (0, 0, 0) (provisional name)
// Builds the table of quantized unit directions (a 41 x 41 grid on each cube face)
GEVector::QuantizedVectorArray::QuantizedVectorArray()
{
    ee_ge_vector_quantized_table(GERust(this->mVectors[0][0]), sizeof(this->mVectors) / sizeof(GEVector));
}

// (0x10035ea3, the destructor of the six face planes: gone with them)

// Returns the nearest entry of the quantized direction table: the cube face the vector's ray hits,
// then the grid cell there (the shared zero vector for (0, 0, 0))
GEVector* GEVector::GetQuantizedVersion() const
{
    GEVector* theQuantizedVector = NULL;
    long theIndex = ee_ge_vector_quantized_index(GERust(*this));
    if (theIndex >= 0)
        theQuantizedVector = &GEVector::sQuantizedVectorArray.mVectors[0][0] + theIndex;
    else if (theIndex == -1)
        theQuantizedVector = &sZeroVector;
    ESVERIFY_AT(theQuantizedVector != NULL, 383);
    return theQuantizedVector;
}

// ---- Exported members with no address of their own (ICF-folded: Load 0xdc23, Save 0x20dc6)
long GEVector::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), false, false);
}

long GEVector::Save(FSFile& inFile) const
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), true, false);
}
