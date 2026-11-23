// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetGamesBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetGamesBox). The WON game room's list of games: 0x25b2ce..0x25cead (ctor, ??_G, dtor, CopyData,
// DrawPolygons, DrawTextA, then the UIListBox<EEUICWNGBLine> instantiation and its vector/line COMDATs). Unit flags:
// the exe module set.
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
#include "lle/UResourceFile.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEStringIDs.h"
#include "rust/won_lobby/wonfront.h"
// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs), over the screens' host
// (EEUIEWonNetArenaGameEvents.cpp)

// Creates the WON game room's game list with its [password], [private] and [in progress] labels measured
EEUICWonNetGamesBox::EEUICWonNetGamesBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation,
                                         unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                         unsigned long inIndex, unsigned long inBorderThickness)
    : UIListBox<EEUICWNGBLine>(inId, std::vector<EEUICWNGBLine>(), inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                               inBorderThickness)
{
    ee_won_front_call(EeWonFrontCall::GamesConstruct, this, (intptr_t)inFont);
}

EEUICWonNetGamesBox::~EEUICWonNetGamesBox() { ee_won_front_call(EeWonFrontCall::GamesDestruct, this, 0); }
long EEUICWonNetGamesBox::CopyData() { return (long)ee_won_front_call(EeWonFrontCall::GamesCopyData, this, 0); }

// Draws the game list's background and selection
long EEUICWonNetGamesBox::DrawPolygons()
{
    return UIListBox<EEUICWNGBLine>::DrawPolygons();
}

// six columns per line; joinable games (not in progress, not full) in bold black, the others in grey
long EEUICWonNetGamesBox::DrawTextA() { return (long)ee_won_front_call(EeWonFrontCall::GamesDrawText, this, 0); }
