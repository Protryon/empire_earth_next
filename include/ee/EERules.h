// ee/EERules.h (new, not in the original game): the game rules that were literals in the code, as data
// (docs/DATA_FORMATS.md 8.8). Rules documents next to the db tables, db\combat.yaml, db\gameoptions.yaml,
// db\civbuilder.yaml, db\civpowers.yaml, db\aipersonalities.yaml, plus each epoch's media in db\epochs.yaml. The
// fields and their defaults are generated from tools/eedata/rules_schema.json (ee_data rules_gen.rs); the reader is Rust
// (rust/crates/ee_data/src/rules.rs). Every member starts at the original game's value; a document on the data path
// sets every value it holds (references by id, resolved by the db tables' Content). Without the documents (the binary
// db tree) the game plays with these defaults, which equal the vanilla documents tools/eedata writes (eedata.py
// cppgate: the content dumps match). Portable C++17 (no engine headers): src/Empire Earth/Database/EERules.cpp, also
// built into the converter's gate.
#ifndef EE_EERULES_H
#define EE_EERULES_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

struct EeDataRules;

// A number that is a constant or one of dbworld's values: `fanaticism.F2` (the constant's id, then F, F2, L or L2:
// mFloatValue, mFloatValue2, mLongValue, mLongValue2). The game reads a world value through gWorldTable when it uses
// the number (EERulesWorld.h), so it follows the loaded dbworld.
struct EERuleNumber {
    float mFloat = 0.0f;                    // a constant (mWorldRow < 0): its value
    long mLong = 0;                         // ... as an integer (a float constant: truncated)
    long mWorldRow = -1;                    // a world value: the dbworld row
    int mWorldValue = 0;                    // 0 F, 1 F2, 2 L, 3 L2

    static EERuleNumber Constant(float inValue);
    static EERuleNumber World(long inRow, int inValue);
};

// How a hit's damage is formed (EEDamageForm): Rust's enums (ee_sim game_ids.rs), emitted back as plain C++ enums with their evidence comments.
#include "rust/sim/game_ids.h"

// A list the Rust rules own (ee_data rules.rs RuleListC): read like a const std::vector
template <class T>
struct EERuleList {
    const T* mData = nullptr;
    size_t mSize = 0;

    size_t size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    const T& operator[](size_t inIndex) const { return mData[inIndex]; }
    const T* begin() const { return mData; }
    const T* end() const { return mData + mSize; }
    bool contains(const T& inValue) const { return std::find(begin(), end(), inValue) != end(); }
};

// A NUL-terminated text the Rust rules own (ee_data rules.rs RuleTextC)
struct EERuleText {
    const char* mData = "";
    size_t mSize = 0;

    const char* c_str() const { return mData; }
    std::string str() const { return std::string(mData, mSize); }
    operator std::string() const { return str(); }
};

// The rules documents' records and enums (EECombatRules .. EERulesData; EECivStat, EECivMode, EECivPowerAction,
// EEAICheat): Rust's (ee_data generated/rules_gen.rs, from tools/eedata/rules_schema.json), emitted back by cbindgen.
// Rust writes every value in place (EERules::Refresh); the lists and texts point into the Rust rules' storage.
#include "rust/data/generated.h"
#include "rust/data/ffi.h"

// ------------------------------------------------------------------------------------------------ all of them

// the members (EERulesData): mCombat, mGameOptions, mCivBuilder, mCivPowers, mAIPersonalities, mEpochs (by epoch
// position, the original's 15 by default), mEpochMusicFromText (the songs came from epochs.yaml, else dbmusic's flags)
struct EERules : EERulesData {
    EERules() : mRust(ee_data_rules_new()) { this->Refresh(); }
    ~EERules() { ee_data_rules_free(this->mRust); }
    EERules(const EERules&) = delete;
    EERules& operator=(const EERules&) = delete;

    // the songs from dbmusic's per-epoch flags (the binary load): a song row's flag k -> epoch k (k < 14), the 15th
    // epoch plays the 14th's songs
    void SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount);
    // the media of an epoch position (past the list: the last epoch's)
    const EEEpochMedia& Epoch(long inEpoch) const;
    // the Rust rules the C++ structs snapshot (ee_tech's civilization manager reads them in place)
    const EeDataRules* RustRules() const { return mRust; }
    // the C++ structs written by the Rust rules (EELoadRules: db_text.rs)
    void Refresh() { ee_data_rules_mirror(this->mRust, static_cast<EERulesData*>(this)); }

private:
    EeDataRules* mRust;                     // the rules' owner (rust/crates/ee_data)
};

// The game's rules (constructed on first use; filled by EELoadRules at database load, EEDataTextFile.cpp)
EERules& EEGetRules();

// the computer players' level (0 easy .. 2 hard: their records' values) of a game option difficulty
long EEDifficultyAILevel(unsigned long inDifficulty);

// TEST ONLY (EE-SIM.exe --set, the headless simulation): a rules document's value changed as the document is read,
// "doc.key.key=value". A key may name a list entry by its id: "aipersonalities.balanced.military.army_floor=0.2" is
// the personalities entry with id balanced (the list may also be named: aipersonalities.personalities.balanced...).
// Only an existing scalar is changed. False, with the reason, when the spec is malformed.
bool EERulesAddOverride(const std::string& inSpec, std::string& outError);
// the overrides that changed nothing (the document is not in the data, or the key is not in it), each with the reason.
// An out parameter, not a return value: on Linux a returned std::string container carries libstdc++'s [abi:cxx11]
// tag in the defining unit's mangled name but not in the Microsoft-dialect caller's, and the link failed
void EERulesOverrideProblems(std::vector<std::string>& out);

#endif
