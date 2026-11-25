// C:\EEX\Empire Earth\User Interface Thread\EEWonNetAvailableGameFormManager.cpp (file name inferred: no __FILE__;
// RTTI class EEWonNetAvailableGameFormManager). 0x27a37e..0x27aa0b: the WON game room form's manager (ctor, ??_G,
// dtor, Get), then the game's free UI helpers in address order (EETranslateString, EEDisplayScenarioIntro, message
// 0x41b, the resource reference parser; the flare effect, the selection and tile lookups and EEExitGame, which the
// rest of the game calls, are Empire Earth/EEGameUtilities.cpp's). The helpers' names are provisional. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
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
#include "lle/GETerrainMesh.h"
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
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEServer.h"
#include "ee/EEClient.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEMap.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "lle/ISModifierState.h"
#include "rust/won_lobby/wonfront.h"

// Creates the WON available-games form manager with a 3-second refresh poll
EEWonNetAvailableGameFormManager::EEWonNetAvailableGameFormManager()
{
    this->mPoll = new TMPoll(3000, 0, false);
}

// Destroys the form manager and its poll
EEWonNetAvailableGameFormManager::~EEWonNetAvailableGameFormManager()
{
    delete this->mPoll;
}

// Returns the available-games form's manager, creating and attaching it on first use (Rust: wonfront.rs)
EEWonNetAvailableGameFormManager* EEWonNetAvailableGameFormManager::Get() { return (EEWonNetAvailableGameFormManager*)ee_won_front_call(EeWonFrontCall::FormManagerGet, NULL, 0); }

// the scenario intro's media from the running scenario (ui_helpers.rs display_scenario_intro)
extern "C" void ee_screenscpp_uh_intro_media_copy() { EEUISCIFormManager::Get()->mMediaStrings = ::gWorld.mScenarioManager->mMediaStrings; }

// (EETranslateString, EEDisplayScenarioIntro, EEPostF9Key and the resource reference parser: Rust, ee_game screens
// ui_helpers.rs)

// (EEExitGame: Empire Earth/EEGameUtilities.cpp)
