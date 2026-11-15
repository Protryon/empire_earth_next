// C:\EEX\Empire Earth\Formations\EEFDeepWedge.cpp (file and folder names inferred: no __FILE__). Formation 4: 0xdd861..0xdd8b2.
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
#include "ee/EEFDeepWedge.h"

const float kEEFDeepWedgeSpacing = 1.0f;
const float kEEFDeepWedgeAngle = 0.785398185f;

// FUNCTION: EXE 0x4dd861
// Creates the deep wedge formation: a 45-degree wedge (type 4) of up to 21 units per wedge
EEFDeepWedge::EEFDeepWedge()
    : EEFWedge(4, kEEFDeepWedgeSpacing, kEEFDeepWedgeAngle, 0x15)
{
}

// SYNTHETIC: EXE 0x4dd88b
// (the scalar deleting destructor)

// NEAR: the original jumps to EEFWedge's implicit dtor (ICF 0xdd1be); ours expands it inline
// FUNCTION: EXE 0x4dd8a7
// Destroys the deep wedge formation
EEFDeepWedge::~EEFDeepWedge()
{
}
