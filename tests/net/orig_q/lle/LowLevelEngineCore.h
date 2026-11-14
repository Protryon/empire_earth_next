// ne_queues_test stand-in for the engine core: the error codes, ESVERIFY_AT (a world line), the world's clock and
// floating-point control.
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "q_world.h"
#define LLE_API
enum { kErrorOK = 0, keseOK = 0, kErrorAlreadyExists = 2, kErrorTimeout = 0x11 };
#define ESVERIFY_AT(inCondition, inLine) do { if (!(inCondition)) gQ->Line("verify %d", (int)(inLine)); } while (0)
inline unsigned long TMGetTime() { return gQ->mClock; }
const unsigned long kFPPrecision53 = 0x10000;
inline void UMSetFloatingPointPrecision(unsigned long inPrecision)
{
    gQ->mPrecision = inPrecision;
    gQ->Line("fpu precision %lx", inPrecision);
}
inline unsigned long UMGetFloatingPointPrecision() { return gQ->mPrecision; }
inline void UMSetFloatingPointRoundingMode(unsigned long inMode)
{
    gQ->mRounding = inMode;
    gQ->Line("fpu rounding %lx", inMode);
}
inline unsigned long UMGetFloatingPointRoundingMode() { return gQ->mRounding; }
inline long UMFloatToLong(float inValue) { return (long)lrintf(inValue); }
