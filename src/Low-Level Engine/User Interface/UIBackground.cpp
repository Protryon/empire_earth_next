// C:\EEX\Low-Level Engine\User Interface\UIBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10058d38..0x1005c34a: the first User Interface unit. It opens with the header-inline members of every UI class it
// is the first to include (UILayoutUtility, TMPoll's implicit operator=, UIBackground, UIEntity, UIEntityGroup,
// UIControl, UIFormManager, UIForm incl. its implicit copy members, UIEvent), the U2D key init chains (0x10059ebb),
// UIManager's inline and implicit copy members, then its own code (0x1005a76e) and the vector/deque/map COMDATs
// (0x1005aa1b..). UIBackgroundTable.cpp starts at 0x1005c34a.
#define LLE_UIBACKGROUND_CPP   // UIManager::sMouseWheel/sMouseLeftHanded are visible here (UIManager.h) (provisional name)
#include <windows.h>
#include <algorithm>

#include "rust/ee_ui.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIBackground.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"
#include "lle/UIManager.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top); guarded: a golden driver includes several
// background units in one translation unit
#ifndef UI_BACKGROUND_RECT_HELPERS
#define UI_BACKGROUND_RECT_HELPERS
static inline EeUiRect UIBgRect(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
static inline GERectangle UIBgRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
#endif

// The Rust moves' and visibility changes' view of a background (ee_ui background.rs, docs/RUST_PLAN.md R6)
struct UIBackgroundRust {
    static UIBackground* Background(void* inBackground) { return static_cast<UIBackground*>(inBackground); }
    static bool Value(void* inBackground, int inWhich)
    {
        UIBackground* theBackground = Background(inBackground);
        if (inWhich == 0)
            return theBackground->mIsVisible;
        return theBackground->mForm == theBackground->mForm->GetManager()->GetCurrentForm();
    }
    static bool& Visible(void* inBackground) { return Background(inBackground)->mIsVisible; }
    static GERectangle& Rectangle(void* inBackground) { return Background(inBackground)->mRectangle; }
    static void Call(void* inBackground, int inCall, const EeUiRect* inRectangle)
    {
        UIForm* theForm = Background(inBackground)->mForm;
        switch (inCall) {
        case 0:
            theForm->LockDrawBackground();
            break;
        case 1:
            theForm->UnlockDrawBackground();
            break;
        case 2:
            theForm->RedrawIntersectControls(UIBgRect(*inRectangle));
            break;
        default:
            theForm->GetManager()->AddDirtyRectangle(UIBgRect(*inRectangle), theForm);
            break;
        }
    }
};
extern "C" bool ee_uicpp_background_value(void* inBackground, int inWhich) { return UIBackgroundRust::Value(inBackground, inWhich); }
extern "C" void ee_uicpp_background_set_visible(void* inBackground, bool inVisible) { UIBackgroundRust::Visible(inBackground) = inVisible; }
extern "C" EeUiRect ee_uicpp_background_rect(void* inBackground) { return UIBgRect(UIBackgroundRust::Rectangle(inBackground)); }
extern "C" void ee_uicpp_background_set_rect(void* inBackground, const EeUiRect* inRectangle)
{
    UIBackgroundRust::Rectangle(inBackground) = UIBgRect(*inRectangle);
}
extern "C" void ee_uicpp_background_call(void* inBackground, int inCall, const EeUiRect* inRectangle)
{
    UIBackgroundRust::Call(inBackground, inCall, inRectangle);
}

// FUNCTION: LLE 0x1005a76e
// Creates a background with its layout and draw priority and adds it to the form
UIBackground::UIBackground(unsigned long inBackgroundID, const UILayoutUtility& inLayout, UIForm* inForm,
                           unsigned long inDrawPriority, bool inVisible)
    : mBackgroundID(inBackgroundID)
{
    this->mLayout = inLayout;
    this->mDrawPriority = inDrawPriority;
    this->mForm = inForm;
    this->mIsVisible = inVisible;
    this->mForm->AddBackground(this);
}

// FUNCTION: LLE 0x1005a7c9
// Destroys the background (nothing to release)
UIBackground::~UIBackground()
{
}

// FUNCTION: LLE 0x1005a7d0
// Closes the background by unloading its textures
void UIBackground::Close()
{
    this->UnloadBackground();
}

// FUNCTION: LLE 0x1005a7d5
// Lays the background out against the window size and loads its textures
void UIBackground::Open()
{
    GEWindow* theWindow = this->mForm->GetManager()->GetRasterizer()->GetWindow(this->mForm->GetManager()->GetScreen());
    GERectangle theScreenRectangle =
        UIBgRect(ee_ui_background_screen_rect(theWindow->GetWidth(), theWindow->GetHeight(), this->mForm->GetUseExclusiveCoords()));
    this->mLayout.CalculateRectangle(theScreenRectangle, this->mRectangle, this->mForm->GetUseExclusiveCoords());
    this->LoadBackground();
}

// FUNCTION: LLE 0x1005a888
// Recomputes the background rectangle from its layout against the window size
void UIBackground::RelayoutBackground()
{
    GEWindow* theWindow = this->mForm->GetManager()->GetRasterizer()->GetWindow(this->mForm->GetManager()->GetScreen());
    GERectangle theScreenRectangle = UIBgRect(ee_ui_background_screen_rect(theWindow->GetWidth(), theWindow->GetHeight(), false));
    this->mLayout.CalculateRectangle(theScreenRectangle, this->mRectangle, this->mForm->GetUseExclusiveCoords());
}

// FUNCTION: LLE 0x1005a92c
// Moves the background, redrawing the old and new areas when its form is the current one
void UIBackground::SetRectangle(const GERectangle& inRectangle)
{
    ee_ui_background_set_rectangle(this, UIBgRect(inRectangle));
}

// FUNCTION: LLE 0x1005a9cc
// Shows or hides the background and redraws its area
void UIBackground::SetVisibility(bool inVisible)
{
    ee_ui_background_set_visibility(this, inVisible);
}

// Header-inline members first emitted here (declaration order; names in analysis/lle/names.txt)
// FUNCTION: LLE 0x10058d38
// FUNCTION: LLE 0x10058d3c
// FUNCTION: LLE 0x10058d40
// FUNCTION: LLE 0x10058d44
// FUNCTION: LLE 0x10058d55
// FUNCTION: LLE 0x10058d65
// FUNCTION: LLE 0x10058d76
// FUNCTION: LLE 0x10058d87
// FUNCTION: LLE 0x10058d9b
// FUNCTION: LLE 0x10058d9f
// FUNCTION: LLE 0x10058da3
// FUNCTION: LLE 0x10058de3
// FUNCTION: LLE 0x10058e86
// FUNCTION: LLE 0x10058e98
// FUNCTION: LLE 0x10058e9f
// FUNCTION: LLE 0x10058f03
// FUNCTION: LLE 0x10058f20
// FUNCTION: LLE 0x10058f25
// FUNCTION: LLE 0x10058f2e
// FUNCTION: LLE 0x10058f3d
// FUNCTION: LLE 0x10058f58
// FUNCTION: LLE 0x10058f73
// FUNCTION: LLE 0x10058f88
// FUNCTION: LLE 0x10058f8e
// FUNCTION: LLE 0x10058f91
// FUNCTION: LLE 0x10058fa6
// FUNCTION: LLE 0x10058fcb
// FUNCTION: LLE 0x10059008
// FUNCTION: LLE 0x10059011
// FUNCTION: LLE 0x10059018
// FUNCTION: LLE 0x10059055
// FUNCTION: LLE 0x1005905c
// FUNCTION: LLE 0x10059063
// FUNCTION: LLE 0x10059084
// FUNCTION: LLE 0x10059097
// FUNCTION: LLE 0x1005909b
// FUNCTION: LLE 0x1005909f
// FUNCTION: LLE 0x100590a6
// FUNCTION: LLE 0x100590d5
// FUNCTION: LLE 0x100590dc
// FUNCTION: LLE 0x100590df
// FUNCTION: LLE 0x1005910c
// FUNCTION: LLE 0x10059118
// FUNCTION: LLE 0x10059155
// FUNCTION: LLE 0x10059162
// FUNCTION: LLE 0x1005917e
// FUNCTION: LLE 0x100591ad
// FUNCTION: LLE 0x100591e8
// FUNCTION: LLE 0x100591f5
// FUNCTION: LLE 0x10059209
// FUNCTION: LLE 0x10059213
// FUNCTION: LLE 0x1005921d
// FUNCTION: LLE 0x1005922a
// FUNCTION: LLE 0x10059361
// FUNCTION: LLE 0x100594ef
// FUNCTION: LLE 0x100594f8
// FUNCTION: LLE 0x100594ff
// FUNCTION: LLE 0x10059559
// FUNCTION: LLE 0x10059568
// FUNCTION: LLE 0x10059577
// FUNCTION: LLE 0x1005957f
// FUNCTION: LLE 0x10059583
// FUNCTION: LLE 0x10059593
// FUNCTION: LLE 0x1005959a
// FUNCTION: LLE 0x100595a1
// FUNCTION: LLE 0x100595a8
// FUNCTION: LLE 0x100595b8
// FUNCTION: LLE 0x100595bf
// FUNCTION: LLE 0x100595c6
// FUNCTION: LLE 0x100595cd
// FUNCTION: LLE 0x100595d4
// FUNCTION: LLE 0x100595db
// FUNCTION: LLE 0x100595e2
// FUNCTION: LLE 0x100595fd
// FUNCTION: LLE 0x1005961f
// FUNCTION: LLE 0x1005962f
// FUNCTION: LLE 0x1005963b
// FUNCTION: LLE 0x10059647
// FUNCTION: LLE 0x1005964e
// FUNCTION: LLE 0x10059655
// FUNCTION: LLE 0x1005965c
// FUNCTION: LLE 0x10059663
// FUNCTION: LLE 0x1005966a
// FUNCTION: LLE 0x10059671
// FUNCTION: LLE 0x10059678
// FUNCTION: LLE 0x1005967f
// FUNCTION: LLE 0x10059686
// FUNCTION: LLE 0x1005968d
// FUNCTION: LLE 0x10059694
// FUNCTION: LLE 0x1005969b
// FUNCTION: LLE 0x100596a2
// FUNCTION: LLE 0x100596b2
// FUNCTION: LLE 0x100596b9
// FUNCTION: LLE 0x100596c0
// FUNCTION: LLE 0x100596c7
// FUNCTION: LLE 0x100596ce
// FUNCTION: LLE 0x100596d9
// FUNCTION: LLE 0x10059702
// FUNCTION: LLE 0x1005970f
// FUNCTION: LLE 0x10059726
// FUNCTION: LLE 0x1005973c
// FUNCTION: LLE 0x1005974e
// FUNCTION: LLE 0x1005975b
// FUNCTION: LLE 0x10059766
// FUNCTION: LLE 0x10059789
// FUNCTION: LLE 0x10059796
// FUNCTION: LLE 0x10059acb
// FUNCTION: LLE 0x10059e7d
// FUNCTION: LLE 0x10059e91
// FUNCTION: LLE 0x10059e9a
// FUNCTION: LLE 0x10059fdb
// FUNCTION: LLE 0x1005a006
// FUNCTION: LLE 0x1005a016
// FUNCTION: LLE 0x1005a023
// FUNCTION: LLE 0x1005a028
// FUNCTION: LLE 0x1005a050
// FUNCTION: LLE 0x1005a058
// FUNCTION: LLE 0x1005a05f
// FUNCTION: LLE 0x1005a066
// FUNCTION: LLE 0x1005a06d
// FUNCTION: LLE 0x1005a088
// FUNCTION: LLE 0x1005a08f
// FUNCTION: LLE 0x1005a0a1
// FUNCTION: LLE 0x1005a0b4
// FUNCTION: LLE 0x1005a0bb
// FUNCTION: LLE 0x1005a0d6
// FUNCTION: LLE 0x1005a0dc
// FUNCTION: LLE 0x1005a0e2
// FUNCTION: LLE 0x1005a0e8
// FUNCTION: LLE 0x1005a0ef
// FUNCTION: LLE 0x1005a0f6
// FUNCTION: LLE 0x1005a0fd
// FUNCTION: LLE 0x1005a104
// FUNCTION: LLE 0x1005a10b
// FUNCTION: LLE 0x1005a112
// FUNCTION: LLE 0x1005a118
// FUNCTION: LLE 0x1005a11f
// FUNCTION: LLE 0x1005a126
// FUNCTION: LLE 0x1005a12d
// FUNCTION: LLE 0x1005a134
// FUNCTION: LLE 0x1005a13b
// FUNCTION: LLE 0x1005a142
// FUNCTION: LLE 0x1005a155
// FUNCTION: LLE 0x1005a1aa
// FUNCTION: LLE 0x1005a1ff
// FUNCTION: LLE 0x1005a207
// FUNCTION: LLE 0x1005a220
// FUNCTION: LLE 0x1005a239
// FUNCTION: LLE 0x1005a24d
// FUNCTION: LLE 0x1005a261
// FUNCTION: LLE 0x1005a269
// FUNCTION: LLE 0x1005a4be

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1005aad3
// TEMPLATE: LLE 0x1005ad05
// TEMPLATE: LLE 0x1005ad98
// TEMPLATE: LLE 0x1005add4
// SYNTHETIC: LLE 0x10059ebb
// SYNTHETIC: LLE 0x10059ec5
// SYNTHETIC: LLE 0x10059ed5
// SYNTHETIC: LLE 0x10059ee1
// SYNTHETIC: LLE 0x10059eeb
// SYNTHETIC: LLE 0x10059ef5
// SYNTHETIC: LLE 0x10059f05
// SYNTHETIC: LLE 0x10059f11
// SYNTHETIC: LLE 0x10059f1b
// SYNTHETIC: LLE 0x10059f25
// SYNTHETIC: LLE 0x10059f35
// SYNTHETIC: LLE 0x10059f41
// SYNTHETIC: LLE 0x10059f4b
// SYNTHETIC: LLE 0x10059f55
// SYNTHETIC: LLE 0x10059f65
// SYNTHETIC: LLE 0x10059f71
// SYNTHETIC: LLE 0x10059f7b
// SYNTHETIC: LLE 0x10059f85
// SYNTHETIC: LLE 0x10059f95
// SYNTHETIC: LLE 0x10059fa1
// SYNTHETIC: LLE 0x10059fab
// SYNTHETIC: LLE 0x10059fb5
// SYNTHETIC: LLE 0x10059fc5
// SYNTHETIC: LLE 0x10059fd1

// Library COMDATs first emitted here (vector/deque/map members and algorithms the UI headers instantiate)
// TEMPLATE: LLE 0x1005aa1b
// TEMPLATE: LLE 0x1005aa47
// TEMPLATE: LLE 0x1005aa7b
// TEMPLATE: LLE 0x1005aaa7
// TEMPLATE: LLE 0x1005ab0f
// TEMPLATE: LLE 0x1005abb3
// TEMPLATE: LLE 0x1005ad40
// TEMPLATE: LLE 0x1005ad6c
// TEMPLATE: LLE 0x1005aef8
// TEMPLATE: LLE 0x1005af69
// TEMPLATE: LLE 0x1005afc9
// TEMPLATE: LLE 0x1005b019
// TEMPLATE: LLE 0x1005b0c2
// TEMPLATE: LLE 0x1005b10c
// TEMPLATE: LLE 0x1005b19e
// TEMPLATE: LLE 0x1005b2de
// TEMPLATE: LLE 0x1005b363
// TEMPLATE: LLE 0x1005b395
// TEMPLATE: LLE 0x1005b632
// TEMPLATE: LLE 0x1005b644
// TEMPLATE: LLE 0x1005b670
// TEMPLATE: LLE 0x1005b69c
// TEMPLATE: LLE 0x1005b6af
// TEMPLATE: LLE 0x1005b7bd
// TEMPLATE: LLE 0x1005b7e0
// TEMPLATE: LLE 0x1005b823
// TEMPLATE: LLE 0x1005b866
// TEMPLATE: LLE 0x1005b897
// TEMPLATE: LLE 0x1005b8c2
// TEMPLATE: LLE 0x1005b8e4
// TEMPLATE: LLE 0x1005b909
// TEMPLATE: LLE 0x1005b92e
// TEMPLATE: LLE 0x1005b94a
// TEMPLATE: LLE 0x1005b97e
// TEMPLATE: LLE 0x1005b9b2
// TEMPLATE: LLE 0x1005bc16
// TEMPLATE: LLE 0x1005bc32
// TEMPLATE: LLE 0x1005bf8b
// TEMPLATE: LLE 0x1005c017
// TEMPLATE: LLE 0x1005c0a8
// TEMPLATE: LLE 0x1005c13c
// TEMPLATE: LLE 0x1005c1cb
// TEMPLATE: LLE 0x1005c2d9
