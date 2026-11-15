// C:\EEX\Empire Earth\Scenario Editor\EEUIFSERedo.cpp (file name inferred: no __FILE__; RTTI class EEUIFSERedo). The
// editor's Redo button and its action: 0x1a4cbb..0x1a4dbe. After EEUIFSERBBrush (..0x1a4cbb), before EEUIFSEUndo's ctor at 0x1a4dbe.
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
#include "ee/EEUIFSERedo.h"

// FUNCTION: EXE 0x5a4cbb
// Creates the editor's Redo button with its redo action
EEUIFSERedo::EEUIFSERedo(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GE2DPoint& inPosition, unsigned long inWidth,
                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap,
               GERectangle(inPosition, inWidth, inHeight), inForm, inIndex, 0)
{
    this->mRedoAction = new EEARedo;
}

// SYNTHETIC: EXE 0x5a4d5a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x5a4d76
// Destroys the Redo button, releasing its redo action
EEUIFSERedo::~EEUIFSERedo()
{
    this->mRedoAction->Dereference();
}

// FUNCTION: EXE 0x5a4d95
// Posts the redo action, then releases the button
void EEUIFSERedo::ButtonUp()
{
    gUserInterface->PostAction(this->mRedoAction);
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x5a4db3
// Redoes the last undone editor change
void EEARedo::Virtual4()
{
    gUserInterface->Redo();
}

// ICF-FOLDED: EXE 0x5720c6
// Does nothing
// (folded into the shared empty slot)
void EEARedo::Virtual8()
{
}
