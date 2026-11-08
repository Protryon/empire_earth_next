/* Lua 5.4.7 (MIT, Lua.org, PUC-Rio; the sources beside this file are unmodified, from github.com/lua/lua tag v5.4.7)
 * as one unit, in the order of Lua's own onelua.c, for the computer player's strategy scripts (docs/AI_LUA_PLAN.md
 * §3). Only what a sandboxed, deterministic script needs:
 *   - the core and the auxiliary library;
 *   - the base, table, string, math and utf8 libraries (the AI runtime removes what a script must not use);
 *   - no io, os, package/loadlib, debug or coroutine library, and no linit (the runtime opens the libraries itself).
 * Determinism (lockstep: every peer and every platform computes the same; docs/AI_LUA_PLAN.md §3.1):
 *   - the string hash seed is fixed: the original seeds it from the time and an address, which would give every
 *     peer a different table iteration order;
 *   - table.sort never randomises its pivot (the original reads clock() and time() after an unbalanced partition,
 *     which changes the order of elements that compare equal);
 *   - `^` (luai_numpow) is square-and-multiply for an integral exponent and an error for any other: libm's pow is
 *     not correctly rounded and differs between the platforms' C libraries (a constant `2^0.5` fails to compile);
 *   - a float's text (tostring, `..`, print) spells NaN and the infinities "nan", "inf" and "-inf" (the C libraries
 *     differ: UCRT prints "-nan(ind)"); finite floats keep %.14g, correctly rounded by every C library we build with;
 *   - the auxiliary, base and string libraries classify and case-map ASCII only: the game sets a Windows locale
 *     whose ctype knows Latin-1 letters, POSIX stays in the C locale.
 * Lua's own files stay unmodified: these are macros Lua leaves open, defined here. */
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
#define l_randomizePivot() (~0u)

#include "lua.h"
static lua_Number ee_lua_numpow(lua_State *L, lua_Number a, lua_Number b);
#define luai_numpow(L, a, b) ee_lua_numpow(L, a, b)
static int ee_lua_number2str(char *s, size_t sz, lua_Number n);
#undef lua_number2str
#define lua_number2str(s, sz, n) ee_lua_number2str(s, sz, n)

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

/* the libraries' character classes: ASCII, whatever the C locale (the core has its own table, lctype.c) */
static int ee_isdigit(int c) { return (unsigned)(c - '0') < 10u; }
static int ee_islower(int c) { return (unsigned)(c - 'a') < 26u; }
static int ee_isupper(int c) { return (unsigned)(c - 'A') < 26u; }
static int ee_isalpha(int c) { return ee_islower(c) || ee_isupper(c); }
static int ee_isalnum(int c) { return ee_isalpha(c) || ee_isdigit(c); }
static int ee_isspace(int c) { return c == ' ' || (unsigned)(c - '\t') < 5u; }
static int ee_iscntrl(int c) { return (unsigned)c < 32u || c == 127; }
static int ee_isgraph(int c) { return (unsigned)(c - 33) < 94u; }
static int ee_ispunct(int c) { return ee_isgraph(c) && !ee_isalnum(c); }
static int ee_isxdigit(int c) { return ee_isdigit(c) || (unsigned)(c - 'a') < 6u || (unsigned)(c - 'A') < 6u; }
static int ee_tolower(int c) { return ee_isupper(c) ? c + 32 : c; }
static int ee_toupper(int c) { return ee_islower(c) ? c - 32 : c; }
#undef isdigit
#define isdigit ee_isdigit
#undef islower
#define islower ee_islower
#undef isupper
#define isupper ee_isupper
#undef isalpha
#define isalpha ee_isalpha
#undef isalnum
#define isalnum ee_isalnum
#undef isspace
#define isspace ee_isspace
#undef iscntrl
#define iscntrl ee_iscntrl
#undef isgraph
#define isgraph ee_isgraph
#undef ispunct
#define ispunct ee_ispunct
#undef isxdigit
#define isxdigit ee_isxdigit
#undef tolower
#define tolower ee_tolower
#undef toupper
#define toupper ee_toupper

#include "lauxlib.c"

#include "lbaselib.c"
#include "lmathlib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"

/* x^y: square-and-multiply for an integral exponent (|y| < 2^53), else an error */
static lua_Number ee_lua_numpow(lua_State *L, lua_Number a, lua_Number b) {
  lua_Number r = 1;
  unsigned long long e;
  int negative = b < 0;
  if (b == 2)
    return a * a;
  if (!(b == l_mathop(floor)(b)) || l_mathop(fabs)(b) >= 9007199254740992.0)   /* NaN fails the first test */
    luaG_runerror(L, "exponent is not an integer ('^' is integral only: pow differs between platforms)");
  e = (unsigned long long)l_mathop(fabs)(b);
  while (e) {
    if (e & 1u)
      r *= a;
    e >>= 1;
    if (e)
      a *= a;
  }
  return negative ? 1 / r : r;
}

/* a float's text: %.14g (LUA_NUMBER_FMT), with one spelling of NaN and the infinities */
static int ee_lua_number2str(char *s, size_t sz, lua_Number n) {
  if (n != n)
    return l_sprintf(s, sz, "%s", "nan");
  if (n == (lua_Number)HUGE_VAL)
    return l_sprintf(s, sz, "%s", "inf");
  if (n == -(lua_Number)HUGE_VAL)
    return l_sprintf(s, sz, "%s", "-inf");
  return l_sprintf(s, sz, LUA_NUMBER_FMT, (LUAI_UACNUMBER)n);
}
