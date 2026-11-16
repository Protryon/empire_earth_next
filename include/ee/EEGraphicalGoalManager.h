// C:\EEX\Empire Earth\Graphics Thread\EEGraphicalGoalManager.h (RECONSTRUCTION; file name inferred). The
// graphical goals (the "go here" markers of the current player's selected units: marching lines and icons, made
// through gEffectManager) kept by EEEffectManager +0x24 (vtable 0x4412a8: only the deleting dtor;
// EEGraphicalGoalManager.cpp 0x107616..0x10972b). Names are provisional.
// port (R4.4, twenty-sixth round): the markers and every algorithm are ee_gfx goal_markers.rs's (the original's
// EEGraphicalGoal, held by value in a map of maps of vectors, is its GoalMarker); this class holds the Rust object
// and answers its questions (EEGraphicalGoalManager.cpp).
#ifndef EE_EEGRAPHICALGOALMANAGER_H
#define EE_EEGRAPHICALGOALMANAGER_H

class FSFile;
struct EeGfxGoalMarkers;

class EEGraphicalGoalManager {
public:
    EEGraphicalGoalManager();
    EEGraphicalGoalManager(const EEGraphicalGoalManager&) = delete;   // owns its markers
    virtual ~EEGraphicalGoalManager();      // +0x00 0x1077e9 (??_G 0x1077cd): every marker's effect destroyed
    // ICF-folded into EEE3DCommandConfirmation::Load 0xea1fc (EEEffectManager::Load calls it) (provisional name)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    // ICF-folded into 0x27cf38 (EEEffectManager::RepairPointers calls it) (provisional name)
    long RepairPointers(unsigned long inFirst, unsigned long inSecond);
    // 0x1081f1 (EEEffectManager::Update, every tick): the current player's markers rebuilt (Rebuild 0x1078c9,
    // AddGoal 0x10786c, TrimGoals 0x108185), then every changed marker's effect moved (EEGraphicalGoal::Update)
    void Update();

    EeGfxGoalMarkers* mMarkers;             // +0x04 (ee_gfx goal_markers.rs; was the map of maps of vectors)
};

#endif
