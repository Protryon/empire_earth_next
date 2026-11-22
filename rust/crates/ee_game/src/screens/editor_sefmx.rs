//! The editor form's host whole in Rust (`ee_screenscpp_sefm`, Scenario Editor/EEUIFScenarioEditor.cpp; the scenario
//! editor lane's round 4): every `EE_SCREENS_SEFM_OP_*` op and the form's `Close` composed of single C++ statements
//! (`EE_SCREENS_SEFMX_*`, `ee_screenscpp_sefmx`, `EEUIFScenarioEditor::Statement`) in the C++'s order, and the tab
//! groups', the unit tab's and the player tab's calls made on their Rust objects directly. [`compose`] answers every
//! op (0 for one the C++ host did not know).
//!
//! The statements: `p0` the object a statement works on (a control, a texture, a file ...), `p1` the form, `p2` a
//! second object; `i` and `f` as each says. A making statement answers its object's handle in i0 (low 32 bits) and
//! i1 (high).

// (the casts of `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_void;

use crate::screens::editor_calls as calls;
use crate::screens::editor_form::*;

// ---- the statements
/// `gRasterizer->GetTextureManager(form->GetManager()->GetScreen())->LoadTexture("textures\\SE Buttons", the
/// screen's description)` -> the texture; p0's `Reference()`.
pub const EE_SCREENS_SEFMX_BUTTON_TEXTURE: u32 = 0;
pub const EE_SCREENS_SEFMX_TEXTURE_REFERENCE: u32 = 1;
/// `new EEAScroll`, `new TMPoll(500, 0, false)`, p0's `Start()`.
pub const EE_SCREENS_SEFMX_NEW_SCROLL: u32 = 2;
pub const EE_SCREENS_SEFMX_NEW_POLL: u32 = 3;
pub const EE_SCREENS_SEFMX_POLL_START: u32 = 4;
/// The active rasterizer's `GetScreenCount()`, `GetIsScreenActivated(i0)`, `GetScreen(i0)` (a handle).
pub const EE_SCREENS_SEFMX_SCREEN_COUNT: u32 = 5;
pub const EE_SCREENS_SEFMX_SCREEN_ACTIVATED: u32 = 6;
pub const EE_SCREENS_SEFMX_SCREEN: u32 = 7;
/// The screen p0's texture manager's `LoadTexture("textures\\scenpart", 16 bits, no alpha)` -> the texture.
pub const EE_SCREENS_SEFMX_PART_TEXTURE: u32 = 8;
/// The texture p0's `SetFrame(0)`, `SetStageAlphaMode(kStageOpTexture, -1)`, `SetStageColorMode(kStageOpTexture,
/// -1)`, `SetStageMinFilterMode(0, -1)`, `SetStageMagFilterMode(0, -1)`, `SetStageMipFilterMode(kMipFilterNone, -1)`,
/// `SetStageMipMapBias(-1.0f, -1)`, `SetStageTextureAddressingMode(kAddressingWrap, -1)` (`_TEXTURE_SETUP + 0..7`).
pub const EE_SCREENS_SEFMX_TEXTURE_SETUP: u32 = 10;
/// `new GEBitmap`; p0's `SetTexture(p2, i0)`; `gScreenIndex`.
pub const EE_SCREENS_SEFMX_NEW_BITMAP: u32 = 20;
pub const EE_SCREENS_SEFMX_BITMAP_TEXTURE: u32 = 21;
pub const EE_SCREENS_SEFMX_SCREEN_INDEX: u32 = 22;
/// `new UIFullBackground("textures\\Scenfull", 4, 3, form, 1024, 768)`.
pub const EE_SCREENS_SEFMX_NEW_FULL_BACKGROUND: u32 = 23;
/// `new EEUIMiniMap(GERectangle(GE2DPoint(f0, f1), f2, f2), form, i0, 8, 3)`.
pub const EE_SCREENS_SEFMX_NEW_MINI_MAP: u32 = 24;
/// The form's `AddControl(p0, i0)`; p0's `SetDisplayMode(kDisplayModeHidden)`.
pub const EE_SCREENS_SEFMX_ADD_CONTROL: u32 = 25;
pub const EE_SCREENS_SEFMX_HIDE: u32 = 26;
/// The player combo box (`new UIComboBox(0x149, 1, no lines, gU531938, 140 x 20 at (0, 0), 400, form, i0, 0, 2)`);
/// the combo box p0's `SetTextBoxBgOpacity(f0)`, `SetListBoxSelectedBgColor(GEColor(f0, f1, f2))`.
pub const EE_SCREENS_SEFMX_NEW_PLAYER_COMBO: u32 = 27;
pub const EE_SCREENS_SEFMX_COMBO_TEXTBOX_OPACITY: u32 = 28;
pub const EE_SCREENS_SEFMX_COMBO_SELECTED_BG: u32 = 29;
/// The `GEText` of the combo box, the label or the button p0 (a handle); the text p0's `SetFgColor(GEColor(f0, f1,
/// f2))`, `SetIsTransparent(true)`.
pub const EE_SCREENS_SEFMX_COMBO_GETEXT: u32 = 30;
pub const EE_SCREENS_SEFMX_LABEL_GETEXT: u32 = 31;
pub const EE_SCREENS_SEFMX_BUTTON_GETEXT: u32 = 32;
pub const EE_SCREENS_SEFMX_TEXT_FG: u32 = 33;
pub const EE_SCREENS_SEFMX_TEXT_TRANSPARENT: u32 = 34;
/// p0's `SetBgOpacity(f0)`, `SetBgColor(GEColor(f0, f1, f2))`, `SetAlwaysDirty(true)`, `SetStringTableID(i0)`,
/// `SetTriggerEvent(i0, p2)`, `SetDirty(true)`.
pub const EE_SCREENS_SEFMX_BG_OPACITY: u32 = 35;
pub const EE_SCREENS_SEFMX_BG_COLOR: u32 = 36;
pub const EE_SCREENS_SEFMX_ALWAYS_DIRTY: u32 = 37;
pub const EE_SCREENS_SEFMX_STRING_ID: u32 = 38;
pub const EE_SCREENS_SEFMX_SET_TRIGGER: u32 = 39;
pub const EE_SCREENS_SEFMX_SET_DIRTY: u32 = 40;
/// `EECreateScreenEvent(i0)`; `new EESEERust(i0, p2)`; the event p0's `Dereference()`.
pub const EE_SCREENS_SEFMX_SCREEN_EVENT: u32 = 41;
pub const EE_SCREENS_SEFMX_SEE_EVENT: u32 = 42;
pub const EE_SCREENS_SEFMX_EVENT_RELEASE: u32 = 43;
/// The load text label (`new UITextLabel("", gU531930, 9, 300 x the font's height at (500, 0), form, 0, i0, 0)`),
/// the help label (`("", gU531934, 0x10, 400 x 400 at (0, 0), form, 1, i0, 0)`); the label p0's `SetAlignment(i0)`,
/// `SetFitRectToText(true)`.
pub const EE_SCREENS_SEFMX_NEW_LOAD_LABEL: u32 = 44;
pub const EE_SCREENS_SEFMX_NEW_HELP_LABEL: u32 = 45;
pub const EE_SCREENS_SEFMX_LABEL_ALIGN: u32 = 46;
pub const EE_SCREENS_SEFMX_LABEL_FIT: u32 = 47;
/// The form's `SetUseToolTips(true)`, `SetHelpTextTimeDelay(i0)`.
pub const EE_SCREENS_SEFMX_FORM_TOOL_TIPS: u32 = 48;
pub const EE_SCREENS_SEFMX_FORM_HELP_DELAY: u32 = 49;
/// The brush group's index (`sSefxIndex = i0`, read back), `new EEUIFSEBrushGroup(sSefxIndex, GE2DPoint(f0, f1),
/// form, gU531930)`.
pub const EE_SCREENS_SEFMX_INDEX_SET: u32 = 50;
pub const EE_SCREENS_SEFMX_INDEX_GET: u32 = 51;
pub const EE_SCREENS_SEFMX_NEW_BRUSH_GROUP: u32 = 52;
/// `new EEARust(kEEAEditorUndo)`, `(kEEAEditorRedo)`; the action button (`new EEUIShellButton(
/// EE_SCREENS_SK_ACTION_BUTTON, {p0}, i0, no bitmaps, 65 x 25 at (f0, f1), form, i1, 0)`).
pub const EE_SCREENS_SEFMX_NEW_UNDO_ACTION: u32 = 53;
pub const EE_SCREENS_SEFMX_NEW_REDO_ACTION: u32 = 54;
pub const EE_SCREENS_SEFMX_NEW_ACTION_BUTTON: u32 = 55;
/// `gResource->GetString(i0, sSefxText)`; the button p0's `SetText(sSefxText, gU531930)`.
pub const EE_SCREENS_SEFMX_STRING: u32 = 56;
pub const EE_SCREENS_SEFMX_BUTTON_TEXT: u32 = 57;
/// `gSeplScreenFont = gU531930`, `= gU531938`; `new EESEUnit(form)`; the tab group (`ee_screens_segr_new(i0, form,
/// its edge scroll, brush group and player combo box, the unit tab's parts p0, the index p2)`).
pub const EE_SCREENS_SEFMX_FONT_SMALL: u32 = 58;
pub const EE_SCREENS_SEFMX_FONT_LARGE: u32 = 59;
pub const EE_SCREENS_SEFMX_NEW_UNIT_PARTS: u32 = 60;
pub const EE_SCREENS_SEFMX_NEW_TAB_GROUP: u32 = 61;
/// A main button (`new EEUIShellRadio(EE_SCREENS_SK_MAIN_BUTTON, {flag, the button texture, tab, the four cells'
/// bits at p0}, ... the `EeScreensSefmButton` p2)`); the form's button texture (a handle), the screen
/// (`form->GetManager()->GetScreen()`); the bitmap p0's `ComputeStretchedCoordsAndScale(dest, dest, source, i0)` of
/// the eight floats at p2 (dest, source); a bitmap button (`new UIButton(i0, p0, NULL, p2, GERectangle(f0, f1, f2,
/// f3), form, i1, 0)`); the bitmap p0's `Dereference()`.
pub const EE_SCREENS_SEFMX_NEW_MAIN_RADIO: u32 = 62;
pub const EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE: u32 = 63;
pub const EE_SCREENS_SEFMX_FORM_SCREEN: u32 = 64;
pub const EE_SCREENS_SEFMX_BITMAP_STRETCH: u32 = 65;
pub const EE_SCREENS_SEFMX_NEW_BUTTON: u32 = 66;
pub const EE_SCREENS_SEFMX_BITMAP_RELEASE: u32 = 67;
/// `new EESEAQuickSaveLoad(gScenarioExtension)`.
pub const EE_SCREENS_SEFMX_NEW_QUICK_ACTION: u32 = 68;
/// The hide-UI check box (`new UICheckBox(kScenarioEditorHideUICheckBox, "", gU531934, 20 x 20 at (849, 718), form,
/// i0, 0, false)`), its `SetStringTableID(IDS_HIDE_UI)`, the check box p0's `SetChecked(true)`.
pub const EE_SCREENS_SEFMX_NEW_HIDE_UI: u32 = 69;
pub const EE_SCREENS_SEFMX_HIDE_UI_HELP: u32 = 70;
pub const EE_SCREENS_SEFMX_CHECK_SET: u32 = 71;
/// The form's `SetMouseWheelMoveEvent(p0)`, `SetMouseMiddleButtonPressedEvent(p0)`, `...UnpressedEvent(p0)`.
pub const EE_SCREENS_SEFMX_FORM_WHEEL_EVENT: u32 = 72;
pub const EE_SCREENS_SEFMX_FORM_MIDDLE_PRESSED: u32 = 73;
pub const EE_SCREENS_SEFMX_FORM_MIDDLE_UNPRESSED: u32 = 74;
/// `new UIHotKey`; p0's `SetKey(key i0)`, `SetJustPressedEvent(p2)`, `SetPressedEvent(p2)`,
/// `SetJustUnpressedEvent(p2)`; the form's `AddHotKey(*p0, 0)`; `delete` p0.
pub const EE_SCREENS_SEFMX_NEW_HOT_KEY: u32 = 75;
pub const EE_SCREENS_SEFMX_HOT_KEY_KEY: u32 = 76;
pub const EE_SCREENS_SEFMX_HOT_KEY_JUST_PRESSED: u32 = 77;
pub const EE_SCREENS_SEFMX_HOT_KEY_PRESSED: u32 = 78;
pub const EE_SCREENS_SEFMX_HOT_KEY_JUST_UNPRESSED: u32 = 79;
pub const EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY: u32 = 80;
pub const EE_SCREENS_SEFMX_HOT_KEY_DELETE: u32 = 81;
/// The form's `AddHotKey(key i0, i1, i2, kKeyNone, the four events at p0, group i3, false)`.
pub const EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY_KEYS: u32 = 82;
/// `EECreateGrabScroll(1)`, `EEReleaseGrabScroll(1)`.
pub const EE_SCREENS_SEFMX_GRAB_SCROLL: u32 = 83;
pub const EE_SCREENS_SEFMX_RELEASE_GRAB_SCROLL: u32 = 84;
/// `EEUIShell::Op(p0, EE_SCREENS_SKOP_CHECK_DIRECT)`; the radio p0's `Check()`.
pub const EE_SCREENS_SEFMX_CHECK_DIRECT: u32 = 85;
pub const EE_SCREENS_SEFMX_RADIO_CHECK: u32 = 86;
/// p0's `Dereference()` as a texture, an action, the edge scroll.
pub const EE_SCREENS_SEFMX_DEREF_TEXTURE: u32 = 87;
pub const EE_SCREENS_SEFMX_DEREF_ACTION: u32 = 88;
pub const EE_SCREENS_SEFMX_DEREF_SCROLL: u32 = 89;
/// `delete` p0 as a control, a tab group (`ee_screens_segr_free`'s unit tab parts deleted), the poll, the brush
/// group, the full background.
pub const EE_SCREENS_SEFMX_DELETE_CONTROL: u32 = 90;
pub const EE_SCREENS_SEFMX_DELETE_TAB_GROUP: u32 = 91;
pub const EE_SCREENS_SEFMX_DELETE_POLL: u32 = 92;
pub const EE_SCREENS_SEFMX_DELETE_BRUSH_GROUP: u32 = 93;
pub const EE_SCREENS_SEFMX_DELETE_FULL_BACKGROUND: u32 = 94;
/// `gGraphics->PostClearRenderObjects()`, `PostPushRenderObject(gU56534C)`, `EEPostGraphicsAll()`.
pub const EE_SCREENS_SEFMX_CLEAR_RENDER: u32 = 95;
pub const EE_SCREENS_SEFMX_PUSH_BASE_RENDER: u32 = 96;
pub const EE_SCREENS_SEFMX_POST_GRAPHICS_ALL: u32 = 97;
/// `gUIManager->RedrawAll()`, `gUIManager->GetCurrentForm()->SetAllControlsDirty()`.
pub const EE_SCREENS_SEFMX_REDRAW_ALL: u32 = 98;
pub const EE_SCREENS_SEFMX_CURRENT_FORM_DIRTY: u32 = 99;
/// The brush group p0's `mIsHighlightShown = i0 != 0`.
pub const EE_SCREENS_SEFMX_BRUSH_HIGHLIGHT: u32 = 100;
/// `EEUnitHandleResolve(i0, sSefxUnit)`, `sSefxUnit` (a handle); the unit p0's saved id.
pub const EE_SCREENS_SEFMX_RESOLVE: u32 = 101;
pub const EE_SCREENS_SEFMX_RESOLVED: u32 = 102;
pub const EE_SCREENS_SEFMX_UNIT_ID: u32 = 103;
/// The unit p0's `mIsEditorSelected = i0 != 0`, `Reference()`, `Dereference()`; its tile node != NULL, its complex
/// unit's `mCargoRoom > 0`, `GetNumCargoUnits() > 0`.
pub const EE_SCREENS_SEFMX_UNIT_EDITOR_SELECTED: u32 = 104;
pub const EE_SCREENS_SEFMX_UNIT_REFERENCE: u32 = 105;
pub const EE_SCREENS_SEFMX_UNIT_DEREFERENCE: u32 = 106;
pub const EE_SCREENS_SEFMX_UNIT_PLACED: u32 = 107;
pub const EE_SCREENS_SEFMX_UNIT_CARGO_ROOM: u32 = 108;
pub const EE_SCREENS_SEFMX_UNIT_CARGO_COUNT: u32 = 109;
/// `EESetGameViewRectangle(GERectangle(f0, f1, f2, f3))`.
pub const EE_SCREENS_SEFMX_GAME_VIEW: u32 = 110;
/// `new EESELoadThread(p2, 0)`; the load thread p0's `Post401()`, `Post404()`, `Post405()`, `Post40B(p2)`,
/// `Post40C(p2)`.
pub const EE_SCREENS_SEFMX_NEW_LOAD_THREAD: u32 = 111;
pub const EE_SCREENS_SEFMX_POST_401: u32 = 112;
pub const EE_SCREENS_SEFMX_POST_404: u32 = 113;
pub const EE_SCREENS_SEFMX_POST_405: u32 = 114;
pub const EE_SCREENS_SEFMX_POST_40B: u32 = 115;
pub const EE_SCREENS_SEFMX_POST_40C: u32 = 116;
/// `gWorld.RebuildActiveUnits()`; `gUIManager->mInScenarioEditor == true`.
pub const EE_SCREENS_SEFMX_REBUILD_ACTIVE_UNITS: u32 = 117;
pub const EE_SCREENS_SEFMX_IN_EDITOR: u32 = 118;
/// The camera's `SetMode(i0)`, `SetMode(gGamePreferences.mSettings.mZoomStyle)`, `SetOffsetTurn(sSefxTurn, i0 !=
/// 0)`, `SetChanged()`; `gWorld.RefreshUnits()`; `gSightUpdatesEnabled = false`; `sSefxTurn = GETransformation()`,
/// its `SetOrientationYPR(f0, f1, f2)`.
pub const EE_SCREENS_SEFMX_CAMERA_MODE: u32 = 119;
pub const EE_SCREENS_SEFMX_CAMERA_MODE_PREFERRED: u32 = 120;
pub const EE_SCREENS_SEFMX_CAMERA_OFFSET_TURN: u32 = 121;
pub const EE_SCREENS_SEFMX_CAMERA_CHANGED: u32 = 122;
pub const EE_SCREENS_SEFMX_REFRESH_UNITS: u32 = 123;
pub const EE_SCREENS_SEFMX_SIGHT_UPDATES_OFF: u32 = 124;
pub const EE_SCREENS_SEFMX_TURN_RESET: u32 = 125;
pub const EE_SCREENS_SEFMX_TURN_YPR: u32 = 126;
/// The camera's `SetViewTurn(sSefxTurn, false)`.
pub const EE_SCREENS_SEFMX_TURN_VIEW: u32 = 221;
/// The form's `mRust` (an `EeScreensSeForm*`); `gUIManager->GetScenarioEditorForm()`.
pub const EE_SCREENS_SEFMX_FORM_RUST: u32 = 222;
pub const EE_SCREENS_SEFMX_MANAGER_EDITOR: u32 = 223;
/// The full background p0's `LoadBackground(the screen's description)`, `DrawBackground(GERectangle(f0, f1, f2,
/// f3))` (its result); the form's full background's `UnloadBackground()`.
pub const EE_SCREENS_SEFMX_LOAD_BACKGROUND: u32 = 127;
pub const EE_SCREENS_SEFMX_DRAW_FULL: u32 = 128;
pub const EE_SCREENS_SEFMX_UNLOAD_BACKGROUND: u32 = 129;
/// `gGraphics->PostFadeIn(0)`, `gMusicManager->Pause()`, `gAmbientSoundManager.StopSounds()`, `.Pause()`.
pub const EE_SCREENS_SEFMX_FADE_IN: u32 = 130;
pub const EE_SCREENS_SEFMX_MUSIC_PAUSE: u32 = 131;
pub const EE_SCREENS_SEFMX_AMBIENT_STOP: u32 = 132;
pub const EE_SCREENS_SEFMX_AMBIENT_PAUSE: u32 = 133;
/// The edge scroll p0's `mEdges` the window less a pixel.
pub const EE_SCREENS_SEFMX_SCROLL_EDGES: u32 = 134;
/// `gFreeCamera = i0 != 0`, `gB529065 = i0 != 0`, `gB529066 = false`, `gCameraFollowing = false`, `gB529068 = false`.
pub const EE_SCREENS_SEFMX_FREE_CAMERA: u32 = 135;
pub const EE_SCREENS_SEFMX_B529065: u32 = 136;
pub const EE_SCREENS_SEFMX_B529066_OFF: u32 = 137;
pub const EE_SCREENS_SEFMX_FOLLOWING_OFF: u32 = 138;
pub const EE_SCREENS_SEFMX_B529068_OFF: u32 = 139;
/// The viewport's view rectangle's left, top, right, bottom into f0.
pub const EE_SCREENS_SEFMX_VIEW_LEFT: u32 = 140;
pub const EE_SCREENS_SEFMX_VIEW_TOP: u32 = 141;
pub const EE_SCREENS_SEFMX_VIEW_RIGHT: u32 = 142;
pub const EE_SCREENS_SEFMX_VIEW_BOTTOM: u32 = 143;
/// `gMiniMap != NULL`, its `RefreshView()`, `EEUIMiniMap::Update(gMiniMap)`, `gMiniMap->Redraw()`.
pub const EE_SCREENS_SEFMX_MINIMAP: u32 = 144;
pub const EE_SCREENS_SEFMX_MINIMAP_REFRESH_VIEW: u32 = 145;
pub const EE_SCREENS_SEFMX_MINIMAP_UPDATE: u32 = 146;
pub const EE_SCREENS_SEFMX_MINIMAP_REDRAW: u32 = 147;
/// `gWorld.mScenarioManager->mIsMovingCamera = false`; the form's `UIForm::Open()`, `UIForm::Close()`.
pub const EE_SCREENS_SEFMX_CAMERA_MOVE_OFF: u32 = 148;
pub const EE_SCREENS_SEFMX_FORM_OPEN: u32 = 149;
pub const EE_SCREENS_SEFMX_FORM_CLOSE: u32 = 150;
/// `new EESEARust(kSEAExit)`; `gUserInterface->PostAction(p0)`.
pub const EE_SCREENS_SEFMX_NEW_EXIT_ACTION: u32 = 151;
pub const EE_SCREENS_SEFMX_POST_ACTION: u32 = 152;
/// `EEPreviewPlacement(gMouse->mPosition)`; the form's manager's `AddDirtyRectangle(GERectangle(f0, f1, f2, f3),
/// form)`.
pub const EE_SCREENS_SEFMX_PREVIEW_PLACEMENT: u32 = 153;
pub const EE_SCREENS_SEFMX_ADD_DIRTY: u32 = 154;
/// The edge scroll p0's `Update(gMouse->mPosition)`.
pub const EE_SCREENS_SEFMX_SCROLL_UPDATE: u32 = 155;
/// The form's `UIForm::DrawBackground(p0, GERectangle(f0, f1, f2, f3), GERectangle(0, 0, 256, 256), the four
/// floats at p2)`.
pub const EE_SCREENS_SEFMX_DRAW_PANEL: u32 = 156;
/// `gISKeyboard->GetEntity(key i0, sSefxKey)`, its `JustPressed()`, `GetValue() != 0`.
pub const EE_SCREENS_SEFMX_KEY_FETCH: u32 = 157;
pub const EE_SCREENS_SEFMX_KEY_JUST_PRESSED: u32 = 158;
pub const EE_SCREENS_SEFMX_KEY_DOWN: u32 = 159;
/// The terrain object's `StepGridMode(i0 != 0)`.
pub const EE_SCREENS_SEFMX_GRID: u32 = 160;
/// The form's mouse location's x, y into f0.
pub const EE_SCREENS_SEFMX_MOUSE_X: u32 = 161;
pub const EE_SCREENS_SEFMX_MOUSE_Y: u32 = 162;
/// `gMouse->SetBounds` the window.
pub const EE_SCREENS_SEFMX_MOUSE_BOUNDS_WINDOW: u32 = 163;
/// The scenario manager's lists (`list` an `EE_SCREENS_SEFM_LIST_*`, record i1): `mTriggers.size()` ...
/// `mAreas.size()` (`_SM_COUNT + list`); `GetTrigger(i1)` ... `GetScenarioObject(i1)` as their `EeScenario*Fields`
/// records (`_SM_RECORD + list`, handles; the first four lists); `GetCondition(i1) != NULL` ... `GetArea(i1) != NULL`
/// (`_SM_EXISTS + list`, the last four); the record's `mName` (a handle on the `UWideString`, `_SM_NAME + list`), the
/// trigger's `mText`.
pub const EE_SCREENS_SEFMX_SM_COUNT: u32 = 170;
pub const EE_SCREENS_SEFMX_SM_RECORD: u32 = 175;
pub const EE_SCREENS_SEFMX_SM_EXISTS: u32 = 180;
pub const EE_SCREENS_SEFMX_SM_NAME: u32 = 185;
pub const EE_SCREENS_SEFMX_SM_TRIGGER_TEXT: u32 = 190;
/// Effect i1's `mObjects.size()`, `mObjects[i2]`; trigger i1's `mConditions.size()`, `mEffects.size()`, its
/// condition i2's `mIsNot`, `mValue`, `mOperator`, its effect i2's `mDelay`, `mEffectIndex`, `mOperator`.
pub const EE_SCREENS_SEFMX_SM_EFFECT_OBJECTS: u32 = 191;
pub const EE_SCREENS_SEFMX_SM_EFFECT_OBJECT: u32 = 192;
pub const EE_SCREENS_SEFMX_SM_TRIGGER_CONDITIONS: u32 = 193;
pub const EE_SCREENS_SEFMX_SM_TRIGGER_EFFECTS: u32 = 194;
pub const EE_SCREENS_SEFMX_SM_CONDITION_NOT: u32 = 195;
pub const EE_SCREENS_SEFMX_SM_CONDITION_VALUE: u32 = 196;
pub const EE_SCREENS_SEFMX_SM_CONDITION_OPERATOR: u32 = 197;
pub const EE_SCREENS_SEFMX_SM_EFFECT_DELAY: u32 = 198;
pub const EE_SCREENS_SEFMX_SM_EFFECT_INDEX: u32 = 199;
pub const EE_SCREENS_SEFMX_SM_EFFECT_OPERATOR: u32 = 220;
pub const EE_SCREENS_SEFMX_FILE_NEW: u32 = 200;
pub const EE_SCREENS_SEFMX_FILE_VOLUME: u32 = 201;
pub const EE_SCREENS_SEFMX_FILE_DIRECTORY: u32 = 202;
pub const EE_SCREENS_SEFMX_FILE_TRIGGERS: u32 = 203;
pub const EE_SCREENS_SEFMX_FILE_CREATE: u32 = 204;
pub const EE_SCREENS_SEFMX_FILE_OPEN_WRITE: u32 = 205;
pub const EE_SCREENS_SEFMX_FILE_OPEN_READ: u32 = 206;
pub const EE_SCREENS_SEFMX_FILE_CLOSE: u32 = 207;
pub const EE_SCREENS_SEFMX_FILE_DELETE: u32 = 208;
pub const EE_SCREENS_SEFMX_SM_SAVE: u32 = 209;
pub const EE_SCREENS_SEFMX_SM_LOAD: u32 = 210;
/// `PlatFOpen(the startup directory's "ScenarioTriggers.txt", "w")` (a handle); `fwrite(p0, 1, i0, p2)`;
/// `fclose(p0)`.
pub const EE_SCREENS_SEFMX_TEXT_OPEN: u32 = 211;
pub const EE_SCREENS_SEFMX_TEXT_WRITE: u32 = 212;
pub const EE_SCREENS_SEFMX_TEXT_CLOSE: u32 = 213;
/// The form p1's `UIForm::CopyData()`, `UIForm::UpdateKeyboard()`, `UIForm::UpdateMouse()` (their answers); its
/// `SetRectangle((f0, f1, f2, f3))`; its civilization's `SetPointsAndMax(the world table's builder points)`; its
/// `SetMusicFileName(the host's text)` (the constructor's, `ee_screens_sefm_construct`).
pub const EE_SCREENS_SEFMX_BASE_COPY_DATA: u32 = 2000;
pub const EE_SCREENS_SEFMX_BASE_UPDATE_KEYBOARD: u32 = 2001;
pub const EE_SCREENS_SEFMX_BASE_UPDATE_MOUSE: u32 = 2002;
pub const EE_SCREENS_SEFMX_SET_RECTANGLE: u32 = 2003;
pub const EE_SCREENS_SEFMX_CIVILIZATION_POINTS: u32 = 2004;
pub const EE_SCREENS_SEFMX_MUSIC_TEXT: u32 = 2005;

/// `kTextAlignLeft`, `kTextAlignRight`; `kControlTriggerSelectionChanged`, `kControlTriggerButtonUp`,
/// `kControlTriggerChecked`, `kControlTriggerUnchecked`; `EE_SCREENS_SE_NEW_PLAYER`; `kErrorOK`.
const SEFMX_ALIGN_LEFT: i32 = 0;
const SEFMX_ALIGN_RIGHT: i32 = 2;
const SEFMX_TRIGGER_SELECTION: i32 = 0x3eb;
const SEFMX_TRIGGER_BUTTON_UP: i32 = 0x3f0;
const SEFMX_TRIGGER_CHECKED: i32 = 0x3e9;
const SEFMX_TRIGGER_UNCHECKED: i32 = 0x3ea;
const SEFMX_SE_NEW_PLAYER: i32 = crate::screens::scenario_editor::EE_SCREENS_SE_NEW_PLAYER as i32;
const SEFMX_ERROR_OK: i32 = 0;
/// `kEditorTabUnits`; `SEFMX_IDS_UNDO`, `SEFMX_IDS_REDO`.
const SEFMX_TAB_UNITS: i32 = 3;
const SEFMX_IDS_UNDO: i32 = 20506;
const SEFMX_IDS_REDO: i32 = 20507;
/// The form's text yellow (`kSefmYellow`), white (`GEColor()`), black.
const SEFMX_YELLOW: [f32; 3] = [0.996_078_43, 0.968_627_45, 0.125_490_2];
const SEFMX_WHITE: [f32; 3] = [1.0, 1.0, 1.0];
const SEFMX_BLACK: [f32; 3] = [0.0, 0.0, 0.0];
/// The editor camera's offset turn (yaw, pitch: the C++'s 0.7853981852531433f, -0.6108652353286743f).
const SEFMX_EDITOR_YAW: f32 = core::f32::consts::FRAC_PI_4;
const SEFMX_EDITOR_PITCH: f32 = -0.610_865_24;

/// One statement: `x(op, p, i, f)`; the answer, i and f as the C++ leaves them.
pub type Sefmx<'a> = dyn FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 + 'a;

/// A statement over copies of its numbers.
fn st(x: &mut Sefmx, op: u32, p: [usize; 3], a: [i32; 4], f: [f32; 4]) -> i32 {
    let (mut a, mut f) = (a, f);
    x(op, p, &mut a, &mut f)
}

/// A statement on p0 with the form p1 and no numbers.
fn on(x: &mut Sefmx, op: u32, c: usize, form: usize) -> i32 {
    st(x, op, [c, form, 0], [0; 4], [0.0; 4])
}

/// A statement's float answer (f0).
fn fl(x: &mut Sefmx, op: u32, form: usize) -> f32 {
    let mut f = [0.0; 4];
    x(op, [0, form, 0], &mut [0; 4], &mut f);
    f[0]
}

/// A making statement: the object's handle, from i0 (low 32 bits) and i1 (high).
fn mk(x: &mut Sefmx, op: u32, p: [usize; 3], a: [i32; 4], f: [f32; 4]) -> usize {
    let (mut a, mut f) = (a, f);
    x(op, p, &mut a, &mut f);
    (a[0] as u32 as usize) | ((a[1] as u32 as usize) << 32)
}

fn rgb(c: [f32; 3]) -> [f32; 4] {
    [c[0], c[1], c[2], 0.0]
}

/// The text `t`'s colour and transparency (the C++'s `SefmColor`: the colour first).
fn color(x: &mut Sefmx, t: usize, c: [f32; 3]) {
    st(x, EE_SCREENS_SEFMX_TEXT_FG, [t, 0, 0], [0; 4], rgb(c));
    on(x, EE_SCREENS_SEFMX_TEXT_TRANSPARENT, t, 0);
}

/// The out word `k` of the op's out array p0.
///
/// # Safety
/// `out` points at the op's out words.
unsafe fn put(out: usize, k: usize, v: usize) {
    unsafe { *(out as *mut usize).add(k) = v };
}

/// The view rectangle (left, top, right, bottom).
fn view(x: &mut Sefmx) -> [f32; 4] {
    [
        fl(x, EE_SCREENS_SEFMX_VIEW_LEFT, 0),
        fl(x, EE_SCREENS_SEFMX_VIEW_TOP, 0),
        fl(x, EE_SCREENS_SEFMX_VIEW_RIGHT, 0),
        fl(x, EE_SCREENS_SEFMX_VIEW_BOTTOM, 0),
    ]
}

/// A record's `UWideString` at `w` copied into `out` (at most `cap` bytes) -> its byte length (the C++'s `SefmText`).
///
/// # Safety
/// `w` is a live `UWideString`; `out` holds `cap` bytes.
unsafe fn text(w: usize, out: usize, cap: i32) -> i32 {
    let w = w as *const crate::screens::hud_direct::CppWide;
    let (s, n) = unsafe { ((*w).s.string as *const u8, (*w).s.byte_length as i32) };
    let k = n.min(cap);
    if k > 0 {
        unsafe { core::ptr::copy_nonoverlapping(s, out as *mut u8, k as usize) };
    }
    n
}

/// The trigger-only save's file `ScenarioTriggers.scn` in the startup directory: made, named and opened for writing
/// (`write`, created first) or reading -> the file and its open's result.
pub(crate) fn triggers_file(x: &mut Sefmx, write: bool) -> (usize, i32) {
    let file = mk(x, EE_SCREENS_SEFMX_FILE_NEW, [0; 3], [0; 4], [0.0; 4]);
    on(x, EE_SCREENS_SEFMX_FILE_VOLUME, file, 0);
    on(x, EE_SCREENS_SEFMX_FILE_DIRECTORY, file, 0);
    on(x, EE_SCREENS_SEFMX_FILE_TRIGGERS, file, 0);
    let open = if write {
        on(x, EE_SCREENS_SEFMX_FILE_CREATE, file, 0);
        EE_SCREENS_SEFMX_FILE_OPEN_WRITE
    } else {
        EE_SCREENS_SEFMX_FILE_OPEN_READ
    };
    (file, on(x, open, file, 0))
}

/// The file's close and its end (the C++ local's destructor).
pub(crate) fn file_done(x: &mut Sefmx, file: usize) {
    on(x, EE_SCREENS_SEFMX_FILE_CLOSE, file, 0);
    on(x, EE_SCREENS_SEFMX_FILE_DELETE, file, 0);
}

/// `EESMLoadTriggers`: the trigger-only save loaded back into the scenario manager (`ScenarioTriggers.scn`).
pub(crate) fn load_triggers(x: &mut Sefmx) {
    let (file, r) = triggers_file(x, false);
    if r == SEFMX_ERROR_OK {
        on(x, EE_SCREENS_SEFMX_SM_LOAD, file, 0);
    }
    file_done(x, file);
}

/// `EEUIFScenarioEditor::Close`: the game camera mode, the full background unloaded, the game's render list, the
/// window's mouse bounds, `UIForm::Close`.
pub fn close(x: &mut Sefmx, form: usize) {
    st(x, EE_SCREENS_SEFMX_FREE_CAMERA, [0; 3], [0; 4], [0.0; 4]);
    on(x, EE_SCREENS_SEFMX_CAMERA_MODE_PREFERRED, 0, 0);
    on(x, EE_SCREENS_SEFMX_CAMERA_CHANGED, 0, 0);
    on(x, EE_SCREENS_SEFMX_UNLOAD_BACKGROUND, 0, form);
    on(x, EE_SCREENS_SEFMX_CLEAR_RENDER, 0, 0);
    on(x, EE_SCREENS_SEFMX_PUSH_BASE_RENDER, 0, 0);
    on(x, EE_SCREENS_SEFMX_MOUSE_BOUNDS_WINDOW, 0, 0);
    st(x, EE_SCREENS_SEFMX_B529065, [0; 3], [0; 4], [0.0; 4]);
    on(x, EE_SCREENS_SEFMX_FORM_CLOSE, 0, form);
}

/// The op composed (`p0` the out words or the op's object, `p1` the form, `p2` the op's second pointer).
///
/// # Safety
/// `p`, `i` and `f` are the op's arguments (the live form, controls and records it names; `p0` the out words of a
/// making op).
pub unsafe fn compose(x: &mut Sefmx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
    let (out, form, p2) = (p[0], p[1], p[2]);
    let ii = *i;
    let ff = *f;
    let none = [0.0f32; 4];
    match op {
        EE_SCREENS_SEFM_OP_BASICS => unsafe {
            let t = mk(x, EE_SCREENS_SEFMX_BUTTON_TEXTURE, [0, form, 0], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_TEXTURE_REFERENCE, t, form);
            put(out, 0, t);
            put(out, 1, mk(x, EE_SCREENS_SEFMX_NEW_SCROLL, [0; 3], [0; 4], none));
            let poll = mk(x, EE_SCREENS_SEFMX_NEW_POLL, [0; 3], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_POLL_START, poll, form);
            put(out, 2, poll);
            let mut screen = 0;
            let count = st(x, EE_SCREENS_SEFMX_SCREEN_COUNT, [0; 3], [0; 4], none) as u32;
            for k in 0..count {
                if st(x, EE_SCREENS_SEFMX_SCREEN_ACTIVATED, [0; 3], [k as i32, 0, 0, 0], none) != 0 {
                    screen = mk(x, EE_SCREENS_SEFMX_SCREEN, [0; 3], [k as i32, 0, 0, 0], none);
                    break;
                }
            }
            let t = mk(x, EE_SCREENS_SEFMX_PART_TEXTURE, [screen, form, 0], [0; 4], none);
            for k in 0..8 {
                on(x, EE_SCREENS_SEFMX_TEXTURE_SETUP + k, t, form);
            }
            let panel = mk(x, EE_SCREENS_SEFMX_NEW_BITMAP, [0; 3], [0; 4], none);
            let index = st(x, EE_SCREENS_SEFMX_SCREEN_INDEX, [0; 3], [0; 4], none);
            st(x, EE_SCREENS_SEFMX_BITMAP_TEXTURE, [panel, form, t], [index, 0, 0, 0], none);
            put(out, 3, panel);
            put(out, 4, mk(x, EE_SCREENS_SEFMX_NEW_FULL_BACKGROUND, [0, form, 0], [0; 4], none));
            0
        },
        EE_SCREENS_SEFM_OP_MINI_MAP => unsafe {
            let t = mk(x, EE_SCREENS_SEFMX_NEW_MINI_MAP, [0, form, 0], [ii[0], 0, 0, 0], ff);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_HIDE, t, form);
            put(out, 0, t);
            0
        },
        EE_SCREENS_SEFM_OP_PLAYER_COMBO => unsafe {
            let t = mk(x, EE_SCREENS_SEFMX_NEW_PLAYER_COMBO, [0, form, 0], [ii[0], 0, 0, 0], none);
            st(x, EE_SCREENS_SEFMX_COMBO_TEXTBOX_OPACITY, [t, form, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
            let g = mk(x, EE_SCREENS_SEFMX_COMBO_GETEXT, [t, form, 0], [0; 4], none);
            color(x, g, SEFMX_YELLOW);
            st(x, EE_SCREENS_SEFMX_COMBO_SELECTED_BG, [t, form, 0], [0; 4], [0.0, 0.0, 1.0, 0.0]);
            st(x, EE_SCREENS_SEFMX_BG_OPACITY, [t, form, 0], [0; 4], [1.0, 0.0, 0.0, 0.0]);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_HIDE, t, form);
            let e = mk(x, EE_SCREENS_SEFMX_SCREEN_EVENT, [0; 3], [SEFMX_SE_NEW_PLAYER, 0, 0, 0], none);
            st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], [SEFMX_TRIGGER_SELECTION, 0, 0, 0], none);
            *(p2 as *mut usize) = e;
            put(out, 0, t);
            0
        },
        EE_SCREENS_SEFM_OP_LABEL => unsafe {
            let t;
            if ii[0] == 0 {
                t = mk(x, EE_SCREENS_SEFMX_NEW_LOAD_LABEL, [0, form, 0], [ii[1], 0, 0, 0], none);
                st(x, EE_SCREENS_SEFMX_LABEL_ALIGN, [t, form, 0], [SEFMX_ALIGN_RIGHT, 0, 0, 0], none);
                st(x, EE_SCREENS_SEFMX_BG_OPACITY, [t, form, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
                let g = mk(x, EE_SCREENS_SEFMX_LABEL_GETEXT, [t, form, 0], [0; 4], none);
                color(x, g, SEFMX_YELLOW);
            } else {
                t = mk(x, EE_SCREENS_SEFMX_NEW_HELP_LABEL, [0, form, 0], [ii[1], 0, 0, 0], none);
                st(x, EE_SCREENS_SEFMX_LABEL_ALIGN, [t, form, 0], [SEFMX_ALIGN_LEFT, 0, 0, 0], none);
                st(x, EE_SCREENS_SEFMX_BG_OPACITY, [t, form, 0], [0; 4], [1.0, 0.0, 0.0, 0.0]);
                st(x, EE_SCREENS_SEFMX_BG_COLOR, [t, form, 0], [0; 4], rgb(SEFMX_WHITE));
                on(x, EE_SCREENS_SEFMX_FORM_TOOL_TIPS, 0, form);
                st(x, EE_SCREENS_SEFMX_FORM_HELP_DELAY, [0, form, 0], [600, 0, 0, 0], none);
                let g = mk(x, EE_SCREENS_SEFMX_LABEL_GETEXT, [t, form, 0], [0; 4], none);
                color(x, g, SEFMX_BLACK);
            }
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_HIDE, t, form);
            on(x, EE_SCREENS_SEFMX_ALWAYS_DIRTY, t, form);
            on(x, EE_SCREENS_SEFMX_LABEL_FIT, t, form);
            put(out, 0, t);
            0
        },
        EE_SCREENS_SEFM_OP_BRUSH_GROUP => unsafe {
            let index = p2 as *mut u32;
            st(x, EE_SCREENS_SEFMX_INDEX_SET, [0; 3], [*index as i32, 0, 0, 0], none);
            put(out, 0, mk(x, EE_SCREENS_SEFMX_NEW_BRUSH_GROUP, [0, form, 0], [0; 4], ff));
            *index = st(x, EE_SCREENS_SEFMX_INDEX_GET, [0; 3], [0; 4], none) as u32;
            0
        },
        EE_SCREENS_SEFM_OP_UNDO_REDO => unsafe {
            let redo = ii[0] != 0;
            let kind = if redo { EE_SCREENS_SEFMX_NEW_REDO_ACTION } else { EE_SCREENS_SEFMX_NEW_UNDO_ACTION };
            let a = mk(x, kind, [0; 3], [0; 4], none);
            let t = mk(x, EE_SCREENS_SEFMX_NEW_ACTION_BUTTON, [a, form, 0], [redo as i32, ii[1], 0, 0], ff);
            st(x, EE_SCREENS_SEFMX_STRING, [0; 3], [if redo { SEFMX_IDS_REDO } else { SEFMX_IDS_UNDO }, 0, 0, 0], none);
            on(x, EE_SCREENS_SEFMX_BUTTON_TEXT, t, form);
            let g = mk(x, EE_SCREENS_SEFMX_BUTTON_GETEXT, [t, form, 0], [0; 4], none);
            color(x, g, SEFMX_WHITE);
            put(out, 0, t);
            0
        },
        EE_SCREENS_SEFM_OP_TAB_GROUP => unsafe {
            let font = if ii[0] < SEFMX_TAB_UNITS { EE_SCREENS_SEFMX_FONT_SMALL } else { EE_SCREENS_SEFMX_FONT_LARGE };
            on(x, font, 0, 0);
            let parts = if ii[0] == SEFMX_TAB_UNITS {
                mk(x, EE_SCREENS_SEFMX_NEW_UNIT_PARTS, [0, form, 0], [0; 4], none)
            } else {
                0
            };
            put(out, 0, mk(x, EE_SCREENS_SEFMX_NEW_TAB_GROUP, [parts, form, p2], [ii[0], 0, 0, 0], none));
            0
        },
        EE_SCREENS_SEFM_OP_BUTTON => unsafe { button(x, out, form, &*(p2 as *const EeScreensSefmButton)) },
        EE_SCREENS_SEFM_OP_QUICK_ACTION => unsafe {
            put(out, 0, mk(x, EE_SCREENS_SEFMX_NEW_QUICK_ACTION, [0; 3], [0; 4], none));
            0
        },
        EE_SCREENS_SEFM_OP_HIDE_UI => unsafe {
            let t = mk(x, EE_SCREENS_SEFMX_NEW_HIDE_UI, [0, form, 0], [ii[0], 0, 0, 0], none);
            on(x, EE_SCREENS_SEFMX_HIDE_UI_HELP, t, form);
            on(x, EE_SCREENS_SEFMX_CHECK_SET, t, form);
            let e = mk(
                x,
                EE_SCREENS_SEFMX_SEE_EVENT,
                [0; 3],
                [crate::screens::editor_events::EE_SCREENS_SEE_HIDE_UI as i32, 0, 0, 0],
                none,
            );
            st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], [SEFMX_TRIGGER_CHECKED, 0, 0, 0], none);
            st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], [SEFMX_TRIGGER_UNCHECKED, 0, 0, 0], none);
            on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, form);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_HIDE, t, form);
            put(out, 0, t);
            0
        },
        EE_SCREENS_SEFM_OP_SCREEN_EVENT => unsafe {
            put(out, 0, mk(x, EE_SCREENS_SEFMX_SCREEN_EVENT, [0; 3], [ii[0], 0, 0, 0], none));
            0
        },
        EE_SCREENS_SEFM_OP_SEE_EVENT => unsafe {
            put(out, 0, mk(x, EE_SCREENS_SEFMX_SEE_EVENT, [0, 0, p2], [ii[0], 0, 0, 0], none));
            0
        },
        EE_SCREENS_SEFM_OP_MOUSE_EVENT => {
            if ii[0] == 0 {
                on(x, EE_SCREENS_SEFMX_FORM_WHEEL_EVENT, out, form);
            } else {
                on(x, EE_SCREENS_SEFMX_FORM_MIDDLE_PRESSED, out, form);
                on(x, EE_SCREENS_SEFMX_FORM_MIDDLE_UNPRESSED, out, form);
            }
            0
        }
        EE_SCREENS_SEFM_OP_HOT_KEY => unsafe {
            let e = out as *const usize;
            let k = mk(x, EE_SCREENS_SEFMX_NEW_HOT_KEY, [0; 3], [0; 4], none);
            st(x, EE_SCREENS_SEFMX_HOT_KEY_KEY, [k, form, 0], [ii[0], 0, 0, 0], none);
            on(x, EE_SCREENS_SEFMX_HOT_KEY_JUST_PRESSED, k, *e);
            st(x, EE_SCREENS_SEFMX_HOT_KEY_PRESSED, [k, 0, *e.add(1)], [0; 4], none);
            st(x, EE_SCREENS_SEFMX_HOT_KEY_JUST_UNPRESSED, [k, 0, *e.add(2)], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY, k, form);
            on(x, EE_SCREENS_SEFMX_HOT_KEY_DELETE, k, form);
            0
        },
        EE_SCREENS_SEFM_OP_ADD_HOT_KEY => {
            let mut events = [0usize; 4];
            events[(ii[3] & 3) as usize] = out;
            st(
                x,
                EE_SCREENS_SEFMX_FORM_ADD_HOT_KEY_KEYS,
                [events.as_ptr() as usize, form, 0],
                [ii[0], ii[1], ii[2], ii[3] >> 4],
                none,
            )
        }
        EE_SCREENS_SEFM_OP_GRAB_SCROLL => unsafe {
            put(out, 0, mk(x, EE_SCREENS_SEFMX_GRAB_SCROLL, [0; 3], [0; 4], none));
            0
        },
        EE_SCREENS_SEFM_OP_RELEASE_GRAB_SCROLL => on(x, EE_SCREENS_SEFMX_RELEASE_GRAB_SCROLL, 0, 0),
        EE_SCREENS_SEFM_OP_TAB_CHECK => {
            let k = if ii[0] != 0 { EE_SCREENS_SEFMX_CHECK_DIRECT } else { EE_SCREENS_SEFMX_RADIO_CHECK };
            on(x, k, out, form);
            0
        }
        EE_SCREENS_SEFM_OP_DEREFERENCE => {
            const KINDS: [u32; 5] = [
                EE_SCREENS_SEFMX_EVENT_RELEASE,
                EE_SCREENS_SEFMX_DEREF_TEXTURE,
                EE_SCREENS_SEFMX_BITMAP_RELEASE,
                EE_SCREENS_SEFMX_DEREF_ACTION,
                EE_SCREENS_SEFMX_DEREF_SCROLL,
            ];
            if let Some(&k) = KINDS.get(ii[0] as usize) {
                on(x, k, out, form);
            }
            0
        }
        EE_SCREENS_SEFM_OP_DELETE => {
            const KINDS: [u32; 5] = [
                EE_SCREENS_SEFMX_DELETE_CONTROL,
                EE_SCREENS_SEFMX_DELETE_TAB_GROUP,
                EE_SCREENS_SEFMX_DELETE_POLL,
                EE_SCREENS_SEFMX_DELETE_BRUSH_GROUP,
                EE_SCREENS_SEFMX_DELETE_FULL_BACKGROUND,
            ];
            if let Some(&k) = KINDS.get(ii[0] as usize) {
                on(x, k, out, form);
            }
            0
        }
        EE_SCREENS_SEFM_OP_GROUP_CALL => {
            // `GERectangle(f0, f1, f2, f3)` in its member order (bottom, left, right, top)
            let r = [ff[3], ff[0], ff[2], ff[1]];
            calls::segr_call(out, ii[0], ff[0], ff[1], r.as_ptr() as usize);
            0
        }
        EE_SCREENS_SEFM_OP_RENDER_LIST => {
            if ii[0] != 0 {
                on(x, EE_SCREENS_SEFMX_CLEAR_RENDER, 0, 0);
                on(x, EE_SCREENS_SEFMX_PUSH_BASE_RENDER, 0, 0);
            } else {
                on(x, EE_SCREENS_SEFMX_POST_GRAPHICS_ALL, 0, 0);
            }
            0
        }
        EE_SCREENS_SEFM_OP_REDRAW_ALL => {
            on(x, EE_SCREENS_SEFMX_REDRAW_ALL, 0, 0);
            on(x, EE_SCREENS_SEFMX_CURRENT_FORM_DIRTY, 0, 0);
            0
        }
        EE_SCREENS_SEFM_OP_HIGHLIGHT => st(x, EE_SCREENS_SEFMX_BRUSH_HIGHLIGHT, [out, form, 0], [ii[0], 0, 0, 0], none),
        EE_SCREENS_SEFM_OP_RESOLVE => unsafe {
            st(x, EE_SCREENS_SEFMX_RESOLVE, [0; 3], [ii[0], 0, 0, 0], none);
            put(out, 0, mk(x, EE_SCREENS_SEFMX_RESOLVED, [0; 3], [0; 4], none));
            0
        },
        EE_SCREENS_SEFM_OP_UNIT_ID => on(x, EE_SCREENS_SEFMX_UNIT_ID, out, form),
        EE_SCREENS_SEFM_OP_UNIT_SELECT => {
            if out != 0 {
                st(x, EE_SCREENS_SEFMX_UNIT_EDITOR_SELECTED, [out, form, 0], [ii[0], 0, 0, 0], none);
                if ii[1] > 0 {
                    on(x, EE_SCREENS_SEFMX_UNIT_REFERENCE, out, form);
                } else if ii[1] < 0 {
                    on(x, EE_SCREENS_SEFMX_UNIT_DEREFERENCE, out, form);
                }
            }
            0
        }
        EE_SCREENS_SEFM_OP_UNIT_CARGO => {
            (on(x, EE_SCREENS_SEFMX_UNIT_PLACED, out, form) != 0
                && (on(x, EE_SCREENS_SEFMX_UNIT_CARGO_ROOM, out, form) != 0
                    || on(x, EE_SCREENS_SEFMX_UNIT_CARGO_COUNT, out, form) != 0)) as i32
        }
        EE_SCREENS_SEFM_OP_SHOW_CARGO | EE_SCREENS_SEFM_OP_SHOW_STATUS => {
            let what = if op == EE_SCREENS_SEFM_OP_SHOW_CARGO {
                calls::EE_SCREENS_SEUN_SHOW_CARGO
            } else {
                calls::EE_SCREENS_SEUN_SHOW_UNIT
            };
            calls::seun_call(calls::segr_tab(out), what, (ii[0] != 0) as i32, form);
            0
        }
        EE_SCREENS_SEFM_OP_GAME_VIEW => st(x, EE_SCREENS_SEFMX_GAME_VIEW, [0; 3], [0; 4], ff),
        EE_SCREENS_SEFM_OP_LOAD_THREAD => unsafe {
            let thread = form;
            match ii[0] {
                0 => put(out, 0, mk(x, EE_SCREENS_SEFMX_NEW_LOAD_THREAD, [0, 0, p2], [0; 4], none)),
                EE_SCREENS_SEFM_POST_401 => {
                    on(x, EE_SCREENS_SEFMX_POST_401, thread, 0);
                }
                EE_SCREENS_SEFM_POST_404 => {
                    on(x, EE_SCREENS_SEFMX_POST_404, thread, 0);
                }
                EE_SCREENS_SEFM_POST_405 => {
                    on(x, EE_SCREENS_SEFMX_POST_405, thread, 0);
                }
                EE_SCREENS_SEFM_POST_40B => {
                    st(x, EE_SCREENS_SEFMX_POST_40B, [thread, 0, p2], [0; 4], none);
                }
                EE_SCREENS_SEFM_POST_40C => {
                    st(x, EE_SCREENS_SEFMX_POST_40C, [thread, 0, p2], [0; 4], none);
                }
                _ => {}
            }
            0
        },
        EE_SCREENS_SEFM_OP_TEST_PREPARE => {
            on(x, EE_SCREENS_SEFMX_REBUILD_ACTIVE_UNITS, 0, 0);
            on(x, EE_SCREENS_SEFMX_IN_EDITOR, 0, 0)
        }
        EE_SCREENS_SEFM_OP_OPEN_CAMERA => {
            on(x, EE_SCREENS_SEFMX_TURN_RESET, 0, 0);
            st(x, EE_SCREENS_SEFMX_CAMERA_MODE, [0; 3], [2, 0, 0, 0], none);
            on(x, EE_SCREENS_SEFMX_REFRESH_UNITS, 0, 0);
            on(x, EE_SCREENS_SEFMX_SIGHT_UPDATES_OFF, 0, 0);
            st(x, EE_SCREENS_SEFMX_TURN_YPR, [0; 3], [0; 4], [SEFMX_EDITOR_YAW, SEFMX_EDITOR_PITCH, 0.0, 0.0]);
            on(x, EE_SCREENS_SEFMX_CAMERA_OFFSET_TURN, 0, 0);
            on(x, EE_SCREENS_SEFMX_CAMERA_CHANGED, 0, 0);
            0
        }
        EE_SCREENS_SEFM_OP_LOAD_BACKGROUND => on(x, EE_SCREENS_SEFMX_LOAD_BACKGROUND, out, form),
        EE_SCREENS_SEFM_OP_OPEN_WORLD => {
            on(x, EE_SCREENS_SEFMX_FADE_IN, 0, 0);
            calls::sepl_call(calls::segr_tab(out), calls::EE_SCREENS_SEPL_REFRESH);
            on(x, EE_SCREENS_SEFMX_MUSIC_PAUSE, 0, 0);
            on(x, EE_SCREENS_SEFMX_AMBIENT_STOP, 0, 0);
            on(x, EE_SCREENS_SEFMX_AMBIENT_PAUSE, 0, 0);
            on(x, EE_SCREENS_SEFMX_SCROLL_EDGES, form, 0);
            st(x, EE_SCREENS_SEFMX_FREE_CAMERA, [0; 3], [1, 0, 0, 0], none);
            st(x, EE_SCREENS_SEFMX_B529065, [0; 3], [1, 0, 0, 0], none);
            *f = view(x);
            st(x, EE_SCREENS_SEFMX_GAME_VIEW, [0; 3], [0; 4], [0.0, 0.0, 1024.0, 606.0]);
            on(x, EE_SCREENS_SEFMX_B529066_OFF, 0, 0);
            on(x, EE_SCREENS_SEFMX_FOLLOWING_OFF, 0, 0);
            on(x, EE_SCREENS_SEFMX_B529068_OFF, 0, 0);
            if on(x, EE_SCREENS_SEFMX_MINIMAP, 0, 0) != 0 {
                on(x, EE_SCREENS_SEFMX_MINIMAP_REFRESH_VIEW, 0, 0);
            }
            on(x, EE_SCREENS_SEFMX_MINIMAP_UPDATE, 0, 0);
            0
        }
        EE_SCREENS_SEFM_OP_OPEN_FORM => {
            on(x, EE_SCREENS_SEFMX_CAMERA_MOVE_OFF, 0, 0);
            on(x, EE_SCREENS_SEFMX_MINIMAP_REDRAW, 0, 0);
            on(x, EE_SCREENS_SEFMX_FORM_OPEN, 0, form);
            0
        }
        EE_SCREENS_SEFM_OP_POST_EXIT => {
            let a = mk(x, EE_SCREENS_SEFMX_NEW_EXIT_ACTION, [0; 3], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_POST_ACTION, a, 0);
            on(x, EE_SCREENS_SEFMX_DEREF_ACTION, a, 0);
            0
        }
        EE_SCREENS_SEFM_OP_COPY_WORLD => {
            if ii[0] == 2 {
                on(x, EE_SCREENS_SEFMX_PREVIEW_PLACEMENT, 0, 0);
                return 0;
            }
            if ii[0] == 1 {
                let mut r = view(x);
                r[2] -= 1.0;
                r[3] -= 1.0;
                st(x, EE_SCREENS_SEFMX_ADD_DIRTY, [0, form, 0], [0; 4], r);
            }
            on(x, EE_SCREENS_SEFMX_SET_DIRTY, out, form);
            0
        }
        EE_SCREENS_SEFM_OP_EDGE_SCROLL => on(x, EE_SCREENS_SEFMX_SCROLL_UPDATE, out, form),
        EE_SCREENS_SEFM_OP_DRAW_FULL => st(x, EE_SCREENS_SEFMX_DRAW_FULL, [out, form, 0], [0; 4], ff),
        EE_SCREENS_SEFM_OP_DRAW_PANEL => st(x, EE_SCREENS_SEFMX_DRAW_PANEL, [out, form, p2], [0; 4], ff),
        EE_SCREENS_SEFM_OP_KEY => {
            st(x, EE_SCREENS_SEFMX_KEY_FETCH, [0; 3], [ii[0], 0, 0, 0], none);
            let pressed = on(x, EE_SCREENS_SEFMX_KEY_JUST_PRESSED, 0, 0) != 0;
            let down = on(x, EE_SCREENS_SEFMX_KEY_DOWN, 0, 0) != 0;
            (pressed as i32) | ((down as i32) << 1)
        }
        EE_SCREENS_SEFM_OP_GRID => st(x, EE_SCREENS_SEFMX_GRID, [0; 3], [ii[0], 0, 0, 0], none),
        EE_SCREENS_SEFM_OP_PLAYER_COUNT => {
            calls::sepl_call(calls::segr_tab(out), calls::EE_SCREENS_SEPL_PLAYER_COUNT) as i32
        }
        EE_SCREENS_SEFM_OP_MOUSE => {
            f[0] = fl(x, EE_SCREENS_SEFMX_MOUSE_X, form);
            f[1] = fl(x, EE_SCREENS_SEFMX_MOUSE_Y, form);
            0
        }
        EE_SCREENS_SEFM_OP_SM_COUNT => on(x, EE_SCREENS_SEFMX_SM_COUNT + list(ii[0]), 0, 0),
        EE_SCREENS_SEFM_OP_SM_RECORD => unsafe {
            let l = list(ii[0]);
            let r = if l == EE_SCREENS_SEFM_LIST_AREAS as u32 || !exists(x, l, ii[1]) {
                0
            } else {
                mk(x, EE_SCREENS_SEFMX_SM_RECORD + l, [0; 3], [0, ii[1], 0, 0], none)
            };
            put(out, 0, r);
            0
        },
        EE_SCREENS_SEFM_OP_SM_TEXT => unsafe {
            let l = list(ii[0]);
            if !exists(x, l, ii[1]) {
                return -1;
            }
            let which =
                if l == 0 && ii[2] != 0 { EE_SCREENS_SEFMX_SM_TRIGGER_TEXT } else { EE_SCREENS_SEFMX_SM_NAME + l };
            text(mk(x, which, [0; 3], [0, ii[1], 0, 0], none), out, ii[3])
        },
        EE_SCREENS_SEFM_OP_SM_VEC_LEN => {
            let k = [0, ii[1], 0, 0];
            if ii[0] == EE_SCREENS_SEFM_VEC_OBJECTS {
                let l = EE_SCREENS_SEFM_LIST_EFFECTS as u32;
                return if !exists(x, l, ii[1]) {
                    0
                } else {
                    st(x, EE_SCREENS_SEFMX_SM_EFFECT_OBJECTS, [0; 3], k, none)
                };
            }
            if !exists(x, EE_SCREENS_SEFM_LIST_TRIGGERS as u32, ii[1]) {
                return 0;
            }
            let s = if ii[0] == EE_SCREENS_SEFM_VEC_CONDITIONS {
                EE_SCREENS_SEFMX_SM_TRIGGER_CONDITIONS
            } else {
                EE_SCREENS_SEFMX_SM_TRIGGER_EFFECTS
            };
            st(x, s, [0; 3], k, none)
        }
        EE_SCREENS_SEFM_OP_SM_VEC_AT => {
            let kn = [0, ii[1], ii[2], 0];
            if ii[0] == EE_SCREENS_SEFM_VEC_OBJECTS {
                i[0] = st(x, EE_SCREENS_SEFMX_SM_EFFECT_OBJECT, [0; 3], kn, none);
            } else {
                let ks = if ii[0] == EE_SCREENS_SEFM_VEC_CONDITIONS {
                    [
                        EE_SCREENS_SEFMX_SM_CONDITION_NOT,
                        EE_SCREENS_SEFMX_SM_CONDITION_VALUE,
                        EE_SCREENS_SEFMX_SM_CONDITION_OPERATOR,
                    ]
                } else {
                    [
                        EE_SCREENS_SEFMX_SM_EFFECT_DELAY,
                        EE_SCREENS_SEFMX_SM_EFFECT_INDEX,
                        EE_SCREENS_SEFMX_SM_EFFECT_OPERATOR,
                    ]
                };
                for (k, &s) in ks.iter().enumerate() {
                    i[k] = st(x, s, [0; 3], kn, none);
                }
            }
            0
        }
        EE_SCREENS_SEFM_OP_REPORT_SAVE => {
            let (file, r) = triggers_file(x, true);
            if r == SEFMX_ERROR_OK {
                on(x, EE_SCREENS_SEFMX_SM_SAVE, file, 0);
            }
            file_done(x, file);
            r
        }
        EE_SCREENS_SEFM_OP_REPORT_WRITE => {
            let t = mk(x, EE_SCREENS_SEFMX_TEXT_OPEN, [0; 3], [0; 4], none);
            if t != 0 {
                st(x, EE_SCREENS_SEFMX_TEXT_WRITE, [out, 0, t], [ii[0], 0, 0, 0], none);
                on(x, EE_SCREENS_SEFMX_TEXT_CLOSE, t, 0);
            }
            0
        }
        _ => 0,
    }
}

/// The list number of `EE_SCREENS_SEFM_LIST_*` (anything else: the areas, as the C++'s fall through).
fn list(l: i32) -> u32 {
    if (EE_SCREENS_SEFM_LIST_TRIGGERS..=EE_SCREENS_SEFM_LIST_AREAS).contains(&l) {
        l as u32
    } else {
        EE_SCREENS_SEFM_LIST_AREAS as u32
    }
}

/// Whether record `k` of list `l` is there (a trigger: below the trigger count; the others: their `Get` not NULL).
fn exists(x: &mut Sefmx, l: u32, k: i32) -> bool {
    if l == EE_SCREENS_SEFM_LIST_TRIGGERS as u32 {
        return (k as u32) < on(x, EE_SCREENS_SEFMX_SM_COUNT, 0, 0) as u32;
    }
    st(x, EE_SCREENS_SEFMX_SM_EXISTS + l, [0; 3], [0, k, 0, 0], [0.0; 4]) != 0
}

/// `EE_SCREENS_SEFM_OP_BUTTON`: a main button (kind 0, a radio over the button texture's cells) or a bitmap button
/// with its button-up event.
fn button(x: &mut Sefmx, out: usize, form: usize, b: &EeScreensSefmButton) -> i32 {
    let none = [0.0f32; 4];
    let p2 = b as *const EeScreensSefmButton as usize;
    let t = if b.kind == 0 {
        let bits = [b.normal[0].to_bits(), b.normal[1].to_bits(), b.pressed[0].to_bits(), b.pressed[1].to_bits()];
        let t = mk(x, EE_SCREENS_SEFMX_NEW_MAIN_RADIO, [bits.as_ptr() as usize, form, p2], [0; 4], none);
        st(x, EE_SCREENS_SEFMX_STRING_ID, [t, form, 0], [b.help as i32, 0, 0, 0], none);
        st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
        t
    } else {
        let screen = on(x, EE_SCREENS_SEFMX_FORM_SCREEN, 0, form);
        let normal = mk(x, EE_SCREENS_SEFMX_NEW_BITMAP, [0; 3], [0; 4], none);
        let pressed = mk(x, EE_SCREENS_SEFMX_NEW_BITMAP, [0; 3], [0; 4], none);
        let texture = mk(x, EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE, [0, form, 0], [0; 4], none);
        st(x, EE_SCREENS_SEFMX_BITMAP_TEXTURE, [normal, form, texture], [screen, 0, 0, 0], none);
        let texture = mk(x, EE_SCREENS_SEFMX_FORM_BUTTON_TEXTURE, [0, form, 0], [0; 4], none);
        st(x, EE_SCREENS_SEFMX_BITMAP_TEXTURE, [pressed, form, texture], [screen, 0, 0, 0], none);
        for (bitmap, cell) in [(normal, b.normal), (pressed, b.pressed)] {
            let r = [b.dest[0], b.dest[1], b.dest[2], b.dest[3], cell[0], cell[1], cell[0] + 32.0, cell[1] + 32.0];
            st(x, EE_SCREENS_SEFMX_BITMAP_STRETCH, [bitmap, form, r.as_ptr() as usize], [screen, 0, 0, 0], none);
        }
        let t =
            mk(x, EE_SCREENS_SEFMX_NEW_BUTTON, [normal, form, pressed], [b.id as i32, b.index as i32, 0, 0], b.rect);
        st(x, EE_SCREENS_SEFMX_STRING_ID, [t, form, 0], [b.help as i32, 0, 0, 0], none);
        st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [0; 4], none);
        let e = if b.object != 0 {
            mk(x, EE_SCREENS_SEFMX_SEE_EVENT, [0, 0, b.object], [b.event as i32, 0, 0, 0], none)
        } else {
            mk(x, EE_SCREENS_SEFMX_SCREEN_EVENT, [0; 3], [b.event as i32, 0, 0, 0], none)
        };
        st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], [SEFMX_TRIGGER_BUTTON_UP, 0, 0, 0], none);
        on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, form);
        on(x, EE_SCREENS_SEFMX_BITMAP_RELEASE, normal, form);
        on(x, EE_SCREENS_SEFMX_BITMAP_RELEASE, pressed, form);
        t
    };
    // SAFETY: the op's out word
    unsafe { put(out, 0, t) };
    0
}

// ---- the C ABI

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sefmx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

/// The C++ statement host (`ee_screenscpp_sefmx`).
pub fn cpp(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
    let p = p.map(|v| v as *mut c_void);
    // SAFETY: the statement's contract (the composer passes each statement's objects)
    unsafe { sys::ee_screenscpp_sefmx(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
}

/// `EEUIFScenarioEditor::Close` (the C++ member forwards here with the form).
///
/// # Safety
/// `form` is the live editor form.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_close(form: *mut c_void) {
    close(&mut cpp, form as usize);
}

#[cfg(test)]
#[path = "editor_sefmx_tests.rs"]
mod tests;
