// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.cpp (__FILE__ 0x4cd490). The game's message box:
// 0x260c27..0x2624a6 (EEUIFEBOK, EEUIFEBCancel, EEUIFError; EEUIFileComboBox's ctor at 0x2624a6). The form and its
// buttons are kinds over the shells (ee_game shells_dialog.rs); this unit keeps the box's host. Unit flags: the exe
// module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "rust/render/ge.h"   // ee_ge_draw2d_fill (ee_ge list2d.rs)
#include "lle/UIEntityGroup.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIBGenericBackground.h"
#include "ee/EEUIShell.h"
// the box's Display inputs while Rust shows it, and the layout's registers (the UI thread's)
static const UWideString* sDialogMessage;
static void* sDialogData;
static EEUIFErrorCallback sDialogCallbacks[2];
static UIForm* sDialogCloseForm;
static GERectangle sDialogRectangle;
static GEFont* sDialogFont;
static float sDialogFloats[9];   // the constructor's sizes, colours and layout (EE_SCREENS_DLG_FLOAT)

// A new box (7 controls, 1 group, no id; its constructor ee_screens dialog.rs construct) shown as inKind
// (EEUIFErrorDisplay, ee/EEUIManager.h, through the game's UI table)
static void EEUIFErrorShow(UIManager* inManager, unsigned long inKind, const UWideString& inMessage,
                           unsigned long inAlignment, void* inData, EEUIFErrorCallback inFirst,
                           EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag)
{
    UIForm* theBox = new EEUIShellForm(EE_SCREENS_SK_DIALOG, {}, 7, 1, inManager, (unsigned long)-1, true);
    sDialogMessage = &inMessage;
    sDialogData = inData;
    sDialogCallbacks[0] = inFirst;
    sDialogCallbacks[1] = inSecond;
    sDialogCloseForm = inCloseForm;
    EEUIShell::Op(theBox, EE_SCREENS_SKOP_DIALOG_SHOW, (int64_t)inKind, (int64_t)(uint32_t)inAlignment | (int64_t)inFlag << 32);
}
static const bool sErrorDisplayRegistered = (gEEGameUI.mErrorDisplay = EEUIFErrorShow, true);

static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);
static float* const kDialogSides[4] = {&sDialogRectangle.mLeft, &sDialogRectangle.mTop, &sDialogRectangle.mRight, &sDialogRectangle.mBottom};
static UWideString sDialogText;          // a button's text (a local of the original case)
static UILayoutUtility sDialogLayout;    // the layout's and the background's layout (locals of the original cases)

// the moved cases' one-statement shims (ee_screens dialog_calls.rs asks them first)
#define DLG_CTL(x) reinterpret_cast<UIControl*>(x)
#define DLG_BUTTON(x) static_cast<UIButton*>(reinterpret_cast<UIControl*>(x))
#define DLG_BOX(x) reinterpret_cast<UIForm*>(x)
extern "C" void ee_screenscpp_dlg_text_load(uint32_t inIds) { gResource->GetString(inIds, sDialogText); }
extern "C" void ee_screenscpp_dlg_button_set_text(uintptr_t inButton)
{ DLG_BUTTON(inButton)->SetText(sDialogText, gFontTahoma12); }
extern "C" uintptr_t ee_screenscpp_dlg_callback(uint32_t inIndex) { return (uintptr_t)sDialogCallbacks[inIndex]; }
extern "C" uintptr_t ee_screenscpp_dlg_edit_text(uintptr_t inEdit)
{ return (uintptr_t)static_cast<UIEditBox*>(DLG_CTL(inEdit))->GetText(0); }
extern "C" uintptr_t ee_screenscpp_dlg_data() { return (uintptr_t)sDialogData; }
extern "C" void ee_screenscpp_dlg_set_callback(uintptr_t inButton, uintptr_t inCallback, uintptr_t inData)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CALLBACK, (int64_t)inCallback, (int64_t)inData); }
extern "C" void ee_screenscpp_dlg_show_button(uintptr_t inButton)
{ DLG_CTL(inButton)->SetDisplayMode(kDisplayModeShown); }
extern "C" void ee_screenscpp_dlg_set_close(uintptr_t inButton)
{ EEUIShell::Op(DLG_CTL(inButton), EE_SCREENS_SKOP_DIALOG_SET_CLOSE, (int64_t)sDialogCloseForm); }
extern "C" void ee_screenscpp_dlg_button_text_color(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFgColor(kMessageTextColor); }
extern "C" void ee_screenscpp_dlg_button_text_transparent(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetIsTransparent(true); }
extern "C" float ee_screenscpp_dlg_window_width() { return (float)gGameWindow->GetWidth(); }
extern "C" float ee_screenscpp_dlg_window_height() { return (float)gGameWindow->GetHeight(); }
extern "C" void ee_screenscpp_dlg_layout_reset() { sDialogLayout = UILayoutUtility(); }
extern "C" void ee_screenscpp_dlg_layout_left(float inV) { sDialogLayout.SetLeft(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_right(float inV) { sDialogLayout.SetRight(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_top(float inV) { sDialogLayout.SetTop(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_bottom(float inV) { sDialogLayout.SetBottom(4, inV); }
extern "C" void ee_screenscpp_dlg_layout_calculate(float inWidth, float inHeight)
{ sDialogLayout.CalculateRectangle(GERectangle(0.0f, 0.0f, inWidth, inHeight), sDialogRectangle, true); }
extern "C" float ee_screenscpp_dlg_font16_height() { return (float)gFontTahoma16->GetHeight(); }
extern "C" void ee_screenscpp_dlg_font_create(int32_t inHeight)
{ sDialogFont = UIFontTable::CreateClosestFont(0x1c, (long)inHeight); }
extern "C" void ee_screenscpp_dlg_font_release() { sDialogFont->Dereference(); }
extern "C" void ee_screenscpp_dlg_button_font(uintptr_t inButton)
{ DLG_BUTTON(inButton)->GetGEText()->SetFont(sDialogFont); }
extern "C" uintptr_t ee_screenscpp_dlg_new_label(uintptr_t inDialog)
{ return (uintptr_t) new UITextLabel(UWideString(""), sDialogFont, 0, GERectangle(GE2DPoint(0.0f, 0.0f), sDialogFloats[0], sDialogFloats[1]), DLG_BOX(inDialog), 1, 0, 0); }
extern "C" uintptr_t ee_screenscpp_dlg_new_edit(uintptr_t inDialog)
{ return (uintptr_t) new UIEditBox(4, sDialogFont, 0x32, false, GERectangle(GE2DPoint(0.0f, 0.0f), sDialogFloats[0], sDialogFloats[1]), DLG_BOX(inDialog), 4, 2, false, false); }
extern "C" uintptr_t ee_screenscpp_dlg_new_button(uintptr_t inDialog, uint32_t inKind, uint32_t inIndex)
{ return (uintptr_t) new EEUIShellButton(inKind, {}, inIndex, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), DLG_BOX(inDialog), inIndex, 1); }
extern "C" void ee_screenscpp_dlg_edit_selected_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetSelectedBgColor(GEColor(sDialogFloats[0], sDialogFloats[1], sDialogFloats[2])); }
extern "C" void ee_screenscpp_dlg_edit_bg_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetBgColor(GEColor(sDialogFloats[3], sDialogFloats[4], sDialogFloats[5])); }
extern "C" void ee_screenscpp_dlg_edit_cursor_color(uintptr_t inEdit)
{ static_cast<UIEditBox*>(DLG_CTL(inEdit))->SetCursorColor(GEColor(sDialogFloats[6], sDialogFloats[7], sDialogFloats[8])); }
extern "C" float ee_screenscpp_dlg_float(uint32_t inIndex) { return sDialogFloats[inIndex]; }
extern "C" void ee_screenscpp_dlg_box_set_layout(uintptr_t inDialog) { DLG_BOX(inDialog)->SetLayout(sDialogLayout); }
extern "C" uintptr_t ee_screenscpp_dlg_new_background(uintptr_t inDialog)
{ return (uintptr_t) new EEUIBGenericBackground(0, "textures\EEGenericBack_Gold", GETextureDescription{1, 0, DLG_BOX(inDialog)->GetManager()->GetUITextureBitDepth(), 0, false, false, true, true}, sDialogLayout, DLG_BOX(inDialog), 0, GEColor(sDialogFloats[4], sDialogFloats[5], sDialogFloats[6]), true, 1.0f, 1.0f, 1.0f, 3); }
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
    case EE_SCREENS_DLG_MESSAGE:
        theError->SetAlignment(inA);
        theError->SetText(*sDialogMessage);
        break;
    case EE_SCREENS_DLG_MESSAGE_SIZE:
        theError->SetRectangle(GERectangle(GE2DPoint(0.0f, 0.0f), (float)inA, (float)inB));
        break;
    case EE_SCREENS_DLG_SWITCH:
        gUserInterface->SwitchToForm(theBox);
        break;
    case EE_SCREENS_DLG_PLACE:
        theControls[inA]->SetRectangle(sDialogRectangle);
        break;
    case EE_SCREENS_DLG_RECT: {
        float theSide = *kDialogSides[inA];
        uint32_t theBits;
        memcpy(&theBits, &theSide, 4);
        return theBits;
    }
    case EE_SCREENS_DLG_SET_RECT:
        memcpy(kDialogSides[inA], &inB, 4);
        break;
    case EE_SCREENS_DLG_LINE_HEIGHT:
        return (uint32_t)gFontTahoma12->GetHeight();
    case EE_SCREENS_DLG_FLOAT:
        memcpy(&sDialogFloats[inA], &inB, 4);
        break;
    }
    return 0;
}
#undef theBox
#undef theControls
#undef theError
