// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintElevation.cpp (file name inferred: no __FILE__; RTTI class
// EESEAPaintElevation; its vtable/ctor/??_G are emitted by EEUIFSEPaintElevation.cpp). The elevation painting action:
// 0x1a830a..0x1a86ff (Virtual4, SetArea, the deque<EESEAPaintRegion> back/_Growmap COMDATs). After EESEAPaintArea.cpp,
// before EESEAPaintMoutains.cpp.
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
#include "lle/GE3DPlane.h"
#include "lle/UIRadioButton.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EESEAPaint.h"
#include "ee/EEUIFSEPaintElevation.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a830a
// Raises each queued area as the elevation tab's buttons say (a plateau at the level, rough hills at 4x, or flat
// ground), fixes its cliffs and redraws the minimap; tells the effects about the last area only (ee_screens
// editor_actions.rs)
void EESEAPaintElevation::Virtual4()
{
    ee_screens_sea_paint_run(this->mPlan);
}

// FUNCTION: EXE 0x5a8507
// Queues the area unless every point already sits at the level's height; returns whether it queued (ee_screens
// editor_actions.rs)
bool EESEAPaintElevation::SetArea(long inLeft, long inTop, long inRight, long inBottom)
{
    return ee_screens_sea_paint_set_area(this->mPlan, inLeft, inTop, inRight, inBottom) != 0;
}

// FUNCTION: EXE 0x5a85d2
// (deque<EESEAPaintRegion>::back: the queues are editor_actions.rs's)

// FUNCTION: EXE 0x5a85f1
// (deque<EESEAPaintRegion>::_Growmap: likewise)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESEAPaintElevation::Virtual8()
{
}
