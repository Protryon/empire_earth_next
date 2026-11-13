// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAMouseClick.cpp (file name inferred: no __FILE__; RTTI class
// EESEAMouseClick). The editor's mouse click action: 0x1a747d..0x1a75e0 (Virtual4, Virtual0C; the ctor and ??_G are
// inline, emitted by their first user). After EESEALasso.cpp, before EESEAMoveUnit's ctor. Unit flags: the exe
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
#include "ee/EEMouse.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEAMouseClick.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a747d
// Applies the queued click message: selects only the unit, toggles it, clears the selection or adds the unit;
// refreshes the unit tab and drops the message (ee_screens editor_actions.rs)
void EESEAMouseClick::Virtual4()
{
    if (this->mMessage == NULL)
        return;
    ee_screens_seu_click(this->mMessage);
    this->mMessage = NULL;
}

// FUNCTION: EXE 0x5a7556
// Handles a click (not while rotating): a selected unit under the mouse is toggled off, empty ground clears the
// selection, an unselected unit does nothing
void EESEAMouseClick::Execute(float inX, float inY, bool inMove, unsigned long inModifiers, bool inIsRotating,
                                float inFacing, bool inForceMove)
{
    EEMessage* theMessage = NULL;
    if (inIsRotating)
        return;
    EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
    if (theUnit != NULL) {
        if (theUnit->mComplexUnit != NULL && theUnit->mIsEditorSelected)
            theMessage = new EEMAddUnitToSelectedGroup(theUnit->mUnitID);
        theUnit->Dereference();
    } else
        theMessage = new EEMUnselectAllUnits;
    if (theMessage != NULL) {
        this->mMessage = theMessage;
        gUserInterface->PostAction(this);
    }
}
