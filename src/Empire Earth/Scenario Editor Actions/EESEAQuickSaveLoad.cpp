// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAQuickSaveLoad.cpp (file name inferred: no __FILE__; RTTI class
// EESEAQuickSaveLoad). The editor's quick save/load action: 0x1aa70e..0x1aa723 (Virtual4 only: the ctor and ??_G
// are inline, emitted in EEUIFScenarioEditor.cpp). After EESEAPlaceWall.cpp's template COMDATs (..0x1aa70e), before
// EESEAQuickSaveScenario's Virtual4. Unit flags: the exe module set.
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
#include "lle/FSMemoryFile.h"

#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EESEAQuickSaveLoad.h"

// FUNCTION: EXE 0x5aa70e
// Runs the quick save or load, then resumes the editor's current tab
void EESEAQuickSaveLoad::Virtual4()
{
    EEAQuickSaveLoad::Virtual4();
    gUIManager->mScenarioEditorForm->ResumeCurrentGroup();
}
