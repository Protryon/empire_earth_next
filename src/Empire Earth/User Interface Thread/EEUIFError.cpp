// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.cpp (__FILE__ 0x4cd490). The game's message box form and its two
// button classes: 0x260c27..0x2624a6 (the init chains of the seven colours, EEUIFEBOK, EEUIFEBCancel, EEUIFError;
// EEUIFileComboBox's ctor at 0x2624a6). Unit flags: the exe module set. The colours, constants and button members
// have provisional names.
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
#include "ee/EEUIFError.h"
#include "rust/game/screens.h"   // ee_screens_dialog_show, _open, _construct (ee_screens dialog.rs)
static const float kMessageButtonOpacity = 0.4f;
static const GEColor kMessageButtonColor(1.0f, 1.0f, 1.0f);
static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);
static const GEColor kMessageFocusedColor(0.61f, 0.46f, 0.0f);

// The message box's buttons: OK/Yes (RTTI EEUIFEBOK > UIButton; vtable 0x44a640) and No/Cancel (EEUIFEBCancel; vtable
// 0x44a6d8, its close form set by DisplayCancel), one class here: 50x20, tinted (a rhombus on the 2D draw list, ee_ge
// list2d.rs); a press marks it pressed and closes the box (or the close form)
class EEUIFEButton : public UIButton {
public:
    EEUIFEButton(UIForm* inForm, unsigned long inId)
        : UIButton(inId, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), inForm, inId, 1),
          mCallback(NULL), mData(NULL), mCloseForm(NULL), mPressed(false)
    {
        this->SetFocusedTextColor(kMessageFocusedColor);
    }
    virtual long DrawPolygons()
    {
        ee_ge_draw2d_fill(this->GetRasterizer(), this->GetScreenIndex(),
                          reinterpret_cast<const EeGeRectangle*>(&this->GetCopiedDrawRectangle()),
                          reinterpret_cast<const float*>(&kMessageButtonColor), kMessageButtonOpacity);
        return UIButton::DrawPolygons();
    }
    void SetCallback(EEUIFErrorCallback inCallback, void* inData)
    {
        this->mCallback = inCallback;
        this->mData = inData;
    }
    EEUIFErrorCallback mCallback;
    void* mData;
    UIForm* mCloseForm;
    bool mPressed;
protected:
    virtual void ButtonUp()
    {
        this->mPressed = true;
        UIButton::ButtonUp();
        gUserInterface->CloseForm(this->mCloseForm);
    }
};

// Creates the game's message box form: message label, OK, No and Cancel buttons, an edit box and the gold
// framed background, laid out for a 1024x768 design (ee_screens dialog.rs construct: the order, sizes, colours and
// layout; each step one op of the host below)
EEUIFError::EEUIFError(UIManager* inManager)
    : UIForm(7, 1, inManager, -1, true), mOk(NULL), mNo(NULL), mCancel(NULL), mError(NULL), mEditBox(NULL)
{
    ee_screens_dialog_construct(this);
}

// Destroys the message box and its controls
EEUIFError::~EEUIFError()
{
    delete this->mError;
    delete this->mOk;
    delete this->mNo;
    delete this->mCancel;
    delete this->mEditBox;
}

// the box's Display inputs while Rust shows it, and the layout's registers (the UI thread's)
static const UWideString* sDialogMessage;
static void* sDialogData;
static EEUIFErrorCallback sDialogCallbacks[2];
static UIForm* sDialogCloseForm;
static GERectangle sDialogRectangle;
static GEFont* sDialogFont;
static float sDialogFloats[9];   // the constructor's sizes, colours and layout (EE_SCREENS_DLG_FLOAT)

// Shows the box as a message, question, Yes/No/Cancel, text prompt or Cancel box (the original's DisplayMessage,
// DisplayQuestion, DisplayYesNoCancel, DisplayPassword and DisplayCancel: ee_screens dialog.rs)
void EEUIFError::Display(unsigned long inKind, const UWideString& inMessage, unsigned long inAlignment, void* inData,
                         EEUIFErrorCallback inFirst, EEUIFErrorCallback inSecond, UIForm* inCloseForm, bool inFlag)
{
    sDialogMessage = &inMessage;
    sDialogData = inData;
    sDialogCallbacks[0] = inFirst;
    sDialogCallbacks[1] = inSecond;
    sDialogCloseForm = inCloseForm;
    ee_screens_dialog_show(this, inKind, inAlignment, inFlag);
}

// The box's host (dialog.rs: EE_SCREENS_DLG_*): each op one toolkit call on its controls (0 the message, 1 OK/Yes,
// 2 No, 3 Cancel, 4 the edit box)
extern "C" uint32_t ee_screenscpp_dialog(void* inDialog, uint32_t inOp, uint32_t inA, uint32_t inB)
{
    EEUIFError* theBox = static_cast<EEUIFError*>(inDialog);
    UIControl* theControls[5] = {theBox->mError, theBox->mOk, theBox->mNo, theBox->mCancel, theBox->mEditBox};
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
            void* theData = (inB & 3) == 3 ? (void*)theBox->mEditBox->GetText(0) : sDialogData;
            static_cast<EEUIFEButton*>(theButton)->SetCallback(theCallback, theData);
        }
        theButton->SetDisplayMode(kDisplayModeShown);
        if (inA == 3 && (inB & 3) != 0)
            theBox->mCancel->mCloseForm = sDialogCloseForm;
        theButton->GetGEText()->SetFgColor(kMessageTextColor);
        theButton->GetGEText()->SetIsTransparent(true);
        break;
    }
    case EE_SCREENS_DLG_SHOW:
        theControls[inA]->SetDisplayMode(inB);
        break;
    case EE_SCREENS_DLG_MESSAGE:
        theBox->mError->SetAlignment(inA);
        theBox->mError->SetText(*sDialogMessage);
        break;
    case EE_SCREENS_DLG_MESSAGE_SIZE:
        theBox->mError->SetRectangle(GERectangle(GE2DPoint(0.0f, 0.0f), (float)inA, (float)inB));
        break;
    case EE_SCREENS_DLG_TABBING:
        theBox->SetTabbing(true);
        theBox->SetTabStop(theBox->mEditBox->GetIndex());
        break;
    case EE_SCREENS_DLG_PASSWORD:
        theBox->mEditBox->SetIsPasswordBox(inA != 0);
        break;
    case EE_SCREENS_DLG_MODE:
        theBox->mMode = inA;
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
        return (uint32_t)theBox->mError->GetLineCount();
    case EE_SCREENS_DLG_FLOAT:
        memcpy(&sDialogFloats[inA], &inB, 4);
        break;
    case EE_SCREENS_DLG_NEW: {
        GERectangle theRectangle(GE2DPoint(0.0f, 0.0f), f[0], f[1]);
        if (inA == 0)   // (the original's ESVERIFYs after each new: new never returns NULL)
            theBox->mError = new UITextLabel(UWideString(""), sDialogFont, 0, theRectangle, theBox, 1, 0, 0);
        else if (inA == 1)
            theBox->mOk = new EEUIFEButton(theBox, 1);
        else if (inA == 4)
            theBox->mEditBox = new UIEditBox(4, sDialogFont, 0x32, false, theRectangle, theBox, 4, 2, false, false);
        else
            (inA == 2 ? theBox->mNo : theBox->mCancel) = new EEUIFEButton(theBox, inA);
        break;
    }
    case EE_SCREENS_DLG_SCROLL_BAR:
        theBox->mError->SetScrollBarBgColor(GEColor(f[0], f[1], f[2]));
        theBox->mError->SetScrollBarBgOpacity(f[3]);
        break;
    case EE_SCREENS_DLG_ALIGN:
        theBox->mError->SetAlignment(inA);
        break;
    case EE_SCREENS_DLG_TEXT_COLOR: {
        GEText* theText = inA == 0 ? theBox->mError->GetGEText() : theBox->mEditBox->GetGEText();
        theText->SetIsTransparent(true);
        theText->SetFgColor(GEColor(f[0], f[1], f[2]));
        break;
    }
    case EE_SCREENS_DLG_ADD:
        theBox->AddControl(theControls[inA], 0);
        break;
    case EE_SCREENS_DLG_EDIT_COLORS:
        theBox->mEditBox->SetSelectedBgColor(GEColor(f[0], f[1], f[2]));
        theBox->mEditBox->SetBgColor(GEColor(f[3], f[4], f[5]));
        theBox->mEditBox->SetCursorColor(GEColor(f[6], f[7], f[8]));
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
        theBox->mBackground = new EEUIBGenericBackground(0, "textures\\EEGenericBack_Gold", theDescription, theLayout, theBox,
                                                         0, GEColor(f[4], f[5], f[6]), true, 1.0f, 1.0f, 1.0f, 3);
        break;
    }
    }
    return 0;
}

// Opens the message box, scales its background to the window and lays out the buttons for its mode (the original's
// LayoutCancel, LayoutPassword, LayoutQuestion, LayoutYesNoCancel and LayoutMessage: ee_screens dialog.rs)
void EEUIFError::Open()
{
    UIForm::Open();
    this->mBackground->SetScale((float)gGameWindow->GetWidth() * (1.0f / 1024.0f),
                          (float)gGameWindow->GetHeight() * (1.0f / 768.0f));
    ee_screens_dialog_open(this, this->mMode);
}

// Closes the message box and posts the pressed button's callback and data (OK first, then No, then Cancel)
void EEUIFError::Close()
{
    EEUIFErrorCallbackData* theData = new EEUIFErrorCallbackData;
    theData->mCallback = NULL;
    theData->mData = NULL;
    for (EEUIFEButton* theButton : {this->mOk, this->mNo, this->mCancel})
        if (theButton != NULL && theButton->mPressed) {
            theData->mCallback = theButton->mCallback;
            theData->mData = theButton->mData;
            break;
        }
    gUserInterface->PostErrorClosed(this, theData);
    UIForm::Close();
}
