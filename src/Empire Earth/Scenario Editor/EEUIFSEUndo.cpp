// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUndo.cpp (file name inferred: no __FILE__; RTTI class EEUIFSEUndo). The
// editor's Undo button and its action: 0x1a4dbe..0x1a4e99. After EEUIFSERedo.cpp, before 0x1a4e99.
// Unit flags: the exe module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERhombus.h"
#include "lle/UIButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFSEUndo.h"

// Creates the editor's Undo button with its undo action
EEUIFSEUndo::EEUIFSEUndo(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GE2DPoint& inPosition, unsigned long inWidth,
                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap,
               GERectangle(inPosition, (float)inWidth, (float)inHeight), inForm, inIndex, 0)
{
    this->mUndoAction = new EEAUndo;
}

// Posts the undo action, then releases the button
// (identical to EEUIFSERedo::ButtonUp)
void EEUIFSEUndo::ButtonUp()
{
    gUserInterface->PostAction(this->mUndoAction);
    UIButton::ButtonUp();
}

// Undoes the last editor change
void EEAUndo::Virtual4()
{
    gUserInterface->Undo();
}

// Does nothing
// (folded into the shared empty slot)
void EEAUndo::Virtual8()
{
}
