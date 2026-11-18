// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.cpp (__FILE__ 0x4cdc20, ESVERIFY lines 415/462). The
// main game form's main group (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84): 0x267ec8..0x2691b2 (ctor,
// Virtual10 (inline), ??_G, dtor, Virtual04, the selection members, EEUIEMGHotKeyStatusButton's ??_G after its first
// user, Virtual14, then the inline COMDATs EETechTreeNode::GetHotKey, EEComplexUnit::GetCitizenBuildTarget and
// EEButtonObject::GetHotKey). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "rust/game/screens.h"   // ee_screens_panel_*: the command panel's layout (ee_screens command_panel.rs)
#include "ee/EEUIManager.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFormation.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPhysics.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIEMGHotKeyStatusButton.h"

// Presses or releases a status button by hot key (pressing also resets the saved-group state). The original (0x6601da)
// lay among the main game form's events (EEUIEMainGameEvents.cpp, now Rust's: ee_screens main_game.rs); this one
// holds a C++ button, so it stays with the panel that makes it.
long EEUIEMGHotKeyStatusButton::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (!this->mButton)
        return kErrorOK;
    if (this->mPressed) {
        this->mButton->HotKeyDown();
        gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x14, 1);
    } else
        this->mButton->HotKeyUp();
    return kErrorOK;
}

// Creates the main game form's main panel group with its status box, no unit shown
EEUIFMGMainGroup::EEUIFMGMainGroup(EEUIFMainGame* inOwner)
    : EEUIFMGGroup(inOwner)
{
    this->mReset = false;
    this->mUnitID = 0;
    this->mHasUnit = false;
    this->mHitPoints = 0;
    this->mL20 = 0;
    this->mPreviousID = 0;
    this->mStatusBox = new EEUIFMGStatusBox(inOwner);
}

// Destroys the main panel group and its status box
EEUIFMGMainGroup::~EEUIFMGMainGroup()
{
    delete this->mStatusBox;
}

// The group's members but ShowCommands (the original's Refresh, SetUnit, GetSelectedUnit, Open, ShowCommandButton,
// ShowSpecialButtons, Update, WantsRefresh): ee_screens main_group.rs over the host below
void EEUIFMGMainGroup::Refresh()
{
    ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_REFRESH, NULL, 0);
}

EEUnit* EEUIFMGMainGroup::GetSelectedUnit()
{
    return reinterpret_cast<EEUnit*>(ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_SELECTED_UNIT, NULL, 0));
}

void EEUIFMGMainGroup::Open(EEUIFMainGame* inForm)
{
    ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_OPEN, NULL, 0);
}

bool EEUIFMGMainGroup::WantsRefresh()
{
    return ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_WANTS_REFRESH, NULL, 0) != 0;
}

// A hot key pressing a status button (inHow 0: always firing, its events), added to the form's group 2 (1), or its
// copy with Shift held, pressing the button, added (2)
static void EEMainGroupHotKey(EEUIFMainGame* inForm, UIHotKey* inHotKey, EEUIFMGStatusButton* inButton, int64_t inHow)
{
    auto Events = [inButton](UIHotKey* inKey) {
        UIEvent* theEvent = new EEUIEMGHotKeyStatusButton(inButton, true);
        inKey->SetJustPressedEvent(theEvent);
        theEvent->Dereference();
        theEvent = new EEUIEMGHotKeyStatusButton(inButton, false);
        inKey->SetJustUnpressedEvent(theEvent);
        theEvent->Dereference();
    };
    if (inHow == 0) {
        inHotKey->SetAlwaysFire(true);
        Events(inHotKey);
    } else if (inHow == 1) {
        inForm->AddHotKey(*inHotKey, 2);
    } else {
        UIHotKey theHotKey(inForm, *inHotKey);
        theHotKey.SetModifier(kKeyShift, 0);
        Events(&theHotKey);
        inForm->AddHotKey(theHotKey, 2);
    }
}

// The main group's host (ee_screens main_group.rs, EE_SCREENS_MGR_OP_*): units as handles (EEUnit*). Its world reads
// and toolkit calls are hud_views.rs' (the world's views, the toolkit's direct path)
extern "C" int64_t ee_screenscpp_main_group(void* inGroup, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(inGroup);
    EEUIFMainGame* theForm = theGroup->mOwner;
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(inA);
    EEUIFMGStatusBox* theBox = theGroup->mStatusBox;
    auto Button = [&]() { return static_cast<EEUIFMGStatusButton*>(theForm->GetControl((unsigned long)inA)); };
    switch (inOp) {
    case EE_SCREENS_MGR_OP_FIELD:
    case EE_SCREENS_MGR_OP_SET_FIELD: {
        bool theSet = inOp == EE_SCREENS_MGR_OP_SET_FIELD;
#define EE_MGR_FIELD(id, member) \
    case id: \
        if (theSet) \
            member = (decltype(member))inB; \
        return (int64_t)member;
        switch (inA) {
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_MODE, theGroup->mMode)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_HIT_POINTS, theGroup->mHitPoints)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_UNIT_ID, theGroup->mUnitID)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_PREVIOUS_ID, theGroup->mPreviousID)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_L1C, theGroup->mL1C)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_L20, theGroup->mL20)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_HAS_UNIT, theGroup->mHasUnit)
            EE_MGR_FIELD(EE_SCREENS_MGR_FIELD_RESET, theGroup->mReset)
        }
#undef EE_MGR_FIELD
        return 0;
    }
    case EE_SCREENS_MGR_OP_UI_STATE: return theForm->mUIState;
    case EE_SCREENS_MGR_OP_FORMS: return inA == 0 ? (int64_t)theForm : (int64_t)gUIManager->mMainGameForm;
    case EE_SCREENS_MGR_OP_WORLD_UNIT: return reinterpret_cast<int64_t>(gWorld.GetUnit((unsigned long)inA));
    case EE_SCREENS_MGR_OP_PLAYER_UNIT:
        return reinterpret_cast<int64_t>(gWorld.GetCurrentPlayer()->GetUnit((unsigned long)inA));
    case EE_SCREENS_MGR_OP_CLOAK:
        if (inA == 0)
            EERemoveCloakEffect(static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(inB)));
        else
            EEAddCloakEffect(reinterpret_cast<EEUnit*>(inB));
        return 0;
    case EE_SCREENS_MGR_OP_BOX: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(inB);
        if (inA == EE_SCREENS_MGR_BOX_FILL)
            theBox->Fill(theTarget);
        else if (inA == EE_SCREENS_MGR_BOX_REFRESH)
            theBox->Refresh(theTarget);
        else if (inA == EE_SCREENS_MGR_BOX_HIDE)
            theBox->HideBackgrounds();
        else
            theBox->UpdateQueuedCounts(theTarget->GetComplexUnit(), theTarget->GetComplexUnit()->GetPlayer()->GetResearchedImprovement(
                                                                        theTarget->GetComplexUnit()->GetClass()->GetUnitType()));
        return 0;
    }
    case EE_SCREENS_MGR_OP_BOX_BIND: {
        static UIControl* EEUIFMGStatusBox::* const kSlots[8] = {
            &EEUIFMGStatusBox::mC8C, &EEUIFMGStatusBox::mC94, &EEUIFMGStatusBox::mC90, &EEUIFMGStatusBox::mC84,
            &EEUIFMGStatusBox::mC88, &EEUIFMGStatusBox::mC98, &EEUIFMGStatusBox::mCAC, &EEUIFMGStatusBox::mCA8};
        theBox->*kSlots[inA] = theForm->GetControl((unsigned long)inB);
        return 0;
    }
    case EE_SCREENS_MGR_OP_BOX_PAIR:
        theBox->SetControls((long)inA, theForm->GetControl((unsigned long)inA + 0x45), theForm->GetControl((unsigned long)inA + 0x20));
        return 0;
    case EE_SCREENS_MGR_OP_ICON_BAR: {
        UIControl* theIconBar = theForm->GetControl(kMainGameUnitIconBar);
        if (theIconBar)
            dynamic_cast<EEUICUnitIconBar*>(theIconBar)->Update();
        return 0;
    }
    case EE_SCREENS_MGR_OP_SHOW_COMMANDS: theGroup->ShowCommands(theUnit); return 0;
    case EE_SCREENS_MGR_OP_BUTTON_OBJECT: return EEButtonObject::Get((unsigned long)inA) != NULL;
    case EE_SCREENS_MGR_OP_SET_COMMAND: Button()->SetCommandButton((unsigned long)inB); return 0;
    case EE_SCREENS_MGR_OP_BEHAVIOR: {
        EEComplexUnit* theComplex = theUnit ? theUnit->GetComplexUnit() : NULL;
        static const long kBehaviors[4] = {0, 1, 2, 5};
        for (long k : kBehaviors)
            if (theComplex->GetBehavior() == EEUnitBehavior::Get(k))
                return k;
        return -1;
    }
    case EE_SCREENS_MGR_OP_FORMATION: return gWorld.GetCurrentPlayer()->GetSelectedGroup()->GetFormation()->GetType();
    case EE_SCREENS_MGR_OP_COPY_BITMAPS: {
        UIButton* theSource = static_cast<UIButton*>(theForm->GetControl((unsigned long)inB));
        Button()->SetNormalBitmap(theSource->GetNormalBitmap());
        Button()->SetPressedBitmap(theSource->GetPressedBitmap());
        return 0;
    }
    case EE_SCREENS_MGR_OP_VERIFY: ESVERIFY_AT(false, (long)inA); return 0;
    case EE_SCREENS_MGR_OP_HAS_HOT_KEY: return EEButtonObject::Get((unsigned long)inA)->GetHotKey() != NULL;
    case EE_SCREENS_MGR_OP_HOT_KEY:
        EEMainGroupHotKey(theForm, EEButtonObject::Get((unsigned long)inA)->GetHotKey(),
                          static_cast<EEUIFMGStatusButton*>(theForm->GetControl((unsigned long)inC)), inB);
        return 0;
    case EE_SCREENS_MGR_OP_TIME_LEFT: return theUnit->mComplexUnit->mGoalQueues[inB].GetTimeLeft();
    case EE_SCREENS_MGR_OP_TIMER_TEXT: {
        UWideString theText;
        unsigned long theMinutes;
        unsigned long theSeconds;
        gPhysics->GetMinutesAndSeconds(&theMinutes, &theSeconds, (unsigned long)inB);
        sprintf(theText, "%ld:%.2ld", theMinutes, theSeconds);
        UIButton* theTimer = static_cast<UIButton*>(theForm->GetControl((unsigned long)inA));
        theTimer->SetDisplayMode(kDisplayModeShown);
        theTimer->SetText(theText, gU531930);
        return 0;
    }
    case EE_SCREENS_MGR_OP_RESET_COUNT:
        Button()->mCount = 0;
        Button()->SetDirty(true);
        return 0;
    // (the production queue's size and keys, inB 0, are hud_views.rs'; these are the citizen's build queue's)
    case EE_SCREENS_MGR_OP_QUEUE_SIZE: return theUnit->GetComplexUnit()->mCitizenBuildQueue.size();
    case EE_SCREENS_MGR_OP_QUEUE_KEY: return theUnit->GetComplexUnit()->GetCitizenBuildTarget((unsigned long)inC)->GetClass()->GetTechnology();
    case EE_SCREENS_MGR_OP_BUTTON_TECH: return Button()->mIsCommand ? EE_SCREENS_MGR_COMMAND : Button()->mTechnology;
    case EE_SCREENS_MGR_OP_ADD_COUNT: Button()->mCount += (long)inB; return 0;
    }
    return 0;
}

// Lays out the command and research buttons of the player's selected unit (civ power buttons, the class's
// commands, its available techs with hot keys, airport timers); hides them all for others
// NEAR: the first mResearchTechnologies[m] calls begin()/operator+ out of line in the original (inline budget), the frame is 8 bytes
// larger for their temporaries; the free-slot search loop rotates differently
void EEUIFMGMainGroup::ShowCommands(EEUnit* inUnit)
{
    EEComplexUnit* theComplex = inUnit ? inUnit->mComplexUnit : NULL;
    this->mOwner->RemoveAllHotKeysForAGroup(2);
    unsigned long i; for (i = 0x6e; i < 0xce; i++) {
        this->mOwner->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
        static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(i))->ReleaseButton();
    }
    for (i = 0x4e; i < 0x57; i++) {
        this->mOwner->GetControl(i)->SetDisplayMode(kDisplayModeHidden);
        static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(i))->ReleaseButton();
    }
    this->mOwner->GetControl(kMainGameTimedFighterRally)->SetDisplayMode(kDisplayModeHidden);
    this->mOwner->GetControl(kMainGameTimedBomberRally)->SetDisplayMode(kDisplayModeHidden);
    this->mOwner->GetControl(kMainGameTimedAtomicBomberRally)->SetDisplayMode(kDisplayModeHidden);
    EEPlayer* theLocalPlayer = gWorld.GetCurrentPlayer();
    EEPlayer* thePlayer = theLocalPlayer->mObserverMode ? NULL : theLocalPlayer;
    // the layout is Rust's (ee_screens command_panel.rs panel_buttons) over what the C++ reads here
    EeScreensPanelView theView;
    memset(&theView, 0, sizeof(theView));
    theView.unit = inUnit != NULL;
    theView.complex = theComplex != NULL;
    if (inUnit != NULL && theComplex != NULL) {
        theView.built = theComplex->mIsBuilt;
        theView.alive = inUnit->mHitPoints != 0;
        theView.commandable = theComplex->mIsCommandable;
        theView.owned = inUnit->mPlayer == thePlayer;
    }
    theView.cancel_hidden = this->mOwner->GetControl(kMainGameCancelUIMode)->GetDisplayMode() == kDisplayModeHidden;
    theView.player = thePlayer != NULL;
    if (!ee_screens_panel_gate(&theView))
        return;
    theView.human = thePlayer->IsHuman() == true;
    if (!ee_screens_panel_shown(&theView))
        return;
    EEUnitClass* theClass = inUnit->mUnitClass;
    EETechTree* theTree = thePlayer->GetTechTree();
    theView.mode = this->mMode;
    if (this->mMode == 1 || this->mMode == 3) {
        theView.fanatic = gCivPowersManager.HasPower(thePlayer, kCivPowerFanaticism) &&
                          gUnitSetManager.GetSet(0x2f)->Contains(inUnit->mUnitClass->mUnitType);
        theView.jit = gCivPowersManager.HasPower(thePlayer, kCivPowerJustInTimeManufacturing) &&
                      theComplex->mUnitClass->HasRallyPoint();
        theView.jit_on = thePlayer->mJITManufacturing;
        EEButtonObject* theObjects[3] = {EEButtonObject::Get(0x28), EEButtonObject::Get(0x29), EEButtonObject::Get(0x2a)};
        theView.fanatic_position = theObjects[0] ? theObjects[0]->mPosition : 0;
        theView.jit_positions[0] = theObjects[1] ? theObjects[1]->mPosition : 0;
        theView.jit_positions[1] = theObjects[2] ? theObjects[2]->mPosition : 0;
    } else if (this->mMode == 4) {
        for (long j = 0; j < 8; j++)
            theView.trade_positions[j] = EEButtonObject::Get(0x5dc + j)->mPosition;
    }
    for (unsigned long k = 0; k < 9; k++)
        theView.commands[k] = theClass->mCommands[k];
    std::vector<EeScreensPanelResearch> theResearch;
    std::vector<EETechTreeNode*> theNodes;
    if (this->mMode == 2 || this->mMode == 3) {
        for (long m = 0; m < theClass->GetNumResearchTechnologies(); m++) {
            EETechTreeNode* theNode = theTree->Get(theClass->mResearchTechnologies[m]);
            EEButtonObject* theObject = theNode->mButton;
            EeScreensPanelResearch theEntry;
            theEntry.position = theObject->mPosition;
            theEntry.command = theObject->mID;
            theEntry.kind = theNode->mData->mKind;
            theEntry.eligible = theObject->mPosition >= 0 && theObject->mPosition < 10000 && theNode->GetIsAvailable() &&
                                (theNode->mIsAllowed || theComplex->IsResearchQueued(theNode->mData->mID));
            theResearch.push_back(theEntry);
            theNodes.push_back(theNode);
        }
    }
    std::vector<EeScreensPanelButton> theButtons(8 + 2 + 9 + theResearch.size() + 1);
    unsigned long theCount = ee_screens_panel_buttons(&theView, theResearch.empty() ? NULL : &theResearch[0],
                                                      theResearch.size(), &theButtons[0], theButtons.size());
    for (unsigned long b = 0; b < theCount; b++) {
        const EeScreensPanelButton& theEntry = theButtons[b];
        if (theEntry.kind == EE_SCREENS_PANEL_COMMAND) {
            ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_COMMAND_BUTTON, inUnit,
                                         (int64_t)theEntry.button | (int64_t)theEntry.control << 32);
        } else if (theEntry.kind == EE_SCREENS_PANEL_STOP) {
            static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(kMainGameActionButton1))->SetCommandButton(3);
            this->mOwner->GetControl(kMainGameActionButton1)->SetDisplayMode(kDisplayModeShown);
        } else {
            long m = theEntry.research;
            EETechTreeNode* theNode = theNodes[m];
            EEUIFMGStatusButton* theButton = static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(theEntry.control));
            theButton->SetTechButton(theClass->mResearchTechnologies[m], theClass->mUnitType, inUnit->mUnitID);
            this->mOwner->GetControl(theEntry.control)->SetDisplayMode(kDisplayModeShown);
            if (theNode->GetHotKey()) {
                EEMainGroupHotKey(this->mOwner, theNode->GetHotKey(), theButton, 0);
                EEMainGroupHotKey(this->mOwner, theNode->GetHotKey(), theButton, 2);
                EEMainGroupHotKey(this->mOwner, theNode->GetHotKey(), theButton, 1);
            }
        }
    }
    if (theComplex->mUnitClass->mUnitType == kUnitTypeAirport)
        ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_SPECIAL_BUTTONS, inUnit, 0);
}

// Returns the tech node's hot key, or NULL
UIHotKey* EETechTreeNode::GetHotKey()
{
    if (this->mData->mHotKey > 0 && this->mData->mHotKey < (long)UIHotKey::GetHotKeyCount())
        return UIHotKey::GetHotKey(this->mData->mHotKey);
    return NULL;
}

// Returns the citizen's queued build target at an index
EEUnit* EEComplexUnit::GetCitizenBuildTarget(unsigned long inIndex)
{
    return this->mCitizenBuildQueue[inIndex];
}

// Returns the button's hot key, or NULL
UIHotKey* EEButtonObject::GetHotKey()
{
    if (this->mHotKeyIndex > 0 && this->mHotKeyIndex < (long)UIHotKey::GetHotKeyCount())
        return UIHotKey::GetHotKey(this->mHotKeyIndex);
    return NULL;
}

void EEUIFMGMainGroup::Select()
{
}

void EEUIFMGMainGroup::Virtual08()
{
}


// Returns the epoch the tech tree shows to the player (two past the start, at most the end; at least the Bronze
// Age's): the statistics group's "wonders available in" (inline in the original EEUIFMGGStatistics.h)
long EETechTree::GetShownEpoch() const
{
    long theEpoch = this->mStartEpoch + 2 > this->mEndEpoch ? this->mEndEpoch : this->mStartEpoch + 2;
    return theEpoch > kTechEpochBronze ? theEpoch : 3;
}

// The panel groups' host (ee_screens panel_groups.rs): a value (EE_SCREENS_GROUP_VALUE_*), or a label's text
// printed from the format with the resource strings[0] and strings[1] (when not -1) or strings[0] and the two values
extern "C" int32_t ee_screenscpp_group(void* inGroup, bool inPrint, uint32_t inWhich, uint32_t inControl,
                                       bool inIfChanged, const char* inFormat, const int32_t* inStrings,
                                       const int32_t* inValues)
{
    EEUIFMGGRust* theGroup = static_cast<EEUIFMGGRust*>(inGroup);
    if (!inPrint) {
        switch (inWhich) {
        case EE_SCREENS_GROUP_VALUE_CONVERSIONS:
            return gWorld.GetCurrentPlayer()->mSavedGroups.GetLastValue(5);
        case EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX:
            return gWorldTable.GetLongValue(kWorldConversionsStatisticMax);
        case EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN:
            return gWorld.mNumberOfWondersToWin;
        case EE_SCREENS_GROUP_VALUE_EPOCH_TITLE:
            return EEEpochTitleID(gWorld.GetCurrentPlayer()->GetTechTree()->GetShownEpoch());
        case EE_SCREENS_GROUP_VALUE_HAS_PLAYER:
            return gWorld.GetCurrentPlayer() != NULL;
        }
        return 0;
    }
    UWideString theText, theFirst, theSecond;
    gResource->GetString(inStrings[0], theFirst);
    if (inStrings[1] != -1) {
        gResource->GetString(inStrings[1], theSecond);
        sprintf(theText, inFormat, (const char*)theFirst, (const char*)theSecond);
    } else {
        sprintf(theText, inFormat, (const char*)theFirst, (long)inValues[0], (long)inValues[1]);
    }
    UITextLabel* theLabel = static_cast<UITextLabel*>(theGroup->mOwner->GetControl(inControl));
    if (!inIfChanged || theLabel->GetText(0) != theText)
        theLabel->SetText(theText);
    return 0;
}

// Refreshes the panel group (the statistics group: the conversions against their limit, when the text changed)
void EEUIFMGGRust::Refresh()
{
    ee_screens_group_call(this, this->mKind, EE_SCREENS_GROUP_REFRESH);
}

// Selects the panel group (the statistics group: the wonders to win, the epoch wonders become available in, then
// the conversions)
void EEUIFMGGRust::Select()
{
    ee_screens_group_call(this, this->mKind, EE_SCREENS_GROUP_SELECT);
}
