//! The game messages' wire layouts: one entry per C++ class with its own codec (`EEMessage.cpp`, 104 classes),
//! the members in wire order after the 4-byte header, and the message types that use each. Written once from the
//! original `ExportMessage`/`ImportMessage` bodies (RUST_PLAN R5b.4); this table is now the format.

use crate::schema::{Field, Kind::*, MsgClass};

/// `EEMAddPatrolPoint`
pub const EE_NETMSG_ADD_PATROL_POINT: u32 = 0;
/// `EEMAddRemoveResources`
pub const EE_NETMSG_ADD_REMOVE_RESOURCES: u32 = 1;
/// `EEMAddUnitToSelectedGroup`
pub const EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP: u32 = 2;
/// `EEMAttackGround`
pub const EE_NETMSG_ATTACK_GROUND: u32 = 3;
/// `EEMAttackPlayers`
pub const EE_NETMSG_ATTACK_PLAYERS: u32 = 4;
/// `EEMAttackUnit`
pub const EE_NETMSG_ATTACK_UNIT: u32 = 5;
/// `EEMBuildGate`
pub const EE_NETMSG_BUILD_GATE: u32 = 6;
/// `EEMBuildUnit`
pub const EE_NETMSG_BUILD_UNIT: u32 = 7;
/// `EEMBuildWall`
pub const EE_NETMSG_BUILD_WALL: u32 = 8;
/// `EEMCameraLocationInfo`
pub const EE_NETMSG_CAMERA_LOCATION_INFO: u32 = 9;
/// `EEMCastCalamity`
pub const EE_NETMSG_CAST_CALAMITY: u32 = 10;
/// `EEMChangeFormation`
pub const EE_NETMSG_CHANGE_FORMATION: u32 = 11;
/// `EEMChangeSelectedUnitCollisionSphere`
pub const EE_NETMSG_CHANGE_SELECTED_UNIT_COLLISION_SPHERE: u32 = 12;
/// `EEMChat`
pub const EE_NETMSG_CHAT: u32 = 13;
/// `EEMCheatCode`
pub const EE_NETMSG_CHEAT_CODE: u32 = 14;
/// `EEMCivTimerExpired`
pub const EE_NETMSG_CIV_TIMER_EXPIRED: u32 = 15;
/// `EEMCivUpgrades`
pub const EE_NETMSG_CIV_UPGRADES: u32 = 16;
/// `EEMClearAirportCommands`
pub const EE_NETMSG_CLEAR_AIRPORT_COMMANDS: u32 = 17;
/// `EEMDisband`
pub const EE_NETMSG_DISBAND: u32 = 18;
/// `EEMDropoffResource`
pub const EE_NETMSG_DROPOFF_RESOURCE: u32 = 19;
/// `EEMEdictPreProposal`
pub const EE_NETMSG_EDICT_PRE_PROPOSAL: u32 = 20;
/// `EEMEdictPreProposalResponse`
pub const EE_NETMSG_EDICT_PRE_PROPOSAL_RESPONSE: u32 = 21;
/// `EEMEdictProposal`
pub const EE_NETMSG_EDICT_PROPOSAL: u32 = 22;
/// `EEMEdictVote`
pub const EE_NETMSG_EDICT_VOTE: u32 = 23;
/// `EEMEndCinematic`
pub const EE_NETMSG_END_CINEMATIC: u32 = 24;
/// `EEMEnterShiftMode`
pub const EE_NETMSG_ENTER_SHIFT_MODE: u32 = 25;
/// `EEMExchangeResources`
pub const EE_NETMSG_EXCHANGE_RESOURCES: u32 = 26;
/// `EEMExitShiftMode`
pub const EE_NETMSG_EXIT_SHIFT_MODE: u32 = 27;
/// `EEMExplore`
pub const EE_NETMSG_EXPLORE: u32 = 28;
/// `EEMFullGameOptions`
pub const EE_NETMSG_FULL_GAME_OPTIONS: u32 = 29;
/// `EEMFullPlayerInformation`
pub const EE_NETMSG_FULL_PLAYER_INFORMATION: u32 = 30;
/// `EEMGameOptions`
pub const EE_NETMSG_GAME_OPTIONS: u32 = 31;
/// `EEMGameRestarting`
pub const EE_NETMSG_GAME_RESTARTING: u32 = 32;
/// `EEMGameSpeed`
pub const EE_NETMSG_GAME_SPEED: u32 = 33;
/// `EEMGatherResource`
pub const EE_NETMSG_GATHER_RESOURCE: u32 = 34;
/// `EEMGoFanatic`
pub const EE_NETMSG_GO_FANATIC: u32 = 35;
/// `EEMHackANewUnit`
pub const EE_NETMSG_HACK_A_NEW_UNIT: u32 = 36;
/// `EEMHaltSelectedGroup`
pub const EE_NETMSG_HALT_SELECTED_GROUP: u32 = 37;
/// `EEMHealUnit`
pub const EE_NETMSG_HEAL_UNIT: u32 = 38;
/// `EEMImmediateCargoUnload`
pub const EE_NETMSG_IMMEDIATE_CARGO_UNLOAD: u32 = 39;
/// `EEMImproveUnit`
pub const EE_NETMSG_IMPROVE_UNIT: u32 = 40;
/// `EEMInGameChat`
pub const EE_NETMSG_IN_GAME_CHAT: u32 = 41;
/// `EEMIncrementEpic`
pub const EE_NETMSG_INCREMENT_EPIC: u32 = 42;
/// `EEMIncrementRallyTimer`
pub const EE_NETMSG_INCREMENT_RALLY_TIMER: u32 = 43;
/// `EEMJITManufacturing`
pub const EE_NETMSG_J_I_T_MANUFACTURING: u32 = 44;
/// `EEMKillUnit`
pub const EE_NETMSG_KILL_UNIT: u32 = 45;
/// `EEMLagStatus`
pub const EE_NETMSG_LAG_STATUS: u32 = 46;
/// `EEMLoadGarrison`
pub const EE_NETMSG_LOAD_GARRISON: u32 = 47;
/// `EEMLoadMainScreen`
pub const EE_NETMSG_LOAD_MAIN_SCREEN: u32 = 48;
/// `EEMLoadMainScreenResponse`
pub const EE_NETMSG_LOAD_MAIN_SCREEN_RESPONSE: u32 = 49;
/// `EEMLoadOrSaveGame`
pub const EE_NETMSG_LOAD_OR_SAVE_GAME: u32 = 50;
/// `EEMLoadOrSaveGameResponse`
pub const EE_NETMSG_LOAD_OR_SAVE_GAME_RESPONSE: u32 = 51;
/// `EEMLoadPercent`
pub const EE_NETMSG_LOAD_PERCENT: u32 = 52;
/// `EEMLoadTransport`
pub const EE_NETMSG_LOAD_TRANSPORT: u32 = 53;
/// `EEMLoadUnitData`
pub const EE_NETMSG_LOAD_UNIT_DATA: u32 = 54;
/// `EEMLockUnlock`
pub const EE_NETMSG_LOCK_UNLOCK: u32 = 55;
/// `EEMMakeWorldVisible`
pub const EE_NETMSG_MAKE_WORLD_VISIBLE: u32 = 56;
/// `EEMMigrationSlotInfo`
pub const EE_NETMSG_MIGRATION_SLOT_INFO: u32 = 57;
/// `EEMNewEdictEnacted`
pub const EE_NETMSG_NEW_EDICT_ENACTED: u32 = 58;
/// `EEMPackUnpack`
pub const EE_NETMSG_PACK_UNPACK: u32 = 59;
/// `EEMPathTo`
pub const EE_NETMSG_PATH_TO: u32 = 60;
/// `EEMPatrol`
pub const EE_NETMSG_PATROL: u32 = 61;
/// `EEMPayTribute`
pub const EE_NETMSG_PAY_TRIBUTE: u32 = 62;
/// `EEMPlayerInformation`
pub const EE_NETMSG_PLAYER_INFORMATION: u32 = 63;
/// `EEMPlayerSlotInfo`
pub const EE_NETMSG_PLAYER_SLOT_INFO: u32 = 64;
/// `EEMRecordFrames`
pub const EE_NETMSG_RECORD_FRAMES: u32 = 65;
/// `EEMRemoveImprovement`
pub const EE_NETMSG_REMOVE_IMPROVEMENT: u32 = 66;
/// `EEMRemoveTechnology`
pub const EE_NETMSG_REMOVE_TECHNOLOGY: u32 = 67;
/// `EEMRemoveUnitFromSelectedGroup`
pub const EE_NETMSG_REMOVE_UNIT_FROM_SELECTED_GROUP: u32 = 68;
/// `EEMRepairUnit`
pub const EE_NETMSG_REPAIR_UNIT: u32 = 69;
/// `EEMReplantFarms`
pub const EE_NETMSG_REPLANT_FARMS: u32 = 70;
/// `EEMReportStatistics`
pub const EE_NETMSG_REPORT_STATISTICS: u32 = 71;
/// `EEMResearchTechnology`
pub const EE_NETMSG_RESEARCH_TECHNOLOGY: u32 = 72;
/// `EEMResign`
pub const EE_NETMSG_RESIGN: u32 = 73;
/// `EEMReturnToBase`
pub const EE_NETMSG_RETURN_TO_BASE: u32 = 74;
/// `EEMSavedGroupAddSelection`
pub const EE_NETMSG_SAVED_GROUP_ADD_SELECTION: u32 = 75;
/// `EEMSavedGroupClear`
pub const EE_NETMSG_SAVED_GROUP_CLEAR: u32 = 76;
/// `EEMSavedGroupCreate`
pub const EE_NETMSG_SAVED_GROUP_CREATE: u32 = 77;
/// `EEMSavedGroupSelect`
pub const EE_NETMSG_SAVED_GROUP_SELECT: u32 = 78;
/// `EEMScrambleAircraft`
pub const EE_NETMSG_SCRAMBLE_AIRCRAFT: u32 = 79;
/// `EEMSelectCustomPlayerForChat`
pub const EE_NETMSG_SELECT_CUSTOM_PLAYER_FOR_CHAT: u32 = 80;
/// `EEMSelectPatrolGroup`
pub const EE_NETMSG_SELECT_PATROL_GROUP: u32 = 81;
/// `EEMSelectPlayerChatType`
pub const EE_NETMSG_SELECT_PLAYER_CHAT_TYPE: u32 = 82;
/// `EEMSelectTheseUnits`
pub const EE_NETMSG_SELECT_THESE_UNITS: u32 = 83;
/// `EEMSelectUnit`
pub const EE_NETMSG_SELECT_UNIT: u32 = 84;
/// `EEMSelectUnits`
pub const EE_NETMSG_SELECT_UNITS: u32 = 85;
/// `EEMSelfDestruct`
pub const EE_NETMSG_SELF_DESTRUCT: u32 = 86;
/// `EEMSendFlare`
pub const EE_NETMSG_SEND_FLARE: u32 = 87;
/// `EEMSetAirportCommand`
pub const EE_NETMSG_SET_AIRPORT_COMMAND: u32 = 88;
/// `EEMSetDiplomaticStance`
pub const EE_NETMSG_SET_DIPLOMATIC_STANCE: u32 = 89;
/// `EEMSetRallyGoal`
pub const EE_NETMSG_SET_RALLY_GOAL: u32 = 90;
/// `EEMSetUnitBehavior`
pub const EE_NETMSG_SET_UNIT_BEHAVIOR: u32 = 91;
/// `EEMSetWaterLevel`
pub const EE_NETMSG_SET_WATER_LEVEL: u32 = 92;
/// `EEMSmartFarmers`
pub const EE_NETMSG_SMART_FARMERS: u32 = 93;
/// `EEMSnapToDaylight`
pub const EE_NETMSG_SNAP_TO_DAYLIGHT: u32 = 94;
/// `EEMStartLoading`
pub const EE_NETMSG_START_LOADING: u32 = 95;
/// `EEMStartLoadingResponse`
pub const EE_NETMSG_START_LOADING_RESPONSE: u32 = 96;
/// `EEMStopActiveGroup`
pub const EE_NETMSG_STOP_ACTIVE_GROUP: u32 = 97;
/// `EEMToggleCollisionDetection`
pub const EE_NETMSG_TOGGLE_COLLISION_DETECTION: u32 = 98;
/// `EEMToggleDayNight`
pub const EE_NETMSG_TOGGLE_DAY_NIGHT: u32 = 99;
/// `EEMUnloadTransport`
pub const EE_NETMSG_UNLOAD_TRANSPORT: u32 = 100;
/// `EEMUnselectAllUnits`
pub const EE_NETMSG_UNSELECT_ALL_UNITS: u32 = 101;
/// `EEMUpdatePingStat`
pub const EE_NETMSG_UPDATE_PING_STAT: u32 = 102;
/// `EEMZoomInfo`
pub const EE_NETMSG_ZOOM_INFO: u32 = 103;

pub const CLASS_COUNT: usize = 104;

pub static CLASSES: [MsgClass; CLASS_COUNT] = [
    MsgClass {
        name: "EEMAddPatrolPoint",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPoint", Point2)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMAddRemoveResources",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mAmount", U32),
            Field::new("mPlayerID", I32),
            Field::new("mResourceType", I32),
            Field::new("mAdd", U8),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMAddUnitToSelectedGroup",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", U32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMAttackGround",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mX", F32), Field::new("mY", F32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMAttackPlayers",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPlayerID", I32), Field::new("mAllies", U8)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMAttackUnit",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mTargetID", I32),
            Field::new("mIsQueued", Bool),
            Field::new("mIsConvert", Bool),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMBuildGate",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPoint", Point2),
            Field::new("mTechnology", I32),
            Field::new("mSide", I32),
            Field::new("mPlayerID", I32),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMBuildUnit",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPoint", Point2),
            Field::new("mTechnology", I32),
            Field::new("mIsQueued", U8),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMBuildWall",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mStart", Point2),
            Field::new("mEnd", Point2),
            Field::new("mTechnology", I32),
            Field::new("mPlayerID", I32),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMCameraLocationInfo",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mLocation", Vec3)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMCastCalamity",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mCasterID", I32),
            Field::new("mTargetID", I32),
            Field::new("mTileX", I32),
            Field::new("mTileY", I32),
            Field::new("mTechnology", I32),
        ],
        import_fields: 6,
    },
    MsgClass {
        name: "EEMChangeFormation",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mFormationType", I32), Field::new("mB14", Bool)],
        import_fields: 2,
    }, // the original's import stops after 2 members
    MsgClass {
        name: "EEMChangeSelectedUnitCollisionSphere",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGrow", Bool)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMChat",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPlayerID", U8), Field::new("mText", WStr)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMCheatCode",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mCode", I32), Field::new("mParameter", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMCivTimerExpired",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mIsWarning", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMCivUpgrades",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPlayerID", U8),
            Field::new("mNameString", I32),
            Field::new("mUpgrades", Records8(4)),
            Field::new("mCivName", WStr),
        ],
        import_fields: 5,
    },
    MsgClass { name: "EEMClearAirportCommands", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass { name: "EEMDisband", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMDropoffResource",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mDropoffID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMEdictPreProposal",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mEdict", U16)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMEdictPreProposalResponse",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mEdict", U16), Field::new("mW12", U16)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMEdictProposal",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mEdict", U16)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMEdictVote",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mEdict", U16),
            Field::new("mIsYes", Bool),
            Field::new("mCount", U8),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMEndCinematic",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mF10", F32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMEnterShiftMode",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGameTime", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMExchangeResources",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mIsSell", U8),
            Field::new("mResourceType", I32),
            Field::new("mFiveTimes", U8),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMExitShiftMode",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGameTime", I32)],
        import_fields: 2,
    },
    MsgClass { name: "EEMExplore", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMFullGameOptions",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mAllowCustomCivs", Bool),
            Field::new("mB53", Bool),
            Field::new("mCheatCodes", Bool),
            Field::new("mDifficulty", I32),
            Field::new("mEpochEnd", I32),
            Field::new("mGameSpeed", I32),
            Field::new("mLoadType", I32),
            Field::new("mVariant", I32),
            Field::new("mLockSpeed", Bool),
            Field::new("mLockTeams", Bool),
            Field::new("mMapSize", I32),
            Field::new("mRecordGames", Bool),
            Field::new("mResources", I32),
            Field::new("mRevealMap", Bool),
            Field::new("mEpochStart", I32),
            Field::new("mUnitLimit", I32),
            Field::new("mVictory", Bool),
            Field::new("mWondersForVictory", I32),
            Field::new("mMapType", WStr),
        ],
        import_fields: 20,
    },
    MsgClass {
        name: "EEMFullPlayerInformation",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mSlot", I32),
            Field::new("mSlotType", I32),
            Field::new("mLoadedType", I32),
            Field::new("mName", WStr),
            Field::new("mPing", F32),
            Field::new("mColor", I32),
            Field::new("mTeam", I32),
            Field::new("mCitizenBonus", I32),
            Field::new("mIsReady", Bool),
            Field::new("mIsLocal", Bool),
            Field::new("mB42", Bool),
            Field::new("mEngine", I32),
        ],
        import_fields: 13,
    },
    MsgClass {
        name: "EEMGameOptions",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mOption", I32),
            Field::new("mValue", U32),
            Field::new("mMapType", WStr),
        ],
        import_fields: 4,
    },
    MsgClass { name: "EEMGameRestarting", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMGameSpeed",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mSpeed", F32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMGatherResource",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTargetID", I32), Field::new("mIsQueued", Bool)],
        import_fields: 3,
    },
    MsgClass { name: "EEMGoFanatic", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMHackANewUnit",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mUnitType", I32),
            Field::new("mPoint", Point2),
            Field::new("mPlayerID", U8),
        ],
        import_fields: 4,
    },
    MsgClass { name: "EEMHaltSelectedGroup", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMHealUnit",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTargetID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMImmediateCargoUnload",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mTransportID", I32),
            Field::new("mCargoID", I32),
            Field::new("mB18", Bool),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMImproveUnit",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", I32), Field::new("mImprovement", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMInGameChat",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mText", WStr)],
        import_fields: 2,
    },
    MsgClass { name: "EEMIncrementEpic", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMIncrementRallyTimer",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGoalQueue", U8)],
        import_fields: 2,
    },
    MsgClass { name: "EEMJITManufacturing", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMKillUnit",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mAllUnits", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMLagStatus",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mStatus", U16), Field::new("mLagWarning", U8)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMLoadGarrison",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mGarrisonID", I32),
            Field::new("mUnitID", I32),
            Field::new("mB18", U8),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMLoadMainScreen",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mB10", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMLoadMainScreenResponse",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mSlot", U8), Field::new("mPlayerID", U8)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMLoadOrSaveGame",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mIsLoad", U8),
            Field::new("mIsQuiet", Bool),
            Field::new("mTick", I32),
            Field::new("mTime", I32),
            Field::new("mIsAutoSave", U8),
            Field::new("mWaitMessageID", U32),
            Field::new("mFileName", WStr),
        ],
        import_fields: 8,
    },
    MsgClass {
        name: "EEMLoadOrSaveGameResponse",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTick", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMLoadPercent",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mSlot", I32), Field::new("mPercent", U32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMLoadTransport",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mTransportID", I32),
            Field::new("mPassengerID", I32),
            Field::new("mB18", U8),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMLoadUnitData",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitType", I32), Field::new("mB14", U8)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMLockUnlock",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mIsLocked", Bool)],
        import_fields: 2,
    },
    MsgClass { name: "EEMMakeWorldVisible", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMMigrationSlotInfo",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPlayerID", U8), Field::new("mSlot", U8)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMNewEdictEnacted",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mEdict", U16),
            Field::new("mYesVotes", U16),
            Field::new("mNoVotes", U16),
            Field::new("mIsEnacted", U8),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMPackUnpack",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPack", Bool)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMPathTo",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mFacing", F32),
            Field::new("mX", F32),
            Field::new("mY", F32),
            Field::new("mIsForcedMove", Bool),
            Field::new("mIsRotating", Bool),
            Field::new("mIsQueued", Bool),
            Field::new("mUseGroupMovement", Bool),
        ],
        import_fields: 8,
    },
    MsgClass {
        name: "EEMPatrol",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTargetID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMPayTribute",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mFood", U32),
            Field::new("mGold", U32),
            Field::new("mIron", U32),
            Field::new("mStone", U32),
            Field::new("mWood", U32),
            Field::new("mReceiver", U8),
        ],
        import_fields: 7,
    },
    MsgClass {
        name: "EEMPlayerInformation",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mValue", U32),
            Field::new("mField", I32),
            Field::new("mPing", F32),
            Field::new("mSlot", I32),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMPlayerSlotInfo",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPlayerID", U8),
            Field::new("mSlot", U8),
            Field::new("mIsBehindFirewall", Bool),
            Field::new("mAddress", WStr),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMRecordFrames",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mSlowPlayback", Bool)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMRemoveImprovement",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", I32), Field::new("mImprovement", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMRemoveTechnology",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mUnitID", I32),
            Field::new("mTechnology", I32),
            Field::new("mToggleJITManufacturing", Bool),
            Field::new("mFiveTimes", Bool),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMRemoveUnitFromSelectedGroup",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMRepairUnit",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTargetID", I32), Field::new("mIsQueued", Bool)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMReplantFarms",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mFarmID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMReportStatistics",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mL10", I32), Field::new("mL14", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMResearchTechnology",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mBuildingID", I32),
            Field::new("mTechnology", I32),
            Field::new("mFiveTimes", Bool),
        ],
        import_fields: 4,
    },
    MsgClass { name: "EEMResign", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMReturnToBase",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mBaseID", I32), Field::new("mKeepFlightTime", Bool)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMSavedGroupAddSelection",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGroup", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSavedGroupClear",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGroup", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSavedGroupCreate",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGroup", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSavedGroupSelect",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mGroup", U8), Field::new("mFlag", Bool)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMScrambleAircraft",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mX", I32),
            Field::new("mY", I32),
            Field::new("mUnitID", I32),
        ],
        import_fields: 4,
    },
    MsgClass {
        name: "EEMSelectCustomPlayerForChat",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPlayerID", I32), Field::new("mIsSelected", Bool)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMSelectPatrolGroup",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSelectPlayerChatType",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mChatType", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSelectTheseUnits",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitIDs", Records32(4))],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSelectUnit",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mUnitID", U32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSelectUnits",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mBufferSize", U8), Field::new("mBuffer", Blob)],
        import_fields: 3,
    },
    MsgClass { name: "EEMSelfDestruct", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMSendFlare",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mX", I32), Field::new("mY", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMSetAirportCommand",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPoint", Point2),
            Field::new("mCommand", I32),
            Field::new("mTargetID", I32),
            Field::new("mIsQueued", U8),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMSetDiplomaticStance",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mPlayerID", I32), Field::new("mStance", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMSetRallyGoal",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mPoint", Point2),
            Field::new("mTargetID", I32),
            Field::new("mBuildingID", I32),
            Field::new("mIsQueued", U8),
        ],
        import_fields: 5,
    },
    MsgClass {
        name: "EEMSetUnitBehavior",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mBehavior", I32)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSetWaterLevel",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mB10", U8)],
        import_fields: 2,
    },
    MsgClass {
        name: "EEMSmartFarmers",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mTargetID", I32), Field::new("mIsQueued", U8)],
        import_fields: 3,
    },
    MsgClass { name: "EEMSnapToDaylight", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMStartLoading",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mNumHumanPlayers", U8), Field::new("mSeed", I32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMStartLoadingResponse",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mSlot", U8),
            Field::new("mPlayerID", U8),
            Field::new("mLobbySlot", U8),
        ],
        import_fields: 4,
    },
    MsgClass { name: "EEMStopActiveGroup", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMToggleCollisionDetection",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mDivisor", I32), Field::new("mL14", I32)],
        import_fields: 3,
    },
    MsgClass { name: "EEMToggleDayNight", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMUnloadTransport",
        fields: &[
            Field::new("mFastSelectionID", U8),
            Field::new("mUnitID", I32),
            Field::new("mX", I32),
            Field::new("mY", I32),
        ],
        import_fields: 4,
    },
    MsgClass { name: "EEMUnselectAllUnits", fields: &[Field::new("mFastSelectionID", U8)], import_fields: 1 },
    MsgClass {
        name: "EEMUpdatePingStat",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mSlot", U8), Field::new("mPing", F32)],
        import_fields: 3,
    },
    MsgClass {
        name: "EEMZoomInfo",
        fields: &[Field::new("mFastSelectionID", U8), Field::new("mZoom", F32)],
        import_fields: 2,
    },
];

/// (message type, class) for every type the game's factory builds (`EEMessage::BuildEEMessage`)
pub static TYPES: [(u8, u32); 107] = [
    (0x41, EE_NETMSG_EDICT_PROPOSAL),
    (0x42, EE_NETMSG_EDICT_VOTE),
    (0x43, EE_NETMSG_NEW_EDICT_ENACTED),
    (0x44, EE_NETMSG_PAY_TRIBUTE),
    (0x46, EE_NETMSG_EDICT_PRE_PROPOSAL),
    (0x47, EE_NETMSG_EDICT_PRE_PROPOSAL_RESPONSE),
    (0x48, EE_NETMSG_SELECT_UNIT),
    (0x49, EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP),
    (0x4a, EE_NETMSG_REMOVE_UNIT_FROM_SELECTED_GROUP),
    (0x4b, EE_NETMSG_UNSELECT_ALL_UNITS),
    (0x4c, EE_NETMSG_ATTACK_UNIT),
    (0x4d, EE_NETMSG_PATH_TO),
    (0x4e, EE_NETMSG_SELECT_UNITS),
    (0x4f, EE_NETMSG_SELECT_UNITS), // EEMAddUnitsToSelectedGroup
    (0x50, EE_NETMSG_SELECT_UNITS), // EEMAddAndRemoveUnitsFromSelectedGroup
    (0x51, EE_NETMSG_GATHER_RESOURCE),
    (0x52, EE_NETMSG_RESEARCH_TECHNOLOGY),
    (0x53, EE_NETMSG_REMOVE_TECHNOLOGY),
    (0x54, EE_NETMSG_SET_WATER_LEVEL),
    (0x55, EE_NETMSG_MAKE_WORLD_VISIBLE),
    (0x56, EE_NETMSG_BUILD_UNIT),
    (0x57, EE_NETMSG_REPAIR_UNIT),
    (0x58, EE_NETMSG_HALT_SELECTED_GROUP),
    (0x59, EE_NETMSG_BUILD_WALL),
    (0x5a, EE_NETMSG_CHAT),
    (0x5b, EE_NETMSG_FULL_PLAYER_INFORMATION),
    (0x5c, EE_NETMSG_KILL_UNIT),
    (0x5d, EE_NETMSG_START_LOADING),
    (0x5e, EE_NETMSG_START_LOADING_RESPONSE),
    (0x5f, EE_NETMSG_GAME_SPEED),
    (0x60, EE_NETMSG_GAME_OPTIONS),
    (0x61, EE_NETMSG_DROPOFF_RESOURCE),
    (0x62, EE_NETMSG_TOGGLE_DAY_NIGHT),
    (0x63, EE_NETMSG_SNAP_TO_DAYLIGHT),
    (0x64, EE_NETMSG_LOAD_OR_SAVE_GAME),
    (0x65, EE_NETMSG_TOGGLE_COLLISION_DETECTION),
    (0x66, EE_NETMSG_SET_RALLY_GOAL),
    (0x67, EE_NETMSG_LOAD_TRANSPORT),
    (0x68, EE_NETMSG_UNLOAD_TRANSPORT),
    (0x6a, EE_NETMSG_PLAYER_SLOT_INFO),
    (0x6b, EE_NETMSG_HACK_A_NEW_UNIT),
    (0x6c, EE_NETMSG_INCREMENT_EPIC),
    (0x6d, EE_NETMSG_RECORD_FRAMES),
    (0x6e, EE_NETMSG_CAMERA_LOCATION_INFO),
    (0x6f, EE_NETMSG_ZOOM_INFO),
    (0x70, EE_NETMSG_ATTACK_PLAYERS),
    (0x71, EE_NETMSG_LOAD_UNIT_DATA),
    (0x72, EE_NETMSG_ADD_PATROL_POINT),
    (0x73, EE_NETMSG_PATROL),
    (0x74, EE_NETMSG_SELECT_PATROL_GROUP),
    (0x75, EE_NETMSG_CAST_CALAMITY),
    (0x76, EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP), // EEMAlwaysAddUnitToSelectedGroup
    (0x77, EE_NETMSG_IMPROVE_UNIT),
    (0x78, EE_NETMSG_CHANGE_FORMATION),
    (0x79, EE_NETMSG_EXPLORE),
    (0x7a, EE_NETMSG_UPDATE_PING_STAT),
    (0x7b, EE_NETMSG_LOAD_GARRISON),
    (0x7c, EE_NETMSG_MIGRATION_SLOT_INFO),
    (0x7d, EE_NETMSG_IN_GAME_CHAT),
    (0x7e, EE_NETMSG_CIV_UPGRADES), // EEMCivUpgradesRequested
    (0x7f, EE_NETMSG_CIV_UPGRADES), // EEMCivUpgradesPurchased
    (0x80, EE_NETMSG_SET_AIRPORT_COMMAND),
    (0x81, EE_NETMSG_REPLANT_FARMS),
    (0x82, EE_NETMSG_SAVED_GROUP_ADD_SELECTION),
    (0x83, EE_NETMSG_SAVED_GROUP_CREATE),
    (0x84, EE_NETMSG_SAVED_GROUP_SELECT),
    (0x85, EE_NETMSG_CHEAT_CODE),
    (0x86, EE_NETMSG_BUILD_GATE),
    (0x87, EE_NETMSG_CLEAR_AIRPORT_COMMANDS),
    (0x88, EE_NETMSG_ENTER_SHIFT_MODE),
    (0x89, EE_NETMSG_EXIT_SHIFT_MODE),
    (0x8a, EE_NETMSG_STOP_ACTIVE_GROUP),
    (0x8b, EE_NETMSG_REMOVE_IMPROVEMENT),
    (0x8c, EE_NETMSG_LOCK_UNLOCK),
    (0x8d, EE_NETMSG_CHANGE_SELECTED_UNIT_COLLISION_SPHERE),
    (0x8e, EE_NETMSG_INCREMENT_RALLY_TIMER),
    (0x8f, EE_NETMSG_SMART_FARMERS),
    (0x90, EE_NETMSG_RETURN_TO_BASE),
    (0x91, EE_NETMSG_GAME_RESTARTING),
    (0x92, EE_NETMSG_HEAL_UNIT),
    (0x93, EE_NETMSG_SAVED_GROUP_CLEAR),
    (0x94, EE_NETMSG_LOAD_PERCENT),
    (0x95, EE_NETMSG_IMMEDIATE_CARGO_UNLOAD),
    (0x96, EE_NETMSG_SELECT_THESE_UNITS),
    (0x97, EE_NETMSG_LOAD_MAIN_SCREEN),
    (0x98, EE_NETMSG_LOAD_MAIN_SCREEN_RESPONSE),
    (0x99, EE_NETMSG_SET_DIPLOMATIC_STANCE),
    (0x9a, EE_NETMSG_LOAD_OR_SAVE_GAME_RESPONSE),
    (0x9b, EE_NETMSG_ADD_REMOVE_RESOURCES),
    (0x9c, EE_NETMSG_SEND_FLARE),
    (0x9e, EE_NETMSG_SET_UNIT_BEHAVIOR),
    (0x9f, EE_NETMSG_ATTACK_GROUND),
    (0xa0, EE_NETMSG_SCRAMBLE_AIRCRAFT),
    (0xa1, EE_NETMSG_SELECT_PLAYER_CHAT_TYPE),
    (0xa2, EE_NETMSG_SELECT_CUSTOM_PLAYER_FOR_CHAT),
    (0xa3, EE_NETMSG_RESIGN),
    (0xa4, EE_NETMSG_DISBAND),
    (0xa5, EE_NETMSG_SELF_DESTRUCT),
    (0xa6, EE_NETMSG_REPORT_STATISTICS),
    (0xa7, EE_NETMSG_CIV_TIMER_EXPIRED),
    (0xa8, EE_NETMSG_LAG_STATUS),
    (0xa9, EE_NETMSG_FULL_GAME_OPTIONS),
    (0xaa, EE_NETMSG_PLAYER_INFORMATION),
    (0xab, EE_NETMSG_PACK_UNPACK),
    (0xac, EE_NETMSG_GO_FANATIC),
    (0xad, EE_NETMSG_J_I_T_MANUFACTURING),
    (0xae, EE_NETMSG_EXCHANGE_RESOURCES),
];
