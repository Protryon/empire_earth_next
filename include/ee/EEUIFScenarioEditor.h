// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.h (RECONSTRUCTION; file name inferred).
// The scenario editor's main form: RTTI EEUIFScenarioEditor > UIForm, vtable 0x445084, sizeof 0x2b8 (`new` at
// 0x273e8e, EEUIManager::CreateScenarioEditor), ctor 0x19d615. The game reaches it as
// gUIManager->mScenarioEditorForm (+0x130), and the editor's UI events get it as their ProcessEvent's inForm
// (`(EEUIFScenarioEditor*)inForm`). EEUIFScenarioEditor.cpp is about 0x19cc39..0x1a0315 (its events' handlers, the
// ctor, the members, then the inline event classes).
// Layout: the ctor 0x19d615 (member ctors in order: EECivilization +0x1a0, three floats +0x1e0, +0x1ec, four floats
// +0x1f0, the two vectors +0x208/+0x218) and a field census over 0x16f892..0x1cba2f.
// Member names by offset (types from the ctor's `new` sites and the imported calls on them); method names
// are provisional. Requires lle/UIForm.h, ee/EECivilization.h, <vector>.
#ifndef EE_EEUIFSCENARIOEDITOR_H
#define EE_EEUIFSCENARIOEDITOR_H
#include "ee/EEScenarioEditorIDs.h"

class EEUIMiniMap;
class EEUIEGrabScroll;
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
class UICheckBox;
class UIComboBox;
class UITextLabel;

class EEUIFScenarioEditor : public UIForm {
public:
    void CreatePanels(unsigned long& ioIndex); // actual430B editor setup
    void CreateBackground(); // actual463B texture/background setup
    void CreateToolbarButtons(unsigned long& ioIndex); // actual1070B toolbar builder
    virtual long UpdateKeyboard(); // inherited slot2c
    virtual void Open(); // inherited slot48
    virtual long DrawBackground(const GERectangle& inRectangle); // inherited slot18, actual base return contract
    void PostLoadEvent(UReferenceCounter* inObject); // actual Post40B, ret4
    void PostShowLoadedObject(UReferenceCounter* inObject); // actual Post40C, ret4
    virtual void Close(); // actual inherited slot38, original19e68a; no added slot
    virtual ~EEUIFScenarioEditor();         // 0x19e3f7 (??_G 0x19e3db)
    virtual long CopyData();                // slot 0x3c 0x19e72c
    virtual long UpdateMouse();             // slot 0x30 0x1a0083
protected:
    virtual long DrawPolygons();            // slot 0x40 0x19f5b3 (UIForm's, protected)
public:
    EEUIFSEMainGroup* CreateTabGroup(long inGroup, unsigned long& ioIndex); //19ef5a, original-backed ret8
    void CreateTabButtons(unsigned long& ioIndex); // original builder ret4
    void LeaveTab(); // actual current entity group refresh
    void EnterFullView(); // original tab view disable
    void LeaveFullView(); // original tab view enable
    // 0x19f6e9: leaves the editor (resets +0x200/+0x2b3, sets +0x2b4); EESEATestScenario passes true,
    // EESEAExitScenario false (provisional name)
    void ExitEditor(bool inTestScenario);
    // 0x19fc97: calls slot +0x1c of the current tab's group (mTabGroups, indexed through the tab table 0x4c46d0 by
    // +0x2a8); EESEAQuickSaveScenario (provisional name)
    void RefreshCurrentGroup();
    // 0x19f757: clears +0x2b4, then slot +0x14 of the current tab's group; EESEAQuickSaveLoad after a quick
    // save/load (provisional name)
    void ResumeCurrentGroup();
    void ClearSelection();                  // 0x19fa54 (provisional name)
    // 0x19fab9: deselects the first selected unit (EESEADeleteUnit::Virtual4 after taking it)
    // (provisional name)
    void DeselectFirstUnit();

    void SelectUnit(EEUnit* inUnit);        // 0x19e606 (provisional name)
    void DeselectUnit(EEUnit* inUnit);      // 0x19fb84 (provisional name)
    // 0x19f615: switches to tab inTab (stored at +0x2a8; EEUIFSEMainButton::Check; provisional name)
    EEUIFScenarioEditor(UIManager* inManager);   // 0x19d615 (EEUIManager::CreateScenarioEditor news 0x2b8 bytes)
    void SelectTab(unsigned char inFlag, long inTab);
    void FnE123D();                         // (ICF-folded into 0xe123d: an empty body)
    // 0x17d597 (copies the rectangle into a member; EESEAPaintArea; its body sits in the trigger screens' range, so
    // it may be an inline COMDAT) (provisional name)
    void SetPaintArea(const GERectangle& inRectangle);
    // 0x19fd09: shows inText as the loading text (EESEAChangeLoadText) (provisional name)
    void SetLoadText(const UWideString& inText);
    // 0x19fcb4: the game view rectangle (EESETriggerScreen 0x17c130/0x17c7d0; provisional name)
    void SetGameView(const GERectangle& inRectangle, bool inIsUIHidden);
    // inline (COMDAT 0x16fabb, emitted by the first unit that calls it out of line)
    unsigned long GetSelectedUnitCount() { return this->mSelectedUnits.size(); }
    // inline const (EESEEObjectUnitSelect 0x16fa57: the const operator[]'s dead `lea`)
    EEUnit* GetSelectedUnit(unsigned long inIndex) const { return this->mSelectedUnits[inIndex]; }
    // inline (COMDAT 0x1a6e0d after its first user in EESEADeleteUnit.cpp's range; EEUIBUpgrade calls it out of line;
    // body shape to be checked against 0x1a6e0d) (provisional name)
    EEUnit* GetFirstSelectedUnit() { if (this->mSelectedUnits.size() == 0) return NULL; return this->GetSelectedUnit(0); }  // COMDAT 0x1a6e0d
    // inline (COMDAT 0x16fa2f: the trigger tab, group 4; EESEEObjectDetail/EESEETriggerChangeIndex call it out of
    // line)
    EESETriggerScreen* GetTriggerScreen() const { return (EESETriggerScreen*)this->mTabGroups[kEditorTabTriggers]; }
    // inline (COMDAT 0x5253 after its first user EEAUpgradeImprovement::Virtual4, which calls it out of line once and
    // expands it once; the unit tab, group 3) (provisional name)
    EESEUnit* GetUnitScreen() const { return (EESEUnit*)this->mTabGroups[3]; }
    // inline (COMDAT 0x19787b after its first user EESEUITechListBox::CopyData; Fill expands it with operator[] out of
    // line; the player tab, group 5) (provisional name)
    EESEPlayerScreen* GetPlayerScreen() const { return (EESEPlayerScreen*)this->mTabGroups[kEditorTabPlayers]; }

    EECivilization mC1A0;                   // +0x1a0 (ctor 0x12c57d; sizeof 0x40)
    GERectangle mR1E0;                      // +0x1e0 (the saved view rectangle; the ctor's inline GERectangle() zeroes it)
    GERectangle mR1F0;                      // +0x1f0 (four floats zeroed: GERectangle's inline ctor)
    EESELoadThread* mLoadThread;            // +0x200 the loading thread (0 in the ctor)
    EEUIFSEMainButton** mTabButtons;        // +0x204 owned seven-button array (0x19eba9 new[7], ExitEditor checks [0])
    std::vector<EEUIFSEMainGroup*> mTabGroups;  // +0x208 the tab groups (indexed by the tab table 0x4c46d0)
    std::vector<EEUnit*> mSelectedUnits;    // +0x218 the selected units
    UITextLabel* mLoadTextLabel;            // +0x228
    GEBitmap* mPanelBackground;             // +0x22c (the trigger tab's panel background: EESETriggerScreen 0x17c399)
    GETexture* mButtonTexture;              // +0x230 (LoadTexture, referenced)
    EEAScroll* mEdgeScroll;                 // +0x234 (a new 0x50-byte object, ctor 0x470f: the edge scroll)
    EEUIMiniMap* mMiniMap;                          // +0x238 (a new object, ctor 0x275459)
    EEUIFSEUndo* mUndoButton;                       // +0x23c
    EEUIFSERedo* mRedoButton;                       // +0x240
    EEUIFSEBrushGroup* GetBrushGroup() const { return this->mBrushGroup; }
    EEUIFSEBrushGroup* mBrushGroup;         // +0x244 (new 0x30, ctor 0x1a0315; SetPaintArea)
    UIFullBackground* mFullBackground;                   // +0x248
    EEUIEGrabScroll* mGrabScroll;           // +0x24c (the ctor: new EEUIEGrabScroll, the middle button's events)
    UIComboBox* mPlayerComboBox;            // +0x250
    UIButton* mLoadButton;                       // +0x254
    UIButton* mTestButton;                       // +0x258
    UIButton* mSaveButton;                       // +0x25c
    UIButton* mExitButton;                       // +0x260
    UIButton* mQuickSaveButton;                  // +0x264
    UIButton* mQuickLoadButton;                  // +0x268
    UIButton* mCivBuilderButton;                 // +0x26c
    UICheckBox* mHideUICheckBox;            // +0x270
    UITextLabel* mHelpLabel;                // +0x274
    UIEvent* mNewPlayerEvent;               // +0x278 (the combo box's trigger event)
    TMPoll* mPoll;                          // +0x27c (new TMPoll(500, 0, 0), started)
    UIEvent* mWheelZoomEvent;               // +0x280 (the mouse wheel event)
    UIEvent* mZoomInEvent;                  // +0x284 (hot key events)
    UIEvent* mZoomOutEvent;                 // +0x288
    long mL28C;                             // +0x28c
    long mL290;                             // +0x290
    UIEvent* mRevolveCameraEvent;           // +0x294
    UIEvent* mRotateCameraEvent;            // +0x298
    UIEvent* mDisableMouseMoveEvent;        // +0x29c
    UIEvent* mEnableMouseMoveEvent;         // +0x2a0
    long mCurrentEntityGroup;               // +0x2a4
    long mCurrentTab;                       // +0x2a8 the current tab (index into the tab table 0x4c46d0)
    long mNumCargoUnits;                    // +0x2ac
    bool mIsEntityGroupEnabled;             // +0x2b0
    bool mB2B1;                             // +0x2b1
    bool mB2B2;                             // +0x2b2
    bool mIsSwitching;                      // +0x2b3
    bool mIsSuspended;                      // +0x2b4
    bool mIsUIHidden;                       // +0x2b5
    bool mWasUIHidden;                      // +0x2b6
    bool mExitAfterSave;                    // +0x2b7
};

#endif
