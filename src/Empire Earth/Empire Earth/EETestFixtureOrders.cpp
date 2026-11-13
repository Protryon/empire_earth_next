// New (port), TEST ONLY: the UI fixture's orders (EETestFixture.cpp; on only with EE_TEST_UI). "<seq> order <form>
// <kind>" gives an order to a group of the local player's units through the real input path, then judges every unit
// of the group by the original's rule once the physics has executed the order's message. The group commands' bodies
// (EEMGatherResource, EEMRepairUnit, EEMPatrol, EEMAttackGround, EEMPathTo: rust/crates/ee_units group.rs) are reached
// by no capture game (EE-SIM's human idles), so this is the only run of their world calls (Fill's sorts, the keeps,
// the spreads, SetGoal, Plan, the markers, the acknowledgements) in a played game.
//
// Kinds:
//   0 gather         up to four citizens; a right click on a resource of the world player in view (a tree, a mine)
//                    that a citizen of the group can gather -> EEMGatherResource. With none in view (a random map's
//                    start often shows none), the group first walks (EEMPathTo) to the nearest one it could gather
//                    until the player sees it, the camera is centred on it (EEACenterCamera) and the view scanned
//                    again (note "scouting" meanwhile; "scout-unseen" / "scout-no-pick" when that fails);
//   1 repair         up to four citizens; one of the player's built land buildings in view is damaged first (its hit
//                    points set to half its maximum under the world's write lock: TEST ONLY, there is no game message
//                    for it); a right click on it -> EEMRepairUnit;
//   2 repair-queued  as 1 with Shift held around the click -> EEMRepairUnit queued. With none of the player's
//                    buildings in view (a gather that scouted moved the camera away from the town), the camera is
//                    first centred on one of them (the town centre, else the capitol, else the first built one;
//                    EEACenterCamera) and the view scanned again (note "home" meanwhile, "homed" through the
//                    verdict; "home-no-pick" when the new view still shows none); "no-target" only for a player with
//                    no built land building;
//   3 patrol         up to six mobile units (other than citizens when the player has some); the Patrol button (the
//                    status button of button object 18) clicked -> EEMPatrol(-1), the nearest town's patrol route;
//   4 attack-ground  up to four units whose class can attack the ground; the Attack Ground button (button object 6),
//                    then a left click on a seen point beyond the first unit's minimum range -> EEMAttackGround;
//   5 move           up to six mobile units; a right click on open ground in view with no unit under it -> EEMPathTo.
// The group is selected with the game's EEMSelectTheseUnits (as the unit icon bar sends it), queued as a UI action;
// the order itself goes through the platform's events (ISHandleEvent: the pointer as the "pointer" command sets it,
// the buttons, Shift), the game form's mode, EEAMouseClick or the status button, as a player's input does. The target
// and the click points are found with the game's own picker (EEUnitPicker::PickUnitUnreferenced, the hit test the
// click uses) over a grid of the game view, under the world's read lock.
//
// The verdict: a hook in EEPhysics::DrainInput (gEEMessageExecutedTestHook, NULL unless the fixture runs) sees the
// order's message before and after it executes (the physics thread, the world's write lock held). Before: the
// sender's active group (the units the body works on) and each unit's state the rule reads (its current goal, whether
// it already gathered the target, its build queue). After, each unit, by the original's rule (EEMessage.cpp's body,
// EEActiveGroup::Fill, the keeps, the spreads and Plan, as the C++ reads the objects, not through the unit store):
//   gather   (target T with resources left) a commandable, living citizen: a Gather goal on T when the continents let
//            it reach T's tile (its current goal; the last of its queue when queued and it had a goal), else a
//            Location goal at T's tile; other units: skip (the spread's own group);
//   repair   (T in the world) a commandable, living citizen: T first in its build queue (queued: T in its queue);
//            other units: skip;
//   patrol   (the first unit's nearest town has a patrol route with a point P) a commandable, living mobile unit that
//            can patrol: a Patrol goal on P's route; one that cannot: a Location goal at P; a transport in a
//            container: skip; an immobile unit: its goal unchanged; no route: every goal unchanged;
//   attack   a commandable, living unit that can attack the ground: an Attack Ground goal at the message's point (a
//            transport in a container: skip); the others: their goals unchanged;
//   move     a commandable, living unit with a movement domain: a Location goal (EEMPathTo's formation lays the goals
//            out; their points are not judged); immobile ones: skip.
// "ok" for a unit that has what the rule says, "bad" for one that does not, "skip" where the rule is not judged here.
// A click that gives no message within 10 s is retried once (note "retry", then "retried" through the verdict): the
// camera centred on the target's tile, the view scanned and the plan chosen again, the group selected again; the
// refusal is "no-message" when the second click gives none either ("retry-no-pick" when the new view has no target).
// Every poll writes "order seq <s> kind <k> phase <p> type <t> target <id> point <x> <y> queued <q> units <n> ok <n>
// bad <n> skip <n> note <word>" (phase 1 choosing, 2 the input sent, 3 judged, 9 refused: the note says why) and per
// unit "orderunit <seq> <id> <ok|bad|skip> <expected> <found>".
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSReadWriteLock.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEMap.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEACenterCamera.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEGraphics.h"

extern void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase);   // EEPhysics.cpp (DrainInput)

namespace {

enum {
    kOrderGather = 0,
    kOrderRepair = 1,
    kOrderRepairQueued = 2,
    kOrderPatrol = 3,
    kOrderAttackGround = 4,
    kOrderMove = 5,
    kOrderKinds = 6
};

const unsigned long kButtonPatrol = 18;         // dbbuttons.csv row 18 "patrol" (EEUIFMGStatusButton::Command 0x12)
const unsigned long kButtonAttackGround = 6;    // dbbuttons.csv row 6 "attack_ground" (Command 6: UI state 0x13)

// a unit's state before the message, as the rule reads it
struct OrderUnit {
    long mID = -1;
    EEUnitGoal* mGoal = NULL;               // the current goal (compared by address: "unchanged")
    bool mHadGoal = false;
    bool mGatheringTarget = false;          // its current goal gathered the target already
    std::string mVerdict, mExpected, mFound;
};

struct OrderState {
    unsigned long mSeq = 0;
    int mKind = -1;
    int mPhase = 0;
    std::string mNote = "-";
    unsigned long mType = 0;                // the message type the hook waits for
    bool mArmed = false;
    bool mBefore = false;                   // the hook saw the message before its execution
    long mTarget = -1;
    float mX = 0.0f, mY = 0.0f;
    int mQueued = 0;
    long mRoutePoint = -1;                  // patrol: the town's point nearest the first unit (-1: none)
    EEPPatrolGroup* mRoute = NULL;
    std::vector<OrderUnit> mUnits;
};

std::mutex sOrderMutex;
OrderState sOrder;
std::atomic<int> sOrderBusy(0);

// ---- reads (the world's lock held by the caller)

EEComplexUnit* OwnComplex(long inID)
{
    RTSUnit* theUnit = gWorld.GetUnit((unsigned long)inID);
    if (theUnit == NULL)
        return NULL;
    EEUnit* theEEUnit = static_cast<EEUnit*>(theUnit);
    EEComplexUnit* theComplex = theEEUnit->mComplexUnit.Get();
    return static_cast<EEUnit*>(theComplex) == theEEUnit ? theComplex : NULL;
}

bool IsCitizen(EEUnit* inUnit)
{
    unsigned long theCategory = inUnit->mUnitClass->mCategory;
    return theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen;
}

bool IsLiving(EEComplexUnit* inUnit)
{
    return inUnit->mIsValid && inUnit->mHitPoints > 0;
}

bool IsTransportInContainer(EEComplexUnit* inUnit)
{
    unsigned long theCategory = inUnit->mUnitClass->mCategory;
    return (theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship) && inUnit->mContainer != NULL;
}

bool IsMobile(EEComplexUnit* inUnit)
{
    return inUnit->mStats->GetMaxVelocity() != 0.0f || inUnit->mUnitClass->mUnitType == kUnitTypeAirport;
}

std::string Number(long inValue)
{
    char theText[24];
    snprintf(theText, sizeof(theText), "%ld", inValue);
    return theText;
}

std::string Point(float inX, float inY)
{
    char theText[64];
    snprintf(theText, sizeof(theText), "%.3f,%.3f", inX, inY);
    return theText;
}

long IDOf(EEUnit* inUnit)
{
    return inUnit != NULL ? (long)inUnit->mUnitID : -1;
}

// a goal as "<kind>:<its target>" (none: "none")
std::string GoalText(EEUnitGoal* inGoal)
{
    if (inGoal == NULL)
        return "none";
    std::string theText = Number(inGoal->mKind) + ":";
    switch (inGoal->mKind) {
    case kGoalGather:
        return "gather:" + Number(IDOf(static_cast<EEUGGather*>(inGoal)->mResource));
    case kGoalLocation:
        return "location:" + Point(static_cast<EEUGLocation*>(inGoal)->GetLocation().GetX(),
                                   static_cast<EEUGLocation*>(inGoal)->GetLocation().GetY());
    case kGoalAttackUnit:
        return "attack:" + Number(IDOf(static_cast<EEUGAttackUnit*>(inGoal)->mTarget));
    case kGoalAttackGround:
        return "ground:" + Point(static_cast<EEUGAttackGround*>(inGoal)->mTarget.GetX(),
                                 static_cast<EEUGAttackGround*>(inGoal)->mTarget.GetY());
    case kGoalPatrol:
        return "patrol:" + Number(IDOf(static_cast<EEUGRust*>(inGoal)->GetPatrolTarget()));
    case kGoalBuildRepair:
        return "build";
    case kGoalExplore:
        return "explore";
    default:
        return theText + "-";
    }
}

EEUnitGoal* LastQueued(EEComplexUnit* inUnit)
{
    if (inUnit->mGoalQueues.empty() || inUnit->mGoalQueues[0].mGoals.empty())
        return NULL;
    return inUnit->mGoalQueues[0].mGoals.back();
}

std::string QueueText(EEComplexUnit* inUnit)
{
    std::string theText = "queue:";
    if (inUnit->mCitizenBuildQueue.empty())
        return theText + "-";
    for (size_t i = 0; i < inUnit->mCitizenBuildQueue.size() && i < 4; i++)
        theText += (i ? "," : "") + Number(IDOf(inUnit->mCitizenBuildQueue[i]));
    return theText;
}

// ---- the verdicts (the physics thread, the world's write lock held)

void Judge(OrderUnit& ioUnit, bool inOK, const std::string& inExpected, const std::string& inFound)
{
    ioUnit.mVerdict = inOK ? "ok" : "bad";
    ioUnit.mExpected = inExpected;
    ioUnit.mFound = inFound;
}

void Skip(OrderUnit& ioUnit, const char* inWhy, const std::string& inFound)
{
    ioUnit.mVerdict = "skip";
    ioUnit.mExpected = inWhy;
    ioUnit.mFound = inFound;
}

void JudgeUnchanged(OrderUnit& ioUnit, EEComplexUnit* inUnit)
{
    Judge(ioUnit, inUnit->mGoalState.Get() == ioUnit.mGoal, "unchanged:" + GoalText(ioUnit.mGoal),
          GoalText(inUnit->mGoalState.Get()));
}

void JudgeGather(OrderState& ioState)
{
    EEUnit* theTarget = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)ioState.mTarget));
    for (size_t i = 0; i < ioState.mUnits.size(); i++) {
        OrderUnit& theRow = ioState.mUnits[i];
        EEComplexUnit* theUnit = OwnComplex(theRow.mID);
        if (theUnit == NULL) {
            Skip(theRow, "gone", "-");
            continue;
        }
        std::string theFound = GoalText(theUnit->mGoalState.Get());
        if (theTarget == NULL || ioState.mNote == "no-resource") {
            JudgeUnchanged(theRow, theUnit);
            continue;
        }
        if (!IsLiving(theUnit) || !IsCitizen(theUnit) || !theUnit->mIsCommandable) {
            Skip(theRow, "not-a-commandable-living-citizen", theFound);
            continue;
        }
        long theX = theTarget->GetTileX(), theY = theTarget->GetTileY();
        bool theQueued = ioState.mQueued && theRow.mHadGoal && !theRow.mGatheringTarget;
        EEUnitGoal* theGoal = theQueued ? LastQueued(theUnit) : theUnit->mGoalState.Get();
        std::string theGot = GoalText(theGoal);
        if (gContinentManager.CanGetTo(theUnit, theX, theY)) {
            bool theOK = theGoal != NULL && theGoal->mKind == kGoalGather
                && static_cast<EEUGGather*>(theGoal)->mResource == theTarget;
            Judge(theRow, theOK, "gather:" + Number(ioState.mTarget), theGot);
        } else {
            bool theOK = theGoal != NULL && theGoal->mKind == kGoalLocation
                && static_cast<EEUGLocation*>(theGoal)->GetLocation().GetX() == (float)theX
                && static_cast<EEUGLocation*>(theGoal)->GetLocation().GetY() == (float)theY;
            Judge(theRow, theOK, "location:" + Point((float)theX, (float)theY), theGot);
        }
    }
}

void JudgeRepair(OrderState& ioState)
{
    EEUnit* theTarget = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)ioState.mTarget));
    for (size_t i = 0; i < ioState.mUnits.size(); i++) {
        OrderUnit& theRow = ioState.mUnits[i];
        EEComplexUnit* theUnit = OwnComplex(theRow.mID);
        if (theUnit == NULL) {
            Skip(theRow, "gone", "-");
            continue;
        }
        if (theTarget == NULL) {
            JudgeUnchanged(theRow, theUnit);
            continue;
        }
        std::string theFound = QueueText(theUnit);
        if (!IsLiving(theUnit) || !IsCitizen(theUnit) || !theUnit->mIsCommandable) {
            Skip(theRow, "not-a-commandable-living-citizen", theFound);
            continue;
        }
        std::deque<EEUnit*>& theQueue = theUnit->mCitizenBuildQueue;
        if (ioState.mQueued) {
            bool theOK = std::find(theQueue.begin(), theQueue.end(), theTarget) != theQueue.end();
            Judge(theRow, theOK, "queue-has:" + Number(ioState.mTarget), theFound);
        } else {
            bool theOK = !theQueue.empty() && theQueue.front() == theTarget;
            Judge(theRow, theOK, "queue-first:" + Number(ioState.mTarget), theFound);
        }
    }
}

// EEActiveGroup::IsInFormationSlot for some slot 1..5
bool InSomeSlot(EEComplexUnit* inUnit)
{
    return (inUnit->mUnitClass->GetDomains() & 0xf) != 0 || inUnit->mUnitClass->IsLandBuilding();
}

void JudgePatrol(OrderState& ioState)
{
    EEComplexUnit* thePoint = ioState.mRoutePoint >= 0 ? OwnComplex(ioState.mRoutePoint) : NULL;
    for (size_t i = 0; i < ioState.mUnits.size(); i++) {
        OrderUnit& theRow = ioState.mUnits[i];
        EEComplexUnit* theUnit = OwnComplex(theRow.mID);
        if (theUnit == NULL) {
            Skip(theRow, "gone", "-");
            continue;
        }
        std::string theFound = GoalText(theUnit->mGoalState.Get());
        if (thePoint == NULL || theUnit->mUnitClass->mUnitType == kUnitTypePathPoint) {
            JudgeUnchanged(theRow, theUnit);
            continue;
        }
        if (!IsLiving(theUnit) || !InSomeSlot(theUnit)) {
            Skip(theRow, "not-taken", theFound);
            continue;
        }
        if (!theUnit->mIsCommandable || !IsMobile(theUnit)) {
            JudgeUnchanged(theRow, theUnit);
            continue;
        }
        if (IsTransportInContainer(theUnit)) {
            Skip(theRow, "transport-in-container", theFound);
            continue;
        }
        EEUnitGoal* theGoal = theUnit->mGoalState.Get();
        if (!theUnit->mUnitClass->mCanPatrol
            && (!theUnit->mIsDetachedFromAI || theUnit->mUnitClass->FindAction(kActionGetNextPatrolPoint) == NULL)) {
            float theX = thePoint->mPhysicalModel.GetLocation().GetX();
            float theY = thePoint->mPhysicalModel.GetLocation().GetY();
            bool theOK = theGoal != NULL && theGoal->mKind == kGoalLocation
                && static_cast<EEUGLocation*>(theGoal)->GetLocation().GetX() == theX
                && static_cast<EEUGLocation*>(theGoal)->GetLocation().GetY() == theY;
            Judge(theRow, theOK, "location:" + Point(theX, theY), theFound);
        } else {
            bool theOK = theGoal != NULL && theGoal->mKind == kGoalPatrol
                && static_cast<EEUGRust*>(theGoal)->GetPatrolGroup() == ioState.mRoute;
            Judge(theRow, theOK, "patrol-route-of:" + Number(ioState.mRoutePoint), theFound);
        }
    }
}

void JudgeAttackGround(OrderState& ioState)
{
    for (size_t i = 0; i < ioState.mUnits.size(); i++) {
        OrderUnit& theRow = ioState.mUnits[i];
        EEComplexUnit* theUnit = OwnComplex(theRow.mID);
        if (theUnit == NULL) {
            Skip(theRow, "gone", "-");
            continue;
        }
        std::string theFound = GoalText(theUnit->mGoalState.Get());
        if (!IsLiving(theUnit) || !InSomeSlot(theUnit)) {
            Skip(theRow, "not-taken", theFound);
            continue;
        }
        if (!theUnit->mUnitClass->mCanAttackGround || !theUnit->mIsCommandable) {
            JudgeUnchanged(theRow, theUnit);
            continue;
        }
        if (IsTransportInContainer(theUnit)) {
            Skip(theRow, "transport-in-container", theFound);
            continue;
        }
        EEUnitGoal* theGoal = theUnit->mGoalState.Get();
        bool theOK = theGoal != NULL && theGoal->mKind == kGoalAttackGround
            && static_cast<EEUGAttackGround*>(theGoal)->mTarget.GetX() == ioState.mX
            && static_cast<EEUGAttackGround*>(theGoal)->mTarget.GetY() == ioState.mY;
        Judge(theRow, theOK, "ground:" + Point(ioState.mX, ioState.mY), theFound);
    }
}

void JudgeMove(OrderState& ioState)
{
    for (size_t i = 0; i < ioState.mUnits.size(); i++) {
        OrderUnit& theRow = ioState.mUnits[i];
        EEComplexUnit* theUnit = OwnComplex(theRow.mID);
        if (theUnit == NULL) {
            Skip(theRow, "gone", "-");
            continue;
        }
        EEUnitGoal* theGoal = theUnit->mGoalState.Get();
        std::string theFound = GoalText(theGoal);
        if (!IsLiving(theUnit) || !theUnit->mIsCommandable || !IsMobile(theUnit) || IsTransportInContainer(theUnit)) {
            Skip(theRow, "not-a-commandable-living-mobile-unit", theFound);
            continue;
        }
        Judge(theRow, theGoal != NULL && theGoal->mKind == kGoalLocation, "location", theFound);
    }
}

// the physics thread (DrainInput), under the world's write lock: inPhase 0 before the message executes, 1 after
void MessageHook(NEMessage* inMessage, int inPhase)
{
    std::lock_guard<std::mutex> theLock(sOrderMutex);
    if (!sOrder.mArmed || inMessage->GetType() != sOrder.mType)
        return;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (thePlayer == NULL || gWorld.GetPlayerBySlot(inMessage->GetOriginatingSlot()) != thePlayer)
        return;
    if (inPhase == 0) {
        sOrder.mBefore = true;
        sOrder.mUnits.clear();
        switch (sOrder.mKind) {
        case kOrderGather: {
            EEMGatherResource* theMessage = static_cast<EEMGatherResource*>(inMessage);
            sOrder.mTarget = theMessage->mTargetID;
            sOrder.mQueued = theMessage->mIsQueued ? 1 : 0;
            break;
        }
        case kOrderRepair:
        case kOrderRepairQueued: {
            EEMRepairUnit* theMessage = static_cast<EEMRepairUnit*>(inMessage);
            sOrder.mTarget = theMessage->mTargetID;
            sOrder.mQueued = theMessage->mIsQueued ? 1 : 0;
            break;
        }
        case kOrderPatrol:
            sOrder.mTarget = static_cast<EEMPatrol*>(inMessage)->mTargetID;
            break;
        case kOrderAttackGround:
            sOrder.mX = static_cast<EEMAttackGround*>(inMessage)->mX;
            sOrder.mY = static_cast<EEMAttackGround*>(inMessage)->mY;
            break;
        default:
            break;
        }
        EEUnit* theTarget = sOrder.mTarget >= 0 ? static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)sOrder.mTarget)) : NULL;
        if (sOrder.mKind == kOrderGather && theTarget != NULL && theTarget->mResourceAmount <= 0)
            sOrder.mNote = "no-resource";
        EESelectedGroup* theGroup = thePlayer->mActiveGroup;
        for (size_t i = 0; theGroup != NULL && i < theGroup->mUnits.size(); i++) {
            EEComplexUnit* theUnit = theGroup->mUnits[i];
            if (theUnit == NULL)
                continue;
            OrderUnit theRow;
            theRow.mID = (long)theUnit->mUnitID;
            theRow.mGoal = theUnit->mGoalState.Get();
            theRow.mHadGoal = theRow.mGoal != NULL;
            theRow.mGatheringTarget = theRow.mGoal != NULL && theRow.mGoal->mKind == kGoalGather && theTarget != NULL
                && static_cast<EEUGGather*>(theRow.mGoal)->mResource == theTarget;
            bool theListed = false;
            for (size_t j = 0; j < sOrder.mUnits.size(); j++)
                theListed = theListed || sOrder.mUnits[j].mID == theRow.mID;
            if (!theListed)
                sOrder.mUnits.push_back(theRow);
        }
        if (sOrder.mKind == kOrderPatrol && theGroup != NULL && !theGroup->mUnits.empty() && theGroup->mUnits[0] != NULL
            && sOrder.mTarget == -1) {   // EEMPatrol(-1): the first unit's nearest town's route (EEUnitHost town point)
            EEComplexUnit* theFirst = theGroup->mUnits[0];
            EETown* theTown = theFirst->FindNearestTown();
            if (theTown != NULL && theTown->mPatrolGroup != NULL) {
                EEComplexUnit* thePoint = theTown->mPatrolGroup->GetNearest(theFirst->GetTileX(), theFirst->GetTileY());
                if (thePoint != NULL) {
                    sOrder.mRoutePoint = (long)thePoint->mUnitID;
                    sOrder.mRoute = thePoint->mPatrolGroup;
                }
            }
            if (sOrder.mRoutePoint < 0)
                sOrder.mNote = "no-route";
        }
        return;
    }
    if (!sOrder.mBefore)
        return;
    switch (sOrder.mKind) {
    case kOrderGather:
        JudgeGather(sOrder);
        break;
    case kOrderRepair:
    case kOrderRepairQueued:
        JudgeRepair(sOrder);
        break;
    case kOrderPatrol:
        JudgePatrol(sOrder);
        break;
    case kOrderAttackGround:
        JudgeAttackGround(sOrder);
        break;
    default:
        JudgeMove(sOrder);
        break;
    }
    sOrder.mRoute = NULL;   // never dereferenced after this
    sOrder.mArmed = false;
    sOrder.mPhase = 3;
}

// ---- the input (the order's own thread; never under the UI lock)

class OrderSendAction : public EEAction {
public:
    explicit OrderSendAction(NEMessage* inMessage) : mMessage(inMessage) {}
    virtual ~OrderSendAction()
    {
        if (this->mMessage)
            this->mMessage->Dereference();
    }
    virtual void Virtual4()
    {
        if (this->mMessage) {
            gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
            this->mMessage = NULL;
        }
    }
    virtual void Virtual8() {}
    NEMessage* mMessage;
};

void Post(NEMessage* inMessage)
{
    OrderSendAction* theAction = new OrderSendAction(inMessage);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

void Pointer(float inX, float inY)
{
    gMouse->mInputLock->Enter();
    gMouse->SetPosition(GE2DPoint(inX, inY));
    gMouse->mInputLock->Leave();
    gUIManager->ForceUIUpdate();
}

void Event(uint32_t inType, uint32_t inCode)
{
    PlatInputEvent theEvent;
    memset(&theEvent, 0, sizeof(theEvent));
    theEvent.mType = inType;
    if (inType == PLAT_EVENT_KEY_DOWN || inType == PLAT_EVENT_KEY_UP)
        theEvent.mKey.mScanCode = inCode;
    else
        theEvent.mMouse.mButton = inCode;
    ISHandleEvent(theEvent);
}

void Wait(int inMilliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(inMilliseconds));
}

// a press and release of a mouse button (1 left, 3 right) at (inX, inY), Shift held around it when asked
void Click(float inX, float inY, uint32_t inButton, bool inShift)
{
    const uint32_t kLeftShift = 0x2a;        // DIK_LSHIFT
    Pointer(inX, inY);
    Wait(300);
    if (inShift) {
        Event(PLAT_EVENT_KEY_DOWN, kLeftShift);
        Wait(150);
    }
    Event(PLAT_EVENT_MOUSE_DOWN, inButton);
    Wait(200);
    Event(PLAT_EVENT_MOUSE_UP, inButton);
    Wait(300);
    if (inShift)
        Event(PLAT_EVENT_KEY_UP, kLeftShift);
}

void Refuse(const char* inNote)
{
    std::lock_guard<std::mutex> theLock(sOrderMutex);
    sOrder.mPhase = 9;
    sOrder.mNote = inNote;
    sOrder.mArmed = false;
}

// the first grid point (step inStep) of the game view where the picker finds each unit, under the world's read lock
// (inPrefer: PickUnit's resource preference, as the click will pass it); the points with no unit in outEmpty
void Scan(long inPrefer, std::map<long, GE2DPoint>& outPoints, std::vector<GE2DPoint>& outEmpty)
{
    GERectangle theView = gViewport.GetViewRectangle();
    const float theStep = 12.0f;
    for (float y = theView.GetTop() + 24.0f; y < theView.GetBottom() - 24.0f; y += theStep)
        for (float x = theView.GetLeft() + 24.0f; x < theView.GetRight() - 24.0f; x += theStep) {
            GE2DPoint thePoint(x, y);
            EEUnit* theUnit = gUnitPicker.PickUnitUnreferenced(thePoint, inPrefer);
            if (theUnit == NULL)
                outEmpty.push_back(thePoint);
            else if (outPoints.find((long)theUnit->mUnitID) == outPoints.end())
                outPoints[(long)theUnit->mUnitID] = thePoint;
        }
}

// the visible status button of the game form for button object inButton -> its centre (false: none)
bool FindButton(unsigned long inButton, GE2DPoint& outCentre)
{
    bool theFound = false;
    gUIManager->LockUIForCopyData();
    UIForm* theForm = gUIManager->GetCurrentForm();
    for (unsigned long i = 0; theForm != NULL && i < theForm->GetControlCount() && !theFound; i++) {
        EEUIFMGStatusButton* theButton = dynamic_cast<EEUIFMGStatusButton*>(theForm->GetControl(i));
        if (theButton == NULL || !theButton->mIsCommand || theButton->mButton != inButton
            || theButton->GetDisplayMode() != kDisplayModeShown)
            continue;
        const GERectangle& theRectangle = theButton->GetRectangle();
        outCentre = GE2DPoint((theRectangle.GetLeft() + theRectangle.GetRight()) * 0.5f,
                              (theRectangle.GetTop() + theRectangle.GetBottom()) * 0.5f);
        theFound = true;
    }
    gUIManager->UnlockUIForCopyData();
    return theFound;
}

// the local player's selected group (the UI's) equals inIDs (as sets)
bool Selected(const std::vector<long>& inIDs)
{
    gWorld.mLock->ReadLock();
    std::vector<long> theGroup;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* theSelected = thePlayer != NULL ? thePlayer->mSelectedGroup : NULL;
    for (size_t i = 0; theSelected != NULL && i < theSelected->mUnits.size(); i++)
        if (theSelected->mUnits[i] != NULL)
            theGroup.push_back((long)theSelected->mUnits[i]->mUnitID);
    gWorld.mLock->ReadUnlock();
    std::vector<long> theWanted(inIDs);
    std::sort(theGroup.begin(), theGroup.end());
    std::sort(theWanted.begin(), theWanted.end());
    return theGroup == theWanted;
}

struct OrderPlan {
    std::vector<long> mUnits;
    GE2DPoint mClick;                       // the target's point (gather, repair, move) or the ground point (attack)
    long mTarget = -1;
    long mDamage = -1;                      // repair: the building to damage
    const char* mRefusal = NULL;
    long mScout = -1;                       // gather with nothing in view: the nearest resource the group can gather
    long mScoutX = 0, mScoutY = 0;          // its tile
    long mHome = -1;                        // repair with none of the player's buildings in view: one of them
    long mHomeX = 0, mHomeY = 0;            // its tile
};

// repair (the world's read lock held): a building the repair could target were it in view (built, living, a land
// building with hit points to lose), into ioPlan.mHome (-1: none): the town centre first, else the capitol, else the
// first one in the player's list
void HomeBuilding(EEPlayer* inPlayer, OrderPlan& ioPlan)
{
    int theRank = 3;
    for (RTSUnit* theNode = inPlayer->mFirstUnit; theNode != NULL; theNode = theNode->mNext) {
        EEComplexUnit* theUnit = OwnComplex((long)theNode->mUnitID);
        if (theUnit == NULL || !theUnit->mUnitClass->IsLandBuilding() || !theUnit->mIsBuilt || !IsLiving(theUnit)
            || theUnit->GetTileX() == -10000 || theUnit->GetStats()->GetMaxHitPoints() < 2)
            continue;
        unsigned long theType = theUnit->mUnitClass->mUnitType;
        int theUnitRank = theType == kUnitTypeTownCenter ? 0 : theType == kUnitTypeCapitol ? 1 : 2;
        if (theUnitRank >= theRank)
            continue;
        theRank = theUnitRank;
        ioPlan.mHome = (long)theUnit->mUnitID;
        ioPlan.mHomeX = theUnit->GetTileX();
        ioPlan.mHomeY = theUnit->GetTileY();
    }
}

// gather (the world's read lock held): the world player's resource nearest inWorker that it could gather were its tile
// in view and that it can reach, into ioPlan.mScout (-1: none). A random map's start often shows none: the human's
// citizens idle and the start's line of sight is short (the lead's runs 2026-11-11: an arctic start and a grass one,
// no tree or mine in view or explored).
void ScoutTarget(EEComplexUnit* inWorker, OrderPlan& ioPlan)
{
    EEPlayer* theWorld = gWorld.mPlayers[0];
    long theBest = -1;
    for (RTSUnit* theNode = theWorld != NULL ? theWorld->mFirstUnit : NULL; theNode != NULL; theNode = theNode->mNext) {
        EEUnit* theUnit = static_cast<EEUnit*>(theNode);
        long theX = theUnit->GetTileX(), theY = theUnit->GetTileY();
        if (!theUnit->mIsValid || theX == -10000 || theUnit->mResourceAmount <= 0
            || theUnit->mUnitClass->mFamily == kUnitFamilyAnimal
            || !inWorker->CanGather(theUnit, theX, theY, true, false)
            || !gContinentManager.CanGetTo(inWorker, theX, theY))
            continue;
        long theDX = theX - inWorker->GetTileX(), theDY = theY - inWorker->GetTileY();
        long theDistance = theDX * theDX + theDY * theDY;
        if (theBest >= 0 && theDistance >= theBest)
            continue;
        theBest = theDistance;
        ioPlan.mScout = (long)theUnit->mUnitID;
        ioPlan.mScoutX = theX;
        ioPlan.mScoutY = theY;
    }
}

// chooses the units, the target and the click point (the world's read lock held)
void Choose(int inKind, OrderPlan& outPlan)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (thePlayer == NULL) {
        outPlan.mRefusal = "no-player";
        return;
    }
    std::vector<EEComplexUnit*> theCitizens, theMobile, theOthers, theGround;
    for (RTSUnit* theNode = thePlayer->mFirstUnit; theNode != NULL; theNode = theNode->mNext) {
        EEComplexUnit* theUnit = OwnComplex((long)theNode->mUnitID);
        if (theUnit == NULL || !IsLiving(theUnit) || !theUnit->mIsBuilt || !theUnit->mIsCommandable
            || theUnit->mUnitClass->mUnitType == kUnitTypePathPoint || theUnit->GetTileX() == -10000)
            continue;
        if (!IsMobile(theUnit))
            continue;
        theMobile.push_back(theUnit);
        if (IsCitizen(theUnit))
            theCitizens.push_back(theUnit);
        else
            theOthers.push_back(theUnit);
        if (theUnit->mUnitClass->mCanAttackGround)
            theGround.push_back(theUnit);
    }
    std::vector<EEComplexUnit*>* theGroup = &theCitizens;
    size_t theLimit = 4;
    if (inKind == kOrderPatrol || inKind == kOrderMove) {
        theGroup = theOthers.empty() ? &theMobile : &theOthers;
        theLimit = 6;
    } else if (inKind == kOrderAttackGround)
        theGroup = &theGround;
    for (size_t i = 0; i < theGroup->size() && outPlan.mUnits.size() < theLimit; i++)
        outPlan.mUnits.push_back((long)(*theGroup)[i]->mUnitID);
    if (outPlan.mUnits.empty()) {
        outPlan.mRefusal = "no-unit";
        return;
    }
    if (inKind == kOrderPatrol)
        return;
    std::map<long, GE2DPoint> thePoints;
    std::vector<GE2DPoint> theEmpty;
    Scan(inKind == kOrderGather || inKind == kOrderRepair || inKind == kOrderRepairQueued ? 1 : 0, thePoints, theEmpty);
    if (inKind == kOrderGather) {
        EEComplexUnit* theWorker = OwnComplex(outPlan.mUnits[0]);
        for (std::map<long, GE2DPoint>::iterator it = thePoints.begin(); it != thePoints.end(); ++it) {
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)it->first));
            if (theUnit == NULL || theUnit->mPlayer == thePlayer || theUnit->mPlayer->mPlayerID != 0
                || theUnit->mResourceAmount <= 0 || theUnit->mUnitClass->mFamily == kUnitFamilyAnimal
                || !EEIsSeenBy(theUnit, thePlayer, true)
                || !theWorker->CanGather(theUnit, theUnit->GetTileX(), theUnit->GetTileY(), false, false))
                continue;
            outPlan.mTarget = it->first;
            outPlan.mClick = it->second;
            if (theUnit->mUnitClass->GetIsTree())   // a tree first, else the first resource found
                break;
        }
        if (outPlan.mTarget < 0) {
            outPlan.mRefusal = "no-target";
            ScoutTarget(theWorker, outPlan);
        }
    } else if (inKind == kOrderRepair || inKind == kOrderRepairQueued) {
        for (std::map<long, GE2DPoint>::iterator it = thePoints.begin(); it != thePoints.end(); ++it) {
            EEComplexUnit* theUnit = OwnComplex(it->first);
            if (theUnit == NULL || theUnit->mPlayer != thePlayer || !theUnit->mUnitClass->IsLandBuilding()
                || !theUnit->mIsBuilt || !IsLiving(theUnit) || theUnit->GetStats()->GetMaxHitPoints() < 2)
                continue;
            outPlan.mTarget = it->first;
            outPlan.mClick = it->second;
            if (theUnit->mHitPoints < theUnit->GetStats()->GetMaxHitPoints())   // a damaged one first
                break;
        }
        if (outPlan.mTarget < 0) {
            outPlan.mRefusal = "no-target";
            HomeBuilding(thePlayer, outPlan);
        } else
            outPlan.mDamage = outPlan.mTarget;
    } else {   // attack ground, move: a point in view with no unit under it, on a tile the player sees
        EEComplexUnit* theFirst = OwnComplex(outPlan.mUnits[0]);
        float theMin = theFirst->mUnitClass->mMinRange;
        for (size_t i = 0; i < theEmpty.size(); i++) {
            size_t k = (i * 7919) % theEmpty.size();   // spread over the view, not its first row
            EETile* theTile = NULL;
            GE3DPoint theWorld(0.0f, 0.0f, 0.0f);
            if (!EEPointUnitOrTile(theEmpty[k], theTile, theWorld, true) || theTile == NULL || !theTile->IsSeenBy(thePlayer))
                continue;
            if (inKind == kOrderAttackGround && theFirst->DistanceSquared3D(theWorld) < theMin * theMin)
                continue;
            outPlan.mClick = theEmpty[k];
            outPlan.mTarget = 0;
            break;
        }
        if (outPlan.mTarget < 0)
            outPlan.mRefusal = "no-point";
        outPlan.mTarget = -1;
    }
}

// gather with no resource in view: the group selected and sent (EEMPathTo, as a UI action) to the nearest resource
// ScoutTarget found, until the player sees its tile (at most 60 s); then the camera centred on it (EEACenterCamera,
// as the mini map's click does) and the plan chosen again from the new view (ioPlan: the new plan, or "no-target")
void Scout(int inKind, OrderPlan& ioPlan)
{
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        sOrder.mNote = "scouting";
    }
    long theScout = ioPlan.mScout;
    long theX = ioPlan.mScoutX, theY = ioPlan.mScoutY;
    long theWorkerID = ioPlan.mUnits[0];
    Post(new EEMSelectTheseUnits(ioPlan.mUnits));
    bool theSelected = false;
    for (int k = 0; k < 40 && !theSelected; k++) {
        Wait(150);
        theSelected = Selected(ioPlan.mUnits);
    }
    if (theSelected)
        Post(new EEMPathTo((float)theX + 0.5f, (float)theY + 0.5f, 0.0f, false, false));
    bool theSeen = false;
    for (int k = 0; theSelected && k < 120 && !theSeen; k++) {
        Wait(500);
        gWorld.mLock->ReadLock();
        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)theScout));
        EEComplexUnit* theWorker = OwnComplex(theWorkerID);
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        theSeen = theUnit != NULL && theWorker != NULL && thePlayer != NULL && theUnit->mResourceAmount > 0
            && EEIsSeenBy(theUnit, thePlayer, true) && theWorker->CanGather(theUnit, theX, theY, false, false);
        gWorld.mLock->ReadUnlock();
    }
    OrderPlan thePlan;
    if (theSeen) {
        EEACenterCamera* theAction = new EEACenterCamera;   // posted outside the world's lock, as Post does
        theAction->SetLocation(theX, theY);
        theAction->Dereference();
        Wait(1500);   // frames drawn from the new view fill the picker
        gWorld.mLock->ReadLock();
        Choose(inKind, thePlan);
        gWorld.mLock->ReadUnlock();
    }
    if (!theSelected)
        thePlan.mRefusal = "not-selected";
    else if (!theSeen)
        thePlan.mRefusal = "scout-unseen";     // not a skip: the driver fails the step with this note
    else if (thePlan.mTarget < 0)
        thePlan.mRefusal = "scout-no-pick";
    thePlan.mScout = -1;
    ioPlan = thePlan;
    std::lock_guard<std::mutex> theLock(sOrderMutex);
    sOrder.mNote = "-";
}

// repair with none of the player's buildings in view (the lead's run 2026-11-12, orders_b10.log: right after a gather
// that scouted, the camera was away from the town): the camera centred on the building HomeBuilding found
// (EEACenterCamera, as the mini map's click does) and the plan chosen again from the new view (ioPlan: the new plan,
// or "home-no-pick")
void Home(int inKind, OrderPlan& ioPlan)
{
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        sOrder.mNote = "home";
    }
    EEACenterCamera* theAction = new EEACenterCamera;   // posted outside the world's lock, as Post does
    theAction->SetLocation(ioPlan.mHomeX, ioPlan.mHomeY);
    theAction->Dereference();
    Wait(1500);   // frames drawn from the new view fill the picker
    OrderPlan thePlan;
    gWorld.mLock->ReadLock();
    Choose(inKind, thePlan);
    gWorld.mLock->ReadUnlock();
    if (thePlan.mTarget < 0)
        thePlan.mRefusal = "home-no-pick";     // not a skip: the driver fails the step with this note
    thePlan.mHome = -1;
    ioPlan = thePlan;
    std::lock_guard<std::mutex> theLock(sOrderMutex);
    sOrder.mNote = thePlan.mRefusal == NULL ? "homed" : "-";   // kept through the verdict: the driver logs it
}

// the group inUnits selected (EEMSelectTheseUnits unless it already is) -> whether the UI's selection matches
bool SelectGroup(std::vector<long>& inUnits)
{
    if (Selected(inUnits))
        return true;
    Post(new EEMSelectTheseUnits(inUnits));
    for (int k = 0; k < 40; k++) {
        Wait(150);
        if (Selected(inUnits))
            return true;
    }
    return false;
}

// the order's input (the click on the plan's point, or the status button inButton, then the ground point) and the
// wait for its message's verdict (at most 10 s) -> the phase then (2: no message executed)
int SendOrder(int inKind, const OrderPlan& inPlan, const GE2DPoint& inButton)
{
    switch (inKind) {
    case kOrderGather:
    case kOrderRepair:
    case kOrderMove:
        Click(inPlan.mClick.GetX(), inPlan.mClick.GetY(), 3, false);
        break;
    case kOrderRepairQueued:
        Click(inPlan.mClick.GetX(), inPlan.mClick.GetY(), 3, true);
        break;
    case kOrderPatrol:
        Click(inButton.GetX(), inButton.GetY(), 1, false);
        break;
    case kOrderAttackGround:
        Click(inButton.GetX(), inButton.GetY(), 1, false);
        Wait(400);
        Click(inPlan.mClick.GetX(), inPlan.mClick.GetY(), 1, false);
        break;
    }
    int thePhase = 2;
    for (int k = 0; k < 100 && thePhase == 2; k++) {
        Wait(100);
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        thePhase = sOrder.mPhase;
    }
    return thePhase;
}

// the retry after a click that gave no message (the lead's run 2026-11-11, orders_b6.log: a repair right after a
// gather that scouted for 14 s; the click on the building gave none): the camera centred on the target's tile (a unit
// target), the view scanned and the plan chosen again (ioPlan), the group selected again, the button found again
// -> the refusal (NULL: ready for the second click)
const char* RetryPlan(int inKind, OrderPlan& ioPlan, GE2DPoint& outButton)
{
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        sOrder.mNote = "retry";
    }
    long theX = -10000, theY = -10000;
    if (ioPlan.mTarget >= 0) {
        gWorld.mLock->ReadLock();
        EEUnit* theTarget = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)ioPlan.mTarget));
        if (theTarget != NULL) {
            theX = theTarget->GetTileX();
            theY = theTarget->GetTileY();
        }
        gWorld.mLock->ReadUnlock();
    }
    if (theX != -10000) {
        EEACenterCamera* theAction = new EEACenterCamera;   // posted outside the world's lock, as Post does
        theAction->SetLocation(theX, theY);
        theAction->Dereference();
    }
    Wait(1500);   // frames drawn from the new view fill the picker
    OrderPlan thePlan;
    gWorld.mLock->ReadLock();
    Choose(inKind, thePlan);
    gWorld.mLock->ReadUnlock();
    if (thePlan.mRefusal != NULL)
        return "retry-no-pick";
    if (thePlan.mDamage >= 0 && thePlan.mDamage != ioPlan.mDamage) {   // TEST ONLY, as OrderRun's first damage
        gWorld.mLock->WriteLock();
        EEComplexUnit* theBuilding = OwnComplex(thePlan.mDamage);
        if (theBuilding != NULL && theBuilding->mHitPoints > theBuilding->GetStats()->GetMaxHitPoints() / 2)
            theBuilding->mHitPoints = theBuilding->GetStats()->GetMaxHitPoints() / 2;
        gWorld.mLock->WriteUnlock();
    }
    ioPlan = thePlan;
    if (!SelectGroup(ioPlan.mUnits))
        return "not-selected";
    Wait(800);   // the group panel shows the selection's buttons
    unsigned long theWanted = inKind == kOrderPatrol ? kButtonPatrol : kButtonAttackGround;
    if ((inKind == kOrderPatrol || inKind == kOrderAttackGround) && !FindButton(theWanted, outButton))
        return "no-button";
    std::lock_guard<std::mutex> theLock(sOrderMutex);
    sOrder.mNote = "retried";   // kept through the verdict: the driver logs it
    return NULL;
}

void OrderRun(unsigned long inSeq, int inKind)
{
    OrderPlan thePlan;
    gWorld.mLock->ReadLock();
    Choose(inKind, thePlan);
    gWorld.mLock->ReadUnlock();
    if (inKind == kOrderGather && thePlan.mTarget < 0 && thePlan.mScout >= 0)
        Scout(inKind, thePlan);
    if ((inKind == kOrderRepair || inKind == kOrderRepairQueued) && thePlan.mTarget < 0 && thePlan.mHome >= 0)
        Home(inKind, thePlan);
    if (thePlan.mRefusal != NULL) {
        Refuse(thePlan.mRefusal);
        fprintf(stderr, "TEST UI order-refused order control %d value 0 on form 0 (seq %lu, top form 0)\n", inKind, inSeq);
        fflush(stderr);
        sOrderBusy = 0;
        return;
    }
    if (thePlan.mDamage >= 0) {   // TEST ONLY: the building damaged (no game message does it)
        gWorld.mLock->WriteLock();
        EEComplexUnit* theBuilding = OwnComplex(thePlan.mDamage);
        if (theBuilding != NULL && theBuilding->mHitPoints > theBuilding->GetStats()->GetMaxHitPoints() / 2)
            theBuilding->mHitPoints = theBuilding->GetStats()->GetMaxHitPoints() / 2;
        gWorld.mLock->WriteUnlock();
    }
    Post(new EEMSelectTheseUnits(thePlan.mUnits));
    bool theSelected = false;
    for (int k = 0; k < 40 && !theSelected; k++) {
        Wait(150);
        theSelected = Selected(thePlan.mUnits);
    }
    if (!theSelected) {
        Refuse("not-selected");
        fprintf(stderr, "TEST UI order-refused order control %d value 0 on form 0 (seq %lu, top form 0)\n", inKind, inSeq);
        fflush(stderr);
        sOrderBusy = 0;
        return;
    }
    Wait(800);   // the group panel shows the selection's buttons
    GE2DPoint theButton;
    unsigned long theWanted = inKind == kOrderPatrol ? kButtonPatrol : kButtonAttackGround;
    if ((inKind == kOrderPatrol || inKind == kOrderAttackGround) && !FindButton(theWanted, theButton)) {
        Refuse("no-button");
        fprintf(stderr, "TEST UI order-refused order control %d value 0 on form 0 (seq %lu, top form 0)\n", inKind, inSeq);
        fflush(stderr);
        sOrderBusy = 0;
        return;
    }
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        static const unsigned long kTypes[kOrderKinds] = {kMessageGatherResource, kMessageRepairUnit, kMessageRepairUnit,
                                                          kMessagePatrol, kMessageAttackGround, kMessagePathTo};
        sOrder.mType = kTypes[inKind];
        sOrder.mArmed = true;
        sOrder.mPhase = 2;
    }
    int thePhase = SendOrder(inKind, thePlan, theButton);
    const char* theRetry = NULL;
    if (thePhase == 2) {   // no message: once more from a fresh view (the hook stays armed: a late message counts)
        theRetry = RetryPlan(inKind, thePlan, theButton);
        {
            std::lock_guard<std::mutex> theLock(sOrderMutex);
            thePhase = sOrder.mPhase;
        }
        if (thePhase == 2 && theRetry == NULL)
            thePhase = SendOrder(inKind, thePlan, theButton);
    }
    if (thePhase == 2) {
        Refuse(theRetry != NULL ? theRetry : "no-message");
        fprintf(stderr, "TEST UI order-refused order control %d value 0 on form 0 (seq %lu, top form 0)\n", inKind, inSeq);
    } else
        fprintf(stderr, "TEST UI order-done order control %d value 0 on form 0 (seq %lu, top form 0)\n", inKind, inSeq);
    fflush(stderr);
    sOrderBusy = 0;
}

}   // namespace

// The fixture's start (EETestFixtureStart): the message hook
void EETestOrdersInstall()
{
    gEEMessageExecutedTestHook = MessageHook;
}

// The order command (under the UI lock, from the fixture's Execute) -> its verdict ("order-started", "order-busy",
// "rejected"); the order runs on a thread of its own
const char* EETestOrderStart(unsigned long inSeq, unsigned long inKind)
{
    if (inKind >= kOrderKinds || gMouse == NULL || gPhysics == NULL || gWorld.mLock == NULL)
        return "rejected";
    if (sOrderBusy.exchange(1))
        return "order-busy";
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        sOrder = OrderState();
        sOrder.mSeq = inSeq;
        sOrder.mKind = (int)inKind;
        sOrder.mPhase = 1;
    }
    std::thread(OrderRun, inSeq, (int)inKind).detach();
    return "order-started";
}

// The order lines for ui-state.txt (no lock of the world's or the UI's needed: the hook's copy)
std::string EETestOrderSnapshot()
{
    OrderState theOrder;
    {
        std::lock_guard<std::mutex> theLock(sOrderMutex);
        if (sOrder.mSeq == 0)
            return std::string();
        theOrder = sOrder;
    }
    unsigned long theOK = 0, theBad = 0, theSkip = 0;
    std::string theUnits;
    for (size_t i = 0; i < theOrder.mUnits.size(); i++) {
        const OrderUnit& theRow = theOrder.mUnits[i];
        if (theRow.mVerdict.empty())
            continue;
        theOK += theRow.mVerdict == "ok";
        theBad += theRow.mVerdict == "bad";
        theSkip += theRow.mVerdict == "skip";
        char theLine[256];
        snprintf(theLine, sizeof(theLine), "orderunit %lu %ld %s %s %s\n", theOrder.mSeq, theRow.mID, theRow.mVerdict.c_str(),
                 theRow.mExpected.c_str(), theRow.mFound.c_str());
        theUnits += theLine;
    }
    char theLine[320];
    snprintf(theLine, sizeof(theLine),
             "order seq %lu kind %d phase %d type %lu target %ld point %.3f %.3f queued %d units %lu ok %lu bad %lu skip %lu "
             "note %s\n",
             theOrder.mSeq, theOrder.mKind, theOrder.mPhase, theOrder.mType, theOrder.mKind == kOrderPatrol
             ? theOrder.mRoutePoint : theOrder.mTarget, theOrder.mX, theOrder.mY, theOrder.mQueued,
             (unsigned long)theOrder.mUnits.size(), theOK, theBad, theSkip, theOrder.mNote.c_str());
    return theLine + theUnits;
}
