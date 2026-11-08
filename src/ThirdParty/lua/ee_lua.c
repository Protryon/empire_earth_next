/* Lua 5.4.7 (MIT, Lua.org, PUC-Rio; the sources beside this file are unmodified, from github.com/lua/lua tag v5.4.7)
 * as one unit, in the order of Lua's own onelua.c, for the computer player's strategy scripts (docs/AI_LUA_PLAN.md
 * §3). Only what a sandboxed, deterministic script needs:
 *   - the core and the auxiliary library;
 *   - the base, table, string, math and utf8 libraries (the AI runtime removes what a script must not use);
 *   - no io, os, package/loadlib, debug or coroutine library, and no linit (the runtime opens the libraries itself).
 * The string hash seed is fixed: the original seeds it from the time and an address, which would give every peer a
 * different table iteration order (lockstep). */
#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include "luaconf.h"

/* every internal function and variable static (as onelua.c) */
#define LUAI_FUNC static
#define LUAI_DDEC(def) /* empty */
#define LUAI_DDEF static

#define luai_makeseed(L) ((unsigned int)0x5eed5eedu)

#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"

#include "lauxlib.c"

#include "lbaselib.c"
#include "lmathlib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
