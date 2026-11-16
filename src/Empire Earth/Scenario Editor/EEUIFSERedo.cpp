// C:\EEX\Empire Earth\Scenario Editor\EEUIFSERedo.cpp (file name inferred: no __FILE__; RTTI class EEUIFSERedo). The
// editor's Redo button and its action: 0x1a4cbb..0x1a4dbe. After EEUIFSERBBrush (..0x1a4cbb), before EEUIFSEUndo's ctor at 0x1a4dbe.
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
#include "ee/EEUIFSERedo.h"

// Creates the editor's Redo button with its redo action
EEUIFSERedo::EEUIFSERedo(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GE2DPoint& inPosition, unsigned long inWidth,
                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap,
               GERectangle(inPosition, inWidth, inHeight), inForm, inIndex, 0)
{
    this->mRedoAction = new EEARedo;
}

// Destroys the Redo button, releasing its redo action
EEUIFSERedo::~EEUIFSERedo()
{
    this->mRedoAction->Dereference();
}

// Posts the redo action, then releases the button
void EEUIFSERedo::ButtonUp()
{
    gUserInterface->PostAction(this->mRedoAction);
    UIButton::ButtonUp();
}

// Redoes the last undone editor change
void EEARedo::Virtual4()
{
    gUserInterface->Redo();
}

// Does nothing
// (folded into the shared empty slot)
void EEARedo::Virtual8()
{
}
