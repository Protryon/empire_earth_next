// Tests of the computer player's Lua runtime (ee/EECPLua.h, docs/AI_LUA_PLAN.md §3), the same source on every
// system: built and run by `python -I tools/xbuild.py luatest` with the vendored Lua (src/ThirdParty/lua/ee_lua.c)
// and the platform layer (PlatFOpen). The argument is the folder that plays the part of Data (tests/platform/lua/,
// whose ai/ holds the test scripts). Prints one line per failed check and a summary; the exit code is the number of
// failed checks (capped at 100).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "ee/EECPLua.h"
#include "ee/EELuaApi.h"
#include "rust/ee_ai.h"

static int sChecks;
static int sFailures;
static const char* sSection = "";

#define CHECK(c) Check((c), #c, __LINE__)

static void Check(bool inOk, const char* inText, int inLine)
{
    sChecks++;
    if (!inOk) {
        sFailures++;
        printf("FAIL [%s] line %d: %s\n", sSection, inLine, inText);
        fflush(stdout);
    }
}

static void Section(const char* inName)
{
    sSection = inName;
    printf("-- %s\n", inName);
    fflush(stdout);
}

static std::string sFolder;
static std::vector<std::string> sPrinted;

static void Capture(void*, const char* inText)
{
    sPrinted.push_back(inText);
}

static EECPLuaConfig Config(uint64_t inSeed = 1)
{
    EECPLuaConfig c;
    c.mSeed = inSeed;
    c.mRead = EECPLuaReadFromFolder;
    c.mReadContext = (void*)sFolder.c_str();
    c.mWrite = Capture;
    return c;
}

// Runs inSource -> its result (a nil value when it fails)
static EECPLuaValue RunOk(EECPLuaState& s, const char* inSource)
{
    EECPLuaValue v;
    if (!s.Run(inSource, "test", v))
        printf("   (%s: %s)\n", EECPLuaStatusName(s.LastStatus()), s.LastError().c_str());
    return v;
}

static bool IsTrue(EECPLuaState& s, const char* inSource)
{
    EECPLuaValue v = RunOk(s, inSource);
    return v.GetType() == EECPLuaValue::kBool && v.AsBool();
}

static EECPLuaStatus RunStatus(EECPLuaState& s, const char* inSource)
{
    EECPLuaValue v;
    s.Run(inSource, "test", v);
    return s.LastStatus();
}

static std::string Hex(const std::string& inBytes)
{
    static const char kHex[] = "0123456789abcdef";
    std::string h;
    for (unsigned char c : inBytes) {
        h += kHex[c >> 4];
        h += kHex[c & 15];
    }
    return h;
}

static EECPLuaValue View(const char* inMode)
{
    EECPLuaValue v = EECPLuaValue::NewMap();
    v.Set("mode", EECPLuaValue::Str(inMode));
    return v;
}

static void TestSandbox()
{
    Section("sandbox");
    EECPLuaState s(Config());
    CHECK(s.IsOpen());
    CHECK(IsTrue(s, "return io == nil and os == nil and debug == nil and package == nil and coroutine == nil"));
    CHECK(IsTrue(s, "return load == nil and loadfile == nil and dofile == nil and loadstring == nil"));
    CHECK(IsTrue(s, "return collectgarbage == nil and warn == nil"));
    CHECK(IsTrue(s, "return string.dump == nil and string.pack == nil and string.unpack == nil and "
                    "string.packsize == nil and (''):len() == 0 and getmetatable('').__index.dump == nil"));
    CHECK(IsTrue(s, "return math.random == nil and math.randomseed == nil and math.sin == nil and math.cos == nil "
                    "and math.tan == nil and math.asin == nil and math.acos == nil and math.atan == nil and "
                    "math.exp == nil and math.log == nil"));
    CHECK(IsTrue(s, "return math.sqrt(16) == 4.0 and math.floor(2.5) == 2 and math.abs(-3) == 3 and "
                    "math.fmod(7, 3) == 1 and math.maxinteger + 1 == math.mininteger and math.ult(1, -1)"));
    CHECK(IsTrue(s, "return type(table.sort) == 'function' and type(utf8.char) == 'function' and "
                    "type(pcall) == 'function' and type(each) == 'function' and type(ai.random) == 'function'"));
    // no addresses in text
    CHECK(IsTrue(s, "return tostring({}) == 'table' and tostring(print) == 'function'"));
    CHECK(IsTrue(s, "return string.format('%s|%5s|%d', {}, 'ab', 3) == 'table|   ab|3'"));
    CHECK(RunStatus(s, "return string.format('%p', {})") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "return string.format('%a', 1.0)") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "return string.format('%q', 0.5)") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "return string.format('%f', 1/0)") == kEECPLuaRuntime);
    CHECK(IsTrue(s, "return string.format('%q', 5) == '5' and string.format('%.3f', 0.125) == '0.125'"));
    CHECK(IsTrue(s, "return tostring(0/0) == 'nan' and tostring(-(0/0)) == 'nan' and tostring(1/0) == 'inf' and "
                    "tostring(-1/0) == '-inf' and tostring(0.1) == '0.1' and tostring(3.0) == '3.0'"));
    // `^`: integral exponents only
    CHECK(IsTrue(s, "return 2^10 == 1024.0 and 2^-1 == 0.5 and (-3)^3 == -27.0 and 10^0 == 1.0"));
    CHECK(RunStatus(s, "local x = 0.5 return 2^x") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "return 2^0.5") != kEECPLuaOK);   // a constant: refused while it compiles
    // ASCII character classes, whatever the locale
    CHECK(IsTrue(s, "return ('\\xe9A'):upper() == '\\xe9A' and ('\\xe9A'):lower() == '\\xe9a' and "
                    "not ('\\xe9'):find('%a')"));
    // metatables: __gc and __mode refused, the rest allowed
    CHECK(RunStatus(s, "setmetatable({}, {__gc = function() end})") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "setmetatable({}, {__mode = 'k'})") == kEECPLuaRuntime);
    CHECK(IsTrue(s, "local t = setmetatable({}, {__index = function() return 7 end}) return t.x == 7"));
    // no bytecode
    CHECK(RunStatus(s, "\x1bLua") == kEECPLuaSyntax);
    CHECK(RunStatus(s, "return +") == kEECPLuaSyntax);
    // print goes to the writer
    sPrinted.clear();
    RunOk(s, "print('a', 1, 2.5, {}, nil, true)");
    CHECK(sPrinted.size() == 1 && sPrinted[0] == "a\t1\t2.5\ttable\tnil\ttrue");
    // table.sort with equal elements: the same order every time
    CHECK(IsTrue(s, "local t = {} for i = 1, 500 do t[i] = {k = i % 3, i = i} end "
                    "table.sort(t, function(a, b) return a.k < b.k end) "
                    "local u = {} for i = 1, 500 do u[i] = {k = i % 3, i = i} end "
                    "table.sort(u, function(a, b) return a.k < b.k end) "
                    "for i = 1, 500 do if t[i].i ~= u[i].i then return false end end return true"));
}

static void TestBudget()
{
    Section("budget");
    EECPLuaConfig c = Config();
    c.mBudget = 100000;
    EECPLuaState s(c);
    CHECK(RunStatus(s, "while true do end") == kEECPLuaBudget);
    CHECK(strstr(s.LastError().c_str(), "budget") != nullptr);
    CHECK(s.LastInstructions() > 100000 && s.LastInstructions() <= 100000 + kEECPLuaHookStep);
    // a script's pcall cannot swallow it
    CHECK(RunStatus(s, "while true do pcall(function() while true do end end) end") == kEECPLuaBudget);
    CHECK(RunStatus(s, "local ok = pcall(function() while true do end end) return ok") == kEECPLuaBudget);
    // the state stays usable, the budget is per call
    CHECK(IsTrue(s, "local n = 0 for i = 1, 1000 do n = n + i end return n == 500500"));
    // the same work counts the same
    RunOk(s, "local n = 0 for i = 1, 20000 do n = n + i end");
    int64_t theFirst = s.LastInstructions();
    RunOk(s, "local n = 0 for i = 1, 20000 do n = n + i end");
    CHECK(theFirst > 0 && s.LastInstructions() == theFirst);
    // the memory limit
    EECPLuaConfig m = Config();
    m.mMemoryLimit = 2u << 20;
    m.mBudget = 1000000000;
    EECPLuaState small(m);
    CHECK(small.IsOpen());
    CHECK(RunStatus(small, "local t = {} for i = 1, 10000000 do t[i] = {i} end") == kEECPLuaMemory);
    CHECK(RunStatus(small, "return ('x'):rep(100000000)") == kEECPLuaMemory);
    CHECK(small.MemoryUsed() <= m.mMemoryLimit && small.MemoryPeak() <= m.mMemoryLimit);
    CHECK(IsTrue(small, "return #('x'):rep(1000) == 1000"));
}

static void TestRandom()
{
    Section("random");
    EECPLuaState a(Config(EECPLuaSeed(1234, 2)));
    EECPLuaState b(Config(EECPLuaSeed(1234, 2)));
    EECPLuaState other(Config(EECPLuaSeed(1234, 3)));
    const char* kDraws = "local t = {} for i = 1, 200 do t[i] = ai.random(1, 100) end "
                         "for i = 201, 300 do t[i] = ai.random(6) end "
                         "for i = 301, 310 do t[i] = ai.random(-5, 5) end return t";
    EECPLuaValue x = RunOk(a, kDraws), y = RunOk(b, kDraws), z = RunOk(other, kDraws);
    CHECK(x.GetType() == EECPLuaValue::kArray && x.Size() == 310);
    CHECK(x == y);
    CHECK(x != z);
    bool theRange = x.Size() == 310;
    for (size_t i = 0; i < x.Size() && theRange; i++) {
        int64_t v = x.At(i).AsInt();
        theRange = i < 200 ? v >= 1 && v <= 100 : i < 300 ? v >= 1 && v <= 6 : v >= -5 && v <= 5;
    }
    CHECK(theRange);
    CHECK(a.RngState() == b.RngState());
    // SplitMix64 from 0: the first output is 0xe220a8397b1dcdaf (the reference sequence); the whole 64-bit range
    // adds it to mininteger
    EECPLuaState zero(Config(0));
    EECPLuaValue r = RunOk(zero, "return ai.random(math.mininteger, math.maxinteger)");
    CHECK(r.GetType() == EECPLuaValue::kInt && (uint64_t)r.AsInt() == 0xe220a8397b1dcdafull + 0x8000000000000000ull);
    CHECK(RunStatus(zero, "return ai.random(5, 1)") == kEECPLuaRuntime);
    CHECK(RunStatus(zero, "return ai.random(1.5)") == kEECPLuaRuntime);
    CHECK(EECPLuaSeed(7, 3) == ((7ull << 32) | 3));
}

static void TestEach()
{
    Section("each");
    EECPLuaState s(Config());
    EECPLuaValue v = RunOk(s, "local t = {b = 1, [3] = 1, a = 1, [1] = 1, aa = 1, [-5] = 1, [2] = 1, ['B'] = 1, ['\\xff'] = 1} "
                              "local o = {} for k, x in each(t) do o[#o + 1] = tostring(k) end "
                              "return table.concat(o, ',')");
    CHECK(v.GetType() == EECPLuaValue::kString && v.AsString() == "-5,1,2,3,B,a,aa,b,\xff");
    CHECK(IsTrue(s, "local k = ai.keys({z = 1, [10] = 2, [9] = 3}) return k[1] == 9 and k[2] == 10 and k[3] == 'z' and #k == 3"));
    CHECK(IsTrue(s, "local n = 0 for _ in each({}) do n = n + 1 end return n == 0"));
    // keys removed during the loop are skipped; values are the current ones
    CHECK(IsTrue(s, "local t = {1, 2, 3, 4} local o = {} for k, x in each(t) do o[#o + 1] = x t[k + 1] = nil end "
                    "return #o == 2 and o[1] == 1 and o[2] == 3"));
    CHECK(RunStatus(s, "for k in each({[true] = 1}) do end") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "for k in each({[1.5] = 1}) do end") == kEECPLuaRuntime);
    CHECK(RunStatus(s, "for k in each({[{}] = 1}) do end") == kEECPLuaRuntime);
    CHECK(IsTrue(s, "local t = {} t[2.0] = 'x' local k = ai.keys(t) return math.type(k[1]) == 'integer'"));
}

static void TestValues()
{
    Section("values");
    // the tree's own order and text
    EECPLuaValue m = EECPLuaValue::NewMap();
    m.Set("b", EECPLuaValue::NewArray()).Push(EECPLuaValue::Bool(true));
    m.Set(EECPLuaKey::Int(2), EECPLuaValue::Str("x"));
    m.Set("a", EECPLuaValue::Int(1));
    CHECK(m.ToText() == "{[2]=\"x\",a=1,b=[true]}");
    CHECK(m.Find("a") && m.Find("a")->AsInt() == 1 && !m.Find("c"));
    // the bytes, pinned: the same on every platform
    std::string theBytes;
    EECPLuaEncode(m, theBytes);
    CHECK(Hex(theBytes) == "0503000000" "030200000000000000" "040100000078" "040100000061" "030100000000000000"
                           "040100000062" "0501000000" "030100000000000000" "02");
    size_t theOffset = 0;
    EECPLuaValue d;
    std::string theError;
    CHECK(EECPLuaDecode(theBytes, theOffset, d, theError) && theOffset == theBytes.size() && d == m);
    // keys out of order ("b" before "a") are not canonical
    std::string theA, theB;
    EECPLuaEncode(EECPLuaValue::Str("a"), theA);
    EECPLuaEncode(EECPLuaValue::Int(1), theA);
    EECPLuaEncode(EECPLuaValue::Str("b"), theB);
    EECPLuaEncode(EECPLuaValue::Int(1), theB);
    std::string theSwapped = std::string("\x05\x02\x00\x00\x00", 5) + theB + theA;
    theOffset = 0;
    EECPLuaValue e;
    CHECK(!EECPLuaDecode(theSwapped, theOffset, e, theError));
    std::string theOrdered = std::string("\x05\x02\x00\x00\x00", 5) + theA + theB;
    theOffset = 0;
    CHECK(EECPLuaDecode(theOrdered, theOffset, e, theError) && e.ToText() == "{a=1,b=1}");

    // through Lua and back
    EECPLuaState s(Config());
    CHECK(s.LoadPersonality("test_personality"));
    EECPLuaValue theView = View("echo");
    EECPLuaValue thePayload = EECPLuaValue::NewMap();
    thePayload.Set("name", EECPLuaValue::Str(std::string("a\0b", 3)));
    thePayload.Set("big", EECPLuaValue::Int(INT64_MIN));
    thePayload.Set("flag", EECPLuaValue::Bool(false));
    EECPLuaValue& theList = thePayload.Set("list", EECPLuaValue::NewArray());
    for (int i = 0; i < 5; i++)
        theList.Push(EECPLuaValue::Int(i * 10));
    EECPLuaValue& theUnits = thePayload.Set("units", EECPLuaValue::NewMap());
    theUnits.Set(EECPLuaKey::Int(1007), EECPLuaValue::Str("clubman"));
    theUnits.Set(EECPLuaKey::Int(-3), EECPLuaValue::Str("tower"));
    thePayload.Set("empty", EECPLuaValue::NewMap());
    theView.Set("payload", thePayload);
    EECPLuaValue theOrders;
    CHECK(s.Init(theView, theOrders));
    CHECK(theOrders.ToText() == "{hello=\"hello none\"}");
    CHECK(s.Step(theView, theOrders));
    CHECK(theOrders == thePayload);
    if (theOrders != thePayload)
        printf("   got %s\n", theOrders.ToText().c_str());

    // floats in the orders are refused, an integral float too
    CHECK(!s.Step(View("float"), theOrders) && s.LastStatus() == kEECPLuaBadValue);
    CHECK(strstr(s.LastError().c_str(), "result.build.count") != nullptr);
    CHECK(!s.Step(View("intfloat"), theOrders) && s.LastStatus() == kEECPLuaBadValue);
    EECPLuaValue r;
    CHECK(!s.Run("return {f = print}", "f", r) && s.LastStatus() == kEECPLuaBadValue);
    CHECK(!s.Run("local t = {} t.t = t return t", "cycle", r) && s.LastStatus() == kEECPLuaBadValue);
    CHECK(!s.Run("return setmetatable({}, {})", "mt", r) && s.LastStatus() == kEECPLuaBadValue);
    CHECK(s.Run("return {}", "empty", r) && r.GetType() == EECPLuaValue::kMap && r.FieldCount() == 0);
    CHECK(s.Run("return {1, nil, 3}", "holes", r) && r.GetType() == EECPLuaValue::kMap && r.FieldCount() == 2);

    // sorted keys from the step
    EECPLuaValue theKeys = View("keys");
    EECPLuaValue& p = theKeys.Set("payload", EECPLuaValue::NewMap());
    p.Set("zeta", EECPLuaValue::Int(1));
    p.Set(EECPLuaKey::Int(40), EECPLuaValue::Int(1));
    p.Set("alpha", EECPLuaValue::Int(1));
    CHECK(s.Step(theKeys, theOrders) && theOrders.ToText() == "[40,\"alpha\",\"zeta\"]");
}

static void TestErrors()
{
    Section("errors");
    EECPLuaState s(Config());
    EECPLuaValue theOrders;
    CHECK(!s.Step(View("echo"), theOrders) && s.LastStatus() == kEECPLuaNotLoaded);
    CHECK(!s.LoadPersonality("syntax_error") && s.LastStatus() == kEECPLuaSyntax);
    CHECK(strstr(s.LastError().c_str(), "ai/syntax_error.lua:") != nullptr);
    CHECK(!s.LoadPersonality("no_step") && s.LastStatus() == kEECPLuaRuntime);
    CHECK(!s.LoadPersonality("missing") && s.LastStatus() == kEECPLuaNoFile);
    CHECK(s.LoadPersonality("test_personality") && s.Init(View("none"), theOrders));
    CHECK(!s.Step(View("error"), theOrders) && s.LastStatus() == kEECPLuaRuntime);
    CHECK(strstr(s.LastError().c_str(), "ai/test_personality.lua:") != nullptr &&
          strstr(s.LastError().c_str(), "step failed on purpose") != nullptr);
    printf("   a runtime error reads: %s\n", s.LastError().c_str());
    EECPLuaConfig c = Config();
    c.mBudget = 50000;
    EECPLuaState t(c);
    CHECK(t.LoadPersonality("test_personality") && t.Init(View("none"), theOrders));
    CHECK(!t.Step(View("loop"), theOrders) && t.LastStatus() == kEECPLuaBudget);
    printf("   a budget error reads: %s\n", t.LastError().c_str());
    CHECK(t.Step(View("none"), theOrders) && theOrders.Find("steps") && theOrders.Find("steps")->AsInt() == 2);
}

static void TestRequire()
{
    Section("require");
    EECPLuaState s(Config());
    CHECK(IsTrue(s, "local u = require('lib.util') return u.greet('x') == 'hello x' and require('lib.util') == u"));
    CHECK(RunStatus(s, "return require('secret')") == kEECPLuaNoFile);   // Data/secret.lua is outside ai/
    CHECK(RunStatus(s, "return require('..secret')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('../secret')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('lib/util')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('lib\\\\util')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('C:')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('lib.')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('')") == kEECPLuaNoFile);
    CHECK(RunStatus(s, "return require('syntax_error')") == kEECPLuaSyntax);
    // no reader: nothing to load
    EECPLuaConfig c = Config();
    c.mRead = nullptr;
    EECPLuaState none(c);
    CHECK(RunStatus(none, "return require('lib.util')") == kEECPLuaNoFile);
}

static void TestMemory()
{
    Section("memory");
    EECPLuaState s(Config(EECPLuaSeed(99, 2)));
    CHECK(s.LoadPersonality("test_personality"));
    EECPLuaValue theView = EECPLuaValue::NewMap();
    theView.Set("name", EECPLuaValue::Str("boom"));
    EECPLuaValue theOrders;
    CHECK(s.Init(theView, theOrders));
    for (int i = 0; i < 5; i++)
        CHECK(s.Step(View("none"), theOrders));
    std::string theBytes;
    CHECK(s.SaveMemory(theBytes));
    CHECK(theBytes.size() > 13 && theBytes.compare(0, 4, "EELM") == 0 && theBytes[4] == 1);

    // a loaded game: a new state, the scripts from scratch, the memory and the RNG back
    EECPLuaState t(Config(12345));
    CHECK(t.LoadPersonality("test_personality"));
    CHECK(t.RestoreMemory(theBytes));
    std::string theAgain;
    CHECK(t.SaveMemory(theAgain) && theAgain == theBytes);
    CHECK(t.RngState() == s.RngState());
    // both go on the same way
    EECPLuaValue a, b;
    CHECK(s.Step(View("none"), a) && t.Step(View("none"), b) && a == b);
    CHECK(a.Find("steps") && a.Find("steps")->AsInt() == 6);
    std::string x, y;
    CHECK(s.SaveMemory(x) && t.SaveMemory(y) && x == y);
    EECPLuaValue theMemory;
    CHECK(t.GetMemory(theMemory) && theMemory.Find("name") && theMemory.Find("name")->AsString() == "boom");

    // what cannot be saved
    CHECK(s.Step(View("badmemory"), a));
    CHECK(!s.SaveMemory(x) && s.LastStatus() == kEECPLuaBadValue && strstr(s.LastError().c_str(), "memory.bad"));
    CHECK(s.Step(View("fixmemory"), a) && s.SaveMemory(x));
    // mCheckMemory reports it at the step
    EECPLuaConfig c = Config();
    c.mCheckMemory = true;
    EECPLuaState checked(c);
    CHECK(checked.LoadPersonality("test_personality") && checked.Init(theView, a));
    CHECK(!checked.Step(View("badmemory"), a) && checked.LastStatus() == kEECPLuaBadValue);

    // bad bytes are refused, and the memory stays as it was
    std::string theBroken = theBytes.substr(0, theBytes.size() - 1);
    CHECK(!t.RestoreMemory(theBroken) && t.LastStatus() == kEECPLuaBadValue);
    CHECK(!t.RestoreMemory(theBytes + "x") && t.LastStatus() == kEECPLuaBadValue);
    CHECK(!t.RestoreMemory("EELM") && t.LastStatus() == kEECPLuaBadValue);
    CHECK(t.SaveMemory(x) && x == y);

    // Reload keeps the memory
    CHECK(t.Reload() && t.SaveMemory(x) && x == y);
}

// the extension hook (EECPLuaConfig::mOpen), Charge and CallOptional (docs/AI_LUA_UNITS.md: the unit functions)
static int LuaCost(lua_State* L)
{
    EECPLuaState::From(L)->Charge(L, (int64_t)luaL_checkinteger(L, 1));
    lua_pushinteger(L, 7);
    return 1;
}

// a per-unit binding's argument reader (ee_ai units_lua.rs): a script's __index may raise through it
static int LuaOptX(lua_State* L)
{
    long v = -1;
    bool theFound = ee_ai_units_opt_int(L, 1, "x", &v);
    lua_pushinteger(L, theFound ? (lua_Integer)v : -1);
    return 1;
}

static void OpenTest(lua_State* L, void* inContext)
{
    lua_pushcfunction(L, LuaCost);
    lua_setfield(L, -2, (const char*)inContext);
    lua_pushcfunction(L, LuaOptX);
    lua_setfield(L, -2, "optx");
}

static void TestExtension()
{
    Section("extension");
    EECPLuaConfig c = Config();
    c.mOpen = OpenTest;
    c.mOpenContext = (void*)"cost";
    c.mBudget = 100000;
    EECPLuaState s(c);
    CHECK(s.IsOpen());
    CHECK(IsTrue(s, "return ai.cost(10) == 7 and type(ai.random) == 'function'"));
    // a charge counts against the budget like instructions, and a pcall cannot swallow it
    CHECK(RunStatus(s, "return ai.cost(200000)") == kEECPLuaBudget);
    CHECK(RunStatus(s, "local ok = pcall(ai.cost, 200000) return 1") == kEECPLuaBudget);
    CHECK(RunStatus(s, "for i = 1, 20 do ai.cost(4000) end return 1") == kEECPLuaOK);
    CHECK(RunStatus(s, "for i = 1, 30 do ai.cost(4000) end return 1") == kEECPLuaBudget);
    // a Lua error raised inside a Rust helper (an __index of the script's) is the script's error, not an abort
    CHECK(IsTrue(s, "return ai.optx({x = 4}) == 4 and ai.optx({}) == -1 and ai.optx(5) == -1"));
    CHECK(RunStatus(s, "return ai.optx(setmetatable({}, {__index = function() error('no x') end}))") == kEECPLuaRuntime);
    CHECK(strstr(s.LastError().c_str(), "no x") != nullptr);
    CHECK(IsTrue(s, "local ok = pcall(ai.optx, setmetatable({}, {__index = function() error('no') end})) return not ok"));
    CHECK(RunStatus(s, "return ai.optx(setmetatable({}, {__index = function() while true do end end}))") == kEECPLuaBudget);
    // an optional entry: found or not, with its own budget
    EECPLuaValue a;
    bool theFound = true;
    CHECK(s.LoadPersonality("test_personality") && s.Init(View("plain"), a));
    CHECK(s.CallOptional("micro", View("plain"), a, 1000, theFound) && !theFound && a.IsNil());
    CHECK(s.CallOptional("step", View("plain"), a, 0, theFound) && theFound);
    CHECK(!s.CallOptional("step", View("loop"), a, 20000, theFound) && s.LastStatus() == kEECPLuaBudget);
    CHECK(s.Step(View("plain"), a));   // the config's budget is back
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: lua_test <folder playing Data, with a trailing separator>\n");
        return 100;
    }
    sFolder = argv[1];
    if (!sFolder.empty() && sFolder.back() != '/' && sFolder.back() != '\\')
        sFolder += '/';
    TestSandbox();
    TestBudget();
    TestRandom();
    TestEach();
    TestValues();
    TestErrors();
    TestRequire();
    TestMemory();
    TestExtension();
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return sFailures > 100 ? 100 : sFailures;
}
