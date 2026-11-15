// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.h (RECONSTRUCTION; file name inferred).
// The cheat-code parser (RTTI EECheatCodes; cluster 0x12a54a; "gCheatCodes != NULL" assert). Names are provisional.
#ifndef EE_EECHEATCODES_H
#define EE_EECHEATCODES_H

#include <map>

class EECheatCodes {                        // RTTI, vtable 0x441ee4 (one slot), sizeof 0x1c, ctor 0x12a54a
public:
    virtual ~EECheatCodes();                // +0x00 (??_G 0x12a589)
    // 0x12b5d6: looks the chat line up; outCode 0 when it is no cheat
    void ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter);
    void ApplyCheatCode(long inCode, long inPlayerID, long inParameter);   // 0x12aef8 (EEMCheatCode::Execute; provisional name)
    // the rest of EECheatCodes.cpp (provisional names)
    EECheatCodes();                         // 0x12a54a
    void AddCheatCode(const UWideString& inText, long inCode, bool inEnabled);   // 0x12a5cb
    void AddResource(long inResource, unsigned long inPlayer, long inAmount);   // 0x12a669
    void ListCheatCodes();                  // 0x12a68c
    void Init();                            // 0x12b169
    void RevealMap(unsigned long inPlayer); // 0x12b4bb
    long LookUp(const UWideString& inText); // 0x12b575
    void ExploreMap(unsigned long inPlayer);    // 0x12b8cc
    void Refuel(unsigned long inPlayer);    // 0x12b8ed
    void HealUnits(unsigned long inPlayer); // 0x12b965
    void RestorePower(unsigned long inPlayer);  // 0x12b9a2
    void MakeWorldVisible(unsigned long inPlayer);  // 0x12b9f2
    void ResearchAll(unsigned long inPlayer);   // 0x12ba6d
    void AdvanceEpoch(long inEpoch, unsigned long inPlayer);    // 0x12bb21
    void ToggleInstantBuild(unsigned long inPlayer);  // 0x12bb58
    void ClearPhysicsFlagForPlayer(unsigned long inPlayer);     // 0x12bb7b
    void SetPhysicsFlagForPlayer(unsigned long inPlayer);       // 0x12bbad
    void ClearResource(long inResource, unsigned long inPlayer);    // 0x12bbdf
    // Layout from the ctor 0x12a54a: the cheat words by text (map<UWideString, long>'s _Init 0x16d077) and a second
    // tree (the ICF-shared map<long, long*> _Init 0x617b6: an 8-byte value; types inferred)
    std::map<UWideString, long> mCodes;     // +0x04
    std::map<long, bool> mIsSinglePlayerOnly;  // +0x10 (bool: AddCheatCode 0x52a5cb stores a byte, LookUp 0x52b575 compares one)
};

extern EECheatCodes* gCheatCodes;           // 0x5319d4

#endif
