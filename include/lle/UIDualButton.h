// C:\EEX\Low-Level Engine\User Interface\UIDualButton.h (RECONSTRUCTION). A UIButton that also answers the right
// mouse button. UIDualButton.cpp (0x10069cc7..0x10069d6b) holds the ctor, dtor and the two overrides; the implicit
// copy members are emitted first by UIControlsTable.cpp (0x1006684c). Vtable 0x10099438.
// Requires lle/UIButton.h.
#ifndef LLE_UIDUALBUTTON_H
#define LLE_UIDUALBUTTON_H

#include "lle/UIButton.h"

class LLE_API UIDualButton : public UIButton {
public:
    UIDualButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap, GEBitmap* inPressedBitmap,
                 const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                 unsigned long inBorderThickness);
    virtual ~UIDualButton();
protected:
    virtual long MouseRButtonDown();
    virtual long MouseRButtonUp();
};

#endif
