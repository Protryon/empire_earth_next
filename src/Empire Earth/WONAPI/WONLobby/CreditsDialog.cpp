// The lobby's credits dialog: credit lines from the layout file scroll up and fade in and out at the edges
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// R6 WONLobby round 3: the scrolling text's lines, clock and paint are ee_won's scrolltext.rs (this unit keeps the
// component's shell and its host) and CreditsDialog is the lobby dialog kind Credits (lobbydlg.rs; its shell is in
// WONLobbyDialogs.h). This unit keeps the dialog config's ADDLINE reader.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_lobby/scrolltext.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbycfgrd.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// the reader's statements (lobbycfgrd.rs composes them, R6 WON round 33)
// the config readers' reads (LobbyNetCtrlHost.cpp)
intptr_t LobbyCfgReadHost(ComponentConfig* c, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc);
static intptr_t CreditsAddLine(LobbyRustDialog* inCredits, const GUIString& inLine, Component* inComponent, intptr_t inColor) { return inCredits->LCall(EeWonLobbyDlgCall::AddLine, (intptr_t)&inLine, (intptr_t)inComponent, (intptr_t)(uint32_t)inColor); }
static intptr_t CfgRdHost(void* inCfg, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyCfgRdOp Op;
    ComponentConfig* c = static_cast<ComponentConfig*>(inCfg);
    CreditsDialogConfig* cr = static_cast<CreditsDialogConfig*>(c);
    switch (inOp) {
    case Op::CreditsBase: return WON_GUARDED(cr->ContainerConfig::Virtual24(*(const std::string*)a));
    case Op::CreditsAddLine: return WON_GUARDED(CreditsAddLine(static_cast<LobbyRustDialog*>(c->mComponent.get()), GUIString((GUIStringData*)a), c->Fn386860(std::string("DialogComponent")), b));
    default: return LobbyCfgReadHost(c, inOp, a, b, cc);
    }
}
static intptr_t CfgRd(ComponentConfig* inCfg, EeWonLobbyCfgRdCall inCall, const std::string& inKey, intptr_t d = 0) { return WonRethrow(ee_won_lobbycfgrd_call(inCfg, CfgRdHost, inCall, (intptr_t)inKey.data(), (intptr_t)inKey.size(), (intptr_t)&inKey, d)); }

// Reads an ADDLINE layout key (text, optional colour) into the credits; passes other keys to the dialog config
bool CreditsDialogConfig::Virtual24(const std::string& inInstruction) { return CfgRd(this, EeWonLobbyCfgRdCall::Credits, inInstruction) != 0; }

// ---- ScrollingTextComponent: the shell and its host (each op the original's statements on the component or the
// paint's Graphics)

static intptr_t ScrollTextHost(void* inComp, EeWonScrollTextOp inOp, intptr_t a, intptr_t b, int x, int y, int w, int h)
{
    typedef EeWonScrollTextOp Op;
    ScrollingTextComponent* aComp = static_cast<ScrollingTextComponent*>(inComp);
    Graphics& g = *reinterpret_cast<Graphics*>(a);
    switch (inOp) {
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::RequestTimer: aComp->RequestTimer(x != 0); return 0;
    case Op::BasePaint: aComp->Component::Paint(g); return 0;
    case Op::SetDefaultFont: g.SetFont(aComp->GetDefaultFont()); return 0;
    case Op::SetColor: g.SetColor((DWORD)(uint32_t)x); return 0;
    case Op::SysHilight: g.SetSysColor(StandardColor_3DHilight); return 0;
    case Op::SysShadow: g.SetSysColor(StandardColor_3DShadow); return 0;
    case Op::FillRect: g.FillRect(x, y, w, h); return 0;
    case Op::DrawRect: g.DrawRect(x, y, w, h); return 0;
    case Op::FontHeight: return g.GetFont()->GetHeight();
    case Op::Color: return (intptr_t)g.mG.st.color;
    case Op::StringWidth: return g.GetFont()->GetStringWidth(*reinterpret_cast<const GUIString*>(b));
    case Op::DrawString: g.DrawString(*reinterpret_cast<const GUIString*>(b), x, y); return 0;
    }
    return 0;
}

// Creates an empty clipped scrolling text box that scrolls one pixel every 60 ms
ScrollingTextComponent::ScrollingTextComponent() : mScroll(ee_won_scrolltext_new(this, ScrollTextHost))
{
    this->SetComponentFlags(ComponentFlag_Clip,true);
}
// Destroys the scrolling text box: its lines front to back, then the component
ScrollingTextComponent::~ScrollingTextComponent() { ee_won_scrolltext_free(this->mScroll); }
}
