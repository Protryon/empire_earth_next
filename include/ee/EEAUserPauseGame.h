// EEAUserPauseGame (RECONSTRUCTION; file name inferred). The action the in-game
// options post to pause a single-player game: RTTI EEAUserPauseGame > EEAction, vtable 0x446790, sizeof 0xc, ctor
// inline (EEAction() then the vptr; 0x1e117d). Virtual4 0x5280 (in another unit), Virtual8 the shared empty slot
// 0x1720c6; its ??_G is the shared 0x1e1b69. Requires ee/EEAction.h.
#ifndef EE_EEAUSERPAUSEGAME_H
#define EE_EEAUSERPAUSEGAME_H

class EEAUserPauseGame : public EEAction {
public:
    virtual void Virtual4();                // 0x5280
    virtual void Virtual8();                // 0x1720c6 (ICF-shared empty body)
};

#endif
