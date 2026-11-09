#ifndef EE_WONAPI_WONLOBBYSKINLISTCONTROLS_H
#define EE_WONAPI_WONLOBBYSKINLISTCONTROLS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWONButton.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
namespace WONAPI {
// Fieldless 0x100-byte renderer derived from WONButton (0x100), vtable 0x00850798 (48 slots).
class WONHeaderButton : public WONButton {
public:
    WONHeaderButton(); // at 0x007191a0 (fieldless, 0x100 bytes)
    virtual void Paint(Graphics&); // slot 0x18, at 0x007193c0
    WONHeaderButton* Duplicate(); // at 0x00719350
};
// WONHeaderControl (0xb4 bytes): MultiListHeader (0xac) + text padding at +0xac + skin pointer at +0xb0.
class WONSkin;
class WONHeaderControl : public MultiListHeader {
public:
    WONHeaderControl(WONSkin* =NULL); // at 0x0071a060
    // Implicit virtual destructor (0x0071a120): releases the skin, then MultiListHeader.
    void SetColumn(int,const GUIString&,int* =NULL); // at 0x0071a1c0
protected:
    int mTextLeftPad; //ac
    SmartPtr<WONSkin> mSkin; // offset 0xb0, retained; class size 0xb4
};
// WONListCtrl (0xc0 bytes): WONScrollContainer (0xbc) plus a retained header at +0xbc.
class WONListCtrl : public WONScrollContainer {
public:
    WONListCtrl(); // at 0x0071a270 (0xc0 bytes)
    // Implicit virtual destructor (0x0071a2b0): releases the header, then WONScrollContainer.
    virtual void Virtual08(); // geometry forwarding (0x0071a580)
    WONListCtrl* Duplicate(); // at 0x0071a520
    void SetHeader(WONHeaderControl*); // at 0x0071a3e0
    WONHeaderControl* GetHeader() { return this->mHeader.get(); } // inline
    MultiListArea* GetListArea() { return static_cast<MultiListArea*>(this->GetScrollArea()); } // inline, borrowed
    void SetStandarColumnHeader(int,const GUIString&,int); // at 0x0071a450 (SDK spelling)
    void SetColumnHeader(int,Component*,int); //31a4c0
    SmartPtr<WONHeaderControl> mHeader; //bc..bf
};
}
#endif
