// TEST ONLY: the host of the scripted scenario editor (EE-SIM.exe --editor-script). Port addition, no original
// address. The script's runner, its language, its steps and its digests are Rust (ee_game sim_driver editor.rs,
// sed_steps.rs, sed_digest.rs); this file runs a step on the UI thread and answers the steps' C++ calls and members
// (ee_simcpp_sed*: one case per call, EeSimDriverSed). A step is an action of its own in the UI thread's action
// queue (so under the UI copy lock and the world's write lock, as every editor action runs) that runs the step's
// code, then a marker that posts itself again three times (an action posted by an action, such as the blank map's
// terrain paint, runs before it) and wakes the driver. While a step runs, TMGetTime answers a pinned time on the UI
// thread (2000, 3000, ... by step).
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETexture.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITriStateCheckBox.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UIBitmap.h"
#include "lle/FSChunk.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMArea.h"
#include "ee/EESMCondition.h"
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEPhysics.h"
#include "ee/EESaveLoad.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEARust.h"
#include "ee/EESEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EEMouse.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUIFSETerrain.h"
#include "ee/EEUIShell.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EESERBUnitTypes.h"
#include "ee/EESELoadThread.h"
#include "ee/EEUnitHandle.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationBuilder.h"
#include "rust/base/time.h"
#include "rust/game/sim_driver.h"   // EeSimDriverSed

// the alignments the condition and effect types gave their labels (EESETSEffect.cpp, the types' host)
extern std::map<const void*, unsigned long> gSeetAlignments;
long EESETestSaveTriggersOnly(UIForm* inForm);   // EEUIFScenarioEditor.cpp (TEST ONLY)
// EESaveLoad.cpp: the save's chunks (the map, the scenario, the players, the world, ...), in their order
void BuildSaveGameChunks(std::vector<FSPersistent*>& outList);

// the plain layouts the Rust reads (GERectangle, GEColor, GETransformation)
static_assert(sizeof(GERectangle) == 16 && sizeof(GEColor) == 12 && sizeof(GETransformation) == 64, "plain layouts");

// ------------------------------------------------------------------------------------------------ the steps

namespace {

// A step's action and end (sim_driver sed_run.rs: the steps, the marker's reposts, the end's rounds and the driver's
// wait; inState is the run's state)
class EESimEditorAction : public EEAction {
public:
    explicit EESimEditorAction(void* inState) : EEAction(false), mState(inState) {}
    virtual void Virtual4() { ee_sim_driver_sed_action(this, this->mState); }
    virtual void Virtual8() {}
    void* mState;
};

class EESimEditorDone : public UIEvent {
public:
    explicit EESimEditorDone(void* inState) : mState(inState) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_sim_driver_sed_done(this->mState); }
    void* mState;
};

// the protected virtuals a person's mouse reaches: a button's release, a combo box's chosen line, an edit box's Enter
struct ButtonAccess : UIButton {
    static void Up(UIButton* inButton) { (inButton->*(&ButtonAccess::ButtonUp))(); }
};
struct ComboAccess : UIComboBox {
    static void Change(UIComboBox* inBox) { (inBox->*(&ComboAccess::ChangeSelection))(); }
};
struct EditAccess : UIEditBox {
    static void Enter(UIEditBox* inBox) { (inBox->*(&EditAccess::ProcessEnterKey))(); }
};
struct CivListAccess : UILBString {
    static unsigned long Index(const UILBString* inList) { return (inList->*(&CivListAccess::GetSelectionIndex))(); }
    static bool Auto(const UILBString* inList) { return (inList->*(&CivListAccess::GetAutoSelect))(); }
    static bool Copied(const UILBString* inList) { return (inList->*(&CivListAccess::GetCopiedHasSelection))(); }
    static unsigned long Drawn(const UILBString* inList) { return (inList->*(&CivListAccess::GetDrawCount))(); }
};

std::string sText;                          // the last text answered (the UI thread's, while a step runs)

}   // namespace


#define P64(x) ((int64_t)(intptr_t)(x))
#define ED ((EEUIFScenarioEditor*)p)
#define CTL ((UIControl*)p)
#define LBS static_cast<UILBString*>(CTL)
#define CMB static_cast<UIComboBox*>(CTL)
#define TTL static_cast<EESEUITerrainTypeLB*>(CTL)
#define TCL static_cast<EESEUITerrainColorLB*>(CTL)
#define ACT ((EEAction*)p)
#define RUST static_cast<EEARust*>(ACT)
#define SM ((EEScenarioManager*)p)
#define TR ((EESMTrigger*)p)
#define EF ((EESMEffect*)p)
#define CO ((EESMCondition*)p)
#define AR ((EESMArea*)p)
#define PL ((EEPlayer*)p)
#define CP ((EEComputerPlayer*)p)
#define UN ((EEUnit*)p)
#define GH ((EEGhostUnit*)p)
#define SP ((FSFileSpec*)p)
#define MF ((FSMemoryFile*)p)
#define CH ((std::vector<FSPersistent*>*)p)
#define NM ((std::vector<UWideString>*)p)
#define SU ((EESEUnit*)p)
#define UF ((UIForm*)p)
#define CL static_cast<EECBUIListBox*>(CTL)
#define CV ((EECivilization*)p)
#define UPG ((std::vector<EECivUpgrade>*)p)

// The steps' C++ calls and members (sed_host.rs EeSimDriverSed): p the object, a and f the arguments, s a text
extern "C" int64_t ee_simcpp_sed(EeSimDriverSed inOp, uintptr_t p, const int64_t* a, const float* f, const char* s)
{
    typedef EeSimDriverSed O;
    switch (inOp) {
    case O::FormSlot: return (int64_t)ED->GetSlot((uint32_t)a[0]);
    case O::ControlById: return P64(ED->GetControlById((unsigned long)a[0]));
    case O::FormControl: return P64(ED->GetControl((unsigned long)a[0]));
    case O::FormControlCount: return ED->GetControlCount();
    case O::FormGroupCount: return ED->GetControlGroupCount();
    case O::FormGroup: return P64(ED->GetEntityGroup((unsigned long)a[0]));
    case O::SelectedCount: return ED->GetSelectedUnitCount();
    case O::SelectedUnit: return P64(ED->GetSelectedUnit((unsigned long)a[0]));
    case O::SelectedUnitId: return ED->GetSelectedUnit((unsigned long)a[0])->GetUnitID();
    case O::FormSetLoadText: ED->SetLoadText(UWideString(s));
        break;
    case O::FormDeselectFirst: ED->DeselectFirstUnit();
        break;
    case O::FormClearSelection: ED->ClearSelection();
        break;
    case O::FormSelectUnit: ED->SelectUnit((EEUnit*)a[0]);
        break;
    case O::FormDeselectUnit: ED->DeselectUnit((EEUnit*)a[0]);
        break;
    case O::FormRefreshGroup: ED->RefreshCurrentGroup();
        break;
    case O::FormResumeGroup: ED->ResumeCurrentGroup();
        break;
    case O::FormSetGameView: ED->SetGameView(GERectangle(f[0], f[1], f[2], f[3]), a[0] != 0);
        break;
    case O::SaveTriggersOnly: return EESETestSaveTriggersOnly(static_cast<UIForm*>(ED));
    case O::TriggerStoring: return gB4C3220;
    case O::GroupIsEnabled: return ((UIEntityGroup*)p)->GetIsEnabled();
    case O::GroupEntityCount: return ((UIEntityGroup*)p)->GetEntityCount();
    case O::GroupEntity: return P64(static_cast<UIControl*>(((UIEntityGroup*)p)->GetEntity((unsigned long)a[0])));
    // the controls
    case O::ShellOp: return EEUIShell::Op(CTL, (uint32_t)a[0], a[1], a[2]);
    case O::ShellOf: return P64(EEUIShell::Of(CTL));
    case O::DisplayMode: return CTL->GetDisplayMode();
    case O::ControlId: return CTL->GetId();
    case O::ControlIndex: return CTL->GetIndex();
    case O::ControlType: return CTL->GetType();
    case O::ControlRect: return P64(&CTL->GetRectangle());
    case O::ControlBgColor: CTL->GetBgColor(*(GEColor*)a[0]);
        break;
    case O::ControlBorder: return CTL->GetBorderThickness();
    case O::ControlStringTableId: return CTL->GetStringTableID();
    case O::TriggerEvent: return P64(CTL->GetTriggerEvent((unsigned long)a[0]));
    case O::ProcessEvent: return ((UIEvent*)p)->ProcessEvent(gUIManager->GetScenarioEditorForm(), (UIControl*)a[0]);
    case O::IsRadio: return dynamic_cast<UIRadioButton*>(CTL) != NULL;
    case O::IsCheck: return dynamic_cast<UICheckBox*>(CTL) != NULL;
    case O::IsButton: return dynamic_cast<UIButton*>(CTL) != NULL;
    case O::IsCombo: return dynamic_cast<UIComboBox*>(CTL) != NULL;
    case O::IsList: return dynamic_cast<UILBString*>(CTL) != NULL;
    case O::IsEdit: return dynamic_cast<UIEditBox*>(CTL) != NULL;
    case O::IsSlider: return dynamic_cast<UISlider*>(CTL) != NULL;
    case O::IsTextLabel: return dynamic_cast<UITextLabel*>(CTL) != NULL;
    case O::RadioIsChecked: return static_cast<UIRadioButton*>(CTL)->GetIsChecked();
    case O::RadioTextLabel: return P64(static_cast<UIRadioButton*>(CTL)->GetTextLabel());
    case O::RadioCheck: static_cast<UIRadioButton*>(CTL)->Check();
        break;
    case O::CheckIsChecked: return static_cast<UICheckBox*>(CTL)->GetIsChecked();
    case O::CheckTextLabel: return P64(static_cast<UICheckBox*>(CTL)->GetTextLabel());
    case O::CheckSetChecked: static_cast<UICheckBox*>(CTL)->SetChecked(a[0] != 0);
        break;
    case O::TriStateCheckState: return static_cast<UITriStateCheckBox*>(CTL)->GetCheckState();
    case O::ButtonGeText: return P64(static_cast<UIButton*>(CTL)->GetGEText());
    case O::ButtonUp: ButtonAccess::Up(static_cast<UIButton*>(CTL));
        break;
    case O::ComboList: return P64(CMB->GetListBox());
    case O::ComboSelection: return CMB->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::ComboSetSelection: CMB->SetCurrentSelection((unsigned long)a[0]);
        break;
    case O::ComboTextBoxType: return CMB->GetTextBoxType();
    case O::ComboTextLabel: return P64(CMB->GetTextLabel());
    case O::ComboChange: ComboAccess::Change(CMB);
        break;
    case O::FileComboSelect: return EEUIFileComboSelect(CTL, UWideString(s));
    case O::ListLineCount: return LBS->GetLineCount();
    case O::ListSelection: return LBS->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::ListChangeSelection: LBS->ChangeSelectionIndex((unsigned long)a[0], a[1] != 0, a[2] != 0);
        break;
    case O::ListCopyData: LBS->CopyData();
        break;
    case O::ListGeText: return P64(LBS->GetGEText());
    case O::EditGeText: return P64(static_cast<UIEditBox*>(CTL)->GetGEText());
    case O::EditLineCount: return static_cast<UIEditBox*>(CTL)->GetLineCount();
    case O::EditSetText: static_cast<UIEditBox*>(CTL)->SetText(UWideString(s));
        break;
    case O::EditSetNumber: static_cast<UIEditBox*>(CTL)->SetText((unsigned long)a[0]);
        break;
    case O::EditEnter: EditAccess::Enter(static_cast<UIEditBox*>(CTL));
        break;
    case O::SliderValue: return static_cast<UISlider*>(CTL)->GetCurrentRangeValue();
    case O::SliderMax: return static_cast<UISlider*>(CTL)->GetMaxRange();
    case O::SliderSetValue: static_cast<UISlider*>(CTL)->SetCurrentRangeValue((long)a[0]);
        break;
    case O::LabelGeText: return P64(static_cast<UITextLabel*>(CTL)->GetGEText());
    case O::LabelLineCount: return static_cast<UITextLabel*>(CTL)->GetLineCount();
    case O::TextFgColor: return P64(&((GEText*)p)->GetFgColor());
    case O::TextIsTransparent: return ((GEText*)p)->GetIsTransparent();
    case O::TextFont: return P64(((GEText*)p)->GetFont());
    case O::FontHeight: return ((GEFont*)p)->GetHeight();
    case O::BitmapGeBitmap: return P64(static_cast<UIBitmap*>(CTL)->GetBitmap());
    case O::GeBitmapCorners:
        ((GEBitmap*)p)->GetColorPerVertex(((GEColor*)a[0])[0], ((GEColor*)a[0])[1], ((GEColor*)a[0])[2], ((GEColor*)a[0])[3]);
        break;
    case O::GeBitmapTexture: return P64(((GEBitmap*)p)->GetTexture(0));
    case O::TerrainTypeCount: return TTL->GetLineCount();
    case O::TerrainTypeSelection: return TTL->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::TerrainTypeTexture: return P64(TTL->GetData((unsigned long)a[0]).mTexture);
    case O::TerrainTypeColorSet: return TTL->GetData((unsigned long)a[0]).mColorSet;
    case O::TerrainTypePick: TTL->ChangeSelectionIndex((unsigned long)a[0], true, false);
        break;
    case O::TerrainColorCount: return TCL->GetLineCount();
    case O::TerrainColorSelection: return TCL->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::TerrainColorSetSize: return TCL->GetData((unsigned long)a[0]).size();
    case O::TerrainColorSetFirst: return P64(&TCL->GetData((unsigned long)a[0])[0]);
    case O::TerrainColorPick: TCL->ChangeSelectionIndex((unsigned long)a[0], true, false);
        break;
    case O::AlignmentHas: return gSeetAlignments.count((const void*)p);
    case O::AlignmentGet: return gSeetAlignments[(const void*)p];
    case O::BrushRect: return P64(&((EEUIFSEBrushGroup*)p)->mRectangle);
    case O::BrushHighlight: return ((EEUIFSEBrushGroup*)p)->mIsHighlightShown;
    case O::BrushCentered: return ((EEUIFSEBrushGroup*)p)->mIsCentered;
    case O::BrushSize: return ((EEUIFSEBrushGroup*)p)->mBrushSize;
    case O::BrushSetHighlight: ((EEUIFSEBrushGroup*)p)->SetHighlight(f[0], f[1], a[0] != 0);
        break;
    case O::EdgeScrollEdges: return P64(&((EEAScroll*)p)->mEdges);
    case O::LoadThreadPostDone: return ((EESELoadThread*)p)->mThread->Post(EE_SCREENS_SE_LOAD_THREAD_MESSAGE_QUEUE_DONE, NULL, NULL, true);
    case O::LoadThreadWaitDone: ((EESELoadThread*)p)->mMessageQueueDone->Wait(kTSWaitInfinite);
        break;
    case O::LoadThreadClearDone: ((EESELoadThread*)p)->mMessageQueueDone->Clear();
        break;
    case O::UnitWallStart: SU->mBuildWall.mStart = {f[0], f[1]};
        break;
    case O::UnitWallEnd: SU->mBuildWall.mEnd = {f[0], f[1]};
        break;
    case O::UnitWallBuild: ee_screens_wall_build(&SU->mBuildWall);
        break;
    case O::UnitWallCancel: ee_screens_wall_cancel(&SU->mBuildWall);
        break;
    case O::UnitGateTileX: SU->mBuildGate.mTileX = (long)a[0];
        break;
    case O::UnitGateTileY: SU->mBuildGate.mTileY = (long)a[0];
        break;
    case O::UnitGateDirection: SU->mBuildGate.mDirection = (long)a[0];
        break;
    case O::UnitGateBuild: ee_screens_gate_build(&SU->mBuildGate, f[0], f[1], (int32_t)a[0]);
        break;
    case O::UnitGateCancel: ee_screens_gate_cancel(&SU->mBuildGate);
        break;
    // the actions and messages
    case O::C12ActionNew: return P64(static_cast<EEAction*>(new EESimEditorAction((void*)p)));
    case O::C12ActionPost: gUserInterface->PostAction((EEAction*)p);
        break;
    case O::C12DoneNew: return P64(static_cast<UIEvent*>(new EESimEditorDone((void*)p)));
    case O::C12DonePost: gUserInterface->PostEvent((UIEvent*)p, NULL);
        break;
    case O::C12Dereference: a[0] ? ((UIEvent*)p)->Dereference() : ((EEAction*)p)->Dereference();
        break;
    case O::SeaNew:
        return P64(static_cast<EEAction*>(
            new EESEARust((uint32_t)a[0], (void*)p, (int32_t)a[1], (int32_t)a[2], (int32_t)a[3], (int32_t)a[4])));
    case O::SeaAction: return P64(static_cast<EESEARust*>(ACT)->mAction);
    case O::PaintNew: return P64(static_cast<EEAction*>(new EESEAPaintRust((uint32_t)a[0], (int32_t)a[1], (int32_t)a[2])));
    case O::PaintSetArea: return static_cast<EESEAPaint*>(ACT)->SetArea((long)a[0], (long)a[1], (long)a[2], (long)a[3]);
    case O::PostAction: gUserInterface->PostAction(ACT);
        break;
    case O::DerefAction: ACT->Dereference();
        break;
    case O::ClickNew: return P64(static_cast<EEAction*>(new EESEAMouseClick));
    case O::TsClickNew: return P64(static_cast<EEAction*>(new EESETSAMouseClick));
    case O::ClickSetMessage: RUST->SetMessage((EEMessage*)a[0]);
        break;
    case O::ClickExecute: RUST->Execute(f[0], f[1], false, 0, a[0] != 0, 0.0f, false);
        break;
    case O::LassoNew: return P64(static_cast<EEAction*>(new EESEALasso));
    case O::LassoSetSelect: RUST->mMessage = EENewSelectUnits(GERectangle(f[0], f[1], f[2], f[3]), (long)a[0], (unsigned char)a[1]);
        break;
    case O::LassoSelectInRectangle: RUST->SelectInRectangle(GERectangle(f[0], f[1], f[2], f[3]), 0);
        break;
    case O::QuickSaveNew: return P64(static_cast<EEAction*>(new EESEAQuickSaveLoad(UString(s))));
    case O::MRecord0: return P64(static_cast<EEMessage*>(new EEMRecord((unsigned char)a[0])));
    case O::MRecord1: return P64(static_cast<EEMessage*>(new EEMRecord((unsigned char)a[0], {(unsigned long)a[1]})));
    case O::MouseSetPosition: gMouse->mPosition = GE2DPoint(f[0], f[1]);
        break;
    // the world
    case O::MapMesh: return P64(gWorld.mMap != NULL ? gWorld.mMap->mTerrainMesh.GetRust() : NULL);
    case O::MapTile: return P64(gWorld.mMap->RTSMap::GetTile((long)a[0], (long)a[1]));
    case O::MapGetTile: return P64(gWorld.GetMap()->GetTile((long)a[0], (long)a[1]));
    case O::NodeUnitPlayer: return P64(UN->GetPlayer());
    case O::NodeUnitId: return UN->GetUnitID();
    case O::PlayerId: return PL->mPlayerID;
    case O::TerrainColorType: return EETerrainTable::Get((long)a[0])->mColorType;
    case O::ViewportRect: return P64(&gViewport.GetViewRectangle());
    case O::WorldGetNumPlayers: return gWorld.GetNumPlayers();
    case O::WorldMaxPlayers: return gMaxPlayers;
    case O::WorldPopulationLimit: return gWorld.mPopulationLimit;
    case O::WorldWondersToWin: return gWorld.mNumberOfWondersToWin;
    case O::WorldVictoryEnabled: return gWorld.mVictoryEnabled;
    case O::WorldTeamsLocked: return gWorld.mTeamsLocked;
    case O::WorldCheatsEnabled: return gWorld.mCheatsEnabled;
    case O::WorldImprovementsAllowed: return gWorld.mUnitImprovementsAllowed;
    case O::WorldUsingCarbon: return gWorld.mUsingCarbon;
    case O::OptionsUnitLimit: return gWorld.mGameOptions.mUnitLimit;
    case O::OptionsWondersForVictory: return gWorld.mGameOptions.mWondersForVictory;
    case O::OptionsVictory: return gWorld.mGameOptions.mVictory;
    case O::OptionsLockTeams: return gWorld.mGameOptions.mLockTeams;
    case O::OptionsCheatCodes: return gWorld.mGameOptions.mCheatCodes;
    case O::OptionsPlayerColor: return gWorld.mGameOptions.mPlayers[a[0]].mColor;
    case O::OptionsPlayerB36: return gWorld.mGameOptions.mPlayers[a[0]].mB36;
    case O::PlayerColor: return P64(&PL->mColor);
    case O::PlayerResourceCap: return PL->mResourceCaps[a[0]];
    case O::PlayerEnemy: return (long)PL->mEnemies[a[0]];
    case O::PlayerSharedLos: return PL->mSharedLOS;
    case O::PlayerPurchasedUpgrades: return PL->mCivilization.mPurchasedUpgrades.size();
    case O::TreeCurrentEpoch: return (long)((EETechTree*)p)->mCurrentEpoch;
    case O::TreeStartEpoch: return (long)((EETechTree*)p)->mStartEpoch;
    case O::TreeEndEpoch: return (long)((EETechTree*)p)->mEndEpoch;
    case O::ComputerActive: return CP->mActive;
    case O::ComputerIsCheating: return CP->mIsCheating;
    case O::ComputerCanAttack: return CP->mCanAttack;
    case O::ComputerCanDefend: return CP->mCanDefend;
    case O::ComputerCanBuild: return CP->mCanBuild;
    case O::ComputerCanExplore: return CP->mCanExplore;
    case O::ComputerIgnoreOthers: return CP->mIgnoreOthers;
    case O::ComputerCanGather: return CP->mCanGather;
    case O::UnitId: return UN->GetUnitID();
    case O::UnitType: return (long)UN->mUnitClass->mUnitType;
    case O::UnitTileX: return (long)UN->mTileX;
    case O::UnitTileY: return (long)UN->mTileY;
    case O::UnitIsEditorSelected: return UN->mIsEditorSelected;
    // the ghosts
    case O::GhostsChanged: gGhostUnitsChanged = true;
        break;
    case O::GhostCount: return gWorld.GetNumGhostUnits();
    case O::GhostAt: return P64(gWorld.mGhostUnits[a[0]]);
    case O::GhostCreate: EEGhostUnit::Create(GE2DPoint(f[0], f[1]), (long)a[0], gWorld.mPlayers[a[1]], f[2], false);
        break;
    case O::GhostSetTileX: GH->mTileX = (long)a[0];
        break;
    case O::GhostSetTileY: GH->mTileY = (long)a[0];
        break;
    case O::GhostTileX: return GH->mTileX;
    case O::GhostTileY: return GH->mTileY;
    case O::GhostUnitType: return GH->mUnitClass->GetUnitType();
    case O::GhostTileSize: return GH->mUnitClass->GetTileSize();
    case O::GhostSetFarmSlot: EEGhostUnit::SetFarmSlotTile(GH, (long)a[0]);
        break;
    case O::GhostSetPosition: GH->mTransformation.SetPosition(GE3DPoint(f[0], f[1], f[2]));
        break;
    case O::GhostSetTransformation: GH->mTransformation = *(const GETransformation*)a[0];
        break;
    case O::GhostFillSquare: GH->FillSquare();
        break;
    case O::GhostLocationToTile: GH->mUnitClass->LocationToTile(GE3DPoint(f[0], f[1], f[2]), GH->mTileX, GH->mTileY);
        break;
    case O::GhostCanPlace: return GH->mCanPlace;
    case O::GhostSetCanPlace: GH->mCanPlace = a[0] != 0;
        break;
    // the scenario manager
    case O::SmTriggerCount: return SM->mTriggers.size();
    case O::SmTriggerAt: return P64(SM->mTriggers[a[0]]);
    case O::SmConditionCount: return SM->mConditions.size();
    case O::SmConditionAt: return P64(SM->mConditions[a[0]]);
    case O::SmEffectCount: return SM->mEffects.size();
    case O::SmEffectAt: return P64(SM->mEffects[a[0]]);
    case O::SmPendingCount: return SM->mPendingEffects.size();
    case O::SmPendingAt: return P64(SM->mPendingEffects[a[0]]);
    case O::SmObjectCount: return SM->mObjects.size();
    case O::SmObjectAt: return P64(SM->mObjects[a[0]]);
    case O::SmAreaCount: return SM->mAreas.size();
    case O::SmAreaAt: return P64(SM->mAreas[a[0]]);
    case O::SmTimedCount: return SM->GetNumTimedEffects();
    case O::SmTimedEffect: return SM->GetTimedEntry((unsigned long)a[0]).mEffectIndex;
    case O::SmTimedTime: return SM->GetTimedEntry((unsigned long)a[0]).mTime;
    case O::SmNextTriggerId: return SM->mNextTriggerID;
    case O::SmNextConditionId: return SM->mNextConditionID;
    case O::SmNextEffectId: return SM->mNextEffectID;
    case O::SmNextObjectId: return SM->mNextObjectID;
    case O::SmNextAreaId: return SM->mNextAreaID;
    case O::SmShowDebugIds: return SM->mShowDebugIDs;
    case O::SmSetShowDebugIds: SM->mShowDebugIDs = a[0] != 0;
        break;
    case O::SmGetNames: SM->GetNames((unsigned long)a[0], *(std::vector<UWideString>*)a[1]);
        break;
    case O::NamesNew: return P64(new std::vector<UWideString>());
    case O::NamesCount: return NM->size();
    case O::NamesFree: delete NM;
        break;
    case O::SmFindCondition: return P64(SM->FindConditionByID((long)a[0]));
    case O::SmFindEffect: return P64(SM->FindEffectByID((long)a[0]));
    case O::SmFindObject: return P64(SM->FindObjectByID((long)a[0]));
    case O::SmObjectIndex: return SM->GetObjectIndex((EESMObject*)a[0]);
    case O::SmGetTrigger: return P64(SM->GetTrigger((unsigned long)a[0]));
    case O::SmGetObject: return P64(SM->GetScenarioObject((unsigned long)a[0]));
    case O::SmGetArea: return P64(SM->GetArea((unsigned long)a[0]));
    case O::SmExportText: return SM->ExportText(*(FSFileSpec*)a[0]);
    case O::SmImportText: return SM->ImportTriggerText(*(FSFileSpec*)a[0], (unsigned long)a[1], a[2] != 0);
    case O::SmClearImportErrors: SM->mImportErrors.clear();
        break;
    case O::SmImportErrorCount: return SM->mImportErrors.size();
    case O::SmAddTimedEffect: SM->AddTimedEffect((long)a[0], (unsigned long)a[1], false);
        break;
    case O::SmAddPendingCopy: SM->AddV134Effect(*(EESMEffect*)a[0]);
        break;
    case O::SmNewEntry: SM->NewEntry((unsigned long)a[0]);
        break;
    case O::SmSortList: SM->SortList((unsigned long)a[0]);
        break;
    case O::SmMoveEntry: SM->MoveEntry((unsigned long)a[0], (unsigned long)a[1], a[2] != 0);
        break;
    case O::SmDeleteEntry: return SM->DeleteEntry((unsigned long)a[0], (unsigned long)a[1]);
    case O::SmDuplicateEntry: SM->DuplicateEntry((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TriggerFields: return P64(static_cast<EeScenarioTriggerFields*>(TR));
    case O::TriggerConditionCount: return TR->mConditions.size();
    case O::TriggerConditionAt: return P64(TR->mConditions[a[0]]);
    case O::TriggerEffectCount: return TR->mEffects.size();
    case O::TriggerEffectAt: return P64(TR->mEffects[a[0]]);
    case O::TriggerGetConditionCount: return TR->GetConditionCount();
    case O::TriggerGetConditionAt: return P64(TR->GetConditionAt((unsigned long)a[0]));
    case O::TriggerGetEffectCount: return TR->GetEffectCount();
    case O::TriggerGetEffectAt: return P64(TR->GetEffectAt((unsigned long)a[0]));
    case O::TriggerSetIsOn: TR->SetIsOn(a[0] != 0);
        break;
    case O::TriggerSetIsLooping: TR->SetIsLooping(a[0] != 0);
        break;
    case O::TriggerSetConditionDuration: TR->SetConditionDuration((unsigned long)a[0]);
        break;
    case O::TriggerSetName: TR->SetName(UWideString(s));
        break;
    case O::TriggerSetText: TR->mText = UWideString(s);
        break;
    case O::WrapperValue: return ((EESMTConditionWrapper*)p)->mValue;
    case O::WrapperOperator: return ((EESMTConditionWrapper*)p)->mOperator;
    case O::WrapperIsNot: return ((EESMTConditionWrapper*)p)->mIsNot;
    case O::WrapperDelay: return ((EESMTEffectWrapper*)p)->mDelay;
    case O::WrapperEffectIndex: return ((EESMTEffectWrapper*)p)->mEffectIndex;
    case O::WrapperEffectOperator: return ((EESMTEffectWrapper*)p)->mOperator;
    case O::ConditionFields: return P64(static_cast<EeScenarioConditionFields*>(CO));
    case O::ConditionSetChatText: CO->mChatText = s;
        break;
    case O::EffectFields: return P64(static_cast<EeScenarioEffectFields*>(EF));
    case O::EffectSetDescription: EF->mDescription = s;
        break;
    case O::EffectSetMessage: EF->mMessage = s;
        break;
    case O::EffectSetNewName: EF->mNewName = s;
        break;
    case O::EffectObjectCount: return EF->mObjects.size();
    case O::EffectObjectAt: return EF->mObjects[a[0]];
    case O::EffectClearObjects: EF->mObjects.clear();
        break;
    case O::EffectPushObject: EF->mObjects.push_back((unsigned long)a[0]);
        break;
    case O::ObjectFields: return P64(static_cast<EeScenarioObjectFields*>((EESMObject*)p));
    case O::ObjectUnitCount: return ((EESMObject*)p)->GetUnitCount();
    case O::ObjectUnitAt: return ((EESMObject*)p)->GetUnitAt((unsigned long)a[0]);
    case O::AreaId: return AR->mID;
    case O::AreaSelectMode: return AR->GetSelectMode();
    case O::AreaHasRectangle: return AR->GetHasRectangle();
    case O::AreaRect: return P64(&AR->GetRect());
    case O::AreaHasAnchor: return AR->GetHasAnchor();
    case O::AreaAnchorX: return AR->GetAnchor().GetX();
    case O::AreaAnchorY: return AR->GetAnchor().GetY();
    case O::AreaSetRectangle: AR->SetRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        break;
    case O::AreaSetAnchor: AR->SetAnchor(U2DPoint<long>((long)a[0], (long)a[1]));
        break;
    // the files and the save
    case O::SpecNew: return P64(new FSFileSpec());
    case O::SpecFree: delete SP;
        break;
    case O::SpecSetVolumeStartup: SP->SetVolume(gStartupVolume);
        break;
    case O::SpecSetDirectoryStartup: SP->SetDirectory(gStartupDirectory + s);
        break;
    case O::SpecSetFile: SP->SetFile(UString(s));
        break;
    case O::SpecCreateDirectory: SP->CreateDirectory();
        break;
    case O::SpecCreate: SP->Create();
        break;
    case O::SpecOpenRead: return SP->Open(kFileAccessRead);
    case O::SpecOpenWrite: return SP->Open(kFileAccessWrite);
    case O::SpecFOpen: return SP->FOpen(const_cast<char*>(s));
    case O::SpecClose: SP->Close();
        break;
    case O::SpecWrite: return SP->Write((const unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::SpecGets: return P64(fgets((char*)a[0], (int)a[1], (FILE*)*SP));
    case O::SpecPuts: fputs(s, (FILE*)*SP);
        break;
    case O::MemNew: return P64(new FSMemoryFile((unsigned long)a[0], a[1] != 0, (unsigned long)a[2]));
    case O::MemFree: delete MF;
        break;
    case O::MemLoad: return MF->Load(*(FSFileSpec*)a[0]);
    case O::MemLength: return MF->GetLength(*(unsigned long*)a[0]);
    case O::MemPosition: return MF->GetPosition(*(long*)a[0]);
    case O::MemData: return P64(MF->GetData());
    case O::HeaderNew: return P64(new EEPSaveGameHeader());
    case O::HeaderLoad: return ((EEPSaveGameHeader*)p)->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, *(FSMemoryFile*)a[0]);
    case O::ChunksNew: return P64(new std::vector<FSPersistent*>());
    case O::ChunksBuild: BuildSaveGameChunks(*CH);
        break;
    case O::ChunksCount: return CH->size();
    case O::ChunksAt: return P64((*CH)[a[0]]);
    case O::ChunksFree: delete CH;
        break;
    case O::PersistentTag: return ((FSPersistent*)p)->GetTagID();
    case O::PersistentFree: delete (FSPersistent*)p;
        break;
    case O::ChunkNew: return P64(new FSChunk());
    case O::ChunkFree: delete (FSChunk*)p;
        break;
    case O::ChunkSavePersistent:
        return ((FSChunk*)p)->SavePersistentToChunk(kSaveGameMajorVersion, kSaveGameMinorVersion, (FSPersistent*)a[0]);
    case O::ChunkSaveToFile: return ((FSChunk*)p)->SaveChunkToFile(*(FSMemoryFile*)a[0]);
    case O::ChunkLoadFromFile: return ((FSChunk*)p)->LoadChunkFromFile(*(FSMemoryFile*)a[0]);
    // the civilization builder (sed_civ.rs)
    case O::UiFormById: return P64(gUIManager->GetFormByID((unsigned long)a[0]));
    case O::UiFormControlById: return P64(UF->GetControlById((unsigned long)a[0]));
    case O::UiFormControlCount: return UF->GetControlCount();
    case O::UiFormControl: return P64(UF->GetControl((unsigned long)a[0]));
    case O::UiFormId: return UF->GetFormID();
    case O::UiFormShellOf: return P64(EEUIShell::Of(UF));
    case O::UiFormCount: return gUIManager->GetFormCount();
    case O::UiFormStacked: return P64(gUIManager->GetStackedForm((unsigned long)a[0]));
    case O::UiPop: gUIManager->Pop();
        break;
    case O::IsCivList: return dynamic_cast<EECBUIListBox*>(CTL) != NULL;
    case O::CivListUpgradesOnly: return CL->mSelectUpgradesOnly;
    case O::CivListShowPoints: return CL->mShowPoints;
    case O::CivListSortLines: return CL->mSortLines;
    case O::CivListHeaderColor: return P64(&CL->mC13C);
    case O::CivListReset: CL->ResetSelection();
        break;
    case O::CivListTabbedTo: CL->TabbedTo();
        break;
    case O::CivListCopyData: return CL->CopyData();
    case O::CivListCopiedShowPoints: return CL->mCopiedShowPoints;
    case O::CivListCopiedColor: return P64(&CL->mC148);
    case O::ControlTabable: return CTL->GetTabable();
    case O::ListAutoSelect: return CivListAccess::Auto(LBS);
    case O::ListHasSelection: return LBS->GetHasSelection();
    case O::ListSelectionIndex: return CivListAccess::Index(LBS);
    case O::ListCopiedHasSelection: return CivListAccess::Copied(LBS);
    case O::ListDrawCount: return CivListAccess::Drawn(LBS);
    case O::ListDataEquals: return LBS->GetData((unsigned long)a[0]) == s;
    case O::ProcessEventOn: return ((UIEvent*)p)->ProcessEvent((UIForm*)a[0], (UIControl*)a[1]);
    case O::CivEdit: return CivBuilderEditCivilization();
    case O::CivTimer: return EEProcessTimerExpired();
    case O::CivBlink: return EESetCivBuilderButtonBlinking(a[0] != 0);
    case O::CivAvailablePoints: return CV->mAvailablePoints;
    case O::CivMaxPoints: return CV->mMaxPoints;
    case O::CivUnchanged: return CV->mUnchanged;
    case O::CivNoPointsAdded: return CV->mNoPointsAdded;
    case O::CivPurchasedCount: return CV->mPurchasedUpgrades.size();
    case O::CivPurchasedAt: return (unsigned long)CV->mPurchasedUpgrades[a[0]];
    case O::CivDeferredCount: return CV->mDeferredUpgrades.size();
    case O::CivDeferredAt: return (unsigned long)CV->mDeferredUpgrades[a[0]];
    case O::EditorCivilization: return P64(&ED->mC1A0);
    case O::PlayerCivilization: return P64(&PL->mCivilization);
    case O::PlayerCivilizationChosen: return PL->mCivilizationChosen;
    case O::PlayerCivilizationLocked: return PL->mCivilizationLocked;
    case O::SpecGetLength: return SP->GetLength(*(unsigned long*)a[0]);
    case O::SpecRead: return SP->Read((unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::SpecDelete: SP->Delete();
        break;
    case O::CivSave: return CivBuilderSaveCivilization(UString(s), a[0] != 0);
    case O::CivLoad:
        return EELoadCivilization(UString(s), EEGetCivilizationDirectory(true), *(UWideString*)a[0],
                                  *(std::vector<EECivUpgrade>*)a[1]);
    case O::CivApply: return EEApplyCivilizationFile(*(UString*)a[0], PL);
    case O::WideNew: return P64(new UWideString());
    case O::UpgradesNew: return P64(new std::vector<EECivUpgrade>());
    case O::UpgradesCount: return UPG->size();
    case O::UpgradesAt: return (unsigned long)(*UPG)[a[0]];
    case O::UpgradesFree: delete UPG;
        break;
    case O::NarrowNew: return P64(new UString(s));
    case O::SetFreeCamera: gFreeCamera = a[0] != 0;
        break;
    case O::SetAllowCustomCivs: gAllowCustomCivs = a[0] != 0;
        break;
    case O::SetScenarioGame: gIsScenarioGame = a[0] != 0;
        break;
    default: break;
    }
    return 0;
}

// The steps' texts: copied into sText, valid until the next text
extern "C" const char* ee_simcpp_sed_text(EeSimDriverSed inOp, uintptr_t p, int64_t a, const char* s)
{
    typedef EeSimDriverSed O;
    switch (inOp) {
    case O::ShellKindName: return (sText = ((EEUIShell*)p)->KindName()).c_str();
    case O::ControlTypeName: return (sText = typeid(*CTL).name()).c_str();
    case O::EventTypeName: return (sText = typeid(*(UIEvent*)p).name()).c_str();
    case O::ListData: return (sText = (const char*)LBS->GetData((unsigned long)a)).c_str();
    case O::EditText: return (sText = static_cast<UIEditBox*>(CTL)->GetText((unsigned long)a)).c_str();
    case O::LabelText: return (sText = (const char*)static_cast<UITextLabel*>(CTL)->GetText((unsigned long)a)).c_str();
    case O::TextText: return (sText = (const char*)((GEText*)p)->GetText()).c_str();
    case O::TerrainTypeName: return (sText = (const char*)TTL->GetData((unsigned long)a).mName).c_str();
    case O::PlayerName: return (sText = (const char*)PL->mName).c_str();
    case O::PlayerCivilizationFile: return (sText = (const char*)PL->mCivilizationFile).c_str();
    case O::NamesAt: return (sText = (const char*)(*NM)[a]).c_str();
    case O::SmImportError: return (sText = (const char*)SM->mImportErrors[a]).c_str();
    case O::SmMediaSection: return (sText = (const char*)SM->mMediaStrings.GetSection((unsigned long)a)).c_str();
    case O::SmExportFileName: return (sText = (const char*)SM->mExportFileName).c_str();
    case O::TriggerName: return (sText = (const char*)TR->mName).c_str();
    case O::TriggerText: return (sText = (const char*)TR->mText).c_str();
    case O::ConditionName: return (sText = (const char*)CO->mName).c_str();
    case O::ConditionText: return (sText = (const char*)CO->mText).c_str();
    case O::ConditionChatText: return (sText = (const char*)CO->mChatText).c_str();
    case O::EffectName: return (sText = (const char*)EF->mName).c_str();
    case O::EffectText: return (sText = (const char*)EF->mText).c_str();
    case O::EffectMessage: return (sText = (const char*)EF->mMessage).c_str();
    case O::EffectNewName: return (sText = (const char*)EF->mNewName).c_str();
    case O::EffectFileName: return (sText = (const char*)EF->mFileName).c_str();
    case O::EffectDescription: return (sText = (const char*)EF->mDescription).c_str();
    case O::ObjectName: return (sText = (const char*)((EESMObject*)p)->mName).c_str();
    case O::AreaName: return (sText = (const char*)AR->GetName()).c_str();
    case O::AreaText: return (sText = (const char*)AR->GetText()).c_str();
    case O::StartupPath: return (sText = (const char*)(gStartupVolume + gStartupDirectory)).c_str();
    case O::SaveTextName: return (sText = (const char*)GetSaveTextName(UString(s), a != 0)).c_str();
    case O::UiFormTypeName: return (sText = typeid(*UF).name()).c_str();
    case O::CivName: return (sText = (const char*)CV->mName).c_str();
    case O::WideText: return (sText = (const char*)*(UWideString*)p).c_str();
    case O::EditorSaveName:
        return (sText = (const char*)GetSaveTextName(UString("EEQuickSave") + gScenarioExtension, true)).c_str();
    default: break;
    }
    return NULL;
}

// The steps' floats
extern "C" double ee_simcpp_sed_float(EeSimDriverSed inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeSimDriverSed O;
    switch (inOp) {
    case O::ControlBgOpacity: return CTL->GetBgOpacity();
    case O::PlayerAvailablePoints: return (double)PL->mCivilization.mAvailablePoints;
    case O::ComputerCitizens: return CP->mCitizenPercentage;
    case O::MeshZ: return gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1]);
    case O::TileHeight: return ((EEMapTile*)p)->mHeight;
    default: break;
    }
    return 0.0;
}
