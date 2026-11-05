// C:\EEX\Low-Level Engine\Utilities\UMath.cpp (file name inferred: the UM* functions sort between the UI* units and
// URegistry.cpp (now UConfig.cpp); Rich has one more C++ object than the units named so far)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10080c07..0x1008104b: UMFastMath::BuildSqrtTable (empty), then the UM functions alphabetical. Its .data is the two
// lookup tables (0x100dd8a4, 0x100ed8a8), its .rdata the named constants 0x10099bf0..0x10099c18.
// UReferenceCounter.cpp's sReferenceDebugInfo init chain starts at 0x1008104b.
#include <windows.h>
#include <math.h>
#include <float.h>
#include <cmath>
#ifndef _WIN32
#include <fenv.h>
#endif

#include "lle/LowLevelEngine.h"

#include "UMTables.h"

static const float kCosTableScale = 5215.189f;          // 16384 / pi
static const float kOneThird = 0.33333334f;
static const float kOneFifth = 0.2f;
static const float kOneSeventh = 0.14285715f;
static const float kArcCosTableLimit = 0.9995f;
static const float kQuarterPi = 0.78539819f;
static const float kMinusHalfPi = -1.5707964f;
static const float kMinusInfinity = -FLT_MAX;
static const float kPlusInfinity = FLT_MAX;

// FUNCTION: LLE 0x10080c07
// Does nothing (the square root table is not built in the retail engine)
void UMFastMath::BuildSqrtTable()
{
}

// FUNCTION: LLE 0x10080c08
// Returns the arc cosine from a lookup table, or a series near 1; clamps below -1, returns 0 above 1
// NEAR: the original loads the 0 above 1 with fldz (series arithmetic identical)
float UMArcCos(float inValue)
{
    if (inValue > 0.0f) {
        if (inValue < kArcCosTableLimit)
            return sArcCosTable[UMFloatToLong(inValue * 32768.0f)];
        if (inValue < 1.0f) {
            // one power variable (kept on the FPU stack) and the sum in inValue (rounded through its float slot):
            // the original's x - x^3/3 subtracts from the float-rounded x and forms x^3 from the unrounded one; a
            // separate square-then-power pair rounds differently (MP game with our engine on one end desynced)
            float thePower = (float)sqrt(1.0f - inValue * inValue) / inValue;
            float theSquare = thePower * thePower;
            inValue = thePower;
            thePower *= theSquare;
            inValue -= kOneThird * thePower;
            thePower *= theSquare;
            inValue += kOneFifth * thePower;
            thePower *= theSquare;
            inValue -= thePower * kOneSeventh;
        } else
            inValue = 0;   // through the variable: the original loads it with fldz (a `return 0` is fld [0.0f])
        return inValue;
    }
    if (inValue < 0.0f) {
        if (inValue < -1.0f)
            inValue = -1.0f;
        return 3.1415927f - UMArcCos(-inValue);
    }
    return 1.5707964f;
}

// FUNCTION: LLE 0x10080ce8
// Returns the arc tangent by a short power series, folding values outside -1..1 back into it
float UMArcTan(float inValue)
{
    if (inValue > 0.0f && inValue < 1.0f) {
        float theSquare = inValue * inValue;
        float thePower = inValue * theSquare;
        float theResult = inValue - kOneThird * thePower;
        thePower = theSquare * thePower;
        theResult += kOneFifth * thePower;
        // the original multiplies (x^2 * x^5) first, then by 1/7 (0x80d2c); without the parentheses VC6 forms
        // (1/7 * x^5) * x^2, which rounds differently (an MP game with our engine on one end only drifted)
        return theResult - (theSquare * thePower) * kOneSeventh;
    }
    if (inValue > 1.0f) {
        float theInverse = 1.0f / inValue;
        if (std::isfinite((double)theInverse))
            return 1.5707964f - UMArcTan(theInverse);
        return 1.5707964f;
    }
    if (inValue < -1.0f) {
        float theInverse = -1.0f / inValue;
        if (std::isfinite((double)theInverse))
            return UMArcTan(theInverse) - 1.5707964f;
        return kMinusHalfPi;
    }
    if (inValue < 0.0f)
        return -UMArcTan(-inValue);
    float theResult;
    theResult = inValue == 0.0f ? 0 : kQuarterPi;
    return theResult;
}

// FUNCTION: LLE 0x10080dff
// Returns the angle of the vector (y, x) in -pi..pi from the arc cosine of its normalised x
float UMArcTan2(float inY, float inX)
{
    if (inX != 0.0f) {
        float theCosine = inX / (float)sqrt(inY * inY + inX * inX);
        if (inY >= 0.0f)
            return UMArcCos(theCosine);
        return -UMArcCos(theCosine);
    }
    if (inY < 0.0f)
        return kMinusHalfPi;
    return 1.5707964f;
}

// FUNCTION: LLE 0x10080e6c
// Returns the cosine of an angle from a lookup table after wrapping it into -pi..pi
float UMCos(float inAngle)
{
    while (inAngle < -3.1415927f)
        inAngle += 6.2831855f;
    while (inAngle > 3.1415927f)
        inAngle -= 6.2831855f;
    if (inAngle < 0.0f)
        inAngle = -inAngle;
    return sCosTable[UMFloatToLong(kCosTableScale * inAngle)];
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
    if (inA != 0.0f) {
        double theDiscriminant = inB * inB - inA * inC * 4.0f;
        inA += inA;
        outRoot1 = ((float)sqrt(theDiscriminant) - inB) / inA;
        inB = -inB;
        outRoot2 = (inB - (float)sqrt(theDiscriminant)) / inA;
        return true;
    }
    if (inB != 0.0f) {
        float theRoot = (-inC) / inB;
        outRoot1 = theRoot;
        outRoot2 = theRoot;
        return true;
    }
    return false;
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
    float theCosine = UMCos(inAngle);
    if (theCosine != 0.0f)
        return UMSin(inAngle) / theCosine;
    if (inAngle >= 0.0f && inAngle < 1.5707964f || inAngle >= -3.1415927f && inAngle <= kMinusHalfPi)
        return kPlusInfinity;
    return kMinusInfinity;
}
