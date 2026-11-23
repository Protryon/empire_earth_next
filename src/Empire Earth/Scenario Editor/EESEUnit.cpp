// C:\EEX\Empire Earth\Scenario Editor\EESEUnit.cpp (file name inferred: no __FILE__; RTTI class EESEUnit). The scenario
// editor's unit tab: 0x199afd..0x19cc39. Its helper controls first, in definition order (EEUIFSELBUnitList,
// EEUIFSELBCargoList, EEUIFSEBUnitType, EEUIFSEBRotation, EEUIFSEBRemoveCargo: not alphabetical, so one unit), then
// the tab's ctor, the inline action ctors it refused, the members, and EEUIBuildWall's ctor COMDAT (first user: the
// ctor) with the build modes' ??_Gs. After EESEUITechListBox.cpp, before EEUIFScenarioEditor.cpp (0x19cc39). Unit
// flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/USingleton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEPlayer.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EESEARust.h"
#include "ee/EESEUICCommandUnits.h"
#include "ee/EESELoadThread.h"
#include "ee/EEMiniMap.h"
#include "rust/game/screens.h"
#include "ee/EEUIShell.h"


// cdecl helpers of other units (provisional names; types inferred)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);  // 0x23eb4c (wall ghosts)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2: a gate fits on inTile's wall

// The upgrade panel is a real 0x60-byte class (new at 0x59b976 in the tab's ctor); both its ctor counters
// are mutable
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEStringIDs.h"






// The sort radio buttons' event (RTTI EEUIFSEESortUnitList > UIEvent; vtable 0x444e88; new 0xc): an EESEERust
// (ee_screens editor_events.rs)


// (the unit tab's controls are shell kinds: ee_game shells_unit.rs)

// (the unit type buttons' cell width, the .rdata float 0x444e7c: editor_seunx.rs's W)



// ---- the unit tab's host (editor_unit.rs; EE_SCREENS_SEUN_OP_*): the tab's own controls, the actions, the world,
// the mouse, the build tools and the unit classes


namespace {

EESEARust* SeunAction(void* inAction)
{
    return static_cast<EESEARust*>(static_cast<EEAction*>(inAction));
}

EEUIFScenarioEditor* SeunEditor(void* inControl)
{
    return static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(inControl)->GetForm());
}

EESEUnit* SeunTab(void* inTab)
{
    return static_cast<EESEUnit*>(inTab);
}

// the click and the lasso (EEActions) as their classes; the pair of them a statement takes
EESEAMouseClick* SeunClick(void* inAction)
{
    return static_cast<EESEAMouseClick*>(static_cast<EEAction*>(inAction));
}

EESEALasso* SeunLasso(void* inAction)
{
    return static_cast<EESEALasso*>(static_cast<EEAction*>(inAction));
}

void* const* SeunPair(void* inPair)
{
    return static_cast<void* const*>(inPair);
}

// the statements' locals (editor_seunx.rs): a string, the wall's tiles
UWideString sSeunxText;
std::vector<EETileCoordinate> sSeunxTiles;

}   // namespace

// The unit tab's host (editor_unit.rs EE_SCREENS_SEUN_OP_*, from 300; p: three handles, i: four integers, f: four
// floats; the class table's count and placement flags are Rust's, ee_game editor_world.rs) and its single statements
// (editor_seunx.rs EE_SCREENS_SEUNX_*: p0 the made object's out slot, else as named; the Rust composes the host's
// sequences; the type buttons' texture coordinates are Rust's, ee_render bmpobj.rs)
extern "C" int32_t ee_screenscpp_seunx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEUN_OP_SORT_EVENT: EESEOutSlot(p) = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_SORT_UNIT_LIST)); return 0;
    case EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS:
        EEClearGateHighlights(gWorld.GetPlayer(SeunTab(p[0])->mBuildGate.mPlayer), SeunTab(p[0])->mBuildGate.mWallType);
        return 0;
    case EE_SCREENS_SEUN_OP_REDRAW: static_cast<UIControl*>(p[0])->GetForm()->GetManager()->RedrawAll(); return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_SELECTION: SeunEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEUN_OP_SHOW_UNIT:
        ee_screens_seup_show(p[0], p[1]);
        return 0;
    case EE_SCREENS_SEUN_OP_TYPE_MODE: return ee_screens_shell_unit_type_mode();
    case EE_SCREENS_SEUN_OP_LIST_TOP: static_cast<UILBString*>(p[0])->SetScrollBarCurrentRange(0); return 0;
    case EE_SCREENS_SEUN_OP_CARGO_CLEAR:
        EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_CARGO_CLEAR);
        return 0;
    case EE_SCREENS_SEUN_OP_SELECTED_COUNT: return (int32_t)SeunEditor(p[1])->GetSelectedUnitCount();
    case EE_SCREENS_SEUN_OP_CARGO_ADD:
        EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_CARGO_ADD, (int64_t)SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit->mCargo[(unsigned long)i[1]]);
        return 0;
    case EE_SCREENS_SEUN_OP_GATE_PLAYER:   // (the build tools' Cancel: editor_unit.rs cancel_gate, cancel_wall)
        EESEOutSlot(p) = gWorld.mPlayers[static_cast<EEUIBuildGate*>(p[1])->mPlayer];
        return 0;
    case EE_SCREENS_SEUN_OP_WALL_PLAYER:
        EESEOutSlot(p) = gWorld.mPlayers[static_cast<EEUIBuildWall*>(p[1])->mPlayer];
        return 0;
    case EE_SCREENS_SEUN_OP_UNLOAD_GATE:
        static_cast<EEPlayer*>(p[1])->UnloadUnitClassMedia(static_cast<EEUnitClass*>(static_cast<EEUIBuildGate*>(p[2])->mUnitClass)->mUnitType);
        return 0;
    case EE_SCREENS_SEUN_OP_UNLOAD_WALL:
        static_cast<EEPlayer*>(p[1])->UnloadUnitClassMedia(static_cast<EEPlayer*>(p[1])->mTechnologies->GetData(static_cast<EEUIBuildWall*>(p[2])->mTechnology)->mUnitClass);
        return 0;
    case EE_SCREENS_SEUN_OP_LIST_SORT: EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_UL_SORT, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_LIST_EMPTY: EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_UL_CLEAR); return 0;
    case EE_SCREENS_SEUN_OP_LIST_ADD: EEUIShell::Op(static_cast<UIControl*>(p[0]), EE_SCREENS_SKOP_UL_ADD, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_IN_VIEW: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1])) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_START_SELECTION:
        static_cast<EEUICSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_COMMAND:
        static_cast<EESEUICCommandUnits*>(p[0])->StartCommand(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_ROTATION_KEYS: static_cast<UIControl*>(p[0])->UpdateKeyboard(); return 0;
    case EE_SCREENS_SEUNX_NEW_BITMAP:
        EESEOutSlot(p) = new GEBitmap;
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXTURE:
        EESEOutSlot(p) = static_cast<EEUIFScenarioEditor*>(EESEForm(p))->GetButtonTexture();
        return 0;
    case EE_SCREENS_SEUNX_TEXTURE_WIDTH: return (int32_t)static_cast<GETexture*>(p[1])->GetWidth();
    case EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM:
        static_cast<GEBitmap*>(p[0])->SetTexture(static_cast<GETexture*>(p[1]), static_cast<UIForm*>(p[2])->GetManager()->GetScreen());
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_RUST:
        EESEOutSlot(p) = static_cast<GEBitmap*>(p[1])->GetRust();
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_SCALE:
        static_cast<GEBitmap*>(p[1])->SetXYScale(f[0], f[1]);
        return 0;
    case EE_SCREENS_SEUNX_TYPE_RADIO:   // i: index, id, checked; f the rectangle
        EESEOutSlot(p) = static_cast<UIRadioButton*>(new EEUIShellRadio(EE_SCREENS_SK_UNIT_TYPE_BUTTON, {}, i[1], UWideString(""), gU531930,
                                                                         EESERect(f), EESEForm(p), i[0], 0, true, i[2] != 0, false));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_UP_CHECKED:
        static_cast<UIRadioButton*>(p[1])->SetUpCheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DOWN_CHECKED:
        static_cast<UIRadioButton*>(p[1])->SetDownCheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DOWN_UNCHECKED:
        static_cast<UIRadioButton*>(p[1])->SetDownUncheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_UP_UNCHECKED:
        static_cast<UIRadioButton*>(p[1])->SetUpUncheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_NO_FOCUS:
        static_cast<UIRadioButton*>(p[1])->SetFocusedBitmap(NULL);
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DRAW_OFFSET:
        static_cast<UIRadioButton*>(p[1])->SetBitmapDrawOffset(GE2DPoint(0.0f, 0.0f));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_TRIGGER:
        static_cast<UIRadioButton*>(p[1])->SetTriggerEvent(kControlTriggerButtonUp, static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_STRING_ID:
        static_cast<UIRadioButton*>(p[1])->SetStringTableID((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_ADD_RADIO:
        EESEForm(p)->AddControl(static_cast<UIRadioButton*>(p[2]), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_STRING_REMOVE:
        gResource->GetString(IDS_SE_REMOVE, sSeunxText);
        return 0;
    case EE_SCREENS_SEUNX_NEW_BUTTON:   // the remove cargo and rotation buttons: i index, kind, id, border; f the rectangle
        EESEOutSlot(p) = static_cast<UIButton*>(new EEUIShellButton(i[1], {}, i[2], NULL, NULL, NULL, EESERect(f), EESEForm(p), i[0], i[3]));
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_SET_TEXT:
        static_cast<UIButton*>(p[1])->SetText(sSeunxText, gSeplScreenFont);
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_OPAQUE:
        static_cast<UIButton*>(p[1])->SetBgOpacity(1.0f);
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXT_WHITE:
        static_cast<UIButton*>(p[1])->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXT_TRANSPARENT:
        static_cast<UIButton*>(p[1])->GetGEText()->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SEUNX_ADD_BUTTON:
        EESEForm(p)->AddControl(static_cast<UIButton*>(p[2]), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_ROTATION_STRING:
        static_cast<UIButton*>(p[1])->SetStringTableID(IDS_ROTATE);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_CLICK:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEAMouseClick);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_LASSO:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEALasso);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_UNIT:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_UNIT));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_GATE:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_GATE));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_WALL:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_WALL));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_MOVE_UNIT:
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_MOVE_UNIT));
        return 0;
    case EE_SCREENS_SEUNX_SELECT_UNITS:   // p2 the click and the lasso
        EESEOutSlot(p) = new EEUICSelectUnits(EESEForm(p), 0x3f6, i[0], SeunClick(SeunPair(p[2])[0]), SeunLasso(SeunPair(p[2])[1]));
        return 0;
    case EE_SCREENS_SEUNX_ADD_SELECT_UNITS:
        EESEForm(p)->AddControl(static_cast<EEUICSelectUnits*>(p[2]), 0);
        return 0;
    case EE_SCREENS_SEUNX_COMMAND_UNITS:
        EESEOutSlot(p) = new EESEUICCommandUnits(EESEForm(p), 0x3f7, i[0], SeunClick(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_ADD_COMMAND_UNITS:
        EESEForm(p)->AddControl(static_cast<EESEUICCommandUnits*>(p[2]), 0);
        return 0;
    case EE_SCREENS_SEUNX_LOAD_SCENUNIT:
        EESEOutSlot(p) = gTextureManager->LoadTexture("textures\\scenunit",
                                               GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, false});
        return 0;
    case EE_SCREENS_SEUNX_TEX_FRAME:
        static_cast<GETexture*>(p[1])->SetFrame(0);
        return 0;
    case EE_SCREENS_SEUNX_TEX_ALPHA:
        static_cast<GETexture*>(p[1])->SetStageAlphaMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_COLOR:
        static_cast<GETexture*>(p[1])->SetStageColorMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MIN:
        static_cast<GETexture*>(p[1])->SetStageMinFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MAG:
        static_cast<GETexture*>(p[1])->SetStageMagFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MIP:
        static_cast<GETexture*>(p[1])->SetStageMipFilterMode(kMipFilterNone, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_BIAS:
        static_cast<GETexture*>(p[1])->SetStageMipMapBias(-1.0f, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_WRAP:
        static_cast<GETexture*>(p[1])->SetStageTextureAddressingMode(kAddressingWrap, -1);
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_TEXTURE_SCREEN:
        static_cast<GEBitmap*>(p[1])->SetTexture(static_cast<GETexture*>(p[2]), gScreenIndex);
        return 0;
    case EE_SCREENS_SEUNX_UPGRADE_PANEL:
        EESEOutSlot(p) = ee_screens_seup_new(static_cast<unsigned long*>(p[2]), static_cast<unsigned long*>(p[2]) + 1, EESEForm(p));
        return 0;
    case EE_SCREENS_SEUNX_RELEASE_ACTION:
        static_cast<EEAction*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SEUNX_RELEASE_BITMAP:
        static_cast<GEBitmap*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SEUNX_RELEASE_EVENT:
        static_cast<UIEvent*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SEUNX_DELETE_PANEL:
        ee_screens_seup_free(p[1]);
        return 0;
    case EE_SCREENS_SEUNX_SEW_ACTION:
        EESEOutSlot(p) = SeunAction(p[1])->mAction;
        return 0;
    case EE_SCREENS_SEUNX_WALL_START:
        memcpy(f, &SeunTab(p[1])->mBuildWall.mStart, sizeof(GE2DPoint));
        return 0;
    case EE_SCREENS_SEUNX_WALL_END:
        memcpy(f, &SeunTab(p[1])->mBuildWall.mEnd, sizeof(GE2DPoint));
        return 0;
    case EE_SCREENS_SEUNX_WALL_TECH: return (int32_t)SeunTab(p[1])->mBuildWall.mTechnology;
    case EE_SCREENS_SEUNX_WALL_PLAYER: return (int32_t)SeunTab(p[1])->mBuildWall.mPlayer;
    case EE_SCREENS_SEUNX_WALL_STATE: return (int32_t)SeunTab(p[1])->mBuildWall.mState;
    case EE_SCREENS_SEUNX_GATE_TILE_X: return (int32_t)SeunTab(p[1])->mBuildGate.mTileX;
    case EE_SCREENS_SEUNX_GATE_TILE_Y: return (int32_t)SeunTab(p[1])->mBuildGate.mTileY;
    case EE_SCREENS_SEUNX_GATE_TECH: return (int32_t)SeunTab(p[1])->mBuildGate.mTechnology;
    case EE_SCREENS_SEUNX_GATE_DIRECTION: return (int32_t)SeunTab(p[1])->mBuildGate.mDirection;
    case EE_SCREENS_SEUNX_LOCK_ENTER:
        gWorld.mGhostUnitsLock->Enter();
        return 0;
    case EE_SCREENS_SEUNX_LOCK_LEAVE:
        gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEUNX_CLEAR_GHOSTS:
        gWorld.ClearGhostUnits();
        return 0;
    case EE_SCREENS_SEUNX_PREVIEW_PLACEMENT:
        EEPreviewPlacement(gMouse->mPosition);
        return 0;
    case EE_SCREENS_SEUNX_MOUSE_TILE:
        EEPointUnitOrTileXY(gMouse->mPosition, *static_cast<EETile**>(p[0]), *static_cast<long*>(p[1]),
                            *static_cast<long*>(p[2]), true);
        return 0;
    case EE_SCREENS_SEUNX_TILES_CLEAR:
        sSeunxTiles.clear();
        return 0;
    case EE_SCREENS_SEUNX_TILES_PUSH:
        sSeunxTiles.push_back(EETileCoordinate((long)i[0], (long)i[1]));
        return 0;
    case EE_SCREENS_SEUNX_PREVIEW_WALL:
        EEPreviewWall(sSeunxTiles, SeunTab(p[1])->mBuildWall.mTechnology, SeunTab(p[1])->mBuildWall.mPlayer);
        return 0;
    case EE_SCREENS_SEUNX_FIND_GATE_WALL:
        return EEFindGateWall(EETileCoordinate((long)i[0], (long)i[1]), *static_cast<long*>(p[2]));
    case EE_SCREENS_SEUNX_CREATE_GATE:
        EEGhostUnit::CreateGate(0.0f, static_cast<EEUnitClass*>(SeunTab(p[1])->mBuildGate.mUnitClass), gWorld.GetPlayer(SeunTab(p[1])->mBuildGate.mPlayer));
        return 0;
    case EE_SCREENS_SEUNX_PREVIEW_GATE:
        EEGhostUnit::PreviewGate(gMouse->mPosition, *static_cast<long*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_SET_DIRECTION:
        SeunTab(p[1])->mBuildGate.mDirection = (long)i[0];
        return 0;
    case EE_SCREENS_SEUNX_GAME_VIEW:
        EESetGameViewRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        return 0;
    case EE_SCREENS_SEUNX_CLEAR_B529066:
        gB529066 = false;
        return 0;
    case EE_SCREENS_SEUNX_CLEAR_FOLLOWING:
        gCameraFollowing = false;
        return 0;
    case EE_SCREENS_SEUNX_CLEAR_B529068:
        gB529068 = false;
        return 0;
    case EE_SCREENS_SEUNX_MINIMAP: return gMiniMap != NULL;
    case EE_SCREENS_SEUNX_MINIMAP_REFRESH:
        gMiniMap->RefreshView();
        return 0;
    case EE_SCREENS_SEUNX_REDRAW_ALL:
        gUIManager->RedrawAll();
        return 0;
    case EE_SCREENS_SEUNX_ALL_DIRTY:
        gUIManager->GetCurrentForm()->SetAllControlsDirty();
        return 0;
    case EE_SCREENS_SEUNX_PANEL_AND:   // the clipped panel into *p0
        return SeunTab(p[1])->mPanelRectangle.And(*static_cast<const GERectangle*>(p[2]), *static_cast<GERectangle*>(p[0]));
    case EE_SCREENS_SEUNX_DRAW_PANEL:   // p0 the clipped panel
        SeunTab(p[1])->mForm->DrawBackground(static_cast<GEBitmap*>(p[2]), *static_cast<GERectangle*>(p[0]),
                                             GERectangle(0.0f, 0.0f, 255.0f, 255.0f), SeunTab(p[1])->mPanelRectangle);
        return 0;
    case EE_SCREENS_SEUNX_ROTATION_ANGLE:
        return (int32_t)(uint32_t)EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_ROTATION_ANGLE);
    case EE_SCREENS_SEUNX_PANEL_LEFT:
        SeunTab(p[1])->mPanelRectangle.SetLeft(f[0]);
        return 0;
    case EE_SCREENS_SEUNX_PANEL_TOP:
        SeunTab(p[1])->mPanelRectangle.SetTop(f[0]);
        return 0;
    case EE_SCREENS_SEUNX_PANEL_RIGHT:
        SeunTab(p[1])->mPanelRectangle.SetRight(f[0]);
        return 0;
    case EE_SCREENS_SEUNX_PANEL_BOTTOM:
        SeunTab(p[1])->mPanelRectangle.SetBottom(f[0]);
        return 0;
    case EE_SCREENS_SEUNX_PANEL_DIRTY:
        static_cast<UIControl*>(p[2])->GetForm()->GetManager()->AddDirtyRectangle(SeunTab(p[1])->mPanelRectangle,
                                                                                 static_cast<UIControl*>(p[2])->GetForm());
        return 0;
    case EE_SCREENS_SEUNX_GATE_CANCEL:
        ee_screens_gate_cancel(&SeunTab(p[1])->mBuildGate);
        return 0;
    case EE_SCREENS_SEUNX_WALL_CANCEL:
        ee_screens_wall_cancel(&SeunTab(p[1])->mBuildWall);
        return 0;
    case EE_SCREENS_SEUNX_PLAYER:
        EESEOutSlot(p) = gWorld.GetPlayer(i[0]);
        return 0;
    case EE_SCREENS_SEUNX_TYPE_AT:
        return (int32_t)(long)EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_UL_TYPE_AT, (unsigned long)i[0]);
    case EE_SCREENS_SEUNX_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SEUNX_CLASS_TECH: return (int32_t)gUnitClassTable[i[0]]->mTechnology;
    case EE_SCREENS_SEUNX_TREE_NODE:
        EESEOutSlot(p) = static_cast<EEPlayer*>(p[1])->GetTechTree()->Get(i[0]);
        return 0;
    case EE_SCREENS_SEUNX_CLASS:
        EESEOutSlot(p) = gUnitClassTable[i[0]];
        return 0;
    case EE_SCREENS_SEUNX_PLAYER_ID: return (int32_t)static_cast<EEPlayer*>(p[1])->GetPlayerID();
    case EE_SCREENS_SEUNX_GAIA_PLACEABLE: return static_cast<EEUnitClass*>(p[1])->mIsGaiaPlaceable;
    case EE_SCREENS_SEUNX_PLAYER_PLACEABLE: return static_cast<EEUnitClass*>(p[1])->mIsPlayerPlaceable;
    case EE_SCREENS_SEUNX_CLASS_IS_GATE: return static_cast<EEUnitClass*>(p[1])->mCategory == kUnitCategoryGate;
    case EE_SCREENS_SEUNX_NODE_KIND: return (int32_t)static_cast<EETechTreeNode*>(p[1])->mData->mKind;
    case EE_SCREENS_SEUNX_NODE_ID: return (int32_t)static_cast<EETechTreeNode*>(p[1])->mData->mID;
    case EE_SCREENS_SEUNX_NODE_UNIT_CLASS: return (int32_t)static_cast<EETechTreeNode*>(p[1])->GetUnitClass(0);
    case EE_SCREENS_SEUNX_WALL_SET_TECH:
        SeunTab(p[1])->mBuildWall.mTechnology = i[0];
        return 0;
    case EE_SCREENS_SEUNX_WALL_INITIALIZE:
        ee_screens_wall_initialize(&SeunTab(p[1])->mBuildWall, (int32_t)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_GATE_SET_TECH:
        SeunTab(p[1])->mBuildGate.mTechnology = i[0];
        return 0;
    case EE_SCREENS_SEUNX_GATE_SET_WALL_TYPE:
        SeunTab(p[1])->mBuildGate.mWallType = i[0];
        return 0;
    case EE_SCREENS_SEUNX_GATE_SET_CLASS:
        SeunTab(p[1])->mBuildGate.mUnitClass = static_cast<EEUnitClass*>(p[2]);
        return 0;
    case EE_SCREENS_SEUNX_GATE_INITIALIZE:
        ee_screens_gate_initialize(&SeunTab(p[1])->mBuildGate, (int32_t)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_POST_402:
        SeunEditor(p[1])->GetLoadThread()->Post402(reinterpret_cast<void*>((long)i[0]), reinterpret_cast<void*>((long)i[1]));
        return 0;
    case EE_SCREENS_SEUNX_SELECTED_COMPLEX:
        EESEOutSlot(p) = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit;
        return 0;
    case EE_SCREENS_SEUNX_CARGO_UNITS: return (int32_t)static_cast<EEComplexUnit*>(p[1])->GetNumCargoUnits();
    case EE_SCREENS_SEUNX_L_DOUBLE: return UIManager::GetLMouseButton()->JustDoublePressed();
    case EE_SCREENS_SEUNX_L_PRESSED: return UIManager::GetLMouseButton()->JustPressed();
    case EE_SCREENS_SEUNX_L_RELEASED: return UIManager::GetLMouseButton()->JustUnpressed();
    case EE_SCREENS_SEUNX_L_DOWN: return UIManager::GetLMouseButton()->GetValue() != 0;
    case EE_SCREENS_SEUNX_R_PRESSED: return UIManager::GetRMouseButton()->JustPressed();
    case EE_SCREENS_SEUNX_R_RELEASED: return UIManager::GetRMouseButton()->JustUnpressed();
    case EE_SCREENS_SEUNX_PICK_UNIT:
        EESEOutSlot(p) = gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), 0);
        return 0;
    case EE_SCREENS_SEUNX_UNIT_ID: return (int32_t)static_cast<EEUnit*>(p[1])->mUnitID;
    case EE_SCREENS_SEUNX_UNIT_IS_GATE: return static_cast<EEUnit*>(p[1])->mUnitClass->mCategory == kUnitCategoryGate;
    case EE_SCREENS_SEUNX_UNIT_DEREFERENCE:
        static_cast<EEUnit*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SEUNX_GET_UNIT:
        EESEOutSlot(p) = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)(uint32_t)i[0]));
        return 0;
    case EE_SCREENS_SEUNX_GATE_TECH_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mTechnology = static_cast<EEUnit*>(p[2])->mUnitClass->mTechnology;
        return 0;
    case EE_SCREENS_SEUNX_GATE_WALL_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mWallType = static_cast<EEUnit*>(p[2])->GetTechTreeNode()->GetUnitClass(0);
        return 0;
    case EE_SCREENS_SEUNX_GATE_CLASS_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mUnitClass = static_cast<EEUnit*>(p[2])->mUnitClass;
        return 0;
    case EE_SCREENS_SEUNX_UNIT_KILL:
        static_cast<EEUnit*>(p[1])->Kill(0, 0, 0);
        return 0;
    case EE_SCREENS_SEUNX_GATE_INITIALIZE_OF_UNIT:
        ee_screens_gate_initialize(&SeunTab(p[1])->mBuildGate, (int32_t)static_cast<EEUnit*>(p[2])->mPlayer->mPlayerID);
        return 0;
    case EE_SCREENS_SEUNX_GATE_UPDATE:
        EEUIBuildGateUpdate(&SeunTab(p[1])->mBuildGate, GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUNX_WALL_UPDATE:
        EEUIBuildWallUpdate(&SeunTab(p[1])->mBuildWall, GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUNX_KEY_DELETE:
        gISKeyboard->GetEntity(kKeyDelete, *static_cast<ISEntity**>(p[0]));
        return 0;
    case EE_SCREENS_SEUNX_KEY_SHIFT:
        gISKeyboard->GetEntity(kKeyShift, *static_cast<ISEntity**>(p[0]));
        return 0;
    case EE_SCREENS_SEUNX_KEY_PRESSED: return static_cast<ISEntity*>(p[1])->JustPressed();
    case EE_SCREENS_SEUNX_KEY_DOWN: return static_cast<ISEntity*>(p[1])->GetValue() != 0;
    case EE_SCREENS_SEUNX_CURRENT_PLAYER: return (int32_t)gWorld.GetCurrentPlayer()->GetPlayerID();
    case EE_SCREENS_SEUNX_DELETE_ACTION:   // i: the shift flag, the player
        EESEOutSlot(p) = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, -1, i[1], i[0] != 0));
        return 0;
    }
    return 0;
}

// the editor's gate and wall placement modes (once EESETSUICBPlayer.cpp): the unit tab places or ends them
// (their Cancel and Build: the placements' records' own, mEditor; placement_ffi.rs calls these)
extern "C" void ee_screenscpp_se_gate_build() { gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_GATE); }
extern "C" void ee_screenscpp_se_gate_cancel(void* inGate)
{ gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_CANCEL_GATE, 0, inGate); }   // (editor_unit.rs cancel_gate)
extern "C" void ee_screenscpp_se_wall_build() { gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_WALL); }
extern "C" void ee_screenscpp_se_wall_cancel(void* inWall)
{ gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_CANCEL_WALL, 0, inWall); }   // (editor_unit.rs cancel_wall)
