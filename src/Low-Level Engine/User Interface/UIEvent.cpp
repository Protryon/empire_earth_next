// C:\EEX\Low-Level Engine\User Interface\UIEvent.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006f278..0x1006f4ef: the U2D key init chains, then CreateControlEvents, CreateFormEvents, Initialize and
// Uninitialize. The inline members are emitted by UIBackground.cpp. UIFileComboBox.cpp's key chains start at
// 0x1006f4ef.
#include <windows.h>

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
static_assert(sizeof(UIDbDataEvents) == 116, "on-disk record: 116 bytes on every target");

// FUNCTION: LLE 0x1006f398
// Loads the control events from the UI database file and adds each to its control table; returns the read result
long UIEvent::CreateControlEvents(FSFileSpec& inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            UIDbDataEvents theData;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (!(theData.mEventID >= 0))
                continue;
            UIControlsTable* theTable = UIControlsTable::GetUIControlsTable(theData.mOwnerID);
            if (!theTable)
                continue;
            UIEventData theEvent;
            theEvent.mEventID = theData.mEventID;
            theEvent.mEventType = theData.mEventType;
            theTable->AddEvent(theEvent);
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1006f42a
// Loads the form events from the UI database file and adds each to its form; returns the read result
long UIEvent::CreateFormEvents(FSFileSpec& inFile, UIManager* inManager)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            UIDbDataEvents theData;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (!(theData.mEventID >= 0))
                continue;
            UIForm* theForm = inManager->GetFormByID(theData.mOwnerID);
            if (!theForm)
                continue;
            UIEventData theEvent;
            theEvent.mEventID = theData.mEventID;
            theEvent.mEventType = theData.mEventType;
            theForm->AddEvent(theEvent);
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1006f4be
// Counts one more user of the event, initialising it on the first
long UIEvent::Initialize()
{
    long theResult = kErrorOK;
    if (this->mInitializeCount == 0)
        theResult = this->InitializeEvent();
    if (theResult == kErrorOK)
        this->mInitializeCount++;
    return theResult;
}

// FUNCTION: LLE 0x1006f4d6
// Counts one user less, uninitialising the event when the last one leaves (also decrements a count already at 0)
long UIEvent::Uninitialize()
{
    long theResult = kErrorOK;
    if (this->mInitializeCount == 1)
        theResult = this->UninitializeEvent();
    if (theResult == kErrorOK)
        this->mInitializeCount--;
    return theResult;
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
