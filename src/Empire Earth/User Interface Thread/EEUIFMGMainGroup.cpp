// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.cpp (__FILE__ 0x4cdc20, ESVERIFY lines 415/462). The
// main game form's main group (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84): 0x267ec8..0x2691b2 (ctor,
// Virtual10 (inline), ??_G, dtor, Virtual04, the selection members, EEUIEMGHotKeyStatusButton's ??_G after its first
// user, Virtual14, then the inline COMDATs EETechTreeNode::GetHotKey, EEComplexUnit::GetCitizenBuildTarget and
// EEButtonObject::GetHotKey). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "rust/ee_rust.h"   // ee_screens_panel_*: the command panel's layout (ee_screens command_panel.rs)
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

// FUNCTION: EXE 0x667ec8
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

// FUNCTION: EXE 0x667f17
// Asks the panel to refill its unit on the next refresh
// (Virtual10, inline in ee/EEUIFMGMainGroup.h)

// SYNTHETIC: EXE 0x667f1c
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFMGMainGroup+0x0)

// FUNCTION: EXE 0x667f38
// Destroys the main panel group and its status box
EEUIFMGMainGroup::~EEUIFMGMainGroup()
{
    delete this->mStatusBox;
}

// FUNCTION: EXE 0x667f61
// Refreshes the panel: the cancel-mode button, then shows a newly selected unit, clears a lost one or updates
// the current one
void EEUIFMGMainGroup::Refresh()
{
    EEUnit* theUnit = this->GetSelectedUnit();
    if (this->mOwner->mUIState != 0 && !gWorld.mInCinematic)
        this->mOwner->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
    else
        this->mOwner->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeHidden);
    if (this->mHasUnit) {
        if (theUnit == NULL) {
            this->mHasUnit = false;
            this->SetUnit(NULL);
            gUIManager->mMainGameForm->GetEntityGroup(9)->Disable();
            gUIManager->mMainGameForm->GetBackgroundByID(0x97)->SetVisibility(false);
            gUIManager->mMainGameForm->GetEntityGroup(8)->Disable();
            gUIManager->mMainGameForm->GetBackgroundByID(0x98)->SetVisibility(false);
        } else if (theUnit->mUnitID != this->mUnitID) {
            this->mUnitID = theUnit->mUnitID;
            this->SetUnit(theUnit);
            gUIManager->mMainGameForm->GetEntityGroup(9)->Disable();
            gUIManager->mMainGameForm->GetBackgroundByID(0x97)->SetVisibility(false);
            gUIManager->mMainGameForm->GetEntityGroup(8)->Disable();
            gUIManager->mMainGameForm->GetBackgroundByID(0x98)->SetVisibility(false);
        } else {
            if (this->mReset) {
                this->mReset = false;
                this->SetUnit(theUnit);
            }
            this->Update(theUnit);
        }
    } else if (theUnit) {
        this->mHasUnit = true;
        this->mUnitID = theUnit->mUnitID;
        this->SetUnit(theUnit);
    }
}

// FUNCTION: EXE 0x668070
// Shows a unit in the panel: command mode (market: its cloak effect), hit points, status box, icon bar and
// command buttons
void EEUIFMGMainGroup::SetUnit(EEUnit* inUnit)
{
    if (this->mMode == 4 && this->mPreviousID != 0) {
        RTSUnit* theUnit = gWorld.GetCurrentPlayer()->GetUnit(this->mPreviousID);
        if (theUnit)
            EERemoveCloakEffect(static_cast<EEComplexUnit*>(theUnit));
    }
    if (inUnit && inUnit->mComplexUnit != NULL && inUnit->mPlayer == gWorld.GetCurrentPlayer()) {
        if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen)
            this->mMode = 3;
        else if (inUnit->mUnitClass->mUnitType == kUnitTypeMarketCP) {
            this->mMode = 4;
            EEAddCloakEffect(inUnit);
        } else
            this->mMode = 3;
    } else
        this->mMode = 0;
    this->mPreviousID = this->mUnitID;
    if (inUnit) {
        this->mHitPoints = inUnit->mHitPoints;
        this->mL20 = inUnit->mUnitClass->mL294;
    } else {
        this->mHitPoints = 0;
        this->mL20 = 0;
    }
    this->mStatusBox->Fill(inUnit);
    this->mL1C = 0;
    UIControl* theIconBar = this->mOwner->GetControl(kMainGameUnitIconBar);
    if (theIconBar)
        dynamic_cast<EEUICUnitIconBar*>(theIconBar)->Update();
    this->ShowCommands(inUnit);
}

// FUNCTION: EXE 0x668162
// Returns the first living selected unit (else the first selected, else the viewed unit)
EEUnit* EEUIFMGMainGroup::GetSelectedUnit()
{
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    unsigned long theCount = theGroup->GetNumUnits();
    EEUnit* theUnit = NULL;
    for (unsigned long i = 0; i < theCount; i++) {
        theUnit = theGroup->GetUnit(i);
        if (theUnit->mHitPoints != 0)
            break;
    }
    if (theUnit != NULL)
        return theUnit;
    if (theCount != 0)
        theUnit = theGroup->GetUnit(0);
    else
        theUnit = static_cast<EEUnit*>(gWorld.GetCurrentPlayer()->mSelectedViewedUnit);
    return theUnit;
}

// FUNCTION: EXE 0x6681c2
// Opens the panel: binds the status box to the form's controls and clears the group's hot keys
void EEUIFMGMainGroup::Open(EEUIFMainGame* inForm)
{
    this->mStatusBox->HideBackgrounds();
    this->mStatusBox->mC8C = inForm->GetControl(kMainGameStatusBarPortrait);
    this->mStatusBox->mC94 = inForm->GetControl(kMainGameStatusBarCivName);
    this->mStatusBox->mC90 = inForm->GetControl(kMainGameStatusBarCivUnitName);
    this->mStatusBox->mC84 = inForm->GetControl(kMainGameStatusBarLeftStatus);
    this->mStatusBox->mC88 = inForm->GetControl(kMainGameStatusBarMiddleStatus);
    this->mStatusBox->mC98 = inForm->GetControl(kMainGameArmorDisplayTitle);
    this->mStatusBox->mCAC = inForm->GetControl(kMainGameSmallStatusBar);
    this->mStatusBox->mCA8 = inForm->GetControl(kMainGameMiddleStatusBar);
    long i;
    for (i = 0; i < 5; i++)
        this->mStatusBox->SetControls(i, inForm->GetControl(i + 0x45), inForm->GetControl(i + 0x20));
    for (i = 5; i < 9; i++)
        this->mStatusBox->SetControls(i, inForm->GetControl(i + 0x45), inForm->GetControl(i + 0x20));
    this->mOwner->RemoveAllHotKeysForAGroup(2);
    this->mHasUnit = false;
}

// FUNCTION: EXE 0x6682ea
// Shows one command button, with the current behaviour or formation icon and its hot keys (also with Shift
// for the market)
// NEAR: the original calls sTable's vector::operator[] out of line (inline budget) and keeps theComplex in ebp
void EEUIFMGMainGroup::ShowCommandButton(EEPlayer* inPlayer, EEUnit* inUnit, unsigned long inButton, unsigned long inControl)
{
    EEComplexUnit* theComplex = inUnit ? inUnit->GetComplexUnit() : NULL;
    EEButtonObject* theObject = EEButtonObject::Get(inButton);
    if (theObject == NULL)
        return;
    EEUIFMGStatusButton* theButton = static_cast<EEUIFMGStatusButton*>(this->GetOwner()->GetControl(inControl));
    theButton->SetCommandButton(inButton);
    if (inButton == 0x1d) {
        if (theComplex->GetBehavior() == EEUnitBehavior::Get(0)) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorScout))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorScout))->GetPressedBitmap());
        } else if (theComplex->GetBehavior() == EEUnitBehavior::Get(1)) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorStandGround))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorStandGround))->GetPressedBitmap());
        } else if (theComplex->GetBehavior() == EEUnitBehavior::Get(2)) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorGuard))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorGuard))->GetPressedBitmap());
        } else if (theComplex->GetBehavior() == EEUnitBehavior::Get(5)) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorAggressive))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameBehaviorAggressive))->GetPressedBitmap());
        } else
            ESVERIFY_AT(false, 415);
    } else if (inButton == 0x1e) {
        long theFormation = inPlayer->GetSelectedGroup()->GetFormation()->GetType();
        if (theFormation == 0) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationShortLine))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationShortLine))->GetPressedBitmap());
        } else if (theFormation == 1) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationLongLine))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationLongLine))->GetPressedBitmap());
        } else if (theFormation == 2) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationLooseLine))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationLooseLine))->GetPressedBitmap());
        } else if (theFormation == 3) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationStaggeredLine))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationStaggeredLine))->GetPressedBitmap());
        } else if (theFormation == 4) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationDeepWedge))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationDeepWedge))->GetPressedBitmap());
        } else if (theFormation == 5) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationShallowWedge))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationShallowWedge))->GetPressedBitmap());
        } else if (theFormation == 6) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationInwardCircle))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationInwardCircle))->GetPressedBitmap());
        } else if (theFormation == 7) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationOutwardCircle))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(this->GetOwner()->GetControl(kMainGameFormationOutwardCircle))->GetPressedBitmap());
        } else
            ESVERIFY_AT(false, 462);
    }
    theButton->SetDisplayMode(kDisplayModeShown);
    if (!theObject->GetHotKey())
        return;
    theObject->GetHotKey()->SetAlwaysFire(true);
    UIEvent* theEvent = new EEUIEMGHotKeyStatusButton(theButton, true);
    theObject->GetHotKey()->SetJustPressedEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGHotKeyStatusButton(theButton, false);
    theObject->GetHotKey()->SetJustUnpressedEvent(theEvent);
    theEvent->Dereference();
    this->mOwner->AddHotKey(*theObject->GetHotKey(), 2);
    if (this->GetMode() != 4)
        return;
    UIHotKey theHotKey(this->mOwner, *theObject->GetHotKey());
    theHotKey.SetModifier(kKeyShift, 0);
    theEvent = new EEUIEMGHotKeyStatusButton(theButton, true);
    theHotKey.SetJustPressedEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGHotKeyStatusButton(theButton, false);
    theHotKey.SetJustUnpressedEvent(theEvent);
    theEvent->Dereference();
    this->GetOwner()->AddHotKey(theHotKey, 2);
}

// FUNCTION: EXE 0x6687db
// Destroys a status-button hot key event (scalar deleting destructor)
// (emitted after its first user)

// FUNCTION: EXE 0x668806
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
            this->ShowCommandButton(thePlayer, inUnit, theEntry.button, theEntry.control);
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
                theNode->GetHotKey()->SetAlwaysFire(true);
                UIEvent* theEvent = new EEUIEMGHotKeyStatusButton(theButton, true);
                theNode->GetHotKey()->SetJustPressedEvent(theEvent);
                theEvent->Dereference();
                theEvent = new EEUIEMGHotKeyStatusButton(theButton, false);
                theNode->GetHotKey()->SetJustUnpressedEvent(theEvent);
                theEvent->Dereference();
                UIHotKey theHotKey(this->mOwner, *theNode->GetHotKey());
                theHotKey.SetModifier(kKeyShift, 0);
                theEvent = new EEUIEMGHotKeyStatusButton(theButton, true);
                theHotKey.SetJustPressedEvent(theEvent);
                theEvent->Dereference();
                theEvent = new EEUIEMGHotKeyStatusButton(theButton, false);
                theHotKey.SetJustUnpressedEvent(theEvent);
                theEvent->Dereference();
                this->mOwner->AddHotKey(theHotKey, 2);
                this->mOwner->AddHotKey(*theNode->GetHotKey(), 2);
            }
        }
    }
    if (theComplex->mUnitClass->mUnitType == kUnitTypeAirport)
        this->ShowSpecialButtons(inUnit);
}

// FUNCTION: EXE 0x668dc6
// Returns the tech node's hot key, or NULL
UIHotKey* EETechTreeNode::GetHotKey()
{
    if (this->mData->mHotKey > 0 && this->mData->mHotKey < (long)UIHotKey::GetHotKeyCount())
        return UIHotKey::GetHotKey(this->mData->mHotKey);
    return NULL;
}

// FUNCTION: EXE 0x668dee
// Shows the airport's three timed rally countdowns, and hides command 0xc before the WW2 epoch
// NEAR: mGoalQueues[i] calls begin()/operator+ out of line in the original (inline budget; 8 more frame bytes); the second
// loop's induction base
void EEUIFMGMainGroup::ShowSpecialButtons(EEUnit* inUnit)
{
    UWideString theText;
    EEComplexUnit* theComplex = inUnit->mComplexUnit;
    if (!theComplex || theComplex->mUnitClass->mUnitType != kUnitTypeAirport)
        return;
    for (unsigned long i = 1; i < 4; i++) {
        UIButton* theButton = static_cast<UIButton*>(this->mOwner->GetControl(0xcf + i));
        unsigned long theTimeLeft = theComplex->mGoalQueues[i].GetTimeLeft();
        if (theTimeLeft > 0) {
            unsigned long theMinutes;
            unsigned long theSeconds;
            gPhysics->GetMinutesAndSeconds(&theMinutes, &theSeconds, theTimeLeft);
            sprintf(theText, "%ld:%.2ld", theMinutes, theSeconds);
            theButton->SetDisplayMode(kDisplayModeShown);
            theButton->SetText(theText, gU531930);
        } else
            theButton->SetDisplayMode(kDisplayModeHidden);
    }
    if (theComplex->mPlayer != gWorld.GetCurrentPlayer())
        return;
    for (unsigned long k = 0; k < 9; k++) {
        unsigned long theCommand = theComplex->mUnitClass->mCommands[k];
        if (!(theCommand >= 2) || !(theCommand < 0x32) || theCommand != 0xc)
            continue;
        EEButtonObject* theObject = EEButtonObject::Get(theCommand);
        if (theComplex->mPlayer->GetTechTree()->mCurrentEpoch < kTechEpochAtomicWW2)
            this->mOwner->GetControl(k + 0x4e)->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x668f3f
// Updates the panel each tick: queued counts on the research buttons, airport timers, the status box's queue
// and the commands again when the unit just died
// NEAR: mBuildQueue[k] calls _Ptrit::operator+ out of line in the original (inline budget); register choice follows
void EEUIFMGMainGroup::Update(EEUnit* inUnit)
{
    for (unsigned long i = 0x6e; i < 0xce; i++) {
        static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(i))->mCount = 0;
        this->mOwner->GetControl(i)->SetDirty(true);
    }
    if (inUnit->GetHitPoints() == 0 && this->mHitPoints != 0)
        this->ShowCommands(inUnit);
    else if (inUnit->GetTileNode() && inUnit->mPlayer == gWorld.GetCurrentPlayer()) {
        EEComplexUnit* theComplex = inUnit->GetComplexUnit();
        bool theIsWorker;
        if (theComplex->GetClass()->GetCategory() == kUnitCategoryLandCitizen || theComplex->GetClass()->GetCategory() == kUnitCategoryNavalCitizen)
            theIsWorker = true;
        else
            theIsWorker = false;
        bool theHasTimers = theComplex->GetClass()->GetUnitType() == kUnitTypeAirport;
        unsigned long theCount = theIsWorker ? theComplex->mCitizenBuildQueue.size() : theComplex->GetBuildQueueSize();
        for (unsigned long j = 0; j < theCount; j++) {
            long theRun = 1;
            long theKey;
            unsigned long k;
            if (theIsWorker) {
                theKey = theComplex->GetCitizenBuildTarget(j)->GetClass()->GetTechnology();
                for (k = j + 1; k < theCount; k++) {
                    if (theComplex->GetCitizenBuildTarget(k)->GetClass()->GetTechnology() != theKey)
                        break;
                    j++;
                    theRun++;
                }
            } else {
                theKey = theComplex->mBuildQueue[j].mTechnology;
                for (k = j + 1; k < theCount; k++) {
                    if (theComplex->mBuildQueue[k].mTechnology != theKey)
                        break;
                    j++;
                    theRun++;
                }
            }
            for (unsigned long n = 0; n < 0x60; n++) {
                EEUIFMGStatusButton* theButton = static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(n + 0x6e));
                if (!theButton->mIsCommand && theButton->mTechnology == theKey) {
                    static_cast<EEUIFMGStatusButton*>(this->mOwner->GetControl(n + 0x6e))->mCount += theRun;
                    break;
                }
            }
        }
        if (theHasTimers)
            this->ShowSpecialButtons(inUnit);
        this->mStatusBox->UpdateQueuedCounts(theComplex,
                             theComplex->GetPlayer()->GetResearchedImprovement(theComplex->GetClass()->GetUnitType()));
    }
    this->mStatusBox->Refresh(inUnit);
}

// FUNCTION: EXE 0x669137
// Returns the citizen's queued build target at an index
EEUnit* EEComplexUnit::GetCitizenBuildTarget(unsigned long inIndex)
{
    return this->mCitizenBuildQueue[inIndex];
}

// FUNCTION: EXE 0x66915e
// Tells whether the selected unit changed since the panel was filled
bool EEUIFMGMainGroup::WantsRefresh()
{
    EEUnit* theUnit = this->GetSelectedUnit();
    bool theResult = false;
    if (this->mHasUnit)
        theResult = theUnit != gWorld.GetUnit(this->mUnitID);
    else if (theUnit)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x66918d
// Returns the button's hot key, or NULL
UIHotKey* EEButtonObject::GetHotKey()
{
    if (this->mHotKeyIndex > 0 && this->mHotKeyIndex < (long)UIHotKey::GetHotKeyCount())
        return UIHotKey::GetHotKey(this->mHotKeyIndex);
    return NULL;
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUIFMGMainGroup::Select()
{
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUIFMGMainGroup::Virtual08()
{
}
