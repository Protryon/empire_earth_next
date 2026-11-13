// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintMoutains.cpp (file name inferred, the RTTI spelling: no __FILE__).
// The "paint mountains" action that clears the units off the painted tiles: 0x1a86ff..0x1a8d3e (Virtual4, SetArea, the
// deque back and std::sort COMDATs of its predicate). After EESEAPaintElevation.cpp, before EESEAPaintTerrain.cpp.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "lle/UIButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EESEAPaint.h"
#include "ee/EESEAPaintMoutains.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a86ff
// Removes every unit standing on each queued area's tiles (whole multi-part units, path points first, resources
// emptied first) and redraws the minimap there (ee_screens editor_actions.rs; the host's units and removal:
// EESEAPaintTerrain.cpp)
void EESEAPaintMoutains::Virtual4()
{
    ee_screens_sea_paint_run(this->mPlan);
}

// FUNCTION: EXE 0x5a88ea
// Queues the area; always returns true
bool EESEAPaintMoutains::SetArea(long inLeft, long inTop, long inRight, long inBottom)
{
    return ee_screens_sea_paint_set_area(this->mPlan, inLeft, inTop, inRight, inBottom) != 0;
}

// (the deque back and the std::sort COMDATs of the removal order: editor_actions.rs's remove_in_order)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESEAPaintMoutains::Virtual8()
{
}
