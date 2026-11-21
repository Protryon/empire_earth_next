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
#include "ee/EESETSConditionControls.h"
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
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the ghost at the mouse)
extern unsigned long& gTexturePixelDepth;

// The upgrade panel is a real 0x60-byte class (new at 0x59b976 in the tab's ctor); both its ctor counters
// are mutable
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFSEUpgradePanel.h"
#include "ee/EEStringIDs.h"






// The sort radio buttons' event (RTTI EEUIFSEESortUnitList > UIEvent; vtable 0x444e88; new 0xc): an EESEERust
// (ee_screens editor_events.rs)

extern GEFont* gSeplScreenFont;   // the screen's font while the tab makes its controls (EESERBUnitTypes.cpp)

// (the unit tab's controls are shell kinds: ee_game shells_unit.rs)

// (the unit type buttons' cell width, the .rdata float 0x444e7c: editor_seunx.rs's W)

// The shared control maker's unit tab lists (ee_game editor_sepl.rs make)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
{
    std::vector<UWideString> theNone;
    if (inKind == EE_SCREENS_SEPL_LIST_UNIT_TYPES)
        return new EEUIShellList(EE_SCREENS_SK_UNIT_CLASS_LIST, {}, inID, theNone, inFont, false,
                                 GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 3);
    return new EEUIShellList(EE_SCREENS_SK_CARGO_LIST, {}, inID, theNone, inFont, false,
                             GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 3);
}


// ---- the unit tab's host (editor_unit.rs; EE_SCREENS_SEUN_OP_*): the tab's own controls, the actions, the world,
// the mouse, the build tools and the unit classes

static_assert(EE_SCREENS_SEUN_IDS_ALPHA == IDS_ALPHA && EE_SCREENS_SEUN_IDS_ALPHA + 1 == IDS_SE_EPOCH &&
                  EE_SCREENS_SEUN_IDS_ALPHA + 2 == IDS_FAMILY && EE_SCREENS_SEUN_IDS_CARGO == IDS_CARGO &&
                  EE_SCREENS_SEUN_IDS_SORT == IDS_SORT && EE_SCREENS_SEUN_IDS_WORLD_PLAYER == IDS_WORLD_PLAYER,
              "the strings");
static_assert(EE_SCREENS_SEUN_PALETTE_LAND == EE_SCREENS_SE_UNIT_PALETTE_LAND_UNITS && EE_SCREENS_SEUN_PALETTE_AIR == EE_SCREENS_SE_UNIT_PALETTE_AIR_UNITS &&
                  EE_SCREENS_SEUN_PALETTE_NAVAL == EE_SCREENS_SE_UNIT_PALETTE_NAVAL_UNITS &&
                  EE_SCREENS_SEUN_PALETTE_STRUCTURES == EE_SCREENS_SE_UNIT_PALETTE_STRUCTURES &&
                  EE_SCREENS_SEUN_PALETTE_HEROES == EE_SCREENS_SE_UNIT_PALETTE_HEROES &&
                  EE_SCREENS_SEUN_PALETTE_RESOURCES == EE_SCREENS_SE_UNIT_PALETTE_RESOURCES,
              "the palettes");

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

// the statements' locals (editor_seunx.rs): a string, the wall's tiles, the panel's clipped rectangle
UWideString sSeunxText;
std::vector<EETileCoordinate> sSeunxTiles;
GERectangle sSeunxRect;

}   // namespace

// The ops (editor_unit.rs; p: three handles, i: four integers, f: four floats). The class table's count and
// placement flags are Rust's (ee_game editor_world.rs); the statement sequences are composed in Rust
// (editor_seunx.rs) of ee_screenscpp_seunx's statements.
extern "C" int32_t ee_screenscpp_seun(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESEUnit* theTab = static_cast<EESEUnit*>(p[0]);
    UILBString* theList = static_cast<UILBString*>(p[0]);
    UIControl* theShell = static_cast<UIControl*>(p[0]);
    void** theOut = static_cast<void**>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEUN_OP_SORT_EVENT: *theOut = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_SORT_UNIT_LIST)); return 0;
    case EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS:
        EEClearGateHighlights(gWorld.GetPlayer(theTab->mBuildGate.mPlayer), theTab->mBuildGate.mWallType);
        return 0;
    case EE_SCREENS_SEUN_OP_REDRAW: static_cast<UIControl*>(p[0])->GetForm()->GetManager()->RedrawAll(); return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_SELECTION: SeunEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEUN_OP_SHOW_UNIT:
        static_cast<EEUIFSEUpgradePanel*>(p[0])->ShowUnit(static_cast<EEUnit*>(p[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_TYPE_MODE: return ee_screens_shell_unit_type_mode();
    case EE_SCREENS_SEUN_OP_LIST_TOP: theList->SetScrollBarCurrentRange(0); return 0;
    case EE_SCREENS_SEUN_OP_CARGO_CLEAR:
        EEUIShell::Op(theShell, EE_SCREENS_SKOP_CARGO_CLEAR);
        return 0;
    case EE_SCREENS_SEUN_OP_SELECTED_COUNT: return (int32_t)SeunEditor(p[1])->GetSelectedUnitCount();
    case EE_SCREENS_SEUN_OP_CARGO_ADD:
    {
        EEUnit* theUnit = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit->mCargo[(unsigned long)i[1]];
        EEUIShell::Op(theShell, EE_SCREENS_SKOP_CARGO_ADD, (int64_t)theUnit);
    }
        return 0;
    case EE_SCREENS_SEUN_OP_LIST_SORT: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_SORT, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_LIST_EMPTY: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_CLEAR); return 0;
    case EE_SCREENS_SEUN_OP_LIST_ADD: EEUIShell::Op(theShell, EE_SCREENS_SKOP_UL_ADD, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_IN_VIEW: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1])) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_START_SELECTION:
        static_cast<EEUICSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_COMMAND:
        static_cast<EESEUICCommandUnits*>(p[0])->StartCommand(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_ROTATION_KEYS: static_cast<UIControl*>(p[0])->UpdateKeyboard(); return 0;
    }
    return 0;
}

// the unit tab's single statements (editor_seunx.rs EE_SCREENS_SEUNX_*: p0 the made object's out slot, else as
// named; the Rust composes the host's sequences)
extern "C" int32_t ee_screenscpp_seunx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UIRadioButton* theRadio = static_cast<UIRadioButton*>(p[1]);
    UIButton* theButton = static_cast<UIButton*>(p[1]);
    GETexture* theTexture = static_cast<GETexture*>(p[1]);
    EEPlayer* thePlayer = static_cast<EEPlayer*>(p[1]);
    EEUnitClass* theClass = static_cast<EEUnitClass*>(p[1]);
    EETechTreeNode* theNode = static_cast<EETechTreeNode*>(p[1]);
    EEUnit* theUnit = static_cast<EEUnit*>(p[1]);
    EEUnit* theOperand = static_cast<EEUnit*>(p[2]);
    switch (inOp) {
    case EE_SCREENS_SEUNX_NEW_BITMAP:
        *theOut = new GEBitmap;
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXTURE:
        *theOut = static_cast<EEUIFScenarioEditor*>(theForm)->GetButtonTexture();
        return 0;
    case EE_SCREENS_SEUNX_TEXTURE_WIDTH: return (int32_t)theTexture->GetWidth();
    case EE_SCREENS_SEUNX_BITMAP_TEXTURE_FORM:
        static_cast<GEBitmap*>(p[0])->SetTexture(theTexture, static_cast<UIForm*>(p[2])->GetManager()->GetScreen());
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_COORDS:
        static_cast<GEBitmap*>(p[1])->SetTextureCoordinates(GETextureCoordinate(f[0], f[1]), GETextureCoordinate(f[2], f[1]),
                                                            GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[2], f[3]));
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_SCALE:
        static_cast<GEBitmap*>(p[1])->SetXYScale(f[0], f[1]);
        return 0;
    case EE_SCREENS_SEUNX_TYPE_RADIO:
        *theOut = static_cast<UIRadioButton*>(new EEUIShellRadio(
            EE_SCREENS_SK_UNIT_TYPE_BUTTON, {}, (long)i[1] + 0x406, UWideString(""), gU531930,
            GERectangle(GE2DPoint((float)(unsigned long)i[3], 0.0f), 35.0f, 25.0f), theForm, (unsigned long)i[0], 0, true,
            i[1] == 0, false));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_UP_CHECKED:
        theRadio->SetUpCheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DOWN_CHECKED:
        theRadio->SetDownCheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DOWN_UNCHECKED:
        theRadio->SetDownUncheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_UP_UNCHECKED:
        theRadio->SetUpUncheckedBitmap(static_cast<GEBitmap*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_NO_FOCUS:
        theRadio->SetFocusedBitmap(NULL);
        return 0;
    case EE_SCREENS_SEUNX_RADIO_DRAW_OFFSET:
        theRadio->SetBitmapDrawOffset(GE2DPoint(0.0f, 0.0f));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_TRIGGER:
        theRadio->SetTriggerEvent(kControlTriggerButtonUp, static_cast<UIEvent*>(p[2]));
        return 0;
    case EE_SCREENS_SEUNX_RADIO_STRING_ID:
        theRadio->SetStringTableID((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_ADD_RADIO:
        theForm->AddControl(static_cast<UIRadioButton*>(p[2]), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_STRING_REMOVE:
        gResource->GetString(IDS_SE_REMOVE, sSeunxText);
        return 0;
    case EE_SCREENS_SEUNX_REMOVE_CARGO_BUTTON:
        *theOut = static_cast<UIButton*>(new EEUIShellButton(EE_SCREENS_SK_REMOVE_CARGO, {}, 0x3f4, NULL, NULL, NULL,
                                                             GERectangle(GE2DPoint(f[0], 707.0f), 100.0f, 25.0f), theForm,
                                                             (unsigned long)i[0], 2));
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_SET_TEXT:
        theButton->SetText(sSeunxText, gSeplScreenFont);
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_OPAQUE:
        theButton->SetBgOpacity(1.0f);
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXT_WHITE:
        theButton->GetGEText()->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
        return 0;
    case EE_SCREENS_SEUNX_BUTTON_TEXT_TRANSPARENT:
        theButton->GetGEText()->SetIsTransparent(true);
        return 0;
    case EE_SCREENS_SEUNX_ADD_BUTTON:
        theForm->AddControl(static_cast<UIButton*>(p[2]), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEUNX_ROTATION_BUTTON:
        *theOut = static_cast<UIButton*>(new EEUIShellButton(EE_SCREENS_SK_ROTATION, {}, 0x3f5, NULL, NULL, NULL,
                                                             GERectangle(GE2DPoint(500.0f, 633.0f), 50.0f, 50.0f), theForm,
                                                             (unsigned long)i[0], 0));
        return 0;
    case EE_SCREENS_SEUNX_ROTATION_STRING:
        theButton->SetStringTableID(IDS_ROTATE);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_CLICK:
        *theOut = static_cast<EEAction*>(new EESEAMouseClick);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_LASSO:
        *theOut = static_cast<EEAction*>(new EESEALasso);
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_UNIT:
        *theOut = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_UNIT));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_GATE:
        *theOut = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_GATE));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_PLACE_WALL:
        *theOut = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_PLACE_WALL));
        return 0;
    case EE_SCREENS_SEUNX_ACTION_MOVE_UNIT:
        *theOut = static_cast<EEAction*>(new EESEARust(EE_SCREENS_SEW_MOVE_UNIT));
        return 0;
    case EE_SCREENS_SEUNX_SELECT_UNITS:
        *theOut = new EEUICSelectUnits(
            theForm, 0x3f6, (unsigned long)i[0],
            static_cast<EESEAMouseClick*>(static_cast<EEAction*>(static_cast<void* const*>(p[2])[0])),
            static_cast<EESEALasso*>(static_cast<EEAction*>(static_cast<void* const*>(p[2])[1])));
        return 0;
    case EE_SCREENS_SEUNX_ADD_SELECT_UNITS:
        theForm->AddControl(static_cast<EEUICSelectUnits*>(p[2]), 0);
        return 0;
    case EE_SCREENS_SEUNX_COMMAND_UNITS:
        *theOut = new EESEUICCommandUnits(theForm, 0x3f7, (unsigned long)i[0],
                                          static_cast<EESEAMouseClick*>(static_cast<EEAction*>(p[2])));
        return 0;
    case EE_SCREENS_SEUNX_ADD_COMMAND_UNITS:
        theForm->AddControl(static_cast<EESEUICCommandUnits*>(p[2]), 0);
        return 0;
    case EE_SCREENS_SEUNX_LOAD_SCENUNIT:
        *theOut = gTextureManager->LoadTexture("textures\\scenunit",
                                               GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, false});
        return 0;
    case EE_SCREENS_SEUNX_TEX_FRAME:
        theTexture->SetFrame(0);
        return 0;
    case EE_SCREENS_SEUNX_TEX_ALPHA:
        theTexture->SetStageAlphaMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_COLOR:
        theTexture->SetStageColorMode(kStageOpTexture, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MIN:
        theTexture->SetStageMinFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MAG:
        theTexture->SetStageMagFilterMode(0, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_MIP:
        theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_BIAS:
        theTexture->SetStageMipMapBias(-1.0f, -1);
        return 0;
    case EE_SCREENS_SEUNX_TEX_WRAP:
        theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
        return 0;
    case EE_SCREENS_SEUNX_BITMAP_TEXTURE_SCREEN:
        static_cast<GEBitmap*>(p[1])->SetTexture(static_cast<GETexture*>(p[2]), gScreenIndex);
        return 0;
    case EE_SCREENS_SEUNX_UPGRADE_PANEL:
        *theOut = new EEUIFSEUpgradePanel(static_cast<unsigned long*>(p[2])[0], static_cast<unsigned long*>(p[2])[1],
                                          static_cast<EEUIFScenarioEditor*>(theForm));
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
        delete static_cast<EEUIFSEUpgradePanel*>(p[1]);
        return 0;
    case EE_SCREENS_SEUNX_SEW_ACTION:
        *theOut = SeunAction(p[1])->mAction;
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
        EEGhostUnit::CreateGate(0.0f, SeunTab(p[1])->mBuildGate.mUnitClass, gWorld.GetPlayer(SeunTab(p[1])->mBuildGate.mPlayer));
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
    case EE_SCREENS_SEUNX_PANEL_AND:
        return SeunTab(p[1])->mPanelRectangle.And(*static_cast<const GERectangle*>(p[2]), sSeunxRect);
    case EE_SCREENS_SEUNX_DRAW_PANEL:
        SeunTab(p[1])->mForm->DrawBackground(static_cast<GEBitmap*>(p[2]), sSeunxRect, GERectangle(0.0f, 0.0f, 255.0f, 255.0f),
                                             SeunTab(p[1])->mPanelRectangle);
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
        SeunTab(p[1])->mBuildGate.Cancel();
        return 0;
    case EE_SCREENS_SEUNX_WALL_CANCEL:
        SeunTab(p[1])->mBuildWall.Cancel();
        return 0;
    case EE_SCREENS_SEUNX_PLAYER:
        *theOut = gWorld.GetPlayer(i[0]);
        return 0;
    case EE_SCREENS_SEUNX_TYPE_AT:
        return (int32_t)(long)EEUIShell::Op(static_cast<UIControl*>(p[1]), EE_SCREENS_SKOP_UL_TYPE_AT, (unsigned long)i[0]);
    case EE_SCREENS_SEUNX_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SEUNX_CLASS_TECH: return (int32_t)gUnitClassTable[i[0]]->mTechnology;
    case EE_SCREENS_SEUNX_TREE_NODE:
        *theOut = thePlayer->GetTechTree()->Get(i[0]);
        return 0;
    case EE_SCREENS_SEUNX_CLASS:
        *theOut = gUnitClassTable[i[0]];
        return 0;
    case EE_SCREENS_SEUNX_PLAYER_ID: return (int32_t)thePlayer->GetPlayerID();
    case EE_SCREENS_SEUNX_GAIA_PLACEABLE: return theClass->mIsGaiaPlaceable;
    case EE_SCREENS_SEUNX_PLAYER_PLACEABLE: return theClass->mIsPlayerPlaceable;
    case EE_SCREENS_SEUNX_CLASS_IS_GATE: return theClass->mCategory == kUnitCategoryGate;
    case EE_SCREENS_SEUNX_NODE_KIND: return (int32_t)theNode->mData->mKind;
    case EE_SCREENS_SEUNX_NODE_ID: return (int32_t)theNode->mData->mID;
    case EE_SCREENS_SEUNX_NODE_UNIT_CLASS: return (int32_t)theNode->GetUnitClass(0);
    case EE_SCREENS_SEUNX_WALL_SET_TECH:
        SeunTab(p[1])->mBuildWall.mTechnology = i[0];
        return 0;
    case EE_SCREENS_SEUNX_WALL_INITIALIZE:
        SeunTab(p[1])->mBuildWall.Initialize(i[0]);
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
        SeunTab(p[1])->mBuildGate.Initialize(i[0]);
        return 0;
    case EE_SCREENS_SEUNX_POST_402:
        SeunEditor(p[1])->GetLoadThread()->Post402(reinterpret_cast<void*>((long)i[0]), reinterpret_cast<void*>((long)i[1]));
        return 0;
    case EE_SCREENS_SEUNX_SELECTED_COMPLEX:
        *theOut = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit;
        return 0;
    case EE_SCREENS_SEUNX_CARGO_UNITS: return (int32_t)static_cast<EEComplexUnit*>(p[1])->GetNumCargoUnits();
    case EE_SCREENS_SEUNX_L_DOUBLE: return UIManager::GetLMouseButton()->JustDoublePressed();
    case EE_SCREENS_SEUNX_L_PRESSED: return UIManager::GetLMouseButton()->JustPressed();
    case EE_SCREENS_SEUNX_L_RELEASED: return UIManager::GetLMouseButton()->JustUnpressed();
    case EE_SCREENS_SEUNX_L_DOWN: return UIManager::GetLMouseButton()->GetValue() != 0;
    case EE_SCREENS_SEUNX_R_PRESSED: return UIManager::GetRMouseButton()->JustPressed();
    case EE_SCREENS_SEUNX_R_RELEASED: return UIManager::GetRMouseButton()->JustUnpressed();
    case EE_SCREENS_SEUNX_PICK_UNIT:
        *theOut = gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), 0);
        return 0;
    case EE_SCREENS_SEUNX_UNIT_ID: return (int32_t)theUnit->mUnitID;
    case EE_SCREENS_SEUNX_UNIT_IS_GATE: return theUnit->mUnitClass->mCategory == kUnitCategoryGate;
    case EE_SCREENS_SEUNX_UNIT_DEREFERENCE:
        theUnit->Dereference();
        return 0;
    case EE_SCREENS_SEUNX_GET_UNIT:
        *theOut = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)(uint32_t)i[0]));
        return 0;
    case EE_SCREENS_SEUNX_GATE_TECH_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mTechnology = theOperand->mUnitClass->mTechnology;
        return 0;
    case EE_SCREENS_SEUNX_GATE_WALL_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mWallType = theOperand->GetTechTreeNode()->GetUnitClass(0);
        return 0;
    case EE_SCREENS_SEUNX_GATE_CLASS_OF_UNIT:
        SeunTab(p[1])->mBuildGate.mUnitClass = theOperand->mUnitClass;
        return 0;
    case EE_SCREENS_SEUNX_UNIT_KILL:
        theUnit->Kill(0, 0, 0);
        return 0;
    case EE_SCREENS_SEUNX_GATE_INITIALIZE_OF_UNIT:
        SeunTab(p[1])->mBuildGate.Initialize(theOperand->mPlayer->mPlayerID);
        return 0;
    case EE_SCREENS_SEUNX_GATE_UPDATE:
        SeunTab(p[1])->mBuildGate.Update(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUNX_WALL_UPDATE:
        SeunTab(p[1])->mBuildWall.Update(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUNX_KEY_DELETE:
        gISKeyboard->GetEntity(kKeyDelete, *reinterpret_cast<ISEntity**>(theOut));
        return 0;
    case EE_SCREENS_SEUNX_KEY_SHIFT:
        gISKeyboard->GetEntity(kKeyShift, *reinterpret_cast<ISEntity**>(theOut));
        return 0;
    case EE_SCREENS_SEUNX_KEY_PRESSED: return static_cast<ISEntity*>(p[1])->JustPressed();
    case EE_SCREENS_SEUNX_KEY_DOWN: return static_cast<ISEntity*>(p[1])->GetValue() != 0;
    case EE_SCREENS_SEUNX_DELETE_ACTION:
        *theOut = static_cast<EEAction*>(
            new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, -1, gWorld.GetCurrentPlayer()->GetPlayerID(), i[0] != 0));
        return 0;
    }
    return 0;
}

// the editor's gate and wall placement modes (once EESETSUICBPlayer.cpp): the unit tab places or ends them
void EESEUIBuildGate::Build(const GE2DPoint& inLocation, long inDirection)
{
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_GATE);
}

void EESEUIBuildGate::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_END_GATE);
    thePlayer->UnloadUnitClassMedia(this->mUnitClass->mUnitType);
}

void EESEUIBuildWall::Build()
{
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_PLACE_WALL);
}

void EESEUIBuildWall::Cancel()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
    gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_END_WALL);
    long theTechnology = this->mTechnology;
    thePlayer->UnloadUnitClassMedia(thePlayer->mTechnologies->GetData(theTechnology)->mUnitClass);
}
