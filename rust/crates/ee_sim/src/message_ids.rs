//! The game network message type ids (ee/EEMessageID.h: `NEMessage::mType`, the EEMessage constructors), as a
//! plain (unscoped) C enum for the C++ headers; each comment gives the evidence for its name, "(provisional name)"
//! marks an inferred one.
#![allow(non_camel_case_types, clippy::enum_variant_names)]

/// Game network message type ids (NEMessage::mType, EEMessage ctor): EEMessage::BuildEEMessage case -> new EEM<Name>; each EEM ctor passes its id.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EEMessageID {
    /// RTTI .?AVEEMEdictProposal@@; EEMEdictProposal ctor passes 0x41 / EEMessage::BuildEEMessage case 0x41 -> new EEMEdictProposal
    kMessageEdictProposal = 0x41,
    /// RTTI .?AVEEMEdictVote@@; EEMEdictVote ctor passes 0x42 / EEMessage::BuildEEMessage case 0x42 -> new EEMEdictVote
    kMessageEdictVote = 0x42,
    /// RTTI .?AVEEMNewEdictEnacted@@; EEMNewEdictEnacted ctor passes 0x43 / EEMessage::BuildEEMessage case 0x43 -> new EEMNewEdictEnacted
    kMessageNewEdictEnacted = 0x43,
    /// RTTI .?AVEEMPayTribute@@; EEMPayTribute ctor passes 0x44 / EEMessage::BuildEEMessage case 0x44 -> new EEMPayTribute
    kMessagePayTribute = 0x44,
    /// RTTI .?AVEEMEdictPreProposal@@; EEMEdictPreProposal ctor passes 0x46 / EEMessage::BuildEEMessage case 0x46 -> new EEMEdictPreProposal
    kMessageEdictPreProposal = 0x46,
    /// RTTI .?AVEEMEdictPreProposalResponse@@; EEMEdictPreProposalResponse ctor passes 0x47 / EEMessage::BuildEEMessage case 0x47 -> new EEMEdictPreProposalResponse
    kMessageEdictPreProposalResponse = 0x47,
    /// RTTI .?AVEEMSelectUnit@@; EEMSelectUnit ctor passes 0x48 / EEMessage::BuildEEMessage case 0x48 -> new EEMSelectUnit
    kMessageSelectUnit = 0x48,
    /// RTTI .?AVEEMAddUnitToSelectedGroup@@; EEMAddUnitToSelectedGroup ctor passes 0x49 / EEMessage::BuildEEMessage case 0x49 -> new EEMAddUnitToSelectedGroup
    kMessageAddUnitToSelectedGroup = 0x49,
    /// RTTI .?AVEEMRemoveUnitFromSelectedGroup@@; EEMRemoveUnitFromSelectedGroup ctor passes 0x4a / EEMessage::BuildEEMessage case 0x4a -> new EEMRemoveUnitFromSelectedGroup
    kMessageRemoveUnitFromSelectedGroup = 0x4a,
    /// RTTI .?AVEEMUnselectAllUnits@@; EEMUnselectAllUnits ctor passes 0x4b / EEMessage::BuildEEMessage case 0x4b -> new EEMUnselectAllUnits
    kMessageUnselectAllUnits = 0x4b,
    /// RTTI .?AVEEMAttackUnit@@; EEMAttackUnit ctor passes 0x4c / EEMessage::BuildEEMessage case 0x4c -> new EEMAttackUnit
    kMessageAttackUnit = 0x4c,
    /// RTTI .?AVEEMPathTo@@; EEMPathTo ctor passes 0x4d / EEMessage::BuildEEMessage case 0x4d -> new EEMPathTo
    kMessagePathTo = 0x4d,
    /// RTTI .?AVEEMSelectUnits@@; EEMSelectUnits ctor passes 0x4e / EEMessage::BuildEEMessage case 0x4e -> new EEMSelectUnits
    kMessageSelectUnits = 0x4e,
    /// RTTI .?AVEEMAddUnitsToSelectedGroup@@; EEMAddUnitsToSelectedGroup ctor passes 0x4f / EEMessage::BuildEEMessage case 0x4f -> new EEMAddUnitsToSelectedGroup
    kMessageAddUnitsToSelectedGroup = 0x4f,
    /// RTTI .?AVEEMAddAndRemoveUnitsFromSelectedGroup@@; EEMAddAndRemoveUnitsFromSelectedGroup ctor passes 0x50 / EEMessage::BuildEEMessage case 0x50 -> new EEMAddAndRemoveUnitsFromSelectedGroup
    kMessageAddAndRemoveUnitsFromSelectedGroup = 0x50,
    /// RTTI .?AVEEMGatherResource@@; EEMGatherResource ctor passes 0x51 / EEMessage::BuildEEMessage case 0x51 -> new EEMGatherResource
    kMessageGatherResource = 0x51,
    /// RTTI .?AVEEMResearchTechnology@@; EEMResearchTechnology ctor passes 0x52 / EEMessage::BuildEEMessage case 0x52 -> new EEMResearchTechnology
    kMessageResearchTechnology = 0x52,
    /// RTTI .?AVEEMRemoveTechnology@@; EEMRemoveTechnology ctor passes 0x53 / EEMessage::BuildEEMessage case 0x53 -> new EEMRemoveTechnology
    kMessageRemoveTechnology = 0x53,
    /// RTTI .?AVEEMSetWaterLevel@@; EEMSetWaterLevel ctor passes 0x54 / EEMessage::BuildEEMessage case 0x54 -> new EEMSetWaterLevel
    kMessageSetWaterLevel = 0x54,
    /// RTTI .?AVEEMMakeWorldVisible@@; EEMMakeWorldVisible ctor passes 0x55 / EEMessage::BuildEEMessage case 0x55 -> new EEMMakeWorldVisible
    kMessageMakeWorldVisible = 0x55,
    /// RTTI .?AVEEMBuildUnit@@; EEMBuildUnit ctor passes 0x56 / EEMessage::BuildEEMessage case 0x56 -> new EEMBuildUnit
    kMessageBuildUnit = 0x56,
    /// RTTI .?AVEEMRepairUnit@@; EEMRepairUnit ctor passes 0x57 / EEMessage::BuildEEMessage case 0x57 -> new EEMRepairUnit
    kMessageRepairUnit = 0x57,
    /// RTTI .?AVEEMHaltSelectedGroup@@; EEMHaltSelectedGroup ctor passes 0x58 / EEMessage::BuildEEMessage case 0x58 -> new EEMHaltSelectedGroup
    kMessageHaltSelectedGroup = 0x58,
    /// RTTI .?AVEEMBuildWall@@; EEMBuildWall ctor passes 0x59 / EEMessage::BuildEEMessage case 0x59 -> new EEMBuildWall
    kMessageBuildWall = 0x59,
    /// RTTI .?AVEEMChat@@; EEMChat ctor passes 0x5a / EEMessage::BuildEEMessage case 0x5a -> new EEMChat
    kMessageChat = 0x5a,
    /// RTTI .?AVEEMFullPlayerInformation@@; EEMFullPlayerInformation ctor passes 0x5b / EEMessage::BuildEEMessage case 0x5b -> new EEMFullPlayerInformation
    kMessageFullPlayerInformation = 0x5b,
    /// RTTI .?AVEEMKillUnit@@; EEMKillUnit ctor passes 0x5c / EEMessage::BuildEEMessage case 0x5c -> new EEMKillUnit
    kMessageKillUnit = 0x5c,
    /// RTTI .?AVEEMStartLoading@@; EEMStartLoading ctor passes 0x5d / EEMessage::BuildEEMessage case 0x5d -> new EEMStartLoading
    kMessageStartLoading = 0x5d,
    /// RTTI .?AVEEMStartLoadingResponse@@; EEMStartLoadingResponse ctor passes 0x5e / EEMessage::BuildEEMessage case 0x5e -> new EEMStartLoadingResponse
    kMessageStartLoadingResponse = 0x5e,
    /// RTTI .?AVEEMGameSpeed@@; EEMGameSpeed ctor passes 0x5f / EEMessage::BuildEEMessage case 0x5f -> new EEMGameSpeed
    kMessageGameSpeed = 0x5f,
    /// RTTI .?AVEEMGameOptions@@; EEMGameOptions ctor passes 0x60 / EEMessage::BuildEEMessage case 0x60 -> new EEMGameOptions
    kMessageGameOptions = 0x60,
    /// RTTI .?AVEEMDropoffResource@@; EEMDropoffResource ctor passes 0x61 / EEMessage::BuildEEMessage case 0x61 -> new EEMDropoffResource
    kMessageDropoffResource = 0x61,
    /// RTTI .?AVEEMToggleDayNight@@; EEMToggleDayNight ctor passes 0x62 / EEMessage::BuildEEMessage case 0x62 -> new EEMToggleDayNight
    kMessageToggleDayNight = 0x62,
    /// RTTI .?AVEEMSnapToDaylight@@; EEMSnapToDaylight ctor passes 0x63 / EEMessage::BuildEEMessage case 0x63 -> new EEMSnapToDaylight
    kMessageSnapToDaylight = 0x63,
    /// RTTI .?AVEEMLoadOrSaveGame@@; EEMLoadOrSaveGame ctor passes 0x64 / EEMessage::BuildEEMessage case 0x64 -> new EEMLoadOrSaveGame
    kMessageLoadOrSaveGame = 0x64,
    /// RTTI .?AVEEMToggleCollisionDetection@@; EEMToggleCollisionDetection ctor passes 0x65 / EEMessage::BuildEEMessage case 0x65 -> new EEMToggleCollisionDetection
    kMessageToggleCollisionDetection = 0x65,
    /// RTTI .?AVEEMSetRallyGoal@@; EEMSetRallyGoal ctor passes 0x66 / EEMessage::BuildEEMessage case 0x66 -> new EEMSetRallyGoal
    kMessageSetRallyGoal = 0x66,
    /// RTTI .?AVEEMLoadTransport@@; EEMLoadTransport ctor passes 0x67 / EEMessage::BuildEEMessage case 0x67 -> new EEMLoadTransport
    kMessageLoadTransport = 0x67,
    /// RTTI .?AVEEMUnloadTransport@@; EEMUnloadTransport ctor passes 0x68 / EEMessage::BuildEEMessage case 0x68 -> new EEMUnloadTransport
    kMessageUnloadTransport = 0x68,
    /// RTTI .?AVEEMPlayerSlotInfo@@; EEMPlayerSlotInfo ctor passes 0x6a / EEMessage::BuildEEMessage case 0x6a -> new EEMPlayerSlotInfo
    kMessagePlayerSlotInfo = 0x6a,
    /// RTTI .?AVEEMHackANewUnit@@; EEMHackANewUnit ctor passes 0x6b / EEMessage::BuildEEMessage case 0x6b -> new EEMHackANewUnit
    kMessageHackANewUnit = 0x6b,
    /// RTTI .?AVEEMIncrementEpic@@; EEMIncrementEpic ctor passes 0x6c / EEMessage::BuildEEMessage case 0x6c -> new EEMIncrementEpic
    kMessageIncrementEpic = 0x6c,
    /// RTTI .?AVEEMRecordFrames@@; EEMRecordFrames ctor passes 0x6d / EEMessage::BuildEEMessage case 0x6d -> new EEMRecordFrames
    kMessageRecordFrames = 0x6d,
    /// RTTI .?AVEEMCameraLocationInfo@@; EEMCameraLocationInfo ctor passes 0x6e / EEMessage::BuildEEMessage case 0x6e -> new EEMCameraLocationInfo
    kMessageCameraLocationInfo = 0x6e,
    /// RTTI .?AVEEMZoomInfo@@; EEMZoomInfo ctor passes 0x6f / EEMessage::BuildEEMessage case 0x6f -> new EEMZoomInfo
    kMessageZoomInfo = 0x6f,
    /// RTTI .?AVEEMAttackPlayers@@; EEMAttackPlayers ctor passes 0x70 / EEMessage::BuildEEMessage case 0x70 -> new EEMAttackPlayers
    kMessageAttackPlayers = 0x70,
    /// RTTI .?AVEEMLoadUnitData@@; EEMLoadUnitData ctor passes 0x71 / EEMessage::BuildEEMessage case 0x71 -> new EEMLoadUnitData
    kMessageLoadUnitData = 0x71,
    /// RTTI .?AVEEMAddPatrolPoint@@; EEMAddPatrolPoint ctor passes 0x72 / EEMessage::BuildEEMessage case 0x72 -> new EEMAddPatrolPoint
    kMessageAddPatrolPoint = 0x72,
    /// RTTI .?AVEEMPatrol@@; EEMPatrol ctor passes 0x73 / EEMessage::BuildEEMessage case 0x73 -> new EEMPatrol
    kMessagePatrol = 0x73,
    /// RTTI .?AVEEMSelectPatrolGroup@@; EEMSelectPatrolGroup ctor passes 0x74 / EEMessage::BuildEEMessage case 0x74 -> new EEMSelectPatrolGroup
    kMessageSelectPatrolGroup = 0x74,
    /// RTTI .?AVEEMCastCalamity@@; EEMCastCalamity ctor passes 0x75 / EEMessage::BuildEEMessage case 0x75 -> new EEMCastCalamity
    kMessageCastCalamity = 0x75,
    /// RTTI .?AVEEMAlwaysAddUnitToSelectedGroup@@; EEMAlwaysAddUnitToSelectedGroup ctor passes 0x76 / EEMessage::BuildEEMessage case 0x76 -> new EEMAlwaysAddUnitToSelectedGroup
    kMessageAlwaysAddUnitToSelectedGroup = 0x76,
    /// RTTI .?AVEEMImproveUnit@@; EEMImproveUnit ctor passes 0x77 / EEMessage::BuildEEMessage case 0x77 -> new EEMImproveUnit
    kMessageImproveUnit = 0x77,
    /// RTTI .?AVEEMChangeFormation@@; EEMChangeFormation ctor passes 0x78 / EEMessage::BuildEEMessage case 0x78 -> new EEMChangeFormation
    kMessageChangeFormation = 0x78,
    /// RTTI .?AVEEMExplore@@; EEMExplore ctor passes 0x79 / EEMessage::BuildEEMessage case 0x79 -> new EEMExplore
    kMessageExplore = 0x79,
    /// RTTI .?AVEEMUpdatePingStat@@; EEMUpdatePingStat ctor passes 0x7a / EEMessage::BuildEEMessage case 0x7a -> new EEMUpdatePingStat
    kMessageUpdatePingStat = 0x7a,
    /// RTTI .?AVEEMLoadGarrison@@; EEMLoadGarrison ctor passes 0x7b / EEMessage::BuildEEMessage case 0x7b -> new EEMLoadGarrison
    kMessageLoadGarrison = 0x7b,
    /// RTTI .?AVEEMMigrationSlotInfo@@; EEMMigrationSlotInfo ctor passes 0x7c / EEMessage::BuildEEMessage case 0x7c -> new EEMMigrationSlotInfo
    kMessageMigrationSlotInfo = 0x7c,
    /// RTTI .?AVEEMInGameChat@@; EEMInGameChat ctor passes 0x7d / EEMessage::BuildEEMessage case 0x7d -> new EEMInGameChat
    kMessageInGameChat = 0x7d,
    /// RTTI .?AVEEMCivUpgradesRequested@@; EEMCivUpgradesRequested ctor passes 0x7e / EEMessage::BuildEEMessage case 0x7e -> new EEMCivUpgradesRequested
    kMessageCivUpgradesRequested = 0x7e,
    /// RTTI .?AVEEMCivUpgradesPurchased@@; EEMCivUpgradesPurchased ctor passes 0x7f / EEMessage::BuildEEMessage case 0x7f -> new EEMCivUpgradesPurchased
    kMessageCivUpgradesPurchased = 0x7f,
    /// RTTI .?AVEEMSetAirportCommand@@; EEMSetAirportCommand ctor passes 0x80 / EEMessage::BuildEEMessage case 0x80 -> new EEMSetAirportCommand
    kMessageSetAirportCommand = 0x80,
    /// RTTI .?AVEEMReplantFarms@@; EEMReplantFarms ctor passes 0x81 / EEMessage::BuildEEMessage case 0x81 -> new EEMReplantFarms
    kMessageReplantFarms = 0x81,
    /// RTTI .?AVEEMSavedGroupAddSelection@@; EEMSavedGroupAddSelection ctor passes 0x82 / EEMessage::BuildEEMessage case 0x82 -> new EEMSavedGroupAddSelection
    kMessageSavedGroupAddSelection = 0x82,
    /// RTTI .?AVEEMSavedGroupCreate@@; EEMSavedGroupCreate ctor passes 0x83 / EEMessage::BuildEEMessage case 0x83 -> new EEMSavedGroupCreate
    kMessageSavedGroupCreate = 0x83,
    /// RTTI .?AVEEMSavedGroupSelect@@; EEMSavedGroupSelect ctor passes 0x84 / EEMessage::BuildEEMessage case 0x84 -> new EEMSavedGroupSelect
    kMessageSavedGroupSelect = 0x84,
    /// RTTI .?AVEEMCheatCode@@; EEMCheatCode ctor passes 0x85 / EEMessage::BuildEEMessage case 0x85 -> new EEMCheatCode
    kMessageCheatCode = 0x85,
    /// RTTI .?AVEEMBuildGate@@; EEMBuildGate ctor passes 0x86 / EEMessage::BuildEEMessage case 0x86 -> new EEMBuildGate
    kMessageBuildGate = 0x86,
    /// RTTI .?AVEEMClearAirportCommands@@; EEMClearAirportCommands ctor passes 0x87 / EEMessage::BuildEEMessage case 0x87 -> new EEMClearAirportCommands
    kMessageClearAirportCommands = 0x87,
    /// RTTI .?AVEEMEnterShiftMode@@; EEMEnterShiftMode ctor passes 0x88 / EEMessage::BuildEEMessage case 0x88 -> new EEMEnterShiftMode
    kMessageEnterShiftMode = 0x88,
    /// RTTI .?AVEEMExitShiftMode@@; EEMExitShiftMode ctor passes 0x89 / EEMessage::BuildEEMessage case 0x89 -> new EEMExitShiftMode
    kMessageExitShiftMode = 0x89,
    /// RTTI .?AVEEMStopActiveGroup@@; EEMStopActiveGroup ctor passes 0x8a / EEMessage::BuildEEMessage case 0x8a -> new EEMStopActiveGroup
    kMessageStopActiveGroup = 0x8a,
    /// RTTI .?AVEEMRemoveImprovement@@; EEMRemoveImprovement ctor passes 0x8b / EEMessage::BuildEEMessage case 0x8b -> new EEMRemoveImprovement
    kMessageRemoveImprovement = 0x8b,
    /// RTTI .?AVEEMLockUnlock@@; EEMLockUnlock ctor passes 0x8c / EEMessage::BuildEEMessage case 0x8c -> new EEMLockUnlock
    kMessageLockUnlock = 0x8c,
    /// RTTI .?AVEEMChangeSelectedUnitCollisionSphere@@; EEMChangeSelectedUnitCollisionSphere ctor passes 0x8d / EEMessage::BuildEEMessage case 0x8d -> new EEMChangeSelectedUnitCollisionSphere
    kMessageChangeSelectedUnitCollisionSphere = 0x8d,
    /// RTTI .?AVEEMIncrementRallyTimer@@; EEMIncrementRallyTimer ctor passes 0x8e / EEMessage::BuildEEMessage case 0x8e -> new EEMIncrementRallyTimer
    kMessageIncrementRallyTimer = 0x8e,
    /// RTTI .?AVEEMSmartFarmers@@; EEMSmartFarmers ctor passes 0x8f / EEMessage::BuildEEMessage case 0x8f -> new EEMSmartFarmers
    kMessageSmartFarmers = 0x8f,
    /// RTTI .?AVEEMReturnToBase@@; EEMReturnToBase ctor passes 0x90 / EEMessage::BuildEEMessage case 0x90 -> new EEMReturnToBase
    kMessageReturnToBase = 0x90,
    /// RTTI .?AVEEMGameRestarting@@; EEMGameRestarting ctor passes 0x91 / EEMessage::BuildEEMessage case 0x91 -> new EEMGameRestarting
    kMessageGameRestarting = 0x91,
    /// RTTI .?AVEEMHealUnit@@; EEMHealUnit ctor passes 0x92 / EEMessage::BuildEEMessage case 0x92 -> new EEMHealUnit
    kMessageHealUnit = 0x92,
    /// RTTI .?AVEEMSavedGroupClear@@; EEMSavedGroupClear ctor passes 0x93 / EEMessage::BuildEEMessage case 0x93 -> new EEMSavedGroupClear
    kMessageSavedGroupClear = 0x93,
    /// RTTI .?AVEEMLoadPercent@@; EEMLoadPercent ctor passes 0x94 / EEMessage::BuildEEMessage case 0x94 -> new EEMLoadPercent
    kMessageLoadPercent = 0x94,
    /// RTTI .?AVEEMImmediateCargoUnload@@; EEMImmediateCargoUnload ctor passes 0x95 / EEMessage::BuildEEMessage case 0x95 -> new EEMImmediateCargoUnload
    kMessageImmediateCargoUnload = 0x95,
    /// RTTI .?AVEEMSelectTheseUnits@@; EEMSelectTheseUnits ctor passes 0x96 / EEMessage::BuildEEMessage case 0x96 -> new EEMSelectTheseUnits
    kMessageSelectTheseUnits = 0x96,
    /// RTTI .?AVEEMLoadMainScreen@@; EEMLoadMainScreen ctor passes 0x97 / EEMessage::BuildEEMessage case 0x97 -> new EEMLoadMainScreen
    kMessageLoadMainScreen = 0x97,
    /// RTTI .?AVEEMLoadMainScreenResponse@@; EEMLoadMainScreenResponse ctor passes 0x98 / EEMessage::BuildEEMessage case 0x98 -> new EEMLoadMainScreenResponse
    kMessageLoadMainScreenResponse = 0x98,
    /// RTTI .?AVEEMSetDiplomaticStance@@; EEMSetDiplomaticStance ctor passes 0x99 / EEMessage::BuildEEMessage case 0x99 -> new EEMSetDiplomaticStance
    kMessageSetDiplomaticStance = 0x99,
    /// RTTI .?AVEEMLoadOrSaveGameResponse@@; EEMLoadOrSaveGameResponse ctor passes 0x9a / EEMessage::BuildEEMessage case 0x9a -> new EEMLoadOrSaveGameResponse
    kMessageLoadOrSaveGameResponse = 0x9a,
    /// RTTI .?AVEEMAddRemoveResources@@; EEMAddRemoveResources ctor passes 0x9b / EEMessage::BuildEEMessage case 0x9b -> new EEMAddRemoveResources
    kMessageAddRemoveResources = 0x9b,
    /// RTTI .?AVEEMSendFlare@@; EEMSendFlare ctor passes 0x9c / EEMessage::BuildEEMessage case 0x9c -> new EEMSendFlare
    kMessageSendFlare = 0x9c,
    /// RTTI .?AVEEMEndCinematic@@; EEMEndCinematic ctor passes 0x9d (not built by BuildEEMessage)
    kMessageEndCinematic = 0x9d,
    /// RTTI .?AVEEMSetUnitBehavior@@; EEMSetUnitBehavior ctor passes 0x9e / EEMessage::BuildEEMessage case 0x9e -> new EEMSetUnitBehavior
    kMessageSetUnitBehavior = 0x9e,
    /// RTTI .?AVEEMAttackGround@@; EEMAttackGround ctor passes 0x9f / EEMessage::BuildEEMessage case 0x9f -> new EEMAttackGround
    kMessageAttackGround = 0x9f,
    /// RTTI .?AVEEMScrambleAircraft@@; EEMScrambleAircraft ctor passes 0xa0 / EEMessage::BuildEEMessage case 0xa0 -> new EEMScrambleAircraft
    kMessageScrambleAircraft = 0xa0,
    /// RTTI .?AVEEMSelectPlayerChatType@@; EEMSelectPlayerChatType ctor passes 0xa1 / EEMessage::BuildEEMessage case 0xa1 -> new EEMSelectPlayerChatType
    kMessageSelectPlayerChatType = 0xa1,
    /// RTTI .?AVEEMSelectCustomPlayerForChat@@; EEMSelectCustomPlayerForChat ctor passes 0xa2 / EEMessage::BuildEEMessage case 0xa2 -> new EEMSelectCustomPlayerForChat
    kMessageSelectCustomPlayerForChat = 0xa2,
    /// RTTI .?AVEEMResign@@; EEMResign ctor passes 0xa3 / EEMessage::BuildEEMessage case 0xa3 -> new EEMResign
    kMessageResign = 0xa3,
    /// RTTI .?AVEEMDisband@@; EEMDisband ctor passes 0xa4 / EEMessage::BuildEEMessage case 0xa4 -> new EEMDisband
    kMessageDisband = 0xa4,
    /// RTTI .?AVEEMSelfDestruct@@; EEMSelfDestruct ctor passes 0xa5 / EEMessage::BuildEEMessage case 0xa5 -> new EEMSelfDestruct
    kMessageSelfDestruct = 0xa5,
    /// RTTI .?AVEEMReportStatistics@@; EEMReportStatistics ctor passes 0xa6 / EEMessage::BuildEEMessage case 0xa6 -> new EEMReportStatistics
    kMessageReportStatistics = 0xa6,
    /// RTTI .?AVEEMCivTimerExpired@@; EEMCivTimerExpired ctor passes 0xa7 / EEMessage::BuildEEMessage case 0xa7 -> new EEMCivTimerExpired
    kMessageCivTimerExpired = 0xa7,
    /// RTTI .?AVEEMLagStatus@@; EEMLagStatus ctor passes 0xa8 / EEMessage::BuildEEMessage case 0xa8 -> new EEMLagStatus
    kMessageLagStatus = 0xa8,
    /// RTTI .?AVEEMFullGameOptions@@; EEMFullGameOptions ctor passes 0xa9 / EEMessage::BuildEEMessage case 0xa9 -> new EEMFullGameOptions
    kMessageFullGameOptions = 0xa9,
    /// RTTI .?AVEEMPlayerInformation@@; EEMPlayerInformation ctor passes 0xaa / EEMessage::BuildEEMessage case 0xaa -> new EEMPlayerInformation
    kMessagePlayerInformation = 0xaa,
    /// RTTI .?AVEEMPackUnpack@@; EEMPackUnpack ctor passes 0xab / EEMessage::BuildEEMessage case 0xab -> new EEMPackUnpack
    kMessagePackUnpack = 0xab,
    /// RTTI .?AVEEMGoFanatic@@; EEMGoFanatic ctor passes 0xac / EEMessage::BuildEEMessage case 0xac -> new EEMGoFanatic
    kMessageGoFanatic = 0xac,
    /// RTTI .?AVEEMJITManufacturing@@; EEMJITManufacturing ctor passes 0xad / EEMessage::BuildEEMessage case 0xad -> new EEMJITManufacturing
    kMessageJITManufacturing = 0xad,
    /// RTTI .?AVEEMExchangeResources@@; EEMExchangeResources ctor passes 0xae / EEMessage::BuildEEMessage case 0xae -> new EEMExchangeResources
    kMessageExchangeResources = 0xae,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ids_keep_their_numbers() {
        assert_eq!(EEMessageID::kMessageEdictProposal as i32, 0x41);
        assert_eq!(EEMessageID::kMessageSelectUnit as i32, 0x48);
        assert_eq!(EEMessageID::kMessageEndCinematic as i32, 0x9d);
        assert_eq!(EEMessageID::kMessageExchangeResources as i32, 0xae);
    }
}
