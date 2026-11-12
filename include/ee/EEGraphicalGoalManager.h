// C:\EEX\Empire Earth\Graphics Thread\EEGraphicalGoalManager.h (RECONSTRUCTION; file name inferred; INCOMPLETE). The
// graphical goals (the "go here" markers: EEGraphicalGoal, vtable 0x4412a0, which create their effects through
// gEffectManager) kept by EEEffectManager +0x24 (vtable 0x4412a8: only the deleting dtor; EEGraphicalGoalManager.cpp
// 0x107616..0x10972b). sizeof 0x10. Names are provisional.
#ifndef EE_EEGRAPHICALGOALMANAGER_H
#define EE_EEGRAPHICALGOALMANAGER_H

#include <map>
#include <vector>

class EEGraphicalGoal;                      // ee/EEGraphicalGoal.h (held by value: 0x28 bytes)
class FSFile;

class EEGraphicalGoalManager {
public:
    EEGraphicalGoalManager();               // 0x10779d
    virtual ~EEGraphicalGoalManager();      // +0x00 0x1077e9 (??_G 0x1077cd)
    // ICF-folded into EEE3DCommandConfirmation::Load 0xea1fc (EEEffectManager::Load calls it) (provisional name)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    // ICF-folded into 0x27cf38 (EEEffectManager::RepairPointers calls it) (provisional name)
    long RepairPointers(unsigned long inFirst, unsigned long inSecond);
    void Update();                          // 0x1081f1 (EEEffectManager::Update, every tick) (provisional name)
    // EEGraphicalGoalManager.cpp's own members (provisional names)
    void Rebuild(long inPlayer);            // 0x1078c9: the current player's selected units' goal markers
    void AddGoal(long inPlayer, unsigned long inCount, long inType);      // 0x10786c: one more when inCount >= the count
    void TrimGoals(long inPlayer, unsigned long inCount, long inType);  // 0x108185: drops goals from the front to inCount
    unsigned long GetGoalCount(long inPlayer, long inType);           // 0x108b0c

    // +0x04 the goals by player, then by type (the dtor/Update walk: outer node +0x10 is the inner map, inner node
    // +0x10 the vector, 0x28-byte elements; was map<unsigned long, EEGraphicalGoal*>)
    std::map<unsigned long, std::map<unsigned long, std::vector<EEGraphicalGoal> > > mGoals;
};

#endif
