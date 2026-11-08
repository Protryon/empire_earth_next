// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// OK/Cancel dialog: a title, a message and two buttons, sized to fit the message
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
OkCancelDialog::OkCancelDialog() {}

// FUNCTION: WON 0x7f8bc0
// Looks up the title, message, close, OK and cancel buttons, frame and body from the layout
void OkCancelDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mTitleLabel, "TitleLabel", __FILE__, 26);
    SafeGetComponent(aConfig, this->mCloseButton, "CloseButton", __FILE__, 27);
    SafeGetComponent(aConfig, this->mBodyLabel, "BodyLabel", __FILE__, 28);
    SafeGetComponent(aConfig, this->mOkButton, "OKButton", __FILE__, 30);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 31);
    SafeGetComponent(aConfig, this->mBack, "Back", __FILE__, 33);
    SafeGetComponent(aConfig, this->mDialogComponent, "DialogComponent", __FILE__, 34);
}
// FUNCTION: WON 0x7f8c90
// Sets title, message and button texts and resizes the dialog to the message (up to 250 wide, 400 for long ones)
void OkCancelDialog::SetText(const GUIString& inTitle, const GUIString& inBody,
                            const GUIString& inOkText, const GUIString& inCancelText)
{
    this->mTitleLabel->SetText(inTitle, true);
    this->mTitleLabel->SetDesiredWidth(0);
    this->mOkButton->SetText(inOkText, true);
    this->mOkButton->SetDesiredSize(0, 0);
    this->mOkButton->SetSizeAtLeast(100, 0);
    this->mCancelButton->SetText(inCancelText, true);
    this->mCancelButton->SetDesiredSize(0, 0);
    this->mCancelButton->SetSizeAtLeast(100, 0);
    this->mBodyLabel->SetVertPad(0, 0);
    this->mBodyLabel->SetText(gEmptyGUIString, true);
    this->mBodyLabel->SetSize(0, 0);
    this->SetSize(0, 0);
    this->SetSizeAtLeast(this->mTitleLabel->Width() + this->mCloseButton->Width() +
              this->mBack->GetFrameLeft() + this->mBack->GetFrameRight() + 5, 0);
    this->mDialogComponent->SetSize(0, 0);
    this->mDialogComponent->SetDesiredSize(0, 0);
    this->SetDesiredSize(20, 20);
    int anOldHeight = this->Height();
    int anOldWidth = this->Width();
    int anOldBodyHeight = this->mBodyLabel->Height();
    Font* aFont = this->mBodyLabel->mWrappedText->mFont;
    int aTextWidth = aFont->GetStringWidth(inBody, 0, inBody.length());
    int aBodyWidth;
    if (aTextWidth < 250)
        aBodyWidth = aTextWidth;
    else if (aTextWidth < 800)
        aBodyWidth = 250;
    else
        aBodyWidth = 400;
    int aWidthChange = aBodyWidth - this->mBodyLabel->Width();
    if (aWidthChange < 0)
        aWidthChange = 0;
    this->mBodyLabel->SetSize(aBodyWidth, this->mBodyLabel->Height());
    this->mBodyLabel->SetText(inBody, true);
    int aNumLines = this->mBodyLabel->Height() / this->mBodyLabel->mWrappedText->mFont->GetHeight();
    if (aNumLines < 5) {
        int aPad = (5 - aNumLines) * this->mBodyLabel->mWrappedText->mFont->GetHeight() / 2;
        this->mBodyLabel->SetVertPad(aPad, aPad);
        this->mBodyLabel->SetDesiredSize(0, 0);
    }
    int aHeightChange = this->mBodyLabel->Height() - anOldBodyHeight;
    if (aHeightChange < 0)
        aHeightChange = 0;
    // The original calls the root accessor twice; its first result is discarded.
    int aRootWidth = GetLobbyRoot()->Width();
    int aRootHeight = GetLobbyRoot()->Height();
    if (anOldHeight + aHeightChange > anOldWidth + aWidthChange ||
        anOldHeight + aHeightChange >= aRootHeight) {
        int anExtraWidth = (anOldWidth + aWidthChange) * 5 / 4 - aWidthChange - anOldWidth;
        aWidthChange += anExtraWidth;
        this->mBodyLabel->SetSize(aBodyWidth + anExtraWidth, this->mBodyLabel->Height());
        aHeightChange = this->mBodyLabel->Height() - anOldBodyHeight;
    }
    this->SetSize(anOldWidth + aWidthChange, anOldHeight + aHeightChange);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8a40
// (public: virtual __thiscall WONAPI::OkCancelDialog::~OkCancelDialog(void)    [??1OkCancelDialog@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007fa620

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8a20
// (public: virtual void * __thiscall WONAPI::OkCancelDialog::`scalar deleting destructor'(unsigned int)    [??_GOkCancelDialog@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f89e0
// (public: __thiscall WONAPI::OkCancelDialog::OkCancelDialog(void)    [??0OkCancelDialog@WONAPI@@QAE@XZ])
