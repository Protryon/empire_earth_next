// C:\EEX\Empire Earth\AI Thread\EECPPlayerInteractionManager.h (RECONSTRUCTION; file name inferred).
// A computer player's chat/taunt manager (RTTI EECPPlayerInteractionManager, no base; vtable 0x43c270: the destructor
// only; sizeof 0x2c: `new` 0x2c in EEComputerPlayer's ctor 0x2f1c8, stored at +0xbd0). EECPPlayerInteractionManager.cpp
// 0x89574..0x89fc4. Member and function names are provisional. Requires lle/LowLevelEngine.h, <map>, <vector>.
#ifndef EE_EECPPLAYERINTERACTIONMANAGER_H
#define EE_EECPPLAYERINTERACTIONMANAGER_H

#include <map>
#include <vector>

class EEComputerPlayer;

// One sent message per string id (the map value; sizeof 0x24 from the map node: key +0xc, value +0x10, _Color +0x34).
// Inline ctor calling the out-of-line Clear 0x89586 (no `return this`), copy ctor = operator= 0x89574 (ICF-folded:
// both call Copy and return this), Copy 0x895a3.
struct EECPPIMEntry {
    EECPPIMEntry() { this->Clear(); }
    EECPPIMEntry(const EECPPIMEntry& inEntry);  // 0x89574
    void Clear();                               // 0x89586
    EECPPIMEntry& operator=(const EECPPIMEntry& inEntry);   // 0x89574 (ICF)
    void Copy(const EECPPIMEntry& inEntry);     // 0x895a3

    long mL00;                              // +0x00
    unsigned long mTime;                    // +0x04 gPhysics->mLD0 + 5000 when sent (compared unsigned)
    long mPlayer;                           // +0x08 gWorld.mCurrentPlayer when sent
    long mL0C;                              // +0x0c
    long mL10;                              // +0x10
    long mL14;                              // +0x14
    long mL18;                              // +0x18
    long mL1C;                              // +0x1c
    long mL20;                              // +0x20
};

class EECPPlayerInteractionManager {
public:
    EECPPlayerInteractionManager(EEComputerPlayer* inComputerPlayer);   // 0x895de
    virtual ~EECPPlayerInteractionManager();                            // 0x89651 (??_G 0x89635)
    // 0x8966c: a random string of the id's group (gResource string inID + 0..9), at most once per 5 s per id
    bool SendChat(long inStringID, long inType);
    bool SendChat(long inStringID, long inType, const UWideString& inString);   // 0x8979b

    std::map<long, EECPPIMEntry> mEntries;  // +0x04
    std::vector<long> mLastIndex;           // +0x10 (resize(8, -1): the last string index per group)
    EEComputerPlayer* mComputerPlayer;      // +0x20
    unsigned long mIndexSeed;               // +0x24 (the player's +0xcd8: a random seed for the index)
    bool mB28;                              // +0x28 (= true)
};

#endif
