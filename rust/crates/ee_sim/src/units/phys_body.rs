//! `EEPhysics`' bodies over their C++ statements (the units' lane, round 7; were EEPhysics.cpp): `Tick`,
//! `DrainInput`, `GetOutOfSynchChecksum` (with its dump), `EndGame`, `Init`, `GetTimeString`,
//! `GetMinutesAndSeconds`, `GetUnitSwapKind`, `UpdateCivTimer`, `ServeLoadSaveRequests`, `CheckShiftMode`,
//! `UpdateWeather`, `ReportStatistics`, `AutoSave`, `FlushWorld`, `RunOwedTicks`, `ClearServerMessages`,
//! `HandleMessage`, `ProcessUnitSwaps`, `SwapUnit`, `UpdateAreaEffectsClock`, `UpdateTiming`, `Idle`,
//! `QueueMessage`, `QueueUnitSwap` and the three posts. The physics' plain members are the Rust record
//! [`EeUnitsPhysicsSaved`] the class derives from: the bodies read and write them through its pointer, one field at a
//! time (no reference across a host call). Every other C++ statement is one operation of [`PhHost::ph`] (the game's:
//! `ee_units_ph_host`, EEPhysics.cpp, one statement a case), asked in the original's order.

#![allow(clippy::unnecessary_cast)]

use crate::units::physics_save::EeUnitsPhysicsSaved;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};

// ---- the statements (`op` on the physics; `a`, `b` objects or values, `c`, `d` values; the answer)
/// `mServerMessageQueueLock->Enter()`
pub const EE_UNITS_PH_QUEUE_LOCK_ENTER: i32 = 1;
/// `mServerMessageQueueLock->Leave()`
pub const EE_UNITS_PH_QUEUE_LOCK_LEAVE: i32 = 2;
/// `mServerMessages.size()`
pub const EE_UNITS_PH_SERVER_MESSAGES: i32 = 3;
/// `mServerMessages.front()`
pub const EE_UNITS_PH_FRONT: i32 = 4;
/// `mServerMessages.pop_front()`
pub const EE_UNITS_PH_POP_FRONT: i32 = 5;
/// message `a`'s `Dereference()`
pub const EE_UNITS_PH_DEREFERENCE: i32 = 6;
/// `mTimerRunning`
pub const EE_UNITS_PH_TIMER_RUNNING: i32 = 7;
/// message `a`'s `GetType()`
pub const EE_UNITS_PH_MESSAGE_TYPE: i32 = 8;
/// record `a`'s `Get<bool>(c)`
pub const EE_UNITS_PH_RECORD_BOOL: i32 = 9;
/// record `a`'s `Get<unsigned char>(c)`
pub const EE_UNITS_PH_RECORD_BYTE: i32 = 10;
/// record `a`'s `Get<uint32_t>(c)`
pub const EE_UNITS_PH_RECORD_U32: i32 = 11;
/// `gCommunications->IsServer()`
pub const EE_UNITS_PH_IS_SERVER: i32 = 12;
/// `gCommunications->IsClient()`
pub const EE_UNITS_PH_IS_CLIENT: i32 = 13;
/// `static_cast<EEClient*>(gCommunications)->mHasLeftGame`
pub const EE_UNITS_PH_HAS_LEFT_GAME: i32 = 14;
/// `GetOutOfSynchChecksum(c != 0, 1)`
pub const EE_UNITS_PH_OUT_OF_SYNCH_CHECKSUM: i32 = 15;
/// `gCommunications->GetNumberOfCTsProcessedInPhysics()`
pub const EE_UNITS_PH_CTS_PROCESSED: i32 = 16;
/// `new NEMRecord(kNetMessageWorldDatabaseChecksumResponse, {c, d})`
pub const EE_UNITS_PH_CHECKSUM_RESPONSE: i32 = 17;
/// message `a`'s `SetOriginatingSlot(gWorld.GetCurrentPlayer()->mSlot)`
pub const EE_UNITS_PH_SET_SLOT_CURRENT: i32 = 18;
/// `gCommunications->ProcessLocalUserInput(message a, kSlotNone)`
pub const EE_UNITS_PH_LOCAL_INPUT: i32 = 19;
/// the server's `LogChecksumResponse(message a)`
pub const EE_UNITS_PH_LOG_CHECKSUM_RESPONSE: i32 = 20;
/// `gCommunications->IncrementNumberOfCTsProcessedInPhysics()`
pub const EE_UNITS_PH_INCREMENT_CTS: i32 = 21;
/// `gCommunications->SetAveragePhysicsUpdateTime(c)`
pub const EE_UNITS_PH_SET_AVERAGE_UPDATE: i32 = 22;
/// `gCommunications->SetAverageTimeBetweenPhysicsTicks(c)`
pub const EE_UNITS_PH_SET_AVERAGE_BETWEEN: i32 = 23;
/// `SetSpeed(f[0])`
pub const EE_UNITS_PH_SET_SPEED: i32 = 24;
/// message `a`'s `GetMessageLabel()`
pub const EE_UNITS_PH_MESSAGE_LABEL: i32 = 25;
/// message `a`'s `GetForSpecificSlot() == kSlotNone`
pub const EE_UNITS_PH_FOR_NO_SLOT: i32 = 26;
/// `mInputMessagesProcessed++`
pub const EE_UNITS_PH_INPUTS_PROCESSED_INCREMENT: i32 = 27;
/// `EEMessageCaptureExecuted(message a)`
pub const EE_UNITS_PH_CAPTURE_EXECUTED: i32 = 28;
/// `EESimCaptureRecord::Active()`
pub const EE_UNITS_PH_CAPTURE_ACTIVE: i32 = 29;
/// `EEUnitStoreCheck(c)`
pub const EE_UNITS_PH_UNIT_STORE_CHECK: i32 = 30;
/// `gEEMessageExecutedTestHook != NULL`
pub const EE_UNITS_PH_TEST_HOOK_SET: i32 = 31;
/// `gEEMessageExecutedTestHook(message a, c)`
pub const EE_UNITS_PH_TEST_HOOK: i32 = 32;
/// `gCommunications->ExecuteRemovePlayer(...)` of the remove-player record `a`
pub const EE_UNITS_PH_REMOVE_PLAYER: i32 = 33;
/// `EEGameMessageCast(message a)->ExecuteSelectionMessage()`
pub const EE_UNITS_PH_EXECUTE_SELECTION: i32 = 34;
/// `EEPhysicsLog(message a, mGameTime, c)`
pub const EE_UNITS_PH_LOG: i32 = 35;
/// `EEGameMessageCast(message a)`
pub const EE_UNITS_PH_GAME_MESSAGE: i32 = 36;
/// game message `a`'s `ExecuteMessage()`
pub const EE_UNITS_PH_EXECUTE_GAME_MESSAGE: i32 = 37;
/// message `a`'s `Execute()`
pub const EE_UNITS_PH_EXECUTE: i32 = 38;
/// `ee_world_view_check(&gWorld, the ticks done)`
pub const EE_UNITS_PH_WORLD_VIEW_CHECK: i32 = 39;
/// `sLoadedSeedPending`
pub const EE_UNITS_PH_LOADED_SEED_PENDING: i32 = 40;
/// `sLoadedSeedPending = false`
pub const EE_UNITS_PH_CLEAR_LOADED_SEED_PENDING: i32 = 41;
/// `mRandom.SetSeed(sLoadedSeed)`
pub const EE_UNITS_PH_SET_LOADED_SEED: i32 = 42;
/// the fix-ups of `EEAfterLoad` (c 0) or `EEAfterLoadLast` (c 1) are empty
pub const EE_UNITS_PH_FIXUPS_EMPTY: i32 = 43;
/// the running fix-ups swapped with those of `EEAfterLoad` (c 0) or `EEAfterLoadLast` (c 1)
pub const EE_UNITS_PH_FIXUPS_TAKE: i32 = 44;
/// the running fix-ups' size
pub const EE_UNITS_PH_FIXUPS_COUNT: i32 = 45;
/// running fix-up `c` called
pub const EE_UNITS_PH_FIXUP_RUN: i32 = 46;
/// the running fix-ups cleared
pub const EE_UNITS_PH_FIXUPS_CLEAR: i32 = 47;
/// `EEUnitOrderReset()`
pub const EE_UNITS_PH_UNIT_ORDER_RESET: i32 = 48;
/// `ReportStatistics()`
pub const EE_UNITS_PH_REPORT_STATISTICS: i32 = 49;
/// `DrainInput()`
pub const EE_UNITS_PH_DRAIN_INPUT: i32 = 50;
/// `ServeLoadSaveRequests()`
pub const EE_UNITS_PH_SERVE_LOAD_SAVE: i32 = 51;
/// `gCommunications->GetIsGamePaused()`
pub const EE_UNITS_PH_PAUSED: i32 = 52;
/// `gWorld.mCameraControl.mUnitA == NULL`
pub const EE_UNITS_PH_NO_CAMERA_UNIT: i32 = 53;
/// `gWorld.mCameraControl.Update(mSpeed)`
pub const EE_UNITS_PH_CAMERA_UPDATE: i32 = 54;
/// the bits of `mRandom.Generate()`
pub const EE_UNITS_PH_RANDOM_GENERATE: i32 = 55;
/// `TMGetTime()`
pub const EE_UNITS_PH_TIME: i32 = 56;
/// `gWorld.UpdateActiveUnits()`
pub const EE_UNITS_PH_UPDATE_ACTIVE_UNITS: i32 = 57;
/// `UpdateCivTimer()`
pub const EE_UNITS_PH_UPDATE_CIV_TIMER: i32 = 58;
/// `gWorldTable.GetLongValue(kWorldDeathmatchJitManufacturingDelay) * 1000`
pub const EE_UNITS_PH_RESOURCE_TIME: i32 = 59;
/// `gWorld.mGameOptions.mResources`
pub const EE_UNITS_PH_RESOURCES: i32 = 60;
/// `gCivPowersManager.HasPower(gWorld.GetCurrentPlayer(), kCivPowerJustInTimeManufacturing)`
pub const EE_UNITS_PH_HAS_JIT_POWER: i32 = 61;
/// `gResource->GetString(IDS_JUST_IN_TIME_MANUFANUFACTURING_IS_NOW, the text)`
pub const EE_UNITS_PH_JIT_TEXT: i32 = 62;
/// `gUserInterface->AddChatLogEntry(the text, 0)`
pub const EE_UNITS_PH_CHAT_TEXT: i32 = 63;
/// `UpdateWeather()`
pub const EE_UNITS_PH_UPDATE_WEATHER: i32 = 64;
/// `gPathManager.Update()`
pub const EE_UNITS_PH_PATH_UPDATE: i32 = 65;
/// `UpdateAreaEffectsClock()`
pub const EE_UNITS_PH_AREA_EFFECTS_CLOCK: i32 = 66;
/// `CheckShiftMode()`
pub const EE_UNITS_PH_CHECK_SHIFT_MODE: i32 = 67;
/// `UpdateUnits()`
pub const EE_UNITS_PH_UPDATE_UNITS: i32 = 68;
/// `gEffectManager->Update()`
pub const EE_UNITS_PH_EFFECTS_UPDATE: i32 = 69;
/// `EECalamityManager::GetInstance()->Update(mTickTime)`
pub const EE_UNITS_PH_CALAMITIES_UPDATE: i32 = 70;
/// `gArtificialIntelligence->Update()`
pub const EE_UNITS_PH_AI_UPDATE: i32 = 71;
/// `gWorld.mInCinematic`
pub const EE_UNITS_PH_IN_CINEMATIC: i32 = 72;
/// `gWorld.mScenarioManager->mGameResult`
pub const EE_UNITS_PH_SCENARIO_RESULT: i32 = 73;
/// `gWorld.mScenarioManager->Update()`
pub const EE_UNITS_PH_SCENARIO_UPDATE: i32 = 74;
/// `gWorld.CheckGameOver()`
pub const EE_UNITS_PH_CHECK_GAME_OVER: i32 = 75;
/// `gWorld.mScenarioManager->mGameResult = c`
pub const EE_UNITS_PH_SET_SCENARIO_RESULT: i32 = 76;
/// `EndGame(c != 0)`
pub const EE_UNITS_PH_END_GAME: i32 = 77;
/// `FlushWorld()`
pub const EE_UNITS_PH_FLUSH_WORLD: i32 = 78;
/// `gWorld.mGameOptions.mCampaignName.GetLength()`
pub const EE_UNITS_PH_CAMPAIGN_NAME_LENGTH: i32 = 79;
/// `gWorld.mGameOptions.mCampaignChecksum`
pub const EE_UNITS_PH_CAMPAIGN_CHECKSUM: i32 = 80;
/// `gWorld.mGameOptions.mIsScenarioGame`
pub const EE_UNITS_PH_SCENARIO_GAME: i32 = 81;
/// `(unsigned long)gWorld.GetCurrentPlayer()->mCivilization.GetAvailablePoints()`
pub const EE_UNITS_PH_AVAILABLE_POINTS: i32 = 82;
/// `EESetCivBuilderButtonBlinking(c != 0)`
pub const EE_UNITS_PH_CIV_BLINK: i32 = 83;
/// `gGamePreferences.mSettings.mAutoSaveInMilliseconds`
pub const EE_UNITS_PH_AUTOSAVE_PERIOD: i32 = 84;
/// `mTimerInterval`
pub const EE_UNITS_PH_TIMER_INTERVAL: i32 = 85;
/// `gWorld.mScenarioManager->mIsShowingDialog`
pub const EE_UNITS_PH_SHOWING_DIALOG: i32 = 86;
/// `gPhysics->PostAutoSave()`
pub const EE_UNITS_PH_POST_AUTOSAVE: i32 = 87;
/// `gCommunications->GetCurrentCommunicationsTick()`
pub const EE_UNITS_PH_CURRENT_CT: i32 = 88;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_PH_NUM_PLAYERS: i32 = 89;
/// `gWorld.mPlayers[c]`
pub const EE_UNITS_PH_PLAYER_AT: i32 = 90;
/// `RTSCheckUnitList(&gWorld.mUnits, "EEPhysics::FlushWorld (start)")`
pub const EE_UNITS_PH_CHECK_UNIT_LIST: i32 = 91;
/// `gWorld.FlushQueuedPlacements()`
pub const EE_UNITS_PH_FLUSH_PLACEMENTS: i32 = 92;
/// player `a`'s `AddPendingUnits(0, 1)`
pub const EE_UNITS_PH_ADD_PENDING_UNITS: i32 = 93;
/// `gWorld.QueueBornDeactivations()`
pub const EE_UNITS_PH_QUEUE_BORN_DEACTIVATIONS: i32 = 94;
/// `ProcessUnitSwaps()`
pub const EE_UNITS_PH_PROCESS_UNIT_SWAPS: i32 = 95;
/// player `a`'s `RemoveKilledUnits()`
pub const EE_UNITS_PH_REMOVE_KILLED_UNITS: i32 = 96;
/// `Tick()`
pub const EE_UNITS_PH_TICK: i32 = 97;
/// `gB529065 = true`
pub const EE_UNITS_PH_SET_GAME_ENDED: i32 = 98;
/// player `a`'s `mObserverMode`
pub const EE_UNITS_PH_OBSERVER: i32 = 99;
/// `gWorld.RemovePlayerFromGame(c != 0, d)`
pub const EE_UNITS_PH_REMOVE_FROM_GAME: i32 = 100;
/// player `a`'s `SetGameResult(c)`
pub const EE_UNITS_PH_SET_GAME_RESULT: i32 = 101;
/// `gWorld.GetCurrentPlayer()->mGameResult`
pub const EE_UNITS_PH_CURRENT_GAME_RESULT: i32 = 102;
/// the campaign progress (a new one) `Load(gCampaignUser, the campaign, its checksum, true)`
pub const EE_UNITS_PH_PROGRESS_LOAD: i32 = 103;
/// the campaign progress' `SaveCompletedScenario(gCampaignUser, the campaign, the scenario, the checksum)`
pub const EE_UNITS_PH_PROGRESS_SAVE: i32 = 104;
/// the main game form is the current form (`dynamic_cast<EEUIFMainGame*>`)
pub const EE_UNITS_PH_MAIN_GAME_CURRENT: i32 = 105;
/// the in-game options button blinks
pub const EE_UNITS_PH_OPTIONS_BLINK: i32 = 106;
/// `gWorld.mTeams.clear()`
pub const EE_UNITS_PH_TEAMS_CLEAR: i32 = 107;
/// `ee_app_teams_clear()`
pub const EE_UNITS_PH_APP_TEAMS_CLEAR: i32 = 108;
/// the server's `PauseGame()`
pub const EE_UNITS_PH_PAUSE_GAME: i32 = 109;
/// the campaign progress released
pub const EE_UNITS_PH_PROGRESS_RELEASE: i32 = 110;
/// the time of day (`TMGetTimeOfDay`) into the scratch time
pub const EE_UNITS_PH_TIME_OF_DAY: i32 = 111;
/// the dump's name ("OOS <date>.log") from the scratch time
pub const EE_UNITS_PH_DUMP_NAME: i32 = 112;
/// the dump file = `PlatFOpen(the name, "w")`
pub const EE_UNITS_PH_DUMP_OPEN: i32 = 113;
/// `fprintf(the dump, "\n")`
pub const EE_UNITS_PH_DUMP_NEWLINE: i32 = 114;
/// the dump's player line of player `a`
pub const EE_UNITS_PH_DUMP_PLAYER: i32 = 115;
/// player `a`'s `mActiveGroup`
pub const EE_UNITS_PH_ACTIVE_GROUP: i32 = 116;
/// group `a`'s `GetNumUnits()`
pub const EE_UNITS_PH_GROUP_NUM_UNITS: i32 = 117;
/// the dump's selected group count line (`c`)
pub const EE_UNITS_PH_DUMP_SELECTED_COUNT: i32 = 118;
/// group `a`'s `GetUnit(c)->mUnitID`
pub const EE_UNITS_PH_GROUP_UNIT_ID: i32 = 119;
/// the dump's unit line (#`c` with id `d`)
pub const EE_UNITS_PH_DUMP_UNIT: i32 = 120;
/// player `a`'s `GetNumRunningGroups()`
pub const EE_UNITS_PH_RUNNING_GROUPS: i32 = 121;
/// the dump's active group count line (`c`)
pub const EE_UNITS_PH_DUMP_ACTIVE_COUNT: i32 = 122;
/// player `a`'s `GetRunningGroup(c)`
pub const EE_UNITS_PH_RUNNING_GROUP: i32 = 123;
/// active group `a`'s `GetNumUnits()`
pub const EE_UNITS_PH_ACTIVE_GROUP_NUM_UNITS: i32 = 124;
/// the dump's active group line (#`c` with `d` units)
pub const EE_UNITS_PH_DUMP_ACTIVE_GROUP: i32 = 125;
/// active group `a`'s `GetUnit(c)->mUnitID`
pub const EE_UNITS_PH_ACTIVE_GROUP_UNIT_ID: i32 = 126;
/// player `a`'s `mAirTraffic[c]`
pub const EE_UNITS_PH_AIR_TRAFFIC: i32 = 127;
/// `ee_units_atc_dump(air traffic a, the dump)`
pub const EE_UNITS_PH_DUMP_AIR_TRAFFIC: i32 = 128;
/// `gPathManager.DumpOutOfSynch(the dump)`
pub const EE_UNITS_PH_DUMP_PATHS: i32 = 129;
/// `gWorld.mMap->DumpOutOfSynch(the dump)`
pub const EE_UNITS_PH_DUMP_MAP: i32 = 130;
/// `EECalamityManager::GetInstance()->DumpOutOfSynch(the dump)`
pub const EE_UNITS_PH_DUMP_CALAMITIES: i32 = 131;
/// `gWorld.GetContagionManager()->DumpOutOfSynch(the dump)`
pub const EE_UNITS_PH_DUMP_CONTAGIONS: i32 = 132;
/// `gWorld.mUnits.mFirst`
pub const EE_UNITS_PH_FIRST_UNIT: i32 = 133;
/// unit `a`'s `mTileNode->mNext`
pub const EE_UNITS_PH_NEXT_UNIT: i32 = 134;
/// unit `a`'s `OutOfSynchDump(the dump, c)`
pub const EE_UNITS_PH_DUMP_UNIT_STATE: i32 = 135;
/// the dump's closing lines and `fclose`
pub const EE_UNITS_PH_DUMP_CLOSE: i32 = 136;
/// `(unsigned long)` unit `a`'s `GetChecksum(c)`
pub const EE_UNITS_PH_UNIT_CHECKSUM: i32 = 137;
/// `mRandom.GetSeed() + mInputMessagesProcessed` (unsigned long)
pub const EE_UNITS_PH_SEED_AND_INPUTS: i32 = 138;
/// `EECPOutOfSynchChecksum()`
pub const EE_UNITS_PH_CP_CHECKSUM: i32 = 139;
/// `(unsigned long)gWorld.mMap->ComputeChecksum()`
pub const EE_UNITS_PH_MAP_CHECKSUM: i32 = 140;
/// `gContinentManagerChecksum`
pub const EE_UNITS_PH_CONTINENT_CHECKSUM: i32 = 141;
/// `(unsigned long)EECalamityManager::GetInstance()->GetOutOfSynchChecksum()`
pub const EE_UNITS_PH_CALAMITY_CHECKSUM: i32 = 142;
/// `mLastOutOfSynchChecksum = c`
pub const EE_UNITS_PH_SET_LAST_CHECKSUM: i32 = 143;
/// player `a`'s `mPlayerID`
pub const EE_UNITS_PH_PLAYER_ID: i32 = 144;
/// the bits of `gWorld.mGameSpeed`... the time text: hours `c` < 100 or not (`d`), minutes, seconds in `b`
pub const EE_UNITS_PH_TIME_TEXT: i32 = 145;
/// the bits of `gPhysics->mGameClock`
pub const EE_UNITS_PH_GAME_CLOCK: i32 = 146;
/// `mUnitSwaps.size()`
pub const EE_UNITS_PH_SWAPS: i32 = 147;
/// `mUnitSwaps[c].mOld`
pub const EE_UNITS_PH_SWAP_OLD: i32 = 148;
/// `mUnitSwaps[c].mNew`
pub const EE_UNITS_PH_SWAP_NEW: i32 = 149;
/// unit `a`'s `GetPlayer()`
pub const EE_UNITS_PH_UNIT_PLAYER: i32 = 150;
/// complex unit `a`'s `mIsCitizen`
pub const EE_UNITS_PH_IS_CITIZEN: i32 = 151;
/// `ClearServerMessages()`
pub const EE_UNITS_PH_CLEAR_SERVER_MESSAGES: i32 = 152;
/// `mSaveGameLock->Enter()`
pub const EE_UNITS_PH_SAVE_LOCK_ENTER: i32 = 153;
/// `mSaveGameLock->Leave()`
pub const EE_UNITS_PH_SAVE_LOCK_LEAVE: i32 = 154;
/// `mLoadSaveRequests.size()`
pub const EE_UNITS_PH_REQUESTS: i32 = 155;
/// `mLoadSaveRequests[c]->Dereference()`
pub const EE_UNITS_PH_REQUEST_DEREFERENCE: i32 = 156;
/// `mLoadSaveRequests.clear()`
pub const EE_UNITS_PH_REQUESTS_CLEAR: i32 = 157;
/// `mInputMessagesProcessed = 0`
pub const EE_UNITS_PH_INPUTS_PROCESSED_CLEAR: i32 = 158;
/// the bits of `mSpeed`
pub const EE_UNITS_PH_SPEED: i32 = 159;
/// `gCommunications != NULL`
pub const EE_UNITS_PH_HAS_COMMUNICATIONS: i32 = 160;
/// `gWorld.mGameOptions.CanBuyCivilization()`
pub const EE_UNITS_PH_CAN_BUY_CIVILIZATION: i32 = 161;
/// `gWorld.GetCurrentPlayer()->mCivilizationChosen`
pub const EE_UNITS_PH_CIVILIZATION_CHOSEN: i32 = 162;
/// `gWorld.GetCurrentPlayer()->mObserverMode`
pub const EE_UNITS_PH_CURRENT_OBSERVER: i32 = 163;
/// `EEGetCivTimerLimit()`
pub const EE_UNITS_PH_CIV_TIMER_LIMIT: i32 = 164;
/// `EEGetCivTimerSecondWarning()`
pub const EE_UNITS_PH_CIV_TIMER_SECOND: i32 = 165;
/// `EEGetCivTimerFirstWarning()`
pub const EE_UNITS_PH_CIV_TIMER_FIRST: i32 = 166;
/// the civ timer message (`(unsigned char)c`) to the local input
pub const EE_UNITS_PH_CIV_TIMER_MESSAGE: i32 = 167;
/// `EELoadOrSave(mLoadSaveRequests[c])`
pub const EE_UNITS_PH_LOAD_OR_SAVE: i32 = 168;
/// `mLoadSaveRequests[c] = NULL`
pub const EE_UNITS_PH_REQUEST_CLEAR: i32 = 169;
/// `new EEMRecord(kMessageLoadOrSaveGameResponse, {c})`
pub const EE_UNITS_PH_LOAD_SAVE_RESPONSE: i32 = 170;
/// message `a`'s `SetOriginatingSlot(gWorld.GetCurrentPlayer()->GetSlot())`
pub const EE_UNITS_PH_SET_SLOT_CURRENT_GET: i32 = 171;
/// the server's `LogResponse(message a)`
pub const EE_UNITS_PH_LOG_RESPONSE: i32 = 172;
/// `gCommunications->ProcessLocalUserInput(message a, kSlotServer)`
pub const EE_UNITS_PH_LOCAL_INPUT_SERVER: i32 = 173;
/// `gWorld.GetCurrentPlayer()->mShiftMode`
pub const EE_UNITS_PH_SHIFT_MODE: i32 = 174;
/// the keyboard's Shift key is up
pub const EE_UNITS_PH_SHIFT_UP: i32 = 175;
/// the exit-shift-mode message (`mGameTime`) to the local input
pub const EE_UNITS_PH_EXIT_SHIFT_MODE: i32 = 176;
/// the bits of `gWorld.GetTimeOfDay()`
pub const EE_UNITS_PH_GET_TIME_OF_DAY: i32 = 177;
/// `gWorld.SetTimeOfDay(f[0])`
pub const EE_UNITS_PH_SET_TIME_OF_DAY: i32 = 178;
/// the bits of `gWorld.mWindYaw`
pub const EE_UNITS_PH_WIND_YAW: i32 = 179;
/// `gWorld.mWindYaw = f[0]`
pub const EE_UNITS_PH_SET_WIND_YAW: i32 = 180;
/// the wind's transformation: a new one, `SetOrientationYPR(f[0], 0, 0)`
pub const EE_UNITS_PH_WIND_ORIENT: i32 = 181;
/// `gWorld.mWindDirection` = the wind's transformation applied to north (0, 0.001, 0)
pub const EE_UNITS_PH_WIND_DIRECTION: i32 = 182;
/// `gWorld.mEnvironmentManager->Tick(mGameTime)`
pub const EE_UNITS_PH_ENVIRONMENT_TICK: i32 = 183;
/// the current player's `mSavedGroups.GetLastValue(c)`
pub const EE_UNITS_PH_LAST_STATISTIC: i32 = 184;
/// the report-statistics message (`c`, `d`) to the local input
pub const EE_UNITS_PH_REPORT_MESSAGE: i32 = 185;
/// `srand(TMGetTime())`
pub const EE_UNITS_PH_SEED_RAND: i32 = 186;
/// `rand()`
pub const EE_UNITS_PH_RAND: i32 = 187;
/// `gGamePreferences.mSettings.mNumberOfAutoSaves`
pub const EE_UNITS_PH_AUTOSAVES: i32 = 188;
/// the autosave's name ("AutoSave <date - time><extension>") from the scratch time
pub const EE_UNITS_PH_AUTOSAVE_NAME: i32 = 189;
/// `gGamePreferences.mSettings.mLastAutoSave = the name`
pub const EE_UNITS_PH_SET_LAST_AUTOSAVE: i32 = 190;
/// `gGamePreferences.Save()`
pub const EE_UNITS_PH_PREFERENCES_SAVE: i32 = 191;
/// the save request (the name) sent to every player, waiting up to 15 s
pub const EE_UNITS_PH_SEND_SAVE_REQUEST: i32 = 192;
/// the base's `RTSPhysics::HandleMessage(c, a, b)`
pub const EE_UNITS_PH_RTS_HANDLE_MESSAGE: i32 = 193;
/// `gWorld.mLock->WriteLock()`
pub const EE_UNITS_PH_WORLD_LOCK: i32 = 194;
/// `gWorld.mLock->WriteUnlock()`
pub const EE_UNITS_PH_WORLD_UNLOCK: i32 = 195;
/// `gWorld.mScenarioManager && !gScenarioSimDialogs`
pub const EE_UNITS_PH_SCENARIO_DIALOGS: i32 = 196;
/// `gWorld.mScenarioManager->ShowDialogMessage()`
pub const EE_UNITS_PH_SHOW_DIALOG: i32 = 197;
/// `Init()`
pub const EE_UNITS_PH_INIT: i32 = 198;
/// `AutoSave()`
pub const EE_UNITS_PH_AUTOSAVE: i32 = 199;
/// `mLoadSaveRequests.push_back(message a)`
pub const EE_UNITS_PH_REQUESTS_PUSH: i32 = 200;
/// player `a`'s `UnitsSwapped(mUnitSwaps)`
pub const EE_UNITS_PH_UNITS_SWAPPED: i32 = 201;
/// `SwapUnit(unit a, unit b)`
pub const EE_UNITS_PH_SWAP_UNIT: i32 = 202;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_PH_UNIT_DEREFERENCE: i32 = 203;
/// `mUnitSwaps.clear()`
pub const EE_UNITS_PH_SWAPS_CLEAR: i32 = 204;
/// `gWorld.mCameraControl.mUnitA`
pub const EE_UNITS_PH_CAMERA_UNIT_A: i32 = 205;
/// `gWorld.mCameraControl.mUnitB`
pub const EE_UNITS_PH_CAMERA_UNIT_B: i32 = 206;
/// the camera follows unit `a` (its following, turn yaw and pitch kept)
pub const EE_UNITS_PH_CAMERA_FOLLOW: i32 = 207;
/// `gWorld.mCameraControl.ClearUnitA()`
pub const EE_UNITS_PH_CAMERA_CLEAR_A: i32 = 208;
/// `gWorld.mCameraControl.ClearUnitB()`
pub const EE_UNITS_PH_CAMERA_CLEAR_B: i32 = 209;
/// `gWorld.GetContagionManager()->SwapUnit(unit a, unit b)`
pub const EE_UNITS_PH_CONTAGION_SWAP: i32 = 210;
/// `gWorld.mScenarioManager->ReplaceUnit(unit a's mUnitID, unit b's)`
pub const EE_UNITS_PH_SCENARIO_REPLACE: i32 = 211;
/// `gEffectManager->ReplaceUnit(unit a, unit b)`
pub const EE_UNITS_PH_EFFECTS_REPLACE: i32 = 212;
/// `gAEManager.Tick(kAreaEffectInterval)`
pub const EE_UNITS_PH_AREA_EFFECTS_TICK: i32 = 213;
/// `mTicksPerSecond`
pub const EE_UNITS_PH_TICKS_PER_SECOND: i32 = 214;
/// the bits of `gWorld.GetGameSpeed()`
pub const EE_UNITS_PH_GAME_SPEED: i32 = 215;
/// `gFreeCamera`
pub const EE_UNITS_PH_FREE_CAMERA: i32 = 216;
/// `Lock()`
pub const EE_UNITS_PH_LOCK: i32 = 217;
/// `Unlock()`
pub const EE_UNITS_PH_UNLOCK: i32 = 218;
/// message `a`'s `Reference()`
pub const EE_UNITS_PH_REFERENCE: i32 = 219;
/// unit `a`'s `Reference()`
pub const EE_UNITS_PH_UNIT_REFERENCE: i32 = 220;
/// `mUnitSwaps.push_back({complex a, complex b})`
pub const EE_UNITS_PH_SWAPS_PUSH: i32 = 221;
/// `mThread != NULL`
pub const EE_UNITS_PH_HAS_THREAD: i32 = 222;
/// `mThread->Post(c, NULL, NULL, true)`
pub const EE_UNITS_PH_POST: i32 = 223;
/// `*(unsigned long*)a = c`
pub const EE_UNITS_PH_SET_ULONG: i32 = 224;
/// `ESVERIFY_AT(the dump file != NULL, 544)`
pub const EE_UNITS_PH_DUMP_VERIFY: i32 = 225;

// ---- the bodies (`ee_units_ph_body`'s `what`)
pub const EE_UNITS_PH_DO_TICK: i32 = 1;
pub const EE_UNITS_PH_DO_DRAIN_INPUT: i32 = 2;
pub const EE_UNITS_PH_DO_OUT_OF_SYNCH_CHECKSUM: i32 = 3;
pub const EE_UNITS_PH_DO_END_GAME: i32 = 4;
pub const EE_UNITS_PH_DO_INIT: i32 = 5;
pub const EE_UNITS_PH_DO_TIME_STRING: i32 = 6;
pub const EE_UNITS_PH_DO_MINUTES_AND_SECONDS: i32 = 7;
pub const EE_UNITS_PH_DO_UNIT_SWAP_KIND: i32 = 8;
pub const EE_UNITS_PH_DO_UPDATE_CIV_TIMER: i32 = 9;
pub const EE_UNITS_PH_DO_SERVE_LOAD_SAVE: i32 = 10;
pub const EE_UNITS_PH_DO_CHECK_SHIFT_MODE: i32 = 11;
pub const EE_UNITS_PH_DO_UPDATE_WEATHER: i32 = 12;
pub const EE_UNITS_PH_DO_REPORT_STATISTICS: i32 = 13;
pub const EE_UNITS_PH_DO_AUTOSAVE: i32 = 14;
pub const EE_UNITS_PH_DO_FLUSH_WORLD: i32 = 15;
pub const EE_UNITS_PH_DO_RUN_OWED_TICKS: i32 = 16;
pub const EE_UNITS_PH_DO_CLEAR_SERVER_MESSAGES: i32 = 17;
pub const EE_UNITS_PH_DO_HANDLE_MESSAGE: i32 = 18;
pub const EE_UNITS_PH_DO_PROCESS_UNIT_SWAPS: i32 = 19;
pub const EE_UNITS_PH_DO_SWAP_UNIT: i32 = 20;
pub const EE_UNITS_PH_DO_AREA_EFFECTS_CLOCK: i32 = 21;
pub const EE_UNITS_PH_DO_UPDATE_TIMING: i32 = 22;
pub const EE_UNITS_PH_DO_IDLE: i32 = 23;
pub const EE_UNITS_PH_DO_QUEUE_MESSAGE: i32 = 24;
pub const EE_UNITS_PH_DO_QUEUE_UNIT_SWAP: i32 = 25;
pub const EE_UNITS_PH_DO_POST: i32 = 26;

// ---- the original's constants
/// `kNetMessagePhysicsTick`, `kNetMessageRemovePlayer`
const NET_MESSAGE_PHYSICS_TICK: i64 = 0x28;
const NET_MESSAGE_REMOVE_PLAYER: i64 = 0x2b;
/// the selection messages (`kMessageSelectUnit` .. `kMessageSelectTheseUnits`): executed as selections
const SELECTION_MESSAGES: [i64; 11] = [0x48, 0x49, 0x4a, 0x4b, 0x4e, 0x4f, 0x50, 0x76, 0x82, 0x84, 0x96];
/// the area effects' period (`kAreaEffectInterval`, ms of game time)
const AREA_EFFECT_INTERVAL: f32 = 2000.0;
/// the shift mode's check period (ms of game time)
const SHIFT_MODE_PERIOD: f32 = 4000.0;
/// the wind direction's random walk step (`kWindDrift`)
const WIND_DRIFT: f32 = 0.005;

/// The C++ statements of the bodies.
pub trait PhHost {
    /// one statement (`EE_UNITS_PH_*`) on the physics
    fn ph(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64;
}

fn s<H: PhHost + ?Sized>(h: &mut H, op: i32) -> i64 {
    h.ph(op, 0, 0, 0, 0, &mut [0.0; 4])
}
fn sa<H: PhHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.ph(op, a, 0, 0, 0, &mut [0.0; 4])
}
fn sc<H: PhHost + ?Sized>(h: &mut H, op: i32, c: i64) -> i64 {
    h.ph(op, 0, 0, c, 0, &mut [0.0; 4])
}
fn sac<H: PhHost + ?Sized>(h: &mut H, op: i32, a: u64, c: i64) -> i64 {
    h.ph(op, a, 0, c, 0, &mut [0.0; 4])
}
fn sab<H: PhHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64) -> i64 {
    h.ph(op, a, b, 0, 0, &mut [0.0; 4])
}
fn scd<H: PhHost + ?Sized>(h: &mut H, op: i32, c: i64, d: i64) -> i64 {
    h.ph(op, 0, 0, c, d, &mut [0.0; 4])
}
fn sf<H: PhHost + ?Sized>(h: &mut H, op: i32, v: f32) -> i64 {
    h.ph(op, 0, 0, 0, 0, &mut [v, 0.0, 0.0, 0.0])
}
fn yes(v: i64) -> bool {
    v != 0
}
fn flt(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}

/// `(long)` of a float as the x86 conversion does it (truncation; out of range or NaN: the integer indefinite).
fn truncate_long(value: f32) -> c_long {
    if value.is_nan() || value >= -(c_long::MIN as f32) || value < c_long::MIN as f32 {
        c_long::MIN
    } else {
        value as c_long
    }
}

/// `(unsigned long)` of a non-negative float (the game clock): truncation.
fn truncate_ulong(value: f32) -> c_ulong {
    value as c_ulong
}

/// The physics' plain members behind their raw pointer: one field read or written at a time.
pub struct Rec(pub *mut EeUnitsPhysicsSaved);

macro_rules! get {
    ($r:expr, $f:ident) => {
        // SAFETY: the record is live for the body (the caller's contract)
        unsafe { std::ptr::addr_of!((*$r.0).$f).read() }
    };
}
macro_rules! set {
    ($r:expr, $f:ident, $v:expr) => {{
        let v = $v;
        // SAFETY: as get!
        unsafe { std::ptr::addr_of_mut!((*$r.0).$f).write(v) }
    }};
}

/// `Init()`: the clocks, tick counters and game flags reset; the queued server messages and load/save requests
/// dropped; the speed set again.
pub fn init<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    set!(r, mCurrentTick, 0);
    set!(r, mNumTicksDone, 0);
    set!(r, mF84, 0.0);
    set!(r, mTicksPerGameSecond, 0.0);
    set!(r, mGameTime, 0);
    set!(r, mTickSeconds, 0.0);
    set!(r, mTickRandom, 0);
    set!(r, mTimeOfDayStep, 0.0);
    set!(r, mTickTime, 0.0);
    set!(r, mGameClock, 0.0);
    set!(r, mTickStartTime, s(h, EE_UNITS_PH_TIME) as c_long);
    set!(r, mPreviousTickStartTime, s(h, EE_UNITS_PH_TIME) as c_long);
    set!(r, mAreaEffectsClock, 0.0);
    set!(r, mTickTimeMs, 0);
    set!(r, mShiftModeClock, 0.0);
    set!(r, mTicksToProcess, 0);
    set!(r, mTickGapSum, 0);
    set!(r, mTickGapCount, 0);
    set!(r, mTickWorkSum, 0);
    set!(r, mTickWorkCount, 0);
    set!(r, mAveragePhysicsUpdateTime, 0);
    set!(r, mAverageTimeBetweenPhysicsTicks, 0);
    set!(r, mLD4, 0);
    set!(r, mTimeOfDayRuns, false);
    set!(r, mGameStarted, false);
    set!(r, mBE2, false);
    set!(r, mCivTimerRuns, true);
    set!(r, mCivTimerFirstWarning, false);
    set!(r, mCivTimerSecondWarning, false);
    set!(r, mGameOver, false);
    set!(r, mBE7, false);
    set!(r, mResourceTimerPending, true);
    s(h, EE_UNITS_PH_CLEAR_SERVER_MESSAGES);
    s(h, EE_UNITS_PH_SAVE_LOCK_ENTER);
    let mut i = 0u32;
    while i < s(h, EE_UNITS_PH_REQUESTS) as u32 {
        sc(h, EE_UNITS_PH_REQUEST_DEREFERENCE, i64::from(i));
        i += 1;
    }
    s(h, EE_UNITS_PH_REQUESTS_CLEAR);
    s(h, EE_UNITS_PH_SAVE_LOCK_LEAVE);
    s(h, EE_UNITS_PH_INPUTS_PROCESSED_CLEAR);
    let speed = flt(s(h, EE_UNITS_PH_SPEED));
    sf(h, EE_UNITS_PH_SET_SPEED, speed);
}

/// `Tick()`: one physics tick (the loaded game's fix-ups first; the game clock, civ and resource timers, weather,
/// paths, area effects, units, effects, calamities, AI, scenario and game over; the timing averages; the autosave
/// clock on a server).
pub fn tick<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    if yes(s(h, EE_UNITS_PH_CAPTURE_ACTIVE)) {
        sc(h, EE_UNITS_PH_UNIT_STORE_CHECK, 0);
        s(h, EE_UNITS_PH_WORLD_VIEW_CHECK);
    }
    if yes(s(h, EE_UNITS_PH_LOADED_SEED_PENDING)) {
        s(h, EE_UNITS_PH_CLEAR_LOADED_SEED_PENDING);
        s(h, EE_UNITS_PH_SET_LOADED_SEED);
    }
    for which in 0..2 {
        if yes(sc(h, EE_UNITS_PH_FIXUPS_EMPTY, which)) {
            continue;
        }
        sc(h, EE_UNITS_PH_FIXUPS_TAKE, which);
        let n = s(h, EE_UNITS_PH_FIXUPS_COUNT);
        for i in 0..n {
            sc(h, EE_UNITS_PH_FIXUP_RUN, i);
        }
        if which == 0 {
            s(h, EE_UNITS_PH_UNIT_ORDER_RESET);
        }
        s(h, EE_UNITS_PH_FIXUPS_CLEAR);
    }
    s(h, EE_UNITS_PH_REPORT_STATISTICS);
    if get!(r, mTicksToProcess) == 0 {
        s(h, EE_UNITS_PH_DRAIN_INPUT);
        s(h, EE_UNITS_PH_SERVE_LOAD_SAVE);
    }
    if get!(r, mTicksToProcess) != 0
        || yes(s(h, EE_UNITS_PH_PAUSED))
        || get!(r, mGameOver)
        || yes(s(h, EE_UNITS_PH_NO_CAMERA_UNIT))
    {
        s(h, EE_UNITS_PH_CAMERA_UPDATE);
    }
    if get!(r, mTicksToProcess) == 0 {
        return;
    }
    set!(r, mGameClock, get!(r, mTickTime) + get!(r, mGameClock));
    set!(r, mGameTime, truncate_long(get!(r, mGameClock)));
    let random = flt(s(h, EE_UNITS_PH_RANDOM_GENERATE));
    set!(r, mTickRandom, ee_base::math::um::lrintf(random) as c_long);
    set!(r, mPreviousTickStartTime, get!(r, mTickStartTime));
    set!(r, mTickStartTime, s(h, EE_UNITS_PH_TIME) as c_long);
    set!(r, mTicksToProcess, get!(r, mTicksToProcess).wrapping_sub(1));
    if get!(r, mGameOver) {
        return;
    }
    s(h, EE_UNITS_PH_UPDATE_ACTIVE_UNITS);
    if get!(r, mCivTimerRuns) {
        s(h, EE_UNITS_PH_UPDATE_CIV_TIMER);
    }
    if get!(r, mResourceTimerPending) {
        let resource_time = s(h, EE_UNITS_PH_RESOURCE_TIME) as c_long;
        if resource_time as f32 <= get!(r, mGameClock) {
            set!(r, mResourceTimerPending, false);
            if s(h, EE_UNITS_PH_RESOURCES) as c_long == 4 && yes(s(h, EE_UNITS_PH_HAS_JIT_POWER)) {
                s(h, EE_UNITS_PH_JIT_TEXT);
                s(h, EE_UNITS_PH_CHAT_TEXT);
            }
        }
    }
    s(h, EE_UNITS_PH_UPDATE_WEATHER);
    s(h, EE_UNITS_PH_PATH_UPDATE);
    s(h, EE_UNITS_PH_AREA_EFFECTS_CLOCK);
    s(h, EE_UNITS_PH_CHECK_SHIFT_MODE);
    s(h, EE_UNITS_PH_UPDATE_UNITS);
    s(h, EE_UNITS_PH_EFFECTS_UPDATE);
    s(h, EE_UNITS_PH_CALAMITIES_UPDATE);
    s(h, EE_UNITS_PH_AI_UPDATE);
    let mut result = 0;
    if !yes(s(h, EE_UNITS_PH_IN_CINEMATIC)) {
        result = s(h, EE_UNITS_PH_SCENARIO_RESULT);
    }
    s(h, EE_UNITS_PH_SCENARIO_UPDATE);
    let mut new_result = if yes(s(h, EE_UNITS_PH_IN_CINEMATIC)) { 0 } else { s(h, EE_UNITS_PH_SCENARIO_RESULT) };
    if new_result == 0 {
        if !yes(s(h, EE_UNITS_PH_IN_CINEMATIC)) {
            new_result = s(h, EE_UNITS_PH_CHECK_GAME_OVER);
            if new_result != 0 {
                sc(h, EE_UNITS_PH_SET_SCENARIO_RESULT, new_result);
                sc(h, EE_UNITS_PH_END_GAME, i64::from(new_result == 1));
            }
        }
    } else if result == 0 {
        sc(h, EE_UNITS_PH_END_GAME, i64::from(new_result == 1));
    }
    s(h, EE_UNITS_PH_FLUSH_WORLD);
    let start = get!(r, mTickStartTime) as c_ulong;
    let now = s(h, EE_UNITS_PH_TIME) as c_ulong;
    set!(r, mTickWorkSum, get!(r, mTickWorkSum).wrapping_add(now.wrapping_sub(start)));
    set!(r, mTickWorkCount, get!(r, mTickWorkCount).wrapping_add(1));
    if get!(r, mTickWorkSum) >= 1000 {
        set!(r, mAveragePhysicsUpdateTime, (get!(r, mTickWorkSum) / get!(r, mTickWorkCount)) as c_long);
        set!(r, mTickWorkSum, 0);
        set!(r, mTickWorkCount, 0);
    }
    let previous = get!(r, mPreviousTickStartTime) as c_ulong;
    set!(r, mTickGapSum, get!(r, mTickGapSum).wrapping_add(start.wrapping_sub(previous)));
    set!(r, mTickGapCount, get!(r, mTickGapCount).wrapping_add(1));
    if get!(r, mTickGapSum) >= 1000 {
        set!(r, mAverageTimeBetweenPhysicsTicks, (get!(r, mTickGapSum) / get!(r, mTickGapCount)) as c_long);
        set!(r, mTickGapSum, 0);
        set!(r, mTickGapCount, 0);
    }
    set!(r, mNumTicksDone, get!(r, mNumTicksDone).wrapping_add(1));
    if get!(r, mGameStarted) {
        set!(r, mGameStarted, false);
        let campaign = s(h, EE_UNITS_PH_CAMPAIGN_NAME_LENGTH) > 0 && s(h, EE_UNITS_PH_CAMPAIGN_CHECKSUM) > 0;
        if (campaign || yes(s(h, EE_UNITS_PH_SCENARIO_GAME))) && s(h, EE_UNITS_PH_AVAILABLE_POINTS) as c_ulong > 0 {
            sc(h, EE_UNITS_PH_CIV_BLINK, 1);
        }
    }
    if !yes(s(h, EE_UNITS_PH_IS_SERVER)) {
        return;
    }
    let period = s(h, EE_UNITS_PH_AUTOSAVE_PERIOD) as c_ulong;
    if period == 0 {
        return;
    }
    let interval = s(h, EE_UNITS_PH_TIMER_INTERVAL) as c_ulong;
    set!(r, mAutoSaveClock, (get!(r, mAutoSaveClock) as c_ulong).wrapping_add(interval) as c_long);
    if !(get!(r, mAutoSaveClock) as c_ulong > s(h, EE_UNITS_PH_AUTOSAVE_PERIOD) as c_ulong)
        || yes(s(h, EE_UNITS_PH_IN_CINEMATIC))
        || yes(s(h, EE_UNITS_PH_SHOWING_DIALOG))
    {
        return;
    }
    set!(r, mAutoSaveClock, 0);
    s(h, EE_UNITS_PH_POST_AUTOSAVE);
    s(h, EE_UNITS_PH_CURRENT_CT);
    s(h, EE_UNITS_PH_CURRENT_CT);
}

/// `DrainInput()`: the queued tick messages (one while the timer runs): the checksum asked, the owed ticks and the
/// speed taken, the tick's player inputs executed; a stray non-tick message at the head dropped.
pub fn drain_input<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    s(h, EE_UNITS_PH_QUEUE_LOCK_ENTER);
    while s(h, EE_UNITS_PH_SERVER_MESSAGES) as u64 > 0 {
        if get!(r, mTicksToProcess) != 0 && yes(s(h, EE_UNITS_PH_TIMER_RUNNING)) {
            break;
        }
        let message = s(h, EE_UNITS_PH_FRONT) as u64;
        if sa(h, EE_UNITS_PH_MESSAGE_TYPE, message) != NET_MESSAGE_PHYSICS_TICK {
            s(h, EE_UNITS_PH_POP_FRONT);
            sa(h, EE_UNITS_PH_DEREFERENCE, message);
            continue;
        }
        s(h, EE_UNITS_PH_POP_FRONT);
        if yes(sac(h, EE_UNITS_PH_RECORD_BOOL, message, 2))
            && (yes(s(h, EE_UNITS_PH_IS_SERVER))
                || (yes(s(h, EE_UNITS_PH_IS_CLIENT)) && s(h, EE_UNITS_PH_HAS_LEFT_GAME) == 0))
        {
            let dump = sac(h, EE_UNITS_PH_RECORD_BOOL, message, 3);
            let checksum = sc(h, EE_UNITS_PH_OUT_OF_SYNCH_CHECKSUM, dump);
            if !yes(dump) {
                let ticks = s(h, EE_UNITS_PH_CTS_PROCESSED);
                let response = scd(h, EE_UNITS_PH_CHECKSUM_RESPONSE, ticks, checksum) as u64;
                sa(h, EE_UNITS_PH_SET_SLOT_CURRENT, response);
                if yes(s(h, EE_UNITS_PH_IS_CLIENT)) {
                    sa(h, EE_UNITS_PH_LOCAL_INPUT, response);
                } else {
                    sa(h, EE_UNITS_PH_LOG_CHECKSUM_RESPONSE, response);
                    sa(h, EE_UNITS_PH_DEREFERENCE, response);
                }
            }
        }
        s(h, EE_UNITS_PH_INCREMENT_CTS);
        sc(h, EE_UNITS_PH_SET_AVERAGE_UPDATE, get!(r, mAveragePhysicsUpdateTime) as i64);
        sc(h, EE_UNITS_PH_SET_AVERAGE_BETWEEN, get!(r, mAverageTimeBetweenPhysicsTicks) as i64);
        let ticks = sac(h, EE_UNITS_PH_RECORD_BYTE, message, 1) as c_ulong;
        set!(r, mCurrentTick, get!(r, mCurrentTick).wrapping_add(ticks));
        set!(r, mTicksToProcess, sac(h, EE_UNITS_PH_RECORD_BYTE, message, 1) as c_ulong);
        let per_second = sac(h, EE_UNITS_PH_RECORD_BYTE, message, 4) as i32;
        sf(h, EE_UNITS_PH_SET_SPEED, 1.0f32 / per_second as f32);
        let mut count = sac(h, EE_UNITS_PH_RECORD_U32, message, 0) as u32;
        sa(h, EE_UNITS_PH_DEREFERENCE, message);
        s(h, EE_UNITS_PH_CTS_PROCESSED);
        while count > 0 {
            let input = s(h, EE_UNITS_PH_FRONT) as u64;
            s(h, EE_UNITS_PH_POP_FRONT);
            sa(h, EE_UNITS_PH_MESSAGE_LABEL, input);
            if yes(sa(h, EE_UNITS_PH_FOR_NO_SLOT, input)) {
                s(h, EE_UNITS_PH_INPUTS_PROCESSED_INCREMENT);
            }
            sa(h, EE_UNITS_PH_CAPTURE_EXECUTED, input);
            if yes(s(h, EE_UNITS_PH_CAPTURE_ACTIVE)) {
                let kind = sa(h, EE_UNITS_PH_MESSAGE_TYPE, input);
                sc(h, EE_UNITS_PH_UNIT_STORE_CHECK, kind);
            }
            if yes(s(h, EE_UNITS_PH_TEST_HOOK_SET)) {
                sac(h, EE_UNITS_PH_TEST_HOOK, input, 0);
            }
            let kind = sa(h, EE_UNITS_PH_MESSAGE_TYPE, input);
            if kind == NET_MESSAGE_REMOVE_PLAYER {
                sa(h, EE_UNITS_PH_REMOVE_PLAYER, input);
            } else if SELECTION_MESSAGES.contains(&kind) {
                sa(h, EE_UNITS_PH_EXECUTE_SELECTION, input);
                sac(h, EE_UNITS_PH_LOG, input, 1);
            } else {
                sac(h, EE_UNITS_PH_LOG, input, 0);
                let game = sa(h, EE_UNITS_PH_GAME_MESSAGE, input) as u64;
                if game != 0 {
                    sa(h, EE_UNITS_PH_EXECUTE_GAME_MESSAGE, game);
                } else {
                    sa(h, EE_UNITS_PH_EXECUTE, input);
                }
            }
            if yes(s(h, EE_UNITS_PH_TEST_HOOK_SET)) {
                sac(h, EE_UNITS_PH_TEST_HOOK, input, 1);
            }
            sa(h, EE_UNITS_PH_DEREFERENCE, input);
            count -= 1;
        }
    }
    s(h, EE_UNITS_PH_QUEUE_LOCK_LEAVE);
}

/// `GetOutOfSynchChecksum(dump, flags)`: the world's out-of-synch checksum (the parts `flags` picks); `dump` also
/// writes everything to "OOS <date>.log". The sum wraps in `unsigned long`.
pub fn out_of_synch_checksum<H: PhHost + ?Sized>(h: &mut H, r: &Rec, dump: bool, flags: u64) -> u64 {
    let mut checksum: u64 = 0;
    s(h, EE_UNITS_PH_TIME);
    if dump {
        write_dump(h, r, flags);
    }
    if flags & 1 != 0 {
        checksum = checksum.wrapping_add(crate::units::ffi::ee_units_store_checksum_flag1() as c_ulong as u64);
    }
    if flags & 2 != 0 {
        checksum = checksum.wrapping_add(crate::units::ffi::ee_units_store_checksum_flag2() as c_ulong as u64);
    }
    let unit_flags = flags & !3u64;
    if unit_flags & (0x20 | 8) != 0 {
        let mut unit = s(h, EE_UNITS_PH_FIRST_UNIT) as u64;
        while unit != 0 {
            checksum = checksum.wrapping_add(sac(h, EE_UNITS_PH_UNIT_CHECKSUM, unit, unit_flags as i64) as u64);
            unit = sa(h, EE_UNITS_PH_NEXT_UNIT, unit) as u64;
        }
    }
    let mut i = 0u32;
    while i < s(h, EE_UNITS_PH_NUM_PLAYERS) as u32 {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        if flags & 0x200 != 0 {
            let group = sa(h, EE_UNITS_PH_ACTIVE_GROUP, player) as u64;
            if group != 0 {
                let mut j = 0u32;
                while j < sa(h, EE_UNITS_PH_GROUP_NUM_UNITS, group) as u32 {
                    let id = sac(h, EE_UNITS_PH_GROUP_UNIT_ID, group, i64::from(j)) as c_ulong;
                    let player_id = sa(h, EE_UNITS_PH_PLAYER_ID, player) as c_ulong;
                    checksum = checksum.wrapping_add(id.wrapping_mul(player_id) as u64);
                    j += 1;
                }
            }
        }
        if flags & 0x40 != 0 {
            for k in 0..4 {
                let traffic = sac(h, EE_UNITS_PH_AIR_TRAFFIC, player, k) as u64 as usize as *mut std::ffi::c_void;
                checksum = checksum.wrapping_add(air_traffic_checksum(traffic) as u64);
            }
        }
        i += 1;
    }
    let tick = get!(r, mCurrentTick);
    let rest = s(h, EE_UNITS_PH_SEED_AND_INPUTS) as c_ulong;
    checksum = checksum.wrapping_add(tick.wrapping_add(rest) as u64);
    checksum = checksum.wrapping_add(s(h, EE_UNITS_PH_CP_CHECKSUM) as u64);
    if flags & 4 != 0 {
        checksum = checksum.wrapping_add(s(h, EE_UNITS_PH_MAP_CHECKSUM) as u64);
    }
    if flags & 0x100 != 0 {
        checksum = checksum.wrapping_add(s(h, EE_UNITS_PH_CONTINENT_CHECKSUM) as u64);
    }
    if flags & 0x10 != 0 {
        checksum = checksum.wrapping_add(s(h, EE_UNITS_PH_CALAMITY_CHECKSUM) as u64);
    }
    s(h, EE_UNITS_PH_TIME);
    let checksum = checksum as c_ulong as u64;
    sc(h, EE_UNITS_PH_SET_LAST_CHECKSUM, checksum as i64);
    checksum
}

/// `ee_units_atc_checksum` of an air traffic control (Rust; none in the unit tests)
fn air_traffic_checksum(traffic: *mut std::ffi::c_void) -> c_ulong {
    #[cfg(not(test))]
    {
        // SAFETY: the player's live air traffic control
        unsafe { crate::units::atc::ee_units_atc_checksum(traffic.cast()) as c_ulong }
    }
    #[cfg(test)]
    {
        traffic as usize as c_ulong
    }
}

/// The out-of-synch dump (GetOutOfSynchChecksum's `inDump`): every player's groups and air traffic, the paths, the
/// map and calamities when asked, every unit, the counters.
fn write_dump<H: PhHost + ?Sized>(h: &mut H, _r: &Rec, flags: u64) {
    s(h, EE_UNITS_PH_TIME_OF_DAY);
    s(h, EE_UNITS_PH_DUMP_NAME);
    s(h, EE_UNITS_PH_DUMP_OPEN);
    s(h, EE_UNITS_PH_DUMP_VERIFY);
    let mut i = 0u32;
    while i < s(h, EE_UNITS_PH_NUM_PLAYERS) as u32 {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        s(h, EE_UNITS_PH_DUMP_NEWLINE);
        sa(h, EE_UNITS_PH_DUMP_PLAYER, player);
        let group = sa(h, EE_UNITS_PH_ACTIVE_GROUP, player) as u64;
        if group != 0 {
            s(h, EE_UNITS_PH_DUMP_NEWLINE);
            let n = sa(h, EE_UNITS_PH_GROUP_NUM_UNITS, group);
            sc(h, EE_UNITS_PH_DUMP_SELECTED_COUNT, n);
            let mut j = 0u32;
            while j < sa(h, EE_UNITS_PH_GROUP_NUM_UNITS, group) as u32 {
                let id = sac(h, EE_UNITS_PH_GROUP_UNIT_ID, group, i64::from(j));
                scd(h, EE_UNITS_PH_DUMP_UNIT, i64::from(j), id);
                j += 1;
            }
        }
        s(h, EE_UNITS_PH_DUMP_NEWLINE);
        let n = sa(h, EE_UNITS_PH_RUNNING_GROUPS, player);
        sc(h, EE_UNITS_PH_DUMP_ACTIVE_COUNT, n);
        let mut j = 0u32;
        while j < sa(h, EE_UNITS_PH_RUNNING_GROUPS, player) as u32 {
            let active = sac(h, EE_UNITS_PH_RUNNING_GROUP, player, i64::from(j)) as u64;
            s(h, EE_UNITS_PH_DUMP_NEWLINE);
            let units = sa(h, EE_UNITS_PH_ACTIVE_GROUP_NUM_UNITS, active);
            scd(h, EE_UNITS_PH_DUMP_ACTIVE_GROUP, i64::from(j), units);
            let mut m = 0u32;
            while m < sa(h, EE_UNITS_PH_ACTIVE_GROUP_NUM_UNITS, active) as u32 {
                let id = sac(h, EE_UNITS_PH_ACTIVE_GROUP_UNIT_ID, active, i64::from(m));
                scd(h, EE_UNITS_PH_DUMP_UNIT, i64::from(m), id);
                m += 1;
            }
            j += 1;
        }
        for k in 0..4 {
            let traffic = sac(h, EE_UNITS_PH_AIR_TRAFFIC, player, k) as u64;
            sa(h, EE_UNITS_PH_DUMP_AIR_TRAFFIC, traffic);
        }
        i += 1;
    }
    s(h, EE_UNITS_PH_DUMP_PATHS);
    if flags & 4 != 0 {
        s(h, EE_UNITS_PH_DUMP_MAP);
    }
    if flags & 0x10 != 0 {
        s(h, EE_UNITS_PH_DUMP_CALAMITIES);
        s(h, EE_UNITS_PH_DUMP_CONTAGIONS);
    }
    let mut unit = s(h, EE_UNITS_PH_FIRST_UNIT) as u64;
    while unit != 0 {
        sac(h, EE_UNITS_PH_DUMP_UNIT_STATE, unit, flags as i64);
        unit = sa(h, EE_UNITS_PH_NEXT_UNIT, unit) as u64;
    }
    s(h, EE_UNITS_PH_DUMP_CLOSE);
}

/// `EndGame(victory)`: every non-observer player but nature wins or loses, a won campaign scenario is recorded, the
/// in-game options button blinks, the teams are cleared and a server pauses the game.
pub fn end_game<H: PhHost + ?Sized>(h: &mut H, r: &Rec, victory: bool) {
    s(h, EE_UNITS_PH_SET_GAME_ENDED);
    let mut i = 1u32;
    while i < s(h, EE_UNITS_PH_NUM_PLAYERS) as u32 {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        if !yes(sa(h, EE_UNITS_PH_OBSERVER, player)) {
            scd(h, EE_UNITS_PH_REMOVE_FROM_GAME, i64::from(victory), i64::from(i));
            let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
            sac(h, EE_UNITS_PH_SET_GAME_RESULT, player, if victory { 1 } else { 2 });
        }
        i += 1;
    }
    if s(h, EE_UNITS_PH_CAMPAIGN_NAME_LENGTH) > 0
        && s(h, EE_UNITS_PH_CAMPAIGN_CHECKSUM) > 0
        && s(h, EE_UNITS_PH_CURRENT_GAME_RESULT) == 1
        && s(h, EE_UNITS_PH_PROGRESS_LOAD) == 0
    {
        s(h, EE_UNITS_PH_PROGRESS_SAVE);
    }
    if s(h, EE_UNITS_PH_CURRENT_GAME_RESULT) != 0 {
        sc(h, EE_UNITS_PH_CIV_BLINK, 0);
        if yes(s(h, EE_UNITS_PH_MAIN_GAME_CURRENT)) {
            s(h, EE_UNITS_PH_OPTIONS_BLINK);
        }
    }
    s(h, EE_UNITS_PH_TEAMS_CLEAR);
    s(h, EE_UNITS_PH_APP_TEAMS_CLEAR);
    if yes(s(h, EE_UNITS_PH_IS_SERVER)) {
        s(h, EE_UNITS_PH_PAUSE_GAME);
    }
    set!(r, mGameOver, true);
    s(h, EE_UNITS_PH_PROGRESS_RELEASE);
}

/// `GetTimeString()`: the game speed and the elapsed game time ("<speed> hh:mm:ss") into the string at `text` (the
/// original's unused time-of-day conversions are pure and left out).
pub fn time_string<H: PhHost + ?Sized>(h: &mut H, text: u64) {
    let time = flt(s(h, EE_UNITS_PH_GAME_CLOCK));
    let hours: c_ulong = if time >= 3_600_000.0 {
        ee_base::math::um::floor_unsigned(time * 2.7777778e-07) as c_ulong
    } else {
        0
    };
    let mut minutes: c_ulong = 0;
    if time >= 60000.0 {
        minutes = truncate_ulong(time).wrapping_sub(hours.wrapping_mul(3_600_000)) / 60000;
    }
    let seconds =
        truncate_ulong(time).wrapping_sub(hours.wrapping_mul(3_600_000)).wrapping_sub(minutes.wrapping_mul(60000)) / 1000;
    let wide = hours >= 100;
    // c: the hours, d: the minutes and seconds (low 32 bits each), b: the wide format
    h.ph(
        EE_UNITS_PH_TIME_TEXT,
        text,
        u64::from(wide),
        hours as i64,
        ((minutes as u64 & 0xffff_ffff) | ((seconds as u64) << 32)) as i64,
        &mut [0.0; 4],
    );
}

/// `GetMinutesAndSeconds(out_minutes, out_seconds, time)`: a game time in ms (0: the current one) as whole minutes
/// and the seconds past them (`unsigned long*` outs).
pub fn minutes_and_seconds<H: PhHost + ?Sized>(h: &mut H, r: &Rec, out_minutes: u64, out_seconds: u64, time: c_ulong) {
    let time = if time == 0 { get!(r, mGameTime) as c_ulong } else { time };
    let minutes = time / 1000 / 60;
    sac(h, EE_UNITS_PH_SET_ULONG, out_minutes, minutes as i64);
    sac(h, EE_UNITS_PH_SET_ULONG, out_seconds, (time.wrapping_sub(minutes.wrapping_mul(60000)) / 1000) as i64);
}

/// `GetUnitSwapKind(unit)`: the unit's role in the queued unit swaps (the earliest wins): old 1, new 3; another
/// owner 2 / 4; citizen to non-citizen 5 / 7, non-citizen to citizen 6 / 8; 0 when it is not swapped.
pub fn unit_swap_kind<H: PhHost + ?Sized>(h: &mut H, unit: u64) -> i64 {
    let mut result = 0;
    let mut i = s(h, EE_UNITS_PH_SWAPS) as i32 - 1;
    while i >= 0 {
        let old = sc(h, EE_UNITS_PH_SWAP_OLD, i64::from(i)) as u64;
        let new = sc(h, EE_UNITS_PH_SWAP_NEW, i64::from(i)) as u64;
        let base = if old == unit {
            Some(1)
        } else if new == unit {
            Some(3)
        } else {
            None
        };
        if let Some(base) = base {
            if sa(h, EE_UNITS_PH_UNIT_PLAYER, old) == sa(h, EE_UNITS_PH_UNIT_PLAYER, new) {
                result = base;
                let old_citizen = yes(sa(h, EE_UNITS_PH_IS_CITIZEN, old));
                let new_citizen = yes(sa(h, EE_UNITS_PH_IS_CITIZEN, new));
                if old_citizen && !new_citizen {
                    result = base + 4;
                } else if !old_citizen && new_citizen {
                    result = base + 5;
                }
            } else {
                result = base + 1;
            }
        }
        i -= 1;
    }
    result
}

/// `UpdateCivTimer()`: on a server the civilization timer's warnings (240 s, 270 s) and expiry (300 s), the civ
/// builder button blinking for a player without a civilization; elsewhere (or with no civ buying) the timer stops.
pub fn update_civ_timer<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    if yes(s(h, EE_UNITS_PH_HAS_COMMUNICATIONS)) && yes(s(h, EE_UNITS_PH_IS_SERVER)) {
        if !yes(s(h, EE_UNITS_PH_CAN_BUY_CIVILIZATION)) {
            set!(r, mCivTimerRuns, false);
            return;
        }
        if get!(r, mTickWorkCount) == 0
            && !yes(s(h, EE_UNITS_PH_CIVILIZATION_CHOSEN))
            && !yes(s(h, EE_UNITS_PH_CURRENT_OBSERVER))
        {
            sc(h, EE_UNITS_PH_CIV_BLINK, 1);
        }
        let time = get!(r, mGameTime) as c_ulong;
        let code = if time > s(h, EE_UNITS_PH_CIV_TIMER_LIMIT) as c_ulong {
            set!(r, mCivTimerRuns, false);
            0
        } else if time > s(h, EE_UNITS_PH_CIV_TIMER_SECOND) as c_ulong && !get!(r, mCivTimerSecondWarning) {
            set!(r, mCivTimerSecondWarning, true);
            1
        } else if time > s(h, EE_UNITS_PH_CIV_TIMER_FIRST) as c_ulong && !get!(r, mCivTimerFirstWarning) {
            set!(r, mCivTimerFirstWarning, true);
            1
        } else {
            return;
        };
        sc(h, EE_UNITS_PH_CIV_TIMER_MESSAGE, code);
    } else {
        set!(r, mCivTimerRuns, false);
        if yes(s(h, EE_UNITS_PH_CAN_BUY_CIVILIZATION))
            && !yes(s(h, EE_UNITS_PH_CIVILIZATION_CHOSEN))
            && !yes(s(h, EE_UNITS_PH_CURRENT_OBSERVER))
        {
            sc(h, EE_UNITS_PH_CIV_BLINK, 1);
        }
    }
}

/// `ServeLoadSaveRequests()`: between ticks every queued load/save request runs (under the save-game lock) and one
/// response carries the last result (logged on a server); with ticks owed it waits.
pub fn serve_load_save<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    s(h, EE_UNITS_PH_SAVE_LOCK_ENTER);
    if s(h, EE_UNITS_PH_REQUESTS) as u64 > 0 {
        if get!(r, mTicksToProcess) == 0 {
            let mut result = 0;
            let mut i = 0u32;
            while i < s(h, EE_UNITS_PH_REQUESTS) as u32 {
                result = sc(h, EE_UNITS_PH_LOAD_OR_SAVE, i64::from(i));
                sc(h, EE_UNITS_PH_REQUEST_DEREFERENCE, i64::from(i));
                sc(h, EE_UNITS_PH_REQUEST_CLEAR, i64::from(i));
                i += 1;
            }
            s(h, EE_UNITS_PH_REQUESTS_CLEAR);
            let response = sc(h, EE_UNITS_PH_LOAD_SAVE_RESPONSE, result) as u64;
            sa(h, EE_UNITS_PH_SET_SLOT_CURRENT_GET, response);
            if yes(s(h, EE_UNITS_PH_IS_SERVER)) {
                sa(h, EE_UNITS_PH_LOG_RESPONSE, response);
                sa(h, EE_UNITS_PH_DEREFERENCE, response);
            } else {
                sa(h, EE_UNITS_PH_LOCAL_INPUT_SERVER, response);
            }
        } else {
            s(h, EE_UNITS_PH_CTS_PROCESSED);
            s(h, EE_UNITS_PH_CURRENT_CT);
        }
    }
    s(h, EE_UNITS_PH_SAVE_LOCK_LEAVE);
}

/// `CheckShiftMode()`: every 4 s of game time the current player leaves shift mode when the Shift key is up.
pub fn check_shift_mode<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    set!(r, mShiftModeClock, get!(r, mShiftModeClock) + get!(r, mTickTime));
    if !(get!(r, mShiftModeClock) >= SHIFT_MODE_PERIOD) {
        return;
    }
    set!(r, mShiftModeClock, get!(r, mShiftModeClock) - SHIFT_MODE_PERIOD);
    if !yes(s(h, EE_UNITS_PH_SHIFT_MODE)) {
        return;
    }
    if yes(s(h, EE_UNITS_PH_SHIFT_UP)) {
        s(h, EE_UNITS_PH_EXIT_SHIFT_MODE);
    }
}

/// `UpdateWeather()`: the day/night clock (when it runs), the wind direction's random walk, the environment's tick.
pub fn update_weather<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    if get!(r, mTimeOfDayRuns) {
        let mut time = get!(r, mTickSeconds) * 0.0016666667f32 + get!(r, mTimeOfDayStep);
        if time >= 0.0 {
            let mut time_of_day = time + flt(s(h, EE_UNITS_PH_GET_TIME_OF_DAY));
            if time_of_day >= 1.0 {
                time_of_day = 0.0;
            }
            sf(h, EE_UNITS_PH_SET_TIME_OF_DAY, time_of_day);
            time = 0.0;
        }
        set!(r, mTimeOfDayStep, time);
    }
    let angle = flt(s(h, EE_UNITS_PH_WIND_YAW));
    let elapsed = get!(r, mTickTime);
    let random = flt(s(h, EE_UNITS_PH_RANDOM_GENERATE));
    let yaw = ((random * 2.0f32 - 1.0f32) * elapsed) * WIND_DRIFT + angle;
    sf(h, EE_UNITS_PH_SET_WIND_YAW, yaw);
    let yaw = flt(s(h, EE_UNITS_PH_WIND_YAW));
    sf(h, EE_UNITS_PH_WIND_ORIENT, yaw);
    s(h, EE_UNITS_PH_WIND_DIRECTION);
    s(h, EE_UNITS_PH_ENVIRONMENT_TICK);
}

/// ReportStatistics' state (wall clock; the physics thread only): the first report done, the period, the last
/// report's time and the two values reported.
static FIRST_REPORT_DONE: AtomicBool = AtomicBool::new(false);
static LAST_REPORT_SET: AtomicBool = AtomicBool::new(false);
static REPORT_PERIOD: AtomicU64 = AtomicU64::new(0);
static LAST_REPORT: AtomicU64 = AtomicU64::new(0);
static REPORTED_FIRST: AtomicU64 = AtomicU64::new(0);
static REPORTED_SECOND: AtomicU64 = AtomicU64::new(0);

/// `ReportStatistics()`: every 2.5..5 s (a random period drawn once from the C runtime) the current player's two
/// statistics go to the others when they changed.
pub fn report_statistics<H: PhHost + ?Sized>(h: &mut H) {
    if !LAST_REPORT_SET.swap(true, Ordering::Relaxed) {
        LAST_REPORT.store(s(h, EE_UNITS_PH_TIME) as c_ulong as u64, Ordering::Relaxed);
    }
    let time = s(h, EE_UNITS_PH_TIME) as c_ulong;
    if !FIRST_REPORT_DONE.swap(true, Ordering::Relaxed) {
        s(h, EE_UNITS_PH_SEED_RAND);
        let draw = s(h, EE_UNITS_PH_RAND) as u32;
        REPORT_PERIOD.store(u64::from(draw % 2500 + 2500), Ordering::Relaxed);
    }
    let last = LAST_REPORT.load(Ordering::Relaxed) as c_ulong;
    if !(time.wrapping_sub(last) as u64 > REPORT_PERIOD.load(Ordering::Relaxed)) {
        return;
    }
    LAST_REPORT.store(time as u64, Ordering::Relaxed);
    let first = sc(h, EE_UNITS_PH_LAST_STATISTIC, 0x14) as c_ulong as u64;
    let second = sc(h, EE_UNITS_PH_LAST_STATISTIC, 0x15) as c_ulong as u64;
    if first == REPORTED_FIRST.load(Ordering::Relaxed) && second == REPORTED_SECOND.load(Ordering::Relaxed) {
        return;
    }
    REPORTED_FIRST.store(first, Ordering::Relaxed);
    REPORTED_SECOND.store(second, Ordering::Relaxed);
    scd(h, EE_UNITS_PH_REPORT_MESSAGE, first as i64, second as i64);
}

/// `AutoSave()`: with autosaves on, the file "AutoSave <date - time>" recorded in the preferences and the save
/// request sent to every player (waiting up to 15 s).
pub fn auto_save<H: PhHost + ?Sized>(h: &mut H) {
    if s(h, EE_UNITS_PH_AUTOSAVES) as c_ulong > 0 {
        s(h, EE_UNITS_PH_TIME_OF_DAY);
        s(h, EE_UNITS_PH_AUTOSAVE_NAME);
        s(h, EE_UNITS_PH_SET_LAST_AUTOSAVE);
        s(h, EE_UNITS_PH_PREFERENCES_SAVE);
        s(h, EE_UNITS_PH_SEND_SAVE_REQUEST);
    }
}

/// `FlushWorld()`: the queued placements, every player's pending units, the born units' deactivations, the queued
/// unit swaps, then every player's killed units removed.
pub fn flush_world<H: PhHost + ?Sized>(h: &mut H) {
    let players = s(h, EE_UNITS_PH_NUM_PLAYERS) as u32;
    s(h, EE_UNITS_PH_CHECK_UNIT_LIST);
    s(h, EE_UNITS_PH_FLUSH_PLACEMENTS);
    for i in 0..players {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        if player != 0 {
            sa(h, EE_UNITS_PH_ADD_PENDING_UNITS, player);
        }
    }
    s(h, EE_UNITS_PH_QUEUE_BORN_DEACTIVATIONS);
    s(h, EE_UNITS_PH_PROCESS_UNIT_SWAPS);
    for i in 0..players {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        if player != 0 {
            sa(h, EE_UNITS_PH_REMOVE_KILLED_UNITS, player);
        }
    }
}

/// `RunOwedTicks()`: every owed tick, draining the input for more whenever none are left.
pub fn run_owed_ticks<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    if get!(r, mTicksToProcess) == 0 {
        s(h, EE_UNITS_PH_DRAIN_INPUT);
    }
    while get!(r, mTicksToProcess) > 0 {
        s(h, EE_UNITS_PH_TICK);
        if get!(r, mTicksToProcess) == 0 {
            s(h, EE_UNITS_PH_DRAIN_INPUT);
        }
    }
}

/// `ClearServerMessages()`: every queued server message dropped (under the queue's lock).
pub fn clear_server_messages<H: PhHost + ?Sized>(h: &mut H) {
    s(h, EE_UNITS_PH_QUEUE_LOCK_ENTER);
    while s(h, EE_UNITS_PH_SERVER_MESSAGES) as u64 > 0 {
        let message = s(h, EE_UNITS_PH_FRONT) as u64;
        s(h, EE_UNITS_PH_POP_FRONT);
        sa(h, EE_UNITS_PH_DEREFERENCE, message);
    }
    s(h, EE_UNITS_PH_QUEUE_LOCK_LEAVE);
}

/// `HandleMessage(message, first, second)`: the physics thread's messages (start, time of day toggle and reset,
/// scenario dialog, re-init, autosave, a load/save request, 0x50c); the rest to the base.
pub fn handle_message<H: PhHost + ?Sized>(h: &mut H, r: &Rec, message: u64, first: u64, second: u64) {
    match message {
        0x404 => {
            let now = s(h, EE_UNITS_PH_TIME) as c_long;
            set!(r, mTickStartTime, now);
            set!(r, mPreviousTickStartTime, now);
            h.ph(EE_UNITS_PH_RTS_HANDLE_MESSAGE, first, second, 0x404, 0, &mut [0.0; 4]);
        }
        0x502 => set!(r, mTimeOfDayRuns, !get!(r, mTimeOfDayRuns)),
        0x503 => {
            set!(r, mTimeOfDayRuns, false);
            s(h, EE_UNITS_PH_WORLD_LOCK);
            sf(h, EE_UNITS_PH_SET_TIME_OF_DAY, 0.35);
            s(h, EE_UNITS_PH_WORLD_UNLOCK);
        }
        0x508 => {
            if yes(s(h, EE_UNITS_PH_SCENARIO_DIALOGS)) {
                s(h, EE_UNITS_PH_SHOW_DIALOG);
            }
        }
        0x509 => {
            s(h, EE_UNITS_PH_INIT);
        }
        0x50a => {
            s(h, EE_UNITS_PH_AUTOSAVE);
        }
        0x50b => {
            sa(h, EE_UNITS_PH_REQUESTS_PUSH, first);
        }
        0x50c => {
            set!(r, mLD4, first as usize as isize as c_long);
            set!(r, mBE7, true);
        }
        _ => {
            h.ph(EE_UNITS_PH_RTS_HANDLE_MESSAGE, first, second, message as i64, 0, &mut [0.0; 4]);
        }
    }
}

/// `ProcessUnitSwaps()`: every player but nature told, each pair swapped and both units released, the queue
/// cleared.
pub fn process_unit_swaps<H: PhHost + ?Sized>(h: &mut H) {
    if s(h, EE_UNITS_PH_SWAPS) as u64 == 0 {
        return;
    }
    let mut i = 1u32;
    while i < s(h, EE_UNITS_PH_NUM_PLAYERS) as u32 {
        let player = sc(h, EE_UNITS_PH_PLAYER_AT, i64::from(i)) as u64;
        sa(h, EE_UNITS_PH_UNITS_SWAPPED, player);
        i += 1;
    }
    let mut k = 0i64;
    while k < s(h, EE_UNITS_PH_SWAPS) {
        let old = sc(h, EE_UNITS_PH_SWAP_OLD, k) as u64;
        let new = sc(h, EE_UNITS_PH_SWAP_NEW, k) as u64;
        sab(h, EE_UNITS_PH_SWAP_UNIT, old, new);
        sa(h, EE_UNITS_PH_UNIT_DEREFERENCE, old);
        sa(h, EE_UNITS_PH_UNIT_DEREFERENCE, new);
        k += 1;
    }
    s(h, EE_UNITS_PH_SWAPS_CLEAR);
}

/// `SwapUnit(old, new)`: the camera follows the new unit (or drops the old one); the contagions, the scenario and
/// the effects switch to it.
pub fn swap_unit<H: PhHost + ?Sized>(h: &mut H, old: u64, new: u64) {
    let unit_a = s(h, EE_UNITS_PH_CAMERA_UNIT_A) as u64;
    if s(h, EE_UNITS_PH_CAMERA_UNIT_B) as u64 == old && unit_a == old {
        sa(h, EE_UNITS_PH_CAMERA_FOLLOW, new);
    } else {
        if unit_a == old {
            s(h, EE_UNITS_PH_CAMERA_CLEAR_A);
        }
        if s(h, EE_UNITS_PH_CAMERA_UNIT_B) as u64 == old {
            s(h, EE_UNITS_PH_CAMERA_CLEAR_B);
        }
    }
    sab(h, EE_UNITS_PH_CONTAGION_SWAP, old, new);
    sab(h, EE_UNITS_PH_SCENARIO_REPLACE, old, new);
    sab(h, EE_UNITS_PH_EFFECTS_REPLACE, old, new);
}

/// `UpdateAreaEffectsClock()`: the area effects tick once per 2 s of game time.
pub fn update_area_effects_clock<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    set!(r, mAreaEffectsClock, get!(r, mAreaEffectsClock) + get!(r, mTickTime));
    if get!(r, mAreaEffectsClock) >= AREA_EFFECT_INTERVAL {
        s(h, EE_UNITS_PH_AREA_EFFECTS_TICK);
        set!(r, mAreaEffectsClock, get!(r, mAreaEffectsClock) - AREA_EFFECT_INTERVAL);
    }
}

/// `UpdateTiming()`: the ticks per game second and the tick's length (s, ms) from the tick rate and the game speed.
pub fn update_timing<H: PhHost + ?Sized>(h: &mut H, r: &Rec) {
    let per_second = s(h, EE_UNITS_PH_TICKS_PER_SECOND) as c_ulong;
    let speed = flt(s(h, EE_UNITS_PH_GAME_SPEED));
    set!(r, mTicksPerGameSecond, per_second as f32 / speed);
    set!(r, mTickSeconds, 1.0f32 / get!(r, mTicksPerGameSecond));
    set!(r, mTickTime, get!(r, mTickSeconds) * 1000.0f32);
    set!(r, mTickTimeMs, ee_base::math::um::lrintf(get!(r, mTickTime)) as c_long);
}

/// `Idle()`: with the free camera, the input drained and the world flushed under the world lock.
pub fn idle<H: PhHost + ?Sized>(h: &mut H) {
    if !yes(s(h, EE_UNITS_PH_FREE_CAMERA)) {
        return;
    }
    s(h, EE_UNITS_PH_LOCK);
    s(h, EE_UNITS_PH_DRAIN_INPUT);
    s(h, EE_UNITS_PH_FLUSH_WORLD);
    s(h, EE_UNITS_PH_UNLOCK);
}

/// `QueueMessage(message)`: a load/save request queued under the save-game lock (a reference taken).
pub fn queue_message<H: PhHost + ?Sized>(h: &mut H, message: u64) {
    s(h, EE_UNITS_PH_SAVE_LOCK_ENTER);
    sa(h, EE_UNITS_PH_REQUESTS_PUSH, message);
    sa(h, EE_UNITS_PH_REFERENCE, message);
    s(h, EE_UNITS_PH_SAVE_LOCK_LEAVE);
}

/// `QueueUnitSwap(old, new)`: a unit swap queued (a reference to both units).
pub fn queue_unit_swap<H: PhHost + ?Sized>(h: &mut H, old: u64, new: u64) {
    sa(h, EE_UNITS_PH_UNIT_REFERENCE, old);
    sa(h, EE_UNITS_PH_UNIT_REFERENCE, new);
    sab(h, EE_UNITS_PH_SWAPS_PUSH, old, new);
}

/// `PostAutoSave` / `PostShowDialogMessage` / `PostInit`: the message posted to the physics thread; the post's
/// result (0 without a thread).
pub fn post<H: PhHost + ?Sized>(h: &mut H, message: i64) -> i64 {
    if yes(s(h, EE_UNITS_PH_HAS_THREAD)) {
        sc(h, EE_UNITS_PH_POST, message)
    } else {
        0
    }
}

/// Runs body `what` (`EE_UNITS_PH_DO_*`) on the physics whose plain members are at `r`.
pub fn run<H: PhHost + ?Sized>(h: &mut H, r: &Rec, what: i32, a: u64, b: u64, c: i64) -> i64 {
    match what {
        EE_UNITS_PH_DO_TICK => tick(h, r),
        EE_UNITS_PH_DO_DRAIN_INPUT => drain_input(h, r),
        EE_UNITS_PH_DO_OUT_OF_SYNCH_CHECKSUM => return out_of_synch_checksum(h, r, a & 0xff != 0, b) as i64,
        EE_UNITS_PH_DO_END_GAME => end_game(h, r, a & 0xff != 0),
        EE_UNITS_PH_DO_INIT => init(h, r),
        EE_UNITS_PH_DO_TIME_STRING => time_string(h, a),
        EE_UNITS_PH_DO_MINUTES_AND_SECONDS => minutes_and_seconds(h, r, a, b, c as c_ulong),
        EE_UNITS_PH_DO_UNIT_SWAP_KIND => return unit_swap_kind(h, a),
        EE_UNITS_PH_DO_UPDATE_CIV_TIMER => update_civ_timer(h, r),
        EE_UNITS_PH_DO_SERVE_LOAD_SAVE => serve_load_save(h, r),
        EE_UNITS_PH_DO_CHECK_SHIFT_MODE => check_shift_mode(h, r),
        EE_UNITS_PH_DO_UPDATE_WEATHER => update_weather(h, r),
        EE_UNITS_PH_DO_REPORT_STATISTICS => report_statistics(h),
        EE_UNITS_PH_DO_AUTOSAVE => auto_save(h),
        EE_UNITS_PH_DO_FLUSH_WORLD => flush_world(h),
        EE_UNITS_PH_DO_RUN_OWED_TICKS => run_owed_ticks(h, r),
        EE_UNITS_PH_DO_CLEAR_SERVER_MESSAGES => clear_server_messages(h),
        EE_UNITS_PH_DO_HANDLE_MESSAGE => handle_message(h, r, c as u64, a, b),
        EE_UNITS_PH_DO_PROCESS_UNIT_SWAPS => process_unit_swaps(h),
        EE_UNITS_PH_DO_SWAP_UNIT => swap_unit(h, a, b),
        EE_UNITS_PH_DO_AREA_EFFECTS_CLOCK => update_area_effects_clock(h, r),
        EE_UNITS_PH_DO_UPDATE_TIMING => update_timing(h, r),
        EE_UNITS_PH_DO_IDLE => idle(h),
        EE_UNITS_PH_DO_QUEUE_MESSAGE => queue_message(h, a),
        EE_UNITS_PH_DO_QUEUE_UNIT_SWAP => queue_unit_swap(h, a, b),
        EE_UNITS_PH_DO_POST => return post(h, c),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::ffi::c_void;

    extern "C" {
        fn ee_units_ph_host(op: i32, physics: *mut c_void, a: u64, b: u64, c: i64, d: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game(*mut c_void);

    impl PhHost for Game {
        fn ph(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live physics
            unsafe { ee_units_ph_host(op, self.0, a, b, c, d, f) }
        }
    }

    /// `EEPhysics`' body `what` (`EE_UNITS_PH_DO_*`) on `physics`, whose plain members are `saved`.
    ///
    /// # Safety
    /// `physics` is the live `EEPhysics` and `saved` its `EeUnitsPhysicsSaved` base.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ph_body(
        what: i32,
        physics: *mut c_void,
        saved: *mut EeUnitsPhysicsSaved,
        a: u64,
        b: u64,
        c: i64,
    ) -> i64 {
        run(&mut Game(physics), &Rec(saved), what, a, b, c)
    }
}

#[cfg(test)]
#[path = "phys_body_tests.rs"]
mod tests;
