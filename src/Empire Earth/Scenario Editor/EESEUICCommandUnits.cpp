// C:\EEX\Empire Earth\Scenario Editor\EESEUICCommandUnits.cpp (file name inferred: no __FILE__). 0x197261..0x197393:
// ctor, ??_G, dtor, MouseMove, MouseRButtonUp, StartCommand. After EESETSUICBPlayer.cpp, before
// EESEUICTriggerSelectUnits.cpp. Unit flags: the exe module set.
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
#include "lle/GE3DPoint.h"
#include "ee/EESEARust.h"
#include "ee/EESEADeleteUnit.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EESEUICCommandUnits.h"
// FUNCTION: EXE 0x597261
// Creates the scenario editor's unit command control with its rotate-units action
EESEUICCommandUnits::EESEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                         EEAMouseClick* inListener)
    : EEUICCommandUnits(inForm, inId, inIndex, inListener)
{
    this->mRotateAction = new EESEARust(kSEARotate);
}

// SYNTHETIC: EXE 0x5972ac
// (the scalar deleting destructor, emitted with the vtable; slot EESEUICCommandUnits+0x0)

// FUNCTION: EXE 0x5972c8
// Destroys the command control, releasing its rotate-units action
EESEUICCommandUnits::~EESEUICCommandUnits()
{
    this->mRotateAction->Dereference();
}

// FUNCTION: EXE 0x5972e6
// Tracks the right-drag and, once it sets a facing, rotates the selected units to it
long EESEUICCommandUnits::MouseMove(const GE2DPoint& inLocation)
{
    EEUICCommandUnits::MouseMove(inLocation);
    if (this->mHasFacing)
        ee_screens_sew_rotate(this->mRotateAction->mAction, this->mFacing);
    return 0;
}

// FUNCTION: EXE 0x597316
// Ends the right-click: without a drag, deletes the unit under the mouse; then hides and frees the mouse
long EESEUICCommandUnits::MouseRButtonUp()
{
    if (!this->mHasFacing)
    {
        EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
        if (theUnit)
        {
            EESEADeleteUnit* theAction = new EESEADeleteUnit(theUnit->mUnitID);
            theAction->Dereference();
            theUnit->Dereference();
        }
    }
    this->SetDisplayMode(kDisplayModeHidden);
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x59737c
// Starts a right-click command and lets any selected unit be rotated by dragging
void EESEUICCommandUnits::StartCommand(const GE2DPoint& inLocation)
{
    EEUICCommandUnits::StartCommand(inLocation);
    this->mCanFace = true;
}
