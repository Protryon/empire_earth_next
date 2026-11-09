// C:\EEX\Empire Earth\Actions\EEAResetAllPlayerInfo.cpp (file name inferred: no __FILE__; RTTI class
// EEAResetAllPlayerInfo, vtable 0x439050). The scenario editor's reset-all-player-info action: 0x4444..0x4613
// (EEARemovePlayer's Virtual4 ends at 0x4444, EEASaveGame's ctor starts at 0x4613). dtor, Virtual4, ??_G.
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
#include "lle/TSThread.h"
#include "lle/TSEvent.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEAResetAllPlayerInfo.h"

// FUNCTION: EXE 0x404444
// Destroys the reset-all-player-info action
EEAResetAllPlayerInfo::~EEAResetAllPlayerInfo()
{
}

// FUNCTION: EXE 0x404464
// Reloads each listed player's civilization in the scenario editor and posts their epoch and refresh requests
void EEAResetAllPlayerInfo::Virtual4()
{
    unsigned long i;
    for (i = 0; i < this->mPlayers.size(); i++) {
        EEPlayer* thePlayer = ::gWorld.mPlayers[this->mPlayers[i]];
        EEApplyCivilizationFile(::gWorld.mPlayers[this->mPlayers[i]]->mCivilizationFile, thePlayer);
        long theCivilization = ::gWorld.mPlayers[this->mPlayers[i]]->GetTechTree()->mStartEpoch;
        gUIManager->mScenarioEditorForm->mLoadThread->Post406(reinterpret_cast<void*>(this->mPlayers[i]), reinterpret_cast<void*>(theCivilization));
    }
    for (i = 0; i < this->mPlayers.size(); i++)
        gUIManager->GetScenarioEditorForm()->mLoadThread->Post403(reinterpret_cast<void*>(this->mPlayers[i]), false);
    for (i = 0; i < this->mPlayers.size(); i++)
        gUIManager->GetScenarioEditorForm()->mLoadThread->Post407(
            reinterpret_cast<void*>(this->mPlayers[i]), reinterpret_cast<void*>(::gWorld.mPlayers[this->mPlayers[i]]->GetTechTree()->mStartEpoch));
}

// SYNTHETIC: EXE 0x4045f7
// (the scalar deleting destructor)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAResetAllPlayerInfo::Virtual8()
{
}
