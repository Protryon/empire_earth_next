// ne_msgmgr_test stand-in for the engine core: the error codes, ESVERIFY_AT, UIsNone32 and the world's clock.
#pragma once
#include <stdint.h>
#include <string.h>
#include "net_world.h"
enum { kErrorOK = 0, kErrorNotFound = 3, keseOK = 0 };
#define ESVERIFY_AT(inCondition, inLine) ((void)(inCondition))
inline bool UIsNone32(unsigned long inValue) { return (uint32_t)inValue == 0xffffffffu; }
inline unsigned long TMGetTime() { return gWorld->mClock; }
