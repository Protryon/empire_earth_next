// The platform layer: the operating-system services the engine and the game use, behind a small C interface.
//
// Every function here is plain C (extern "C", fixed-width types, opaque handles), so that an implementation for
// another system, or one written in Rust (`#[no_mangle] extern "C"`), can replace the Win32 one without touching the
// callers. The Win32 implementation lives in src/Platform/Win32/ and keeps the original game's behaviour exactly
// (it calls the same Win32 functions the code called before it was routed through here).
//
//   platform/PlatTime.h         clocks, the high-resolution counter, sleep, local time, system timers
//   platform/PlatThread.h       mutexes, events (wait one / wait any), atomics, thread ids, the last native error
//   platform/PlatString.h       case-insensitive compares, integer formatting, the multibyte (_mbs*) string functions
//   platform/PlatPreferences.h  the preferences store (the Windows registry on Win32)
//   platform/PlatFile.h         files, directory search, temporary files, file mapping (included where used)
//   platform/PlatPath.h         the game's Windows paths on the native file system, the data root
//   platform/PlatSocket.h       IPv4 sockets, readiness events, name lookups, network adapters (Winsock subset)
//   platform/PlatMessage.h      a modal message to the user (MessageBox)
//
// Implementations: src/Platform/Win32/ (Windows) and src/Platform/Posix/ (Linux, macOS). Tests:
// tests/platform/ (tools/xbuild.py test).
//
// Rules: no Windows type or header in these interfaces; results are documented per function; a handle created by
// one function family is destroyed by the same family. Plan and census: PORT.md "Cross-platform".
#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include "platform/PlatTime.h"
#include "platform/PlatThread.h"
#include "platform/PlatString.h"
#include "platform/PlatPreferences.h"

#endif
