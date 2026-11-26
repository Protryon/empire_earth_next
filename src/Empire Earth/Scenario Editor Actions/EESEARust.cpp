// C:\EEX\Empire Earth\Scenario Editor Actions\EESEARust.cpp (port addition, no original address). The host of the
// scenario editor's actions whole in Rust (ee_screens editor_actions.rs, editor_whole.rs, editor_place.rs): the paint
// actions' world edits (ee_screenscpp_sea), the actions' world, screens and files (ee_screenscpp_sew), the
// placements' (ee_screenscpp_sep) and the selection's (ee_screenscpp_seu; their sequences composed in editor_seax.rs
// over ee_screenscpp_seax's statements: the
// click, the lasso, the rotation, the deletion; the original's EESEADeleteUnit.cpp 0x1a6b45..0x1a7331, its class an
// EESEARust kind, ee/EESEARust.h; EESEALasso.cpp 0x1a7358..0x1a747d and EESEAMouseClick.cpp 0x1a747d..0x1a75e0, their
// classes' bodies inline over Rust). The paint actions themselves are
// EESEAPaintRust (ee/EESEAPaint.h): the original's EESEAPaintElevation.cpp 0x1a830a..0x1a86ff, EESEAPaintMoutains.cpp
// 0x1a86ff..0x1a8d3e and EESEAPaintTerrain.cpp 0x1a8d3e..0x1a9112 (their deque and std::sort COMDATs with them).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEMouse.h"
#include "ee/EEMessage.h"
#include "ee/EEUnitPicker.h"
#include "ee/EESEARust.h"
#include "ee/EESERBUnitTypes.h"   // gMaxPlayers
#include "ee/EEUICCommandUnits.h"
#include "ee/EESEUICCommandUnits.h"
#include "ee/EEUIShell.h"
#include "rust/game/gamecpp.h"
#include "rust/game/rts.h"
#include "rust/game/screens.h"
#include "rust/sim/app.h"
#include "rust/sim/units.h"

// ------------------------------------------------------------------------------------------------ the classes' parts
// The editor kinds' C++ (ee/EEARust.h): the load text's post from the load thread (the trigger tab's object
// screen refresh after a click's or lasso's selection is EEARust.cpp's host op EE_SCREENS_GA_E_OBJECT_SELECTION)

void EESEAChangeLoadText::Post(const UWideString& inText)
{
    this->mText = inText;
    gUserInterface->PostAction(this);
}

// The editor's right-drag control (EESEUICCommandUnits.cpp 0x197261..0x197393, its overrides ee_screens
// command_units.rs's editor path): its rotation action, and the rotation's and the deletion's ops
EESEUICCommandUnits::EESEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                         EEAMouseClick* inListener)
    : EEUICCommandUnits(inForm, inId, inIndex, inListener)
{
    this->mEditor = true;
    this->mRotateAction = new EESEARust(EE_SCREENS_SEW_ROTATE);
}

int64_t EESEUICCommandUnits::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    switch (inOp) {
    case EE_SCREENS_CU_ROTATE: ee_screens_sew_rotate(this->mRotateAction->mAction, ioF[0]); return 0;
    case EE_SCREENS_CU_PICK: return (int64_t)gUnitPicker.PickUnit(gMouse->mPosition, 0);
    case EE_SCREENS_CU_DELETE: (new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, reinterpret_cast<EEUnit*>(inA)->mUnitID))->Dereference(); return 0;
    case EE_SCREENS_CU_UNIT_RELEASE: reinterpret_cast<EEUnit*>(inA)->Dereference(); return 0;
    }
    return EEUICCommandUnits::RustOp(inOp, inA, inB, ioF);
}

// ------------------------------------------------------------------------------------------------ the host
// The paint actions' world-edit host (ee_screens editor_actions.rs: EE_SCREENS_SEA_*): the terrain, elevation and
// bulldozer paints call it from their runs, under the UI copy lock and the world's write lock (the action queue's).
// Its sequences (the follow stop, the point, the terrain, the plant, the elevation mode, the raise, the tile's units,
// the bulldozer's removal) are composed in ee_screens editor_seax.rs over ee_screenscpp_seax's statements.

// ------------------------------------------------------------------------------------------------ the whole actions
// The host of the editor's actions whole in Rust (ee_screens editor_whole.rs: EE_SCREENS_SEW_OP_*; ee/EESEARust.h):
// the original EESEASaveScenario, EESEATestScenario, EESEAExitScenario, EESEAQuickSaveScenario, EESEATechTree,
// EESEARotateUnits, EESEAPaintArea, EESEACreateBlankMap, EESEACreateRandomMap, EESEAPlaceWall and EESEAPlaceGate.
// Its sequences (and the elevation file pair's read) are composed in editor_seax.rs.

extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)

// ------------------------------------------------------------------------------------------------ placement and move
// The host of the unit placement and move (ee_screens editor_place.rs: EE_SCREENS_SEP_*; the original EESEAPlaceUnit
// and EESEAMoveUnit, EESEARust kinds): units by id (the original's handles), referenced while the actions hold them;
// t a GETransformation (sixteen floats). Its sequences are composed in editor_seax.rs.

static EEUnit* EESEPUnit(int32_t inID) { return EEUnitHandle<EEUnit>::FromSavedID((uint32_t)inID).Get(); }

static EEComplexUnit* EESEPComplexUnit(int32_t inID) { return EEUnitHandle<EEComplexUnit>::FromSavedID((uint32_t)inID).Get(); }

// ------------------------------------------------------------------------------------------------ the host
// The selection's actions' host (ee_screens editor_actions.rs, EE_SCREENS_SEU_*: the click, the lasso, the rotation
// and the deletion (an EESEARust kind) call it under the UI copy lock and the world's write lock). A unit is an EEUnit*.
// Its sequences (the rotation, the removal, the air traffic, the part's kill, the unload, the lasso's pick) are
// composed in editor_seax.rs.


static GERectangle sSeuRectangle;   // the box selection's rectangle (this thread's copy)

// ------------------------------------------------------------------------------------------------ the statements
// The single statements editor_seax.rs composes the hosts' sequences of (EE_SCREENS_SEAX_*, 3000..): p and q objects,
// t a GETransformation, i up to eight integers, f up to four floats (in and out). A statement's locals that the next
// one reads are the statics below (the original's locals of one case).

static long sSeaxLong[4];                       // Raise's rectangle, GetMapDimensions', GetClosestTile's outputs
static std::vector<EEPlayer*> sSeaxPlayers;     // the random map's players
static GE3DPoint sSeaxPoint;                    // the random map's start location, the camera object's point
static std::vector<EEUnit*> sSeaxWalls;         // PlanWall's walls (the original's local, dropped)
static EETileCoordinate sSeaxTile;              // EEBuildGate's tile
static unsigned long sSeaxSelection;            // the elevation list's selection
static unsigned long sSeaxRead;                 // a file read's count
static GETransformation sSeaxT;                 // the rotation's transformation
static std::vector<unsigned long> sSeaxIDs;     // the lasso's picked ids


extern "C" intptr_t ee_screenscpp_seax(uint32_t inOp, void* p, void* q, float* t, int32_t* i, float* f)
{
    switch (inOp) {
    // the merged hosts' ops (EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_*, ...: editor_seax_ffi.rs host): the paint actions'
    // world edits (sea: i, f), the whole actions' (sew: p, i, f; a mesh coordinate i0, i1 as the original's unsigned
    // long, x - 1 at 0 wraps), the placements' (sep: p, t a GETransformation, i, f), the selection's (seu: p a unit,
    // a message or an action, q its integer or message)
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_TILE: return gWorld.mMap->GetTile(i[0], i[1]) != NULL;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_PAINT_RECT: ee_gamecpp_mx_paint_rect(gWorld.mMap, i[0], i[1], i[2], i[3], i[4], i[5]); return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_UPDATE_CLIFFS: ee_gamecpp_mx_update_cliffs(gWorld.mMap, i[0], i[1], i[2], i[3]); return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_MINIMAP_RECT: gMiniMap->RedrawRect(i[0], i[1], i[2], i[3]); return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_MINIMAP_REFRESH: gMiniMap->Refresh(); return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_TERRAIN_CLASS: return (int32_t)EETerrainTable::Get(i[0])->GetUnitClass((unsigned long)(long)i[1]);
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_TIME: return (int32_t)TMGetTime();
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_REMOVE_FLAT_CLIFFS: ee_gamecpp_mx_remove_flat_cliffs(gWorld.mMap, (long)i[0], (long)i[1], (long)i[2], (long)i[3]); return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_STORE_MOUSE_TILE:
        EEUIShell::Op(gUIManager->GetScenarioEditorForm()->GetControlById(EE_SCREENS_SE_ELEVATION_PAINT), EE_SCREENS_SKOP_PAINT_STORE_TILE);
        return 0;
    case EE_SCREENS_SEAX_SEA + EE_SCREENS_SEA_ELEVATION_CHANGED: gWorld.mEffectManager->ElevationChanged(i[0], i[1], i[2], i[3]); return 0;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_POST: gUserInterface->PostAction(static_cast<EEAction*>(p)); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_REFERENCE: static_cast<EEAction*>(p)->Reference(); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_DEREFERENCE: static_cast<EEAction*>(p)->Dereference(); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_EXIT_EDITOR: gUIManager->GetScenarioEditorForm()->ExitEditor(i[0] != 0); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_REFRESH_GROUP: gUIManager->GetScenarioEditorForm()->RefreshCurrentGroup(); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_IN_EDITOR: return gUIManager->mInScenarioEditor == true;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_PREPARE_SAVE: gWorld.mMap->PrepareSave(); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_ROTATE: ee_screens_seu_rotate(f[0]); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_PLAYER_COUNT: return (int32_t)gWorld.GetNumPlayers();
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_MAP_SIZE: return (int32_t)gWorld.mMap->mSize;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_POINT: return gWorld.mMap->mTerrainMesh.GetIsValidPoint((unsigned long)(long)i[0], (unsigned long)(long)i[1]);
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_TILE: return gWorld.mMap->mTerrainMesh.GetIsValidTile((unsigned long)(long)i[0], (unsigned long)(long)i[1]);
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_GET_Z: f[0] = gWorld.mMap->mTerrainMesh.GetZ((unsigned long)(long)i[0], (unsigned long)(long)i[1]); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_SET_Z: gWorld.mMap->mTerrainMesh.SetZ((unsigned long)(long)i[0], (unsigned long)(long)i[1], f[0]); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_NORMAL:
        gWorld.mMap->mTerrainMesh.ComputeVertexNormal((unsigned long)(long)i[0], (unsigned long)(long)i[1]);
        break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_SHORELINES:
        gWorld.mMap->mTerrainMesh.ComputeShorelines((unsigned long)(long)i[0], (unsigned long)(long)i[1]);
        break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_PAINT_TERRAIN: ee_gamecpp_mx_paint(gWorld.mMap, i[0], i[1], i[2]); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_WATER_COLOR:
        gWorld.mMap->mTerrainMesh.SetWaterColorSet((unsigned long)(long)i[0], (unsigned long)(long)i[1]);
        break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_UPDATE_CLIFFS: ee_gamecpp_mx_update_cliffs(gWorld.mMap, i[0], i[1], i[2], i[3]); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_CONTINENTS_CLEAR: gContinentManager.Clear(gWorld.mMap); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_CONTINENTS_BUILD: gContinentManager.BuildRegions(gWorld.mMap); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_TERRAIN_AMBIENT: return EETerrainTable::Get(i[0])->mAmbientUnitChance > 0;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_INFLUENCE: gWorld.CreateInfluenceTable(gWorld.mMap); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_CAMERA: gWorld.mCameraControl.SetLocation(GE3DPoint(f[0], f[1], f[2]), true, true); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_LOCAL_PLAYER: return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_DEFAULT_TERRAIN: return (int32_t)gDefaultTerrain;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_MAX_PLAYERS: return (int32_t)gWorld.mMaxPlayers;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_LOAD_MEDIA: gWorld.mPlayers[i[0]]->SetUnitClassMedia(gTechTree.GetData(i[1])->mUnitClass, 0x7f); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_UNLOAD_MEDIA: gWorld.mPlayers[i[0]]->UnloadUnitClassMedia(gTechTree.GetData(i[1])->mUnitClass); break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_SET_MAX_PLAYERS: gWorld.mMaxPlayers = (unsigned long)i[0]; break;
    case EE_SCREENS_SEAX_SEW + EE_SCREENS_SEW_OP_SELECT_UNIT_TYPE: gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_SELECT); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_GHOST_COUNT: return (int32_t)(unsigned long)ee_app_ghost_count();
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_UNDER_MOUSE: return (int32_t)gUIManager->GetScenarioEditorForm()->GetUnitUnderMouse();
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_LOAD_MEDIA: gWorld.mPlayers[i[0]]->SetUnitClassMedia(i[1], 0x7f); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_POST: gUserInterface->PostAction(static_cast<EEAction*>(p)); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_PITCHES: return gUnitClassTable[i[0]]->mPitchesWithTerrain != 0;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_TERRAIN_PITCH: f[0] = gWorld.mMap->GetTerrainPitch(*reinterpret_cast<GETransformation*>(t)); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_LIFT: EESEPUnit(i[0])->MoveTo(-10000, -10000, true); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_CLEAR_GHOSTS: ee_gamecpp_world_clear_ghosts(&gWorld); break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_GHOST_FOR:
        EEGhostUnit::Create(gMouse->mPosition, EESEPUnit(i[0])->GetUnitClass()->mUnitType, EESEPUnit(i[0])->mPlayer, f[0], true);
        break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_SET_LOCATION:
        EESEPUnit(i[0])->mComplexUnit->mPhysicalModel.SetLocation(*reinterpret_cast<GETransformation*>(t));
        break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_LEVEL:
        EESEPUnit(i[0])->mComplexUnit->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, i[1], i[2], &f[0]);
        break;
    case EE_SCREENS_SEAX_SEP + EE_SCREENS_SEP_DELETE: new EESEARust(EE_SCREENS_SEW_DELETE_UNIT, NULL, i[0]); break;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_SELECTED_COUNT: return gUIManager->GetScenarioEditorForm()->GetSelectedUnitCount();
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_SELECTED:
        return (intptr_t)(EEUnit*)gUIManager->GetScenarioEditorForm()->GetSelectedUnit((unsigned long)(intptr_t)q);
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_FIRST_SELECTED: return (intptr_t)gUIManager->GetScenarioEditorForm()->GetFirstSelectedUnit();
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_CLEAR: gUIManager->GetScenarioEditorForm()->ClearSelection(); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_DESELECT_FIRST: gUIManager->GetScenarioEditorForm()->DeselectFirstUnit(); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_SELECT: gUIManager->GetScenarioEditorForm()->SelectUnit(static_cast<EEUnit*>(p)); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_DESELECT: gUIManager->GetScenarioEditorForm()->DeselectUnit(static_cast<EEUnit*>(p)); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_UPDATE_SELECTION: gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_UPDATE_SELECTION); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_UNIT: return (intptr_t)gWorld.GetUnit((unsigned long)(intptr_t)q);
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_PLAYER_UNIT:
        return (intptr_t)static_cast<EEUnit*>(gWorld.GetPlayer((long)(intptr_t)p)->GetUnit((unsigned long)(intptr_t)q));
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_IS_SELECTED: return static_cast<EEUnit*>(p)->mIsEditorSelected ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_HAS_COMPLEX: return static_cast<EEUnit*>(p)->mComplexUnit != NULL ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_TURNS: return static_cast<EEUnit*>(p)->mUnitClass->mAngularAcceleration > 0.0f ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_PATH_POINT: return static_cast<EEUnit*>(p)->GetUnitClass()->GetUnitType() == kUnitTypePathPoint ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_OFF_MAP:
        return static_cast<EEUnit*>(p)->GetTileX() == -10000 && static_cast<EEUnit*>(p)->GetTileY() == -10000 ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_ON_TILE: return static_cast<EEUnit*>(p)->mTileNode != NULL ? 1 : 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_PLAYER: return (intptr_t)static_cast<EEComplexUnit*>(p)->mPlayer;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_CARGO_COUNT: return static_cast<EEComplexUnit*>(p)->GetNumCargoUnits();
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_KILL_CARGO: ee_units_cu_body(EE_UNITS_CU_DO_KILL_CARGO, static_cast<EEComplexUnit*>(p), 0, 0, 0); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_NEXT_PART: return (intptr_t)static_cast<EEComplexUnit*>(p)->mNextPart;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_CONTAINER: return (intptr_t)(EEComplexUnit*)static_cast<EEComplexUnit*>(p)->mContainer;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_CARGO: return (intptr_t)(EEUnit*)static_cast<EEComplexUnit*>(p)->GetCargo((unsigned long)(intptr_t)q);
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_MESSAGE_TYPE: return static_cast<EEMessage*>(p)->GetType();
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_MESSAGE_UNIT:   // Select Unit, the toggle, Always Add: records whose first member is the unit
        return static_cast<EEMRecord*>(p)->Get<uint32_t>(1);
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_MESSAGE_RELEASE: static_cast<EEMessage*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_MESSAGE_UNIT_TYPE:
        return (intptr_t)(intptr_t)static_cast<EEMRecord*>(p)->Get<int32_t>(4);   // the unit type (off the wire)
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_PICK_AT_MOUSE: return (intptr_t)gUnitPicker.PickUnit(gMouse->mPosition, 0);
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_UNIT_RELEASE: static_cast<EEUnit*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_NEW_TOGGLE:
        return (intptr_t)static_cast<EEMessage*>(new EEMRecord(kMessageAddUnitToSelectedGroup, {static_cast<EEUnit*>(p)->mUnitID}));
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_NEW_UNSELECT_ALL: return (intptr_t)static_cast<EEMessage*>(new EEMRecord(kMessageUnselectAllUnits));
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_SET_MESSAGE:
        static_cast<EEAMouseClick*>(static_cast<EEAction*>(p))->SetMessage(static_cast<EEMessage*>(q));
        return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_POST: gUserInterface->PostAction(static_cast<EEAction*>(p)); return 0;
    case EE_SCREENS_SEAX_SEU + EE_SCREENS_SEU_MESSAGE_RECTANGLE:   // the box selection's rectangle (member 3, off the wire)
        return (intptr_t)&(sSeuRectangle = static_cast<EEMRecord*>(p)->Get<GERectangle>(3));
    case EE_SCREENS_SEAX_POST_ACTION: gUserInterface->PostAction(static_cast<EEAction*>(p)); return 0;
    case EE_SCREENS_SEAX_DEREFERENCE_ACTION: static_cast<EEAction*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_FOLLOW_529066: gB529066 = false; return 0;
    case EE_SCREENS_SEAX_CAMERA_FOLLOWING: gCameraFollowing = false; return 0;
    case EE_SCREENS_SEAX_FOLLOW_529068: gB529068 = false; return 0;
    case EE_SCREENS_SEAX_MESH_VALID_POINT: return gWorld.mMap->mTerrainMesh.GetIsValidPoint(i[0], i[1]);
    case EE_SCREENS_SEAX_MESH_Z: f[0] = gWorld.mMap->mTerrainMesh.GetZ(i[0], i[1]); return 0;
    case EE_SCREENS_SEAX_TERRAIN_AMBIENTS_ONLY: return EETerrainTable::Get(i[0])->mIsAmbientsOnly ? 1 : 0;
    case EE_SCREENS_SEAX_TERRAIN_NUM_CLASSES: return (int32_t)EETerrainTable::Get(i[0])->GetNumUnitClasses();
    case EE_SCREENS_SEAX_TERRAIN_CHANCE: return (int32_t)EETerrainTable::Get(i[0])->mAmbientUnitChance;
    case EE_SCREENS_SEAX_PLANT_CAN_PLACE: return ee_units_uu_can_place_unit_on_tile((uint64_t)(uintptr_t)gWorld.mPlayers[0], 0, i[0], i[1], i[2], 0, 0, NULL);
    case EE_SCREENS_SEAX_PLANT_CREATE: gUnitClassTable[i[0]]->CreateUnit(i[1], i[2], gWorld.mPlayers[0]->mPlayerID, false, NULL); return 0;
    case EE_SCREENS_SEAX_RADIO_CHECKED: return static_cast<UIRadioButton*>(gUIManager->GetScenarioEditorForm()->GetControlById(i[0]))->GetIsChecked() == true;
    case EE_SCREENS_SEAX_LONG_SET: sSeaxLong[i[0]] = i[1]; return 0;
    case EE_SCREENS_SEAX_LONG_GET: return (int32_t)sSeaxLong[i[0]];
    case EE_SCREENS_SEAX_RAISE: gWorld.mMap->Raise(&sSeaxLong[0], &sSeaxLong[1], &sSeaxLong[2], &sSeaxLong[3], f[0], f[1], f[2], f[3], i[0] != 0); return 0;
    case EE_SCREENS_SEAX_TILE: return (intptr_t)static_cast<EEMapTile*>(gWorld.mMap->RTSMap::GetTile(i[0], i[1]));
    case EE_SCREENS_SEAX_TILE_FIRST_NODE: return (intptr_t)static_cast<EEMapTile*>(p)->mUnits;
    case EE_SCREENS_SEAX_NODE_NEXT: return (intptr_t)static_cast<RTSTileUnitNode*>(p)->mNext;
    case EE_SCREENS_SEAX_NODE_UNIT: return (intptr_t)static_cast<EEUnit*>(static_cast<RTSTileUnitNode*>(p)->mUnit);
    case EE_SCREENS_SEAX_UNIT_ON_TILE: return static_cast<EEUnit*>(p)->mTileNode != NULL;
    case EE_SCREENS_SEAX_UNIT_PREVIOUS_PART: return (intptr_t)static_cast<EEUnit*>(static_cast<EEUnit*>(p)->mComplexUnit->mPreviousPart);
    case EE_SCREENS_SEAX_UNIT_REFERENCE: static_cast<EEUnit*>(p)->Reference(); return 0;
    case EE_SCREENS_SEAX_UNIT_PATH_POINT: return static_cast<EEUnit*>(p)->GetUnitClass()->GetUnitType() == kUnitTypePathPoint;
    case EE_SCREENS_SEAX_UNIT_RESOURCE: return static_cast<EEUnit*>(p)->GetUnitClass()->GetCategory() == kUnitCategoryResource;
    case EE_SCREENS_SEAX_UNIT_EMPTY: static_cast<EEUnit*>(p)->SetResourceAmount(0); return 0;
    case EE_SCREENS_SEAX_UNIT_KILL: static_cast<EEUnit*>(p)->Kill(false, false, false); return 0;
    case EE_SCREENS_SEAX_UNIT_PLAYER_KILL: static_cast<EEUnit*>(p)->GetPlayer()->KillUnit(static_cast<EEUnit*>(p), false); return 0;
    case EE_SCREENS_SEAX_UNIT_DEREFERENCE: static_cast<EEUnit*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_SCREEN_EVENT_NEW: return (intptr_t)EECreateScreenEvent((uint32_t)i[0]);
    case EE_SCREENS_SEAX_EVENT_POST: gUserInterface->PostEvent(static_cast<UIEvent*>(p), NULL); return 0;
    case EE_SCREENS_SEAX_EVENT_DEREFERENCE: static_cast<UIEvent*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_QUICK_SAVE_NOT_LOAD: static_cast<EEAQuickSaveLoad*>(static_cast<EEAction*>(p))->mIsLoad = false; return 0;
    case EE_SCREENS_SEAX_QUICK_SAVE_WRITE: static_cast<EEAQuickSaveLoad*>(static_cast<EEAction*>(p))->mWriteToFile = true; return 0;
    case EE_SCREENS_SEAX_TECH_FILL: EEUIShell::Op(gUIManager->GetScenarioEditorForm()->GetControlById(i[0]), EE_SCREENS_SKOP_TECH_FILL, i[1], i[2]); return 0;
    case EE_SCREENS_SEAX_PAINT_AREA: gUIManager->GetScenarioEditorForm()->SetPaintArea(GERectangle((long)i[0], (long)i[1], (long)i[2], (long)i[3])); return 0;
    case EE_SCREENS_SEAX_PAUSE_POST: ee_rts_graphics_post_pause(gGraphics); return 0;
    case EE_SCREENS_SEAX_WORLD_WRITE_UNLOCK: gWorld.mLock->WriteUnlock(); return 0;
    case EE_SCREENS_SEAX_UI_UNLOCK: gUIManager->UnlockUIForCopyData(); return 0;
    case EE_SCREENS_SEAX_GRAPHICS_PAUSED: return gGraphics->mIsPaused;
    case EE_SCREENS_SEAX_SLEEP: PlatSleep(1); return 0;
    case EE_SCREENS_SEAX_UI_LOCK: gUIManager->LockUIForCopyData(); return 0;
    case EE_SCREENS_SEAX_WORLD_WRITE_LOCK: gWorld.mLock->WriteLock(); return 0;
    case EE_SCREENS_SEAX_WORLD_RESET: gWorld.Reset(); return 0;
    case EE_SCREENS_SEAX_NEW_SCENARIO_MANAGER: gWorld.SetScenarioManager(new EEScenarioManager); return 0;
    case EE_SCREENS_SEAX_NEW_ENVIRONMENT_MANAGER: gWorld.SetEnvironmentManager(new EEEnvironmentManager); return 0;
    case EE_SCREENS_SEAX_MAP_NEW: return (intptr_t)new EEMap(i[0], i[1], (unsigned long)i[2], i[3]);
    case EE_SCREENS_SEAX_MAP_VERIFY: ESVERIFY_AT(static_cast<EEMap*>(p), 249); return 0;
    case EE_SCREENS_SEAX_MAP_SET: gWorld.SetMap(static_cast<EEMap*>(p)); return 0;
    case EE_SCREENS_SEAX_MESH_SET_WATER:
        gWorld.mMap->mTerrainMesh.SetIsWater((unsigned long)(long)i[0], (unsigned long)(long)i[1], i[2] != 0 ? 1 : 0);
        return 0;
    case EE_SCREENS_SEAX_TILE_SET_HEIGHT: gWorld.mMap->GetTile((unsigned long)(long)i[0], (unsigned long)(long)i[1])->mHeight = f[0]; return 0;
    case EE_SCREENS_SEAX_PAINT_NEW:
        return (intptr_t)static_cast<EEAction*>(new EESEAPaintRust(EE_SCREENS_SEA_PAINT_TERRAIN, i[0], EETerrainTable::Get(i[0])->mColorType));
    case EE_SCREENS_SEAX_PAINT_SET_AREA: ee_screens_ga_paint_set_area(static_cast<EESEAPaint*>(static_cast<EEAction*>(p)), (int32_t)(i[0]), (int32_t)(i[1]), (int32_t)(i[2]), (int32_t)(i[3])); return 0;
    case EE_SCREENS_SEAX_MINIMAP_SET_MAP: gMiniMap->SetMap(gWorld.mMap); return 0;
    case EE_SCREENS_SEAX_MINIMAP_UPDATE: EEUIMiniMap::Update(gMiniMap); return 0;
    case EE_SCREENS_SEAX_MINIMAP_REDRAW_ALL: gMiniMap->RedrawAll(); return 0;
    case EE_SCREENS_SEAX_MINIMAP_REDRAW: ee_gamecpp_minimap_redraw(gMiniMap); return 0;
    case EE_SCREENS_SEAX_MINIMAP_REFRESH: gMiniMap->Refresh(); return 0;
    case EE_SCREENS_SEAX_GRAPHICS_RELEASE_SCENE: gGraphics->PostReleaseScene(); return 0;
    case EE_SCREENS_SEAX_GRAPHICS_INITIALISE_SCENE: gGraphics->PostInitialiseScene(); return 0;
    case EE_SCREENS_SEAX_GRAPHICS_RESUME: gGraphics->PostResume(); return 0;
    case EE_SCREENS_SEAX_OPTIONS_NEW: return (intptr_t)new EEGameOptions;
    case EE_SCREENS_SEAX_OPTIONS_MAP_SIZE: static_cast<EEGameOptions*>(p)->mMapSize = (unsigned long)i[0]; return 0;
    case EE_SCREENS_SEAX_OPTIONS_EPOCH: static_cast<EEGameOptions*>(p)->mEpochStart = i[0]; return 0;
    case EE_SCREENS_SEAX_OPTIONS_DIMENSIONS: ee_gamecpp_options_map_dimensions(static_cast<EEGameOptions*>(p), &(sSeaxLong[0]), &(sSeaxLong[1])); return 0;
    case EE_SCREENS_SEAX_OPTIONS_DELETE: delete static_cast<EEGameOptions*>(p); return 0;
    case EE_SCREENS_SEAX_PLAYER: return (intptr_t)gWorld.mPlayers[i[0]];
    case EE_SCREENS_SEAX_PLAYER_OBSERVER: return static_cast<EEPlayer*>(p)->mObserverMode != false;
    case EE_SCREENS_SEAX_PLAYER_HUMAN: return dynamic_cast<EEHumanPlayer*>(static_cast<EEPlayer*>(p)) != NULL;
    case EE_SCREENS_SEAX_PLAYER_START_EPOCH: return (int32_t)static_cast<EEPlayer*>(p)->GetTechTree()->mStartEpoch;
    case EE_SCREENS_SEAX_PLAYERS_CLEAR: sSeaxPlayers.clear(); return 0;
    case EE_SCREENS_SEAX_PLAYERS_PUSH: sSeaxPlayers.push_back(gWorld.mPlayers[i[0]]); return 0;
    case EE_SCREENS_SEAX_POINT_ZERO: sSeaxPoint = GE3DPoint(0.0f, 0.0f, 0.0f); return 0;
    case EE_SCREENS_SEAX_RANDOM_MAP:
        EERandomMap::CreateRandomMap(static_cast<EEGameOptions*>(p), sSeaxPlayers, (unsigned long)i[0], EERMGetGeneratorName((unsigned long)i[1]), gUserInterface->mRandomSeed, gWorld.mMap, sSeaxPoint);
        return 0;
    case EE_SCREENS_SEAX_POINT_X: f[0] = sSeaxPoint.mX; return 0;
    case EE_SCREENS_SEAX_POINT_Y: f[0] = sSeaxPoint.mY; return 0;
    case EE_SCREENS_SEAX_POINT_Z: f[0] = sSeaxPoint.mZ; return 0;
    case EE_SCREENS_SEAX_PLAYER_PENDING_COUNT: return (intptr_t)static_cast<EEPlayer*>(p)->GetNumPendingUnits();
    case EE_SCREENS_SEAX_PENDING: return (intptr_t)static_cast<EEUnit*>(static_cast<RTSUnit*>(ee_rts_player_pending_unit(static_cast<EEPlayer*>(p), i[0])));
    case EE_SCREENS_SEAX_UNIT_LOAD_MEDIA: static_cast<EEUnit*>(p)->LoadMedia(static_cast<EEUnit*>(p)->IsFoundation()); return 0;
    case EE_SCREENS_SEAX_ADD_UNIT_ENTRY: gWorld.AddUnitEntry(static_cast<RTSUnit*>(ee_rts_player_pending_unit(static_cast<EEPlayer*>(p), i[0])), NULL); return 0;
    case EE_SCREENS_SEAX_UNIT_BUILT_LAND_BUILDING:
        return static_cast<EEUnit*>(p)->GetUnitClass()->IsLandBuilding() && static_cast<EEUnit*>(p)->GetComplexUnit()->mIsBuilt == true;
    case EE_SCREENS_SEAX_PLAYER_LOAD_BUTTONS: static_cast<EEPlayer*>(p)->LoadUnitClassButtons(static_cast<EEUnit*>(q)->GetUnitClass()->GetUnitType()); return 0;
    case EE_SCREENS_SEAX_WALLS_CLEAR: sSeaxWalls.clear(); return 0;
    case EE_SCREENS_SEAX_PLAN_WALL:
        ee_units_uu_plan_wall(f[0], f[1], f[2], f[3], (uint64_t)(uintptr_t)gUnitClassTable[gWorld.mPlayers[i[0]]->mTechnologies->GetData(i[1])->mUnitClass], (uint64_t)(uintptr_t)gWorld.mPlayers[i[0]], 0, 1, &sSeaxWalls);
        return 0;
    case EE_SCREENS_SEAX_GHOST_ENTER:
        gWorld.mGhostUnitsLock->Enter();
        return 0;
    case EE_SCREENS_SEAX_GHOST_LEAVE:
        gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEAX_GHOST_COUNT:
        return (intptr_t)(unsigned long)ee_app_ghost_count();
    case EE_SCREENS_SEAX_GHOST:
        return (intptr_t)gWorld.GetGhostUnit(i[0]);
    case EE_SCREENS_SEAX_BUILD_GATE:
        EEBuildGate(sSeaxTile = EETileCoordinate(i[0], i[1]), i[2], i[3], i[4]);
        return 0;
    case EE_SCREENS_SEAX_UNIT_BY_ID:
        return (intptr_t)gWorld.GetUnit((unsigned long)(uint32_t)i[0]);
    case EE_SCREENS_SEAX_RTS_UNIT_PLAYER_ID:
        return (int32_t)static_cast<RTSUnit*>(p)->mPlayer->mPlayerID;
    case EE_SCREENS_SEAX_ELEVATION_SELECTION:
        static_cast<UIComboBox*>(gUIManager->GetScenarioEditorForm()->GetControlById(EE_SCREENS_SE_MAP_ELEVATION_FILE))->GetCurrentSelection(sSeaxSelection);
        return 0;
    case EE_SCREENS_SEAX_SELECTION_GET: return (intptr_t)sSeaxSelection;
    case EE_SCREENS_SEAX_SPEC_NEW: return (intptr_t)new FSFileSpec;
    case EE_SCREENS_SEAX_SPEC_DIRECTORY: static_cast<FSFileSpec*>(p)->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\" + "Elevation Maps\\"); return 0;
    case EE_SCREENS_SEAX_SPEC_FILE:
        static_cast<FSFileSpec*>(p)->SetFile(UString(static_cast<UIComboBox*>(gUIManager->GetScenarioEditorForm()->GetControlById(EE_SCREENS_SE_MAP_ELEVATION_FILE))->GetListBox()->GetData(sSeaxSelection) + (i[0] != 0 ? ".txt" : ".dat")));
        return 0;
    case EE_SCREENS_SEAX_SPEC_OPEN: return static_cast<FSFileSpec*>(p)->Open(kFileAccessRead);
    case EE_SCREENS_SEAX_SPEC_READ: static_cast<FSFileSpec*>(p)->Read(static_cast<unsigned char*>(q), (unsigned long)(uint32_t)i[0], sSeaxRead); return 0;
    case EE_SCREENS_SEAX_READ_COUNT: return (intptr_t)sSeaxRead;
    case EE_SCREENS_SEAX_SPEC_CLOSE: static_cast<FSFileSpec*>(p)->Close(); return 0;
    case EE_SCREENS_SEAX_SPEC_DELETE: delete static_cast<FSFileSpec*>(p); return 0;
    case EE_SCREENS_SEAX_EDIT_INT: return static_cast<UIEditBox*>(gUIManager->GetScenarioEditorForm()->GetControlById(i[0]))->GetTextAsInt(0);
    case EE_SCREENS_SEAX_GHOST_TYPE: return (intptr_t)static_cast<EEGhostUnit*>(p)->mUnitClass->mUnitType;
    case EE_SCREENS_SEAX_CLASS: return (intptr_t)gUnitClassTable[i[0]];
    case EE_SCREENS_SEAX_CLASS_SMALL: return static_cast<EEUnitClass*>(p)->mTileSize <= 1 && !static_cast<EEUnitClass*>(p)->IsLandBuilding();
    case EE_SCREENS_SEAX_CLASS_FLIES: return static_cast<EEUnitClass*>(p)->FliesOver() == true;
    case EE_SCREENS_SEAX_CLASS_CRUISE: f[0] = static_cast<EEUnitClass*>(p)->GetCruiseAltitude(); return 0;
    case EE_SCREENS_SEAX_GHOST_TILE_X: return (int32_t)static_cast<EEGhostUnit*>(p)->mTileX;
    case EE_SCREENS_SEAX_GHOST_TILE_Y: return (int32_t)static_cast<EEGhostUnit*>(p)->mTileY;
    case EE_SCREENS_SEAX_GHOST_CLASS_TILE_SIZE: return (int32_t)static_cast<EEGhostUnit*>(p)->mUnitClass->mTileSize;
    case EE_SCREENS_SEAX_GHOST_ANIMATION: return (int32_t)static_cast<EEGhostUnit*>(p)->mAnimation;
    case EE_SCREENS_SEAX_GHOST_TRANSFORMATION: *reinterpret_cast<GETransformation*>(t) = static_cast<EEGhostUnit*>(p)->mTransformation; return 0;
    case EE_SCREENS_SEAX_GHOST_CAN_PLACE: return static_cast<EEGhostUnit*>(p)->mCanPlace;
    case EE_SCREENS_SEAX_TILE_HEIGHT: f[0] = static_cast<EEMapTile*>(p)->mHeight; return 0;
    case EE_SCREENS_SEAX_SEP_UNIT: return (intptr_t)EESEPUnit(i[0]);
    case EE_SCREENS_SEAX_SEP_COMPLEX: return (intptr_t)EESEPComplexUnit(i[0]);
    case EE_SCREENS_SEAX_UNIT_COMPLEX: return (intptr_t)static_cast<EEUnit*>(p)->mComplexUnit;
    case EE_SCREENS_SEAX_UNIT_PLAYER: return (intptr_t)static_cast<EEUnit*>(p)->mPlayer;
    case EE_SCREENS_SEAX_COMPLEX_PLAYER_IS: return static_cast<EEComplexUnit*>(p)->mPlayer == static_cast<EEPlayer*>(q);
    case EE_SCREENS_SEAX_UNIT_TRANSPORTS: return static_cast<EEUnit*>(p)->mUnitClass->mTransportCapacity > 0;
    case EE_SCREENS_SEAX_COMPLEX_ROOM_FOR: return static_cast<EEComplexUnit*>(p)->mCargoRoom >= static_cast<EEUnitClass*>(q)->mWeight;
    case EE_SCREENS_SEAX_CLASS_CAN_ENTER: return static_cast<EEUnitClass*>(p)->CanEnter(static_cast<EEComplexUnit*>(q)->mUnitClass);
    case EE_SCREENS_SEAX_COMPLEX_REFERENCE: static_cast<EEComplexUnit*>(p)->Reference(); return 0;
    case EE_SCREENS_SEAX_COMPLEX_ID: return (int32_t)static_cast<EEComplexUnit*>(p)->mUnitID;
    case EE_SCREENS_SEAX_CLASS_TYPE: return (intptr_t)static_cast<EEUnitClass*>(p)->mUnitType;
    case EE_SCREENS_SEAX_CREATE_AT: return (intptr_t)static_cast<EEUnitClass*>(p)->CreateUnitAt(*reinterpret_cast<GETransformation*>(t), i[0], false, NULL);
    case EE_SCREENS_SEAX_CREATE_AT_WORLD: return (intptr_t)static_cast<EEUnitClass*>(p)->CreateUnitAt(gWorld.mTransformation, i[0], false, NULL);
    case EE_SCREENS_SEAX_UNIT_CLOAKABLE: static_cast<EEUnit*>(p)->GetComplexUnit()->SetCloakableChain(true); return 0;
    case EE_SCREENS_SEAX_UNIT_CLOAKED: static_cast<EEUnit*>(p)->GetComplexUnit()->SetCloakedChain(true); return 0;
    case EE_SCREENS_SEAX_CLOSEST_TILE:
        gWorld.mMap->GetClosestTile(&(sSeaxPoint = GE3DPoint(*reinterpret_cast<GETransformation*>(t))), &sSeaxLong[0], &sSeaxLong[1]);
        return 0;
    case EE_SCREENS_SEAX_UNIT_SET_TILE_X: static_cast<EEUnit*>(p)->mTileX = sSeaxLong[0]; return 0;
    case EE_SCREENS_SEAX_UNIT_SET_TILE_Y: static_cast<EEUnit*>(p)->mTileY = sSeaxLong[1]; return 0;
    case EE_SCREENS_SEAX_UNIT_UPDATE_TILE: ee_rts_unit_update_tile(static_cast<EEUnit*>(p), gWorld.mMap); return 0;
    case EE_SCREENS_SEAX_PLAYER_UNLOAD_MEDIA: static_cast<EEPlayer*>(p)->UnloadUnitClassMedia(i[0]); return 0;
    case EE_SCREENS_SEAX_CLASS_LAND_BUILDING: return static_cast<EEUnitClass*>(p)->IsLandBuilding();
    case EE_SCREENS_SEAX_UNIT_SET_BUILT: static_cast<EEUnit*>(p)->GetComplexUnit()->SetBuilt(1, 0, 1); return 0;
    case EE_SCREENS_SEAX_UNIT_SET_ANIMATION: static_cast<EEUnit*>(p)->GetComplexUnit()->SetAnimation(i[0], -1, 0); return 0;
    case EE_SCREENS_SEAX_UNIT_POWERED: return static_cast<EEUnit*>(p)->mStats->mPower > 0;
    case EE_SCREENS_SEAX_UNIT_SET_POWER: static_cast<EEUnit*>(p)->GetComplexUnit()->SetPower(static_cast<EEUnit*>(p)->mStats->mPower); return 0;
    case EE_SCREENS_SEAX_PLAYER_ADD_PENDING: static_cast<EEPlayer*>(p)->AddPendingUnits(0, 1); return 0;
    case EE_SCREENS_SEAX_COMPLEX_LOAD_UNIT: (long)ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, static_cast<EEComplexUnit*>(p), (int64_t)(uintptr_t)(static_cast<EEUnit*>(q)->GetComplexUnit()), false, 0); return 0;
    case EE_SCREENS_SEAX_COMPLEX_EDITOR_SELECTED: return static_cast<EEComplexUnit*>(p)->mIsEditorSelected;
    case EE_SCREENS_SEAX_EDITOR_SELECT_COMPLEX: gUIManager->GetScenarioEditorForm()->SelectUnit(static_cast<EEComplexUnit*>(p)); return 0;
    case EE_SCREENS_SEAX_EDITOR_UPDATE_SELECTION: gUIManager->GetScenarioEditorForm()->UnitCall(EE_SCREENS_SEUN_UPDATE_SELECTION); return 0;
    case EE_SCREENS_SEAX_COMPLEX_DEREFERENCE: static_cast<EEComplexUnit*>(p)->Dereference(); return 0;
    case EE_SCREENS_SEAX_UNIT_ID: return (int32_t)static_cast<EEUnit*>(p)->mUnitID;
    case EE_SCREENS_SEAX_COMPLEX_NEXT: return (intptr_t)static_cast<EEComplexUnit*>(p)->mNextPart;
    case EE_SCREENS_SEAX_UNIT_CLASS: return (intptr_t)static_cast<EEUnit*>(p)->GetUnitClass();
    case EE_SCREENS_SEAX_CLASS_PITCHES: return static_cast<EEUnitClass*>(p)->mPitchesWithTerrain != 0;
    case EE_SCREENS_SEAX_COMPLEX_LOCATION: *reinterpret_cast<GETransformation*>(t) = static_cast<EEComplexUnit*>(p)->mPhysicalModel.GetLocation(); return 0;
    case EE_SCREENS_SEAX_COMPLEX_CLASS: return (intptr_t)static_cast<EEComplexUnit*>(p)->GetUnitClass();
    case EE_SCREENS_SEAX_COMPLEX_LOAD_COMPLEX: (long)ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, static_cast<EEComplexUnit*>(p), (int64_t)(uintptr_t)(static_cast<EEUnit*>(q)->mComplexUnit), false, 0); return 0;
    case EE_SCREENS_SEAX_COMPLEX_MOVE_TO: static_cast<EEComplexUnit*>(p)->MoveTo(i[0], i[1], true); return 0;
    case EE_SCREENS_SEAX_UNIT_MOVE_TO: static_cast<EEUnit*>(p)->MoveTo(i[0], i[1], true); return 0;
    case EE_SCREENS_SEAX_FACING_QUARTER: return (intptr_t)EEGetUnitFacingQuarter(static_cast<EEComplexUnit*>(p));
    case EE_SCREENS_SEAX_FACING: return (intptr_t)static_cast<EEComplexUnit*>(p)->mFacing;
    case EE_SCREENS_SEAX_SET_FACING: static_cast<EEComplexUnit*>(p)->mFacing = i[0]; return 0;
    case EE_SCREENS_SEAX_LEVEL_GROUND:
        static_cast<EEComplexUnit*>(p)->GetUnitClass()->LevelGround(&gWorld.mMap->mTerrainMesh, static_cast<EEComplexUnit*>(p)->mTileX, static_cast<EEComplexUnit*>(p)->mTileY, &f[0]);
        return 0;
    case EE_SCREENS_SEAX_T_FROM_LOCATION: sSeaxT = static_cast<EEUnit*>(p)->mComplexUnit->mPhysicalModel.GetLocation(); return 0;
    case EE_SCREENS_SEAX_T_YPR: sSeaxT.SetOrientationYPR(f[0], 0.0f, 0.0f); return 0;
    case EE_SCREENS_SEAX_T_APPLY: static_cast<EEUnit*>(p)->mComplexUnit->SetTransformation(sSeaxT, true); return 0;
    case EE_SCREENS_SEAX_COMPLEX_ATC_AIRPORT:
        return static_cast<EEComplexUnit*>(p)->mUnitClass->IsLandBuilding() && static_cast<EEComplexUnit*>(p)->mUnitClass->mAirTrafficType != 0;
    case EE_SCREENS_SEAX_ATC_REMOVE:
        ee_units_atc_remove_airport(static_cast<EEComplexUnit*>(p)->mPlayer->mAirTraffic[static_cast<EEComplexUnit*>(p)->mUnitClass->mAirTrafficType - 1], static_cast<EEComplexUnit*>(p));
        return 0;
    case EE_SCREENS_SEAX_COMPLEX_KILL: static_cast<EEComplexUnit*>(p)->Kill(false, false, false); return 0;
    case EE_SCREENS_SEAX_PLAYER_KILL_COMPLEX: static_cast<EEPlayer*>(p)->KillUnit(static_cast<EEComplexUnit*>(q), false); return 0;
    case EE_SCREENS_SEAX_COMPLEX_UNLOAD:
        static_cast<EEComplexUnit*>(p)->UnloadCargoAt((unsigned long)(intptr_t)q, GETransformation(static_cast<EEComplexUnit*>(p)->mPhysicalModel.GetLocation()));
        return 0;
    case EE_SCREENS_SEAX_IDS_CLEAR: sSeaxIDs.clear(); return 0;
    case EE_SCREENS_SEAX_PICK: ee_gamecpp_pz_box(&gUnitPicker, &(*static_cast<const GERectangle*>(p)), true, NULL, (long)(i[0]), &sSeaxIDs); return 0;
    case EE_SCREENS_SEAX_IDS_COUNT: return (intptr_t)sSeaxIDs.size();
    case EE_SCREENS_SEAX_ID: return (intptr_t)(uint32_t)sSeaxIDs[i[0]];
    }
    return 0;
}
