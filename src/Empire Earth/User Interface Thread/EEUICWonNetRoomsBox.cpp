// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetRoomsBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetRoomsBox). The WON room list: 0x25cead..0x25e2f8 (ctor, ??_G, dtor, CopyData, DrawTextA, then the
// UIListBox<EEUICWNRBLine> instantiation and its vector/line COMDATs). Unit flags: the exe module set.
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
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "rust/won_lobby/wonfront.h"

// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs), over the screens' host
// (EEUIEWonNetArenaGameEvents.cpp)

// Creates the WON room list
EEUICWonNetRoomsBox::EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation,
                                         unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                         unsigned long inIndex, unsigned long inBorderThickness)
    : UIListBox<EEUICWNRBLine>(inId, std::vector<EEUICWNRBLine>(), inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                               inBorderThickness)
{
    ee_won_front_call(EeWonFrontCall::RoomsConstruct, this, (intptr_t)inFont);
}

EEUICWonNetRoomsBox::~EEUICWonNetRoomsBox() { ee_won_front_call(EeWonFrontCall::RoomsDestruct, this, 0); }
long EEUICWonNetRoomsBox::CopyData() { return (long)ee_won_front_call(EeWonFrontCall::RoomsCopyData, this, 0); }
// four columns per visible line: name, description, number of games and number of players
long EEUICWonNetRoomsBox::DrawTextA() { return (long)ee_won_front_call(EeWonFrontCall::RoomsDrawText, this, 0); }
