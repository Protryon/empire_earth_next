// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintMoutains.h (RECONSTRUCTION; file name inferred, the RTTI
// spelling). The editor's "paint mountains" action, which removes the units on the painted tiles
// (RTTI EESEAPaintMoutains > EESEAPaint > EEAction; vtable 0x4459dc). The inline ctor/??_G 0x1ab48a are emitted by
// EEUIFSETerrain.cpp; Virtual4 0x1a86ff and SetArea 0x1a88ea are in EESEAPaintMoutains.cpp. Names are provisional.
// Requires ee/EESEAPaint.h.
#ifndef EE_EESEAPAINTMOUTAINS_H
#define EE_EESEAPAINTMOUTAINS_H
#include "rust/ee_screens.h"

class EESEAPaintMoutains : public EESEAPaint {
public:
    EESEAPaintMoutains() : mPlan(ee_screens_sea_paint_new(kSEAPaintBulldozer, 0, 0)) {}
    virtual ~EESEAPaintMoutains() { ee_screens_sea_paint_free(this->mPlan); }   // +0x00 (??_G 0x1ab48a)
    virtual void Virtual4();                // +0x04 0x1a86ff
    virtual void Virtual8();                // +0x08 (0xe123d: an empty body)
    virtual bool SetArea(long inLeft, long inTop, long inRight, long inBottom);   // +0x0c 0x1a88ea

    EeScreensSeaPaint* mPlan;               // the areas still to clear (+0x0c was their deque; editor_actions.rs)
};

#endif
