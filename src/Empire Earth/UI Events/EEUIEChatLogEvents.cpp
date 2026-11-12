// C:\EEX\Empire Earth\UI Events\EEUIEChatLogEvents.cpp (file name inferred: no __FILE__; class prefix EEUIECLE).
// 0x1d321b..0x1d3a77: the init chain of the unused file-static key, EEUIECLEFormManager's members, the entry's
// implicit members after their first users, the three ProcessEvents (now Rust's: ee_screens chat.rs), then the deque<EEChatLogEntry> template members
// (VC6 emits template COMDATs at the end of the unit). Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UIFormManager.h"
#include "lle/FSFile.h"
#include "lle/UResourceFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEStringIDs.h"
#include "lle/GETextAlignment.h"
static const UString kChatLogKey(UWideString("EEUIECLESaveLoadChatLog::"));   // 0x962ef0 (unused)

// FUNCTION: EXE 0x5d3269
// Creates the chat log's form manager with an empty log
EEUIECLEFormManager::EEUIECLEFormManager()
{
}

// FUNCTION: EXE 0x5d3289
// Destroys the chat log form manager and frees it when asked (scalar deleting destructor)
// (??_G, emitted with the vtable)

// FUNCTION: EXE 0x5d32a5
// Destroys the chat log form manager and its entries
EEUIECLEFormManager::~EEUIECLEFormManager()
{
}

// FUNCTION: EXE 0x5d32bf
// Appends an entry to the chat log, dropping the oldest entries beyond 40
void EEUIECLEFormManager::AddEntry(const EEChatLogEntry& inEntry)
{
    EEUIECLEFormManager* theManager = EEUIECLEFormManager::Get();
    theManager->mEntries.push_back(inEntry);
    if (theManager->mEntries.size() > 40)
        for (unsigned long i = 0; i < theManager->mEntries.size() - 40; i++)
            theManager->mEntries.pop_front();
}

// FUNCTION: EXE 0x5d32fe
// Returns the chat log form's manager, creating and attaching it on first use
EEUIECLEFormManager* EEUIECLEFormManager::Get()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormChatLog);
    EEUIECLEFormManager* theManager;
    if (theForm->GetFormManager() != NULL)
        theManager = static_cast<EEUIECLEFormManager*>(theForm->GetFormManager());
    else {
        theManager = new EEUIECLEFormManager;
        theForm->SetFormManager(theManager);
    }
    return theManager;
}

// FUNCTION: EXE 0x5d3345
// Empties the chat log
void EEUIECLEFormManager::Clear()
{
    EEUIECLEFormManager* theManager = EEUIECLEFormManager::Get();
    theManager->mEntries.clear();
}

// FUNCTION: EXE 0x5d3352
// Returns the number of chat log entries
unsigned long EEUIECLEFormManager::GetEntryCount()
{
    return EEUIECLEFormManager::Get()->mEntries.size();
}

// FUNCTION: EXE 0x5d335b
// Replaces the chat log with the entries read from a saved game; returns the first read error
long EEUIECLEFormManager::Load(unsigned long inVersion, unsigned long inSize, FSFile& inFile)
{
    EEChatLogEntry theEntry;
    unsigned long theCount;
    long theResult;
    EEUIECLEFormManager::Clear();
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
            theResult = inFile.Read(theEntry.mType);
            if (theResult != kErrorOK)
                continue;
            theResult = theEntry.mName.Load(inFile);
            if (theResult != kErrorOK)
                continue;
            theResult = theEntry.mColor.Load(inFile);
            if (theResult != kErrorOK)
                continue;
            theResult = theEntry.mText.Load(inFile);
            if (theResult != kErrorOK)
                continue;
            theResult = inFile.Read(theEntry.mTime);
            if (theResult == kErrorOK)
                EEUIECLEFormManager::AddEntry(theEntry);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5d340d
// Creates an empty chat log entry
// (EEChatLogEntry's implicit ctor)

// FUNCTION: EXE 0x5d3434
// Writes the chat log entries (type, name, colour, text, time) to a saved game; returns the first write error
long EEUIECLEFormManager::Save(unsigned long inVersion, unsigned long inSize, FSFile& inFile)
{
    EEChatLogEntry theEntry;
    EEUIECLEFormManager* theManager = EEUIECLEFormManager::Get();
    unsigned long theCount = theManager->mEntries.size();
    long theResult = inFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        theEntry = theManager->mEntries[i];
        theResult = inFile.Write(theEntry.mType);
        if (theResult != kErrorOK)
            continue;
        theResult = theEntry.mName.Save(inFile);
        if (theResult != kErrorOK)
            continue;
        theResult = theEntry.mColor.Save(inFile);
        if (theResult != kErrorOK)
            continue;
        theResult = theEntry.mText.Save(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Write(theEntry.mTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x5d3520
// Copies a chat log entry
// (EEChatLogEntry's implicit operator=)

// (0x5d3565..0x5d38c7: the chat log form's events, Rust's whole: ee_screens chat.rs)

// FUNCTION: EXE 0x5d38c7
// Removes the oldest chat log entry
// (deque<EEChatLogEntry>::pop_front)

// FUNCTION: EXE 0x5d391c
// Appends a chat log entry
// (deque<EEChatLogEntry>::push_back)

// FUNCTION: EXE 0x5d39a8
// Removes every chat log entry
// (deque<EEChatLogEntry>::clear)

// FUNCTION: EXE 0x5d39c1
// Removes the newest chat log entry
// (deque<EEChatLogEntry>::pop_back)

// FUNCTION: EXE 0x5d3a10
// Copy-constructs a chat log entry in the log's storage
// (EEChatLogEntry's implicit copy ctor, from the deque's allocator construct)

// FUNCTION: EXE 0x5d3a4e
// Destroys a chat log entry and frees it when asked (scalar deleting destructor)
// (EEChatLogEntry's ??_G, from the deque's allocator destroy)

// SYNTHETIC: EXE 0x5d321b
// SYNTHETIC: EXE 0x5d3225
// SYNTHETIC: EXE 0x5d3252
// SYNTHETIC: EXE 0x5d325e
