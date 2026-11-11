# Rust: a research plan for an iterative rewrite

Status (2026-11-09): R0-R3, R4.1-R4.2 and R5b.1's formations built on the `rust` branch, except what the table below lists as open.
Revised for the WON library (module `won`, the upstream merge to 3f51317465d6) and its lobby on the platform layer. The goal is a game written wholly in Rust. The way there is one binary
in which C++ and Rust coexist for a long time. Each step moves one module across a seam, and the game keeps passing
its gates at every commit. This document covers:

- what the code base looks like from that angle (modules, coupling, the seams that already exist);
- the invariants a Rust replacement must keep;
- the existing work that makes the rewrite testable;
- an ordered plan and the decisions it needs from the owner.

Companion documents: PORT.md (the port, the platform layer, GX), docs/MODDING_PLAN.md (schemas, rules as data;
several items there are deferred "to the Rust rewrite"), docs/AI_RESEARCH.md (the AI and its scripting runtime: mlua is
the chosen Rust path), PORT_LEARNINGS.md.

## Progress (the `rust` branch, 2026-11-09)

| Step | State |
|---|---|
| R0 Rust library | Done |
| R1.1-R1.3 platform leaves, files, threads, sockets | Done (`ee_plat_core`, `ee_plat_sys`). Windows PlatString still calls the CRT, whose behaviour depends on the locale |
| R1.4-R1.5 window, fonts | Done (`ee_plat_ui`, `ee_plat_sdl`): SDL3 kept (D4), stb_truetype ported line by line, 51,520 glyphs byte-equal |
| R2.1-R2.3 rules schema, rustgen, text compiler | Done (`ee_data`) |
| R2.4 glTF and PNG | Done (`ee_openformats`), 7,150 of 7,150 gate cases equal to the C++ digests |
| D9 models and textures at run time | Done (branch `rust-d9-load`): glTF decoded straight into the model `GEModel::StoreLoaded` stores, PNG into the frames `GETexture::Load` stores; the `.cem` reader/writer, the `.sst`/`.tga` texture reading, `GETexture::Save` and the `.cem`/`.sst` writers deleted. Gates: `storegate.py` (the game's loaders over the whole open tree, 25,140 lines equal to the digests recorded from the pre-D9 loaders), `cppgate` (7,104 decoder cases equal to digests recorded with the old path checked alongside), the sim oracle |
| D9 open formats only, the data side | Done (branch `rust-d9-data`): the db tables are read only as text (no `.dat` fallback, the dead `dbrandommap.dat` load removed); the pointers, palette, volcano, scenario map and campaign background are PNG read by `GESurface::LoadPNG` (`ee_openformats` `png_to_picture`; 50/50 retail pictures and 2,666/2,666 tree PNGs pixel-equal to the `LoadTGA` path); `open_data.py` builds only the open tree (no binary form in it or in the release parts); the gates, the sim tools and the harness use it. Left: the model and texture loaders (another branch) |
| R3.1-R3.3 mixer logic, compression, encryption, TCP | Done (`ee_mixer`, `ee_codec`, `ee_net_tcp`). miniaudio stays; no audio has been listened to through the Rust mixer |
| R3.4 random maps | Done (`ee_rmap`), 2,088 of 2,088 maps equal to the C++ digests. The `.rmv` reader (`Unit10.cpp`) stays C++ |
| R3.5 File System | Done (`ee_fs`) behind the C++ facades |
| R3.6 Sound Engine | Logic done (`ee_sound`). `SESoundManager`: instance selection, limits, priorities, the paused queue, the scavenger's decisions, one-shots, random offsets (142 C++ golden scenarios). `SEConductor`: the score table and every decision (conductor.rs, 2,590 golden ops). `EEMusicManager`: the music thread's step, song changes, fades, intensity rules, timers (music.rs, 80 golden thread runs). `SEMixer`/`Sound Engine.cpp`: the mixers' registry, `UseMixer`, the start's plug-in choice, the shutdown (mixers.rs, 60 golden scenarios). C++ keeps the objects: nodes, samples, threads, events, locks, dmusic's loader/performance and `SEScore` (no decisions), the SPIM entry points, `sActiveMixer`. MCI CD audio (`SECD`) is dead on this port (never initialised) and not ported; DirectMusic is gone, the vendored dmusic stays. The positional sound (`EEComputeSoundParameters`) and the sound triggers are game code, mapped in PORT_LEARNINGS for the simulation lanes. No audio has been listened to |
| R4.1 GX on the `wgpu` crate | Done (`ee_gx`); wgpu-native is gone |
| R4.2 DX7 plug-in | Done (`ee_dx7`), a faithful port (D5) with C++ trampolines. C++ keeps the classes, object lifetimes, `Set3DMode`'s matrix composition, the PIM table and pl_mpeg |
| R5b.1 formations | Done (`ee_formations`): the C++ classes keep their members and call Rust for the geometry. 14,068 `Place` calls captured from the C++ in the six oracle games (`tools/sim_capture.py`: real games plus a sweep of every formation type over real units) equal bit for bit |
| **R5b.1 A* pathfinding, continents, choke points** | **In progress** (`ee_astar`, branch `rust-r5b-astar`): the tile goals read the world through a per-search view (`ee/EEAIAStarView.h`); the node storage, the search machine and every goal's hooks of every tile search (land/sea/space paths and the bounded search with their modifiers' costs and tile tests, the continent, hill, choke point, path area and path sector fills) are Rust (branches `rust-r5b-astar`, `rust-r5b-astar2`). Gate: an ASRN capture of every tile search of the six oracle games (82,302 runs, 34.8 million goal and view events) equal to the C++ digests, and the sim oracle. The graph searches (branch `rust-r5b-graph`): `EEAIAStarGraphStorage`'s nodes, lists and tables and `EEAIAStarGraphMachine`'s steps are Rust (`ee_astar/src/graph.rs`); the path graph and continental chain goals and the route planners stay C++ and get node views (`EEAIAStarGraphNode` without links) behind `EeAStarGraphCallbacks`. Gate: the ASGR capture (14,992 graph runs in the six oracle games, plus the late/space/calamity profiles' runs, recorded from the C++) equal, and the sim oracle. The continent and choke point analysis (`EEAIContinentManager`, `EEAIBuildChokePoints`) is captured (CNTB after `BuildRegions`, CNTR after every `Rebuild`: the whole analysis the game reads; golden from the C++) and moving leaf by leaf (branch `rust-r5b-cont`; the C++ manager keeps its objects and stores what Rust computes): `CreateContinentGrid` whole (the id grid, adjacency, neighbours, shore and ice shore tiles), `UpdateTileValues`, the choke pass's classification and smoothing, the sub sector link walks of `LinkSubSectorGrid`/`RelinkSubSectors`, the sector grid's sizes and rectangles, `EEAIGetTileKind`/`EEAIGetTileWallID`, `CreatePathAreaKind`'s start test, `RepairPathArea`'s choice (a plan the C++ applies) with `GetAdjacentAreaID`, and the queries `AreConnected`, `GetConnectedIDs`/`GetAdjacentIDs` and `GetLargestContinent` are Rust (`continent_grid.rs`, `tile_values.rs`, `choke.rs`, `sub_sector_links.rs`, `sectors.rs`, `tile_kind.rs`, `path_area_kind.rs`, `repair.rs`, `queries.rs`); the CNTB/CNTR digests equal in the four profiles. Still C++: the object bookkeeping of `Rebuild` and `RepairPathArea`, `CanGetTo` and the area classes' geometry (on `U2DPolygon<long>`; `PruneChokePoints`, the scenario areas, `GetTilesNextTo` and `U2DSparseArray` moved next, below). The address-ordered set at `EEAIAStarPathAreaGoal.cpp:282` turned out order-independent. Branch `rust-r5b-cont2` (PORT_LEARNINGS "R5b.1, finishing"): `U2DSparseArrayPointContainer` whole (`sparse.rs`: the areas', sub sectors' and explore goal's point sets; the C++ class owns a handle, its run lists and node pool deleted), `GetTilesNextTo`, `PruneChokePoints`' loop (the closest points behind a host) and `CreateScenarioAreas`' choice (`manager.rs`), and the path manager's splicing and step budgets (`path_splice.rs`: `TrimPath`, `AppendWaypoints`, `JoinPaths`, `FinishFailedPath`'s detour search, `TryShortPath`'s revolutions and the bounds, `SetGraphRevs`/`SetPathRevs`) are Rust; gated by CNTR, two new capture records recorded from the C++ first (CNTN every `GetTilesNextTo`, PMSP every splice call) and the oracle. Branch `rust-r5b-cont3` (PORT_LEARNINGS "R5b.1, the geometry"): `U2DPolygon<long>`/`U2DConvexHull<long>`'s algorithms (`hull.rs`: `SetPoints`' bounds, hull, center and area, `ProjectPoint`, `GetClosestOutlinePoint`, `GetClosestPoints`, `GetClosestPoint(Index)`, `Contains`, `GetScanLines`, `Sort`; the class keeps its members and slots, the points pass in place) on the convex hull and scan lines now shared with the engine's float hull (`ee_ge/src/hull.rs`; the float template bodies, `U2DGrahamsAlgorithm` and the dead slots deleted), then the areas' geometry on them (`area_geom.rs`: `EEAIArea::GetClosestHullPoint`, `GetHullDistanceSquared`, `GetClosestPoints`' loop, `GetTiles`, `EEAIContinent::GetClosestPoints`' loop and `GetShoreInRegions` behind `EeShoreCallbacks`; `PruneChokePoints`' near test reaches them through the areas' `GetClosestPoints`); gated by two capture records recorded from the C++ first (HULL every hull call, AREA every area geometry call; the outermost call of a thread only), CNTR and the oracle. Left, mapped: (b) the object bookkeeping of `Rebuild` and `RepairPathArea` and the manager's object queries (`GetChokePointAt`, `GetNearestLandTile`, `GetRandomArea`, `RemoveForestTile`, `UpdateWallUnit`), which walk the C++ area, sector and sub sector objects through their virtuals and the fill machines; they move with those objects, which are blocked by their reach: `EEAIArea` has nine subclasses (the seven analysis areas plus `EETown`, `EEHerd`, `EECPIArmy`: AI decision objects and saved state, branch `rust-r5a-aisave3`'s layout), 48 AI and unit files read their members directly (`mHull`, `mTiles`, `mShoreTiles`, `mCategory`, the id lists), and the continent is a graph node too (`AStarGraphableObjectBase`); a Rust area store needs accessors at those sites first. `EEAIArea::Create`/`UpdateHull`/`RemoveTile` are three calls each (the sparse set's, the hull's, the bounds' division) and stay C++ as glue; (c) needs the unit store (branch `rust-r5b4-core`): `CanGetTo` (the unit class's domains, the `CanMoveOnTerrain` virtual, the complex unit's lazily written `mPlanetID`, `GetFootprintContinent`), the goals' `SetUnit`/`CollectBlockedTiles` (`EEAIPathBlockerCollector` reads the units around the goal) and the path manager's request handling (`RequestUnitPath`, `StartGraphPath`, `FindPath`, `RequestUnitPaths`, `FinishGraphPath`, `UpdatePaths`' queue: unit goals, waypoints, `TakePath`/`DropPath`); (d) the path manager's history and regions (`PruneHistory`, `PruneRegions`, `SetGraphRevs`' history walk) are saved state, branch `rust-r5a-aisave3`'s. `MergeWaypoints` has no caller. The goals' other C++ (`GetPath`/`GetPoints`, the graph goals' path walks) forwards to the Rust storage: no decision left |
| **R5b.2 tech tree, events, calamities** | **In progress** (`ee_tech`, branch `rust-r5b-tech`): the C++ classes keep their members and fill plain views (`ee/EETechRust.h`); a node's and an epoch's availability, prerequisites, research decisions, epoch counters and costs, the tree's checksum, `DependsOn` and `SumCosts`, the effects' arithmetic (`EEApplyEventEffect`, which stat an attribute changes, hit points and flight time rescale, gather times, terrain masks) and `RunEffect`'s dispatch, and the calamities' age are Rust. Gate: a capture of research starts and completions, prerequisites, costs, every effect with its values before and after, calamities, and a sweep of every tech tree, in the six oracle games and three profiles (`sim_capture.py --profile late`/`space`: later start epochs with cheats; `calamity`: EE-SIM `--calamities` casts every class), equal to the C++ digests; the sim oracle. Open: the calamity classes' own updates and `CanCastAt` (no game casts through it), the effects that touch units and media, `ResearchToEpoch`, saves |
| **R4.3 Graphics Engine (17k)** | **In progress** (`ee_ge`): the math and geometry (points, vectors, lines, planes, transformations, quaternions, bounding boxes, rectangles), `UMath`'s table trigonometry, `GETerrainMesh`'s height, normal, closest-tile and line queries, its vertex normals, shorelines, fog of war and `Load`/`Save` (a field stream over the C++ `FSFile`), `GEPhysicalModel`'s per-triangle line and box tests, and `GEModel`'s `.cem` reader (both formats, the version-1 conversion, the sub-models; 'GEModel Legacy.cpp' deleted), its `Save` chunk writers and its geometry (bounds, normals, the graphics transform, the scale removal, the tag point order, `Verify`), bit-exact against the C++ (golden digests from the original text, the drivers relinked over the forwarders, the sim oracle). `ee_dx7` uses `ee_ge`'s `GETransformation::Apply`. The header inlines stay C++. Then (branch `rust-r4-ge4`): the pixel formats (`GEPackColor`, `GEUnpack*`, a frame's store into a surface), `GETexture`'s surface and frame-grid planning, `GETextureSet`'s grid and `GEMaterial::SizeOf`, the texture and model managers' decisions, names and texture pack (the maps, the lock and the lifetimes stay C++), `CreateFlatMesh`'s layout, the water texture grid, colour index and animation step, and `GetTileRowsInVolume` with its convex hull (`U2DConvexHull<float>`, one declared difference in collinear points: PORT_LEARNINGS "R4.3, fourth part"); 'GEModel LOD.cpp' and 'GEModel Caching.cpp' deleted (no callers). Gates: the store gate and `cppgate` (the stored textures equal), the sim oracle. Then (branch `rust-r4-ge5`): `UpdateVisibleTerrain` with the tile, water and cliff polygons and the eight edge skirts as a plan the C++ fills its material lists and polygon arrays from (`terrain_visible.rs`; the materials as texture handles, the corners as references; one declared difference: a frame past the arrays' 12,000/5,000 capacity is clamped where the original overran), `GEViewport`'s matrices, clipping planes, frustum points and planes and setters on a `#[repr(C)]` mirror (`viewport.rs`), `GEPhysicalModel::Intersects(viewport)`, and `GEBitmap`'s and `GERhombus`'s corners and stretched texture coordinates (`bitmap.rs`); each pinned by a golden driver over the unmodified units with fake textures (1,280 frames, 1,920 viewports, 9,000 bitmap cases; PORT_LEARNINGS "R4.3, fifth part") and relinked over the forwarders, rechecked against a `git archive` of the commit before (its own Rust) and over 15 real EE-SIM maps of every map type (960 frames); `GEPhysicalModel`'s walks (line, scaled per axis, model against model) over a callback table of `GEModel*` handles (`physical_walk.rs`; the exact retry's temporary placed model stays a C++ callback), pinned by a golden of 24,000 cases over fake models from the original unit and the sim oracle. Then (branch `rust-r4-ge6`): the text: `GEText::ComputeTextExtent`'s chunked measure, `GEFontBitmap`'s atlas layout, alpha pass and both draws' walks (UTF-8, the Windows-1252 cell, the pen, the clip's whole / cut / skipped glyphs) as plans the C++ runs its texture and `GEBitmap` calls from (`font.rs`; the platform's text functions through a table), pinned by a golden driver over the unmodified units with fake fonts, rasterizer and textures (2,387 cases) and relinked over the forwarders; `UWideString(const char*)`'s D6 heap overrun fixed (PORT_LEARNINGS "R4.3, sixth part"). `GEFont` has no logic; the classes stay C++. Open: `GEBitmap`'s and `GERhombus`'s vertex-array plumbing (no logic) |
| **R4.4 Graphics Thread (27.8k)** | **Started** (`ee_gfx`, branch `rust-gfx7`; PORT_LEARNINGS "R4.4"). **The map** (`src/Empire Earth/Graphics Thread/`, 77 units, 26.7k lines, and `RTSGraphics`/`RTSMouse`/`RTSMousePointer`, 1.1k): *the frame* (`RTSGraphics.cpp` 564: the thread loop, no rendering under `gPlatSimMode`, `Render` = FrameStarting under the world's read lock, then SceneStarting, RenderScene, FrameDone, the cursor, ShowScene; `RenderLobby`'s 4:3 letterbox arithmetic; `EEGraphics.cpp` 565: the lock modes, `EETakeRenderSnapshot`, `RenderTo` (the post-game capture moves the camera itself)); *the capture* (`EERenderObject.cpp` 2,047: `IsUnitDrawn`, `GetUnitOpacity` with its near-plane depth fade, `FillUnit`/`FillObject`/`FillRenderLists` into `EERenderLists`, `QueueEffects`, the per-frame clocks, fade, letterbox and time-of-day light blend, the ground range circle; it reads the live world under the lock, R5a.3); *the render objects* (`EEROUnits.cpp` 1,403: selection rings (air :193, ground :515 with its size choice, objects :671), the area-effect ring segments, the bars (`DrawBar`'s rectangle split, `DrawUnitBars`' stacking, `DrawBars`' sizes from the window), `DrawPlacement`'s footprint edges, labels and their clip rectangles, `GetScreenPoint` (world to screen); `EEROTerrain.cpp` 734: sky colours, cloud layer, the fog of war update, the grid; `EEROGroundShadows` 251, `EEROEffects` 66, `EEROUserInterface` 136: the UI's CopyData under the lock); all draw through `GERasterizer`, `GEVertexArray`, `GEModel` and `GEText` from the render lists and `gRenderSnapshot`, a few flags live (`gGameWindow`'s size, `gFreeCamera`); *picking* (`EEUnitPicker.cpp` 639: the pick ray, the ray-model tests through `GEPhysicalModel`, `FindUnit`'s ranking by distance, priority, exactness and size, the box selection over `GetFrustrumWorldPlanes`; the UI thread's, resolving its ids through the live world); *the cursor* (`RTSMouse.cpp` 424: `ClampPosition` and `Update`'s axis arithmetic, the save-under surface; `RTSMousePointer` 118: its own 25 ms thread; `EEMouse` 74; the cursor's kind is the UI's, `EEUIFMainGame::UpdateCursorKind`); *the effects* (`EEEffect*`, `EEEffectManager` 1,091, 24 2D and 36 3D effects, 17.6k: `Step`/`SetUp`/`Load` run in the simulation tick, `Queue`/`DrawPass*` are the renderer's; the frustum sphere tests of `EEEffect2D/3D::Queue` and the sprite and strip quads of `EEEffect.cpp` are geometry; the heavy vertex builders are `EEE3DFarm`, `EEE3DVolcano`, `EEE3DEarthquake`, `EEE3DTracerBeam`). Outside the folder but the renderer's to port: the camera (`EECameraControl.cpp` 1,193, whose `Update` runs in the simulation tick, `EEPhysics.cpp:756`, and from the UI's scroll, zoom and rotate events; `RTSWorld::SetCameraTransformation`'s yaw and pitch) and the minimap's arithmetic (`EEMiniMap.cpp` 962: `SetMap`'s map/pixel transforms, `Refresh`'s view trapezoid, `ProjectToGround`; `EEUIMiniMap::CenterCamera`), drawn by the simulation on the CPU. **Moved:** the camera's math (`camera.rs`): `GetPitch`, `GetOffset`/`GetLocation`, `GoTo`'s speed and snaps, `ClampHeight`, `BuildView`, `SetClippingDistances` with `EECameraRayDistance` (each ray built once; `EEMap::GetScreenLine` stays C++), `ClipToEdges`, `GetGroundPoint`, `Scroll`'s direction, near test and edge clamp, `Update`'s step and orthographic view; the terrain through callbacks. Pinned by a golden driver over the unmodified unit (24 meshes, 12,984 cases replayed in Rust and 1,440 C++-only movers; the relink printed the same bytes, and on two more seeds at four times the size), replayed in `golden_camera.rs`. The C++ keeps `MoveTo`, the followed units, the key path, `KeepPose`, `Revolve`/`Rotate` and the movers. Sim oracle identical; no tour run. **Second round** (branch `rust-gfx8`; PORT_LEARNINGS "R4.4, second round"): the free camera's pitch held past |zoom| 1 (a behaviour change, the owner's: the original's was NaN; 71 golden cases re-recorded); the pointer's `ClampPosition` and `Update` move (`mouse.rs`, 4,000 golden cases); `GetScreenPoint`, `DrawBars`' sizes, `DrawBar`'s and `DrawObjectBars`' rectangles (`units.rs`, 4,344 golden cases; the C++ keeps the rhombus calls); the fixture's camera telemetry, `key`/`wheel` commands and `fixture_game.py --scroll-check`/`--zoom-captures` (not yet run against the game). **Next:** the pick ray and `FindUnit`'s ranking, the minimap's transforms and trapezoid, the selection rings' size choice, `GetUnitOpacity`'s depth fade, the effects' frustum test |
| **R6 LLE User Interface toolkit (14.4k)** | **Started** (`ee_ui`, branch `rust-r6-ui`): `UILayoutUtility` (whole unit), `UISlider`'s arithmetic, a multi-line `UITextLabel`'s word wrapping, `UIForm`'s focus walk (`Tab`, `ShiftTab`, `CheckedRadioButton`) and control hit test, each pinned by a golden trace of the real C++ unit and re-run over the forwarders. The C++ classes keep their members, vtables and exported names. Then (branch `rust-r6-ui2`) `UIEditBox`'s text model (insertion and word wrap, deletion, caret, selection, scrolling, IME bookkeeping; the lines stay a `std::vector<UWideString>`) and `UIListBox<T>`'s selection and scrolling (host calls as a table per `T`, the virtuals stay virtual), each pinned the same way; one divergence (an edit-box crash) and one fix (`InsertLine`'s index), PORT_LEARNINGS "R6: edit boxes and list boxes". Every reading of game text is in `ee_ui/src/text.rs` (still cp1252 bytes; D6's UTF-8 switch changes that module), the case-folded sort order included. Then (branch `rust-r6-ui3`) the buttons (`UIButton`'s state machine and `CopyData`'s choice, `UIDualButton`, `UIScrollButton`'s repeat, `UICheckBox`, `UIRadioButton`), `UIScrollBar` and `UIComboBox`'s logic, each pinned the same way (the combo box's driver over fake part classes), no behaviour change (PORT_LEARNINGS "R6: buttons, check boxes, radio buttons, scroll bars and combo boxes"). Then (branch `rust-r6-ui4`) `UIControlsTable` (the record's fields, colours, layout and texture rectangles, `AddControlLike`, `MoveControlsHorizontally`, the scaled font's size, the string range, the rectangles `CreateControl` lays a control out with), `UIManager`'s form stack (visible forms under covering forms, `Push`/`Pop`/`PopTo`/`SwapForm`, dirty and clip rectangles, `GetFormByID`, the event queue's dispatch; the forms as opaque handles, the lists stay the C++ vectors), `UITriStateCheckBox`, `UIBlinkingButton`'s fade and `UIScalableButton`'s three-slice draw, each pinned by a golden trace of the original unit re-run over the forwarders (PORT_LEARNINGS "R6: the controls table, the form stack and the last button subclasses"). Then (branch `rust-r6-ui5`) `UIForm`'s dirty-rectangle bookkeeping and its `Open`, `Close`, `CopyData`, `SetPopup` (`form_state.rs`), the hot keys (`CalculateKeyCount`, `GetHotKeyName`, `FireEvents`, the sorter, `UpdateKeyboard`'s hot-key pass) and the font table (`Create`'s records, `CreateClosestFont`, `FindOrCreateFont`) (`hotkey.rs`, `font.rs`), `UIManager`'s music bookkeeping and IME members (`manager_media.rs`) and the backgrounds' logic (the tiling plans of every background kind and of `UIForm::DrawBackground`, the screen rectangle, moves and visibility; `background.rs`), each pinned by a golden trace of the original units re-run over the forwarders, no behaviour change (PORT_LEARNINGS "R6: the form's bookkeeping, hot keys, fonts, music, IME and backgrounds"). Then (branch `rust-r6-ui6`) the drawing as plans the C++ executes: `UIControl::DrawBorder`'s lines, `UIForm::DrawClippedText`'s pieces (clip rectangles, popup, IME candidate window), the text labels' placement and copied lines, the list boxes' copied lines, selection and focus rectangle, `UILBString`'s rows, the check box's and radio button's bitmap scale (`draw.rs`), `UIEditBox`'s copied selection and composition, selection rectangles, rows and IME underlines (`edit_draw.rs`), pinned by a golden trace of every draw call (a fake rasterizer under the original units, 2,100 cases replayed whole in Rust, the driver over the forwarders printing the same), no behaviour change (PORT_LEARNINGS "R6: the controls' drawing"). The pure call sequences (buttons, bitmaps, sliders, scroll bars, combo boxes, the form's and manager's draw lists) and the member copies of `CopyData` stay C++. Then (branch `rust-r6-ui7`) `UIEditBox`'s key dispatch (`UpdateKeyboard`'s repeat, `ProcessInput` and its Shift and Ctrl halves, deleted from the C++; `Cut`/`Copy`/`Paste`, `AddIMEResultString`'s typing, `MouseMove`'s drag selection; `edit_keys.rs`), `UIBackgroundTable::Create` (`background_table.rs`), `UIEvent`'s loops and count (`event.rs`), `UIFullBackground::LoadBackground`'s tile names and `UIStatusBar`'s arithmetic (`status.rs`), each pinned by a golden trace of the unmodified unit (keys: every key entity read digested, UTF-8 for every editing key) re-run over the forwarders, no behaviour change (PORT_LEARNINGS "R6: the edit box's key dispatch, ..."). **What is still C++ in the toolkit** (the units' function bodies outside the Rust forwarders, about 3,500 lines of 13.9k): construction and destruction everywhere (`UIManager`'s constructor 150 lines, `UIEditBox`'s 80, `UIForm::Init` 50, the destructors); `UIControlsTable::Create*` (300 lines: which control class a record makes and its constructor's arguments, a factory over C++ constructors); the draw and copy sequences (each `CopyData`/`DrawPolygons`/`DrawTextA` calling the GE objects; the decisions in them already Rust); the containers' plumbing (the forms' event, hot-key and entity-group vectors, `UIForm::SetFormEvent`'s switch of 13 member assignments, `UIManager::LoadDatabase`/`CreateBackgrounds`' file and class dispatch); the platform calls (the clipboard, IME, the file list's directory search in `UIFileListBox`/`UIFileComboBox::CreateEntries`). Logic that is still C++ and is a leaf, for the next round: `UIForm::UpdateMouse` (60 lines: which control gets the mouse, capture, double clicks), `UIControl::UpdateMouse` (20), `UITextLabel::ComputeTextLabelRect`/`CheckScrollBar`/`SetLocation` (the label's fitted rectangle, its scroll bar's appearance and range; blocked in a driver only by the real scroll bar's drag-bar bitmap), `UITLScrolling` (the scrolling label's timing and offsets, 120 lines), `UIIMECandidate::SetCandidateList`/`DrawPolygons` (the candidate list's paging and layout, 85), `UIGraph`/`UILineGraph` (the post-game graphs' scales and grid text, 300), `UIBFull::DrawBackground` (another full-screen tiling, 50), `UILBString::UpdateTranslatedKey` (a list's type-to-find, 27), `UIEntityGroup::Enable`/`Disable` (26), `UIHotKey`'s setters and the radio button's group uncheck (`RadioUnCheck`, 23) |

| **R6 LLE User Interface toolkit (14.4k)** | **Started** (`ee_ui`, branch `rust-r6-ui`): `UILayoutUtility` (whole unit), `UISlider`'s arithmetic, a multi-line `UITextLabel`'s word wrapping, `UIForm`'s focus walk (`Tab`, `ShiftTab`, `CheckedRadioButton`) and control hit test, each pinned by a golden trace of the real C++ unit and re-run over the forwarders. The C++ classes keep their members, vtables and exported names. Then (branch `rust-r6-ui2`) `UIEditBox`'s text model (insertion and word wrap, deletion, caret, selection, scrolling, IME bookkeeping; the lines stay a `std::vector<UWideString>`) and `UIListBox<T>`'s selection and scrolling (host calls as a table per `T`, the virtuals stay virtual), each pinned the same way; one divergence (an edit-box crash) and one fix (`InsertLine`'s index), PORT_LEARNINGS "R6: edit boxes and list boxes". Every reading of game text is in `ee_ui/src/text.rs` (still cp1252 bytes; D6's UTF-8 switch changes that module), the case-folded sort order included. Then (branch `rust-r6-ui3`) the buttons (`UIButton`'s state machine and `CopyData`'s choice, `UIDualButton`, `UIScrollButton`'s repeat, `UICheckBox`, `UIRadioButton`), `UIScrollBar` and `UIComboBox`'s logic, each pinned the same way (the combo box's driver over fake part classes), no behaviour change (PORT_LEARNINGS "R6: buttons, check boxes, radio buttons, scroll bars and combo boxes"). Then (branch `rust-r6-ui4`) `UIControlsTable` (the record's fields, colours, layout and texture rectangles, `AddControlLike`, `MoveControlsHorizontally`, the scaled font's size, the string range, the rectangles `CreateControl` lays a control out with), `UIManager`'s form stack (visible forms under covering forms, `Push`/`Pop`/`PopTo`/`SwapForm`, dirty and clip rectangles, `GetFormByID`, the event queue's dispatch; the forms as opaque handles, the lists stay the C++ vectors), `UITriStateCheckBox`, `UIBlinkingButton`'s fade and `UIScalableButton`'s three-slice draw, each pinned by a golden trace of the original unit re-run over the forwarders (PORT_LEARNINGS "R6: the controls table, the form stack and the last button subclasses"). Then (branch `rust-r6-ui5`) `UIForm`'s dirty-rectangle bookkeeping and its `Open`, `Close`, `CopyData`, `SetPopup` (`form_state.rs`), the hot keys (`CalculateKeyCount`, `GetHotKeyName`, `FireEvents`, the sorter, `UpdateKeyboard`'s hot-key pass) and the font table (`Create`'s records, `CreateClosestFont`, `FindOrCreateFont`) (`hotkey.rs`, `font.rs`), `UIManager`'s music bookkeeping and IME members (`manager_media.rs`) and the backgrounds' logic (the tiling plans of every background kind and of `UIForm::DrawBackground`, the screen rectangle, moves and visibility; `background.rs`), each pinned by a golden trace of the original units re-run over the forwarders, no behaviour change (PORT_LEARNINGS "R6: the form's bookkeeping, hot keys, fonts, music, IME and backgrounds"). Then (branch `rust-r6-ui6`) the drawing as plans the C++ executes: `UIControl::DrawBorder`'s lines, `UIForm::DrawClippedText`'s pieces (clip rectangles, popup, IME candidate window), the text labels' placement and copied lines, the list boxes' copied lines, selection and focus rectangle, `UILBString`'s rows, the check box's and radio button's bitmap scale (`draw.rs`), `UIEditBox`'s copied selection and composition, selection rectangles, rows and IME underlines (`edit_draw.rs`), pinned by a golden trace of every draw call (a fake rasterizer under the original units, 2,100 cases replayed whole in Rust, the driver over the forwarders printing the same), no behaviour change (PORT_LEARNINGS "R6: the controls' drawing"). The pure call sequences (buttons, bitmaps, sliders, scroll bars, combo boxes, the form's and manager's draw lists) and the member copies of `CopyData` stay C++. Then (branch `rust-r6-ui7`) `UIEditBox`'s key dispatch (`UpdateKeyboard`'s repeat, `ProcessInput` and its Shift and Ctrl halves, deleted from the C++; `Cut`/`Copy`/`Paste`, `AddIMEResultString`'s typing, `MouseMove`'s drag selection; `edit_keys.rs`), `UIBackgroundTable::Create` (`background_table.rs`), `UIEvent`'s loops and count (`event.rs`), `UIFullBackground::LoadBackground`'s tile names and `UIStatusBar`'s arithmetic (`status.rs`), each pinned by a golden trace of the unmodified unit (keys: every key entity read digested, UTF-8 for every editing key) re-run over the forwarders, no behaviour change (PORT_LEARNINGS "R6: the edit box's key dispatch, ..."). Then (branch `rust-r6-ui8`) `UIForm::UpdateMouse` and `UIControl::UpdateMouse` (`mouse.rs`), `UIEntityGroup::Enable`/`Disable` (`entity_group.rs`), `UILBString::UpdateTranslatedKey`'s search (`find_key.rs`), `UITextLabel::ComputeTextLabelRect`/`CheckScrollBar`/`SetLocation`'s rectangles (`label_fit.rs`) and `UITLScrolling`'s scroll, timers and line places (`scrolling.rs`), each pinned by a golden trace of the unmodified unit re-run over the forwarders; one behaviour change, the owner's decision: an edit box's deletion that leaves the selection anchor outside the text resets it to the caret (round 7's two hazards; PORT_LEARNINGS "R6: the mouse dispatch, the edit box's anchor fix, ..."). **What is still C++ in the toolkit** (the units' function bodies outside the Rust forwarders, about 3,500 lines of 13.9k at round 7; round 8 moved about 350 of them): construction and destruction everywhere (`UIManager`'s constructor 150 lines, `UIEditBox`'s 80, `UIForm::Init` 50, the destructors); `UIControlsTable::Create*` (300 lines: which control class a record makes and its constructor's arguments, a factory over C++ constructors); the draw and copy sequences (each `CopyData`/`DrawPolygons`/`DrawTextA` calling the GE objects; the decisions in them already Rust); the containers' plumbing (the forms' event, hot-key and entity-group vectors, `UIForm::SetFormEvent`'s switch of 13 member assignments, `UIManager::LoadDatabase`/`CreateBackgrounds`' file and class dispatch); the platform calls (the clipboard, IME, the file list's directory search in `UIFileListBox`/`UIFileComboBox::CreateEntries`). Logic that is still C++ and is a leaf, for the next round: `UIIMECandidate::SetCandidateList`/`DrawPolygons` (the candidate list's paging and layout, 85), `UIGraph`/`UILineGraph` (the post-game graphs' layout, zoom, grid and series scaling and the segment clip, 300), `UIBFull::DrawBackground` (another full-screen tiling, 50, a different formula from `UIFullBackground`'s), `UIHotKey`'s setters, `UIButtonRust::RadioSwap` (reference counts only) |
| **R5b.2 combat and unit formulas** | **Done, first part** (`ee_combat`, branch `rust-r5b-combat`): damage (direct, splash, trample), the attack factor's difficulty scale, elevation, morale, a hit's hit points, regeneration and healing, conversion cost and chance, the ballistic pitch, the projectile arc, the range and reach tests, the damage table rows; the C++ methods fill plain records (`ee/EECombatFormulas.h`). Gate: a CB** capture of the six oracle games plus a sweep over real units (1.7 to 1.8 million records a game, 20 formulas, 1.6 million of them the gather reach) equal to the C++ copies' digests and replayed byte for byte, and the sim oracle. Open: the target screens and walks (`ApplyAreaDamage`, `EEFindNewTarget`), the drop point, `EEIsLineToUnitBlocked`, `EECanConvert`/`EECanHeal`, the damage table storage (the AI indexes `gDamageTable` directly), the physics models' steps |

| **R5b.2 combat and unit formulas** | **Done, second part** (`ee_combat`, branch `rust-r5b-combat`): damage (direct, splash, trample), the attack factor's difficulty scale, elevation, morale, a hit's hit points, regeneration and healing, conversion cost and chance, the ballistic pitch, the projectile arc, the range and reach tests, the damage table rows; the C++ methods fill plain records (`ee/EECombatFormulas.h`). Gate: a CB** capture of the six oracle games plus a sweep over real units (1.7 to 1.8 million records a game, 20 formulas, 1.6 million of them the gather reach) equal to the C++ copies' digests and replayed byte for byte, and the sim oracle. Second round (branch `rust-combat2`, `ee_combat` decide.rs, walks.rs): `EECanConvert`, `EECanHeal` and the drop point over plain records; the line of fire (`EEIsLineToUnitBlocked`), splash damage's victims (`ApplyAreaDamage`) and the new target search (`EEFindNewTarget` with its region walk) over host callback tables, units as opaque handles. Gate: CBCN/CBCH/CBDP/CBLB/CBAD/CBFT captures recorded from the unmodified C++ (CBCH from EE-SIM's sweep: no game heals) equal in the four profiles. Open: the target searches' scoring (`EEUUTargetSearch`, `EESearchTargetAlongList`), `EEGetPursuerOf`, the weapon tag point and `EEGetTileLine`, the damage table storage (the AI indexes `gDamageTable` directly), the physics models' steps |
| R5a.1 per-CT checksum trace | Done: `EE-SIM --checksum-trace`, `sim_oracle.py run --checksums` and `ticks` (the first divergent tick). The option leaves the traces of seeds 2-7 equal to the reference |
| R5a.2 side-effect-free save | Done (a declared rebaseline for games that save): three more Save functions wrote the running state. With autosaves every 150 game s, seeds 2-7 now trace like the unsaved games (before: seeds 2 and 4 diverged). **Load equals continue: in progress.** `EE-SIM --save-at/--load-save`, `sim_oracle.py saveload` (18 saves of seeds 2-7). Save version 0.254 writes the running values and much state the original rebuilt at load (path points, herds, forests, unit order, unit and goal fields); save version 0.256 adds the AI's schedule, timers, resource lists, attack sub groups' targets and path requests, the path manager's history, the area effects and the tile order. Third round (save versions 0.257-0.262): the map regions' unit order, the AI experiments' timers, the favourite unit types (no format change), the units in production, unfinished buildings out of the rebuilt towns (no format change), the towns' ids, order and counters, the gatherer lists, the dead attackers (as units out of the world), the managers restored after every player. Still 0 of 18 continue identically to the end, but the first divergent CT line after the load moved from 0-23 to 0-178 (seed 2 600 s 4 -> 58, seed 3 600 s 0 -> 94, seed 5 1350 s 11 -> 91, seed 7 1350 s 22 -> 178). Fourth round (0.263-0.264): the continent manager as it is (every area with its serial, the sectors and sub sectors, the grids, the map tiles' AI values, the herds' and towns' serials; the computer players' locations keep their saved areas through the load), each attack sub group's position. The manager of a loaded game now equals the continued game's (digest) and the first divergent CT line moved for 5 saves (seed 2 1350 s 2 -> 86, 2100 s 1 -> 20, seed 3 2100 s 2 -> 57, seed 4 600 s 6 -> 80, seed 7 600 s 3 -> 10); still 0 of 18 to the end. Fifth round (0.265-0.268): the claim ranges and danger zones, the harass and cautious behaviours kept (no format change), the order of the units the unit managers track, the attack count and combat state as the running game counts them (no format change), the repair and site timers, the towns made from their saved areas. With captures on, all 18 loads ran to the end with no unit store (USCK) mismatch. First divergent CT line after the load (600/1350/2100 s) now seed 2 308/683/27, seed 3 207/309/569, seed 4 208/205/52, seed 5 312/479/71, seed 6 205/202/101, seed 7 171/559/420 (from 58/86/20, 94/0/57, 80/1/0, 23/91/23, 59/16/0, 10/178/1); still 0 of 18 to the end. Next: the 2100 s saves of seeds 2, 4 and 5 (CT 27, 52, 71; seed 4's an AI checksum only) (PORT_LEARNINGS "R5a.2: load equals continue" and its second to fifth rounds). Since the upstream merge of ce3df9e4 the live state is version 0.254 (upstream took 0.253 for the next engine's AI ids). |
| R6 WON leaves (D8) | Done (`ee_won`, branch `rust-r6-won`): the message buffers' format and WON's text conversions, CRC-16, MD5, the random generator, Blowfish, big integers, ElGamal, the auth blocks and the CD key logic, behind C++ forwarders (BigInteger, Random, IntegerExtractor deleted). The clang-built C++ MD5Digest and Karatsuba were wrong (PORT_LEARNINGS "R6") |
| R6 WON HTML (D8) | Removed: the HTML parser, documents and the segment layout engine; the lobby's pages (MOTD, terms of use, help, news) are plain text in a `TextArea` (`ee_won` plaintext) |
| **R6 WON, the rest** | **In progress** (`ee_won`, branches `rust-r6-won2`, `rust-r6-won3`, `rust-r6-won4`, `rust-r6-won5`, `rust-r6-won6`, `rust-r6-won7`, `rust-r6-won8`, `rust-r6-won9`, `rust-r6-won10`): the socket layer on the Rust platform layer (status names, NetStats, IPAddr, AsyncSocket's core, the ops' time left, length prefix, send and CR-LF loops; `won_ops_test`); the async-op machinery (`asyncop.rs`: AsyncOp's run state and transitions, the timer thread's deadline maps, the op container's queue and pump, the socket thread's op set, QueueSocket's decisions; the C++ classes keep the objects, locks, threads and completions and hold the ops' references; SocketThreadWinsock2/Ex deleted). **The LAN oracle exists:** `tools/harness/lan_game.py` plays a LAN game between two instances through the lobby's own UI (the fixture's lobby hooks; PORT_LEARNINGS "R6: a LAN game through the lobby's own UI") and passes, before and after the ports below. StringParser (`parser.rs`: the reads, scans and formats; the CRT's ctype keeps the locale's say; `won_parse_test`, 3537 golden lines in the C and the English locale); the LAN protocol (`lan.rs`: LanLogic's game-channel and discovery messages built and read, the join status, chat routing, timer rules and client ids; `won_lan_test` against the original's WriteBuffer statements). Open: AuthContext, peer auth (reached only by direct-connect games with the CD-key check), LobbyLanSocket's object, the services (directory, routing, DB, HTTP, misc), FileReader/FileWriter, the config objects, the lobby logic (WONLobby) and the GUI toolkit (WONGUI). **Round 4 (branch `rust-r6-won4`), the map of what is left** (lines of linked C++; reach): *every lobby load* (so the LAN game and a lobby tour): FileReader/FileWriter (403), WONConfig (the .cfg lexer ConfigParser + ConfigToken 618; the object model ConfigObject + ConfigValue 627, on virtual hooks), StringUtil 221, WONFile 353, RegKey 295, WONDebug 449, WONLobby 28.5k (LobbyModel 1551, LanLogic 1304, LobbyPersistentData 1020, StagingLogic 698, InitLogic 652, ChatCommandParser 438, the resource table WONLobbyResource 3865, RoutingLogic 2174 internet only), WONGUI 39.3k; *direct-connect games with the CD-key check only*: AuthContext 952 (also the internet login), peer auth (PeerAuthClient/Server and their ops 864, AuthSession 134, AuthPeerData 84, Auth2Certificate 79); *nothing* (the internet login only, the servers are gone): GetCertOp 442, WONDir 1618, WONDB 947, WONRouting 4523, WONServer 1439 (ServerConnection, ServerRequestOp, MultiPingOp), WONMisc 4051 (HTTP 1687, FTP 476, the patch and version ops), WONInternal 2540 (admin ops, TitleConfig), UDPManager 444. Dependencies: FileReader -> ConfigParser -> ConfigObject -> WONGUI's ResourceConfig / ComponentConfig -> the lobby's layout; AuthContext <- GetCertOp, peer auth; each service op <- ServerConnection / ServerRequestOp. LobbyLanSocket is a `BlockingSocket` subclass with a client link and a time (its stale rule is `lan.rs` already). **Done in round 4:** FileReader/FileWriter (`file.rs`: the readers' and writer's core on the CRT's stdio; `won_file_test`, 252 golden steps), ConfigParser (`config.rs`: characters, comments, tokens, defines, group defines, the UTF-16 sniff; `won_config_test`, 4060 golden lines in two locales, the 63 lobby .cfg files' token streams the same), AuthContext's login secrets, community data, hashes and refresh wait (`authctx.rs`; `won_auth_test`, 237 checks against the original's statements), the directory server's entity replies (`dir.rs`; `won_dir_test`, 548 golden replies); PORT_LEARNINGS "R6: WON's file reader and writer, the config parser...". **Done in round 5:** `won_parse_test`'s golden and model follow D6 (39 `wstrs`/formats lines re-recorded as UTF-8); WONFile's path logic (`path.rs`; `won_path_test`, 170 lines), RegKey's config.yaml names and binary values (`regkey.rs`; `won_regkey_test`, 97), the lobby's chat command parser (`chat.rs`; `won_chat_test`, 439, the lobby's own command names), WONGUI's GUIStringData (`guistring.rs`; `won_guistring_test`, 1280 in two locales), ConfigObject's grammar (`configobj.rs`: token kinds, the group header, the instructions, condition branches, the optional suffix, include paths; `won_cfgobj_test`, 103, and the 61 lobby files the same); one behaviour change (a failed `unsigned short` config read keeps its value); PORT_LEARNINGS "R6: WON's paths, RegKey, the chat parser, GUIStringData and ConfigObject's grammar". Found for D6 (fixed in round 6): GUIString's narrow constructor and `GetString` were Latin-1. **Done in round 6:** GUIString's narrow crossing is UTF-8 (the constructors, `GetString`, the narrow compares; a census of the 328 callers; `won_guistring_test` re-recorded, 1468 lines); the lobby's game room (`staging.rs`: StagingLogic's room messages built and read, who acts on which, the ready sounds, LobbyGame's join rules and listing summary, the player's head; `won_staging_test`, 175,517 checks against the original's statements); the saved settings (`persist.rs`: both files' layouts written from lists and read as events in the original's order, the user file name, free id, pruning, recent LAN name, default name number; ten LobbyPersistentData helpers deleted; `won_persist_test`, 26,855 checks); PORT_LEARNINGS "R6: GUIString's narrow text as UTF-8, the lobby's game room and its saved settings". **Done in round 7 (branch `rust-r6-won7`):** the lobby model's lists' decisions (`model.rs`: the routing maps' merge plan, the populations, the client, member and room flags, the friend's status, the ping states, the stale rule, `IsSameGame`, the status texts, `AreAllReady`, the directory objects; `won_model_test`, 607,933 checks; found: the original merges' insert hint dangled after an erase, UB, now `end()`); LanLogic's flow (`lanlogic.rs`: received messages, routes, closed sockets, summaries, the join loop and roles, actions, chat commands, dialogs, direct connect; `won_lanlogic_test`, 10,881); StagingLogic's and InitLogic's flows (`flow.rs`, the advert's `strtok` parse included; `won_flow_test`, 96,285); the resource table (`resource.rs`: 469 rows in the original's order, the glue and unload bodies deleted, golden of the original's 968 statements); RoutingLogic's decisions (`routing.rs`: room flags, Quick Play's pick, the friend search, the default server; `won_routing_test`, 75,029); StringUtil's case under D6 (`strcase.rs`: ASCII only, the original's signed order; `won_strcase_test`, 417,690); PORT_LEARNINGS "R6: the lobby model's lists, LanLogic's and the game room's flows...". Open: WONGUI's components and layout (next), RoutingLogic's flow over its ops and servers, the lobby's dialogs, and (internet only) peer auth, AuthContext's sessions and certificate flow, GetCertOp, the other services' requests and replies. **Round 8 (branch `rust-r6-won8`), WONGUI's map** (39.2k lines in 264 units, 3.1k of them the Win32 backend not linked; lobby use from the 63 .cfg files and the lobby's code): *layout* 1.3k (WONRectangle, the child layout instructions, centre / same-size / clear-right / space-evenly / no-overlap / size-at-least / percentage / vertical-flow layouts, the LAYOUT rule; the cfg's 367 Layout, 61 ChainLayout, 33 HorzCenterLayout, 20 RepeatLayout ... rules: every lobby load); *components* 5.0k (Button's press, hover, check, radio and repeat machine, WONButton's state table, Scrollbar / ScrollbarScroller / ButtonScroller / ScrollArea, ComboBox, TabCtrl, Splitter, Dialog, PopupMenu; Spinner, MenuBar and Animation unused); *events and focus* 5.2k (Component, Container's hit test and mouse routing, tab focus, focus ids, RootContainer, Window's double click, WindowManager 1.5k: modality, popups, dialogs; PlatWindowManager and PlatGUIWindow the new platform backend); *text* 2.7k (InputBox 855: selection, cursor, history, paste; WrappedText's wrap; TextArea and its segments: the chat output; the text width is the font's, so those take widths from the C++); *lists* 2.7k (ListArea 846: lines, selection, keys, insert / refresh / delete keeping the view; MultiListArea / header / item; TreeArea / TreeItem); *config readers* 7.0k (ComponentConfig, ResourceConfig, the per-class configs; on ConfigObject, round 5); *drawing, images, decoders, sound* 7.5k (Surface, StretchImage's nine slices, the decoders, PlatGraphics / PlatNativeImage / PlatGUIFont, WONSound's mixer). Dependencies, leaves first: WONRectangle and Component's sizes <- the layouts <- the configs' LAYOUT rules; ScrollArea <- ListArea, TextArea, the scrollers <- Scrollbar; InputBox <- ComboBox; Container's hit test and focus <- Window <- WindowManager. Order of the moves: layout arithmetic, scrolling, the input box's text model, the list's lines and selection, focus and the buttons' machine, then the font-measured pieces (wrap, chat text) through widths the C++ hands over. **Done in round 8:** WONGUI's layout arithmetic (`guilayout.rs`: WONRectangle, Component's sizes, the child layout instructions, the centre / same-size / clear-right / space-evenly / no-overlap / size-at-least / percentage / vertical-flow layouts, the LAYOUT rule's names and offsets; `won_guilayout_test`, 2,914,011 checks), scrolling (`guiscroll.rs`: Scrollbar's thumb, drag and steps, ScrollArea's clamp, view and bottom pin, the scrollers' conditions; `won_guiscroll_test`, 18,889,559), the input box's text model (`guiedit.rs`: cursor moves, selection, deletes, paste cut and limit, blink, the character under the mouse from the font's widths, the scrolls; `won_guiedit_test`, 9,437,056), the list's lines and selection (`guilist.rs`: FindLineAt's lower_bound walk, visibility, stacking, the selectable walk, keys, the scrolls of inserts, refreshes and deletes; `won_guilist_test`, 22,613,891; found: the wrap flag's endless FindSelectableItem recursion, now no item), focus and buttons (`guifocus.rs`: WantFocus, the hit verdict, focus ids and the last-focus pick, TabFocus's start, Button's release, drag, keys and repeat, WONButton's state; `won_guifocus_test`, 905,952); the word wrap (`guiwrap.rs`: CalcWrap's breaks and SetMaxWidth; `won_guiwrap_test`, 3,559,089); PORT_LEARNINGS "R6: WONGUI's layout arithmetic, scrolling...". Open in WONGUI: TextArea and its segments (the chat), ComboBox, TabCtrl, Splitter, MultiListArea, TreeItem, the menus, the dialogs' rules, WindowManager's modality and popups, the config readers' expressions, the drawing. **Done in round 9 (branch `rust-r6-won9`):** the text area (`guitext.rs`: line metrics, alignment, fit, the segment wrap, the bottom rule, the delayed wrap, the limits' trims and renumbering, the segment, character and selection under a point, the scrolls, the copied parts, the text splits and the lobby's format commands; `won_guitext_test`, 8,400,570 checks), ComboBox, TabCtrl and Splitter (`guictl.rs`; `won_guictl_test`, 11,578,060), MultiListArea, its header and TreeItem (`guimulti.rs`; `won_guimulti_test`, 3,900,000), the config readers' expressions and operand names (`guicfg.rs`, exceptions carried across as `exception_ptr`; `won_guicfg_test`, 300,271; INT_MIN / -1 gives INT_MIN), the popups, modality and popup menus' timing (`guiwin.rs`; `won_guiwin_test`, 8,000,000), the lobby dialogs' decisions (`dialogs.rs`; `won_dialogs_test`, 12,000,000); PORT_LEARNINGS "R6: WONGUI's text area, compound controls...". RoutingLogic's flow stays C++ (Internet servers only, nothing reaches it). **Done in round 10 (branch `rust-r6-won10`):** the tree list (`guitree.rs`: TreeItem's walks through callbacks, the open, insert, remove, sort and mouse rules; `won_guitree_test`, 6,233,856 checks; reached by nothing, the Internet room list only), the menus' keys and choices (`guimenu.rs`: PopupMenu's keys after the submenu, the submenus' open and close, MenuBar's keys, items and menus; no mnemonics in the original; `won_guimenu_test`, 22,000,002), the config readers' key tables (`guiconf.rs`: 103 tables, 509 instruction keys and flag and value names in 42 units, edited by a script checked by undoing it; `won_guiconf_test`, 839,246), the event dispatch decisions (`guievent.rs`: Window's mouse, double click, resize, hover and cursor, WindowManager's timer step, popups, focus, capture and keys, `CheckPopupClick`'s walk, the root's window drag, Container's mouse child, desired size and insert place; `won_guievent_test`, 40,578,784); PORT_LEARNINGS "R6: WONGUI's tree list, the menus' keys...". Open in WONGUI: the objects and their calls, the config readers' branch bodies, the drawing. |
| **R6 WON, Internet code** | **In progress** (`ee_won`, branch `rust-won-net1`; kept and ported as a reference, reached by nothing: the WON servers are gone). **Owner's direction (2026-11-11):** WON wholly internal to Rust (the game reaches it only through EEWonNet, the EEUIEWonNet* events, the test fixture and WONSurface.h): move whole classes, Rust objects owning their state, the C++ class deleted or a forwarding shim while a C++ caller remains. **Done in round 1:** WONServer's exchange (`server.rs`: ServerRequestOp's RunHook, TryNextServer, TryConnect, TryPeerToPeerAuth, Send, Recv, CheckExpiredSession, CallbackHook and the last-error rule over the op's C++ primitives handed over as hooks; CleanupHook's plan; ServerOp's step limits; PingOp's request and reply; MultiPingOp's request, reply test, give-up, wait, loop and order rules; ServerConnection's no-op and keep-alive; `won_server_test`, 200,000 scripted scenarios blocking and asynchronous against the original's statements, `golden_server.txt` replayed in Rust); ServerContext wholly (`servctx.rs`: the list, its lock, order, failover and one-time shuffle in a Rust object; the C++ class a shim converting IPAddr; `won_server_test` +248,531 checks against the original class over op sequences); WONDir's requests and replies (`dirops.rs`: all 13 ops' request bytes, the status replies with a service's returned address, the multi-directory accounting, FindEntityOp's parser choice, GetEntityRequest's Pack and GetDirEntityMap's grouping; `won_dirops_test`, 2,232,701 checks against the original's statements: 30,000 generated requests, every cut of 20,000 status replies, 20,000 multi-directory runs, 30,000 listings). The ops (ServerRequestOp and its ~60 subclasses, MultiPingOp, ServerConnection) stay C++ objects over Rust algorithms: they are AsyncOp / BlockingSocket objects, whose machinery's objects are C++ (asyncop.rs owns only its algorithms); a whole move needs the op and socket objects in Rust first. **Left** (lines of linked C++): WONServer's objects 1439 (after the op machinery); WONDir's objects 1618 (DirEntity, the op classes' fields: they become Rust records once ServerRequestOp is a Rust object); WONRouting 4523 (RoutingConnection 925, SmartRoutingConnection 967 + its commands 460 and snapshots 172, RoutingRequestOps 496, RoutingNotifications 338, 20 op classes ~1100: messages built and read, the connection's reply dispatch and the smart connection's model); WONDB 947 (the DB proxy ops, ProfileData 166, EventReportOp 178); WONMisc 4051 (HTTPGetOp 1133, HTTPEngine 290, HTTPCache 264, FTPGetOp 476, GameSpySupport 366, FindPatchOp 296, GetVersionOp 258, DetectFirewallOp 142, PatchTypes 206); WONInternal 2540 (admin ops, TitleConfig 283, FilePushOp 310, MasterKeyOp 301, BannedKeyOp 281, KeySubscribeOp 264); UDPManager 444; GetCertOp 442; AuthContext's sessions and certificate flow (of 894); peer auth (PeerAuthOp 310, PeerAuthClient 222, PeerAuthServer 183, PeerAuthServerOp 149, AuthSession 134, AuthPeerData 84, Auth2Certificate 79). Order: the op and socket objects into Rust (with the other WON lane's machinery), then ServerRequestOp as a Rust object, then each service's ops as Rust records; WONRouting's messages can move first as builders and readers. |
| **R5a.7 gates on layout-independent observation** | **Done, gate runs pending** (branch `rust-r5a-gates`): `tools/gates.sh` runs `fixture_game.py` (the game's UI fixture, its telemetry, stderr verdicts and game log; window captures; no layout, mangled name, debugger or import hook); the harness clock is `PlatSimSetTimeFactor` in `ee_plat_core` (clocks F x, sleeps/timers/wait timeouts 1/F); `--legacy` keeps `game_session.py`. Still on the layout: exe_boot.c's guards and hooks, the RNG trace, the human driver, `differential/` |
| **R5a.4-R5a.6 handles, names, the RTS layer** | **Started** (branch `rust-r5a-handles`, C++ only, no behaviour change; sim oracle identical per commit). R5a.4: measured who holds unit pointers (PORT_LEARNINGS "R5a.4-R5a.6"); `EEUnitHandle<T>` (`ee/EEUnitHandle.h`) is the unit id resolved through a registry that lives from `RTSPlayer::InsertUnit` to `RTSUnit::FreeNode`, so a freed unit's handle is NULL, never a reborn slot; the AI's 14 stored unit pointers are handles. R5a.5: the simulation's address-named globals and functions named, `gU531CF0`/`CF4` folded into `EEPhysics`. R5a.6: the RTS objects import 26 game symbols (1 from Graphics Thread, `RTSMouse::EndFrame`); `RTSMouse`/`RTSUnit` moved to their own headers, RTS -> Graphics Thread include edges 3/12 (direct/transitive) -> 1/4. Then (branch `rust-r5a-handles2`): the registry is Rust (`ee_units`, a lock-free radix tree of atomic pointers, 9 tests), so any thread resolves handles; the UI portrait, the editor's undo records and selection, the camera's followed units, the improvement action, EEWorld's five containers (the stepped units, the queues, the active map, the resource users) and 12 of the AI's containers (vectors, deques and a type-keyed multimap of referenced units: their order is the elements' order) are handles; the prophet analyzers' members too (PORT_LEARNINGS "R5a.4, finished"). Open: `EECPUnitManager`'s class-keyed deques (the type reaches ~20 units through `GetUnits`/`CountUnits`), the function objects' containers (one call's life: left as pointers), the A* goals (`rust-r5b-astar`), the Graphics Thread holders and names (`rust-r5a-snap`), RTSGraphics.cpp's game globals |
| D6 game text is UTF-8 | Done (branch `rust-d6-utf8`, a declared rebaseline; PORT_LEARNINGS "D6: game text is UTF-8"): `ee_plat_core::utf8` is the one text implementation (`PlatStrICmp`, the `PlatMbs*` family, the case folding, the wide conversions) on every system, the CRT and the POSIX single-byte port gone; `PlatFont` renders by code point (U+0020..U+00FF at creation, the rest on first use), `GEFontBitmap` keeps its Windows-1252 cells; `ee_ui`'s `text.rs`, SDL text input, the clipboard, WON's `StringUtil` (through `ee_won`'s `text.rs`), `PlatGUIFont`, `PlatClipboard` and the lobby's pages are UTF-8; the text compiler keeps UTF-8 in the records (the Python importer the same, retail transcoded once at load), the Language strings are UTF-8; saves are version 0.255, older saves and files without a version convert their strings from Windows-1252 on load. Sim oracle identical, the Rust gate, eedata check/cppgate, plat_test, font_test and the WON tests pass; the menus' pixels are for the lead's tours. Left: the fonts' coverage (Liberation: Latin, Greek, Cyrillic; no CJK), `GEFontBitmap` draws only what Windows-1252 has, `_mbs*` stubs in tools/rmap_gate, campaign archive tags are not converted |
| **R5b.4 world and units** | **Started** (branch `rust-r5b4-core`): the map (section 5, R5b item 4: 657 `dynamic_cast` sites, 337 of them unit goal, object and class tests; 133 message codecs). The game messages' codec is Rust (`ee_netmsg`: the wire layout of the 104 game message classes, the box selection's packing; the C++ classes hand their members as slots), gated by an NM** capture (every executed message, a sweep of every type over edge payloads, the packings) equal to the C++ goldens in the four profiles. Unit kinds are Rust tables (`ee_units` kinds.rs: the goal classes per `mKind`, the unit classes per category; `ee/EEUnitKind.h`); 118 goal casts and 34 unit class casts of the simulation (Units, Physics Thread, Communications, Player) are kind reads. Second round: the 28 engine messages (`NEMessage.cpp`) and the players' inputs nested in the ticks are in the codec too (`ee_netmsg` engine.rs; gate NMNE, 549 engine messages per game, equal to the C++ goldens in the four profiles); the message kind by type (`ee_netmsg_message_kind`) replaces the four `dynamic_cast<EEMessage*>`; the object kind (`RTSUnit::mObjectKind`, `EE_UNIT_OBJECT_KINDS`) replaces 28 `dynamic_cast<EEComplexUnit*>` of the simulation; leaf 4 (the `Execute` bodies) mapped. Third round: leaf 4 (a) and (b) in Rust (`ee_netmsg` execute.rs, ee/EEMessageExecute.h): the selection id check of the two entry points and the toggles' passes; the take test, the market price (`EEGetTradePrice`'s curve), the exchange step, the tribute's orders and viewer, the disband refund; gate NMDC (every call, the decisions' tables, the real players' prices and units every five game minutes) equal to the C++ goldens in the four profiles; (c) mapped: it needs the unit store. Fourth round: the unit store's first stage (`ee_units` store.rs: each unit's plain state as a Rust record by id, kept current by write-through holders on eight C++ members, ee/EEUnitStore.h; gate USCK: the store equals every unit at every tick and before every executed message in the four profiles), the world calls of Rust decisions (host.rs: a group's units as ids, goal creation returning a handle), the box selection's pick in Rust over the store (select.rs; gate USPK, equal to the C++ pick's goldens). Fifth round: a box selection exercise through the real input path in the UI fixture (`boxselect`; fixture_game.py `--box-select`: select, add with Shift, a unit deleted before the release), the store's class domains and land-building flag, the active groups' Fill test in Rust over the store (group.rs `fill_members`; gate USFL, equal to the C++ test's goldens), Explore's body in Rust over host calls (group.rs `explore`). Sixth round: the store's commandable flag, formation slot, the class's ground attack flag and the resource amount (USCK); Attack Ground, Patrol, Gather and Repair in Rust over host calls (group.rs), gated by a sweep over a dry host (USGC: the Rust bodies' and the original statements' logs equal over every player's units every five game minutes, four profiles) and mock-host unit tests; the out-of-sync checksum mapped. Seventh round: Add Patrol Point (KeepNonCitizens' test over the store), Attack Unit, Heal Unit and Path To in Rust over host calls (group.rs; the USGC sweep and mock-host tests); the UI fixture's orders through the real input path with a per-unit verdict after each message executes (`order`; fixture_game.py `--orders`: gather, repair plain and queued, the Patrol button, Attack Ground, a move). Open: the 54 other `EEComplexUnit` casts (17 on the load path, 31 AI), the AI's, UI's and editor's casts, the 6 other goal-giving `Execute` bodies (leaf 4 c), the store's next fields and positions, the tick, the save codec (PORT_LEARNINGS "R5b.4", "R5b.4, second round", "R5b.4, third round", "R5b.4, fourth round", "R5b.4, fifth round", "R5b.4, sixth round", "R5b.4, seventh round") |
| **R5a.3 the renderer reads a snapshot** | **Done for units, players, the map and the camera** (branch `rust-r5a-snap`; PORT_LEARNINGS "R5a.3", with the measured read table): FrameStarting under the existing read lock is the capture (`ee/EERenderSnapshot.h`, the render lists, the effects' `Queue`); after it the render phase reads no unit, player or physics state, and the unit picker holds ids, so the graphics thread takes no unit references. Left: the effects' own state (stepped by the tick while drawn), the terrain mesh's heights, the see-through flags written in the capture, the UI's CopyData. Sim oracle identical; no tour run |























Checked on Windows at the end: link 0/0, the Rust gate, `eedata check` and `cppgate`, `gx_test`, `gates.sh` (tours and AI games on both
trees), six seeded 45-minute sim traces equal to the pre-Rust reference, menu captures pixel-equal, a saved game loaded and
played on. Linux: the game cross-compiles and links, but has not been run. macOS: Rust type-checks only; nothing was
built or run.

---

## 0. Summary

- **Size.** About 570k lines of first-party C++: `src/` 484k, `include/` 86k. Of that, 131k is the WON library and
  its lobby; the other ~440k is the engine and the game. The vendored C is ~210k more (miniaudio, zlib, libpng,
  libjpeg, dmusic, pl_mpeg, stb, cgltf, Lua, the codec headers WON carries). The engine (`src/Low-Level Engine`, 68k)
  is cleanly layered. The game (`src/Empire Earth` without WON, ~300k) is one strongly connected component around
  `EEWorld`, the units, `EEPlayer` and the physics tick.
- **The seams are better than the coupling suggests.**
  - The platform layer is already a C ABI that names Rust as an intended implementation (`include/platform/Platform.h`).
  - The renderer sits behind the GX contract and four abstract engine classes.
  - Plug-ins are name-indexed entry-point tables (PIM).
  - UI input reaches the simulation only as network messages (`NEMessage`), never as direct writes.
  - Every on-disk record is size-pinned, and every network message has a fixed wire format.
  - The WON lobby reaches the screen as one pulled BGRA frame, and its GUI backend is already ours (`WONGUI/Plat*.cpp`).
- **The rewrite is testable.** EE-SIM plays a seeded 45-minute 2-AI game in ~15 s. Its AI trace is identical
  between runs and equal to the harness's. "Same trace for seeds 2-7 over 2700 game seconds" is a ready-made,
  layout-independent oracle for any change on the simulation side. The data side has the content-dump parity gate
  (`eedata.py cppgate`). The platform, GX and TCP layers have self-tests that can run against a second
  implementation.
- **The hard parts:**
  - the simulation's bit-exact float and RNG behaviour (lockstep, checksums of float bit patterns);
  - 638 `dynamic_cast`s and intrusive, non-atomic reference counts on objects shared between threads;
  - the Windows harness, which reads game memory by clang record layout and hooks C++ mangled names;
  - `UString`/`std::vector` in nearly every interface, including the "C" plug-in entry points.
- **The plan, in order:**
  - R0: one Rust static library in the build, absorbing wgpu-native.
  - R1: platform-layer leaves.
  - R2: the data layer.
  - R3: engine services and plug-ins.
  - R4: the renderer.
  - R5: the simulation, after C++-side untangling.
  - R6: UI, editor and the lobby (what is left of WON is decision D8).
  - R7: the entry point and the last C/C++.

  Every Rust module lands with unit tests (section 5). Each phase ends with the existing gates. R5 is about half the code outside WON and most of the risk; R0-R4 are
  worth doing even if R5 slips, because they remove the C toolchain's hardest dependencies (webgpu.h pinning,
  Win32/POSIX forks, the text compilers).

---

## 1. The code base from a rewrite's point of view

### 1.1 Modules

Lines include each module's headers. "Build module" is `tools/units.py`.

| Area | Lines | What | Coupling |
|---|---|---|---|
| Platform (`src/Platform`, `include/platform`) | 7.7k | Time, threads, files, paths, strings, config, sockets, window (SDL3), fonts (stb_truetype) | **No outbound edges.** C ABI. Tests in `tests/platform` (about 265 checks) |
| LLE Utilities | 9.4k | UString, URandom, compression (PKWARE DCL), encryption, graphs, 2D geometry | Leaf; fan-in 35 modules |
| LLE File System | 3.9k | FSFile, FSArchive, FSChunk, FSMemoryFile, FSPersistent | Utilities, threads, platform; fan-in 31 |
| LLE Thread System / Time Manager / Error System | 1.8k | TSThread queues, locks, scheduler, timers | Platform |
| LLE Sound Engine (+ Mixer plug-in) | 3.5k | Sound manager, conductor (dmusic), mixer on miniaudio | Platform, core |
| LLE Network Engine (+ TCP plug-in) | 7.2k | Lockstep server/client, messages, TCP on PlatSocket | Platform, core |
| LLE Graphics Engine | 26.6k | GE* models, textures, fonts, cameras; the DX7 renderer plug-in (9.7k) over GX (3.1k on webgpu.h) | Clean; fan-in 31 |
| LLE User Interface | 14.4k | UI* widget toolkit, forms from the UI db tables | Graphics Engine; one edge to Database |
| RTS Engine | 3.6k | RTSWorld, RTSUnit, RTSTechTreeNode base classes | Points *upward* into Units, Player, Map, AI |
| Database | 8.5k | db tables, `EEDataText` text compiler, `EERules`, `EEDataDump` | Mostly GE/FS; 2 files touch `gWorld` |
| Random Map (+ Default generator) | 7.3k | Map scripts (.rmv), terrain/player/resource placement | Generator depends only on Utilities and `EERandomMap.h` |
| Formations, Map, Tech Tree | 5.3k | Formation geometry; tiles/minimap; techs (declared in `EEPlayer.h`) | Small, but Tech Tree is tied to `gWorld` |
| Units, Physics, Player, Calamities, Actions | 61k | The simulation | The strongly connected component |
| AI Thread | 63k | Computer player (EECP*), A* pathfinding (5.9k), continents | Runs *inside* the physics tick (`EEPhysics.cpp:756`), not on a thread |
| Communications | 12.9k | Game messages (EEM*), input -> commands | |
| Graphics Thread | 27.1k | The game's renderer; reads the live world under the read lock | Graphics Engine heavy |
| User Interface Thread, UI Events | 36.5k | In-game UI | |
| Scenario Editor (+ Actions), Scenario Manager, Campaign | 39.5k | Editor, triggers | Largest fan-out (30 modules) |
| Empire Earth | 14.4k | WinMain, EEWorld, game state, EESim, test fixture, game log | Hub |
| WON library (`src/Empire Earth/WONAPI`, `include/ee/wonapi`) | 131k | WONGUI toolkit 49k; WONLobby 28k (with the game glue in `GameCode/EmpireEarth`); sockets, async ops, crypto, auth, directory, routing, HTTP and the rest 32k; headers 21k, not split by area | On the platform layer (46 files call `Plat*`). Own `SmartPtr` objects (173 typedefs), 52 `dynamic_cast`s. The game side is `EEWonNet`, `EEAWonNet` and the WonNet forms (~4k, counted in the rows above) |

### 1.2 Coupling

- **Engine is layered.** The LLE subsystems depend only on the core header, Utilities, Thread System, File System
  and Platform. The only cycles are core <-> subsystem ones, created by the `LowLevelEngine.h` umbrella header.
- **Game is one knot.** Every game module is in one strongly connected component. The heaviest mutual edges are:
  - AI <-> Units (181/81 include edges);
  - Graphics Thread <-> Units (89/18) and Graphics Thread <-> Physics (64/8);
  - Physics <-> Units (47/72);
  - RTS -> Graphics Thread (146): the base layer includes the game. Measured by symbols (R5a.6), the RTS objects
    import one Graphics Thread symbol, the RTS class's own `RTSMouse::EndFrame`; the edges were the RTS classes
    declared in game headers.
- **Global state.** 224 `g*` names are declared in headers and ~459 are defined in `src`; the
  simulation's address-named ones are named since R5a.5 (`gU4E8708` is `gDamageTable`); the UI's, the editor's and the
  renderer's are not yet. The ones touched most:
  - `gWorld`: 310 files in 23 modules;
  - `gPhysics`: 174;
  - `gUIManager`: 123;
  - `gUserInterface`: 97;
  - `gContinentManager`: 76;
  - `gUnitClassTable`: 70.

  `EEPlayer.h` is a second god header: it also declares the tech tree and `gTechTree`.
- **Most include edges are noise.** Nearly every `.cpp` opens with the same ~40-header preamble. Real symbol use is
  much narrower: all 10 Formations files include UIForm and SESoundManager without using them. Before taking a
  module across, measure its real surface: the symbols its objects import, from `llvm-nm` on its objects, not its
  `#include`s.
- **WON sits beside the knot, not in it, as far as the source shows.** The game calls the lobby through about ten
  `EELobby*` functions. The other direction is wider: `EELobbyInterface.cpp` creates and drives `EEServer` and
  `gCommunications` itself. 538 WON files open with `lle/LowLevelEngine.h` and `ee/EEWonNet.h`, the same preamble
  noise as above, so its real surface still needs the `llvm-nm` measurement.

### 1.3 The seams that exist today

| Seam | Shape | Rust-ready? |
|---|---|---|
| `include/platform/Plat*.h` | `extern "C"`, `<stdint.h>` types, opaque handles; 11 headers, ~195 functions | **Yes**, as is (`#[no_mangle] extern "C"`). The tests run against any implementation |
| PIM plug-ins (`include/lle/PIMModule.h`) | Name -> `void*` tables: renderer `'rast'`, network `'ntwk'`, mixer `' snd'`, random map `'rmap'` | Mixer: nearly (36 `SPIM*`, POD and opaque pointers). The others pass C++ objects: `UString&`, `TCPServer**`, `EERandomMap*` (vectors of vectors), `GERasterizer*` |
| GX (`include/gx/GX.h`) | COM-style C++ classes (DirectDraw 7/D3D7 subset) over webgpu.h, behind one lock; ~15 caller files | Needs C++ shim classes forwarding to `extern "C"` (thin: ~60 methods) |
| GERasterizer / GEScreen / GESurface / GEVertexArray | Pure-virtual C++ (54/19/31/15 pure virtuals) | Via C++ trampolines (a vtable implemented by forwarding to Rust) |
| Commands into the simulation | `NEMessage`s through the local server, drained by `EEPhysics::DrainInput` | **A good seam.** The wire format is fixed-width bytes (`ExportMessage`/`ImportMessage`) |
| On-disk records | 37 raw records, 51 `static_assert(sizeof)` pins, `tools/record_sizes.py` | **Yes:** one `#[repr(C)]` mirror each, with the same asserts |
| Saved games | FSChunk tags, field-by-field writes with version gates (`tools/harness/savefile.py` documents it) | The order is the contract, not the layout; a Rust codec can match it |
| `platform/PlatSim.h` | C globals and a per-tick callback (the sim's switchboard) | Yes, but it switches C++ behaviour |
| Lobby frame (`EELobbyLockFrame`/`UnlockFrame`/`SetPresentation`/`GetSurfaceSize`, `RTSGraphics::RenderLobby`) | BGRA pixels, sizes and a present rectangle, pulled by the graphics thread | **Yes**, plain data; only `EELobbyLockFrame`'s C++ reference parameters need flattening |
| WONGUI backend (`WONGUI/Plat*.cpp`, 2.3k; `units.WON_REPLACED`) | WON's WindowManager, Window, Graphics, NativeImage and Font implemented on the platform layer and WON's software rasteriser | A replaceable C++ class boundary, if WONGUI is kept while the rest moves (D8) |
| WON wire messages (`WONBuffer.h`, `ReadBuffer`/`WriteBuffer`) | Little-endian fields, UTF-16 LE wide strings per unit, pinned by `tests/won` | **Yes:** a Rust codec has a byte-for-byte test |

---

## 2. Invariants a Rust replacement must keep

### 2.1 Determinism (simulation code only)

The lockstep sim and every oracle in section 3 rest on a bit-identical simulation. The out-of-sync checksum
(`EEPhysics::GetOutOfSynchChecksum`, `EEPhysics.cpp:249`) is a wrapping `uint32` sum that adds every unit's float
positions **as bit patterns** (`EEComplexUnit::GetChecksum`, `EEComplexUnit.cpp:1896-1911`). A Rust sim component
must reproduce:

- **The same float operations in the same order, in `f32`**, with no fused multiply-add and no reassociation.
  - Rust never contracts on its own. clang does with `-ffp-contract=on` (the default; no build sets it).
  - On x86-64 baseline there is no FMA instruction, so the Windows and Linux builds cannot fuse. **The arm64 macOS
    build can.** Check whether the Mac sim already differs from Windows; set `-ffp-contract=off` for both C++ builds
    before any C++/Rust comparison on arm64.
- **URandom** (`include/lle/URandom.h`): the LCG with 32-bit state, `Generate()` dividing in **double**, and
  `URandomIntegers` rounding with `lrintf`. Draws must happen in the same order and number. The physics stream has
  ~207 call sites.
- **Float-to-int conversion** with `UMFloatToLong` = `lrintf` (round half to even), not Rust's `as` (truncating,
  saturating).
- **libm.** 15 sim files call `sin`/`cos`/`atan2`/`sqrt`. Rust's `f32::sin` lowers to the same CRT/libm call as
  clang's C++ on the same target; the pure-Rust `libm` crate does not give the same bits. Use std, or call the C
  functions explicitly, and never the `libm` crate in sim code.
- **Container iteration order.** No `HashMap` iteration in the sim. Address-ordered sets are a hazard; the one in the
  A* code (`EEAIAStarPathAreaGoal.cpp:282`) turned out harmless, since every visiting order gives the same state
  (PORT_LEARNINGS "R5b.1").
- **Wrapping integer arithmetic** where C++ relies on `-fwrapv`: `wrapping_*`, or `Wrapping<T>`.

Policy proposal: **bit-exact while porting, divergence only at declared rebaseline points.** CLAUDE.md allows the game
to diverge from the original, and multiplayer compatibility with retail is gone. But while code moves language, the
bit-exact trace is the only cheap way to catch a porting mistake. A deliberate behaviour change lands as its own
commit, with a statistical gate (`sim_bench.py`, `ai_scorecard.py`) and new reference traces, never mixed with a port.

### 2.2 Memory and threads

- **Unit hierarchy:** `UReferenceCounter -> RTSUnit -> EEUnit -> EEComplexUnit`, with 137 virtuals in `EEUnit.h`.
  Units come from a pool whose slots are reused at once. The bugs fixed this week (a freed slot reborn as a
  projectile inside an AI sub group) show the cost.
- **Reference counts are intrusive and not atomic.** Safety rests on the world lock: one `TSReadWriteLock`. The
  physics tick writes under it; the graphics thread renders straight from the live objects under the read lock; the
  UI updates under the read lock.
- **Rust cannot own an object that C++ still holds by raw pointer, calls virtuals on and `dynamic_cast`s.** Ownership
  can only move when the C++ users hold a handle. The ids exist already: saved games store 32-bit `USavedID`s and
  rebuild pointers on load.
- **No Rust reference may live across a call back into C++,** because C++ may reach the same object. Rust code
  called under the world lock receives `&mut` derived from the lock contract and drops it before returning.
- **The lobby runs inside a posted UI action and owns the UI thread while it is open.** `EEUserInterface::ProcessActions`
  holds the UI copy lock and the world write lock around every action, so `EELobbyRunLoop` releases both and takes
  them back in that order; otherwise the graphics thread blocks in `EEGraphics::FrameStarting`. The frame's lock
  order is `EELobbyLock`, then the frame lock. A replacement keeps both rules or removes the nesting.

### 2.3 Text

`UString`, `UWideString` and the records' `char[]` fields hold UTF-8 (decision D6, done on branch `rust-d6-utf8`;
PORT_LEARNINGS "D6: game text is UTF-8"). Rust keeps game text as bytes (`&[u8]`/`Vec<u8>`): a stray byte is one
character of its own (`ee_plat_core::utf8`), so a `String` would refuse what the game must carry. Characters are code
points everywhere a character is counted (`PlatMbs*`, the UI's stepping and wrapping, the fonts' glyphs); old
Windows-1252 inputs (saves before 0.255, files without a version) are converted where they enter.

WON's text is different: `GUIString` and `wchar_t` in memory, UTF-8 at the platform calls, and UTF-16 LE code units
on the wire and in WON's files (`wchar_t` is 4 bytes on Linux and macOS, which is why `WideAppendUtf16LE` exists).
Rust keeps those as `u16` units until they reach a display, because a name from the wire need not be valid UTF-16;
the game's bytes meet them in `ee_won`'s `text.rs` (`StringUtil.cpp`).

### 2.4 Observability

The **Windows harness** depends on the C++ layout:

- `tools/harness/layout_gen.py` dumps clang record layouts, and `game_state_probe.py` reads `gWorld`/`gPhysics`/
  `EEPlayer` members by offset;
- `exe_boot.c` and `hbuild.py` hook mangled names: `?Generate@URandom@@QEAAMXZ`, `?NextInSeries@URandom@@AEAAKXZ`,
  `?gWorld@@3VEEWorld@@A`, `?ShowScene@DX7Rasterizer@@...`, `?Enter@TSCriticalSection@@...`.

Everything Rust owns is invisible to that harness. The layout-independent observation points survive any language:

- the `EE-AI` trace;
- the `EE-GAME` game log;
- the EE-SIM JSON report;
- `.ees` save diffs;
- the `EE_TEST_UI` fixture (`ui-state.txt`/`test.command`, used by `posix_game.py`);
- OOS dumps.

The fixture does not acknowledge commands while the lobby is open, because the lobby owns the UI thread. The
harness's `--exercise lobby` waits for presented frames instead, so a lobby replacement must keep presenting frames
or give the fixture a real way in.

**Move the gates onto these before R5.** R1-R4 can keep exporting the mangled C++ shims the harness hooks.
Done in R5a.7: the gates run `tools/harness/fixture_game.py` (fixture, stderr, game log, window captures) and the
harness clock is `PlatSimSetTimeFactor`; only `gates.sh --legacy` and the debugger tools still need the layout.

---

## 3. Existing work that carries the rewrite

| Asset | Compares | Use for Rust |
|---|---|---|
| EE-SIM + `tools/sim.py`, `sim_bench.py`, `tools/harness/ai_trace.py compare` | Seeded games: trace, report, first difference | **The simulation oracle.** A mixed C++/Rust build must print the reference trace for seeds 2-7 over 2700 s. `compare` pins down the first divergent *decision*; the per-CT checksum lines (`run --checksums`, `ticks`; R5a.1) pin down the first divergent *tick* and what differs there. Saving no longer changes the game (R5a.2), so `--autosave-ms` runs are comparable too |
| `eedata.py check` / `cppgate`, `EEDataDump` | Text -> records, content dump parity (floats as bits) | A Rust text compiler is gated by an identical dump and identical record bytes |
| `tools/eedata/specs.json` (38 tables, typed fields with offsets and sizes) + `cppgen.py` | Generates the C++ specs and `EEWorldConstants.h` | Add `rustgen.py` with the same `--check` gate: generated `#[repr(C)]` + serde structs, never hand-maintained twice (MODDING_PLAN §3) |
| `eemodel.py cppgate` | glTF/PNG -> .cem/.sst bytes, C++ vs Python | Gates a Rust model loader the same way |
| `tests/platform` (plat_test, font_test), `tools/gx_test`, `tools/sdl_test`, `tools/tcp_test` | Self-checks of a C API, exit code = failures | Run unchanged against the Rust implementation (link swap) |
| `tools/record_sizes.py`, the `static_assert` pins | Record sizes | Mirror each assert in Rust (`const _: () = assert!(size_of::<T>() == N)`) |
| `tools/harness/posix_game.py` + `EETestFixture.cpp` | Live game through the UI fixture | The layout-independent live harness; port its Windows half before R5 |
| `tests/won` (`xbuild.py wontest`: `won_wire_test`, `won_crypto_test`, `won_socket_test`) | WON's wire format byte for byte, its socket layer over loopback | Gates R1's PlatSocket and any Rust WON codec. Not part of `gates.sh` today. `won_gui_test.cpp` (the GUI backend's GDI rules, offscreen) is in the tree, but `wontest` does not build it |
| Harness `--exercise lobby` (`--refuse-autodial`) | Main menu -> MultiPlayer -> the lobby's frames -> Other Multiplayer -> the LAN screen | Smoke test of a lobby replacement. **It plays no game:** a LAN or Direct Connect game between two instances through the lobby is still untested (`mp_game.py` bypasses the lobby), and that test must exist before the lobby moves |
| wgpu-native in the exe (`tools/build.py:127`) | | **Precedent:** a Rust staticlib already links with lld-link and `/MT`, with the system libraries Rust std needs |
| MODDING_PLAN / AI_RESEARCH choices | | JSON-shaped canonical data "read by C++ and Rust alike"; strict YAML; `f32::from_str` float parsing; Lua through mlua; deferred caps (16 players, 6 armour kinds, 7 resources) planned for Rust |

---

## 4. Interop mechanics

**One Rust static library.** `wgpu_native.lib` already carries a copy of Rust std. A second, independent Rust
staticlib brings another copy, and the build rejects duplicate symbols. So:

- `rust/` is a cargo workspace with one `staticlib` crate (`ee_rust`) that depends on every Rust crate;
- in R0 it absorbs wgpu-native: pinned to the v24.0.3.1 tag as a git dependency, built from source, so the
  webgpu.h ABI GX uses is unchanged;
- later, GX in Rust uses the `wgpu` crate directly and the C API goes.

Before R0 lands, verify that a dependency's `#[no_mangle]` exports survive into the staticlib (they should, because
a staticlib carries all object code of its dependencies), and that the link reports `0 unresolved, 0 duplicates`.

**Profile:**

- `panic = "abort"`, plus a panic hook that writes to the game log and `ESRecordNativeError`, because a GUI exe has
  no stderr (PORT_LEARNINGS "wgpu errors are Rust panics");
- `-C target-feature=+crt-static` on MSVC; `rust-toolchain.toml` pins a stable version;
- `opt-level` matched to the C++ `-O1` for comparability, then free;
- a baseline `target-cpu` (no FMA) for the sim;
- no nightly, so no Rust ASan: the macOS ASan builds instrument only the C++ side. The Rust side gets
  `cargo test` and Miri on its pure crates.

**Binding style, per seam:**

- **Default: plain C ABI.** `extern "C"` in Rust, a header generated by cbindgen and checked in with a
  staleness check, like `cppgen --check`. It matches the existing seams and keeps the exported names stable for the
  harness, and the MS-dialect C++ (`-fms-compatibility`, delayed template parsing) never meets generated C++.
- **cxx** only where an interface really is C++ objects for a long stretch: `UString`, `std::vector` members of
  `EERandomMap`, `GESurface` reference counts. It is the mature choice (KDAB's 2026 comparison). Its generated C++ is
  standard and compiles with clang-cl; verify it once with the dialect flags.
- **Not autocxx/Crubit:** they bind arbitrary C++ headers, and these headers are MS-dialect, macro-heavy and
  reverse-engineered. Crubit also expects Google's build system.
- **C++ trampolines** for the abstract engine classes: a C++ class implementing `GERasterizer` whose methods call
  `extern "C"` Rust functions with an opaque `this`.

**Build integration:**

- `tools/build.py`: `cargo build --release --target x86_64-pc-windows-msvc` before the link, the `.lib` next to
  wgpu in `static_libs()`. `/NODEFAULTLIB` already drops Rust's default-library directives.
- `tools/xbuild.py`: the same, for `x86_64-unknown-linux-gnu` (cross from Windows with the Debian sysroot: cargo's
  linker = clang + `--sysroot`) and `aarch64-apple-darwin`.
- CI: a rustup step. The gates gain `cargo test`, `cargo clippy -D warnings` and the cbindgen staleness check.
- **Sources are staged under `C:\EEX\`.** Cargo builds from `rust/` in place with its own `target/` under `build/`
  (per worktree; `EE_ORIG_SRC_ROOT`'s rule applies to `CARGO_TARGET_DIR` too).

**Allocation:** whoever allocates frees. A Rust object given to C++ is an opaque pointer with an explicit
`*_destroy`. A C++ object seen from Rust is an opaque handle, never a struct mirror, unless it is a pinned record.

---

## 5. The plan

Each step: one concern per commit, the CLAUDE.md gates, a PORT_LEARNINGS entry, the C++ it replaces deleted in the
same branch (no long-lived dual implementations beyond the comparison period).

**Everything written in Rust is unit tested (the owner's rule).** Every module lands with its tests, in the same
commit, in every phase:

- Tests cover each module's public behaviour, its edge cases and its error paths, and run under `cargo test` with no
  game, data tree or window. Code that seems to need those is split so that its logic can be tested alone, with the
  I/O or the FFI as a thin layer around it.
- Behaviour carried over from C++ is pinned by golden values captured from the C++ before it is deleted (RNG
  sequences, float-to-int rounding, record bytes, wire messages, map dumps).
- A bug fix comes with a test that fails without it.
- The oracles in section 3 (the sim trace, `cppgate`, the tours) are additional. They show that the whole game still
  agrees; they do not replace a module's tests.

### R0: the Rust library (foundation)

- `rust/` workspace, `ee_rust` staticlib, wgpu-native from source at v24.0.3.1, panic hook, cbindgen, build.py /
  xbuild.py / CI integration.
- Gates: both Windows builds link 0/0; the full gates (`gates.sh`); a macOS build plus an ASan `ai-game` run;
  `gx_test` and `sdl_test` pass. The exe behaves identically: same AI trace in EE-SIM.

### R1: platform leaves (prove the seam on the easiest code)

1. **PlatConfig** (7 functions, 1 caller, its own YAML subset, tests exist): Rust's first real code. The parser is a
   hand-written strict subset; no `serde_yaml`, which is archived.
2. **PlatPath, PlatString** (23 functions, 42 callers; the multibyte `_mbs*` semantics need care), **PlatTime,
   PlatMessage.**
3. **PlatFile, PlatThread, PlatSocket:** one Rust implementation for all three systems in place of
   `src/Platform/{Win32,Posix}`, with the Windows-only differences in `cfg(windows)`.
   - Constraint: the harness's time shim hooks the Win32 imports PlatTime calls, so the Rust PlatTime must call the
     same Win32 functions until the harness clock moves to PlatSim (section 5 pre-work).
   - WON is now a large PlatSocket and PlatThread client: `PlatSocketWait`/`GetError`/`Shutdown`, the socket options
     and thread wait were added for it, and its `WSAE*` names are the `PLAT_SOCK_*` values, so those numbers are
     part of the contract. PlatString's `PlatCp1252ToWide`/`PlatWideToCp1252` move with step 2.
4. **PlatWindow** on winit (PORT.md already names it) or kept on SDL3 through `sdl3-sys`. Decide on the evidence:
   SDL3 works on all three systems today.
5. **PlatFont** (stb_truetype -> `fontdue`/`ab_glyph`, or `swash`). The lobby's text goes through it too
   (`PlatGUIFont.cpp`, rasterised at size x the pixel scale). Gate: font_test and menu and lobby screenshots within
   the hinting tolerance.

Gate: `tests/platform` and `xbuild.py wontest` on all three systems with the Rust implementation, the tour,
`--exercise lobby`, an AI game.

### R2: the data layer

1. Pre-work: a machine-readable **rules schema** (today it exists only as C++ name tables in `EERules.cpp` and
   `tools/eedata/rules.py`). This is MODDING_PLAN's JSON Schema step, and both languages then generate from it.
2. `rustgen.py`: the 38 table specs -> `#[repr(C)]` record mirrors plus serde text structs, with `--check`.
3. **The text compiler in Rust** (`EEDataText`, `EEDataTextFile`, `EERules`, `EEDataDump`; ~4k lines). C++ calls
   `ee_data_compile(table, text, out)` and receives the same record bytes the C++ compiler gives.
   - Gates: `cppgate` (dump and byte parity) and `eedata.py check`.
   - Then the mod layer (MODDING_PLAN phase 3) is written in Rust from the start.
4. **Open formats:** `GEOpenFormats` (glTF -> .cem bytes, PNG -> .sst) in Rust (crate `ee_openformats`: hand ports of
   cgltf's and stb_image's readers, not the `gltf` and `png` crates, which accept and refuse differently), replacing
   cgltf and stb_image. Gate: `eemodel.py cppgate` (Rust vs Python and the C++ golden digests; done, see PORT_LEARNINGS
   "R2.4"; full-game gates not re-run).

### R3: engine services and plug-ins

Ordered by isolation:

1. **Mixer plug-in** (36 `SPIM*`; cpal or keep miniaudio through FFI). The first PIM plug-in in Rust.
2. **Compression (PKWARE DCL) and UEncryption:** pure, tiny, golden-testable against the save files.
3. **TCP Protocol plug-in** (33 `NPIM*` on PlatSocket). `tcp_test` is the gate; a C++ shim converts `UString`.
4. **Random map generator** (`Default`, 5.7k, deterministic). The entry points mutate `EERandomMap`, a struct of
   vectors and `UString`s.
   - Invert the interface: Rust generates into flat arrays that a C++ adapter copies into `EERandomMap`.
   - Gates: a map dump identical for every map script x seeds; the sim trace unchanged.
   - The `.rmv` script reader (`EERandomMap.cpp`) follows.
5. **File System:** `FSArchive`, `FSChunk`, `FSMemoryFile`, behind the existing classes (fan-in 911 includes, so
   the C++ facade stays until R6).
6. **Sound Engine** (SE*), then music (dmusic stays C until a Rust DirectMusic player exists) and movies (pl_mpeg ->
   a Rust MPEG-1 decoder or keep C).

### R4: the renderer

1. **GX in Rust on the `wgpu` crate (done: `rust/crates/ee_gx`).** GX's contract is ~60 methods with DirectX 7 semantics: surfaces with
   CPU/GPU copies, colour keys, the fixed-function WGSL shader. C++ keeps `GX.h` as shim classes. The webgpu.h C
   API and the wgpu-native pin go.
   - Gates: `gx_test`, the tour's screenshots, `EE_GX_STATS` summaries.
2. **The DX7 renderer plug-in** (9.7k; in progress on `ee_dx7`, PORT_LEARNINGS "R4.2") behind `GERasterizer`/`GEScreen`/`GESurface`/`GEVertexArray`
   trampolines. Then a choice: port it faithfully, or redesign the renderer in Rust from the GX contract
   (PORT_LEARNINGS "GX"). Either way the lobby's path stays: a system-memory surface with a 1024x768 logical size over
   the frame's pixels (`GESurface::SetLogicalSize`), letterboxed onto the back buffer by `RTSGraphics::RenderLobby`.
3. **The Graphics Engine** (GE*: models, textures, fonts, cameras; 17k) last in this phase. It is shared with the
   game's renderer (Graphics Thread), so it may move with R5's renderer decoupling instead.

### R5: the simulation

Two stages. The first is in C++ and changes no behaviour (each step gated by the sim trace); the second is the move.

**R5a. Untangle in C++:**

1. **Per-CT checksum trace** in EE-SIM: `GetOutOfSynchChecksum` and the RNG seed every CT, and an
   `ai_trace.py`-style compare. The oracle then names the first divergent tick, not only the first divergent
   decision. (Done: `EE-SIM --checksum-trace`, `sim_oracle.py ticks`; PORT_LEARNINGS "R5a.1 and R5a.2".)
2. **Make saving free of side effects** (`EECPCivilizationManager::Save` set `mBuildingCountsChecked`;
   PORT_LEARNINGS "The headless simulation"), so games with autosaves stay comparable. (Done: that one and three
   more, in the gather goal, the wall manager and the defense commander.)
3. **The renderer reads a snapshot,** not the live world under the read lock. Graphics Thread gets a per-frame view:
   positions, animation states, selection. This cuts the Graphics <-> Units/Physics edges and is the precondition
   for Rust-owned units. It also lets the physics tick stop sharing reference counts with the render thread.
4. **Handles instead of pointers** at module boundaries: the AI, the UI and the editor hold unit ids (the `USavedID`
   machinery). Measure `EEComplexUnit*` across module boundaries first.
5. **Name the address-named globals** and fold the reachable ones into context objects (the world, the physics).
6. **The RTS base layer stops including the game** (146 edges RTS -> Graphics Thread): merge RTSWorld/RTSUnit into
   their EE subclasses, or invert the edges.
7. Move the gates to layout-independent observation (section 2.4): the Windows fixture driver for `gates.sh`, and the
   harness clock through PlatSim instead of import hooks.

**R5b. Port, leaves first, each behind a C ABI with plain-data inputs and outputs, gated by the trace:**

1. **Formations** (1k: geometry in, slots out), then **A* pathfinding** (5.9k) over a terrain/occupancy grid
   snapshot, then the continent and choke-point analysis.
2. **Combat and unit formulas** (the rules documents are already data: `combat.yaml`), the tech tree and events
   (dbevents/dbeffects are already a small effect language), calamities.
3. **The computer player** (63k). This should meet the AI rework:
   - the AI already runs inside the physics tick and talks to the world through managers;
   - AI_RESEARCH plans a narrow, C-shaped script API (Lua through mlua);
   - the new-AI work could target that API and Rust instead of growing the C++ managers. See decision D2.
4. **The world and the units themselves:** Rust owns the unit store (struct-of-arrays or an ECS, ids as handles),
   the physics tick, the command queue (`NEMessage` decode in Rust) and the save/load codec. The C++ that is still
   left (UI, editor) reads through the snapshot and the id API. This is where the 638 `dynamic_cast`s turn into
   unit-kind data.

   Lifting the deferred caps (16 players, 6 armour kinds, 7 resources: MODDING_PLAN §4) belongs here, as rebaselines.

#### R5b.4: the map (measured 2026-11-10, branch `rust-r5b4-core`)

**`dynamic_cast` sites** (657 in code under `src/` and `include/`, 683 with comments; the 638 above is an older
count), by the class they test:

| Tested class | Sites | Becomes |
|---|---|---|
| unit goals `EEUG*` (13 classes; `EEUGAttackUnit` 58, `EEUGLocation` 34, `EEUGGather` 27) | 191 | the goal kind: `EEUnitGoal::mKind`, set by each constructor, never written after (`EEUGHealUnit` 15 derives from `EEUGLocation` 1) |
| unit objects `EEComplexUnit` 81, `EEUnit` 8 | 89 | the object kind (simple or complex), fixed at construction |
| unit classes `EEUC*` (`EEUCGate` 24, `EEUCTransport` 17, `EEUCBasicCitizen` 7) | 57 | the class kind: `CreateUnitClasses` builds the class from the record's category alone, and `mCategory` is written only by the constructor |
| players `EEComputerPlayer`, `EEHumanPlayer`, `EEPlayer` | 12 | a player kind |
| AI `EECP*`, `EEAI*` | 55 | held with the AI (R5b.3) |
| UI toolkit and in-game UI (`UI*`, `EEUI*`) | 121 | R6 |
| network (`EEServer`, `EEClient`, `NES*`, `EEMessage`) | 27 | the communications role; the message kind by type |
| other (WON 49, persistent helpers 17, input keys, formations, effects, calamities) | 105 | per module |

By module: AI Thread 162, Units 150, UI Events 92, WON 50, Physics Thread 29, Communications 26, User Interface
Thread 26, Empire Earth 24, Civilization Builder 20, Persistent 17, Scenario Manager 16, Graphics Thread 14, the rest 31.
The three unit tables (goal, object, class: 337 sites) are the simulation's; 97 of them are in the AI Thread.

**Commands** (`NEMessage`):

| | |
|---|---|
| Type ids | 108 game messages (`EEMessageID`, 0x41..0xfd, factory `EEMessage::BuildEEMessage`) and 28 engine messages (`NEMessageID`, 0x20..0x3c, `NEMessage::BuildNEMessage`) |
| Codecs | 133 classes with `ExportMessage`/`ExportSize`/`ImportMessage` (312 functions): 104 game classes in `EEMessage.cpp` (8,961 lines with the `Execute` bodies), 29 engine classes in `NEMessage.cpp`. 94 carry fixed fields only; the others carry wide strings (`UWideString::Export`: u32 length + 2, the bytes, u16 0), the compressed box selection (`CompressUnits`/`DecompressUnits`), vectors (`EEMSelectTheseUnits`, `EEMCivUpgrades`) or nested messages (`NEMPlayerInput`, `NEMCommunicationsTick[Response]`) |
| Encoded | `NEMessageManager` (the send queues), `NESynchronousClient::Send`, `NESynchronousServer`, `NETimestampedMessage`, `NERecordPlayback`, `NEMPlayerInput` (nested) |
| Decoded | `NEMessage::CreateMessage` -> `sConstructors[type]` (the two factories), also inside `NEMPlayerInput::ImportMessage` |
| Sent | 114 `new EEM*` in 47 files: UI events 11 files, the user interface thread 9, actions 6, the editor's actions 3, the computer player once (`EECPWallManager`: walls and gates), EE-SIM's save |
| Executed | `EEPhysics::DrainInput`, per communications tick: the selection types -> `EEMessage::ExecuteSelectionMessage`, the rest -> `ExecuteMessage` (the selection id check) -> `Execute`; engine messages -> `Execute` |

**Order of the leaves:**

1. The game messages' codec: the wire layout of the 104 game classes in Rust (`ee_netmsg`), the C++ classes keeping
   their members and calling it; `CompressUnits`/`DecompressUnits` with it. Gate: an NM** capture of every message the
   games import and a sweep of every class with edge values (this branch).
2. Unit kinds as data: the goal, object and class kinds as Rust tables, the `dynamic_cast` sites as kind queries,
   the simulation's first (this branch).
3. The engine messages and the nested player inputs into the codec; the message kind (game or engine) by type.
4. The `Execute` bodies' decisions (the selection id check, resource arithmetic, the box selection's pick) over the id
   API.
5. The unit store: each unit's plain state (position, hit points, the goal queues as ids) in a Rust struct of arrays
   behind the `EEUnit` accessors; the out-of-sync checksum over it.
6. The physics tick (`EEPhysics::Update`, `DrainInput`, the units' steps) over the store.
7. The save codec, object by object, the 0.256 field order as the contract.
8. The other casts with their modules (AI in R5b.3, UI and WON in R6); then the caps, as declared rebaselines.

**Leaf 4, the `Execute` bodies (measured 2026-11-10):** 108 `EEM*::Execute` bodies in `EEMessage.cpp`, 2,204
lines (the largest `EEMSetRallyGoal` 156, `EEMSetAirportCommand` 104, `EEMPayTribute` 94, `EEMLoadTransport` 72,
`EEMCastCalamity` 69, `EEMPathTo` 68). 67 find the sender by slot (`GetPlayerBySlot`), 44 look units up by id
(`gWorld.GetUnit`), 43 work on the sender's active group, 18 hand out goals, 18 touch resources, 19 call the UI
(`gUserInterface`, the local player only) and 12 the communications object. The two entry points
(`ExecuteSelectionMessage`, `ExecuteMessage`) are the selection id check: under the fast-selection lock, a command
runs only when its selection id equals the sender's last one (`mFastSelection[player]`), else the sender's active
group is reset to the selection; `EEMGameRestarting` always runs, on a client `EEMMigrationSlotInfo` too. In order:
(a) that check and the id bookkeeping as a Rust decision over plain values (the ids, the type, server or client),
the locks and the group copy staying C++; (b) the bodies that only read ids and members and pick (the resource
arithmetic of `EEMAddRemoveResources`, `EEMExchangeResources`, `EEMPayTribute`; the box selection's pick), behind
the unit id API; (c) the goal-giving bodies with the unit store (leaf 5). The bodies that call the UI stay C++
until R6. Gate: the oracle and the capture, plus an NMEX-style record of each body's inputs and decisions.

**Leaf 4, done and left (2026-11-10, third round):** (a) and (b) are Rust (`ee_netmsg` execute.rs; gate NMDC). (c)
waits for the unit store: 15 bodies split the selection by category (`EEActiveGroup::Fill` for 1..5), spread it,
draw one `GetRandom` for the acknowledging unit and give goals (AddPatrolPoint, AttackGround, AttackUnit, BuildUnit,
BuildWall, CastCalamity, DropoffResource, Explore, GatherResource, HealUnit, PathTo, Patrol, RepairUnit,
SmartFarmers, UnloadTransport); 18 construct goals, 24 look units up by id, 22 call the UI (C++ until R6). Their
decisions read units' plain state between world calls that change it, so they need: each unit's plain state by id
from Rust without a call into C++ (category, hit points, built, type, tile, goal kind), the active group as an id
list, goal creation as a host call returning a handle. The box selection's pick (`EEPlayer::SelectUnits`) is the
first once the store exists.

**Leaf 5, the unit store, stage one: the design (2026-11-10, fourth round).** What the store is for first: the
decisions of the selection pick and the 15 goal-giving bodies read units' plain state by id between world calls that
change it, so the state must be current at every point of the tick, not only between ticks.

- **Where the plain state lives.** A Rust-owned record per unit (`ee_units` store.rs, `UnitRecord`: the owner's
  player id, the object kind, whether the unit is its own complex unit, valid, hit points, the tile, the class's
  category and unit type, for a complex unit built and the current goal's kind), keyed by the unit id. Its lifetime
  is the player list's: inserted by `RTSPlayer::InsertUnit` (seeded whole from the object: a new unit, a loaded one,
  a kept id), removed by `RTSPlayer::RemoveKilledUnits` when the unit leaves the list (and by `RTSUnit::FreeNode`
  after a teardown), and a player's records dropped by `~RTSPlayer`, by the player object. That is exactly the domain of `RTSPlayer::GetUnit` / `gWorld.GetUnit` (the ids the bodies
  look up); the registry behind `EEUnitHandle<T>` keeps its own, longer lifetime (to `FreeNode`, where the UI
  thread's last `Dereference` may run) and stays the way to the object. Every store write happens where the C++
  member is written, under the world's write lock (the physics tick, the loads, the editor); the store has its own
  mutex as well, so a read from another thread is never torn.
- **How it stays current: the members become write-through holders.** Each mirrored C++ member becomes an
  `EEUnitStoreField<T, F>` (ee/EEUnitStore.h): the same size and layout as `T` (offsets, `memcpy`, the save code's
  bytes unchanged), read implicitly as `T`, and every assignment form (`=`, `+=`, `-=`, `++`, `--`, holder to
  holder) stores the value and reports it to the store when the unit is in it (`RTSUnit::mInUnitStore`, set by the
  insert, cleared by the removal). The compiler finds every writer (no reference or pointer to a holder converts to
  `T&`/`T*`; a load reads through `EEUnitStoreRead`; a holder is not copyable), so no write but a `memcpy` can bypass
  the store (the hit points' load, before the insert). Derived fields
  are reported at the write that changes them: the class's category and type at `mUnitClass` (a class's own
  category and type are written only by its constructor), the goal's kind at `mGoalState` (fixed by the goal's
  constructor), the complex flag at `mComplexUnit`.
- **The alternative, a per-call view** (C++ fills plain records of the units a decision reads, before each call):
  nothing at the writes, but every decision needs its own filling code, a view goes stale at the first world call
  that changes a unit (`Fill`, `ClearGoal`, `SetGoal`) so an interleaved body must refill, and the C++ members stay
  the truth forever. The holders cost one store call per write and a second copy of each value until the members go;
  they buy a store that is current everywhere and a path to deleting the members: once every reader of a field
  reads the store (Rust, or C++ through `EEUnitStore` accessors), the holder drops its inline value and becomes an
  accessor over the store, and the member goes when the save codec writes from the store (leaf 7).
- **Determinism.** The game only looks records up by id (a `BTreeMap`: no hash order anywhere, and the only walk,
  the check's and a player's reset, is in id order and observes nothing the game reads). The game's own orders (a
  player's list, a group's vector) stay the C++ containers' until the tick moves; then the store gains the per-player
  order as an id list kept at the list's insert and unlink, never the map's order. A write is a plain store: no
  random draw, no float operation (the values are copied as they are, hit points and tiles as the C++ `long`).
- **Saves.** The store is not saved: the format is the save work's (0.262) and does not change. A load rebuilds it,
  since every loaded unit passes `InsertUnit` (seeded from the object as loaded) and the Restore pass writes the
  pointers (`mGoalState`, ...) through the holders.
- **Handles and the store.** A unit handle stays the id; the store is the second thing an id resolves to (the plain
  state, for decisions), the registry the first (the object, for world calls). Rust decisions read the store and
  hand ids back; C++ resolves them for the world calls. Goals have no ids: a Rust decision gets a goal as an opaque
  handle from a host call (`new EEUG*` behind it, one reference owned by the handle), hands it to the host's group
  calls and never dereferences it.
- **The gate.** TEST ONLY, with the simulation capture on: after every physics tick and before every game message
  the physics executes, C++ walks every player's list and hands every unit's fields to the store, which compares them
  with its records and counts the records (a USCK record per check: the units compared, the mismatches; a mismatch
  prints the unit and field and stops EE-SIM with exit code 3). So the store equals the objects on every unit at every
  point where a decision reads it, over the oracle games and the four capture profiles.
- **The path to the tick.** Stage one: the fields above, the active group as ids, goal creation as a host call, the
  pick. Then the fields the bodies' helpers read (the formation slot, mobility, the resource amount), the 15 bodies;
  then positions and the out-of-sync checksum over the store (leaf 5's end); then each unit step moves with the
  fields it writes, the holders turning into accessors as their last C++ readers go, and the physics tick (leaf 6)
  iterates the store's per-player orders.

**Leaf 5, stage one, done (2026-11-10, fourth round).** The store is `ee_units` store.rs (records by id: owner and
player object, object kind, complex flag, valid, hit points, tile, category, unit type, built, goal kind), kept by
eight holders (`RTSUnit::mTileX`, `mTileY`, `mIsValid`, `EEUnit::mUnitClass`, `mComplexUnit`, `mHitPoints`,
`EEComplexUnit::mIsBuilt`, `mGoalState`; ee/EEUnitStore.h); the USCK check found no difference over the 24 capture
games (every tick, every executed message). The world calls are `ee_units` host.rs (a group's units as ids, goals
from an `EeGoalSpec` as owning handles; six constructors), not called in game yet. The box selection's pick is Rust
(`ee_units` select.rs over the store; USPK equal to the C++ pick's goldens in the four profiles); only a played game
reaches its game call. Next: the first of the 15 bodies (Explore is the smallest: the host calls of `EEActiveGroup`'s
`Fill`, `KeepMobileUnits`, `ClearGoal`, `SetGoal`, `Plan`, the pending list, one `GetRandom` draw and the
acknowledgement), then the fields their helpers read (PORT_LEARNINGS "R5b.4, fourth round").

**Leaf 4 (c) and leaf 5, fifth round (2026-11-10).** `EEActiveGroup::Fill`'s test (valid, hit points, the formation
slot) is Rust over the store (`ee_units` group.rs; the store gained the class's domains and land-building flag), for
every `Fill`; its gate USFL (every call, every player's units in every slot every five game minutes) equals the C++
test's goldens. Explore is the first body in Rust (`group::explore` over the host calls of EEUnitHost.cpp: the active
group, `Fill`, the pending list, `KeepMobileUnits`, `ClearGoal`, the goal, `SetGoal`, the action, `Plan`, the draw,
the acknowledgement). No capture game sends a group command (the human idles, the computer players send none), and a
body cannot run in a sweep (it creates goals and gives them to units), so the bodies' call order is gated by unit tests
over a mock host and only a played game runs them. Next, in order: Patrol and AttackGround (the same shape: Fill per
slot, a goal from the host, Plan, the draw), then the bodies that spread a group (`SpreadAroundTile`: the formation
slot), then those that read a target's state (Gather, Repair: the resource amount, `CanTake`); `KeepMobileUnits`
(commandable, the stats' velocity: the stats objects change in place with upgrades, so not a holder) stays a host call.

**Leaf 4 (c) and leaf 5, sixth round (2026-11-11).** Attack Ground, Patrol, Gather and Repair are Rust (`ee_units`
group.rs `attack_ground`, `patrol`, `gather`, `repair` over the host calls of EEUnitHost.cpp, in the original's order
and draw count); the store gained the commandable flag, the formation slot, the class's ground attack flag and the
resource amount (USCK compares them), and KeepGroundAttackers' test reads it. The coverage is a sweep over a dry
host (TEST ONLY, ee/EEUnitHost.h): every host call has a branch that logs and answers without changing the world, and
every five game minutes the Rust bodies and the original bodies' statements (copied from EEMessage.cpp, world calls
through the same host calls, units read on the objects) run for every player over its units and a set of targets
and points; the logs must be equal (USGC, 780-2,340 records a game). The world calls themselves (Fill's sorts,
the keeps but the ground one, the spreads, SetGoal, Plan, the markers, the acknowledgements) are not run by the sweep:
only a played game does. Next, in order: AddPatrolPoint (KeepNonCitizens over the store: commandable, hit points,
category; the patrol groups' creation as host calls), PathTo (the formation slot), AttackUnit and HealUnit (the
targets' kinds), the others with the UI calls.

**Leaf 4 (c), seventh round (2026-11-11).** Add Patrol Point, Attack Unit, Heal Unit and Path To are Rust (`ee_units`
group.rs `add_patrol_point`, `attack_unit`, `heal_unit`, `path_to` over the host calls of EEUnitHost.cpp, in the
original's order and draw count; KeepNonCitizens' test reads the store), each gated by the USGC sweep (its original
statements next to the Rust body over the dry host) and mock-host tests; nine of the 15 goal-giving bodies are now
Rust. The UI fixture gives orders through the real input path (`order`: gather, repair plain and queued, the Patrol
button, Attack Ground, a move; fixture_game.py `--orders`) and judges every unit by the original's rule after the
message executed: the first run of these bodies' world calls in a played game (the lead's). The reconstruction's UI
sends no `EEMAddPatrolPoint` (the original's patrol-point mode is missing; its Patrol button sends `EEMPatrol(-1)`).
Next, in order: the store's on-the-map flag (the out-of-sync checksum's flag 1), BuildUnit and BuildWall (the late
profile's walls run them), DropoffResource and SmartFarmers, then the bodies with UI calls.

**The out-of-sync checksum, mapped (2026-11-11).** `EEPhysics::GetOutOfSynchChecksum` sums, over `gWorld.mUnits` (the
units on the map's tile lists, not the players' lists: the walk follows `mTileNode->mNext`), each unit's
`GetChecksum(flags)` (0 for a simple unit; for a complex one: flag 1 hit points + tile x + tile y, flag 2 the times
updated, the physical model's location and height as float bits, whether it has an active group, its command's type,
flag 0x20 its goals' checksums, flag 8 its area effects'), then per player the selection (0x200) and the air traffic
(0x40), the tick, the seed, the inputs processed, the AI's checksum, the map (4), the continents (0x100) and the
calamities (0x10). The sums wrap and commute, so the walk's order does not matter. Over the store today: flag 1 only,
and only with an "on the map" flag per record (a unit is in `gWorld.mUnits` while its tile node is linked:
`RTSWorld`'s tile list insert and removal): the two domains differ at least for units that are in a list but have
no tile node (a unit inside a transport or building has tile -10000; not yet measured). Flag 2 needs the positions, the times updated, the active group and the command (the physics tick's
fields: leaf 6), 0x20 the goals (save and AI work). So the checksum moves in two steps: flag 1 over the store with the
on-map flag (the oracle's `oos=` gates it), then the rest with the tick.

### R6: UI, events, editor

The UI toolkit (14.4k, forms from the UI db tables), the in-game UI (36.5k) and the scenario editor and manager
(39.5k). These are large and not deterministic: their gates are the tour, the fixture-driven menu checks and
screenshot comparisons. Port the toolkit faithfully (the forms are data); an immediate-mode replacement is a design
change, not a port.

**The WON lobby** is a separate job with its own toolkit: WONGUI is not the UI* toolkit, and porting one does not
port the other. It blocks nothing in R0-R5. It only needs its platform calls (R1), its surface path (R4) and
`EEServer`/`gCommunications` to stay reachable from `EELobbyInterface.cpp` (R5). The scope is decision D8.

Pre-work, whichever way D8 goes:

1. A LAN and a Direct Connect game between two instances through the lobby, as a harness exercise (PORT.md "Other next
   steps"). It is the only oracle for what the lobby must keep doing.
2. A run on Linux and macOS through the lobby's screens.
3. The `llvm-nm` surface of the WON objects (section 1.2), which also shows how much of the 32k service code the LAN
   path pulls in. `LanLogic` uses `AuthContext` and the peer-auth ops, so some of WONAuth and WONCrypt is on that
   path even without the online services.

### R7: the end

- `main` in Rust;
- the last vendored C (zlib/libpng/libjpeg -> `flate2`/`png`/`jpeg-decoder`, miniaudio -> cpal, dmusic, pl_mpeg)
  replaced or kept behind `-sys` crates (WON's PNG and JPEG decoders use the same codecs, so they go first or
  together);
- the clang toolchain becomes optional, then gone;
- build.py and xbuild.py become `cargo build`, and the harness keeps only its layout-independent drivers.

### Size and order of value

| Phase | C++ replaced | Main risk | Value even alone |
|---|---|---|---|
| R0 | none | linking (std duplication) | the pipeline |
| R1 | ~8k | Win32 hook compatibility with the harness | one platform implementation for 3 systems |
| R2 | ~5k | none (strong gates) | the mod layer in Rust |
| R3 | ~25k | interface inversion | plug-ins and services |
| R4 | ~30k | D3D7 semantics | the renderer on the wgpu crate; no webgpu.h pin |
| R5 | ~190k | determinism, ownership | the game |
| R6 | ~90k, plus WON's 131k ported or retired (D8) | fidelity (no exact oracle) | |
| R7 | rest + vendored C | | total Rust |

---

## 6. Decisions for the owner

- **D1. Bit-exact policy** (section 2.1): exact while porting, rebaseline only in its own commits. Proposed: yes.
- **D2. Where new AI code goes.** The new-ai branch grows the C++ managers now. Options:
  - (a) keep C++ and port it in R5b.3;
  - (b) give the new AI a narrow API now (AI_RESEARCH's script API) and write new managers in Rust behind it;
  - (c) Lua first, as planned, which survives the port as is.

  Proposed: (c) for tuning, with (b) as the host of the API.
- **D3. Binding style:** C ABI + cbindgen by default, cxx only where named. Proposed: yes.
- **D4. Window and font backends in R1:** keep SDL3 (proven on three systems) or move to winit. Proposed: keep
  SDL3 through `sdl3-sys` until R7.
- **D5. Renderer in R4:** a faithful port of the DX7 plug-in, or a redesign from the GX contract.
- **D6. Text encoding: decided (owner, 2026-11-09): UTF-8 all the way.** Game text becomes UTF-8 in memory, in
  files and in saves; Windows-1252 survives only as an import conversion for old inputs. It is a game change, done as
  its own track, not mixed into a port: the data converters write UTF-8, `UString` holds UTF-8, PlatFont and the
  text layout work in code points, saves get a version gate (older saves converted on load), and WON's UTF-16 wire
  strings convert at the wire. **Done** (branch `rust-d6-utf8`, the Progress row "D6"; PORT_LEARNINGS "D6: game text
  is UTF-8"): ported Rust keeps game text as bytes, now UTF-8 behind `ee_plat_core::utf8`.
- **D7. Lockstep across systems:** with `-ffp-contract` unset, arm64 may already disagree with x86-64. Decide
  whether cross-system multiplayer is a goal; if so, set `-ffp-contract=off` now and add a Mac-vs-Windows sim trace
  comparison.
- **D8. The WON lobby: decided (owner, 2026-11-09): port all of it for now (option a), except the HTML rendering**
  (the WONLobbyHTML* segments and parsers, HTMLParser2, ~6k lines), which is not ported: where the lobby shows HTML
  (news, help, messages) it shows plain text instead. The online services are ported with the rest even though their
  servers are gone.

- **D9. Open formats only: decided (owner, 2026-11-09).** The game reads glTF models, PNG textures and the text db
  tables (CSV/YAML) only. The binary forms are retired at run time: `.cem` models (current and legacy), `.sst`/`.tga`
  textures and the binary db records. The models and textures load natively (glTF -> GEModel, PNG -> texture) instead of
  through in-memory `.cem`/`.sst` bytes, and their loaders and savers are deleted rather than ported. The binary data
  tree leaves the gates; the importers that turn the retail installs into the open tree (tools/open_data.py,
  tools/eedata, tools/eemodel) keep reading the binary forms, since that is where the open data comes from.

## 7. First steps

1. R0 on a branch: the workspace, wgpu-native from source, the link at 0/0, CI.
2. The cheap oracles, independent of Rust and useful now:
   - the per-CT checksum trace (R5a.1, done);
   - the side-effect-free save (R5a.2, done);
   - `-ffp-contract=off` with a Mac/Windows trace comparison (D7).
3. PlatConfig in Rust (R1.1) as the first code across the seam.
4. `rustgen.py` from `specs.json` (R2.2): it costs little and makes every later phase's records free.
