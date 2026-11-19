// C:\EEX\Empire Earth\Scenario Editor\EESETriggerScreen.h (RECONSTRUCTION; file name inferred). The editor's
// trigger tab (RTTI EESETriggerScreen > EEUIFSEMainGroup; vtable 0x44379c, sizeof 0x74, ctor 0x17b0b7, ??_G
// 0x17bf7b, dtor 0x17bf97; EESETriggerScreen.cpp 0x17b0b7..0x17ccf3): the editor's group 4
// (EEUIFScenarioEditor::GetTriggerScreen). Its ctor made the five sub-screens (the factory 0x17c1e5;
// now the tab's Rust record, ee_game editor_tsubs.rs). Only what matched code uses; provisional names. Requires
// ee/EEUIFSEMainGroup.h, lle/UIEditBox.h, rust/game/screens.h.
#ifndef EE_EESETRIGGERSCREEN_H
#define EE_EESETRIGGERSCREEN_H

class UIButton;
class UICheckBox;
class UILBString;
class UIRadioButton;
class UITextLabel;

// (The trigger event of the editor's control 0x12, EESEEHideUI, is an EESEERust (ee/EESEARust.h): Enter sets its
// two views through ee_screens_see_hide_ui_views, the original's inline SetRectangles 0x17ccba.)

extern bool gB4C3220;                       // 0x4c3220 (= true; set while an entry is written back; provisional name)

// Whole in Rust (ee_screens editor_tscreen.rs: its controls, the entry list and its buttons, the sub-tabs, the
// store and load of the open entry, entering and leaving, the five sub-screens); the class forwards.
struct EeScreensSeTriggerTab;

// opens the tab of a combo box's right-clicked entry (EESETSTrigger.cpp; inKind the combo's screen)
void EESETriggerJump(unsigned long inKind, unsigned long inSelection);

class EESETriggerScreen : public EEUIFSEMainGroup {
public:
    EESETriggerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont);
    virtual ~EESETriggerScreen();           // +0x00 0x17bf97
    virtual void Update();                  // +0x04 0x17c1d9
    virtual void Leave();                   // +0x08 0x17c130 (leaves the tab: the full game view)
    virtual void Draw(const GERectangle& inRectangle);          // +0x0c 0x17c399 (the panel background)
    virtual void Disable();                 // +0x10 0x17c5b3
    virtual void Load();                    // +0x14 0x17c78b
    virtual void Enter();                   // +0x18 0x17c7d0 (enters the tab)
    virtual void Store();                   // +0x1c 0x17ca61
    virtual void Enable();                  // +0x20 0x17ca30
    virtual void MouseMove(const GE2DPoint& inLocation);       // +0x24 0x17ca79 (the screen's Virtual40)
    virtual void UpdateKeys();              // +0x28 0x17ca66
    // new slots (+0x00..+0x28 as EEUIFSEMainGroup's); names by offset
    virtual void HideText();                // +0x2c 0x17c5d9 (ICF-shared with EESETSEffect's +0x28)
    virtual void HideAlphaSort();           // +0x30 0x17c5f2
    virtual void ShowText();                // +0x34 0x182d69 (ICF-shared; EESETSCondition::Load calls it first)
    virtual void ShowAlphaSort();           // +0x38 0x17ca56
    virtual void RefreshScreen();           // +0x3c 0x17ca92
    // the event handlers (EESEEvents.cpp; provisional names after their events)
    void AlphaSort();
    void SelectEntry();                     // 0x17c0ad (also after EESEEShowDebugIDs)
    void DecPriority();
    void DeleteEntry();
    void DuplicateEntry();
    void ShowTab(unsigned long inControlID);
    void IncPriority();
    void NewEntry();
    void SelectName();

    // the sub-screen inScreen's Rust object (kSETriggerScreen*: an EeScreensSeTrigger*, EeScreensSeConditionScreen*,
    // EeScreensSeEffectScreen*, EeScreensSeObject* or EeScreensSeArea*), one of its base slots (EE_SCREENS_SETS_SCREEN_*)
    // and its own calls with its selection where its class passed it (ee_screens_sets_sub_op)
    void* GetScreen(long inScreen) const { return ee_screens_sets_sub(this->mRust, (uint32_t)inScreen, NULL, NULL); }
    void ScreenSlot(long inScreen, int32_t inSlot) const
    {
        ee_screens_sets_sub_call(this->mRust, (uint32_t)inScreen, inSlot, 0.0f, 0.0f);
    }
    uint32_t ScreenOp(long inScreen, uint32_t inWhat, uint32_t inA = 0, uint32_t inB = 0) const
    {
        return ee_screens_sets_sub_op(this->mRust, (uint32_t)inScreen, inWhat, inA, inB);
    }
    void FillList();                        // 0x17c99f: refills the list from the current screen's names

    // inline (COMDAT 0x17eaab, called out of line by EESETSCondition::Store): the name edit box's first line
    const char* GetName() const { return this->GetNameEditBox()->GetText(0); }
    // the name and description boxes (EESETSCondition::Store/Virtual2C); read from outside (the scenario combo
    // boxes' jump to an entry, the editor gate): the entry list, a sub-tab, the open sub-screen
    UIEditBox* GetNameEditBox() const;
    UIEditBox* GetTextEditBox() const;
    UILBString* GetList() const;
    UIRadioButton* GetTab(long inScreen) const;
    long GetCurrentScreen() const;

    // the tab (ee_screens editor_tscreen.rs: its controls, the open sub-screen and its control group, the five
    // sub-screens; the panel's rectangle +0x18 is the host's constant)
    EeScreensSeTriggerTab* mRust;
    // sizeof 0x74 (its `new`)
};

#endif
