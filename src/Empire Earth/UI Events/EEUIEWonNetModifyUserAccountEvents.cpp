// C:\EEX\Empire Earth\UI Events\EEUIEWonNetModifyUserAccountEvents.cpp (file name inferred: no __FILE__; class
// prefix EEUIEWNMUA). The WON update-account form's (0x25) events: 0x1eb5e2..0x1eb7fb. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

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
#include "lle/FSPath.h"
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUIEWonNetModifyUserAccountEvents.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x5eb5e2
// Sends the new password, e-mail address and e-mail opt-in to WON if both passwords match; else shows a message
long EEUIEWNMUASubmit::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString thePassword;
    UWideString theConfirmation;
    UWideString theNewPassword;
    UWideString theMessage;
    thePassword = static_cast<UIEditBox*>(inForm->GetControl(kUpdateAccountNewPasswordEditBox))->GetText(0);
    theConfirmation = static_cast<UIEditBox*>(inForm->GetControl(kUpdateAccountConfirmPasswordEditBox))->GetText(0);
    theNewPassword = static_cast<UIEditBox*>(inForm->GetControl(kUpdateAccountEnterEmailEditBox))->GetText(0);
    bool theFlag = static_cast<UICheckBox*>(inForm->GetControl(kUpdateAccountSendMeEmailCheckbox))->GetIsChecked();
    if (thePassword == theConfirmation)
        gWonNet->UpdateAccount(thePassword, theNewPassword, theFlag);
    else {
        gResource->GetString(IDS_YOUR_TWO_PASSWORDS_DONT_MATCH, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb704
// Hooks the update-account screen's Update and Don't Update buttons to their events
long EEUIEWNMUAOpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEUIEWNSetTriggerEvent(new EEUIEWNMUACancel, inForm->GetControl(kUpdateAccountDontUpdateButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNMUASubmit, inForm->GetControl(kUpdateAccountUpdateButton), kControlTriggerButtonUp);
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb792
// Destroys the update-account cancel event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIEWNMUACancel, then EEUIEWNMUASubmit's)

// FUNCTION: EXE 0x5eb7bd
// Destroys the update-account submit event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eb7e8
// Returns to the previous screen
// (also EEUIEWNARBack's and EEUIEWNSSCancel's ProcessEvent: ICF keeps this copy)
long EEUIEWNMUACancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return inForm->GetManager()->Pop();
}
