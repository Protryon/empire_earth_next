// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetStatBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetStatBox). The WON statistics list: 0x25e2f8..0x25fdb3 (ctor, ??_G, dtor, CopyData, DrawTextA, Refresh,
// then the UIListBox<EEUICWNSBStat> instantiation and its vector/line COMDATs). The twin of EEUICWonNetRoomsBox.cpp;
// EEUIEMainGameEvents.cpp follows. Unit flags: the exe module set.
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
#include "ee/EEUICWonNetStatBox.h"
#include "rust/won_lobby/wonfront.h"

// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs), over the screens' host
// (EEUIEWonNetArenaGameEvents.cpp)

// Creates the WON statistics list
// (the base gets mStats before mStats is constructed: the original pushes this+0x138)
EEUICWonNetStatBox::EEUICWonNetStatBox(unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                                       unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                       unsigned long inIndex, unsigned long inBorder)
    : UIListBox<EEUICWNSBStat>(inID, mStats, inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, inBorder)
{
    ee_won_front_call(EeWonFrontCall::StatConstruct, this, (intptr_t)inFont);
}

// Destroys the statistics list and its text
EEUICWonNetStatBox::~EEUICWonNetStatBox()
{
    delete this->mText;
}

// no selection highlight with fewer than two lines
long EEUICWonNetStatBox::CopyData() { return (long)ee_won_front_call(EeWonFrontCall::StatCopyData, this, 0); }
// twelve blue columns per row: name, the counts, three percentages of games and the average game time
long EEUICWonNetStatBox::DrawTextA() { return (long)ee_won_front_call(EeWonFrontCall::StatDrawText, this, 0); }
// one placeholder "Joe Mamma II" line (the developers') for every visible row
void EEUICWonNetStatBox::Refresh() { ee_won_front_call(EeWonFrontCall::StatRefresh, this, 0); }
