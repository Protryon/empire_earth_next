// EEUISCIFormManager (RECONSTRUCTION; file name inferred; provisional field names). The scenario
// info form's (0x13) manager: RTTI EEUISCIFormManager > UIFormManager, vtable 0x446a34, sizeof 0xa8. Get() 0x1e7270
// creates it on first use (EESMMediaStrings at +0x04; the inline ctor calls Clear 0x1e71cc). Its members are defined
// at the top of EEUIEScenarioIntroEvents.cpp (0x1e71cc..0x1e76c1; the ??_G 0x1e83cb closes that unit). The four
// files are FSFileSpec* (gDataPath->FindFile's out type; Get* copy them through FSFileSpec::operator=). Member names
// are provisional. Requires lle/UIFormManager.h, ee/EESMMediaStrings.h.
#ifndef EE_EEUISCIFORMMANAGER_H
#define EE_EEUISCIFORMMANAGER_H
#include "rust/game/screens.h"

#include "lle/UIFormManager.h"
#include "ee/EESMMediaStrings.h"

class EEUISCIFormManager : public UIFormManager, public EeScreensSciFormFields {
public:
    EEUISCIFormManager() { this->Clear(); }  // inline (Get: UIFormManager's vptr, the media strings, then Clear)
    virtual ~EEUISCIFormManager() { ee_screens_sci_delete_objects((uintptr_t)this); }  // inline (the ??_G 0x1e83cb expands it)

    // Resets the scenario intro manager: no open files and every flag cleared
    void Clear() { ee_screens_sci_clear((uintptr_t)this); }
    // Returns the scenario intro form's manager, creating and attaching it on first use
    static EEUISCIFormManager* Get() { return reinterpret_cast<EEUISCIFormManager*>(ee_screens_sci_get()); }
    // Returns the scenario's briefing text
    UWideString GetBriefing() { return this->mMediaStrings.mInstructions; }              // 0x1e72cf (mMediaStrings.mInstructions)
    // the map's and the sound's file names (sci_calls.rs' shims: the strings are private to the friends)
    const UWideString& MapFileName() const { return this->mMediaStrings.mMapFile; }
    const UWideString& SoundFileName() const { return this->mMediaStrings.mSoundFile; }
    // Returns the scenario's history text
    UWideString GetHistory() { return this->mMediaStrings.mHistory; }               // 0x1e7563 (mHistory)
    // Returns the scenario's hints text
    UWideString GetHints() { return this->mMediaStrings.mHints; }                 // 0x1e7578 (mW14)
    // Opens the scenario's map picture, the scenario file or the sound and copies its file spec; returns whether it was
    // found
    bool GetMap(FSFileSpec& outSpec) { return ee_screens_sci_get_map((uintptr_t)this, (uintptr_t)&outSpec); }
    bool GetSound(FSFileSpec& outSpec) { return ee_screens_sci_get_sound((uintptr_t)this, (uintptr_t)&outSpec); }
    // Starts or stops the main game's briefing button blinking, showing the large briefing alert while it blinks
    void SetBlinking(bool inBlinking) { ee_screens_sci_set_blinking(inBlinking); }      // 0x1e766a (the main game form's info button; 0x1be512, 0x26615a)
    // 0x1d2ae5 (out of line COMDAT; the SCL/SPL load events call it): owns the opened scenario file
    void SetFile(FSFileSpec* inFile) { delete this->mFile; this->mFile = inFile; }

    EESMMediaStrings mMediaStrings;         // +0x04 (sizeof 0x8c)
    FSFileSpec* mMap;                       // +0x90 (OpenMap)
    // (+0x94: the original's movie file, OpenMovie's)
    FSFileSpec* mFile;                      // +0x98 (SetFile; OpenScenario)
    FSFileSpec* mSound;                     // +0x9c (OpenSound)
};

#endif
