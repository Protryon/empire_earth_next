// Layout parameters and layout classes.
#ifndef EE_WONAPI_WONLOBBYLAYOUT_H
#define EE_WONAPI_WONLOBBYLAYOUT_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// PercentLayoutParam (0x40 bytes): explicit constructor, borrowed component pointers, plain copy.
class PercentLayoutParam {
public:
    PercentLayoutParam(Component *left, Component *top, Component *right, Component *bottom, int px, int py, int pw = -1, int ph = -1, int lp = -1, int tp = -1, int rp = -1, int bp = -1); // at 0x00737320
    void Init(Component*,Component*,Component*,Component*,int,int,int,int,int,int,int,int); // at 0x007372c0
    void SetPercentages(int,int,int,int); // at 0x007373f0
    void SetPadding(int,int,int,int); // at 0x00737410
    void SetBorders(Component*, Component*, Component*, Component*);
    Component *left, *top, *right, *bottom;
    int px, py, pw, ph;
    int lp, tp, rp, bp;
    int dx, dy, dw, dh; // offsets 0x30..0x3c, copied by the factory; DoCalc does not read them
};
// The instruction classes' shells (R6 WON round 31: their numbers and lists are the Rust instruction, see
// ChildLayoutInstructionBase); the constructors and Add as the original's inline ones.
class SameSizeLayout : public ChildLayoutInstructionBase {
public:
    SameSizeLayout(bool horz=true,bool vert=true) { this->Make(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,horz,vert); } // inline (as at 0x00788e56)
    SameSizeLayout(Component* first,bool horz=true,bool vert=true) { this->Make(EeWonLayoutKind::SameSize,NULL,NULL,0,0,0,horz,vert); this->Add(first); } // inline (as at 0x00701964)
    SameSizeLayout* Add(Component* c) { this->AddComponent(c); return this; } // at 0x007377a0
};
class ClearRightLayout : public ChildLayoutInstructionBase {
public:
    ClearRightLayout(Component* component,int pad=0) { this->Make(EeWonLayoutKind::ClearRight,component,NULL,pad); } //3890d8..116
    ClearRightLayout* Add(Component* component) { this->AddComponent(component); return this; }
};
class SpaceEvenlyLayout : public ChildLayoutInstructionBase {
public:
    SpaceEvenlyLayout(Component* box,int left=0,int right=0,bool horz=true) { this->Make(EeWonLayoutKind::SpaceEvenly,box,NULL,left,right,0,horz); } //3892ea..337
    SpaceEvenlyLayout* Add(Component* component) { this->AddComponent(component); return this; }
};
class NoOverlapLayout : public ChildLayoutInstructionBase {
public:
    NoOverlapLayout(Component* target,int type,Component* ref,int horz=0,int vert=0) { this->Make(EeWonLayoutKind::NoOverlap,ref,target,type,horz,vert); } //389608..64d
    NoOverlapLayout* Add(Component* component) { this->AddComponent(component); return this; }
};
class SizeAtLeastLayout : public ChildLayoutInstructionBase {
public:
    SizeAtLeastLayout(Component* target,int horz=0,int vert=0) { this->Make(EeWonLayoutKind::SizeAtLeast,target,NULL,horz,vert); } //3898c3..909
    SizeAtLeastLayout* Add(Component* component) { this->AddComponent(component); return this; }
};
// Container::Fn2F6140's instruction: the child placed against its reference by the flags and the four offsets
class ChildLayoutInstruction : public ChildLayoutInstructionBase {
public:
    ChildLayoutInstruction(int flags,Component* child,Component* reference,int x,int y,int width,int height);
};
// Complete ChildLayoutManager20 declaration is in WONLobbyGUI.h before Container.
}
#endif
