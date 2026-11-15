// C:\EEX\Empire Earth\Formations\EEFShallowWedge.cpp (file and folder names inferred: no __FILE__). Formation 5: 0xdde43..0xdde94.
// Unit flags: the exe's module set.
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
#include "lle/NESynchronousCommunications.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"

#include "ee/EEFormation.h"
#include "ee/EEFWedge.h"
#include "ee/EEFShallowWedge.h"

const float kEEFShallowWedgeSpacing = 1.0f;
const float kEEFShallowWedgeAngle = 1.39626348f;

// FUNCTION: EXE 0x4dde43
// Creates the shallow wedge formation: an 80-degree wedge (type 5) of up to 21 units per wedge
EEFShallowWedge::EEFShallowWedge()
    : EEFWedge(5, kEEFShallowWedgeSpacing, kEEFShallowWedgeAngle, 0x15)
{
}

// SYNTHETIC: EXE 0x4dde6d
// (the scalar deleting destructor)

// NEAR: the original jumps to EEFWedge's implicit dtor (ICF 0xdd1be); ours expands it inline
// FUNCTION: EXE 0x4dde89
// Destroys the shallow wedge formation
EEFShallowWedge::~EEFShallowWedge()
{
}
