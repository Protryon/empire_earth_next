// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUndo.cpp (file name inferred: no __FILE__; RTTI class EEUIFSEUndo). The
// editor's Undo button and its action: 0x1a4dbe..0x1a4e99. After EEUIFSERedo.cpp, before 0x1a4e99.
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
#include "ee/EEUserInterface.h"
#include "ee/EEUIFSEUndo.h"

// FUNCTION: EXE 0x5a4dbe
// Creates the editor's Undo button with its undo action
EEUIFSEUndo::EEUIFSEUndo(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GE2DPoint& inPosition, unsigned long inWidth,
                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap,
               GERectangle(inPosition, (float)inWidth, (float)inHeight), inForm, inIndex, 0)
{
    this->mUndoAction = new EEAUndo;
}

// SYNTHETIC: EXE 0x5a4e5d
// (the scalar deleting destructor, with the inline dtor)

// ICF-FOLDED: EXE 0x5a4d95
// Posts the undo action, then releases the button
// (identical to EEUIFSERedo::ButtonUp)
void EEUIFSEUndo::ButtonUp()
{
    gUserInterface->PostAction(this->mUndoAction);
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x5a4e8e
// Undoes the last editor change
void EEAUndo::Virtual4()
{
    gUserInterface->Undo();
}

// ICF-FOLDED: EXE 0x5720c6
// Does nothing
// (folded into the shared empty slot)
void EEAUndo::Virtual8()
{
}
