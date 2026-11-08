// The game's WON lobby classes (C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\). Class names and
// bases from RTTI; sizes from EELobbyInit.cpp's factories; everything else (inferred). Requires <string>, <list>,
// ee/EEWonNet.h, ee/wonapi/WONLobbyResource.h, ee/wonapi/WONLobbyGUI.h.
#ifndef EE_WONAPI_EELOBBYGAMECODE_H
#define EE_WONAPI_EELOBBYGAMECODE_H

#include <set>                              // EELobbyFileTransfer::mClients
#include "ee/wonapi/WONLobbyEvents.h" // the shared LobbyEvent base (0x20 bytes)
#include "ee/wonapi/WONLobbyEventTypes.h"

namespace WONAPI {

class EELobbyPlayer;

// one player slot of a lobby game (EELobbyPlayer.cpp; 0x18 bytes; EELobbyGame +0xc0, eight of them; RTTI
// EELobbyPlayerSlot, vtable 0x84c6fc). Its six options (0x299910 reads them by index, the message loops
// 0x2999f3/0x299a1e walk them): 0 state (0 open, 1 computer, 2 closed, 3 a client's, 4 reserved by name for a
// human player of a saved game), 1 team, 2 color, 3 citizen (1-based), 4 observe, 5 file transfer percent
// (100: done).
class EELobbyPlayerSlot {
public:
    EELobbyPlayerSlot();                    // 0x299838
    virtual ~EELobbyPlayerSlot();           // 0x299894
    void Reset();                           // 0x299880
    const GUIString& GetName();             // 0x2998c8
    void SetState(int inState);             // 0x2998e3 (4 keeps the name)
    unsigned char GetOption(int inOption);  // 0x299910
    void SetOption(int inOption, unsigned char inValue);   // 0x299928
    void ClearPlayer();                     // 0x29994f
    void SetOpen();                         // 0x299973 (SetState(0))
    void SetClosed();                       // 0x29997b (SetState(2))
    void SetComputer();                     // 0x299983 (SetState(1))
    void SetName(const GUIString& inName);  // 0x29998b (SetState(4): reserves the slot for a saved game's player)
    void SetPlayer(LobbyClient* inClient, EELobbyPlayer* inPlayer);   // 0x2999a7
    void ReadData(ReadBuffer& inBuffer);    // 0x2999f3
    void WriteData(WriteBuffer& outBuffer); // 0x299a1e
    unsigned char GetIndex() { return this->mIndex; }  // inline (Virtual4C 0x293195: `mov al,[esi+4]; movzx eax,al`)

    unsigned char mIndex;                   // +0x04 (the slot's number; given to its player's +0x10)
    unsigned char mOptions[6];              // +0x05
    SmartPtr<LobbyClient> mClient;          // +0x0c
    SmartPtr<EELobbyPlayer> mPlayer;        // +0x10
    GUIString mName;                        // +0x14
};

// a saved game's transfer between the host and the clients (EELobbyFileTransfer.cpp 0x291463..; 0x68 bytes;
// vtable 0x84c440; method names inferred) (provisional name)
class AsyncSocket;
class QueueSocket;
class AsyncOp;
class EELobbyFileTransfer : public RefCount {
public:
    EELobbyFileTransfer();                  // 0x2916b8
    virtual ~EELobbyFileTransfer();         // 0x29175b
    void SetPath(const std::string& inPath);            // 0x2917a2 (where the file is)
    void SetFileInfo(const GUIString& inName, unsigned long inSize, unsigned long inChecksum);  // 0x2917e9
    void SetHost(const IPAddr& inAddress);  // 0x29180c (fetch it from the host)
    bool IsFileHere();                      // 0x291859 (the file is here already)
    long Serve(unsigned short inPort);      // 0x291a09
    void Stop();                            // 0x291b1e
    void CloseClients();                    // 0x291b40
    void Download();                        // 0x291b79
    // the completions' callbacks (LobbyCompletion<EELobbyFileTransfer>)
    void OnClientClosed(AsyncOp* inOp);     // 0x291870 (a client's socket closed)
    void OnClientAccepted(AsyncOp* inOp);   // 0x2918b2 (a client connected: send it the file)
    void OnProgress(AsyncOp* inOp);         // 0x29199e
    void OnFileReceived(AsyncOp* inOp);     // 0x2919dc (the file arrived)

    // (the constructor 0x2916b8 and the destructor 0x29175b give the members) (provisional names)
    SmartPtr<QueueSocket> mConnection;      // +0x08 the listening socket (Serve; Stop closes it)
    std::set<SmartPtr<AsyncSocket> > mClients;   // +0x0c the clients' sockets (CloseClients closes each)
    std::string mPath;                      // +0x18 (SetPath)
    unsigned long mLocalSize;               // +0x28 (EELobbyGetSavedGameInfo of mPath)
    unsigned long mLocalChecksum;           // +0x2c
    GUIString mName;                        // +0x30
    unsigned long mSize;                    // +0x34
    unsigned long mChecksum;                // +0x38
    IPAddr mHost;                           // +0x3c (SetHost)
    long mLastProgressTime;                 // +0x64
};

// the lobby's game (0x180 bytes; RTTI EELobbyGame > LobbyGame; EELobbyGame.cpp 0x29222b..) (provisional name)
class EELobbyGame : public LobbyGame {
public:
    EELobbyGame();                          // 0x29222b
    virtual ~EELobbyGame();                 // 0x2922b2
    virtual void PostNetLeaveGameHook();    // +0x18 0x292797 (ICF: +0x1c)
    virtual void NotifyStartGameHook();
    virtual bool TryStartGameHook();        // +0x20 0x292719 (false: everyone is on one team)
#ifdef EE_DINKUM_SXL233
    virtual bool CanStartGameHook();
#else
    virtual long CanStartGameHook();        // +0x24 0x2926f9 (returns through eax: push 1/pop eax, xor eax, eax)
#endif
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
    int GetNumFiles();                      // 0x2922f2
    const GUIString& GetFile(int inIndex);  // 0x292305
    void WriteGameFile(WriteBuffer& outBuffer);  // 0x29247e (the saved game's details)
    void ReadGameFile(ReadBuffer& inBuffer);  // 0x2924e0
    void StopTransfer();                    // 0x2925cb
    void UpdateTransferProgress(int inDone, int inTotal); // 0x2925d8 (transfer progress)
    void SetTransferPercent(unsigned char inPercent); // 0x292616
    void StartTransfer();                   // 0x292630 (a client starts its transfer)
    void ResetSlots();                      // 0x29279c
    void CountPlayers(bool inNotify);       // 0x2927d0 (counts the players)
    void WriteOption(WriteBuffer& outBuffer, int inOption);    // 0x29288b
    void ReadOption(ReadBuffer& inBuffer, int inOption);       // 0x2928e0
    const GUIString& GetGameFileName();     // 0x29246d (+0x78's text +0x30, or the empty string)
    EELobbyPlayerSlot* GetPlayerSlot(int inSlot);   // 0x292847 (NULL outside 0..7)
    bool HasGameFile();                     // 0x292862 (GetOption(0) != 0: not a random map)
    long GetOption(int inOption);           // 0x292870 (0 outside 0..15)
    void SetOption(int inOption, unsigned long inValue);    // 0x293894 (below the option's limit 0x965734)
    void ChangeOption(int inOption, unsigned long inValue, bool inFlag);  // 0x2938b8
    void SendGameState();                   // 0x293a8b
    void ChangePlayerOption(int inSlot, int inWhat, unsigned char inValue);  // 0x293b36 (a player option changed)
    void SendConnectStatus(bool inConnected);  // 0x293437
    void SendCancelConnect();               // 0x2934a0
    void SendOption(int inOption);          // 0x29382d
    // (provisional names): 0x292d71 a free colour for slot inExcept (inColor when free), 0x292daa the players'
    // colours made distinct, 0x293075 inClient into its player's slot (the slot, -1), 0x2933ce message 0x69
    unsigned char GetFreeColor(int inExcept, unsigned char inColor);
    void MakeColorsDistinct();
    int SeatClient(LobbyClient* inClient);
    void BroadcastConnectStatus(unsigned char inSlot, bool inConnected);
    // the game messages of 0x2932d9 (0x6a, 0x68/0x69, 0x66, 0x64) (provisional names)
    void HandleCancelConnect(ReadBuffer& inBuffer, LobbyClient* inFrom);
    void HandleConnectStatus(ReadBuffer& inBuffer, LobbyClient* inFrom, bool inFromClient);
    void HandlePlayerOptionRequest(ReadBuffer& inBuffer, LobbyClient* inFrom);
    void HandleOptionChange(ReadBuffer& inBuffer, LobbyClient* inFrom);
    void HandlePlayerOptionChange(ReadBuffer& inBuffer, LobbyClient* inFrom);  // 0x65
    void HandleGameState(ReadBuffer& inBuffer, LobbyClient* inFrom);  // 0x67
    // 0x29232f: a saved game's file name, size and checksum
    int GetMySlot();                        // 0x29398e
    void SetGameFile(const GUIString& inName, unsigned long inSize, unsigned long inChecksum, bool inFlag);
    void GetTakenColors(bool* outTaken, int inExcept);  // 0x292ade
    bool IsColorFree(unsigned char inColor);  // 0x292b29 (the colour is free)
    int GetMyTeam();                        // 0x2939a6 (the local player's team, -1)

    std::vector<GUIString> mFiles;          // +0x68
    SmartPtr<EELobbyFileTransfer> mFileTransfer;    // +0x78
    bool mHasFile;                          // +0x7c (the saved game is here)
    unsigned char mNumHumanPlayers;         // +0x7d (human players)
    unsigned char mNumComputerSlots;        // +0x7e (computer players: state 1 slots)
    unsigned char mNumReservedSlots;        // +0x7f (state 4 slots, not observing: saved-game players yet to join)
    long mOptions[16];                      // +0x80
    EELobbyPlayerSlot mSlots[8];            // +0xc0
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

class EELobbyPlayer : public LobbyPlayer {
public:
    EELobbyPlayer();                        // 0x299a3f (0x14 bytes)
    virtual ~EELobbyPlayer();               // 0x299a75
    virtual bool ReadData(ReadBuffer& inBuffer);            // +0x04 0x299a80
    virtual void WriteData(WriteBuffer& outBuffer);         // +0x08 0x299a95
    virtual bool IsTeammate(LobbyPlayer* inOther);          // +0x0c 0x299aa7
    unsigned char GetTeam();                // 0x299ad1
    unsigned char GetSlot();                // 0x299adf (+0x10)

    EELobbyPlayerSlot* mPlayerSlot;         // +0x0c
    unsigned char mSlot;                    // +0x10
};

// a player's colour button (EELobbyColorButton.cpp; 0x4c bytes; vtable 0x84c074): a click opens the colour
// chooser popup (provisional name)
class EELobbyColorChooser;

class EELobbyColorButton : public Button {
public:
    EELobbyColorButton();                   // 0x290578 (0x4c bytes)
    virtual ~EELobbyColorButton();          // 0x2905ca
    bool SetColor(int inColor);             // 0x2905e8 (0..15)
    virtual void Paint(Graphics& inGraphics);                  // +0x18 0x29060b
    virtual bool HandleActivate();          // +0xac 0x290669 (opens or closes the chooser)
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x290751

    int mColor;                             // +0x44
    SmartPtr<EELobbyColorChooser> mChooser; // +0x48
};

// the players' connection state before a game starts (EELobbyConnectDialog.cpp; 0xa4 bytes; vtable 0x84c1f0)
class EELobbyConnectDialog : public Dialog {
public:
    EELobbyConnectDialog();                 // 0x2909a1 (0xa4 bytes)
    virtual ~EELobbyConnectDialog();        // 0x2909f6
    virtual void InitComponent(ComponentInit& inInit);         // +0x1c 0x290a41
    bool Run(EELobbyGame* inGame);          // 0x290a8a (runs the dialog: true when every player connected)
    void End(bool inOK);                    // 0x290c85 (ends it: 100001 / 100002)
    void SetConnected(unsigned char inSlot, bool inConnected);  // 0x290cb3 (a player's state)

    SmartPtr<ListArea> mConnectList;        // +0x98 "ConnectList"
    SmartPtr<Button> mCancelButton;         // +0x9c "CancelButton"
    bool mIsRunning;                        // +0xa0 (the dialog is running)
    bool mIsEnded;                          // +0xa1 (it was ended by End)
};

// the saved-games dialog (EELobbyFileDialog.cpp; 0xc8 bytes; vtable 0x84c338)
class EELobbyFileDialog : public Dialog {
public:
    EELobbyFileDialog();                    // 0x290e89 (0xc8 bytes)
    virtual ~EELobbyFileDialog();           // 0x290efc
    virtual void InitComponent(ComponentInit& inInit);         // +0x1c 0x290f66
    void ListFiles(const std::string& inDirectory, const std::string& inExtension);  // 0x290fd9 (lists the files)
    void LoadSelectedFile();                // 0x291249 (loads the selected saved game)
    std::string GetSelectedFile();          // 0x29129a ("" without a selection)
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x29136a

    std::string mDirectory;                 // +0x98
    std::string mExtension;                 // +0xa8
    SmartPtr<ListArea> mFileList;           // +0xb8 "FileList"
    SmartPtr<Button> mDeleteButton;         // +0xbc "DeleteButton"
    SmartPtr<Button> mLoadButton;           // +0xc0 "LoadButton"
    SmartPtr<Button> mCancelButton;         // +0xc4 "CancelButton"
};

// the game room's game options (EELobbyGameOptionsCtrl.cpp; 0xd8 bytes; vtable 0x84c540). Its 16 option controls
// are named by a table (0x9657b8) of combo boxes (type 0) and check boxes (type 1); control ids are index + 1.
class EELobbyGameOptionsCtrl : public Container {
public:
    EELobbyGameOptionsCtrl();               // 0x2942ce
    virtual ~EELobbyGameOptionsCtrl();      // 0x294336
    virtual void InitComponent(ComponentInit& inInit);         // +0x1c 0x2944ae
    Component* GetControl(int inOption);    // 0x294680
    ComboBox* GetComboBox(int inOption);    // 0x29469b
    Button* GetCheckBox(int inOption);      // 0x2946c0
    void UpdateOption(int inOption, EELobbyGame* inGame);      // 0x2946e5
    void UpdateOption(int inOption);        // 0x294788
    void UpdateAll();                       // 0x2947a0
    void Activate();                        // 0x29488a
    void Deactivate();                      // 0x2948e2
    void ChangeGameType();                  // 0x2948f5 (the game type changed)
    void OptionChanged(int inOption);       // 0x294a77
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x294b57
    EELobbyGame* GetGame();                 // ICF-folded into 0x299dcd (jmp GetMyLobbyGame)

    SmartPtr<Label> mCampaignInfoLabel;     // +0x84
    SmartPtr<Container> mCampaignInfoBox1;  // +0x88
    SmartPtr<Container> mCampaignInfoBox2;  // +0x8c
    long mRoomSpec;                         // +0x90 (2; ORed with event 100054's parameter)
    bool mIsActive;                         // +0x94
    SmartPtr<Component> mOptions[16];       // +0x98
};

// the game room's players: eight rows of a name combo, team, color, citizen, observe and transfer controls
// (EELobbyPlayerOptionsCtrl.cpp; 0x154 bytes; vtable 0x84c718); control ids are slot * 6 + column + 1
class EELobbyPlayerOptionsCtrl : public Container {
public:
    EELobbyPlayerOptionsCtrl();             // 0x299b17
    virtual ~EELobbyPlayerOptionsCtrl();    // 0x299ba5
    virtual void InitComponent(ComponentInit& inInit);         // +0x1c 0x299c56
    EELobbyGame* GetGame();                 // 0x299dcd (jmp GetMyLobbyGame; ICF-shared with the game options')
    void UpdatePlayer(int inSlot, EELobbyGame* inGame);        // 0x299dd2
    void UpdatePlayer(int inSlot);          // 0x29a0f5
    void UpdateAll();                       // 0x29a10d
    void Activate();                        // 0x29a132
    void Deactivate();                      // 0x29a183
    void PlayerOptionChanged(int inSlot, int inWhat);          // 0x29a19f
    void HandlePlayerChanged(ComponentEvent* inEvent); // 0x29a240
    virtual void HandleComponentEvent(ComponentEvent* inEvent);    // +0x7c 0x29a27c

    long mRoomSpec;                         // +0x84 (2; ORed with event 100054's parameter)
    bool mIsActive;                         // +0x88
    long mMySlot;                           // +0x8c (-1)
    bool mIsHost;                           // +0x90
    SmartPtr<ComboBox> mUserNameCombo[8];   // +0x94
    SmartPtr<ComboBox> mTeamCombo[8];       // +0xb4
    SmartPtr<EELobbyColorButton> mColorButton[8];  // +0xd4
    SmartPtr<ComboBox> mCitizenCombo[8];    // +0xf4
    SmartPtr<Button> mObserveCheck[8];      // +0x114
    SmartPtr<Label> mFileTransferLabel[8];  // +0x134
};

// the lobby's configuration: WONAPI's plus the file transfer port (0xfc bytes; vtable 0x44c610)
class EELobbyConfig : public LobbyConfig {
public:
    EELobbyConfig();                        // 0x294dbe

    unsigned short mFileTransferPort;       // +0xf8 "EEFileTransferPort" (8889)
};

// a game of the server list (vtable 0x44c674; the inline ctor is the factory's)
class EELobbyServerDetailsItem : public ServerDetailsItem {
public:
    EELobbyGame* GetGame() const { return static_cast<EELobbyGame*>(this->mGame.get()); }
    GUIString GetOptionText(int inOption, int inFirstString, unsigned long inCount);   // 0x294ebc
    GUIString GetNumberText(int inOption);  // 0x294f02
    GUIString GetWondersText();             // 0x294f39
    GUIString GetEpochText(bool inStart);   // 0x294f7f
    GUIString GetOnOffText(int inOption);   // 0x294fd7

    virtual void PaintDetailsHook(Graphics& inGraphics, MultiListArea* inList);   // +0x38 0x295010
    virtual int GetDetailsHeightHook(MultiListArea* inList);    // +0x3c 0x2954fc
};

}

// EELobbyInit.cpp
bool EELobbyIsConnectDialogRunning();                       // 0x295d52
void EELobbySetSlotConnected(unsigned char inSlot, bool inConnected);  // 0x295cab
void EELobbyCancelConnectDialog();                          // 0x295cc3
WONAPI::EELobbyFileDialog* EELobbyGetFileDialog();         // 0x295c0b
WONAPI::EELobbyConnectDialog* EELobbyGetConnectDialog();   // 0x295c11

// EELobbyResource.cpp
WONAPI::ResourceConfigTablePtr EELobbyResource_Init(WONAPI::ResourceConfigTable* inCollection);
WONAPI::GUIString& EELobbyResource_GetString(int inID);
void EELobbyResource_Destroy();
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_Epoch16;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_GameType2;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_OptionOn;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_OptionOff;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_WondersOff;
extern WONAPI::GUIString gEELobbyPlayerOptionsCtrl_TransferPercent;
extern WONAPI::GUIString gEELobbyInterface_LoadGameError;
extern WONAPI::GUIString gEELobbyInterface_ErrorReadingFile;
extern WONAPI::GUIString gEELobbyInterface_PlayerNotInGame;
// the server details item's labels
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_GameType;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_MapSize;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_MapType;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_Resources;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_StartingEpoch;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_EndingEpoch;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_GameUnitLimit;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_GameVariant;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_DifficultyLevel;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_GameSpeed;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_RevealMap;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_UseCustomCivs;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_LockTeams;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_LockSpeed;
extern WONAPI::GUIString gEELobbyGameOptionsCtrl_NumberOfWonders;
extern WONAPI::GUIString gEELobbyInterface_Players;

// the 0x297ee7 unit
class EEGameOptions;
class EEGameOptionsPlayer;
class EEPSaveGameHeader;
class UWideString;
// 0x297f2d: a saved game's file length and header checksum (false when it cannot be read)
bool EELobbyGetSavedGameInfo(const char* inFile, unsigned long& outLength, unsigned long& outChecksum);
bool EELobbyLoadSavedGame(const char* inFile);              // 0x298005 (checks the room's players against it)
WONAPI::GUIString EELobbyToGUIString(const UWideString& inText);    // 0x2981ff
void EELobbySetupSavedGame(const EEGameOptions& inOptions, const char* inFile, const EEPSaveGameHeader& inHeader);   // 0x298266
void EELobbySetSlot(WONAPI::EELobbyPlayerSlot* inSlot, const EEGameOptionsPlayer& inPlayer);   // 0x2984ca
void EELobbySetGameOptions(const EEGameOptions& inOptions, WONAPI::EELobbyGame* inGame);       // 0x2985a8
WONAPI::GUIString& EELobbyGetMapTypeName(int inMapType);    // 0x2986e9
void EELobbyFillMapTypeCombo(WONAPI::ComboBox* inCombo);    // 0x29876a (fills the map types)
void EELobbyAddMapTypeOptions();                            // 0x2987bd (the map types as "MapType" game options)
unsigned long EELobbyGetNumMapTypes();                      // 0x298824 (jmp EERMGetNumberOfGenerators)
void EELobbyGetPlayerColors(unsigned long* outColors, int inCount);  // 0x298829 (the player colours, 0x00rrggbb)
void EELobbySetDefaultGameOptions(WONAPI::EELobbyGame* inGame);  // 0x2988d8 (the preferences' last game options)
const WONAPI::GUIString& EELobbyFn2988E9();                 // 0x2988e9 (a function-local static GUIString)
void EELobbyGetVersion(WONAPI::GUIString& outVersion, unsigned long& outChecksum);    // 0x2989b8
void EELobbyAddResources(WONAPI::ResourceConfigTable* inCollection);  // 0x2989ef
// the 0x296aaa unit
void EELobbyClose();                                        // 0x297e51
bool EELobbyHostGame();                                     // 0x296d1e
unsigned short EELobbyGetFileTransferPort();                // 0x295bf5 (EELobbyInit.cpp)
// 0x8d01f4: the lobby's sixteen player colours (.data; filled by EELobbyInit through 0x298829; the colour button
// and the connect dialog read it; defined in the 0x290514 range) (provisional name)
extern unsigned long gEELobbyColors[16];

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
