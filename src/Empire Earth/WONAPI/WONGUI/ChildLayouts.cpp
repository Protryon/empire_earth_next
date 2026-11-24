// The child layouts (R6 WON round 31): the Rust layout manager's and instructions' calls out
// (rust/crates/ee_won_gui/src/layoutobj.rs, EeWonGuiOp's layout ops), the instruction shell's Make and destructor, and
// CenterLayout::Center (R6 WON GUI round 35: the manager is the Rust container's, the instruction classes one shell). The original's
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
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/layoutobj.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }

// every op a layout object makes is on a component or an instruction it is given (never on its own shell)
extern "C" intptr_t ee_won_guicpp_layout_call(void*, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    switch (op) {
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::ChildSetPosSize: Container::Place(Comp(a), *(const EeWonRect4*)b); return 0;
    case Op::LaySetSizeAtLeast: Comp(a)->SetSizeAtLeast((int)b, (int)c); return 0;
    case Op::LayHold: ((ChildLayoutInstructionBase*)a)->AddRef(); return 0;
    case Op::LayRelease: ((ChildLayoutInstructionBase*)a)->Release(); return 0;
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// Destroys an instruction: its Rust instruction (the list borrows its components)
ChildLayoutInstructionBase::~ChildLayoutInstructionBase() { ee_won_layout_free(this->mLay); }
// the record's numbers are layoutobj.rs's (R6 WON round 33)
void ChildLayoutInstructionBase::Make(EeWonLayoutKind k, Component* r0, Component* r1, int i0, int i1, int i2, bool h,
                                      bool v)
{
    this->mLay = ee_won_layout_new_basic(this, ee_won_guicpp_layout_call, k, r0, r1, i0, i1, i2, h, v);
}
void ChildLayoutInstructionBase::Make(const EeWonLayoutInit& init)
{
    this->mLay = ee_won_layout_new(this, ee_won_guicpp_layout_call, &init);
}

// Centres a child in its parent once, horizontally and/or vertically (the original's temporary centre layout is the
// Rust's)
void CenterLayout::Center(Component* parent, Component* child, bool horz, bool vert)
{
    ee_won_layout_center(ee_won_guicpp_layout_call, (intptr_t)parent, (intptr_t)child, horz, vert);
}
}
