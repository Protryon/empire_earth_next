// C:\EEX\Empire Earth\User Interface Thread\EEUIFError.cpp (__FILE__ 0x4cd490). The game's message box form and its two
// button classes: 0x260c27..0x2624a6 (the init chains of the seven colours, EEUIFEBOK, EEUIFEBCancel, EEUIFError;
// EEUIFileComboBox's ctor at 0x2624a6). Unit flags: the exe module set. The colours, constants and button members
// have provisional names.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
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
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIBGenericBackground.h"
#include "ee/EEUIFError.h"
#include "ee/EEStringIDs.h"
#include "lle/UILayoutUtility.h"
#include "rust/ee_screens.h"   // ee_screens_dialog_show, _open (ee_screens dialog.rs)
static const float kMessageScrollBarOpacity = 0.3f;             // 0x44a634
static const float kMessageButtonOpacity = 0.4f;                // 0x44a638
static const GEColor kMessageScrollBarColor(1.0f, 1.0f, 1.0f);  // 0x5645f8
static const GEColor kMessageButtonColor(1.0f, 1.0f, 1.0f);     // 0x5645e8
static const GEColor kMessageTextColor(0.0f, 0.0f, 0.0f);       // 0x5645d8
static const GEColor kMessageFocusedColor(0.61f, 0.46f, 0.0f);  // 0x5645c8
static const GEColor kMessageEditBoxColor(1.0f, 1.0f, 1.0f);    // 0x5645b8
static const GEColor kMessageSelectedColor(0.61f, 0.46f, 0.0f); // 0x5645a8
static const GEColor kMessageBackgroundColor(0.91f, 0.85f, 0.73f); // 0x564598

// The OK button (RTTI EEUIFEBOK > UIButton; vtable 0x44a640; sizeof 0x110). The in-class virtuals follow the ctor
// with the ??_G in declaration order.
class EEUIFEBOK : public UIButton {
    friend class EEUIFError;
public:
    EEUIFEBOK(UIForm* inForm, unsigned long inId);
    virtual long DrawPolygons()
    {
        GERhombus theRhombus;
        theRhombus.SetColor(kMessageButtonColor);
        theRhombus.SetRectangle(this->mCopiedDrawRectangle);
        theRhombus.SetOpacity(kMessageButtonOpacity);
        this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        return UIButton::DrawPolygons();
    }
    virtual ~EEUIFEBOK() {}
    void SetCallback(EEUIFErrorCallback inCallback, void* inData)
    {
        this->mCallback = inCallback;
        this->mData = inData;
    }
protected:
    virtual void ButtonUp();

    EEUIFErrorCallback mCallback;           // +0x104
    void* mData;                            // +0x108
    bool mPressed;                          // +0x10c
};

// The No and Cancel buttons (RTTI EEUIFEBCancel > UIButton; vtable 0x44a6d8; sizeof 0x114).
class EEUIFEBCancel : public UIButton {
    friend class EEUIFError;
    friend uint32_t ee_screenscpp_dialog(void* inDialog, uint32_t inOp, uint32_t inA, uint32_t inB);
public:
    EEUIFEBCancel(UIForm* inForm, unsigned long inId);
    virtual long DrawPolygons()
    {
        GERhombus theRhombus;
        theRhombus.SetColor(kMessageButtonColor);
        theRhombus.SetRectangle(this->mCopiedDrawRectangle);
        theRhombus.SetOpacity(kMessageButtonOpacity);
        this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        return UIButton::DrawPolygons();
    }
    virtual ~EEUIFEBCancel() {}
    void SetCallback(EEUIFErrorCallback inCallback, void* inData)
    {
        this->mCallback = inCallback;
        this->mData = inData;
    }
protected:
    virtual void ButtonUp();

    EEUIFErrorCallback mCallback;           // +0x104
    void* mData;                            // +0x108
    UIForm* mCloseForm;                     // +0x10c (DisplayCancel)
    bool mPressed;                          // +0x110
};

// FUNCTION: EXE 0x660d85
// Creates the message box's OK/Yes button (50x20) with its callback unset
EEUIFEBOK::EEUIFEBOK(UIForm* inForm, unsigned long inId)
    : UIButton(inId, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), inForm, inId, 1),
      mCallback(NULL), mData(NULL)
{
    this->SetFocusedTextColor(kMessageFocusedColor);
    this->mPressed = false;
}

// FUNCTION: EXE 0x660e01
// Draws the button over a light translucent panel
// (EEUIFEBOK::DrawPolygons, in the class)

// SYNTHETIC: EXE 0x660e7b
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFEBOK+0x0)

// FUNCTION: EXE 0x660e9e
// Marks the OK/Yes button pressed and closes the message box
void EEUIFEBOK::ButtonUp()
{
    this->mPressed = true;
    UIButton::ButtonUp();
    gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x660eb9
// Creates a No or Cancel button (50x20) with its callback and close form unset
EEUIFEBCancel::EEUIFEBCancel(UIForm* inForm, unsigned long inId)
    : UIButton(inId, NULL, NULL, NULL, GERectangle(GE2DPoint(0.0f, 0.0f), 50.0f, 20.0f), inForm, inId, 1),
      mCallback(NULL), mData(NULL), mCloseForm(NULL)
{
    this->SetFocusedTextColor(kMessageFocusedColor);
    this->mPressed = false;
}

// FUNCTION: EXE 0x660f3b
// Draws the button over a light translucent panel
// (EEUIFEBCancel::DrawPolygons, in the class)

// SYNTHETIC: EXE 0x660fb5
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFEBCancel+0x0)

// FUNCTION: EXE 0x660fd8
// Marks the No/Cancel button pressed and closes the message box (or its close form)
void EEUIFEBCancel::ButtonUp()
{
    this->mPressed = true;
    UIButton::ButtonUp();
    gUserInterface->CloseForm(this->mCloseForm);
}

// FUNCTION: EXE 0x660ffb
// Creates the game's message box form: message label, OK, No and Cancel buttons, an edit box and the gold
// framed background, laid out for a 1024x768 design
EEUIFError::EEUIFError(UIManager* inManager)
    : UIForm(7, 1, inManager, -1, true)
{
    GERectangle theScreen(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
    UILayoutUtility theLayout;
    GETextureDescription theDescription;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    GEFont* theFont = UIFontTable::CreateClosestFont(0x1c, UMFloatToLong((gFontTahoma16->GetHeight() * (1.0f / 768.0f)) *
                                                                         theScreen.GetHeight()));
    this->mError = new UITextLabel(UWideString(""), theFont, 0, GERectangle(GE2DPoint(0.0f, 0.0f), 280.0f, 135.0f), this, 1,
                             0, 0);
    ESVERIFY_AT(this->mError, 220);
    this->mError->SetScrollBarBgColor(kMessageScrollBarColor);
    this->mError->SetScrollBarBgOpacity(kMessageScrollBarOpacity);
    this->mError->SetAlignment(kTextAlignCenter);
    GEText* theGEText = this->mError->GetGEText();
    theGEText->SetIsTransparent(true);
    theGEText->SetFgColor(kMessageTextColor);
    this->AddControl(this->mError, 0);

    this->mOk = new EEUIFEBOK(this, 1);
    ESVERIFY_AT(this->mOk, 236);
    this->AddControl(this->mOk, 0);

    this->mNo = new EEUIFEBCancel(this, 2);
    ESVERIFY_AT(this->mNo, 244);
    this->AddControl(this->mNo, 0);

    this->mCancel = new EEUIFEBCancel(this, 3);
    ESVERIFY_AT(this->mCancel, 252);
    this->AddControl(this->mCancel, 0);

    this->mEditBox = new UIEditBox(4, theFont, 0x32, false, GERectangle(GE2DPoint(0.0f, 0.0f), 220.0f, 24.0f), this, 4, 2,
                             false, false);
    ESVERIFY_AT(this->mEditBox, 260);
    this->mEditBox->GetGEText()->SetIsTransparent(true);
    this->mEditBox->GetGEText()->SetFgColor(kMessageTextColor);
    this->mEditBox->SetSelectedBgColor(kMessageSelectedColor);
    this->mEditBox->SetBgColor(kMessageEditBoxColor);
    this->mEditBox->SetCursorColor(kMessageTextColor);
    this->AddControl(this->mEditBox, 0);
    this->GetEntityGroup(0)->Enable();
    this->mMode = 0;
    theLayout.SetLeft(4, 295.0f / 1024.0f);
    theLayout.SetRight(4, 730.0f / 1024.0f);
    theLayout.SetTop(4, 262.0f / 768.0f);
    theLayout.SetBottom(4, 506.0f / 768.0f);
    this->SetLayout(theLayout);
    theDescription.mPixelDepth = this->GetManager()->GetUITextureBitDepth();
    theDescription.mHasAlpha = true;
    this->mBackground = new EEUIBGenericBackground(0, "textures\\EEGenericBack_Gold", theDescription, theLayout, this, 0,
                                             kMessageBackgroundColor, true, 1.0f, 1.0f, 1.0f, 3);
    theFont->Dereference();
}

// SYNTHETIC: EXE 0x66149b
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFError+0x0)

// FUNCTION: EXE 0x6614b7
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

// FUNCTION: EXE 0x66151a
// FUNCTION: EXE 0x66167f
// FUNCTION: EXE 0x661788
// FUNCTION: EXE 0x661894
// FUNCTION: EXE 0x6619f9
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
    switch (inOp) {
    case EE_SCREENS_DLG_BUTTON: {
        UWideString theText;
        gResource->GetString(inB >> 2, theText);
        theButton->SetText(theText, gFontTahoma12);
        if ((inB & 3) != 0) {
            EEUIFErrorCallback theCallback = sDialogCallbacks[(inB & 3) == 2 ? 1 : 0];
            void* theData = (inB & 3) == 3 ? (void*)theBox->mEditBox->GetText(0) : sDialogData;
            if (inA == 1)
                theBox->mOk->SetCallback(theCallback, theData);
            else
                static_cast<EEUIFEBCancel*>(theButton)->SetCallback(theCallback, theData);
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
    }
    return 0;
}

// FUNCTION: EXE 0x661baf
// Draws the message box background
long EEUIFError::DrawBackground(const GERectangle& inRectangle)
{
    return UIForm::DrawBackground(inRectangle);
}

// FUNCTION: EXE 0x661bbc
// FUNCTION: EXE 0x661cda
// FUNCTION: EXE 0x661e18
// FUNCTION: EXE 0x661f9c
// FUNCTION: EXE 0x662121
// FUNCTION: EXE 0x6622ed
// Opens the message box, scales its background to the window and lays out the buttons for its mode (the original's
// LayoutCancel, LayoutPassword, LayoutQuestion, LayoutYesNoCancel and LayoutMessage: ee_screens dialog.rs)
void EEUIFError::Open()
{
    UIForm::Open();
    this->mBackground->SetScale((float)gGameWindow->GetWidth() * (1.0f / 1024.0f),
                          (float)gGameWindow->GetHeight() * (1.0f / 768.0f));
    ee_screens_dialog_open(this, this->mMode);
}

// FUNCTION: EXE 0x661c4c
// Closes the message box and posts the pressed button's callback and data (OK first, then No, then Cancel)
void EEUIFError::Close()
{
    EEUIFErrorCallbackData* theData = new EEUIFErrorCallbackData;
    theData->mCallback = NULL;
    theData->mData = NULL;
    if (this->mOk != NULL && this->mOk->mPressed) {
        theData->mCallback = this->mOk->mCallback;
        theData->mData = this->mOk->mData;
    } else if (this->mNo != NULL && this->mNo->mPressed) {
        theData->mCallback = this->mNo->mCallback;
        theData->mData = this->mNo->mData;
    } else if (this->mCancel != NULL && this->mCancel->mPressed) {
        theData->mCallback = this->mCancel->mCallback;
        theData->mData = this->mCancel->mData;
    }
    gUserInterface->PostErrorClosed(this, theData);
    UIForm::Close();
}

// SYNTHETIC: EXE 0x660c27
// SYNTHETIC: EXE 0x660c31
// SYNTHETIC: EXE 0x660c4a
// SYNTHETIC: EXE 0x660c56
// SYNTHETIC: EXE 0x660c57
// SYNTHETIC: EXE 0x660c61
// SYNTHETIC: EXE 0x660c7a
// SYNTHETIC: EXE 0x660c86
// SYNTHETIC: EXE 0x660c87
// SYNTHETIC: EXE 0x660c91
// SYNTHETIC: EXE 0x660caa
// SYNTHETIC: EXE 0x660cb6
// SYNTHETIC: EXE 0x660cb7
// SYNTHETIC: EXE 0x660cc1
// SYNTHETIC: EXE 0x660cde
// SYNTHETIC: EXE 0x660cea
// SYNTHETIC: EXE 0x660ceb
// SYNTHETIC: EXE 0x660cf5
// SYNTHETIC: EXE 0x660d0e
// SYNTHETIC: EXE 0x660d1a
// SYNTHETIC: EXE 0x660d1b
// SYNTHETIC: EXE 0x660d25
// SYNTHETIC: EXE 0x660d42
// SYNTHETIC: EXE 0x660d4e
// SYNTHETIC: EXE 0x660d4f
// SYNTHETIC: EXE 0x660d59
// SYNTHETIC: EXE 0x660d78
// SYNTHETIC: EXE 0x660d84
