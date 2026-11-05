// POSIX only (tools/xbuild.py census, the Linux/macOS builds): `#include <windows.h>` finds this header and gets the
// Windows types and macros (platform/wintypes.h), no Windows function. Every call a unit still makes to Windows
// fails to compile here: that is the census (PORT.md step 4). Never on the include path of a Windows build.
#ifndef EE_COMPAT_WINDOWS_H
#define EE_COMPAT_WINDOWS_H
#include "platform/wintypes.h"
#endif
