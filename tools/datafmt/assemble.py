"""Assemble db_formats.md from template.md, tables.json and the per-table prose below."""
import json, os, struct, collections
BS2 = chr(10)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'db_formats.md')
T = json.load(open(os.path.join(HERE, 'tables.json')))
tpl = open(os.path.join(HERE, 'template.md'), encoding='utf-8').read()

EE = 'src/Empire Earth/'
LLE = 'src/Low-Level Engine/'
# file: (title, loader, container, prose, xrefs, recommended)
P = {
 'dbobjects': ('Unit classes', f'`EEUnitClass::CreateUnitClasses` 0x6358b1 -> `EEDatabase::Load` 0x4d60ae / `EEDbObject::Load` 0x4d689b ({EE}Database/EEDatabase.cpp, EEDbObject.cpp); fields consumed by `EEUnitClass::EEUnitClass` 0x62f317 ({EE}Units/EEUnitClass.cpp) and the `EEUC*` subclass ctors. Layout: include/ee/EEUnitClassRecord.h',
   '`u32 count` + count x 1960-byte `EEUnitClassRecord` (read whole into `EEDbObject::mRecord[0x7a8]`).',
   'Rows with `mIsEnabled == 0` or `mUnitType == -1` are skipped. The class factory switches on `mCategory` (0..0x22: ambient, resource, building, airplane, naval, land, citizen, helicopter, projectile, bomb, missile, priest, torpedo...). `gUnitClassTable[mUnitType]` is the class (0x367 slots). Bugs kept by the port: terrain slot 5 copies `mB2DC` again, so `mC2E0`/`mC2E1` are never read. +0x3b4..+0x47b (`mU3B4..mU478`, 200 bytes) is unread and holds stale converter heap values. In the "ctor ->" notes, the left side is the `EEUnitClass` member the field is copied into. Subclass ctors may read more fields.',
   '`mFamily` -> dbfamily row (damage table); `mButton` -> dbbuttons; `mGraphic` -> dbgraphics (media index); `mL150` (creation sound), `mAcknowledgeSound1/2`, `mSelectionSound1/2`, `mDeathSound` -> dbsounds; `mL24C..mL2BC`, `mCreationEffect`, `mLeprosyEffect`, `mPlagueEffect`, `mMoraleEffect` -> dbgfxeffects; `mAreaEffectTemplate0..3` -> dbareaeffect; `mUpgradeTableIndex` -> dbupgrade; `mBehavior` -> dbunitbehavior; `mTechnology`, `mGarrisonTechnology`, `mAttributes[0..mNumAttributes)` -> dbtechtree node ids; `mOverlapUnitSet`, `mWonderUnitSet` -> dbunitset; `mAIScriptName` -> `Unit AI Scripts\\<name>.tai`; `mNameStringID` -> Language.dll; `mUnitType`, `mDeathSpawnType`, `mPackedType` -> unit type ids.',
   'YAML'),
 'dbgraphics': ('Graphic objects (models + textures + animation states)', f'`EEGraphicObject::LoadTable` 0x4d7833 + `EELoadGraphicObjectRecord` 0x4d7b37 ({EE}Data Pre-Loading Thread/EEGraphicObject.cpp)',
   '`u32 count`; per entry: **33 strings** (`u32 len` + bytes, no NUL): [0] row name (discarded), [1] model file (`mFileName`, e.g. `models\\gun_80mmAA_10`), [2] texture (`mTextureName`), then 14 x ([texture, model] for animation state k), then shadow model and shadow texture. These are followed by the 716-byte `EEGraphicObjectRecord`.',
   'Entries with `mMediaIndex == 0` are deleted after loading (23 rows). The 14 state blocks are stored in the order 0, 13, 7, 8, 4, 5, 12, 11, 9, 10, 6, 3, 1, 2. Each block holds sharesFrames, firstFrame, lastFrame, frameTime, loops, holdTime, then 2 x (sound frame, sound index), then isSpeedScaled; blocks 4 and 5 have no sound fields. The flags of neighbouring blocks share dwords, so the field order is irregular. The meaning of the state slots is not decoded; the model names suggest: 0 hunt/attack, 1 build, 2 carry, 3 chop, 4/5 death (`gen_deathT`), 6 farm, 7/8 idle/fidget, 9 mine/gate, 10 forage/diagonal, 11 attack, 12 unused, 13 walk (walls reuse the slots for wall pieces). `mHoldTime0` is replaced by 300000 at load. Graphic-effect tag-point kinds are named in `gTagPointKindNames[0x48]`.',
   '`mSound1IndexN`/`mSound2IndexN` -> dbsounds (-1 = none); model/texture strings -> `models\\*.cem`, `textures\\*.sst|.tga`; referenced by dbobjects `mGraphic`, dbgfxeffects `mMediaIndex`, dbeffects `mMediaIndex`, dbareaeffect `mMedia`, dbanimals `mMedia`.',
   'YAML'),
 'dbsounds': ('Sounds', f'`EESoundObject::LoadTable` 0x4da680 ({EE}Data Pre-Loading Thread/EESoundObject.cpp)',
   '`u32 count`; per entry: one string (`u32 len` + bytes, no NUL) = sample file (e.g. `sounds\\heligun1-8.wav`), then a 132-byte `EESoundRecord`.',
   'Every row becomes an object, holes included (215 rows with an empty file name). `mVolume` is a percentage (x 0.01 at load). `mCategory` is the sound-manager category.',
   'Referenced by dbobjects, dbgraphics, dbcalamity, dbambientsounds, dbeffects (`mSoundID`) and dbanimals by row position.',
   'CSV'),
 'dbgfxeffects': ('Graphic effects', f'`EEGraphicEffect::LoadTable` 0x4d6e9e, ctor 0x4d6ceb ({EE}Data Pre-Loading Thread/EEGraphicEffect.cpp)',
   '`u32 count` + count x 232-byte `EEGraphicEffectRecord`.',
   'A row with `mID > 0` makes an effect; row 0 ("None") and the holes stay NULL. `mLinkedEffectIndex1..3` are row indices, resolved to pointers after loading. `mKind` selects the effect class; `mParam1..12` and `mL90..mL98` are class-specific parameters (provisional).',
   '`mMediaIndex` -> dbgraphics; `mLinkedEffectIndex1..3` -> dbgfxeffects; `mNameStringId` -> Language.dll; `mTagPointKind` -> `gTagPointKindNames` (0x48 kinds). Referenced by dbobjects (20+ slots), dbcalamity, dbareaeffect.',
   'CSV'),
 'dbmusic': ('Music (songs per epoch)', f'`EEMusicObject::LoadTable` 0x4d9f6c ({EE}Data Pre-Loading Thread/EEMusicObject.cpp)',
   '`u32 count` + count x 196-byte `EEMusicRecord`. Row layout: dbId, index, name, then the file name.',
   'Row 0 is a hole that still gets loaded (all epoch flags are 0, so it is never played). `mFileName` + ".sgt" is the DirectMusic segment. The space epoch reuses the nano flag. If `Data\\Music\\New Music.txt` exists, it replaces the table (section 4). +0xb4 is an unread bool, perhaps an epoch slot.',
   'none (file names point at `Data\\Music`).',
   'CSV'),
 'dbworld': ('World constants', f'`EEWorldTable::Load` 0x4dcb79 ({EE}Database/EEUnitBehavior.cpp); struct in include/ee/EEDatabaseTables.h',
   '`u32 count` + count x 28-byte `EEWorldTableEntry` (no name column).',
   'There are 95 constants, read as `gWorldTable.mEntries[k].field` from about 65 sites. Examples: entry 42 float = game speed, entry 26 = lobby unit limit, 0x56 = civ-powers category cost, 0x59 = damage scale factors. No index has a source name. A converter should add a hand-maintained `name` column; per-index meanings have to be collected from the call sites.',
   'Indexed by position from code.',
   'CSV'),
 'dbfamily': ('Damage table (families x attack kinds)', f'`EETerrainTypeTable::LoadDamageTables` 0x4dbf11 ({EE}Database/EETerrainTypeTable.cpp); lookup `EEToHitTable::GetAttackFactor` 0x4dbe44',
   '`u32 count` (<= 60) + count x 380-byte `EEDamageTableRecord`: name, dbId, index, then `s32 damage[68]`.',
   'The values are percentages (x 0.01) into `table[family][kind]`. Two scaled copies are made with world constant 0x59; which copy applies depends on difficulty and human/AI. The row is the unit\'s `mFamily`; the column is the attack kind. Which unit field picks the column is not traced (probably `mWeapon` or the projectile class).',
   'Rows <- dbobjects `mFamily`, dbunitset `mFamilies[]`.',
   'CSV (matrix)'),
 'dbbuttons': ('Buttons (UI icon cells)', f'`EEButtonObject::Load` 0x4d5500 ({EE}Database/EEButtonObject.cpp)',
   '`u32 count` + count x 224-byte `EEButtonRecord`: name[100], texture file[0x68], id, row, column, position, hot key.',
   'Every row becomes an object (holes included). The texture is loaded on demand; ids below 50 get alpha. `mRow`/`mColumn` are the cell in a sheet (nearly always 0).',
   '`mHotKey` -> dbuihotkey index; referenced by dbobjects/dbtechtree `mButton`, dbeffects `mButton`.',
   'CSV'),
 'dbtechtree': ('Tech tree (epochs and nodes)', f'`EETechTree::Load` 0x5cbdf0 -> `EETechTreeEpoch::Load` 0x5ce10e -> `EETechTreeNode::Load` 0x5cf370 ({EE}Tech Tree/); record in include/ee/EETechTreeRecord.h',
   'No global count. **15 epoch blocks**, each: `u32 nodeCount`, the epoch\'s own 268-byte record, `u32 n` + n x `u32` parent unit types, then nodeCount x (268-byte record + `u32 n` + n x `u32`). In AoC the epochs have 929/55/201/111/202/901/103/93/95/205/921/113/210/301/39 node slots (4,494 records in all). Only 793 are real: each epoch is a dense id range (e.g. epoch 0 ids 1004..1928), and the empty ids are 0xCC hole records with id 0.',
   'Nodes with `mID <= 15` (`kTechEpochAll`) are dropped. Node epoch = id / 1000 - 1. The trailing list is the unit types offering the node (the converter log calls them "Parents"); costs are scaled per parent by `mFactor1..4` (parents 2..5). The `mCost2..6` names follow the resource slots: Food=mCost2, Wood=mCost3, Stone=mCost4, Gold=mCost5, Iron=mCost6. The 0xCC words at `mUD4..mUE0`, `mUF8`, `mFirstNodeID` and in `mBB6/mBB7` appear in hole rows only.',
   '`mResearchedEvent`/`mObsoleteEvent` -> dbevents; `mButton` -> dbbuttons; `mUnitClass`, `mDefaultUnitClass`, parent lists -> unit types; `mPrerequisite1..4` -> node ids; `mCalamity` -> dbcalamity; `mHotKey` -> dbuihotkey; `mNameID` -> Language.dll. Referenced by dbobjects `mAttributes[]`, dbeffects `mTechnology`, dbcivpowers `mTechnology`.',
   'YAML'),
 'dbevents': ('Events (tech/effect triggers)', f'`EEEvent::LoadEvents` 0x5ca63b ({EE}Scenario Manager/EEEvent.cpp)',
   'No count: **exactly 1500 rows**, each `char name[100]`, `u32 id`, `u32 numEffects`, numEffects x `s32 effect index`. (The first dword is the name\'s first bytes, not a count.)',
   'Event i is `EEEvent::sEvents[i]`. Triggers run the listed effects (`gEventEffects[idx]`). The names are never used by the game.',
   'effect indices -> dbeffects row; event ids <- dbtechtree `mResearchedEvent`/`mObsoleteEvent`, scenario triggers.',
   'YAML'),
 'dbeffects': ('Effects (what an event does)', 'same `EEEvent::LoadEvents` (one `Read` of `sizeof(gEventEffects)`)',
   'No count: a raw array of **exactly 2000** 88-byte `EEEventEffect` (176,000 bytes).',
   '1193 real effects; 807 holes (`mKind = -1`, 0xCC in padding and `mL54`). `mKind` selects the action in `EEEvent::RunEffect` 0x5ca7cf: 1 button, 2 area effect, 6 model, 7, 8, 9 tech, 10, 11, 12..14 tech-tree background, 15..18/20/21 sounds, 19 unit, 22. The value is the absolute value when offset and factor are 0. `mRunOnLoad` = run while loading a save.',
   '`mUnitClass`, `mNewUnitClass` -> unit types; `mUnitSet` -> dbunitset; `mMediaIndex` -> dbgraphics; `mTechnology` -> dbtechtree; `mSoundID` -> dbsounds; `mButton` -> dbbuttons; `mAreaEffect` -> dbareaeffect; `mAttribute` -> attribute id (0x28 = area effect).',
   'YAML (with events) or CSV'),
 'dbanimals': ('Animals (herd AI)', f'`EEAnimal::Load` 0x4d4a5d ({EE}Database/EEAnimal.cpp)',
   '`u32 count` + count x 272-byte `EEAnimalRecord`.',
   'Every row is loaded. Intervals are in seconds (x 1000 at load). +0xf4..+0x10f is zero in every row.',
   '`mUnitType` -> unit type; `mMedia` -> dbgraphics; `mSound1..3` -> dbsounds.',
   'CSV'),
 'dbcolortable': ('Player colours', f'`EEColorTable::Load` 0x4d5d91 ({EE}Database/EEColorTable.cpp)',
   '`u32 count` + count x 168-byte `EEColorTableRecord`: name, dbId, index, three RGB float triples, 24 zero bytes.',
   'Colour 2 also yields a 60% dark shade. "Color 3" is missing between colour 2 and colour 4; the gap at +0x90 (zero) may be it, or a 5th/6th colour.',
   'Index = colour slot (players, "Nocolor" 68).',
   'CSV'),
 'dbaiunittargeting': ('AI unit targeting weights', f'`EEAIUnitTargetingTable::Load` 0x4d47a1 ({EE}Database/EEAIUnitTargetingTable.cpp)',
   '`u32 count` (**must be 3**) + 3 x 160-byte `EEAIUnitTargetingRecord`.',
   'Rows: Melee, Short Range, Long Range. Each yields 10 weights. `mL90..mL98` are not kept.',
   'none',
   'CSV'),
 'dbterrain': ('Terrains', f'`EETerrainTable::Load` 0x4db2ce ({EE}Database/EETerrainTable.cpp)',
   '`u32 count`; per entry: one string (texture base, e.g. `textures\\BaseGrass_1.sst`, no NUL) + 204-byte `EETerrainRecord`.',
   'Texture ids are numbered cumulatively (`columns x rows` per terrain). The first named terrain with `mID != -1` becomes the default terrain. Colour is 0..255 per channel (x 1/255). `mFirst[k]..mLast[k]` (k < 3) are inclusive unit-type ranges of ambient units.',
   '`mTerrainType` -> dbterraintype; `mFirst/mLast` -> unit types; `mColorType` -> terrain colour palette; `mNameID` -> Language.dll; referenced by dbcliffterrain.',
   'CSV'),
 'dbterraintype': ('Terrain types', f'`EETerrainTypeTable::Load` 0x4dbd2e ({EE}Database/EETerrainTypeTable.cpp)',
   '`u32 count` + count x 128-byte `EETerrainTypeRecord`.',
   'Every row is loaded. `mL78` is often 999999999.',
   '`mFirstColor..mLastColor` -> entries of the 256-colour `textures\\ColorPalette.tga` palette; `mNameID` -> Language.dll.',
   'CSV'),
 'dbcliffterrain': ('Cliff terrains', f'`EECliffTerrainTable::Load` 0x4dbb22 ({EE}Database/EETerrainTable.cpp)',
   '`u32 count` + count x 124-byte `EECliffTerrainRecord`.', 'Rows with a negative index stay NULL. The first valid row is `gFirstCliffTerrain`.',
   '`mFirstTerrain..mLastTerrain` -> dbterrain ids.', 'CSV'),
 'dbcivpowers': ('Civilization powers', f'`EECivPowersManager::Load` 0x683124 ({EE}Civilization Builder/EECivPowersManager.cpp)',
   '`u32 count` + count x 152-byte `EECivPower`.', 'Power ids start at 0x26 (`GetPower(id)` indexes id - 0x26). Each power\'s bonus becomes a level of the synthetic "special powers" category in dbcivilization.',
   '`mTechnology` -> dbtechtree; `mUnitSet` -> dbunitset; `mUnitType` -> unit type.', 'CSV'),
 'dbcivilization': ('Civilization builder categories', f'`EECivilizationManager::Load` 0x681514 ({EE}Civilization Builder/EECivilizationManager.cpp)',
   '`u32 count` + count x 600-byte `EECivilizationCategoryEntry`: dbId, index, name[100] (at +0x08), name id, cost, unit set, then 60 x (f32 value, f32 bonus).',
   'Rows with `index <= 0` are skipped. A 36th "special powers" category is synthesised from dbcivpowers. Upgrade ids are `(category << 6) / level` (level 1..60). The hole rows hold 0xCC level pairs.',
   '`mUnitSet` -> dbunitset; `mNameID` -> Language.dll; `.civ` files store upgrade ids into this table.', 'YAML or CSV (long format)'),
 'dbpremadecivs': ('Premade civilizations', f'`EECivilizationManager::LoadPremadeCivilizations` 0x681672',
   '`u32 count` + count x 120-byte `EECivilizationEntry`: dbId, index, file name[100] (at +0x08), checksum, name id, era name id.',
   'Sorted by index after loading; the distinct era ids form the era list. `mChecksum` identifies an unmodified premade `.civ`.',
   '`mFileName` -> `civilizations\\<name>.civ`; `mNameID`, `mEraNameID` -> Language.dll.', 'CSV'),
 'dbweapontohit': ('Weapon to-hit matrix', f'`EEToHitTable::Load` 0x4dcaad ({EE}Database/EEUnitBehavior.cpp)',
   '`u32 count` (**must be 6**) + 6 x 132-byte `EEToHitRecord`: name, dbId, index, `s32 percent[6]`.',
   'Rows and columns: shock, arrow, pierce, gun, laser, missile (row 0 is named "Defender: Shock"). Percent x 0.01 into `gWeaponToHitFactors[6][6]`.', 'none', 'CSV (matrix)'),
 'dbcalamity': ('Calamities / special powers', f'`EECalamityManager::LoadCalamityClasses` 0x4ae070 ({EE}Calamities/EECalamityManager.cpp); record include/ee/EECalamityClass.h',
   '`u32 count` + count x 236-byte `EECalamityData`.',
   'Here dbId (+0x7c) and index (+0x80, `mID`) follow the name and six floats. `mClassEnum` (kCalamity*: 3 earthquake, 4 hurricane, 7 plague, 8 leprosy, 9 volcano, 10 fire, 13 morale, 14 time warp, 15 teleport, 16 area effect, 17 AE extra damage, 18 special attack, 19 mind control, 20 conscription, 21 logic bomb, 22 drop object, 23 cloaking, 24 meteor storm, 25 line of sight, 26 special location) selects the C++ class. The parameter slots mean different things per class.',
   '`mGraphicEffectID1..4` -> dbgfxeffects; `mSoundID`, `mSecondarySoundID` -> dbsounds; `mUnitSet` -> dbunitset; `mUnitClassIndex` -> unit type; `mNameID` -> Language.dll; <- dbtechtree `mCalamity`.', 'CSV'),
 'dbunitset': ('Unit sets', f'`EEUnitSetManager::Load` 0x536fa9 ({EE}Empire Earth/EEUnitSetManager.cpp)',
   '`u32 count` + count x 184-byte `EEUnitSetRecord`.', 'Rows with `mID <= 0` are NULL. A set is the union of 8 families and 8 unit types, plus an included set and an excluded set (row indices).',
   '`mFamilies[]` -> dbfamily rows; `mUnitTypes[]` -> unit types; `mIncludedSet`/`mExcludedSet` -> dbunitset; referenced by dbcalamity, dbareaeffect, dbcivilization, dbcivpowers, dbgamevariant, dbeffects, dbobjects.', 'CSV (list columns) or YAML'),
 'dbareaeffect': ('Area effect templates', f'`EEAEManager::Load` 0x67b079 ({EE}Civilization Builder/EEAEManager.cpp); record include/ee/EEAreaEffectTemplate.h',
   '`u32 count` + count x 188-byte `EEAreaEffectData`.', 'Rows with `mID > 0` become templates; `mKind` picks the area-effect class (economy, healing, morale, cloaking...).',
   '`mGraphicEffect`, `mGraphicEffect18` -> dbgfxeffects; `mUnitSet` -> dbunitset; `mMedia` -> dbgraphics; <- dbobjects `mAreaEffectTemplate0..3`, dbeffects `mAreaEffect`.', 'CSV'),
 'dbupgrade': ('Unit upgrade (improvement) tables', f'`EEUCUpgradeTable::Load` 0x4dc6f0 ({EE}Database/EEUCUpgradeTable.cpp)',
   '`u32 count` + count x 240-byte `EEUCUpgradeRecord`: name[100], 14 step floats, 15 cost floats, max level, ?, index, ?, ?, repair flag.',
   'This table puts its index late, at +0xe0, after the data. +0xdc (`mLDC`) looks like the db id. Factor 13 is never filled (a source slip kept by the port).', '<- dbobjects `mUpgradeTableIndex`.', 'CSV'),
 'dbcpbehavior': ('Computer-player behaviour values', f'`EECPBehavior::Load` 0x4d5f97 ({EE}Database/EECPBehaviorTable.cpp); record include/ee/EECPBehavior.h',
   '`u32 count` + count x 56-byte `EECPBehavior` (**no name column**; +0x00 = dbId, +0x04 = slot index).',
   'Stored into `gCPBehaviors[index]` (129 slots). The names come from Language.rc 28000+index and from the `EECPBehaviorID` enum (114 named ids). Slot 48 (maximum attack response range) is squared after loading.', 'none', 'CSV'),
 'dbgamevariant': ('Game variants', f'`EEGameVariant::Load` 0x4d693c ({EE}Database/EEGameVariant.cpp); record include/ee/EEGameVariant.h',
   '`u32 count` + count x 276-byte `EEGameVariantData`.', 'Standard / Tournament / Scenarios: 10 (unit set, factor) pairs plus cost and work-time factors.', '`mSet0..9`, `mCostSet` -> dbunitset; `mNameID` -> Language.dll.', 'CSV'),
 'dbstartingresources': ('Starting resources presets', f'`EEStartingResources::Load` 0x4daa05 ({EE}Database/EEStartingResources.cpp)',
   '`u32 count` + count x 132-byte `EEStartingResourcesRecord`.', 'Rows with index < 0 are skipped.', '`mNameID` -> Language.dll.', 'CSV'),
 'dbunitbehavior': ('Unit stances', f'`EEUnitBehavior::Load` 0x4dc973 ({EE}Database/EEUnitBehavior.cpp)',
   '`u32 count` + count x 124-byte `EEUnitBehaviorRecord`.', 'Scout, Stand Ground, Guard, Harass, Cautious, Aggressive... Seven permission bools.', '<- dbobjects `mBehavior`.', 'CSV'),
 'dbambientsounds': ('Ambient sounds', f'`EEAmbientSoundManager::Load` 0x525012 ({EE}Empire Earth/EEAmbientSoundManager.cpp); record include/ee/EEAmbientSound.h',
   '`u32 count` + count x 152-byte `EEAmbientSoundRecord`.', 'Each record is stored at `mSounds[mIndex]`. Index < 0 is skipped. Slots 7 (wind) and 9 (birds) are used by fixed index.', '`mSoundID` -> dbsounds.', 'CSV'),
 'dbuifonts': ('UI fonts', f'`UIFontTable::Create` 0x1007011f ({LLE}User Interface/UIFontTable.cpp); loaded first by `UIManager::LoadDatabase` 0x1007a78a',
   '`u32 count` + count x 128-byte `UIDbDataFonts`.', 'A row with an empty name is NULL. The face name comes from the UI string table (`mStringID`), not from the record name.', '<- dbuicontrols `mFontTableIndex`.', 'CSV'),
 'dbuiforms': ('UI forms (screens)', f'`UIForm::Create` 0x100716c0 ({LLE}User Interface/UIForm.cpp); record include/lle/UIForm.h (exported type `UIDbDataForms`)',
   '`u32 count` + count x 268-byte `UIDbDataForms`.', 'Here the form name and music file come first and the ids follow at +0xc8/+0xcc. `mFormID <= 0` means no form.', '<- dbuicontrols/dbuiback `mFormID`, dbuiformevents `mOwnerID`.', 'CSV'),
 'dbuiback': ('UI backgrounds', f'`UIBackgroundTable::Create` 0x1005c631 ({LLE}User Interface/UIBackgroundTable.cpp); record include/lle/UIBackgroundTable.h',
   '`u32 count` + count x 308-byte `UIDbDataBackgrounds`.', 'An empty name leaves the slot NULL (75 holes). The tile percentages hold `inf` in 39 rows.', '`mFormID` -> dbuiforms; texture file -> textures.', 'CSV'),
 'dbuicontrols': ('UI controls', f'`UIControlsTable::Create` 0x10067ee6, record ctor 0x1006796f ({LLE}User Interface/UIControlsTable.cpp)',
   '`u32 count` + count x 860-byte `UIDbControl`.', 'This is the largest file (2.9 MB, 60% holes). A row with `mID >= 0` is added to its form (`GetFormByID(mFormID)`). `mControlType` selects the control class (bitmap, button, list box, slider...). There are three rectangles each for the normal, pressed and focused texture. No static_assert pins it in this tree, but the parse confirms 860 bytes (0x35c, with one tail pad byte).',
   '`mFormID` -> dbuiforms; `mFontTableIndex` -> dbuifonts; `mStringTableBegin/End/ID` -> UI string table (Language.dll); file names -> textures, sounds; <- dbuicontrolevents `mOwnerID` (row index).', 'CSV (wide) or YAML by form'),
 'dbuicontrolevents': ('UI control events', f'`UIEvent::CreateControlEvents` 0x1006f398 ({LLE}User Interface/UIEvent.cpp)',
   '`u32 count` + count x 116-byte `UIDbDataEvents`: name[100], dbId, event id, owner, event type.', 'Rows with event id < 0 are skipped.', '`mOwnerID` -> dbuicontrols row index; `mEventID` -> the exe\'s event classes (EEUIManager switch).', 'CSV'),
 'dbuiformevents': ('UI form events', f'`UIEvent::CreateFormEvents` 0x1006f42a', 'Same layout as dbuicontrolevents.', 'Rows with event id < 0 are skipped.', '`mOwnerID` -> form id (dbuiforms `mFormID`).', 'CSV'),
 'dbuihotkey': ('UI hot keys', f'`UIHotKey::CreateHotKeys` 0x10076fa8 ({LLE}User Interface/UIHotKey.cpp)',
   '`u32 count` + count x 124-byte `UIDbDataHotKeys`: dbId, index, name[100] at +0x08, 3 modifier DIK codes (0x7fff = none), key DIK code.', 'Index < 0 = NULL.', '<- dbbuttons/dbtechtree `mHotKey`.', 'CSV'),
}

ORDER = ['dbobjects', 'dbgraphics', 'dbsounds', 'dbgfxeffects', 'dbmusic', 'dbtechtree', 'dbevents', 'dbeffects', 'dbbuttons',
         'dbfamily', 'dbweapontohit', 'dbworld', 'dbunitbehavior', 'dbstartingresources', 'dbanimals', 'dbcolortable',
         'dbaiunittargeting', 'dbterrain', 'dbterraintype', 'dbcliffterrain', 'dbcivpowers', 'dbcivilization',
         'dbpremadecivs', 'dbcalamity', 'dbunitset', 'dbareaeffect', 'dbupgrade', 'dbcpbehavior', 'dbgamevariant',
         'dbambientsounds', 'dbuifonts', 'dbuiforms', 'dbuiback', 'dbuicontrols', 'dbuicontrolevents', 'dbuiformevents',
         'dbuihotkey']

# dbevents has no struct in tables.json; parse it here
DBDIR = os.path.join(HERE, '..', 'aoc', 'db')
ev = open(os.path.join(DBDIR, 'dbevents.dat'), 'rb').read()
p = 0; nev = 0; neff = 0; mx = 0
for i in range(1500):
    name = ev[p:p + 100]; p += 100
    eid, n = struct.unpack_from('<II', ev, p); p += 8 + 4 * n
    if name.split(b'\0')[0]:
        nev += 1
    neff += n; mx = max(mx, n)
assert p == len(ev)
T['dbevents'] = dict(struct='inline (char[100], u32 id, u32 n, s32[n])', size='var', records=1500, filesize=len(ev),
                     classes={'named': 100}, rows=[
    '| 0x000 | char[100] | `name` | named | event name (unused by the game) | ' + f'{nev}/1500 non-empty |',
    '| 0x064 | u32 | `mID` | named | stored in EEEvent::mID | |',
    '| 0x068 | u32 | `mNumEffects` | named | count of the list that follows | ' + f'0..{mx}, {neff} effect refs in all |',
    '| 0x06c | s32[n] | `mEffects` | named | indices into dbeffects | |'])

sum_rows = ['| # | File (inside data.ssa) | Loader | Record type | Record bytes | AoC rows (real) | File bytes | named / inferred / provisional / unknown % | Suggested |',
            '|---|---|---|---|---|---|---|---|---|']
sections = []
holes = []
HC = {}
import sys as _s
_s.argv = [_s.argv[0]]
exec(open(os.path.join(HERE, 'analyze.py')).read().split(BS2 + 'def main')[0])
for _f, _sn, _k in TABLES:
    _d, _r, _e, _end = records(_f, _sn, _k)
    _c = collections.Counter(_r).most_common(1)[0][1]
    HC[_f] = _c if _c > 1 else 0
HC['dbevents'] = 1500 - nev
for n, f in enumerate(ORDER, 1):
    t = T[f]
    title, loader, cont, prose, xr, rec = P[f]
    c = t['classes']
    pct = f"{c.get('named', 0)} / {c.get('inferred', 0)} / {c.get('provisional', 0)} / {c.get('unknown', 0)}"
    short_loader = loader.split(' (')[0].split(';')[0]
    sum_rows.append(f"| 3.{n} | `db\\{f}.dat` | {short_loader} | `{t['struct'].split(' ')[0]}` | {t['size']} | {t['records']} ({t['records'] - HC.get(f, 0)}) | {t['filesize']:,} | {pct} | {rec} |")
    sec = [f"### 3.{n} `db\\{f}.dat`: {title}", '',
           f"* **Loader:** {loader}", f"* **Container:** {cont}",
           f"* **AoC data:** {t['records']} records, {t['filesize']:,} bytes unpacked. Decoded bytes per record: named {c.get('named', 0)}%, inferred {c.get('inferred', 0)}%, provisional {c.get('provisional', 0)}%, unknown {c.get('unknown', 0)}%.",
           f"* **Semantics:** {prose}", f"* **Cross-references:** {xr}", f"* **Suggested text form:** {rec}", '',
           '| Off | Type | Field | Class | Source comment | Observed (AoC) |', '|---|---|---|---|---|---|']
    sec += t['rows']
    sections.append('\n'.join(sec))

# holes table
import sys
sys.argv = [sys.argv[0]]
exec(open(os.path.join(HERE, 'analyze.py')).read().split('\ndef main')[0])
for f, sname, kind in TABLES:
    d, recs, extra, end = records(f, sname, kind)
    c = collections.Counter(recs)
    top, cnt = c.most_common(1)[0]
    if cnt > 1:
        holes.append(f"| {f} | {len(recs)} | {cnt} | one template + list of hole positions (or implied by gaps in `index`) |")
    else:
        holes.append(f"| {f} | {len(recs)} | 0 | rows as they are |")

# extra manual sections: random map, gray textures
rm = f"""### 3.38 `db\\dbrandommap.dat`: random map terrain table (mis-loaded, dead)

* **Loader:** `EERandomMapTable::Load` 0x4da47b ({EE}Database/EERandomMapTable.cpp). It reads `u32 count` and then
  count x 0x84 bytes into a local buffer that is **thrown away**. It creates `count` zeroed `EERandomMapTable`
  objects; nothing reads them.
* **Real layout (decoded from the data; cross-checked with the converter log's 29 "RandomMap" rows):**
  `u32 a = 10` (probably the number of groups; the highest group id is 10), `u32 n = 29`, then n x 132-byte records.
  The file is 3836 = 8 + 29 x 132 bytes. The game treats `a` as the count, so it reads 10 records that are shifted by
  4 bytes.
* **Suggested:** CSV (only for completeness; editing it changes nothing in game).

| Off | Type | Field | Class | Comment | Observed |
|---|---|---|---|---|---|
| 0x00 | s32 | `(db id)` | inferred | 4..71, unique | |
| 0x04 | s32 | `(group)` | inferred | 1..10; two or three rows per group | |
| 0x08 | char[100] | `(name)` | inferred | terrain name ("Grass Tufts", "Snow", "Ice", "StoneWork") | |
| 0x6c | s32 | `(min?)` | unknown | -10, -2, 0 | elevation-like |
| 0x70 | s32 | `(max?)` | unknown | -4, -3, -1, 20 | elevation-like |
| 0x74 | s32[4] | `(terrain ids?)` | unknown | four equal values per row (2..71: dbterrain ids?) | |

### 3.39 `db\\dbterraingraytextures.dat`: grey terrain textures (not loaded by AoC)

* **Loader:** none in the AoC exe (only the key `kDbTerrainGrayTexturesFile`). It is still in `data.ssa` and listed
  as "TerrainGrayTextures" in the converter log.
* **Container (decoded from the data, parses exactly to the end):** `u32 count = 59`; per entry: one string (`u32 len`
  + bytes, no NUL, e.g. `textures\\GWildGrass_1.sst`), then a 120-byte record.
* **Suggested:** CSV.

| Off | Type | Field | Class | Comment |
|---|---|---|---|---|
| 0x00 | char[100] | `(name)` | inferred | "Grass Rough (x)" ... "Space" |
| 0x64 | s32 | `(db id)` | inferred | 2..60 |
| 0x68 | s32 | `(index)` | inferred | -1 (row 0) .. 58 |
| 0x6c | s32[3] | `(rgb?)` | inferred | 0..255 triples (e.g. 130,190,50), like dbterrain's colour |
"""

openq = """1. **dbobjects provisional block.** What are +0x06c (`mL06C`, 30 values; -> `EEUnitClass::mL208`), +0x1a4, +0x1c4, +0x1ec, +0x21c/+0x220, +0x2c4, +0x310, +0x31c/+0x320, +0x32c, +0x354, +0x364 (holds float
   bit patterns, so probably a float) and +0x368? Is +0x3b4..+0x47b (200 bytes) a dead fixed buffer in the exporter (it
   looks like stale memory)? Tracing the `EEUnitClass` members (`mL208`, `mL294`, `mF18C`, `mB2F6`...) to their readers
   would name them.
2. **Animation state slots** of dbgraphics: the 14 indices have no enum. Their order in the record (0, 13, 7, 8, 4, 5,
   12, 11, 9, 10, 6, 3, 1, 2) suggests database column order. The names from model files are a guess.
3. **Attack-kind columns of dbfamily (68).** Which unit field chooses the column (`mWeapon`, `mProjectileType` or
   `mAmmoType`)? What is each column's name? The source DB had them as columns, but the export keeps only numbers.
4. **dbworld index names.** 95 constants have no names. About 65 call sites read specific indices. A name list has to
   be collected by hand from those sites.
5. **Effect kinds** in dbeffects: kinds 7, 8, 10, 11, 22 and the meaning of `mL00` (looks like a db id, 0..1809), `mB45`,
   `mU46`, `mL48..mL54` are not decoded.
6. **dbgfxeffects class parameters** (`mParam1..12`, `mL90..mL98`, the four bools): their meaning depends on `mKind`
   (60 kinds used). This is a per-class decode job in the `EEE*` effect classes.
7. **dbcalamity parameter slots** (`mIntParam1..6`, `mFloatParam1..4`, `mBCC`, `mBE8`): per-calamity meanings, partly
   given in the comments.
8. **dbrandommap**: the meaning of the last six columns and of the header dword `10`. Is the table dead in AoC because
   of the framing bug, or does `Default.dll` have another reader? (No reader was found in `src/`.)
9. **dbcolortable +0x90..+0xa7** (24 zero bytes): a missing colour 3 and a 5th colour? dbambientsounds +0x84..+0x8f
   and +0x94. dbunitbehavior +0x73.. These are zero everywhere, so they are harmless as defaults.
10. **dbtechtree `mU84`, `mU8C`, `mBB6/mBB7`, `mUD4..mUE0`, `mUF8`.** In real rows they are 0 except for two rows with
    `mU84 = 100000` and two with `mUD4/mUD8` = 2163200/4100, which look like stale data. Are they Access columns that
    were never used?
11. **Byte-exactness of `0xCC` and junk.** The text format should keep a row's tail and padding bytes so that the
    multiplayer checksum survives. Does the owner want that (*exact* mode), or only *clean* functional equivalence?
12. **Base-game format.** The base game's `dbobjects.dat` uses 1948-byte records. If base-game data is ever wanted,
    its layout has to be derived separately (the base exe is out of scope).
"""

md = tpl.replace('{{SUMMARY}}', '\n'.join(sum_rows) + "\n\nCounts are AoC retail (`data.ssa` Timestamp 4035612). The % columns are bytes per record by field class (section header). For dbevents the % is per row structure. \"real\" = rows that are not hole/filler rows.")
md = md.replace('{{HOLES}}', '\n'.join(holes))
md = md.replace('{{TABLES}}', '\n\n'.join(sections) + '\n\n' + rm)
md = md.replace('{{OPEN}}', openq)
open(OUT, 'w', encoding='utf-8').write(md)
print('wrote', OUT, len(md))
