// C:\EEX\Empire Earth\Actions\EEAWonNet.cpp (file name inferred: no __FILE__; RTTI class EEAWonNet, vtable
// 0x43914c). The WON lobby's UI-thread action: 0x52f3..0x5d47.
// R6 WON lobby and network round 37: the three constructors and Virtual4 (the room list's and game room's forms
// fed, with its statics) are Rust (ee_won_lobby wonaction.rs); this unit is the action's host, one statement a
// case (ee/EEAWonNetHost.inl).
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

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
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEAction.h"
#include "ee/EEAWonNet.h"
#include "ee/EEStringIDs.h"

#include "ee/EEAWonNetHost.inl"

// Applies a WON lobby update on the UI thread: room and user lists, games box lines and selection, chat text
void EEAWonNet::Virtual4() { ee_won_action_run(this->mRust, WonActionHost); }
