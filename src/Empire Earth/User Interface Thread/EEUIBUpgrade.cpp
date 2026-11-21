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
#include "ee/EEARust.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFMGStatusBar.h"
#include "rust/game/screens.h"   // ee_screens_upgrade_* (ee_screens upgrade.rs)
static const float kBgOpacity = 0.15f;

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

// Presses the button unless the improvement is unavailable, and updates the help text
void EEUIBUpgrade::ButtonDown()
{
    if (!this->GetDisabled())
        UIButton::ButtonDown();
    this->GetForm()->UpdateHelpString();
}

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

// Shows the hover state only when the improvement can be bought
void EEUIBUpgrade::MouseOver()
{
    EEUnit* theUnit = NULL;
    if (this->GetCanUpgrade(theUnit, NULL))
        UIButton::MouseOver();
}

// Presses the button on a right press, capturing the mouse
long EEUIBUpgrade::MouseRButtonDown()
{
    this->CaptureMouse();
    this->ButtonDown();
    return kErrorOK;
}

// Marks the button and its cost label dirty
void EEUIBUpgrade::SetDirty(bool inDirty)
{
    if (inDirty == true && this->mCostLabel != NULL)
        this->mCostLabel->SetDirty(true);
    UIControl::SetDirty(inDirty);
}

// The button's host (upgrade.rs EE_SCREENS_UP_OP_*): units (as EEUnit*) and nodes as handles (the selection's reads
// are hud_views.rs')
extern "C" int64_t ee_screenscpp_upgrade(void* inCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    return EEUIBUpgrade::HostOp(*static_cast<EEUIBUpgradeCall*>(inCall), inOp, inA, inB, inC);
}

int64_t EEUIBUpgrade::HostOp(EEUIBUpgradeCall& ioCall, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUIBUpgrade* theButton = ioCall.mButton;
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(inA);
    long theAttribute = theButton->mAttribute;
    switch (inOp) {
    case EE_SCREENS_UP_OP_ALLOWED:
        return gUnitImprovementsAllowed == true;
    case EE_SCREENS_UP_OP_CAN_BUY:
        return theUnit->mPlayer->CanBuy(theUnit->mUnitClass->GetUnitType(), theAttribute, ioCall.mOut);
    case EE_SCREENS_UP_OP_EDITOR_UNIT:
        return (int64_t) static_cast<EEUIFScenarioEditor*>(theButton->GetForm())->GetFirstSelectedUnit();
    case EE_SCREENS_UP_OP_CAN_RAISE:
        return theUnit->mStats->mImprovement.CanUpgrade(theUnit->mStats->mUnitClass, theAttribute) == true;
    case EE_SCREENS_UP_OP_MAXED:
        return theUnit->mPlayer->IsImprovementMaxed(theUnit->mUnitClass->GetUnitType(), theAttribute);
    case EE_SCREENS_UP_OP_LEVEL:
        return theUnit->GetPlayer()->GetImprovementLevel(theUnit->mUnitClass->GetUnitType(), theAttribute);
    case EE_SCREENS_UP_OP_TEXT_NAME:
        theUnit->GetStats()->GetImprovementName(ioCall.mText, theAttribute);
        return 0;
    case EE_SCREENS_UP_OP_TEXT_GAIN:
        theUnit->GetStats()->GetImprovementGainText(ioCall.mText, theAttribute, (long)inB);
        return 0;
    case EE_SCREENS_UP_OP_TEXT_STRING:
        gResource->GetString((long)inA, ioCall.mText);
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
    case EE_SCREENS_UP_OP_BORDER_THICKNESS:
        theButton->SetBorderThickness((unsigned long)inA);
        return 0;
    case EE_SCREENS_UP_OP_BAR_SET_PRESSED: static_cast<EEUIFMGStatusBar*>(theButton)->mPressed = inA != 0; return 0;
    case EE_SCREENS_UP_OP_BAR_BASE:
        return inA == 0 ? theButton->UIButton::MouseLButtonDown()
             : inA == 1 ? theButton->UIButton::MouseLButtonUp() : theButton->EEUIBUpgrade::MouseRButtonDown();
    }
    return 0;
}

// ---- the host's multi-statement cases (UI lane round 9: ee_game screens upgrade_calls.rs), each one statement on
// the call record's button
#include "ee/EEScreensShims.h"
#define UPC(inCall) (*reinterpret_cast<EEUIBUpgradeCall*>(inCall))
#define UPB(inCall) UPC(inCall).mButton
static const unsigned long kUpSounds[] = {kSoundButton2, kSoundButton1};
extern "C" int64_t ee_screenscpp_up_attribute(uintptr_t inCall) { return (int64_t)UPB(inCall)->mAttribute; }
extern "C" void ee_screenscpp_up_set_attribute(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mAttribute = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_level(uintptr_t inCall) { return (int64_t)UPB(inCall)->mLevel; }
extern "C" void ee_screenscpp_up_set_level(uintptr_t inCall, int64_t inValue) { UPB(inCall)->mLevel = (long)inValue; }
extern "C" int64_t ee_screenscpp_up_last_message(uintptr_t inCall)
{ return (int64_t)(long)UPB(inCall)->mLastMessageTime; }
extern "C" void ee_screenscpp_up_set_last_message(uintptr_t inCall, int64_t inValue)
{ UPB(inCall)->mLastMessageTime = (unsigned long)(long)inValue; }
extern "C" bool ee_screenscpp_up_editor(uintptr_t inCall) { return UPB(inCall)->mScenarioEditor; }
extern "C" bool ee_screenscpp_up_disabled(uintptr_t inCall) { return UPB(inCall)->mDisabled; }
extern "C" void ee_screenscpp_up_set_disabled(uintptr_t inCall, bool inValue) { UPB(inCall)->mDisabled = inValue; }
extern "C" uintptr_t ee_screenscpp_up_editor_complex(uintptr_t inCall)
{ return (uintptr_t)static_cast<EEUnit*>(static_cast<EEUIFScenarioEditor*>(UPB(inCall)->GetForm())->GetFirstSelectedUnit()->GetComplexUnit()); }
extern "C" float ee_screenscpp_up_step(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mEntries[UPB(inCall)->mAttribute].mStepFactor; }
extern "C" float ee_screenscpp_up_cost_factor(uintptr_t inCall, uintptr_t inUnit)
{ return reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->mImprovements->mEntries[UPB(inCall)->mAttribute].mCostFactor; }
extern "C" int64_t ee_screenscpp_up_unit_type(uintptr_t inUnit)
{ return (int64_t)reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(); }
extern "C" bool ee_screenscpp_up_class_exists(int64_t inUnitType) { return gUnitClassTable[(long)inUnitType] != NULL; }
extern "C" int64_t ee_screenscpp_up_class_technology(int64_t inUnitType)
{ return (int64_t)gUnitClassTable[(long)inUnitType]->GetTechnology(); }
extern "C" uintptr_t ee_screenscpp_up_node(int64_t inTechnology)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->GetTechTree()->Get((long)inTechnology); }
extern "C" void ee_screenscpp_up_text_cost(uintptr_t inCall, uintptr_t inNode, uintptr_t inUnit, float inFactor)
{ reinterpret_cast<EETechTreeNode*>(inNode)->GetCostString(UPC(inCall).mText, reinterpret_cast<EEUnit*>(inUnit)->mUnitClass->GetUnitType(), inFactor); }
extern "C" void ee_screenscpp_up_sound(uint32_t inWhich)
{ gSoundManager->Play(EESoundObject::Get(kUpSounds[inWhich])->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL); }
extern "C" uintptr_t ee_screenscpp_up_help_label(uintptr_t inCall)
{ return (uintptr_t)UPB(inCall)->GetForm()->GetHelpTextLabel(); }
extern "C" void ee_screenscpp_up_help_label_set(uintptr_t inLabel, uintptr_t inCall)
{ reinterpret_cast<UITextLabel*>(inLabel)->SetText(UPC(inCall).mText); }
extern "C" void ee_screenscpp_up_bg_opacity(uintptr_t inCall, float inOpacity) { UPB(inCall)->SetBgOpacity(inOpacity); }
extern "C" void ee_screenscpp_up_border_opacity(uintptr_t inCall, float inOpacity)
{ UPB(inCall)->SetBorderOpacity(inOpacity); }
extern "C" bool ee_screenscpp_up_bar_enabled(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mEnabled; }
extern "C" bool ee_screenscpp_up_bar_pressed(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mPressed; }
extern "C" bool ee_screenscpp_up_has_capture(uintptr_t inCall) { return UPB(inCall)->GetHasMouseCapture(); }
extern "C" bool ee_screenscpp_up_inside_form_mouse(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(UPB(inCall)->GetForm()->GetMouseLocation()); }
extern "C" bool ee_screenscpp_up_inside_location(uintptr_t inCall)
{ return UPB(inCall)->GetRectangle().Contains(*UPC(inCall).mLocation); }
extern "C" float ee_screenscpp_up_icon_left(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon->GetRectangle().GetTopLeft().GetX(); }
extern "C" float ee_screenscpp_up_icon_top(uintptr_t inCall)
{ return static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon->GetRectangle().GetTopLeft().GetY(); }
extern "C" void ee_screenscpp_up_icon_set_location(uintptr_t inCall, float inX, float inY)
{ static_cast<EEUIFMGStatusBar*>(UPB(inCall))->mIcon->SetLocation(GE2DPoint(inX, inY)); }
