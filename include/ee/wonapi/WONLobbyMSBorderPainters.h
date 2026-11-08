#ifndef EE_WONAPI_WONLOBBYMSBORDERPAINTERS_H
#define EE_WONAPI_WONLOBBYMSBORDERPAINTERS_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// MSEtchedFrame: fieldless Component subclass, vtable 0x0084ec7c; paint only.
class MSEtchedFrame : public Component {
public:
    MSEtchedFrame(); // at 0x00700840 (fieldless, 0x30 bytes)
    virtual void Paint(Graphics&); // slot 0x18, at 0x00700860
};
// MSInputBox: fieldless InputBox subclass (0xec bytes), allocated at 0x0073bc8e. The implicit
// constructor calls InputBox (0x006f7fd0) and sets the vtable 0x00851858; the destructor
// (0x0073bec0) is implicit.
class MSInputBox : public InputBox {
public:
    virtual void Paint(Graphics&); // slot 0x18, at 0x006ffad0
    virtual void GetDesiredSize(int&,int&); // forwards the desired size
};
}
#endif
