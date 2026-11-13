// C:\EEX\Empire Earth\Scenario Editor\EESETriggerScreen.h (RECONSTRUCTION; file name inferred). The editor's
// trigger tab (RTTI EESETriggerScreen > EEUIFSEMainGroup; vtable 0x44379c, sizeof 0x74, ctor 0x17b0b7, ??_G
// 0x17bf7b, dtor 0x17bf97; EESETriggerScreen.cpp 0x17b0b7..0x17ccf3): the editor's group 4
// (EEUIFScenarioEditor::GetTriggerScreen). Its ctor fills mScreens[0..4] from the factory 0x17c1e5 (see
// ee/EESETSBaseObject.h). Only what matched code uses; provisional names. Requires ee/EEUIFSEMainGroup.h,
// ee/EESETSBaseObject.h, lle/UIEditBox.h.
#ifndef EE_EESETRIGGERSCREEN_H
#define EE_EESETRIGGERSCREEN_H

class UIButton;
class UICheckBox;
class UILBString;
class UIRadioButton;
class UITextLabel;

// The trigger screen's "show debug ids" check box (RTTI EESETSShowIDsCheckBox > UICheckBox; vtable 0x4434f8, sizeof
// 0x10c; ctor inline, ??_G 0x17bf58 after the screen's ctor). Name is provisional.
class EESETSShowIDsCheckBox : public UICheckBox {
public:
    EESETSShowIDsCheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GE2DPoint& inLocation,
                          unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
        : UICheckBox(inId, inText, inFont, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 0,
                     false)
    {
    }
    virtual ~EESETSShowIDsCheckBox() {}
};

// (The trigger event of the editor's control 0x12, EESEEHideUI, is an EESEERust (ee/EESEARust.h): Enter sets its
// two views through ee_screens_see_hide_ui_views, the original's inline SetRectangles 0x17ccba.)

extern bool gB4C3220;                       // 0x4c3220 (= true; set while an entry is written back; provisional name)

class EESETriggerScreen : public EEUIFSEMainGroup {
public:
    EESETriggerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont);  // 0x17b0b7
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
    void AlphaSort();                       // 0x17c057
    void SelectEntry();                     // 0x17c0ad (also after EESEEShowDebugIDs)
    void DecPriority();                     // 0x17c2ae
    void DeleteEntry();                     // 0x17c31b
    void DuplicateEntry();                  // 0x17c411
    void ShowTab(unsigned long inControlID);    // 0x17c467
    void IncPriority();                     // 0x17c6e0
    void NewEntry();                        // 0x17c790
    void SelectName();                      // 0x17c92e

    EESETSBaseObject* GetScreen(long inIndex) const { return this->mScreens[inIndex]; }
    // 0x17c1e5: the factory (0 trigger, 1 condition, 2 effect, 3 object, 4 area; ret 0x10, `this` unused)
    EESETSBaseObject* CreateScreen(unsigned long inType, GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);
    void StoreEntry();                      // 0x17c599: writes the current entry back (its Virtual24)
    void ShowSelection();                   // 0x17c5fd: shows the list's selection
    void HideGroups();                      // 0x17c743: disables the five entity groups 9..13
    void FillList();                        // 0x17c99f: refills the list from the current screen's names

    // inline (COMDAT 0x17eaab, called out of line by EESETSCondition::Store): the name edit box's first line
    const char* GetName() const { return this->mNameEditBox->GetText(0); }
    // inline accessors (EESETSCondition::Store/Virtual2C)
    UIEditBox* GetNameEditBox() const { return this->mNameEditBox; }
    UIEditBox* GetTextEditBox() const { return this->mTextEditBox; }

    EESETSBaseObject* mScreens[5];          // +0x04 (0 trigger, 1 condition, 2 effect, 3 object, 4 area)
    GERectangle mR18;                       // +0x18
    UIEditBox* mNameEditBox;                // +0x28 (the entry's name; EESETSCondition::Load SetText)
    UITextLabel* mNameLabel;                // +0x2c (ctor: UITextLabel id 0x7dc)
    UITextLabel* mTextLabel;                // +0x30 (ctor: UITextLabel id 0x962)
    UIEditBox* mTextEditBox;                // +0x34 (the entry's text; GetAllText / SetText; id 0x963)
    // members below from the ctor 0x17b0b7 (control ids) and the dtor 0x17bf97 (deletes through slot 0); names
    // are provisional.
    UIRadioButton* mTabs[5];                // +0x38 (ids 0x7d0..0x7d4: one per mScreens entry)
    EESETSShowIDsCheckBox* mShowIDsCheckBox;  // +0x4c (id 0x7de)
    UILBString* mList;                      // +0x50 (id 0x7da: the entries)
    UIButton* mNewButton;                   // +0x54 (id 0x7d5)
    UIButton* mDuplicateButton;             // +0x58 (id 0x7d6)
    UIButton* mDeleteButton;                // +0x5c (id 0x7d7)
    UIButton* mAlphaSortButton;             // +0x60 (id 0x7dd)
    UIButton* mIncPriorityButton;           // +0x64 (id 0x7d8)
    UIButton* mDecPriorityButton;           // +0x68 (id 0x7d9)
    long mCurrentEntityGroup;               // +0x6c (ctor 9)
    long mCurrentScreen;                    // +0x70 (ctor 0)
    // sizeof 0x74 (its `new`)
};

#endif
