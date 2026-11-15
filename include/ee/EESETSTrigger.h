// C:\EEX\Empire Earth\Scenario Editor\EESETSTrigger.h (RECONSTRUCTION; file name inferred). The trigger screen's
// trigger sub-screen (RTTI EESETSTrigger > EESETSBaseObject; vtable 0x4447ac, sizeof 0xac, ctor 0x19393b, ??_G
// 0x195fc2, dtor 0x195fde; EESETSTrigger.cpp 0x19393b..0x196ebe). It edits the scenario manager's trigger mSelection:
// two condition rows and two effect rows. Whole in Rust (ee_screens editor_trigger.rs: its controls, rows, operators,
// store and load): the class holds the object and its members forward. Requires ee/EESETSBaseObject.h.
#ifndef EE_EESETSTRIGGER_H
#define EE_EESETSTRIGGER_H

class GEFont;
class UIForm;
struct EeScreensSeTrigger;

class EESETSTrigger : public EESETSBaseObject {
public:
    EESETSTrigger(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex);
    virtual ~EESETSTrigger();               // +0x00 0x195fde
    virtual void Sort();                    // +0x04 0x1961d4 (SortTriggers)
    virtual void MoveDown();                // +0x0c 0x1961df (MoveTriggerDown)
    virtual void Delete();                  // +0x10 0x1961fd (DeleteTrigger)
    virtual void Deactivate();              // +0x14 (EESETSCondition's shared `ret` 0xe123d)
    virtual void Duplicate();               // +0x18 0x1962b0 (DuplicateTrigger)
    virtual void GetNames(std::vector<UWideString>& outNames);      // +0x1c 0x196725
    virtual void Activate();                // +0x20 0x1962cd (fills the entry combo boxes)
    virtual void Store();                   // +0x24 0x196415 (writes the controls into the trigger)
    virtual void Hide();                    // +0x28 0x196737 (hides the controls)
    virtual void Load();                    // +0x2c 0x196785 (shows the trigger)
    virtual void MoveUp();                  // +0x30 0x1961ee (MoveTriggerUp)
    virtual void New();                     // +0x34 0x196c50 (NewTrigger)
    virtual void Show();                    // +0x38 0x196d94 (shows the controls)
    void ChangeOperator(unsigned long inControlID);  // 0x19666d (EESEETriggerOperator: the control's id)
    void RemoveEntry(unsigned long inControlID);    // 0x196c69 (EESEETriggerRemove: the control's id)
    // the rows' steps (EESEETriggerChangeIndex; the original's inline COMDATs 0x16fdfc..0x16fe41): stored, the first
    // condition or effect one back or on, loaded
    void DecrementA4();
    void IncrementA4();
    void DecrementA8();
    void IncrementA8();

    EeScreensSeTrigger* mRust;              // the screen (editor_trigger.rs)
};

#endif
