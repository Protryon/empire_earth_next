// C:\EEX\Low-Level Engine\User Interface\UIEvent.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006f278..0x1006f4ef: the U2D key init chains, then CreateControlEvents, CreateFormEvents, Initialize and
// Uninitialize. The inline members are emitted by UIBackground.cpp. UIFileComboBox.cpp's key chains start at
// 0x1006f4ef.
//
// The loops over the events files, the record test and the initialisation count are Rust
// (rust/crates/ee_ui/src/event.rs, docs/RUST_PLAN.md R6); the owners, their event lists and the virtual
// InitializeEvent/UninitializeEvent are reached here, and so are the UI database files' reads (ee_uicpp_file_*, also
// UIBackgroundTable::Create's).
#include <windows.h>

#include "rust/ee_rust.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIControlsTable.h"

// One event record of the UI database (0x74 bytes) (provisional names).
struct UIDbDataEvents {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mEventID;                       // +0x68 negative: no event
    uint32_t mOwnerID;                      // +0x6c control table index / form ID
    uint32_t mEventType;                    // +0x70
};
static_assert(sizeof(UIDbDataEvents) == 116, "on-disk record: 116 bytes on every target (ee_ui/src/event.rs DbEvent)");

// The Rust loops' view of the events' owners and an event's virtuals
struct UIEventRust {
    static long Initialize(void* inEvent) { return static_cast<UIEvent*>(inEvent)->InitializeEvent(); }
    static long Uninitialize(void* inEvent) { return static_cast<UIEvent*>(inEvent)->UninitializeEvent(); }
};

// The host calls of ee_ui/src/background_table_ffi.rs: the UI database files (FSFileSpec&)
extern "C" void ee_uicpp_file_open(void* inFile) { static_cast<FSFileSpec*>(inFile)->Open(kFileAccessRead); }
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount)
{
    return static_cast<FSFileSpec*>(inFile)->Read(*outCount);
}
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength)
{
    unsigned long theRead = 0;
    return static_cast<FSFileSpec*>(inFile)->Read(outData, inLength, theRead);
}
extern "C" void ee_uicpp_file_close(void* inFile) { static_cast<FSFileSpec*>(inFile)->Close(); }
// kind 0: a control table by index (CreateControlEvents), 1: the manager's form by id (CreateFormEvents)
extern "C" void* ee_uicpp_event_owner(int inKind, void* inManager, unsigned long inOwner)
{
    if (inKind == 0)
        return UIControlsTable::GetUIControlsTable(inOwner);
    return static_cast<UIManager*>(inManager)->GetFormByID(inOwner);
}
extern "C" void ee_uicpp_event_add(int inKind, void* inOwner, unsigned long inEventID, unsigned long inEventType)
{
    UIEventData theEvent;
    theEvent.mEventID = inEventID;
    theEvent.mEventType = inEventType;
    if (inKind == 0)
        static_cast<UIControlsTable*>(inOwner)->AddEvent(theEvent);
    else
        static_cast<UIForm*>(inOwner)->AddEvent(theEvent);
}
extern "C" long ee_uicpp_event_initialize(void* inEvent) { return UIEventRust::Initialize(inEvent); }
extern "C" long ee_uicpp_event_uninitialize(void* inEvent) { return UIEventRust::Uninitialize(inEvent); }

// FUNCTION: LLE 0x1006f398
// Loads the control events from the UI database file and adds each to its control table; returns the read result
long UIEvent::CreateControlEvents(FSFileSpec& inFile)
{
    return ee_ui_event_create(&inFile, 0, NULL);
}

// FUNCTION: LLE 0x1006f42a
// Loads the form events from the UI database file and adds each to its form; returns the read result
long UIEvent::CreateFormEvents(FSFileSpec& inFile, UIManager* inManager)
{
    return ee_ui_event_create(&inFile, 1, inManager);
}

// FUNCTION: LLE 0x1006f4be
// Counts one more user of the event, initialising it on the first
long UIEvent::Initialize()
{
    return ee_ui_event_initialize(&this->mInitializeCount, this);
}

// FUNCTION: LLE 0x1006f4d6
// Counts one user less, uninitialising the event when the last one leaves (also decrements a count already at 0)
long UIEvent::Uninitialize()
{
    return ee_ui_event_uninitialize(&this->mInitializeCount, this);
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1006f278
// SYNTHETIC: LLE 0x1006f282
// SYNTHETIC: LLE 0x1006f292
// SYNTHETIC: LLE 0x1006f29e
// SYNTHETIC: LLE 0x1006f2a8
// SYNTHETIC: LLE 0x1006f2b2
// SYNTHETIC: LLE 0x1006f2c2
// SYNTHETIC: LLE 0x1006f2ce
// SYNTHETIC: LLE 0x1006f2d8
// SYNTHETIC: LLE 0x1006f2e2
// SYNTHETIC: LLE 0x1006f2f2
// SYNTHETIC: LLE 0x1006f2fe
// SYNTHETIC: LLE 0x1006f308
// SYNTHETIC: LLE 0x1006f312
// SYNTHETIC: LLE 0x1006f322
// SYNTHETIC: LLE 0x1006f32e
// SYNTHETIC: LLE 0x1006f338
// SYNTHETIC: LLE 0x1006f342
// SYNTHETIC: LLE 0x1006f352
// SYNTHETIC: LLE 0x1006f35e
// SYNTHETIC: LLE 0x1006f368
// SYNTHETIC: LLE 0x1006f372
// SYNTHETIC: LLE 0x1006f382
// SYNTHETIC: LLE 0x1006f38e
