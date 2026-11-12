// C:\EEX\Empire Earth\Scenario Editor Actions\EESEASaveScenario.cpp (file name inferred: no __FILE__; RTTI class
// EESEASaveScenario). The editor's "save scenario" action: 0x1aa802..0x1aaa78 (ctor, Virtual4, then the save
// screen event's inline ctor, ??_G and dtor, emitted here: this is their first user). After EESEARotateUnits.cpp
// (..0x1aa802), before EESEATechTree's Virtual4. Unit flags: the exe module set.
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
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "ee/EESEASaveScenario.h"

// FUNCTION: EXE 0x5aa802
// Creates the save-scenario action
EESEASaveScenario::EESEASaveScenario()
{
}

// FUNCTION: EXE 0x5aa816
// Opens the save screen: for scenarios (after preparing the map) in the editor, else for saved games
void EESEASaveScenario::Virtual4()
{
    // Only the save screen that is posted is created (an unused first one leaked on every Save).
    UIEvent* theEvent;
    gUIManager->mScenarioEditorForm->RefreshCurrentGroup();
    if (gUIManager->mInScenarioEditor == true) {
        gWorld.mMap->PrepareSave();
        theEvent = EECreateScreenEvent(EE_SCREENS_SL_SAVE_SCREEN_SCENARIO);   // (ee_screens save_load.rs)
    } else
        theEvent = EECreateScreenEvent(EE_SCREENS_SL_SAVE_SCREEN_GAME);
    gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
}

// ICF-FOLDED: EXE 0x5720c6
// Does nothing
// (folded into the shared empty slot)
void EESEASaveScenario::Virtual8()
{
}

