// C:\EEX\Empire Earth\Actions\EEABootClientFromCanceledGame.cpp (file name inferred: no __FILE__; RTTI class
// EEABootClientFromCanceledGame, vtable 0x43d0b4). The client's "game cancelled" action: 0x16c0..0x173b (the first
// unit, EEAAddChatMessage.cpp, ends at 0x16c0; EEABuild's dtor starts at 0x173b). Virtual4 only.
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
#include "lle/UIBlinkingButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EEAction.h"
#include "ee/EEABootClientFromCanceledGame.h"

// FUNCTION: EXE 0x4016c0
// Tells a client its game was cancelled: rejoins the WON room, leaves the form and shows the reason string
void EEABootClientFromCanceledGame::Virtual4()
{
    UWideString theMessage;
    if (gWonNet) {
        gWonNet->ClearGameList();
        gWonNet->RejoinRoom(false);
    }
    if (!this->mNoPop)
        gUIManager->Pop();
    if (this->mStringID) {
        gResource->GetString(this->mStringID, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
    }
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEABootClientFromCanceledGame::Virtual8()
{
}
