// PercentLayout (0x50 bytes), used by ContainerConfig at 0x007885d0.
#ifndef EE_WONAPI_WONPERCENTLAYOUT_H
#define EE_WONAPI_WONPERCENTLAYOUT_H
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
class PercentLayout : public ChildLayoutInstructionBase {
public:
    PercentLayout(Component* target,const PercentLayoutParam& param) : mTarget(target),mParam(param) {} // inline (as at 0x007886ea..0x0078870f: copies all 16 DWORDs)
    PercentLayout(Component *target,Component *ref,int px,int py,int pw=-1,int ph=-1,int lp=-1,int tp=-1,int rp=-1,int bp=-1);
    PercentLayout(Component *target,Component *ref1,Component *ref2,int px,int py,int pw=-1,int ph=-1,int lp=-1,int tp=-1,int rp=-1,int bp=-1);
    PercentLayout(Component *target,Component *left,Component *top,Component *right,Component *bottom,int px,int py,int pw=-1,int ph=-1,int lp=-1,int tp=-1,int rp=-1,int bp=-1);
    virtual void Execute(Component*); // at 0x00737580: forwards to Param::DoCalc
    Component* mTarget; //0c borrowed
    PercentLayoutParam mParam; // offsets 0x10..0x4f: 16 DWORDs, no owned pointers
};
}
#endif
