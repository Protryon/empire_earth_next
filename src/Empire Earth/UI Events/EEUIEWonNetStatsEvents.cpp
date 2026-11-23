// C:\EEX\Empire Earth\UI Events\EEUIEWonNetStatsEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNS). The WON statistics form's open event: 0x1ebc13..0x1ebc47. Unit flags: the exe module set.
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
#include "lle/UIListBox.h"

#include "ee/EEWonNet.h"
#include "ee/EEUICWonNetStatBox.h"
#include "ee/EEUIEWonNetStatsEvents.h"
#include "ee/EEUIManager.h"
#include "rust/won_lobby/wonfront.h"

// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs)
long EEUIEWNSOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::StatsOpen, inForm, (intptr_t)inEntity); }
long EEUIEWNSCancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::StatsCancel, inForm, (intptr_t)inEntity); }
