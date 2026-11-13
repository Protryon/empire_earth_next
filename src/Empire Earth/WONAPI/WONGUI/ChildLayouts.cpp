// The child layouts (R6 WON round 31): the Rust layout manager's and instructions' calls out
// (rust/crates/ee_won/src/layoutobj.rs, EeWonGuiOp's layout ops), the shells' constructors and destructors, the
// manager's AddInstruction and Container::AddChildLayout's statements, and CenterLayout::Center. The original's
// ChildLayouts.cpp, ChildLayoutsAdvanced.cpp's Execute bodies and DoCalc, and CenterLayoutExecute.cpp are the Rust
// objects.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONPercentLayout.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }

// every op a layout object makes is on a component or an instruction it is given (never on its own shell)
extern "C" intptr_t ee_won_guicpp_layout_call(void*, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    switch (op) {
    case Op::ChildLeft: return Comp(a)->Left();
    case Op::ChildTop: return Comp(a)->Top();
    case Op::ChildWidth: return Comp(a)->Width();
    case Op::ChildHeight: return Comp(a)->Height();
    case Op::LayParentOf: return (intptr_t)static_cast<Component*>(Comp(a)->mParent);
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* r = (const EeWonRect4*)b;
        Comp(a)->SetPosSize(r->x, r->y, r->w, r->h);
        return 0;
    }
    case Op::LaySetSizeAtLeast: Comp(a)->SetSizeAtLeast((int)b, (int)c); return 0;
    case Op::LayHold: ((ChildLayoutInstructionBase*)a)->AddRef(); return 0;
    case Op::LayRelease: ((ChildLayoutInstructionBase*)a)->Release(); return 0;
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// Creates an empty layout manager (the Rust manager: no instruction, no layout point)
ChildLayoutManager::ChildLayoutManager() { this->mMgr = ee_won_lmgr_new(this, ee_won_guicpp_layout_call); }
// Destroys the manager: the Rust manager releases the layout points, then the instructions
ChildLayoutManager::~ChildLayoutManager() { ee_won_lmgr_free(this->mMgr); }
// FUNCTION: WON 0x00736d60
// Appends a layout instruction to the manager's list (held)
void ChildLayoutManager::AddInstruction(ChildLayoutInstructionBase* instruction)
{
    this->MCall(EeWonLayoutMgrCall::AddInstruction, (intptr_t)instruction, (intptr_t)instruction->mLay);
}
// Container::AddChildLayout's statements: the group (a byte), group 0 enabled and run at once on the container
// before the manager holds it, any other group disabled
void ChildLayoutManager::AddChildLayout(ChildLayoutInstructionBase* instr, int id, Component* container)
{
    ee_won_lmgr_call(this->mMgr, EeWonLayoutMgrCall::AddChildLayout, (intptr_t)instr, (intptr_t)instr->mLay, id,
                     (intptr_t)container);
}

// Destroys an instruction: its Rust instruction (the list borrows its components)
ChildLayoutInstructionBase::~ChildLayoutInstructionBase() { ee_won_layout_free(this->mLay); }
void ChildLayoutInstructionBase::Make(EeWonLayoutKind k, Component* r0, Component* r1, int i0, int i1, int i2, bool h,
                                      bool v)
{
    EeWonLayoutInit init;
    memset(&init, 0, sizeof(init));
    init.kind = k;
    init.refs[0] = (intptr_t)r0;
    init.refs[1] = (intptr_t)r1;
    init.ints[0] = i0;
    init.ints[1] = i1;
    init.ints[2] = i2;
    init.horz = h;
    init.vert = v;
    this->Make(init);
}
void ChildLayoutInstructionBase::Make(const EeWonLayoutInit& init)
{
    this->mLay = ee_won_layout_new(this, ee_won_guicpp_layout_call, &init);
}

// Fn2F6140's instruction (its fields were set one by one after `new`)
ChildLayoutInstruction::ChildLayoutInstruction(int flags, Component* child, Component* reference, int x, int y,
                                               int width, int height)
{
    EeWonLayoutInit init;
    memset(&init, 0, sizeof(init));
    init.kind = EeWonLayoutKind::Child;
    init.refs[0] = (intptr_t)child;
    init.refs[1] = (intptr_t)reference;
    init.ints[0] = flags;
    init.ints[1] = x;
    init.ints[2] = y;
    init.ints[3] = width;
    init.ints[4] = height;
    this->Make(init);
}

// the target, the param's anchors (after SetBorders), percentages and padding
PercentLayout::PercentLayout(Component* target, const PercentLayoutParam& param)
{
    EeWonLayoutInit init;
    memset(&init, 0, sizeof(init));
    init.kind = EeWonLayoutKind::Percent;
    init.refs[0] = (intptr_t)target;
    init.refs[1] = (intptr_t)param.left;
    init.refs[2] = (intptr_t)param.top;
    init.refs[3] = (intptr_t)param.right;
    init.refs[4] = (intptr_t)param.bottom;
    const int n[8] = { param.px, param.py, param.pw, param.ph, param.lp, param.tp, param.rp, param.bp };
    for (int i = 0; i < 8; i++) init.ints[i] = n[i];
    this->Make(init);
}

// FUNCTION: WON 0x00737690
// Centres a child in its parent once, horizontally and/or vertically (the original's temporary centre layout is the
// Rust's)
void CenterLayout::Center(Component* parent, Component* child, bool horz, bool vert)
{
    ee_won_layout_center(ee_won_guicpp_layout_call, (intptr_t)parent, (intptr_t)child, horz, vert);
}
}
