// C:\EEX\Empire Earth\Actions\EEASaveGame.cpp (file name inferred: no __FILE__; RTTI class EEASaveGame,
// vtable 0x439060). The save-game action: 0x4613..0x470f (EEAResetAllPlayerInfo's ??_G ends at 0x4613, EEAScroll's
// ctor starts at 0x470f).
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
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EESaveLoad.h"
#include "ee/EEAction.h"
#include "ee/EEASaveGame.h"

// FUNCTION: EXE 0x404613
// Creates the save-game action for a file name, flag and description
EEASaveGame::EEASaveGame(const UString& inFileName, bool inFlag, UString inDescription)
    : EEAction(false)
{
    this->mFileName = inFileName;
    this->mFlag = inFlag;
    this->mDescription = inDescription;
}

// FUNCTION: EXE 0x404672
// Saves the game; closes the form on success, else shows the load/save error message
void EEASaveGame::Virtual4()
{
    long theResult = SaveGame(this->mFileName, TMGetTime(), this->mFlag, this->mDescription);
    if (theResult == 0)
        gUserInterface->CloseForm(NULL);
    else
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true);
}

// FUNCTION: EXE 0x4046e5
// Destroys the save-game action
EEASaveGame::~EEASaveGame()
{
}

// SYNTHETIC: EXE 0x4046c9
// (the scalar deleting destructor)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEASaveGame::Virtual8()
{
}
