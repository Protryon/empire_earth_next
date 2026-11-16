// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.h (RECONSTRUCTION; file name inferred).
// The cheat-code parser (RTTI EECheatCodes; cluster 0x12a54a; "gCheatCodes != NULL" assert). Names are provisional.
#ifndef EE_EECHEATCODES_H
#define EE_EECHEATCODES_H

struct EeAppCheats;   // rust/ee_app.h

class EECheatCodes {                        // RTTI, vtable 0x441ee4 (one slot), sizeof 0x1c, ctor 0x12a54a
public:
    virtual ~EECheatCodes();                // +0x00 (??_G 0x12a589)
    // 0x12b5d6: looks the chat line up; outCode 0 when it is no cheat
    void ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter);
    void ApplyCheatCode(long inCode, long inPlayerID, long inParameter);   // 0x12aef8 (EEMCheatCode::Execute; provisional name)
    EECheatCodes();
    // port: the original's table (map<UWideString, long> mCodes at +0x04, map<long, bool> mIsSinglePlayerOnly at
    // +0x10), its members' bodies and the dispatch are ee_app cheats.rs (EECheatCodes.cpp is their host)
    EeAppCheats* mRust;
};

extern EECheatCodes* gCheatCodes;

#endif
