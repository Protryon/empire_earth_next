// The game's WON lobby classes (C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\). Class names and
// bases from RTTI; sizes from EELobbyInit.cpp's factories; everything else (inferred). Requires <string>, <list>,
// ee/EEWonNet.h, ee/wonapi/WONLobbyResource.h, ee/wonapi/WONLobbyGUI.h.
#ifndef EE_WONAPI_EELOBBYGAMECODE_H
#define EE_WONAPI_EELOBBYGAMECODE_H

#include "ee/wonapi/WONLobbyEvents.h" // the shared LobbyEvent base (0x20 bytes)
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eexfer.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"

namespace WONAPI {

class EELobbyPlayer;

// a GUIStringData from the Rust as a GUIString (0: the shared empty string)
inline GUIString EELobbyStr(intptr_t inData)
{
    return inData == 0 ? GUIString() : GUIString(reinterpret_cast<GUIStringData*>(inData));
}

// one player slot of a lobby game (0x18 bytes in the original; EELobbyGame +0xc0, eight of them). Its six options:
// 0 state (0 open, 1 computer, 2 closed, 3 a client's, 4 reserved by name for a human player of a saved game), 1
// team, 2 color, 3 citizen (1-based), 4 observe, 5 file transfer percent (100: done).
// R6 WONLobby round 5: the slot is the Rust game's (ee_won eegame.rs). R6 WON round 34: its C++ view
// (EELobbyPlayerSlot: the game and the slot's number) is gone; a player names its slot by the two (EELobbyPlayer).

// a saved game's transfer between the host and the clients (EELobbyFileTransfer.cpp 0x291463..; 0x68 bytes;
// vtable 0x84c440) (provisional name). R6 WONLobby round 7: a shell over ee_won's eexfer.rs object (the path, the
// local and host file's size and checksum, the listening and clients' sockets, the progress time, every method and
// completion callback); the host is EELobbyGameHost.cpp. The shell keeps what the lobby game hands it as C++ objects.
class AsyncSocket;
class QueueSocket;
class AsyncOp;
class EELobbyFileTransfer : public RefCount {
public:
    EELobbyFileTransfer();
    virtual ~EELobbyFileTransfer();

    EeWonEeXfer* mRust;
    GUIString mName;                        // +0x30 the host's file name (SetFileInfo)
    IPAddr mHost;                           // +0x3c (SetHost: fetch it from the host)
};

// the lobby's game (EELobbyGame, 0x180 bytes; EELobbyGame.cpp 0x29222b..): LobbyGame holding the Rust game
// (WONLobbyModelObj.h; R6 WON lobby and network round 37) made by the EE factory (EELobbyGameHost.cpp)
LobbyGame* EELobbyNewGame();

// a game room player (0x14 bytes in the original; EELobbyPlayer.cpp's methods are in WONLobby/EELobbyGameHost.cpp)
class EELobbyPlayer : public LobbyPlayer {
public:
    EELobbyPlayer();                        // 0x299a3f (0x14 bytes)
    virtual ~EELobbyPlayer();
    virtual bool ReadData(ReadBuffer& inBuffer);            // +0x04 0x299a80
    virtual void WriteData(WriteBuffer& outBuffer);         // +0x08 0x299a95
    virtual bool IsTeammate(LobbyPlayer* inOther);          // +0x0c 0x299aa7
    unsigned char GetTeam();
    unsigned char GetSlot() { return this->mSlot; }  // 0x299adf (+0x10)

    LobbyGame* mSlotGame;                   // +0x0c the slot's game (the original's slot pointer: NULL for none)
    unsigned char mSlot;                    // +0x10
    unsigned char mSlotIndex;               // the slot's number in mSlotGame (set by the Rust game with it)
};

// a player's colour button (EELobbyColorButton.cpp; 0x4c bytes; vtable 0x84c074): a click opens the colour
// chooser popup, 4 x 4 swatches of the colours no other player uses (0x48 bytes; vtable 0x84c140) (provisional names).
// R6 WONLobby round 6: Rust objects (ee_won eecolor.rs: the colour, the held chooser, the hover, the choice, the taken
// colours, the sixteen player colours, every method); the virtuals forward (WONLobby/EELobbyGlueHost.cpp)
class EELobbyColorButton : public Button {
public:
    EELobbyColorButton();                   // 0x290578 (0x4c bytes)
    virtual ~EELobbyColorButton();
    bool SetColor(int inColor);             // 0x2905e8 (0..15)
    int GetColor();                         // mColor (+0x44)
    virtual void Paint(Graphics& inGraphics);                  // +0x18 0x29060b
    virtual bool HandleActivate();          // +0xac 0x290669 (opens or closes the chooser)
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x290751

    EeWonEeColor* mRust;
};

class EELobbyColorChooser : public Component {
public:
    EELobbyColorChooser();
    virtual ~EELobbyColorChooser();
    virtual void GetDesiredSize(int& outWidth, int& outHeight);   // 0x2908af (120 x 120)
    virtual void Paint(Graphics& inGraphics);
    virtual void MouseExit();
    virtual void MouseDown(int inA, int inB, int inC);
    virtual void MouseMove(int inX, int inY);
    virtual void AddedToParent();

    EeWonEeColor* mRust;
};

// R6 WON lobby and network round 37: the EE dialogs and controls are plain shells of their Rust kinds (the factories
// in EELobbyGlueHost.cpp):
// - EELobbyConnectDialog (EELobbyConnectDialog.cpp; 0xa4 bytes; vtable 0x84c1f0): the players' connection state
//   before a game starts, lobbydlg.rs's Connect (Run 0x290a8a, End 0x290c85);
// - EELobbyFileDialog (EELobbyFileDialog.cpp; 0xc8 bytes; vtable 0x84c338): the saved games, lobbydlg.rs's File
//   (ListFiles 0x290fd9);
// - EELobbyGameOptionsCtrl (EELobbyGameOptionsCtrl.cpp; 0xd8 bytes; vtable 0x84c540): the game room's 16 game
//   options (a table 0x9657b8 of combo and check boxes, ids index + 1), lobbyctl.rs's GameOptions;
// - EELobbyPlayerOptionsCtrl (EELobbyPlayerOptionsCtrl.cpp; 0x154 bytes; vtable 0x84c718): the game room's eight
//   player rows (ids slot * 6 + column + 1), lobbyctl.rs's PlayerOptions.

// the lobby's configuration: WONAPI's plus the file transfer port (0xfc bytes; vtable 0x44c610)
class EELobbyConfig : public LobbyConfig {
public:
    EELobbyConfig();

    unsigned short mFileTransferPort;       // +0xf8 "EEFileTransferPort" (8889)
};

}

// the lobby's glue (R6 WONLobby round 6: ee_won eeglue.rs; the entry points are WONLobby/EELobbyGlueHost.cpp's)
bool EELobbyIsConnectDialogRunning();
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected);
void EELobbyCancelConnectDialog();
WONAPI::LobbyRustDialog* EELobbyGetFileDialog(); // the File kind

// EELobbyResource.cpp
WONAPI::ResourceConfigTablePtr EELobbyResource_Init(WONAPI::ResourceConfigTable* inCollection);
WONAPI::GUIString& EELobbyResource_GetString(int inID);
void EELobbyResource_Destroy();
// a lobby string by its original global's name (its row in ee_won eeresource.rs)
WONAPI::GUIString& EELobbyResource_Named(const char* inName);
#define EE_LOBBY_STRING(name) EELobbyResource_Named(#name)

// the 0x297ee7 unit
class EEGameOptions;
class EEGameOptionsPlayer;
class EEPSaveGameHeader;
class UWideString;
// 0x297f2d: a saved game's file length and header checksum (false when it cannot be read)
bool EELobbyGetSavedGameInfo(const char* inFile, uint32_t& outLength, uint32_t& outChecksum);
bool EELobbyLoadSavedGame(const char* inFile);              // 0x298005 (checks the room's players against it)
WONAPI::GUIString EELobbyToGUIString(const UWideString& inText);
void EELobbyFillMapTypeCombo(WONAPI::ComboBox* inCombo);    // 0x29876a (fills the map types)
unsigned long EELobbyGetNumMapTypes();                      // 0x298824 (jmp EERMGetNumberOfGenerators)
void EELobbySetDefaultGameOptions(WONAPI::LobbyGame* inGame);  // 0x2988d8 (the preferences' last game options)
const WONAPI::GUIString& EELobbyFn2988E9();                 // 0x2988e9 (a function-local static GUIString)
// the 0x296aaa unit
void EELobbyClose();
bool EELobbyHostGame();
unsigned short EELobbyGetFileTransferPort();                // 0x295bf5 (EELobbyInit.cpp)

// WONAPI lobby functions the init calls (provisional names)
namespace WONAPI {
int LobbyDoDialog(Component* inDialog);
// 0x358f20 (cdecl): a yes/no box
int LobbyMessageBoxYesNo(const GUIString& inTitle, const GUIString& inText, bool inFlag);
void LobbyMessageBox(const GUIString& inTitle, const GUIString& inText);
bool LobbyAddComponentLoader(ResourceConfigTable* inCollection, void (*inFunc)(ComponentConfig* inConfig)); // 0x361fb0 (the init ignores the result)
void LobbySetProductName(const std::wstring& inLoginCommunity, const char* inHashFileListStr);
}

#endif
