#ifndef EE_WONAPI_WONLOBBYSKINTAB_H
#define EE_WONAPI_WONLOBBYSKINTAB_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
namespace WONAPI {
// WONTabCtrl (0xf8 bytes): constructor 0x0071a830, destructor 0x0071a910 (declarations only).
class WONTabCtrl : public TabCtrl {
public:
    WONTabCtrl(WONSkin* =NULL); // at 0x0071a830; the skin code passes NULL
    void SetFrame(const WONFrame&); //31aaa0
    WONTabCtrl* Duplicate(); // at 0x0071ad20
    void SetTransparent(bool); // conditional forwarding (0x0071ac50)
    void AddStandardTab(const GUIString&,Container*); //31ac70
    SmartPtr<WONSkin> mSkin; //d8
    WONFrame mFrame; // offsets 0xdc..0xef, includes a retained image at +0xec
    SmartPtr<Component> mBack; //f0
    bool mIsTransparent; //f4 +natural padding throughf7
};
}
#endif
