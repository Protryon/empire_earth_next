// Dialog (R6 WON round 26): the Rust Dialog's calls out (rust/crates/ee_won/src/dialogobj.rs, EeWonGuiOp's dialog
// ops; the container's fall back to Container.cpp's), its constructor, destructor and default button. The original's
// DialogBehavior.cpp (dragging, focus, Escape and Enter, the default button) is the Rust object.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

extern "C" intptr_t ee_won_guicpp_dlg_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Dialog* t = static_cast<Dialog*>((Component*)inComp);
    switch (op) {
    case Op::DlgDefaultButton:
        if (a) Button::SetDialogDefaultButton((Button*)(Component*)b);
        else Button::UnSetDialogDefaultButton((Button*)(Component*)b);
        return 0;
    case Op::DlgBringToTop: t->mParent->BringToTop(t); return 0;
    case Op::DlgInvalidateRect: t->InvalidateRect(*(WONRectangle*)a); return 0;
    case Op::DlgEndDialog: t->GetRoot()->EndDialog((int)a, (Component*)b); return 0;
    case Op::DlgClick: ((Button*)(Component*)a)->Fn2F3440(); return 0;
    case Op::DlgIsModal: return t->GetRoot()->mManager->mModalComponent == t ? 1 : 0;
    case Op::DlgIsDefault: return ((Button*)(Component*)a)->IsDefaultButton() ? 1 : 0;
    case Op::DlgEventButtonFlags: return static_cast<Button*>(((ComponentEvent*)a)->mComponent)->GetButtonFlags();
    case Op::ChildSetPos: ((Component*)a)->SetPos((int)b, (int)c); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventCompControlId: return ((ComponentEvent*)a)->mComponent->mControlId;
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    default: return ContainerHostCall(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x006f7910
// Creates a focusable, draggable dialog that grabs its background, stays inside the root window and ignores Escape
// (the Rust dialog: no drag, drag allowed and kept in the root, the Escape result -1, no default button)
Dialog::Dialog()
{
    this->mDlg=ee_won_dlg_new(static_cast<Component*>(this),ee_won_guicpp_dlg_call,this->mCt);
    this->SetComponentFlags(0x118,true);
}
// Destroys the dialog: the Rust dialog gives the default button back (the implicit destructor in the original)
Dialog::~Dialog() { ee_won_dlg_free(this->mDlg); }
// FUNCTION: WON 0x006f79c0
// Sets the button that Enter activates (held by reference)
void Dialog::SetDefaultButton(Button* button) { this->DCall(EeWonDlgCall::SetDefaultButton,(intptr_t)static_cast<Component*>(button)); }
}
