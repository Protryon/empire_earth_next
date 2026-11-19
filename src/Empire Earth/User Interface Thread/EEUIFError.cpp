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
void EEUIFErrorDisplay(UIManager* inManager, unsigned long inKind, const UWideString& inMessage, unsigned long inAlignment,
                       void* inData, EEUIFErrorCallback inFirst, EEUIFErrorCallback inSecond, UIForm* inCloseForm,
                       bool inFlag)
{
    UIForm* theBox = new EEUIShellForm(EE_SCREENS_SK_DIALOG, {}, 7, 1, inManager, (unsigned long)-1, true);
    sDialogMessage = &inMessage;
    sDialogData = inData;
    sDialogCallbacks[0] = inFirst;
    sDialogCallbacks[1] = inSecond;
    sDialogCloseForm = inCloseForm;
    EEUIShell::Op(theBox, EE_SCREENS_SKOP_DIALOG_SHOW, (int64_t)inKind, (int64_t)(uint32_t)inAlignment | (int64_t)inFlag << 32);
}

static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);

// The box's host (dialog.rs: EE_SCREENS_DLG_*; shells_dialog.rs the box's own): each op one toolkit call on its
// controls, the kind's slots (0 the message, 1 OK/Yes, 2 No, 3 Cancel, 4 the edit box, 5 the background)
extern "C" uint32_t ee_screenscpp_dialog(void* inDialog, void** ioSlots, uint32_t inOp, uint32_t inA, uint32_t inB)
{
    UIForm* theBox = static_cast<UIForm*>(inDialog);
    UIControl** theControls = reinterpret_cast<UIControl**>(ioSlots);
    UITextLabel* theError = static_cast<UITextLabel*>(theControls[0]);
    UIEditBox* theEditBox = static_cast<UIEditBox*>(theControls[4]);
    UIButton* theButton = inA >= 1 && inA <= 3 ? static_cast<UIButton*>(theControls[inA]) : NULL;
    float* theSides[4] = {&sDialogRectangle.mLeft, &sDialogRectangle.mTop, &sDialogRectangle.mRight,
                          &sDialogRectangle.mBottom};
    const float* f = sDialogFloats;
    switch (inOp) {
    case EE_SCREENS_DLG_BUTTON: {
        UWideString theText;
        gResource->GetString(inB >> 2, theText);
        theButton->SetText(theText, gFontTahoma12);
        if ((inB & 3) != 0) {
            EEUIFErrorCallback theCallback = sDialogCallbacks[(inB & 3) == 2 ? 1 : 0];
            void* theData = (inB & 3) == 3 ? (void*)theEditBox->GetText(0) : sDialogData;
            EEUIShell::Op(theButton, EE_SCREENS_SKOP_DIALOG_SET_CALLBACK, (int64_t)theCallback, (int64_t)theData);
        }
        theButton->SetDisplayMode(kDisplayModeShown);
        if (inA == 3 && (inB & 3) != 0)
            EEUIShell::Op(theControls[3], EE_SCREENS_SKOP_DIALOG_SET_CLOSE, (int64_t)sDialogCloseForm);
        theButton->GetGEText()->SetFgColor(kMessageTextColor);
        theButton->GetGEText()->SetIsTransparent(true);
        break;
    }
    case EE_SCREENS_DLG_SHOW:
        theControls[inA]->SetDisplayMode(inB);
        break;
    case EE_SCREENS_DLG_MESSAGE:
        theError->SetAlignment(inA);
        theError->SetText(*sDialogMessage);
        break;
    case EE_SCREENS_DLG_MESSAGE_SIZE:
        theError->SetRectangle(GERectangle(GE2DPoint(0.0f, 0.0f), (float)inA, (float)inB));
        break;
    case EE_SCREENS_DLG_TABBING:
        theBox->SetTabbing(true);
        theBox->SetTabStop(theEditBox->GetIndex());
        break;
    case EE_SCREENS_DLG_PASSWORD:
        theEditBox->SetIsPasswordBox(inA != 0);
        break;
    case EE_SCREENS_DLG_SWITCH:
        gUserInterface->SwitchToForm(theBox);
        break;
    case EE_SCREENS_DLG_CALC: {
        UILayoutUtility theLayout;
        GERectangle theScreen(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
        theLayout.SetLeft(4, (float)(inA & 0xffff) / 1024.0f);
        theLayout.SetRight(4, (float)(inA >> 16) / 1024.0f);
        theLayout.SetTop(4, (float)(inB & 0xffff) / 768.0f);
        theLayout.SetBottom(4, (float)(inB >> 16) / 768.0f);
        theLayout.CalculateRectangle(theScreen, sDialogRectangle, true);
        break;
    }
    case EE_SCREENS_DLG_PLACE:
        theControls[inA]->SetRectangle(sDialogRectangle);
        break;
    case EE_SCREENS_DLG_FONT:
        if (inA == 0) {
            GERectangle theScreen(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
            sDialogFont = UIFontTable::CreateClosestFont(0x1c, UMFloatToLong((gFontTahoma16->GetHeight() * (1.0f / 768.0f)) *
                                                                             theScreen.GetHeight()));
        } else if (inA == 4)
            sDialogFont->Dereference();
        else
            theButton->GetGEText()->SetFont(sDialogFont);
        break;
    case EE_SCREENS_DLG_RECT: {
        float theSide = *theSides[inA];
        uint32_t theBits;
        memcpy(&theBits, &theSide, 4);
        return theBits;
    }
    case EE_SCREENS_DLG_SET_RECT:
        memcpy(theSides[inA], &inB, 4);
        break;
    case EE_SCREENS_DLG_LINE_HEIGHT:
        return (uint32_t)gFontTahoma12->GetHeight();
    case EE_SCREENS_DLG_LINES:
        return (uint32_t)theError->GetLineCount();
    case EE_SCREENS_DLG_FLOAT:
        memcpy(&sDialogFloats[inA], &inB, 4);
        break;
    case EE_SCREENS_DLG_NEW: {
        GERectangle theRectangle(GE2DPoint(0.0f, 0.0f), f[0], f[1]);
        if (inA == 0)   // (the original's ESVERIFYs after each new: new never returns NULL)
            theControls[0] = new UITextLabel(UWideString(""), sDialogFont, 0, theRectangle, theBox, 1, 0, 0);
        else if (inA == 4)
            theControls[4] = new UIEditBox(4, sDialogFont, 0x32, false, theRectangle, theBox, 4, 2, false, false);
        else   // OK/Yes (EEUIFEBOK), No and Cancel (EEUIFEBCancel): 50x20, a one-pixel border, the index the id
            theControls[inA] = new EEUIShellButton(inA == 1 ? EE_SCREENS_SK_DIALOG_OK : EE_SCREENS_SK_DIALOG_CANCEL, {}, inA,
                                                   NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f),
                                                   theBox, inA, 1);
        break;
    }
    case EE_SCREENS_DLG_SCROLL_BAR:
        theError->SetScrollBarBgColor(GEColor(f[0], f[1], f[2]));
        theError->SetScrollBarBgOpacity(f[3]);
        break;
    case EE_SCREENS_DLG_ALIGN:
        theError->SetAlignment(inA);
        break;
    case EE_SCREENS_DLG_TEXT_COLOR: {
        GEText* theText = inA == 0 ? theError->GetGEText() : theEditBox->GetGEText();
        theText->SetIsTransparent(true);
        theText->SetFgColor(GEColor(f[0], f[1], f[2]));
        break;
    }
    case EE_SCREENS_DLG_ADD:
        theBox->AddControl(theControls[inA], 0);
        break;
    case EE_SCREENS_DLG_EDIT_COLORS:
        theEditBox->SetSelectedBgColor(GEColor(f[0], f[1], f[2]));
        theEditBox->SetBgColor(GEColor(f[3], f[4], f[5]));
        theEditBox->SetCursorColor(GEColor(f[6], f[7], f[8]));
        break;
    case EE_SCREENS_DLG_ENABLE:
        theBox->GetEntityGroup(0)->Enable();
        break;
    case EE_SCREENS_DLG_BACKGROUND: {
        UILayoutUtility theLayout;
        theLayout.SetLeft(4, f[0]);
        theLayout.SetRight(4, f[1]);
        theLayout.SetTop(4, f[2]);
        theLayout.SetBottom(4, f[3]);
        theBox->SetLayout(theLayout);
        GETextureDescription theDescription;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mPixelDepth = theBox->GetManager()->GetUITextureBitDepth();
        theDescription.mNumMipLevelsToDrop = 0;
        theDescription.mSurfaceVolatility = 1;
        theDescription.mFrameFormat = 0;
        theDescription.mHasAlpha = true;
        theDescription.mCreateMIPMaps = true;
        ioSlots[5] = new EEUIBGenericBackground(0, "textures\\EEGenericBack_Gold", theDescription, theLayout, theBox,
                                                         0, GEColor(f[4], f[5], f[6]), true, 1.0f, 1.0f, 1.0f, 3);
        break;
    }
    // the box's own (shells_dialog.rs): the destructor's deletes, Open's background scale, Close's callback posted
    case EE_SCREENS_DLG_DELETE:
        for (int i = 0; i < 5; i++)
            delete theControls[i];
        break;
    case EE_SCREENS_DLG_SCALE_BACKGROUND:
        static_cast<EEUIBGenericBackground*>(ioSlots[5])->SetScale((float)gGameWindow->GetWidth() * (1.0f / 1024.0f),
                                                                   (float)gGameWindow->GetHeight() * (1.0f / 768.0f));
        break;
    case EE_SCREENS_DLG_POST_CLOSED: {
        EEUIFErrorCallbackData* theData = new EEUIFErrorCallbackData;
        theData->mCallback = NULL;
        theData->mData = NULL;
        for (int i = 1; i <= 3; i++)
            if (theControls[i] != NULL && EEUIShell::Op(theControls[i], EE_SCREENS_SKOP_DIALOG_PRESSED)) {
                theData->mCallback = (EEUIFErrorCallback)EEUIShell::Op(theControls[i], EE_SCREENS_SKOP_DIALOG_CALLBACK);
                theData->mData = (void*)EEUIShell::Op(theControls[i], EE_SCREENS_SKOP_DIALOG_DATA);
                break;
            }
        gUserInterface->PostErrorClosed(theBox, theData);
        break;
    }
    }
    return 0;
}
