// C:\EEX\Empire Earth\Scenario Editor\EESETSObject.h (RECONSTRUCTION; file name inferred). The trigger screen's object
// sub-screen (RTTI EESETSObject > EESETSBaseObject; vtable 0x444638, sizeof 0xe0, ctor 0x18efb7, ??_G 0x191935, dtor
// 0x191973; EESETSObject.cpp 0x18efb7..0x19393b). Whole in Rust (ee_screens editor_object.rs: its controls, modes,
// filters, store, load and generated names): the class holds the object and its members forward. Requires
// ee/EESETSBaseObject.h, <vector>.
#ifndef EE_EESETSOBJECT_H
#define EE_EESETSOBJECT_H

class GEFont;
class UIEntity;
class UIForm;
struct EeScreensSeObject;

class EESETSObject : public EESETSBaseObject {
public:
    EESETSObject(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);    // 0x18efb7 (EESETriggerScreen's factory)
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
    virtual bool GetIsSelectOnMap();        // +0x4c 0x1924f3

    void Refresh();                         // 0x191c24 (EESEEObjectDetail, after setting the mode)
    void UpdateSelection();                 // 0x192ad3 (EESEEObjectDetail, the click, lasso, combo and count boxes)
    void CheckChanged(UIEntity* inCheckBox);    // 0x192ee0 (EESEEObjectCheck: the check box, by its id 0x846..)
    void SetIsSelectOnMap(bool inMap);      // the mode (the original's +0xdc store; EESEEObjectDetail)
    void ClearUnitIDs();                    // the picked units' ids cleared (EESEEObjectDetail)

    EeScreensSeObject* mRust;               // the screen (editor_object.rs)
};

#endif
