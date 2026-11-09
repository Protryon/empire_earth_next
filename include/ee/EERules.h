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

#include <string>
#include <vector>

struct EeDataRules;
namespace EEDataText {
class Content;
}

// A number that is a constant or one of dbworld's values: `fanaticism.F2` (the constant's id, then F, F2, L or L2:
// mFloatValue, mFloatValue2, mLongValue, mLongValue2). The game reads a world value through gWorldTable when it uses
// the number (EERulesWorld.h), so it follows the loaded dbworld.
struct EERuleNumber {
    float mFloat = 0.0f;                    // a constant (mWorldRow < 0): its value
    long mLong = 0;                         // ... as an integer (a float constant: truncated)
    long mWorldRow = -1;                    // a world value: the dbworld row
    int mWorldValue = 0;                    // 0 F, 1 F2, 2 L, 3 L2

    static EERuleNumber Constant(float inValue);
    static EERuleNumber Integer(long inValue);
    static EERuleNumber World(long inRow, int inValue);
};

// How a hit's damage is formed (EEUnitClass::ComputeDamage)
enum EEDamageForm {
    kDamageDirect,                          // a unit's or projectile's hit (EEUnitClass::ApplyHit)
    kDamageSplash,                          // splash damage around it (EEUnitClass::ApplyAreaDamage)
    kDamageTrample                          // a moving unit pushing a stopped enemy (EEPhysicsModel)
};

#include "ee/EERulesGen.h"

long EEAICheatBit(const std::string& inName);  // 0: not a cheat name

// ------------------------------------------------------------------------------------------------ all of them

struct EERules {
    EECombatRules mCombat;
    EEGameOptionRules mGameOptions;
    EECivBuilderRules mCivBuilder;
    EECivPowerRules mCivPowers;
    EEAIPersonalityRules mAIPersonalities;
    std::vector<EEEpochMedia> mEpochs;      // by epoch position (the original's 15 by default)
    bool mEpochMusicFromText = false;       // the songs came from epochs.yaml (else dbmusic's flags)

    EERules();
    ~EERules();
    EERules(const EERules&) = delete;
    EERules& operator=(const EERules&) = delete;

    // reads the rules documents and epochs.yaml's media from the Content (references by id); a document the Content
    // has no text for keeps its defaults. False with a message ("combat line 12: ...") on the first problem.
    bool Load(EEDataText::Content& inContent, std::string& outError);
    // the songs from dbmusic's per-epoch flags (the binary load): a song row's flag k -> epoch k (k < 14), the 15th
    // epoch plays the 14th's songs
    void SetMusicFromFlags(long inSong, const bool* inFlags, unsigned long inCount);
    // the same from a whole dbmusic.dat (the gate's and tools' path); false on a short file
    bool SetMusicFromDbmusicFile(const unsigned char* inData, size_t inSize);
    // the content dump of the rules (docs/MODDING_PLAN.md gate G1): `"rules.<doc>":{...}` lines, values resolved
    void Dump(std::string& outJson) const;
    // the media of an epoch position (past the list: the last epoch's)
    const EEEpochMedia& Epoch(long inEpoch) const;

private:
    void Refresh();                         // the C++ structs from the Rust rules' snapshot
    EeDataRules* mRust;                     // the rules' owner (rust/crates/ee_data)
};

// The game's rules (constructed on first use; filled by EELoadRules at database load, EEDataTextFile.cpp)
EERules& EEGetRules();

// the computer players' level (0 easy .. 2 hard: their records' values) of a game option difficulty
long EEDifficultyAILevel(unsigned long inDifficulty);
// the computer players' cheats (EEAICheat bits) of a game option difficulty
long EEDifficultyAICheats(unsigned long inDifficulty);

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
