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
    EECheatCodes();
    void AddCheatCode(const UWideString& inText, long inCode, bool inEnabled);
    void AddResource(long inResource, unsigned long inPlayer, long inAmount);
    void ListCheatCodes();
    void Init();
    void RevealMap(unsigned long inPlayer);
    long LookUp(const UWideString& inText);
    void ExploreMap(unsigned long inPlayer);
    void Refuel(unsigned long inPlayer);
    void HealUnits(unsigned long inPlayer);
    void RestorePower(unsigned long inPlayer);
    void MakeWorldVisible(unsigned long inPlayer);
    void ResearchAll(unsigned long inPlayer);
    void AdvanceEpoch(long inEpoch, unsigned long inPlayer);
    void ToggleInstantBuild(unsigned long inPlayer);
    void ClearPhysicsFlagForPlayer(unsigned long inPlayer);
    void SetPhysicsFlagForPlayer(unsigned long inPlayer);
    void ClearResource(long inResource, unsigned long inPlayer);
    // Layout from the ctor 0x12a54a: the cheat words by text (map<UWideString, long>'s _Init 0x16d077) and a second
    // tree (the ICF-shared map<long, long*> _Init 0x617b6: an 8-byte value; types inferred)
    std::map<UWideString, long> mCodes;     // +0x04
    std::map<long, bool> mIsSinglePlayerOnly;  // +0x10 (bool: AddCheatCode 0x52a5cb stores a byte, LookUp 0x52b575 compares one)
};

extern EECheatCodes* gCheatCodes;

#endif
