//! The UI database's form ids and the forms' control indices the game names (once enums of ee/EEUIManager.h, UI
//! round 13): `UIForm::GetControl` indices (dbuicontrols +0x29c) and `UIManager::GetFormByID` ids (dbuiforms +0xcc),
//! names from the database's control and form names. C++ sees them as unscoped `enum X : uint32_t` (the
//! `cbindgen:enum-class=false` annotation), under the C++ names; Rust uses them as numbers (`as u32`).
#![allow(non_camel_case_types)]

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 32 'Available
/// Games (Sierra) Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAvailableGamesControl {
    /// dbuicontrols form 32 index 2 id 3080 'AG Player's Name Label' (type 15); site form via UITextLabel* theUserLabel = (UITextLabel
    kAvailableGamesPlayersNameLabel = 2,
    /// dbuicontrols form 32 index 4 id 3082 'AG Room's Name Label' (type 15); site form via UITextLabel* theRoomLabel = (UITextLabel
    kAvailableGamesRoomsNameLabel = 4,
    /// dbuicontrols form 32 index 5 id 3083 'AG Join Game Tab' (type 12); site form via TriggerEvent(new EEUIEWNAGToggleJoinScre
    kAvailableGamesJoinGameTab = 5,
    /// dbuicontrols form 32 index 6 id 3084 'AG Host Game Tab' (type 12); site form via TriggerEvent(new EEUIEWNAGToggleHostScre
    kAvailableGamesHostGameTab = 6,
    /// dbuicontrols form 32 index 8 id 3086 'AG Players in Room List box' (type 9); site form via gWonNet->RefreshGameList(inForm
    kAvailableGamesPlayersInRoomListBox = 8,
    /// dbuicontrols form 32 index 13 id 3091 'AG Aval. Games List Box' (type 1019); site form via EUICWonNetGamesBox*>(gUIManager->GetForm
    kAvailableGamesAvalGamesListBox = 13,
    /// dbuicontrols form 32 index 16 id 3094 'AG Chat Edit Box' (type 8); site form via UIEditBox* theChat = (UIEditBox*)inForm
    kAvailableGamesChatEditBox = 16,
    /// dbuicontrols form 32 index 17 id 3095 'AG Join/Host Game Button' (type 28); site form via etTriggerEvent(new EEUIEWNAGHostOrJoinGa
    kAvailableGamesJoinHostGameButton = 17,
    /// dbuicontrols form 32 index 18 id 3096 'AG Back Button' (type 28); site form via EEUIEWNSetTriggerEvent(new EEUIEWNAGBack
    kAvailableGamesBackButton = 18,
    /// dbuicontrols form 32 index 24 id 3102 'AG Host Game Name Edit Box' (type 8); site form via UIEditBox* theName = (UIEditBox*)inForm
    kAvailableGamesHostGameNameEditBox = 24,
    /// dbuicontrols form 32 index 27 id 3129 'AG PW for Hosted Game Edit Box' (type 8); site form via UIEditBox* thePassword = (UIEditBox*)inF
    kAvailableGamesPWForHostedGameEditBox = 27,
    /// dbuicontrols form 32 index 28 id 3130 'AG PW for Hosted Game Checkbox' (type 2); site form via UICheckBox* theCheckBox = (UICheckBox*)i
    kAvailableGamesPWForHostedGameCheckbox = 28,
    /// dbuicontrols form 32 index 29 id 3133 'AG PW Retype for Hosted Game Edit Box' (type 8); site form via UIEditBox* theConfirmation = (UIEditBox*
    kAvailableGamesPWRetypeForHostedGameEditBox = 29,
    /// dbuicontrols form 32 index 30 id 3131 'AG PW  for Hosted Game Label' (type 15); site form via UIControl* thePasswordLabel = inForm
    kAvailableGamesPWForHostedGameLabel = 30,
    /// dbuicontrols form 32 index 31 id 3132 'AG PW Retype for Hosted Game Label' (type 15); site form via UIControl* theConfirmationLabel = inForm
    kAvailableGamesPWRetypeForHostedGameLabel = 31,
    /// dbuicontrols form 32 index 32 id 3350 'AG In Progress Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
    kAvailableGamesInProgressToggle = 32,
    /// dbuicontrols form 32 index 33 id 3351 'AG Password Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
    kAvailableGamesPasswordToggle = 33,
    /// dbuicontrols form 32 index 34 id 3352 'AG Private Toggle' (type 2); site form via tTriggerEvent(new EEUIEWNAGClickGameFilt
    kAvailableGamesPrivateToggle = 34,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 31 'Available
/// Rooms (Sierra) Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEAvailableRoomsControl {
    /// dbuicontrols form 31 index 2 id 2963 'AR Player 's Name Label' (type 15); site form via UITextLabel* theUserLabel = (UITextLabel
    kAvailableRoomsPlayersNameLabel = 2,
    /// dbuicontrols form 31 index 6 id 2967 'AR Avail. Rooms List Box' (type 1018); site form via EUICWonNetRoomsBox*>(gUIManager->GetForm
    kAvailableRoomsAvailRoomsListBox = 6,
    /// dbuicontrols form 31 index 7 id 2968 'AR Join Room Button' (type 28); site form via UIEWNSetTriggerEvent(new EEUIEWNARJoinRo
    kAvailableRoomsJoinRoomButton = 7,
    /// dbuicontrols form 31 index 8 id 2969 'AR Back Button' (type 28); site form via EEUIEWNSetTriggerEvent(new EEUIEWNARBack
    kAvailableRoomsBackButton = 8,
    /// dbuicontrols form 31 index 9 id 3269 'AR Update My Account' (type 1); site form via iggerEvent(new EEUIEWNARClickUpdateAccou
    kAvailableRoomsUpdateMyAccount = 9,
    /// dbuicontrols form 31 index 14 id 3293 'AR Total Games Number' (type 20); site form via ((UITextLabel*)gUIManager->GetFormByID(0
    kAvailableRoomsTotalGamesNumber = 14,
    /// dbuicontrols form 31 index 16 id 3295 'AR Server's Name Label' (type 15); site form via ((UITextLabel*)gUIManager->GetFormByID(0
    kAvailableRoomsServersNameLabel = 16,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 24 'Campaign
/// Editor Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EECampaignEditorControl {
    /// dbuicontrols form 24 index 44 id 3106 'CE Background Image Combo Box' (type 32); site form via UIFileComboBox* theList = (UIFileComboBo
    kCampaignEditorBackgroundImageComboBox = 44,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 18 'Chat In Game
/// pop-up dialog'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEChatControl {
    /// dbuicontrols form 18 index 1 id 414 'Chat Text Edit Box' (type 8); site form via inForm
    kChatTextEditBox = 1,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 29 'Chat Log
/// Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEChatLogControl {
    /// dbuicontrols form 29 index 1 id 3069 'Chat Log Text Box' (type 16); site form via UITextLabel* theLabel = (UITextLabel*)th
    kChatLogChatLogTextBox = 1,
}

/// UI database form ids (dbuiforms form id field +0xcc: UIManager::GetFormByID, the controls' form field); names from
/// the form names.
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEFormID {
    /// dbuiforms form id 3 'In Game Scenario & Save Game Screen'
    kFormSaveGame = 3,
    /// dbuiforms form id 11 'Scott Main Game Form'
    kFormMainGame = 11,
    /// dbuiforms form id 12 'Main Menu Screen'
    kFormMainMenu = 12,
    /// dbuiforms form id 19 'Scenario Intro Screen(Briefing Screen)'
    kFormScenarioIntro = 19,
    /// dbuiforms form id 20 'Scenario Load Screen'
    kFormScenarioLoad = 20,
    /// dbuiforms form id 22 'DS Debug Sound Screen'
    kFormDebugSound = 22,
    /// dbuiforms form id 23 'Splash Screen'
    kFormSplash = 23,
    /// dbuiforms form id 25 'Post Game Summary Screen'
    kFormPostGameSummary = 25,
    /// dbuiforms form id 26 'Multi/Single Setup Screen(Lobby)'
    kFormLobby = 26,
    /// dbuiforms form id 28 'In-game Options form'
    kFormGameOptions = 28,
    /// dbuiforms form id 29 'Chat Log Form'
    kFormChatLog = 29,
    /// dbuiforms form id 30 'Multiplayer Menu Form'
    kFormMultiplayer = 30,
    /// dbuiforms form id 31 'Available Rooms (Sierra) Form'
    kFormAvailableRooms = 31,
    /// dbuiforms form id 32 'Available Games (Sierra) Form'
    kFormAvailableGames = 32,
    /// dbuiforms form id 33 'Campaign Scenario Select Form'
    kFormCampaignScenarioSelect = 33,
    /// dbuiforms form id 35 'Predefined Civ Form'
    kFormPredefinedCiv = 35,
    /// dbuiforms form id 37 'Update Sierra Account Form'
    kFormUpdateSierraAccount = 37,
    /// dbuiforms form id 38 'Select Server Form'
    kFormSelectServer = 38,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 5 'Game Settings
/// Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEGameSettingsControl {
    /// port: not in the database (EEUIManager's ctor adds it: UIControlsTable::AddControlLike, a copy of the video
    /// card combo box; strings kTextureResolutionOriginalString..)
    kGameSettingsTextureResolutionComboBox = 111,
    /// port: the modern display and audio settings (EEUIManager.cpp AddControlLike; ee_screens settings_form.rs)
    /// port: the graphics quality preset (replaces the 5 radio buttons)
    kGameSettingsQualityComboBox = 123,
}

/// port: the texture resolution combo box (kGameSettingsTextureResolutionComboBox): its control id (past the
/// database's) and its strings (Language.rc)
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EETextureResolutionControl {
    kTextureResolutionControlID = 90001,
    /// "Textures: Original", then 2x (33101) and 4x (33102)
    kTextureResolutionOriginalString = 33100,
    kTextureResolutionHelpString = 33103,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 4 'In Game
/// Scenario & Load Game Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EELoadGameControl {
    /// dbuicontrols form 4 index 1 id 143 'Load Button' (type 1); site form via inForm
    kLoadGameLoadButton = 1,
    /// dbuicontrols form 4 index 4 id 141 'Load File List Box' (type 22); site form via UIFileListBox* theList = (UIFileListBox*
    kLoadGameLoadFileListBox = 4,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 26 'Multi/Single
/// Setup Screen(Lobby)'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EELobbyControl {
    /// dbuicontrols form 26 index 8 id 2509 'M-SS Chat Previous Messages' (type 16); site form via Chat = (UITextLabel*)gUIManager->GetForm
    kLobbyChatPreviousMessages = 8,
    /// port: not in the database (EEUIManager's ctor adds them: UIControlsTable::AddControlLike). The computer slots'
    /// engine (ee/EECPEngine.h; docs/AI_LUA_PLAN.md S0) in the ping column, which a computer row leaves empty: its
    /// header (a copy of the ping label, shown in place of it), per slot a combo box (a copy of the citizens combo,
    /// the host's before ready) and a label (a copy of the citizens label, everyone else)
    kLobbyEngineHeader = 274,
    /// + slot (16)
    kLobbyEngineCombo = 275,
    /// + slot (16)
    kLobbyEngineLabel = 291,
}

/// port: the lobby's engine controls' ids (past the database's and the settings' 90001..) and strings (Language.rc)
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EELobbyEngineControl {
    /// the header; + 1 + slot the combo boxes, + 17 + slot the labels
    kLobbyEngineControlID = 90100,
    /// "Classic", then "Random": the combo's first lines (ee/EECPEngine.h
    kLobbyEngineClassicString = 33170,
    /// EECPEngineChoice); the AI manifest's names follow
    kLobbyEngineRandomString = 33171,
    /// "AI" (single player)
    kLobbyEngineHeaderString = 33173,
    kLobbyEngineHelpString = 33175,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 11 'Scott Main
/// Game Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEMainGameControl {
    /// dbuicontrols form 11 index 3 id 250 'SRMIngame Options*' (type 27); site form via ((UIBlinkingButton*)gUIManager->mMainGam
    kMainGameIngameOptions = 3,
    /// dbuicontrols form 11 index 5 id 252 'SRMCiv Selector/Builder*' (type 27); site form via UIControl* theControl = theForm
    kMainGameCivSelectorBuilder = 5,
    /// dbuicontrols form 11 index 13 id 246 'SRMWin/Lose*' (type 15); site form via this
    kMainGameWinLose = 13,
    /// dbuicontrols form 11 index 24 id 263 'SRMPopulation*' (type 20); site form via this
    kMainGamePopulation = 24,
    /// dbuicontrols form 11 index 25 id 264 'SRMTitle*' (type 15); site form via this
    kMainGameTitle = 25,
    /// dbuicontrols form 11 index 26 id 265 'SRMMiniMap*' (type 1001); site form via this
    kMainGameMiniMap = 26,
    /// dbuicontrols form 11 index 27 id 266 'SRMError Log*' (type 1003); site form via this
    kMainGameErrorLog = 27,
    /// dbuicontrols form 11 index 28 id 267 'SRMSelect Units*' (type 1007); site form via ((EEUICSelectUnits*)theForm
    kMainGameSelectUnits = 28,
    /// dbuicontrols form 11 index 29 id 268 'SRMCommand Units*' (type 1008); site form via ((EEUICCommandUnits*)theForm
    kMainGameCommandUnits = 29,
    /// dbuicontrols form 11 index 78 id 321 'SRMAction Button 1*' (type 1000); site form via ((EEUIFMGStatusButton*)mOwner
    kMainGameActionButton1 = 78,
    /// dbuicontrols form 11 index 98 id 300 'SRMTemp Object List*' (type 4); site form via ((UIComboBox*)mForm
    kMainGameTempObjectList = 98,
    /// dbuicontrols form 11 index 99 id 301 'SRMTemp Player List*' (type 4); site form via ((UIComboBox*)mForm
    kMainGameTempPlayerList = 99,
    /// dbuicontrols form 11 index 101 id 429 'SRMChat Line 1*' (type 1010); site form via this
    kMainGameChatLine1 = 101,
    /// dbuicontrols form 11 index 107 id 435 'SRMChat Line 7*' (type 1010); site form via this
    kMainGameChatLine7 = 107,
    /// dbuicontrols form 11 index 108 id 797 'SRM Game Clock' (type 20); site form via this
    kMainGameGameClock = 108,
    /// dbuicontrols form 11 index 109 id 578 'SRMGame Info* (Briefing)' (type 27); site form via this
    kMainGameGameInfoBriefing = 109,
    /// dbuicontrols form 11 index 208 id 829 'SRMTimedFighterRally' (type 1); site form via mOwner
    kMainGameTimedFighterRally = 208,
    /// dbuicontrols form 11 index 209 id 830 'SRMTimedBomberRally' (type 1); site form via mOwner
    kMainGameTimedBomberRally = 209,
    /// dbuicontrols form 11 index 210 id 831 'SRMTimedAtomicBomberRally' (type 1); site form via mOwner
    kMainGameTimedAtomicBomberRally = 210,
    /// dbuicontrols form 11 index 211 id 832 'SRMDialogMessage' (type 26); site form via this
    kMainGameDialogMessage = 211,
    /// dbuicontrols form 11 index 222 id 891 'SRMUnit Icon Bar' (type 1014); site form via UIControl* theIconBar = mOwner
    kMainGameUnitIconBar = 222,
    /// dbuicontrols form 11 index 223 id 892 'SRMDialog Portrait' (type 0); site form via this
    kMainGameDialogPortrait = 223,
    /// dbuicontrols form 11 index 234 id 3066 'SRM Large Civ Builder Alert*' (type 27); site form via _cast<UIBlinkingButton*>(gUIManager->mMa
    kMainGameLargeCivBuilderAlert = 234,
    /// dbuicontrols form 11 index 235 id 3067 'SRM Large Scen Briefing Alert*' (type 27); site form via this
    kMainGameLargeScenBriefingAlert = 235,
    /// dbuicontrols form 11 index 244 id 3226 'SRM Cancel UI Mode' (type 31); site form via theForm
    kMainGameCancelUIMode = 244,
    /// dbuicontrols form 11 index 253 id 3270 'SRMPrevious Messages*' (type 1); site form via this
    kMainGamePreviousMessages = 253,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 25 'Post Game
/// Summary Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEPostGameSummaryControl {
    /// dbuicontrols form 25 index 0 id 916 'PGS Line Graph' (type 1016); site form via ineGraph* theLineGraph = (EEUIPGSLineGra
    kPostGameLineGraph = 0,
    /// dbuicontrols form 25 index 790 id 1691 'PGS Gold Star' (type 0); site form via tmap* theTemplate = dynamic_cast<UIBitma
    kPostGameGoldStar = 790,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 38 'Select Server
/// Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EESelectServerControl {
    /// dbuicontrols form 38 index 1 id 3266 'Serv  List Box' (type 9); site form via String>* theList = (UIListBox<UWideStrin
    kSelectServerListBox = 1,
    /// dbuicontrols form 38 index 2 id 3267 'Serv OK Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSOk,
    kSelectServerOKButton = 2,
    /// dbuicontrols form 38 index 3 id 3268 'Serv Cancel Button' (type 1); site form via EEUIEWNSetTriggerEvent(new EEUIEWNSSCanc
    kSelectServerCancelButton = 3,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 13 'Single Player
/// Load Game Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EESinglePlayerLoadGameControl {
    /// dbuicontrols form 13 index 4 id 394 'SP_Load File List Box' (type 22); site form via nt = new EEUIESLEDeleteFile((UIFileListB
    kSPLoadGameFileListBox = 4,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 23 'Splash
/// Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EESplashControl {
    /// dbuicontrols form 23 index 3 id 803 'SS Load Label' (type 15); site form via ((UITextLabel*)gUIManager->GetFormByID(0
    kSplashLoadLabel = 3,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 37 'Update Sierra
/// Account Form'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEUpdateSierraAccountControl {
    /// dbuicontrols form 37 index 2 id 3256 'USA New Password Edit Box' (type 8); site form via thePassword = ((UIEditBox*)inForm
    kUpdateAccountNewPasswordEditBox = 2,
    /// dbuicontrols form 37 index 4 id 3258 'USA Confirm Password Edit Box' (type 8); site form via theConfirmation = ((UIEditBox*)inForm
    kUpdateAccountConfirmPasswordEditBox = 4,
    /// dbuicontrols form 37 index 6 id 3260 'USA Enter Email Edit Box' (type 8); site form via theNewPassword = ((UIEditBox*)inForm
    kUpdateAccountEnterEmailEditBox = 6,
    /// dbuicontrols form 37 index 7 id 3261 'USA Send Me Email Checkbox' (type 2); site form via bool theFlag = ((UICheckBox*)inForm
    kUpdateAccountSendMeEmailCheckbox = 7,
    /// dbuicontrols form 37 index 8 id 3262 'USA Update Button' (type 1); site form via EUIEWNSetTriggerEvent(new EEUIEWNMUASubm
    kUpdateAccountUpdateButton = 8,
    /// dbuicontrols form 37 index 9 id 3263 'USA Don't Update Button' (type 1); site form via EUIEWNSetTriggerEvent(new EEUIEWNMUACanc
    kUpdateAccountDontUpdateButton = 9,
}

/// Control indices (UIForm::GetControl: the control's index in the form, dbuicontrols +0x29c) of form 21 'WON.net Stat
/// Screen'; names from the dbuicontrols control names (form prefix dropped).
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEWonNetStatsControl {
    /// dbuicontrols form 21 index 2 id 607 'WNS Stat Box' (type 1013); site form via ((EEUICWonNetStatBox*)inForm
    kWonStatsStatBox = 2,
    /// dbuicontrols form 21 index 22 id 824 'WNS My Stats Line' (type 1013); site form via ((EEUICWonNetStatBox*)inForm
    kWonStatsMyStatsLine = 22,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::ui_manager::*;

    #[test]
    fn the_manager_ids_are_the_enums() {
        assert_eq!(EETextureResolutionControl::kTextureResolutionControlID as i32, EE_SCREENS_UM_TEXTURE_RESOLUTION_ID);
        assert_eq!(
            EEGameSettingsControl::kGameSettingsTextureResolutionComboBox as i32,
            EE_SCREENS_UM_TEXTURE_RESOLUTION_INDEX
        );
        assert_eq!(
            EETextureResolutionControl::kTextureResolutionOriginalString as i32,
            EE_SCREENS_UM_TEXTURE_RESOLUTION_STRING
        );
        assert_eq!(
            EETextureResolutionControl::kTextureResolutionHelpString as i32,
            EE_SCREENS_UM_TEXTURE_RESOLUTION_HELP
        );
        assert_eq!(EEFormID::kFormLobby as i32, EE_SCREENS_UM_FORM_LOBBY);
        assert_eq!(EELobbyEngineControl::kLobbyEngineControlID as i32, EE_SCREENS_UM_ENGINE_ID);
        assert_eq!(EELobbyControl::kLobbyEngineHeader as i32, EE_SCREENS_UM_ENGINE_HEADER);
        assert_eq!(EELobbyControl::kLobbyEngineCombo as i32, EE_SCREENS_UM_ENGINE_COMBO);
        assert_eq!(EELobbyControl::kLobbyEngineLabel as i32, EE_SCREENS_UM_ENGINE_LABEL);
        assert_eq!(EELobbyEngineControl::kLobbyEngineClassicString as i32, EE_SCREENS_UM_ENGINE_CLASSIC);
        assert_eq!(EELobbyEngineControl::kLobbyEngineRandomString as i32, EE_SCREENS_UM_ENGINE_RANDOM);
        assert_eq!(EELobbyEngineControl::kLobbyEngineHeaderString as i32, EE_SCREENS_UM_ENGINE_HEADER_STRING);
        assert_eq!(EELobbyEngineControl::kLobbyEngineHelpString as i32, EE_SCREENS_UM_ENGINE_HELP);
        assert_eq!(EEGameSettingsControl::kGameSettingsQualityComboBox as u32, 123);
    }
}
