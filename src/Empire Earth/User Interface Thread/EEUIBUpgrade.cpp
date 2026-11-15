// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.cpp (file name inferred: no __FILE__; RTTI class EEUIBUpgrade).
// The in-game improvement button: 0x256845..0x257223 (ctor, the in-class mouse forwarders, ??_G, then the out-of-line
// members in alphabetical order; EEUICChatLine's ctor at 0x257223). Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "lle/UITextLabel.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAction.h"
#include "ee/EEAUpgradeImprovement.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEStringIDs.h"
#include "ee/EEImprovementIDs.h"
#include "rust/ee_screens.h"   // ee_screens_upgrade_* (ee_screens upgrade.rs)
static const float kBgOpacity = 0.15f;      // 0x449d4c

// FUNCTION: EXE 0x656845
// Creates an improvement button, faintly tinted with a thin border, without a cost label
EEUIBUpgrade::EEUIBUpgrade(unsigned long inId, const GE2DPoint& inLocation, unsigned long inWidth,
                           unsigned long inHeight, UIForm* inForm, unsigned long inIndex, bool inScenarioEditor,
                           unsigned long inBorderThickness)
    : UIButton(inId, NULL, NULL, NULL, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
               inBorderThickness),
      mScenarioEditor(inScenarioEditor)
{
    this->mNumQueued = 0;
    this->mLevel = 0;
    this->mLastMessageTime = 0;
    this->mCostLabel = NULL;
    this->SetBgOpacity(kBgOpacity);
    this->SetBorderOpacity(kBgOpacity * 3.0f);
    this->SetBorderThickness(1);
    this->mDisabled = false;
}

// FUNCTION: EXE 0x656943
// Handles a left press as a button press
// (EEUIBUpgrade::MouseLButtonDown, in the class)

// FUNCTION: EXE 0x656949
// Handles a left release as a button release
// (EEUIBUpgrade::MouseLButtonUp, in the class)

// SYNTHETIC: EXE 0x65694f
// (the scalar deleting destructor, emitted with the vtable; slot EEUIBUpgrade+0x0)

// FUNCTION: EXE 0x656980
// Presses the button unless the improvement is unavailable, and updates the help text
void EEUIBUpgrade::ButtonDown()
{
    if (!this->GetDisabled())
        UIButton::ButtonDown();
    this->GetForm()->UpdateHelpString();
}

// FUNCTION: EXE 0x656ae2
// Snapshots the button and its cost label for drawing
long EEUIBUpgrade::CopyData()
{
    long theResult = UIButton::CopyData();
    if (theResult == kErrorOK && this->mCostLabel != NULL) {
        this->GetForm()->GetManager()->AddDirtyRectangle(this->mCostLabel->GetRectangle(), this->GetForm());
        theResult = this->mCostLabel->CopyData();
    }
    return theResult;
}

// FUNCTION: EXE 0x656b1e
// Draws the button text and its cost label
long EEUIBUpgrade::DrawTextA()
{
    long theResult = UIButton::DrawTextA();
    if (theResult == kErrorOK && this->mCostLabel != NULL)
        theResult = this->mCostLabel->DrawTextA();
    return theResult;
}

// One call into ee_screens upgrade.rs: the button, the out text (the tooltip, the refusal's reason, the new cost;
// NULL for none), the held text, the mouse location
struct EEUIBUpgradeCall {
    EEUIBUpgrade* mButton;
    UWideString* mOut;
    UWideString mText;
    const GE2DPoint* mLocation;
};

// The status bar's mouse handler `inWhich` (EE_SCREENS_UP_BAR_*) at the location (MouseMove's)
long EEUIFMGStatusBar::BarMouse(uint32_t inWhich, const GE2DPoint* inLocation)
{
    EEUIBUpgradeCall theCall = {this, NULL};
    theCall.mLocation = inLocation;
    return (long)ee_screens_upgrade_bar_mouse(&theCall, inWhich);
}

// FUNCTION: EXE 0x6569a2
// FUNCTION: EXE 0x656b3c
// FUNCTION: EXE 0x656bd3
// FUNCTION: EXE 0x656c83
// FUNCTION: EXE 0x656f5a
// FUNCTION: EXE 0x657081
// FUNCTION: EXE 0x657169
// The presses, availability, tooltip, level tracking and cost update: ee_screens upgrade.rs over the host below
void EEUIBUpgrade::ButtonUp()
{
    UWideString theReason;
    EEUIBUpgradeCall theCall = {this, &theReason};
    ee_screens_upgrade_button_up(&theCall);
}

bool EEUIBUpgrade::GetCanUpgrade(EEUnit*& outUnit, UWideString* outReason)
{
    EEUIBUpgradeCall theCall = {this, outReason};
    int64_t theUnit = -1;
    bool theResult = ee_screens_upgrade_can_upgrade(&theCall, &theUnit);
    if (theUnit != -1)
        outUnit = reinterpret_cast<EEUnit*>(theUnit);
    return theResult;
}

bool EEUIBUpgrade::GetDisabled()
{
    EEUIBUpgradeCall theCall = {this, NULL};
    return ee_screens_upgrade_disabled(&theCall);
}

bool EEUIBUpgrade::GetHelpString(UWideString& outString) const
{
    EEUIBUpgradeCall theCall = {const_cast<EEUIBUpgrade*>(this), &outString};
    return ee_screens_upgrade_help(&theCall);
}

long EEUIBUpgrade::MouseMove(const GE2DPoint& inLocation)
{
    EEUIBUpgradeCall theCall = {this, NULL};
    theCall.mLocation = &inLocation;
    return (long)ee_screens_upgrade_mouse_move(&theCall);
}

long EEUIBUpgrade::MouseRButtonUp()
{
    EEUIBUpgradeCall theCall = {this, NULL};
    return (long)ee_screens_upgrade_mouse_r_up(&theCall);
}

void EEUIBUpgrade::UpdateCost(const UWideString& inText)
{
    EEUIBUpgradeCall theCall = {this, const_cast<UWideString*>(&inText)};
    ee_screens_upgrade_update_cost(&theCall);
}

// FUNCTION: EXE 0x657040
// Shows the hover state only when the improvement can be bought
void EEUIBUpgrade::MouseOver()
{
    EEUnit* theUnit = NULL;
    if (this->GetCanUpgrade(theUnit, NULL))
        UIButton::MouseOver();
}

// FUNCTION: EXE 0x65706a
// Presses the button on a right press, capturing the mouse
long EEUIBUpgrade::MouseRButtonDown()
{
    this->CaptureMouse();
    this->ButtonDown();
    return kErrorOK;
}

// FUNCTION: EXE 0x65713e
// Marks the button and its cost label dirty
void EEUIBUpgrade::SetDirty(bool inDirty)
{
    if (inDirty == true && this->mCostLabel != NULL)
        this->mCostLabel->SetDirty(true);
    UIControl::SetDirty(inDirty);
}

// The button's host (upgrade.rs EE_SCREENS_UP_OP_*): units (as EEUnit*) and nodes as handles
extern "C" int64_t ee_screenscpp_upgrade(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    return EEUIBUpgrade::HostOp(*static_cast<EEUIBUpgradeCall*>(inCall), inOp, inA, inB, inC);
}

int64_t EEUIBUpgrade::HostOp(EEUIBUpgradeCall& ioCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIBUpgrade* theButton = ioCall.mButton;
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(inA);
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    long* theFields[5] = {&theButton->mAttribute, NULL, &theButton->mLevel, NULL, (long*)&theButton->mLastMessageTime};
    long theAttribute = theButton->mAttribute;
    float theFloat;
    uint32_t theBits;
    switch (inOp) {
    case EE_SCREENS_UP_OP_FIELD:
        if (inA == EE_SCREENS_UP_FIELD_EDITOR || inA == EE_SCREENS_UP_FIELD_DISABLED)
            return inA == EE_SCREENS_UP_FIELD_EDITOR ? theButton->mScenarioEditor : theButton->mDisabled;
        return *theFields[inA];
    case EE_SCREENS_UP_OP_SET_FIELD:
        if (inA == EE_SCREENS_UP_FIELD_DISABLED)
            theButton->mDisabled = inB != 0;
        else
            *theFields[inA] = (long)inB;
        return 0;
    case EE_SCREENS_UP_OP_HAS_GROUP:
        return theGroup != NULL;
    case EE_SCREENS_UP_OP_FIRST_UNIT:
        return (int64_t) static_cast<EEUnit*>(theGroup->GetFirstUnit());
    case EE_SCREENS_UP_OP_GROUP_COUNT:
        return theGroup->GetNumUnits();
    case EE_SCREENS_UP_OP_GROUP_UNIT:
        return (int64_t) static_cast<EEUnit*>(theGroup->GetUnit(0));
    case EE_SCREENS_UP_OP_ALLOWED:
        return gUnitImprovementsAllowed == true;
    case EE_SCREENS_UP_OP_CAN_BUY:
        return theUnit->mPlayer->CanBuy(theUnit->mUnitClass->GetUnitType(), theAttribute, ioCall.mOut);
    case EE_SCREENS_UP_OP_EDITOR_UNIT:
        return (int64_t) static_cast<EEUIFScenarioEditor*>(theButton->GetForm())->GetFirstSelectedUnit();
    case EE_SCREENS_UP_OP_EDITOR_COMPLEX:
        return (int64_t) static_cast<EEUnit*>(
            static_cast<EEUIFScenarioEditor*>(theButton->GetForm())->GetFirstSelectedUnit()->GetComplexUnit());
    case EE_SCREENS_UP_OP_CAN_RAISE:
        return theUnit->mStats->mImprovement.CanUpgrade(theUnit->mStats->mUnitClass, theAttribute) == true;
    case EE_SCREENS_UP_OP_MAXED:
        return theUnit->mPlayer->IsImprovementMaxed(theUnit->mUnitClass->GetUnitType(), theAttribute);
    case EE_SCREENS_UP_OP_STEP:
    case EE_SCREENS_UP_OP_COST_FACTOR:
        theFloat = inOp == EE_SCREENS_UP_OP_STEP ? theUnit->mUnitClass->mImprovements->mEntries[theAttribute].mStepFactor
                                                 : theUnit->mUnitClass->mImprovements->mEntries[theAttribute].mCostFactor;
        memcpy(&theBits, &theFloat, 4);
        return theBits;
    case EE_SCREENS_UP_OP_LEVEL:
        return theUnit->GetPlayer()->GetImprovementLevel(theUnit->mUnitClass->GetUnitType(), theAttribute);
    case EE_SCREENS_UP_OP_NODE: {
        long theType = theUnit->mUnitClass->GetUnitType();
        return (int64_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get(
            gUnitClassTable[theType] ? gUnitClassTable[theType]->GetTechnology() : 100000);
    }
    case EE_SCREENS_UP_OP_TEXT_NAME:
        theUnit->GetStats()->GetImprovementName(ioCall.mText, theAttribute);
        return 0;
    case EE_SCREENS_UP_OP_TEXT_GAIN:
        theUnit->GetStats()->GetImprovementGainText(ioCall.mText, theAttribute, (long)inB);
        return 0;
    case EE_SCREENS_UP_OP_TEXT_STRING:
        gResource->GetString((long)inA, ioCall.mText);
        return 0;
    case EE_SCREENS_UP_OP_TEXT_COST:
        theBits = (uint32_t)inC;
        memcpy(&theFloat, &theBits, 4);
        reinterpret_cast<EETechTreeNode*>(inA)->GetCostString(
            ioCall.mText, reinterpret_cast<EEUnit*>(inB)->mUnitClass->GetUnitType(), theFloat);
        return 0;
    case EE_SCREENS_UP_OP_OUT_APPEND:
        *ioCall.mOut += ioCall.mText;
        return 0;
    case EE_SCREENS_UP_OP_OUT_SET:
        *ioCall.mOut = ioCall.mText;
        return 0;
    case EE_SCREENS_UP_OP_OUT_BYTES:
        *ioCall.mOut += std::string(reinterpret_cast<const char*>(inA), (size_t)inB).c_str();
        return 0;
    case EE_SCREENS_UP_OP_BASE_HELP:
        theButton->UIControl::GetHelpString(inA == 0 ? *ioCall.mOut : ioCall.mText);
        return 0;
    case EE_SCREENS_UP_OP_SOUND:
        gSoundManager->Play(EESoundObject::Get(inA == 1 ? kSoundButton1 : kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f,
                            false, NULL);
        return 0;
    case EE_SCREENS_UP_OP_UPGRADE:
        (new EEAUpgradeImprovement(theUnit, theAttribute, inB != 0, !theButton->mScenarioEditor))->Dereference();
        return 0;
    case EE_SCREENS_UP_OP_OUT_LENGTH:
        return ioCall.mOut->GetLength();
    case EE_SCREENS_UP_OP_REFUSAL_INTERVAL:
        return gWorldTable.GetLongValue(kWorldRefusalMessageInterval);
    case EE_SCREENS_UP_OP_TIME:
        return TMGetTime();
    case EE_SCREENS_UP_OP_CHAT:
        gUserInterface->AddChatLogEntry(*ioCall.mOut, 0);
        return 0;
    case EE_SCREENS_UP_OP_UPDATE_HELP:
        theButton->GetForm()->UpdateHelpString();
        return 0;
    case EE_SCREENS_UP_OP_PRESSED:
        return theButton->GetState() == kButtonStatePressed;
    case EE_SCREENS_UP_OP_BASE_BUTTON_UP:
        theButton->UIButton::ButtonUp();
        return 0;
    case EE_SCREENS_UP_OP_BASE_MOVE:
        return theButton->UIButton::MouseMove(*ioCall.mLocation);
    case EE_SCREENS_UP_OP_HELP_TEXT:
        theButton->GetHelpString(ioCall.mText);
        return 0;
    case EE_SCREENS_UP_OP_HELP_LABEL:
        if (theButton->GetForm()->GetHelpTextLabel() != NULL)
            theButton->GetForm()->GetHelpTextLabel()->SetText(ioCall.mText);
        return 0;
    case EE_SCREENS_UP_OP_CONTAINS:
        return theButton->GetRectangle().Contains(theButton->GetMouseLocation());
    case EE_SCREENS_UP_OP_RELEASE_MOUSE:
        theButton->ReleaseCapturedMouse();
        return 0;
    case EE_SCREENS_UP_OP_COST_DIFFERS:
        return theButton->mCostLabel != NULL && *ioCall.mOut != theButton->mCostLabel->GetText(0);
    case EE_SCREENS_UP_OP_COST_SET:
        theButton->mCostLabel->SetText(*ioCall.mOut);
        return 0;
    case EE_SCREENS_UP_OP_DIRTY:
        theButton->SetDirty(true);
        return 0;
    case EE_SCREENS_UP_OP_BG_OPACITY:
    case EE_SCREENS_UP_OP_BORDER_OPACITY:
        theBits = (uint32_t)inA;
        memcpy(&theFloat, &theBits, 4);
        inOp == EE_SCREENS_UP_OP_BG_OPACITY ? theButton->SetBgOpacity(theFloat) : theButton->SetBorderOpacity(theFloat);
        return 0;
    case EE_SCREENS_UP_OP_BORDER_THICKNESS:
        theButton->SetBorderThickness((unsigned long)inA);
        return 0;
    case EE_SCREENS_UP_OP_BAR_FIELD: {
        EEUIFMGStatusBar* theBar = static_cast<EEUIFMGStatusBar*>(theButton);
        return inA == 0 ? theBar->mEnabled : inA == 1 ? theBar->mPressed : theButton->GetHasMouseCapture();
    }
    case EE_SCREENS_UP_OP_BAR_SET_PRESSED: static_cast<EEUIFMGStatusBar*>(theButton)->mPressed = inA != 0; return 0;
    case EE_SCREENS_UP_OP_BAR_INSIDE:
        return theButton->GetRectangle().Contains(inA == 0 ? theButton->GetForm()->GetMouseLocation() : *ioCall.mLocation);
    case EE_SCREENS_UP_OP_BAR_BASE:
        return inA == 0 ? theButton->UIButton::MouseLButtonDown()
             : inA == 1 ? theButton->UIButton::MouseLButtonUp() : theButton->EEUIBUpgrade::MouseRButtonDown();
    case EE_SCREENS_UP_OP_BAR_ICON: {
        UIBitmap* theIcon = static_cast<EEUIFMGStatusBar*>(theButton)->mIcon;
        GE2DPoint theTopLeft = theIcon->GetRectangle().GetTopLeft();
        theIcon->SetLocation(GE2DPoint(theTopLeft.GetX() + (float)inA, theTopLeft.GetY() + (float)inA));
        return 0;
    }
    }
    return 0;
}
