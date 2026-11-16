// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.h (RECONSTRUCTION; file name inferred).
// The scenario editor's main form: RTTI EEUIFScenarioEditor > UIForm, vtable 0x445084, sizeof 0x2b8 (`new` at
// 0x273e8e, EEUIManager::CreateScenarioEditor), ctor 0x19d615. The game reaches it as
// gUIManager->mScenarioEditorForm (+0x130), and the editor's UI events get it as their ProcessEvent's inForm
// (`(EEUIFScenarioEditor*)inForm`). EEUIFScenarioEditor.cpp is about 0x19cc39..0x1a0315 (its events' handlers, the
// ctor, the members, then the inline event classes).
// Layout: the ctor 0x19d615 (member ctors in order: EECivilization +0x1a0, three floats +0x1e0, +0x1ec, four floats
// +0x1f0, the two vectors +0x208/+0x218) and a field census over 0x16f892..0x1cba2f.
// Method names are provisional. Whole in Rust (ee_screens editor_form.rs, R6 editor round 7): the form's controls,
// events, state and selection are the Rust object's slots (EE_SCREENS_SEFM_*), read by the accessors below; the class
// keeps its civilization (the builder's) and forwards. Requires lle/UIForm.h, ee/EECivilization.h.
#ifndef EE_EEUIFSCENARIOEDITOR_H
#define EE_EEUIFSCENARIOEDITOR_H
#include "ee/EEScenarioEditorIDs.h"
#include "ee/EEUnitHandle.h"

class EEUIMiniMap;
class UIFullBackground;
class UIButton;
class EEUIFSEUndo;
class EEUIFSERedo;
class EEAScroll;
class EESELoadThread;
class EESEPlayerScreen;
class GEBitmap;
class EESEUnit;
class EESETriggerScreen;
class EEUIFSEBrushGroup;
class EEUIFSEMainButton;
class EEUIFSEMainGroup;
class EEUnit;
class GETexture;
class TMPoll;
class TSThread;
class UIEvent;
struct EeScreensSeForm;
class UICheckBox;
class UIComboBox;
class UITextLabel;

// the form's object (rust/ee_screens.h declares these too; EEUIFScenarioEditor.cpp asserts the slots)
extern "C" size_t ee_screens_sefm_get(const EeScreensSeForm* form, uint32_t slot);
extern "C" void ee_screens_sefm_set(const EeScreensSeForm* form, uint32_t slot, size_t value);
extern "C" uint32_t ee_screens_sefm_selected(const EeScreensSeForm* form, ptrdiff_t index);
extern "C" int32_t ee_screens_sefm_call(const EeScreensSeForm* form, uint32_t what, int32_t a, void* p, const float* r);

class EEUIFScenarioEditor : public UIForm {
public:
    // the slots the accessors read (editor_form.rs EE_SCREENS_SEFM_*)
    enum {
        kSlotButtonTexture = 0, kSlotEdgeScroll = 1, kSlotPanelBackground = 3, kSlotFullBackground = 4,
        kSlotPlayerComboBox = 6, kSlotBrushGroup = 9, kSlotGroup = 12, kSlotTabButton = 19,
        kSlotQuickSaveButton = 31, kSlotLoadThread = 43, kSlotUIHidden = 52, kSlotWasUIHidden = 53,
        kSlotExitAfterSave = 54
    };
    EEUIFScenarioEditor(UIManager* inManager);   // 0x19d615 (EEUIManager::CreateScenarioEditor news 0x2b8 bytes)
    virtual ~EEUIFScenarioEditor();
    virtual long UpdateKeyboard();          // slot 0x2c 0x19fdc8
    virtual void Open();                    // slot 0x48 0x19f794
    virtual long DrawBackground(const GERectangle& inRectangle);   // slot 0x18 0x19f501
    virtual void Close();                   // slot 0x38 0x19e68a
    virtual long CopyData();                // slot 0x3c 0x19e72c
    virtual long UpdateMouse();             // slot 0x30 0x1a0083
protected:
    virtual long DrawPolygons();            // slot 0x40 0x19f5b3 (UIForm's, protected)
public:
    void PostLoadEvent(UReferenceCounter* inObject);
    void PostShowLoadedObject(UReferenceCounter* inObject);
    void LeaveTab();
    void EnterFullView();
    void LeaveFullView();
    // 0x19f6e9: leaves the editor through the load thread (EESEATestScenario passes true, EESEAExitScenario false)
    void ExitEditor(bool inTestScenario);
    void RefreshCurrentGroup();             // 0x19fc97: the current group's Store (EESEAQuickSaveScenario)
    void ResumeCurrentGroup();              // 0x19f757: resumed, the current group's Load (after a quick save/load)
    void ClearSelection();
    void DeselectFirstUnit();               // 0x19fab9 (EESEADeleteUnit)
    void SelectUnit(EEUnit* inUnit);
    void DeselectUnit(EEUnit* inUnit);
    void SelectTab(unsigned char inFlag, long inTab);   // 0x19f615 (EEUIFSEMainButton::Check)
    void FnE123D();                         // (ICF-folded into 0xe123d: an empty body)
    // 0x17d597 (the brush group's rectangle; EESEAPaintArea; defined in EESETSArea.cpp)
    void SetPaintArea(const GERectangle& inRectangle);
    void SetLoadText(const UWideString& inText);   // 0x19fd09 (EESEAChangeLoadText)
    // 0x19fcb4: the game view rectangle, the current group disabled while the UI is hidden (EESETriggerScreen)
    void SetGameView(const GERectangle& inRectangle, bool inIsUIHidden);

    unsigned long GetSelectedUnitCount() const { return ee_screens_sefm_selected(this->mRust, -1); }
    EEUnit* GetSelectedUnit(unsigned long inIndex) const
    {
        EEUnit* theUnit;
        EEUnitHandleResolve(ee_screens_sefm_selected(this->mRust, (ptrdiff_t)inIndex), theUnit);
        return theUnit;
    }
    EEUnit* GetFirstSelectedUnit() const { return this->GetSelectedUnitCount() == 0 ? NULL : this->GetSelectedUnit(0); }
    size_t GetSlot(uint32_t inSlot) const { return ee_screens_sefm_get(this->mRust, inSlot); }
    EEUIFSEMainGroup* GetTabGroup(long inGroup) const { return (EEUIFSEMainGroup*)this->GetSlot(kSlotGroup + inGroup); }
    EESETriggerScreen* GetTriggerScreen() const { return (EESETriggerScreen*)this->GetTabGroup(kEditorTabTriggers); }
    EESEUnit* GetUnitScreen() const { return (EESEUnit*)this->GetTabGroup(kEditorTabUnits); }
    EESEPlayerScreen* GetPlayerScreen() const { return (EESEPlayerScreen*)this->GetTabGroup(kEditorTabPlayers); }
    EEUIFSEBrushGroup* GetBrushGroup() const { return (EEUIFSEBrushGroup*)this->GetSlot(kSlotBrushGroup); }
    EESELoadThread* GetLoadThread() const { return (EESELoadThread*)this->GetSlot(kSlotLoadThread); }
    GETexture* GetButtonTexture() const { return (GETexture*)this->GetSlot(kSlotButtonTexture); }
    EEAScroll* GetEdgeScroll() const { return (EEAScroll*)this->GetSlot(kSlotEdgeScroll); }
    GEBitmap* GetPanelBackground() const { return (GEBitmap*)this->GetSlot(kSlotPanelBackground); }
    UIComboBox* GetPlayerComboBox() const { return (UIComboBox*)this->GetSlot(kSlotPlayerComboBox); }
    UIButton* GetQuickSaveButton() const { return (UIButton*)this->GetSlot(kSlotQuickSaveButton); }
    EEUIFSEMainButton* GetTabButton(long inTab) const { return (EEUIFSEMainButton*)this->GetSlot(kSlotTabButton + inTab); }
    bool GetIsUIHidden() const { return this->GetSlot(kSlotUIHidden) != 0; }
    bool GetWasUIHidden() const { return this->GetSlot(kSlotWasUIHidden) != 0; }
    void SetExitAfterSave(bool inExit) { ee_screens_sefm_set(this->mRust, kSlotExitAfterSave, inExit ? 1 : 0); }

    // the host of the Rust object's calls (ee_screenscpp_sefm; a member for UIForm's protected calls)
    static int32_t Host(uint32_t inOp, void* const* p, int32_t* i, float* f);

    EECivilization mC1A0;                   // +0x1a0 (the civilization builder's; EESEEvents.cpp)
    EeScreensSeForm* mRust;                 // the form (editor_form.rs)
};

#endif
