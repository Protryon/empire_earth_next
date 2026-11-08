// The computer player's Lua runtime (ee/EECPLua.h; docs/AI_LUA_PLAN.md §3, "Runtime as built"). New in this port
// (no original address).
//
// Lua reports errors with longjmp (it is compiled as C). The functions that Lua calls, and the ones they call while
// a Lua error can be raised, hold no C++ object with a destructor; every C++ object lives outside lua_pcall or in a
// helper that returns before Lua can raise (Require's file text, ToValue's results).
#include "ee/EECPLua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "ee/EELuaApi.h"
#include "platform/PlatFile.h"

// ---- the value tree

EECPLuaKey EECPLuaKey::Int(int64_t inValue)
{
    EECPLuaKey k;
    k.mIsInt = true;
    k.mInt = inValue;
    return k;
}

EECPLuaKey EECPLuaKey::Str(const std::string& inValue)
{
    EECPLuaKey k;
    k.mIsInt = false;
    k.mString = inValue;
    return k;
}

bool EECPLuaKey::operator<(const EECPLuaKey& inOther) const
{
    if (this->mIsInt != inOther.mIsInt)
        return this->mIsInt;
    if (this->mIsInt)
        return this->mInt < inOther.mInt;
    return this->mString < inOther.mString;   // std::string compares bytes as unsigned char
}

bool EECPLuaKey::operator==(const EECPLuaKey& inOther) const
{
    return this->mIsInt == inOther.mIsInt &&
           (this->mIsInt ? this->mInt == inOther.mInt : this->mString == inOther.mString);
}

EECPLuaValue EECPLuaValue::Bool(bool inValue)
{
    EECPLuaValue v;
    v.mType = kBool;
    v.mBool = inValue;
    return v;
}

EECPLuaValue EECPLuaValue::Int(int64_t inValue)
{
    EECPLuaValue v;
    v.mType = kInt;
    v.mInt = inValue;
    return v;
}

EECPLuaValue EECPLuaValue::Str(const std::string& inValue)
{
    EECPLuaValue v;
    v.mType = kString;
    v.mString = inValue;
    return v;
}

EECPLuaValue EECPLuaValue::NewArray()
{
    EECPLuaValue v;
    v.mType = kArray;
    return v;
}

EECPLuaValue EECPLuaValue::NewMap()
{
    EECPLuaValue v;
    v.mType = kMap;
    return v;
}

EECPLuaValue& EECPLuaValue::Push(const EECPLuaValue& inValue)
{
    this->mItems.push_back(inValue);
    return this->mItems.back();
}

EECPLuaValue& EECPLuaValue::Set(const EECPLuaKey& inKey, const EECPLuaValue& inValue)
{
    std::vector<EECPLuaKey>::iterator it = std::lower_bound(this->mKeys.begin(), this->mKeys.end(), inKey);
    size_t i = (size_t)(it - this->mKeys.begin());
    if (it != this->mKeys.end() && *it == inKey) {
        this->mItems[i] = inValue;
        return this->mItems[i];
    }
    this->mKeys.insert(it, inKey);
    this->mItems.insert(this->mItems.begin() + (ptrdiff_t)i, inValue);
    return this->mItems[i];
}

const EECPLuaValue* EECPLuaValue::Find(const EECPLuaKey& inKey) const
{
    std::vector<EECPLuaKey>::const_iterator it = std::lower_bound(this->mKeys.begin(), this->mKeys.end(), inKey);
    if (it == this->mKeys.end() || !(*it == inKey))
        return nullptr;
    return &this->mItems[(size_t)(it - this->mKeys.begin())];
}

bool EECPLuaValue::operator==(const EECPLuaValue& inOther) const
{
    if (this->mType != inOther.mType)
        return false;
    switch (this->mType) {
    case kNil:
        return true;
    case kBool:
        return this->mBool == inOther.mBool;
    case kInt:
        return this->mInt == inOther.mInt;
    case kString:
        return this->mString == inOther.mString;
    default:
        return this->mKeys == inOther.mKeys && this->mItems == inOther.mItems;
    }
}

static void QuoteText(const std::string& inText, std::string& ioOut)
{
    static const char kHex[] = "0123456789abcdef";
    ioOut += '"';
    for (unsigned char c : inText) {
        if (c == '"' || c == '\\') {
            ioOut += '\\';
            ioOut += (char)c;
        } else if (c < 32 || c > 126) {
            ioOut += "\\x";
            ioOut += kHex[c >> 4];
            ioOut += kHex[c & 15];
        } else {
            ioOut += (char)c;
        }
    }
    ioOut += '"';
}

static bool IsIdentifier(const std::string& inText)
{
    if (inText.empty() || (inText[0] >= '0' && inText[0] <= '9'))
        return false;
    for (char c : inText)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

std::string EECPLuaValue::ToText() const
{
    char theNumber[32];
    switch (this->mType) {
    case kNil:
        return "nil";
    case kBool:
        return this->mBool ? "true" : "false";
    case kInt:
        snprintf(theNumber, sizeof(theNumber), "%lld", (long long)this->mInt);
        return theNumber;
    case kString: {
        std::string s;
        QuoteText(this->mString, s);
        return s;
    }
    case kArray: {
        std::string s = "[";
        for (size_t i = 0; i < this->mItems.size(); i++)
            s += (i ? "," : "") + this->mItems[i].ToText();
        return s + "]";
    }
    default: {
        std::string s = "{";
        for (size_t i = 0; i < this->mKeys.size(); i++) {
            if (i)
                s += ',';
            const EECPLuaKey& k = this->mKeys[i];
            if (k.mIsInt) {
                snprintf(theNumber, sizeof(theNumber), "[%lld]", (long long)k.mInt);
                s += theNumber;
            } else if (IsIdentifier(k.mString)) {
                s += k.mString;
            } else {
                s += '[';
                QuoteText(k.mString, s);
                s += ']';
            }
            s += '=' + this->mItems[i].ToText();
        }
        return s + "}";
    }
    }
}

// ---- the memory's bytes

enum { kTagNil = 0, kTagFalse = 1, kTagTrue = 2, kTagInt = 3, kTagString = 4, kTagTable = 5 };

static void PutU32(std::string& ioOut, uint32_t inValue)
{
    for (int i = 0; i < 4; i++)
        ioOut += (char)(unsigned char)(inValue >> (8 * i));
}

static void PutU64(std::string& ioOut, uint64_t inValue)
{
    for (int i = 0; i < 8; i++)
        ioOut += (char)(unsigned char)(inValue >> (8 * i));
}

static void EncodeKey(const EECPLuaKey& inKey, std::string& ioOut)
{
    if (inKey.mIsInt) {
        ioOut += (char)kTagInt;
        PutU64(ioOut, (uint64_t)inKey.mInt);
    } else {
        ioOut += (char)kTagString;
        PutU32(ioOut, (uint32_t)inKey.mString.size());
        ioOut += inKey.mString;
    }
}

void EECPLuaEncode(const EECPLuaValue& inValue, std::string& outBytes)
{
    switch (inValue.GetType()) {
    case EECPLuaValue::kNil:
        outBytes += (char)kTagNil;
        return;
    case EECPLuaValue::kBool:
        outBytes += (char)(inValue.AsBool() ? kTagTrue : kTagFalse);
        return;
    case EECPLuaValue::kInt:
        outBytes += (char)kTagInt;
        PutU64(outBytes, (uint64_t)inValue.AsInt());
        return;
    case EECPLuaValue::kString:
        outBytes += (char)kTagString;
        PutU32(outBytes, (uint32_t)inValue.AsString().size());
        outBytes += inValue.AsString();
        return;
    case EECPLuaValue::kArray: {   // the keys 1..n; a nil item is a hole (no entry), as in Lua
        uint32_t theCount = 0;
        for (size_t i = 0; i < inValue.Size(); i++)
            theCount += inValue.At(i).IsNil() ? 0 : 1;
        outBytes += (char)kTagTable;
        PutU32(outBytes, theCount);
        for (size_t i = 0; i < inValue.Size(); i++) {
            if (inValue.At(i).IsNil())
                continue;
            EncodeKey(EECPLuaKey::Int((int64_t)i + 1), outBytes);
            EECPLuaEncode(inValue.At(i), outBytes);
        }
        return;
    }
    case EECPLuaValue::kMap: {
        uint32_t theCount = 0;
        for (size_t i = 0; i < inValue.FieldCount(); i++)
            theCount += inValue.ValueAt(i).IsNil() ? 0 : 1;
        outBytes += (char)kTagTable;
        PutU32(outBytes, theCount);
        for (size_t i = 0; i < inValue.FieldCount(); i++) {
            if (inValue.ValueAt(i).IsNil())
                continue;
            EncodeKey(inValue.KeyAt(i), outBytes);
            EECPLuaEncode(inValue.ValueAt(i), outBytes);
        }
        return;
    }
    }
}

static bool GetU32(const std::string& inBytes, size_t& ioOffset, uint32_t& outValue)
{
    if (ioOffset > inBytes.size() || inBytes.size() - ioOffset < 4)
        return false;
    outValue = 0;
    for (int i = 0; i < 4; i++)
        outValue |= (uint32_t)(unsigned char)inBytes[ioOffset + i] << (8 * i);
    ioOffset += 4;
    return true;
}

static bool GetU64(const std::string& inBytes, size_t& ioOffset, uint64_t& outValue)
{
    if (ioOffset > inBytes.size() || inBytes.size() - ioOffset < 8)
        return false;
    outValue = 0;
    for (int i = 0; i < 8; i++)
        outValue |= (uint64_t)(unsigned char)inBytes[ioOffset + i] << (8 * i);
    ioOffset += 8;
    return true;
}

static bool DecodeValue(const std::string& inBytes, size_t& ioOffset, EECPLuaValue& outValue, std::string& outError,
                        int inDepth)
{
    if (ioOffset >= inBytes.size()) {
        outError = "truncated";
        return false;
    }
    int theTag = (unsigned char)inBytes[ioOffset++];
    switch (theTag) {
    case kTagNil:
        outValue = EECPLuaValue();
        return true;
    case kTagFalse:
    case kTagTrue:
        outValue = EECPLuaValue::Bool(theTag == kTagTrue);
        return true;
    case kTagInt: {
        uint64_t v;
        if (!GetU64(inBytes, ioOffset, v)) {
            outError = "truncated integer";
            return false;
        }
        outValue = EECPLuaValue::Int((int64_t)v);
        return true;
    }
    case kTagString: {
        uint32_t n;
        if (!GetU32(inBytes, ioOffset, n) || inBytes.size() - ioOffset < n) {
            outError = "truncated string";
            return false;
        }
        outValue = EECPLuaValue::Str(inBytes.substr(ioOffset, n));
        ioOffset += n;
        return true;
    }
    case kTagTable: {
        if (inDepth >= kEECPLuaMaxDepth) {
            outError = "nested too deep";
            return false;
        }
        uint32_t n;
        if (!GetU32(inBytes, ioOffset, n) || inBytes.size() - ioOffset < (size_t)n * 2) {
            outError = "truncated table";
            return false;
        }
        EECPLuaValue theMap = EECPLuaValue::NewMap();
        bool theArray = n > 0;
        for (uint32_t i = 0; i < n; i++) {
            EECPLuaValue k, v;
            if (!DecodeValue(inBytes, ioOffset, k, outError, inDepth + 1))
                return false;
            if (k.GetType() != EECPLuaValue::kInt && k.GetType() != EECPLuaValue::kString) {
                outError = "a table key that is not an integer or a string";
                return false;
            }
            EECPLuaKey theKey = k.GetType() == EECPLuaValue::kInt ? EECPLuaKey::Int(k.AsInt())
                                                                  : EECPLuaKey::Str(k.AsString());
            if (theMap.FieldCount() && !(theMap.KeyAt(theMap.FieldCount() - 1) < theKey)) {
                outError = "table keys out of order";
                return false;
            }
            if (!DecodeValue(inBytes, ioOffset, v, outError, inDepth + 1))
                return false;
            if (v.IsNil()) {
                outError = "a nil table value";
                return false;
            }
            theArray = theArray && theKey.mIsInt && theKey.mInt == (int64_t)i + 1;
            theMap.Set(theKey, v);
        }
        if (theArray) {
            outValue = EECPLuaValue::NewArray();
            for (size_t i = 0; i < theMap.FieldCount(); i++)
                outValue.Push(theMap.ValueAt(i));
        } else {
            outValue = theMap;
        }
        return true;
    }
    default:
        outError = "unknown tag";
        return false;
    }
}

bool EECPLuaDecode(const std::string& inBytes, size_t& ioOffset, EECPLuaValue& outValue, std::string& outError)
{
    return DecodeValue(inBytes, ioOffset, outValue, outError, 0);
}

// ---- Lua tables -> the tree (no Lua error can be raised here: it runs outside lua_pcall)

namespace {

struct Converter {
    lua_State* mL;
    std::vector<const void*> mOpen;   // the tables on the current path (a cycle), compared, never ordered
    std::string mPath;
    std::string mError;

    bool Fail(const char* inWhat)
    {
        this->mError = (this->mPath.empty() ? std::string("the value") : this->mPath) + ": " + inWhat;
        return false;
    }

    bool Convert(int inIndex, EECPLuaValue& outValue)
    {
        lua_State* L = this->mL;
        switch (lua_type(L, inIndex)) {
        case LUA_TNIL:
            outValue = EECPLuaValue();
            return true;
        case LUA_TBOOLEAN:
            outValue = EECPLuaValue::Bool(lua_toboolean(L, inIndex) != 0);
            return true;
        case LUA_TNUMBER:
            if (!lua_isinteger(L, inIndex))
                return this->Fail("a float (only integers, strings, booleans and tables)");
            outValue = EECPLuaValue::Int((int64_t)lua_tointeger(L, inIndex));
            return true;
        case LUA_TSTRING: {
            size_t n = 0;
            const char* s = lua_tolstring(L, inIndex, &n);
            outValue = EECPLuaValue::Str(std::string(s, n));
            return true;
        }
        case LUA_TTABLE:
            return this->Table(lua_absindex(L, inIndex), outValue);
        default:
            return this->Fail((std::string("a ") + lua_typename(L, lua_type(L, inIndex)) +
                               " (only integers, strings, booleans and tables)").c_str());
        }
    }

    bool Table(int inIndex, EECPLuaValue& outValue)
    {
        lua_State* L = this->mL;
        const void* theTable = lua_topointer(L, inIndex);
        if (std::find(this->mOpen.begin(), this->mOpen.end(), theTable) != this->mOpen.end())
            return this->Fail("a cycle");
        if ((int)this->mOpen.size() >= kEECPLuaMaxDepth)
            return this->Fail("nested too deep");
        if (lua_getmetatable(L, inIndex)) {
            lua_pop(L, 1);
            return this->Fail("a table with a metatable");
        }
        if (!lua_checkstack(L, 4))
            return this->Fail("no Lua stack left");
        this->mOpen.push_back(theTable);
        std::vector<std::pair<EECPLuaKey, EECPLuaValue>> theFields;
        int theTop = lua_gettop(L);
        size_t thePathLength = this->mPath.size();
        bool theOK = true;
        lua_pushnil(L);
        while (theOK && lua_next(L, inIndex)) {
            EECPLuaKey theKey;
            int theKeyType = lua_type(L, -2);
            char theText[32];
            if (theKeyType == LUA_TNUMBER && lua_isinteger(L, -2)) {
                theKey = EECPLuaKey::Int((int64_t)lua_tointeger(L, -2));
                snprintf(theText, sizeof(theText), "[%lld]", (long long)theKey.mInt);
                this->mPath += theText;
            } else if (theKeyType == LUA_TSTRING) {
                size_t n = 0;
                const char* s = lua_tolstring(L, -2, &n);
                theKey = EECPLuaKey::Str(std::string(s, n));
                this->mPath += "." + theKey.mString;
            } else {
                theOK = this->Fail(theKeyType == LUA_TNUMBER ? "a float key" : "a key that is not an integer or a string");
                break;
            }
            EECPLuaValue theValue;
            theOK = this->Convert(lua_gettop(L), theValue);
            if (theOK)
                theFields.push_back(std::make_pair(theKey, theValue));
            this->mPath.resize(thePathLength);
            lua_pop(L, 1);
        }
        lua_settop(L, theTop);
        this->mOpen.pop_back();
        if (!theOK)
            return false;
        std::sort(theFields.begin(), theFields.end(),
                  [](const std::pair<EECPLuaKey, EECPLuaValue>& a, const std::pair<EECPLuaKey, EECPLuaValue>& b) {
                      return a.first < b.first;
                  });
        bool theArray = !theFields.empty();
        for (size_t i = 0; i < theFields.size() && theArray; i++)
            theArray = theFields[i].first.mIsInt && theFields[i].first.mInt == (int64_t)i + 1;
        outValue = theArray ? EECPLuaValue::NewArray() : EECPLuaValue::NewMap();
        for (size_t i = 0; i < theFields.size(); i++) {
            if (theArray)
                outValue.Push(theFields[i].second);
            else
                outValue.Set(theFields[i].first, theFields[i].second);   // in order: appends
        }
        return true;
    }
};

}  // namespace

// ---- the tree -> Lua (inside lua_pcall: may raise a memory error)

static void PushKey(lua_State* L, const EECPLuaKey& inKey)
{
    if (inKey.mIsInt)
        lua_pushinteger(L, (lua_Integer)inKey.mInt);
    else
        lua_pushlstring(L, inKey.mString.data(), inKey.mString.size());
}

static void PushValue(lua_State* L, const EECPLuaValue& inValue, int inDepth)
{
    if (inDepth > kEECPLuaMaxDepth)
        luaL_error(L, "the value is nested too deep");
    luaL_checkstack(L, 4, "the value");
    switch (inValue.GetType()) {
    case EECPLuaValue::kNil:
        lua_pushnil(L);
        return;
    case EECPLuaValue::kBool:
        lua_pushboolean(L, inValue.AsBool());
        return;
    case EECPLuaValue::kInt:
        lua_pushinteger(L, (lua_Integer)inValue.AsInt());
        return;
    case EECPLuaValue::kString:
        lua_pushlstring(L, inValue.AsString().data(), inValue.AsString().size());
        return;
    case EECPLuaValue::kArray:
        lua_createtable(L, (int)inValue.Size(), 0);
        for (size_t i = 0; i < inValue.Size(); i++) {
            PushValue(L, inValue.At(i), inDepth + 1);
            lua_rawseti(L, -2, (lua_Integer)i + 1);
        }
        return;
    case EECPLuaValue::kMap:
        lua_createtable(L, 0, (int)inValue.FieldCount());
        for (size_t i = 0; i < inValue.FieldCount(); i++) {
            PushKey(L, inValue.KeyAt(i));
            PushValue(L, inValue.ValueAt(i), inDepth + 1);
            lua_rawset(L, -3);
        }
        return;
    }
}

// ---- the runtime's Lua functions

static const char* const kLoadedKey = "ee.ai.loaded";   // registry: module name -> its value (false while loading)

// tostring without addresses: a table, function or userdata without __tostring is its type's name
static const char* DeterministicToString(lua_State* L, int inIndex)
{
    inIndex = lua_absindex(L, inIndex);
    switch (lua_type(L, inIndex)) {
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
    case LUA_TTHREAD:
        if (luaL_getmetafield(L, inIndex, "__tostring") == LUA_TNIL) {
            lua_pushstring(L, luaL_typename(L, inIndex));
            return lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        break;
    default:
        break;
    }
    return luaL_tolstring(L, inIndex, nullptr);
}

static int LuaToString(lua_State* L)
{
    luaL_checkany(L, 1);
    DeterministicToString(L, 1);
    return 1;
}

static int LuaPrint(lua_State* L)
{
    int n = lua_gettop(L);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (int i = 1; i <= n; i++) {
        if (i > 1)
            luaL_addchar(&b, '\t');
        DeterministicToString(L, i);
        luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
    EECPLuaState::From(L)->Write(lua_tostring(L, -1));
    return 0;
}

// string.format, refusing what prints differently per platform: %p (an address), %a (hex floats), %q of a float
// (hex floats too) and a non-finite float in a float conversion. %s of a table and the like prints its type's name.
static int LuaFormat(lua_State* L)
{
    size_t n = 0;
    const char* f = luaL_checklstring(L, 1, &n);
    int theArgument = 1;
    for (size_t i = 0; i < n; i++) {
        if (f[i] != '%')
            continue;
        if (++i < n && f[i] == '%')
            continue;
        while (i < n && strchr("-+ #0123456789.", f[i]) != nullptr)
            i++;
        if (i >= n)
            break;
        char c = f[i];
        theArgument++;
        if (c == 'p')
            return luaL_error(L, "string.format: %%p prints an address");
        if (c == 'a' || c == 'A')
            return luaL_error(L, "string.format: %%%c differs between platforms", c);
        if (theArgument > lua_gettop(L))
            continue;   // the original reports the missing argument
        bool isFloat = lua_type(L, theArgument) == LUA_TNUMBER && !lua_isinteger(L, theArgument);
        if (c == 'q' && isFloat)
            return luaL_error(L, "string.format: %%q of a float differs between platforms");
        if (strchr("eEfFgG", c) != nullptr && lua_type(L, theArgument) == LUA_TNUMBER) {
            lua_Number v = lua_tonumber(L, theArgument);
            if (v != v || v - v != 0)
                return luaL_error(L, "string.format: %%%c of nan or inf differs between platforms", c);
        }
        if (c == 's' && lua_type(L, theArgument) != LUA_TSTRING && lua_type(L, theArgument) != LUA_TNUMBER) {
            DeterministicToString(L, theArgument);
            lua_replace(L, theArgument);
        }
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, 1);
    return 1;
}

// setmetatable, refusing __gc and __mode: finalisers and weak tables run when the collector decides
static int LuaSetMetatable(lua_State* L)
{
    if (lua_type(L, 2) == LUA_TTABLE) {
        static const char* const kRefused[] = {"__gc", "__mode"};
        for (const char* theName : kRefused) {
            lua_pushstring(L, theName);
            if (lua_rawget(L, 2) != LUA_TNIL)
                return luaL_error(L, "setmetatable: %s is not allowed (the collector's timing is not deterministic)",
                                  theName);
            lua_pop(L, 1);
        }
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, 1);
    return 1;
}

namespace {
struct SortEntry {
    int mIsString;
    lua_Integer mInt;
    const char* mString;
    size_t mLength;
    lua_Integer mIndex;   // in the collected keys
};

bool SortLess(const SortEntry& a, const SortEntry& b)
{
    if (a.mIsString != b.mIsString)
        return !a.mIsString;
    if (!a.mIsString)
        return a.mInt < b.mInt;
    int c = memcmp(a.mString, b.mString, a.mLength < b.mLength ? a.mLength : b.mLength);
    return c != 0 ? c < 0 : a.mLength < b.mLength;
}
}  // namespace

// Pushes an array of t's keys in the total order: integers ascending, then strings by bytes
static void PushSortedKeys(lua_State* L, int inTable)
{
    inTable = lua_absindex(L, inTable);
    luaL_checktype(L, inTable, LUA_TTABLE);
    lua_newtable(L);   // the keys as found
    int theKeys = lua_gettop(L);
    lua_Integer n = 0;
    lua_pushnil(L);
    while (lua_next(L, inTable)) {
        lua_pop(L, 1);
        int t = lua_type(L, -1);
        if (t == LUA_TNUMBER && !lua_isinteger(L, -1))
            luaL_error(L, "each: a float key (keys are integers or strings)");
        if (t != LUA_TNUMBER && t != LUA_TSTRING)
            luaL_error(L, "each: a %s key (keys are integers or strings)", luaL_typename(L, -1));
        lua_pushvalue(L, -1);
        lua_rawseti(L, theKeys, ++n);
    }
    SortEntry* e = (SortEntry*)lua_newuserdatauv(L, (size_t)n * sizeof(SortEntry), 0);
    for (lua_Integer i = 0; i < n; i++) {
        lua_rawgeti(L, theKeys, i + 1);   // the keys table keeps the string alive (and in place)
        e[i].mIsString = lua_type(L, -1) == LUA_TSTRING;
        e[i].mInt = e[i].mIsString ? 0 : lua_tointeger(L, -1);
        e[i].mString = e[i].mIsString ? lua_tolstring(L, -1, &e[i].mLength) : nullptr;
        if (!e[i].mIsString)
            e[i].mLength = 0;
        e[i].mIndex = i + 1;
        lua_pop(L, 1);
    }
    std::sort(e, e + n, SortLess);
    lua_createtable(L, (int)n, 0);
    for (lua_Integer i = 0; i < n; i++) {
        lua_rawgeti(L, theKeys, e[i].mIndex);
        lua_rawseti(L, -2, i + 1);
    }
    lua_replace(L, theKeys);   // the sorted array in the keys' place
    lua_settop(L, theKeys);
}

static int LuaEachNext(lua_State* L)
{
    lua_Integer i = lua_tointeger(L, lua_upvalueindex(3));
    for (;;) {
        i++;
        if (lua_rawgeti(L, lua_upvalueindex(2), i) == LUA_TNIL)
            return 0;
        lua_pushvalue(L, -1);
        if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {   // a key removed meanwhile is skipped
            lua_pushinteger(L, i);
            lua_replace(L, lua_upvalueindex(3));
            return 2;
        }
        lua_pop(L, 2);
    }
}

// for k, v in each(t): t's keys in the total order (the keys are taken when the loop starts)
static int LuaEach(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
    PushSortedKeys(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, LuaEachNext, 3);
    return 1;
}

static int LuaKeys(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
    PushSortedKeys(L, 1);
    return 1;
}

// ai.random(n): 1..n; ai.random(lo, hi): lo..hi (uniform: rejection sampling)
static int LuaRandom(lua_State* L)
{
    lua_Integer lo, hi;
    if (lua_gettop(L) >= 2) {
        lo = luaL_checkinteger(L, 1);
        hi = luaL_checkinteger(L, 2);
    } else {
        lo = 1;
        hi = luaL_checkinteger(L, 1);
    }
    luaL_argcheck(L, lo <= hi, lua_gettop(L) >= 2 ? 2 : 1, "interval is empty");
    EECPLuaState* s = EECPLuaState::From(L);
    uint64_t theRange = (uint64_t)hi - (uint64_t)lo + 1u;   // 0: the whole 64-bit range
    uint64_t x = s->NextRandom();
    if (theRange != 0) {
        uint64_t theThreshold = (0u - theRange) % theRange;   // 2^64 mod range: the draws below it are biased
        while (x < theThreshold)
            x = s->NextRandom();
        x %= theRange;
    }
    lua_pushinteger(L, (lua_Integer)((uint64_t)lo + x));
    return 1;
}

static bool ModuleName(const char* inName)
{
    size_t n = strlen(inName);
    if (n == 0 || n > 96)
        return false;
    bool theStart = true;
    for (size_t i = 0; i < n; i++) {
        char c = inName[i];
        if (c == '.') {
            if (theStart)
                return false;
            theStart = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            theStart = false;
        } else {
            return false;
        }
    }
    return !theStart;
}

// Reads ai\<name>.lua and compiles it (text only) -> the lua_load status, -1 when there is no such file. Returns
// before Lua can raise, so its string is gone by then.
static int LoadModule(lua_State* L, const char* inPath, const char* inChunkName)
{
    std::string theText;
    if (!EECPLuaState::From(L)->ReadModule(inPath, theText))
        return -1;
    return luaL_loadbufferx(L, theText.data(), theText.size(), inChunkName, "t");
}

// require(name): Data/ai/<name with dots as folders>.lua, loaded once per state
static int LuaRequire(lua_State* L)
{
    const char* theName = luaL_checkstring(L, 1);
    lua_settop(L, 1);
    if (!ModuleName(theName)) {
        EECPLuaState::From(L)->mNoFile = true;
        return luaL_error(L, "require: '%s' is not a module name (letters, digits, _ and dots: Data/ai/a/b.lua is 'a.b')",
                          theName);
    }
    lua_getfield(L, LUA_REGISTRYINDEX, kLoadedKey);   // 2
    lua_getfield(L, 2, theName);
    if (lua_toboolean(L, -1))
        return 1;
    if (lua_type(L, -1) == LUA_TBOOLEAN)
        return luaL_error(L, "require: '%s' requires itself", theName);
    lua_pop(L, 1);
    char thePath[128];
    char theChunk[128];
    snprintf(thePath, sizeof(thePath), "ai\\%s.lua", theName);
    snprintf(theChunk, sizeof(theChunk), "@ai/%s.lua", theName);
    for (char* p = thePath + 3; *p; p++)
        if (*p == '.' && strcmp(p, ".lua") != 0)
            *p = '\\';
    for (char* p = theChunk + 4; *p; p++)
        if (*p == '.' && strcmp(p, ".lua") != 0)
            *p = '/';
    int theStatus = LoadModule(L, thePath, theChunk);
    if (theStatus == -1) {
        EECPLuaState::From(L)->mNoFile = true;
        return luaL_error(L, "require: no module '%s' (Data/%s)", theName, theChunk + 1);
    }
    if (theStatus != LUA_OK) {
        EECPLuaState::From(L)->mSyntax = theStatus == LUA_ERRSYNTAX;
        return lua_error(L);
    }
    lua_pushboolean(L, 0);   // loading
    lua_setfield(L, 2, theName);
    lua_pushvalue(L, 1);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pushnil(L);
        lua_setfield(L, 2, theName);
        return lua_error(L);
    }
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, 2, theName);
    return 1;
}

static void LuaHook(lua_State* L, lua_Debug*)
{
    EECPLuaState::From(L)->OnHook(L);
}

static void* LuaAllocate(void* inState, void* inBlock, size_t inOld, size_t inNew)
{
    return ((EECPLuaState*)inState)->Allocate(inBlock, inOld, inNew);
}

static int LuaPanic(lua_State* L)
{
    const char* m = lua_tostring(L, -1);
    fprintf(stderr, "EECPLua: unprotected Lua error: %s\n", m ? m : "?");
    abort();
    return 0;
}

// the message handler: the error as text (no addresses) with a traceback
static int LuaMessage(lua_State* L)
{
    const char* m = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : DeterministicToString(L, 1);
    luaL_traceback(L, L, m, 1);
    return 1;
}

// ---- the protected calls

namespace {
enum JobKind { kJobSetup, kJobRequire, kJobEntry, kJobRun, kJobRestore };
struct Job {
    JobKind mKind;
    const char* mText;           // the module, the entry's name or the source
    size_t mLength;
    const char* mChunkName;
    const EECPLuaValue* mValue;  // the view, or the restored memory
    bool mReset;                 // require: forget the loaded modules first
    bool mOptional;              // entry: a missing function is no error
    bool mFound;
    int* mPersonality;
    int* mMemory;
};

const char* const kMathRemoved[] = {"random", "randomseed", "sin", "cos", "tan", "asin", "acos", "atan", "exp", "log"};
const char* const kBaseRemoved[] = {"dofile", "loadfile", "load", "collectgarbage", "warn"};
const char* const kStringRemoved[] = {"dump", "pack", "packsize", "unpack"};

void Setup(lua_State* L, Job& j)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_settop(L, 0);
    lua_pushglobaltable(L);   // 1
    for (const char* n : kBaseRemoved) {
        lua_pushnil(L);
        lua_setfield(L, 1, n);
    }
    lua_pushcfunction(L, LuaPrint);
    lua_setfield(L, 1, "print");
    lua_pushcfunction(L, LuaToString);
    lua_setfield(L, 1, "tostring");
    lua_getfield(L, 1, "setmetatable");
    lua_pushcclosure(L, LuaSetMetatable, 1);
    lua_setfield(L, 1, "setmetatable");
    lua_pushcfunction(L, LuaRequire);
    lua_setfield(L, 1, "require");
    lua_pushcfunction(L, LuaEach);
    lua_setfield(L, 1, "each");
    lua_newtable(L);
    lua_pushcfunction(L, LuaRandom);
    lua_setfield(L, -2, "random");
    lua_pushcfunction(L, LuaKeys);
    lua_setfield(L, -2, "keys");
    lua_setfield(L, 1, "ai");
    lua_getfield(L, 1, LUA_STRLIBNAME);   // 2 (also the strings' __index)
    for (const char* n : kStringRemoved) {
        lua_pushnil(L);
        lua_setfield(L, 2, n);
    }
    lua_getfield(L, 2, "format");
    lua_pushcclosure(L, LuaFormat, 1);
    lua_setfield(L, 2, "format");
    lua_getfield(L, 1, LUA_MATHLIBNAME);   // 3
    for (const char* n : kMathRemoved) {
        lua_pushnil(L);
        lua_setfield(L, 3, n);
    }
    lua_settop(L, 0);
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kLoadedKey);
    lua_newtable(L);
    *j.mMemory = luaL_ref(L, LUA_REGISTRYINDEX);
}

int RunJob(lua_State* L)
{
    Job& j = *(Job*)lua_touserdata(L, 1);
    lua_settop(L, 0);
    switch (j.mKind) {
    case kJobSetup:
        Setup(L, j);
        return 0;
    case kJobRequire:
        if (j.mReset) {
            lua_newtable(L);
            lua_setfield(L, LUA_REGISTRYINDEX, kLoadedKey);
        }
        lua_pushcfunction(L, LuaRequire);
        lua_pushstring(L, j.mText);
        lua_call(L, 1, 1);
        if (!lua_istable(L, 1))
            return luaL_error(L, "personality '%s': the script returns a %s, not a table", j.mText, luaL_typename(L, 1));
        if (lua_getfield(L, 1, "step") != LUA_TFUNCTION)
            return luaL_error(L, "personality '%s': no step function", j.mText);
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, *j.mPersonality);
        *j.mPersonality = luaL_ref(L, LUA_REGISTRYINDEX);
        return 0;
    case kJobEntry:
        lua_rawgeti(L, LUA_REGISTRYINDEX, *j.mPersonality);
        if (lua_getfield(L, 1, j.mText) != LUA_TFUNCTION) {
            if (j.mOptional) {
                j.mFound = false;
                return 0;
            }
            return luaL_error(L, "the personality has no %s function", j.mText);
        }
        j.mFound = true;
        PushValue(L, *j.mValue, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, *j.mMemory);
        lua_call(L, 2, 1);
        return 1;
    case kJobRun: {
        int theStatus = luaL_loadbufferx(L, j.mText, j.mLength, j.mChunkName, "t");
        if (theStatus != LUA_OK) {
            EECPLuaState::From(L)->mSyntax = theStatus == LUA_ERRSYNTAX;
            return lua_error(L);
        }
        lua_call(L, 0, 1);
        return 1;
    }
    case kJobRestore:
        PushValue(L, *j.mValue, 0);
        luaL_unref(L, LUA_REGISTRYINDEX, *j.mMemory);
        *j.mMemory = luaL_ref(L, LUA_REGISTRYINDEX);
        return 0;
    }
    return 0;
}

Job NewJob(JobKind inKind, int* inPersonality, int* inMemory)
{
    Job j;
    memset(&j, 0, sizeof(j));
    j.mKind = inKind;
    j.mPersonality = inPersonality;
    j.mMemory = inMemory;
    return j;
}
}  // namespace

// ---- the state

const char* EECPLuaStatusName(EECPLuaStatus inStatus)
{
    switch (inStatus) {
    case kEECPLuaOK:
        return "ok";
    case kEECPLuaNoState:
        return "no Lua state";
    case kEECPLuaNotLoaded:
        return "no personality loaded";
    case kEECPLuaNoFile:
        return "no such module";
    case kEECPLuaSyntax:
        return "syntax error";
    case kEECPLuaRuntime:
        return "runtime error";
    case kEECPLuaBudget:
        return "budget exceeded";
    case kEECPLuaMemory:
        return "out of memory";
    case kEECPLuaBadValue:
        return "bad value";
    }
    return "?";
}

bool EECPLuaReadFromFolder(void* inContext, const char* inPath, std::string& outText)
{
    std::string thePath = std::string(inContext ? (const char*)inContext : "") + inPath;
    FILE* f = PlatFOpen(thePath.c_str(), "rb");
    if (f == nullptr)
        return false;
    outText.clear();
    char theBuffer[4096];
    size_t n;
    while ((n = fread(theBuffer, 1, sizeof(theBuffer), f)) > 0)
        outText.append(theBuffer, n);
    bool theOK = !ferror(f);
    fclose(f);
    return theOK;
}

uint64_t EECPLuaSeed(uint32_t inGameSeed, uint32_t inPlayer)
{
    return ((uint64_t)inGameSeed << 32) | inPlayer;
}

EECPLuaState* EECPLuaState::From(lua_State* inL)
{
    void* theState = nullptr;
    lua_getallocf(inL, &theState);
    return (EECPLuaState*)theState;
}

// SplitMix64 (Steele, Lea, Flood 2014): the state advances by the golden gamma; the output is its mix
uint64_t EECPLuaState::NextRandom()
{
    this->mRng += 0x9E3779B97F4A7C15ull;
    uint64_t z = this->mRng;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void EECPLuaState::Write(const char* inText)
{
    if (this->mConfig.mWrite)
        this->mConfig.mWrite(this->mConfig.mWriteContext, inText);
}

bool EECPLuaState::ReadModule(const char* inPath, std::string& outText)
{
    return this->mConfig.mRead != nullptr && this->mConfig.mRead(this->mConfig.mReadContext, inPath, outText);
}

void* EECPLuaState::Allocate(void* inBlock, size_t inOld, size_t inNew)
{
    if (inBlock == nullptr)
        inOld = 0;   // then it is the object's type
    if (inNew == 0) {
        free(inBlock);
        this->mBytes -= inOld;
        return nullptr;
    }
    if (inNew > inOld && this->mBytes - inOld + inNew > this->mConfig.mMemoryLimit) {
        this->mAllocFailed = true;
        return nullptr;   // Lua collects and tries once more, then raises "not enough memory"
    }
    void* theBlock = realloc(inBlock, inNew);
    if (theBlock == nullptr) {
        this->mAllocFailed = true;
        return nullptr;
    }
    this->mBytes = this->mBytes - inOld + inNew;
    if (this->mBytes > this->mPeak)
        this->mPeak = this->mBytes;
    return theBlock;
}

// Every kEECPLuaHookStep instructions. Over the budget, it raises and keeps raising at every instruction, so a
// script's pcall cannot swallow the abort.
void EECPLuaState::OnHook(lua_State* inL)
{
    this->mUsed += kEECPLuaHookStep;
    if (this->mOverBudget || this->mUsed > this->mConfig.mBudget) {
        if (!this->mOverBudget) {
            this->mOverBudget = true;
            lua_sethook(inL, LuaHook, LUA_MASKCOUNT, 1);
        }
        luaL_error(inL, "instruction budget exceeded (%I instructions)", (lua_Integer)this->mConfig.mBudget);
    }
}

EECPLuaState::EECPLuaState(const EECPLuaConfig& inConfig) : mConfig(inConfig), mRng(inConfig.mSeed)
{
    this->mPersonality = LUA_NOREF;
    this->mMemory = LUA_NOREF;
    this->mL = lua_newstate(LuaAllocate, this);
    if (this->mL == nullptr) {
        this->Fail(kEECPLuaNoState, "lua_newstate failed (memory limit too small?)");
        return;
    }
    lua_atpanic(this->mL, LuaPanic);
    Job j = NewJob(kJobSetup, &this->mPersonality, &this->mMemory);
    if (!this->Call((void*)&j, nullptr, "setup")) {
        std::string theError = this->mError;
        lua_close(this->mL);
        this->mL = nullptr;
        this->Fail(kEECPLuaNoState, theError);
    }
}

EECPLuaState::~EECPLuaState()
{
    if (this->mL)
        lua_close(this->mL);
}

bool EECPLuaState::Fail(EECPLuaStatus inStatus, const std::string& inError)
{
    this->mStatus = inStatus;
    this->mError = inError;
    return false;
}

// Runs a job in lua_pcall with the budget; with outResult, converts the job's result
bool EECPLuaState::Call(void* inJob, EECPLuaValue* outResult, const char* inWhat)
{
    lua_State* L = this->mL;
    this->mStatus = kEECPLuaOK;
    this->mError.clear();
    int theBase = lua_gettop(L);
    lua_pushcfunction(L, LuaMessage);
    lua_pushcfunction(L, RunJob);
    lua_pushlightuserdata(L, inJob);
    this->mUsed = 0;
    this->mOverBudget = false;
    this->mNoFile = this->mSyntax = this->mAllocFailed = false;
    lua_sethook(L, LuaHook, LUA_MASKCOUNT, kEECPLuaHookStep);
    int theStatus = lua_pcall(L, 1, 1, theBase + 1);
    lua_sethook(L, nullptr, 0, 0);
    bool theOK = true;
    if (theStatus != LUA_OK) {
        const char* m = lua_tostring(L, -1);
        std::string theMessage = std::string(inWhat) + ": " + (m ? m : "error");
        // a memory error re-raised by require arrives as a runtime error with Lua's memory message
        bool theMemory = theStatus == LUA_ERRMEM ||
                         (this->mAllocFailed && strstr(theMessage.c_str(), "not enough memory") != nullptr);
        EECPLuaStatus s = this->mOverBudget ? kEECPLuaBudget
                          : theMemory       ? kEECPLuaMemory
                          : this->mSyntax   ? kEECPLuaSyntax
                          : this->mNoFile   ? kEECPLuaNoFile
                                            : kEECPLuaRuntime;
        theOK = this->Fail(s, theMessage);
    } else if (outResult) {
        Converter c;
        c.mL = L;
        c.mPath = "result";
        if (!c.Convert(-1, *outResult))
            theOK = this->Fail(kEECPLuaBadValue, std::string(inWhat) + ": " + c.mError);
    }
    lua_settop(L, theBase);
    return theOK;
}

bool EECPLuaState::LoadPersonality(const char* inModule)
{
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    this->mModule = inModule;
    Job j = NewJob(kJobRequire, &this->mPersonality, &this->mMemory);
    j.mText = this->mModule.c_str();
    return this->Call((void*)&j, nullptr, this->mModule.c_str());
}

bool EECPLuaState::Reload()
{
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    if (this->mModule.empty())
        return this->Fail(kEECPLuaNotLoaded, "no personality loaded");
    Job j = NewJob(kJobRequire, &this->mPersonality, &this->mMemory);
    j.mText = this->mModule.c_str();
    j.mReset = true;
    return this->Call((void*)&j, nullptr, this->mModule.c_str());
}

bool EECPLuaState::CallEntry(const char* inName, bool inOptional, const EECPLuaValue& inView, EECPLuaValue& outOrders)
{
    outOrders = EECPLuaValue();
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    if (this->mPersonality == LUA_NOREF || this->mPersonality == LUA_REFNIL)
        return this->Fail(kEECPLuaNotLoaded, "no personality loaded");
    Job j = NewJob(kJobEntry, &this->mPersonality, &this->mMemory);
    j.mText = inName;
    j.mOptional = inOptional;
    j.mValue = &inView;
    std::string theWhat = this->mModule + "." + inName;
    if (!this->Call((void*)&j, &outOrders, theWhat.c_str()))
        return false;
    if (!j.mFound)
        outOrders = EECPLuaValue();
    return !this->mConfig.mCheckMemory || this->CheckMemory();
}

bool EECPLuaState::Init(const EECPLuaValue& inView, EECPLuaValue& outOrders)
{
    return this->CallEntry("init", true, inView, outOrders);
}

bool EECPLuaState::Step(const EECPLuaValue& inView, EECPLuaValue& outOrders)
{
    return this->CallEntry("step", false, inView, outOrders);
}

bool EECPLuaState::Run(const char* inSource, const char* inName, EECPLuaValue& outResult)
{
    outResult = EECPLuaValue();
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    std::string theChunk = std::string("=") + inName;
    Job j = NewJob(kJobRun, &this->mPersonality, &this->mMemory);
    j.mText = inSource;
    j.mLength = strlen(inSource);
    j.mChunkName = theChunk.c_str();
    return this->Call((void*)&j, &outResult, inName);
}

bool EECPLuaState::GetMemory(EECPLuaValue& outMemory)
{
    outMemory = EECPLuaValue();
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    this->mStatus = kEECPLuaOK;
    this->mError.clear();
    lua_State* L = this->mL;
    int theBase = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, this->mMemory);
    Converter c;
    c.mL = L;
    c.mPath = "memory";
    bool theOK = c.Convert(-1, outMemory);
    lua_settop(L, theBase);
    if (!theOK)
        return this->Fail(kEECPLuaBadValue, c.mError);
    return true;
}

bool EECPLuaState::CheckMemory()
{
    EECPLuaValue theMemory;
    return this->GetMemory(theMemory);
}

static const char kMemoryMagic[4] = {'E', 'E', 'L', 'M'};
enum { kMemoryVersion = 1 };

bool EECPLuaState::SaveMemory(std::string& outBytes)
{
    outBytes.clear();
    EECPLuaValue theMemory;
    if (!this->GetMemory(theMemory))
        return false;
    outBytes.append(kMemoryMagic, 4);
    outBytes += (char)kMemoryVersion;
    PutU64(outBytes, this->mRng);
    EECPLuaEncode(theMemory, outBytes);
    return true;
}

bool EECPLuaState::RestoreMemory(const std::string& inBytes)
{
    if (!this->mL)
        return this->Fail(kEECPLuaNoState, "no Lua state");
    size_t theOffset = 5;
    uint64_t theRng = 0;
    if (inBytes.size() < 5 || memcmp(inBytes.data(), kMemoryMagic, 4) != 0 || inBytes[4] != (char)kMemoryVersion ||
        !GetU64(inBytes, theOffset, theRng))
        return this->Fail(kEECPLuaBadValue, "memory: not an EELM version 1 block");
    EECPLuaValue theMemory;
    std::string theError;
    if (!EECPLuaDecode(inBytes, theOffset, theMemory, theError))
        return this->Fail(kEECPLuaBadValue, "memory: " + theError);
    if (theOffset != inBytes.size())
        return this->Fail(kEECPLuaBadValue, "memory: bytes after the table");
    if (theMemory.GetType() != EECPLuaValue::kMap && theMemory.GetType() != EECPLuaValue::kArray)
        return this->Fail(kEECPLuaBadValue, "memory: not a table");
    Job j = NewJob(kJobRestore, &this->mPersonality, &this->mMemory);
    j.mValue = &theMemory;
    if (!this->Call((void*)&j, nullptr, "restore memory"))
        return false;
    this->mRng = theRng;
    return true;
}
