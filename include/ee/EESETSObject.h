// C:\EEX\Empire Earth\Scenario Editor\EESETSObject.h (RECONSTRUCTION; file name inferred). The trigger screen's object
// sub-screen (RTTI EESETSObject > EESETSBaseObject; vtable 0x444638, sizeof 0xe0, ctor 0x18efb7, ??_G 0x191935, dtor
// 0x191973; EESETSObject.cpp 0x18efb7..0x19393b). Only what matched code uses; provisional names. Requires
// ee/EESETSBaseObject.h, <vector>.
#ifndef EE_EESETSOBJECT_H
#define EE_EESETSOBJECT_H

class GEFont;
class UIEntity;
class UIForm;
class UIRadioButton;
class UITextLabel;
class UICheckBox;
class UITriStateCheckBox;
class UIComboBox;
class UIEditBox;
class EESETSOUnitList;
class EESETSAEditBox;
class EESETSAComboBox;
class EESETSUICBPlayer;
class EESETSEScenarioComboBox;
class EESEUICTriggerSelectUnits;
class EESETSAMouseClick;
class EESETSALasso;

class EESETSObject : public EESETSBaseObject {
public:
    EESETSObject(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);    // 0x18efb7 (EESETriggerScreen's factory)
    // the overrides (vtable 0x444638; bodies not written yet)
    virtual ~EESETSObject();                // +0x00 0x191973
    virtual void Sort();                    // +0x04 0x191bb4
    virtual void MoveDown();                // +0x0c 0x191bbf
    virtual void Delete();                  // +0x10 0x191be1
    virtual void Deactivate();              // +0x14 0x191c16
    virtual void Duplicate();               // +0x18 0x191d52
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x1924e1
    virtual void Activate();                // +0x20 0x191d63
    virtual void Store();                   // +0x24 0x1920f1
    virtual void Hide();                    // +0x28 0x1924fd
    virtual void Load();                    // +0x2c 0x1925a2
    virtual void MoveUp();                  // +0x30 0x191bd0
    virtual void New();                     // +0x34 0x192ac8
    virtual void Show();                    // +0x38 0x192f9f
    virtual void MouseMove(const GE2DPoint& inLocation);       // +0x40 0x192fbf
    // slots +0x00..+0x44 as EESETSBaseObject's; new ones:
    virtual void ClearUnitList();           // +0x48 0x191c06
    virtual bool GetIsSelectOnMap();        // +0x4c 0x1924f3 (+0x3c's +0x110)

    // inline (0x191759: the ctor calls it out of line, its only user)
    void SetDetailMode(bool inDetail) { this->mIsSelectOnMap = inDetail; this->Refresh(); }
    void Refresh();                         // 0x191c24 (EESEEObjectDetail, after setting mIsSelectOnMap)
    void UpdateSelection();                 // 0x192ad3 (EESEEObjectDetail; resizes mUnitIDs to the editor's selection)
    void CheckChanged(UIEntity* inCheckBox);    // 0x192ee0 (EESEEObjectCheck: the check box, by its id 0x846..)

    unsigned long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<unsigned long> mUnitIDs;    // +0x0c (0x192b39 resizes it to the editor's mSelectedUnits and copies the
                                            // units' mUnitID)
    std::vector<unsigned long> mUnitTypes;  // +0x1c (Virtual20: 0, then the unit classes of the type combo box)
    std::vector<long> mUnitSets;            // +0x2c (the ctor: the unit sets listed after the 0x75 unit groups)
    // the controls (the ctor's `new`s; ids 0x834..0x859; types from the vptr stores and the ctor calls)
    UIRadioButton* mUnitsRadio;             // +0x3c 0x834 (string 0x7664; Virtual4C: checked = the detail mode)
    UIRadioButton* mTypeRadio;              // +0x40 0x835 (string 0x7665)
    UITextLabel* mUnitListLabel;            // +0x44 0x845 (string 0x7672)
    EESETSOUnitList* mUnitList;             // +0x48 0x836
    long mL4C;                              // +0x4c (not touched by this unit)
    UITextLabel* mMinLabel;                 // +0x50 0x839 (string 0x7669)
    EESETSAEditBox* mMinEditBox;            // +0x54 0x837
    UITextLabel* mMaxLabel;                 // +0x58 0x83a (string 0x766a)
    EESETSAEditBox* mMaxEditBox;            // +0x5c 0x838
    UICheckBox* mDynamicCheckBox;           // +0x60 0x844 (string 0x766b)
    UITextLabel* mPlayerLabel;              // +0x64 0x83e (string 0x7666)
    EESETSEScenarioComboBox* GetAreaComboBox() const { return this->mAreaComboBox; }
    EESETSUICBPlayer* GetPlayerBC() const { return this->mPlayerBC; }
    EESETSUICBPlayer* GetPlayerCC() const { return this->mPlayerCC; }
    EESETSUICBPlayer* GetPlayer() const { return this->mPlayer; }
    EESETSUICBPlayer* mPlayer;              // +0x68 0x841
    UITextLabel* mGroupLabel;               // +0x6c 0x83e (string 0x503a)
    EESETSAComboBox* mGroupComboBox;        // +0x70 0x841 (the 0x75 unit groups, then the unit sets)
    UITextLabel* mTypeLabel;                // +0x74 0x83e (string 0x7667)
    UITextLabel* mParentLabel;              // +0x78 0x83e (string 0x76a7)
    EESETSAComboBox* mTypeComboBox;         // +0x7c 0x841 (Virtual20 fills it with mUnitTypes' names)
    UITriStateCheckBox* mAreaCheck;         // +0x80 0x846 (string 0x766c)
    EESETSEScenarioComboBox* mAreaComboBox; // +0x84 0x847
    UITriStateCheckBox* mNearCheck;         // +0x88 0x848
    UIComboBox* mNearModeComboBox;          // +0x8c 0x849 (strings 0x7149..0x714b)
    UIComboBox* mNearObjectComboBox;        // +0x90 0x849
    EESETSAComboBox* mParentComboBox;       // +0x94 0x859 (EESMObject::SetParent)
    UITriStateCheckBox* mHasAttributeCheck;  // +0x98 0x84a (string 0x7670)
    UIComboBox* mAttributeComboBox;         // +0x9c 0x84b (strings 0x71ac..)
    UITextLabel* mAttributeMinLabel;        // +0xa0 0x84b (string 0x7669)
    UIEditBox* mAttributeMinEditBox;        // +0xa4 0x84a
    UITextLabel* mAttributeMaxLabel;        // +0xa8 0x84d (string 0x766a)
    UIEditBox* mAttributeMaxEditBox;        // +0xac 0x84c
    UITextLabel* mPercentLabel;             // +0xb0 0x856
    UICheckBox* mAttributePercentCheckBox;  // +0xb4 0x857
    UITriStateCheckBox* mVisibleByCheck;    // +0xb8 0x84e (string 0x766d)
    EESETSUICBPlayer* mPlayerBC;            // +0xbc 0x84f
    UITriStateCheckBox* mHasStateCheck;     // +0xc0 0x850 (string 0x766e)
    UIComboBox* mActionComboBox;            // +0xc4 0x851 (Virtual20: the unit actions of gUnitActionTable)
    UITriStateCheckBox* mSelectedByCheck;   // +0xc8 0x852 (string 0x766f)
    EESETSUICBPlayer* mPlayerCC;            // +0xcc 0x853
    EESEUICTriggerSelectUnits* mSelectUnits;    // +0xd0 0x855
    EESETSAMouseClick* mMouseClick;         // +0xd4 (referenced)
    EESETSALasso* mLasso;                   // +0xd8 (referenced)
    bool mIsSelectOnMap;                    // +0xdc (EESEEObjectDetail: the detail control's id == 0x834)
};

#endif
