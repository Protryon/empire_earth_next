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
#include "rust/won_lobby/wonfront.h"
// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs)
long EEUIEWNMUASubmit::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::MuaSubmit, inForm, (intptr_t)inEntity); }
long EEUIEWNMUAOpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::MuaOpenScreen, inForm, (intptr_t)inEntity); }

// Returns to the previous screen
// (also EEUIEWNARBack's and EEUIEWNSSCancel's ProcessEvent: ICF keeps this copy)
long EEUIEWNMUACancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return inForm->GetManager()->Pop();
}
