// ee/EERules.h (new, not in the original game): the game rules that were literals in the code, as data
// (docs/DATA_FORMATS.md 8.8). Rules documents next to the db tables, db\combat.yaml, db\gameoptions.yaml,
// db\civbuilder.yaml, db\civpowers.yaml, db\aipersonalities.yaml, plus each epoch's media in db\epochs.yaml. The
// fields and their defaults are generated from tools/eedata/rules_schema.json (ee/EERulesGen.h); the reader is Rust
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

// How a hit's damage is formed (EEUnitClass::ComputeDamage)
enum EEDamageForm {
    kDamageDirect,                          // a unit's or projectile's hit (EEUnitClass::ApplyHit)
    kDamageSplash,                          // splash damage around it (EEUnitClass::ApplyAreaDamage)
    kDamageTrample                          // a moving unit pushing a stopped enemy (EEPhysicsModel)
};

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

#include "ee/EERulesGen.h"

// ------------------------------------------------------------------------------------------------ all of them

// the members (EERulesData): mCombat, mGameOptions, mCivBuilder, mCivPowers, mAIPersonalities, mEpochs (by epoch
// position, the original's 15 by default), mEpochMusicFromText (the songs came from epochs.yaml, else dbmusic's flags)
struct EERules : EERulesData {
    EERules();
    ~EERules();
    EERules(const EERules&) = delete;
    EERules& operator=(const EERules&) = delete;

    // the songs from dbmusic's per-epoch flags (the binary load): a song row's flag k -> epoch k (k < 14), the 15th
    // epoch plays the 14th's songs
    void SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount);
    // the media of an epoch position (past the list: the last epoch's)
    const EEEpochMedia& Epoch(long inEpoch) const;
    // the Rust rules the C++ structs snapshot (ee_tech's civilization manager reads them in place)
    const EeDataRules* RustRules() const { return mRust; }
    void Refresh();                         // the C++ structs written by the Rust rules (EELoadRules: db_text.rs)

private:
    EeDataRules* mRust;                     // the rules' owner (rust/crates/ee_data)
};

// The game's rules (constructed on first use; filled by EELoadRules at database load, EEDataTextFile.cpp)
EERules& EEGetRules();
// (a difficulty's computer player level: ee_data_rules_ai_level, rust/data/ffi.h)

#endif
