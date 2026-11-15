// C:\EEX\Empire Earth\Scenario Editor\EESETSObject.cpp (file name inferred: no __FILE__; RTTI class EESETSObject).
// The trigger screen's object sub-screen: 0x18efb7..0x19393b (the ctor first, as EESETSArea/EESETSTrigger; then the
// out-of-line inlines it refused, the small control classes EESETSOUnitList/EESETSAMouseClick/EESETSALasso/
// EESETSAComboBox/EESETSAEditBox, the ??_G block, the dtor and the slots, the unit type sort's COMDATs and the event
// ??_Gs). After EESETSETSetObjectStance.cpp, before EESETSTrigger.cpp (0x19393b). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string.h>
#include <string>
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"

// Unit action names are the real EEUnitAction::sActionNames table at 0x563668.

// 0x563678: the unit action ids by name (the object screen's action combo box) are EEUnitAction::sActionIDs
#include "ee/EEUnitAction.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
// The unit type combo box's sort (0x19330c, the std::sort instantiation 0x193077..0x19370e): type 0 first, then the
// unit classes by name (provisional name)
struct EESETSOUnitTypeNameLess {
    bool operator()(const unsigned long& inA, const unsigned long& inB) const
    {
        UWideString theA;
        UWideString theB;
        bool theResult;
        if (inA != 0) {
            if (gUnitClassTable[inA] != NULL)
                gResource->GetString(gUnitClassTable[inA]->mNameStringID, theA);
            if (gUnitClassTable[inB] != NULL)
                gResource->GetString(gUnitClassTable[inB]->mNameStringID, theB);
            theResult = theA < theB;
        } else
            theResult = true;
        return theResult;
    }
};

// FUNCTION: EXE 0x591759
// Switches between pick-on-map and specification mode and refreshes the controls (inline; the ctor calls it)

// FUNCTION: EXE 0x59176b
// Creates the editor's mouse click selection action (EESEAMouseClick ctor, inline; the ctor calls it)

// FUNCTION: EXE 0x59177d
// Creates the editor's lasso selection action (EESEALasso ctor, inline; the ctor calls it)

// SYNTHETIC: EXE 0x591795
// (EESEALasso's scalar deleting destructor, ICF-shared with EESETSALasso's)

// FUNCTION: EXE 0x5917b7
// Selects a line of the unit list and always fires its selection-changed event (inFireEvent is ignored)
void EESETSOUnitList::ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
{
    UILBString::ChangeSelectionIndex(inIndex, false, inScrollTo);
    this->FireTriggerEvent(kControlTriggerSelectionChanged);
}

// SYNTHETIC: EXE 0x5917db
// (EESETSOUnitList's scalar deleting destructor)

// FUNCTION: EXE 0x5917fe
// Performs the editor's click selection, then refreshes the object page's unit list and name
void EESETSAMouseClick::Virtual4()
{
    EESEAMouseClick::Virtual4();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x59181f
// Performs the editor's lasso selection, then refreshes the object page's unit list and name
void EESETSALasso::Virtual4()
{
    EESEALasso::Virtual4();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x591840
// Creates an object page combo box, 20 pixels high
EESETSAComboBox::EESETSAComboBox(unsigned long inID, unsigned long inTextBoxType, const std::vector<UWideString>& inStrings,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inMaxListBoxHeight,
                                 UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inID, inTextBoxType, inStrings, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight, inForm, inIndex,
                 inMaxChars, inBorderThickness)
{
}

// FUNCTION: EXE 0x5918b8
// Changes the selection, then refreshes the object page's generated name
void EESETSAComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// FUNCTION: EXE 0x5918da
// Accepts the typed count, stores the object page into its scenario object and refreshes the page
void EESETSAEditBox::ProcessEnterKey()
{
    UIEditBox::ProcessEnterKey();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject)->Store();
    static_cast<EESETSObject*>(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
}

// SYNTHETIC: EXE 0x591918
// (EESETSAEditBox's scalar deleting destructor)

// SYNTHETIC: EXE 0x591935
// (EESETSObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x591951
// (EESEAMouseClick's scalar deleting destructor, ICF-shared with EESETSAMouseClick's)

// The object screen whole in Rust (ee_screens editor_object.rs): the constructor (0x58efb7), the destructor
// (0x591973), Sort, MoveDown, MoveUp, Delete, ClearUnitList, Deactivate, Refresh, Duplicate, Activate (0x591d63),
// Store (0x5920f1), GetNames, GetIsSelectOnMap, Hide, Load (0x5925a2), New, UpdateSelection (0x592ad3),
// CheckChanged, Show and MouseMove are its members; the class forwards. The control classes below stay C++, and
// this unit is the screen's host (ee_screenscpp_seob).

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// FUNCTION: EXE 0x58efb7
// Builds the trigger editor's object page: pick mode radios, count, owner, family, class and parent fields,
// the selected unit list and the area, near, attribute, visible-by, state and selected-by filters (editor_object.rs)
EESETSObject::EESETSObject(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_seob_new(inForm, &theIndex);
    ioIndex = theIndex;
}

// FUNCTION: EXE 0x591973
// Destroys the object page, deleting its controls and releasing its click and lasso actions (editor_object.rs)
EESETSObject::~EESETSObject()
{
    ee_screens_seob_free(this->mRust);
}

static uint32_t EESETSObjectCall(EeScreensSeObject* inRust, uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0)
{
    return ee_screens_seob_call(inRust, inWhat, inA, inB);
}

void EESETSObject::Sort() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_SORT, (uint32_t)this->mSelection); }
void EESETSObject::MoveDown() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_DOWN, (uint32_t)this->mSelection); }
void EESETSObject::MoveUp() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_UP, (uint32_t)this->mSelection); }
void EESETSObject::Delete() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_DELETE, (uint32_t)this->mSelection); }
void EESETSObject::Duplicate() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_DUPLICATE, (uint32_t)this->mSelection); }
void EESETSObject::New() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LIST_OP, EE_SCREENS_SEOB_SM_NEW, (uint32_t)this->mSelection); }
void EESETSObject::ClearUnitList() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_CLEAR_UNIT_LIST); }
void EESETSObject::Deactivate() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_DEACTIVATE); }
void EESETSObject::Refresh() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_REFRESH); }
void EESETSObject::Activate() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_ACTIVATE); }
void EESETSObject::Store() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_STORE, (uint32_t)this->mSelection); }
void EESETSObject::Hide() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_HIDE); }
void EESETSObject::Load() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_LOAD, (uint32_t)this->mSelection); }
void EESETSObject::Show() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_SHOW); }
void EESETSObject::UpdateSelection() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_UPDATE_SELECTION, (uint32_t)this->mSelection); }
bool EESETSObject::GetIsSelectOnMap() { return EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_IS_SELECT_ON_MAP) != 0; }
void EESETSObject::SetIsSelectOnMap(bool inMap) { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_SET_SELECT_ON_MAP, inMap); }
void EESETSObject::ClearUnitIDs() { EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_CLEAR_UNIT_IDS); }
void EESETSObject::GetNames(std::vector<UWideString>& outNames) { ee_screens_seob_get_names(this->mRust, &outNames); }
void EESETSObject::MouseMove(const GE2DPoint& inLocation)
{
    ee_screens_seob_mouse_move(this->mRust, inLocation.GetX(), inLocation.GetY());
}
void EESETSObject::CheckChanged(UIEntity* inCheckBox)
{
    EESETSObjectCall(this->mRust, EE_SCREENS_SEOB_CHECK_CHANGED, (uint32_t) static_cast<UIControl*>(inCheckBox)->GetId(),
                     (uint32_t) static_cast<UITriStateCheckBox*>(inCheckBox)->GetCheckState());
}

// ---- the host (editor_object.rs: the ops from 100)

static_assert(EE_SCREENS_SEOB_IDS_ANY_CLASS == IDS_ANY_CLASS && EE_SCREENS_SEOB_IDS_NONE == IDS_NONE &&
                  EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS == IDS_SE_UNIT_GROUP_UNITS &&
                  EE_SCREENS_SEOB_IDS_FAMILY == IDS_FAMILY && EE_SCREENS_SEOB_IDS_SELECT_ON_MAP == IDS_SELECT_ON_MAP &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 1 == IDS_OBJECT_SPECIFICATION &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 2 == IDS_TRIGGER_EDITOR_OWNER &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 3 == IDS_CLASS &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 5 == IDS_TRIGGER_EDITOR_MIN &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 6 == IDS_TRIGGER_EDITOR_MAX &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 7 == IDS_DYNAMIC && EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 8 == IDS_IN_AREA &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 9 == IDS_VISIBLE_BY &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 10 == IDS_HAS_STATE &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 11 == IDS_SELECTED_BY &&
                  EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 12 == IDS_HAS_ATTRIBUTE &&
                  EE_SCREENS_SEOB_IDS_UNIT_SELECTED_LIST == IDS_UNIT_SELECTED_LIST &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_EDITOR_USE_OBJECT == IDS_TRIGGER_EDITOR_USE_OBJECT &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_PROXIMITY_FIRST == IDS_TRIGGER_PROXIMITY_FIRST &&
                  EE_SCREENS_SEOB_IDS_TRIGGER_ATTRIBUTE_FIRST == IDS_TRIGGER_ATTRIBUTE_FIRST,
              "the strings");
static_assert(EE_SCREENS_SEOB_AREA_CHECK == kSETSObjectAreaCheck && EE_SCREENS_SEOB_NEAR_CHECK == kSETSObjectNearCheck &&
                  EE_SCREENS_SEOB_VISIBLE_BY_CHECK == kSETSObjectVisibleByCheck &&
                  EE_SCREENS_SEOB_HAS_STATE_CHECK == kSETSObjectHasStateCheck &&
                  EE_SCREENS_SEOB_SELECTED_BY_CHECK == kSETSObjectSelectedByCheck &&
                  EE_SCREENS_SEOB_SELECT_UNITS == kSETSObjectSelectUnits,
              "the ids");

namespace {

EEUIFScenarioEditor* SeobEditor(void* inControl)
{
    return static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(inControl)->GetForm());
}

// a scenario object's number field (EE_SCREENS_SEOB_F_*; the three flags and the parent aside)
unsigned long* SeobField(EESMObject* o, int32_t inField)
{
    switch (inField) {
    case EE_SCREENS_SEOB_F_OWNER: return &o->mOwner;
    case EE_SCREENS_SEOB_F_FAMILY: return &o->mFamily;
    case EE_SCREENS_SEOB_F_CLASS: return &o->mClass;
    case EE_SCREENS_SEOB_F_MIN: return &o->mMinCount;
    case EE_SCREENS_SEOB_F_MAX: return &o->mMaxCount;
    case EE_SCREENS_SEOB_F_AREA: return &o->mArea;
    case EE_SCREENS_SEOB_F_NEAR_MODE: return &o->mNearMode;
    case EE_SCREENS_SEOB_F_NEAR_OBJECT: return &o->mNearObject;
    case EE_SCREENS_SEOB_F_PARENT: return &o->mParent;
    case EE_SCREENS_SEOB_F_ATTRIBUTE: return &o->mAttribute;
    case EE_SCREENS_SEOB_F_ATTRIBUTE_MIN: return &o->mAttributeMin;
    case EE_SCREENS_SEOB_F_ATTRIBUTE_MAX: return &o->mAttributeMax;
    case EE_SCREENS_SEOB_F_VISIBLE_BY: return &o->mVisibleByPlayer;
    case EE_SCREENS_SEOB_F_STATE: return &o->mState;
    case EE_SCREENS_SEOB_F_SELECTED_BY: return &o->mSelectedByPlayer;
    case EE_SCREENS_SEOB_F_IN_AREA_CHECK: return &o->mInAreaCheck;
    case EE_SCREENS_SEOB_F_NEAR_CHECK: return &o->mNearCheck;
    case EE_SCREENS_SEOB_F_HAS_ATTRIBUTE_CHECK: return &o->mHasAttributeCheck;
    case EE_SCREENS_SEOB_F_VISIBLE_BY_CHECK: return &o->mVisibleByCheck;
    case EE_SCREENS_SEOB_F_HAS_STATE_CHECK: return &o->mHasStateCheck;
    case EE_SCREENS_SEOB_F_SELECTED_BY_CHECK: return &o->mSelectedByCheck;
    }
    return NULL;
}

bool* SeobFlag(EESMObject* o, int32_t inField)
{
    switch (inField) {
    case EE_SCREENS_SEOB_F_SELECT_ON_MAP: return &o->mIsSelectOnMap;
    case EE_SCREENS_SEOB_F_DYNAMIC: return &o->mIsDynamic;
    case EE_SCREENS_SEOB_F_PERCENT: return &o->mIsAttributePercent;
    }
    return NULL;
}

// a text into Rust's buffer: its whole length in bytes
int32_t SeobText(const char* inText, void* outBuffer, int32_t inSize)
{
    size_t theLength = strlen(inText);
    memcpy(outBuffer, inText, (std::min)(theLength, (size_t)(inSize > 0 ? inSize : 0)));
    return (int32_t)theLength;
}

int32_t SeobGetText(int32_t* i, void* const* p)
{
    UWideString theText;
    switch (i[0]) {
    case EE_SCREENS_SEOB_TEXT_OBJECT_NAME: {
        EESMObject* o = gWorld.mScenarioManager->GetScenarioObject(i[1]);
        if (o == NULL)
            return -1;
        theText = o->mName;
        break;
    }
    case EE_SCREENS_SEOB_TEXT_UNIT_CLASS:
        gResource->GetString(static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[1]))->mSelectedUnits[i[1]]
                                 ->mUnitClass->mNameStringID,
                             theText);
        break;
    case EE_SCREENS_SEOB_TEXT_STRING: gResource->GetString(i[1], theText); break;
    case EE_SCREENS_SEOB_TEXT_LINE: {
        // (a line past the list: the original read past it; none here)
        auto* theList = static_cast<UIComboBox*>(p[1])->GetListBox();
        if ((unsigned long)i[1] < theList->GetLineCount())
            theText = theList->GetData((unsigned long)i[1]);
        break;
    }
    }
    return SeobText((const char*)theText, p[0], i[2]);
}

}   // namespace

extern "C" int32_t ee_screenscpp_seob(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIComboBox* theCombo = static_cast<UIComboBox*>(p[0]);
    EESMObject* o = NULL;
    if (inOp == EE_SCREENS_SEOB_OP_OBJ_GET || inOp == EE_SCREENS_SEOB_OP_OBJ_SET || inOp == EE_SCREENS_SEOB_OP_OBJ_EXISTS ||
        (inOp >= EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN && inOp <= EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS) ||
        inOp == EE_SCREENS_SEOB_OP_OBJ_SET_NAME) {
        o = gWorld.mScenarioManager->GetScenarioObject(i[0]);
        if (o == NULL && inOp != EE_SCREENS_SEOB_OP_OBJ_EXISTS)
            return 0;
    }
    // (the trigger screen: not there yet while the editor makes its tabs)
    auto theScreen = [] { return gUIManager->mScenarioEditorForm->GetTriggerScreen(); };
    switch (inOp) {
    case EE_SCREENS_SEOB_OP_SELECT_UNITS: {
        UIForm* theForm = static_cast<UIForm*>(p[2]);
        EESETSAMouseClick* theClick = new EESETSAMouseClick;
        EESETSALasso* theLasso = new EESETSALasso;
        EESEUICTriggerSelectUnits* theSelect =
            new EESEUICTriggerSelectUnits(theForm, kSETSObjectSelectUnits, (unsigned long)i[0], theClick, theLasso);
        theForm->AddControl(theSelect, 0);
        void** theOut = static_cast<void**>(p[0]);
        theOut[0] = theClick;
        theOut[1] = theLasso;
        theOut[2] = theSelect;
        return 0;
    }
    case EE_SCREENS_SEOB_OP_RELEASE:
        static_cast<EESETSALasso*>(p[1])->Dereference();
        static_cast<EESETSAMouseClick*>(p[0])->Dereference();
        return 0;
    case EE_SCREENS_SEOB_OP_UNIT_SET_COUNT: return (int32_t)gUnitSetManager.GetSize();
    case EE_SCREENS_SEOB_OP_UNIT_SET_SHOWN:
        return gUnitSetManager.GetSet(i[0]) != NULL && gUnitSetManager.GetSet(i[0])->mShowInEditor == true;
    case EE_SCREENS_SEOB_OP_UNIT_TYPES: {
        std::vector<unsigned long> theTypes;
        theTypes.push_back(0);
        for (unsigned long t = 1; t < EEUnitClassCount(); t++) {
            EEUnitClass* theClass = gUnitClassTable[t];
            if (theClass != NULL && theClass->mCategory != kUnitCategoryProjectile &&
                theClass->mCategory != kUnitCategoryMissile && theClass->mIsScenarioSelectable == true)
                theTypes.push_back(t);
        }
        std::sort(theTypes.begin(), theTypes.end(), EESETSOUnitTypeNameLess());
        uint32_t* theOut = static_cast<uint32_t*>(p[0]);
        for (size_t k = 0; k < theTypes.size() && k < (size_t)i[0]; k++)
            theOut[k] = (uint32_t)theTypes[k];
        return (int32_t)theTypes.size();
    }
    case EE_SCREENS_SEOB_OP_CLASS_NAMES: {
        UWideString theName;
        const uint32_t* theTypes = static_cast<const uint32_t*>(p[1]);
        theCombo->DeleteAllEntries();
        gResource->GetString(IDS_ANY_CLASS, theName);
        theCombo->AppendLine(theName);
        for (int32_t k = 1; k < i[0]; k++) {
            gResource->GetString(gUnitClassTable[theTypes[k]]->mNameStringID, theName);
            theCombo->AppendLine(theName);
        }
        return 0;
    }
    case EE_SCREENS_SEOB_OP_ACTION_APPEND: {
        const char* theName = EEUnitAction::GetActionName(i[0]);   // port: the Rust table ("" unknown)
        if (*theName != 0)
            theCombo->AppendLine(UWideString(UString(theName)));
        return 0;
    }
    case EE_SCREENS_SEOB_OP_SM:
        switch (i[0]) {
        case EE_SCREENS_SEOB_SM_SORT: gWorld.mScenarioManager->SortList(EE_SCENARIO_LIST_OBJECTS); break;
        case EE_SCREENS_SEOB_SM_DOWN: gWorld.mScenarioManager->MoveEntry(EE_SCENARIO_LIST_OBJECTS, i[1], false); break;
        case EE_SCREENS_SEOB_SM_UP: gWorld.mScenarioManager->MoveEntry(EE_SCENARIO_LIST_OBJECTS, i[1], true); break;
        case EE_SCREENS_SEOB_SM_DELETE: gWorld.mScenarioManager->DeleteEntry(EE_SCENARIO_LIST_OBJECTS, i[1]); break;
        case EE_SCREENS_SEOB_SM_DUPLICATE: gWorld.mScenarioManager->DuplicateEntry(EE_SCENARIO_LIST_OBJECTS, i[1]); break;
        default: gWorld.mScenarioManager->NewEntry(EE_SCENARIO_LIST_OBJECTS); break;
        }
        return 0;
    case EE_SCREENS_SEOB_OP_CLEAR_SELECTION: SeobEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEOB_OP_PLAYER_COUNT: return (int32_t)SeobEditor(p[0])->GetPlayerScreen()->GetPlayerCount();
    case EE_SCREENS_SEOB_OP_TRIGGER_TEXT:
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->HideText();
        gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->ShowAlphaSort();
        return 0;
    case EE_SCREENS_SEOB_OP_PLAYER_COMBO: {
        EESETSUICBPlayer* thePlayer = static_cast<EESETSUICBPlayer*>(p[0]);
        switch (i[0]) {
        case EE_SCREENS_SEOB_PC_COUNT: thePlayer->SetPlayerCount((unsigned long)i[1]); break;
        case EE_SCREENS_SEOB_PC_FLAGS: thePlayer->SetFlags((unsigned long)i[1]); break;
        case EE_SCREENS_SEOB_PC_GET: return (int32_t)thePlayer->GetSelectedPlayer();
        default: thePlayer->SetSelectedPlayer((unsigned long)(long)i[1]); break;
        }
        return 0;
    }
    case EE_SCREENS_SEOB_OP_APPEND_NAMES: {
        std::vector<UWideString> theNames;
        gWorld.GetScenarioManager()->GetNames(i[0] == EE_SCREENS_SEOB_NAMES_AREAS ? EE_SCENARIO_NAMES_AREAS
                                                                                 : EE_SCENARIO_NAMES_OBJECTS,
                                              theNames);
        for (size_t k = 0; k < theNames.size(); k++) {
            theCombo->AppendLine(theNames[k]);
            if (p[1] != NULL)
                static_cast<UIComboBox*>(p[1])->AppendLine(theNames[k]);
        }
        return 0;
    }
    case EE_SCREENS_SEOB_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_OBJECTS,
                                          *static_cast<std::vector<UWideString>*>(p[0]));
        return 0;
    case EE_SCREENS_SEOB_OP_OBJ_GET:
        if (bool* theFlag = SeobFlag(o, i[1]))
            return *theFlag;
        return (int32_t)*SeobField(o, i[1]);
    case EE_SCREENS_SEOB_OP_OBJ_SET:
        if (bool* theFlag = SeobFlag(o, i[1]))
            *theFlag = i[2] != 0;
        else if (i[1] == EE_SCREENS_SEOB_F_PARENT)
            o->SetParent((long)i[2]);
        else   // (a none selection is all ones, as the original's unsigned long)
            *SeobField(o, i[1]) = (unsigned long)(long)i[2];
        return 0;
    case EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN: o->SetName(UWideString(theScreen()->GetName())); return 0;
    case EE_SCREENS_SEOB_OP_OBJ_SET_UNITS: {
        const uint32_t* theIDs = static_cast<const uint32_t*>(p[0]);
        o->SetUnits(std::vector<unsigned long>(theIDs, theIDs + i[1]));
        return 0;
    }
    case EE_SCREENS_SEOB_OP_OBJ_EXISTS: return o != NULL;
    case EE_SCREENS_SEOB_OP_NAME_TO_EDIT: theScreen()->GetNameEditBox()->SetText(o->mName); return 0;
    case EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS: {
        std::vector<unsigned long> theUnits;
        o->GetUnits(theUnits);
        for (size_t k = 0; k < theUnits.size(); k++) {
            RTSUnit* theUnit = gWorld.GetUnit(theUnits[k]);
            if (theUnit != NULL)
                SeobEditor(p[0])->SelectUnit(static_cast<EEUnit*>(theUnit));
        }
        return 0;
    }
    case EE_SCREENS_SEOB_OP_ACTION_ID: {
        // (no selected line: the original read before its list; the empty name here)
        unsigned long theAction;
        theCombo->GetCurrentSelection(theAction);
        UString theActionName;
        if (theAction < theCombo->GetListBox()->GetLineCount())
            theActionName = *(theCombo->GetListBox()->GetDataArray().begin() + theAction);
        return (int32_t)EEUnitAction::GetActionID((const char*)theActionName);
    }
    case EE_SCREENS_SEOB_OP_ACTION_INDEX: {
        std::vector<UWideString> theActions = theCombo->GetListBox()->GetDataArray();
        size_t k;
        for (k = 0; k < theActions.size(); k++) {
            const char* theName = EEUnitAction::GetActionName((unsigned long)i[0]);
            if (*theName != 0 && theActions[k] == UString(theName))
                break;
        }
        return (int32_t)k;
    }
    case EE_SCREENS_SEOB_OP_UNIT_LIST_FILL: {
        EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[1]));
        UILBString* theList = static_cast<UILBString*>(p[0]);
        UWideString theName;
        theList->DeleteAllEntries();
        unsigned long theCount = theEditor->GetSelectedUnitCount();
        for (unsigned long k = 0; k < theCount; k++) {
            gResource->GetString(theEditor->mSelectedUnits[k]->mUnitClass->mNameStringID, theName);
            theList->AppendLine(theName);
        }
        return (int32_t)theCount;
    }
    case EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->mSelectedUnits[i[0]]->mUnitID;
    case EE_SCREENS_SEOB_OP_SELECTED_COUNT:
        return (int32_t) static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetSelectedUnitCount();
    case EE_SCREENS_SEOB_OP_TEXT: {
        int32_t k[3] = {i[0], i[1], i[2]};
        void* const q[2] = {p[0], p[1]};
        return SeobGetText(k, q);
    }
    case EE_SCREENS_SEOB_OP_OBJ_SET_NAME:
        o->SetName(UWideString(std::string(static_cast<const char*>(p[0]), (size_t)i[1]).c_str()));
        theScreen()->GetNameEditBox()->SetText(o->mName);
        return 0;
    case EE_SCREENS_SEOB_OP_VIEW_PRESS: {
        GE2DPoint theLocation(f[0], f[1]);
        return gViewport.GetViewRectangle().Contains(theLocation) &&
               (UIManager::GetLMouseButton()->JustPressed() || UIManager::GetRMouseButton()->JustPressed());
    }
    case EE_SCREENS_SEOB_OP_START_SELECTION:
        static_cast<EESEUICTriggerSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    }
    return 0;
}
// FUNCTION: EXE 0x593014
// Finds a unit action name by id (map<unsigned long, UString>::find)

// FUNCTION: EXE 0x59304e
// Returns the first action name entry whose id is not below a key (map<unsigned long, UString>::_Lbound)

// FUNCTION: EXE 0x593077
// Sorts the class list by name, class 0 (any class) first (std::sort with EESETSOUnitTypeNameLess)
// (the sort's helpers follow: 0x593077..0x59370e)

// FUNCTION: EXE 0x593134
// Insertion-sorts a range of the class list by name (std::_Insertion_sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59315d
// Quicksorts a range of the class list by name, heap-sorting when too deep (std::_Sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593230
// Insertion-sorts the class list unguarded (std::_Unguarded_insertion_sort, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593261
// Insertion-sorts a range of the class list by name (std::_Insertion_sort_1, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59330c
// Compares two unit classes by name, class 0 always first (EESETSOUnitTypeNameLess; also true for 0 vs 0)

// FUNCTION: EXE 0x5933a0
// Makes a heap of a range of the class list (std::make_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5933da
// Sorts a heap of the class list (std::sort_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59341b
// Partitions a range of the class list around a median (std::_Unguarded_partition, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59352c
// Inserts one class into the sorted part before it (std::_Unguarded_insert, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x59355c
// Makes a heap of a range of the class list (std::_Make_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5935a0
// Moves the heap's top class to the end (std::pop_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5935c9
// Sifts a class down the heap (std::_Adjust_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593636
// Moves the heap's top class to the end (std::_Pop_heap_0, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x593689
// Sifts a class up the heap (std::_Push_heap, EESETSOUnitTypeNameLess)

// FUNCTION: EXE 0x5936da
// Moves the heap's top class to a given slot and re-heaps (std::_Pop_heap, EESETSOUnitTypeNameLess)

// SYNTHETIC: EXE 0x59370e
// (UITriStateCheckBox's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5937ac
// Destroys a list of strings (vector<UWideString>::~vector, a jump to _Tidy; the ctor calls it)

// FUNCTION: EXE 0x5937b1
// Copies a list of strings (vector<UWideString>::operator=)

// FUNCTION: EXE 0x593925
// Returns the number of strings in a vector<UWideString> (size(), called by operator=)
