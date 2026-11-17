// C:\EEX\Low-Level Engine\Utilities\UMath.cpp (file name inferred: the UM* functions sort between the UI* units and
// URegistry.cpp (now UConfig.cpp); Rich has one more C++ object than the units named so far)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10080c07..0x1008104b: UMFastMath::BuildSqrtTable (empty), then the UM functions alphabetical. Its .data is the two
// lookup tables (0x100dd8a4, 0x100ed8a8), its .rdata the named constants 0x10099bf0..0x10099c18.
// UReferenceCounter.cpp's sReferenceDebugInfo init chain starts at 0x1008104b.
// The table trigonometry and the quadratic forward to Rust (rust/crates/ee_render/src/ge um.rs, RUST_PLAN R4.3); the tables
// (UMTables.h, deleted) are rebuilt there and pinned by a digest. The floating-point control is ee_util fp.rs (LLE lane round 7).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_ge.h"
#include "rust/ee_util.h"

// Does nothing (the square root table is not built in the retail engine)
void UMFastMath::BuildSqrtTable()
{
}

// Returns the arc cosine from a lookup table, or a series near 1; clamps below -1, returns 0 above 1
float UMArcCos(float inValue)
{
    return ee_ge_um_arccos(inValue);
}

// Returns the arc tangent by a short power series, folding values outside -1..1 back into it
float UMArcTan(float inValue)
{
    return ee_ge_um_arctan(inValue);
}

// Returns the angle of the vector (y, x) in -pi..pi from the arc cosine of its normalised x
float UMArcTan2(float inY, float inX)
{
    return ee_ge_um_arctan2(inY, inX);
}

// Returns the cosine of an angle from a lookup table after wrapping it into -pi..pi
float UMCos(float inAngle)
{
    return ee_ge_um_cos(inAngle);
}

// Returns the FPU precision setting (24, 53 or 64 bits): 53 without an x87 (ee_util fp.rs)
unsigned long UMGetFloatingPointPrecision() { return ee_util_fp_precision(); }

// Returns the FPU rounding mode (float.h's _RC_* value)
unsigned long UMGetFloatingPointRoundingMode() { return ee_util_fp_rounding(); }

// Sets the FPU precision (nothing to set without an x87)
void UMSetFloatingPointPrecision(unsigned long inPrecision) { ee_util_fp_set_precision(inPrecision); }

// Sets the FPU rounding mode
void UMSetFloatingPointRoundingMode(unsigned long inMode) { ee_util_fp_set_rounding(inMode); }

// Returns the tangent from the table sine and cosine; plus or minus FLT_MAX where the cosine is 0
float UMTan(float inAngle)
{
    return ee_ge_um_tan(inAngle);
}
