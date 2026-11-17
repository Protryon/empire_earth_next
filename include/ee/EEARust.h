// The game's actions whole in Rust (ee_screens game_actions.rs; port addition, no original address): one EEAction
// over a Rust record (EeScreensGaFields, its second base: the members keep the original names) whose kind picks the
// original class's behaviour, run over one host (Actions/EEARust.cpp). The original classes (Actions/EEA*.cpp and
// every class of Scenario Editor Actions: the editor's click, lasso, load text, paints and whole actions, whose kinds
// run ee_screens editor_actions.rs and editor_whole.rs over their hosts in Scenario Editor Actions/EESEARust.cpp) are
// the one-line constructors below; their users keep the names, the members and the calls. The C++ objects (the
// strings, the poll, the mini map's point, the scroll's edges) stay members here, read by the host.
#ifndef EE_EEARUST_H
#define EE_EEARUST_H

#include <stdint.h>
#include <vector>
#include "ee/EEAction.h"
#include "lle/TMPoll.h"
#include "rust/game/screens.h"

class EEMessage;
class EEUnit;
class EEPlayer;
class UIForm;
class EECalamityClass;

// the kinds (EE_SCREENS_GA_*)
enum {
    kEEAChat = 1, kEEABoot, kEEABuild, kEEACalamity, kEEACenterCamera, kEEAJoin, kEEALasso, kEEAMiniMap, kEEAMouseClick,
    kEEAPlayerEvent, kEEARemovePlayer, kEEAResetPlayers, kEEASaveGame, kEEAScroll, kEEAMigration, kEEAUpgrade, kEEAPause,
    kEEAQuick, kEEAEditorQuick,
    // the scenario editor's
    kEEAEditorClick, kEEAEditorLasso, kEEATriggerClick, kEEATriggerLasso, kEEALoadText, kEEAEditorPaint, kEEAEditorWhole
};

class EEARust : public EEAction, public EeScreensGaFields {
public:
    explicit EEARust(uint32_t inKind) : EEAction(false), EeScreensGaFields(), mPoll(10, 0, false)
    {
        this->mKind = inKind;
        ee_screens_ga_init(this);
    }
    virtual ~EEARust() { ee_screens_ga_free(this); }
    virtual void Virtual4() { ee_screens_ga_run(this); }
    virtual void Virtual8() {}
    // the mouse click's order for a right click at a world point (the editor's: a deselection at the mouse)
    virtual void Execute(float inX, float inY, bool inMove, unsigned long inModifiers, bool inIsRotating, float inFacing,
                         bool inForceMove)
    {
        ee_screens_ga_click_execute(this, inX, inY, inMove, (uint32_t)inModifiers, inIsRotating, inFacing, inForceMove);
    }
    // the lasso's: the editor's deselect the selected units inside the rectangle and post the action (inArg2 unused)
    virtual void SelectInRectangle(const GERectangle& inRectangle, long inArg2)
    {
        ee_screens_ga_select_in_rectangle(this, &inRectangle);
    }
    // the mouse click's plain click (inModifiers & 1: add to the selection)
    void SelectAtMouse(unsigned long inModifiers) { ee_screens_ga_select_at_mouse(this, (uint32_t)inModifiers); }
    void SetMessage(EEMessage* inMessage) { this->mMessage = inMessage; }
    // the lasso's message over inRectangle (inFlags 2: add and remove, 1: add; the path point type: the patrol group)
    void SelectUnits(const GERectangle& inRectangle, long inFlags, unsigned long inUnitType, long inPatrolGroup)
    {
        ee_screens_ga_select_units(this, &inRectangle, (int32_t)inFlags, (uint32_t)inUnitType, (int32_t)inPatrolGroup);
    }
    // the build's placements of mTechnology
    void Build(const GE2DPoint& inLocation) { ee_screens_ga_build(this, inLocation.GetX(), inLocation.GetY()); }
    void BuildGate(const GE2DPoint& inLocation, long inSide)
    {
        ee_screens_ga_build_gate(this, inLocation.GetX(), inLocation.GetY(), (int32_t)inSide);
    }
    void BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd)
    {
        ee_screens_ga_build_wall(this, inStart.GetX(), inStart.GetY(), inEnd.GetX(), inEnd.GetY());
    }
    // the calamity's cast and check at a screen point
    void Cast(const GE2DPoint& inLocation) { ee_screens_ga_cast(this, inLocation.GetX(), inLocation.GetY()); }
    long Check(const GE2DPoint& inLocation) { return ee_screens_ga_check(this, inLocation.GetX(), inLocation.GetY()); }
    void SetCalamity(long inTechnology, EECalamityClass* inClass)
    {
        this->mTechnology = (int32_t)inTechnology;
        this->mCalamityClass = inClass;
    }
    void SetCasterID(long inCasterID) { this->mCasterID = (int32_t)inCasterID; }
    // the camera's targets (both post the action)
    void SetLocation(long inX, long inY) { ee_screens_ga_set_location(this, (int32_t)inX, (int32_t)inY); }
    void SetUnit(EEUnit* inUnit) { ee_screens_ga_set_unit(this, inUnit); }
    // the edge scroll: which edges the mouse touches; true while it scrolls
    bool Update(const GE2DPoint& inPoint) { return ee_screens_ga_scroll_update(this, inPoint.GetX(), inPoint.GetY()); }

    GE2DPoint mPoint;                       // the mini map's click (the file-static transformation maps it)
    GERectangle mEdges;                     // the edge scroll's inner rectangle (outside it the camera scrolls)
    TMPoll mPoll;                           // the edge scroll's (10 ms, not started)
    UWideString mText;                      // the chat line; the migration's dropped host
    UWideString mPlayerName;                // the join's
    UWideString mGameName;                  // the join's game name or address; the migration's new host
    UString mFileName;                      // the saved game's; the quick save's; the join's detected address
    UString mDescription;                   // the saved game's; the join's detected service
    UString mName;                          // the quick save's name; the migration's new host's address
    UWideString mDetectedName;              // the join's detected game name
    std::vector<unsigned long> mPlayers;    // the civilization reset's players (indices into gWorld.mPlayers)
};

// the original classes, as their constructors
class EEAAddChatMessage : public EEARust {
public:
    EEAAddChatMessage(const UWideString& inText, bool inFromLocalPlayer) : EEARust(kEEAChat)
    {
        this->mText = inText;
        this->mFromLocalPlayer = inFromLocalPlayer;
    }
};
class EEABootClientFromCanceledGame : public EEARust {
public:
    EEABootClientFromCanceledGame(long inStringID = 0, bool inNoPop = false) : EEARust(kEEABoot)
    {
        this->mStringID = (int32_t)inStringID;
        this->mNoPop = inNoPop;
    }
};
class EEABuild : public EEARust {
public:
    EEABuild() : EEARust(kEEABuild) {}
};
class EEACalamity : public EEARust {
public:
    EEACalamity() : EEARust(kEEACalamity) {}
};
class EEACenterCamera : public EEARust {
public:
    EEACenterCamera() : EEARust(kEEACenterCamera) {}
};
class EEAJoinOrHostAGame : public EEARust {
public:
    // inMode 0: host, 1: join a detected game, 2/3: connect to the address inGameName, 4: host for WON
    EEAJoinOrHostAGame(long inMode, const UWideString& inPlayerName, const UWideString& inGameName) : EEARust(kEEAJoin)
    {
        this->mJoinMode = (int32_t)inMode;
        this->mPlayerName = inPlayerName;
        this->mGameName = inGameName;
    }
};
class EEALasso : public EEARust {
public:
    EEALasso(uint32_t inKind = kEEALasso) : EEARust(inKind) {}
};
class EEAMiniMap : public EEARust {
public:
    EEAMiniMap() : EEARust(kEEAMiniMap) {}
};
class EEAMouseClick : public EEARust {
public:
    EEAMouseClick(uint32_t inKind = kEEAMouseClick) : EEARust(inKind) {}
};
class EEAPlayerEvent : public EEARust {
public:
    EEAPlayerEvent() : EEARust(kEEAPlayerEvent) {}
};
class EEARemovePlayer : public EEARust {
public:
    EEARemovePlayer(unsigned char inPlayer, bool inExiting = false) : EEARust(kEEARemovePlayer)
    {
        this->mPlayer = inPlayer;
        this->mIsExiting = inExiting;
    }
};
class EEAResetAllPlayerInfo : public EEARust {
public:
    EEAResetAllPlayerInfo(std::vector<unsigned long> inPlayers) : EEARust(kEEAResetPlayers) { this->mPlayers = inPlayers; }
};
class EEASaveGame : public EEARust {
public:
    EEASaveGame(const UString& inFileName, bool inFlag, UString inDescription) : EEARust(kEEASaveGame)
    {
        this->mFileName = inFileName;
        this->mFlag = inFlag;
        this->mDescription = inDescription;
    }
};
class EEAScroll : public EEARust {
public:
    EEAScroll() : EEARust(kEEAScroll) {}
};
class EEAServerMigration : public EEARust {
public:
    EEAServerMigration() : EEARust(kEEAMigration) {}
};
class EEAUpgradeImprovement : public EEARust {
public:
    // posts itself (inRaise: raise, else lower; inIsInGame: by message, else the scenario editor's direct change)
    EEAUpgradeImprovement(EEUnit* inUnit, long inImprovement, bool inRaise, bool inIsInGame) : EEARust(kEEAUpgrade)
    {
        this->mImprovement = (int32_t)inImprovement;
        this->mRaise = inRaise;
        this->mIsInGame = inIsInGame;
        ee_screens_ga_upgrade_begin(this, inUnit);
    }
};
class EEAUserPauseGame : public EEARust {
public:
    EEAUserPauseGame() : EEARust(kEEAPause) {}
};
// the quick save/load of EEQuickSave<inName> (mIsLoad: load, else save; mWriteToFile: the save written to disk)
class EEAQuickSaveLoad : public EEARust {
public:
    EEAQuickSaveLoad(const UString& inName, uint32_t inKind = kEEAQuick) : EEARust(inKind) { this->mName = inName; }
};
// the scenario editor's: its current tab resumed after
class EESEAQuickSaveLoad : public EEAQuickSaveLoad {
public:
    EESEAQuickSaveLoad(const UString& inName) : EEAQuickSaveLoad(inName, kEEAEditorQuick) {}
};

// The scenario editor's (Scenario Editor Actions). The click and lasso apply their message to the editor's
// selection and refresh the unit tab (the trigger tab's then their object screen); the click's Execute toggles a
// selected unit under the mouse off or clears the selection on empty ground (not while rotating), the lasso's
// SelectInRectangle deselects the selected units inside the rectangle; both post themselves
class EESEAMouseClick : public EEAMouseClick {
public:
    EESEAMouseClick(uint32_t inKind = kEEAEditorClick) : EEAMouseClick(inKind) {}
};
class EESETSAMouseClick : public EESEAMouseClick {
public:
    EESETSAMouseClick() : EESEAMouseClick(kEEATriggerClick) {}
};
class EESEALasso : public EEALasso {
public:
    EESEALasso(uint32_t inKind = kEEAEditorLasso) : EEALasso(inKind) {}
};
class EESETSALasso : public EESEALasso {
public:
    EESETSALasso() : EESEALasso(kEEATriggerLasso) {}
};
// the load thread's text (mText) shown in the editor form; Post sets it and posts the action (EESELoadThread)
class EESEAChangeLoadText : public EEARust {
public:
    EESEAChangeLoadText() : EEARust(kEEALoadText) {}
    void Post(const UWideString& inText);
};
// The paints (kSEAPaintTerrain: a the EETerrainTable index, b the texture (the table entry's mL68); kSEAPaintElevation:
// a the level (a char); kSEAPaintBulldozer): Virtual4 paints each queued area, SetArea queues one when the paint
// changes anything there (the terrain's: any tile on the map; the elevation's: any point off the level's height; the
// bulldozer's: always) and says whether it did (ee_screens editor_actions.rs; the record mPaint)
enum { kSEAPaintTerrain = 0, kSEAPaintElevation = 1, kSEAPaintBulldozer = 2 };   // EE_SCREENS_SEA_PAINT_*
class EESEAPaint : public EEARust {
public:
    EESEAPaint() : EEARust(kEEAEditorPaint) {}
    bool SetArea(long inLeft, long inTop, long inRight, long inBottom)
    {
        return ee_screens_ga_paint_set_area(this, (int32_t)inLeft, (int32_t)inTop, (int32_t)inRight, (int32_t)inBottom);
    }
};
class EESEAPaintRust : public EESEAPaint {
public:
    EESEAPaintRust(uint32_t inKind, int32_t inA = 0, int32_t inB = 0) { this->mPaint = ee_screens_sea_paint_new(inKind, inA, inB); }
};
// the editor's whole actions (ee/EESEARust.h's kSEA* kinds; ee_screens editor_whole.rs, the record mAction): the save,
// test, exit and quick save, the tech lists' move, the rotation, the trigger area's rectangle, the blank and random
// maps, the wall, gate and unit placements, the move and the deletion; the kinds that posted themselves when made
// post when made
class EESEARust : public EEARust {
public:
    EESEARust(uint32_t inKind, void* inObject = 0, int32_t inA = 0, int32_t inB = 0, int32_t inC = 0, int32_t inD = 0)
        : EEARust(kEEAEditorWhole)
    {
        this->mAction = ee_screens_sew_new(static_cast<EEAction*>(this), inKind, inObject, inA, inB, inC, inD);
        ee_screens_sew_begin(this->mAction);
    }
};

// the mini map's pixel-to-map transformation (EEUIMiniMap sets it)
extern GETransformation gMiniMapTransformation;
// the join's message box callback: a fresh client started
void EEAJoinOrHostAGameRestartClient(UIForm* inForm);
// a player name is non-empty and free of control, quote, backslash and @ characters (ee_screens single_player.rs)
bool EEIsValidName(const UWideString& inName);
// the civilization file applied to a player (EECBCivilizationFiles.cpp)
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer);

#endif
