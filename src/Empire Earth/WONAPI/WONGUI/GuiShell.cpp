// The host of WONGUI's generic widget shells (ee/wonapi/WONGUIShell.h; rust/crates/ee_won_gui/src/shell.rs): a kind's
// calls the base classes' Rust objects do not answer in the game (the base call, a draw, the wrapped text, a new
// widget, a base class's C++ members and layouts), and the maker of a kind's widget.
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONSkinLayout.h"
using namespace WONAPI;

typedef EeWonShellHostOp H;

static Component* NewBase(EeWonShellBaseNew k)
{
    switch (k) {
    case EeWonShellBaseNew::InputBox: return new InputBox;
    case EeWonShellBaseNew::ListArea: return new ListArea;
    case EeWonShellBaseNew::MultiListArea: return new MultiListArea;
    case EeWonShellBaseNew::Button: return new Button;
    default: return new Label;
    }
}

extern "C" intptr_t WONAPI::ee_won_guicpp_shell_call(void* inShell, EeWonShellHostOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONShell* s = (WONShell*)inShell;
    Component* t = s->mComp;
    ComboBox* cb = static_cast<ComboBox*>(t);
    switch (op) {
    case H::Base: return s->BaseCall((EeWonShellVirt)a, b, c, d);
    case H::Draw: WONDrawPlain(*(Graphics*)a, *(const EeWonDrawOp*)b); return 0;
    case H::WrapSetText: static_cast<MSButtonBase*>(t)->mText->SetText(*(const GUIString*)a); return 0;
    case H::WrapWidth: return static_cast<MSButtonBase*>(t)->mText->GetWidth();
    case H::SortType: return (intptr_t)MultiListHeader::GetSortTypeForButton(t);
    case H::New: return (intptr_t)WONShellNew((EeWonShellKind)a, b, c, d);
    case H::NewBase: return (intptr_t)NewBase((EeWonShellBaseNew)a);
    case H::Ref: if (b > 0) ((Component*)a)->AddRef(); else ((Component*)a)->Release(); return 0;
    case H::SelfVirt: t->Virtual08(); return 0;
    case H::SkinCall: return ee_won_guicpp_skin_call(t, EeWonGuiOp::SkinLay, a, b, c, d);
    case H::Layout: { const int* r = (const int*)d; static_cast<Container*>(t)->Fn2F6140((Component*)a, (int)b, (Component*)c, r[0], r[1], r[2], r[3], r[4]); return 0; }
    case H::TabLayoutPoint: return (intptr_t)static_cast<Component*>(static_cast<TabCtrl*>(t)->mContainerLayout.get());
    case H::LabelSetText: static_cast<Label*>((Component*)a)->SetText(*(const GUIString*)b); return 0;
    case H::NamedFont: // the MSButton font on an input box or a list
        if ((EeWonShellBaseNew)b == EeWonShellBaseNew::InputBox) static_cast<InputBox*>((Component*)a)->SetFont(t->GetNamedFont("MSButton"));
        else static_cast<ListArea*>((Component*)a)->SetFont(t->GetNamedFont("MSButton"));
        return 0;
    case H::ComboPart:
        switch ((EeWonShellComboPart)a) {
        case EeWonShellComboPart::Button: cb->mButton = static_cast<Button*>((Component*)b); return 0;
        case EeWonShellComboPart::Edit: cb->mEdit = static_cast<InputBox*>((Component*)b); return 0;
        case EeWonShellComboPart::ListArea: cb->mListArea = static_cast<ListArea*>((Component*)b); return 0;
        default: cb->mListBox = static_cast<Scroller*>((Component*)b); return 0;
        }
    default: return 0; // the component's, button's and other widgets' calls: their Rust objects (shell.rs Sh)
    }
}

Component* WONAPI::WONShellNew(EeWonShellKind k, intptr_t a, intptr_t b, intptr_t c)
{
    switch (ee_won_shell_base(k)) {
    case EeWonShellBase::Button: return new WONShellButton(k, a, b, c);
    case EeWonShellBase::MsButton: return new WONShellMsButton(k, a, b, c);
    case EeWonShellBase::Input: return new WONShellInput(k, a, b, c);
    case EeWonShellBase::Container: return new WONShellContainer(k, a, b, c);
    case EeWonShellBase::Combo: return new WONShellCombo(k, a, b, c);
    case EeWonShellBase::Scrollbar: return new WONShellBar(k, a, b, c);
    case EeWonShellBase::Scroller: return new WONShellScroller(k, a, b, c);
    case EeWonShellBase::Header: return new WONShellHeader(k, a, b, c);
    case EeWonShellBase::Tab: return new WONShellTab(k, a, b, c);
    default: return new WONShellComp(k, a, b, c);
    }
}
