// C:\EEX\Low-Level Engine\Utilities\UMath.cpp (file name inferred: the UM* functions sort between the UI* units and
// URegistry.cpp (now UConfig.cpp); Rich has one more C++ object than the units named so far)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10080c07..0x1008104b: UMFastMath::BuildSqrtTable (empty), then the UM functions alphabetical. Its .data is the two
// lookup tables (0x100dd8a4, 0x100ed8a8), its .rdata the named constants 0x10099bf0..0x10099c18.
// UReferenceCounter.cpp's sReferenceDebugInfo init chain starts at 0x1008104b.
// The table trigonometry and the quadratic forward to Rust (rust/crates/ee_ge um.rs, RUST_PLAN R4.3); the tables
// (UMTables.h, deleted) are rebuilt there and pinned by a digest. The floating-point control stays here.
#include <windows.h>
#include <math.h>
#include <float.h>
#include <cmath>
#ifndef _WIN32
#include <fenv.h>
#endif

#include "lle/LowLevelEngine.h"
#include "rust/ee_ge.h"

// FUNCTION: LLE 0x10080c07
// Does nothing (the square root table is not built in the retail engine)
void UMFastMath::BuildSqrtTable()
{
}

// FUNCTION: LLE 0x10080c08
// Returns the arc cosine from a lookup table, or a series near 1; clamps below -1, returns 0 above 1
float UMArcCos(float inValue)
{
    return ee_ge_um_arccos(inValue);
}

// FUNCTION: LLE 0x10080ce8
// Returns the arc tangent by a short power series, folding values outside -1..1 back into it
float UMArcTan(float inValue)
{
    return ee_ge_um_arctan(inValue);
}

// FUNCTION: LLE 0x10080dff
// Returns the angle of the vector (y, x) in -pi..pi from the arc cosine of its normalised x
float UMArcTan2(float inY, float inX)
{
    return ee_ge_um_arctan2(inY, inX);
}

// FUNCTION: LLE 0x10080e6c
// Returns the cosine of an angle from a lookup table after wrapping it into -pi..pi
float UMCos(float inAngle)
{
    return ee_ge_um_cos(inAngle);
}

// FUNCTION: LLE 0x10080ed1
// Returns the FPU precision setting (24, 53 or 64 bits)
unsigned long UMGetFloatingPointPrecision()
{
#if !defined(_M_IX86)
    // only the x87 has a precision control; SSE computes doubles at 53 bits, the precision the game asks for
    return kFPPrecision53;
#else
    unsigned long theControl = _controlfp(0, 0);
    if ((theControl & _PC_24) == _PC_24)
        return _PC_24;
    return (theControl & _PC_53) == _PC_53 ? _PC_53 : _PC_64;
#endif
}

#ifndef _WIN32
// float.h's _RC_* rounding-control values (what the engine stores and compares) and <fenv.h>'s modes
static const unsigned long kRoundingControl[4] = {0x000, 0x100, 0x200, 0x300};   // _RC_NEAR, _RC_DOWN, _RC_UP, _RC_CHOP
static const int kRoundingMode[4] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
#endif

// FUNCTION: LLE 0x10080f02
// Returns the FPU rounding mode
unsigned long UMGetFloatingPointRoundingMode()
{
#ifdef _WIN32
    return _controlfp(0, 0) & _MCW_RC;
#else
    int theMode = fegetround();
    for (int i = 0; i < 4; i++)
        if (kRoundingMode[i] == theMode)
            return kRoundingControl[i];
    return 0;
#endif
}

// FUNCTION: LLE 0x10080f14
// Solves a*x^2 + b*x + c = 0 into two roots (a negative discriminant is not checked); false when a and b are 0
bool UMQuadraticEquation(float inA, float inB, float inC, float& outRoot1, float& outRoot2)
{
    return ee_ge_um_quadratic(inA, inB, inC, &outRoot1, &outRoot2);
}

// FUNCTION: LLE 0x10080fab
// Sets the FPU precision
void UMSetFloatingPointPrecision(unsigned long inPrecision)
{
#if defined(_M_IX86)
    _controlfp(inPrecision, _MCW_PC);
#else
    // x64 has no precision control: the CRT treats _MCW_PC as an invalid parameter (the process ends)
    (void)inPrecision;
#endif
}

// FUNCTION: LLE 0x10080fbd
// Sets the FPU rounding mode
void UMSetFloatingPointRoundingMode(unsigned long inMode)
{
#ifdef _WIN32
    _controlfp(inMode, _MCW_RC);
#else
    for (int i = 0; i < 4; i++)
        if (kRoundingControl[i] == (inMode & 0x300))
            fesetround(kRoundingMode[i]);
#endif
}

// FUNCTION: LLE 0x10080fcf
// Returns the tangent from the table sine and cosine; plus or minus FLT_MAX where the cosine is 0
float UMTan(float inAngle)
{
    return ee_ge_um_tan(inAngle);
}
