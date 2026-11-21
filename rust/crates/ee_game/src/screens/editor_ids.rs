//! The scenario editor's named integer constants (once the reconstruction's `ee/EEScenarioEditorIDs.h`):
//! its tab groups, entity groups, load thread messages and control ids. Each doc gives the evidence for its
//! name; "(provisional name)" marks an inferred one. The C++ reads them from `rust/game/screens.h`
//! (`kSEElevationRaiseButton` is `EE_SCREENS_SE_ELEVATION_RAISE_BUTTON`).

// Scenario editor main tabs / tab groups (EEUIFScenarioEditor::mTabGroups index, CreateTabGroup 0x59ef5a; sEditorTabs 0x8c46d0 help strings).
// (C++ enum `EESEEditorTabGroup`)
/// `kEditorTabMap`: sEditorTabs[0] help Language.rc 0x213a 'Map'; CreateTabGroup new EEUIFSEMap
pub const EE_SCREENS_SE_EDITOR_TAB_MAP: i32 = 0;
/// `kEditorTabElevation`: help 0x213b 'Elevation'; new EEUIFSEElevation
pub const EE_SCREENS_SE_EDITOR_TAB_ELEVATION: i32 = 1;
/// `kEditorTabTerrain`: help 0x213c 'Terrain'; new EEUIFSETerrain
pub const EE_SCREENS_SE_EDITOR_TAB_TERRAIN: i32 = 2;
/// `kEditorTabUnits`: help 0x213d 'Units'; new EESEUnit
pub const EE_SCREENS_SE_EDITOR_TAB_UNITS: i32 = 3;
/// `kEditorTabTriggers`: help 0x213e 'Triggers'; new EESETriggerScreen
pub const EE_SCREENS_SE_EDITOR_TAB_TRIGGERS: i32 = 4;
/// `kEditorTabPlayers`: help 0x1b61 'Players'; new EESEPlayerScreen
pub const EE_SCREENS_SE_EDITOR_TAB_PLAYERS: i32 = 5;
/// `kEditorTabInstructions`: help 0x2143 'Story/Instructions'; new EESEInstructions
pub const EE_SCREENS_SE_EDITOR_TAB_INSTRUCTIONS: i32 = 6;

// Scenario editor form entity groups of the trigger sub-screens (EESETriggerScreen::mCurrentEntityGroup; ShowTab 0x57c467 pairs each with its tab; each sub-screen's AddControl(.., group)).
// (C++ enum `EESEEntityGroup`)
/// `kEntityGroupTrigger`: ShowTab case 0x7d0 (tab 'Trigger'); EESETSTrigger adds its controls to group 9
pub const EE_SCREENS_SE_ENTITY_GROUP_TRIGGER: i32 = 9;
/// `kEntityGroupObject`: ShowTab case 0x7d3 (tab 'Object'); EESETSObject adds its controls to group 10
pub const EE_SCREENS_SE_ENTITY_GROUP_OBJECT: i32 = 10;
/// `kEntityGroupArea`: ShowTab case 0x7d4 (tab 'Area'); EESETSArea adds its controls to group 11
pub const EE_SCREENS_SE_ENTITY_GROUP_AREA: i32 = 11;
/// `kEntityGroupCondition`: ShowTab case 0x7d1 (tab 'Condition'); EESETSCondition/EESETSConditionControls add to group 12
pub const EE_SCREENS_SE_ENTITY_GROUP_CONDITION: i32 = 12;
/// `kEntityGroupEffect`: ShowTab case 0x7d2 (tab 'Effect'); EESETSEffect/EESETSEffectControls add to group 13
pub const EE_SCREENS_SE_ENTITY_GROUP_EFFECT: i32 = 13;

// Messages posted to the scenario editor's load thread (EESELoadThread::Post* -> HandleMessage 0x57285c). Names are
// provisional from the handlers.
// (C++ enum `EESELoadThreadMessage`)
/// `kLoadThreadCreateGhostUnit`: Handle402 loads the unit class media, EEGhostUnit::Create at the mouse, unloads; posted by EESEUnit (unit list selection) (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_CREATE_GHOST_UNIT: i32 = 0x402;
/// `kLoadThreadStartScenarioTest`: handler calls gUserInterface->StartScenarioTest(); posted by ExitEditor(true) while mInScenarioEditor (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_START_SCENARIO_TEST: i32 = 0x404;
/// `kLoadThreadStartScenarioTest2`: handler calls gUserInterface->StartScenarioTest2(); posted by ExitEditor(true) when not mInScenarioEditor (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_START_SCENARIO_TEST2: i32 = 0x405;
/// `kLoadThreadLoadEpochMedia`: Handle406 loads the first n tech nodes' media with 'Loading %s Epoch' (0x7659, names 0x75c1 + i) (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_LOAD_EPOCH_MEDIA: i32 = 0x406;
/// `kLoadThreadUnloadEpochMedia`: Handle407 unloads the first n tech nodes' media (counterpart of 0x406) (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_UNLOAD_EPOCH_MEDIA: i32 = 0x407;
/// `kLoadThreadLoadTerrain`: Handle408(terrain, false): EETerrainTable::Load + its unit classes' media (0x7b27); posted by EEUIFSETerrain (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_LOAD_TERRAIN: i32 = 0x408;
/// `kLoadThreadUnloadTerrain`: Handle408(terrain, true): EETerrainTable::Unload + unit class media unload (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_UNLOAD_TERRAIN: i32 = 0x409;
/// `kLoadThreadMessageQueueDone`: handler sets mMessageQueueDone (no poster found in the tree) (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_MESSAGE_QUEUE_DONE: i32 = 0x40a;
/// `kLoadThreadPostEvent`: handler gUserInterface->PostEvent(param, 0) and dereferences it; EEUIFScenarioEditor::PostLoadEvent (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_POST_EVENT: i32 = 0x40b;
/// `kLoadThreadShowLoadedObject`: handler gUserInterface->ShowLoadedObject(param); EEUIFScenarioEditor::PostShowLoadedObject (provisional name)
pub const EE_SCREENS_SE_LOAD_THREAD_SHOW_LOADED_OBJECT: i32 = 0x40c;

// Player screen pages (EESEPlayerScreen::mTabs index, Fn1761C8 0x5761c8 inPage, EESEPlayerRow::Show 0x579d63 inPage); EESETab::Check 0x5731b0 maps each tab id to its page.
// (C++ enum `EESEPlayerScreenPage`)
/// `kPlayerScreenPlayerPage1`: Language.rc 0x5027 'Player Page 1' (tab id 0x12c -> Fn1761C8(0))
pub const EE_SCREENS_SE_PLAYER_SCREEN_PLAYER_PAGE1: i32 = 0;
/// `kPlayerScreenPlayerPage2`: Language.rc 0x5028 'Player Page 2' (tab id 0x15d -> Fn1761C8(1))
pub const EE_SCREENS_SE_PLAYER_SCREEN_PLAYER_PAGE2: i32 = 1;
/// `kPlayerScreenAISettings`: Language.rc 0x5055 'AI Settings' (tab id 0x170 -> Fn1761C8(2))
pub const EE_SCREENS_SE_PLAYER_SCREEN_AI_SETTINGS: i32 = 2;
/// `kPlayerScreenTechTree`: Language.rc 0x5029 'Tech Tree' (tab id 0x12d -> Fn1761C8(3))
pub const EE_SCREENS_SE_PLAYER_SCREEN_TECH_TREE: i32 = 3;
/// `kPlayerScreenOptions`: Language.rc 0x5042 'Options' (tab id 0x15a -> Fn1761C8(4))
pub const EE_SCREENS_SE_PLAYER_SCREEN_OPTIONS: i32 = 4;

// Control ids of the player screen's tab buttons (EESETab ctor 0x573060 picks the label by id).
// (C++ enum `EESEPlayerTabControlID`)
/// `kPlayerTabPlayerPage1`: EESETab ctor case 0x12c: Language.rc 0x5027 'Player Page 1'
pub const EE_SCREENS_SE_PLAYER_TAB_PLAYER_PAGE1: i32 = 0x12c;
/// `kPlayerTabTechTree`: EESETab ctor case 0x12d: Language.rc 0x5029 'Tech Tree'
pub const EE_SCREENS_SE_PLAYER_TAB_TECH_TREE: i32 = 0x12d;
/// `kPlayerTabOptions`: EESETab ctor case 0x15a: Language.rc 0x5042 'Options'
pub const EE_SCREENS_SE_PLAYER_TAB_OPTIONS: i32 = 0x15a;
/// `kPlayerTabPlayerPage2`: EESETab ctor case 0x15d: Language.rc 0x5028 'Player Page 2'
pub const EE_SCREENS_SE_PLAYER_TAB_PLAYER_PAGE2: i32 = 0x15d;
/// `kPlayerTabAISettings`: EESETab ctor case 0x170: Language.rc 0x5055 'AI Settings'
pub const EE_SCREENS_SE_PLAYER_TAB_AI_SETTINGS: i32 = 0x170;

// Control ids of the player screen's tech tree move buttons (EESEBSelect ctor: button text by id; Select 0x572f3b moves between list 0x154 and 0x155).
// (C++ enum `EESESelectButtonID`)
/// `kSelectButtonMoveRight`: button text '>'; Select moves the selected entry of list 0x154 (theLeft true), repeats while held (provisional name)
pub const EE_SCREENS_SE_SELECT_BUTTON_MOVE_RIGHT: i32 = 0x156;
/// `kSelectButtonMoveLeft`: button text '<'; Select moves the selected entry of list 0x155 back, repeats while held (provisional name)
pub const EE_SCREENS_SE_SELECT_BUTTON_MOVE_LEFT: i32 = 0x157;
/// `kSelectButtonMoveAllRight`: button text '>>'; Select moves every entry of list 0x154 (provisional name)
pub const EE_SCREENS_SE_SELECT_BUTTON_MOVE_ALL_RIGHT: i32 = 0x158;
/// `kSelectButtonMoveAllLeft`: button text '<<'; Select moves every entry of list 0x155 (provisional name)
pub const EE_SCREENS_SE_SELECT_BUTTON_MOVE_ALL_LEFT: i32 = 0x159;

// The player screen's tech-tree list filter radio buttons (EESEPlayerScreen::mTypeButtons index; labels Language.rc 0x5033 + index; Fn1760AD 0x5760ad picks the tech list).
// (C++ enum `EESETechListType`)
/// `kSETechListUnits`: Language.rc 0x5033 'Units' (button 0x14a)
pub const EE_SCREENS_SE_TECH_LIST_UNITS: i32 = 0;
/// `kSETechListBuildings`: Language.rc 0x5034 'Buildings' (button 0x14b)
pub const EE_SCREENS_SE_TECH_LIST_BUILDINGS: i32 = 1;
/// `kSETechListHeroes`: Language.rc 0x5035 'Heroes' (button 0x14c)
pub const EE_SCREENS_SE_TECH_LIST_HEROES: i32 = 2;
/// `kSETechListTechnologies`: Language.rc 0x5036 'Technologies' (button 0x14d; Fn1760AD's fallback list)
pub const EE_SCREENS_SE_TECH_LIST_TECHNOLOGIES: i32 = 3;

// Control ids of the scenario editor tool bar buttons (sToolbarButtons 0x8c47b0; CreateToolbarButtons 0x59f0b1 creates each button's event).
// (C++ enum `EESEToolbarButtonID`)
/// `kToolbarButtonLoad`: sToolbarButtons help Language.rc 0x1b58 'Load'; new EESEELoadScenario
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_LOAD: i32 = 10;
/// `kToolbarButtonSave`: help 0x1b59 'Save'; new EESEESaveScenario
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_SAVE: i32 = 11;
/// `kToolbarButtonQuickLoad`: help 0x2140 'Quick Load'; new EESEEQuickLoad
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_QUICK_LOAD: i32 = 12;
/// `kToolbarButtonQuickSave`: help 0x2141 'Quick Save'; new EESEEQuickSave
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_QUICK_SAVE: i32 = 13;
/// `kToolbarButtonTest`: help 0x2142 'Test'; new EESEETestScenario
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_TEST: i32 = 14;
/// `kToolbarButtonExit`: help 0x213f 'Exit'; new EESEEExitScenario
pub const EE_SCREENS_SE_TOOLBAR_BUTTON_EXIT: i32 = 15;

// Trigger editor entry kinds: the EESETriggerScreen screen / tab index (mScreens, mTabs, mCurrentScreen; CreateScreen 0x57c1e5) and the kind of list a scenario combo box shows or belongs to (EESETSEScenarioComboBox mMode/mLE4, EESETSTriggerUIComboBox mKind). Tab labels Language.rc 0x765b + kind.
// (C++ enum `EESETriggerScreenKind`)
/// `kSETriggerScreenTrigger`: Language.rc 0x765b 'Trigger'; CreateScreen case 0 new EESETSTrigger
pub const EE_SCREENS_SE_TRIGGER_SCREEN_TRIGGER: i32 = 0;
/// `kSETriggerScreenCondition`: Language.rc 0x765c 'Condition'; CreateScreen case 1 new EESETSCondition
pub const EE_SCREENS_SE_TRIGGER_SCREEN_CONDITION: i32 = 1;
/// `kSETriggerScreenEffect`: Language.rc 0x765d 'Effect'; CreateScreen case 2 new EESETSEffect
pub const EE_SCREENS_SE_TRIGGER_SCREEN_EFFECT: i32 = 2;
/// `kSETriggerScreenObject`: Language.rc 0x765e 'Object'; CreateScreen case 3 new EESETSObject
pub const EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT: i32 = 3;
/// `kSETriggerScreenArea`: Language.rc 0x765f 'Area'; CreateScreen case 4 new EESETSArea
pub const EE_SCREENS_SE_TRIGGER_SCREEN_AREA: i32 = 4;

// Control ids of the trigger editor's five tab radio buttons (EESETriggerScreen ctor: new UIRadioButton(2000 + kind, string 0x765b + kind); ShowTab 0x57c467 switches on them).
// (C++ enum `EESETriggerTabControlID`)
/// `kSETriggerTabTrigger`: ctor UIRadioButton(2000 + 0, Language.rc 0x765b 'Trigger'); ShowTab -> screen 0
pub const EE_SCREENS_SE_TRIGGER_TAB_TRIGGER: i32 = 0x7d0;
/// `kSETriggerTabCondition`: ctor UIRadioButton(2000 + 1, Language.rc 0x765c 'Condition'); ShowTab -> screen 1
pub const EE_SCREENS_SE_TRIGGER_TAB_CONDITION: i32 = 0x7d1;
/// `kSETriggerTabEffect`: ctor UIRadioButton(2000 + 2, Language.rc 0x765d 'Effect'); ShowTab -> screen 2
pub const EE_SCREENS_SE_TRIGGER_TAB_EFFECT: i32 = 0x7d2;
/// `kSETriggerTabObject`: ctor UIRadioButton(2000 + 3, Language.rc 0x765e 'Object'); ShowTab -> screen 3
pub const EE_SCREENS_SE_TRIGGER_TAB_OBJECT: i32 = 0x7d3;
/// `kSETriggerTabArea`: ctor UIRadioButton(2000 + 4, Language.rc 0x765f 'Area'); ShowTab -> screen 4
pub const EE_SCREENS_SE_TRIGGER_TAB_AREA: i32 = 0x7d4;

// Unit tab list sort modes (EESEUnit::GetSortMode: the Alpha/Epoch/Family radio buttons; EEUIFSELBUnitList::SetSortMode 0x599d49 comparators).
// (C++ enum `EESEUnitSortMode`)
/// `kUnitSortAlpha`: GetSortMode: radio 0x3ee Language.rc 0x5038 'Alpha'; name comparator
pub const EE_SCREENS_SE_UNIT_SORT_ALPHA: i32 = 0;
/// `kUnitSortEpoch`: radio 0x3ef Language.rc 0x5039 'Epoch'; EESEUnitEpochLess
pub const EE_SCREENS_SE_UNIT_SORT_EPOCH: i32 = 1;
/// `kUnitSortFamily`: radio 0x3f0 Language.rc 0x503a 'Family'; EESEUnitLayerLess (mL1C8 = family)
pub const EE_SCREENS_SE_UNIT_SORT_FAMILY: i32 = 2;

// Unit tab unit type filter (EEUIFSEBUnitType::Check sets sUnitTypeMode; EESEUnit::RefreshUnitList filters by it; sUnitTypeButtons 0x8c4458 help strings).
// (C++ enum `EESEUnitTypeMode`)
/// `kUnitPaletteLandUnits`: button 0x406 help Language.rc 8501 'Land Units'; RefreshUnitList mDomains & 2
pub const EE_SCREENS_SE_UNIT_PALETTE_LAND_UNITS: i32 = 0;
/// `kUnitPaletteAirUnits`: button 0x407 help 8505 'Air Units'; RefreshUnitList mDomains & 1 or & 8
pub const EE_SCREENS_SE_UNIT_PALETTE_AIR_UNITS: i32 = 1;
/// `kUnitPaletteNavalUnits`: button 0x408 help 8502 'Naval Units'; RefreshUnitList mDomains & 4
pub const EE_SCREENS_SE_UNIT_PALETTE_NAVAL_UNITS: i32 = 2;
/// `kUnitPaletteStructures`: button 0x409 help 8504 'Structures'; RefreshUnitList IsLandBuilding
pub const EE_SCREENS_SE_UNIT_PALETTE_STRUCTURES: i32 = 3;
/// `kUnitPaletteHeroes`: button 0x40a help 8503 'Heroes'; RefreshUnitList hero category
pub const EE_SCREENS_SE_UNIT_PALETTE_HEROES: i32 = 4;
/// `kUnitPaletteResources`: button 0x40b help 8500 'Resources'; RefreshUnitList remainder
pub const EE_SCREENS_SE_UNIT_PALETTE_RESOURCES: i32 = 5;

// EESETSUICBPlayer::mFlags (+0xe0): the entries FillList 0x597014 shows, one per bit (the entry's string)
// (C++ enum `EESETSPlayerListFlag`)
/// `kPlayerListWorld`: FillList: IDS_TRIGGER_PLAYER_WORLD
pub const EE_SCREENS_SE_PLAYER_LIST_WORLD: i32 = 1;
/// `kPlayerListAnyHuman`: FillList: IDS_ANY_HUMAN
pub const EE_SCREENS_SE_PLAYER_LIST_ANY_HUMAN: i32 = 2;
/// `kPlayerListAnyComputer`: FillList: IDS_ANY_COMPUTER
pub const EE_SCREENS_SE_PLAYER_LIST_ANY_COMPUTER: i32 = 4;
/// `kPlayerListAllHuman`: FillList: IDS_ALL_HUMAN
pub const EE_SCREENS_SE_PLAYER_LIST_ALL_HUMAN: i32 = 8;
/// `kPlayerListAllComputer`: FillList: IDS_ALL_COMPUTER
pub const EE_SCREENS_SE_PLAYER_LIST_ALL_COMPUTER: i32 = 0x10;
/// `kPlayerListAnyPlayer`: FillList: IDS_ANY_PLAYER
pub const EE_SCREENS_SE_PLAYER_LIST_ANY_PLAYER: i32 = 0x20;
/// `kPlayerListAnyExcludeWorld`: FillList: IDS_ANY_EXCLUDE_WORLD
pub const EE_SCREENS_SE_PLAYER_LIST_ANY_EXCLUDE_WORLD: i32 = 0x40;
/// `kPlayerListPlayers`: FillList: IDS_TRIGGER_PLAYER once per player (1..mPlayer)
pub const EE_SCREENS_SE_PLAYER_LIST_PLAYERS: i32 = 0x80;

// Control ids (UIForm::GetControlById, dbuicontrols id +0x28c) of the civilization-builder forms; names from the
// dbuicontrols names ("CB " dropped).
// (C++ enum `EECivBuilderControlID`)
/// `kCivBuilderIdCivNameMainTitle`: dbuicontrols form 7 id 196 "CB Civ Name (Main Title)"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_CIV_NAME_MAIN_TITLE: i32 = 0xc4;
/// `kCivBuilderIdCivPtsLeftLabel`: dbuicontrols form 7 id 209 "CB Civ Pts Left Label"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_CIV_PTS_LEFT_LABEL: i32 = 0xd1;
/// `kCivBuilderIdCivPtsLeft`: dbuicontrols form 7 id 210 "CB Civ Pts Left (#)"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_CIV_PTS_LEFT: i32 = 0xd2;
/// `kCivBuilderIdNewCivNameTextField`: dbuicontrols form 15 id 410 "CB New Civ: Name Text Field"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_NEW_CIV_NAME_TEXT_FIELD: i32 = 0x19a;
/// `kCivBuilderIdSaveCivFileListBox`: dbuicontrols form 16 id 420 "CB Save Civ: File List Box"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_SAVE_CIV_FILE_LIST_BOX: i32 = 0x1a4;
/// `kCivBuilderIdLoadCivFileListBox`: dbuicontrols form 17 id 425 "CB Load Civ: File List Box"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_LOAD_CIV_FILE_LIST_BOX: i32 = 0x1a9;
/// `kCivBuilderIdCivAcceptChangesButton`: dbuicontrols form 7 id 448 "CB Civ Accept Changes Button"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_CIV_ACCEPT_CHANGES_BUTTON: i32 = 0x1c0;
/// `kCivBuilderIdSaveCivNameTextField`: dbuicontrols form 16 id 562 "CB Save Civ: Name Text Field"
pub const EE_SCREENS_SE_CIV_BUILDER_ID_SAVE_CIV_NAME_TEXT_FIELD: i32 = 0x232;

// Control ids of the EEUIFSEElevation panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESEElevationControlID`)
/// `kSEElevationRaiseButton`: EEUIFSEElevation.cpp:141 mRaiseButton = new UIRadioButton(id 0x96) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_RAISE_BUTTON: i32 = 0x96;
/// `kSEElevationLowerButton`: EEUIFSEElevation.cpp:154 mLowerButton = new UIRadioButton(id 0x97) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_LOWER_BUTTON: i32 = 0x97;
/// `kSEElevationElevationTitle`: EEUIFSEElevation.cpp:180 mElevationTitle = new UITextLabel(id 0x98) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_ELEVATION_TITLE: i32 = 0x98;
/// `kSEElevationStrengthTitle`: EEUIFSEElevation.cpp:192 mStrengthTitle = new UITextLabel(id 0x99) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_STRENGTH_TITLE: i32 = 0x99;
/// `kSEElevationElevationSlider`: EEUIFSEElevation.cpp:216 mElevationSlider = new EEUIFSEESElevation(id 0x9d) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_ELEVATION_SLIDER: i32 = 0x9d;
/// `kSEElevationStrengthLabel`: EEUIFSEElevation.cpp:205 mStrengthLabel = new UITextLabel(id 0x9f) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_STRENGTH_LABEL: i32 = 0x9f;
/// `kSEElevationLevelButton`: EEUIFSEElevation.cpp:166 mLevelButton = new UIRadioButton(id 0xa0) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_LEVEL_BUTTON: i32 = 0xa0;
/// `kSEElevationPaint`: EEUIFSEElevation.cpp:135 mPaint = new EEUIFSEPaintElevation(id 0xa1) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_PAINT: i32 = 0xa1;
/// `kSEElevationStrengthSlider`: EEUIFSEElevation.cpp:227 mStrengthSlider = new UISlider(id 0xa2) (provisional name)
pub const EE_SCREENS_SE_ELEVATION_STRENGTH_SLIDER: i32 = 0xa2;

// Control ids of the EESEInstructions panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESEInstructionsControlID`)
/// `kSEInstructionsLocalizationEditBox`: EESEInstructions.cpp:354 mLocalizationEditBox = new UIEditBox(id 0x45b) (provisional name)
pub const EE_SCREENS_SE_INSTRUCTIONS_LOCALIZATION_EDIT_BOX: i32 = 0x45b;

// Control ids of the EEUIFSEMap panel (scenario editor): the ids its controls are built with (the ctor id argument);
// names from the member each control is stored in (provisional names).
// (C++ enum `EESEMapControlID`)
/// `kSEMapDefaultTerrainLabel`: EEUIFSEMap.cpp:296 mDefaultTerrainLabel = new UITextLabel(id 0x64) (provisional name)
pub const EE_SCREENS_SE_MAP_DEFAULT_TERRAIN_LABEL: i32 = 0x64;
/// `kSEMapWidthLabel`: EEUIFSEMap.cpp:242 mWidthLabel = new UITextLabel(id 0x66) (provisional name)
pub const EE_SCREENS_SE_MAP_WIDTH_LABEL: i32 = 0x66;
/// `kSEMapHeightLabel`: EEUIFSEMap.cpp:254 mHeightLabel = new UITextLabel(id 0x67) (provisional name)
pub const EE_SCREENS_SE_MAP_HEIGHT_LABEL: i32 = 0x67;
/// `kSEMapMapStyleLabel`: EEUIFSEMap.cpp:311 mMapStyleLabel = new UITextLabel(id 0x68) (provisional name)
pub const EE_SCREENS_SE_MAP_MAP_STYLE_LABEL: i32 = 0x68;
/// `kSEMapDefaultTerrain`: EEUIFSEMap.cpp:353 mDefaultTerrain = new UIComboBox(id 0x69) (provisional name)
pub const EE_SCREENS_SE_MAP_DEFAULT_TERRAIN: i32 = 0x69;
/// `kSEMapMapStyle`: EEUIFSEMap.cpp:328 mMapStyle = new UIComboBox(id 0x6a) (provisional name)
pub const EE_SCREENS_SE_MAP_MAP_STYLE: i32 = 0x6a;
/// `kSEMapWidth`: EEUIFSEMap.cpp:271 mWidth = new UIComboBox(id 0x6b) (provisional name)
pub const EE_SCREENS_SE_MAP_WIDTH: i32 = 0x6b;
/// `kSEMapHeight`: EEUIFSEMap.cpp:276 mHeight = new UIComboBox(id 0x6c) (provisional name)
pub const EE_SCREENS_SE_MAP_HEIGHT: i32 = 0x6c;
/// `kSEMapSeedEditBox`: EEUIFSEMap.cpp:370 mSeedEditBox = new UIEditBox(id 0x70) (provisional name)
pub const EE_SCREENS_SE_MAP_SEED_EDIT_BOX: i32 = 0x70;
/// `kSEMapSeedLabel`: EEUIFSEMap.cpp:383 mSeedLabel = new UITextLabel(id 0x71) (provisional name)
pub const EE_SCREENS_SE_MAP_SEED_LABEL: i32 = 0x71;
/// `kSEMapRandomMapSize`: EEUIFSEMap.cpp:470 mRandomMapSize = new UIComboBox(id 0x72) (provisional name)
pub const EE_SCREENS_SE_MAP_RANDOM_MAP_SIZE: i32 = 0x72;
/// `kSEMapRandomMapSizeLabel`: EEUIFSEMap.cpp:450 mRandomMapSizeLabel = new UITextLabel(id 0x73) (provisional name)
pub const EE_SCREENS_SE_MAP_RANDOM_MAP_SIZE_LABEL: i32 = 0x73;
/// `kSEMapElevationFileLabel`: EEUIFSEMap.cpp:493 mElevationFileLabel = new UITextLabel(id 0x74) (provisional name)
pub const EE_SCREENS_SE_MAP_ELEVATION_FILE_LABEL: i32 = 0x74;
/// `kSEMapElevationFile`: EEUIFSEMap.cpp:506 mElevationFile = new EEUIFileComboBox(id 0x75) (provisional name)
pub const EE_SCREENS_SE_MAP_ELEVATION_FILE: i32 = 0x75;
/// `kSEMapLandCutoffLabel`: EEUIFSEMap.cpp:602 mLandCutoffLabel = new UITextLabel(id 0x76) (provisional name)
pub const EE_SCREENS_SE_MAP_LAND_CUTOFF_LABEL: i32 = 0x76;
/// `kSEMapLandHeightLabel`: EEUIFSEMap.cpp:576 mLandHeightLabel = new UITextLabel(id 0x77) (provisional name)
pub const EE_SCREENS_SE_MAP_LAND_HEIGHT_LABEL: i32 = 0x77;
/// `kSEMapLandCutoff`: EEUIFSEMap.cpp:614 mLandCutoff = new UIEditBox(id 0x78) (provisional name)
pub const EE_SCREENS_SE_MAP_LAND_CUTOFF: i32 = 0x78;
/// `kSEMapLandHeight`: EEUIFSEMap.cpp:588 mLandHeight = new UIEditBox(id 0x79) (provisional name)
pub const EE_SCREENS_SE_MAP_LAND_HEIGHT: i32 = 0x79;
/// `kSEMapWaterCutoffLabel`: EEUIFSEMap.cpp:550 mWaterCutoffLabel = new UITextLabel(id 0x7a) (provisional name)
pub const EE_SCREENS_SE_MAP_WATER_CUTOFF_LABEL: i32 = 0x7a;
/// `kSEMapWaterDepthLabel`: EEUIFSEMap.cpp:524 mWaterDepthLabel = new UITextLabel(id 0x7b) (provisional name)
pub const EE_SCREENS_SE_MAP_WATER_DEPTH_LABEL: i32 = 0x7b;
/// `kSEMapWaterDepth`: EEUIFSEMap.cpp:536 mWaterDepth = new UIEditBox(id 0x7c) (provisional name)
pub const EE_SCREENS_SE_MAP_WATER_DEPTH: i32 = 0x7c;
/// `kSEMapWaterCutoff`: EEUIFSEMap.cpp:562 mWaterCutoff = new UIEditBox(id 0x7d) (provisional name)
pub const EE_SCREENS_SE_MAP_WATER_CUTOFF: i32 = 0x7d;

// Control ids of the EESERBUnitTypes panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESERBUnitTypesControlID`)
/// `kSERBUnitTypesTechList`: EESERBUnitTypes.cpp:647 mTechList = new EESEUITechListBox(id 0x154) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_TECH_LIST: i32 = 0x154;
/// `kSERBUnitTypesOtherTechList`: EESERBUnitTypes.cpp:682 mOtherTechList = new EESEUITechListBox(id 0x155) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_OTHER_TECH_LIST: i32 = 0x155;
/// `kSERBUnitTypesAdd`: EESERBUnitTypes.cpp:658 mAdd = new EESEBSelect(id 0x156) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_ADD: i32 = 0x156;
/// `kSERBUnitTypesRemove`: EESERBUnitTypes.cpp:664 mRemove = new EESEBSelect(id 0x157) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_REMOVE: i32 = 0x157;
/// `kSERBUnitTypesAddAll`: EESERBUnitTypes.cpp:670 mAddAll = new EESEBSelect(id 0x158) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_ADD_ALL: i32 = 0x158;
/// `kSERBUnitTypesRemoveAll`: EESERBUnitTypes.cpp:676 mRemoveAll = new EESEBSelect(id 0x159) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_REMOVE_ALL: i32 = 0x159;
/// `kSERBUnitTypesDisplayEpoch`: EESERBUnitTypes.cpp:694 mDisplayEpoch = new UICheckBox(id 0x15e) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_DISPLAY_EPOCH: i32 = 0x15e;
/// `kSERBUnitTypesPerPlayerCountLabel`: EESERBUnitTypes.cpp:1233 mPerPlayerCountLabel = new UITextLabel(id 0x162) (provisional name)
pub const EE_SCREENS_SE_RB_UNIT_TYPES_PER_PLAYER_COUNT_LABEL: i32 = 0x162;

// Control ids of the EESETSCondition panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESETSConditionControlID`)
/// `kSETSConditionTitleLabel`: EESETSCondition.cpp:99 mTitleLabel = new UITextLabel(id 0x8fc) (provisional name)
pub const EE_SCREENS_SE_TS_CONDITION_TITLE_LABEL: i32 = 0x8fc;
/// `kSETSConditionTypeComboBox`: EESETSCondition.cpp:112 mTypeComboBox = new UIComboBox(id 0x8fd) (provisional name)
pub const EE_SCREENS_SE_TS_CONDITION_TYPE_COMBO_BOX: i32 = 0x8fd;

// Control ids of the EESETSEComboBox panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESETSEComboBoxControlID`)
/// `kSETSEComboBoxTextEditBox`: EESETSEComboBox.cpp:191 mTextEditBox = new UIEditBox(id 0x983) (provisional name)
pub const EE_SCREENS_SE_TSE_COMBO_BOX_TEXT_EDIT_BOX: i32 = 0x983;

// Control ids of the EESETSEffect panel (scenario editor): the ids its controls are built with (the ctor id argument);
// names from the member each control is stored in (provisional names).
// (C++ enum `EESETSEffectControlID`)
/// `kSETSEffectTitleLabel`: EESETSEffect.cpp:110 mTitleLabel = new UITextLabel(id 0x960) (provisional name)
pub const EE_SCREENS_SE_TS_EFFECT_TITLE_LABEL: i32 = 0x960;
/// `kSETSEffectTypeComboBox`: EESETSEffect.cpp:123 mTypeComboBox = new UIComboBox(id 0x961) (provisional name)
pub const EE_SCREENS_SE_TS_EFFECT_TYPE_COMBO_BOX: i32 = 0x961;

// Control ids of the EESETSObject panel (scenario editor): the ids its controls are built with (the ctor id argument);
// names from the member each control is stored in (provisional names).
// (C++ enum `EESETSObjectControlID`)
/// `kSETSObjectUnitsRadio`: EESETSObject.cpp:168 mUnitsRadio = new UIRadioButton(id 0x834) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_UNITS_RADIO: i32 = 0x834;
/// `kSETSObjectAreaCheck`: EESETSObject.cpp:400 mAreaCheck = new UITriStateCheckBox(id 0x846) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_AREA_CHECK: i32 = 0x846;
/// `kSETSObjectNearCheck`: EESETSObject.cpp:430 mNearCheck = new UITriStateCheckBox(id 0x848) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_NEAR_CHECK: i32 = 0x848;
/// `kSETSObjectVisibleByCheck`: EESETSObject.cpp:563 mVisibleByCheck = new UITriStateCheckBox(id 0x84e) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_VISIBLE_BY_CHECK: i32 = 0x84e;
/// `kSETSObjectHasStateCheck`: EESETSObject.cpp:593 mHasStateCheck = new UITriStateCheckBox(id 0x850) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_HAS_STATE_CHECK: i32 = 0x850;
/// `kSETSObjectSelectedByCheck`: EESETSObject.cpp:623 mSelectedByCheck = new UITriStateCheckBox(id 0x852) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_SELECTED_BY_CHECK: i32 = 0x852;
/// `kSETSObjectSelectUnits`: EESETSObject.cpp:161 mSelectUnits = new EESEUICTriggerSelectUnits(id 0x855) (provisional name)
pub const EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS: i32 = 0x855;

// Control ids of the EESETSTrigger panel (scenario editor): the ids its controls are built with (the ctor id argument);
// names from the member each control is stored in (provisional names).
// (C++ enum `EESETSTriggerControlID`)
/// `kSETSTriggerSecondsLabel`: EESETSTrigger.cpp:179 mSecondsLabel = new UITextLabel(id 0x9cb) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_SECONDS_LABEL: i32 = 0x9cb;
/// `kSETSTriggerPrevConditionsButton`: EESETSTrigger.cpp:433 mPrevConditionsButton = new UIButton(id 0x9dc) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_PREV_CONDITIONS_BUTTON: i32 = 0x9dc;
/// `kSETSTriggerNextConditionsButton`: EESETSTrigger.cpp:442 mNextConditionsButton = new UIButton(id 0x9dd) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_NEXT_CONDITIONS_BUTTON: i32 = 0x9dd;
/// `kSETSTriggerPrevEffectsButton`: EESETSTrigger.cpp:451 mPrevEffectsButton = new UIButton(id 0x9de) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_PREV_EFFECTS_BUTTON: i32 = 0x9de;
/// `kSETSTriggerNextEffectsButton`: EESETSTrigger.cpp:460 mNextEffectsButton = new UIButton(id 0x9df) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_NEXT_EFFECTS_BUTTON: i32 = 0x9df;
/// `kSETSTriggerConditionDurationEditBox`: EESETSTrigger.cpp:491 mConditionDurationEditBox = new UIEditBox(id 0x9e2) (provisional name)
pub const EE_SCREENS_SE_TS_TRIGGER_CONDITION_DURATION_EDIT_BOX: i32 = 0x9e2;

// Control ids of the EEUIFSETerrain panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESETerrainControlID`)
/// `kSETerrainTerrainButton`: EEUIFSETerrain.cpp:365 mTerrainButton = new EEUIFSETRBPaintType(id 0xc8) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_TERRAIN_BUTTON: i32 = 0xc8;
/// `kSETerrainBitmap`: EEUIFSETerrain.cpp:457 mBitmap = new UIBitmap(id 0xc9) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_BITMAP: i32 = 0xc9;
/// `kSETerrainMountainsButton`: EEUIFSETerrain.cpp:374 mMountainsButton = new EEUIFSETRBPaintType(id 0xca) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_MOUNTAINS_BUTTON: i32 = 0xca;
/// `kSETerrainTerrainTypeLabel`: EEUIFSETerrain.cpp:383 mTerrainTypeLabel = new UITextLabel(id 0xcb) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE_LABEL: i32 = 0xcb;
/// `kSETerrainTerrainType`: EEUIFSETerrain.cpp:417 mTerrainType = new EESEUITerrainTypeLB(id 0xcc) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_TERRAIN_TYPE: i32 = 0xcc;
/// `kSETerrainTerrainColorTypeLabel`: EEUIFSETerrain.cpp:393 mTerrainColorTypeLabel = new UITextLabel(id 0xce) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE_LABEL: i32 = 0xce;
/// `kSETerrainTerrainColorType`: EEUIFSETerrain.cpp:437 mTerrainColorType = new EESEUITerrainColorLB(id 0xcf) (provisional name)
pub const EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE: i32 = 0xcf;

// Control ids of the EESETriggerScreen panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EESETriggerScreenControlID`)
/// `kSETriggerScreenDuplicateButton`: EESETriggerScreen.cpp:169 mDuplicateButton = new UIButton(id 0x7d6) (provisional name)
pub const EE_SCREENS_SE_TRIGGER_SCREEN_DUPLICATE_BUTTON: i32 = 0x7d6;
/// `kSETriggerScreenDeleteButton`: EESETriggerScreen.cpp:183 mDeleteButton = new UIButton(id 0x7d7) (provisional name)
pub const EE_SCREENS_SE_TRIGGER_SCREEN_DELETE_BUTTON: i32 = 0x7d7;
/// `kSETriggerScreenNameEditBox`: EESETriggerScreen.cpp:254 mNameEditBox = new UIEditBox(id 0x7db) (provisional name)
pub const EE_SCREENS_SE_TRIGGER_SCREEN_NAME_EDIT_BOX: i32 = 0x7db;
/// `kSETriggerScreenNameLabel`: EESETriggerScreen.cpp:244 mNameLabel = new UITextLabel(id 0x7dc) (provisional name)
pub const EE_SCREENS_SE_TRIGGER_SCREEN_NAME_LABEL: i32 = 0x7dc;

// Control ids of the EEUIFScenarioEditor panel (scenario editor): the ids its controls are built with (the ctor id
// argument); names from the member each control is stored in (provisional names).
// (C++ enum `EEScenarioEditorControlID`)
/// `kScenarioEditorHideUICheckBox`: EEUIFScenarioEditor.cpp:436 mHideUICheckBox = new UICheckBox(id 0x12) (provisional name)
pub const EE_SCREENS_SE_SCENARIO_EDITOR_HIDE_UI_CHECK_BOX: i32 = 0x12;

#[cfg(test)]
mod tests {
    use super::*;

    /// The values the C++ header (the reconstruction) had, a sample of each table.
    #[test]
    fn values_match_the_reconstruction() {
        assert_eq!(EE_SCREENS_SE_EDITOR_TAB_MAP, 0x0); // kEditorTabMap
        assert_eq!(EE_SCREENS_SE_LOAD_THREAD_CREATE_GHOST_UNIT, 0x402); // kLoadThreadCreateGhostUnit
        assert_eq!(EE_SCREENS_SE_PLAYER_SCREEN_AI_SETTINGS, 0x2); // kPlayerScreenAISettings
        assert_eq!(EE_SCREENS_SE_TECH_LIST_UNITS, 0x0); // kSETechListUnits
        assert_eq!(EE_SCREENS_SE_TRIGGER_SCREEN_EFFECT, 0x2); // kSETriggerScreenEffect
        assert_eq!(EE_SCREENS_SE_UNIT_PALETTE_AIR_UNITS, 0x1); // kUnitPaletteAirUnits
        assert_eq!(EE_SCREENS_SE_PLAYER_LIST_PLAYERS, 0x80); // kPlayerListPlayers
        assert_eq!(EE_SCREENS_SE_ELEVATION_STRENGTH_TITLE, 0x99); // kSEElevationStrengthTitle
        assert_eq!(EE_SCREENS_SE_MAP_MAP_STYLE, 0x6a); // kSEMapMapStyle
        assert_eq!(EE_SCREENS_SE_MAP_LAND_HEIGHT, 0x79); // kSEMapLandHeight
        assert_eq!(EE_SCREENS_SE_RB_UNIT_TYPES_PER_PLAYER_COUNT_LABEL, 0x162); // kSERBUnitTypesPerPlayerCountLabel
        assert_eq!(EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS, 0x855); // kSETSObjectSelectUnits
        assert_eq!(EE_SCREENS_SE_TERRAIN_TERRAIN_COLOR_TYPE_LABEL, 0xce); // kSETerrainTerrainColorTypeLabel
    }
}
