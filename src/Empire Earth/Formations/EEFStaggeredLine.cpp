// C:\EEX\Empire Earth\Formations\EEFStaggeredLine.cpp (file and folder names inferred: no __FILE__). Formation 3: 0xddee7..0xddf3a.
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
#include "ee/EEFStaggeredLine.h"

const float kEEFStaggeredLineSpacing = 1.5f;
const float kEEFStaggeredLineDepth = 0.75f;

// FUNCTION: EXE 0x4ddee7
// Creates the staggered line formation (type 3, rows of 5 at 1.5 spacing, odd rows shifted, a gap row after every 5)
EEFStaggeredLine::EEFStaggeredLine()
    : EEFLine(3, kEEFStaggeredLineSpacing, kEEFStaggeredLineDepth, 5, 5)
{
}

// SYNTHETIC: EXE 0x4ddf13
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4ddf2f
// Destroys the staggered line formation
EEFStaggeredLine::~EEFStaggeredLine()
{
}
