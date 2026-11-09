#ifndef EE_WONAPI_WONLOBBYMSREMAININGPAINTERS_H
#define EE_WONAPI_WONLOBBYMSREMAININGPAINTERS_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// MSHeaderButton: fieldless MSButton (0x60 bytes), vtable 0x0084e1f8; explicit constructor,
// destructor out of line.
class MSHeaderButton : public MSButton {
public:
    MSHeaderButton(const GUIString& text=""); // at 0x006fef40 (fieldless, 0x60 bytes)
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ff000
};
// MSCloseButton: fieldless Button, vtable 0x0084e378; paint only.
class MSCloseButton : public Button {
public:
    MSCloseButton(); // at 0x006ff200 (0x44 bytes)
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ff2a0
    virtual void GetDesiredSize(int& width,int& height) { width=16; height=14; } // inline (body at 0x006ff250)
};
// MSLabel: fieldless MSButtonBase, vtable 0x0084e668; paint only.
class MSLabel : public MSButtonBase {
public:
    MSLabel(); // at 0x006ff6c0 (0x5c bytes)
    MSLabel(const GUIString&); // at 0x006ff740 (fieldless, 0x5c bytes)

    virtual void Paint(Graphics&); // slot 0x18, at 0x006ff7d0
};
// MSScrollThumb: fieldless Component, vtable 0x0084e438; paint only.
class MSScrollThumb : public Component {
public:
    MSScrollThumb(); // at 0x006ff310 (0x30 bytes)
    virtual ~MSScrollThumb(); //original external lifetime
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ff360
};
}
#endif
