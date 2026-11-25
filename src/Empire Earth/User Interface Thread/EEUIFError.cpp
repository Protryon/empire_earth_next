// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.cpp (__FILE__ 0x4cd490). The game's message box:
// 0x260c27..0x2624a6 (EEUIFEBOK, EEUIFEBCancel, EEUIFError; EEUIFileComboBox's ctor at 0x2624a6). The form and its
// buttons are kinds over the shells (ee_game shells_dialog.rs); this unit keeps the box's host. Unit flags: the exe
// module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GERhombus.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIBGenericBackground.h"
#include "ee/EEUIShell.h"
#include "ee/EEScreensShims.h"
// the layout's registers (the UI thread's; the box's Display inputs are dialog_inputs_ffi.rs')

// A new box (7 controls, 1 group, no id; its constructor ee_screens dialog.rs construct) shown as inKind
// (EEUIFErrorDisplay, ee/EEUIManager.h, through the game's UI table)
static void EEUIFErrorShow(UIManager* inManager, unsigned long inKind, const UWideString& inMessage,
                           unsigned long inAlignment, void* inData, EEUIFErrorCallback inFirst,
                           EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag)
{
    ee_screens_dialog_show(static_cast<UIForm*>(new EEUIShellForm(EE_SCREENS_SK_DIALOG, {}, 7, 1, inManager, (unsigned long)-1, true)),
                           (uint32_t)inKind, &inMessage, (uint32_t)inAlignment, inData, reinterpret_cast<const void*>(inFirst),
                           reinterpret_cast<const void*>(inSecond), inCloseForm, inFlag);
}
static const bool sErrorDisplayRegistered = (gEEGameUI.mErrorDisplay = EEUIFErrorShow, true);

static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);
static float* const kDialogSides[4] = {&EEScratch<GERectangle>(kScratchDialogRectangle).mLeft, &EEScratch<GERectangle>(kScratchDialogRectangle).mTop, &EEScratch<GERectangle>(kScratchDialogRectangle).mRight, &EEScratch<GERectangle>(kScratchDialogRectangle).mBottom};

// the moved cases' one-statement shims (ee_screens dialog_calls.rs asks them first)
#define DLG_CTL(x) reinterpret_cast<UIControl*>(x)
#define DLG_BUTTON(x) static_cast<UIButton*>(reinterpret_cast<UIControl*>(x))
#define DLG_BOX(x) reinterpret_cast<UIForm*>(x)
extern "C" void ee_screenscpp_dlg_text_load(uint32_t inIds) { gResource->GetString(inIds, EEScratch<UWideString>(kScratchDialogText)); }
extern "C" void ee_screenscpp_dlg_button_set_text(uintptr_t inButton)
{ DLG_BUTTON(inButton)->SetText(EEScratch<UWideString>(kScratchDialogText), gFontTahoma12); }
extern "C" uintptr_t ee_screenscpp_dlg_edit_text(uintptr_t inEdit)
{ return (uintptr_t)static_cast<UIEditBox*>(DLG_CTL(inEdit))->GetText(0); }
extern "C" void ee_screenscpp_dlg_message_align(uintptr_t inLabel, uint32_t inAlignment)
{ static_cast<UITextLabel*>(DLG_CTL(inLabel))->SetAlignment(inAlignment); }
extern "C" void ee_screenscpp_dlg_message_text(uintptr_t inLabel)
{ static_cast<UITextLabel*>(DLG_CTL(inLabel))->SetText(*static_cast<const UWideString*>(ee_screens_dlg_message())); }
extern "C" float ee_screenscpp_dlg_rect_side(uint32_t inSide) { return *kDialogSides[inSide]; }
extern "C" void ee_screenscpp_dlg_set_callback(uintptr_t inButton, uintptr_t inCallback, uintptr_t inData)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CALLBACK, (int64_t)inCallback, (int64_t)inData); }
extern "C" void ee_screenscpp_dlg_show_button(uintptr_t inButton)
{ DLG_CTL(inButton)->SetDisplayMode(kDisplayModeShown); }
extern "C" void ee_screenscpp_dlg_set_close(uintptr_t inButton)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CLOSE, (int64_t)ee_screens_dlg_close_form()); }
extern "C" void ee_screenscpp_dlg_button_text_color(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFgColor(kMessageTextColor); }
extern "C" void ee_screenscpp_dlg_button_text_transparent(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetIsTransparent(true); }
extern "C" float ee_screenscpp_dlg_window_width() { return (float)gGameWindow->GetWidth(); }
extern "C" float ee_screenscpp_dlg_window_height() { return (float)gGameWindow->GetHeight(); }
extern "C" void ee_screenscpp_dlg_layout_reset() { EEScratch<UILayoutUtility>(kScratchDialogLayout) = UILayoutUtility(); }
extern "C" void ee_screenscpp_dlg_layout_left(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetLeft(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_right(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetRight(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_top(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetTop(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_bottom(float inV) { EEScratch<UILayoutUtility>(kScratchDialogLayout).SetBottom(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_calculate(float inWidth, float inHeight)
{ EEScratch<UILayoutUtility>(kScratchDialogLayout).CalculateRectangle(GERectangle(0.0f, 0.0f, inWidth, inHeight), EEScratch<GERectangle>(kScratchDialogRectangle), true); }
extern "C" float ee_screenscpp_dlg_font16_height() { return (float)gFontTahoma16->GetHeight(); }
extern "C" void ee_screenscpp_dlg_font_create(int32_t inHeight)
{ EEScratch<GEFont*>(kScratchDialogFont) = UIFontTable::CreateClosestFont(0x1c, (long)inHeight); }
extern "C" void ee_screenscpp_dlg_font_release() { EEScratch<GEFont*>(kScratchDialogFont)->Dereference(); }
extern "C" void ee_screenscpp_dlg_button_font(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFont(EEScratch<GEFont*>(kScratchDialogFont)); }
extern "C" uintptr_t ee_screenscpp_dlg_new_label(uintptr_t inDialog)
{ return (uintptr_t) new UITextLabel(UWideString(""), EEScratch<GEFont*>(kScratchDialogFont), 0, GERectangle(GE2DPoint(0.0f, 0.0f), EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1]), DLG_BOX(inDialog), 1, 0, 0); }
extern "C" uintptr_t ee_screenscpp_dlg_new_edit(uintptr_t inDialog)
{ return (uintptr_t) new UIEditBox(4, EEScratch<GEFont*>(kScratchDialogFont), 0x32, false, GERectangle(GE2DPoint(0.0f, 0.0f), EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1]), DLG_BOX(inDialog), 4, 2, false, false); }
extern "C" uintptr_t ee_screenscpp_dlg_new_button(uintptr_t inDialog, uint32_t inKind, uint32_t inIndex)
{ return (uintptr_t) new EEUIShellButton(inKind, {}, inIndex, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), DLG_BOX(inDialog), inIndex, 1); }
extern "C" void ee_screenscpp_dlg_edit_selected_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetSelectedBgColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[0], EEScratch<float[9]>(kScratchDialogFloats)[1], EEScratch<float[9]>(kScratchDialogFloats)[2])); }
extern "C" void ee_screenscpp_dlg_edit_bg_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetBgColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[3], EEScratch<float[9]>(kScratchDialogFloats)[4], EEScratch<float[9]>(kScratchDialogFloats)[5])); }
extern "C" void ee_screenscpp_dlg_edit_cursor_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetCursorColor(GEColor(EEScratch<float[9]>(kScratchDialogFloats)[6], EEScratch<float[9]>(kScratchDialogFloats)[7], EEScratch<float[9]>(kScratchDialogFloats)[8])); }
extern "C" float ee_screenscpp_dlg_float(uint32_t inIndex) { return EEScratch<float[9]>(kScratchDialogFloats)[inIndex]; }
extern "C" void ee_screenscpp_dlg_box_set_layout(uintptr_t inDialog) { DLG_BOX(inDialog)->SetLayout(EEScratch<UILayoutUtility>(kScratchDialogLayout)); }
extern "C" uintptr_t ee_screenscpp_dlg_new_background(uintptr_t inDialog)
{ return (uintptr_t) new EEUIBGenericBackground(0, "textures\\EEGenericBack_Gold", GETextureDescription{1, 0, DLG_BOX(inDialog)->GetManager()->GetUITextureBitDepth(), 0, false, false, true, true}, EEScratch<UILayoutUtility>(kScratchDialogLayout), DLG_BOX(inDialog), 0, GEColor(EEScratch<float[9]>(kScratchDialogFloats)[4], EEScratch<float[9]>(kScratchDialogFloats)[5], EEScratch<float[9]>(kScratchDialogFloats)[6]), true, 1.0f, 1.0f, 1.0f, 3); }
extern "C" void ee_screenscpp_dlg_delete(uintptr_t inControl) { delete DLG_CTL(inControl); }
extern "C" void ee_screenscpp_dlg_scale_background(uintptr_t inBackground)
{ reinterpret_cast<EEUIBGenericBackground*>(inBackground)->SetScale((float)gGameWindow->GetWidth() * (1.0f / 1024.0f), (float)gGameWindow->GetHeight() * (1.0f / 768.0f)); }
extern "C" bool ee_screenscpp_dlg_pressed(uintptr_t inButton)
{ return EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_PRESSED) != 0; }
extern "C" uintptr_t ee_screenscpp_dlg_pressed_callback(uintptr_t inButton)
{ return (uintptr_t)EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_CALLBACK); }
extern "C" uintptr_t ee_screenscpp_dlg_pressed_data(uintptr_t inButton)
{ return (uintptr_t)EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_DATA); }
extern "C" void ee_screenscpp_dlg_post_closed(uintptr_t inDialog, uintptr_t inCallback, uintptr_t inData)
{ gUserInterface->PostErrorClosed(DLG_BOX(inDialog), new EEUIFErrorCallbackData{(EEUIFErrorCallback)inCallback, (void*)inData}); }

// The box's host (dialog.rs: EE_SCREENS_DLG_*; shells_dialog.rs the box's own): each op one toolkit call on its
// controls, the kind's slots (0 the message, 1 OK/Yes, 2 No, 3 Cancel, 4 the edit box, 5 the background); the plain
// toolkit calls (show, tabbing, password, lines, scroll bar, alignment, text colours, add, enable) are
// shells_dialog.rs' on the toolkit's Rust objects
extern "C" uint32_t ee_screenscpp_dialog(void* inDialog, void** ioSlots, uint32_t inOp, uint32_t inA, uint32_t inB)
{
#define theBox static_cast<UIForm*>(inDialog)
#define theControls reinterpret_cast<UIControl**>(ioSlots)
#define theError static_cast<UITextLabel*>(theControls[0])
    switch (inOp) {
    case EE_SCREENS_DLG_MESSAGE_SIZE:
        theError->SetRectangle(GERectangle(GE2DPoint(0.0f, 0.0f), (float)inA, (float)inB));
        break;
    case EE_SCREENS_DLG_SWITCH: gUserInterface->SwitchToForm(theBox); break;
    case EE_SCREENS_DLG_PLACE: theControls[inA]->SetRectangle(EEScratch<GERectangle>(kScratchDialogRectangle)); break;
    case EE_SCREENS_DLG_SET_RECT: memcpy(kDialogSides[inA], &inB, 4); break;
    case EE_SCREENS_DLG_LINE_HEIGHT: return (uint32_t)gFontTahoma12->GetHeight();
    case EE_SCREENS_DLG_FLOAT: memcpy(&EEScratch<float[9]>(kScratchDialogFloats)[inA], &inB, 4); break;
    }
    return 0;
}
#undef theBox
#undef theControls
#undef theError
