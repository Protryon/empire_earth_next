// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBar.cpp (__FILE__ 0x4cdcb0, ESVERIFY lines 125/148). An
// improvement button drawn as an icon, a status bar and two value labels (RTTI EEUIFMGStatusBar > EEUIBUpgrade;
// vtable 0x44acc4): 0x2691b2..0x269ca8 (ctor, ??_G, GEFontBitmap's local ??_G, dtor, the overrides, MoveIcon).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "lle/UIStatusBar.h"
#include "ee/EEUIFMGStatusBar.h"
#include "rust/game/screens.h"   // ee_screens_upgrade_bar_mouse's constants (ee_screens upgrade.rs)


// ---- the one-statement shims of ee_screens status_bar_calls.rs (UI lane round 8: the constructor's layout, the
// destructor, CopyData, DrawPolygons, DrawTextA): each one call, no logic
#include "ee/EEScreensShims.h"
#define SBA(x) reinterpret_cast<EEUIFMGStatusBar*>(x)
#define SBAR(x) (*reinterpret_cast<const GERectangle*>(x))
#define SBAF(x) reinterpret_cast<UIForm*>(x)
static void* EEUIFMGStatusBar::* const kSBAValues[2] = {&EEUIFMGStatusBar::mValue, &EEUIFMGStatusBar::mValue2};
extern "C" void ee_screenscpp_sba_set_pressed(uintptr_t inBar, bool inPressed) { SBA(inBar)->mPressed = inPressed; }
extern "C" float ee_screenscpp_sba_border(uintptr_t inBar) { return (float)SBA(inBar)->GetBorderThickness(); }
extern "C" float ee_screenscpp_sba_rect_left(uintptr_t inRect) { return SBAR(inRect).GetLeft(); }
extern "C" float ee_screenscpp_sba_rect_top(uintptr_t inRect) { return SBAR(inRect).GetTop(); }
extern "C" float ee_screenscpp_sba_rect_right(uintptr_t inRect) { return SBAR(inRect).GetRight(); }
extern "C" float ee_screenscpp_sba_rect_bottom(uintptr_t inRect) { return SBAR(inRect).GetBottom(); }
extern "C" float ee_screenscpp_sba_rect_width(uintptr_t inRect) { return SBAR(inRect).GetWidth(); }
extern "C" float ee_screenscpp_sba_rect_height(uintptr_t inRect) { return SBAR(inRect).GetHeight(); }
extern "C" void ee_screenscpp_sba_icon_new(uintptr_t inBar, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->mIcon = new UIBitmap(SBA(inBar)->GetId(), NULL, GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), SBA(inBar)->GetIndex()); }
extern "C" void ee_screenscpp_sba_bar_new(uintptr_t inBar, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->mBar = new UIStatusBar(SBA(inBar)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), SBA(inBar)->GetIndex(), 0); }
extern "C" void ee_screenscpp_sba_bar_verify(uintptr_t inBar) { ESVERIFY_AT(static_cast<UIStatusBar*>(SBA(inBar)->mBar), 125); }
extern "C" void ee_screenscpp_sba_bar_green(uintptr_t inBar)
{ static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetStatusBarColor(GEColor(0.0f, 1.0f, 0.0f)); }
extern "C" void ee_screenscpp_sba_bar_grey(uintptr_t inBar) { static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetBgColor(GEColor(0.5f, 0.5f, 0.5f)); }
extern "C" void ee_screenscpp_sba_bar_opaque(uintptr_t inBar) { static_cast<UIStatusBar*>(SBA(inBar)->mBar)->SetBgOpacity(1.0f); }
extern "C" float ee_screenscpp_sba_bar_top(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->GetRectangle().GetTop(); }
extern "C" float ee_screenscpp_sba_bar_bottom(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->GetRectangle().GetBottom(); }
extern "C" uintptr_t ee_screenscpp_sba_closest_font(uint32_t inHeight)
{ return (uintptr_t)UIFontTable::CreateClosestFont(1, (unsigned long)inHeight); }
extern "C" bool ee_screenscpp_sba_font_has_bitmap(uintptr_t inFont)
{ return reinterpret_cast<GEFont*>(inFont)->GetFontBitmap() != NULL; }
extern "C" bool ee_screenscpp_sba_form_gdi(uintptr_t inForm) { return SBAF(inForm)->GetUseGDIForText(); }
extern "C" void ee_screenscpp_sba_font_bitmap_new(uintptr_t inFont, uintptr_t inForm)
{ (new GEFontBitmap(SBAF(inForm)->GetManager()->GetRasterizer(), reinterpret_cast<GEFont*>(inFont), SBAF(inForm)->GetManager()->GetScreen()))->Dereference(); }
extern "C" void ee_screenscpp_sba_font_dereference(uintptr_t inFont)
{ reinterpret_cast<GEFont*>(inFont)->Dereference(); }
extern "C" void ee_screenscpp_sba_value_new(uintptr_t inBar, uint32_t inWhich, uintptr_t inFont, uintptr_t inForm, float inLeft, float inTop, float inRight, float inBottom)
{ SBA(inBar)->*kSBAValues[inWhich] = new UITextLabel(UWideString("888"), reinterpret_cast<GEFont*>(inFont), SBA(inBar)->GetId(), GERectangle(inLeft, inTop, inRight, inBottom), SBAF(inForm), 0, SBA(inBar)->GetIndex(), 0); }
extern "C" void ee_screenscpp_sba_value_verify(uintptr_t inBar) { ESVERIFY_AT(static_cast<UITextLabel*>(SBA(inBar)->mValue), 148); }
extern "C" void ee_screenscpp_sba_value_right(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->SetAlignment(kTextAlignRight); }
extern "C" void ee_screenscpp_sba_value_white(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sba_value_transparent(uintptr_t inBar, uint32_t inWhich)
{ static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->GetGEText()->SetIsTransparent(true); }
extern "C" void ee_screenscpp_sba_bitmap140_new(uintptr_t inBar) { SBA(inBar)->mBitmap140 = new GEBitmap(); }
extern "C" void ee_screenscpp_sba_bitmap144_new(uintptr_t inBar) { SBA(inBar)->mBitmap144 = new GEBitmap(); }
extern "C" void ee_screenscpp_sba_count_new(uintptr_t inBar, uintptr_t inFont)
{ SBA(inBar)->mCount = new GEText(reinterpret_cast<GEFont*>(inFont)); }
extern "C" float ee_screenscpp_sba_left(uintptr_t inBar) { return SBA(inBar)->GetRectangle().GetLeft(); }
extern "C" float ee_screenscpp_sba_top(uintptr_t inBar) { return SBA(inBar)->GetRectangle().GetTop(); }
extern "C" void ee_screenscpp_sba_count_at(uintptr_t inBar, float inX, float inY)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetLocation(inX, inY); }
extern "C" void ee_screenscpp_sba_count_white(uintptr_t inBar)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetFgColor(GEColor(1.0f, 1.0f, 1.0f)); }
extern "C" void ee_screenscpp_sba_count_transparent(uintptr_t inBar) { static_cast<GEText*>(SBA(inBar)->mCount)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_sba_set_show_count(uintptr_t inBar, bool inShow) { SBA(inBar)->mShowCount = inShow; }
extern "C" void ee_screenscpp_sba_black(uintptr_t inBar) { SBA(inBar)->SetBgColor(GEColor(0.0f, 0.0f, 0.0f)); }
extern "C" void ee_screenscpp_sba_bg_opaque(uintptr_t inBar) { SBA(inBar)->SetBgOpacity(1.0f); }
extern "C" void ee_screenscpp_sba_bar_delete(uintptr_t inBar) { delete static_cast<UIStatusBar*>(SBA(inBar)->mBar); }
extern "C" void ee_screenscpp_sba_icon_delete(uintptr_t inBar) { delete static_cast<UIBitmap*>(SBA(inBar)->mIcon); }
extern "C" void ee_screenscpp_sba_value_delete(uintptr_t inBar, uint32_t inWhich)
{ delete static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich]); }
extern "C" void ee_screenscpp_sba_bitmap140_dereference(uintptr_t inBar) { static_cast<GEBitmap*>(SBA(inBar)->mBitmap140)->Dereference(); }
extern "C" void ee_screenscpp_sba_bitmap144_dereference(uintptr_t inBar) { static_cast<GEBitmap*>(SBA(inBar)->mBitmap144)->Dereference(); }
extern "C" void ee_screenscpp_sba_count_delete(uintptr_t inBar) { delete static_cast<GEText*>(SBA(inBar)->mCount); }
extern "C" int64_t ee_screenscpp_sba_base_copy_data(uintptr_t inBar) { return SBA(inBar)->EEUIBUpgrade::CopyData(); }
extern "C" int64_t ee_screenscpp_sba_bar_copy_data(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_icon_copy_data(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_value_copy_data(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->CopyData(); }
extern "C" int64_t ee_screenscpp_sba_queued(uintptr_t inBar) { return SBA(inBar)->mNumQueued; }
extern "C" void ee_screenscpp_sba_count_text(uintptr_t inBar, EeScreensShimText inText)
{ static_cast<GEText*>(SBA(inBar)->mCount)->SetText(EEShimText(inText)); }
extern "C" void ee_screenscpp_sba_copy_enabled(uintptr_t inBar) { SBA(inBar)->mCopiedEnabled = SBA(inBar)->mEnabled; }
extern "C" bool ee_screenscpp_sba_copied_enabled(uintptr_t inBar) { return SBA(inBar)->mCopiedEnabled; }
extern "C" int64_t ee_screenscpp_sba_button_draw_polygons(uintptr_t inBar)
{ return SBA(inBar)->UIButton::DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_icon_draw_polygons(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_bar_draw_polygons(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->DrawPolygons(); }
extern "C" int64_t ee_screenscpp_sba_value_draw_polygons(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->DrawPolygons(); }
extern "C" float ee_screenscpp_sba_count_x(uintptr_t inBar) { return static_cast<GEText*>(SBA(inBar)->mCount)->GetLocation().mX; }
extern "C" float ee_screenscpp_sba_count_y(uintptr_t inBar) { return static_cast<GEText*>(SBA(inBar)->mCount)->GetLocation().mY; }
extern "C" float ee_screenscpp_sba_width(uintptr_t inBar) { return SBA(inBar)->GetWidth(); }
extern "C" float ee_screenscpp_sba_height(uintptr_t inBar) { return SBA(inBar)->GetHeight(); }
extern "C" int64_t ee_screenscpp_sba_control_draw_text(uintptr_t inBar) { return SBA(inBar)->UIControl::DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_bar_draw_text(uintptr_t inBar) { return static_cast<UIStatusBar*>(SBA(inBar)->mBar)->DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_icon_draw_text(uintptr_t inBar) { return static_cast<UIBitmap*>(SBA(inBar)->mIcon)->DrawTextA(); }
extern "C" int64_t ee_screenscpp_sba_value_draw_text(uintptr_t inBar, uint32_t inWhich)
{ return static_cast<UITextLabel*>(SBA(inBar)->*kSBAValues[inWhich])->DrawTextA(); }
extern "C" bool ee_screenscpp_sba_value2_has_text(uintptr_t inBar)
{ return static_cast<UITextLabel*>(SBA(inBar)->mValue2)->GetText(0).GetLength() > 0; }
extern "C" bool ee_screenscpp_sba_show_count(uintptr_t inBar) { return SBA(inBar)->mShowCount; }
extern "C" int64_t ee_screenscpp_sba_draw_count(uintptr_t inBar, float inLeft, float inTop, float inRight, float inBottom)
{ return SBA(inBar)->GetForm()->DrawClippedText(GERectangle(inLeft, inTop, inRight, inBottom), static_cast<GEText*>(SBA(inBar)->mCount), SBA(inBar)); }
