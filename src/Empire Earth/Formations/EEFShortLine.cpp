// C:\EEX\Empire Earth\Formations\EEFShortLine.cpp (file and folder names inferred: no __FILE__). Formation 0: 0xdde94..0xddee7.
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
#include "ee/EEFLine.h"
#include "ee/EEFShortLine.h"

const float kEEFShortLineSpacing = 1.0f;
const float kEEFShortLineDepth = 0.0f;

// FUNCTION: EXE 0x4dde94
// Creates the short line formation (type 0, rows of 5 at unit spacing, a gap row after every 5)
EEFShortLine::EEFShortLine()
    : EEFLine(0, kEEFShortLineSpacing, kEEFShortLineDepth, 5, 5)
{
}

// SYNTHETIC: EXE 0x4ddec0
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4ddedc
// Destroys the short line formation
EEFShortLine::~EEFShortLine()
{
}
