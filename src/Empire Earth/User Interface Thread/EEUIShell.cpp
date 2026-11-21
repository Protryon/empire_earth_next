// New (port; UI cluster round 3): the generic C++ shells' base calls and their host (ee/EEUIShell.h; ee_game screens
// shells.rs): the game's C++ each kind's Rust calls, one case per step of the deleted classes' code.
#include <windows.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"
#include "lle/UIControl.h"
#include "lle/UIFontTable.h"
#include "lle/UIButton.h"
#include "lle/UIRadioButton.h"
#include "lle/UICheckBox.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UISlider.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "lle/GEBitmap.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEUserInterface.h"
#include "ee/EESEARust.h"
#include "lle/Input System.h"
#include "ee/EEGhostUnit.h"
#include "ee/EESEUnit.h"
#include "GEWindow.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEUIManager.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"
#include "rust/render/ge.h"
#include "lle/SESoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EEMessage.h"
#include "ee/EEMessageID.h"
#include "ee/EECommunications.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/GELine.h"
#include "lle/UIControlsTable.h"
#include <set>
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEUIShell.h"

int64_t EEUIShellButton::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIButton)
    EE_SHELL_BUTTON_BASE(UIButton)
    }
    return 0;
}

int64_t EEUIShellControl::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(EEUIRustControl)
    }
    return 0;
}

int64_t EEUIShellForm::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    GERectangle r = ioF != NULL ? GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]) : GERectangle();
    switch (inVirtual) {
    case EE_SCREENS_SV_ACTIVATE: UIForm::Activate(); return 0;
    case EE_SCREENS_SV_DEACTIVATE: UIForm::Deactivate(); return 0;
    case EE_SCREENS_SV_HELP_POPUP: UIForm::SetHelpTextPopupLocation(); return 0;
    case EE_SCREENS_SV_UPDATE_KEYBOARD: return UIForm::UpdateKeyboard();
    case EE_SCREENS_SV_UPDATE_MOUSE: return UIForm::UpdateMouse();
    case EE_SCREENS_SV_RELAYOUT: UIForm::RelayoutForm(r); return 0;
    case EE_SCREENS_SV_TRANSLATED_KEY: return UIForm::UpdateTranslatedKey(reinterpret_cast<const char*>(inA), (short)inB);
    case EE_SCREENS_SV_DRAW_BACKGROUND: return UIForm::DrawBackground(r);
    case EE_SCREENS_SV_CLOSE: UIForm::Close(); return 0;
    case EE_SCREENS_SV_COPY_DATA: return UIForm::CopyData();
    case EE_SCREENS_SV_DRAW_POLYGONS: return UIForm::DrawPolygons();
    case EE_SCREENS_SV_DRAW_TEXT: return UIForm::DrawTextA();
    case EE_SCREENS_SV_OPEN: UIForm::Open(); return 0;
    case EE_SCREENS_SV_SET_RECTANGLE: UIForm::SetRectangle(r); return 0;
    }
    return 0;
}

// the form's own virtual inVirtual through its vtable (a kind's override runs)
int64_t EEUIShellForm::SelfCall(uint32_t inVirtual)
{
    switch (inVirtual) {
    case EE_SCREENS_SV_ACTIVATE: this->Activate(); return 0;
    case EE_SCREENS_SV_DEACTIVATE: this->Deactivate(); return 0;
    case EE_SCREENS_SV_UPDATE_KEYBOARD: return this->UpdateKeyboard();
    case EE_SCREENS_SV_UPDATE_MOUSE: return this->UpdateMouse();
    case EE_SCREENS_SV_CLOSE: this->Close(); return 0;
    case EE_SCREENS_SV_COPY_DATA: return this->CopyData();
    case EE_SCREENS_SV_DRAW_POLYGONS: return this->DrawPolygons();
    case EE_SCREENS_SV_OPEN: this->Open(); return 0;
    }
    return 0;
}

// The kinds' class ops (EEUIRustControl's host passes them on): the icon bar's (icon_bar.rs EE_SCREENS_IB_OP_*; once
// EEUICUnitIconBar::RustOp), its slots 0 the icon, 1 the frame, 2 and 3 the press and release samples
int64_t EEUIShellControl::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    if (this->mKind != EE_SCREENS_SK_ICON_BAR)
        return 0;
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(inA);
    EEButtonObject* theButton = reinterpret_cast<EEButtonObject*>(inA);
    GEBitmap* theIcon = static_cast<GEBitmap*>(this->mSlots[0]);
    GEBitmap* theFrame = static_cast<GEBitmap*>(this->mSlots[1]);
    NEMessage* theMessage = NULL;
    switch (inOp - EE_SCREENS_HC_CLASS) {
    case EE_SCREENS_IB_OP_CLASS: {
        EEUnitClass* theClass = theUnit->GetUnitClass();
        return (theClass->IsTransport() ? EE_SCREENS_IB_CLASS_TRANSPORT : 0) |
               (theClass->IsAirBase() ? EE_SCREENS_IB_CLASS_AIR_BASE : 0) |
               (theClass->mIsParatrooperBomber ? EE_SCREENS_IB_CLASS_PARATROOPER_BOMBER : 0) | (int64_t)theClass->mFamily << 8;
    }
    case EE_SCREENS_IB_OP_QUEUED_GOAL: return theUnit->GetQueuedGoal((unsigned long)inB) != NULL;
    case EE_SCREENS_IB_OP_NO_GOALS: return theUnit->GetQueueGoals(1).size() == 0 || theUnit->mGoalQueues[1].GetFront() == NULL;
    case EE_SCREENS_IB_OP_SELECT_THESE: {
        const int32_t* theIDs = reinterpret_cast<const int32_t*>(inA);
        std::vector<long> theList(theIDs, theIDs + inB);
        theMessage = new EEMRecord(kMessageSelectTheseUnits, {theList});
        break;
    }
    case EE_SCREENS_IB_OP_SOUND:
        gSoundManager->Play((unsigned long)(uintptr_t)this->mSlots[inA == 0 ? 2 : 3], 1.0f, 0.0f, 1.0f, false, NULL);
        break;
    case EE_SCREENS_IB_OP_CTRL: return UIControl::ControlKeyPressed();
    case EE_SCREENS_IB_OP_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_IB_OP_MOUSE: ioF[0] = this->GetMouseLocation().GetX(); break;
    case EE_SCREENS_IB_OP_CONTAINS: return this->GetRectangle().Contains(this->GetMouseLocation());
    case EE_SCREENS_IB_OP_ACQUIRE: theButton->Acquire(); break;
    case EE_SCREENS_IB_OP_RELEASE: theButton->Release(); break;
    case EE_SCREENS_IB_OP_ICON:
        theIcon->SetTexture(theButton->GetTexture(), this->GetScreenIndex());
        theButton->SetFaceBitmap(theIcon, GERectangle(ioF[2], ioF[3], ioF[4], ioF[5]), gScreenIndex);
        theIcon->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        this->GetRasterizer()->Draw(theIcon, this->GetScreenIndex());
        break;
    case EE_SCREENS_IB_OP_FRAME:
        theFrame->SetLocation(GE2DPoint(ioF[0], ioF[1]));
        return this->GetRasterizer()->Draw(theFrame, this->GetScreenIndex());
    }
    if (theMessage != NULL)
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    return 0;
}

// A new selected-units icon bar (EEUICUnitIconBar: the user-defined control factory's): the icon bitmap, the
// table's normal bitmap at one icon's rectangle as its frame, the two button sounds loaded
UIControl* EEUIShellNewIconBar(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
{
    EEUIShellControl* theBar = new EEUIShellControl(EE_SCREENS_SK_ICON_BAR, {}, inTable.GetID(), inRectangle, inForm,
                                                    inTable.GetFormIndex(), 0);
    float theIcon[4];
    EEUIShell::Op(theBar, EE_SCREENS_SKOP_IB_ICON_RECT, 0, 0, theIcon);
    theBar->mSlots[0] = new GEBitmap;
    theBar->mSlots[1] = inTable.CreateNormalBitmap(inForm->GetManager(), GERectangle(theIcon[0], theIcon[1], theIcon[2], theIcon[3]));
    for (int i = 0; i < 2; i++)
        if (EESoundObject* theSound = EESoundObject::Get(i == 0 ? kSoundButton1 : kSoundButton2)) {
            theSound->LoadMedia();
            theBar->mSlots[2 + i] = (void*)(uintptr_t)theSound->GetSampleID();
        }
    return theBar;
}

// A new chat display line (EEUICChatLine: the user-defined control factory's; shells_chat.rs): the table's
// background, then its bold name, the message and their drawn copies (the kind's record holds them; the name's two
// at the line's top left)
UIControl* EEUIShellNewChatLine(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
{
    float theOpacity = inTable.GetBackgroundOpacity();
    uint32_t theBits;
    memcpy(&theBits, &theOpacity, 4);
    EEUIShellControl* theLine = new EEUIShellControl(EE_SCREENS_SK_CHAT_LINE, {(int64_t)theBits}, inTable.GetID(),
                                                     inRectangle, inForm, inTable.GetFormIndex(),
                                                     inTable.GetBorderThickness());
    theLine->SetAlwaysDirty(inTable.GetAlwaysDirty());
    theLine->SetBgColor(inTable.GetBackground());
    theLine->SetBgOpacity(theOpacity);
    GEFont* theFont = inTable.GetFont();
    for (int i = 0; i < 4; i++) {   // the name, its copy, the message, its copy
        GEText* theText = new GEText(i >= 2 ? theFont : UIFontTable::FindOrCreateFont(theFont->GetFontName(),
                                     theFont->GetFontStyle() | kFontStyleBold, theFont->GetFontQuality(), theFont->GetFontSize()));
        inTable.SetupGEText(theText);
        if (i < 2)
            theText->SetLocation(theLine->GetRectangle().GetTopLeft());
        theLine->mSlots[i] = theText;
    }
    EEUIShell::Op(theLine, EE_SCREENS_SKOP_CHAT_PARTS, (int64_t)theLine->mSlots);
    return theLine;
}

// A new file combo box (EEUIFileComboBox: the editor's map, sound and elevation lists): the path referenced, the
// list filled by the kind (shells_files.rs)
UIComboBox* EEUIShellNewFileCombo(FSPath* inPath, const UString& inPattern, unsigned long inId, GEFont* inFont,
                                  const GE2DPoint& inLocation, unsigned long inWidth, UIForm* inForm, unsigned long inIndex)
{
    inPath->Reference();
    return new EEUIShellCombo(EE_SCREENS_SK_FILE_COMBO, {(int64_t)inPath, (int64_t)(const char*)inPattern}, inId, 1, {},
                              inFont, GERectangle(inLocation, (float)inWidth, 20.0f), 400, inForm, inIndex, 0, 2);
}

int64_t EEUIShellRadio::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIRadioButton)
    EE_SHELL_BUTTON_BASE(UIRadioButton)
    case EE_SCREENS_SV_CHECK: UIRadioButton::Check(); return 0;
    case EE_SCREENS_SV_UNCHECK: UIRadioButton::UnCheck(); return 0;
    }
    return 0;
}

int64_t EEUIShellCheck::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UICheckBox)
    EE_SHELL_BUTTON_BASE(UICheckBox)
    }
    return 0;
}

int64_t EEUIShellCombo::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIComboBox)
    case EE_SCREENS_SV_CHANGE_SELECTION: UIComboBox::ChangeSelection(); return 0;
    }
    return 0;
}

int64_t EEUIShellList::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UILBString)
    case EE_SCREENS_SV_BUTTON_UP: UILBString::ButtonUp(); return 0;
    case EE_SCREENS_SV_CHANGE_SELECTION_INDEX:
        UILBString::ChangeSelectionIndex((unsigned long)inA, (inB & 1) != 0, (inB & 2) != 0);
        return 0;
    }
    return 0;
}

int64_t EEUIShellEdit::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIEditBox)
    case EE_SCREENS_SV_PROCESS_ENTER: UIEditBox::ProcessEnterKey(); return 0;
    }
    return 0;
}

int64_t EEUIShellSlider::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UISlider)
    case EE_SCREENS_SV_CHANGE_RANGE: UISlider::ChangeRange((unsigned long)inA); return 0;
    }
    return 0;
}

// a blue bar behind each line inLines names (EESEUITechListBox::DrawPolygons) -> the last draw's result
long EEUIShellList::DrawBars(const uint32_t* inLines, size_t inCount)
{
    GERhombus theRhombus;
    GERectangle theRectangle;
    long theResult = kErrorOK;
    theRhombus.SetColor(GEColor(0.0f, 0.0f, 1.0f));
    theRectangle.SetLeft(this->GetCopyTextRectangle().GetLeft());
    theRectangle.SetRight(this->GetCopyTextRectangle().GetRight());
    for (size_t i = 0; i < inCount; i++) {
        theRectangle.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(inLines[i] * this->GetLineHeight()));
        theRectangle.SetBottom(theRectangle.GetTop() + (float)this->GetLineHeight());
        theRhombus.SetRectangle(theRectangle);
        theResult = this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
    }
    return theResult;
}

// the edited player's tech tree (the player tab's player)
static EETechTree* EEShellTree()
{
    unsigned long theSelection;   // (the player combo box's selection + 1 indexes gWorld.mPlayers)
    gUIManager->GetScenarioEditorForm()->GetPlayerComboBox()->GetCurrentSelection(theSelection);
    return gWorld.mPlayers[theSelection + 1]->GetTechTree();
}

// Turns every placement ghost (inGhosts) or selected unit whose class can turn to the yaw inAngle (once
// EEUIFSEBRotation's RotateGhosts and RotateSelection)
static void EEShellRotate(UIControl* inControl, bool inGhosts, float inAngle)
{
    GETransformation theTransformation;
    if (inGhosts) {
        gWorld.mGhostUnitsLock->Enter();
        unsigned long theCount = gWorld.GetNumGhostUnits();
        for (unsigned long i = 0; i < theCount; i++) {
            EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(gWorld.mGhostUnits[i]);
            if (!(gUnitClassTable[theGhost->mUnitClass->mUnitType]->mAngularAcceleration > 0.0f))
                continue;
            theTransformation = theGhost->mTransformation;
            theTransformation.SetOrientationYPR(inAngle, 0.0f, 0.0f);
            theGhost->mTransformation = theTransformation;
        }
        gWorld.mGhostUnitsLock->Leave();
        return;
    }
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(inControl->GetForm());
    unsigned long theCount = theEditor->GetSelectedUnitCount();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theUnit = theEditor->GetSelectedUnit(i);
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit == NULL || !(theUnit->mUnitClass->mAngularAcceleration > 0.0f))
            continue;
        theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(inAngle, 0.0f, 0.0f);
        theComplexUnit->SetTransformation(theTransformation, true);
    }
}

// the control's text (inWhich 0: its own GEText, 1: its label's)
static GEText* EEShellText(UIControl* inControl, int64_t inWhich)
{
    if (UIRadioButton* r = dynamic_cast<UIRadioButton*>(inControl))
        return inWhich ? r->GetTextLabel()->GetGEText() : r->GetGEText();
    if (UICheckBox* k = dynamic_cast<UICheckBox*>(inControl))
        return inWhich ? k->GetTextLabel()->GetGEText() : k->GetGEText();
    if (UIButton* b = dynamic_cast<UIButton*>(inControl))
        return b->GetGEText();
    if (UIComboBox* c = dynamic_cast<UIComboBox*>(inControl))
        return c->GetGEText();
    if (UILBString* l = dynamic_cast<UILBString*>(inControl))
        return l->GetGEText();
    return static_cast<UIEditBox*>(inControl)->GetGEText();
}

static EEUIFScenarioEditor* EEShellTriggers()
{
    return gUIManager->GetScenarioEditorForm();
}

extern "C" int64_t ee_screenscpp_shell(void* inControl, uint32_t inOp, int64_t a, int64_t b, float* f)
{
    UIControl* c = static_cast<UIControl*>(inControl);
    UWideString theText;
    if (inOp >= EE_SCREENS_SH_FORM_BASE)   // a form shell's (its host object is the UIForm*)
        return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inControl))->BaseCall(inOp - EE_SCREENS_SH_FORM_BASE, a, b, f);
    if (inOp == EE_SCREENS_SH_FORM_SELF)
        return static_cast<EEUIShellForm*>(static_cast<UIForm*>(inControl))->SelfCall((uint32_t)a);
    if (inOp >= EE_SCREENS_SH_BASE)
        return EEUIShell::Of(c)->BaseCall(inOp - EE_SCREENS_SH_BASE, a, b, f);
    switch (inOp) {
    case EE_SCREENS_SH_BG_OPACITY: c->SetBgOpacity(f[0]); break;
    case EE_SCREENS_SH_BG_COLOR: c->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_BG_PLAYER_COLOR: c->SetBgColor(EEGetPlayerColor((unsigned long)a)); break;
    case EE_SCREENS_SH_SELECTED_COLOR:
        static_cast<UILBString*>(c)->GetSelectedBackground()->SetColor(GEColor(f[0], f[1], f[2]));
        break;
    case EE_SCREENS_SH_SELECTED_OPACITY: static_cast<UILBString*>(c)->GetSelectedBackground()->SetOpacity(f[0]); break;
    case EE_SCREENS_SH_SELECTION_INDEX: return (int64_t)(uint32_t)static_cast<EEUIShellCombo*>(c)->Selection();
    case EE_SCREENS_SH_SET_SELECTION: static_cast<UIComboBox*>(c)->SetCurrentSelection((unsigned long)a); break;
    case EE_SCREENS_SH_FILE_APPEND_LISTED: {
        std::set<UString> theFiles;
        reinterpret_cast<FSPath*>(a)->ListFiles(UString(reinterpret_cast<const char*>(b)), theFiles);
        for (std::set<UString>::const_iterator it = theFiles.begin(); it != theFiles.end(); ++it) {
            theText = *it;
            theText.InsertString("", theText.GetLength() - 4, true);
            static_cast<UIComboBox*>(c)->AppendLine(theText);
        }
        break;
    }
    case EE_SCREENS_SH_FILE_PATH_RELEASE: reinterpret_cast<FSPath*>(a)->Dereference(); break;
    case EE_SCREENS_SH_DELETE_ALL: static_cast<UIComboBox*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_APPEND_NUMBERED: {
        UWideString theLine;
        gResource->GetString((unsigned long)a, theText);
        for (unsigned long i = 1; i <= (unsigned long)b; i++) {
            sprintf(theLine, theText, i);
            static_cast<UIComboBox*>(c)->AppendLine(theLine);
        }
        break;
    }
    case EE_SCREENS_SH_SCROLL_CAPTURED: return static_cast<UILBString*>(c)->GetScrollBarCaptured();
    case EE_SCREENS_SH_RELEASE_MOUSE: c->ReleaseCapturedMouse(); break;
    case EE_SCREENS_SH_FIRE: c->FireTriggerEvent((unsigned long)a); break;
    case EE_SCREENS_SH_SLIDER_VALUE: return (int64_t)(uint32_t)static_cast<UISlider*>(c)->GetCurrentRangeValue();
    case EE_SCREENS_SH_SLIDER_MAX: return static_cast<UISlider*>(c)->GetMaxRange();
    case EE_SCREENS_SH_LABEL_NUMBER:
        static_cast<UITextLabel*>(c->GetForm()->GetControlById((unsigned long)a))->SetText((long)b);
        break;
    case EE_SCREENS_SH_BUTTON_TEXT_BYTES:
        static_cast<UIButton*>(c)->SetText(UWideString(reinterpret_cast<const char*>(a)), reinterpret_cast<GEFont*>(b));
        break;
    case EE_SCREENS_SH_CLEAR_RADIO_BITMAPS: {
        UIRadioButton* r = static_cast<UIRadioButton*>(c);
        r->SetDownCheckedBitmap(NULL);
        r->SetUpCheckedBitmap(NULL);
        r->SetDownUncheckedBitmap(NULL);
        r->SetUpUncheckedBitmap(NULL);
        break;
    }
    case EE_SCREENS_SH_LIST_COUNT:
        return static_cast<UIListBox<UWideString>*>(c->GetForm()->GetControlById((unsigned long)a))->GetLineCount();
    case EE_SCREENS_SH_LIST_SELECTION: {
        unsigned long s;
        UIListBox<UWideString>* l = static_cast<UIListBox<UWideString>*>(c->GetForm()->GetControlById((unsigned long)a));
        return l->GetCurrentSelection(s) != 0 ? -1 : (int64_t)s;
    }
    case EE_SCREENS_SH_EDITOR_PLAYER: {
        unsigned long s;
        gUIManager->GetScenarioEditorForm()->GetPlayerComboBox()->GetCurrentSelection(s);
        return (int64_t)s;
    }
    case EE_SCREENS_SH_TECH_TREE_MOVE:
        (new EESEARust(kSEATechTree, NULL, (int32_t)(uint32_t)a, (int32_t)(uint32_t)(a >> 32), (int32_t)(uint32_t)b,
                       (b >> 32) != 0))->Dereference();
        break;
    case EE_SCREENS_SH_PLAYER_PAGE:
        static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SELECT_PAGE, (uint32_t)a);
        break;
    case EE_SCREENS_SH_FILL_TECH_LISTS: gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_FILL_TECH_LISTS); break;
    case EE_SCREENS_SH_SET_PLAYER_COUNT:
        static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_SET_PLAYER_COUNT, (uint32_t)a);
        break;
    case EE_SCREENS_SH_LIST_OF_COMBO_SELECTION: {
        unsigned long s;
        const_cast<UILBString*>(static_cast<UIComboBox*>(c)->GetListBox())->GetCurrentSelection(s);
        return (int64_t)s;
    }
    case EE_SCREENS_SH_MAP_CREATE:   // a the map tab's group
        ee_screens_semp_call((EeScreensSeMap*)ee_screens_segr_tab(reinterpret_cast<EeScreensSeGroup*>(a)),
                             EE_SCREENS_SEMP_CREATE_MAP, 0);
        break;
    case EE_SCREENS_SH_NEW_BITMAP: {
        GEBitmap* theBitmap = new GEBitmap;
        theBitmap->SetTexture(reinterpret_cast<GETexture*>(a), b != 0 ? c->GetForm()->GetManager()->GetScreen() : gScreenIndex);
        return (int64_t)theBitmap;
    }
    case EE_SCREENS_SH_BITMAP_COORDS:
        reinterpret_cast<GEBitmap*>(a)->SetTextureCoordinates(GETextureCoordinate(f[0], f[1]), GETextureCoordinate(f[2], f[1]),
                                                               GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[2], f[3]));
        break;
    case EE_SCREENS_SH_BITMAP_SCALE: reinterpret_cast<GEBitmap*>(a)->SetXYScale(f[0], f[1]); break;
    case EE_SCREENS_SH_BITMAP_RELEASE: reinterpret_cast<GEBitmap*>(a)->Dereference(); break;
    case EE_SCREENS_SH_TEXTURE_SIZE:
        f[0] = (float)reinterpret_cast<GETexture*>(a)->GetWidth();
        f[1] = (float)reinterpret_cast<GETexture*>(a)->GetHeight();
        break;
    case EE_SCREENS_SH_SET_NORMAL: static_cast<UIButton*>(c)->SetNormalBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_SET_PRESSED: static_cast<UIButton*>(c)->SetPressedBitmap(reinterpret_cast<GEBitmap*>(a)); break;
    case EE_SCREENS_SH_BITMAP_OFFSET: static_cast<UIButton*>(c)->SetBitmapDrawOffset(GE2DPoint(f[0], f[1])); break;
    case EE_SCREENS_SH_SELECT_TAB: {
        EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(c->GetForm());
        if (a == 0 && gUserInterface)
            gUserInterface->PostSelectEditorTab((unsigned char)(b >> 32), (long)(int32_t)b);
        else
            theEditor->SelectTab((unsigned char)(b >> 32), (long)(int32_t)b);
        break;
    }
    // the tech lists' (EESEUITechListBox)
    case EE_SCREENS_SH_LIST_CURRENT: {
        unsigned long s;
        long r = static_cast<UILBString*>(c)->GetCurrentSelection(s);
        return a != 0 || r == kErrorOK ? (int64_t)s : -1;
    }
    case EE_SCREENS_SH_LIST_VIEW: return static_cast<EEUIShellList*>(c)->View();
    case EE_SCREENS_SH_LIST_DELETE_ALL: static_cast<UILBString*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_LIST_APPEND_BYTES:
        static_cast<UILBString*>(c)->AppendLine(UWideString(reinterpret_cast<const char*>(a)));
        break;
    case EE_SCREENS_SH_LIST_LINE_COUNT: return static_cast<UILBString*>(c)->GetLineCount();
    case EE_SCREENS_SH_LIST_START_LINE: static_cast<UILBString*>(c)->SetStartingDisplayLine((unsigned long)a); break;
    case EE_SCREENS_SH_LIST_SELECT_SCROLL: static_cast<UILBString*>(c)->ChangeSelectionIndex((unsigned long)a, false, true); break;
    case EE_SCREENS_SH_FORM_CHECKED: {
        UIControl* theControl = c->GetForm()->GetControlById((unsigned long)a);
        if (UIRadioButton* r = dynamic_cast<UIRadioButton*>(theControl))
            return r->GetIsChecked();
        return static_cast<UICheckBox*>(theControl)->GetIsChecked();
    }
    case EE_SCREENS_SH_FORM_CONTROL: return (int64_t)c->GetForm()->GetControlById((unsigned long)a);
    case EE_SCREENS_SH_TECH_NAME_ID: {
        EETechTreeNode* theNode = gTechTree.Get((long)a);
        return b != 0 ? theNode->mData->mNameID : gUnitClassTable[theNode->mData->mUnitClass]->mNameStringID;
    }
    case EE_SCREENS_SH_TECH_EPOCH: return EETechTreeEpochOf((long)a);
    case EE_SCREENS_SH_TECH_DATA_ID: return gTechTree.Get((long)a)->mData->mID;
    case EE_SCREENS_SH_TECH_DEPENDS: {
        EETechTree* theTree = EEShellTree();
        long theOn = (long)(int32_t)(a >> 32);
        if (b != 0)
            theOn = theTree->Get(theOn)->mData->mID;
        return theTree->Get((long)(int32_t)(uint32_t)a)->DependsOn(theOn);
    }
    case EE_SCREENS_SH_TECH_OBSOLETE: {
        EETechTree* theTree = EEShellTree();
        theTree->Get((long)a)->SetScenEditorObsoleted(b != 0, true);
        return theTree->Get((long)a)->mEpoch <= theTree->mCurrentEpoch;
    }
    case EE_SCREENS_SH_TECH_OBSOLETED: return EEShellTree()->Get((long)a)->mScenEditorObsoleted;
    case EE_SCREENS_SH_TECH_MARK_PLAYER:
        gUIManager->GetScenarioEditorForm()->PlayerCall(EE_SCREENS_SEPL_MARK_CHANGED, EEShellTree()->GetPlayer()->mPlayerID, false);
        break;
    case EE_SCREENS_SH_TECH_BARS:
        return static_cast<EEUIShellList*>(c)->DrawBars(reinterpret_cast<const uint32_t*>(a), (size_t)b);
    // the unit tab's (EEUIFSELBUnitList, EEUIFSELBCargoList, EEUIFSEBRotation)
    case EE_SCREENS_SH_UNIT_CLASS: {
        EEUnitClass* theClass = gUnitClassTable[(long)a];
        return b == 0 ? theClass->mNameStringID : b == 1 ? theClass->mEpoch : theClass->mFamily;
    }
    case EE_SCREENS_SH_LIST_INSERT_BYTES: {
        UILBNoSort<UWideString> theNoSort;
        static_cast<UILBString*>(c)->InsertLine(UWideString(reinterpret_cast<const char*>(a)), (unsigned long)b, theNoSort);
        break;
    }
    case EE_SCREENS_SH_LIST_LINE: {
        const char* theLine = static_cast<UILBString*>(c)->GetData((unsigned long)a);
        strncpy(reinterpret_cast<char*>(b), theLine, 4499);
        reinterpret_cast<char*>(b)[4499] = 0;
        break;
    }
    case EE_SCREENS_SH_UNIT_SELECT_TYPE:
        static_cast<EEUIFScenarioEditor*>(c->GetForm())->UnitCall(EE_SCREENS_SEUN_SELECT);
        break;
    case EE_SCREENS_SH_ENTER_KEY: return EEUIShellList::EnterKey();
    case EE_SCREENS_SH_UNIT_NAME_AND_ID: {
        EEUnit* theUnit = reinterpret_cast<EEUnit*>(a);
        return (int64_t)theUnit->mUnitClass->mNameStringID << 32 | (uint32_t)theUnit->GetUnitID();
    }
    case EE_SCREENS_SH_DELETE_UNIT: (new EESEARust(kSEADeleteUnit, NULL, (int32_t)a))->Dereference(); break;
    case EE_SCREENS_SH_BITMAP_ROTATION: reinterpret_cast<GEBitmap*>(a)->SetRotation(f[0]); break;
    case EE_SCREENS_SH_ROTATE:
        if (a == 2)
            return gWorld.GetNumGhostUnits() > 0;
        EEShellRotate(c, a != 0, f[0]);
        break;
    case EE_SCREENS_SH_KEY: {
        ISEntity* theKey;
        long theResult = gISKeyboard->GetEntity(a == 0 ? kKeyComma : kKeyPeriod, theKey);
        return (int64_t)theResult << 8 | (theKey->JustPressed() ? 1 : theKey->JustUnpressed() ? 2 : 0);
    }
    case EE_SCREENS_SH_SELF: return EEUIShell::Of(c)->SelfCall((uint32_t)a);
    case EE_SCREENS_SH_KEYBOARD: EEUIShell::Of(c)->Keyboard(a != 0); break;
    case EE_SCREENS_SH_ALWAYS_DIRTY: c->SetAlwaysDirty(a != 0); break;
    case EE_SCREENS_SH_EDITOR_TEXTURE: return (int64_t) static_cast<EEUIFScenarioEditor*>(c->GetForm())->GetButtonTexture();
    // the message box's buttons' (shells_dialog.rs)
    case EE_SCREENS_SH_FOCUSED_TEXT_COLOR: static_cast<UIButton*>(c)->SetFocusedTextColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_FILL:
        return ee_ge_draw2d_fill(c->GetRasterizer(), c->GetScreenIndex(),
                                 reinterpret_cast<const EeGeRectangle*>(&c->GetCopiedDrawRectangle()), f, f[3]);
    case EE_SCREENS_SH_CLOSE_FORM: gUserInterface->CloseForm(reinterpret_cast<UIForm*>(a)); break;
    // the saved-group buttons' (shells_saved.rs)
    case EE_SCREENS_SH_SG_NEW_PORTRAIT: return (int64_t) new GEBitmap;
    case EE_SCREENS_SH_SG_PLAY: gSoundManager->Play((unsigned long)a, 1.0f, 0.0f, 1.0f, false, NULL); break;
    case EE_SCREENS_SH_SG_PLAYER: return (int64_t)gWorld.GetCurrentPlayer();
    case EE_SCREENS_SH_SG_SHIFT: return UIControl::ShiftKeyPressed();
    case EE_SCREENS_SH_SG_INDEX: return (int64_t)c->GetIndex();
    case EE_SCREENS_SH_SG_SET_TEXTURE: reinterpret_cast<GEBitmap*>(a)->SetTexture(reinterpret_cast<GETexture*>(b), c->GetScreenIndex()); break;
    // the icon bar's (shells_icon_bar.rs): the slots let go as its destructor did
    // the chat line's (shells_chat.rs): its texts deleted, as its destructor did
    // the paint areas' (shells_paint.rs)
    case EE_SCREENS_SH_MAP_HALF: f[0] = gWorld.GetMap()->GetF1955F4(); break;
    case EE_SCREENS_SH_SCREEN_LINE: {
        GE3DLine theLine;
        gWorld.mMap->GetScreenLine(GE2DPoint(f[0], f[1]), theLine);
        float* o = reinterpret_cast<float*>(a);
        o[0] = theLine.GetStart().GetX(); o[1] = theLine.GetStart().GetY(); o[2] = theLine.GetStart().GetZ();
        o[3] = theLine.GetDirection().GetX(); o[4] = theLine.GetDirection().GetY(); o[5] = theLine.GetDirection().GetZ();
        break;
    }
    case EE_SCREENS_SH_WINDOW_SIZE: return (int64_t)gGameWindow->GetWidth() | (int64_t)gGameWindow->GetHeight() << 32;
    case EE_SCREENS_SH_MOUSE_BOUNDS: gMouse->SetBounds(GERectangle(f[0], f[1], f[2], f[3])); break;
    case EE_SCREENS_SH_SCROLL_EDGES: reinterpret_cast<EEAScroll*>(a)->mEdges = GERectangle(f[0], f[1], f[2], f[3]); break;
    case EE_SCREENS_SH_CAPTURE_MOUSE: static_cast<EEUIShellControl*>(c)->Capture(); break;
    case EE_SCREENS_SH_MOUSE_BUTTONS:
        return (c->GetRightMouseButtonDown() ? 1 : 0) | (c->GetLeftMouseButtonDown() ? 2 : 0) | (c->GetHasMouseCapture() ? 4 : 0);
    case EE_SCREENS_SH_PAINT_NEW: return (int64_t) static_cast<EESEAPaint*>(new EESEAPaintRust((uint32_t)a, (char)b));
    case EE_SCREENS_SH_PAINT_AREA: {
        const int32_t* r = reinterpret_cast<const int32_t*>(b);
        return reinterpret_cast<EESEAPaint*>(a)->SetArea(r[0], r[1], r[2], r[3]);
    }
    case EE_SCREENS_SH_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EESEAPaint*>(a)); break;
    case EE_SCREENS_SH_ACTION_RELEASE: reinterpret_cast<EESEAPaint*>(a)->Dereference(); break;
    // the scenario editor lane's (0xa0..): the diplomacy buttons' state, an action's post and release
    case EE_SCREENS_SH_SE_DIPLOMACY: gEEUIShellDiplomacyShow(static_cast<UIButton*>(c), (unsigned long)a, b != 0); break;
    case EE_SCREENS_SH_SE_ACTION_POST: gUserInterface->PostAction(reinterpret_cast<EEAction*>(a)); break;
    case EE_SCREENS_SH_SE_ACTION_RELEASE: reinterpret_cast<EEAction*>(a)->Dereference(); break;
    case EE_SCREENS_SH_BRUSH_HIGHLIGHT: gUIManager->GetScenarioEditorForm()->GetBrushGroup()->SetHighlight(f[0], f[1], a != 0); break;
    case EE_SCREENS_SH_BRUSH_GROUP_SIZE: return reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize;
    case EE_SCREENS_SH_SPACE: {
        ISEntity* theKey;
        return gISKeyboard->GetEntity(kKeySpace, theKey) == 0 && theKey->GetValue() > 0;
    }
    case EE_SCREENS_SH_TERRAIN_RUST: return (int64_t)ee_screens_segr_tab(reinterpret_cast<EeScreensSeGroup*>(a));
    case EE_SCREENS_SH_SLIDER_OF: {
        UISlider* theSlider = static_cast<UISlider*>(c->GetForm()->GetControlById((unsigned long)a));
        return (int64_t)theSlider->GetMaxRange() << 32 | (uint32_t)theSlider->GetCurrentRangeValue();
    }
    // the game's
    case EE_SCREENS_SH_TRIGGER_REFRESH: EEShellTriggers()->TriggerSubSlot((long)a, EE_SCREENS_SETS_SCREEN_REFRESH); break;
    case EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN: EEShellTriggers()->TriggerCall(EE_SCREENS_SETS_REFRESH_SCREEN); break;
    case EE_SCREENS_SH_TRIGGER_JUMP: EESETriggerJump((unsigned long)a, (unsigned long)b); break;
    case EE_SCREENS_SH_OBJECT_UPDATE_SELECTION:
        EEShellTriggers()->TriggerSubOp(kSETriggerScreenObject, EE_SCREENS_SEOB_UPDATE_SELECTION);
        break;
    case EE_SCREENS_SH_OBJECT_STORE: EEShellTriggers()->TriggerSubSlot(kSETriggerScreenObject, EE_SCREENS_SETS_SCREEN_STORE); break;
    case EE_SCREENS_SH_COPY_FIRST_TO_ALL:
        static_cast<EEUIFScenarioEditor*>(c->GetForm())->PlayerCall(EE_SCREENS_SEPL_COPY_FIRST_TO_ALL);
        break;
    case EE_SCREENS_SH_BRUSH_SIZE: reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize = (long)b; break;
    }
    return 0;
}

// ---- the shims of the moved cases (UI lane round 7: ee_game screens shell_calls.rs, game_cpp.rs), each one C++
// statement on the shell's control or the world
#include "ee/EEScreensShims.h"
static UIControl* C(uintptr_t inControl) { return reinterpret_cast<UIControl*>(inControl); }
static EETile* sShimTile;          // the paint areas' pick: its tile and point (locals of the original case)
static GE3DPoint sShimPoint;
static EEMapTile* sShimMapTile;    // the map tile read's
// shims6
extern "C" void ee_screenscpp_shell_text_fg(uintptr_t inControl, uint32_t inWhich, float inRed, float inGreen, float inBlue)
{ EEShellText(C(inControl), inWhich)->SetFgColor(GEColor(inRed, inGreen, inBlue)); }
extern "C" void ee_screenscpp_shell_text_transparent(uintptr_t inControl, uint32_t inWhich)
{ EEShellText(C(inControl), inWhich)->SetIsTransparent(true); }
extern "C" void ee_screenscpp_shell_button_set_text(uintptr_t inControl, EeScreensShimText inText, uintptr_t inFont)
{ static_cast<UIButton*>(C(inControl))->SetText(EEShimText(inText), reinterpret_cast<GEFont*>(inFont)); }
extern "C" void ee_screenscpp_shell_combo_append(uintptr_t inControl, EeScreensShimText inText)
{ static_cast<UIComboBox*>(C(inControl))->AppendLine(EEShimText(inText)); }
extern "C" uint32_t ee_screenscpp_shell_combo_line_count(uintptr_t inControl)
{ return (uint32_t)static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetLineCount(); }
extern "C" bool ee_screenscpp_shell_combo_line_is(uintptr_t inControl, uint32_t inLine, uintptr_t inText)
{ return static_cast<UIComboBox*>(C(inControl))->GetListBox()->GetData(inLine) == *reinterpret_cast<const UWideString*>(inText); }
extern "C" void ee_screenscpp_sound_unload_media(uintptr_t inSound)
{ reinterpret_cast<EESoundObject*>(inSound)->UnloadMedia(); }
extern "C" uint32_t ee_screenscpp_sound_sample_id(uintptr_t inSound)
{ return (uint32_t)reinterpret_cast<EESoundObject*>(inSound)->GetSampleID(); }
extern "C" uintptr_t ee_screenscpp_selected_first_unit()
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mSelectedGroup->GetFirstUnit(); }
extern "C" uintptr_t ee_screenscpp_player_group(uint32_t inGroup)
{ return (uintptr_t)gWorld.GetCurrentPlayer()->mGroups[inGroup]; }
extern "C" uint32_t ee_screenscpp_group_count(uintptr_t inGroup)
{ return (uint32_t)reinterpret_cast<EEGroup*>(inGroup)->GetNumUnits(); }
extern "C" uintptr_t ee_screenscpp_group_first_unit(uintptr_t inGroup)
{ return (uintptr_t)reinterpret_cast<EEGroup*>(inGroup)->GetFirstUnit(); }
extern "C" void ee_screenscpp_send_saved_group_select(uint32_t inGroup, bool inAdd)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupSelect, {(unsigned long)inGroup, inAdd}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_add_selection(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupAddSelection, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_create(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupCreate, {(unsigned long)inGroup}), kSlotNone); }
extern "C" void ee_screenscpp_send_saved_group_clear(uint32_t inGroup)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSavedGroupClear, {(unsigned long)inGroup}), kSlotNone); }
extern "C" uintptr_t ee_screenscpp_unit_stats_button(uintptr_t inUnit)
{ return (uintptr_t)reinterpret_cast<EEComplexUnit*>(inUnit)->mStats->mButton; }
extern "C" void ee_screenscpp_button_set_face(uintptr_t inButton, uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<EEButtonObject*>(inButton)->SetFaceBitmap(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetRectangle(), gScreenIndex); }
extern "C" float ee_screenscpp_button_cell_u(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellU(); }
extern "C" float ee_screenscpp_button_cell_v(uintptr_t inButton)
{ return reinterpret_cast<EEButtonObject*>(inButton)->GetCellV(); }
extern "C" uintptr_t ee_screenscpp_button_texture(uintptr_t inButton)
{ return (uintptr_t)reinterpret_cast<EEButtonObject*>(inButton)->GetTexture(); }
extern "C" void ee_screenscpp_texture_reference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Reference(); }
extern "C" void ee_screenscpp_texture_dereference(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->Dereference(); }
extern "C" void ee_screenscpp_bitmap_at_button(uintptr_t inBitmap, uintptr_t inControl)
{ reinterpret_cast<GEBitmap*>(inBitmap)->SetLocation(static_cast<UIButton*>(C(inControl))->GetBitmapLocation()); }
extern "C" void ee_screenscpp_shell_draw_bitmap(uintptr_t inControl, uintptr_t inBitmap)
{ C(inControl)->GetRasterizer()->Draw(reinterpret_cast<GEBitmap*>(inBitmap), C(inControl)->GetScreenIndex()); }
extern "C" uintptr_t ee_screenscpp_shell_slot(uintptr_t inControl, uint32_t inSlot)
{ return (uintptr_t)static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot]; }
extern "C" void ee_screenscpp_shell_clear_slot(uintptr_t inControl, uint32_t inSlot)
{ static_cast<EEUIShellControl*>(C(inControl))->mSlots[inSlot] = NULL; }
extern "C" void ee_screenscpp_text_delete(uintptr_t inText) { delete reinterpret_cast<GEText*>(inText); }
extern "C" float ee_screenscpp_control_width(uintptr_t inControl) { return C(inControl)->GetRectangle().GetWidth(); }
extern "C" float ee_screenscpp_control_height(uintptr_t inControl) { return C(inControl)->GetRectangle().GetHeight(); }
extern "C" void ee_screenscpp_shim_tile_clear() { sShimTile = NULL; }
extern "C" void ee_screenscpp_shim_point_clear() { sShimPoint = GE3DPoint(); }
extern "C" bool ee_screenscpp_pick_tile_xy(float inX, float inY, long* inTileX, long* inTileY)
{ return EEPointUnitOrTileXY(GE2DPoint(inX, inY), sShimTile, *inTileX, *inTileY, false); }
extern "C" bool ee_screenscpp_shim_tile_exists() { return sShimTile != NULL; }
extern "C" float ee_screenscpp_shim_tile_height() { return sShimTile->GetHeight(); }
extern "C" void ee_screenscpp_pick_map_location(float inX, float inY)
{ sShimTile = gWorld.GetMap()->GetLocationAt(GE2DPoint(inX, inY), sShimPoint, false); }
extern "C" void ee_screenscpp_pick_unit_or_tile(float inX, float inY)
{ EEPointUnitOrTile(GE2DPoint(inX, inY), sShimTile, sShimPoint, false); }
extern "C" float ee_screenscpp_shim_point_x() { return sShimPoint.GetX(); }
extern "C" float ee_screenscpp_shim_point_y() { return sShimPoint.GetY(); }
extern "C" float ee_screenscpp_shim_point_z() { return sShimPoint.GetZ(); }
extern "C" void ee_screenscpp_map_tile_load(uint32_t inX, uint32_t inY)
{ sShimMapTile = gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY); }
extern "C" bool ee_screenscpp_shim_map_tile_exists() { return sShimMapTile != NULL; }
extern "C" float ee_screenscpp_shim_map_tile_height() { return sShimMapTile->GetHeight(); }
extern "C" float ee_screenscpp_mouse_x() { return gMouse->mPosition.GetX(); }
extern "C" float ee_screenscpp_mouse_y() { return gMouse->mPosition.GetY(); }
extern "C" float ee_screenscpp_shell_mouse_x(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetX(); }
extern "C" float ee_screenscpp_shell_mouse_y(uintptr_t inControl)
{ return static_cast<EEUIShellControl*>(C(inControl))->MouseLocation().GetY(); }
extern "C" float ee_screenscpp_view_left() { return gViewport.GetViewRectangle().GetLeft(); }
extern "C" float ee_screenscpp_view_top() { return gViewport.GetViewRectangle().GetTop(); }
extern "C" float ee_screenscpp_view_right() { return gViewport.GetViewRectangle().GetRight(); }
extern "C" float ee_screenscpp_view_bottom() { return gViewport.GetViewRectangle().GetBottom(); }
// end shims6
