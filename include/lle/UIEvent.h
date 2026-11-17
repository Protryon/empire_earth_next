// C:\EEX\Low-Level Engine\User Interface\UIEvent.h (RECONSTRUCTION). Base of the UI's triggered events (reference
// counted). Vtable 0x10098b5c: ProcessEvent (pure, named from the UIE* overrides), InitializeEvent, UninitializeEvent.
// The original's sizeof was 0xc.
//
// Whole in Rust (rust/crates/ee_ui/src/eventobj.rs, R6 ui round 21): the member is EeUiEventFields, the class's private
// base (the inline constructor and GetIsInitialized read it in place); Initialize, Uninitialize and the two loaders
// forward there (UIManager.cpp), calling out through HostOp: the virtuals, the events file and the owners.
#ifndef LLE_UIEVENT_H
#define LLE_UIEVENT_H

#include "rust/ui/eventobj.h"
#include "rust/ui/obj.h"

class FSFileSpec;
class UIEntity;
class UIForm;
class UIManager;

class LLE_API UIEvent : public UReferenceCounter, private EeUiEventFields {
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
private:
    // the Rust record's and loaders' calls out (UIManager.cpp): the object is the record or the file
    static intptr_t HostOp(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f);
};

#endif
