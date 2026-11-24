// The percentage layout (0x50 bytes in the original, used by ContainerConfig at 0x007885d0).
#ifndef EE_WONAPI_WONPERCENTLAYOUT_H
#define EE_WONAPI_WONPERCENTLAYOUT_H
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// PercentLayout(target, PercentLayoutParam(refs[0..3], params[0..7])) (R6 WON GUI round 35: the param's SetBorders,
// percentages and padding and the layout's numbers are layoutobj.rs's ee_won_layout_init_percent; its Execute is the
// param's DoCalc, 0x00737430 / 0x00737580)
inline ChildLayoutInstructionBase* WONNewPercentLayout(Component* target,Component* const* refs,const int* params) { return new ChildLayoutInstructionBase(ee_won_layout_init_percent(target,(void* const*)refs,params)); }
}
#endif
