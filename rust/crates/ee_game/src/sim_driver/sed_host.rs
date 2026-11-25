//! TEST ONLY: what the scripted scenario editor ([`super::sed_steps`], [`super::sed_digest`]) asks of the game's C++
//! objects: one host operation per C++ call or member (EESimEditor.cpp `ee_simcpp_sed`, `ee_simcpp_sed_text`,
//! `ee_simcpp_sed_float`: a one-statement case each), the step's run on the UI thread ([`super::sed_run`]), and
//! the editor's Rust objects (ee_screens) called directly.

use crate::gamecpp as gc;
use crate::screens::editor_form_ffi::{self as sefm, EeScreensSeForm};
use std::ffi::{c_long, c_ulong, c_void, CStr, CString};

/// The host operations (`p` the object, `a` and `f` the integer and float arguments, `s` a text). A pointer is
/// answered as an integer (a control as its `UIControl*`, an action as its `EEAction*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeSimDriverSed {
    // the editor form
    /// `mRust`
    FormRust = 1,
    ControlById,
    FormControl,
    FormControlCount,
    FormGroupCount,
    FormGroup,
    SelectedUnit,
    SelectedUnitId,
    FormSetLoadText,
    TriggerStoring,
    GroupIsEnabled,
    GroupEntityCount,
    GroupEntity,
    // the controls
    ShellOp,
    ShellOf,
    ControlTypeName,
    EventTypeName,
    DisplayMode,
    ControlId,
    ControlIndex,
    ControlType,
    ControlRect,
    ControlBgColor,
    ControlBgOpacity,
    ControlBorder,
    ControlStringTableId,
    ProcessEvent,
    IsRadio,
    IsCheck,
    IsButton,
    IsCombo,
    IsList,
    IsEdit,
    IsSlider,
    IsTextLabel,
    RadioIsChecked,
    RadioTextLabel,
    RadioCheck,
    CheckIsChecked,
    CheckTextLabel,
    CheckSetChecked,
    TriStateCheckState,
    ButtonGeText,
    ButtonUp,
    ComboList,
    ComboSelection,
    ComboSetSelection,
    ComboTextBoxType,
    ComboTextLabel,
    ComboChange,
    FileComboSelect,
    ListLineCount,
    ListData,
    ListSelection,
    ListChangeSelection,
    ListCopyData,
    ListGeText,
    EditGeText,
    EditLineCount,
    EditText,
    EditSetText,
    EditSetNumber,
    EditEnter,
    SliderValue,
    SliderMax,
    SliderSetValue,
    LabelGeText,
    LabelLineCount,
    LabelText,
    TextFgColor,
    TextIsTransparent,
    TextFont,
    TextText,
    FontHeight,
    BitmapGeBitmap,
    GeBitmapCorners,
    GeBitmapTexture,
    TerrainTypeCount,
    TerrainTypeSelection,
    TerrainTypeName,
    TerrainTypeTexture,
    TerrainTypeColorSet,
    TerrainTypePick,
    TerrainColorCount,
    TerrainColorSelection,
    TerrainColorSetSize,
    TerrainColorSetFirst,
    TerrainColorPick,
    AlignmentHas,
    AlignmentGet,
    BrushRect,
    BrushHighlight,
    BrushCentered,
    BrushSize,
    BrushSetHighlight,
    EdgeScrollEdges,
    LoadThreadPostDone,
    LoadThreadWaitDone,
    LoadThreadClearDone,
    UnitWallStart,
    UnitWallEnd,
    UnitWallBuild,
    UnitWallCancel,
    UnitGateTileX,
    UnitGateTileY,
    UnitGateDirection,
    UnitGateBuild,
    UnitGateCancel,
    // the actions and messages
    SeaNew,
    SeaAction,
    PaintNew,
    PaintSetArea,
    PostAction,
    ClickNew,
    TsClickNew,
    ClickSetMessage,
    ClickExecute,
    LassoNew,
    LassoSetSelect,
    LassoSelectInRectangle,
    QuickSaveNew,
    MRecord0,
    MRecord1,
    MouseSetPosition,
    // the world
    MapMesh,
    MapTile,
    MapGetTile,
    MeshZ,
    TerrainColorType,
    ViewportRect,
    WorldMaxPlayers,
    WorldPopulationLimit,
    WorldWondersToWin,
    WorldVictoryEnabled,
    WorldTeamsLocked,
    WorldCheatsEnabled,
    WorldImprovementsAllowed,
    WorldUsingCarbon,
    OptionsUnitLimit,
    OptionsWondersForVictory,
    OptionsVictory,
    OptionsLockTeams,
    OptionsCheatCodes,
    OptionsPlayerColor,
    OptionsPlayerB36,
    PlayerName,
    PlayerColor,
    PlayerCivilizationFile,
    PlayerResourceCap,
    PlayerEnemy,
    PlayerPurchasedUpgrades,
    PlayerAvailablePoints,
    TreeCurrentEpoch,
    TreeStartEpoch,
    TreeEndEpoch,
    ComputerActive,
    ComputerIsCheating,
    ComputerCanAttack,
    ComputerCanDefend,
    ComputerCanBuild,
    ComputerCanExplore,
    ComputerIgnoreOthers,
    ComputerCanGather,
    ComputerCitizens,
    NodeUnitPlayer,
    UnitIsEditorSelected,
    // the ghosts
    GhostsChanged,
    GhostCount,
    GhostAt,
    GhostCreate,
    GhostSetTileX,
    GhostSetTileY,
    GhostTileX,
    GhostTileY,
    GhostUnitType,
    GhostTileSize,
    GhostSetFarmSlot,
    GhostSetPosition,
    GhostSetTransformation,
    GhostFillSquare,
    GhostLocationToTile,
    GhostSetCanPlace,
    // the scenario manager
    SmTriggerCount,
    SmTriggerAt,
    SmConditionCount,
    SmConditionAt,
    SmEffectCount,
    SmEffectAt,
    SmPendingCount,
    SmPendingAt,
    SmObjectCount,
    SmObjectAt,
    SmAreaCount,
    SmAreaAt,
    SmTimedCount,
    SmTimedEffect,
    SmTimedTime,
    SmNextTriggerId,
    SmNextConditionId,
    SmNextEffectId,
    SmNextObjectId,
    SmNextAreaId,
    SmShowDebugIds,
    SmSetShowDebugIds,
    SmGetNames,
    NamesNew,
    NamesCount,
    NamesAt,
    NamesFree,
    SmFindCondition,
    SmFindEffect,
    SmFindObject,
    SmObjectIndex,
    SmGetTrigger,
    SmGetObject,
    SmGetArea,
    SmExportText,
    SmImportText,
    SmClearImportErrors,
    SmImportErrorCount,
    SmImportError,
    SmMediaSection,
    SmExportFileName,
    SmAddTimedEffect,
    SmAddPendingCopy,
    SmNewEntry,
    SmSortList,
    SmMoveEntry,
    SmDeleteEntry,
    SmDuplicateEntry,
    TriggerFields,
    TriggerName,
    TriggerText,
    TriggerConditionCount,
    TriggerConditionAt,
    TriggerEffectCount,
    TriggerEffectAt,
    TriggerGetConditionCount,
    TriggerGetConditionAt,
    TriggerGetEffectCount,
    TriggerGetEffectAt,
    TriggerSetIsOn,
    TriggerSetIsLooping,
    TriggerSetConditionDuration,
    TriggerSetName,
    TriggerSetText,
    WrapperValue,
    WrapperOperator,
    WrapperIsNot,
    WrapperDelay,
    WrapperEffectIndex,
    WrapperEffectOperator,
    ConditionFields,
    ConditionName,
    ConditionText,
    ConditionChatText,
    ConditionSetChatText,
    EffectFields,
    EffectName,
    EffectText,
    EffectMessage,
    EffectNewName,
    EffectFileName,
    EffectDescription,
    EffectSetDescription,
    EffectSetMessage,
    EffectSetNewName,
    EffectObjectCount,
    EffectObjectAt,
    EffectClearObjects,
    EffectPushObject,
    ObjectFields,
    ObjectName,
    ObjectUnitCount,
    ObjectUnitAt,
    AreaName,
    AreaText,
    AreaId,
    AreaSelectMode,
    AreaHasRectangle,
    AreaRect,
    AreaHasAnchor,
    AreaAnchorX,
    AreaAnchorY,
    AreaSetRectangle,
    AreaSetAnchor,
    // the files and the save
    StartupPath,
    SaveTextName,
    EditorSaveName,
    SpecNew,
    SpecSetDirectoryStartup,
    SpecSetFile,
    SpecOpenWrite,
    /// the spec's FOpen(s): its open mode ("r", "w") -> 0 opened
    SpecFOpen,
    SpecWrite,
    SpecGets,
    SpecPuts,
    MemNew,
    MemFree,
    MemLoad,
    MemLength,
    HeaderNew,
    HeaderLoad,
    ChunksNew,
    ChunksBuild,
    ChunksCount,
    ChunksAt,
    ChunksFree,
    PersistentTag,
    PersistentFree,
    ChunkNew,
    ChunkFree,
    ChunkSavePersistent,
    ChunkSaveToFile,
    ChunkLoadFromFile,
    // the civilization builder (sed_civ.rs)
    UiFormById,
    UiFormControlById,
    UiFormShellOf,
    UiFormTypeName,
    UiFormCount,
    UiFormStacked,
    UiPop,
    IsCivList,
    CivListUpgradesOnly,
    CivListShowPoints,
    CivListSortLines,
    CivListHeaderColor,
    CivListReset,
    CivListTabbedTo,
    CivListCopyData,
    CivListCopiedShowPoints,
    CivListCopiedColor,
    ControlTabable,
    ListAutoSelect,
    ListHasSelection,
    ListSelectionIndex,
    ListCopiedHasSelection,
    ListDrawCount,
    ListDataEquals,
    ProcessEventOn,
    CivEdit,
    CivTimer,
    CivBlink,
    CivName,
    CivAvailablePoints,
    CivMaxPoints,
    CivUnchanged,
    CivNoPointsAdded,
    CivPurchasedCount,
    CivPurchasedAt,
    CivDeferredCount,
    CivDeferredAt,
    EditorCivilization,
    PlayerCivilization,
    PlayerCivilizationChosen,
    PlayerCivilizationLocked,
    CivSave,
    CivLoad,
    CivApply,
    WideNew,
    WideText,
    UpgradesNew,
    UpgradesCount,
    UpgradesAt,
    UpgradesFree,
    NarrowNew,
    SetFreeCamera,
    SetAllowCustomCivs,
    SetScenarioGame,
    // the steps' action and end (sed_run.rs)
    C12ActionNew,
    C12DoneNew,
    C12Dereference,
}

use EeSimDriverSed as O;

/// `kControlTriggerRangeChanged`, `kControlTriggerSelectionChanged`, `kControlTriggerButtonUp` (lle/UIControl.h).
pub(crate) const TRIGGER_RANGE_CHANGED: c_ulong = 0x3e8;
pub(crate) const TRIGGER_SELECTION_CHANGED: c_ulong = 0x3eb;
pub(crate) const TRIGGER_BUTTON_UP: c_ulong = 0x3f0;

/// cbindgen:ignore
mod sys {
    use std::ffi::c_char;

    use super::EeSimDriverSed;

    extern "C" {
        // EESimEditor.cpp
        pub fn ee_simcpp_sed(op: EeSimDriverSed, p: usize, a: *const i64, f: *const f32, s: *const c_char) -> i64;
        pub fn ee_simcpp_sed_text(op: EeSimDriverSed, p: usize, a: i64, s: *const c_char) -> *const c_char;
        pub fn ee_simcpp_sed_float(op: EeSimDriverSed, p: usize, a: *const i64, f: *const f32) -> f64;
    }
}

// the editor's Rust objects (ee_screens; their declarations with plain pointers)
pub use screens_sys::*;

/// cbindgen:ignore
mod screens_sys {
    extern "C" {
        pub fn ee_screens_segr_tab(g: usize) -> usize;
        pub fn ee_screens_segr_parts(g: usize) -> usize;
        pub fn ee_screens_segr_call(g: usize, slot: i32, x: f32, y: f32, p: usize);
        pub fn ee_screens_sets_call(tab: usize, what: u32, a: u32, x: f32, y: f32, p: usize);
        pub fn ee_screens_sets_control(tab: usize, what: u32, a: u32) -> usize;
        pub fn ee_screens_sets_sub(tab: usize, k: u32, selection: *mut i32, has: *mut bool) -> usize;
        pub fn ee_screens_sets_sub_op(tab: usize, k: u32, what: u32, a: u32, b: u32) -> u32;
        pub fn ee_screens_sets_sub_call(tab: usize, k: u32, slot: i32, x: f32, y: f32);
        pub fn ee_screens_seun_call(tab: usize, what: u32, a: i32, x: f32, y: f32, p: usize);
        pub fn ee_screens_seun_get(tab: usize, what: u32) -> i32;
        pub fn ee_screens_seun_angle(tab: usize) -> f32;
        pub fn ee_screens_seun_set_under_mouse(tab: usize, unit: i32);
        pub fn ee_screens_sepl_control(screen: usize, row: u32, which: u32) -> usize;
        pub fn ee_screens_secd_handle(screen: usize, which: u32) -> usize;
        pub fn ee_screens_sefx_handle(screen: usize, which: u32) -> usize;
        pub fn ee_screens_sear_state(screen: usize, rect: *mut f32, point: *mut f32) -> u32;
        pub fn ee_screens_sear_center(screen: usize, out: *mut f32) -> bool;
        pub fn ee_screens_setn_get(tab: usize, what: u32, k: u32) -> i32;
        pub fn ee_screens_sew_area_corner(action: usize, x: i32, y: i32);
        pub fn ee_screens_sew_place_push(action: usize, player: i32, unit_type: i32, t: *const f32, animation: i32);
        pub fn ee_screens_sew_rotate(action: usize, yaw: f32);
        pub fn ee_screens_sew_pick_up(action: usize, id: u32);
        pub fn ee_screens_sew_put_down(action: usize);
        pub fn ee_screens_sew_wall(action: usize, x0: f32, y0: f32, x1: f32, y1: f32, player: i32, technology: i32);
        pub fn ee_screens_sew_gate(action: usize, x: i32, y: i32, player: i32, technology: i32, direction: i32) -> u8;
        pub fn ee_screens_sew_place_unit(action: usize, player: i32) -> u8;
        pub fn ee_screens_sew_cancel(action: usize);
        pub fn ee_screens_cb_civilization_query(what: u32, index: u32) -> i64;
        pub fn ee_screens_cb_civilization_name(out: *mut u8, capacity: usize) -> usize;
        pub fn ee_screens_cb_last_file(out: *mut u8, capacity: usize) -> usize;
    }
}

fn c_text(text: &[u8]) -> CString {
    CString::new(text.iter().copied().filter(|&b| b != 0).collect::<Vec<u8>>()).expect("no NUL")
}

fn call(op: O, p: usize, a: &[i64], f: &[f32], s: Option<&[u8]>) -> i64 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    let text = s.map(c_text);
    // SAFETY: the host's operation over the C++ objects the editor's step holds (the UI thread, the world locked)
    unsafe {
        sys::ee_simcpp_sed(op, p, ai.as_ptr(), fi.as_ptr(), text.as_ref().map_or(std::ptr::null(), |t| t.as_ptr()))
    }
}

/// An operation with integer arguments.
pub fn h(op: O, p: usize, a: &[i64]) -> i64 {
    call(op, p, a, &[], None)
}

/// An operation with integer and float arguments.
pub fn hf(op: O, p: usize, a: &[i64], f: &[f32]) -> i64 {
    call(op, p, a, f, None)
}

/// An operation with a text.
pub fn hs(op: O, p: usize, s: &str) -> i64 {
    call(op, p, &[], &[], Some(s.as_bytes()))
}

/// An operation with integer arguments and a text.
pub fn hsa(op: O, p: usize, a: &[i64], s: &str) -> i64 {
    call(op, p, a, &[], Some(s.as_bytes()))
}

/// An operation with a text of bytes.
pub fn hsb(op: O, p: usize, s: &[u8]) -> i64 {
    call(op, p, &[], &[], Some(s))
}

/// A text the object holds (its bytes).
pub fn htb_s(op: O, p: usize, a: i64, s: Option<&str>) -> Vec<u8> {
    let text = s.map(|s| c_text(s.as_bytes()));
    // SAFETY: as call; the text is the object's (or the host's buffer), copied at once
    unsafe {
        let t = sys::ee_simcpp_sed_text(op, p, a, text.as_ref().map_or(std::ptr::null(), |t| t.as_ptr()));
        if t.is_null() {
            Vec::new()
        } else {
            CStr::from_ptr(t).to_bytes().to_vec()
        }
    }
}

/// A text the object holds (its bytes).
pub fn htb(op: O, p: usize, a: i64) -> Vec<u8> {
    htb_s(op, p, a, None)
}

/// A text the object holds.
pub fn ht(op: O, p: usize, a: i64) -> String {
    String::from_utf8_lossy(&htb(op, p, a)).into_owned()
}

/// A text the host makes of `s`.
pub fn hts(op: O, p: usize, a: i64, s: &str) -> String {
    String::from_utf8_lossy(&htb_s(op, p, a, Some(s))).into_owned()
}

/// A float (a C++ float or double, widened).
pub fn hd(op: O, p: usize, a: i64) -> f64 {
    hdf(op, p, &[a], &[])
}

/// A float with float arguments.
pub fn hdf(op: O, p: usize, a: &[i64], f: &[f32]) -> f64 {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    // SAFETY: as call
    unsafe { sys::ee_simcpp_sed_float(op, p, ai.as_ptr(), fi.as_ptr()) }
}

/// The C++'s `Num`: an unsigned long printed as a long.
pub fn num(v: c_ulong) -> String {
    (v as c_long).to_string()
}

/// A `GetCurrentSelection(out) == kErrorOK` query: the selection, or None.
pub fn sel(op: O, p: usize) -> Option<c_ulong> {
    let mut v: c_ulong = 0;
    (h(op, p, &[&mut v as *mut c_ulong as i64]) == 0).then_some(v)
}

/// A GERectangle's sides at `addr` (left, top, right, bottom; its layout is bottom, left, right, top).
pub fn rect(addr: usize) -> [f32; 4] {
    // SAFETY: a live GERectangle (four floats)
    let r = unsafe { *(addr as *const [f32; 4]) };
    [r[1], r[3], r[2], r[0]]
}

/// A GEColor at `addr` (red, green, blue).
pub fn color(addr: usize) -> [f32; 3] {
    // SAFETY: a live GEColor (three floats)
    unsafe { *(addr as *const [f32; 3]) }
}

// ------------------------------------------------------------------------------------------------ the editor

/// The editor form (`gUIManager->GetScenarioEditorForm()`).
pub fn editor() -> usize {
    gc::h(gc::O::UiEditorForm, 0, &[]) as usize
}

/// The editor form `e`'s Rust record (`mRust`).
fn form(e: usize) -> *const EeScreensSeForm {
    h(O::FormRust, e, &[]) as usize as *const EeScreensSeForm
}

/// One of the form's slots (`GetSlot`; editor_form.rs EE_SCREENS_SEFM_*).
pub fn slot(s: u32) -> usize {
    // SAFETY: the editor form's record (or NULL)
    unsafe { sefm::ee_screens_sefm_get(form(editor()), s) }
}

/// The form `e`'s selected unit count (`GetSelectedUnitCount`).
pub fn selected_count(e: usize) -> i64 {
    // SAFETY: as slot
    i64::from(unsafe { sefm::ee_screens_sefm_selected(form(e), -1) })
}

/// The form `e`'s call `what` (`EEUIFScenarioEditor::Call`: editor_form.rs EE_SCREENS_SEFM_*) with `a`, the object
/// `p` and the rectangle `r` (left, top, right, bottom).
pub fn form_call(e: usize, what: u32, a: i32, p: usize, r: Option<&[f32; 4]>) -> i32 {
    let r = r.map_or(std::ptr::null(), |r| r.as_ptr());
    // SAFETY: as slot; the rectangle lives through the call
    unsafe { sefm::ee_screens_sefm_call(form(e), what, a, p as *mut c_void, r) }
}

/// A tab group (`GetTabGroup`: kSlotGroup + the group).
pub fn tab_group(g: u32) -> usize {
    slot(12 + g)
}

/// A tab's Rust object (`GetTab`).
pub fn tab(g: u32) -> usize {
    // SAFETY: the group's Rust object
    unsafe { ee_screens_segr_tab(tab_group(g)) }
}

/// kEditorTabTriggers, kEditorTabUnits, kEditorTabPlayers.
pub fn trigger_tab() -> usize {
    tab(4)
}
pub fn unit_tab() -> usize {
    tab(3)
}
pub fn player_tab() -> usize {
    tab(5)
}

/// The brush group (kSlotBrushGroup).
pub fn brush_group() -> usize {
    slot(9)
}

/// An editor control by its id.
pub fn control_by_id(id: i64) -> usize {
    h(O::ControlById, editor(), &[id]) as usize
}

/// An editor control by its id (decimal or 0x), or "%N": the form's control N (ids repeat on the trigger screens).
pub fn editor_control(word: &str) -> usize {
    use ee_render::gx::cformat::strtoul_0;
    match word.strip_prefix('%') {
        Some(n) => h(O::FormControl, editor(), &[strtoul_0(n) as i64]) as usize,
        None => control_by_id(strtoul_0(word) as i64),
    }
}

/// A trigger tab sub-screen's Rust object (`GetTriggerSub`).
pub fn trigger_sub(k: u32) -> usize {
    // SAFETY: the trigger tab's Rust object
    unsafe { ee_screens_sets_sub(trigger_tab(), k, std::ptr::null_mut(), std::ptr::null_mut()) }
}

/// `TriggerCall`.
pub fn trigger_call(what: u32, a: u32) {
    // SAFETY: as trigger_sub
    unsafe { ee_screens_sets_call(trigger_tab(), what, a, 0.0, 0.0, 0) }
}

/// `TriggerSubOp`.
pub fn trigger_sub_op(k: u32, what: u32) -> u32 {
    // SAFETY: as trigger_sub
    unsafe { ee_screens_sets_sub_op(trigger_tab(), k, what, 0, 0) }
}

/// `TriggerSubSlot`.
pub fn trigger_sub_slot(k: u32, slot: i32) {
    // SAFETY: as trigger_sub
    unsafe { ee_screens_sets_sub_call(trigger_tab(), k, slot, 0.0, 0.0) }
}

/// `UnitCall`.
pub fn unit_call(what: u32) {
    // SAFETY: the unit tab's Rust object
    unsafe { ee_screens_seun_call(unit_tab(), what, 0, 0.0, 0.0, 0) }
}

/// `EEUIShell::Op(control, op, a, b)`.
pub fn shell_op(control: usize, op: u32, a: i64, b: i64) -> i64 {
    h(O::ShellOp, control, &[op as i64, a, b])
}

/// A control's display mode.
pub fn display_mode(control: usize) -> c_ulong {
    h(O::DisplayMode, control, &[]) as c_ulong
}

/// A string list's line count.
pub fn line_count(list: usize) -> c_ulong {
    h(O::ListLineCount, list, &[]) as c_ulong
}

/// A control's trigger event (0: none).
pub fn trigger_event(control: usize, trigger: c_ulong) -> usize {
    super::sim_ops::h(super::sim_ops::EeSimDriverSim::ControlTriggerEvent, control, &[trigger as i64]) as usize
}

/// An event processed by the editor form for `control`.
pub fn process_event(event: usize, control: usize) {
    h(O::ProcessEvent, event, &[control as i64]);
}

/// The trigger tab's condition screen's controls (EESETSConditionControls: 23 pointers).
#[repr(C)]
pub struct CondControls {
    pub labels: [usize; 5],
    pub edits: [usize; 2],
    pub text_edit: usize,
    pub combos: [usize; 3],
    pub scenario: [usize; 4],
    pub checks: [usize; 3],
    pub players: [usize; 2],
    pub radios: [usize; 2],
    pub slider: usize,
}

/// The effect screen's controls (EESETSEffectControls: 46 pointers).
#[repr(C)]
pub struct EffControls {
    pub labels: [usize; 13],
    pub edits: [usize; 2],
    pub edits2: [usize; 2],
    pub text_edit: usize,
    pub edit48: usize,
    pub combos: [usize; 4],
    pub effect_combos: [usize; 2],
    pub scenario: [usize; 4],
    pub players: [usize; 3],
    pub radios: [usize; 6],
    pub sliders: [usize; 4],
    pub checks: [usize; 2],
    pub lists: [usize; 2],
}

/// The condition screen's controls.
pub fn condition_controls() -> &'static CondControls {
    // SAFETY: the screen's shared controls (EESETSConditionControls, pointers only), alive while the editor is
    unsafe { &*(ee_screens_secd_handle(trigger_sub(1), 0) as *const CondControls) }
}

/// The effect screen's controls.
pub fn effect_controls() -> &'static EffControls {
    // SAFETY: as condition_controls
    unsafe { &*(ee_screens_sefx_handle(trigger_sub(2), 0) as *const EffControls) }
}

// ------------------------------------------------------------------------------------------------ the actions

/// `new EESEARust(kind, object, a...)` (an EEAction*).
pub fn sea_new(kind: u32, object: usize, a: &[c_long]) -> usize {
    let mut v = [kind as i64, 0, 0, 0, 0];
    for (k, x) in a.iter().enumerate() {
        v[k + 1] = *x as i64;
    }
    h(O::SeaNew, object, &v) as usize
}

/// An EESEARust's Rust action (`mAction`).
pub fn sea_action(action: usize) -> usize {
    h(O::SeaAction, action, &[]) as usize
}

/// `gUserInterface->PostAction`.
pub fn post_action(action: usize) {
    h(O::PostAction, action, &[]);
}

/// An action's `Dereference`.
pub fn deref(action: usize) {
    gc::h(gc::O::EntryDereferenceAction, action, &[]);
}

/// `new EEMRecord(kind, {id})` (or without a value).
pub fn record(kind: i64, id: Option<c_ulong>) -> usize {
    match id {
        Some(id) => h(O::MRecord1, 0, &[kind, id as i64]) as usize,
        None => h(O::MRecord0, 0, &[kind]) as usize,
    }
}

// ------------------------------------------------------------------------------------------------ the scenario

/// The scenario manager.
pub fn sm() -> usize {
    gc::h(gc::O::PsWorldScenario, 0, &[]) as usize
}

/// The scenario manager's name list `which` (GetNames).
pub fn sm_names(which: i64) -> Vec<String> {
    let names = h(O::NamesNew, 0, &[]) as usize;
    h(O::SmGetNames, sm(), &[which, names as i64]);
    let out = (0..h(O::NamesCount, names, &[])).map(|k| ht(O::NamesAt, names, k)).collect();
    h(O::NamesFree, names, &[]);
    out
}

// ------------------------------------------------------------------------------------------------ the files

/// A file spec in the startup directory's `dir` (its volume and directory set).
pub fn spec_in(dir: &str) -> usize {
    let spec = h(O::SpecNew, 0, &[]) as usize;
    gc::h(gc::O::FileSpecSetVolumeStartup, spec, &[]);
    hs(O::SpecSetDirectoryStartup, spec, dir);
    spec
}

/// data\Scenarios\`name`.
pub fn scenario_spec(name: &str) -> usize {
    let spec = spec_in("data\\Scenarios\\");
    hs(O::SpecSetFile, spec, name);
    spec
}

/// The trigger text file `index` (data\Scenarios\smtext<index>.lcl).
pub fn text_spec(index: i64) -> usize {
    scenario_spec(&format!("smtext{index}.lcl"))
}

/// `fgets` of an FOpen'ed spec (at most `size` - 1 bytes): the line, or None at the end.
pub fn spec_gets(spec: usize, size: usize) -> Option<Vec<u8>> {
    let mut buffer = vec![0u8; size];
    if h(O::SpecGets, spec, &[buffer.as_mut_ptr() as i64, size as i64]) == 0 {
        return None;
    }
    let n = buffer.iter().position(|&b| b == 0).unwrap_or(size);
    buffer.truncate(n);
    Some(buffer)
}

/// `fputs` to an FOpen'ed spec.
pub fn spec_puts(spec: usize, text: &[u8]) {
    hsb(O::SpecPuts, spec, text);
}

/// A memory file's written length.
pub fn mem_length(file: usize) -> c_ulong {
    let mut n: c_ulong = 0;
    h(O::MemLength, file, &[&mut n as *mut c_ulong as i64]);
    n
}

/// A memory file's position.
pub fn mem_position(file: usize) -> c_long {
    let mut n: c_long = 0;
    gc::h(gc::O::MemFilePosition, file, &[&mut n as *mut c_long as i64]);
    n
}

/// `length` bytes of a memory file's data from `start`.
pub fn mem_bytes(file: usize, start: usize, length: c_ulong) -> Vec<u8> {
    let data = gc::h(gc::O::MemFileData, file, &[]) as usize;
    if data == 0 || length == 0 {
        return Vec::new();
    }
    // SAFETY: the memory file's buffer holds the written bytes
    unsafe { std::slice::from_raw_parts((data + start) as *const u8, length as usize) }.to_vec()
}

// ------------------------------------------------------------------------------------------------ the step

/// A step's code: its output lines pushed.
pub type StepCode<'a> = Box<dyn FnMut(&mut Vec<String>) + 'a>;

struct StepContext<'a, 'b> {
    code: &'a mut (dyn FnMut(&mut Vec<String>) + 'b),
    out: &'a mut Vec<String>,
}

extern "C" fn run_code(ctx: *mut c_void) {
    // SAFETY: the context run_step passed; the driver waits for the step
    let c = unsafe { &mut *(ctx as *mut StepContext) };
    (c.code)(c.out);
}

/// `code` run as a step on the UI thread (its output into `out`); false when it did not finish within a minute.
pub fn run_step(mut code: StepCode<'_>, out: &mut Vec<String>) -> bool {
    let mut ctx = StepContext { code: &mut *code, out };
    // SAFETY: the host runs `run_code` on the UI thread while this thread waits for it
    super::sed_run::run(run_code, &mut ctx as *mut StepContext as *mut c_void)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layouts() {
        assert_eq!(std::mem::size_of::<CondControls>(), 23 * std::mem::size_of::<usize>());
        assert_eq!(std::mem::size_of::<EffControls>(), 46 * std::mem::size_of::<usize>());
        let r = [4.0f32, 1.0, 3.0, 2.0]; // bottom, left, right, top
        assert_eq!(rect(r.as_ptr() as usize), [1.0, 2.0, 3.0, 4.0]);
        assert_eq!(num(c_ulong::MAX), "-1");
        assert_eq!(c_text(b"a\0b").as_bytes(), b"ab");
    }
}
