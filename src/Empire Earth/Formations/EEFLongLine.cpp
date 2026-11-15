// C:\EEX\Empire Earth\Formations\EEFLongLine.cpp (file and folder names inferred: no __FILE__). Formation 1: 0xddc20..0xddc73.
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
#include "ee/EEFLongLine.h"

const float kEEFLongLineSpacing = 1.0f;
const float kEEFLongLineDepth = 0.0f;

// FUNCTION: EXE 0x4ddc20
// Creates the long line formation (type 1, rows of 10 at unit spacing, a gap row after every 5)
EEFLongLine::EEFLongLine()
    : EEFLine(1, kEEFLongLineSpacing, kEEFLongLineDepth, 10, 5)
{
}

// SYNTHETIC: EXE 0x4ddc4c
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4ddc68
// Destroys the long line formation
EEFLongLine::~EEFLongLine()
{
}
