#ifndef EE_WONAPI_WONLOBBYWONINPUTBOX_H
#define EE_WONAPI_WONLOBBYWONINPUTBOX_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySkinFrame.h"
namespace WONAPI {
// WONInputBox (0x100 bytes): InputBox (0xec) plus a WONFrame (0x14); constructor 0x00719580,
// allocated at 0x00716c8f, 43-slot vtable (RTTI 0x008509ec).
class WONInputBox : public InputBox {
protected:
    WONFrame mFrame; //ec..ff: four border ints, ImageSmartPtrfc
public:
    WONInputBox(); // empty body at 0x00719580
    // Implicit destructor (0x00719600): releases the frame, then InputBox; stores no own vptr.
    virtual void Paint(Graphics&); // slot 0x18 override, at 0x007197e0
    WONInputBox* Duplicate(); //319770 returns new defaultinput with themeframe only
    void SetFrame(const WONFrame& frame) {this->mFrame=frame;} // inline deep copy
};
}
#endif
