// C:\EEX\Empire Earth\Actions\EEAUserPauseGame.cpp (file name inferred: no __FILE__; RTTI class
// EEAUserPauseGame, vtable 0x446790). The pause action of a single-player game: 0x5280..0x52f3 (EEAUpgradeImprovement's
// last function ends at 0x5280, EEAWonNet's ??_G starts at 0x52f3). Ctor inline, ??_G the shared EEAction one,
// Virtual8 the shared empty slot.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECommunications.h"
#include "ee/EEServer.h"
#include "ee/EEAction.h"
#include "ee/EEAUserPauseGame.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"

// FUNCTION: EXE 0x405280
// Toggles the server's user pause and tells the UI, once physics has processed a command turn
void EEAUserPauseGame::Virtual4()
{
    if (!(gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) || !gCommunications->IsServer())
        return;
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    if (gWorld.mUserPaused == true) {
        theServer->UnpauseGame();
        gUserInterface->PostPauseInformation(false);
        gWorld.mUserPaused = false;
    } else {
        theServer->PauseGame();
        gUserInterface->PostPauseInformation(true);
        gWorld.mUserPaused = true;
    }
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAUserPauseGame::Virtual8()
{
}
