#ifndef EE_WONAPI_WONLOBBYSKINNEDCONTAINERS_H
#define EE_WONAPI_WONLOBBYSKINNEDCONTAINERS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySkinFrame.h"
namespace WONAPI {
// WONComboBox (0xbc bytes; parent 0xa4), vtable 0x00850a9c (51 slots).
class WONComboBox : public ComboBox {
public:
    WONComboBox(); // at 0x00719800 (0xbc bytes)
    WONComboBox* Duplicate(); // at 0x007199e0
    virtual void Virtual08(); // SizeChanged (slot 0x08), at 0x00719b00
    virtual void GetDesiredSize(int&,int&); //319a70 desired-size0c
    virtual void Paint(Graphics&); // slot 0x18, at 0x00719bf0
    void SetFrame(const WONFrame& f) { this->mFrame=f; } // inline deep copy (WONFrame assignment 0x007177a0)
    bool mDrawFrame,mButtonInFrame; //a4,a5; natural a6/a7 padding
    WONFrame mFrame; //a8..bb
};
// WONScrollContainer (0xbc bytes): ScrollbarScroller (0x9c) base, vtable 0x00850b6c (52 slots);
// constructor and destructor out of line.
class WONScrollContainer : public ScrollbarScroller {
public:
    WONScrollContainer(); // at 0x00719c50 (0xbc bytes)
    void CopyAttributes(WONScrollContainer*); // copies the attributes, at 0x00719f70
    void SetFrame(const WONFrame& f) { this->mFrame=f; } // inline
    void SetCorner(Image* i) { this->mCorner=i; } // inline; retained image at +0xb8
    WONScrollContainer* Duplicate(); // at 0x00719f00
    virtual void Virtual08(); // slot 0x08, at 0x00719df0: the Container base hook
    virtual void Paint(Graphics&); // slot 0x18, at 0x00719ff0
    int mHeaderHeight; //9c
    bool mTransparent; //a0; natural a1..a3 padding
    WONFrame mFrame; //a4..b7
    SmartPtr<Image> mCorner; //b8..bb
};
}
#endif
