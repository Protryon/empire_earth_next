// The lobby's page view (layout type BrowserComponent): the message of the day, the terms of use and the help and
// news tabs. WON's HTML rendering is not ported (docs/RUST_PLAN.md D8): a page is shown as plain text in a TextArea,
// HTML pages with their tags dropped and entities decoded by ee_won (rust/crates/ee_won_net/src/plaintext.rs). There are
// no links, so no history; a web address is shown as text (the WON servers are gone).
// R6 WON round 34: the class is the Rust record's (rust/crates/ee_won_gui/src/browserobj.rs: its data, Go, ShowFiles,
// the page reads, Refresh, Reset, SetPageText, SetScroller, AddedToParent, SetTransparent); this is its host, each op
// one of the original's statements, and the constructor and destructor.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "ee/wonapi/WONGUIShell.h"
using namespace WONAPI;
typedef EeWonBrowserOp Bo;

extern "C" intptr_t ee_won_guicpp_browser_call(void* inMe, EeWonBrowserOp op, intptr_t a, intptr_t b)
{
    BrowserComponent* t = (BrowserComponent*)inMe;
    Scroller* s = (Scroller*)a;
    switch (op) {
    case Bo::NewView: t->mHTMLView = new TextArea; return 0;
    case Bo::ViewNoScrollOnBottom: t->mHTMLView->SetTextFlags(TextFlag_ScrollOnBottom, false); return 0;
    case Bo::ScrollerAssign: t->mScroller = s; return 0;
    case Bo::ScrollerHasView: return s->GetScrollArea() == t->mHTMLView.get() ? 1 : 0;
    case Bo::ScrollerSetView: s->SetScrollArea(t->mHTMLView.get()); return 0;
    case Bo::SameSize: t->Fn2F6140(s, 3, t, 0, 0, 0, 0, 0); return 0; // CLI_SameSize = 3
    case Bo::AddChild: t->AddChild(s); return 0;
    case Bo::HasScroller: return t->mScroller.get() != NULL ? 1 : 0;
    case Bo::NewScrollContainer:
        return (intptr_t)static_cast<Scroller*>(static_cast<ScrollbarScroller*>(WONShellNew(EeWonShellKind::MsScrollContainer, (intptr_t)static_cast<ScrollArea*>(t->mHTMLView.get()))));
    case Bo::Ref: s->AddRef(); return 0;
    case Bo::Unref: s->Release(); return 0;
    case Bo::ScrollbarsConditional: static_cast<ScrollbarScroller*>(s)->SetScrollbarConditions(ScrollbarCondition_Conditional, ScrollbarCondition_Conditional); return 0;
    case Bo::BaseAddedToParent: t->Container::AddedToParent(); return 0;
    case Bo::ViewNoBackground: t->mHTMLView->SetBackground(Background(-1, -1)); return 0;
    case Bo::ViewGrabBg: t->mHTMLView->SetComponentFlags(ComponentFlag_GrabBG, a != 0); return 0;
    case Bo::ViewClear: t->mHTMLView->Clear(); return 0;
    case Bo::ViewAddText: t->mHTMLView->AddFormatedText(GUIString(std::wstring((const wchar_t*)a, (size_t)b)), false); return 0;
    case Bo::ViewTop: t->mHTMLView->SetOffsets(0, 0); return 0;
    case Bo::Invalidate: t->Invalidate(); return 0;
    }
    return 0;
}

namespace WONAPI {
// Creates the view: an empty, word-wrapped, read-only text area (the Rust record's constructor)
BrowserComponent::BrowserComponent() { this->mRust = ee_won_browser_new(this, ee_won_guicpp_browser_call, sizeof(wchar_t)); }
BrowserComponent::~BrowserComponent() { ee_won_browser_free(this->mRust); }
}
