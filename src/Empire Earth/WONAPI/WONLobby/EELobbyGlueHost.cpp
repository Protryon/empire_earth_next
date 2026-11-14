// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The Empire Earth lobby's glue (R6 WONLobby round 6). The colour button and its chooser (EELobbyColorButton.cpp)
// are Rust objects (rust/crates/ee_won/src/eecolor.rs). This unit is their shells (the virtuals forward) and their
// host (each op the original's statements on a component, a graphics context, the root window or the lobby, with no
// decision).
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/ee_won.h"

using namespace WONAPI;

// ---- the colour button and chooser (EELobbyColorButton.cpp's classes; the Rust objects' shells)

typedef EeWonEeColorCall CC;
static intptr_t ColorHost(void*, EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e,
                          intptr_t f);

EELobbyColorButton::EELobbyColorButton() : mRust(ee_won_eecolor_new(this, EeWonEeColorKind::Button, ColorHost))
{
    this->SetButtonFlags(0x40, true);
}
EELobbyColorButton::~EELobbyColorButton() { ee_won_eecolor_free(this->mRust); }
bool EELobbyColorButton::SetColor(int inColor) { return ee_won_eecolor_call(this->mRust, CC::SetColor, inColor, 0, 0) != 0; }
int EELobbyColorButton::GetColor() { return (int)ee_won_eecolor_call(this->mRust, CC::GetColor, 0, 0, 0); }
void EELobbyColorButton::Paint(Graphics& inGraphics) { ee_won_eecolor_call(this->mRust, CC::Paint, (intptr_t)&inGraphics, 0, 0); }
bool EELobbyColorButton::HandleActivate() { return ee_won_eecolor_call(this->mRust, CC::Activate, 0, 0, 0) != 0; }
void EELobbyColorButton::HandleComponentEvent(ComponentEvent* inEvent)
{
    ee_won_eecolor_call(this->mRust, CC::Event, (intptr_t)inEvent, inEvent->mType == ComponentEvent_EndPopup, 0);
}

EELobbyColorChooser::EELobbyColorChooser() : mRust(ee_won_eecolor_new(this, EeWonEeColorKind::Chooser, ColorHost)) {}
EELobbyColorChooser::~EELobbyColorChooser() { ee_won_eecolor_free(this->mRust); }
// FUNCTION: EXE 0x6908af
// Returns the chooser's fixed size of 120 x 120 pixels
void EELobbyColorChooser::GetDesiredSize(int& outWidth, int& outHeight) { outWidth = outHeight = 120; }
void EELobbyColorChooser::Paint(Graphics& inGraphics) { ee_won_eecolor_call(this->mRust, CC::Paint, (intptr_t)&inGraphics, 0, 0); }
void EELobbyColorChooser::MouseExit() { ee_won_eecolor_call(this->mRust, CC::MouseExit, 0, 0, 0); }
void EELobbyColorChooser::MouseDown(int inA, int inB, int inC) { ee_won_eecolor_call(this->mRust, CC::MouseDown, inA, inB, inC); }
void EELobbyColorChooser::MouseMove(int inX, int inY) { ee_won_eecolor_call(this->mRust, CC::MouseMove, inX, inY, 0); }
void EELobbyColorChooser::AddedToParent() { ee_won_eecolor_call(this->mRust, CC::AddedToParent, 0, 0, 0); }

static intptr_t ColorHost(void*, EeWonEeColorOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e,
                          intptr_t f)
{
    typedef EeWonEeColorOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    switch (inOp) {
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::IsDisabled: return reinterpret_cast<Button*>(a)->IsDisabled() ? 1 : 0;
    case Op::IsPressed: return reinterpret_cast<Button*>(a)->IsPressed() ? 1 : 0;
    case Op::BasePaint: aComp->Component::Paint(*reinterpret_cast<Graphics*>(b)); return 0;
    case Op::SetColor: g->SetColor((unsigned long)b); return 0;
    case Op::SetMenuBackColor: g->SetSysColor(StandardColor_MenuBack); return 0;
    case Op::FillRect: g->FillRect((int)b, (int)c, (int)d, (int)e); return 0;
    case Op::DrawRect: g->DrawRect((int)b, (int)c, (int)d, (int)e); return 0;
    case Op::Draw3DFrame: LobbyDraw3DFrame(*g, (int)b, (int)c, (int)d, (int)e, f != 0); return 0;
    case Op::DrawFrame: LobbyDrawFrame(*g, (int)b, (int)c, (int)d, (int)e, f != 0); return 0;
    case Op::ChooserNew: {
        EELobbyColorChooser* aNew = new EELobbyColorChooser;
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::ChooserRust: return (intptr_t)reinterpret_cast<EELobbyColorChooser*>(a)->mRust;
    case Op::SetDesiredSize: aComp->SetDesiredSize((int)b, (int)c); return 0;
    case Op::AddListener: aComp->AddListener(new ComponentListenerComp(reinterpret_cast<Component*>(b))); return 0;
    case Op::Parent: return (intptr_t)aComp->mParent;
    case Op::ClosePopup: aComp->GetRoot()->ClosePopup(reinterpret_cast<Component*>(b)); return 0;
    case Op::LocalToRoot: aComp->LocalToRoot(*reinterpret_cast<int*>(b), *reinterpret_cast<int*>(c)); return 0;
    case Op::OpenPopup: {
        PopupParams theParams(reinterpret_cast<Component*>(b), 0x23e7, aComp, (int)c, (int)d);
        theParams.mAvoidRect.height = aComp->Height();
        theParams.mAvoidRect.y = (int)d - aComp->Height();
        theParams.mInputComponent = aComp;
        aComp->GetRoot()->OpenPopup(theParams);
        return 0;
    }
    case Op::FireColorChanged: aComp->Fn2F5380(100064, 0); return 0;
    case Op::BaseEvent: reinterpret_cast<Button*>(a)->Button::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)); return 0;
    case Op::BaseAddedToParent: aComp->Component::AddedToParent(); return 0;
    case Op::BaseMouseExit: aComp->Component::MouseExit(); return 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameRust: return (intptr_t) static_cast<EELobbyGame*>(reinterpret_cast<LobbyGame*>(a))->mRust;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    }
    return 0;
}
