// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10035941..0x100367fc: the static init chains (the four string IDs, sQuantizedVectorArray, the zero vector), the
// QuantizedVectorArray ctor/dtor, then the quantization members. The basic GEVector operations are emitted in the
// GE2DPoint.cpp block (see lle/GEVector.h). GetQuantizedVersion expands the inline-asm UMFloatToLong, which turns
// off most of VC6's optimization for that function.
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10035941
// SYNTHETIC: LLE 0x1003594b
// SYNTHETIC: LLE 0x1003595b
// SYNTHETIC: LLE 0x10035967
// SYNTHETIC: LLE 0x10035982
// SYNTHETIC: LLE 0x1003598c
// SYNTHETIC: LLE 0x1003599c
// SYNTHETIC: LLE 0x100359a8
// SYNTHETIC: LLE 0x100359c3
// SYNTHETIC: LLE 0x100359cd
// SYNTHETIC: LLE 0x100359dd
// SYNTHETIC: LLE 0x100359e9
// SYNTHETIC: LLE 0x10035a04
// SYNTHETIC: LLE 0x10035a0e
// SYNTHETIC: LLE 0x10035a1e
// SYNTHETIC: LLE 0x10035a2a
// SYNTHETIC: LLE 0x10035a45
// SYNTHETIC: LLE 0x10035a4f
// SYNTHETIC: LLE 0x10035a59
// SYNTHETIC: LLE 0x10035a65
// SYNTHETIC: LLE 0x10035a80
// SYNTHETIC: LLE 0x10035a8a
// SYNTHETIC: LLE 0x10035aa3
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
// FUNCTION: LLE 0x10035ab0
// Builds the table of quantized unit directions (a 41 x 41 grid on each cube face)
GEVector::QuantizedVectorArray::QuantizedVectorArray()
{
    ee_ge_vector_quantized_table(GERust(this->mVectors[0][0]), sizeof(this->mVectors) / sizeof(GEVector));
}

// (0x10035ea3, the destructor of the six face planes: gone with them)

// FUNCTION: LLE 0x10035f0e
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

// FUNCTION: LLE 0x1003676f
// Reads a quantized direction index from an open file and returns its table entry (the zero vector if
// out of range)
long GEVector::LoadQuantizedIndex(FSFile& inFile, GEVector*& outVector)
{
    long theResult;
    unsigned short theIndex;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(theIndex);
        if (theResult == kErrorOK) {
            if (theIndex < sizeof(GEVector::sQuantizedVectorArray.mVectors) / sizeof(GEVector))
                outVector = &GEVector::sQuantizedVectorArray.mVectors[0][0] + theIndex;
            else
                outVector = &sZeroVector;
        }
    } else
        theResult = 4;
    return theResult;
}

// FUNCTION: LLE 0x100367be
// Writes the vector's quantized direction index to an open file
long GEVector::SaveQuantizedIndex(FSFile& inFile) const
{
    unsigned short theIndex = this->GetQuantizedVersion() - &GEVector::sQuantizedVectorArray.mVectors[0][0];
    if (inFile.GetIsOpen())
        return inFile.Write(theIndex);
    return 4;
}

// The zero vector's destructor (the members below have no address of their own)
// FUNCTION: LLE 0x10035aaf
// Destroys the file-static zero vector at shutdown (an empty body)

// ---- Exported members with no address of their own (ICF-folded: Load 0xdc23, Save 0x20dc6)
long GEVector::Load(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(this->mX);
        if (theResult == 0) {
            theResult = inFile.Read(this->mY);
            if (theResult == 0)
                theResult = inFile.Read(this->mZ);
        }
    } else
        theResult = 4;
    return theResult;
}

long GEVector::Save(FSFile& inFile) const
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Write(this->mX);
        if (theResult == 0) {
            theResult = inFile.Write(this->mY);
            if (theResult == 0)
                theResult = inFile.Write(this->mZ);
        }
    } else
        theResult = 4;
    return theResult;
}
