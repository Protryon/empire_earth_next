"""Hand-maintained knowledge the source structs do not carry (docs/DATA_FORMATS.md is the evidence for each entry).

specgen.py combines this with the record structs parsed from the sources into specs.json. Nothing here repeats a
field layout the source defines: the struct name points at the source, SPLITS names the pieces of the source's
undecoded gaps that the survey identified from the data, and the TABLES rows describe each file's container.
"""

# Source files of the on-disk record structs (relative to the repository root).
STRUCT_SOURCES = {
    'EEUnitClassRecord': 'include/ee/EEUnitClassRecord.h',
    'EEGraphicObjectRecord': 'src/Empire Earth/Data Pre-Loading Thread/EEGraphicObject.cpp',
    'EESoundRecord': 'src/Empire Earth/Data Pre-Loading Thread/EESoundObject.cpp',
    'EEGraphicEffectRecord': 'src/Empire Earth/Data Pre-Loading Thread/EEGraphicEffect.cpp',
    'EEMusicRecord': 'src/Empire Earth/Data Pre-Loading Thread/EEMusicObject.cpp',
    'EEWorldTableEntry': 'include/ee/EEDatabaseTables.h',
    'EEDamageTableRecord': 'src/Empire Earth/Database/EETerrainTypeTable.cpp',
    'EETerrainTypeRecord': 'src/Empire Earth/Database/EETerrainTypeTable.cpp',
    'EEButtonRecord': 'src/Empire Earth/Database/EEButtonObject.cpp',
    'EETechTreeRecord': 'include/ee/EETechTreeRecord.h',
    'EEEventEffect': 'include/ee/EEEvent.h',
    'EEAnimalRecord': 'src/Empire Earth/Database/EEAnimal.cpp',
    'EEColorTableRecord': 'src/Empire Earth/Database/EEColorTable.cpp',
    'EEAIUnitTargetingRecord': 'src/Empire Earth/Database/EEAIUnitTargetingTable.cpp',
    'EETerrainRecord': 'src/Empire Earth/Database/EETerrainTable.cpp',
    'EECliffTerrainRecord': 'src/Empire Earth/Database/EETerrainTable.cpp',
    'EECivPower': 'include/ee/EECivPowersManager.h',
    'EECivilizationCategoryEntry': 'include/ee/EECivilizationManager.h',
    'EECivilizationEntry': 'include/ee/EECivilizationManager.h',
    'EEToHitRecord': 'src/Empire Earth/Database/EEUnitBehavior.cpp',
    'EEUnitBehaviorRecord': 'src/Empire Earth/Database/EEUnitBehavior.cpp',
    'EECalamityData': 'include/ee/EECalamityClass.h',
    'EEUnitSetRecord': 'src/Empire Earth/Empire Earth/EEUnitSetManager.cpp',
    'EEAreaEffectData': 'include/ee/EEAreaEffectTemplate.h',
    'EEUCUpgradeRecord': 'src/Empire Earth/Database/EEUCUpgradeTable.cpp',
    'EECPBehavior': 'include/ee/EECPBehavior.h',
    'EEGameVariantData': 'include/ee/EEGameVariant.h',
    'EEStartingResourcesRecord': 'src/Empire Earth/Database/EEStartingResources.cpp',
    'EEAmbientSoundRecord': 'include/ee/EEAmbientSound.h',
    'UIDbControl': 'src/Low-Level Engine/User Interface/UIControlsTable.cpp',
    'UIDbDataBackgrounds': 'include/lle/UIBackgroundTable.h',
    'UIDbDataForms': 'include/lle/UIForm.h',
    'UIDbDataEvents': 'src/Low-Level Engine/User Interface/UIEvent.cpp',
    'UIDbDataFonts': 'src/Low-Level Engine/User Interface/UIFontTable.cpp',
    'UIDbDataHotKeys': 'src/Low-Level Engine/User Interface/UIHotKey.cpp',
}

# Sizes for the two structs whose source states the size in a comment instead of a static_assert
# (EEUnitClassRecord.h: "EEDbObject::mRecord, 0x7a8 bytes"; UIControlsTable.cpp: "0x35c bytes each").
COMMENT_SIZES = {'EEUnitClassRecord': 0x7a8, 'UIDbControl': 0x35c}

# Pieces of the source's undecoded gaps identified from the data by the survey (DATA_FORMATS.md, "inferred"):
# struct -> {offset: kind}; kind 'name' is a NUL-terminated char[100], 'dbId' the database row id, 'index' the
# 0-based row index. The rest of every gap becomes explicit raw fields (u_0x<off>), never dropped.
SPLITS = {
    'EEAIUnitTargetingRecord': {0x00: 'name', 0x64: 'dbId', 0x68: 'index'},
    'EEAmbientSoundRecord': {0x00: 'name', 0x64: 'dbId'},
    'EEAnimalRecord': {0x00: 'name', 0x64: 'dbId'},
    'EEButtonRecord': {0x00: 'name'},
    'EECivilizationCategoryEntry': {0x08: 'name'},
    'EEColorTableRecord': {0x00: 'name', 0x64: 'dbId', 0x68: 'index'},
    'EEDamageTableRecord': {0x00: 'name', 0x64: 'dbId', 0x68: 'index'},
    'EEGraphicEffectRecord': {0x00: 'name', 0x64: 'dbId'},
    'EEGraphicObjectRecord': {0x00: 'name', 0x64: 'dbId'},
    'EEMusicRecord': {0x00: 'dbId', 0x04: 'index', 0x08: 'name'},
    'EESoundRecord': {0x00: 'name', 0x64: 'dbId', 0x68: 'index'},
    'EEStartingResourcesRecord': {0x00: 'name', 0x64: 'dbId'},
    'EETerrainTypeRecord': {0x00: 'name', 0x64: 'dbId'},
    'UIDbDataEvents': {0x00: 'name', 0x64: 'dbId'},
    'EEUnitBehaviorRecord': {0x00: 'name', 0x64: 'dbId'},
    'EEToHitRecord': {0x00: 'name', 0x64: 'dbId', 0x68: 'index'},
    'EEUCUpgradeRecord': {0x00: 'name'},
}

# Records with no source struct (the game does not load these files, or loads them field by field). Layouts
# decoded from the data (DATA_FORMATS.md 3.7, 3.38, 3.39). Fields: (name, type, count, class).
VIRTUAL_STRUCTS = {
    # dbevents.dat row head; the u32 effect count and the effect list follow (EEEvent::LoadEvents)
    'EEEventRow': [('name', 'str', 100, 'named'), ('mID', 'u32', 1, 'named')],
    # dbrandommap.dat (mis-framed by EERandomMapTable::Load; dead data)
    'RandomMapRecord': [('dbId', 's32', 1, 'inferred'), ('group', 's32', 1, 'inferred'),
                        ('name', 'str', 100, 'inferred'), ('u_0x06c', 's32', 1, 'unknown'),
                        ('u_0x070', 's32', 1, 'unknown'), ('u_0x074', 's32', 4, 'unknown')],
    # dbterraingraytextures.dat (not loaded by AoC)
    'TerrainGrayTextureRecord': [('name', 'str', 100, 'inferred'), ('dbId', 's32', 1, 'inferred'),
                                 ('index', 's32', 1, 'inferred'), ('u_0x06c', 's32', 3, 'unknown')],
}

# Tables: file stem (inside db\ of data.ssa) -> container and text form.
#   container: 'count'    u32 count + records
#              'lead'     u32 count + per record the 'lead' strings (u32 length + bytes, no NUL), then the record
#              'fixed'    records only, no count (the loader reads a fixed number)
#              'events'   rows of EEEventRow + u32 n + n x s32 until the end of the file
#              'techtree' epoch blocks until the end: u32 nodeCount, epoch record + list, nodeCount x (record + list)
#              'header'   u32 header word + u32 count + records (dbrandommap)
#   lead: names of the leading strings, in file order; 'Name[14]' expands to a list of 14 strings, 'A|B[14]' is a
#         pair of interleaved lists (A0, B0, A1, B1, ...).
#   list: name of the trailing u32-counted list (events, techtree) and its element type.
#   fixed_rows: the row count the loader expects (export warns when the file differs; import writes what it has).
TABLES = {
    'dbaiunittargeting': dict(struct='EEAIUnitTargetingRecord', container='count', format='csv', fixed_rows=3),
    'dbambientsounds': dict(struct='EEAmbientSoundRecord', container='count', format='csv'),
    'dbanimals': dict(struct='EEAnimalRecord', container='count', format='csv'),
    'dbareaeffect': dict(struct='EEAreaEffectData', container='count', format='csv'),
    'dbbuttons': dict(struct='EEButtonRecord', container='count', format='csv'),
    'dbcalamity': dict(struct='EECalamityData', container='count', format='csv'),
    'dbcivilization': dict(struct='EECivilizationCategoryEntry', container='count', format='yaml'),
    'dbcivpowers': dict(struct='EECivPower', container='count', format='csv'),
    'dbcliffterrain': dict(struct='EECliffTerrainRecord', container='count', format='csv'),
    'dbcolortable': dict(struct='EEColorTableRecord', container='count', format='csv'),
    'dbcpbehavior': dict(struct='EECPBehavior', container='count', format='csv'),
    'dbeffects': dict(struct='EEEventEffect', container='fixed', format='csv', fixed_rows=2000),
    'dbevents': dict(struct='EEEventRow', container='events', format='yaml', list=('effects', 's32'),
                     fixed_rows=1500),
    'dbfamily': dict(struct='EEDamageTableRecord', container='count', format='csv'),
    'dbgamevariant': dict(struct='EEGameVariantData', container='count', format='csv'),
    'dbgfxeffects': dict(struct='EEGraphicEffectRecord', container='count', format='csv'),
    'dbgraphics': dict(struct='EEGraphicObjectRecord', container='lead', format='yaml',
                       lead=['rowName', 'mFileName', 'mTextureName', 'mTextureNames|mModelNames[14]',
                             'mShadowModelName', 'mShadowTextureName']),
    'dbmusic': dict(struct='EEMusicRecord', container='count', format='csv'),
    'dbobjects': dict(struct='EEUnitClassRecord', container='count', format='yaml'),
    'dbpremadecivs': dict(struct='EECivilizationEntry', container='count', format='csv'),
    'dbrandommap': dict(struct='RandomMapRecord', container='header', format='csv'),
    'dbsounds': dict(struct='EESoundRecord', container='lead', format='csv', lead=['mSampleName']),
    'dbstartingresources': dict(struct='EEStartingResourcesRecord', container='count', format='csv'),
    'dbtechtree': dict(struct='EETechTreeRecord', container='techtree', format='yaml', list=('parents', 'u32')),
    'dbterrain': dict(struct='EETerrainRecord', container='lead', format='csv', lead=['mTextureName']),
    'dbterraingraytextures': dict(struct='TerrainGrayTextureRecord', container='lead', format='csv',
                                  lead=['textureName']),
    'dbterraintype': dict(struct='EETerrainTypeRecord', container='count', format='csv'),
    'dbuiback': dict(struct='UIDbDataBackgrounds', container='count', format='csv'),
    'dbuicontrolevents': dict(struct='UIDbDataEvents', container='count', format='csv'),
    'dbuicontrols': dict(struct='UIDbControl', container='count', format='csv'),
    'dbuifonts': dict(struct='UIDbDataFonts', container='count', format='csv'),
    'dbuiformevents': dict(struct='UIDbDataEvents', container='count', format='csv'),
    'dbuiforms': dict(struct='UIDbDataForms', container='count', format='csv'),
    'dbuihotkey': dict(struct='UIDbDataHotKeys', container='count', format='csv'),
    'dbunitbehavior': dict(struct='EEUnitBehaviorRecord', container='count', format='csv'),
    'dbunitset': dict(struct='EEUnitSetRecord', container='count', format='csv'),
    'dbupgrade': dict(struct='EEUCUpgradeRecord', container='count', format='csv'),
    'dbweapontohit': dict(struct='EEToHitRecord', container='count', format='csv', fixed_rows=6),
    'dbworld': dict(struct='EEWorldTableEntry', container='count', format='csv'),
}

# Text-only row labels (checked aliases; the binary has no name column): table -> {row position: name}.
# dbworld: names from the call sites of gWorldTable.Get*Value(k) (docs/DATA_FORMATS.md 3.12).
# Each name says what the reading code does with the row (file, function); rows without a clear reader stay
# unnamed. Long/Long2 and Float/Float2 are the entry's two values of that type (mLongValue, mLongValue2, ...).
ROW_NAMES = {'dbworld': {
    21: 'conversionsShownMax',        # Long: the statistics' conversions n/max (EEUIFMGGStatistics::Refresh)
    22: 'miningLimit',                # Long -> EEPlayer::mMiningLimit (EEPlayer::Reset)
    25: 'chatMessageTime',            # Long: chat log entry time, ms (x2, x3 for system lines; EEUserInterface)
    26: 'unitLimitRange',             # Long..Long2: the unit limit choices, step 50 (EEGameOptions, lobby)
    27: 'unitLimitDefault',           # Long: EEGameOptions::SetDefaults mUnitLimit
    34: 'wonderVictoryTime',          # Long: wonder victory countdown, s (EEVictoryCondition::GetWonderCountdown)
    35: 'conversionSearchRadius',     # Float: priest ConvertTarget's EEGetUnitsNear radius
    36: 'shareBonus',                 # Float/Float2 x population limit (EEWorld::ApplyShareBonus/RemoveShareBonus)
    37: 'healAmount',                 # Long: heal base and factor (EEPlayer::UpdatePlayer)
    42: 'gameSpeedDefault',           # Float -> gGameStateGameSpeed (EEWorldTable::Load)
    43: 'gameSpeedRange',             # Float..Float2: the in-game speed limits (EEUIEMGChangeGameSpeed)
    44: 'gameSpeedStep',              # Float: the in-game speed step (EEUIEMGChangeGameSpeed)
    45: 'killRefundFactor',           # Float: RefundTechnology factor (EEMKillUnit::Execute)
    46: 'conversionCost',             # Long: EEGetConversionCost base
    47: 'improvementPointLimit',      # Long: improvement cost cap (EEPlayer::CanImprove, EEUICPortrait::Update)
    48: 'startingCitizens',           # Long -> mStartingCitizens / mMaxStartingCitizens (EEServer, EEPlayer::Reset)
    49: 'groupCapFactor',             # Float -> EEActiveGroup::mCapFactor
    50: 'wonderPowerAmount',          # Float: EEPlayer::AddWonderPower/RemoveWonderPower
    51: 'healEpochFactor',            # Float: heal per epoch step (EEPlayer::UpdatePlayer)
    52: 'healEpochDivisor',           # Long: epochs per heal step (EEPlayer::UpdatePlayer)
    53: 'elevationHighThreshold',     # Float x tile height (EEUnitClass::GetElevationFactor)
    54: 'elevationLowThreshold',      # Float x tile height (EEUnitClass::GetElevationFactor)
    55: 'elevationHighBonus',         # Float: added to the uphill factor (EEUnitClass::GetElevationFactor)
    56: 'elevationLowFactor',         # Float: the downhill factor (EEUnitClass::GetElevationFactor)
    **{57 + k: f'improvementTime{k}' for k in range(16)},   # Long: first level, Long2: later levels, ms
                                                             # (EEPlayer::AdvanceImprovement: improvement + 57)
    74: 'complexUnitSight',           # Float: EEComplexUnit::SightOn/SightOff radius
    75: 'upgradeButtonDelay',         # Long, ms (EEUIBUpgrade::ButtonUp, EEUIFMGStatusButton::ButtonUp)
    76: 'civilizationPoints',         # Long: civilization builder points (EEGameState::Create, editors)
    77: 'wondersForVictoryMax',       # Long: the wonders-for-victory choices 1..n (EEGameOptions, lobby)
    80: 'unitEpochCostFactor',        # Float: unit cost growth per epoch behind (EETechTreeNode::GetCost, kind 4)
    81: 'gameSpeedOption01',          # Float: game speed option 0, Float2: option 1 (EEGameState::Create)
    82: 'gameSpeedOption23',          # Float: game speed option 2, Float2: option 3 (EEGameState::Create)
    83: 'acknowledgementDelay',       # Long, ms (EEPlayer::PlayAcknowledgement)
    84: 'aiAttackChance',             # Float/Float2: random thresholds (EECPAttackSubGroup::AttackWith)
    85: 'camouflage',                 # Long/Long2: camouflage timers, Float: cloak detection range factor
    86: 'civPowerCost',               # Long -> the civ powers category cost (EECivilizationManager::Load)
    88: 'fanatic',                    # Long: duration ms, Float: hit point loss, Float2: damage factor (GoFanatic)
    89: 'damageTableScale',           # Float/Float2: the two scaled damage tables (LoadDamageTables)
    90: 'unitCostScale',              # Float: unit cost factor (EETechTreeNode::GetCost, resources option != 4)
    91: 'tradeRate46',                # Float: trade rate case 4, Float2: case 6 (EEGetTradePrice)
    92: 'tradeRate23',                # Float: trade rate case 2, Float2: case 3 (EEGetTradePrice)
    93: 'unitCostScaleResources4',    # Float: unit cost factor with resources option 4 (EETechTreeNode::GetCost)
    94: 'resourceTime',               # Long, s (EEPhysics::Tick, resources option 4)
}}
