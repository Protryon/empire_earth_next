// Layout parameters and layout classes.
#ifndef EE_WONAPI_WONLOBBYLAYOUT_H
#define EE_WONAPI_WONLOBBYLAYOUT_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/layoutobj.h"
namespace WONAPI {
// The instruction classes' shells (R6 WON round 31: their numbers and lists are the Rust instruction, see
// ChildLayoutInstructionBase); the constructors and Add as the original's inline ones.
class SameSizeLayout : public ChildLayoutInstructionBase {
public:
    SameSizeLayout(bool horz=true,bool vert=true) { this->Make(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,horz,vert); } // inline (as at 0x00788e56)
    SameSizeLayout(Component* first,bool horz=true,bool vert=true) { this->Make(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,horz,vert); this->Add(first); } // inline (as at 0x00701964)
    SameSizeLayout* Add(Component* c) { this->AddComponent(c); return this; } // at 0x007377a0
};
// The clear-right, space-evenly, no-overlap and size-at-least layouts and Container::Fn2F6140's instruction (R6 WON
// GUI round 35) are ChildLayoutInstructionBase's kind constructor; only the config table made them.
}
#endif
