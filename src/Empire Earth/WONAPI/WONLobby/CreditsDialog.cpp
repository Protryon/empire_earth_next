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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// Reads an ADDLINE layout key (text, optional colour) into the credits; passes other keys to the dialog config
bool CreditsDialogConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CREDITSDIALOGCONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) {
    GUIString aLine=this->ReadString(false);
    DWORD aColor=-1;
    if(!this->EndOfString()) {
        this->EnsureComma();
        aColor=this->ReadColor();
    }
    this->mCredits->AddLine(aLine,this->Fn386860(std::string("DialogComponent")),aColor);
    return true;
    }
    return DialogConfig::Virtual24(inInstruction);
}

void CreditsDialogConfig::SetComponent(Component* inComponent)
{
    DialogConfig::SetComponent(inComponent);
    this->mCredits=static_cast<CreditsDialog*>(inComponent);
}

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
    case Op::LineNew: return (intptr_t) new GUIString(*reinterpret_cast<const GUIString*>(a));
    case Op::LineFree: delete reinterpret_cast<GUIString*>(a); return 0;
    case Op::BasePaint: aComp->Component::Paint(g); return 0;
    case Op::SetDefaultFont: g.SetFont(aComp->GetDefaultFont()); return 0;
    case Op::SetColor: g.SetColor((DWORD)(uint32_t)x); return 0;
    case Op::SysHilight: g.SetSysColor(StandardColor_3DHilight); return 0;
    case Op::SysShadow: g.SetSysColor(StandardColor_3DShadow); return 0;
    case Op::FillRect: g.FillRect(x, y, w, h); return 0;
    case Op::DrawRect: g.DrawRect(x, y, w, h); return 0;
    case Op::FontHeight: return g.mFont->GetHeight();
    case Op::Color: return (intptr_t)(uint32_t)g.mColor;
    case Op::StringWidth: return g.mFont->GetStringWidth(*reinterpret_cast<const GUIString*>(b));
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
