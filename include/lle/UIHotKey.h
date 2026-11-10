// C:\EEX\Low-Level Engine\User Interface\UIHotKey.h (RECONSTRUCTION). A key combination that fires up to
// four events. UIHotKey.cpp (0x10076f27..) holds the out-of-line members; the inline ones are emitted by UIEMouseWheelScroll.cpp. Vtable 0x1009997c (??_E,
// FireEvents). sizeof 0x38 (UIForm::AddHotKey news 0x38). Owner: the utils worker (UIHotKey.cpp); this partial
// header was started by ui-core with the members UIForm.cpp needs (layout from the ctors 0x1006e075/0x1006e0b3).
// Requires lle/UIEntity.h, lle/Input System.h (gISKeyboard).
#ifndef LLE_UIHOTKEY_H
#define LLE_UIHOTKEY_H

#include <vector>

#include "lle/UIEntity.h"
#include "lle/TMPoll.h"

class FSFileSpec;
class ISEntity;
class UIEvent;
class UIForm;

// winspool.h's `#define SetForm SetFormA` would rename UIHotKey::SetForm (exported ?SetForm@UIHotKey@@QAEXPAVUIForm@@@Z)
#ifdef SetForm
#undef SetForm
#endif

class LLE_API UIHotKey : public UIEntity {
    friend class UIForm;                    // UIForm.cpp reads and writes the fields directly
    friend struct UIHotKeyRust;             // UIHotKey.cpp: the Rust FireEvents' view (ee_ui hotkey.rs)
public:
    // header-inline: emitted by the first includer, UIEMouseWheelScroll.cpp (0x1006e075..0x1006e2a9), in this order
    UIHotKey()
        : UIEntity(0)
    {
        this->mKey = NULL;
        this->mModifiers[0] = NULL;
        this->mModifiers[1] = NULL;
        this->mModifiers[2] = NULL;
        this->mDoublePressedEvent = NULL;
        this->mJustPressedEvent = NULL;
        this->mPressedEvent = NULL;
        this->mJustUnpressedEvent = NULL;
        this->mForm = NULL;
        this->CalculateKeyCount();
        this->mModifierState = 0;
        this->mAlwaysFire = false;
        this->mEnabled = true;
    }
    UIHotKey(UIForm* inForm, const UIHotKey& inHotKey)
        : UIEntity(0)
    {
        this->mKey = inHotKey.mKey;
        this->mModifiers[0] = inHotKey.mModifiers[0];
        this->mModifiers[1] = inHotKey.mModifiers[1];
        this->mModifiers[2] = inHotKey.mModifiers[2];
        this->mDoublePressedEvent = NULL;
        this->mJustPressedEvent = NULL;
        this->mPressedEvent = NULL;
        this->mJustUnpressedEvent = NULL;
        this->SetDoublePressedEvent(inHotKey.mDoublePressedEvent);
        this->SetJustPressedEvent(inHotKey.mJustPressedEvent);
        this->SetJustUnpressedEvent(inHotKey.mJustUnpressedEvent);
        this->SetPressedEvent(inHotKey.mPressedEvent);
        this->mForm = inForm;
        this->mKeyCount = inHotKey.mKeyCount;
        this->mEnabled = inHotKey.mEnabled;
        this->mModifierState = inHotKey.mModifierState;
        this->mAlwaysFire = inHotKey.mAlwaysFire;
    }
    virtual ~UIHotKey()
    {
        this->SetDoublePressedEvent(NULL);
        this->SetJustPressedEvent(NULL);
        this->SetJustUnpressedEvent(NULL);
        this->SetPressedEvent(NULL);
    }
    static long CreateHotKeys(FSFileSpec& inFile);
    static void DeleteHotKeys();
    virtual bool FireEvents();              // slot 0x04
    bool GetAlwaysFire() const { return this->mAlwaysFire; }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe: GetHotKey inlines (__imp_sHotKey), GetHotKeyCount is called
    // through the import (0x268dc6; our compile does the same with both inline)
    static UIHotKey* GetHotKey(unsigned long inIndex) { return UIHotKey::sHotKey[inIndex]; }
    static unsigned long GetHotKeyCount() { return UIHotKey::sHotKey.size(); }
#endif
    void GetHotKeyName(UWideString& outName);
    unsigned long GetKeyCount() const { return this->mKeyCount; }
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long GetPressedPollAmount() { return UIHotKey::sPressedPollAmount; }
#endif
    void SetAlwaysFire(bool inAlwaysFire) { this->mAlwaysFire = inAlwaysFire; }
    void SetDoublePressedEvent(UIEvent* inEvent);
    void SetEnabled(bool inEnabled) { this->mEnabled = inEnabled; }
    void SetForm(UIForm* inForm) { this->mForm = inForm; }
    void SetJustPressedEvent(UIEvent* inEvent);
    void SetJustUnpressedEvent(UIEvent* inEvent);
    void SetKey(unsigned long inKey)
    {
        gISKeyboard->GetEntity(inKey, this->mKey);
        this->CalculateKeyCount();
    }
    void SetModifier(unsigned long inKey, unsigned char inIndex);
    void SetPressedEvent(UIEvent* inEvent);
#ifdef LOWLEVELENGINE_EXPORTS
    static void UpdatePressedPollAmount() { UIHotKey::sPressedPollAmount = UIHotKey::sPressedPoll.Poll(); }
#endif
private:
    void CalculateKeyCount();

    ISEntity* mKey;                         // +0x08
    ISEntity* mModifiers[3];                // +0x0c
    UIEvent* mDoublePressedEvent;           // +0x18
    UIEvent* mJustPressedEvent;             // +0x1c
    UIEvent* mJustUnpressedEvent;           // +0x20
    UIEvent* mPressedEvent;                 // +0x24
    UIForm* mForm;                          // +0x28
    unsigned long mModifierState;           // +0x2c (provisional name)
    unsigned long mKeyCount;                // +0x30 (provisional name; CalculateKeyCount)
    bool mEnabled;                          // +0x34
    bool mAlwaysFire;                       // +0x35

#ifdef LOWLEVELENGINE_EXPORTS
    static TMPoll sPressedPoll;                     // 0x1012e7d8
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    static std::vector<UIHotKey*> sHotKey;          // 0x1012e7f8 (exe import)
#endif
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long sPressedPollAmount;        // 0x1012e808
#endif
};

#endif
