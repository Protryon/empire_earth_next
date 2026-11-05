// C:\EEX\Empire Earth\Scenario Editor\EESEUICTriggerSelectUnits.cpp (file name inferred: no __FILE__).
// 0x197393..0x19764b: ctor, ??_G, dtor, MouseLButtonDoublePressed, MouseLButtonUp, MouseRButtonDown, MouseRButtonUp.
// After EESEUICCommandUnits.cpp, before EESEUITechListBox.cpp. Unit flags: the exe module set.
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
#include "ee/EEMessage.h"
#include "ee/EERenderObject.h"
#include "ee/EEROEffects.h"
#include "lle/GE3DPoint.h"
#include "ee/EEGraphics.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"

// FUNCTION: EXE 0x597393
// Creates the trigger screen's unit selection control, with the right button up
EESEUICTriggerSelectUnits::EESEUICTriggerSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                                     EEAMouseClick* inClick, EEALasso* inLasso)
    : EEUICSelectUnits(inForm, inId, inIndex, inClick, inLasso)
{
    this->mRightButton = false;
}

// SYNTHETIC: EXE 0x5973c1
// (the scalar deleting destructor, emitted with the vtable; slot EESEUICTriggerSelectUnits+0x0)

// FUNCTION: EXE 0x5973dd
// Destroys the unit selection control
EESEUICTriggerSelectUnits::~EESEUICTriggerSelectUnits()
{
}

// FUNCTION: EXE 0x5973e8
// Selects every on-screen unit of the double-clicked unit's type, then ends the click and frees the mouse
long EESEUICTriggerSelectUnits::MouseLButtonDoublePressed()
{
    EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
    if (theUnit)
    {
        if (theUnit->mComplexUnit)
            this->mLasso->SelectUnits(gU4E8F08.GetViewRectangle(), 1, theUnit->mUnitClass->mUnitType, theUnit->mUnitID);
        theUnit->Dereference();
    }
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    this->SetDisplayMode(kDisplayModeHidden);
    this->ReleaseCapturedMouse();
    this->mDragging = false;
    return 0;
}

// FUNCTION: EXE 0x597491
// Ends a click or drag: selects units in the dragged box (right button: picks by rectangle) or at the
// mouse (right button: acts on the spot), then hides the control and frees the mouse
long EESEUICTriggerSelectUnits::MouseLButtonUp()
{
    GE3DPoint thePoint;
    EETile* theLocation;
    GERectangle theRectangle;
    float theTemp;

    EEPointUnitOrTile(this->GetMouseLocation(), theLocation, thePoint, false);
    if (this->mDragging)
    {
        this->mD1 = false;
        theRectangle.SetTopLeft(this->mDragStart);
        theRectangle.SetBottomRight(this->mDragEnd);
        if (theRectangle.GetLeft() > theRectangle.GetRight())
        {
            theTemp = theRectangle.GetLeft();
            theRectangle.SetLeft(theRectangle.GetRight());
            theRectangle.SetRight(theTemp);
        }
        if (theRectangle.GetTop() > theRectangle.GetBottom())
        {
            theTemp = theRectangle.GetTop();
            theRectangle.SetTop(theRectangle.GetBottom());
            theRectangle.SetBottom(theTemp);
        }
        if (this->mRightButton)
            this->mLasso->SelectInRectangle(theRectangle, 2);
        else
            this->mLasso->SelectUnits(theRectangle, 1, kAnyUnitType, 0);
    }
    else if (gViewport.GetViewRectangle().Contains(this->GetMouseLocation()))
    {
        if (this->mRightButton)
            this->mClick->Execute(thePoint.GetX(), thePoint.GetY(), true, 1, false, 0.0f, false);
        else
            this->mClick->SelectAtMouse(1);
    }
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    this->SetDisplayMode(kDisplayModeHidden);
    this->ReleaseCapturedMouse();
    this->mDragging = false;
    this->mRightButton = false;
    return 0;
}

// FUNCTION: EXE 0x597624
// Starts a right-button click or drag, handled like a left press
long EESEUICTriggerSelectUnits::MouseRButtonDown()
{
    this->mRightButton = true;
    return this->MouseLButtonDown();
}

// FUNCTION: EXE 0x597630
// Ends a right-button click or drag like a left release; does nothing without a right press
long EESEUICTriggerSelectUnits::MouseRButtonUp()
{
    long theResult = 0;
    if (this->mRightButton)
    {
        theResult = this->MouseLButtonUp();
        this->mRightButton = false;
    }
    return theResult;
}
