// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGAttribute.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFMGAttribute). The main game form's attribute button: 0x26783d..0x267ad9 (ctor, ??_G, dtor, CopyData,
// DrawTextA, GetIconRectangle; EEUIFMGErrorLog's ctor at 0x267ad9). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/GEText.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEScreensShims.h"
#include "rust/game/screens.h"


// Destroys the attribute button and its level text
EEUIFMGAttribute::~EEUIFMGAttribute()
{
    delete static_cast<GEText*>(this->mLevelText);
}

// the Rust bodies' shims (ee_screens hud_more.rs: the constructor's level text, CopyData, DrawTextA)
#define ATT(a) reinterpret_cast<EEUIFMGAttribute*>(a)
extern "C" uintptr_t ee_screenscpp_att_new_text(uintptr_t inAttr, uintptr_t inFont) { return (uintptr_t)(ATT(inAttr)->mLevelText = new GEText(reinterpret_cast<GEFont*>(inFont))); }
extern "C" void ee_screenscpp_att_text_transparent(uintptr_t inAttr) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_att_text_fg(uintptr_t inAttr, uintptr_t inColor) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetFgColor(*reinterpret_cast<const GEColor*>(inColor)); }
extern "C" float ee_screenscpp_att_left(uintptr_t inAttr) { return ATT(inAttr)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_att_top(uintptr_t inAttr) { return ATT(inAttr)->GetRectangle().GetTop(); }
extern "C" uint32_t ee_screenscpp_att_border(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetBorderThickness(); }
extern "C" void ee_screenscpp_att_text_set_location(uintptr_t inAttr, float inX, float inY) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetLocation(inX, inY); }
extern "C" int64_t ee_screenscpp_att_base_copy_data(uintptr_t inAttr) { return ATT(inAttr)->EEUIBUpgrade::CopyData(); }
extern "C" int64_t ee_screenscpp_att_queued(uintptr_t inAttr) { return ATT(inAttr)->mNumQueued; }
extern "C" void ee_screenscpp_att_level_text(uintptr_t inAttr, EeScreensShimText inText) { static_cast<GEText*>(ATT(inAttr)->mLevelText)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_att_set_show(uintptr_t inAttr, bool inShow) { ATT(inAttr)->mShowLevel = inShow; }
extern "C" bool ee_screenscpp_att_show(uintptr_t inAttr) { return ATT(inAttr)->mShowLevel; }
extern "C" float ee_screenscpp_att_text_x(uintptr_t inAttr) { return static_cast<GEText*>(ATT(inAttr)->mLevelText)->GetLocation().GetX(); }
extern "C" float ee_screenscpp_att_text_y(uintptr_t inAttr) { return static_cast<GEText*>(ATT(inAttr)->mLevelText)->GetLocation().GetY(); }
extern "C" uint32_t ee_screenscpp_att_height(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetHeight(); }
extern "C" uint32_t ee_screenscpp_att_width(uintptr_t inAttr) { return (uint32_t)ATT(inAttr)->GetWidth(); }
extern "C" int64_t ee_screenscpp_att_base_draw_text(uintptr_t inAttr) { return ATT(inAttr)->EEUIBUpgrade::DrawTextA(); }
extern "C" int64_t ee_screenscpp_att_draw_level(uintptr_t inAttr, float inLeft, float inTop, float inRight, float inBottom)
{
    return ATT(inAttr)->GetForm()->DrawClippedText(GERectangle(GE2DPoint(inLeft, inTop), GE2DPoint(inRight, inBottom)), static_cast<GEText*>(ATT(inAttr)->mLevelText), ATT(inAttr));
}

// Returns the icon's rectangle: the whole button, or a 17-pixel strip at its left in free camera mode
GERectangle EEUIFMGAttribute::GetIconRectangle() const
{
    GERectangle theRectangle;
    if (!gFreeCamera)
        theRectangle = this->GetRectangle();
    else
        theRectangle = GERectangle(GE2DPoint(this->GetRectangle().GetLeft(), this->GetRectangle().GetTop()), 17.0f,
                                   this->GetRectangle().GetHeight());
    return theRectangle;
}
