// The game's WON lobby classes (C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\). Class names and
// bases from RTTI; sizes from EELobbyInit.cpp's factories; everything else (inferred). Requires <string>, <list>,
// ee/EEWonNet.h, ee/wonapi/WONLobbyResource.h, ee/wonapi/WONLobbyGUI.h.
#ifndef EE_WONAPI_EELOBBYGAMECODE_H
#define EE_WONAPI_EELOBBYGAMECODE_H

#include "ee/wonapi/WONLobbyEvents.h" // the shared LobbyEvent base (0x20 bytes)
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/ee_won.h"

namespace WONAPI {

class EELobbyPlayer;

// a GUIStringData from the Rust as a GUIString (0: the shared empty string)
inline GUIString EELobbyStr(intptr_t inData)
{
    return inData == 0 ? GUIString() : GUIString(reinterpret_cast<GUIStringData*>(inData));
}

class EELobbyGame;
// one player slot of a lobby game (0x18 bytes in the original; EELobbyGame +0xc0, eight of them). Its six options:
// 0 state (0 open, 1 computer, 2 closed, 3 a client's, 4 reserved by name for a human player of a saved game), 1
// team, 2 color, 3 citizen (1-based), 4 observe, 5 file transfer percent (100: done).
// R6 WONLobby round 5: the slot is the Rust game's (ee_won eegame.rs); this is its view (the game and the slot's
// number), whose methods forward (WONLobby/EELobbyGameHost.cpp). Option/SetRawOption are the original's direct
// mOptions reads and stores, GetClient its mClient.mPtr.
class EELobbyPlayerSlot {
public:
    EELobbyPlayerSlot() : mGame(NULL), mIndex(0) {}
    unsigned char GetIndex() { return this->mIndex; }
    unsigned char Option(int inOption);     // mOptions[inOption]

    EELobbyGame* mGame;
    unsigned char mIndex;
private:
    intptr_t RCall(EeWonEeGameCall inCall, intptr_t b = 0, intptr_t c = 0);
};

// a saved game's transfer between the host and the clients (EELobbyFileTransfer.cpp 0x291463..; 0x68 bytes;
// vtable 0x84c440) (provisional name). R6 WONLobby round 7: a shell over ee_won's eexfer.rs object (the path, the
// local and host file's size and checksum, the listening and clients' sockets, the progress time, every method and
// completion callback); the host is EELobbyGameHost.cpp. The shell keeps what the lobby game hands it as C++ objects.
class AsyncSocket;
class QueueSocket;
class AsyncOp;
class EELobbyFileTransfer : public RefCount {
public:
    EELobbyFileTransfer();                  // 0x2916b8
    virtual ~EELobbyFileTransfer();         // 0x29175b

    EeWonEeXfer* mRust;
    GUIString mName;                        // +0x30 the host's file name (SetFileInfo)
    IPAddr mHost;                           // +0x3c (SetHost: fetch it from the host)
};

// the lobby's game (0x180 bytes in the original; RTTI EELobbyGame > LobbyGame; EELobbyGame.cpp 0x29222b..)
// R6 WONLobby round 5: one Rust object (ee_won eegame.rs: the options, the slots, the counts, the file transfer, the
// details' player names, every method and hook); the class keeps its virtual hooks and methods as forwards
// (WONLobby/EELobbyGameHost.cpp) and the eight slot views.
class EELobbyGame : public LobbyGame {
public:
    EELobbyGame();                          // 0x29222b
    virtual ~EELobbyGame();                 // 0x2922b2
    virtual void PostNetLeaveGameHook();    // +0x18 0x292797 (ICF: +0x1c)
    virtual void NotifyStartGameHook();
    virtual bool TryStartGameHook();        // +0x20 0x292719 (false: everyone is on one team)
    virtual bool CanStartGameHook();
    virtual bool CanReadyHook();            // +0x28 0x2926df
    virtual void HandleHostGameHook(LobbyClient* inClient);  // +0x30 0x292a23
    virtual void GetJoinGameRequestHook(WriteBuffer& outBuffer);  // +0x34 0x292e42
    virtual short HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient);  // +0x38 0x292e9f
    virtual void GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer);  // +0x3c 0x293029
    virtual void HandleJoinGameReplyHook(ReadBuffer& inBuffer);  // +0x44 0x2930b5
    virtual void HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer);  // +0x48 0x29321e
    virtual void HandlePlayerJoinedHook(LobbyClient* inClient);  // +0x4c 0x293195
    virtual void WriteSummaryHook(WriteBuffer& outBuffer);  // +0x50 0x292b57
    virtual void WriteDetailsHook(WriteBuffer& outBuffer);  // +0x54 0x292be0
    virtual bool ReadSummaryHook(ReadBuffer& inBuffer);  // +0x58 0x292b95
    virtual bool ReadDetailsHook(ReadBuffer& inBuffer);  // +0x5c 0x292c7e
    virtual void HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom);  // +0x60 0x2932d9
    virtual GUIString GetStatusStringHook(short inStatus);  // +0x64 0x292df3
    void UpdateTransferProgress(int inDone, int inTotal); // 0x2925d8 (transfer progress)
    intptr_t RCall(EeWonEeGameCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0);

    EeWonEeGame* mRust;
    EELobbyPlayerSlot mSlots[8];            // the slots' views
};

// EELobbyGame's notifications to the lobby's controls: a
// player slot (type 100063) or a game option (100062) changed, its number in mParam
class EELobbySyncPlayerEvent : public LobbyEvent {
public:
    EELobbySyncPlayerEvent(int inSlot) : LobbyEvent(LobbyEvent_EELobbySyncPlayer), mParam(inSlot) {}
    int mParam;                            // +0x20 the slot number
};

class EELobbySyncOptionEvent : public LobbyEvent {
public:
    EELobbySyncOptionEvent(int inOption) : LobbyEvent(LobbyEvent_EELobbySyncOption), mParam(inOption) {}
    int mParam;                            // +0x20 the option number
};

// a game room player (0x14 bytes in the original; EELobbyPlayer.cpp's methods are in WONLobby/EELobbyGameHost.cpp)
class EELobbyPlayer : public LobbyPlayer {
public:
    EELobbyPlayer();                        // 0x299a3f (0x14 bytes)
    virtual ~EELobbyPlayer();               // 0x299a75
    virtual bool ReadData(ReadBuffer& inBuffer);            // +0x04 0x299a80
    virtual void WriteData(WriteBuffer& outBuffer);         // +0x08 0x299a95
    virtual bool IsTeammate(LobbyPlayer* inOther);          // +0x0c 0x299aa7
    unsigned char GetTeam();                // 0x299ad1
    unsigned char GetSlot() { return this->mSlot; }  // 0x299adf (+0x10)

    EELobbyPlayerSlot* mPlayerSlot;         // +0x0c (the slot's view, set by the Rust game)
    unsigned char mSlot;                    // +0x10
};

// a player's colour button (EELobbyColorButton.cpp; 0x4c bytes; vtable 0x84c074): a click opens the colour
// chooser popup, 4 x 4 swatches of the colours no other player uses (0x48 bytes; vtable 0x84c140) (provisional names).
// R6 WONLobby round 6: Rust objects (ee_won eecolor.rs: the colour, the held chooser, the hover, the choice, the taken
// colours, the sixteen player colours, every method); the virtuals forward (WONLobby/EELobbyGlueHost.cpp)
class EELobbyColorButton : public Button {
public:
    EELobbyColorButton();                   // 0x290578 (0x4c bytes)
    virtual ~EELobbyColorButton();          // 0x2905ca
    bool SetColor(int inColor);             // 0x2905e8 (0..15)
    int GetColor();                         // mColor (+0x44)
    virtual void Paint(Graphics& inGraphics);                  // +0x18 0x29060b
    virtual bool HandleActivate();          // +0xac 0x290669 (opens or closes the chooser)
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x290751

    EeWonEeColor* mRust;
};

class EELobbyColorChooser : public Component {
public:
    EELobbyColorChooser();                  // 0x290789
    virtual ~EELobbyColorChooser();         // 0x2907c2
    virtual void GetDesiredSize(int& outWidth, int& outHeight);   // 0x2908af (120 x 120)
    virtual void Paint(Graphics& inGraphics);   // 0x2908c1
    virtual void MouseExit();               // 0x290894
    virtual void MouseDown(int inA, int inB, int inC);   // 0x29086e
    virtual void MouseMove(int inX, int inY);   // 0x290801
    virtual void AddedToParent();           // 0x2907cd

    EeWonEeColor* mRust;
};

// the players' connection state before a game starts (EELobbyConnectDialog.cpp; 0xa4 bytes; vtable 0x84c1f0).
// R6 WONLobby round 5: a lobbydlg.rs kind (the list and cancel button held, the running and ended flags, Run, End,
// SetConnected); the host and its list line are WONLobby/LobbyDialogHost.cpp's
class EELobbyConnectDialog : public LobbyRustDialog {
public:
    EELobbyConnectDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Connect) {}
    // runs the dialog: true when every player connected (0x290a8a)
    bool Run(EELobbyGame* inGame) { return this->LCall(EeWonLobbyDlgCall::ConnectRun, (intptr_t)inGame) != 0; }
    void End(bool inOK) { this->LCall(EeWonLobbyDlgCall::ConnectEnd, inOK); }  // 0x290c85 (100001 / 100002)
    void SetConnected(unsigned char inSlot, bool inConnected)  // 0x290cb3 (a player's state)
    { this->LCall(EeWonLobbyDlgCall::ConnectSetConnected, inSlot, inConnected); }
    bool IsRunning() { return this->LCall(EeWonLobbyDlgCall::Field, 7) != 0; }  // mIsRunning
    bool IsEnded() { return this->LCall(EeWonLobbyDlgCall::Field, 8) != 0; }    // mIsEnded (ended by End)
};

// the saved-games dialog (EELobbyFileDialog.cpp; 0xc8 bytes; vtable 0x84c338).
// R6 WONLobby round 6: a lobbydlg.rs kind (its list and buttons held, the directory and extension, ListFiles, the
// selected file's load or delete); the host is WONLobby/LobbyDialogHost.cpp's
class EELobbyFileDialog : public LobbyRustDialog {
public:
    EELobbyFileDialog() : LobbyRustDialog(EeWonLobbyDlgKind::File) {}
    void ListFiles(const std::string& inDirectory, const std::string& inExtension)  // 0x290fd9 (lists the files)
    { this->LCall(EeWonLobbyDlgCall::FileListFiles, (intptr_t)inDirectory.c_str(), (intptr_t)inExtension.c_str()); }
};

// the game room's game options (EELobbyGameOptionsCtrl.cpp; 0xd8 bytes; vtable 0x84c540). Its 16 option controls
// are named by a table (0x9657b8) of combo boxes (type 0) and check boxes (type 1); control ids are index + 1.
// R6 WONLobby round 4: a lobbyctl.rs object (the controls, the room flags, the active flag, every rule); the
// shell is WONLobbyRustCtrl.h's, the host WONLobby/LobbyCtrlHost.cpp
class EELobbyGameOptionsCtrl : public LobbyRustCtrl {
public:
    EELobbyGameOptionsCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::GameOptions) {}
};

// the game room's players: eight rows of a name combo, team, color, citizen, observe and transfer controls
// (EELobbyPlayerOptionsCtrl.cpp; 0x154 bytes; vtable 0x84c718); control ids are slot * 6 + column + 1.
// R6 WONLobby round 5: a lobbyctl.rs object (the 48 controls, the room flags, the active and host flags, its own
// slot, every rule); the shell is WONLobbyRustCtrl.h's, the host WONLobby/LobbyCtrlHost.cpp
class EELobbyPlayerOptionsCtrl : public LobbyRustCtrl {
public:
    EELobbyPlayerOptionsCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::PlayerOptions) {}
};

// the lobby's configuration: WONAPI's plus the file transfer port (0xfc bytes; vtable 0x44c610)
class EELobbyConfig : public LobbyConfig {
public:
    EELobbyConfig();                        // 0x294dbe

    unsigned short mFileTransferPort;       // +0xf8 "EEFileTransferPort" (8889)
};

// a game of the server list (vtable 0x44c674; the inline ctor is the factory's). R6 WONLobby round 6: its texts
// (GetOptionText 0x294ebc, GetNumberText, GetWondersText, GetEpochText, GetOnOffText) and the details' paint are ee_won
// eeglue.rs's
class EELobbyServerDetailsItem : public ServerDetailsItem {
public:
    virtual void PaintDetailsHook(Graphics& inGraphics, MultiListArea* inList);   // +0x38 0x295010
    virtual int GetDetailsHeightHook(MultiListArea* inList);    // +0x3c 0x2954fc
};

}

// the lobby's glue (R6 WONLobby round 6: ee_won eeglue.rs; the entry points are WONLobby/EELobbyGlueHost.cpp's)
bool EELobbyIsConnectDialogRunning();                       // 0x295d52
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected);  // 0x295cab
void EELobbyCancelConnectDialog();                          // 0x295cc3
WONAPI::EELobbyFileDialog* EELobbyGetFileDialog();         // 0x295c0b

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
bool EELobbyGetSavedGameInfo(const char* inFile, unsigned long& outLength, unsigned long& outChecksum);
bool EELobbyLoadSavedGame(const char* inFile);              // 0x298005 (checks the room's players against it)
WONAPI::GUIString EELobbyToGUIString(const UWideString& inText);    // 0x2981ff
void EELobbyFillMapTypeCombo(WONAPI::ComboBox* inCombo);    // 0x29876a (fills the map types)
unsigned long EELobbyGetNumMapTypes();                      // 0x298824 (jmp EERMGetNumberOfGenerators)
void EELobbySetDefaultGameOptions(WONAPI::EELobbyGame* inGame);  // 0x2988d8 (the preferences' last game options)
const WONAPI::GUIString& EELobbyFn2988E9();                 // 0x2988e9 (a function-local static GUIString)
void EELobbyGetVersion(WONAPI::GUIString& outVersion, unsigned long& outChecksum);    // 0x2989b8
// the 0x296aaa unit
void EELobbyClose();                                        // 0x297e51
bool EELobbyHostGame();                                     // 0x296d1e
unsigned short EELobbyGetFileTransferPort();                // 0x295bf5 (EELobbyInit.cpp)

// WONAPI lobby functions the init calls (provisional names)
namespace WONAPI {
int LobbyDoDialog(Component* inDialog);     // 0x358e50
// 0x358f20 (cdecl): a yes/no box
int LobbyMessageBoxYesNo(const GUIString& inTitle, const GUIString& inText, bool inFlag);
void LobbyMessageBox(const GUIString& inTitle, const GUIString& inText);   // 0x358eb0
bool LobbyAddComponentLoader(ResourceConfigTable* inCollection, void (*inFunc)(ComponentConfig* inConfig)); // 0x361fb0 (the init ignores the result)
void LobbyAddGameOption(const GUIString& inOption, const GUIString& inValue, bool inFlag);   // 0x35e040
void LobbySetProductName(const std::wstring& inLoginCommunity, const char* inHashFileListStr); // 0x362150
extern GUIString gEmptyGUIString;           // 0x966244
}

#endif
