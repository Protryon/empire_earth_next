// The vendored Lua 5.4 C API (src/ThirdParty/lua, compiled as C in ee_lua.c) with C linkage, for C++ units. The
// sources are staged before they compile (tools/verify.py), include/ is not: the path is relative to this header.
#ifndef EE_EELUAAPI_H
#define EE_EELUAAPI_H

extern "C" {
#include "../../src/ThirdParty/lua/lua.h"
#include "../../src/ThirdParty/lua/lauxlib.h"
#include "../../src/ThirdParty/lua/lualib.h"
}

#endif
