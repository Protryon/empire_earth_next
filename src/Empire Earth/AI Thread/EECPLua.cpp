// The computer player's Lua runtime (ee/EECPLua.h; docs/AI_LUA_PLAN.md §3, "Runtime as built"). New in this port
// (no original address).
//
// The value tree (EECPLuaValue) is C++: callers build views and read orders with it. Its order, equality, text and
// bytes are Rust (ee_ai lua_value.rs), and so is the state (ee_ai lua_host.rs: the sandbox, the hook, the allocator,
// the runtime's Lua functions, the conversions), over the vendored Lua (src/ThirdParty/lua/ee_lua.c) that the C++
// build compiles. Rust reads and builds C++ trees, appends to C++ strings and reads scripts through
// kEECPLuaTreeOps; EECPLuaState is the handle C++ holds.
#include "ee/EECPLua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "ee/EELuaApi.h"
#include "platform/PlatFile.h"
#include "rust/ee_ai.h"   // ee_ai_lua_*: the tree's order, equality, text and bytes (lua_value.rs), the state (lua_host.rs)

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

// the order and equality are Rust's (ee_ai lua_value.rs)
static EeAiLuaKey KeyView(const EECPLuaKey& inKey)
{
    EeAiLuaKey k;
    k.is_int = inKey.mIsInt ? 1 : 0;
    k.integer = inKey.mInt;
    k.text = (const uint8_t*)inKey.mString.data();
    k.text_len = inKey.mString.size();
    return k;
}

bool EECPLuaKey::operator<(const EECPLuaKey& inOther) const
{
    EeAiLuaKey a = KeyView(*this), b = KeyView(inOther);
    return ee_ai_lua_key_less(&a, &b);
}

bool EECPLuaKey::operator==(const EECPLuaKey& inOther) const
{
    EeAiLuaKey a = KeyView(*this), b = KeyView(inOther);
    return ee_ai_lua_key_equal(&a, &b);
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

EECPLuaValue& EECPLuaValue::Push(EECPLuaValue&& inValue)
{
    this->mItems.push_back(std::move(inValue));
    return this->mItems.back();
}

EECPLuaValue& EECPLuaValue::Set(const EECPLuaKey& inKey, EECPLuaValue&& inValue)
{
    std::vector<EECPLuaKey>::iterator it = std::lower_bound(this->mKeys.begin(), this->mKeys.end(), inKey);
    size_t i = (size_t)(it - this->mKeys.begin());
    if (it != this->mKeys.end() && *it == inKey) {
        this->mItems[i] = std::move(inValue);
        return this->mItems[i];
    }
    this->mKeys.insert(it, inKey);
    this->mItems.insert(this->mItems.begin() + (ptrdiff_t)i, std::move(inValue));
    return this->mItems[i];
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

// ---- the tree as Rust reads and builds it (ee_ai lua_value.rs: Tree, CppTree, build)

static void TreeNode(const void* inValue, EeAiLuaNode* outNode)
{
    const EECPLuaValue& v = *(const EECPLuaValue*)inValue;
    EeAiLuaNode& n = *outNode;
    n.kind = (uint8_t)v.GetType();
    n.boolean = v.AsBool() ? 1 : 0;
    n.integer = v.AsInt();
    n.text = (const uint8_t*)v.AsString().data();
    n.text_len = v.AsString().size();
    bool isMap = v.GetType() == EECPLuaValue::kMap;
    n.count = isMap ? v.FieldCount() : v.GetType() == EECPLuaValue::kArray ? v.Size() : 0;
    n.items = n.count ? (const void*)(isMap ? &v.ValueAt(0) : &v.At(0)) : nullptr;
    n.item_stride = sizeof(EECPLuaValue);
    n.keys = isMap && n.count ? (const void*)&v.KeyAt(0) : nullptr;
    n.key_stride = sizeof(EECPLuaKey);
}

static void TreeKey(const void* inKey, EeAiLuaKey* outKey)
{
    *outKey = KeyView(*(const EECPLuaKey*)inKey);
}

static void TreeSet(void* inValue, uint8_t inKind, uint8_t inBool, int64_t inInt, const uint8_t* inText, size_t inLength)
{
    EECPLuaValue& v = *(EECPLuaValue*)inValue;
    switch (inKind) {
    case EECPLuaValue::kBool:
        v = EECPLuaValue::Bool(inBool != 0);
        break;
    case EECPLuaValue::kInt:
        v = EECPLuaValue::Int(inInt);
        break;
    case EECPLuaValue::kString:
        v = EECPLuaValue::Str(std::string((const char*)inText, inLength));
        break;
    case EECPLuaValue::kArray:
        v = EECPLuaValue::NewArray();
        break;
    case EECPLuaValue::kMap:
        v = EECPLuaValue::NewMap();
        break;
    default:
        v = EECPLuaValue();
        break;
    }
}

static void* TreePush(void* inArray)
{
    return &((EECPLuaValue*)inArray)->Push(EECPLuaValue());
}

static void* TreeField(void* inMap, const EeAiLuaKey* inKey)
{
    EECPLuaKey k = inKey->is_int ? EECPLuaKey::Int(inKey->integer)
                                 : EECPLuaKey::Str(std::string((const char*)inKey->text, inKey->text_len));
    return &((EECPLuaValue*)inMap)->Set(k, EECPLuaValue());
}

static void TreeAppend(void* inString, const uint8_t* inData, size_t inLength)
{
    ((std::string*)inString)->append((const char*)inData, inLength);
}

// require's reader: the config's mRead into the state's buffer, kept until the text has compiled
struct EECPLuaStateAccess {
    static bool Read(void* inOwner, const char* inPath, const uint8_t** outText, size_t* outLength)
    {
        EECPLuaState* s = (EECPLuaState*)inOwner;
        s->mReadText.clear();
        if (s->mConfig.mRead == nullptr || !s->mConfig.mRead(s->mConfig.mReadContext, inPath, s->mReadText))
            return false;
        *outText = (const uint8_t*)s->mReadText.data();
        *outLength = s->mReadText.size();
        return true;
    }

    static void ReadDone(void* inOwner) { std::string().swap(((EECPLuaState*)inOwner)->mReadText); }
};

static bool TreeRead(void* inOwner, const char* inPath, const uint8_t** outText, size_t* outLength)
{
    return EECPLuaStateAccess::Read(inOwner, inPath, outText, outLength);
}

static void TreeReadDone(void* inOwner)
{
    EECPLuaStateAccess::ReadDone(inOwner);
}

static const EeAiLuaTreeOps kEECPLuaTreeOps = {TreeNode, TreeKey, TreeSet, TreePush, TreeField, TreeAppend, TreeRead,
                                               TreeReadDone};

bool EECPLuaValue::operator==(const EECPLuaValue& inOther) const
{
    return ee_ai_lua_value_equal(&kEECPLuaTreeOps, this, &inOther);
}

std::string EECPLuaValue::ToText() const
{
    std::string s;
    ee_ai_lua_value_text(&kEECPLuaTreeOps, this, &s);
    return s;
}

// ---- the memory's bytes (ee_ai lua_value.rs: encode, decode)

void EECPLuaEncode(const EECPLuaValue& inValue, std::string& outBytes)
{
    ee_ai_lua_encode(&kEECPLuaTreeOps, &inValue, &outBytes);
}

bool EECPLuaDecode(const std::string& inBytes, size_t& ioOffset, EECPLuaValue& outValue, std::string& outError)
{
    std::string theError;
    if (ee_ai_lua_decode(&kEECPLuaTreeOps, (const uint8_t*)inBytes.data(), inBytes.size(), &ioOffset, &outValue,
                         &theError))
        return true;
    outError = theError;
    return false;
}

// ---- the state (ee_ai lua_host.rs)

const char* EECPLuaStatusName(EECPLuaStatus inStatus)
{
    return ee_ai_lua_status_name((uint32_t)inStatus);
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
    return ee_ai_lua_seed(inGameSeed, inPlayer);
}

// the facts of the Lua build the Rust binding (mlua-sys, lua54) assumes; ee_ai lua_host.rs checks its side
static_assert(LUA_VERSION_NUM == 504, "Lua 5.4");
static_assert(sizeof(lua_Integer) == 8 && sizeof(lua_Number) == 8, "64-bit integers and doubles");
static_assert(LUAI_MAXSTACK == 1000000 && LUA_REGISTRYINDEX == -1001000, "the stack size and the registry index");
static_assert(LUAL_BUFFERSIZE == 16 * sizeof(void*) * sizeof(lua_Number), "luaL_Buffer's initial size");
static_assert(LUA_EXTRASPACE == sizeof(void*) && LUA_IDSIZE == 60, "the extra space and the chunk id size");
static_assert(LUA_MASKCOUNT == 8 && LUA_NOREF == -2 && LUA_REFNIL == -1, "the count hook's mask, the references");
static_assert(sizeof(luaL_Buffer) == 4 * sizeof(void*) + LUAL_BUFFERSIZE, "luaL_Buffer's layout");
static_assert(kEECPLuaHookStep == 1000 && kEECPLuaMaxDepth == 64, "lua_host.rs HOOK_STEP, lua_value.rs MAX_DEPTH");

EECPLuaState* EECPLuaState::From(lua_State* inL)
{
    return (EECPLuaState*)ee_ai_lua_owner(inL);
}

uint64_t EECPLuaState::NextRandom()
{
    return ee_ai_lua_next_random(this->mHost);
}

void EECPLuaState::Charge(lua_State* inL, int64_t inAmount)
{
    ee_ai_lua_charge(inL, inAmount);
}

EECPLuaState::EECPLuaState(const EECPLuaConfig& inConfig) : mConfig(inConfig)
{
    EeAiLuaConfig c;
    c.seed = inConfig.mSeed;
    c.memory_limit = inConfig.mMemoryLimit;
    c.budget = inConfig.mBudget;
    c.has_read = inConfig.mRead != nullptr;
    c.write = inConfig.mWrite;
    c.write_context = inConfig.mWriteContext;
    c.check_memory = inConfig.mCheckMemory;
    c.open = (void*)inConfig.mOpen;   // called as C-unwind: it may raise a Lua error
    c.open_context = inConfig.mOpenContext;
    this->mHost = ee_ai_lua_new(&c, &kEECPLuaTreeOps, this);
    this->Sync(true);
}

EECPLuaState::~EECPLuaState()
{
    ee_ai_lua_free(this->mHost);
}

bool EECPLuaState::Sync(bool inOK)
{
    this->mStatus = (EECPLuaStatus)ee_ai_lua_status(this->mHost);
    this->mError.clear();
    ee_ai_lua_error(this->mHost, &this->mError);
    return inOK;
}

bool EECPLuaState::IsOpen() const
{
    return ee_ai_lua_is_open(this->mHost);
}

int64_t EECPLuaState::LastInstructions() const
{
    return ee_ai_lua_used(this->mHost);
}

int64_t EECPLuaState::Budget() const
{
    return ee_ai_lua_budget(this->mHost);
}

size_t EECPLuaState::MemoryUsed() const
{
    return ee_ai_lua_memory_used(this->mHost);
}

size_t EECPLuaState::MemoryPeak() const
{
    return ee_ai_lua_memory_peak(this->mHost);
}

uint64_t EECPLuaState::RngState() const
{
    return ee_ai_lua_rng_state(this->mHost);
}

bool EECPLuaState::LoadPersonality(const char* inModule)
{
    return this->Sync(ee_ai_lua_load(this->mHost, inModule, false));
}

bool EECPLuaState::Reload()
{
    return this->Sync(ee_ai_lua_load(this->mHost, "", true));
}

bool EECPLuaState::Init(const EECPLuaValue& inView, EECPLuaValue& outOrders)
{
    bool theFound = false;
    return this->Sync(ee_ai_lua_entry(this->mHost, "init", true, &inView, 0, &outOrders, &theFound));
}

bool EECPLuaState::Step(const EECPLuaValue& inView, EECPLuaValue& outOrders)
{
    bool theFound = false;
    return this->Sync(ee_ai_lua_entry(this->mHost, "step", false, &inView, 0, &outOrders, &theFound));
}

bool EECPLuaState::CallOptional(const char* inName, const EECPLuaValue& inArg, EECPLuaValue& outResult, int64_t inBudget,
    bool& outFound)
{
    return this->Sync(ee_ai_lua_entry(this->mHost, inName, true, &inArg, inBudget, &outResult, &outFound));
}

bool EECPLuaState::Run(const char* inSource, const char* inName, EECPLuaValue& outResult)
{
    return this->Sync(ee_ai_lua_run(this->mHost, inSource, strlen(inSource), inName, &outResult));
}

bool EECPLuaState::GetMemory(EECPLuaValue& outMemory)
{
    return this->Sync(ee_ai_lua_get_memory(this->mHost, &outMemory));
}

bool EECPLuaState::SaveMemory(std::string& outBytes)
{
    outBytes.clear();
    return this->Sync(ee_ai_lua_save_memory(this->mHost, &outBytes));
}

bool EECPLuaState::RestoreMemory(const std::string& inBytes)
{
    return this->Sync(ee_ai_lua_restore_memory(this->mHost, (const uint8_t*)inBytes.data(), inBytes.size()));
}
