"""Hand-maintained knowledge the source structs do not carry (docs/DATA_FORMATS.md is the evidence for each entry).

specgen.py combines this with the record structs parsed from the sources into specs.json. Nothing here repeats a
field layout the source defines: the struct name points at the source, DEFAULTS and HOLES give the values the
importer writes where the text has none, and the TABLES rows describe each file's container.
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

# Records with no source struct (loaded field by field). Fields: (name, type, count, class).
VIRTUAL_STRUCTS = {
    # dbevents.dat row head; the u32 effect count and the effect list follow (EEEvent::LoadEvents reads the name into
    # a local it never uses, the id into EEEvent::mID, which is private and read by nothing)
    'EEEventRow': [('mLabel', 'str', 100, 'label'), ('mUnusedID', 'raw', 4, 'unused')],
}

# Tables: file stem (inside db\ of data.ssa) -> container and text form.
#   container: 'count'    u32 count + records
#              'lead'     u32 count + per record the 'lead' strings (u32 length + bytes, no NUL), then the record
#              'fixed'    records only, no count (the loader reads a fixed number)
#              'events'   rows of EEEventRow + u32 n + n x s32 until the end of the file
#              'techtree' epoch blocks until the end: u32 nodeCount, epoch record + list, nodeCount x (record + list)
#   lead: names of the leading strings, in file order; 'Name[14]' expands to a list of 14 strings, 'A|B[14]' is a
#         pair of interleaved lists (A0, B0, A1, B1, ...).
#   list: name of the trailing u32-counted list (events, techtree) and its element type.
#   fixed_rows: the row count the loader expects (export warns when the file differs; import writes what it has).
# dbrandommap.dat (mis-framed by EERandomMapTable::Load: every record is thrown away) and dbterraingraytextures.dat
# (no AoC code opens it) have no text form: the game reads nothing of them.
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
    'dbsounds': dict(struct='EESoundRecord', container='lead', format='csv', lead=['mSampleName']),
    'dbstartingresources': dict(struct='EEStartingResourcesRecord', container='count', format='csv'),
    'dbtechtree': dict(struct='EETechTreeRecord', container='techtree', format='yaml', list=('parents', 'u32')),
    # the epoch registry: text only (techtree.py); it compiles with dbtechtree.yaml into db\dbtechtree.dat
    'epochs': dict(struct='EETechTreeRecord', container='epochs', format='yaml', list=('parents', 'u32'),
                   text_only=True),
    'dbterrain': dict(struct='EETerrainRecord', container='lead', format='csv', lead=['mTextureName']),
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


# Values the importer writes into fields the text does not carry (struct -> {member: value}); every other unread
# byte is zero. The unread effect slots of dbobjects are effect row numbers whose media the class still loads: -1
# (none) keeps it from loading effect row 0 for every unit.
DEFAULTS = {
    'EEUnitClassRecord': {**{f'mUnusedEffect{k}': -1 for k in range(1, 9)}, 'mUnusedFireTrailEffect': -1},
}

# Hole rows: positions the text does not list. The importer writes the table's hole record there: DEFAULTS, then
# these values (table -> {field: value}: the database's empty-row values, i.e. what the retail hole rows hold in the
# fields the game reads, so a hole loads exactly as before); everything else zero, lead strings empty, lists empty.
# Generated once from the retail data (eedata.py holes prints it); check.py proves it against the retail holes.
HOLES = {
    'dbareaeffect': {
        'mID': -1},
    'dbbuttons': {
        'mID': -1, 'mHotKey': -1},
    'dbeffects': {
        'mUnitClass': -1, 'mNewUnitClass': -1, 'mUnitSet': -1, 'mKind': -1, 'mMediaIndex': -1, 'mTechnology':
        -1, 'mAttribute': -1, 'mSoundID': -1, 'mButton': -1, 'mAreaEffect': -1},
    'dbgfxeffects': {
        'mID': -1, 'mKind': -1, 'mTagPointKind': -1, 'mMediaIndex': -1, 'mStartSize': 1.0, 'mHeight': 1.0,
        'mLinkedEffectIndex1': -1, 'mLinkedEffectIndex2': -1, 'mLinkedEffectIndex3': -1},
    'dbgraphics': {
        'mSound1Index0': 4294967295, 'mSound2Index0': 4294967295, 'mSound1Index13': 4294967295,
        'mSound2Index13': 4294967295, 'mSound1Index7': 4294967295, 'mSound2Index7': 4294967295, 'mSound1Index8':
        4294967295, 'mSound2Index8': 4294967295, 'mSound1Index12': 4294967295, 'mSound2Index12': 4294967295,
        'mSound1Index11': 4294967295, 'mSound2Index11': 4294967295, 'mSound1Index9': 4294967295,
        'mSound2Index9': 4294967295, 'mSound1Index10': 4294967295, 'mSound2Index10': 4294967295,
        'mSound1Index6': 4294967295, 'mSound2Index6': 4294967295, 'mSound1Index3': 4294967295, 'mSound2Index3':
        4294967295, 'mSound1Index1': 4294967295, 'mSound2Index1': 4294967295, 'mSound1Index2': 4294967295,
        'mSound2Index2': 4294967295, 'mKind': -1, 'mOpacity': 1.0},
    'dbobjects': {
        'mFamily': -1, 'mUnitType': -1, 'mWeapon': -1, 'mProjectileType': -1, 'mAmmoType': -1, 'mCategory': -1,
        'mButton': -1, 'mGraphic': -1, 'mCreationSound': -1, 'mAcknowledgeSound1': -1, 'mAcknowledgeSound2': -1,
        'mSelectionSound1': -1, 'mSelectionSound2': -1, 'mDeathSound': -1, 'mCommand0': -1, 'mCommand1': -1,
        'mCommand2': -1, 'mCommand3': -1, 'mCommand4': -1, 'mCommand5': -1, 'mCommand6': -1, 'mCommand7': -1,
        'mCommand8': -1, 'mCanSetRallyPoint': 1, 'mIsPlayerPlaceable': 1, 'mDeathEffect': -1, 'mDamageEffect':
        -1, 'mMuzzleFlashEffect': -1, 'mTrailEffect': -1, 'mRippleEffect': -1, 'mSplashEffect': -1,
        'mHealEffect': -1, 'mImpactSparksEffect': -1, 'mImpactBloodEffect': -1, 'mAttachedEffect': -1,
        'mCreationEffect': -1, 'mSpecialAttackEffect': -1, 'mShotEffect': -1, 'mLeprosyEffect': -1,
        'mPlagueEffect': -1, 'mMoraleEffect': -1, 'mPulseCannonEffect': -1, 'mBattleCryEffect': -1,
        'mShellHoleEffect': -1, 'mBloodStainEffect': -1, 'mCanMoveOnLand': 1, 'mCanMoveOnDeepWater': 1,
        'mCanMoveOnRock': 1, 'mCanMoveOnTerrainType3': 1, 'mCanMoveOnMud': 1, 'mCanEnterFortresses': 1,
        'mCanMoveOnRoughGrass': 1, 'mCanMoveOnTerrainType7': 1, 'mCanMoveOnPavement': 1, 'mCanMoveOnCliffs': 1,
        'mCanMoveOnShallowWater': 1, 'mCanMoveOnSand': 1, 'mAreaEffectTemplate0': -1, 'mAreaEffectTemplate1':
        -1, 'mAreaEffectTemplate2': -1, 'mAreaEffectTemplate3': -1, 'mGarrisonTechnology': -1, 'mTechnology':
        100000, 'mBlocksTile': 1, 'mAttributes': 4294967295},
    'dbtechtree': {
        'mObsoleteEpoch': 15, 'mResearchedEvent': -1, 'mObsoleteEvent': -1, 'mUnitClass': -1, 'mButton': 1,
        'mAvailableWithEpoch': 1, 'mResearchedWithEpoch': 1},
    'dbterrain': {
        'mID': -1},
    'dbuiback': {
        'mOpacity': 1.0, 'mBackgroundID': 4294967295, 'mFormID': 4294967295, 'mDrawPriority': 1},
    'dbuicontrolevents': {
        'mEventID': -1, 'mOwnerID': 4294967295, 'mEventType': 4294967295},
    'dbuicontrols': {
        'mBackgroundOpacity': 1.0, 'mTextBoxOpacity': 1.0, 'mScrollBarBgOpacity': 1.0, 'mID': -1,
        'mControlType': 4294967295, 'mStringTableBegin': -1, 'mStringTableEnd': -1},
    'dbuiforms': {
        'mFormID': -1, 'mHelpTextTimeDelay': 1},
    'dbunitset': {
        'mID': -1, 'mFamilies': 4294967295, 'mUnitTypes': -1, 'mIncludedSet': -1, 'mExcludedSet': -1},
}

# dbworld: the constants by row (names from their readers, docs/DATA_FORMATS.md 3.12; the reader of each is noted).
# Value letters: F mFloatValue, F2 mFloatValue2, L mLongValue, L2 mLongValue2: the values the game reads (the text
# leaves the others empty). Rows not listed have no reader: the text leaves them out, the importer writes zeros.
WORLD_ROWS = {
    21: ('conversionsStatisticMax', 'L'),       # the "max" of the statistics' conversions n/max (EEUIFMGGStatistics)
    22: ('gatherersPerResourceMax', 'L'),       # -> EEPlayer::mMiningLimit: gatherers on one resource (EEPlayer::Reset)
    25: ('chatLogDisplayTime', 'L'),            # chat log entry lifetime, ms (x2 notices, x3 console; EEUserInterface)
    26: ('unitLimitRange', 'L L2'),             # smallest / largest unit limit choice, step 50 (EEGameOptions, lobby)
    27: ('unitLimitDefault', 'L'),              # EEGameOptions::SetDefaults mUnitLimit
    34: ('wonderVictoryTime', 'L'),             # wonder victory countdown, s (EEVCWonder; EECPIntelligence's percentage)
    35: ('areaConversionRadius', 'F'),          # extra conversions around a converted unit (wonder power 1; priests)
    36: ('wonderPopulationBonus', 'F F2'),      # population limit fraction added / removed (EEWorld::ApplyShareBonus)
    37: ('wonderHealAmount', 'L'),              # heal per interval of wonder power 6 (EEPlayer::UpdatePlayer)
    42: ('gameSpeedDefault', 'F'),              # -> gGameStateGameSpeed (EEWorldTable::Load)
    43: ('gameSpeedRange', 'F F2'),             # lowest / highest in-game speed (EEUIEMGChangeGameSpeed)
    44: ('gameSpeedStep', 'F'),                 # speed change per key press (EEUIEMGChangeGameSpeed)
    45: ('deleteRefundFactor', 'F'),            # cost share refunded for a deleted unfinished unit (EEMKillUnit)
    46: ('conversionFaithCost', 'L'),           # faith a conversion costs (EEUCBasicPriest, EEGetConversionCost)
    47: ('unitImprovementPointsMax', 'L'),      # cap on a unit type's improvement points (EEPlayer::CanImprove)
    48: ('startingCitizensDefault', 'L'),       # -> mStartingCitizens / mMaxStartingCitizens (EEServer, EEPlayer)
    49: ('groupSpeedCapFactor', 'F'),           # -> EEActiveGroup::mCapFactor: x the slowest unit's speed
    50: ('wonderHitPointBonus', 'F'),           # hit points added by wonder power 2 (EEPlayer::AddWonderPower)
    51: ('wonderHealEpochFactor', 'F'),         # heal growth per epoch step (EEPlayer::UpdatePlayer)
    52: ('wonderHealEpochStep', 'L'),           # epochs per heal step (EEPlayer::UpdatePlayer)
    53: ('highGroundHeight', 'F'),              # x tile height: counts as high ground (EEUnitClass::GetElevationFactor)
    54: ('lowGroundHeight', 'F'),               # x tile height: counts as low ground (EEUnitClass::GetElevationFactor)
    55: ('highGroundAttackFactor', 'F'),        # attack factor from high ground (+ the player's bonus)
    56: ('lowGroundAttackFactor', 'F'),         # attack factor from low ground
    **{57 + k: (f'improvementTime{n}', 'L L2') for k, n in enumerate(   # first level / later levels, ms
        ['AreaEffect', 'Attack', 'ArrowArmor', 'GunArmor', 'LaserArmor', 'MissileArmor', 'PierceArmor',
         'ShockArmor', 'FlightTimeOrPower', 'HitPoints', 'Leadership', 'Range', 'Repair', 'Kind13', 'Speed',
         'Transport'])},                        # (EEPlayer::AdvanceImprovement: improvement kind + 57)
    74: ('wonderSightRadius', 'F'),             # sight around a wonder with bonus 3 (EEComplexUnit::SightOn/SightOff)
    75: ('refusalMessageInterval', 'L'),        # ms between repeated "cannot" messages (EEUIBUpgrade, status buttons)
    76: ('civilizationBuilderPoints', 'L'),     # civilization builder points (EEGameState::Create, editors)
    77: ('wondersForVictoryMax', 'L'),          # the wonders-for-victory choices 1..n (EEGameOptions, lobby)
    78: ('tileStrayDistance', 'F'),             # LocationWayOffMap trigger distance; MoveToTileCentre offset (EEUnitAI)
    79: ('trampleMinDistanceSquared', 'F'),     # -> gPhysicsModelsF: squared distance for a trample hit
    80: ('unitEpochCostFactor', 'F'),           # unit cost growth per epoch past the node's (EETechTreeNode::GetCost)
    81: ('gameSpeedSlowNormal', 'F F2'),        # speed of game speed option 0 / option 1 (EEGameState::Create)
    82: ('gameSpeedFastFastest', 'F F2'),       # speed of option 2 / option 3 (EEGameState::Create)
    83: ('acknowledgementDelay', 'L'),          # ms between acknowledgements (EEPlayer::PlayAcknowledgement)
    84: ('aiSpecialAttackChance', 'F F2'),      # AI flaming arrows on buildings / AI fanaticism (EECPAttackSubGroup)
    85: ('camouflageTiming', 'F L L2'),         # L idle ms before cloaking, L2 ms the cloak lasts, F detection x range
    86: ('specialPowersCategoryCost', 'L'),     # the civ builder's special powers category (EECivilizationManager)
    87: ('reloadTimeRandomRange', 'F F2'),      # min / max random factor on each reload time (EEComplexUnit)
    88: ('fanaticism', 'F F2 L'),               # L duration ms, F hit point share lost, F2 damage factor (GoFanatic)
    89: ('difficultyDamageScale', 'F F2'),      # the two scaled damage tables (EETerrainTypeTable::LoadDamageTables)
    90: ('jitManufacturingCostFactor', 'F'),    # rally-point unit cost factor (EETechTreeNode::GetCost)
    91: ('tradeRateStoneIron', 'F F2'),         # stone / iron base market rate (EEGetTradePrice)
    92: ('tradeRateFoodWood', 'F F2'),          # food / wood base market rate (EEGetTradePrice)
    93: ('jitManufacturingCostFactorDeathmatch', 'F'),  # the same with resources option 4 (EETechTreeNode::GetCost)
    94: ('deathmatchJitManufacturingDelay', 'L'),       # s until the deathmatch JIT buttons open (EEPhysics::Tick)
}
WORLD_VALUES = {'F': 'mFloatValue', 'F2': 'mFloatValue2', 'L': 'mLongValue', 'L2': 'mLongValue2'}


def snake(name):
    """camelCase -> snake_case (the ids of named constants: unitLimitRange -> unit_limit_range)."""
    import re
    return re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', '_', name).lower()


# dbworld: each row's id is its constant's name (checked: the id must match its row; these become the define names)
# and the fields each row's readers use.
ROW_NAMES = {'dbworld': {k: snake(n) for k, (n, _) in WORLD_ROWS.items()}}
ROW_FIELDS = {'dbworld': {k: [WORLD_VALUES[v] for v in u.split()] for k, (_, u) in WORLD_ROWS.items()}}


# ---------------------------------------------------------------- ids and references (format 3)
# Every listed row carries an `id`: a stable, unique, lowercase ASCII key (ids.py generates it once from the row's
# name; after that the text owns it). References between tables are written as the target row's id; an integer is
# kept only where it names no row of the target (the "none" values: -1, 0 where row 0 is a hole, 100000, ...).
# The runtime integer of a row (its key) is its row position, except dbtechtree, whose key is mID (nodes are found
# by id, EETechTreeEpoch.cpp). The evidence for every reference: docs/DATA_FORMATS.md 8.6 (loader -> consumer).

KEYS = {'dbtechtree': 'mID', 'epochs': 'position'}

# epochs.yaml: the epoch record fields that follow from the epoch's position p (value = a * p + b): left out where
# they do (techtree.py)
DERIVED = {'epochs': {'mID': (1, 0), 'mEpoch': (1, 0), 'mNextEpoch': (1, 1), 'mFirstNodeID': (1000, 1000)}}

# Per-epoch flag arrays whose text is in epochs.yaml (table: (field, epochs.yaml key)): element k of the field is
# "epoch k's list names this row". The table's text leaves the field out; the importer sets it from the lists of the
# first `count` epochs (dbmusic: the songs of each epoch, epochs.yaml music; the game reads the lists, EERules.cpp).
EPOCH_FLAGS = {'dbmusic': ('mInEpoch', 'music')}

# Fields that hold the row's own key (+ an offset): the text leaves them out when they are equal, the importer
# writes key + offset. dbcivpowers: GetPower(id) reads row id - 0x26.
SELF = {
    'dbobjects': ('mUnitType', 0), 'dbareaeffect': ('mID', 0), 'dbbuttons': ('mID', 0), 'dbcalamity': ('mID', 0),
    'dbcivpowers': ('mID', 0x26), 'dbgfxeffects': ('mID', 0), 'dbunitset': ('mID', 0), 'dbanimals': ('mAnimalID', 0),
    'dbupgrade': ('mIndex', 0), 'dbunitbehavior': ('mIndex', 0), 'dbcpbehavior': ('mIndex', 0),
    'dbterrain': ('mID', 0), 'dbterraintype': ('mID', 0), 'dbgraphics': ('mMediaIndex', 0),
    'dbcliffterrain': ('mIndex', 0), 'dbgamevariant': ('mIndex', 0), 'dbpremadecivs': ('mIndex', 0),
    'dbstartingresources': ('mIndex', 0), 'dbambientsounds': ('mIndex', 0), 'dbuihotkey': ('mID', 0),
    'dbuicontrols': ('mID', 0), 'dbuiforms': ('mFormID', 0),
}

_UT, _TECH, _SET, _GFX, _SND, _MEDIA = 'dbobjects', 'dbtechtree', 'dbunitset', 'dbgfxeffects', 'dbsounds', 'dbgraphics'
_GFXFIELDS = ['mDeathEffect', 'mDamageEffect', 'mMuzzleFlashEffect', 'mTrailEffect', 'mRippleEffect', 'mSplashEffect',
              'mHealEffect', 'mImpactSparksEffect', 'mImpactBloodEffect', 'mAttachedEffect', 'mCreationEffect',
              'mSpecialAttackEffect', 'mShotEffect', 'mLeprosyEffect', 'mPlagueEffect', 'mMoraleEffect',
              'mPulseCannonEffect', 'mBattleCryEffect', 'mShellHoleEffect', 'mBloodStainEffect']
_ANIMS = [0, 13, 7, 8, 12, 11, 9, 10, 6, 3, 1, 2]   # the dbgraphics animation slots that carry sounds

# table -> {field: target table}; '@list' is the table's trailing list. {'table': t, 'none': [v]}: values that mean
# "none" although they are a key of the target; they stay integers.
REFS = {
    'dbobjects': {
        'mFamily': 'dbfamily', 'mWeapon': 'dbweapontohit', 'mButton': 'dbbuttons', 'mGraphic': _MEDIA,
        **{k: _UT for k in ('mProjectileType', 'mScaffoldingType', 'mDeathType', 'mDeathSpawnType', 'mPackedType',
                            'mRemainsType')},
        **{k: _SND for k in ('mCreationSound', 'mAcknowledgeSound1', 'mAcknowledgeSound2', 'mSelectionSound1',
                             'mSelectionSound2', 'mDeathSound')},
        **{k: _GFX for k in _GFXFIELDS},
        'mUpgradeTableIndex': 'dbupgrade', 'mBehavior': 'dbunitbehavior',
        **{f'mAreaEffectTemplate{k}': 'dbareaeffect' for k in range(4)},
        'mTechnology': _TECH, 'mGarrisonTechnology': _TECH, 'mAttributes': _TECH,
        'mOverlapUnitSet': _SET, 'mWonderUnitSet': _SET, 'mMiniMapColor': 'dbcolortable',
        'mEpoch': {'table': 'epochs', 'never': True},   # 15 (the original's epoch count) on a few: no epoch
    },
    'dbtechtree': {
        'mResearchedEvent': 'dbevents', 'mObsoleteEvent': 'dbevents',
        **{f'mPrerequisite{k}': {'table': _TECH, 'none': [0]} for k in range(1, 5)},   # 0: pre-ticked (none)
        'mUnitClass': _UT, 'mDefaultUnitClass': _UT, '@list': _UT, 'mButton': 'dbbuttons',
        'mCalamity': 'dbcalamity', 'mHotKey': 'dbuihotkey',
        # a tech's epoch fields: epochs (mObsoleteEpoch's "never" is the epoch count: the original's 15)
        'mEpoch': 'epochs', 'mObsoleteEpoch': {'table': 'epochs', 'never': True},
    },
    'dbevents': {'@list': 'dbeffects'},
    'dbeffects': {
        'mUnitClass': _UT, 'mNewUnitClass': _UT, 'mUnitSet': _SET, 'mMediaIndex': _MEDIA, 'mTechnology': _TECH,
        'mSoundID': _SND, 'mButton': 'dbbuttons', 'mAreaEffect': 'dbareaeffect',
    },
    'dbgraphics': {f'mSound{s}Index{a}': _SND for a in _ANIMS for s in (1, 2)},
    'dbgfxeffects': {'mMediaIndex': _MEDIA, **{f'mLinkedEffectIndex{k}': _GFX for k in (1, 2, 3)}},
    'dbanimals': {'mUnitType': _UT, 'mMedia': _MEDIA, **{f'mSound{k}': _MEDIA for k in (1, 2, 3)}},
    'dbambientsounds': {'mSoundID': _SND},
    'dbaiunittargeting': {'mPreferredUnitType': _UT},
    'dbareaeffect': {'mGraphicEffect': _GFX, 'mGraphicEffect18': _GFX, 'mUnitSet': _SET, 'mMedia': _MEDIA},
    'dbbuttons': {'mHotKey': 'dbuihotkey'},
    'dbcalamity': {**{f'mGraphicEffectID{k}': _GFX for k in (1, 2, 3, 4)}, 'mSoundID': _SND,
                   'mSecondarySoundID': _SND, 'mUnitClassIndex': _UT, 'mUnitSet': _SET},
    'dbcivpowers': {'mUnitSet': _SET, 'mTechnology': _TECH, 'mUnitType': _UT},
    'dbcivilization': {'mUnitSet': _SET},
    'dbgamevariant': {**{f'mSet{k}': _SET for k in range(10)}, 'mCostSet': _SET},
    'dbunitset': {'mFamilies': 'dbfamily', 'mUnitTypes': _UT, 'mIncludedSet': _SET, 'mExcludedSet': _SET},
    'dbterrain': {'mFirst': _UT, 'mLast': _UT, 'mColorType': 'dbterraintype'},
    'dbcliffterrain': {'mFirstTerrain': 'dbterrain', 'mLastTerrain': 'dbterrain'},
    'dbuicontrols': {'mFormID': 'dbuiforms', 'mFontTableIndex': 'dbuifonts'},
    'dbuicontrolevents': {'mOwnerID': 'dbuicontrols'},
    'dbuiformevents': {'mOwnerID': 'dbuiforms'},
    'dbuiback': {'mFormID': 'dbuiforms'},
}
REFS['epochs'] = dict(REFS['dbtechtree'])

# Where ids.py takes a row's name from (the first non-empty of these text keys); '@world' and '@cpbehavior' are the
# readers' names (WORLD_ROWS, include/ee/EECPBehavior.h), '@effects' the first event that lists the effect.
ID_NAMES = {
    'dbworld': ['@world'], 'dbcpbehavior': ['@cpbehavior'], 'dbeffects': ['@effects'],
    'dbobjects': ['mName'], 'dbgraphics': ['rowName', 'label'], 'dbsounds': ['label', 'mSampleName'],
    'dbareaeffect': ['mName'], 'dbcliffterrain': ['mName'], 'dbterrain': ['mName'], 'dbuiback': ['mName'],
    'dbunitset': ['mName'], 'dbuifonts': ['mFontName'], 'dbpremadecivs': ['mFileName'],
}
ID_DEFAULT = ['label']
# Names that make a better id than the row's own (table -> {key: name}).
_EPOCHS = ['prehistoric_age', 'stone_age', 'copper_age', 'bronze_age', 'dark_age', 'middle_ages', 'renaissance',
           'imperial_age', 'industrial_age', 'atomic_age_ww1', 'atomic_age_ww2', 'atomic_age_modern', 'digital_age',
           'nano_age', 'space_age']   # the epochs' titles (Language.rc 30145..30159)
ID_OVERRIDES = {'dbtechtree': dict(enumerate(_EPOCHS)),
                'dbweapontohit': {0: 'shock'},   # "Defender: Shock": the row is the shock weapon kind
                'dbareaeffect': {0: 'none'}}      # "None (don't delete!!!!)": the no-area-effect row
