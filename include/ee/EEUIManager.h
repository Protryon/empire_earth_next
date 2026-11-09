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

class EEUIManager : public UIManager {
public:
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
    kCampaignEditorCampaignNameEditBox = 2,            // dbuicontrols form 24 index 2 id 895 'CE Campaign Name Edit Box' (type 8); site form via || UWideString(((UIEditBox*)inForm
    kCampaignEditorAuthorNameEditBox = 3,              // dbuicontrols form 24 index 3 id 896 'CE Author Name Edit Box' (type 8); site form via || UWideString(((UIEditBox*)inForm
    kCampaignEditorAuthorsEmailEditBox = 7,            // dbuicontrols form 24 index 7 id 900 'CE Author's Email Edit Box' (type 8); site form via || UWideString(((UIEditBox*)inForm
    kCampaignEditorCampaignFileListBox = 10,           // dbuicontrols form 24 index 10 id 903 'CE Campaign File List Box' (type 9); site form via ist = dynamic_cast<UIListBox<UWideString
    kCampaignEditorCampaignDescripEditBox = 11,        // dbuicontrols form 24 index 11 id 904 'CE Campaign Descrip. Edit Box' (type 7); site form via IEditBox* theBox = dynamic_cast<UIEditBo
    kCampaignEditorScensAvailListBox = 14,             // dbuicontrols form 24 index 14 id 907 'CE Scens Avail. List Box' (type 9); site form via ist = dynamic_cast<UIListBox<UWideString
    kCampaignEditorAssetsInCampaignListBox = 15,       // dbuicontrols form 24 index 15 id 3008 'CE Assets in Campaign List Box' (type 9); site form via ist = dynamic_cast<UIListBox<UWideString
    kCampaignEditorDisabledAuthorNameEditBox = 25,     // dbuicontrols form 24 index 25 id 2914 'CE Disabled Author Name Edit Box' (type 15); site form via abel* theLabel = dynamic_cast<UITextLabe
    kCampaignEditorDisabledAuthorsEmailEditBox = 26,   // dbuicontrols form 24 index 26 id 2915 'CE Disabled Author's Email Edit Box' (type 15); site form via abel* theLabel = dynamic_cast<UITextLabe
    kCampaignEditorDisabledCampaignDescripEditBox = 27,  // dbuicontrols form 24 index 27 id 2916 'CE Disabled Campaign Descrip. Edit Box' (type 16); site form via abel* theLabel = dynamic_cast<UITextLabe
    kCampaignEditorCampaignPasswordEditBox = 35,       // dbuicontrols form 24 index 35 id 2924 'CE Campaign Password Edit Box' (type 8); site form via || UWideString(((UIEditBox*)inForm
    kCampaignEditorScensInCampListBox = 40,            // dbuicontrols form 24 index 40 id 908 'CE Scens in Camp. List Box' (type 9); site form via ist = dynamic_cast<UIListBox<UWideString
    kCampaignEditorBackgroundImageComboBox = 44,       // dbuicontrols form 24 index 44 id 3106 'CE Background Image Combo Box' (type 32); site form via UIFileComboBox* theList = (UIFileComboBo
    kCampaignEditorDisabledBGImageCombo = 45,          // dbuicontrols form 24 index 45 id 3107 'CE Disabled BG Image Combo' (type 15); site form via abel* theLabel = dynamic_cast<UITextLabe
    kCampaignEditorCivNameEditBox = 47,                // dbuicontrols form 24 index 47 id 3234 'CE Civ Name Edit Box' (type 8); site form via IEditBox* theBox = dynamic_cast<UIEditBo
    kCampaignEditorDisabledCivNameEditBox = 48,        // dbuicontrols form 24 index 48 id 3235 'CE Disabled Civ Name Edit Box' (type 15); site form via abel* theLabel = dynamic_cast<UITextLabe
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 33 'Campaign
// Scenario Select Form'; names from the dbuicontrols control names (form prefix dropped).
enum EECampaignScenarioSelectControl {
    kCampaignScenarioSelectScenarioListBox = 2,      // dbuicontrols form 33 index 2 id 2953 'CS Select Scenario List Box' (type 9); site form via theForm
    kCampaignScenarioScenIntroTextMultiLineLabel = 4,  // dbuicontrols form 33 index 4 id 2955 'CS Scen. Intro Text multi-line Label' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
    kCampaignScenarioDifficultyLabel = 5,            // dbuicontrols form 33 index 5 id 2956 'CS Difficulty Label' (type 15); site form via theForm
    kCampaignScenarioDifficultyComboBox = 6,         // dbuicontrols form 33 index 6 id 2957 'CS Difficulty Combo Box' (type 4); site form via theForm
    kCampaignScenarioStartGameButton = 7,            // dbuicontrols form 33 index 7 id 2958 'CS Start Game Button' (type 28); site form via theForm
    kCampaignScenarioBackButton = 8,                 // dbuicontrols form 33 index 8 id 2959 'CS Back Button' (type 28); site form via theForm
    kCampaignScenarioCustomCampaignSelectListBox = 9,  // dbuicontrols form 33 index 9 id 2960 'CS Custom Campaign Select List Box' (type 9); site form via theForm
    kCampaignScenarioCampaignProgressLabel = 10,     // dbuicontrols form 33 index 10 id 3118 'CS Campaign Progress Label' (type 15); site form via UITextLabel* theLabel = (UITextLabel*)th
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 18 'Chat In Game
// pop-up dialog'; names from the dbuicontrols control names (form prefix dropped).
enum EEChatControl {
    kChatTextEditBox = 1,      // dbuicontrols form 18 index 1 id 414 'Chat Text Edit Box' (type 8); site form via inForm
    kChatAllyRadioButton = 2,  // dbuicontrols form 18 index 2 id 415 'Chat Ally Radio Button' (type 11); site form via ((UIRadioButton*)inForm
    kChatAllRadioButton = 3,   // dbuicontrols form 18 index 3 id 416 'Chat All Radio Button' (type 11); site form via if (((UIRadioButton*)inForm
    kChatEnemyRadioButton = 4,  // dbuicontrols form 18 index 4 id 417 'Chat Enemy Radio Button' (type 11); site form via else if (((UIRadioButton*)inForm
    kChatCustomRadioButton = 5,  // dbuicontrols form 18 index 5 id 418 'Chat Custom Radio Button' (type 11); site form via ((UIRadioButton*)inForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 29 'Chat Log
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEChatLogControl {
    kChatLogChatLogTextBox = 1,  // dbuicontrols form 29 index 1 id 3069 'Chat Log Text Box' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 34 'Choose Custom
// Campaign Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEChooseCustomCampaignControl {
    kChooseCampaignDescripTextBox = 1,  // dbuicontrols form 34 index 1 id 3121 'Choose Custom Camp (descrip) Text Box' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
    kChooseCampaignCampListBox = 2,   // dbuicontrols form 34 index 2 id 3122 'Choose Custom Camp (camp) List Box' (type 9); site form via theForm
    kChooseCampaignCancelButton = 3,  // dbuicontrols form 34 index 3 id 3123 'Choose Custom Camp Cancel Button' (type 1); site form via theForm
    kChooseCampaignOKButton = 4,      // dbuicontrols form 34 index 4 id 3124 'Choose Custom Camp OK Button' (type 1); site form via theForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 7 'CivBuilder Main
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EECivBuilderControl {
    kCivBuilderLoadCivButton = 2,          // dbuicontrols form 7 index 2 id 198 'CB Load Civ Button' (type 1); site form via theForm
    kCivBuilderCivSaveButton = 15,         // dbuicontrols form 7 index 15 id 211 'CB Civ Save Button' (type 1); site form via theForm
    kCivBuilderCivExitButton = 16,         // dbuicontrols form 7 index 16 id 212 'CB Civ Exit Button' (type 1); site form via theForm
    kCivBuilderCivAcceptChangesButton = 17,  // dbuicontrols form 7 index 17 id 448 'CB Civ Accept Changes Button' (type 1); site form via theForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 27 'Diplomacy
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEDiplomacyControl {
    kDiplomacyOKButton = 184,           // dbuicontrols form 27 index 184 id 2887 'D OK Button' (type 1); site form via Button* theButton = dynamic_cast<UIButto
    kDiplomacyClearTributesButton = 185,  // dbuicontrols form 27 index 185 id 2888 'D Clear Tributes Button' (type 1); site form via theButton = dynamic_cast<UIButton*>(inFo
    kDiplomacyCancelButton = 186,       // dbuicontrols form 27 index 186 id 2889 'D Cancel Button' (type 1); site form via theButton = dynamic_cast<UIButton*>(inFo
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

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 28 'In-game
// Options form'; names from the dbuicontrols control names (form prefix dropped).
enum EEGameOptionsControl {
    kGameOptionsReturnToGameButton = 0,  // dbuicontrols form 28 index 0 id 2971 'IGO Return to Game Button' (type 1); site form via inForm
    kGameOptionsResignButton = 1,       // dbuicontrols form 28 index 1 id 2972 'IGO Resign Button' (type 1); site form via inForm
    kGameOptionsPauseUnpauseButton = 2,  // dbuicontrols form 28 index 2 id 2973 'IGO Pause-unpause Button' (type 1); site form via UIButton* theButton = (UIButton*)inForm
    kGameOptionsRestartGameButton = 3,  // dbuicontrols form 28 index 3 id 2974 'IGO Restart Game Button' (type 1); site form via inForm
    kGameOptionsPrevMessagesButton = 4,  // dbuicontrols form 28 index 4 id 2975 'IGO Prev. Messages Button' (type 1); site form via inForm
    kGameOptionsPlaySavedGameButton = 5,  // dbuicontrols form 28 index 5 id 2976 'IGO Play Saved Game Button' (type 1); site form via inForm
    kGameOptionsPlayScenarioButton = 6,  // dbuicontrols form 28 index 6 id 2977 'IGO Play Scenario Button' (type 1); site form via inForm
    kGameOptionsSaveGameButton = 7,     // dbuicontrols form 28 index 7 id 2978 'IGO Save Game Button' (type 1); site form via inForm
    kGameOptionsGameBriefingButton = 8,  // dbuicontrols form 28 index 8 id 2979 'IGO Game Briefing Button' (type 1); site form via inForm
    kGameOptionsGameSettingsButton = 9,  // dbuicontrols form 28 index 9 id 2980 'IGO Game Settings Button' (type 1); site form via inForm
    kGameOptionsExitEEButton = 10,      // dbuicontrols form 28 index 10 id 2981 'IGO Exit EE Button' (type 1); site form via inForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 5 'Game Settings
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEGameSettingsControl {
    kGameSettingsVideoCardLabel = 5,               // dbuicontrols form 5 index 5 id 150 'GS Video Card Label' (type 15); site form via inForm
    kGameSettingsVideoCardComboBox = 6,            // dbuicontrols form 5 index 6 id 151 'GS Video Card Combo Box' (type 4); site form via UIComboBox* theScreenBox = (UIComboBox*)
    kGameSettings3DRenderingSysLabel = 7,          // dbuicontrols form 5 index 7 id 152 'GS 3D Rendering Sys Label' (type 15); site form via inForm
    kGameSettingsRenderingSysListBox = 8,          // dbuicontrols form 5 index 8 id 153 'GS Rendering Sys List Box' (type 9); site form via ILBString* theRasterizerList = (UILBStri
    kGameSettingsScreenResLabel = 9,               // dbuicontrols form 5 index 9 id 154 'GS Screen Res Label' (type 15); site form via inForm
    kGameSettingsScreenResListBox = 10,            // dbuicontrols form 5 index 10 id 155 'GS Screen Res List Box' (type 9); site form via ILBString* theResolutionList = (UILBStri
    kGameSettingsShowBloodCheckBox = 13,           // dbuicontrols form 5 index 13 id 158 'GS Show Blood Check Box' (type 2); site form via UICheckBox* theCheck13 = (UICheckBox*)in
    kGameSettingsOKButton = 18,                    // dbuicontrols form 5 index 18 id 163 'GS OK Button' (type 1); site form via inForm
    kGameSettingsMusicVolumeSlider = 25,           // dbuicontrols form 5 index 25 id 174 'GS Music Volume Slider' (type 17); site form via unsigned long theValue = ((UISlider*)inF
    kGameSettingsMusicQualHighRadio = 27,          // dbuicontrols form 5 index 27 id 176 'GS Music Qual High Radio' (type 11); site form via IRadioButton* theReverbOn = (UIRadioButt
    kGameSettingsMusicQualLowRadio = 28,           // dbuicontrols form 5 index 28 id 177 'GS Music Qual Low Radio' (type 11); site form via RadioButton* theReverbOff = (UIRadioButt
    kGameSettingsMusicOffRadioButton = 29,         // dbuicontrols form 5 index 29 id 178 'GS Music Off Radio Button' (type 11); site form via IRadioButton* theMusicOff = (UIRadioButt
    kGameSettingsSoundVolumeSlider = 35,           // dbuicontrols form 5 index 35 id 183 'GS Sound Volume Slider' (type 17); site form via UISlider* theSoundVolume = (UISlider*)in
    kGameSettingsMaxNumSoundsSlider = 39,          // dbuicontrols form 5 index 39 id 187 'GS Max # Sounds Slider' (type 17); site form via UISlider* theVoiceVolume = (UISlider*)in
    kGameSettingsSideScrollSpeedSlider = 49,       // dbuicontrols form 5 index 49 id 218 'GS Side Scroll Speed Slider' (type 17); site form via UISlider* theSideScrollSpeed = (UISlider
    kGameSettingsRightHandMouseRadio = 53,         // dbuicontrols form 5 index 53 id 222 'GS Right-Hand Mouse Radio' (type 11); site form via dioButton* theRightHanded = (UIRadioButt
    kGameSettingsLeftHandMouseRadio = 54,          // dbuicontrols form 5 index 54 id 223 'GS Left-Hand Mouse Radio' (type 11); site form via adioButton* theLeftHanded = (UIRadioButt
    kGameSettingsMouseSensitivitySlider = 56,      // dbuicontrols form 5 index 56 id 225 'GS Mouse Sensitivity Slider' (type 17); site form via UISlider* theMouseSensitivity = (UISlide
    kGameSettingsGrabScrollSpeedSlider = 62,       // dbuicontrols form 5 index 62 id 444 'GS Grab Scroll Speed Slider' (type 17); site form via UISlider* theGrabScrollSpeed = (UISlider
    kGameSettingsAmbientSoundsCheckBox = 66,       // dbuicontrols form 5 index 66 id 804 'GS Ambient Sounds Check Box' (type 2); site form via UICheckBox* theAmbientSounds = (UICheckB
    kGameSettingsGQRadio1Lowest = 67,              // dbuicontrols form 5 index 67 id 3011 'GS GQ radio 1 Lowest' (type 11); site form via IRadioButton* theQuality0 = (UIRadioButt
    kGameSettingsGQRadio2 = 68,                    // dbuicontrols form 5 index 68 id 3012 'GS GQ radio 2' (type 11); site form via IRadioButton* theQuality1 = (UIRadioButt
    kGameSettingsGQRadio3Middle = 69,              // dbuicontrols form 5 index 69 id 3013 'GS GQ radio 3 Middle' (type 11); site form via IRadioButton* theQuality2 = (UIRadioButt
    kGameSettingsGQRadio4 = 70,                    // dbuicontrols form 5 index 70 id 3014 'GS GQ radio 4' (type 11); site form via IRadioButton* theQuality3 = (UIRadioButt
    kGameSettingsGQRadio5Highest = 71,             // dbuicontrols form 5 index 71 id 3015 'GS GQ radio 5 Highest' (type 11); site form via IRadioButton* theQuality4 = (UIRadioButt
    kGameSettingsGQRadio6Custom = 72,              // dbuicontrols form 5 index 72 id 3016 'GS GQ radio 6 Custom' (type 11); site form via UIRadioButton* theCustom = (UIRadioButto
    kGameSettingsModelDetailRadioHigh = 75,        // dbuicontrols form 5 index 75 id 3019 'GS Model Detail Radio High' (type 11); site form via UIRadioButton* theRadio75 = (UIRadioButt
    kGameSettingsModelDetailRadioMed = 76,         // dbuicontrols form 5 index 76 id 3020 'GS Model Detail Radio Med' (type 11); site form via UIRadioButton* theRadio76 = (UIRadioButt
    kGameSettingsModelDetailRadioLow = 77,         // dbuicontrols form 5 index 77 id 3021 'GS Model Detail Radio Low' (type 11); site form via UIRadioButton* theRadio77 = (UIRadioButt
    kGameSettingsEffectsDetailHigh = 79,           // dbuicontrols form 5 index 79 id 3023 'GS Effects Detail High' (type 11); site form via UIRadioButton* theRadio79 = (UIRadioButt
    kGameSettingsEffectsDetailMed = 80,            // dbuicontrols form 5 index 80 id 3024 'GS Effects Detail Med' (type 11); site form via UIRadioButton* theRadio80 = (UIRadioButt
    kGameSettingsEffectsDetailLow = 81,            // dbuicontrols form 5 index 81 id 3025 'GS Effects Detail Low' (type 11); site form via UIRadioButton* theRadio81 = (UIRadioButt
    kGameSettingsHiResFilterCheckbox = 82,         // dbuicontrols form 5 index 82 id 3026 'GS Hi-Res Filter Checkbox' (type 2); site form via UICheckBox* theCheck82 = (UICheckBox*)in
    kGameSettingsHiResLightingCheckbox = 83,       // dbuicontrols form 5 index 83 id 3027 'GS Hi-Res Lighting Checkbox' (type 2); site form via UICheckBox* theCheck83 = (UICheckBox*)in
    kGameSettingsHiResTexturesCheckbox = 84,       // dbuicontrols form 5 index 84 id 3028 'GS Hi-Res Textures Checkbox' (type 2); site form via UICheckBox* theCheck84 = (UICheckBox*)in
    kGameSettingsVsyncCheckbox = 85,               // dbuicontrols form 5 index 85 id 3029 'GS Vsync Checkbox' (type 2); site form via UICheckBox* theWaitForVSync = (UICheckBo
    kGameSettingsShadowsCheckbox = 86,             // dbuicontrols form 5 index 86 id 3030 'GS Shadows Checkbox' (type 2); site form via UICheckBox* theCheck86 = (UICheckBox*)in
    kGameSettingsCloudsCheckbox = 87,              // dbuicontrols form 5 index 87 id 3031 'GS Clouds Checkbox' (type 2); site form via UICheckBox* theCheck87 = (UICheckBox*)in
    kGameSettingsWeatherEffectsCheckbox = 88,      // dbuicontrols form 5 index 88 id 3032 'GS Weather Effects Checkbox' (type 2); site form via UICheckBox* theCheck88 = (UICheckBox*)in
    kGameSettingsAutosaveFreqSlider = 91,          // dbuicontrols form 5 index 91 id 3073 'GS Autosave Freq. Slider' (type 17); site form via UISlider* theAutoSaveTime = (UISlider*)i
    kGameSettingsAutosaveFreqNumberLabel = 92,     // dbuicontrols form 5 index 92 id 3074 'GS Autosave Freq. Number Label' (type 15); site form via heEvent = new UIESliderChange((UITextLab
    kGameSettingsAutosavesToKeepSlider = 94,       // dbuicontrols form 5 index 94 id 3076 'GS Autosaves to Keep Slider' (type 17); site form via UISlider* theAutoSaveCount = (UISlider*)
    kGameSettingsAutosavesToKeepNumberLabel = 95,  // dbuicontrols form 5 index 95 id 3077 'GS Autosaves to Keep Number Label' (type 15); site form via heEvent = new UIESliderChange((UITextLab
    kGameSettingsFullscreenAntialiasCheckbox = 105,  // dbuicontrols form 5 index 105 id 3287 'GS Fullscreen Antialias Checkbox' (type 2); site form via UICheckBox* theCheck105 = (UICheckBox*)i
    kGameSettingsCantChangeResInGameWarning = 106,  // dbuicontrols form 5 index 106 id 3355 'GS Can't Change Res in Game Warning' (type 16); site form via inForm
    kGameSettingsAdaptorSeletionComboBox = 109,    // dbuicontrols form 5 index 109 id 3372 'GS Adaptor Seletion Combo Box' (type 4); site form via UIComboBox* theAdapterBox = (UIComboBox*
    // port: not in the database (EEUIManager's ctor adds it: UIControlsTable::AddControlLike, a copy of the video
    // card combo box; strings kTextureResolutionOriginalString..)
    kGameSettingsTextureResolutionComboBox = 111,
    // port: the modern display and audio settings (EEUIManager.cpp AddControlLike; EEUIEGameSettingsEvents.cpp)
    kGameSettingsWindowModeComboBox = 112,
    kGameSettingsUIScaleComboBox = 113,
    kGameSettingsRenderResolutionComboBox = 114,
    kGameSettingsFrameRateComboBox = 115,
    kGameSettingsTextureFilteringComboBox = 116,
    kGameSettingsBrightnessComboBox = 117,
    kGameSettingsMaximumZoomOutComboBox = 118,
    kGameSettingsMasterVolumeLabel = 119,
    kGameSettingsMasterVolumeSlider = 120,
    kGameSettingsOutputDeviceComboBox = 121,
    kGameSettingsMuteInBackgroundCheckBox = 122,
    kGameSettingsQualityComboBox = 123,            // port: the graphics quality preset (replaces the 5 radio buttons)
    kGameSettingsBattlefieldAmbientsCheckBox = 110,  // dbuicontrols form 5 index 110 id 3400 'GS Battlefield Ambients Check Box' (type 2); site form via UICheckBox* theCheck110 = (UICheckBox*)i
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
    kLoadGameCancelButton = 2,   // dbuicontrols form 4 index 2 id 144 'Load's Cancel Button' (type 1); site form via inForm
    kLoadGameLoadFileListBox = 4,  // dbuicontrols form 4 index 4 id 141 'Load File List Box' (type 22); site form via UIFileListBox* theList = (UIFileListBox*
    kLoadGameLoadScreenTitle = 6,  // dbuicontrols form 4 index 6 id 139 'Load Screen Title' (type 15); site form via ((UITextLabel*)inForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 26 'Multi/Single
// Setup Screen(Lobby)'; names from the dbuicontrols control names (form prefix dropped).
enum EELobbyControl {
    kLobbyReadyLabel = 1,                // dbuicontrols form 26 index 1 id 2502 'M-SS Ready? Label' (type 15); site form via inForm
    kLobbyPingLabel = 6,                 // dbuicontrols form 26 index 6 id 2507 'M-SS Ping Label' (type 15); site form via inForm
    kLobbyChatPreviousMessages = 8,      // dbuicontrols form 26 index 8 id 2509 'M-SS Chat Previous Messages' (type 16); site form via Chat = (UITextLabel*)gUIManager->GetForm
    kLobbyChatTextEditBox = 9,           // dbuicontrols form 26 index 9 id 2510 'M-SS Chat Text Edit Box' (type 8); site form via inForm
    kLobbyStartGameButton = 10,          // dbuicontrols form 26 index 10 id 2511 'M-SS Start Game Button' (type 27); site form via inForm
    kLobbyCancelButton = 11,             // dbuicontrols form 26 index 11 id 2512 'M-SS Cancel Button' (type 1); site form via inForm
    kLobbyScenarioInfoLabel = 28,        // dbuicontrols form 26 index 28 id 2529 'M-SS Scenario Info Label' (type 15); site form via theForm
    kLobbyScenarioInfoMLText = 29,       // dbuicontrols form 26 index 29 id 2530 'M-SS Scenario Info ML Text' (type 16); site form via theForm
    kLobbyGameTypeCombo = 175,           // dbuicontrols form 26 index 175 id 2678 'M-SS Game Type Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyMapSizeLabel0 = 176,           // dbuicontrols form 26 index 176 id 2679 'M-SS Map Size Label 0' (type 15); site form via theForm
    kLobbyMapSizeCombo = 177,            // dbuicontrols form 26 index 177 id 2680 'M-SS Map Size Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyMapTypeLabel0 = 178,           // dbuicontrols form 26 index 178 id 2681 'M-SS Map Type Label 0' (type 15); site form via theForm
    kLobbyMapTypeCombo = 179,            // dbuicontrols form 26 index 179 id 2682 'M-SS Map Type Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyResourcesCombo = 181,          // dbuicontrols form 26 index 181 id 2684 'M-SS Resources Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyEpochStartCombo = 183,         // dbuicontrols form 26 index 183 id 2686 'M-SS Epoch Start Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyEpochEndCombo = 185,           // dbuicontrols form 26 index 185 id 2601 'M-SS Epoch End Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyWorldUnitLimitCombo = 187,     // dbuicontrols form 26 index 187 id 2688 'M-SS World Unit Limit Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyWondersVictoryCombo = 189,     // dbuicontrols form 26 index 189 id 2690 'M-SS Wonders Victory Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyGameTypeLabel2 = 194,          // dbuicontrols form 26 index 194 id 2695 'M-SS Game Type Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyMapSizeLabel2 = 195,           // dbuicontrols form 26 index 195 id 2696 'M-SS Map Size Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyMapTypeLabel2 = 196,           // dbuicontrols form 26 index 196 id 2697 'M-SS Map Type Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyResourcesLabel2 = 197,         // dbuicontrols form 26 index 197 id 2698 'M-SS Resources Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyEpochStartLabel2 = 198,        // dbuicontrols form 26 index 198 id 2699 'M-SS Epoch Start Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyEpochEndLabel2 = 199,          // dbuicontrols form 26 index 199 id 2700 'M-SS Epoch End Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyWorldPopLimitLabel2 = 200,     // dbuicontrols form 26 index 200 id 2701 'M-SS World Pop Limit Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyWondersVictoryLabel2 = 201,    // dbuicontrols form 26 index 201 id 2702 'M-SS Wonders Victory Label2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameVariantCombo = 205,        // dbuicontrols form 26 index 205 id 2706 'M-SS Game Variant Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyDifficultyCombo = 207,         // dbuicontrols form 26 index 207 id 2708 'M-SS Difficulty Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyGameSpeedCombo = 209,          // dbuicontrols form 26 index 209 id 2710 'M-SS Game Speed Combo' (type 4); site form via ((UIComboBox*)inForm
    kLobbyGameOpCheckBox6RecGame = 214,  // dbuicontrols form 26 index 214 id 2715 'M-SS Game Op Check Box 6 (Rec Game)' (type 2); site form via inForm
    kLobbyGameOpCheckBox1RevealMap = 215,  // dbuicontrols form 26 index 215 id 2716 'M-SS Game Op Check Box 1 (Reveal Map)' (type 2); site form via inForm
    kLobbyGameOpCheckBox2CustomCiv = 216,  // dbuicontrols form 26 index 216 id 2717 'M-SS Game Op Check Box 2 (Custom Civ?)' (type 2); site form via inForm
    kLobbyGameOpCheckBox3LockTeam = 217,  // dbuicontrols form 26 index 217 id 2718 'M-SS Game Op Check Box 3 (Lock Team)' (type 2); site form via inForm
    kLobbyGameOpCheckBox4LockSpeed = 218,  // dbuicontrols form 26 index 218 id 2719 'M-SS Game Op Check Box 4 (Lock Speed)' (type 2); site form via inForm
    kLobbyGameOpCheckBox5Cheats = 219,   // dbuicontrols form 26 index 219 id 2720 'M-SS Game Op Check Box 5 (Cheats)' (type 2); site form via inForm
    kLobbyGameOpCheckBox7Victory = 220,  // dbuicontrols form 26 index 220 id 2721 'M-SS Game Op Check Box 7 (victory)' (type 2); site form via inForm
    kLobbyGameVariantLabel2 = 221,       // dbuicontrols form 26 index 221 id 2722 'M-SS Game Variant Label 2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyDifficultyLabel2 = 222,        // dbuicontrols form 26 index 222 id 2723 'M-SS Difficulty Label 2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameSpeedLabel2 = 223,         // dbuicontrols form 26 index 223 id 2724 'M-SS Game Speed Label 2' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox6Bitmap = 226,   // dbuicontrols form 26 index 226 id 2727 'M-SS Game Op Check Box 6 Bitmap' (type 0); site form via ((UIBitmap*)theForm
    kLobbyGameOpCheckBox1Bitmap = 227,   // dbuicontrols form 26 index 227 id 2728 'M-SS Game Op Check Box 1 Bitmap' (type 0); site form via ((UIBitmap*)theForm
    kLobbyGameOpCheckBox2Bitmap = 228,   // dbuicontrols form 26 index 228 id 2729 'M-SS Game Op Check Box 2 Bitmap' (type 0); site form via ((UIBitmap*)theForm
    kLobbyGameOpCheckBox3Bitmap = 229,   // dbuicontrols form 26 index 229 id 2730 'M-SS Game Op Check Box 3 Bitmap' (type 0); site form via ((UIBitmap*)theForm
    kLobbyGameOpCheckBox4Bitmap = 230,   // dbuicontrols form 26 index 230 id 2731 'M-SS Game Op Check Box 4 Bitmap' (type 0); site form via ((UIBitmap*)theForm
    kLobbyGameOpCheckBox5Bitmap = 231,   // dbuicontrols form 26 index 231 id 2732 'M-SS Game Op Check Box 5 Bitmap' (type 0); site form via inForm
    kLobbyGameOpCheckBox7Bitmap = 232,   // dbuicontrols form 26 index 232 id 2733 'M-SS Game Op Check Box 7 Bitmap' (type 0); site form via theForm
    kLobbyGameOpCheckBox6Label = 249,    // dbuicontrols form 26 index 249 id 2906 'M-SS Game Op Check Box 6  Label' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox1Label = 250,    // dbuicontrols form 26 index 250 id 2907 'M-SS Game Op Check Box 1 Label' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox2Label = 251,    // dbuicontrols form 26 index 251 id 2908 'M-SS Game Op Check Box 2 Label' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox3Label = 252,    // dbuicontrols form 26 index 252 id 2909 'M-SS Game Op Check Box 3 Label' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox4Label = 253,    // dbuicontrols form 26 index 253 id 2910 'M-SS Game Op Check Box 4 Label' (type 15); site form via ((UITextLabel*)theForm
    kLobbyGameOpCheckBox5Label = 254,    // dbuicontrols form 26 index 254 id 2911 'M-SS Game Op Check Box 5 Label' (type 15); site form via inForm
    kLobbyGameOpCheckBox7Label = 255,    // dbuicontrols form 26 index 255 id 2912 'M-SS Game Op Check Box 7 Label' (type 15); site form via theForm
    kLobbyLagIndicator = 273,            // dbuicontrols form 26 index 273 id 3296 'M-SS Lag Indicator' (type 27); site form via nkingButton* theButton = (UIBlinkingButt
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
    kLobbyEnginePingHeaderString = 33174,   // "Ping / AI" (multiplayer)
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

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 12 'Main Menu
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEMainMenuControl {
    kMainMenuBuildInformationTemporary = 5,   // dbuicontrols form 12 index 5 id 356 'Build Information (temporary)' (type 15); site form via ((UITextLabel*)inForm
    kMainMenuMainMenuCopyrightNoticeLabel = 8,  // dbuicontrols form 12 index 8 id 3356 'Main Menu Copyright Notice label' (type 15); site form via inForm
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

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 8 'Play Campaign
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEPlayCampaignControl {
    kPlayCampaignCampaignIntroMultiLineText = 2,  // dbuicontrols form 8 index 2 id 397 'PC Campaign Intro Multi-Line Text' (type 25); site form via heLabel = (UITextLabel*)gUIManager->GetF
    kPlayCampaignGreekCampaignIButton = 3,      // dbuicontrols form 8 index 3 id 398 'PC (Greek) Campaign I Button' (type 28); site form via theForm
    kPlayCampaignBritishCampaignIIButton = 4,   // dbuicontrols form 8 index 4 id 399 'PC (British) Campaign II Button' (type 28); site form via theForm
    kPlayCampaignGermanCampaignIIIButton = 5,   // dbuicontrols form 8 index 5 id 400 'PC (German) Campaign III Button' (type 28); site form via theForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 25 'Post Game
// Summary Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEPostGameSummaryControl {
    kPostGameLineGraph = 0,   // dbuicontrols form 25 index 0 id 916 'PGS Line Graph' (type 1016); site form via ineGraph* theLineGraph = (EEUIPGSLineGra
    kPostGameTabSummary = 34,  // dbuicontrols form 25 index 34 id 950 'PGS Tab - Summary' (type 11); site form via theRectangle.SetTop(inForm
    kPostGameTabTimeLine = 35,  // dbuicontrols form 25 index 35 id 951 'PGS Tab - Time Line' (type 11); site form via theRectangle = inForm
    kPostGameTabMilitary = 36,  // dbuicontrols form 25 index 36 id 952 'PGS Tab - Military' (type 11); site form via theRectangle.SetTop(inForm
    kPostGameTabEconomy = 37,  // dbuicontrols form 25 index 37 id 953 'PGS Tab - Economy' (type 11); site form via inForm
    kPostGameTabEpoch = 38,   // dbuicontrols form 25 index 38 id 954 'PGS Tab - Epoch' (type 11); site form via theRectangle = inForm
    kPostGameTabSociety = 39,  // dbuicontrols form 25 index 39 id 955 'PGS Tab - Society' (type 11); site form via theRectangle.SetTop(inForm
    kPostGameTabMisc = 40,    // dbuicontrols form 25 index 40 id 956 'PGS Tab - Misc' (type 11); site form via theRectangle = inForm
    kPostGameMainTitle = 43,  // dbuicontrols form 25 index 43 id 959 'PGS Main Title' (type 15); site form via UITextLabel* theLabel = (UITextLabel*)in
    kPostGameGoldStar = 790,  // dbuicontrols form 25 index 790 id 1691 'PGS Gold Star' (type 0); site form via tmap* theTemplate = dynamic_cast<UIBitma
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 35 'Predefined Civ
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EEPredefinedCivFormControl {
    kPredefinedCivCancelButton = 0,     // dbuicontrols form 35 index 0 id 3227 'Predefined Civ Cancel Button' (type 1); site form via inForm
    kPredefinedCivOKButton = 1,         // dbuicontrols form 35 index 1 id 3228 'Predefined Civ OK Button' (type 1); site form via inForm
    kPredefinedCivPredefinedListBox = 3,  // dbuicontrols form 35 index 3 id 3230 'Predefined List Box' (type 1011); site form via EECBUIListBox* theCivs = (EECBUIListBox*
    kPredefinedCivStatsListBox = 4,     // dbuicontrols form 35 index 4 id 3231 'Predefined Civ Stats List Box' (type 1011); site form via EECBUIListBox* theStats = (EECBUIListBox
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 3 'In Game
// Scenario & Save Game Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EESaveGameControl {
    kSaveGameSaveFileListBox = 4,  // dbuicontrols form 3 index 4 id 133 'Save File List Box' (type 22); site form via UIFileListBox* theList = (UIFileListBox*
    kSaveGameSaveAsGameEditBox = 5,  // dbuicontrols form 3 index 5 id 134 'Save As Game Edit Box' (type 8); site form via UIEditBox* theEdit = (UIEditBox*)theForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 19 'Scenario Intro
// Screen(Briefing Screen)'; names from the dbuicontrols control names (form prefix dropped).
enum EEScenarioIntroControl {
    kScenarioIntroMapButton = 0,          // dbuicontrols form 19 index 0 id 449 'SI Map Button' (type 1); site form via UIControl* theMapButton = inForm
    kScenarioIntroMovieButton = 4,        // dbuicontrols form 19 index 4 id 553 'SI Movie Button' (type 1); site form via UIControl* theMovieButton = inForm
    kScenarioIntroStartGamePlayButton = 5,  // dbuicontrols form 19 index 5 id 554 'SI Start Game (play) Button' (type 1); site form via inForm
    kScenarioIntroTextLabel = 7,          // dbuicontrols form 19 index 7 id 563 'SI Text Label' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
    kScenarioIntroTitle = 8,              // dbuicontrols form 19 index 8 id 2987 'SI Title' (type 15); site form via gUIManager->GetFormByID(0x13)
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 20 'Scenario Load
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EEScenarioLoadControl {
    kScenarioLoadLoadFileList = 3,  // dbuicontrols form 20 index 3 id 558 'SL Load File List' (type 22); site form via nt = new EEUIESLEDeleteFile((UIFileListB
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 38 'Select Server
// Form'; names from the dbuicontrols control names (form prefix dropped).
enum EESelectServerControl {
    kSelectServerListBox = 1,     // dbuicontrols form 38 index 1 id 3266 'Serv  List Box' (type 9); site form via String>* theList = (UIListBox<UWideStrin
    kSelectServerOKButton = 2,    // dbuicontrols form 38 index 2 id 3267 'Serv OK Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSOk,
    kSelectServerCancelButton = 3,  // dbuicontrols form 38 index 3 id 3268 'Serv Cancel Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSCanc
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 6 'Single Player
// Menu'; names from the dbuicontrols control names (form prefix dropped).
enum EESinglePlayerControl {
    kSinglePlayerPlayerNameEditBox = 8,  // dbuicontrols form 6 index 8 id 3117 'SP Player Name Edit Box' (type 8); site form via ((UIEditBox*)inForm
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 13 'Single Player
// Load Game Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EESinglePlayerLoadGameControl {
    kSPLoadGameFileListBox = 4,  // dbuicontrols form 13 index 4 id 394 'SP_Load File List Box' (type 22); site form via nt = new EEUIESLEDeleteFile((UIFileListB
};

// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 23 'Splash
// Screen'; names from the dbuicontrols control names (form prefix dropped).
enum EESplashControl {
    kSplashRegisteredTo = 0,  // dbuicontrols form 23 index 0 id 800 'SS Registered To' (type 15); site form via ((UITextLabel*)inForm
    kSplashCopyRight = 1,   // dbuicontrols form 23 index 1 id 801 'SS CopyRight' (type 15); site form via ((UITextLabel*)inForm
    kSplashLoadStatus = 2,  // dbuicontrols form 23 index 2 id 802 'SS Load Status' (type 13); site form via ((UIStatusBar*)gUIManager->GetFormByID(0
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
