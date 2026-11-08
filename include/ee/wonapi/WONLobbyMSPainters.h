#ifndef EE_WONAPI_WONLOBBYMSPAINTERS_H
#define EE_WONAPI_WONLOBBYMSPAINTERS_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Fieldless Component subclasses (SDK MSControls.h). MSPopup (0x30 bytes): allocated at 0x006fd222,
// Component constructor 0x006f49e0, vtable 0x0084ddc0. MSDlgBack (0x30 bytes): allocated at 0x0071ab5a,
// Component constructor 0x006f49e0, vtable 0x0084e99c. Implicit constructors and destructors
// (the deleting destructor 0x006fd3e0 calls 0x00706860, which jumps to the base cleanup 0x006f4a70).
class MSPopup : public Component { public: virtual void Paint(Graphics&); }; //300450, VT44ddc0
class MSDlgBack : public Component { public: virtual void Paint(Graphics&); }; //300520, VT44e99c
}
#endif
