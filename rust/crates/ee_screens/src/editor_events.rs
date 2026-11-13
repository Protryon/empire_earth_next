//! The scenario editor form's own events with state, whole in Rust behind one C++ event (`EESEERust`,
//! `ee/EESEARust.h`): the hide-UI check box (`EESEEHideUI`: the framed or the full game view posted as a viewport
//! change), the viewport change (`EESEEChangeViewport`: the editor's game view set, its UI shown or hidden, all
//! redrawn), the toolbar's quick load and quick save (`EESEEQuickLoad`, `EESEEQuickSave`: the editor's shared quick
//! save/load action, referenced while the event lives, shown loaded or saved through a quick save action); and the
//! trigger screens' controls' events (`EESEESliderChange`: the slider's value times a scale plus an offset printed
//! into a label through the host's `sprintf` of the event's format; `EESEECheckDisplay`: controls shown while a check
//! box is checked; `EESEEUseObjectArea`: a scenario combo box refilled with the areas or the objects as the area
//! radio or another was checked, its first line picked); and the effect editors' own events
//! (`EESETSETCAMGModifyObject`: an object moved into or out of a military group by the list clicked;
//! `EESETSETMSDCameraScrollChange`: the dialogue's camera scroll speed shown; `EESETSETSAAAttributeChange`: the AI
//! attribute's ranges shown; `EESEEUseAreaObject`, `EESEEMoveAreaObject`, `EESEEFaceAreaObject`: the form's location
//! combo box refilled with the areas or the objects by the radio checked); and the editor tabs' own (`EEUIFSEEMapType`:
//! the elevation boxes shown while an elevation file is picked; `EEUIFSEESortUnitList`: the unit tab's list rebuilt;
//! `EESEEChangeStartEpoch`: a player row's end epochs refilled from the start epoch on, keeping the end epoch;
//! `EESEEGameUnitLimit`: the unit limit per player and the slider's own label).

// ---- the kinds (`EESEERust`'s constructor)

/// `EESEEHideUI`.
pub const EE_SCREENS_SEE_HIDE_UI: u32 = 1;
/// `EESEEChangeViewport` (its rectangle and flag: `ee_screens_see_viewport`).
pub const EE_SCREENS_SEE_CHANGE_VIEWPORT: u32 = 2;
/// `EESEEQuickLoad` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_LOAD: u32 = 3;
/// `EESEEQuickSave` (object: the quick save/load action).
pub const EE_SCREENS_SEE_QUICK_SAVE: u32 = 4;
/// `EESEESliderChange` (its label, format, scale and offset: `ee_screens_see_slider`).
pub const EE_SCREENS_SEE_SLIDER_CHANGE: u32 = 5;
/// `EESEECheckDisplay` (its controls: `ee_screens_see_push_control`).
pub const EE_SCREENS_SEE_CHECK_DISPLAY: u32 = 6;
/// `EESEEUseObjectArea` (its label, combo box and area control: `ee_screens_see_object_area`).
pub const EE_SCREENS_SEE_USE_OBJECT_AREA: u32 = 7;
/// `EESETSETCAMGModifyObject` (the military group type, its two lists: `ee_screens_see_objects`).
pub const EE_SCREENS_SEE_CAMG_MODIFY_OBJECT: u32 = 8;
/// `EESETSETMSDCameraScrollChange` (the send-dialogue type, the effect's controls: `ee_screens_see_objects`).
pub const EE_SCREENS_SEE_MSD_CAMERA_SCROLL: u32 = 9;
/// `EESETSETSAAAttributeChange` (the set-AI-attribute type, the effect's controls: `ee_screens_see_objects`).
pub const EE_SCREENS_SEE_SAA_ATTRIBUTE: u32 = 10;
/// `EESEEUseAreaObject` and `EESEEMoveAreaObject` (one body: ICF folded them): the label 0x96d, the combo box 0x99e,
/// the area radio 0x988.
pub const EE_SCREENS_SEE_USE_AREA_OBJECT: u32 = 11;
/// `EESEEFaceAreaObject`: the label 0x96e, the combo box 0x99f, the area radio 0x98a.
pub const EE_SCREENS_SEE_FACE_AREA_OBJECT: u32 = 12;
/// `EEUIFSEEMapType` (object: the map tab, `EEUIFSEMap`).
pub const EE_SCREENS_SEE_MAP_TYPE: u32 = 13;
/// `EEUIFSEESortUnitList`.
pub const EE_SCREENS_SEE_SORT_UNIT_LIST: u32 = 14;
/// `EESEEChangeStartEpoch` (object: the row's end epoch combo box).
pub const EE_SCREENS_SEE_START_EPOCH: u32 = 15;
/// `EESEEGameUnitLimit` (its label, modifier and offset: `ee_screens_see_slider` with the format "%g").
pub const EE_SCREENS_SEE_UNIT_LIMIT: u32 = 16;

/// The game page's per-player count label (`kSERBUnitTypesPerPlayerCountLabel`).
pub const EE_SCREENS_SEE_PER_PLAYER_LABEL_ID: i32 = 0x162;

/// The military group's "to add" list: a click there adds (`EESETSETCAMGModifyObject`).
pub const EE_SCREENS_SEE_TO_ADD_LIST_ID: u32 = 0x9bf;

// ---- the host's ops (`ee_screenscpp_see`: `p`, `q` and `r` pointers, `i` up to 4 integers, `f` up to 4 floats)

/// The object p referenced / released.
pub const EE_SCREENS_SEE_OP_REFERENCE: u32 = 1;
pub const EE_SCREENS_SEE_OP_DEREFERENCE: u32 = 2;
/// -> whether the check box p is checked.
pub const EE_SCREENS_SEE_OP_CHECKED: u32 = 3;
/// A viewport change to the rectangle f0..f3 (left, top, right, bottom), the UI hidden when i0, posted -> the
/// post's result.
pub const EE_SCREENS_SEE_OP_POST_VIEWPORT: u32 = 4;
/// -> whether the scenario editor's form exists.
pub const EE_SCREENS_SEE_OP_HAS_EDITOR: u32 = 5;
/// The editor's game view set to the rectangle f0..f3, its UI shown when i0 (`SetGameView`).
pub const EE_SCREENS_SEE_OP_SET_GAME_VIEW: u32 = 6;
/// Everything redrawn (`gUIManager->RedrawAll`).
pub const EE_SCREENS_SEE_OP_REDRAW_ALL: u32 = 7;
/// The scenario editor shows the loaded object p (`PostShowLoadedObject`; the event's form is the editor's).
pub const EE_SCREENS_SEE_OP_SHOW_LOADED: u32 = 8;
/// A quick save action of the quick save/load action p posted.
pub const EE_SCREENS_SEE_OP_QUICK_SAVE: u32 = 9;
/// -> the slider p's current value (`GetCurrentRangeValue`).
pub const EE_SCREENS_SEE_OP_RANGE: u32 = 10;
/// The label p's text set to the value f0 printed by the NUL-terminated format q (`sprintf` into a `UWideString`,
/// the float passed as a double as through the original's `...`).
pub const EE_SCREENS_SEE_OP_SLIDER_TEXT: u32 = 11;
/// The control p's display mode set to i0.
pub const EE_SCREENS_SEE_OP_SET_DISPLAY: u32 = 12;
/// The scenario combo box p refilled with the areas (i0 != 0) or the objects, the label q titled
/// (`FillScenarioComboBox`).
pub const EE_SCREENS_SEE_OP_FILL_SCENARIO: u32 = 13;
/// The combo box p's selection set to i0.
pub const EE_SCREENS_SEE_OP_SET_SELECTION: u32 = 14;
/// -> the control p's id (`GetId`).
pub const EE_SCREENS_SEE_OP_ENTITY_ID: u32 = 15;
/// -> the list box p's selection, -1 for none.
pub const EE_SCREENS_SEE_OP_LIST_SELECTION: u32 = 16;
/// The military group type p's `AddObject(i0, q, r)` / `RemoveObject(i0, q, r)` (the lists q and r).
pub const EE_SCREENS_SEE_OP_CAMG_ADD: u32 = 17;
pub const EE_SCREENS_SEE_OP_CAMG_REMOVE: u32 = 18;
/// The send-dialogue type p's `ShowCameraScrollSpeed(q)` (q the effect's controls).
pub const EE_SCREENS_SEE_OP_MSD_SHOW: u32 = 19;
/// -> the combo box p's selection, -1 for none.
pub const EE_SCREENS_SEE_OP_COMBO_SELECTION: u32 = 20;
/// The set-AI-attribute type p's `ShowAttributeRanges(i0, q)` (q the effect's controls).
pub const EE_SCREENS_SEE_OP_SAA_SHOW: u32 = 21;
/// The form p's label i0 and scenario combo box i1 refilled with the areas (i2 != 0) or the objects
/// (`FillAreaObjectComboBox`).
pub const EE_SCREENS_SEE_OP_FILL_AREA_OBJECT: u32 = 22;
/// The map tab p's elevation boxes shown (i0 != 0) or hidden (`ShowElevationControls`).
pub const EE_SCREENS_SEE_OP_MAP_ELEVATION: u32 = 23;
/// The editor form p's unit tab's list rebuilt (`EESEUnit::RefreshUnitList`).
pub const EE_SCREENS_SEE_OP_REFRESH_UNIT_LIST: u32 = 24;
/// -> the epoch count (`EEEpochCount`).
pub const EE_SCREENS_SEE_OP_EPOCH_COUNT: u32 = 25;
/// -> the combo box p's line count.
pub const EE_SCREENS_SEE_OP_LINE_COUNT: u32 = 26;
/// The combo box p emptied and given the epochs' titles from i0 on (one `AppendStringArray`).
pub const EE_SCREENS_SEE_OP_FILL_EPOCHS: u32 = 27;
/// -> `gWorld.mMaxPlayers`.
pub const EE_SCREENS_SEE_OP_MAX_PLAYERS: u32 = 28;
/// The form p's label i0 set to the number i1 (`SetText(unsigned long)`).
pub const EE_SCREENS_SEE_OP_LABEL_NUMBER: u32 = 29;

/// The form, the controls and the actions.
pub trait SeeHost {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32;
}

fn call<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: usize, ints: [i32; 2], floats: [f32; 4]) -> i32 {
    call3(h, op, [p, 0, 0], [ints[0], ints[1], 0, 0], floats)
}

fn call2<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: usize, q: usize, ints: [i32; 2], floats: [f32; 4]) -> i32 {
    call3(h, op, [p, q, 0], [ints[0], ints[1], 0, 0], floats)
}

fn call3<H: SeeHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], ints: [i32; 4], floats: [f32; 4]) -> i32 {
    let (mut i, mut f) = (ints, floats);
    h.op(op, p, &mut i, &mut f)
}

/// What an event does.
#[derive(Clone, Debug, PartialEq)]
pub enum SeeKind {
    /// the views posted unchecked and checked (the original's +0x0c and +0x1c rectangles): all zero when made,
    /// set by the trigger tab's `Enter` (`ee_screens_see_hide_ui_views`)
    HideUi {
        unchecked: [f32; 4],
        checked: [f32; 4],
    },
    ChangeViewport {
        rectangle: [f32; 4],
        hidden: bool,
    },
    QuickLoad {
        owner: usize,
    },
    QuickSave {
        owner: usize,
    },
    /// `format`: the bytes of the original's `UWideString` format and a NUL
    SliderChange {
        label: usize,
        format: Vec<u8>,
        scale: f32,
        offset: f32,
    },
    CheckDisplay {
        controls: Vec<usize>,
    },
    /// `area`: the control whose check means areas (compared with the event's entity)
    UseObjectArea {
        label: usize,
        combo: usize,
        area: usize,
    },
    /// `ty`: the `EESETSETCreateAIMilitaryGroup`; `first`, `second`: its "to add" and "added" lists
    CamgModifyObject {
        ty: usize,
        first: usize,
        second: usize,
    },
    /// `ty`: the `EESETSETMediaSendDialog`; `controls`: the effect's `EESETSEffectControls`
    MsdCameraScroll {
        ty: usize,
        controls: usize,
    },
    /// `ty`: the `EESETSETSetAIAttribute`; `controls`: the effect's `EESETSEffectControls`
    SaaAttribute {
        ty: usize,
        controls: usize,
    },
    MapType {
        map: usize,
    },
    SortUnitList,
    StartEpoch {
        end_combo: usize,
    },
    /// the `UIESliderChange` part: `label`, `format` ("%g": no value is mapped to a string), `scale` (its modifier),
    /// `offset` (its `long` offset as a float, as the original's sum converts it)
    UnitLimit {
        label: usize,
        format: Vec<u8>,
        scale: f32,
        offset: f32,
    },
    /// the form's label and combo box ids and the id of the radio that means areas
    AreaObject {
        label_id: i32,
        combo_id: i32,
        area_id: u32,
    },
}

impl SeeKind {
    /// The event of `kind` (`object`: the quick save/load action, referenced); None for an unknown kind.
    pub fn new<H: SeeHost + ?Sized>(h: &mut H, kind: u32, object: usize) -> Option<SeeKind> {
        Some(match kind {
            EE_SCREENS_SEE_HIDE_UI => SeeKind::HideUi { unchecked: [0.0; 4], checked: [0.0; 4] },
            EE_SCREENS_SEE_CHANGE_VIEWPORT => SeeKind::ChangeViewport { rectangle: [0.0; 4], hidden: false },
            EE_SCREENS_SEE_SLIDER_CHANGE => {
                SeeKind::SliderChange { label: 0, format: vec![0], scale: 0.0, offset: 0.0 }
            }
            EE_SCREENS_SEE_CHECK_DISPLAY => SeeKind::CheckDisplay { controls: Vec::new() },
            EE_SCREENS_SEE_USE_OBJECT_AREA => SeeKind::UseObjectArea { label: 0, combo: 0, area: 0 },
            EE_SCREENS_SEE_CAMG_MODIFY_OBJECT => SeeKind::CamgModifyObject { ty: 0, first: 0, second: 0 },
            EE_SCREENS_SEE_MSD_CAMERA_SCROLL => SeeKind::MsdCameraScroll { ty: 0, controls: 0 },
            EE_SCREENS_SEE_SAA_ATTRIBUTE => SeeKind::SaaAttribute { ty: 0, controls: 0 },
            EE_SCREENS_SEE_USE_AREA_OBJECT => SeeKind::AreaObject { label_id: 0x96d, combo_id: 0x99e, area_id: 0x988 },
            EE_SCREENS_SEE_FACE_AREA_OBJECT => SeeKind::AreaObject { label_id: 0x96e, combo_id: 0x99f, area_id: 0x98a },
            EE_SCREENS_SEE_MAP_TYPE => SeeKind::MapType { map: object },
            EE_SCREENS_SEE_SORT_UNIT_LIST => SeeKind::SortUnitList,
            EE_SCREENS_SEE_START_EPOCH => SeeKind::StartEpoch { end_combo: object },
            EE_SCREENS_SEE_UNIT_LIMIT => SeeKind::UnitLimit { label: 0, format: vec![0], scale: 0.0, offset: 0.0 },
            EE_SCREENS_SEE_QUICK_LOAD | EE_SCREENS_SEE_QUICK_SAVE => {
                call(h, EE_SCREENS_SEE_OP_REFERENCE, object, [0; 2], [0.0; 4]);
                if kind == EE_SCREENS_SEE_QUICK_LOAD {
                    SeeKind::QuickLoad { owner: object }
                } else {
                    SeeKind::QuickSave { owner: object }
                }
            }
            _ => return None,
        })
    }

    /// The viewport change's rectangle and flag.
    pub fn set_viewport(&mut self, to: [f32; 4], to_hidden: bool) {
        if let SeeKind::ChangeViewport { rectangle, hidden } = self {
            *rectangle = to;
            *hidden = to_hidden;
        }
    }

    /// The slider event's label, format (up to a NUL), scale and offset (the original's constructor).
    pub fn set_slider(&mut self, to_label: usize, to_format: &[u8], to_scale: f32, to_offset: f32) {
        if let SeeKind::SliderChange { label, format, scale, offset }
        | SeeKind::UnitLimit { label, format, scale, offset } = self
        {
            *label = to_label;
            *format = to_format.iter().copied().take_while(|&b| b != 0).chain(core::iter::once(0)).collect();
            *scale = to_scale;
            *offset = to_offset;
        }
    }

    /// A control shown and hidden with the check box (the original's `mControls.push_back`).
    pub fn push_control(&mut self, control: usize) {
        if let SeeKind::CheckDisplay { controls } = self {
            controls.push(control);
        }
    }

    /// The object/area event's label, combo box and area control (the original's constructor).
    pub fn set_object_area(&mut self, to_label: usize, to_combo: usize, to_area: usize) {
        if let SeeKind::UseObjectArea { label, combo, area } = self {
            *label = to_label;
            *combo = to_combo;
            *area = to_area;
        }
    }

    /// The effect editors' events' objects (their constructors): the type and its two lists, or the type and the
    /// effect's controls (`c` unused).
    pub fn set_objects(&mut self, a: usize, b: usize, c: usize) {
        match self {
            SeeKind::CamgModifyObject { ty, first, second } => {
                *ty = a;
                *first = b;
                *second = c;
            }
            SeeKind::MsdCameraScroll { ty, controls } | SeeKind::SaaAttribute { ty, controls } => {
                *ty = a;
                *controls = b;
            }
            _ => {}
        }
    }

    /// The hide-UI event's two views (`EESETriggerScreen::Enter`: the original's inline `SetRectangles`).
    pub fn set_hide_ui_views(&mut self, to_unchecked: [f32; 4], to_checked: [f32; 4]) {
        if let SeeKind::HideUi { unchecked, checked } = self {
            *unchecked = to_unchecked;
            *checked = to_checked;
        }
    }

    /// `ProcessEvent(form, entity)` for the events that do not read their form.
    pub fn process<H: SeeHost + ?Sized>(&self, h: &mut H, entity: usize) -> i32 {
        self.process_in(h, 0, entity)
    }

    /// `ProcessEvent(form, entity)` (only the area/object radios read the form: the editor's).
    pub fn process_in<H: SeeHost + ?Sized>(&self, h: &mut H, form: usize, entity: usize) -> i32 {
        match self {
            SeeKind::HideUi { unchecked, checked } => {
                let is_checked = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]) != 0;
                let rectangle = if is_checked { *checked } else { *unchecked };
                // the check box read again for the flag, as the original's second GetIsChecked
                let hidden = call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]);
                call(h, EE_SCREENS_SEE_OP_POST_VIEWPORT, 0, [hidden, 0], rectangle)
            }
            SeeKind::ChangeViewport { rectangle, hidden } => {
                if call(h, EE_SCREENS_SEE_OP_HAS_EDITOR, 0, [0; 2], [0.0; 4]) != 0 {
                    call(h, EE_SCREENS_SEE_OP_SET_GAME_VIEW, 0, [i32::from(!*hidden), 0], *rectangle);
                    call(h, EE_SCREENS_SEE_OP_REDRAW_ALL, 0, [0; 2], [0.0; 4]);
                }
                0
            }
            SeeKind::QuickLoad { owner } => {
                call(h, EE_SCREENS_SEE_OP_SHOW_LOADED, *owner, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::QuickSave { owner } => {
                call(h, EE_SCREENS_SEE_OP_QUICK_SAVE, *owner, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::SliderChange { label, format, scale, offset } => {
                // the original's `long * float + float`: the value made a float, one product, one sum (no FMA)
                let value = call(h, EE_SCREENS_SEE_OP_RANGE, entity, [0; 2], [0.0; 4]) as f32 * *scale + *offset;
                let text = format.as_ptr() as usize;
                call2(h, EE_SCREENS_SEE_OP_SLIDER_TEXT, *label, text, [0; 2], [value, 0.0, 0.0, 0.0]);
                0
            }
            SeeKind::CheckDisplay { controls } => {
                let mode = i32::from(call(h, EE_SCREENS_SEE_OP_CHECKED, entity, [0; 2], [0.0; 4]) != 0);
                for &control in controls {
                    call(h, EE_SCREENS_SEE_OP_SET_DISPLAY, control, [mode, 0], [0.0; 4]);
                }
                0
            }
            SeeKind::UseObjectArea { label, combo, area } => {
                let areas = i32::from(entity == *area);
                call2(h, EE_SCREENS_SEE_OP_FILL_SCENARIO, *combo, *label, [areas, 0], [0.0; 4]);
                call(h, EE_SCREENS_SEE_OP_SET_SELECTION, *combo, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::CamgModifyObject { ty, first, second } => {
                let selection = call(h, EE_SCREENS_SEE_OP_LIST_SELECTION, entity, [0; 2], [0.0; 4]);
                if selection == -1 {
                    return 0;
                }
                let op = if call(h, EE_SCREENS_SEE_OP_ENTITY_ID, entity, [0; 2], [0.0; 4]) as u32
                    == EE_SCREENS_SEE_TO_ADD_LIST_ID
                {
                    EE_SCREENS_SEE_OP_CAMG_ADD
                } else {
                    EE_SCREENS_SEE_OP_CAMG_REMOVE
                };
                call3(h, op, [*ty, *first, *second], [selection, 0, 0, 0], [0.0; 4]);
                0
            }
            SeeKind::MsdCameraScroll { ty, controls } => {
                call2(h, EE_SCREENS_SEE_OP_MSD_SHOW, *ty, *controls, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::SaaAttribute { ty, controls } => {
                // the original reads the selection whatever the call answered: none passes -1 here
                let selection = call(h, EE_SCREENS_SEE_OP_COMBO_SELECTION, entity, [0; 2], [0.0; 4]);
                call2(h, EE_SCREENS_SEE_OP_SAA_SHOW, *ty, *controls, [selection, 0], [0.0; 4]);
                0
            }
            SeeKind::MapType { map } => {
                // the original's unsigned `selection > 0` (none reads as -1: shown)
                let selection = call(h, EE_SCREENS_SEE_OP_COMBO_SELECTION, entity, [0; 2], [0.0; 4]) as u32;
                call(h, EE_SCREENS_SEE_OP_MAP_ELEVATION, *map, [i32::from(selection > 0), 0], [0.0; 4]);
                0
            }
            SeeKind::SortUnitList => {
                call(h, EE_SCREENS_SEE_OP_REFRESH_UNIT_LIST, form, [0; 2], [0.0; 4]);
                0
            }
            SeeKind::StartEpoch { end_combo } => {
                let start = call(h, EE_SCREENS_SEE_OP_COMBO_SELECTION, entity, [0; 2], [0.0; 4]);
                if start == -1 {
                    return 0;
                }
                let end = call(h, EE_SCREENS_SEE_OP_COMBO_SELECTION, *end_combo, [0; 2], [0.0; 4]);
                if end == -1 {
                    return 0;
                }
                // the end epoch's index in all the epochs (the list starts at the old start epoch); unsigned
                let count = call(h, EE_SCREENS_SEE_OP_EPOCH_COUNT, 0, [0; 2], [0.0; 4]) as u32;
                let lines = call(h, EE_SCREENS_SEE_OP_LINE_COUNT, *end_combo, [0; 2], [0.0; 4]) as u32;
                let (start, end) = (start as u32, (end as u32).wrapping_add(count.wrapping_sub(lines)));
                call(h, EE_SCREENS_SEE_OP_FILL_EPOCHS, *end_combo, [start as i32, 0], [0.0; 4]);
                // the original: `start <= end ? end - start : 0`
                let pick = end.saturating_sub(start);
                call(h, EE_SCREENS_SEE_OP_SET_SELECTION, *end_combo, [pick as i32, 0], [0.0; 4]);
                0
            }
            SeeKind::UnitLimit { label, format, scale, offset } => {
                let range = call(h, EE_SCREENS_SEE_OP_RANGE, entity, [0; 2], [0.0; 4]);
                // the original's unsigned division by the non-gaia players (none: the original divided by zero)
                let players = (call(h, EE_SCREENS_SEE_OP_MAX_PLAYERS, 0, [0; 2], [0.0; 4]) as u32).wrapping_sub(1);
                if let Some(per_player) = (range as u32).checked_div(players) {
                    let ints = [EE_SCREENS_SEE_PER_PLAYER_LABEL_ID, per_player as i32];
                    call(h, EE_SCREENS_SEE_OP_LABEL_NUMBER, form, ints, [0.0; 4]);
                }
                // then UIESliderChange's: the value printed "%g" (no value maps to a string)
                let value = range as f32 * *scale + *offset;
                let text = format.as_ptr() as usize;
                call2(h, EE_SCREENS_SEE_OP_SLIDER_TEXT, *label, text, [0; 2], [value, 0.0, 0.0, 0.0]);
                0
            }
            SeeKind::AreaObject { label_id, combo_id, area_id } => {
                let areas = call(h, EE_SCREENS_SEE_OP_ENTITY_ID, entity, [0; 2], [0.0; 4]) as u32 == *area_id;
                call3(
                    h,
                    EE_SCREENS_SEE_OP_FILL_AREA_OBJECT,
                    [form, 0, 0],
                    [*label_id, *combo_id, i32::from(areas), 0],
                    [0.0; 4],
                );
                0
            }
        }
    }

    /// The event freed: the quick save/load action released.
    pub fn free<H: SeeHost + ?Sized>(&self, h: &mut H) {
        if let SeeKind::QuickLoad { owner } | SeeKind::QuickSave { owner } = self {
            call(h, EE_SCREENS_SEE_OP_DEREFERENCE, *owner, [0; 2], [0.0; 4]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Rec {
        log: Vec<String>,
        checked: bool,
        editor: bool,
        range: i32,
        id: i32,
        selection: i32,
        end: i32,
        lines: i32,
        players: i32,
    }

    impl SeeHost for Rec {
        fn op(&mut self, op: u32, ptrs: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            let [p, q, r] = ptrs;
            match op {
                EE_SCREENS_SEE_OP_ENTITY_ID => return self.id,
                EE_SCREENS_SEE_OP_LIST_SELECTION | EE_SCREENS_SEE_OP_COMBO_SELECTION => {
                    return if p == 0xe0 { self.end } else { self.selection };
                }
                EE_SCREENS_SEE_OP_EPOCH_COUNT => return 15,
                EE_SCREENS_SEE_OP_LINE_COUNT => return self.lines,
                EE_SCREENS_SEE_OP_MAX_PLAYERS => return self.players,
                EE_SCREENS_SEE_OP_CAMG_ADD | EE_SCREENS_SEE_OP_CAMG_REMOVE | EE_SCREENS_SEE_OP_FILL_AREA_OBJECT => {
                    self.log.push(format!("{op} p{p} q{q} r{r} {i:?}"));
                    return 0;
                }
                EE_SCREENS_SEE_OP_CHECKED => return i32::from(self.checked),
                EE_SCREENS_SEE_OP_RANGE => return self.range,
                EE_SCREENS_SEE_OP_SLIDER_TEXT => {
                    // the format the host prints with: up to its NUL
                    let format = unsafe { core::ffi::CStr::from_ptr(q as *const core::ffi::c_char) };
                    self.log.push(format!("text p{p} {:?} {}", format.to_str().unwrap(), f[0]));
                    return 0;
                }
                EE_SCREENS_SEE_OP_FILL_SCENARIO => {
                    self.log.push(format!("fill p{p} q{q} {}", i[0]));
                    return 0;
                }
                EE_SCREENS_SEE_OP_HAS_EDITOR => return i32::from(self.editor),
                EE_SCREENS_SEE_OP_POST_VIEWPORT => {
                    self.log.push(format!("post {f:?} {}", i[0]));
                    return 5;
                }
                _ => {}
            }
            self.log.push(format!("{op} p{p} {:?} {f:?}", &i[..2]));
            0
        }
    }

    fn rec() -> Rec {
        Rec {
            log: Vec::new(),
            checked: false,
            editor: true,
            range: 0,
            id: 0,
            selection: 0,
            end: 0,
            lines: 15,
            players: 3,
        }
    }

    #[test]
    fn hide_ui_posts_the_view_of_the_check_state() {
        let mut h = rec();
        let e = SeeKind::new(&mut h, EE_SCREENS_SEE_HIDE_UI, 0).unwrap();
        assert_eq!(e.process(&mut h, 2), 5);
        h.checked = true;
        e.process(&mut h, 2);
        assert_eq!(h.log, ["post [0.0, 0.0, 0.0, 0.0] 0", "post [0.0, 0.0, 0.0, 0.0] 1"]);
        // the trigger tab's views: unchecked the first, checked the second
        let mut e = e;
        e.set_hide_ui_views([0.0, 0.0, 1024.0, 606.0], [0.0, 0.0, 1024.0, 447.0]);
        h.log.clear();
        e.process(&mut h, 2);
        h.checked = false;
        e.process(&mut h, 2);
        assert_eq!(h.log, ["post [0.0, 0.0, 1024.0, 447.0] 1", "post [0.0, 0.0, 1024.0, 606.0] 0"]);
    }

    #[test]
    fn viewport_change_sets_the_view_when_the_editor_is_open() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CHANGE_VIEWPORT, 0).unwrap();
        e.set_viewport([1.0, 2.0, 3.0, 4.0], true);
        e.process(&mut h, 0);
        assert_eq!(h.log, ["6 p0 [0, 0] [1.0, 2.0, 3.0, 4.0]", "7 p0 [0, 0] [0.0, 0.0, 0.0, 0.0]"]);
        h.log.clear();
        h.editor = false;
        e.process(&mut h, 0);
        assert!(h.log.is_empty());
        assert_eq!(SeeKind::new(&mut h, 99, 0), None);
    }

    #[test]
    fn quick_load_and_save_hold_their_action() {
        let mut h = rec();
        let l = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_LOAD, 0x40).unwrap();
        let s = SeeKind::new(&mut h, EE_SCREENS_SEE_QUICK_SAVE, 0x40).unwrap();
        l.process(&mut h, 0);
        s.process(&mut h, 0);
        l.free(&mut h);
        s.free(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|x| x.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["1", "1", "8", "9", "2", "2"]);
        assert!(h.log.iter().all(|x| x.contains("p64")));
    }

    #[test]
    fn slider_prints_value_times_scale_plus_offset() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_SLIDER_CHANGE, 0).unwrap();
        e.set_slider(0x30, b"%#2.1f", 0.1, 0.1);
        h.range = 17;
        assert_eq!(e.process(&mut h, 2), 0);
        // 17.0f * 0.1f + 0.1f in f32, as the original
        let want = 17.0f32 * 0.1f32 + 0.1f32;
        assert_eq!(h.log, [format!("text p48 \"%#2.1f\" {want}")]);
        assert_eq!(want.to_bits(), 0x3fe6_6667);
        // a format with its NUL keeps one NUL
        e.set_slider(0x30, b"%2.0f\0", 1.0, 0.0);
        if let SeeKind::SliderChange { format, .. } = &e {
            assert_eq!(format, b"%2.0f\0");
        }
    }

    #[test]
    fn check_display_shows_its_controls_while_checked() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CHECK_DISPLAY, 0).unwrap();
        e.push_control(0x10);
        e.push_control(0x20);
        h.checked = true;
        e.process(&mut h, 2);
        h.checked = false;
        e.process(&mut h, 2);
        assert_eq!(
            h.log,
            [
                "12 p16 [1, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p32 [1, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p16 [0, 0] [0.0, 0.0, 0.0, 0.0]",
                "12 p32 [0, 0] [0.0, 0.0, 0.0, 0.0]"
            ]
        );
    }

    #[test]
    fn object_area_refills_with_areas_from_the_area_control() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_USE_OBJECT_AREA, 0).unwrap();
        e.set_object_area(0x30, 0x40, 0x50);
        e.process(&mut h, 0x50);
        e.process(&mut h, 0x60);
        assert_eq!(
            h.log,
            [
                "fill p64 q48 1",
                "14 p64 [0, 0] [0.0, 0.0, 0.0, 0.0]",
                "fill p64 q48 0",
                "14 p64 [0, 0] [0.0, 0.0, 0.0, 0.0]"
            ]
        );
        // the events without an action release nothing
        e.free(&mut h);
        assert_eq!(h.log.len(), 4);
    }

    #[test]
    fn military_group_lists_add_from_the_first_and_remove_from_the_second() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_CAMG_MODIFY_OBJECT, 0).unwrap();
        e.set_objects(0x10, 0x20, 0x30);
        h.selection = 2;
        h.id = 0x9bf;
        e.process(&mut h, 7);
        h.id = 0x9c0;
        e.process(&mut h, 7);
        // no selection: nothing moved
        h.selection = -1;
        e.process(&mut h, 7);
        assert_eq!(h.log, ["17 p16 q32 r48 [2, 0, 0, 0]", "18 p16 q32 r48 [2, 0, 0, 0]"]);
    }

    #[test]
    fn scroll_speed_and_attribute_ranges_shown_by_their_type() {
        let mut h = rec();
        let mut m = SeeKind::new(&mut h, EE_SCREENS_SEE_MSD_CAMERA_SCROLL, 0).unwrap();
        m.set_objects(0x10, 0x20, 0);
        let mut a = SeeKind::new(&mut h, EE_SCREENS_SEE_SAA_ATTRIBUTE, 0).unwrap();
        a.set_objects(0x40, 0x20, 0);
        h.selection = 60;
        m.process(&mut h, 5);
        a.process(&mut h, 5);
        assert_eq!(h.log, ["19 p16 [0, 0] [0.0, 0.0, 0.0, 0.0]", "21 p64 [60, 0] [0.0, 0.0, 0.0, 0.0]"]);
    }

    #[test]
    fn area_object_radios_refill_the_forms_combo() {
        let mut h = rec();
        let u = SeeKind::new(&mut h, EE_SCREENS_SEE_USE_AREA_OBJECT, 0).unwrap();
        let f = SeeKind::new(&mut h, EE_SCREENS_SEE_FACE_AREA_OBJECT, 0).unwrap();
        h.id = 0x988;
        u.process_in(&mut h, 0x50, 1);
        f.process_in(&mut h, 0x50, 1);
        h.id = 0x98a;
        f.process_in(&mut h, 0x50, 1);
        assert_eq!(
            h.log,
            ["22 p80 q0 r0 [2413, 2462, 1, 0]", "22 p80 q0 r0 [2414, 2463, 0, 0]", "22 p80 q0 r0 [2414, 2463, 1, 0]"]
        );
    }

    #[test]
    fn map_type_shows_the_elevation_boxes_for_a_file() {
        let mut h = rec();
        let e = SeeKind::new(&mut h, EE_SCREENS_SEE_MAP_TYPE, 0x40).unwrap();
        for selection in [0, 2, -1] {
            h.selection = selection;
            e.process(&mut h, 1);
        }
        let shows: Vec<&str> = h.log.iter().map(|x| &x[..9]).collect();
        assert_eq!(shows, ["23 p64 [0", "23 p64 [1", "23 p64 [1"]);
        let s = SeeKind::new(&mut h, EE_SCREENS_SEE_SORT_UNIT_LIST, 0).unwrap();
        h.log.clear();
        s.process_in(&mut h, 0x50, 1);
        assert_eq!(h.log, ["24 p80 [0, 0] [0.0, 0.0, 0.0, 0.0]"]);
    }

    #[test]
    fn start_epoch_refills_the_end_epochs_keeping_the_end() {
        let mut h = rec();
        let e = SeeKind::new(&mut h, EE_SCREENS_SEE_START_EPOCH, 0xe0).unwrap();
        // all 15 listed, end 14; start 5: 10 lines, end at 9
        (h.selection, h.end, h.lines) = (5, 14, 15);
        e.process(&mut h, 1);
        // 10 listed from 5, end line 3 (epoch 8); start 9: 6 lines, epoch 8 before it: the first
        (h.selection, h.end, h.lines) = (9, 3, 10);
        e.process(&mut h, 1);
        // no start or no end: nothing
        h.selection = -1;
        e.process(&mut h, 1);
        (h.selection, h.end) = (2, -1);
        e.process(&mut h, 1);
        assert_eq!(
            h.log,
            [
                "27 p224 [5, 0] [0.0, 0.0, 0.0, 0.0]",
                "14 p224 [9, 0] [0.0, 0.0, 0.0, 0.0]",
                "27 p224 [9, 0] [0.0, 0.0, 0.0, 0.0]",
                "14 p224 [0, 0] [0.0, 0.0, 0.0, 0.0]"
            ]
        );
    }

    #[test]
    fn unit_limit_shows_per_player_then_the_value() {
        let mut h = rec();
        let mut e = SeeKind::new(&mut h, EE_SCREENS_SEE_UNIT_LIMIT, 0).unwrap();
        e.set_slider(0x30, b"%g", 1.0, 0.0);
        h.range = 301;
        e.process_in(&mut h, 0x50, 2);
        assert_eq!(h.log, ["29 p80 [354, 150] [0.0, 0.0, 0.0, 0.0]", "text p48 \"%g\" 301"]);
        // one player: no per-player count (the original divided by zero)
        h.players = 1;
        h.log.clear();
        e.process_in(&mut h, 0x50, 2);
        assert_eq!(h.log, ["text p48 \"%g\" 301"]);
    }
}
