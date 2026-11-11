// The computer player's Lua runtime (docs/AI_LUA_PLAN.md §3, "Runtime as built"). New in this port (no original
// address).
//
// One EECPLuaState per computer player: a sandboxed, deterministic Lua 5.4 state (src/ThirdParty/lua) that runs a
// personality script's `step(view, memory)` and hands back its orders. The runtime knows nothing about the game's
// managers: the caller builds the view as an EECPLuaValue tree and reads the orders back as one.
//
// Determinism (the same input gives the same output on every peer and every platform):
//   - only the base (without dofile, loadfile, load, collectgarbage, warn), table, string (without dump, pack,
//     packsize, unpack) and math (integer and IEEE-exact functions only) libraries, utf8, and the runtime's own
//     functions (print, require, each, ai.*);
//   - an RNG per player (ai.random), SplitMix64 from the caller's seed; its state is saved with the memory;
//   - each(t) and ai.keys(t) visit keys in one total order (integers ascending, then strings by bytes);
//   - an instruction budget per call (a count hook): running over it aborts the call the same way everywhere;
//   - a memory limit per state (a counting allocator);
//   - the orders and the memory hold no floats; the memory's bytes are the same on every platform.
// Every entry point catches the script's errors (syntax, runtime, budget, memory) and returns false with the status
// and message in LastStatus() / LastError(); nothing a script does can crash the game.
#ifndef EE_EECPLUA_H
#define EE_EECPLUA_H

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

struct lua_State;

// ---- the value tree: what the caller passes in (the view) and gets back (the orders, the memory)

// A table key: an integer or a string. The order is the runtime's one total order: integers ascending, then strings
// by their bytes (a prefix first).
struct EECPLuaKey {
    bool mIsInt = true;
    int64_t mInt = 0;
    std::string mString;

    static EECPLuaKey Int(int64_t inValue);
    static EECPLuaKey Str(const std::string& inValue);
    bool operator<(const EECPLuaKey& inOther) const;
    bool operator==(const EECPLuaKey& inOther) const;
};

// nil, a boolean, an integer, a string, an array (a Lua table with the keys 1..n, n > 0) or a map (any other table:
// integer and string keys, kept in key order). An empty Lua table converts to an empty map.
class EECPLuaValue {
public:
    enum Type { kNil, kBool, kInt, kString, kArray, kMap };

    EECPLuaValue() {}
    static EECPLuaValue Bool(bool inValue);
    static EECPLuaValue Int(int64_t inValue);
    static EECPLuaValue Str(const std::string& inValue);
    static EECPLuaValue NewArray();
    static EECPLuaValue NewMap();

    Type GetType() const { return mType; }
    bool IsNil() const { return mType == kNil; }
    bool AsBool() const { return mBool; }
    int64_t AsInt() const { return mInt; }
    const std::string& AsString() const { return mString; }

    // an array's items (index 0 is Lua's 1)
    size_t Size() const { return mItems.size(); }
    const EECPLuaValue& At(size_t inIndex) const { return mItems[inIndex]; }
    EECPLuaValue& Push(const EECPLuaValue& inValue);   // an array: appends

    // a map's fields, in key order
    size_t FieldCount() const { return mKeys.size(); }
    const EECPLuaKey& KeyAt(size_t inIndex) const { return mKeys[inIndex]; }
    const EECPLuaValue& ValueAt(size_t inIndex) const { return mItems[inIndex]; }
    EECPLuaValue& Set(const EECPLuaKey& inKey, const EECPLuaValue& inValue);   // a map: inserts or replaces
    EECPLuaValue& Set(const char* inKey, const EECPLuaValue& inValue) { return Set(EECPLuaKey::Str(inKey), inValue); }
    const EECPLuaValue* Find(const EECPLuaKey& inKey) const;
    const EECPLuaValue* Find(const char* inKey) const { return Find(EECPLuaKey::Str(inKey)); }

    bool operator==(const EECPLuaValue& inOther) const;
    bool operator!=(const EECPLuaValue& inOther) const { return !(*this == inOther); }
    // deterministic text for traces and tests: nil, true, 12, "text", [1,2], {a=1,[3]=true}
    std::string ToText() const;

private:
    Type mType = kNil;
    bool mBool = false;
    int64_t mInt = 0;
    std::string mString;
    std::vector<EECPLuaKey> mKeys;        // a map's keys, sorted
    std::vector<EECPLuaValue> mItems;     // an array's items, or a map's values beside mKeys
};

// The memory's bytes (EECPLuaState::SaveMemory): canonical, so equal content gives equal bytes on every platform.
//   value   = 0x00 nil | 0x01 false | 0x02 true | 0x03 int64 (8 bytes, little endian, two's complement)
//           | 0x04 string (uint32 length, little endian, then the bytes)
//           | 0x05 table (uint32 count, then count pairs key value; the keys are 0x03 or 0x04 values in key order)
// An array is the table with the keys 1..n. Decoding rejects anything not canonical (keys out of order, trailing
// bytes, nesting deeper than kEECPLuaMaxDepth).
enum { kEECPLuaMaxDepth = 64 };
void EECPLuaEncode(const EECPLuaValue& inValue, std::string& outBytes);   // appends
bool EECPLuaDecode(const std::string& inBytes, size_t& ioOffset, EECPLuaValue& outValue, std::string& outError);

// ---- the state

enum EECPLuaStatus {
    kEECPLuaOK = 0,
    kEECPLuaNoState,        // lua_newstate failed, or the sandbox could not be set up
    kEECPLuaNotLoaded,      // Init/Step before LoadPersonality succeeded
    kEECPLuaNoFile,         // require: no such module, or a name outside Data/ai
    kEECPLuaSyntax,         // a script does not compile
    kEECPLuaRuntime,        // a script raised an error
    kEECPLuaBudget,         // the call ran over its instruction budget
    kEECPLuaMemory,         // the state ran out of its memory limit
    kEECPLuaBadValue        // a value that the tree or the memory cannot hold (a float, a function, a cycle ...)
};
const char* EECPLuaStatusName(EECPLuaStatus inStatus);   // "ok", "budget exceeded" ...

// Reads one script: inPath is relative to the data folder ("ai\boom.lua", "ai\lib\economy.lua"; the runtime builds
// it from a checked module name, so it never leaves ai\) -> false when there is no such file.
typedef bool (*EECPLuaReadFile)(void* inContext, const char* inPath, std::string& outText);
// One line of script output (print): the AI trace or the debug output.
typedef void (*EECPLuaWrite)(void* inContext, const char* inText);

// The default reader: inContext is a const char* folder that plays the part of Data (with a trailing separator, e.g.
// "Data\" relative to the data root, the game's current directory), read through PlatFOpen.
bool EECPLuaReadFromFolder(void* inContext, const char* inPath, std::string& outText);

// The RNG seed of a computer player: the game's seed and the player id (each player has its own stream).
uint64_t EECPLuaSeed(uint32_t inGameSeed, uint32_t inPlayer);

struct EECPLuaConfig {
    uint64_t mSeed = 0;                          // EECPLuaSeed(game seed, player)
    size_t mMemoryLimit = 32u << 20;             // bytes the state may hold (everything Lua allocates)
    int64_t mBudget = 2000000;                   // VM instructions per call (counted in steps of kEECPLuaHookStep)
    EECPLuaReadFile mRead = nullptr;             // nullptr: require finds nothing
    void* mReadContext = nullptr;
    EECPLuaWrite mWrite = nullptr;               // nullptr: print discards
    void* mWriteContext = nullptr;
    bool mCheckMemory = false;                   // after each Init/Step, fail when the memory holds what cannot be saved
    // called once while the state is set up, with the `ai` table on top of the stack: adds more functions to it
    // (docs/AI_LUA_UNITS.md: the next engine's unit queries and commands); it must leave the stack as it found it
    void (*mOpen)(lua_State* inL, void* inContext) = nullptr;
    void* mOpenContext = nullptr;
};
enum { kEECPLuaHookStep = 1000 };

class EECPLuaState {
public:
    explicit EECPLuaState(const EECPLuaConfig& inConfig);
    ~EECPLuaState();
    EECPLuaState(const EECPLuaState&) = delete;
    EECPLuaState& operator=(const EECPLuaState&) = delete;

    bool IsOpen() const { return this->mL != nullptr; }

    // require(inModule), which must return a table with a `step` function (and optionally `init`): the personality.
    bool LoadPersonality(const char* inModule);
    // Developer loop: forgets every loaded module and loads the personality again; the memory and the RNG stay.
    bool Reload();
    // A new game: personality.init(view, memory) when the script has one -> its return value converted (the orders).
    bool Init(const EECPLuaValue& inView, EECPLuaValue& outOrders);
    // One strategy step: personality.step(view, memory) -> its return value converted (the orders).
    bool Step(const EECPLuaValue& inView, EECPLuaValue& outOrders);
    // Runs a chunk of source text (mode "t") in the sandbox, with the budget -> its first result. Tests and a
    // developer console; the name is the chunk's name in error messages.
    bool Run(const char* inSource, const char* inName, EECPLuaValue& outResult);

    // Another entry of the personality, if it has one: personality[inName](inArg, memory) with inBudget (0: the
    // config's) -> its return value converted. outFound false: no such function (and true is returned).
    bool CallOptional(const char* inName, const EECPLuaValue& inArg, EECPLuaValue& outResult, int64_t inBudget, bool& outFound);
    // The memory and the RNG state as bytes ("EELM", version 1, the RNG state as uint64 LE, the memory table as an
    // EECPLuaEncode value) / restored from them (a loaded game: LoadPersonality first, then RestoreMemory, no Init).
    bool SaveMemory(std::string& outBytes);
    bool RestoreMemory(const std::string& inBytes);
    // The memory table converted (a test or a trace).
    bool GetMemory(EECPLuaValue& outMemory);

    EECPLuaStatus LastStatus() const { return this->mStatus; }
    const std::string& LastError() const { return this->mError; }
    int64_t LastInstructions() const { return this->mUsed; }    // the last call's count (a multiple of the hook step)
    int64_t Budget() const { return this->mConfig.mBudget; }    // the instructions a call may use
    size_t MemoryUsed() const { return this->mBytes; }
    size_t MemoryPeak() const { return this->mPeak; }
    uint64_t RngState() const { return this->mRng; }

    // ---- for the runtime's C functions (EECPLua.cpp)
    static EECPLuaState* From(lua_State* inL);
    uint64_t NextRandom();
    void Write(const char* inText);
    bool ReadModule(const char* inPath, std::string& outText);
    void* Allocate(void* inBlock, size_t inOld, size_t inNew);
    void OnHook(lua_State* inL);
    // A C function's cost in instruction equivalents (docs/AI_LUA_UNITS.md §2.1): added to the call's count, which
    // the budget bounds; over the budget it raises as the hook does
    void Charge(lua_State* inL, int64_t inAmount);
    void OpenExtension(lua_State* inL);   // the setup's call of mOpen
    bool mNoFile = false;           // this call: require found no module (or a bad name)
    bool mSyntax = false;           // this call: a chunk did not compile
    bool mAllocFailed = false;      // this call: the allocator refused a block

private:
    bool Fail(EECPLuaStatus inStatus, const std::string& inError);
    bool Call(void* inJob, EECPLuaValue* outResult, const char* inWhat);
    bool CallEntry(const char* inName, bool inOptional, const EECPLuaValue& inView, EECPLuaValue& outOrders);
    bool CheckMemory();

    EECPLuaConfig mConfig;
    lua_State* mL = nullptr;
    uint64_t mRng = 0;
    size_t mBytes = 0;
    size_t mPeak = 0;
    int64_t mUsed = 0;
    bool mOverBudget = false;
    int mPersonality = -2;          // registry reference of the personality table (LUA_NOREF: none)
    int mMemory = -2;               // registry reference of the memory table
    std::string mModule;
    EECPLuaStatus mStatus = kEECPLuaOK;
    std::string mError;
};

#endif
