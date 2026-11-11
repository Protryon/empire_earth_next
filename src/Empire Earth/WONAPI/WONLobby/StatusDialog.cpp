// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Status dialog: a title, a status message and one Cancel or OK button, sized to fit the message
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
#include "rust/ee_rust.h"
// The dialog's decisions are ee_won's dialogs module (rust/crates/ee_won/src/dialogs.rs); this unit keeps the
// dialog, its controls and events.
namespace WONAPI {
// FUNCTION: WON 0x7f91b0
// Creates the status dialog, remembering when it was created
StatusDialog::StatusDialog() { this->mStartTick = WONTickCount(); }

// FUNCTION: WON 0x7f9370
// Looks up the title, status label, close and cancel buttons, frame and body from the layout
void StatusDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mTitleLabel, "TitleLabel", __FILE__, 28);
    SafeGetComponent(aConfig, this->mCloseButton, "CloseButton", __FILE__, 29);
    SafeGetComponent(aConfig, this->mStatusLabel, "StatusLabel", __FILE__, 30);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 31);
    SafeGetComponent(aConfig, this->mBack, "Back", __FILE__, 33);
    SafeGetComponent(aConfig, this->mDialogComponent, "DialogComponent", __FILE__, 34);
}
// FUNCTION: WON 0x7f9430
// Shows a new status message under the current title, with a Cancel or an OK button
void StatusDialog::SetStatus(const GUIString& inStatus, bool inCancel)
{
    this->SetText(this->mTitleLabel->mWrappedText->mText, inStatus, inCancel);
}
// FUNCTION: WON 0x7f9450
// Sets title, message and Cancel/OK button and resizes the dialog to fit; within 50 ms of creation it never shrinks
void StatusDialog::SetText(const GUIString& inTitle, const GUIString& inStatus, bool inCancel)
{
    bool anOldB8C = this->mDragging;
    int anInitialWidth = this->Width();
    int anInitialHeight = this->Height();
    int anInitialStatusWidth = this->mStatusLabel->Width();
    int anInitialStatusHeight = this->mStatusLabel->Height();
    int anInitialComponentWidth = this->mDialogComponent->Width();
    int anInitialComponentHeight = this->mDialogComponent->Height();
    int anInitialButtonY = this->mCancelButton->Top();
    this->mTitleLabel->SetText(inTitle, true);
    this->mTitleLabel->SetDesiredWidth(0);
    this->mCancelButton->SetText(inCancel ? ::LobbyGlobal_Cancel_String : ::LobbyGlobal_OK_String, true);
    this->mCancelButton->SetDesiredSize(0, 0);
    this->mCancelButton->SetSizeAtLeast(100, 0);
    this->mStatusLabel->SetVertPad(0, 0);
    this->mStatusLabel->SetText(gEmptyGUIString, true);
    this->mStatusLabel->SetSize(0, 0);
    this->SetSize(0, 0);
    this->SetSizeAtLeast(this->mTitleLabel->Width() + this->mCloseButton->Width() +
              this->mBack->GetFrameRight() + this->mBack->GetFrameLeft() + 5, 0);
    this->mDialogComponent->SetSize(0, 0);
    this->mDialogComponent->SetDesiredSize(0, 0);
    this->SetDesiredSize(20, 20);
    int anOldWidth = this->Width();
    int anOldHeight = this->Height();
    int anOldStatusHeight = this->mStatusLabel->Height();
    int aRootWidth = GetLobbyRoot()->Width();
    int aRootHeight = GetLobbyRoot()->Height();
    int aBodyWidth = ee_won_dlg_status_body_width(this->mStatusLabel->mWrappedText->mFont->GetStringWidth(inStatus),
                                                  this->mStatusLabel->mWrappedText->mFont->GetCharacterWidth('M'),
                                                  aRootWidth);
    int aWidthChange = ee_won_dlg_growth(aBodyWidth, this->mStatusLabel->Width());
    this->mStatusLabel->SetSize(aBodyWidth, this->mStatusLabel->Height());
    this->mStatusLabel->SetText(inStatus, true);
    int aPad = ee_won_dlg_status_pad(this->mStatusLabel->Height(), this->mStatusLabel->mWrappedText->mFont->GetHeight());
    if (aPad >= 0) {
        this->mStatusLabel->SetVertPad(aPad, aPad);
        this->mStatusLabel->SetDesiredSize(0, 0);
    }
    int aHeightChange = ee_won_dlg_growth(this->mStatusLabel->Height(), anOldStatusHeight);
    int aNewWidth = anOldWidth + aWidthChange;
    int aNewHeight = aHeightChange + anOldHeight;
    int anExtraWidth;
    if (ee_won_dlg_status_extra_width(aNewWidth, aNewHeight, aRootHeight, aWidthChange, anOldWidth, &anExtraWidth)) {
        aWidthChange += anExtraWidth;
        this->mStatusLabel->SetSize(aBodyWidth + anExtraWidth, this->mStatusLabel->Height());
        aHeightChange = this->mStatusLabel->Height() - anOldStatusHeight;
    }
    this->SetSize(aWidthChange + anOldWidth, aHeightChange + anOldHeight);
    if (ee_won_dlg_status_young(WONTickCount(), (uint32_t)this->mStartTick)) {
        this->mStatusLabel->SetSizeAtLeast(anInitialStatusWidth, anInitialStatusHeight);
        this->mDialogComponent->SetSizeAtLeast(anInitialComponentWidth, anInitialComponentHeight);
        this->SetSizeAtLeast(anInitialWidth, anInitialHeight);
        if (anInitialButtonY > this->mCancelButton->Top())
            this->mCancelButton->SetPos(this->mCancelButton->Left(), anInitialButtonY);
    }
    this->Invalidate();
    this->mDragging = anOldB8C;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f9200
// (public: virtual void * __thiscall WONAPI::StatusDialog::`scalar deleting destructor'(unsigned int)    [??_GStatusDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f9220
// (public: virtual __thiscall WONAPI::StatusDialog::~StatusDialog(void)    [??1StatusDialog@WONAPI@@UAE@XZ])
