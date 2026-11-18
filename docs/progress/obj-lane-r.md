# Lane R: the renderer's fill and the effect host over ee_sim::world's views

The object plan's Lane R (scratchpad `object_model_plan.md`): move `EERenderObject.cpp`'s fill (`IsUnitDrawn`,
`FillObject`, `FillUnit`, `FillRenderLists`' tile walk, `GetUnitOpacity`, the ghost and doppelganger records) to
`ee_render::gfx` over Lane V's views, keep a host of about 250 lines (`LoadMedia`, the picker's `AddUnit`, the ambient
counters, the render-list and texture-map inserts, `DrawGroundCircle`), and move `EEEffectHost.cpp`'s `Ask` readers
to the views. Lane V's `ee_sim::world` merged (origin/rust f63bd44ba); round 2 is the move.

## State (branch rust-objR; round 2, the move, on origin/rust f63bd44ba with Lane V's views)

| Commit | What | C++ (src, include/ee) |
|---|---|---|
| 4bc91446, a90033b6, e2489fb6 | round 1: RFIL (TEST ONLY), the fill golden, the split of `FillUnit`/`FillObject` into calls and fields | +432 test code; split net +27 |
| merge of origin/rust f63bd44ba | Lane V's `ee_sim::world` | |
| f2e5e267 | `FillUnitFields` and `FillObjectFields` are `ee_gfx fill.rs` over the views; `EERenderUnit`'s head and `EERenderShadow` mirrored; the players' `mViewedUnit`/`mSelectedViewedUnit` in `EeWorldLayout`; `golden_fill_replay.rs` | +317 -314, net +3 |
| 29cec903 | sixteen `Ask` readers of `EEEffectHost.cpp` are `ee_gfx effect_views.rs` (the hosts carry the unit and the current player) | +24 -97, net -73 |
| 1147a921 | `IsUnitDrawn`'s facts and `GetUnitOpacity` over the views | +56 -75, net -19 |

**Not reached: the plan's -850 to -1,050.** The round's code commits are net -89 C++. The views made every plain
read free, but most of what the fill and the effect host do is not a plain read of a record: the graphic objects
(`mModels`, `mTextures`, `mTurretParts`, `LoadMedia`), the physical model, virtual class members (`GetModel`,
`IsLandBuilding`, `GroundLocation`, `IsSidePart`, `UpdateHidden`), the C++ vectors and names of `EERenderUnit`, the
render lists, the picker, the upgrades table and `FindWaypointAtTile`. Each became a host entry of three to ten
lines, so `FillUnitFields` (210 lines) became 26 host functions (about 190 lines). Only the `Ask` readers, whose
answers were wholly record fields, freed real C++. What would free more: views of `EEGraphicObject` and
`GEPhysicalModel` (a render-side layout like `EeWorldLayout`: the models, textures, turret parts, frame counts, the
physical model's scale and frame), Rust ports of the class virtuals' bodies (`IsLandBuilding` is a body table
already in `ee_sim::units::cls_dispatch`), and moving `EERenderUnit`'s vectors into a Rust record. Then the host
shrinks to the media, the picker and the lists, and the tile walk, the doppelgangers and the ghost records can
follow without a host entry per read.

**Left:** the tile walk of `FillRenderLists`, `FillDoppelgangerUnit`/`FillDoppelgangerObject` (they read
`EEDoppelganger`, which has no view), `FillGhostRecords` (`EEGhostUnit`, no view), the `Ask` readers that call into
C++ objects (tag points, models, media, the effect manager, the doppelganger manager).

### How each move is checked

- The stub-GE driver (`build/gxR/drv/`) now holds the Rust records as the game's classes do (`rust/sim/world.h`:
  units, classes, stats, players, the map, its tiles with their visibility words and unit nodes) and registers its own
  `EeWorldLayout`, so the moved Rust reads the stand-in world through the same views the game uses. Its stubs answer
  as the game does where the Rust re-implements a C++ member (`IsPathMarker` by class type, `IsSeenBy` /
  `IsVisibleTo` through the visibility words, `Fn27C0B4` `return false`, the mesh's rows on the map). `run.sh
  4aeedb1a1 o2` (the unmodified fill) and `runw.sh` (the working tree with its helpers, `helpers.py`): seed 1 (300
  frames) and seed 2 (3,000 frames) byte-identical after every step; mutations of the Rust (a fog scale, the
  see-through opacity, the size jitter, a circle's flag) make them differ.
- `golden_fill_replay.rs` rebuilds each of the golden's 40 frames as fake objects at a test layout and replays
  `fill_unit_fields` and `fill_object_fields` for every render unit and object the C++ filled, each from its pool
  slot's previous dump: every head member bit for bit, the bars, the name, the selected and pointed lists and the
  label. 
- RFIL: the tours of the moved exe against the unmodified exe's two (`a.bin`, `b.bin`): all matched frames equal.
- The oracle at seeds 2-3; the effects capture at seeds 2-7 for the `Ask` readers (EE-SIM steps every effect kind).

## The gates

- **RFIL, the game.** `EE_RENDER_CAPTURE=<absolute file>` makes every `FillRenderLists` that fills append one record:
  the keys (frame counter, physics tick, sway clock, camera transformation and yaw, the free-camera, orthographic and
  ambient flags, the unit under the pointer, and a digest of the visible tiles' units that the harness's input can
  change: hit points, resources, a complex unit's location, frame, animation, command, build state and selection),
  then a digest of every field of every `EERenderUnit` and `EERenderShadow` (pointers as their content's name and
  their order of first use, never their address), the six texture maps (keys named by their first entry's pool index:
  the maps order by texture address), the selected and picked lists, the ghost records (the fields
  `FillGhostRecords` sets: the record is a local whose other scalars are indeterminate), the ground circles and the
  fighting and unit counts. The live game draws on wall time, so `tools/rfil.py compare A B` matches frames by their
  keys and requires the matched ones equal; the objects' sway fractions run on wall time and are compared only with
  `--sway`.
  - Recording: `build/gxR/tour.sh TAG EXE` (scratch): the fixture tour through `tools/harness/slot.py`, seed pinned,
    `--pt-rate 40`, `--time-factor 1/20` (about one frame per physics tick on the software renderer, so two runs share
    many ticks), `--play-seconds 120`, `--zoom-captures`. The box selection's "dead" step fails at that clock (the
    disbanded unit does not finish dying within its wait), so the RFIL tours run without `--box-select`.
  - The base: see "Results" below.
- **RFIL, the stub-GE driver** (`build/gxR/drv/`, scratch; `make_rfil.py COMMIT|WORKTREE DIR [helper signatures]`,
  `run.sh`): the fill's functions cut from a commit or the working tree, compiled over a stand-in world (24 classes
  with every special unit type the fill tests, 4 players with upgrades, enemies and civilisations, 10 graphic objects
  with 14 models and textures and turret parts, a 64 x 64 map with a plane terrain and water tiles, up to 36 units a
  frame on the visible rows with complex units, three-part gates, cargo, area effects, path markers and their
  carriers' waypoints, remembered units on unseen tiles, ghost units) and a stub GE that prints every call the fill
  makes (`LoadMedia`/`UnloadMedia`, the picker's `AddUnit` with the transformation's hash and the opacity, the
  view tests and their answers, the ambient counts, `UpdateHidden`, `Fn23CEE2`, the waypoint lookups, the music
  intensity, the locks), then each frame's every render unit, object, map, list, ghost record and circle in full.
  Seed 1, 300 frames (3.86 MB) and seed 2, 3,000 frames (34 MB, 21,344 render units, 10,882 objects, 1,512 ghost
  records, 125 circles): two runs identical; WORKTREE (with the capture line) identical to 4aeedb1a1.
  `golden_fill.txt` holds seed 1's first 40 frames (the W/T/D lines are the frame's world: a Rust test rebuilds it).

## What the fill and the Ask readers read

Every read, by the object it reads through. "call" marks a read that is a C++ call doing more than a field load;
"virtual" a virtual call. V's views must answer each plain field; the calls either become view helpers (Rust over
the same fields) or stay host calls.

### EEUnit (any unit; base fields)

| Field | Readers |
|---|---|
| `mUnitID` (`GetUnitID`) | IsUnitDrawn, FillObject, FillUnit, FillDoppelgangerObject (doppelganger's unit), the tile walk (`FillRenderLists`' robotic-farm check via `gWorld.GetUnit`), Ask: ConfirmUnit, TracerTargetID, MarchUnitID |
| `mUnitClass` (`GetUnitClass`, `GetClass`) | all |
| `mStats` (`GetStats`) | IsUnitDrawn, FillObject, FillUnit (also the cargo's), GetUnitOpacity, Ask: UnitScale, VentUnit, SmokeUnit, StatsScale, MaxHitPoints, GlowPlace, TracerEnds (`mStats->mModel->mDefaultPoint`) |
| `mPlayer` (`GetPlayer`) | IsUnitDrawn, FillUnit, Ask: BeamHidden, UnitHidden, WallPiece, PlayerColor, ConfirmEnemy, ConfirmGoal, IconUnit, IconsSelected, IconsOwner, GlowPlace |
| `mComplexUnit` (`GetComplexUnit`) | all; the tile walk follows it to the first part (`mPreviousPart`) and on (`mNextPart`) |
| `mTile` | FillUnit (`IsSeenBy`, `GetHeight`), Ask: FarmDoppelgangerAny, FarmDoppelganger |
| `mTileX`, `mTileY` (`GetTileX/Y`) | IsUnitDrawn, FillObject, FillUnit, GetUnitOpacity, Ask: UnitTile, ConfirmUnit, FarmUnit, EmitterPlace, GlowArea |
| `mTileNode` | the tile walk (a doppelganger's unit on the map), Ask: UnitTile, UnitRaisedPosition |
| `mResourceType`, `mResourceAmount` | IsUnitDrawn, FillUnit (the owned food) |
| `mHitPoints` (`GetHitPoints`) | FillObject, FillUnit (also a path marker's carrier id), the tile walk (the picked unit; the robotic farm), Ask: SmokeUnit, EmitterUnit, UnitRaisedPosition, ConfirmUnit |
| `mIsEditorSelected` | FillObject, FillUnit |
| `mHiddenFlags` | GetUnitOpacity |
| `IsPathMarker()` (call: the class's type is 0x104 or 0xdf) | IsUnitDrawn, FillUnit |
| `Fn27C0B4()` (call: `return false`) | IsUnitDrawn |
| `GetModel()` (virtual: the stats' model; a foundation's is `EEGetPlayerClassGraphic`) | IsUnitDrawn, FillObject, FillUnit, GetUnitOpacity, most Ask readers |
| `UpdateHidden()` (virtual; **writes** the hidden flags and the neighbours' on a phase change) | GetUnitOpacity (show-hidden mode 1): stays a host call |
| `GetPosition()` (call) | Ask: UnitPosition, ConfirmUnit, UnitRaisedPosition, MarchEnds, WallGatePosts |

### EEComplexUnit

| Field | Readers |
|---|---|
| `mPhysicalModel`: `GetLocation()` (the transformation), `GetScale()`, `GetFrame()` | IsUnitDrawn (`Intersects`: a GE call), FillUnit, GetUnitOpacity, Ask: Follow, BeamStart, BeamUnit, UnitLocation, UnitFrame, SpawnAt, AnimationSound, TracerEnds, TracerImpact, VentPoints, SmokeUnit, EmitterPlace, EmitterUnit, SpawnPoint, TagPoint, IconUnit, PartFollow, GlowSetUp, GlowPlace, ConfirmUnit |
| `mUnitClass` (the part's class) | IsUnitDrawn, FillUnit |
| `mCommand->mType` (`GetCommandType`) | IsUnitDrawn, FillUnit, Ask: VentUnit, SmokeUnit, EmitterUnit |
| `IsFoundation()` (`mIsFoundation`) | IsUnitDrawn, FillUnit, the tile walk (a doppelganger's unit), Ask: WallPiece, MarketUnbuilt |
| `mIsTinted`, `mColor` | FillUnit |
| `GetOpenFraction()` (call) | FillUnit (gates), Ask: GateOpen |
| `mActiveGoal->mKind` | FillUnit (the battle music), Ask: ConfirmGoal |
| `mIsBuilt` | FillUnit, Ask: FarmUnit, MarketUnbuilt, IconsOwner, GlowPlace |
| `mTurretAngles[i]` | FillUnit, Ask: TracerEnds, EmitterUnit |
| `mNumOrbitals`, `mOrbitals[i]->mAngle/mModel` | FillUnit |
| `mNumWheels`, `mWheels[i]->mAngle/mModel` | FillUnit |
| `GetNumCargoUnits()`, `mCargo[j]` (each: `mHitPoints`, `mStats->mMaxHitPoints`) | FillUnit |
| `mPower` | FillUnit |
| `mStats->mName` | FillUnit (the picked unit's name) |
| `mCurrentResearch`, `mProductionTime` | FillUnit |
| `mNextFrame`, `mAnimationFraction` | FillUnit |
| `mAnimation` (`GetAnimation`) | FillUnit, GetUnitOpacity, Ask: Follow, BeamStart, BeamUnit, UnitFrame, TracerEnds, VentLists, SmokeUnit, EmitterUnit, SpawnPoint, TagCount, TagPoint, WeaponPoint, GlowSetUp, GlowPlace |
| `mAffectingAreaEffects.mItems[i]` (`->mTemplate->mKind`, `GetStrength()`) | FillUnit |
| `mPreviousPart`, `mNextPart` | the tile walk |
| `FindWaypointAtTile()` (call: the waypoint list) | FillUnit (a path marker's carrier) |
| `GetPlayer()->mEnemies[]` | FillUnit (the cargo bars) |
| `mIsCloakable`, `mIsCloaked` | Ask: BeamHidden, UnitHidden, GlowPlace |
| `mCloaked` (the farm's crop variant; **written** by FarmSetVariant) | Ask: FarmUnit |
| `mAreaEffects`, `mNumAreaEffects` (**written** by SetAreaEffects) | Ask: AreaEffect, AreaEffects |
| `mGoalQueues[i].mGoals` | Ask: ConfirmGoal |
| `GetEffectHeight()` (call) | Ask: UnitRaisedPosition |
| `mWeaponTagPoint` | EEEffect's set-up (not an Ask reader) |
| `Fn23CEE2(unit, ...)` (call: the unit's own translucency) | GetUnitOpacity, Ask: FarmOpacity |

### EEUnitClass

| Field | Readers |
|---|---|
| `mCategory` | IsUnitDrawn, FillUnit, FillDoppelgangerUnit, FillGhostRecords, Ask: WallGatePosts, GateOpen |
| `mTileSize` (`GetTileSize`) | IsUnitDrawn, FillObject, FillUnit, the doppelgangers, the ghosts, the tile walk, Ask: UnitTile, ConfirmUnit, FarmUnit, EmitterPlace, IconUnit |
| `mUnitType` (`GetUnitType`) | IsUnitDrawn, FillUnit, the doppelgangers, the ghosts (`EEGhostUnit::GetModel/GetScale` index the upgrades by it), Ask: WallPiece, MarketUnbuilt, IconsSelected |
| `mFamily` | FillUnit, FillDoppelgangerUnit |
| `mSelectionSize` | FillObject, FillUnit, Ask: SelectionSize |
| `mIsTree` | IsUnitDrawn, GetTileHasTree, the tile walk, Ask: ConfirmUnit |
| `mIsFlare` | IsUnitDrawn (the part's class) |
| `mIsObstacle` | GetUnitOpacity, the tile walk (a doppelganger's map) |
| `mFacesCamera` | FillUnit, FillDoppelgangerUnit |
| `mScale` | FillDoppelgangerObject, EEEffect's smoke set-up |
| `mScaffoldingClass` (its `GetUnitType`) | FillUnit, FillDoppelgangerUnit |
| `GetShadowScale()`, `GetF170()` (calls) | FillObject, FillUnit, the doppelgangers |
| `IsLandBuilding()` (call) | FillUnit, GetUnitOpacity, Ask: IconUnit, GlowPlace |
| `GroundLocation(point)` (virtual) | FillUnit (a placed object's point): stays a host call or becomes a per-kind view helper |
| `IsSidePart(part)` (gate kind: `EEUnitClassOfKind<EE_CLASS_KIND_GATE>`) | FillUnit, Ask: WallPiece |
| `GetPart(unit, i)` (gate kind) | Ask: WallGatePosts |
| `GetScale()` | Ask: UnitClassScale |
| `UnitDamaged()` (virtual; a write) | Ask: ShotHit, TracerHit: stays a host call |

### EEUnitClassUpgrade (a unit's stats, a player's upgrade)

| Field | Readers |
|---|---|
| `mScale` (`GetScale`) | IsUnitDrawn, FillObject, FillUnit, GetUnitOpacity, the ghosts, Ask: UnitScale, StatsScale, GlowPlace, VentPoints |
| `mMaxHitPoints` (`GetMaxHitPoints`) | FillObject, FillUnit (and the cargo's), the doppelgangers (the owner's upgrade), Ask: VentUnit, SmokeUnit, MaxHitPoints |
| `mPower` | FillUnit |
| `mRange` | FillUnit (the priest tower's circle) |
| `mModel` | `EEUnit::GetModel`, the ghosts, FillUnit and FillDoppelgangerUnit (the scaffolding's upgrade), Ask: TracerEnds |
| `mName` | FillUnit |

### EEPlayer

| Field | Readers |
|---|---|
| `mPlayerID` (`GetPlayerID`) | IsUnitDrawn, FillUnit, Ask: BeamHidden, UnitHidden, WallPiece, ConfirmEnemy, IconUnit, FarmDoppelganger |
| `mColor` | FillUnit, the doppelgangers, the ghosts, Ask: PlayerColor |
| `mEnemies[]` | FillUnit, Ask: BeamHidden, UnitHidden, ConfirmEnemy |
| `mSelectedGroup->Contains(unit)` (virtual) | FillUnit; RFIL's world key |
| `mActiveGroup->HasUnitType(type)` (call) | Ask: IconsSelected |
| `mViewedUnit` | FillObject |
| `mSelectedViewedUnit` | FillUnit |
| `IsHuman()`, `mCivilization.GetUnchanged()`, `mCivilization.mName`, `mName` | FillUnit (the picked enemy's label) |
| `mUnitClassUpgrades[type]`, `GetUpgrade(type)` | the doppelgangers, the ghosts, FillUnit (the scaffolding) |
| `mSeesAll` | Ask: FarmDoppelganger |

### EETile / RTSTile, EEMap, GETerrainMesh

| Field | Readers |
|---|---|
| `IsSeenBy(player)`, `IsVisibleTo(player)` (the visibility words) | IsUnitDrawn, FillUnit, the tile walk, Ask: ConfirmGoal |
| `mUnits` (the `RTSTileUnitNode` list: `mUnit`, `mNext`) | GetTileHasTree, the tile walk |
| `GetNumDoppelgangers()`, `mDoppelgangers[k]` | the tile walk |
| `GetHeight()` | FillUnit (a path marker under the sea level) |
| `EEMap::GetTile(x, y)` (NULL off the map) | FillObject, FillDoppelgangerObject (`GetTileHasTree` of two neighbours), the tile walk, Ask: ConfirmGoal, GroundAt |
| `EEMap::GetGroundPoint(point)` (call) | FillUnit, FillDoppelgangerUnit, Ask: GroundPoint, GroundAt |
| `EEMap::GetHeight(point)` (call) | Ask: EmitterPlace |
| `GETerrainMesh::GetMeshZ` (Rust: ee_ge tmobj) | IsUnitDrawn, FillObject, FillUnit, the doppelgangers, GetUnitOpacity, Ask: TerrainZ, the farm's terrain, EEEffect's set-ups |
| `GetIsWaterTile`, `GetIsValidTile`, `GetIsSpaceTile` | FillUnit, FillDoppelgangerUnit, Ask: SpaceTile, EmitterPlace |
| `GetNumVisibleTileRows`, `GetVisibleTileRow`, `GetTerrainVisibleBottomLeftIndices`, `GetTerrainVisibleTileDimensions` | the tile walk, Ask: GlowRows, GlowArea |
| `gWorld.IsTileBlocked(x, y)` | Ask: TileBlocked |
| `EEGetUnitsOnTile`, `EEGetFarmDropSites`, `EECanStandOn` (calls) | Ask: WallPiece, FarmDropSite, ConfirmGoal |

### The graphic object (EEModel = EEGraphicObject), GEModel, tag points

| Field | Readers |
|---|---|
| `mModels[i]` (GEModel: `GetFrameCount`, `GetCenterPoint`, `GetBoundingRadius`, `GetBoundingBox`, `GetTagPoint`, `GetTagPointName`) | IsUnitDrawn, FillObject, the doppelgangers, GetUnitOpacity, Ask: ModelHeight, ModelAt, UnitModelRadius, UnitModelAt, VentPoints, PartFollow, GlowTagName |
| `mTextures[i]`, `mShadowTexture` | FillObject, FillUnit, the doppelgangers (the map keys), Ask: Push, Texture, TextureAt, QueueScreen, PartTexture |
| `mTurretParts[i]->mModel` (and `.size()`) | FillUnit, Ask: TracerEnds, EmitterUnit (`FindTurretPart`) |
| `mModel`, `mTopPoint`, `mDefaultPoint` | Ask: Model, ConfirmUnit, DefaultPoint, TracerEnds, PartFollow |
| `mTagPoints[anim]`, `GetTagPoints(anim, kind)` (call) | Ask: Follow, BeamStart, SpawnCount, VentLists, TracerEnds, SmokeUnit, EmitterUnit, SpawnPoint, TagCount, TagPoint, WeaponPoint, GlowSetUp, GlowPlace |
| `mOrbitals`, `GetOrbital(i)` | Ask: OrbitalCount, Orbital |
| `mStartFrames`, `mFrameTimes`, `mFrameCounts`, `mLastFrames`, `mLoops` | Ask: Animation, Frames, PartFrames, FarmLoops |
| `LoadMedia()`, `UnloadMedia()` (calls; reference counts and file loads) | the fill (host), Ask: Media, UnitModelMedia, ReloadMedia, MarchMedia, OwnMedia: stay host calls |

### EEDoppelganger, EEGhostUnit (the remembered and the placed units)

| Field | Readers |
|---|---|
| doppelganger `mUnit`, `mPlayer`, `GetPlayer()`, `GetModel()`, `GetAnimation()`, `GetFrame()`, `GetHitPoints()`, `IsComplex()`, `GetTransformation()`, `GetScale()`, `IsFoundation()`, `IsBuilt()` | the tile walk, FillDoppelgangerUnit, FillDoppelgangerObject |
| `gDoppelgangerManager.HasDoppelganger/GetDoppelganger(tile, unit, player)` (calls) | Ask: FarmDoppelgangerAny, FarmDoppelganger |
| ghost `mTransformation`, `mHeights`, `mFootprint`, `mTileX`, `mTileY`, `mFrame`, `mAnimation`, `mPlayer`, `mUnitClass`, `mCanPlace`; `GetModel()`, `GetScale()` (the player's upgrade) | FillGhostRecords |
| `gWorld.mGhostUnits`, `GetNumGhostUnits()`, `mGhostUnitsLock` | FillGhostRecords, Ask: GhostUnits |

### The world and the frame (not views of an object)

`gWorld.mMap`, `GetCurrentPlayer()`, `mPlayers[]` (by the pool slot's stale `mPlayerID` in FillDoppelgangerUnit: keep
it), `GetUnit(id)` (the path marker's carrier; the robotic farm), `mTransformation` (the camera), `mInCinematic`,
`mWindDirection` (Ask: Wind); `gFreeCamera`, `gOrthographicView`, `gViewport`, `gMusicFollowsBattle`,
`gCollisionSphereMode`, `gRenderEnvironment` (`mCameraYaw`, `mSeaLevel`, `mShowHiddenUnits`), `gPhysics->mTicksRun`,
`gB529065`, `gSpaceTurretHeight`, `gSpaceStationHeight`, `gGhostUnitsChanged`, `gDrawBlood`, the UI's selection
rectangles (`EEUICSelectUnits`, the editor's and the trigger tab's), `gMouse->mPosition`, `gUnitPicker`
(`Contains`, `PickUnitUnreferenced`, `PickUnits`, `PickUnitsInRectangle`, `Reset`, `GetLock`, `AddUnit`),
`gAmbientSoundManager` (`mFrameCount`, `IsUpdateDue`, `Update`, `mAnalyzer.CountTree/CountUnit`),
`gMusicManager` (`mIntensity`, `SetIntensity`).

### Writes into the world the move must keep as calls

`UpdateHidden()` (GetUnitOpacity), `LoadMedia`/`UnloadMedia` (reference counts), the picker's entries, the ambient
counters, `gU4E9230` (the rectangle's picks), the music intensity; Ask: `SetTransformation`, `mGraphicObject`,
`mUnitModel`, `mSound`, `mCloaked` (FarmSetVariant), `mNumAreaEffects`/`FnE123D` (SetAreaEffects),
`SetScaledModel`, `mTarget`/`mFromUnit`/`mToUnit` (saved ids), the parts, the effect manager's creations,
`UnitDamaged`.

## Behaviour notes for the move (from the driver)

- `FillGhostRecords` fills a local `EERenderUnitRecord`: the fields it does not set (`mSelectionSize`, `mGroundZ`,
  `mUnitId`, ...) are indeterminate in the original; the driver and RFIL compare only the set fields.
- `FillDoppelgangerUnit` reads `outUnit->mPlayerID` before writing it (the scaffolding's upgrade): the pool slot's
  previous unit's player. A Rust fill must keep the pool's stale fields (the driver keeps the pool across frames).
- `GETransformation theTransformation(false)` locals are uninitialised; every path writes all 16 elements before use.
