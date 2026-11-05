// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGGStatistics.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFMGGStatistics). The in-game form's statistics group: 0x267ca8..0x267ec8 (ctor, ??_G, dtor, Virtual04,
// Virtual0C, then EETechTree::GetShownEpoch's COMDAT: this unit is its first user). Unit flags: the exe module set.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIFMGGStatistics.h"
#include "ee/EEStringIDs.h"
#include "ee/EEUIManager.h"
// FUNCTION: EXE 0x667ca8
// Creates the main game form's statistics panel group
EEUIFMGGStatistics::EEUIFMGGStatistics(EEUIFMainGame* inOwner)
    : EEUIFMGGroup(inOwner)
{
}

// SYNTHETIC: EXE 0x667cc0
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFMGGStatistics+0x0)

// FUNCTION: EXE 0x667cdc
// Destroys the statistics panel group
EEUIFMGGStatistics::~EEUIFMGGStatistics()
{
}

// FUNCTION: EXE 0x667ce7
// Shows the local player's conversions against the conversion limit, updating the label only when it changed
void EEUIFMGGStatistics::Refresh()
{
    UWideString theText;
    UWideString theLabel;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (!thePlayer)
        return;
    gResource->GetString(IDS_GAME_CONVERSIONS, theLabel);
    sprintf(theText, "%-18.18s  %4.4ld/%-4.4ld", (const char*)theLabel,
            thePlayer->mSavedGroups.GetLastValue(5), gWorldTable.GetLongValue(kWorldConversionsStatisticMax));
    if (static_cast<UITextLabel*>(this->mOwner->GetControl(kMainGameConversions))->GetText(0) != theText)
        (static_cast<UITextLabel*>(this->mOwner->GetControl(kMainGameConversions)))->SetText(theText);
}

void EEUIFMGGStatistics::Virtual08()
{
}

// FUNCTION: EXE 0x667db1
// Fills the statistics panel: wonders needed to win, the epoch wonders become available in, conversions
void EEUIFMGGStatistics::Select()
{
    UWideString theText;
    UWideString theLabel;
    UWideString theEpoch;
    gResource->GetString(IDS_WONDERS_TO_WIN, theLabel);
    sprintf(theText, "%-18.18s  %4.4ld", (const char*)theLabel, gWorld.mNumberOfWondersToWin);
    static_cast<UITextLabel*>(this->mOwner->GetControl(kMainGameWondersToWin))->SetText(theText);
    gResource->GetString(IDS_WONDERS_AVAILABLE_IN, theLabel);
    gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->GetTechTree()->GetShownEpoch()), theEpoch);
    sprintf(theText, "%-18.18s %-18.18s", (const char*)theLabel, (const char*)theEpoch);
    static_cast<UITextLabel*>(this->mOwner->GetControl(kMainGameWondersAvailable))->SetText(theText);
    this->Refresh();
}

bool EEUIFMGGStatistics::WantsRefresh()
{
    return false;
}

// FUNCTION: EXE 0x667eaa
// Returns the epoch the tech tree shows to the player
// (EETechTree::GetShownEpoch, inline in ee/EEPlayer.h; this unit is its first user)
