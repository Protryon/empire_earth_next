//! The scenario manager's named integer constants (once the reconstruction's `ee/EEScenarioIDs.h`): the
//! effect, condition, action and attribute kinds the triggers store. Each doc gives the evidence for its name;
//! "(provisional name)" marks an inferred one. The C++ reads them from `rust/sim/scenario.h`
//! (`kSMEffectObject` is `EE_SCENARIO_SM_EFFECT_OBJECT`).

// AI effect actions (EESMEffect::mAIAction; EESETSETAI::mTypes; Execute mType 6 switch).
// (C++ enum `EESMAIAction`)
/// `kSMAIActionModifyAIMilitaryTarget`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29690 (0x73fa) "Modify AI Military Target"
pub const EE_SCENARIO_SM_AI_ACTION_MODIFY_AI_MILITARY_TARGET: i32 = 0;
/// `kSMAIActionCreateAIMilitaryGroup`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29691 (0x73fb) "Create AI Military Group"
pub const EE_SCENARIO_SM_AI_ACTION_CREATE_AI_MILITARY_GROUP: i32 = 1;
/// `kSMAIActionBuildObject`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29692 (0x73fc) "Build Object"
pub const EE_SCENARIO_SM_AI_ACTION_BUILD_OBJECT: i32 = 2;
/// `kSMAIActionMaintainObject`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29693 (0x73fd) "Maintain Object"
pub const EE_SCENARIO_SM_AI_ACTION_MAINTAIN_OBJECT: i32 = 3;
/// `kSMAIActionSetAIToggle`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29694 (0x73fe) "Set AI Toggle"
pub const EE_SCENARIO_SM_AI_ACTION_SET_AI_TOGGLE: i32 = 4;
/// `kSMAIActionSetAIVariable`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29695 (0x73ff) "Set AI Variable"
pub const EE_SCENARIO_SM_AI_ACTION_SET_AI_VARIABLE: i32 = 5;
/// `kSMAIActionSetFavoriteUnits`: EESETSETAI "Action" list 0x73fa+i (GetString(mCurrentType + 0x73fa)): Language.rc 29696 (0x7400) "Set Favorite Units"
pub const EE_SCENARIO_SM_AI_ACTION_SET_FAVORITE_UNITS: i32 = 6;

// AI toggles (EESMEffect::mAIToggle; SetAIToggle 0x5c26da switch).
// (C++ enum `EESMAIToggle`)
/// `kSMAIToggleActive`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29625 (0x73b9) "Active"
pub const EE_SCENARIO_SM_AI_TOGGLE_ACTIVE: i32 = 0;
/// `kSMAIToggleCheat`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29626 (0x73ba) "Cheat"
pub const EE_SCENARIO_SM_AI_TOGGLE_CHEAT: i32 = 1;
/// `kSMAIToggleAttack`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29627 (0x73bb) "Attack"
pub const EE_SCENARIO_SM_AI_TOGGLE_ATTACK: i32 = 2;
/// `kSMAIToggleDefend`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29628 (0x73bc) "Defend"
pub const EE_SCENARIO_SM_AI_TOGGLE_DEFEND: i32 = 3;
/// `kSMAIToggleBuildPerRepair`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29629 (0x73bd) "Build/Repair"
pub const EE_SCENARIO_SM_AI_TOGGLE_BUILD_PER_REPAIR: i32 = 4;
/// `kSMAIToggleExplore`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29630 (0x73be) "Explore"
pub const EE_SCENARIO_SM_AI_TOGGLE_EXPLORE: i32 = 5;
/// `kSMAIToggleIgnore`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29631 (0x73bf) "Ignore"
pub const EE_SCENARIO_SM_AI_TOGGLE_IGNORE: i32 = 6;
/// `kSMAIToggleGather`: Set AI Toggle "AI Toggle" list 0x73b9+i (EESETSETAI): Language.rc 29632 (0x73c0) "Gather"
pub const EE_SCENARIO_SM_AI_TOGGLE_GATHER: i32 = 7;

// Condition comparison operators (EESMCondition::Compare 0x5ba998; editor list 0x727e..0x7282).
// (C++ enum `EESMCompareOperator`)
/// `kSMCompareLessThan`: condition operator list 0x727e+i: Language.rc 29310 "<"; EESMCondition::Compare case 0
pub const EE_SCENARIO_SM_COMPARE_LESS_THAN: i32 = 0;
/// `kSMCompareLessOrEqual`: condition operator list 0x727e+i: Language.rc 29311 "<="; EESMCondition::Compare case 1
pub const EE_SCENARIO_SM_COMPARE_LESS_OR_EQUAL: i32 = 1;
/// `kSMCompareEqual`: condition operator list 0x727e+i: Language.rc 29312 "="; EESMCondition::Compare case 2
pub const EE_SCENARIO_SM_COMPARE_EQUAL: i32 = 2;
/// `kSMCompareGreaterOrEqual`: condition operator list 0x727e+i: Language.rc 29313 ">="; EESMCondition::Compare case 3
pub const EE_SCENARIO_SM_COMPARE_GREATER_OR_EQUAL: i32 = 3;
/// `kSMCompareGreater`: condition operator list 0x727e+i: Language.rc 29314 ">"; EESMCondition::Compare case 4
pub const EE_SCENARIO_SM_COMPARE_GREATER: i32 = 4;

// Scenario condition kinds (EESMCondition::mType; EESMCondition::Evaluate 0x5baabf switch; EESETSCondition::mTypes index).
// (C++ enum `EESMConditionType`)
/// `kSMConditionAlwaysTrue`: EESETSCondition mTypes[0] = EESETSCTAlwaysTrue; EESMCondition::Evaluate case 0 returns true (provisional name)
pub const EE_SCENARIO_SM_CONDITION_ALWAYS_TRUE: i32 = 0;
/// `kSMConditionObject`: the scenario-object condition (EEScenarioManager::NewObject stores 1 with mObjectIndex; Evaluate case 1 = EESMObject::Evaluate) (provisional name)
pub const EE_SCENARIO_SM_CONDITION_OBJECT: i32 = 1;
/// `kSMConditionPlayerAttribute`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29300 "Player-Attribute"
pub const EE_SCENARIO_SM_CONDITION_PLAYER_ATTRIBUTE: i32 = 2;
/// `kSMConditionPlayerTechnology`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29301 "Player-Technology"
pub const EE_SCENARIO_SM_CONDITION_PLAYER_TECHNOLOGY: i32 = 3;
/// `kSMConditionTriggerAttribute`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29302 "Trigger- Attribute"
pub const EE_SCENARIO_SM_CONDITION_TRIGGER_ATTRIBUTE: i32 = 4;
/// `kSMConditionPlayerDiplomacy`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29303 "Player-Diplomacy"
pub const EE_SCENARIO_SM_CONDITION_PLAYER_DIPLOMACY: i32 = 5;
/// `kSMConditionGameAttribute`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29304 "Game-Attribute"
pub const EE_SCENARIO_SM_CONDITION_GAME_ATTRIBUTE: i32 = 6;
/// `kSMConditionChatMessageContains`: EESETSCondition::RefreshControls GetString(mType + 0x7272): Language.rc 29305 "Chat Message Contains"
pub const EE_SCENARIO_SM_CONDITION_CHAT_MESSAGE_CONTAINS: i32 = 7;

// Scenario effect kinds (EESMEffect::mType; EESETSEffect::mTypes index; EESMEffect::Execute 0x5bd5eb switch).
// (C++ enum `EESMEffectType`)
/// `kSMEffectNone`: EESETSEffect mTypes[0] = EESETSETNoEffect; EEScenarioManager::Reset stores 0 in the null effect (provisional name)
pub const EE_SCENARIO_SM_EFFECT_NONE: i32 = 0;
/// `kSMEffectObject`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29321 (0x7289) "Object"
pub const EE_SCENARIO_SM_EFFECT_OBJECT: i32 = 1;
/// `kSMEffectPlayer`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29322 (0x728a) "Player"
pub const EE_SCENARIO_SM_EFFECT_PLAYER: i32 = 2;
/// `kSMEffectTrigger`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29323 (0x728b) "Trigger"
pub const EE_SCENARIO_SM_EFFECT_TRIGGER: i32 = 3;
/// `kSMEffectMedia`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29324 (0x728c) "Media"
pub const EE_SCENARIO_SM_EFFECT_MEDIA: i32 = 4;
/// `kSMEffectGame`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29325 (0x728d) "Game"
pub const EE_SCENARIO_SM_EFFECT_GAME: i32 = 5;
/// `kSMEffectAI`: EESETSEffect ctor type names 0x7289+i for mTypes[1..6]: Language.rc 29326 (0x728e) "AI"
pub const EE_SCENARIO_SM_EFFECT_AI: i32 = 6;

// Game effect actions (EESMEffect::mGameAction; EESETSETGame::mTypes; Execute mType 5 switch).
// (C++ enum `EESMGameAction`)
/// `kSMGameActionDaylightHour`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29575 (0x7387) "Daylight Hour"
pub const EE_SCENARIO_SM_GAME_ACTION_DAYLIGHT_HOUR: i32 = 0;
/// `kSMGameActionDayPerNightCycleTime`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29576 (0x7388) "Day/Night Cycle Time"
pub const EE_SCENARIO_SM_GAME_ACTION_DAY_PER_NIGHT_CYCLE_TIME: i32 = 1;
/// `kSMGameActionCalamity`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29577 (0x7389) "Calamity"
pub const EE_SCENARIO_SM_GAME_ACTION_CALAMITY: i32 = 2;
/// `kSMGameActionGraphicEffect`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29578 (0x738a) "Graphic Effect"
pub const EE_SCENARIO_SM_GAME_ACTION_GRAPHIC_EFFECT: i32 = 3;
/// `kSMGameActionStartCinematicMode`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29579 (0x738b) "Start-Cinematic Mode"
pub const EE_SCENARIO_SM_GAME_ACTION_START_CINEMATIC_MODE: i32 = 4;
/// `kSMGameActionEndCinematicMode`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29580 (0x738c) "End-Cinematic Mode"
pub const EE_SCENARIO_SM_GAME_ACTION_END_CINEMATIC_MODE: i32 = 5;
/// `kSMGameActionAmbientWeather`: EESETSETGame "Action" list 0x7387+i (GetString(mCurrentType + 0x7387)): Language.rc 29581 (0x738d) "Ambient-Weather"
pub const EE_SCENARIO_SM_GAME_ACTION_AMBIENT_WEATHER: i32 = 6;

// Media effect actions (EESMEffect::mMediaAction; EESETSETMedia::mTypes; Execute mType 4 switch).
// (C++ enum `EESMMediaAction`)
/// `kSMMediaActionPlaySound`: EESETSETMedia "Action" list 0x7355+i (GetString(mCurrentType + 0x7355)): Language.rc 29525 (0x7355) "Play Sound"
pub const EE_SCENARIO_SM_MEDIA_ACTION_PLAY_SOUND: i32 = 0;
/// `kSMMediaActionChangeText`: EESETSETMedia "Action" list 0x7355+i (GetString(mCurrentType + 0x7355)): Language.rc 29526 (0x7356) "Change Text"
pub const EE_SCENARIO_SM_MEDIA_ACTION_CHANGE_TEXT: i32 = 1;
/// `kSMMediaActionSendDialogue`: EESETSETMedia "Action" list 0x7355+i (GetString(mCurrentType + 0x7355)): Language.rc 29527 (0x7357) "Send Dialogue"
pub const EE_SCENARIO_SM_MEDIA_ACTION_SEND_DIALOGUE: i32 = 2;
/// `kSMMediaActionSendChatMessage`: EESETSETMedia "Action" list 0x7355+i (GetString(mCurrentType + 0x7355)): Language.rc 29528 (0x7358) "Send Chat Message"
pub const EE_SCENARIO_SM_MEDIA_ACTION_SEND_CHAT_MESSAGE: i32 = 3;

// Change Text effect targets (EESMEffect::mTextObject; Execute media case 1 switch: hints/instructions/history).
// (C++ enum `EESMMediaText`)
/// `kSMMediaTextHints`: Change Text "Text Object" list 0x736e+i (EESETSETMedia): Language.rc 29550 (0x736e) "Hints"
pub const EE_SCENARIO_SM_MEDIA_TEXT_HINTS: i32 = 0;
/// `kSMMediaTextInstructions`: Change Text "Text Object" list 0x736e+i (EESETSETMedia): Language.rc 29551 (0x736f) "Instructions"
pub const EE_SCENARIO_SM_MEDIA_TEXT_INSTRUCTIONS: i32 = 1;
/// `kSMMediaTextHistory`: Change Text "Text Object" list 0x736e+i (EESETSETMedia): Language.rc 29552 (0x7370) "History"
pub const EE_SCENARIO_SM_MEDIA_TEXT_HISTORY: i32 = 2;

// Object effect actions (EESMEffect::mObjectAction; EESETSETObject::mTypes/mCurrentType; Execute mType 1 switch).
// (C++ enum `EESMObjectAction`)
/// `kSMObjectActionCreate`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29350 (0x72a6) "Create"
pub const EE_SCENARIO_SM_OBJECT_ACTION_CREATE: i32 = 0;
/// `kSMObjectActionFreeze`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29351 (0x72a7) "Freeze"
pub const EE_SCENARIO_SM_OBJECT_ACTION_FREEZE: i32 = 1;
/// `kSMObjectActionUnfreeze`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29352 (0x72a8) "Unfreeze"
pub const EE_SCENARIO_SM_OBJECT_ACTION_UNFREEZE: i32 = 2;
/// `kSMObjectActionKill`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29353 (0x72a9) "Kill"
pub const EE_SCENARIO_SM_OBJECT_ACTION_KILL: i32 = 3;
/// `kSMObjectActionRemove`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29354 (0x72aa) "Remove"
pub const EE_SCENARIO_SM_OBJECT_ACTION_REMOVE: i32 = 4;
/// `kSMObjectActionTaskObject`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29355 (0x72ab) "Task-Object"
pub const EE_SCENARIO_SM_OBJECT_ACTION_TASK_OBJECT: i32 = 5;
/// `kSMObjectActionTaskArea`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29356 (0x72ac) "Task-Area"
pub const EE_SCENARIO_SM_OBJECT_ACTION_TASK_AREA: i32 = 6;
/// `kSMObjectActionStance`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29357 (0x72ad) "Stance"
pub const EE_SCENARIO_SM_OBJECT_ACTION_STANCE: i32 = 7;
/// `kSMObjectActionUnitAttribute`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29358 (0x72ae) "Unit-Attribute"
pub const EE_SCENARIO_SM_OBJECT_ACTION_UNIT_ATTRIBUTE: i32 = 8;
/// `kSMObjectActionOwner`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29359 (0x72af) "Owner"
pub const EE_SCENARIO_SM_OBJECT_ACTION_OWNER: i32 = 9;
/// `kSMObjectActionClassName`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29360 (0x72b0) "Class-Name"
pub const EE_SCENARIO_SM_OBJECT_ACTION_CLASS_NAME: i32 = 10;
/// `kSMObjectActionFaceObject`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29361 (0x72b1) "Face Object"
pub const EE_SCENARIO_SM_OBJECT_ACTION_FACE_OBJECT: i32 = 11;
/// `kSMObjectActionClassAttribute`: EESETSETObject "Action" list 0x72a6+i (GetString(mCurrentType + 0x72a6)): Language.rc 29362 (0x72b2) "Class-Attribute"
pub const EE_SCENARIO_SM_OBJECT_ACTION_CLASS_ATTRIBUTE: i32 = 12;

// Player effect actions (EESMEffect::mPlayerAction; EESETSETPlayer::mTypes/mCurrentType; Execute mType 2 switch).
// (C++ enum `EESMPlayerAction`)
/// `kSMPlayerActionVictory`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29452 (0x730c) "Victory"
pub const EE_SCENARIO_SM_PLAYER_ACTION_VICTORY: i32 = 0;
/// `kSMPlayerActionScriptCamera`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29453 (0x730d) "Script-Camera"
pub const EE_SCENARIO_SM_PLAYER_ACTION_SCRIPT_CAMERA: i32 = 1;
/// `kSMPlayerActionDefeat`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29454 (0x730e) "Defeat"
pub const EE_SCENARIO_SM_PLAYER_ACTION_DEFEAT: i32 = 2;
/// `kSMPlayerActionDiplomacy`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29455 (0x730f) "Diplomacy"
pub const EE_SCENARIO_SM_PLAYER_ACTION_DIPLOMACY: i32 = 3;
/// `kSMPlayerActionSetAttributeLevel`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29456 (0x7310) "Set-Attribute Level"
pub const EE_SCENARIO_SM_PLAYER_ACTION_SET_ATTRIBUTE_LEVEL: i32 = 4;
/// `kSMPlayerActionModifyTechTree`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29457 (0x7311) "Modify-Tech-Tree"
pub const EE_SCENARIO_SM_PLAYER_ACTION_MODIFY_TECH_TREE: i32 = 5;
/// `kSMPlayerActionAwardCivPoints`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29458 (0x7312) "Award-Civ-Points"
pub const EE_SCENARIO_SM_PLAYER_ACTION_AWARD_CIV_POINTS: i32 = 6;
/// `kSMPlayerActionFadeIn`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29459 (0x7313) "Fade-In"
pub const EE_SCENARIO_SM_PLAYER_ACTION_FADE_IN: i32 = 7;
/// `kSMPlayerActionFadeOut`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29460 (0x7314) "Fade-Out"
pub const EE_SCENARIO_SM_PLAYER_ACTION_FADE_OUT: i32 = 8;
/// `kSMPlayerActionVisibilityOn`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29461 (0x7315) "Visibility-On"
pub const EE_SCENARIO_SM_PLAYER_ACTION_VISIBILITY_ON: i32 = 9;
/// `kSMPlayerActionVisibilityOff`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29462 (0x7316) "Visibility-Off"
pub const EE_SCENARIO_SM_PLAYER_ACTION_VISIBILITY_OFF: i32 = 10;
/// `kSMPlayerActionMiniMapFlare`: EESETSETPlayer "Action" list 0x730c+i (GetString(mCurrentType + 0x730c)): Language.rc 29463 (0x7317) "MiniMap-Flare"
pub const EE_SCENARIO_SM_PLAYER_ACTION_MINI_MAP_FLARE: i32 = 11;
/// `kSMPlayerActionTotal`: count of player actions; EESMEffect::Load resets mPlayerAction >= 12 to 0 (provisional name)
pub const EE_SCENARIO_SM_PLAYER_ACTION_TOTAL: i32 = 12;

// Player attribute ids of the player conditions/effects (EESMCondition::GetPlayerValue 0x5baf78, EESMEffect::SetPlayerAttribute 0x5bcb1f; list built in EESETSConditionType/EESETSETPlayer).
// (C++ enum `EESMPlayerValue`)
/// `kSMPlayerValueAnyResource`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 0: Language.rc 29670 "Any Resource"
pub const EE_SCENARIO_SM_PLAYER_VALUE_ANY_RESOURCE: i32 = 0;
/// `kSMPlayerValueAllResources`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 1: Language.rc 29671 "All Resources"
pub const EE_SCENARIO_SM_PLAYER_VALUE_ALL_RESOURCES: i32 = 1;
/// `kSMPlayerValueStone`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 2: Language.rc 29672 "Stone"
pub const EE_SCENARIO_SM_PLAYER_VALUE_STONE: i32 = 2;
/// `kSMPlayerValueGold`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 3: Language.rc 29673 "Gold"
pub const EE_SCENARIO_SM_PLAYER_VALUE_GOLD: i32 = 3;
/// `kSMPlayerValueWood`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 4: Language.rc 29674 "Wood"
pub const EE_SCENARIO_SM_PLAYER_VALUE_WOOD: i32 = 4;
/// `kSMPlayerValueFood`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 5: Language.rc 29675 "Food"
pub const EE_SCENARIO_SM_PLAYER_VALUE_FOOD: i32 = 5;
/// `kSMPlayerValueIron`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 6: Language.rc 29676 "Iron"
pub const EE_SCENARIO_SM_PLAYER_VALUE_IRON: i32 = 6;
/// `kSMPlayerValueCurrentPop`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 7: Language.rc 29677 "Current Pop"
pub const EE_SCENARIO_SM_PLAYER_VALUE_CURRENT_POP: i32 = 7;
/// `kSMPlayerValuePopCap`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 8: Language.rc 29678 "Pop Cap"
pub const EE_SCENARIO_SM_PLAYER_VALUE_POP_CAP: i32 = 8;
/// `kSMPlayerValueUnitCount`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 9: Language.rc 29679 "Unit Count"
pub const EE_SCENARIO_SM_PLAYER_VALUE_UNIT_COUNT: i32 = 9;
/// `kSMPlayerValueDifficultyLevel`: player attribute list (EESETSConditionType/EESETSETPlayer) entry 10: Language.rc 29680 "Difficulty Level"
pub const EE_SCENARIO_SM_PLAYER_VALUE_DIFFICULTY_LEVEL: i32 = 10;
/// `kSMPlayerValueVariable0`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 0); GetPlayerValue mScenarioVariables[0]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE0: i32 = 11;
/// `kSMPlayerValueVariable1`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 1); GetPlayerValue mScenarioVariables[1]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE1: i32 = 12;
/// `kSMPlayerValueVariable2`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 2); GetPlayerValue mScenarioVariables[2]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE2: i32 = 13;
/// `kSMPlayerValueVariable3`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 3); GetPlayerValue mScenarioVariables[3]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE3: i32 = 14;
/// `kSMPlayerValueVariable4`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 4); GetPlayerValue mScenarioVariables[4]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE4: i32 = 15;
/// `kSMPlayerValueVariable5`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 5); GetPlayerValue mScenarioVariables[5]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE5: i32 = 16;
/// `kSMPlayerValueVariable6`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 6); GetPlayerValue mScenarioVariables[6]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE6: i32 = 17;
/// `kSMPlayerValueVariable7`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 7); GetPlayerValue mScenarioVariables[7]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE7: i32 = 18;
/// `kSMPlayerValueVariable8`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 8); GetPlayerValue mScenarioVariables[8]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE8: i32 = 19;
/// `kSMPlayerValueVariable9`: list entries 11..20: sprintf("%s%d", Language.rc 0x79e0 "Variable", 9); GetPlayerValue mScenarioVariables[9]
pub const EE_SCENARIO_SM_PLAYER_VALUE_VARIABLE9: i32 = 20;
/// `kSMPlayerValueFoodSent`: list entry: Language.rc 30116 "Food" + 0x7bd6 "Sent"; GetPlayerValue mTributes[0]
pub const EE_SCENARIO_SM_PLAYER_VALUE_FOOD_SENT: i32 = 21;
/// `kSMPlayerValueFoodReceived`: list entry: Language.rc 30116 "Food" + 0x7bd8 "Received"; GetPlayerValue mTributes[5]
pub const EE_SCENARIO_SM_PLAYER_VALUE_FOOD_RECEIVED: i32 = 22;
/// `kSMPlayerValueWoodSent`: list entry: Language.rc 30117 "Wood" + 0x7bd6 "Sent"; GetPlayerValue mTributes[1]
pub const EE_SCENARIO_SM_PLAYER_VALUE_WOOD_SENT: i32 = 23;
/// `kSMPlayerValueWoodReceived`: list entry: Language.rc 30117 "Wood" + 0x7bd8 "Received"; GetPlayerValue mTributes[6]
pub const EE_SCENARIO_SM_PLAYER_VALUE_WOOD_RECEIVED: i32 = 24;
/// `kSMPlayerValueStoneSent`: list entry: Language.rc 30118 "Stone" + 0x7bd6 "Sent"; GetPlayerValue mTributes[2]
pub const EE_SCENARIO_SM_PLAYER_VALUE_STONE_SENT: i32 = 25;
/// `kSMPlayerValueStoneReceived`: list entry: Language.rc 30118 "Stone" + 0x7bd8 "Received"; GetPlayerValue mTributes[7]
pub const EE_SCENARIO_SM_PLAYER_VALUE_STONE_RECEIVED: i32 = 26;
/// `kSMPlayerValueGoldSent`: list entry: Language.rc 30119 "Gold" + 0x7bd6 "Sent"; GetPlayerValue mTributes[3]
pub const EE_SCENARIO_SM_PLAYER_VALUE_GOLD_SENT: i32 = 27;
/// `kSMPlayerValueGoldReceived`: list entry: Language.rc 30119 "Gold" + 0x7bd8 "Received"; GetPlayerValue mTributes[8]
pub const EE_SCENARIO_SM_PLAYER_VALUE_GOLD_RECEIVED: i32 = 28;
/// `kSMPlayerValueIronSent`: list entry: Language.rc 30120 "Iron" + 0x7bd6 "Sent"; GetPlayerValue mTributes[4]
pub const EE_SCENARIO_SM_PLAYER_VALUE_IRON_SENT: i32 = 29;
/// `kSMPlayerValueIronReceived`: list entry: Language.rc 30120 "Iron" + 0x7bd8 "Received"; GetPlayerValue mTributes[9]
pub const EE_SCENARIO_SM_PLAYER_VALUE_IRON_RECEIVED: i32 = 30;
/// `kSMPlayerValueStatBuildingsLost`: list entry 31: Language.rc 31201 "Buildings Lost" (statistic; GetPlayerValue mSavedGroups.GetLastValue(1))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_BUILDINGS_LOST: i32 = 31;
/// `kSMPlayerValueStatBuildingsRazed`: list entry 32: Language.rc 31202 "Buildings Razed" (statistic; GetPlayerValue mSavedGroups.GetLastValue(2))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_BUILDINGS_RAZED: i32 = 32;
/// `kSMPlayerValueStatUnitsKilled`: list entry 33: Language.rc 31203 "Units Killed" (statistic; GetPlayerValue mSavedGroups.GetLastValue(3))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_UNITS_KILLED: i32 = 33;
/// `kSMPlayerValueStatUnitsLost`: list entry 34: Language.rc 31204 "Units Lost" (statistic; GetPlayerValue mSavedGroups.GetLastValue(4))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_UNITS_LOST: i32 = 34;
/// `kSMPlayerValueStatConversions`: list entry 35: Language.rc 31205 "Conversions" (statistic; GetPlayerValue mSavedGroups.GetLastValue(5))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_CONVERSIONS: i32 = 35;
/// `kSMPlayerValueStatCalamities`: list entry 36: Language.rc 31206 "Calamities" (statistic; GetPlayerValue mSavedGroups.GetLastValue(6))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_CALAMITIES: i32 = 36;
/// `kSMPlayerValueStatFood`: list entry 37: Language.rc 31207 "Food" (statistic; GetPlayerValue mSavedGroups.GetLastValue(7))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FOOD: i32 = 37;
/// `kSMPlayerValueStatWood`: list entry 38: Language.rc 31208 "Wood" (statistic; GetPlayerValue mSavedGroups.GetLastValue(8))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_WOOD: i32 = 38;
/// `kSMPlayerValueStatGold`: list entry 39: Language.rc 31209 "Gold" (statistic; GetPlayerValue mSavedGroups.GetLastValue(9))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_GOLD: i32 = 39;
/// `kSMPlayerValueStatStone`: list entry 40: Language.rc 31210 "Stone" (statistic; GetPlayerValue mSavedGroups.GetLastValue(10))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_STONE: i32 = 40;
/// `kSMPlayerValueStatIron`: list entry 41: Language.rc 31211 "Iron" (statistic; GetPlayerValue mSavedGroups.GetLastValue(11))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_IRON: i32 = 41;
/// `kSMPlayerValueStatEpochReached`: list entry 42: Language.rc 31212 "Epoch Reached" (statistic; GetPlayerValue mSavedGroups.GetLastValue(12))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_EPOCH_REACHED: i32 = 42;
/// `kSMPlayerValueStatArmySize`: list entry 43: Language.rc 31213 "Army Size" (statistic; GetPlayerValue mSavedGroups.GetLastValue(13))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_ARMY_SIZE: i32 = 43;
/// `kSMPlayerValueStatCitizenCount`: list entry 44: Language.rc 31214 "Citizen Count" (statistic; GetPlayerValue mSavedGroups.GetLastValue(14))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_CITIZEN_COUNT: i32 = 44;
/// `kSMPlayerValueStatHeroCount`: list entry 45: Language.rc 31215 "Hero Count" (statistic; GetPlayerValue mSavedGroups.GetLastValue(15))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_HERO_COUNT: i32 = 45;
/// `kSMPlayerValueStatWonderCount`: list entry 46: Language.rc 31216 "Wonder Count" (statistic; GetPlayerValue mSavedGroups.GetLastValue(16))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_WONDER_COUNT: i32 = 46;
/// `kSMPlayerValueStatTributeSent`: list entry 47: Language.rc 31217 "Tribute Sent" (statistic; GetPlayerValue mSavedGroups.GetLastValue(17))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_TRIBUTE_SENT: i32 = 47;
/// `kSMPlayerValueStatTributeReceived`: list entry 48: Language.rc 31218 "Tribute Received" (statistic; GetPlayerValue mSavedGroups.GetLastValue(18))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_TRIBUTE_RECEIVED: i32 = 48;
/// `kSMPlayerValueStatResearchCount`: list entry 49: Language.rc 31219 "Research Count" (statistic; GetPlayerValue mSavedGroups.GetLastValue(19))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_RESEARCH_COUNT: i32 = 49;
/// `kSMPlayerValueStatHotKeysPressed`: list entry 50: Language.rc 31220 "Hot Keys Pressed" (statistic; GetPlayerValue mSavedGroups.GetLastValue(20))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_HOT_KEYS_PRESSED: i32 = 50;
/// `kSMPlayerValueStatMouseClicks`: list entry 51: Language.rc 31221 "Mouse Clicks" (statistic; GetPlayerValue mSavedGroups.GetLastValue(21))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_MOUSE_CLICKS: i32 = 51;
/// `kSMPlayerValueStatResearchAvailable`: list entry 52: Language.rc 31222 "Research Available" (statistic; GetPlayerValue mSavedGroups.GetLastValue(22))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_RESEARCH_AVAILABLE: i32 = 52;
/// `kSMPlayerValueStatTotalPopulation`: list entry 53: Language.rc 31223 "Total Population" (statistic; GetPlayerValue mSavedGroups.GetLastValue(23))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_TOTAL_POPULATION: i32 = 53;
/// `kSMPlayerValueStatTilesExplored`: list entry 54: Language.rc 31224 "Tiles Explored" (statistic; GetPlayerValue mSavedGroups.GetLastValue(24))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_TILES_EXPLORED: i32 = 54;
/// `kSMPlayerValueStatFutureUse1`: list entry 55: Language.rc 31225 "Stat - for future use1" (statistic; GetPlayerValue mSavedGroups.GetLastValue(25))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE1: i32 = 55;
/// `kSMPlayerValueStatFutureUse2`: list entry 56: Language.rc 31226 "Stat - for future use2" (statistic; GetPlayerValue mSavedGroups.GetLastValue(26))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE2: i32 = 56;
/// `kSMPlayerValueStatFutureUse3`: list entry 57: Language.rc 31227 "Stat - for future use3" (statistic; GetPlayerValue mSavedGroups.GetLastValue(27))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE3: i32 = 57;
/// `kSMPlayerValueStatFutureUse4`: list entry 58: Language.rc 31228 "Stat - for future use4" (statistic; GetPlayerValue mSavedGroups.GetLastValue(28))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE4: i32 = 58;
/// `kSMPlayerValueStatFutureUse5`: list entry 59: Language.rc 31229 "Stat - for future use5" (statistic; GetPlayerValue mSavedGroups.GetLastValue(29))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE5: i32 = 59;
/// `kSMPlayerValueStatFutureUse6`: list entry 60: Language.rc 31230 "Stat - for future use6" (statistic; GetPlayerValue mSavedGroups.GetLastValue(30))
pub const EE_SCENARIO_SM_PLAYER_VALUE_STAT_FUTURE_USE6: i32 = 60;
/// `kSMPlayerValueMiningLimit`: list entry 61: Language.rc 29681 "Mining Limit"
pub const EE_SCENARIO_SM_PLAYER_VALUE_MINING_LIMIT: i32 = 61;
/// `kSMPlayerValuePlaceholder1`: list entry 62: Language.rc 29682 "Placeholder1"
pub const EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER1: i32 = 62;
/// `kSMPlayerValuePlaceholder2`: list entry 63: Language.rc 29683 "Placeholder2"
pub const EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER2: i32 = 63;
/// `kSMPlayerValuePlaceholder3`: list entry 64: Language.rc 29684 "Placeholder3"
pub const EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER3: i32 = 64;
/// `kSMPlayerValuePlaceholder4`: list entry 65: Language.rc 29685 "Placholder4"
pub const EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER4: i32 = 65;
/// `kSMPlayerValuePlaceholder5`: list entry 66: Language.rc 29686 "Placeholder5"
pub const EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER5: i32 = 66;

// Set-Attribute Level modes (EESMEffect::SetPlayerAttribute inMode: 0 sets, 2 negates the amount).
// (C++ enum `EESMPlayerValueMode`)
/// `kSMValueModeToExactly`: Set-Attribute Level mode list 0x733c+i (EESETSETPlayer): Language.rc 29500 (0x733c) "To Exactly"
pub const EE_SCENARIO_SM_VALUE_MODE_TO_EXACTLY: i32 = 0;
/// `kSMValueModeToIncreaseBy`: Set-Attribute Level mode list 0x733c+i (EESETSETPlayer): Language.rc 29501 (0x733d) "To Increase By"
pub const EE_SCENARIO_SM_VALUE_MODE_TO_INCREASE_BY: i32 = 1;
/// `kSMValueModeToDecreaseBy`: Set-Attribute Level mode list 0x733c+i (EESETSETPlayer): Language.rc 29502 (0x733e) "To Decrease By"
pub const EE_SCENARIO_SM_VALUE_MODE_TO_DECREASE_BY: i32 = 2;

// Object stance effect values (EESMEffect::mStance; SetUnitsStance 0x5c2811 switch; 5..10 index EEUnitBehavior::sTable).
// (C++ enum `EESMStance`)
/// `kSMStanceExplore`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29475 (0x7323) "Explore"
pub const EE_SCENARIO_SM_STANCE_EXPLORE: i32 = 0;
/// `kSMStanceIdle`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29476 (0x7324) "Idle"
pub const EE_SCENARIO_SM_STANCE_IDLE: i32 = 1;
/// `kSMStanceLocked`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29477 (0x7325) "Locked"
pub const EE_SCENARIO_SM_STANCE_LOCKED: i32 = 2;
/// `kSMStanceUnlocked`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29478 (0x7326) "Unlocked"
pub const EE_SCENARIO_SM_STANCE_UNLOCKED: i32 = 3;
/// `kSMStanceTownPatrol`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29479 (0x7327) "Town Patrol"
pub const EE_SCENARIO_SM_STANCE_TOWN_PATROL: i32 = 4;
/// `kSMStanceScout`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29480 (0x7328) "Scout"
pub const EE_SCENARIO_SM_STANCE_SCOUT: i32 = 5;
/// `kSMStanceStandGround`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29481 (0x7329) "Stand Ground"
pub const EE_SCENARIO_SM_STANCE_STAND_GROUND: i32 = 6;
/// `kSMStanceGuard`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29482 (0x732a) "Guard"
pub const EE_SCENARIO_SM_STANCE_GUARD: i32 = 7;
/// `kSMStanceHarass`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29483 (0x732b) "Harass"
pub const EE_SCENARIO_SM_STANCE_HARASS: i32 = 8;
/// `kSMStanceCautious`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29484 (0x732c) "Cautious"
pub const EE_SCENARIO_SM_STANCE_CAUTIOUS: i32 = 9;
/// `kSMStanceAggressive`: Stance effect "Stance" list 0x7323+i (EESETSETSetObjectStance): Language.rc 29485 (0x732d) "Aggressive"
pub const EE_SCENARIO_SM_STANCE_AGGRESSIVE: i32 = 10;

// Task-to-object/area actions (EESMEffect::mTaskAction); values above kSMTaskActionCalamityBase are calamity classes.
// (C++ enum `EESMTaskAction`)
/// `kSMTaskActionNormal`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29375 (0x72bf) "Normal"
pub const EE_SCENARIO_SM_TASK_ACTION_NORMAL: i32 = 0;
/// `kSMTaskActionPatrol`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29376 (0x72c0) "Patrol"
pub const EE_SCENARIO_SM_TASK_ACTION_PATROL: i32 = 1;
/// `kSMTaskActionAttackMove`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29377 (0x72c1) "Attack-Move"
pub const EE_SCENARIO_SM_TASK_ACTION_ATTACK_MOVE: i32 = 2;
/// `kSMTaskActionLoad`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29378 (0x72c2) "Load"
pub const EE_SCENARIO_SM_TASK_ACTION_LOAD: i32 = 3;
/// `kSMTaskActionUnload`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29379 (0x72c3) "Unload"
pub const EE_SCENARIO_SM_TASK_ACTION_UNLOAD: i32 = 4;
/// `kSMTaskActionGarrison`: Task-Object/Task-Area "Action" list 0x72bf+i (EESETSETObject mActions.push_back(theIndex - 0x72bf)): Language.rc 29380 (0x72c4) "Garrison"
pub const EE_SCENARIO_SM_TASK_ACTION_GARRISON: i32 = 5;
/// `kSMTaskActionCalamityBase`: calamity task actions are the calamity class mL18 + 6 (EESETSETObject); TaskUnitsTo* test mTaskAction > 6 (provisional name)
pub const EE_SCENARIO_SM_TASK_ACTION_CALAMITY_BASE: i32 = 6;

// Operator linking a trigger condition/effect to the next (EESMTConditionWrapper/EESMTEffectWrapper::mOperator; EESETSTrigger::ShowOperator).
// (C++ enum `EESMTriggerOperator`)
/// `kSMTriggerOperatorEnd`: EESETSTrigger::ShowOperator case 0 label Language.rc 0x76bd "End"; wrapper ctors store 0; EESMTConditionWrapper::Evaluate case 0 stops
pub const EE_SCENARIO_SM_TRIGGER_OPERATOR_END: i32 = 0;
/// `kSMTriggerOperatorAnd`: Language.rc 0x76b6 "And" (EESETSTrigger combo line 0); EEUIESaveScenarioTriggersOnly writes " AND" for 1
pub const EE_SCENARIO_SM_TRIGGER_OPERATOR_AND: i32 = 1;
/// `kSMTriggerOperatorOr`: Language.rc 0x76b7 "Or" (combo line 1); EEUIESaveScenarioTriggersOnly writes " OR" for 2
pub const EE_SCENARIO_SM_TRIGGER_OPERATOR_OR: i32 = 2;

// Trigger effect states (EESMEffect::mTriggerState; Execute mType 3).
// (C++ enum `EESMTriggerState`)
/// `kSMTriggerStateOn`: Trigger effect "State" list 0x73d2+i (EESETSETTrigger GetString(theSelection + 0x73d2)): Language.rc 29650 (0x73d2) "On"
pub const EE_SCENARIO_SM_TRIGGER_STATE_ON: i32 = 0;
/// `kSMTriggerStateOff`: Trigger effect "State" list 0x73d2+i (EESETSETTrigger GetString(theSelection + 0x73d2)): Language.rc 29651 (0x73d3) "Off"
pub const EE_SCENARIO_SM_TRIGGER_STATE_OFF: i32 = 1;
/// `kSMTriggerStateFired`: Trigger effect "State" list 0x73d2+i (EESETSETTrigger GetString(theSelection + 0x73d2)): Language.rc 29652 (0x73d4) "Fired"
pub const EE_SCENARIO_SM_TRIGGER_STATE_FIRED: i32 = 2;
/// `kSMTriggerStateNotFired`: Trigger effect "State" list 0x73d2+i (EESETSETTrigger GetString(theSelection + 0x73d2)): Language.rc 29653 (0x73d5) "Not Fired"
pub const EE_SCENARIO_SM_TRIGGER_STATE_NOT_FIRED: i32 = 3;

// Scenario unit attributes (EESMObject::GetUnitAttribute switch, EEChangeUnit/EEModifyUnitAttribute, EESMEffect::mAttribute; Language.rc 29100+N).
// (C++ enum `EESMUnitAttribute`)
/// `kSMUnitAttributeHitPoints`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29100 (0x71ac) "HitPoints"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_HIT_POINTS: i32 = 0;
/// `kSMUnitAttributeAnyResource`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29101 (0x71ad) "Any Resource"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ANY_RESOURCE: i32 = 1;
/// `kSMUnitAttributeStone`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29102 (0x71ae) "Stone"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_STONE: i32 = 2;
/// `kSMUnitAttributeGold`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29103 (0x71af) "Gold"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_GOLD: i32 = 3;
/// `kSMUnitAttributeWood`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29104 (0x71b0) "Wood"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_WOOD: i32 = 4;
/// `kSMUnitAttributeFood`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29105 (0x71b1) "Food"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_FOOD: i32 = 5;
/// `kSMUnitAttributeIron`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29106 (0x71b2) "Iron"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_IRON: i32 = 6;
/// `kSMUnitAttributeFlightTime`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29107 (0x71b3) "Flight Time"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_FLIGHT_TIME: i32 = 7;
/// `kSMUnitAttributeCargoCount`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29108 (0x71b4) "Cargo Count"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_CARGO_COUNT: i32 = 8;
/// `kSMUnitAttributeGarrisonLevel`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29109 (0x71b5) "Garrison Level"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_GARRISON_LEVEL: i32 = 9;
/// `kSMUnitAttributeAmmunition`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29110 (0x71b6) "Ammunition"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_AMMUNITION: i32 = 10;
/// `kSMUnitAttributeAnimationIsPaused`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29111 (0x71b7) "Animation Is Paused"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ANIMATION_IS_PAUSED: i32 = 11;
/// `kSMUnitAttributeReloadTime`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29112 (0x71b8) "Reload Time"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_RELOAD_TIME: i32 = 12;
/// `kSMUnitAttributeGraphicScale`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29113 (0x71b9) "Graphic Scale"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_GRAPHIC_SCALE: i32 = 13;
/// `kSMUnitAttributePower`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29114 (0x71ba) "Power"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_POWER: i32 = 14;
/// `kSMUnitAttributeRottingTime`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29115 (0x71bb) "Rotting Time"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ROTTING_TIME: i32 = 15;
/// `kSMUnitAttributeConstructionComplete`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29116 (0x71bc) "Construction Complete"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_CONSTRUCTION_COMPLETE: i32 = 16;
/// `kSMUnitAttributeUnitVariable1`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29117 (0x71bd) "Unit Variable1"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_UNIT_VARIABLE1: i32 = 17;
/// `kSMUnitAttributeUnitVariable2`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29118 (0x71be) "Unit Variable2"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_UNIT_VARIABLE2: i32 = 18;
/// `kSMUnitAttributeUnitVariable3`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29119 (0x71bf) "Unit Variable3"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_UNIT_VARIABLE3: i32 = 19;
/// `kSMUnitAttributeShockArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29120 (0x71c0) "ShockArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_SHOCK_ARMOR_PLUS: i32 = 20;
/// `kSMUnitAttributeArrowArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29121 (0x71c1) "ArrowArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ARROW_ARMOR_PLUS: i32 = 21;
/// `kSMUnitAttributePierceArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29122 (0x71c2) "PierceArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_PIERCE_ARMOR_PLUS: i32 = 22;
/// `kSMUnitAttributeGunArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29123 (0x71c3) "GunArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_GUN_ARMOR_PLUS: i32 = 23;
/// `kSMUnitAttributeLaserArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29124 (0x71c4) "LaserArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_LASER_ARMOR_PLUS: i32 = 24;
/// `kSMUnitAttributeMissileArmorPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29125 (0x71c5) "MissileArmor+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_MISSILE_ARMOR_PLUS: i32 = 25;
/// `kSMUnitAttributeReloadTimePlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29126 (0x71c6) "ReloadTime+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_RELOAD_TIME_PLUS: i32 = 26;
/// `kSMUnitAttributeAttackPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29127 (0x71c7) "Attack+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ATTACK_PLUS: i32 = 27;
/// `kSMUnitAttributeWorkRatePlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29128 (0x71c8) "WorkRate+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_WORK_RATE_PLUS: i32 = 28;
/// `kSMUnitAttributeFlightTimePlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29129 (0x71c9) "FlightTime+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_FLIGHT_TIME_PLUS: i32 = 29;
/// `kSMUnitAttributeLeadershipPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29130 (0x71ca) "Leadership+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_LEADERSHIP_PLUS: i32 = 30;
/// `kSMUnitAttributeMaxObjectsCarriedPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29131 (0x71cb) "MaxObjectsCarried+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_MAX_OBJECTS_CARRIED_PLUS: i32 = 31;
/// `kSMUnitAttributeRepairPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29132 (0x71cc) "Repair+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_REPAIR_PLUS: i32 = 32;
/// `kSMUnitAttributeHitPointsPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29133 (0x71cd) "HitPoints+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_HIT_POINTS_PLUS: i32 = 33;
/// `kSMUnitAttributeCarryAmountPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29134 (0x71ce) "CarryAmount+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_CARRY_AMOUNT_PLUS: i32 = 34;
/// `kSMUnitAttributeContainerHealingHPPerSecPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29135 (0x71cf) "ContainerHealingHP/Sec+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_CONTAINER_HEALING_HP_PER_SEC_PLUS: i32 = 35;
/// `kSMUnitAttributeRangePlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29136 (0x71d0) "Range+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_RANGE_PLUS: i32 = 36;
/// `kSMUnitAttributeLineOfSightPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29137 (0x71d1) "LineOfSight+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_LINE_OF_SIGHT_PLUS: i32 = 37;
/// `kSMUnitAttributeMaxVelocityPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29138 (0x71d2) "MaxVelocity+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_MAX_VELOCITY_PLUS: i32 = 38;
/// `kSMUnitAttributePowerPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29139 (0x71d3) "Power+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_POWER_PLUS: i32 = 39;
/// `kSMUnitAttributeAnimalAIActivePlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29140 (0x71d4) "Animal AI Active+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_ANIMAL_AI_ACTIVE_PLUS: i32 = 40;
/// `kSMUnitAttributeCameraFocusPlus`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29141 (0x71d5) "CameraFocus+"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_CAMERA_FOCUS_PLUS: i32 = 41;
/// `kSMUnitAttributeTotalShockArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29142 (0x71d6) "Total ShockArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_SHOCK_ARMOR: i32 = 42;
/// `kSMUnitAttributeTotalArrowArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29143 (0x71d7) "Total ArrowArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_ARROW_ARMOR: i32 = 43;
/// `kSMUnitAttributeTotalPierceArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29144 (0x71d8) "Total PierceArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PIERCE_ARMOR: i32 = 44;
/// `kSMUnitAttributeTotalGunArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29145 (0x71d9) "Total GunArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_GUN_ARMOR: i32 = 45;
/// `kSMUnitAttributeTotalLaserArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29146 (0x71da) "Total LaserArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_LASER_ARMOR: i32 = 46;
/// `kSMUnitAttributeTotalMissileArmor`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29147 (0x71db) "Total MissileArmor"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_MISSILE_ARMOR: i32 = 47;
/// `kSMUnitAttributeTotalReloadTime`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29148 (0x71dc) "Total ReloadTime"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_RELOAD_TIME: i32 = 48;
/// `kSMUnitAttributeTotalAttack`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29149 (0x71dd) "Total Attack"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_ATTACK: i32 = 49;
/// `kSMUnitAttributeTotalWorkRate`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29150 (0x71de) "Total WorkRate"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_WORK_RATE: i32 = 50;
/// `kSMUnitAttributeTotalFlightTime`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29151 (0x71df) "Total FlightTime"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_FLIGHT_TIME: i32 = 51;
/// `kSMUnitAttributeTotalLeadership`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29152 (0x71e0) "Total Leadership"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_LEADERSHIP: i32 = 52;
/// `kSMUnitAttributeTotalMaxObjectsCarried`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29153 (0x71e1) "Total MaxObjectsCarried"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_MAX_OBJECTS_CARRIED: i32 = 53;
/// `kSMUnitAttributeTotalRepair`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29154 (0x71e2) "Total Repair"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_REPAIR: i32 = 54;
/// `kSMUnitAttributeTotalHitPoints`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29155 (0x71e3) "Total HitPoints"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_HIT_POINTS: i32 = 55;
/// `kSMUnitAttributeTotalCarryAmount`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29156 (0x71e4) "Total CarryAmount"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CARRY_AMOUNT: i32 = 56;
/// `kSMUnitAttributeTotalHealingHPPerSec`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29157 (0x71e5) "Total HealingHP/Sec"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_HEALING_HP_PER_SEC: i32 = 57;
/// `kSMUnitAttributeTotalRange`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29158 (0x71e6) "Total Range"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_RANGE: i32 = 58;
/// `kSMUnitAttributeTotalLineOfSight`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29159 (0x71e7) "Total LineOfSight"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_LINE_OF_SIGHT: i32 = 59;
/// `kSMUnitAttributeTotalMaxVelocity`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29160 (0x71e8) "Total MaxVelocity"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_MAX_VELOCITY: i32 = 60;
/// `kSMUnitAttributeTotalPower`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29161 (0x71e9) "Total Power"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_POWER: i32 = 61;
/// `kSMUnitAttributeTotalPlaceHolder2`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29162 (0x71ea) "Total PlaceHolder2"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PLACE_HOLDER2: i32 = 62;
/// `kSMUnitAttributeTotalPlaceHolder3`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29163 (0x71eb) "Total PlaceHolder3"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PLACE_HOLDER3: i32 = 63;
/// `kSMUnitAttributeTotalTransportWeight`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29164 (0x71ec) "Total TransportWeight"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_TRANSPORT_WEIGHT: i32 = 64;
/// `kSMUnitAttributeTotalWeight`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29165 (0x71ed) "Total Weight"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_WEIGHT: i32 = 65;
/// `kSMUnitAttributeTotalTileSize`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29166 (0x71ee) "Total Tile Size"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_TILE_SIZE: i32 = 66;
/// `kSMUnitAttributeTotalCruiseAltitude`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29167 (0x71ef) "Total Cruise Altitude"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CRUISE_ALTITUDE: i32 = 67;
/// `kSMUnitAttributeTotalMinimumRange`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29168 (0x71f0) "Total Minimum Range"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_MINIMUM_RANGE: i32 = 68;
/// `kSMUnitAttributeTotalCanEnterAirTransports`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29169 (0x71f1) "Total Can Enter Air Transports"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CAN_ENTER_AIR_TRANSPORTS: i32 = 69;
/// `kSMUnitAttributeTotalCanEnterLandTransports`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29170 (0x71f2) "Total Can Enter Land Transports"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CAN_ENTER_LAND_TRANSPORTS: i32 = 70;
/// `kSMUnitAttributeTotalCanEnterSeaTransports`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29171 (0x71f3) "Total Can Enter Sea Transports"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CAN_ENTER_SEA_TRANSPORTS: i32 = 71;
/// `kSMUnitAttributeTotalPlaceHolder1`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29172 (0x71f4) "Total Place Holder1"
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PLACE_HOLDER1: i32 = 72;
/// `kSMUnitAttributeTotalPlaceHolder2B`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29173 (0x71f5) "Total Place Holder2"; B: the text equals entry 62 but for the space in Place Holder
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PLACE_HOLDER2_B: i32 = 73;
/// `kSMUnitAttributeTotalPlaceHolder3B`: unit attribute list 0x71ac+i (EESETSObject/EESETSETObject GetString(i + 0x71ac)): Language.rc 29174 (0x71f6) "Total Place Holder3"; B: the text equals entry 63 but for the space in Place Holder
pub const EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_PLACE_HOLDER3_B: i32 = 74;

// Unit-attribute change modes (EEChangeUnit inMode: 0 adds, 1 subtracts, 2 sets).
// (C++ enum `EESMUnitAttributeMode`)
/// `kSMAttributeModeIncrease`: unit attribute operator list 0x7396+i (EESETSETSAAAttributeChange): Language.rc 29590 (0x7396) "Increase"
pub const EE_SCENARIO_SM_ATTRIBUTE_MODE_INCREASE: i32 = 0;
/// `kSMAttributeModeDecrease`: unit attribute operator list 0x7396+i (EESETSETSAAAttributeChange): Language.rc 29591 (0x7397) "Decrease"
pub const EE_SCENARIO_SM_ATTRIBUTE_MODE_DECREASE: i32 = 1;
/// `kSMAttributeModeExactly`: unit attribute operator list 0x7396+i (EESETSETSAAAttributeChange): Language.rc 29592 (0x7398) "Exactly"
pub const EE_SCENARIO_SM_ATTRIBUTE_MODE_EXACTLY: i32 = 2;

#[cfg(test)]
mod tests {
    use super::*;

    /// The values the C++ header (the reconstruction) had, a sample of each table.
    #[test]
    fn values_match_the_reconstruction() {
        assert_eq!(EE_SCENARIO_SM_AI_ACTION_MODIFY_AI_MILITARY_TARGET, 0x0); // kSMAIActionModifyAIMilitaryTarget
        assert_eq!(EE_SCENARIO_SM_CONDITION_ALWAYS_TRUE, 0x0); // kSMConditionAlwaysTrue
        assert_eq!(EE_SCENARIO_SM_GAME_ACTION_END_CINEMATIC_MODE, 0x5); // kSMGameActionEndCinematicMode
        assert_eq!(EE_SCENARIO_SM_OBJECT_ACTION_FACE_OBJECT, 0xb); // kSMObjectActionFaceObject
        assert_eq!(EE_SCENARIO_SM_PLAYER_VALUE_FOOD, 0x5); // kSMPlayerValueFood
        assert_eq!(EE_SCENARIO_SM_PLAYER_VALUE_STONE_SENT, 0x19); // kSMPlayerValueStoneSent
        assert_eq!(EE_SCENARIO_SM_PLAYER_VALUE_STAT_HERO_COUNT, 0x2d); // kSMPlayerValueStatHeroCount
        assert_eq!(EE_SCENARIO_SM_PLAYER_VALUE_PLACEHOLDER4, 0x41); // kSMPlayerValuePlaceholder4
        assert_eq!(EE_SCENARIO_SM_TASK_ACTION_UNLOAD, 0x4); // kSMTaskActionUnload
        assert_eq!(EE_SCENARIO_SM_UNIT_ATTRIBUTE_AMMUNITION, 0xa); // kSMUnitAttributeAmmunition
        assert_eq!(EE_SCENARIO_SM_UNIT_ATTRIBUTE_LEADERSHIP_PLUS, 0x1e); // kSMUnitAttributeLeadershipPlus
        assert_eq!(EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_WORK_RATE, 0x32); // kSMUnitAttributeTotalWorkRate
        assert_eq!(EE_SCENARIO_SM_UNIT_ATTRIBUTE_TOTAL_CAN_ENTER_LAND_TRANSPORTS, 0x46); // kSMUnitAttributeTotalCanEnterLandTransports
    }
}
