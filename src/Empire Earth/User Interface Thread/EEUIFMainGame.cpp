// C:\EEX\Empire Earth\User Interface Thread\EEUIFMainGame.cpp (__FILE__ 0x262e73). The in-game form: 0x26272b..0x26783d
// (the default player names' init chain first). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureManager.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "lle/UResourceFile.h"
#include "lle/GEText.h"
#include "lle/UITLScrolling.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIBitmap.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EmpireEarth.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIFMGStatusButton.h"
#include "lle/UIFormManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEStringIDs.h"
// the command buttons' part of the host (EEUIFMGStatusButton.cpp)
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2 (EEUIBuildGate.cpp's)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the calamity's ghost at the mouse) (provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (Open passes mIsButtonBlinking)
void EEPostGraphicsAll();                   // 0x10fc0c (Open) (provisional name)
class EEMiniMap;
extern EEMiniMap* gMiniMap;                 // 0x564888 (ee/EEMiniMap.h; stored in EEGlobals.cpp)
extern RTSRenderObject* gU56534C;           // 0x56534c (the render object pushed when the form closes)
extern float& gMasterVolume;                 // 0x530830 (the master volume Open restores)

// the information panel's message is the scenario's dialog message (EEUserInterface's message 0x422 hands over an
// EESMDialogMessage; ShowInformation 0x26533e read it through the 32-bit offsets of a stand-in class, whose fields
// lined up there and not on x64: the portrait's class was read from the duration, the scroll time from the unit id)

// The unit hack whole in Rust (ee_screens unit_hack.rs): the original's members 0x2628e5 (ctor: the sorted
// class names), 0x262a91 (dtor), 0x262aa5 PlaceUnit, 0x262c6f FillComboBox, 0x262cdc LoadUnitData, 0x262d66
// Update, 0x262e29 GetSelection, and the two message ctors' COMDATs 0x262c24 (EEMHackANewUnit) and 0x262d32
// (EEMLoadUnitData). The host answers its ops (EE_SCREENS_UH_OP_*): the class table, the object and player lists,
// the keys, the mouse, the messages.
#include "ee/EEScreensShims.h"
// the moved cases' shims (ee_screens unit_hack_calls.rs asks them first)
#define UH_OBJECTS(x) static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(x))->GetControl(kMainGameTempObjectList))
static GE3DPoint sUHPoint;   // the point's out (a local of the original case)
static ISEntity* EEKey(unsigned long inKey);
static EETile* sShimTile;   // the mouse tile's other out (a local of the original case)
extern "C" bool ee_screenscpp_uh_class_exists(int32_t inIndex) { return gUnitClassTable[inIndex] != NULL; }
extern "C" bool ee_screenscpp_uh_class_enabled(int32_t inIndex) { return gUnitClassTable[inIndex]->mIsEnabled; }
extern "C" bool ee_screenscpp_uh_class_placeable(int32_t inIndex)
{ return gUnitClassTable[inIndex]->mIsPlayerPlaceable; }
extern "C" void ee_screenscpp_uh_class_name(int32_t inIndex, void* inName)
{ EEShimOut(inName, UWideString((const char*)gUnitClassTable[inIndex]->mName)); }
extern "C" void ee_screenscpp_uh_combo_append(uintptr_t inForm, EeScreensShimText inLine)
{ UH_OBJECTS(inForm)->AppendLine(EEShimText(inLine)); }
extern "C" uintptr_t ee_screenscpp_uh_combo_trigger(uintptr_t inForm)
{ return (uintptr_t)UH_OBJECTS(inForm)->GetTriggerEvent(kControlTriggerSelectionChanged); }
extern "C" void ee_screenscpp_uh_combo_set_trigger(uintptr_t inForm, uintptr_t inEvent)
{ UH_OBJECTS(inForm)->SetTriggerEvent(kControlTriggerSelectionChanged, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_uh_combo_select_first(uintptr_t inForm) { UH_OBJECTS(inForm)->SetCurrentSelection(0); }
extern "C" void ee_screenscpp_event_reference(uintptr_t inEvent) { reinterpret_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_uh_combo_selection(uintptr_t inForm, unsigned long* inSelection)
{ UH_OBJECTS(inForm)->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_object_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)UH_OBJECTS(inForm)->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_player_list_selection(uintptr_t inForm, unsigned long* inSelection)
{ return (int32_t)static_cast<UIComboBox*>(static_cast<EEUIFMainGame*>(reinterpret_cast<void*>(inForm))->GetControl(kMainGameTempPlayerList))->GetListBox()->GetCurrentSelection(*inSelection); }
extern "C" int32_t ee_screenscpp_uh_control_value() { return (int32_t)EEKey(kKeyControl)->GetValue(); }
extern "C" void ee_screenscpp_uh_send_place(int32_t inUnitType, float inX, float inY, int32_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHackANewUnit, {(long)inUnitType, GE2DPoint(inX, inY), (unsigned char)inFlag}), kSlotNone); }
extern "C" bool ee_screenscpp_uh_point(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), sShimTile, sUHPoint, true); }
extern "C" float ee_screenscpp_uh_point_x() { return sUHPoint.GetX(); }
extern "C" float ee_screenscpp_uh_point_y() { return sUHPoint.GetY(); }
extern "C" void ee_screenscpp_uh_point_reset() { sUHPoint = GE3DPoint(); }
extern "C" int32_t ee_screenscpp_unit_hack(void* inForm, uint32_t inOp, int32_t inA, int32_t inB, float* ioPoint,
                                           uint32_t* ioSelection, uint8_t* ioText, size_t inLength)
{
    switch (inOp) {
    case EE_SCREENS_UH_OP_CLASS_COUNT:
        return (int32_t)EEUnitClassCount();
    case EE_SCREENS_UH_OP_CLASS_CATEGORY:
        return gUnitClassTable[inA]->mCategory;
    case EE_SCREENS_UH_OP_CLASS_TYPE:
        return gUnitClassTable[inA]->mUnitType;
    case EE_SCREENS_UH_OP_COMBO_CLEAR:
        UH_OBJECTS(inForm)->DeleteAllEntries();
        return 0;
    case EE_SCREENS_UH_OP_TILE:
        return gWorld.mMap->GetTile((long)ioPoint[0], (long)ioPoint[1]) != NULL;
    case EE_SCREENS_UH_OP_PLAYER_SKIPPED:
        return gWorld.mPlayers[inA] == NULL || gWorld.mPlayers[inA] == gWorld.GetCurrentPlayer();
    case EE_SCREENS_UH_OP_LOCAL_PLAYER:
        return (int32_t)gWorld.GetCurrentPlayer()->mPlayerID;
    case EE_SCREENS_UH_OP_SEND_LOAD:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadUnitData, {inA, true}), kSlotNone);
        return 0;
    case EE_SCREENS_UH_OP_IN_VIEW:
        return gViewport.GetViewRectangle().Contains(GE2DPoint(ioPoint[0], ioPoint[1]));
    case EE_SCREENS_UH_OP_LEFT_BUTTON:
        return inA == 0 ? UIManager::GetLMouseButton()->JustPressed() : UIManager::GetLMouseButton()->JustUnpressed();
    }
    return 0;
}

EEUIFMGUnitHack::EEUIFMGUnitHack(EEUIFMainGame* inForm)
{
    this->mForm = inForm;
    this->mRecord = ee_screens_unit_hack_new(inForm);
}

EEUIFMGUnitHack::~EEUIFMGUnitHack()
{
    ee_screens_unit_hack_free(this->mRecord);
}

// (EEUIFMGFormCall, one call of the form's work in Rust: ee/EEUIFMainGame.h)
static_assert(offsetof(EEUIFMGFormCall, mForm) == 0, "main_form_ffi.rs reads the form as the record's first word");
static_assert(offsetof(EEUIFMGFormCall, mText) == 2 * sizeof(void*) &&
                  offsetof(EEUIFMGFormCall, mName) == 3 * sizeof(void*) + sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mChat) == 3 * sizeof(void*) + 2 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mButton) == 15 * sizeof(void*) + 3 * sizeof(UWideString) &&
                  offsetof(EEUIFMGFormCall, mOut) == 16 * sizeof(void*) + 3 * sizeof(UWideString),
              "hud_direct.rs FormCallHead: the held text, the chat line's name and text, the button, the help");

// Creates the main game form: its panel groups, mouse actions (scroll, zoom, click, lasso, build, calamity),
// the mouse and keyboard tests of every interface mode, the unit hack, the hot keys (ee_screens main_form.rs
// construct; it reads no control: they are made after it)
EEUIFMainGame::EEUIFMainGame(UIManager* inManager, const UIDbDataForms& inData)
    : UIForm(inManager, inData, true), EeScreensMainFormFields()
{
    ee_screens_main_form_fields(this, static_cast<EeScreensMainFormFields*>(this));
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_construct(&theCall);
}

// Destroys the main game form, its actions, panel groups, poll and unit hack
EEUIFMainGame::~EEUIFMainGame() { ee_screens_mfb_destroy((uintptr_t)this); }

// Shows a chat message in the next free chat line (scrolling the lines up when full), wrapping the rest of a
// long message onto further lines (ee_screens main_form.rs)
void EEUIFMainGame::DisplayChatLine(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                    unsigned long inFlags)
{
    EEUIFMGFormCall theCall(this);
    theCall.mColor = &inColor;
    theCall.mName = inName;
    theCall.mChat = inText;
    theCall.mFlags = inFlags;
    ee_screens_main_form_chat_line(&theCall);
}

// (BuildGate and BuildWall: the placements call the build action, ee/EEARust.h)

// Snapshots the form for drawing: selection state, error log, status refresh each poll, the panel group, edge
// scrolling and the gate or wall preview of the current mode (ee_screens main_form.rs)
long EEUIFMainGame::CopyData()
{
    EEUIFMGFormCall theCall(this);
    return (long)ee_screens_main_form_copy_data(&theCall);
}

// Shows a chat message when the form has its chat lines
void EEUIFMainGame::DisplayChat(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                unsigned long inFlags, unsigned long inArg5)
{
    if (this->GetControl(kMainGameChatLine7) != NULL)
        this->DisplayChatLine(inColor, inName, inText, inFlags);
}

// Shows an information message: scrolling text, the speaker's portrait and its sound; hides the messages
// without a text control (ee_screens main_form.rs)
void EEUIFMainGame::ShowInformation(EEUIFMGInformation* inInformation)
{
    EEUIFMGFormCall theCall(this);
    theCall.mMessage = reinterpret_cast<const EESMDialogMessage*>(inInformation);
    ee_screens_main_form_show_information(&theCall);
}

// Switches the bottom panel to another entity group and selects its panel group (ee_screens main_form.rs)
void EEUIFMainGame::SetEntityGroup(long inGroup)
{
    EEUIFMGFormCall theCall(this);
    ee_screens_main_form_entity_group(&theCall, inGroup);
}

// (BuildUnit and the start modes StartGate, StartWall, StartCalamity, StartFlare, StartPatrol: inline in
// ee/EEUIFMainGame.h)

// Switches the interface mode, clearing ghosts and ending a wall or gate placement

#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EECombatGoals.h"
#include "rust/sim/units.h"

static_assert(EE_SCREENS_MF_SOUND_BUTTON1 == kSoundButton1 && EE_SCREENS_MF_SOUND_BUTTON2 == kSoundButton2 &&
                  EE_SCREENS_MF_IDS_FPS == IDS_FPS && EE_SCREENS_MF_IDS_GAME_PAUSED == IDS_GAME_PAUSED,
              "main_form.rs' sounds and strings");

// ---- the mouse modes' ops (ee_screens mouse_modes.rs, EE_SCREENS_MF_OP_MD_*): the picked unit, the first selected
// unit and the pointed tile held in the call; io.f[0..2] the mouse location where an op takes one
static_assert(EE_SCREENS_MF_TYPE_FARM == kUnitTypeFarm && EE_SCREENS_MF_TYPE_ROBOTIC_FARM == kUnitTypeRoboticFarm &&
                  EE_SCREENS_MF_KEY_ESCAPE == kKeyEscape && EE_SCREENS_MF_KEY_F1 == kKeyF1 && EE_SCREENS_MF_KEY_F3 == kKeyF3 &&
                  EE_SCREENS_MF_KEY_F9 == kKeyF9,
              "mouse_modes.rs' farms and keys");
// ---- (the mouse modes' ops end)

// The cursor's reads for one UpdateCursorKind (ee_screens cursor.rs asks them through ee_screenscpp_cursor)
struct EEUIFMGCursorRead {
    EEUIFMainGame* mForm;
    EEPlayer* mPlayer;
    EESelectedGroup* mGroup;
    EEComplexUnit* mFirst;
    EEUnit* mTarget;
};
static_assert(sizeof(EEUIFMGCursorRead) == 5 * sizeof(void*), "hud_direct.rs CursorRead");

// The world's answers to the cursor's ops (EE_SCREENS_CURSOR_OP_*): the target, the selection by index, an air
// base's cargo by index; the positions' floats into outF
#define CR(x) static_cast<EEUIFMGCursorRead*>(reinterpret_cast<void*>(x))
static bool sCurShielded;   // EECanConvert's in-out shielded flag (a local of the original case)
extern "C" uintptr_t ee_screenscpp_cur_mouse_over(uintptr_t inCtx)
{ return (uintptr_t)CR(inCtx)->mForm->GetMouseOverControl(); }
extern "C" uintptr_t ee_screenscpp_cur_command_units(uintptr_t inCtx)
{ return (uintptr_t)CR(inCtx)->mForm->GetControl(kMainGameCommandUnits); }
extern "C" void ee_screenscpp_cur_pick(uintptr_t inCtx, uint32_t inFlags)
{ CR(inCtx)->mTarget = gUnitPicker.PickUnit(gMouse->mPosition, inFlags); }
extern "C" bool ee_screenscpp_cur_has_target(uintptr_t inCtx) { return CR(inCtx)->mTarget != NULL; }
extern "C" bool ee_screenscpp_cur_t_on_tile(uintptr_t inCtx) { return CR(inCtx)->mTarget->mTileNode != NULL; }
extern "C" bool ee_screenscpp_cur_t_disabled(uintptr_t inCtx) { return CR(inCtx)->mTarget->mComplexUnit->mIsDisabled; }
extern "C" bool ee_screenscpp_cur_t_protected(uintptr_t inCtx)
{ return CR(inCtx)->mTarget->mComplexUnit->IsProtected(); }
extern "C" bool ee_screenscpp_cur_t_has_complex(uintptr_t inCtx)
{ return CR(inCtx)->mTarget->GetComplexUnit() != NULL; }
extern "C" bool ee_screenscpp_cur_t_has_model(uintptr_t inCtx)
{ return CR(inCtx)->mTarget->GetModel()->mModel != NULL; }
extern "C" float ee_screenscpp_cur_t_scale(uintptr_t inCtx) { return CR(inCtx)->mTarget->mStats->GetScale(); }
extern "C" void ee_screenscpp_cur_t_default_point(uintptr_t inCtx, float* inPoint)
{ *reinterpret_cast<GE3DPoint*>(inPoint) = CR(inCtx)->mTarget->GetModel()->mDefaultPoint; }
extern "C" void ee_screenscpp_cur_t_place(uintptr_t inCtx, float inX, float inY, float inZ, float* inAim)
{ CR(inCtx)->mTarget->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(GE3DPoint(inX, inY, inZ), *reinterpret_cast<GE3DPoint*>(inAim)); }
extern "C" int32_t ee_screenscpp_cur_t_tile_x(uintptr_t inCtx) { return (int32_t)CR(inCtx)->mTarget->GetTileX(); }
extern "C" int32_t ee_screenscpp_cur_t_tile_y(uintptr_t inCtx) { return (int32_t)CR(inCtx)->mTarget->GetTileY(); }
extern "C" float ee_screenscpp_cur_t_tile_height(uintptr_t inCtx) { return CR(inCtx)->mTarget->mTile->GetHeight(); }
extern "C" bool ee_screenscpp_cur_can_gather(uintptr_t inCtx, uint32_t inFlags)
{ return CR(inCtx)->mGroup->FindUnitWithFlags(inFlags)->CanGather(CR(inCtx)->mTarget, CR(inCtx)->mTarget->GetTileX(), CR(inCtx)->mTarget->GetTileY(), false, false); }
extern "C" void ee_screenscpp_cur_set_shielded(bool inShielded) { sCurShielded = inShielded; }
extern "C" bool ee_screenscpp_cur_convert(uintptr_t inCtx, uint32_t inIndex)
{ return EECanConvert(CR(inCtx)->mGroup->GetUnit(inIndex), CR(inCtx)->mTarget->mComplexUnit, sCurShielded); }
extern "C" bool ee_screenscpp_cur_shielded() { return sCurShielded; }
#define theRead (*static_cast<EEUIFMGCursorRead*>(inContext))
#define theTarget theRead.mTarget
#define theUnit theRead.mGroup->GetUnit(inA)
#define theCargo theRead.mGroup->GetUnit(inA)->GetComplexUnit()->mCargo[inB]
extern "C" int32_t ee_screenscpp_cursor(void* inContext, uint32_t inOp, uint32_t inA, uint32_t inB, float* outF)
{
    switch (inOp) {
    case EE_SCREENS_CURSOR_OP_FLAGS:
        return (int32_t)theRead.mGroup->mKindFlags;
    case EE_SCREENS_CURSOR_OP_IN_VIEW:
        return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(gMouse->mPosition);
    case EE_SCREENS_CURSOR_OP_RELEASE:
        theTarget->Dereference();
        return 0;
    case EE_SCREENS_CURSOR_OP_SEEN:
        return EECombatIsSeenBy(theTarget, theRead.mPlayer, true);
    case EE_SCREENS_CURSOR_OP_T_TARGETABLE:
        return ee_units_uu_targetable(0, (uint64_t)(uintptr_t)theTarget->mUnitClass, 0);
    case EE_SCREENS_CURSOR_OP_CAN_HEAL:
        return EECanHeal(theRead.mFirst, theTarget->mComplexUnit);
    case EE_SCREENS_CURSOR_OP_SEL_AIR_BASE:
        return theUnit->GetUnitClass()->IsAirBase();
    case EE_SCREENS_CURSOR_OP_SEL_CRUSADER:
        return theUnit->mUnitClass->IsCrusader();
    case EE_SCREENS_CURSOR_OP_SEL_PRIEST:
        return theUnit->mUnitClass->IsPriest();
    case EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK:
        return theUnit->GetComplexUnit()->CanAttackClass(theTarget->GetUnitClass());
    case EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK:
        return theCargo->CanAttackClass(theTarget->GetUnitClass());
    }
    return 0;
}
#undef theRead
#undef theTarget
#undef theUnit
#undef theCargo

// Picks the mouse cursor for the unit under it from what the selection can do to it (ee_screens cursor.rs, with
// GetAttackStance, GetTransportCommand, GetRepairCommand, GetCommandOnUnit, Fn265D7A and EEGetAttackRangeKind)
void EEUIFMainGame::UpdateCursorKind()
{
    EEUIFMGCursorRead theRead;
    theRead.mForm = this;
    theRead.mPlayer = gWorld.GetCurrentPlayer();
    theRead.mGroup = theRead.mPlayer->mSelectedGroup;
    theRead.mFirst = theRead.mGroup->GetFirstUnit();
    theRead.mTarget = NULL;
    gMouse->mPointer = ee_screens_cursor_kind(&theRead);
}

// the wall's and the gate's members from mState / mUnitClass on as mouse_modes.rs' records
static_assert(offsetof(EEUIBuildWall, mPlayer) - offsetof(EEUIBuildWall, mState) == offsetof(EeScreensBuildWallFields, player) &&
                  offsetof(EEUIBuildWall, mEnd) - offsetof(EEUIBuildWall, mState) == offsetof(EeScreensBuildWallFields, end) &&
                  offsetof(EEUIBuildGate, mPlayer) - offsetof(EEUIBuildGate, mUnitClass) == offsetof(EeScreensBuildGateFields, player) &&
                  offsetof(EEUIBuildGate, mTileX) - offsetof(EEUIBuildGate, mUnitClass) == offsetof(EeScreensBuildGateFields, tile_x),
              "the placements' records");

// Tracks the mouse in wall placement (ee_screens mouse_modes.rs wall_update): a left click sets the start, then
// builds each segment and chains from its end; a right click cancels; moving previews the wall ghosts
void EEUIBuildWall::Update(const GE2DPoint& inLocation)
{
    EEUIFMGFormCall theCall(gUIManager->GetMainGameForm());
    theCall.mWall = this;
    ee_screens_wall_update(&theCall, reinterpret_cast<EeScreensBuildWallFields*>(&this->mState), inLocation.GetX(),
                           inLocation.GetY());
}

// Tracks the mouse in gate placement (ee_screens mouse_modes.rs gate_update): a gate ghost on the wall under the
// mouse, built on a left click, cancelled on a right click
void EEUIBuildGate::Update(const GE2DPoint& inLocation)
{
    EEUIFMGFormCall theCall(gUIManager->GetMainGameForm());
    theCall.mGate = this;
    ee_screens_gate_update(&theCall, reinterpret_cast<EeScreensBuildGateFields*>(&this->mUnitClass), inLocation.GetX(),
                           inLocation.GetY());
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens main_form_calls.rs, game_cpp.rs), each one
// C++ statement on the host's call record or the world
static EEUIFMGFormCall* R(uintptr_t inRec) { return reinterpret_cast<EEUIFMGFormCall*>(inRec); }
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
// the keyboard's key (NULL when the keyboard has none)
static ISEntity* EEKey(unsigned long inKey)
{
    ISEntity* theKey = NULL;
    return gISKeyboard->GetEntity(inKey, theKey) != 0 ? NULL : theKey;
}
// shims5
extern "C" void ee_screenscpp_mf_set_ui_state(uintptr_t inRec, int32_t inState)
{ R(inRec)->mForm->SetUIState(inState); }
extern "C" bool ee_screenscpp_mf_has_wall(uintptr_t inRec) { return R(inRec)->mWall != NULL; }
extern "C" void ee_screenscpp_mf_wall_cancel(uintptr_t inRec) { R(inRec)->mWall->Cancel(); }
extern "C" void ee_screenscpp_mf_gate_cancel(uintptr_t inRec) { R(inRec)->mGate->Cancel(); }
extern "C" bool ee_screenscpp_mouse_left_just_pressed() { return UIManager::GetLMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_left_down() { return UIManager::GetLMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_left_just_unpressed() { return UIManager::GetLMouseButton()->JustUnpressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_pressed() { return UIManager::GetRMouseButton()->JustPressed() != 0; }
extern "C" bool ee_screenscpp_mouse_right_down() { return UIManager::GetRMouseButton()->GetValue() != 0; }
extern "C" bool ee_screenscpp_mouse_right_just_unpressed()
{ return UIManager::GetRMouseButton()->JustUnpressed() != 0; }
extern "C" int32_t ee_screenscpp_key_shift_value() { return (int32_t)EEKey(kKeyShift)->GetValue(); }
extern "C" bool ee_screenscpp_key_exists(uint32_t inKey) { return EEKey(inKey) != NULL; }
extern "C" bool ee_screenscpp_key_value(uint32_t inKey) { return EEKey(inKey)->GetValue() != 0; }
extern "C" bool ee_screenscpp_key_just_pressed(uint32_t inKey) { return EEKey(inKey)->JustPressed() != 0; }
extern "C" bool ee_screenscpp_rasterizer_view_contains(float inX, float inY)
{ return gRasterizer->GetViewport(gScreenIndex).GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_picked_owned_by_current(uintptr_t inRec)
{ return gWorld.GetCurrentPlayer() == R(inRec)->mPicked->mPlayer; }
extern "C" bool ee_screenscpp_picked_owner_hostile(uintptr_t inRec)
{ return R(inRec)->mPicked->mPlayer->mEnemies[gWorld.GetCurrentPlayer()->mPlayerID] != 0; }
extern "C" bool ee_screenscpp_picked_seen_by_current(uintptr_t inRec)
{ return EECombatIsSeenBy(static_cast<EEComplexUnit*>(R(inRec)->mPicked), gWorld.GetCurrentPlayer(), true); }
extern "C" bool ee_screenscpp_mf_first_from_group(uintptr_t inRec)
{ return (R(inRec)->mFirst = gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit()) != NULL; }
extern "C" void ee_screenscpp_mf_first_clear(uintptr_t inRec) { R(inRec)->mFirst = NULL; }
extern "C" bool ee_screenscpp_mf_first_selected(uintptr_t inRec)
{ return EEGetFirstSelectedUnit(R(inRec)->mFirst) == true; }
extern "C" bool ee_screenscpp_mf_has_first(uintptr_t inRec) { return R(inRec)->mFirst != NULL; }
extern "C" float ee_screenscpp_first_distance_squared(uintptr_t inRec, float inX, float inY, float inZ)
{ return R(inRec)->mFirst->mComplexUnit->DistanceSquared3D(GE3DPoint(inX, inY, inZ)); }
extern "C" float ee_screenscpp_first_min_range(uintptr_t inRec) { return R(inRec)->mFirst->GetUnitClass()->mMinRange; }
extern "C" void ee_screenscpp_player_add_event(int64_t inX, int64_t inY, int32_t inKind)
{ gWorld.GetCurrentPlayer()->mEvents.AddEvent(U2DPoint<long>((long)inX, (long)inY), 2, gWorld.GetCurrentPlayer(), inKind, false); }
extern "C" uint32_t ee_screenscpp_ghost_count() { return (uint32_t)gWorld.GetNumGhostUnits(); }
extern "C" bool ee_screenscpp_ghost_can_place(uint32_t inGhost)
{ return static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mCanPlace; }
extern "C" uint32_t ee_screenscpp_ghost_type(uint32_t inGhost)
{ return (uint32_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mUnitClass->mUnitType; }
extern "C" int64_t ee_screenscpp_ghost_tile_x(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mTileX; }
extern "C" int64_t ee_screenscpp_ghost_tile_y(uint32_t inGhost)
{ return (int64_t)static_cast<EEGhostUnit*>(gWorld.mGhostUnits[inGhost])->mTileY; }
extern "C" void ee_screenscpp_build_set_queued(uintptr_t inRec, bool inQueued)
{ R(inRec)->mForm->mBuild->mIsQueued = inQueued; }
extern "C" void ee_screenscpp_build_at(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mBuild->Build(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_camera_clear_unit_a() { gWorld.mCameraControl.ClearUnitA(); }
extern "C" void ee_screenscpp_camera_clear_unit_b() { gWorld.mCameraControl.ClearUnitB(); }
extern "C" void ee_screenscpp_hack_update(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->Update(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_point(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->Fn157269(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_hack_place(uintptr_t inRec, float inX, float inY)
{ R(inRec)->mForm->mUnitHack->PlaceUnit(GE2DPoint(inX, inY)); }
extern "C" float ee_screenscpp_mf_mouse_x(uintptr_t inRec) { return R(inRec)->mForm->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mf_mouse_y(uintptr_t inRec) { return R(inRec)->mForm->GetMouseLocation().GetY(); }
extern "C" int64_t ee_screenscpp_mf_mouse_mode(uintptr_t inRec)
{ return (int64_t)R(inRec)->mForm->mMouseModes[R(inRec)->mForm->mUIState]; }
extern "C" bool ee_screenscpp_cinematic_ending() { return gWorld.GetScenarioManager()->mIsEndingCinematic; }
extern "C" void ee_screenscpp_post_cinematic_screen_shot() { gUserInterface->PostScreenShot(-1, true, false); }
extern "C" uintptr_t ee_screenscpp_end_cinematic_event_new()
{ return (uintptr_t)static_cast<UIEvent*>(new EEUIEndCinematicModeEvent); }
extern "C" void ee_screenscpp_mf_start_selection(uintptr_t inRec)
{ static_cast<EEUICSelectUnits*>(R(inRec)->mForm->GetControl(kMainGameSelectUnits))->StartSelection(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_start_command(uintptr_t inRec)
{ static_cast<EEUICCommandUnits*>(R(inRec)->mForm->GetControl(kMainGameCommandUnits))->StartCommand(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_wall_build(uintptr_t inRec) { R(inRec)->mWall->Build(); }
extern "C" void ee_screenscpp_mf_gate_build(uintptr_t inRec, float inX, float inY, int32_t inDirection)
{ R(inRec)->mGate->Build(GE2DPoint(inX, inY), (long)inDirection); }
extern "C" void ee_screenscpp_mf_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, R(inRec)->mGate->mUnitClass, gWorld.GetPlayer(R(inRec)->mGate->mPlayer)); }
extern "C" void ee_screenscpp_gate_preview(float inX, float inY, long* inDirection)
{ EEGhostUnit::PreviewGate(GE2DPoint(inX, inY), *inDirection); }
extern "C" void ee_screenscpp_mf_wall_update(uintptr_t inRec)
{ R(inRec)->mForm->mBuildWall.Update(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_gate_update(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.Update(R(inRec)->mForm->GetMouseLocation()); }
extern "C" void ee_screenscpp_mf_error_lock_enter(uintptr_t inRec) { R(inRec)->mForm->mErrorCriticalSection->Enter(); }
extern "C" void ee_screenscpp_mf_error_lock_leave(uintptr_t inRec) { R(inRec)->mForm->mErrorCriticalSection->Leave(); }
extern "C" void ee_screenscpp_mf_group_reset(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Reset(); }
extern "C" void ee_screenscpp_mf_group_refresh(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Refresh(); }
extern "C" void ee_screenscpp_mf_group_select(uintptr_t inRec)
{ R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->Select(); }
extern "C" bool ee_screenscpp_mf_group_wants_refresh(uintptr_t inRec)
{ return R(inRec)->mForm->mGroups[R(inRec)->mForm->mCurrentGroup]->WantsRefresh() == true; }
extern "C" uintptr_t ee_screenscpp_mf_pick(uintptr_t inRec)
{ return (uintptr_t)(R(inRec)->mPicked = gUnitPicker.PickUnit(gMouse->mPosition, 0)); }
extern "C" bool ee_screenscpp_mf_picked_is_wall(uintptr_t inRec) { return R(inRec)->mPicked->mUnitClass->mIsWall; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_x(uintptr_t inRec) { return (int64_t)R(inRec)->mPicked->mTileX; }
extern "C" int64_t ee_screenscpp_mf_picked_tile_y(uintptr_t inRec) { return (int64_t)R(inRec)->mPicked->mTileY; }
extern "C" void ee_screenscpp_mouse_tile(long* inX, long* inY)
{ EEPointUnitOrTileXY(gMouse->mPosition, sShimTile, *inX, *inY, true); }
extern "C" void ee_screenscpp_mf_picked_dereference(uintptr_t inRec) { R(inRec)->mPicked->Dereference(); }
extern "C" void ee_screenscpp_mf_picked_clear(uintptr_t inRec) { R(inRec)->mPicked = NULL; }
extern "C" void ee_screenscpp_mf_form_gate_create_ghost(uintptr_t inRec)
{ EEGhostUnit::CreateGate(0.0f, R(inRec)->mForm->mBuildGate.mUnitClass, gWorld.mPlayers[R(inRec)->mForm->mBuildGate.mPlayer]); }
extern "C" void ee_screenscpp_gate_preview_at_mouse(long* inDirection)
{ EEGhostUnit::PreviewGate(gMouse->mPosition, *inDirection); }
extern "C" float ee_screenscpp_mf_wall_start_x(uintptr_t inRec) { return R(inRec)->mForm->mBuildWall.mStart.GetX(); }
extern "C" float ee_screenscpp_mf_wall_start_y(uintptr_t inRec) { return R(inRec)->mForm->mBuildWall.mStart.GetY(); }
extern "C" uintptr_t ee_screenscpp_team_wonder_countdown(uint32_t inTeam, unsigned long* inValue)
{ return (uintptr_t)gWorld.mTeams[inTeam].mVictoryCondition->GetWonderCountdown(*inValue); }
extern "C" void ee_screenscpp_mf_info_set_text(uintptr_t inRec)
{ static_cast<UITLScrolling*>(R(inRec)->mForm->GetControl(kMainGameDialogMessage))->SetText(R(inRec)->mMessage->mText); }
extern "C" uintptr_t ee_screenscpp_mf_control(uintptr_t inRec, uint32_t inControl)
{ return (uintptr_t)R(inRec)->mForm->GetControl(inControl); }
extern "C" uint32_t ee_screenscpp_mf_info_unit_type(uintptr_t inRec) { return (uint32_t)R(inRec)->mMessage->mUnitType; }
extern "C" uint32_t ee_screenscpp_unit_class_count() { return (uint32_t)EEUnitClassCount(); }
extern "C" bool ee_screenscpp_unit_class_exists(uint32_t inKind) { return gUnitClassTable[inKind] != NULL; }
extern "C" bool ee_screenscpp_player_class_has_button(uint32_t inKind)
{ return gWorld.GetCurrentPlayer()->mUnitClassUpgrades[inKind]->mButton != NULL; }
extern "C" void ee_screenscpp_mf_info_set_face(uintptr_t inRec, uintptr_t inBitmap, uintptr_t inPortrait)
{ gWorld.GetCurrentPlayer()->mUnitClassUpgrades[R(inRec)->mMessage->mUnitType]->mButton->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inPortrait)->GetRectangle(), C(inPortrait)->GetScreenIndex()); }
extern "C" void ee_screenscpp_ui_bitmap_set_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ static_cast<UIBitmap*>(C(inControl))->SetBitmap(reinterpret_cast<GEBitmap*>(inBitmap)); }
extern "C" void ee_screenscpp_control_refresh_opacity(uintptr_t inControl)
{ C(inControl)->SetBgOpacity(C(inControl)->GetBgOpacity()); }
extern "C" void ee_screenscpp_mf_info_load_sound(uintptr_t inRec, unsigned long* inSample, unsigned long* inLength)
{ EELoadSound("Sounds\\" + UString((const char*)R(inRec)->mMessage->mSound), 0x31, *inSample, *inLength, 1); }
extern "C" void ee_screenscpp_mf_poll_new(uintptr_t inRec, uint32_t inMilliseconds)
{ R(inRec)->mForm->mPoll = new TMPoll((unsigned long)inMilliseconds, 0, false); }
extern "C" void ee_screenscpp_mf_poll_start(uintptr_t inRec) { R(inRec)->mForm->mPoll->Start(); }
extern "C" uint32_t ee_screenscpp_game_size_width() { return (uint32_t)gGameWindowWidth; }
extern "C" uint32_t ee_screenscpp_game_size_height() { return (uint32_t)gGameWindowHeight; }
extern "C" void ee_screenscpp_mf_scroll_new(uintptr_t inRec) { R(inRec)->mForm->mScroll = new EEAScroll; }
extern "C" void ee_screenscpp_mf_scroll_set_edges(uintptr_t inRec, float inLeft, float inTop, float inRight, float inBottom)
{ R(inRec)->mForm->mScroll->mEdges = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mf_mouse_click_new(uintptr_t inRec) { R(inRec)->mForm->mMouseClick = new EEAMouseClick; }
extern "C" void ee_screenscpp_mf_lasso_new(uintptr_t inRec) { R(inRec)->mForm->mLasso = new EEALasso; }
extern "C" void ee_screenscpp_mf_build_new(uintptr_t inRec) { R(inRec)->mForm->mBuild = new EEABuild; }
extern "C" void ee_screenscpp_mf_calamity_new(uintptr_t inRec) { R(inRec)->mForm->mCalamity = new EEACalamity; }
extern "C" void ee_screenscpp_mf_wheel_zoom_new(uintptr_t inRec, uint32_t inCode)
{ R(inRec)->mForm->mWheelZoom = EECreateScreenEvent((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_wheel_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseWheelMoveEvent(R(inRec)->mForm->mWheelZoom); }
extern "C" void ee_screenscpp_mf_grab_scroll_new(uintptr_t inRec, uint32_t inCode)
{ R(inRec)->mForm->mGrabScroll = EECreateGrabScroll((unsigned long)inCode); }
extern "C" void ee_screenscpp_mf_set_middle_pressed_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseMiddleButtonPressedEvent(R(inRec)->mForm->mGrabScroll); }
extern "C" void ee_screenscpp_mf_set_middle_unpressed_event(uintptr_t inRec)
{ R(inRec)->mForm->SetMouseMiddleButtonUnpressedEvent(R(inRec)->mForm->mGrabScroll); }
extern "C" void ee_screenscpp_mf_add_hot_key(uintptr_t inRec, uint32_t inKey, uint32_t inModifier1, uint32_t inModifier2, uint32_t inModifier3, uintptr_t inEvent1, uintptr_t inEvent2, uintptr_t inEvent3, uintptr_t inEvent4, uint32_t inId, bool inFlag)
{ R(inRec)->mForm->AddHotKey(inKey, inModifier1, inModifier2, inModifier3, reinterpret_cast<UIEvent*>(inEvent1), reinterpret_cast<UIEvent*>(inEvent2), reinterpret_cast<UIEvent*>(inEvent3), reinterpret_cast<UIEvent*>(inEvent4), (unsigned long)inId, inFlag); }
extern "C" void ee_screenscpp_mf_unit_load(uintptr_t inRec, uint32_t inUnit)
{ R(inRec)->mUnit = static_cast<EEUnit*>(gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inUnit)); }
extern "C" uintptr_t ee_screenscpp_mf_unit(uintptr_t inRec) { return (uintptr_t)R(inRec)->mUnit; }
extern "C" void ee_screenscpp_mf_ghost_of_node(uintptr_t inRec)
{ EEGhostUnit::Create(R(inRec)->mForm->GetMouseLocation(), R(inRec)->mNode->mData->mUnitClass, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_ghost_of_calamity(uintptr_t inRec)
{ EEGhostUnit::Create(R(inRec)->mForm->GetMouseLocation(), R(inRec)->mCalamity->mUnitClassIndex, gWorld.GetCurrentPlayer(), 0.0f, false); }
extern "C" void ee_screenscpp_mf_gate_set_technology(uintptr_t inRec, int32_t inTechnology)
{ R(inRec)->mForm->mBuildGate.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_gate_wall_type_from_node(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.mWallType = R(inRec)->mNode->GetUnitClass(0); }
extern "C" void ee_screenscpp_mf_gate_class_from_node(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.SetUnitClass(gUnitClassTable[R(inRec)->mNode->mData->mUnitClass]); }
extern "C" void ee_screenscpp_mf_gate_initialize(uintptr_t inRec)
{ R(inRec)->mForm->mBuildGate.Initialize(gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" void ee_screenscpp_mf_wall_set_technology(uintptr_t inRec, int32_t inTechnology)
{ R(inRec)->mForm->mBuildWall.mTechnology = (long)inTechnology; }
extern "C" void ee_screenscpp_mf_wall_initialize(uintptr_t inRec)
{ R(inRec)->mForm->mBuildWall.Initialize(gWorld.GetCurrentPlayer()->mPlayerID); }
extern "C" uint32_t ee_screenscpp_selected_count()
{ return (uint32_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" void ee_screenscpp_mf_caster_first(uintptr_t inRec)
{ R(inRec)->mCaster = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0); }
extern "C" void ee_screenscpp_mf_caster_clear(uintptr_t inRec) { R(inRec)->mCaster = NULL; }
extern "C" bool ee_screenscpp_mf_has_caster(uintptr_t inRec) { return R(inRec)->mCaster != NULL; }
extern "C" void ee_screenscpp_mf_calamity_set(uintptr_t inRec, int32_t inCalamity)
{ R(inRec)->mForm->mCalamity->SetCalamity((long)inCalamity, R(inRec)->mCalamity); }
extern "C" void ee_screenscpp_mf_calamity_set_caster(uintptr_t inRec)
{ R(inRec)->mForm->mCalamity->SetCasterID(R(inRec)->mCaster->mUnitID); }
extern "C" int64_t ee_screenscpp_mf_calamity_target_kind(uintptr_t inRec)
{ return (int64_t)R(inRec)->mCalamity->GetTargetKind(); }
extern "C" void ee_screenscpp_mouse_shift_off() { gMouse->mShiftMode = false; }
extern "C" void ee_screenscpp_mouse_set_pointer(int32_t inPointer) { gMouse->mPointer = (long)inPointer; }
extern "C" void ee_screenscpp_mouse_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ gMouse->SetBounds(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_server_unpause(uintptr_t inServer)
{ reinterpret_cast<EEServer*>(inServer)->UnpauseGame(); }
extern "C" uintptr_t ee_screenscpp_graphics() { return (uintptr_t)gGraphics; }
extern "C" void ee_screenscpp_gfx_clear_render_objects() { gGraphics->PostClearRenderObjects(); }
extern "C" void ee_screenscpp_mf_cancel_button_up(uintptr_t inRec, uintptr_t inEvent)
{ R(inRec)->mForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_cancel_right_button_up(uintptr_t inRec, uintptr_t inEvent)
{ R(inRec)->mForm->GetControl(kMainGameCancelUIMode)->SetTriggerEvent(kControlTriggerRightButtonUp, reinterpret_cast<UIEvent*>(inEvent)); }
extern "C" void ee_screenscpp_mf_main_group_open(uintptr_t inRec)
{ static_cast<EEUIFMGMainGroup*>(R(inRec)->mForm->mGroups[0])->Open(R(inRec)->mForm); }
extern "C" void ee_screenscpp_mf_main_group_clear_l20(uintptr_t inRec)
{ static_cast<EEUIFMGMainGroup*>(R(inRec)->mForm->mGroups[0])->mL20 = 0; }
extern "C" void ee_screenscpp_post_graphics_all() { EEPostGraphicsAll(); }
extern "C" void ee_screenscpp_gfx_fade_in(uint32_t inValue) { gGraphics->PostFadeIn(inValue); }
extern "C" void ee_screenscpp_mf_media_load(uintptr_t inRec) { R(inRec)->mMedia->LoadMedia(); }
extern "C" void ee_screenscpp_mf_old_media_unload(uintptr_t inRec) { R(inRec)->mOldMedia->UnloadMedia(); }
extern "C" void ee_screenscpp_shell_op(uintptr_t inControl, uint32_t inOp) { EEUIShell::Op(C(inControl), inOp); }
extern "C" uintptr_t ee_screenscpp_mf_background(uintptr_t inRec, uint32_t inId)
{ return (uintptr_t)R(inRec)->mForm->GetBackgroundByID((unsigned long)inId); }
extern "C" void ee_screenscpp_background_draw(uintptr_t inBackground)
{ reinterpret_cast<UIBackground*>(inBackground)->DrawBackground(reinterpret_cast<UIBackground*>(inBackground)->GetRectangle()); }
extern "C" float ee_screenscpp_mf_fps(uintptr_t inRec)
{ return gRasterizer->GetFramesPerSecond(R(inRec)->mForm->GetManager()->GetScreen()); }
// end shims5

// ---- the rest of the original host (EEUIFMainGame::HostOp, the mouse modes' EEMouseModeOp; UI lane round 9:
// main_form_calls.rs), each one statement on the call record, the form or the world, and the cases' locals
#define MF(inRec) R(inRec)->mForm
static EETileCoordinate sMFTile;                // FIND_GATE_WALL's tile, PREVIEW_WALL's tiles
static std::vector<EETileCoordinate> sMFTiles;
static UWideString sMFFormat;                   // TEXT_WONDER's format
static GE3DPoint sMFPoint;                      // MD_POINT's point
static bool sMFInRange;                         // MD_CAN_CONVERT's in-range flag
extern "C" int64_t ee_screenscpp_mf_gate_direction(uintptr_t inRec)
{ return (int64_t)MF(inRec)->mBuildGate.mDirection; }
extern "C" void ee_screenscpp_mf_set_gate_direction(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildGate.mDirection = (decltype(MF(inRec)->mBuildGate.mDirection))inValue; }
extern "C" int64_t ee_screenscpp_mf_wall_state(uintptr_t inRec) { return (int64_t)MF(inRec)->mBuildWall.mState; }
extern "C" void ee_screenscpp_mf_set_wall_state(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuildWall.mState = (decltype(MF(inRec)->mBuildWall.mState))inValue; }
extern "C" bool ee_screenscpp_mf_tooltip_shadow(uintptr_t inRec) { return MF(inRec)->GetDrawToolTipShadow(); }
extern "C" void ee_screenscpp_mf_set_tooltip_shadow(uintptr_t inRec, bool inValue)
{ MF(inRec)->SetDrawToolTipShadow(inValue); }
extern "C" int64_t ee_screenscpp_mf_build_tech(uintptr_t inRec) { return (int64_t)MF(inRec)->mBuild->mTechnology; }
extern "C" void ee_screenscpp_mf_set_build_tech(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->mBuild->mTechnology = (decltype(MF(inRec)->mBuild->mTechnology))inValue; }
extern "C" void ee_screenscpp_mf_log_line(uintptr_t inRec, int64_t inIndex)
{ static_cast<EEUIShellList*>(MF(inRec)->GetControl(kMainGameErrorLog))->SetLine((unsigned long)inIndex, MF(inRec)->mStrings[inIndex]); }
extern "C" int64_t ee_screenscpp_mf_poll(uintptr_t inRec) { return (int64_t)MF(inRec)->mPoll->Poll(); }
extern "C" void ee_screenscpp_mf_scroll(uintptr_t inRec) { MF(inRec)->mScroll->Update(gMouse->mPosition); }
extern "C" void ee_screenscpp_mf_tile_x(int64_t inX) { sMFTile.mX = (long)inX; }
extern "C" void ee_screenscpp_mf_tile_y(int64_t inY) { sMFTile.mY = (long)inY; }
extern "C" bool ee_screenscpp_mf_find_gate_wall(long* inDirection) { return EEFindGateWall(sMFTile, *inDirection); }
extern "C" void ee_screenscpp_mf_clear_ghosts() { gWorld.ClearGhostUnits(); }
extern "C" void ee_screenscpp_mf_tiles_clear() { sMFTiles.clear(); }
extern "C" void ee_screenscpp_mf_tiles_add() { sMFTiles.push_back(sMFTile); }
extern "C" void ee_screenscpp_mf_preview_wall_call(uintptr_t inRec)
{ EEPreviewWall(sMFTiles, R(inRec)->mWall->mTechnology, R(inRec)->mWall->mPlayer); }
extern "C" void ee_screenscpp_mf_preview_wall_form(uintptr_t inRec)
{ EEPreviewWall(sMFTiles, MF(inRec)->mBuildWall.mTechnology, MF(inRec)->mBuildWall.mPlayer); }
extern "C" void ee_screenscpp_mf_preview_placement_mouse() { EEPreviewPlacement(gMouse->mPosition); }
extern "C" int64_t ee_screenscpp_mf_population()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetTotalPopulation(false, false); }
extern "C" int64_t ee_screenscpp_mf_population_limit()
{ return (int64_t)gWorld.GetCurrentPlayer()->GetPopulationLimit(false); }
extern "C" int64_t ee_screenscpp_mf_num_teams() { return (int64_t)gWorld.GetNumTeams(); }
extern "C" void ee_screenscpp_mf_label_player_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UITextLabel*>(MF(inRec)->GetControl((unsigned long)inControl))->GetGEText()->SetFgColor(reinterpret_cast<EEPlayer*>(inPlayer)->mColor); }
extern "C" void ee_screenscpp_mf_wonder_format()
{ gResource->GetString(IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY, sMFFormat); }
extern "C" void ee_screenscpp_mf_wonder_print(uintptr_t inRec, int64_t inPlayer, int64_t inTime)
{ sprintf(R(inRec)->mText, sMFFormat, (const char*)reinterpret_cast<EEPlayer*>(inPlayer)->mName, (unsigned long)inTime); }
extern "C" void ee_screenscpp_mf_text_time(uintptr_t inRec) { R(inRec)->mText = gPhysics->GetTimeString(); }
extern "C" bool ee_screenscpp_mf_comms_paused() { return gCommunications->GetIsGamePaused(); }
extern "C" bool ee_screenscpp_mf_has_teams() { return gWorld.mTeams.size() > 0; }
extern "C" void ee_screenscpp_mf_pause_text(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_PAUSE_TEXT, inValue); }
extern "C" int64_t ee_screenscpp_mf_lag_status() { return (int64_t)gWorld.mLagStatus; }
extern "C" void ee_screenscpp_mf_lag_color(uintptr_t inRec, int64_t inControl, int64_t inPlayer)
{ static_cast<UIBitmap*>(MF(inRec)->GetControl((unsigned long)inControl))->GetBitmap()->SetColor(gWorld.mPlayers[inPlayer]->mColor); }
extern "C" bool ee_screenscpp_mf_has_instructions()
{ return gWorld.mScenarioManager->mMediaStrings.GetSection(3).GetLength() > 0; }
extern "C" void ee_screenscpp_mf_update_cursor(uintptr_t inRec) { MF(inRec)->UpdateCursorKind(); }
extern "C" int64_t ee_screenscpp_mf_info_duration(uintptr_t inRec) { return (int64_t)R(inRec)->mMessage->mDuration; }
extern "C" void ee_screenscpp_mf_chat_top(uintptr_t inRec, int64_t inValue)
{ MF(inRec)->Rust(EE_SCREENS_MF_DO_CHAT_TOP, inValue); }
extern "C" void ee_screenscpp_mf_hide_messages(uintptr_t inRec) { MF(inRec)->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); }
extern "C" void ee_screenscpp_mf_create_error_lock(uintptr_t inRec)
{ ESVERIFY_AT(TSScheduler::CreateCriticalSection(MF(inRec)->mErrorCriticalSection) == keseOK, 633); }
extern "C" void ee_screenscpp_mf_create_group(uintptr_t inRec, int64_t inIndex)
{ MF(inRec)->CreateGroup((unsigned long)inIndex); }
extern "C" void ee_screenscpp_mf_new_unit_hack(uintptr_t inRec)
{ MF(inRec)->mUnitHack = new EEUIFMGUnitHack(MF(inRec)); }
extern "C" void ee_screenscpp_mf_wall_terminate(uintptr_t inRec)
{ ((EEUIBuildWall&)MF(inRec)->mBuildWall).Terminate(); }
extern "C" void ee_screenscpp_mf_gate_terminate(uintptr_t inRec)
{ ((EEUIBuildGate&)MF(inRec)->mBuildGate).Terminate(); }
extern "C" void ee_screenscpp_mf_node_load(uintptr_t inRec, int64_t inTechnology)
{ R(inRec)->mNode = gWorld.GetCurrentPlayer()->mTechnologies->Get((long)inTechnology); }
extern "C" bool ee_screenscpp_mf_has_node(uintptr_t inRec) { return R(inRec)->mNode != NULL; }
extern "C" bool ee_screenscpp_mf_node_class_exists(uintptr_t inRec)
{ return gUnitClassTable[R(inRec)->mNode->mData->mUnitClass] != NULL; }
extern "C" int64_t ee_screenscpp_mf_can_produce(uintptr_t inRec, int64_t inTechnology)
{ return (int64_t)gWorld.GetCurrentPlayer()->CanProduce((long)inTechnology, R(inRec)->mUnit->mComplexUnit->mUnitClass->GetUnitType(), 1, true, 3, &R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_play_sound(int64_t inSound)
{ gSoundManager->Play(EESoundObject::Get((unsigned long)inSound)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" void ee_screenscpp_mf_chat_log_error(uintptr_t inRec)
{ gUserInterface->AddChatLogEntry(R(inRec)->mText, 0); }
extern "C" int64_t ee_screenscpp_mf_selected_count()
{ return (int64_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits(); }
extern "C" int64_t ee_screenscpp_mf_node_calamity(uintptr_t inRec)
{ return (int64_t)R(inRec)->mNode->mData->mCalamity; }
extern "C" void ee_screenscpp_mf_calamity_load(uintptr_t inRec, int64_t inCalamity)
{ R(inRec)->mCalamity = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)inCalamity); }
extern "C" bool ee_screenscpp_mf_has_calamity(uintptr_t inRec) { return R(inRec)->mCalamity != NULL; }
extern "C" void ee_screenscpp_mf_calamity_cast(uintptr_t inRec)
{ MF(inRec)->mCalamity->Cast(MF(inRec)->GetMouseLocation()); }
extern "C" bool ee_screenscpp_mf_user_paused() { return gWorld.mUserPaused; }
extern "C" void ee_screenscpp_mf_master_volume() { gSoundManager->SetMasterVolume(gMasterVolume); }
extern "C" void ee_screenscpp_mf_mouse_position(float inX, float inY) { gMouse->SetPosition(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mf_fill_hack(uintptr_t inRec) { MF(inRec)->mUnitHack->FillComboBox(); }
extern "C" bool ee_screenscpp_mf_has_techs() { return gWorld.GetCurrentPlayer()->mTechnologies != NULL; }
extern "C" void ee_screenscpp_mf_text_epoch_title(uintptr_t inRec)
{ gResource->GetString(EEEpochTitleID(gWorld.GetCurrentPlayer()->mTechnologies->mCurrentEpoch), R(inRec)->mText); }
extern "C" void ee_screenscpp_mf_unit_info_background(uintptr_t inRec)
{ MF(inRec)->mUnitInfoBackground = MF(inRec)->GetBackgroundByID(0x64); }
extern "C" void ee_screenscpp_mf_media_old_load(uintptr_t inRec)
{ R(inRec)->mOldMedia = EEMedia::Get(MF(inRec)->mMediaID); }
extern "C" bool ee_screenscpp_mf_has_media_old(uintptr_t inRec) { return R(inRec)->mOldMedia != NULL; }
extern "C" int64_t ee_screenscpp_mf_player_media_id()
{ return (int64_t)gWorld.GetCurrentPlayer()->mTechnologies->mMediaID; }
extern "C" bool ee_screenscpp_mf_using_carbon() { return gWorld.mUsingCarbon; }
extern "C" void ee_screenscpp_mf_media_get(uintptr_t inRec, int64_t inMedia)
{ R(inRec)->mMedia = EEMedia::Get((long)inMedia); }
extern "C" bool ee_screenscpp_mf_has_media(uintptr_t inRec) { return R(inRec)->mMedia != NULL; }
extern "C" void ee_screenscpp_mf_bg_texture(uintptr_t inRec, int64_t inBackground, int64_t inTexture)
{ static_cast<UIBitmapBackground*>(MF(inRec)->GetBackgroundByID((unsigned long)inBackground))->SetBitmapTexture(static_cast<EEGraphicObject*>(R(inRec)->mMedia)->mTextures[inTexture]); }
extern "C" bool ee_screenscpp_md_captured(uintptr_t inRec) { return MF(inRec)->GetIsMouseCaptured(); }
extern "C" void ee_screenscpp_md_point_reset() { sMFPoint = GE3DPoint(0.0f, 0.0f, 0.0f); }
extern "C" bool ee_screenscpp_md_point(uintptr_t inRec, float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), R(inRec)->mTile, sMFPoint, true); }
extern "C" float ee_screenscpp_md_point_x() { return sMFPoint.GetX(); }
extern "C" float ee_screenscpp_md_point_y() { return sMFPoint.GetY(); }
extern "C" float ee_screenscpp_md_point_z() { return sMFPoint.GetZ(); }
extern "C" bool ee_screenscpp_md_point_xy(uintptr_t inRec, float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), R(inRec)->mTile, *inTileX, *inTileY, true); }
extern "C" bool ee_screenscpp_md_tile_seen(uintptr_t inRec)
{ return R(inRec)->mTile->IsSeenBy(gWorld.GetCurrentPlayer()); }
extern "C" bool ee_screenscpp_md_pick(uintptr_t inRec, float inX, float inY)
{ return (R(inRec)->mPicked = gUnitPicker.PickUnit(GE2DPoint(inX, inY), 0)) != NULL; }
extern "C" bool ee_screenscpp_md_can_be_attacked(uintptr_t inRec)
{ return R(inRec)->mPicked->GetUnitClass()->CanBeAttacked(); }
extern "C" bool ee_screenscpp_md_has_picked(uintptr_t inRec) { return R(inRec)->mPicked != NULL; }
extern "C" bool ee_screenscpp_md_in_wonder_set(uintptr_t inRec)
{ return gUnitSetManager.Contains(R(inRec)->mPicked->mComplexUnit->GetUnitClass()->GetUnitType(), R(inRec)->mFirst->mStats->mWonderUnitSet); }
extern "C" float ee_screenscpp_md_damage(int64_t inA, int64_t inB) { return EEDamageFactor((long)inA, (long)inB); }
extern "C" bool ee_screenscpp_md_is_caster(uintptr_t inRec)
{ return R(inRec)->mFirst->mUnitID == MF(inRec)->mCalamity->mCasterID; }
extern "C" void ee_screenscpp_md_in_range_set(bool inValue) { sMFInRange = inValue; }
extern "C" bool ee_screenscpp_md_can_convert(uintptr_t inRec, int64_t inUnit)
{ return EECanConvert(gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit((unsigned long)inUnit), R(inRec)->mPicked->GetComplexUnit(), sMFInRange); }
extern "C" bool ee_screenscpp_md_in_range() { return sMFInRange; }
extern "C" void ee_screenscpp_md_send_load_garrison(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadGarrison, {R(inRec)->mPicked->GetUnitID(), R(inRec)->mFirst->mComplexUnit->mUnitID, 0}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_unload(uintptr_t inRec, int64_t inX, int64_t inY)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageUnloadTransport, {R(inRec)->mFirst->GetComplexUnit()->GetUnitID(), (long)inX, (long)inY}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_heal(uintptr_t inRec)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageHealUnit, {R(inRec)->mPicked->mComplexUnit->mUnitID}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_convert(uintptr_t inRec, bool inShielded)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageAttackUnit, {R(inRec)->mPicked->mComplexUnit->mUnitID, inShielded, true}), kSlotNone); }
extern "C" void ee_screenscpp_md_send_rally(uintptr_t inRec, float inX, float inY, int64_t inKind, int64_t inFlag)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSetRallyGoal, {GE2DPoint(inX, inY), (long)inKind, R(inRec)->mFirst->mComplexUnit->GetUnitID(), (unsigned char)inFlag}), kSlotNone); }
extern "C" void ee_screenscpp_md_order_at_none(float inX, float inY) { EEOrderAtCursor(GE2DPoint(inX, inY), NULL); }
extern "C" void ee_screenscpp_md_order_at_picked(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), R(inRec)->mPicked); }
extern "C" void ee_screenscpp_md_order_at_complex(uintptr_t inRec, float inX, float inY)
{ EEOrderAtCursor(GE2DPoint(inX, inY), static_cast<EEUnit*>(R(inRec)->mPicked->mComplexUnit)); }
extern "C" void ee_screenscpp_md_calamity_effect(uintptr_t inRec, float inX, float inY, float inZ)
{ EEMakeCalamityEffect(GE3DPoint(inX, inY, inZ), R(inRec)->mPicked); }
extern "C" void ee_screenscpp_md_preview(uintptr_t inRec) { EEPreviewPlacement(MF(inRec)->GetMouseLocation()); }
extern "C" int64_t ee_screenscpp_md_calamity_check(uintptr_t inRec, float inX, float inY)
{ return (int64_t)MF(inRec)->mCalamity->Check(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_in_game_view(uintptr_t inRec, float inX, float inY)
{ return MF(inRec)->IsInGameView(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_md_entity_enabled(uintptr_t inRec, int64_t inGroup)
{ return MF(inRec)->GetEntityGroup((unsigned long)inGroup)->GetIsEnabled(); }
extern "C" void ee_screenscpp_md_set_entity_group(uintptr_t inRec, int64_t inGroup)
{ MF(inRec)->SetEntityGroup((long)inGroup); }
extern "C" void ee_screenscpp_md_gate_clear(uintptr_t inRec) { R(inRec)->mGate->ClearGhostUnits(); }

// the destructor's, CreateGroup's and IsInGameView's bodies: hud_bodies.rs over these (UI lane round 12)
#define MFB(f) ((EEUIFMainGame*)(f))
extern "C" void ee_screenscpp_mfb_group_main(uintptr_t inForm)
{ MFB(inForm)->mGroups[0] = new EEUIFMGMainGroup(MFB(inForm)); }
extern "C" void ee_screenscpp_mfb_group_rust(uintptr_t inForm, uint32_t inIndex)
{ MFB(inForm)->mGroups[inIndex] = new EEUIFMGGRust(MFB(inForm), inIndex); }
extern "C" uintptr_t ee_screenscpp_mfb_unit_info(uintptr_t inForm)
{ return (uintptr_t)MFB(inForm)->mUnitInfoBackground; }
extern "C" bool ee_screenscpp_mfb_background_visible(uintptr_t inBackground)
{ return ((UIBackground*)inBackground)->GetIsVisible(); }
extern "C" bool ee_screenscpp_mfb_background_contains(uintptr_t inBackground, float inX, float inY)
{ return ((UIBackground*)inBackground)->GetRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" bool ee_screenscpp_mfb_view_contains(float inX, float inY)
{ return gViewport.GetViewRectangle().Contains(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mfb_grab_scroll_dereference(uintptr_t inForm) { MFB(inForm)->mGrabScroll->Dereference(); }
extern "C" void ee_screenscpp_mfb_release_grab_scroll() { EEReleaseGrabScroll(0); }
extern "C" void ee_screenscpp_mfb_wheel_zoom_dereference(uintptr_t inForm) { MFB(inForm)->mWheelZoom->Dereference(); }
extern "C" void ee_screenscpp_mfb_scroll_dereference(uintptr_t inForm) { MFB(inForm)->mScroll->Dereference(); }
extern "C" void ee_screenscpp_mfb_mouse_click_dereference(uintptr_t inForm) { MFB(inForm)->mMouseClick->Dereference(); }
extern "C" void ee_screenscpp_mfb_lasso_dereference(uintptr_t inForm) { MFB(inForm)->mLasso->Dereference(); }
extern "C" void ee_screenscpp_mfb_destroy_error_section(uintptr_t inForm)
{ TSScheduler::DestroyCriticalSection(MFB(inForm)->mErrorCriticalSection); }
extern "C" void ee_screenscpp_mfb_group_delete(uintptr_t inForm, uint32_t inIndex)
{ delete MFB(inForm)->mGroups[inIndex]; }
extern "C" void ee_screenscpp_mfb_poll_delete(uintptr_t inForm) { delete MFB(inForm)->mPoll; }
extern "C" void ee_screenscpp_mfb_build_dereference(uintptr_t inForm) { MFB(inForm)->mBuild->Dereference(); }
extern "C" void ee_screenscpp_mfb_calamity_dereference(uintptr_t inForm) { MFB(inForm)->mCalamity->Dereference(); }
extern "C" void ee_screenscpp_mfb_unit_hack_delete(uintptr_t inForm) { delete MFB(inForm)->mUnitHack; }
