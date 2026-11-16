// EEPGSFormManager.h (RECONSTRUCTION; file name inferred). The post-game summary form's manager: RTTI
// EEPGSFormManager > UIFormManager, vtable 0x4469c0. The original held nine summary graphs (one series per player)
// and the time line's range; the graphs live in the summary's line graph now (rust/crates/ee_ui/src/graphobj.rs,
// R6 game screens B round 3) and the range is ee_screens post_game.rs's, so the class only marks the form's manager
// as the summary's. Requires lle/UIFormManager.h.
#ifndef EE_EEPGSFORMMANAGER_H
#define EE_EEPGSFORMMANAGER_H

class EEPGSFormManager : public UIFormManager {
public:
    virtual ~EEPGSFormManager() {}
};

// Globals of other units the summary reads (provisional names; no evidence beyond these reads)
extern GEFont* gFontTahoma8;                // 0x531990 (the status bars' font; Empire Earth.cpp)
extern long& gGameResources;                 // 0x53123c (0x2ec52 compares it with 4)

#endif
