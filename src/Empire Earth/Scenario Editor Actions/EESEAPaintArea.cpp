// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintArea.cpp (file name inferred: no __FILE__; RTTI class
// EESEAPaintArea). The editor's area painting action: 0x1a8284..0x1a830a (Virtual4, SetCorner; the ctor is emitted by
// its first user). After EESEAMoveUnit.cpp's template COMDATs (..0x1a8284), before EESEAPaintElevation's Virtual4.
// Unit flags: the exe module set.
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
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEAPaintArea.h"
#include "rust/ee_screens.h"

// FUNCTION: EXE 0x5a8284
// Highlights the dragged rectangle as the editor's paint area
void EESEAPaintArea::Virtual4()
{
    gUIManager->mScenarioEditorForm->SetPaintArea(this->mR0C);
}

// FUNCTION: EXE 0x5a8299
// Moves the dragged corner; if it changed, rebuilds the rectangle between the start corner and it and posts
// the action
void EESEAPaintArea::SetCorner(long inX, long inY)
{
    if (this->mEndX == inX && this->mEndY == inY)
        return;
    this->mEndY = inY;
    this->mEndX = inX;
    int32_t theRectangle[4];   // left, top, right, bottom (ee_screens editor_actions.rs)
    ee_screens_sea_area_rectangle(this->mStartX, this->mStartY, this->mEndX, this->mEndY, theRectangle);
    this->mR0C.SetLeft((long)theRectangle[0]);
    this->mR0C.SetRight((long)theRectangle[2]);
    this->mR0C.SetTop((long)theRectangle[1]);
    this->mR0C.SetBottom((long)theRectangle[3]);
    gUserInterface->PostAction(this);
}

// ICF-FOLDED: EXE 0x5720c6
// Does nothing
// (folded into the shared empty slot)
void EESEAPaintArea::Virtual8()
{
}
