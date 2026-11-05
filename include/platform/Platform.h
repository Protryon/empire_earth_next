// The platform layer: the operating-system services the engine and the game use, behind a small C interface.
//
// Every function here is plain C (extern "C", fixed-width types, opaque handles), so that an implementation for
// another system, or one written in Rust (`#[no_mangle] extern "C"`), can replace the Win32 one without touching the
// callers. The Win32 implementation lives in src/Platform/Win32/ and keeps the original game's behaviour exactly
// (it calls the same Win32 functions the code called before it was routed through here).
//
//   platform/PlatTime.h         clocks, the high-resolution counter, sleep, local time, system timers
//   platform/PlatThread.h       mutexes, events (wait one / wait any), atomics, thread ids, the last native error
//
// Rules: no Windows type or header in these interfaces; results are documented per function; a handle created by
// one function family is destroyed by the same family. Plan and census: PORT.md "Cross-platform".
#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include "platform/PlatTime.h"
#include "platform/PlatThread.h"

#endif
