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
#include "rust/won_gui/shell_msgbox.h"
#include "ee/wonapi/WONSkinLayout.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/WONLobbyMSSkinImage.h"
using namespace WONAPI;

typedef EeWonShellHostOp H;

static Component* NewBase(EeWonShellBaseNew k)
{
    switch (k) {
    case EeWonShellBaseNew::InputBox: return new InputBox;
    case EeWonShellBaseNew::ListArea: return new ListArea;
    case EeWonShellBaseNew::MultiListArea: return new MultiListArea;
    case EeWonShellBaseNew::Button: return new Button;
    case EeWonShellBaseNew::WonButton: return new WONButton;
    default: return new Label;
    }
}

// a combo box's part member = c
static intptr_t ComboPart(ComboBox* cb, intptr_t which, intptr_t c)
{
    switch ((EeWonShellComboPart)which) {
    case EeWonShellComboPart::Button: cb->mButton = static_cast<Button*>((Component*)c); return 0;
    case EeWonShellComboPart::Edit: cb->mEdit = static_cast<InputBox*>((Component*)c); return 0;
    case EeWonShellComboPart::ListArea: cb->mListArea = static_cast<ListArea*>((Component*)c); return 0;
    default: cb->mListBox = static_cast<Scroller*>((Component*)c); return 0;
    }
}
// an image's method (EeWonWbImage; a duplicate's reference goes to the Rust)
static intptr_t ImageOp(intptr_t which, intptr_t b)
{
    Image* i = (Image*)b;
    switch ((EeWonWbImage)which) {
    case EeWonWbImage::Hold: i->AddRef(); return 0;
    case EeWonWbImage::Release: i->Release(); return 0;
    case EeWonWbImage::Width: return i->GetWidth();
    case EeWonWbImage::Height: return i->GetHeight();
    case EeWonWbImage::Duplicate: { SmartPtr<Image> d = i->Duplicate(); Image* p = d.get(); if (p) p->AddRef(); return (intptr_t)p; }
    default: return 0;
    }
}

extern "C" intptr_t WONAPI::ee_won_guicpp_shell_call(void* inShell, EeWonShellHostOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONShell* s = (WONShell*)inShell;
    Component* t = s ? s->mComp : NULL; // (the skin's calls have no shell: skinobj.rs)
    ComboBox* cb = static_cast<ComboBox*>(t);
    switch (op) {
    case H::Base: return s->BaseCall((EeWonShellVirt)a, b, c, d);
    case H::Draw: WONDrawPlain(*(Graphics*)a, *(const EeWonDrawOp*)b); return 0;
    case H::WrapSetText: static_cast<MSButtonBase*>(t)->mText->SetText(*(const GUIString*)a); return 0;
    case H::WrapWidth: return static_cast<MSButtonBase*>(t)->mText->GetWidth();
    case H::WrapWidthOf: return static_cast<MSButtonBase*>((Component*)a)->mText->GetWidth();
    case H::SetWrap: static_cast<MSButtonBase*>((Component*)a)->SetWrap(b != 0); return 0;
    case H::SameSize: {
        SmartPtr<ChildLayoutInstructionBase> l = new ChildLayoutInstructionBase(EeWonLayoutKind::SameSize, NULL, NULL, 0, 0, 0, true, true);
        for (intptr_t x : {a, b, c}) l->AddComponent((Component*)x);
        l->Execute(static_cast<Container*>(t));
        return 0;
    }
    case H::CenterLayout: {
        SmartPtr<CenterLayout> l = new CenterLayout(t, (Component*)a, true, false);
        if (b) l->Add((Component*)b);
        if (c) l->Add((Component*)c);
        static_cast<Container*>(t)->AddChildLayout(l.get());
        return 0;
    }
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
    case H::ComboPart: return ComboPart(cb, a, b);
    case H::ComboPartOn: return ComboPart(static_cast<ComboBox*>((Component*)a), b, c);
    case H::ComboGet:
        switch ((EeWonShellComboPart)a) {
        case EeWonShellComboPart::Button: return (intptr_t)static_cast<Component*>(cb->mButton.get());
        case EeWonShellComboPart::Edit: return (intptr_t)static_cast<Component*>(cb->mEdit.get());
        case EeWonShellComboPart::ListArea: return (intptr_t)static_cast<Component*>(cb->mListArea.get());
        default: return (intptr_t)static_cast<Component*>(cb->mListBox.get());
        }
    case H::Img: return ImageOp(a, b);
    case H::DrawImages: {
        WONDrawSlots aSlots;
        for (int i = 0; i < 4; i++) aSlots.mImages[i] = ((Image* const*)c)[i];
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    case H::WonBtn: { const intptr_t* x = (const intptr_t*)c; return ee_won_wonbtn_call(static_cast<WONButton*>((Component*)a)->mWb, (EeWonWbCall)b, x[0], x[1], x[2], x[3]); }
    case H::WonBtnState: { const intptr_t* x = (const intptr_t*)b; static_cast<WONButton*>((Component*)a)->SetButtonState((int)x[0], (Image*)x[1], (int)x[2], (Font*)x[3], x[4] != 0); return 0; }
    case H::WonBtnCopy: static_cast<WONButton*>((Component*)a)->CopyAttributes(static_cast<WONButton*>((Component*)b)); return 0;
    case H::BtnCopy: static_cast<Button*>((Component*)a)->CopyAttributes(static_cast<Button*>((Component*)b)); return 0;
    case H::ClickSound: static_cast<Button*>((Component*)a)->SetClickSound((Sound*)b); return 0;
    case H::TextWidth: return ((WrappedText*)a)->GetWidth();
    case H::VDesiredSize: { int* wh = (int*)b; ((Component*)a)->GetDesiredSize(wh[0], wh[1]); return 0; }
    case H::NewStretchImage: { ImageComponent* i = new ImageComponent; i->mStretch = true; i->SetImage((Image*)a); return (intptr_t)static_cast<Component*>(i); }
    case H::NewMsImage: return (intptr_t)WONNewMsImage((unsigned char)a, (unsigned char)b, (int)c, (int)d);
    case H::GetNamedFont: {
        Font* f = WindowManager::GetDefaultWindowManager()->GetNamedFont(std::string((const char*)a));
        if (f) f->AddRef();
        return (intptr_t)f;
    }
    case H::RefObj: if (b > 0) ((RefCount*)a)->AddRef(); else ((RefCount*)a)->Release(); return 0;
    // MSMessageBox::Msg's statements (shell_msgbox.rs ee_won_msgbox_run)
    case H::MsgBoxNew: return (intptr_t)WONShellNew(EeWonShellKind::MsMessageBox, a);
    case H::MsgWinNew: return (intptr_t)CreateNativeWindow();
    case H::MsgWinParent: ((Window*)a)->SetParent((Window*)b); return 0;
    case H::MsgWinTitle: ((Window*)a)->SetTitle(*(const GUIString*)b); return 0;
    case H::MsgWinStyle: ((Window*)a)->SetStyle((int)b); return 0;
    case H::MsgWinCreate: ((Window*)a)->Create(WONRectangle(0, 0, (int)b, (int)c)); return 0;
    case H::MsgWinAdd: ((Window*)a)->AddComponent((Component*)b); return 0;
    case H::MsgShellOp: return WONShell::Op((Component*)a, (EeWonShellOp)b);
    case H::MsgFocus: ((Component*)a)->RequestFocus(); return 0;
    case H::MsgRunDialog: return ((Window*)a)->RunDialog((Component*)b);
    case H::MsgWinClose: ((Window*)a)->Close(); return 0;
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
    case EeWonShellBase::Dialog: return new WONShellDialog(k, a, b, c);
    case EeWonShellBase::WonButton: return new WONButton(k);
    default: return new WONShellComp(k, a, b, c);
    }
}

// MSMessageBox::Msg, PrepareWindow and DoDialog: the box in a dialog window (style 0x59) titled and sized for it, its
// first button focused, run modally, the window closed (shell_msgbox.rs ee_won_msgbox_run since R6 WON GUI round 36)
int WONAPI::MSMessageBoxMsg(Window* inParent, const GUIString& inTitle, const GUIString& inMessage,
                            const GUIString& inButton1, const GUIString& inButton2, const GUIString& inButton3)
{
    static const GUIString kOk("Ok");
    // the texts (shell_msgbox.rs EeWonMsgBoxTexts): the title, the message, the three buttons, the "Ok" for none
    intptr_t aTexts[6] = {(intptr_t)&inTitle, (intptr_t)&inMessage, (intptr_t)&inButton1, (intptr_t)&inButton2, (intptr_t)&inButton3, (intptr_t)&kOk};
    return ee_won_msgbox_run(ee_won_guicpp_shell_call, (intptr_t)inParent, (const EeWonMsgBoxTexts*)aTexts);
}
