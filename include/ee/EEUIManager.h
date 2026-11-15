// C:\EEX\Empire Earth\User Interface Thread\EEUIManager.h (RECONSTRUCTION; file name inferred). The game's UI
// manager (RTTI EEUIManager > UIManager; vtable 0x44ae90, ctor 0x271319, sizeof 0x140: `new` at the startup code).
// The game holds it in `gUIManager` (0x53191c, assigned at 0x1424f7; provisional name). EEUIManager.cpp is
// 0x271319..0x2750a9+ (the ctor registers the control-event factories 0x2726b3.. with RegisterControlEventCallback
// and one form-event factory 0x271a10; catalog). Field names
// are provisional. Requires lle/UIManager.h.
#ifndef EE_EEUIMANAGER_H
#define EE_EEUIMANAGER_H

class EEUIFMainGame;
class EEUIFScenarioEditor;

// the message boxes' button callbacks (EEUIFError, 0x260ffb)
typedef void (*EEUIFErrorCallback)(UIForm* inForm);
// the password prompt's callback: gets the typed text (EEUIFError 0x26151a stores it with the edit box's text;
// provisional name)
typedef void (*EEUIFPasswordCallback)(const char* inPassword);

struct EEUIManagerCall;                     // (EEUIManager.cpp: the host's record of one ui_manager.rs call)

class EEUIManager : public UIManager {
public:
    // the ee_screens ui_manager.rs host (EE_SCREENS_UM_OP_*; a member: UIManager's updates are protected)
    static int64_t HostOp(EEUIManagerCall& ioCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC);
    // EEUIManager.cpp: 0x271319 (`ret 0x10`; the fourth argument is unused) registers the event
    // factories, loads the UI database, then the members (store order +0x12c, +0x13c, +0x130, +0x13d, +0x138)
    EEUIManager(GERasterizer* inRasterizer, unsigned long inNumberOfSurfaces, unsigned long inScreen,
                const UString& inUnused);
    virtual ~EEUIManager();                 // 0x27156e (??_G 0x271552)
    // vtable overrides (slots from 0x44ae90): CreateUserDefinedBackground 0x14 (0x274564), CreateUserDefinedControl
    // 0x18 (0x274647; not declared yet), CreateForm 0x1c (0x274c94), UpdateKeyboard 0x2c (0x2750a9), UpdateMouse
    // 0x30 (0x2751ab)
    virtual UIBackground* CreateUserDefinedBackground(const UIBackgroundTable& inTable); //274564, actual base slot14
    virtual UIControl* CreateUserDefinedControl(UIControlsTable& inTable); //274647, actual base slot18
    virtual UIForm* CreateForm(const UIDbDataForms& inData);
    virtual long UpdateKeyboard();
    virtual long UpdateMouse(const GE2DPoint& inMouseLocation);
    // 0x274d79 / 0x274e2d: a new EEUIFError with the cancel / yes-no-cancel buttons (provisional names).
    // inAlignment: EEUIFError::Display*'s (pushed as a dword)
    void DisplayCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inCallback,
                       UIForm* inCancel);
    void DisplayYesNoCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                            EEUIFErrorCallback inNo);
    void CreateScenarioEditor();            // 0x273e7d (creates the editor form at +0x130 once)
    // 0x274db5: a new EEUIFError (0x1bc bytes) showing inMessage with one button (0x261788). Parameter names are
    // provisional
    void DisplayMessage(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inCallback,
                        bool inFlag);
    // 0x274d40: a new EEUIFError (0x1bc bytes) with a password edit box (0x26151a); inCallback gets the text
    // (the WON game room's join, 0x1ea544). Parameter names are provisional
    void DisplayPasswordPrompt(const UWideString& inMessage, unsigned long inAlignment,
                               EEUIFPasswordCallback inCallback, bool inFlag);
    // 0x274df1: the same with two buttons (0x261894)
    void DisplayQuestion(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm, EEUIFErrorCallback inYes,
                         EEUIFErrorCallback inNo);
    void StartScenarioEditor();             // 0x274e69 (sets +0x13c, starts a local server)
    void DeleteScenarioEditor();            // 0x274d24 (deletes +0x130 through its vtable; the main screen's Open)
    void FnE123D();                         // (ICF-folded into 0xe123d: an empty body; message 0x407)
    // 0x26fa55 / 0x26fa6a: the main group's mL1C = 0 / 1 and its mB25 (EEUIFMGStatusButton::Command's buttons 1 and
    // 0; defined in EEUIFMGStatusButton.cpp's range; provisional names)
    void Fn26FA55();
    void Fn26FA6A();

    // ctor store order (0x2714ad..0x2714e5): +0x12c, +0x13c, +0x130, +0x13d, SetMusicVolume, +0x138
    EEUIFScenarioEditor* GetScenarioEditorForm() const { return this->mScenarioEditorForm; }
    EEUIFMainGame* GetMainGameForm() const { return this->mMainGameForm; }  // (EEUserInterface 0x277023/0x277315 load it before GetCurrentForm)
    EEUIFMainGame* mMainGameForm;           // +0x12c GetFormByID(0xb) in the ctor; cleared at 0x271587
    EEUIFScenarioEditor* mScenarioEditorForm;   // +0x130 new(0x2b8) EEUIFScenarioEditor at 0x273e8e; 0 in the ctor
    unsigned long mU134;                    // +0x134 (no game access found)
    unsigned long mU138;                    // +0x138 0 in the ctor (no other access found)
    bool GetInScenarioEditor() const { return this->mInScenarioEditor; }  // (MGOE OpenOptionsScreen's inline budget)
    bool mInScenarioEditor;                 // +0x13c (cleared by the main-menu events; set by StartScenarioEditor)
    bool mB13D;                             // +0x13d 0 in the ctor
};

extern EEUIManager* gUIManager;             // 0x53191c

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 32 'Available
// Games (Sierra) Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEAvailableGamesControl {
    kAvailableGamesPlayersNameLabel = 2,             // dbuicontrols form 32 index 2 id 3080 'AG Player's Name Label' (type 15); site form via UITextLabel* theUserLabel = (UITextLabel
    kAvailableGamesRoomsNameLabel = 4,               // dbuicontrols form 32 index 4 id 3082 'AG Room's Name Label' (type 15); site form via UITextLabel* theRoomLabel = (UITextLabel
    kAvailableGamesJoinGameTab = 5,                  // dbuicontrols form 32 index 5 id 3083 'AG Join Game Tab' (type 12); site form via TriggerEvent(new EEUIEWNAGToggleJoinScre
    kAvailableGamesHostGameTab = 6,                  // dbuicontrols form 32 index 6 id 3084 'AG Host Game Tab' (type 12); site form via TriggerEvent(new EEUIEWNAGToggleHostScre
    kAvailableGamesPlayersInRoomListBox = 8,         // dbuicontrols form 32 index 8 id 3086 'AG Players in Room List box' (type 9); site form via gWonNet->RefreshGameList(inForm
    kAvailableGamesAvalGamesListBox = 13,            // dbuicontrols form 32 index 13 id 3091 'AG Aval. Games List Box' (type 1019); site form via EUICWonNetGamesBox*>(gUIManager->GetForm
    kAvailableGamesChatEditBox = 16,                 // dbuicontrols form 32 index 16 id 3094 'AG Chat Edit Box' (type 8); site form via UIEditBox* theChat = (UIEditBox*)inForm
    kAvailableGamesJoinHostGameButton = 17,          // dbuicontrols form 32 index 17 id 3095 'AG Join/Host Game Button' (type 28); site form via etTriggerEvent(new EEUIEWNAGHostOrJoinGa
    kAvailableGamesBackButton = 18,                  // dbuicontrols form 32 index 18 id 3096 'AG Back Button' (type 28); site form via EEUIEWNSetTriggerEvent(new EEUIEWNAGBack
    kAvailableGamesHostGameNameEditBox = 24,         // dbuicontrols form 32 index 24 id 3102 'AG Host Game Name Edit Box' (type 8); site form via UIEditBox* theName = (UIEditBox*)inForm
    kAvailableGamesPWForHostedGameEditBox = 27,      // dbuicontrols form 32 index 27 id 3129 'AG PW for Hosted Game Edit Box' (type 8); site form via UIEditBox* thePassword = (UIEditBox*)inF
    kAvailableGamesPWForHostedGameCheckbox = 28,     // dbuicontrols form 32 index 28 id 3130 'AG PW for Hosted Game Checkbox' (type 2); site form via UICheckBox* theCheckBox = (UICheckBox*)i
    kAvailableGamesPWRetypeForHostedGameEditBox = 29,  // dbuicontrols form 32 index 29 id 3133 'AG PW Retype for Hosted Game Edit Box' (type 8); site form via UIEditBox* theConfirmation = (UIEditBox*
    kAvailableGamesPWForHostedGameLabel = 30,        // dbuicontrols form 32 index 30 id 3131 'AG PW  for Hosted Game Label' (type 15); site form via UIControl* thePasswordLabel = inForm
    kAvailableGamesPWRetypeForHostedGameLabel = 31,  // dbuicontrols form 32 index 31 id 3132 'AG PW Retype for Hosted Game Label' (type 15); site form via UIControl* theConfirmationLabel = inForm
    kAvailableGamesInProgressToggle = 32,            // dbuicontrols form 32 index 32 id 3350 'AG In Progress Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
    kAvailableGamesPasswordToggle = 33,              // dbuicontrols form 32 index 33 id 3351 'AG Password Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
    kAvailableGamesPrivateToggle = 34,               // dbuicontrols form 32 index 34 id 3352 'AG Private Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 31 'Available
// Rooms (Sierra) Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEAvailableRoomsControl {
    kAvailableRoomsPlayersNameLabel = 2,  // dbuicontrols form 31 index 2 id 2963 'AR Player 's Name Label' (type 15); site form via UITextLabel* theUserLabel = (UITextLabel
    kAvailableRoomsAvailRoomsListBox = 6,  // dbuicontrols form 31 index 6 id 2967 'AR Avail. Rooms List Box' (type 1018); site form via EUICWonNetRoomsBox*>(gUIManager->GetForm
    kAvailableRoomsJoinRoomButton = 7,   // dbuicontrols form 31 index 7 id 2968 'AR Join Room Button' (type 28); site form via UIEWNSetTriggerEvent(new EEUIEWNARJoinRo
    kAvailableRoomsBackButton = 8,       // dbuicontrols form 31 index 8 id 2969 'AR Back Button' (type 28); site form via EEUIEWNSetTriggerEvent(new EEUIEWNARBack
    kAvailableRoomsUpdateMyAccount = 9,  // dbuicontrols form 31 index 9 id 3269 'AR Update My Account' (type 1); site form via iggerEvent(new EEUIEWNARClickUpdateAccou
    kAvailableRoomsTotalGamesNumber = 14,  // dbuicontrols form 31 index 14 id 3293 'AR Total Games Number' (type 20); site form via ((UITextLabel*)gUIManager->GetFormByID(0
    kAvailableRoomsServersNameLabel = 16,  // dbuicontrols form 31 index 16 id 3295 'AR Server's Name Label' (type 15); site form via ((UITextLabel*)gUIManager->GetFormByID(0
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 24 'Campaign
// Editor Form'; names from the dbuicontrols control names (form prefix dropped).
enum EECampaignEditorControl {
    kCampaignEditorBackgroundImageComboBox = 44,       // dbuicontrols form 24 index 44 id 3106 'CE Background Image Combo Box' (type 32); site form via UIFileComboBox* theList = (UIFileComboBo
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 18 'Chat In Game
// pop-up dialog'; names from the dbuicontrols control names (form prefix dropped).
enum EEChatControl {
    kChatTextEditBox = 1,      // dbuicontrols form 18 index 1 id 414 'Chat Text Edit Box' (type 8); site form via inForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 29 'Chat Log
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEChatLogControl {
    kChatLogChatLogTextBox = 1,  // dbuicontrols form 29 index 1 id 3069 'Chat Log Text Box' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
};

// UI database form ids (dbuiforms form id field +0xcc: UIManager::GetFormByID, the controls' form field); names from
// the form names.
enum EEFormID {
    kFormTestForm = 2,               // dbuiforms form id 2 'Test form'
    kFormSaveGame = 3,               // dbuiforms form id 3 'In Game Scenario & Save Game Screen'
    kFormLoadGame = 4,               // dbuiforms form id 4 'In Game Scenario & Load Game Screen'
    kFormGameSettings = 5,           // dbuiforms form id 5 'Game Settings Screen'
    kFormSinglePlayer = 6,           // dbuiforms form id 6 'Single Player Menu'
    kFormCivBuilder = 7,             // dbuiforms form id 7 'CivBuilder Main Form'
    kFormPlayCampaign = 8,           // dbuiforms form id 8 'Play Campaign Form'
    kFormGameTools = 9,              // dbuiforms form id 9 'Game Tools Menu'
    kFormChooseCampaign = 10,        // dbuiforms form id 10 'Choose Campaign Screen'
    kFormMainGame = 11,              // dbuiforms form id 11 'Scott Main Game Form'
    kFormMainMenu = 12,              // dbuiforms form id 12 'Main Menu Screen'
    kFormSinglePlayerLoadGame = 13,  // dbuiforms form id 13 'Single Player Load Game Screen'
    kFormCampaign = 14,              // dbuiforms form id 14 'Campaign Screen'
    kFormCivBuilderNewCiv = 15,      // dbuiforms form id 15 'CivBuilder New Civilization Form'
    kFormCivBuilderSaveCiv = 16,     // dbuiforms form id 16 'CivBuilder Save Civilization Form'
    kFormCivBuilderLoadCiv = 17,     // dbuiforms form id 17 'CivBuilder Load Civilization Form'
    kFormChat = 18,                  // dbuiforms form id 18 'Chat In Game pop-up dialog'
    kFormScenarioIntro = 19,         // dbuiforms form id 19 'Scenario Intro Screen(Briefing Screen)'
    kFormScenarioLoad = 20,          // dbuiforms form id 20 'Scenario Load Screen'
    kFormWonNetStats = 21,           // dbuiforms form id 21 'WON.net Stat Screen'
    kFormDebugSound = 22,            // dbuiforms form id 22 'DS Debug Sound Screen'
    kFormSplash = 23,                // dbuiforms form id 23 'Splash Screen'
    kFormCampaignEditor = 24,        // dbuiforms form id 24 'Campaign Editor Form'
    kFormPostGameSummary = 25,       // dbuiforms form id 25 'Post Game Summary Screen'
    kFormLobby = 26,                 // dbuiforms form id 26 'Multi/Single Setup Screen(Lobby)'
    kFormDiplomacy = 27,             // dbuiforms form id 27 'Diplomacy Screen'
    kFormGameOptions = 28,           // dbuiforms form id 28 'In-game Options form'
    kFormChatLog = 29,               // dbuiforms form id 29 'Chat Log Form'
    kFormMultiplayer = 30,           // dbuiforms form id 30 'Multiplayer Menu Form'
    kFormAvailableRooms = 31,        // dbuiforms form id 31 'Available Rooms (Sierra) Form'
    kFormAvailableGames = 32,        // dbuiforms form id 32 'Available Games (Sierra) Form'
    kFormCampaignScenarioSelect = 33,  // dbuiforms form id 33 'Campaign Scenario Select Form'
    kFormChooseCustomCampaign = 34,  // dbuiforms form id 34 'Choose Custom Campaign Form'
    kFormPredefinedCiv = 35,         // dbuiforms form id 35 'Predefined Civ Form'
    kFormCreateSierraAccount = 36,   // dbuiforms form id 36 'Create Sierra Account Form'
    kFormUpdateSierraAccount = 37,   // dbuiforms form id 37 'Update Sierra Account Form'
    kFormSelectServer = 38,          // dbuiforms form id 38 'Select Server Form'
    kFormDemoOutro = 39,             // dbuiforms form id 39 'Demo Outro Form'
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 5 'Game Settings
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEGameSettingsControl {
    // port: not in the database (EEUIManager's ctor adds it: UIControlsTable::AddControlLike, a copy of the video
    // card combo box; strings kTextureResolutionOriginalString..)
    kGameSettingsTextureResolutionComboBox = 111,
    // port: the modern display and audio settings (EEUIManager.cpp AddControlLike; ee_screens settings_form.rs)
    kGameSettingsQualityComboBox = 123,            // port: the graphics quality preset (replaces the 5 radio buttons)
};

// port: the texture resolution combo box (kGameSettingsTextureResolutionComboBox): its control id (past the
// database's) and its strings (Language.rc)
enum EETextureResolutionControl {
    kTextureResolutionControlID = 90001,
    kTextureResolutionOriginalString = 33100,   // "Textures: Original", then 2x (33101) and 4x (33102)
    kTextureResolutionHelpString = 33103,
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 4 'In Game
// Scenario & Load Game Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EELoadGameControl {
    kLoadGameLoadButton = 1,     // dbuicontrols form 4 index 1 id 143 'Load Button' (type 1); site form via inForm
    kLoadGameLoadFileListBox = 4,  // dbuicontrols form 4 index 4 id 141 'Load File List Box' (type 22); site form via UIFileListBox* theList = (UIFileListBox*
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 26 'Multi/Single
// Setup Screen(Lobby)'; names from the dbuicontrols control names (form prefix dropped).
enum EELobbyControl {
    kLobbyChatPreviousMessages = 8,      // dbuicontrols form 26 index 8 id 2509 'M-SS Chat Previous Messages' (type 16); site form via Chat = (UITextLabel*)gUIManager->GetForm
    // port: not in the database (EEUIManager's ctor adds them: UIControlsTable::AddControlLike). The computer slots'
    // engine (ee/EECPEngine.h; docs/AI_LUA_PLAN.md S0) in the ping column, which a computer row leaves empty: its
    // header (a copy of the ping label, shown in place of it), per slot a combo box (a copy of the citizens combo,
    // the host's before ready) and a label (a copy of the citizens label, everyone else)
    kLobbyEngineHeader = 274,
    kLobbyEngineCombo = 275,             // + slot (16)
    kLobbyEngineLabel = 291,             // + slot (16)
};

// port: the lobby's engine controls' ids (past the database's and the settings' 90001..) and strings (Language.rc)
enum EELobbyEngineControl {
    kLobbyEngineControlID = 90100,      // the header; + 1 + slot the combo boxes, + 17 + slot the labels
    kLobbyEngineClassicString = 33170,  // "Classic", then "Random": the combo's first lines (ee/EECPEngine.h
    kLobbyEngineRandomString = 33171,   // EECPEngineChoice); the AI manifest's names follow
    kLobbyEngineHeaderString = 33173,   // "AI" (single player)
    kLobbyEngineHelpString = 33175,
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 11 'Scott Main
// Game Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEMainGameControl {
    kMainGameIngameOptions = 3,           // dbuicontrols form 11 index 3 id 250 'SRMIngame Options*' (type 27); site form via ((UIBlinkingButton*)gUIManager->mMainGam
    kMainGameCivSelectorBuilder = 5,      // dbuicontrols form 11 index 5 id 252 'SRMCiv Selector/Builder*' (type 27); site form via UIControl* theControl = theForm
    kMainGameWinLose = 13,                // dbuicontrols form 11 index 13 id 246 'SRMWin/Lose*' (type 15); site form via this
    kMainGamePopulation = 24,             // dbuicontrols form 11 index 24 id 263 'SRMPopulation*' (type 20); site form via this
    kMainGameTitle = 25,                  // dbuicontrols form 11 index 25 id 264 'SRMTitle*' (type 15); site form via this
    kMainGameMiniMap = 26,                // dbuicontrols form 11 index 26 id 265 'SRMMiniMap*' (type 1001); site form via this
    kMainGameErrorLog = 27,               // dbuicontrols form 11 index 27 id 266 'SRMError Log*' (type 1003); site form via this
    kMainGameSelectUnits = 28,            // dbuicontrols form 11 index 28 id 267 'SRMSelect Units*' (type 1007); site form via ((EEUICSelectUnits*)theForm
    kMainGameCommandUnits = 29,           // dbuicontrols form 11 index 29 id 268 'SRMCommand Units*' (type 1008); site form via ((EEUICCommandUnits*)theForm
    kMainGameStatusBarLeftStatus = 30,    // dbuicontrols form 11 index 30 id 269 'SRMSB: Left Status*' (type 1006); site form via mStatusBox->mC84 = inForm
    kMainGameStatusBarMiddleStatus = 31,  // dbuicontrols form 11 index 31 id 270 'SRMSB: Middle Status*' (type 1006); site form via mStatusBox->mC88 = inForm
    kMainGameArmorDisplayTitle = 41,      // dbuicontrols form 11 index 41 id 311 'SRMArmor Display Title*' (type 15); site form via mStatusBox->mC98 = inForm
    kMainGameStatusBarPortrait = 42,      // dbuicontrols form 11 index 42 id 271 'SRMSB: Portrait*' (type 1009); site form via mStatusBox->mC8C = inForm
    kMainGameStatusBarCivUnitName = 43,   // dbuicontrols form 11 index 43 id 272 'SRMSB: Civ Unit Name*' (type 15); site form via mStatusBox->mC90 = inForm
    kMainGameSmallStatusBar = 50,         // dbuicontrols form 11 index 50 id 273 'SRM Small Status Bar' (type 1006); site form via mStatusBox->mCAC = inForm
    kMainGameMiddleStatusBar = 51,        // dbuicontrols form 11 index 51 id 274 'SRM Middle Status Bar' (type 1006); site form via mStatusBox->mCA8 = inForm
    kMainGameActionButton1 = 78,          // dbuicontrols form 11 index 78 id 321 'SRMAction Button 1*' (type 1000); site form via ((EEUIFMGStatusButton*)mOwner
    kMainGameConversions = 95,            // dbuicontrols form 11 index 95 id 297 'SRMConversions*' (type 15); site form via if (((UITextLabel*)mOwner
    kMainGameWondersToWin = 96,           // dbuicontrols form 11 index 96 id 298 'SRMWonders To Win*' (type 15); site form via ((UITextLabel*)mOwner
    kMainGameWondersAvailable = 97,       // dbuicontrols form 11 index 97 id 299 'SRMWonders Available*' (type 15); site form via ((UITextLabel*)mOwner
    kMainGameTempObjectList = 98,         // dbuicontrols form 11 index 98 id 300 'SRMTemp Object List*' (type 4); site form via ((UIComboBox*)mForm
    kMainGameTempPlayerList = 99,         // dbuicontrols form 11 index 99 id 301 'SRMTemp Player List*' (type 4); site form via ((UIComboBox*)mForm
    kMainGameStatusBarCivName = 100,      // dbuicontrols form 11 index 100 id 330 'SRMSB: Civ Name*' (type 15); site form via mStatusBox->mC94 = inForm
    kMainGameChatLine1 = 101,             // dbuicontrols form 11 index 101 id 429 'SRMChat Line 1*' (type 1010); site form via this
    kMainGameChatLine7 = 107,             // dbuicontrols form 11 index 107 id 435 'SRMChat Line 7*' (type 1010); site form via this
    kMainGameGameClock = 108,             // dbuicontrols form 11 index 108 id 797 'SRM Game Clock' (type 20); site form via this
    kMainGameGameInfoBriefing = 109,      // dbuicontrols form 11 index 109 id 578 'SRMGame Info* (Briefing)' (type 27); site form via this
    kMainGameTimedFighterRally = 208,     // dbuicontrols form 11 index 208 id 829 'SRMTimedFighterRally' (type 1); site form via mOwner
    kMainGameTimedBomberRally = 209,      // dbuicontrols form 11 index 209 id 830 'SRMTimedBomberRally' (type 1); site form via mOwner
    kMainGameTimedAtomicBomberRally = 210,  // dbuicontrols form 11 index 210 id 831 'SRMTimedAtomicBomberRally' (type 1); site form via mOwner
    kMainGameDialogMessage = 211,         // dbuicontrols form 11 index 211 id 832 'SRMDialogMessage' (type 26); site form via this
    kMainGameUnitIconBar = 222,           // dbuicontrols form 11 index 222 id 891 'SRMUnit Icon Bar' (type 1014); site form via UIControl* theIconBar = mOwner
    kMainGameDialogPortrait = 223,        // dbuicontrols form 11 index 223 id 892 'SRMDialog Portrait' (type 0); site form via this
    kMainGameBehaviorStandGround = 228,   // dbuicontrols form 11 index 228 id 3005 'SRM Behavior Stand Ground' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameBehaviorAggressive = 229,    // dbuicontrols form 11 index 229 id 3004 'SRM Behavior Aggressive' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameBehaviorScout = 232,         // dbuicontrols form 11 index 232 id 3006 'SRM Behavior Scout' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameBehaviorGuard = 233,         // dbuicontrols form 11 index 233 id 3007 'SRM Behavior Guard' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameLargeCivBuilderAlert = 234,  // dbuicontrols form 11 index 234 id 3066 'SRM Large Civ Builder Alert*' (type 27); site form via _cast<UIBlinkingButton*>(gUIManager->mMa
    kMainGameLargeScenBriefingAlert = 235,  // dbuicontrols form 11 index 235 id 3067 'SRM Large Scen Briefing Alert*' (type 27); site form via this
    kMainGameFormationShortLine = 236,    // dbuicontrols form 11 index 236 id 3108 'SRM Formation Short Line' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationLongLine = 237,     // dbuicontrols form 11 index 237 id 3109 'SRM Formation Long Line' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationLooseLine = 238,    // dbuicontrols form 11 index 238 id 3110 'SRM Formation Loose Line' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationStaggeredLine = 239,  // dbuicontrols form 11 index 239 id 3111 'SRM Formation Staggered Line' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationDeepWedge = 240,    // dbuicontrols form 11 index 240 id 3112 'SRM Formation Deep Wedge' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationShallowWedge = 241,  // dbuicontrols form 11 index 241 id 3113 'SRM Formation Shallow Wedge' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationInwardCircle = 242,  // dbuicontrols form 11 index 242 id 3114 'SRM Formation Inward Circle' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameFormationOutwardCircle = 243,  // dbuicontrols form 11 index 243 id 3115 'SRM Formation Outward Circle' (type 1); site form via NormalBitmap(((UIButton*)gUIManager->mMa
    kMainGameCancelUIMode = 244,          // dbuicontrols form 11 index 244 id 3226 'SRM Cancel UI Mode' (type 31); site form via theForm
    kMainGamePreviousMessages = 253,      // dbuicontrols form 11 index 253 id 3270 'SRMPrevious Messages*' (type 1); site form via this
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 30 'Multiplayer
// Menu Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEMultiplayerControl {
    kMultiplayerMultiplayerTitleLabel = 0,       // dbuicontrols form 30 index 0 id 2926 'MPS Multiplayer Title Label' (type 15); site form via inForm
    kMultiplayerPlayerNameLabel = 1,             // dbuicontrols form 30 index 1 id 2927 'MPS Player Name Label' (type 15); site form via inForm
    kMultiplayerPlayerNameEditBox = 2,           // dbuicontrols form 30 index 2 id 2928 'MPS Player Name Edit Box' (type 8); site form via inForm
    kMultiplayerJoinNetworkGameTab = 3,          // dbuicontrols form 30 index 3 id 2929 'MPS Join Network Game Tab' (type 12); site form via inForm
    kMultiplayerJoinByIPTab = 4,                 // dbuicontrols form 30 index 4 id 2930 'MPS Join By IP Tab' (type 12); site form via inForm
    kMultiplayerHostNetworkGameTab = 5,          // dbuicontrols form 30 index 5 id 2931 'MPS Host Network Game Tab' (type 12); site form via inForm
    kMultiplayerPlayOnSierraTab = 6,             // dbuicontrols form 30 index 6 id 2932 'MPS Play on Sierra Tab' (type 12); site form via inForm
    kMultiplayerAvailGamesLabel = 7,             // dbuicontrols form 30 index 7 id 2933 'MPS Avail. Games Label' (type 15); site form via inForm
    kMultiplayerAvailGamesListBox = 9,           // dbuicontrols form 30 index 9 id 2935 'MPS Avail. Games List Box' (type 9); site form via inForm
    kMultiplayerJoinHostButton = 10,             // dbuicontrols form 30 index 10 id 2936 'MPS Join/Host Button' (type 28); site form via inForm
    kMultiplayerMainMenuButton = 11,             // dbuicontrols form 30 index 11 id 2937 'MPS Main Menu Button' (type 28); site form via inForm
    kMultiplayerHostGameNameEditBox = 13,        // dbuicontrols form 30 index 13 id 2939 'MPS Host Game Name Edit Box' (type 8); site form via inForm
    kMultiplayerShowIPButton = 16,               // dbuicontrols form 30 index 16 id 2942 'MPS Show IP Button' (type 1); site form via inForm
    kMultiplayerSierraUserIDEditBox = 20,        // dbuicontrols form 30 index 20 id 2946 'MPS Sierra User ID Edit Box' (type 8); site form via inForm
    kMultiplayerSierraPasswordEditBox = 22,      // dbuicontrols form 30 index 22 id 2948 'MPS Sierra Password Edit Box' (type 8); site form via inForm
    kMultiplayerCreateSierraAccountButton = 23,  // dbuicontrols form 30 index 23 id 2949 'MPS Create Sierra Account Button' (type 1); site form via inForm
    kMultiplayerSierraEmailMyPasswordButton = 26,  // dbuicontrols form 30 index 26 id 3237 'MPS Sierra Email My Password Button' (type 1); site form via inForm
    kMultiplayerMultiPlayerOOSWarningLabel = 27,  // dbuicontrols form 30 index 27 id 3288 'MPS MultiPlayer OOS Warning Label' (type 16); site form via inForm
    kMultiplayerRememberPasswordCheckbox = 28,   // dbuicontrols form 30 index 28 id 3368 'MPS Remember Password Checkbox' (type 2); site form via if (((UICheckBox*)theForm
    kMultiplayerMessageOfTheDayTextLabel = 29,   // dbuicontrols form 30 index 29 id 3369 'MPS Message Of the Day Text Label' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 25 'Post Game
// Summary Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEPostGameSummaryControl {
    kPostGameLineGraph = 0,   // dbuicontrols form 25 index 0 id 916 'PGS Line Graph' (type 1016); site form via ineGraph* theLineGraph = (EEUIPGSLineGra
    kPostGameGoldStar = 790,  // dbuicontrols form 25 index 790 id 1691 'PGS Gold Star' (type 0); site form via tmap* theTemplate = dynamic_cast<UIBitma
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 38 'Select Server
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EESelectServerControl {
    kSelectServerListBox = 1,     // dbuicontrols form 38 index 1 id 3266 'Serv  List Box' (type 9); site form via String>* theList = (UIListBox<UWideStrin
    kSelectServerOKButton = 2,    // dbuicontrols form 38 index 2 id 3267 'Serv OK Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSOk,
    kSelectServerCancelButton = 3,  // dbuicontrols form 38 index 3 id 3268 'Serv Cancel Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSCanc
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 13 'Single Player
// Load Game Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EESinglePlayerLoadGameControl {
    kSPLoadGameFileListBox = 4,  // dbuicontrols form 13 index 4 id 394 'SP_Load File List Box' (type 22); site form via nt = new EEUIESLEDeleteFile((UIFileListB
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 23 'Splash
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EESplashControl {
    kSplashLoadLabel = 3,   // dbuicontrols form 23 index 3 id 803 'SS Load Label' (type 15); site form via ((UITextLabel*)gUIManager->GetFormByID(0
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 37 'Update Sierra
// Account Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEUpdateSierraAccountControl {
    kUpdateAccountNewPasswordEditBox = 2,    // dbuicontrols form 37 index 2 id 3256 'USA New Password Edit Box' (type 8); site form via thePassword = ((UIEditBox*)inForm
    kUpdateAccountConfirmPasswordEditBox = 4,  // dbuicontrols form 37 index 4 id 3258 'USA Confirm Password Edit Box' (type 8); site form via theConfirmation = ((UIEditBox*)inForm
    kUpdateAccountEnterEmailEditBox = 6,     // dbuicontrols form 37 index 6 id 3260 'USA Enter Email Edit Box' (type 8); site form via theNewPassword = ((UIEditBox*)inForm
    kUpdateAccountSendMeEmailCheckbox = 7,   // dbuicontrols form 37 index 7 id 3261 'USA Send Me Email Checkbox' (type 2); site form via bool theFlag = ((UICheckBox*)inForm
    kUpdateAccountUpdateButton = 8,          // dbuicontrols form 37 index 8 id 3262 'USA Update Button' (type 1); site form via EUIEWNSetTriggerEvent(new EEUIEWNMUASubm
    kUpdateAccountDontUpdateButton = 9,      // dbuicontrols form 37 index 9 id 3263 'USA Don't Update Button' (type 1); site form via EUIEWNSetTriggerEvent(new EEUIEWNMUACanc
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 21 'WON.net Stat
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEWonNetStatsControl {
    kWonStatsStatBox = 2,     // dbuicontrols form 21 index 2 id 607 'WNS Stat Box' (type 1013); site form via ((EEUICWonNetStatBox*)inForm
    kWonStatsMyStatsLine = 22,  // dbuicontrols form 21 index 22 id 824 'WNS My Stats Line' (type 1013); site form via ((EEUICWonNetStatBox*)inForm
};

#endif
