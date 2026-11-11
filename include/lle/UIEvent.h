// C:\EEX\Low-Level Engine\User Interface\UIEvent.h (RECONSTRUCTION). Base of the UI's triggered events (reference
// counted). UIEvent.cpp (0x1006f398..) holds CreateControlEvents/CreateFormEvents/Initialize/Uninitialize; the inline
// members are emitted first by UIBackground.cpp (0x10059e7d..0x10059ebb). Vtable 0x10098b5c: ProcessEvent (pure,
// named from the UIE* overrides), InitializeEvent, UninitializeEvent. sizeof 0xc.
#ifndef LLE_UIEVENT_H
#define LLE_UIEVENT_H

class FSFileSpec;
class UIEntity;
class UIForm;
class UIManager;

class LLE_API UIEvent : public UReferenceCounter {
    friend struct UIEventRust;              // the Rust count's calls of the virtuals (UIEvent.cpp)
public:
    UIEvent() { this->mInitializeCount = 0; }  // body assignment: the store follows the vptr load (exe 0x1aa9ef)
    virtual ~UIEvent() {}
    static long CreateControlEvents(FSFileSpec& inFile);
    static long CreateFormEvents(FSFileSpec& inFile, UIManager* inManager);
    bool GetIsInitialized() const { return this->mInitializeCount != 0; }
    long Initialize();
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) = 0;   // slot 0x04
    long Uninitialize();
protected:
    virtual long InitializeEvent() { return kErrorOK; }
    virtual long UninitializeEvent() { return kErrorOK; }

    unsigned long mInitializeCount;         // +0x08 Initialize/Uninitialize nesting (provisional name)
};

#endif
