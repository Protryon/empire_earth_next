// PercentLayout (0x50 bytes), used by ContainerConfig at 0x007885d0.
#ifndef EE_WONAPI_WONPERCENTLAYOUT_H
#define EE_WONAPI_WONPERCENTLAYOUT_H
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
class PercentLayout : public ChildLayoutInstructionBase {
public:
    // the target and the param's anchors, percentages and padding handed to the Rust instruction, whose Execute is the
    // param's DoCalc (0x00737430 / 0x00737580)
    PercentLayout(Component* target,const PercentLayoutParam& param); // inline in the original (0x007886ea..0x0078870f)
};
}
#endif
