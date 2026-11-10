// C:\EEX\Empire Earth\Scenario Editor Actions\EESEARotateUnits.cpp (file name inferred: no __FILE__; RTTI class
// EESEARotateUnits). The editor's "rotate the selected units" action: 0x1aa74f..0x1aa802 (Virtual4, Rotate). After
// EESEAQuickSaveScenario.cpp, before EESEASaveScenario's ctor. Unit flags: the exe module set.
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

#include "ee/EmpireEarth.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEARotateUnits.h"

// FUNCTION: EXE 0x5aa74f
// Turns every selected unit that can turn to face the action's yaw
void EESEARotateUnits::Virtual4()
{
    GETransformation theTransformation;
    EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
    unsigned long theCount = theEditor->GetSelectedUnitCount();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theUnit = theEditor->mSelectedUnits[i];
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit == NULL || !(theUnit->mUnitClass->mAngularAcceleration > 0.0f))
            continue;
        theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(this->mYaw, 0.0f, 0.0f);
        theComplexUnit->SetTransformation(theTransformation, true);
    }
}

// FUNCTION: EXE 0x5aa7ec
// Sets the yaw to turn the selected units to and posts the action
void EESEARotateUnits::Rotate(float inYaw)
{
    this->mYaw = inYaw;
    gUserInterface->PostAction(this);
}

// ICF-FOLDED: EXE 0x5720c6
// Does nothing
// (folded into the shared empty slot)
void EESEARotateUnits::Virtual8()
{
}
