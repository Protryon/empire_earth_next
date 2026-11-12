// EEPGSFormManager.h (RECONSTRUCTION; file name inferred; layout proven: EEPGSFormManager sizeof 0x350 = its new-site
// size, ctor 0x1e44da exact in EEUIEPostGameSummaryEvents.cpp; member names are provisional). The
// post-game summary form's manager: RTTI EEPGSFormManager > UIFormManager, vtable 0x4469c0 (the scalar deleting dtor
// alone), sizeof 0x350 (new at 0x1e3e25). Ctor 0x1e44da, ??_G 0x1e4546 and dtor 0x1e4562 follow the summary's Open
// (their first user) in EEUIEPostGameSummaryEvents.cpp. Nine summary graphs (one series per player, filled by
// 0x1e46b5) and the range of the time line. Requires lle/UIFormManager.h, lle/UGraph.h.
#ifndef EE_EEPGSFORMMANAGER_H
#define EE_EEPGSFORMMANAGER_H

class EEPGSFormManager : public UIFormManager {
public:
    // 0x1e44da and 0x1e4562 (out of line in the original's summary unit; the summary is ee_screens post_game.rs now,
    // its host UI Events/EEScreens.cpp; the time line's range is Rust's, mFirstTime and mLastTime stay unused)
    EEPGSFormManager()
    {
        this->mU34C = 0;
        this->mU348 = -1;
    }
    virtual ~EEPGSFormManager() {}

    UGraph mEconomyScoreGraph;              // +0x004
    UGraph mTotalCollectedGraph;            // +0x060
    UGraph mTotalSpentGraph;                // +0x0bc
    UGraph mEpochScoreGraph;                // +0x118
    UGraph mMilitaryScoreGraph;             // +0x174
    UGraph mGraph1D0;                       // +0x1d0
    UGraph mSpeedRatingGraph;               // +0x22c
    UGraph mSocietyScoreGraph;              // +0x288
    UGraph mScoreGraph;                     // +0x2e4
    unsigned long mFirstTime;               // +0x340 (the time line's first and last values)
    unsigned long mLastTime;                // +0x344
    long mU348;                             // +0x348 (-1)
    long mU34C;                             // +0x34c (0)
};

// Globals of other units the summary reads (provisional names; no evidence beyond these reads)
extern GEFont* gFontTahoma8;                // 0x531990 (the status bars' font; Empire Earth.cpp)
extern long& gGameResources;                 // 0x53123c (0x2ec52 compares it with 4)

#endif
