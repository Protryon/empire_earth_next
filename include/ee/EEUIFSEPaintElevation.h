// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEPaintElevation.h (RECONSTRUCTION; file name inferred). The elevation tab's
// brush control (RTTI EEUIFSEPaintElevation > EEUIFSEPaint; vtable 0x44556c, sizeof 0xe4: EEUIFSEElevation's ctor news
// 0xe4 bytes; ctor 0x1a4470); its action is an EESEAPaintRust of kind kSEAPaintElevation (ee/EESEAPaint.h). Names
// are provisional. Requires ee/EEUIFSEPaint.h, ee/EEAction.h, lle/GE3DPlane.h, <deque>.
// NOTE: src/Empire Earth/Scenario Editor/EEUIFSEElevation.cpp declares its own provisional EEUIFSEPaintElevation (never
// include both).
#ifndef EE_EEUIFSEPAINTELEVATION_H
#define EE_EEUIFSEPAINTELEVATION_H

#include "ee/EESEAPaint.h"

class EEUIFSEPaintElevation : public EEUIFSEPaint {
public:
    EEUIFSEPaintElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long inId,
                          const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                          unsigned long inIndex);   // 0x1a4470
    virtual ~EEUIFSEPaintElevation();       // +0x00 0x1a44dc (??_G 0x1a44c0)
    virtual long MouseLButtonUp();          // +0x38 0x1a475d
    virtual long MouseMove(const GE2DPoint& inLocation);   // +0x40 0x1a4782
    virtual long MouseRButtonDown();        // +0x44 0x1a4a31
    virtual long MouseRButtonUp();          // +0x48 0x1a4b6b
    virtual EESEAPaint* CreatePaintAction();  // +0x88 0x1a44e7 (one paint step: a new elevation EESEAPaintRust)

    GE3DPlane mPlane;                       // +0xbc (Define(the tile's point, the normal) on the right button)
    long mTileX;                            // +0xd4
    long mTileY;                            // +0xd8
    long mBrushSize;                        // +0xdc (the brush group's +0x28 at the right button)
    char mLevel;                            // +0xe0 (the clicked tile's level)
};

#endif
