// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.cpp (file name inferred): the class family of the scenario
// editor's player tab, EESEPlayerScreen (vtable 0x4430dc), its controls EESERBUnitTypes/EESECBMaxPlayer/
// EESEUIBDiplomacy and the player-count action: 0x573218..0x57acf6. The player tab, its player rows and its game
// options page are whole in Rust (ee_screens editor_players.rs; the class EESEPlayerScreen is gone: its group,
// editor_groups_ffi.rs, holds the object), and this unit is its host (ee_screenscpp_sepl). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIEntityGroup.h"
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
#include "ee/EEUIFScenarioEditor.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EEARust.h"
#include "ee/EESERBUnitTypes.h"
#include "lle/UIESliderChange.h"
#include "ee/EESEARust.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERulesWorld.h"
#include "ee/EEUIShell.h"
#include "ee/EEPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EESELoadThread.h"
#include "lle/UITriStateCheckBox.h"
#include "ee/EEUnitSetManager.h"

// the diplomacy buttons' shared bitmaps (once EESEUIBDiplomacy's statics 0x531fc0/0x531fc8/0x531fd0): normal,
// focused, pressed, each for allies (0) and enemies (1)
static GEBitmap* sSeplDiplomacy[3][2];

#include "ee/EECivilizationManager.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"
extern FSPath* gCivilizationDataPath;
extern GEFont* gU531930;                    // 0x531930 (a UI font; Empire Earth.cpp's globals)
UString gSECivilizationExtension(".civ");   // (once EESESCComboBox.cpp's unit global)
// the player tab's techs by kind (once EESEPSSetPlayers' four class statics: units, buildings, heroes, technologies)
static std::vector<long> sSeplV531FB0, sSeplBuildingTechs, sSeplV531F88, sSeplTechnologyTechs;

// The player tab whole in Rust (ee_screens editor_players.rs): EESEPlayerScreen's constructor (0x573313), CreatePlayerControls (0x5737f9), CreateTechControls (0x57509c), the
// destructor (0x5733bf), SetPlayerCount (0x573653), Leave (0x5737ac), GetPlayerCount (0x575a1f), Refresh
// (0x575a3e), Load (0x575c0b), Enter (0x575c48), MarkPlayerChanged (0x575d58), Store (0x575d7e),
// CopyFirstPlayerToAll (0x575eb2), ApplyPlayerSettings (0x575ee5), BuildTechTypeLists (0x575f38), FillTechLists
// (0x5760ad), SelectPage (0x5761c8), RebuildTechLists (0x5766ed); the game options page EESEGamePage (0x576be4 ..
// 0x578342) and the player rows EESEPlayerRow (0x578a29 .. 0x57a53e) are the object's records.

// the screen's font while a screen's object makes its controls (the player tab, the trigger screens' objects)
extern GEFont* gSeplScreenFont;   // EEGlobals.cpp
// the unit tab's lists (EESEUnit.cpp; declared in ee/EESEUnit.h)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex);

// Performs the action: sets gMaxPlayers to the new player count
void EESEAChangeWorldPlayerCount::Virtual4()
{
    gMaxPlayers = this->mCount;
}

// The diplomacy buttons (once EESEUIBDiplomacy > UIButton, vtable 0x443298: a kind over the button shell, ee_game
// shells_editor.rs; its state 0 allies, 1 enemies, toggled by ButtonUp): their bitmaps cut from the editor's button
// texture (0x1768a6), released (0x176b18), and a button's bitmaps and help text for its state (UpdateBitmaps), then
// the form's help text when inHelp (ButtonUp): ee_game screens editor_seplx.rs
void EESEDiplomacyShow(UIButton* inButton, unsigned long inState, bool inHelp)
{
    ee_screens_sepl_diplomacy_show(inButton, inState, inHelp);
}
static const bool sDiplomacyShowRegistered = (gEEUIShellDiplomacyShow = EESEDiplomacyShow, true);   // ee/EEUIShell.h

// ---- the player tab's host (editor_players.rs; EE_SCREENS_SEPL_OP_*): the controls it makes and drives, the
// players and the world it loads from and stores to

static_assert(EE_SCREENS_SEPL_ALIGN_LEFT == kTextAlignLeft && EE_SCREENS_SEPL_ALIGN_CENTER == kTextAlignCenter,
              "the alignments");
static_assert(EE_SCREENS_SEPL_SHOWN == kDisplayModeShown && EE_SCREENS_SEPL_HIDDEN == kDisplayModeHidden, "modes");
static_assert(EE_SCREENS_SEPL_TRIGGER_RANGE == kControlTriggerRangeChanged &&
                  EE_SCREENS_SEPL_TRIGGER_CHECKED == kControlTriggerChecked &&
                  EE_SCREENS_SEPL_TRIGGER_SELECTION == kControlTriggerSelectionChanged,
              "the triggers");
static_assert(EE_SCREENS_SEPL_IDS_MAX_PLAYERS == IDS_MAX_PLAYERS && EE_SCREENS_SEPL_IDS_START_EPOCH == IDS_START_EPOCH &&
                  EE_SCREENS_SEPL_IDS_ALL_PLAYERS == IDS_ALL_PLAYERS && EE_SCREENS_SEPL_IDS_SE_PLAYERS == IDS_SE_PLAYERS &&
                  EE_SCREENS_SEPL_IDS_SE_PLAYER == IDS_SE_PLAYER &&
                  EE_SCREENS_SEPL_IDS_SE_CIVILIZATION == IDS_SE_CIVILIZATION &&
                  EE_SCREENS_SEPL_IDS_ENABLE_TECHNOLOGIES == IDS_ENABLE_TECHNOLOGIES &&
                  EE_SCREENS_SEPL_IDS_DISABLE_TECHNOLOGIES == IDS_DISABLE_TECHNOLOGIES &&
                  EE_SCREENS_SEPL_IDS_UNIT_TYPES == IDS_UNIT_TYPES &&
                  EE_SCREENS_SEPL_IDS_SE_UNIT_GROUP_FIRST == IDS_SE_UNIT_GROUP_FIRST &&
                  EE_SCREENS_SEPL_IDS_DISPLAY_EPOCHS == IDS_DISPLAY_EPOCHS &&
                  EE_SCREENS_SEPL_IDS_END_EPOCH == IDS_END_EPOCH && EE_SCREENS_SEPL_IDS_SE_HUMAN == IDS_SE_HUMAN &&
                  EE_SCREENS_SEPL_IDS_SE_ACTIVE == IDS_SE_ACTIVE && EE_SCREENS_SEPL_IDS_SE_ACTIVE + 4 == IDS_SE_BUILD_REPAIR &&
                  EE_SCREENS_SEPL_IDS_SE_ACTIVE + 8 == IDS_CITIZEN_PERCENTAGE &&
                  EE_SCREENS_SEPL_IDS_RESOURCE_NAME_FIRST == IDS_RESOURCE_NAME_FIRST &&
                  EE_SCREENS_SEPL_IDS_GAME_OPTIONS == IDS_GAME_OPTIONS &&
                  EE_SCREENS_SEPL_IDS_LOBBY_RESOURCES == IDS_LOBBY_RESOURCES &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT == IDS_GAME_GAME_UNIT_LIMIT &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 6 == IDS_UNIT_IMPROVEMENTS &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 7 == IDS_CPU &&
                  EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 10 == IDS_GAME_LOS &&
                  EE_SCREENS_SEPL_IDS_MUTUAL_DIPLOMACY_SETTINGS == IDS_MUTUAL_DIPLOMACY_SETTINGS &&
                  EE_SCREENS_SEPL_IDS_USE_CARBON_RESOURCE == IDS_USE_CARBON_RESOURCE,
              "the strings");
static_assert(EE_SCREENS_SEPX_IDS_NONE == IDS_NONE && EE_SCREENS_SEPX_IDS_UNKNOWN_CIVILIZATION == IDS_UNKNOWN_CIVILIZATION &&
                  EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ALLIES == IDS_PLAYERS_CURRENTLY_ALLIES &&
                  EE_SCREENS_SEPX_IDS_PLAYERS_CURRENTLY_ENEMIES == IDS_PLAYERS_CURRENTLY_ENEMIES &&
                  EE_SCREENS_SEPX_IDS_SET == IDS_SET && EE_SCREENS_SEPX_IDS_UNIT_FAMILY_FIRST == IDS_UNIT_FAMILY_FIRST,
              "editor_seplx.rs's strings");

namespace {

const GEColor kSeplYellow(0.99607843f, 0.96862745f, 0.1254902f);

GEFont* SeplFont(uint32_t inFont)
{
    switch (inFont) {
    case EE_SCREENS_SEPL_FONT_530: return gU531930;
    case EE_SCREENS_SEPL_FONT_534: return gU531934;
    case EE_SCREENS_SEPL_FONT_538: return gU531938;
    case EE_SCREENS_SEPL_FONT_ARIAL18: return gFontArial18;
    default: return gSeplScreenFont;
    }
}


// a text's colour by the record's flags: yellow, white, red (transparent), or none
void SeplColor(GEText* inText, uint32_t inFlags)
{
    if (inFlags & (EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FG_WHITE | EE_SCREENS_SEPL_FG_RED)) {
        inText->SetFgColor((inFlags & EE_SCREENS_SEPL_FG_WHITE) ? GEColor(1.0f, 1.0f, 1.0f)
                           : (inFlags & EE_SCREENS_SEPL_FG_RED) ? GEColor(1.0f, 0.0f, 0.0f)
                                                                : kSeplYellow);
        inText->SetIsTransparent(true);
    }
}

std::vector<UWideString> sSeplLines;   // EE_SCREENS_SEPL_OP_LINE_PUSH's lines, for the next combo box


// the civilization combo's fill, pick and apply (EESEPlayerRow::Load 0x579fee, ApplySettings 0x57a53e) and the epochs'
// combo: ee_game screens editor_seplx.rs over the statements below (EE_SCREENS_SEPX_CIV_* ...)
std::set<UString> sSepxFiles;          // EE_SCREENS_SEPX_FILES_LIST's custom civilization files
std::vector<UWideString> sSepxLines;   // EE_SCREENS_SEPX_LINES_PUSH's lines

}   // namespace

// a line for the next combo box made with pending lines (the map tab's host, EEUIFSEMap.cpp)
void SeplLinePush(const UWideString& inLine)
{
    sSeplLines.push_back(inLine);
}

// The ops (editor_players.rs; p: three handles, i: four integers, f: four floats). The plain world reads (the
// player count, a player's presence, computer player, enemies and id) are Rust's (ee_game editor_world.rs).
extern "C" int32_t ee_screenscpp_sepl(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIControl* c = static_cast<UIControl*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEPL_OP_DELETE: delete c; return 0;
    case EE_SCREENS_SEPL_OP_COLOR: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_COLOR);
    case EE_SCREENS_SEPL_OP_SET_COLOR: EEUIShell::Op(c, EE_SCREENS_SKOP_SET_COLOR, i[0]); return 0;
    case EE_SCREENS_SEPL_OP_RADIO_CHECK: static_cast<UIRadioButton*>(c)->Check(); return 0;
    case EE_SCREENS_SEPL_OP_DIPLOMACY: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_DIPLOMACY_STATE);
    case EE_SCREENS_SEPL_OP_SET_DIPLOMACY: EEUIShell::Op(c, EE_SCREENS_SKOP_DIPLOMACY_SET, i[0]); return 0;
    case EE_SCREENS_SEPL_OP_MAX_PLAYERS: return (int32_t)gMaxPlayers;
    case EE_SCREENS_SEPL_OP_NAME_TO_EDIT: static_cast<UIEditBox*>(c)->SetText(gWorld.mPlayers[i[0]]->mName); return 0;
    case EE_SCREENS_SEPL_OP_NAME_FROM_EDIT:
        gWorld.mPlayers[i[0]]->mName = UWideString(static_cast<UIEditBox*>(c)->GetText(0));
        return 0;
    case EE_SCREENS_SEPL_OP_NAME_TO_LABEL: static_cast<UITextLabel*>(c)->SetText(gWorld.mPlayers[i[0]]->mName); return 0;
    case EE_SCREENS_SEPL_OP_EPOCH_RANGE: gWorld.mPlayers[i[0]]->GetTechTree()->ResetEpochRange(i[1], i[2]); return 0;
    case EE_SCREENS_SEPL_OP_SET_STANCE: gWorld.SetDiplomaticStance(i[0], i[1], i[2]); return 0;
    case EE_SCREENS_SEPL_OP_MINIMAP_RECOLOR: gMiniMap->Recolor(); return 0;
    }
    return 0;
}

// a making statement's object handed back in i0 (low 32 bits) and i1 (high) (editor_sepl.rs mk)
static int32_t SepxHandle(int32_t* i, const void* inObject)
{
    i[0] = (int32_t)(uint32_t)(uintptr_t)inObject;
    i[1] = (int32_t)(uint32_t)((uint64_t)(uintptr_t)inObject >> 32);
    return 0;
}
static EEPlayer* SepxPlayer(int32_t* i) { return gWorld.mPlayers[i[0]]; }
static EEComputerPlayer* SepxAI(int32_t* i) { return gWorld.mPlayers[i[0]]->mComputerPlayer; }
static std::vector<long>* SepxTechs(int32_t inList)
{
    static std::vector<long>* const kLists[4] = {&sSeplV531FB0, &sSeplBuildingTechs, &sSeplV531F88, &sSeplTechnologyTechs};
    return kLists[inList];
}
static std::vector<long> sSepxIDs;   // EE_SCREENS_SEPX_TT_IDS's node ids
static UIButton* SepxButton(void* const* p) { return static_cast<UIButton*>(p[0]); }
static GETexture* SepxTexture(void* const* p) { return static_cast<EEUIFScenarioEditor*>(p[0])->GetButtonTexture(); }
static unsigned long SepxScreen(void* const* p) { return static_cast<EEUIFScenarioEditor*>(p[0])->GetManager()->GetScreen(); }
static GEBitmap*& SepxBitmap(int32_t* i) { return sSeplDiplomacy[i[0]][i[1]]; }
static UWideString SepxTextIn(void* const* p, int32_t* i) { return UWideString(std::string((const char*)p[1], (size_t)i[2]).c_str()); }
static const UString& SepxFile(int32_t* i) { return *std::next(sSepxFiles.begin(), i[0]); }

// a text into the caller's buffer (p1, i3 bytes): its whole length
static int32_t SepxWrite(const UWideString& inText, void* const* p, int32_t* i)
{
    int32_t theLength = (int32_t)inText.GetTCHARLength();
    memcpy(p[1], (const char*)inText, (size_t)(theLength < i[3] ? theLength : i[3]));
    return theLength;
}

// the maker's statements' arguments (editor_sepl.rs make): the record p0, the form p1, the made control p2, the
// rectangle f (left, top, right, bottom)
static const EeScreensSeplCtl& C(void* const* p) { return *static_cast<const EeScreensSeplCtl*>(p[0]); }
static UIForm* F(void* const* p) { return static_cast<UIForm*>(p[1]); }
static UIControl* H(void* const* p) { return static_cast<UIControl*>(p[2]); }
static GEFont* SepxFont(void* const* p) { return SeplFont(C(p).font); }
static UWideString sSepxText;   // the maker's text (EE_SCREENS_SEPX_SET_TEXT: SeplText, editor_seplx.rs prestep)
static UWideString SepxText(void* const* p) { return sSepxText; }
static GERectangle SepxRect(const float* f) { return GERectangle(f[0], f[1], f[2], f[3]); }

// the shared maker's single statements (editor_sepl.rs EE_SCREENS_SEPX_*: the Rust composes the host's sequences)
extern "C" int32_t ee_screenscpp_sepx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIControl* c = static_cast<UIControl*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_POPULATION: return (int32_t)gWorld.mPopulationLimit;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_WONDERS: return (int32_t)gWorld.mNumberOfWondersToWin;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_VICTORY: return gWorld.mVictoryEnabled;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_TEAMS: return gWorld.mTeamsLocked;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_CHEATS: return gWorld.mCheatsEnabled;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_IMPROVEMENTS: return gWorld.mUnitImprovementsAllowed;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_CARBON: return gWorld.mUsingCarbon;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_OPT_UNIT_LIMIT: return (int32_t)gWorld.mGameOptions.mUnitLimit;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_OPT_WONDERS: return (int32_t)gWorld.mGameOptions.mWondersForVictory;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_OPT_VICTORY: return gWorld.mGameOptions.mVictory;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_OPT_TEAMS: return gWorld.mGameOptions.mLockTeams;
    case EE_SCREENS_SEPX_WORLD_GET + EE_SCREENS_SEPL_WORLD_OPT_CHEATS: return gWorld.mGameOptions.mCheatCodes;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_POPULATION: gWorld.SetPopulationLimit(i[0]);
        return 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_WONDERS: return (int32_t)(gWorld.mNumberOfWondersToWin = i[0]);
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_VICTORY: return gWorld.mVictoryEnabled = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_TEAMS: return gWorld.mTeamsLocked = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_CHEATS: return gWorld.mCheatsEnabled = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_IMPROVEMENTS: return gWorld.mUnitImprovementsAllowed = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_CARBON: return gWorld.mUsingCarbon = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_OPT_UNIT_LIMIT: return (int32_t)(gWorld.mGameOptions.mUnitLimit = i[0]);
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_OPT_WONDERS: return (int32_t)(gWorld.mGameOptions.mWondersForVictory = i[0]);
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_OPT_VICTORY: return gWorld.mGameOptions.mVictory = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_OPT_TEAMS: return gWorld.mGameOptions.mLockTeams = i[0] != 0;
    case EE_SCREENS_SEPX_WORLD_SET + EE_SCREENS_SEPL_WORLD_OPT_CHEATS: return gWorld.mGameOptions.mCheatCodes = i[0] != 0;
    case EE_SCREENS_SEPX_CAP: return (int32_t)SepxPlayer(i)->mResourceCaps[i[2]];
    case EE_SCREENS_SEPX_SET_CAP: return (int32_t)(SepxPlayer(i)->mResourceCaps[i[2]] = i[3]);
    case EE_SCREENS_SEPX_CURRENT_EPOCH: return SepxPlayer(i)->GetTechTree()->mCurrentEpoch;
    case EE_SCREENS_SEPX_END_EPOCH: return SepxPlayer(i)->GetTechTree()->mEndEpoch;
    case EE_SCREENS_SEPX_START_EPOCH: return SepxPlayer(i)->GetTechTree()->mStartEpoch;
    case EE_SCREENS_SEPX_LOS: return SepxPlayer(i)->mSharedLOS;
    case EE_SCREENS_SEPX_SET_LOS: SepxPlayer(i)->SetSharedLOS(i[3] != 0, gWorld.mMap);
        return 0;
    case EE_SCREENS_SEPX_OPTIONS_COLOR: return (int32_t)gWorld.mGameOptions.mPlayers[i[0] - 1].mColor;
    case EE_SCREENS_SEPX_SET_OPTIONS_COLOR: return (int32_t)(gWorld.mGameOptions.mPlayers[i[0] - 1].mColor = (unsigned long)i[3]);
    case EE_SCREENS_SEPX_OPTIONS_CPU: return gWorld.mGameOptions.mPlayers[i[0] - 1].mB36;
    case EE_SCREENS_SEPX_SET_OPTIONS_CPU: return gWorld.mGameOptions.mPlayers[i[0] - 1].mB36 = i[3] != 0;
    case EE_SCREENS_SEPX_CITIZENS: f[0] = SepxAI(i)->mCitizenPercentage;
        return 0;
    case EE_SCREENS_SEPX_SET_CITIZENS: SepxAI(i)->SetCitizenPercentage((float)i[3] * kHeightEpsilon);
        return 0;
    case EE_SCREENS_SEPX_AI_GET + 0: return SepxAI(i)->mActive;
    case EE_SCREENS_SEPX_AI_GET + 1: return SepxAI(i)->mIsCheating;
    case EE_SCREENS_SEPX_AI_GET + 2: return SepxAI(i)->mCanAttack;
    case EE_SCREENS_SEPX_AI_GET + 3: return SepxAI(i)->mCanDefend;
    case EE_SCREENS_SEPX_AI_GET + 4: return SepxAI(i)->mCanBuild;
    case EE_SCREENS_SEPX_AI_GET + 5: return SepxAI(i)->mCanExplore;
    case EE_SCREENS_SEPX_AI_GET + 6: return SepxAI(i)->mIgnoreOthers;
    case EE_SCREENS_SEPX_AI_GET + 7: return SepxAI(i)->mCanGather;
    case EE_SCREENS_SEPX_AI_SET + 0: SepxAI(i)->SetActive(i[3] != 0);
        return 0;
    case EE_SCREENS_SEPX_AI_SET + 1: return SepxAI(i)->mIsCheating = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 2: return SepxAI(i)->mCanAttack = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 3: return SepxAI(i)->mCanDefend = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 4: return SepxAI(i)->mCanBuild = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 5: return SepxAI(i)->mCanExplore = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 6: return SepxAI(i)->mIgnoreOthers = i[3] != 0;
    case EE_SCREENS_SEPX_AI_SET + 7: return SepxAI(i)->mCanGather = i[3] != 0;
    case EE_SCREENS_SEPX_EVENT_START_EPOCH: return SepxHandle(i, static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_START_EPOCH, p[1])));
    case EE_SCREENS_SEPX_EVENT_SLIDER:
        return SepxHandle(i, static_cast<UIEvent*>(new UIESliderChange(static_cast<UITextLabel*>(p[1]), 1.0f, 0)));
    case EE_SCREENS_SEPX_EVENT_UNIT_LIMIT:
        return SepxHandle(i, static_cast<UIEvent*>(EESEENewUnitLimit(static_cast<UITextLabel*>(p[1]), 1.0f, 0)));
    case EE_SCREENS_SEPX_EVENT_DISPLAY_EPOCH: return SepxHandle(i, EECreateScreenEvent(EE_SCREENS_SE_DISPLAY_EPOCH));
    case EE_SCREENS_SEPX_EVENT_SCREEN: return SepxHandle(i, EECreateScreenEvent((uint32_t)i[0]));
    case EE_SCREENS_SEPX_SET_TRIGGER: c->SetTriggerEvent((unsigned long)i[0], static_cast<UIEvent*>(p[1]));
        return 0;
    case EE_SCREENS_SEPX_EVENT_RELEASE: static_cast<UIEvent*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SEPX_COUNT_ACTION: return SepxHandle(i, static_cast<EEAction*>(new EESEAChangeWorldPlayerCount((unsigned long)i[0])));
    case EE_SCREENS_SEPX_POST_ACTION: gUserInterface->PostAction(static_cast<EEAction*>(p[0]));
        return 0;
    case EE_SCREENS_SEPX_ACTION_RELEASE: static_cast<EEAction*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEPX_TECH_REBUILD: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_TECH_REBUILD);
    case EE_SCREENS_SEPX_TECH_CLEAR: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_TECH_CLEAR);
    case EE_SCREENS_SEPX_TECH_ADD: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_TECH_ADD, (*SepxTechs(i[0]))[(size_t)i[1]]);
    case EE_SCREENS_SEPX_UNIT_LIMIT_MIN: return (int32_t)EERuleLong(EEGetRules().mGameOptions.mUnitLimitMinimum);
    case EE_SCREENS_SEPX_UNIT_LIMIT_MAX: return (int32_t)EERuleLong(EEGetRules().mGameOptions.mUnitLimitMaximum);
    case EE_SCREENS_SEPX_UNIT_LIMIT_DEFAULT: return (int32_t)EERuleLong(EEGetRules().mGameOptions.mUnitLimitDefault);
    case EE_SCREENS_SEPX_COLOR_DIFFERS:
        return gWorld.mPlayers[i[0]]->mColor != EEGetPlayerColor((unsigned long)EEUIShell::Op(c, EE_SCREENS_SKOP_COLOR));
    case EE_SCREENS_SEPX_SET_PLAYER_COLOR:
        gWorld.mPlayers[i[0]]->SetColor(EEGetPlayerColor((unsigned long)EEUIShell::Op(c, EE_SCREENS_SKOP_COLOR)));
        return 0;
    case EE_SCREENS_SEPX_MINIMAP: return gMiniMap != NULL;
    case EE_SCREENS_SEPX_MINIMAP_REDRAW: gMiniMap->Redraw();
        return 0;
    case EE_SCREENS_SEPX_POST_406:
        gUIManager->GetScenarioEditorForm()->GetLoadThread()->Post406(reinterpret_cast<void*>((uintptr_t)i[0]), reinterpret_cast<void*>((intptr_t)gWorld.GetPlayer(i[0])->GetTechTree()->mStartEpoch));
        return 0;
    case EE_SCREENS_SEPX_POST_403: gUIManager->GetScenarioEditorForm()->GetLoadThread()->Post403(reinterpret_cast<void*>((uintptr_t)i[0]), i[1] != 0);
        return 0;
    case EE_SCREENS_SEPX_POST_407:
        gUIManager->GetScenarioEditorForm()->GetLoadThread()->Post407(reinterpret_cast<void*>((uintptr_t)i[0]), reinterpret_cast<void*>((intptr_t)gWorld.GetPlayer(i[0])->GetTechTree()->mStartEpoch));
        return 0;
    case EE_SCREENS_SEPX_TT_IDS_CLEAR: sSepxIDs.clear();
        return 0;
    case EE_SCREENS_SEPX_TT_IDS: gTechTree.GetNodeIDs(sSepxIDs);
        return 0;
    case EE_SCREENS_SEPX_TT_COUNT: return (int32_t)sSepxIDs.size();
    case EE_SCREENS_SEPX_TT_CLEAR: SepxTechs(i[0])->clear();
        return 0;
    case EE_SCREENS_SEPX_TT_CLASS: return (int32_t)gTechTree.Get(sSepxIDs[(size_t)i[0]])->mData->mUnitClass;
    case EE_SCREENS_SEPX_TT_CLASS_EXISTS: return gUnitClassTable[i[0]] != NULL;
    case EE_SCREENS_SEPX_TT_LAND_BUILDING: return gUnitClassTable[i[0]]->IsLandBuilding();
    case EE_SCREENS_SEPX_TT_AI_KIND: return (int32_t)gUnitClassTable[i[0]]->mAIKind;
    case EE_SCREENS_SEPX_TT_PUSH: SepxTechs(i[1])->push_back(sSepxIDs[(size_t)i[0]]);
        return 0;
    case EE_SCREENS_SEPX_FT_COUNT: return (int32_t)SepxTechs(i[0])->size();
    case EE_SCREENS_SEPX_FT_SHOWN: return gWorld.mPlayers[i[1]]->GetTechTree()->Get((*SepxTechs(i[0]))[(size_t)i[2]])->mData->mShowInEditor == true;
    case EE_SCREENS_SEPX_FT_OBSOLETED: return gWorld.mPlayers[i[1]]->GetTechTree()->Get((*SepxTechs(i[0]))[(size_t)i[2]])->mScenEditorObsoleted;
    case EE_SCREENS_SEPX_FONT_HEIGHT: return (int32_t)SeplFont((uint32_t)i[0])->GetHeight();
    case EE_SCREENS_SEPX_MAKE_LABEL:
        return SepxHandle(i, static_cast<UIControl*>(new UITextLabel(SepxText(p), SepxFont(p), C(p).id, SepxRect(f), F(p), 0, C(p).index, (unsigned long)C(p).extra[0])));
    case EE_SCREENS_SEPX_MAKE_EDIT:
        return SepxHandle(i, static_cast<UIControl*>(new UIEditBox(C(p).id, SepxFont(p), C(p).extra[0], C(p).extra[1] != 0, SepxRect(f), F(p), C(p).index, 2, C(p).extra[2] != 0, false)));
    case EE_SCREENS_SEPX_MAKE_COMBO:
        return SepxHandle(i, static_cast<UIControl*>(new UIComboBox(C(p).id, 1, std::vector<UWideString>(), SepxFont(p), SepxRect(f), 400, F(p), C(p).index, 0, 2)));
    case EE_SCREENS_SEPX_MAKE_COMBO_EPOCHS:
        return SepxHandle(i, static_cast<UIControl*>(new UIComboBox(C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), 400, F(p), C(p).index, 0, 2)));
    case EE_SCREENS_SEPX_MAKE_MAX_PLAYER:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellCombo(EE_SCREENS_SK_MAX_PLAYER, {}, C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), SepxFont(p)->GetHeight() << 4, F(p), C(p).index, 0, 2)));
    case EE_SCREENS_SEPX_MAKE_CHECK:
        return SepxHandle(i, static_cast<UIControl*>(new UICheckBox(C(p).id, SepxText(p), SepxFont(p), SepxRect(f), F(p), C(p).index, 0, false)));
    case EE_SCREENS_SEPX_MAKE_SLIDER: return SepxHandle(i, static_cast<UIControl*>(new UISlider(C(p).id, SepxRect(f), 0, F(p), C(p).index, 2)));
    case EE_SCREENS_SEPX_MAKE_UNIT_TYPE:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellRadio(EE_SCREENS_SK_UNIT_TYPES, {}, C(p).id, SepxText(p), SepxFont(p), SepxRect(f), F(p), C(p).index, 0, false, C(p).extra[0] != 0, false)));
    case EE_SCREENS_SEPX_MAKE_TECH_LIST:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellList(EE_SCREENS_SK_TECH_LIST, {}, C(p).id, std::vector<UWideString>(), SepxFont(p), false, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_SELECT:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_SELECT, {(int64_t)SepxFont(p)}, C(p).id, NULL, NULL, NULL, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_TAB:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellRadio(EE_SCREENS_SK_TAB, {(int64_t)SepxFont(p)}, C(p).id, UWideString(""), SepxFont(p), SepxRect(f), F(p), C(p).index, 3, true, C(p).extra[0] != 0, false)));
    case EE_SCREENS_SEPX_MAKE_SET_PLAYERS:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_SET_PLAYERS, {(int64_t)SepxFont(p)}, 0x144, NULL, NULL, NULL, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_COLOR:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_COLOR, {C(p).extra[0]}, C(p).id, NULL, NULL, NULL, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_DIPLOMACY:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellButton(EE_SCREENS_SK_DIPLOMACY, {}, C(p).id, sSeplDiplomacy[0][1], sSeplDiplomacy[1][1], sSeplDiplomacy[2][1], SepxRect(f), F(p), C(p).index, 0)));
    case EE_SCREENS_SEPX_MAKE_RADIO:
        return SepxHandle(i, static_cast<UIControl*>(new UIRadioButton(C(p).id, SepxText(p), SepxFont(p), SepxRect(f), F(p), C(p).index, 0, false, C(p).extra[0] != 0, false)));
    case EE_SCREENS_SEPX_MAKE_TRISTATE:
        return SepxHandle(i, static_cast<UIControl*>(new UITriStateCheckBox(C(p).id, SepxText(p), SepxFont(p), SepxRect(f), F(p), C(p).index, 0)));
    case EE_SCREENS_SEPX_MAKE_OBJ_EDIT:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellEdit(EE_SCREENS_SK_OBJECT_EDIT, {}, C(p).id, SepxFont(p), C(p).extra[0], C(p).extra[1] != 0, SepxRect(f), F(p), C(p).index, 2, false, false)));
    case EE_SCREENS_SEPX_MAKE_SHELL_COMBO:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellCombo((uint32_t)i[0], {}, C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), (unsigned long)C(p).list_height, F(p), C(p).index, (unsigned long)C(p).max_chars, (unsigned long)C(p).extra[2])));
    case EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellCombo((uint32_t)i[0], {(int64_t)(unsigned long)C(p).number}, C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), (unsigned long)C(p).list_height, F(p), C(p).index, (unsigned long)C(p).max_chars, (unsigned long)C(p).extra[2])));
    case EE_SCREENS_SEPX_MAKE_SCENARIO_COMBO:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellCombo((uint32_t)i[0], {(int64_t)C(p).number, (int64_t)(unsigned long)C(p).string}, C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), (unsigned long)C(p).list_height, F(p), C(p).index, (unsigned long)C(p).max_chars, (unsigned long)C(p).extra[2])));
    case EE_SCREENS_SEPX_MAKE_PLAIN_COMBO:
        return SepxHandle(i, static_cast<UIControl*>(new UIComboBox(C(p).id, 1, sSepxLines, SepxFont(p), SepxRect(f), (unsigned long)C(p).list_height, F(p), C(p).index, (unsigned long)C(p).max_chars, (unsigned long)C(p).extra[2])));
    case EE_SCREENS_SEPX_MAKE_BUTTON:
        return SepxHandle(i, static_cast<UIControl*>(new UIButton(C(p).id, NULL, NULL, NULL, SepxRect(f), F(p), C(p).index, (unsigned long)C(p).extra[0])));
    case EE_SCREENS_SEPX_MAKE_ARROW:
        return SepxHandle(i, static_cast<UIControl*>(new UIButton(C(p).id, C(p).extra[0] != 0 ? F(p)->GetManager()->GetSBBitmapRightNormal() : F(p)->GetManager()->GetSBBitmapLeftNormal(), NULL, C(p).extra[0] != 0 ? F(p)->GetManager()->GetSBBitmapRightPressed() : F(p)->GetManager()->GetSBBitmapLeftPressed(), SepxRect(f), F(p), C(p).index, 0)));
    case EE_SCREENS_SEPX_MAKE_LIST_PLAIN:
        return SepxHandle(i, static_cast<UIControl*>(new UILBString(C(p).id, std::vector<UWideString>(), SepxFont(p), false, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_LIST_EFFECT:
        return SepxHandle(i, static_cast<UIControl*>(new UILBString(C(p).id, std::vector<UWideString>(), SepxFont(p), true, SepxRect(f), F(p), C(p).index, 2)));
    case EE_SCREENS_SEPX_MAKE_LIST_OBJECT:
        return SepxHandle(i, static_cast<UIControl*>(new EEUIShellList(EE_SCREENS_SK_UNIT_LIST, {}, C(p).id, std::vector<UWideString>(), SepxFont(p), false, SepxRect(f), F(p), C(p).index, 3)));
    case EE_SCREENS_SEPX_MAKE_LIST_UNIT:
        return SepxHandle(i, static_cast<UIControl*>(EESEUnitNewList(C(p).extra[0], C(p).id, SepxFont(p), GE2DPoint(C(p).x, C(p).y), (unsigned long)C(p).w, (unsigned long)C(p).h, F(p), C(p).index)));
    case EE_SCREENS_SEPX_ALIGN: static_cast<UITextLabel*>(H(p))->SetAlignment((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEPX_BG: H(p)->SetBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEPX_RADIO_LABEL_BG: static_cast<UIRadioButton*>(H(p))->GetTextLabel()->SetBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_LABEL: SeplColor(static_cast<UITextLabel*>(H(p))->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_EDIT: SeplColor(static_cast<UIEditBox*>(H(p))->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_COMBO: SeplColor(static_cast<UIComboBox*>(H(p))->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_LIST: SeplColor(static_cast<UILBString*>(H(p))->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_BUTTON: SeplColor(static_cast<UIButton*>(H(p))->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_CHECK: SeplColor(static_cast<UICheckBox*>(H(p))->GetTextLabel()->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_RADIO: SeplColor(static_cast<UIRadioButton*>(H(p))->GetTextLabel()->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COLOR_TRISTATE: SeplColor(static_cast<UITriStateCheckBox*>(H(p))->GetTextLabel()->GetGEText(), (uint32_t)i[0]);
        return 0;
    case EE_SCREENS_SEPX_EDIT_TEXT: static_cast<UIEditBox*>(H(p))->SetText(SepxText(p));
        return 0;
    case EE_SCREENS_SEPX_COMBO_TEXT_BG: static_cast<UIComboBox*>(H(p))->SetTextBoxBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEPX_COMBO_SELECT_BLUE: static_cast<UIComboBox*>(H(p))->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        return 0;
    case EE_SCREENS_SEPX_COMBO_SELECT: static_cast<UIComboBox*>(H(p))->SetCurrentSelection((unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEPX_COMBO_ALWAYS_DIRTY: static_cast<UIComboBox*>(H(p))->SetListBoxAlwaysDirty(true);
        return 0;
    case EE_SCREENS_SEPX_SLIDER_PAGE: static_cast<UISlider*>(H(p))->SetPageSize(i[0]);
        return 0;
    case EE_SCREENS_SEPX_SLIDER_RANGE: static_cast<UISlider*>(H(p))->SetRange(i[0], i[1]);
        return 0;
    case EE_SCREENS_SEPX_SLIDER_VALUE: static_cast<UISlider*>(H(p))->SetCurrentRangeValue(i[0]);
        return 0;
    case EE_SCREENS_SEPX_LIST_SCROLL_BG: static_cast<UILBString*>(H(p))->SetScrollBarBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEPX_LIST_SELECT_BLUE: static_cast<UILBString*>(H(p))->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
        return 0;
    case EE_SCREENS_SEPX_BUTTON_TEXT: static_cast<UIButton*>(H(p))->SetText(SepxText(p), SepxFont(p));
        return 0;
    case EE_SCREENS_SEPX_HIDE: H(p)->SetDisplayMode(kDisplayModeHidden);
        return 0;
    case EE_SCREENS_SEPX_ADD_CONTROL: F(p)->AddControl(H(p), (unsigned long)i[0]);
        return 0;
    case EE_SCREENS_SEPX_CIV_COUNT: return (int32_t)gCivilizationManager.GetNumCivilizations();
    case EE_SCREENS_SEPX_CIV_NAME_ID: return gCivilizationManager.GetCivilization((unsigned long)i[0]).mNameID;
    case EE_SCREENS_SEPX_CIV_FILE: return SepxWrite(UWideString(gCivilizationManager.GetCivilization((unsigned long)i[0]).mFileName), p, i);
    case EE_SCREENS_SEPX_FILES_CLEAR: sSepxFiles.clear();
        return 0;
    case EE_SCREENS_SEPX_FILES_LIST: gCivilizationDataPath->ListFiles(UString("*") + gSECivilizationExtension, sSepxFiles);
        return 0;
    case EE_SCREENS_SEPX_FILES_COUNT: return (int32_t)sSepxFiles.size();
    case EE_SCREENS_SEPX_FILE_STEM: return SepxWrite(UWideString(SepxFile(i).GetIndexedSubstring(0, SepxFile(i).GetLength() - 5)), p, i);
    case EE_SCREENS_SEPX_PLAYER_CIV: return SepxWrite(UWideString((const char*)SepxPlayer(i)->mCivilizationFile), p, i);
    case EE_SCREENS_SEPX_PLAYER_CIV_CLEAR: SepxPlayer(i)->mCivilization.mPurchasedUpgrades.clear();
        return 0;
    case EE_SCREENS_SEPX_PLAYER_CIV_POINTS: SepxPlayer(i)->mCivilization.mAvailablePoints = SepxPlayer(i)->mCivilization.mMaxPoints;
        return 0;
    case EE_SCREENS_SEPX_PLAYER_SET_CIV: SepxPlayer(i)->mCivilizationFile = UString(std::string((const char*)p[1], (size_t)i[2]).c_str());
        return 0;
    case EE_SCREENS_SEPX_PLAYER_APPLY_CIV: EEApplyCivilizationFile(SepxPlayer(i)->mCivilizationFile, SepxPlayer(i));
        return 0;
    case EE_SCREENS_SEPX_LINES_CLEAR: sSepxLines.clear();
        return 0;
    case EE_SCREENS_SEPX_LINES_PUSH: sSepxLines.push_back(SepxTextIn(p, i));
        return 0;
    case EE_SCREENS_SEPX_COMBO_APPEND_LINES: static_cast<UIComboBox*>(c)->AppendStringArray(sSepxLines);
        return 0;
    case EE_SCREENS_SEPX_DIPLO_WIDTH: return (int32_t)SepxTexture(p)->GetWidth();
    case EE_SCREENS_SEPX_DIPLO_NEW: SepxBitmap(i) = new GEBitmap;
        return 0;
    case EE_SCREENS_SEPX_DIPLO_TEXTURE: SepxBitmap(i)->SetTexture(SepxTexture(p), SepxScreen(p));
        return 0;
    case EE_SCREENS_SEPX_DIPLO_COORDS:
        SepxBitmap(i)->SetTextureCoordinates(GETextureCoordinate(f[0], f[2]), GETextureCoordinate(f[1], f[2]),
                                             GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[1], f[3]));
        return 0;
    case EE_SCREENS_SEPX_DIPLO_SCALE: SepxBitmap(i)->SetXYScale(f[0], f[0]);
        return 0;
    case EE_SCREENS_SEPX_DIPLO_RELEASE: SepxBitmap(i)->Dereference();
        return 0;
    case EE_SCREENS_SEPX_DIPLO_PRESSED: SepxButton(p)->SetPressedBitmap(sSeplDiplomacy[2][i[0]]);
        return 0;
    case EE_SCREENS_SEPX_DIPLO_FOCUSED: SepxButton(p)->SetFocusedBitmap(sSeplDiplomacy[1][i[0]]);
        return 0;
    case EE_SCREENS_SEPX_DIPLO_NORMAL: SepxButton(p)->SetNormalBitmap(sSeplDiplomacy[0][i[0]]);
        return 0;
    case EE_SCREENS_SEPX_DIPLO_UPDATE_HELP: SepxButton(p)->GetForm()->UpdateHelpString();
        return 0;
    case EE_SCREENS_SEPX_SET_TEXT: sSepxText = SepxTextIn(p, i);
        return 0;
    case EE_SCREENS_SEPX_LINES_TAKE_PENDING: sSepxLines.swap(sSeplLines);
        return 0;
    case EE_SCREENS_SEPX_PENDING_CLEAR: sSeplLines.clear();
        return 0;
    case EE_SCREENS_SEPX_PENDING_PUSH: sSeplLines.push_back(SepxTextIn(p, i));
        return 0;
    case EE_SCREENS_SEPX_SETS_COUNT: return (int32_t)gUnitSetManager.GetSize();
    case EE_SCREENS_SEPX_SET_SHOWN: return gUnitSetManager.GetSet(i[0]) != NULL && gUnitSetManager.GetSet(i[0])->ShowInEditor() == true;
    case EE_SCREENS_SEPX_SET_NAME: return SepxWrite(UWideString(gUnitSetManager.GetSet(i[0])->GetName()), p, i);
    case EE_SCREENS_SEPX_SCREEN_FONT: return SepxHandle(i, gSeplScreenFont);
    case EE_SCREENS_SEPX_SET_SCREEN_FONT: gSeplScreenFont = static_cast<GEFont*>(p[0]);
        return 0;
    case EE_SCREENS_SEPX_BRUSH_SIZE: return (int32_t)EEUIShell::Op(c, EE_SCREENS_SKOP_BRUSH_SIZE);
    }
    return 0;
}
