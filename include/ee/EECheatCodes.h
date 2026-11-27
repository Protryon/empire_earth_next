// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.h (RECONSTRUCTION; file name inferred).
// The cheat-code parser (RTTI EECheatCodes; cluster 0x12a54a; "gCheatCodes != NULL" assert). Names are provisional.
#ifndef EE_EECHEATCODES_H
#define EE_EECHEATCODES_H

#include "rust/game/gamecpp.h"
struct EeAppCheats;   // rust/ee_app.h

class EECheatCodes {                        // RTTI, vtable 0x441ee4 (one slot), sizeof 0x1c, ctor 0x12a54a
public:
    // Destroys the cheat code table
    // +0x00 (??_G 0x12a589)
    virtual ~EECheatCodes() { ee_app_cheats_free(this->mRust); }
    // 0x12b5d6: looks the chat line up; outCode 0 when it is no cheat
    // Parses a chat line: "goto x y" and "find [player] id" move the camera; otherwise strips a trailing number as
    // the parameter and looks the rest up as a cheat code
    void ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter) { ee_gamecpp_cheats_parse(this->mRust, (const char*)inText, &outCode, &outParameter); }
    // Runs a cheat code for a player (when cheats are enabled): resources, reveal/explore, epochs, instant build,
    // research, healing, power, refuel, win/lose, kill units, or the code list
    // 0x12aef8 (EEMCheatCode::Execute; provisional name)
    void ApplyCheatCode(long inCode, long inPlayerID, long inParameter) { ee_gamecpp_cheats_apply(this->mRust, inCode, inPlayerID, inParameter); }
    // Creates the cheat code table from the string resources
    EECheatCodes() { this->mRust = ee_gamecpp_cheats_new(); }
    // port: the original's table (map<UWideString, long> mCodes at +0x04, map<long, bool> mIsSinglePlayerOnly at
    // +0x10), its members' bodies and the dispatch are ee_app cheats.rs (EECheatCodes.cpp is their host)
    EeAppCheats* mRust;
};

extern EECheatCodes* gCheatCodes;

#endif
