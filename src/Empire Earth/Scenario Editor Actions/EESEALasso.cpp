// C:\EEX\Empire Earth\Scenario Editor Actions\EESEALasso.cpp (file name inferred: no __FILE__; RTTI class EESEALasso).
// The editor's lasso action: 0x1a7358..0x1a747d (SelectUnits, Virtual4, Virtual0C; the ctor and ??_G are inline,
// emitted by their first user). After EESEAExitScenario.cpp, before EESEAMouseClick's Virtual4. Unit flags: the exe
// module set.
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

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEALasso.h"
#include "ee/EESEALasso.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a7358
// Selects the units of a type inside a screen rectangle that are not selected yet (unless deselect-only) and
// deselects the others when asked (ee_screens editor_actions.rs)
void EESEALasso::SelectUnits(const GERectangle& inRectangle, long inUnitType, bool inDeselect, bool inDeselectOnly)
{
    ee_screens_seu_lasso_select(&inRectangle, inUnitType, inDeselect ? 1 : 0, inDeselectOnly ? 1 : 0);
}

// FUNCTION: EXE 0x5a73ef
// Applies the queued lasso message: replaces the selection with, adds, or toggles the units in its rectangle;
// refreshes the unit tab and drops the message (ee_screens editor_actions.rs)
void EESEALasso::Virtual4()
{
    if (this->mMessage == NULL)
        return;
    ee_screens_seu_lasso(this->mMessage, &this->mMessage->mRectangle);
    this->mMessage = NULL;
}

// FUNCTION: EXE 0x5a745b
// Deselects the selected units inside the rectangle and posts the action (the second argument is unused)
void EESEALasso::SelectInRectangle(const GERectangle& inRectangle, long inArg2)
{
    this->SelectUnits(inRectangle, 0, true, true);
    gUserInterface->PostAction(this);
}
