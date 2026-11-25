// C:\EEX\Empire Earth\Scenario Manager\EESMMediaStrings.h (RECONSTRUCTION; provisional name). Seven wide
// strings of a scenario (EESMMediaStrings.cpp, __FILE__ 0x4c5944; Load 0x1c5e78 and Save 0x1c5ecc take the save
// version and the file). sizeof 0x8c: EEPSaveGameHeader +0x438..+0x4c4. Its implicit default ctor is an out-of-line
// COMDAT (0x14bd35, emitted after EEPSaveGameHeader's ctor, its first user): seven UWideString default ctors in
// member order. Member names are provisional (by offset).
// Requires lle/LowLevelEngine.h.
#ifndef EE_EESMMEDIASTRINGS_H
#define EE_EESMMEDIASTRINGS_H

#include "rust/sim/scenario.h"
class EESMMediaStrings {
public:
    // Reads the seven media strings from the saved file; returns the first error (ee_scenario save.rs strings)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_media_io(this, &inFile, false); }
    // Writes the seven media strings to the saved file; returns the first error
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_media_io(this, &outFile, true); }
    // 0x1c5783 (`ret 0x10`; EECampaign::ListScenarioMedia 0x28cc2e: the scenario file, the media file list, the
    // error text, a flag; returns bool; provisional name)
    // Appends the scenario's sound and map picture files (lower case; the movie is not listed) to the list and each
    // missing file's error text to outError; returns false if one is missing
    // (ee_scenario entry.rs: the search of each, GetMediaFile 0x1c5828, in the data path: the movie in Movies, the sound
    // in Sounds as .wav then .mp3, the map picture in Scenarios, with the campaign prefix on request; the error text
    // built as a string, where the original's sprintf into outError's buffer overran it for long names)
    bool ListMediaFiles(const UWideString& inFile, std::vector<UWideString>& ioFiles, UWideString& outError, bool inFlag) { return ee_scenario_en_media_files(this, (void*)&inFile, &ioFiles, &outError, inFlag); }
    // the trigger text file's sections by their tag's number (ee_scenario text.rs, with Export 0x1c5d2e, Import
    // 0x1c5dfe and Validate 0x1c5f20): 0 the hints, 1 the aftermath, 2 the history, 3 the instructions
    UWideString& GetSection(unsigned long inSlot)
    {
        UWideString* theSections[] = {&this->mHints, &this->mAftermath, &this->mHistory, &this->mInstructions};
        return *theSections[inSlot];
    }
    // string inSlot in the save's order (aftermath, hints, history, movie, sound, instructions, map; ee_scenario
    // entry.rs)
    UWideString* GetString(unsigned long inSlot) { return &(this->*sStrings[inSlot]); }

private:
    static UWideString EESMMediaStrings::* const sStrings[7];
    friend class EEUIFMainGame; // actual briefing-string read at266f03; nominal access placement provisional
    friend struct EEScreensMedia;           // the campaign scenario select's Start (UI Events/EEScreens.cpp) prefixes
                                            // mSoundFile/mMapFile with the campaign's scenario prefix (0x1d2910)
    friend class EEUISCIFormManager;        // reads the strings (OpenMap/OpenSound, the Get* copies; 0x1e72cf..)
    friend class EESMEffect;                // Execute 0x1be3eb..0x1be4e6 appends to / clears mW14, mHistory, mInstructions
    friend struct EESEInstructionsHost;     // the editor's instructions tab: Virtual1C 0x171f8b writes mAftermath..mMapFile, Virtual14 0x171ce3 reads them
    UWideString mAftermath;                // +0x00
    UWideString mHints;                     // +0x14
    UWideString mHistory;                   // +0x28
    UWideString mMovieFile;                 // +0x3c
    UWideString mSoundFile;                 // +0x50
    UWideString mInstructions;              // +0x64
    UWideString mMapFile;                   // +0x78
};

// the strings in the save's order (ee_scenario entry.rs EE_SCENARIO_EN_MEDIA_STRING); the unit's whole table
// (EESMMediaStrings.cpp 0x1c5783..0x1c5f73, the rest is ee_scenario's)
inline UWideString EESMMediaStrings::* const EESMMediaStrings::sStrings[7] = {&EESMMediaStrings::mAftermath, &EESMMediaStrings::mHints, &EESMMediaStrings::mHistory, &EESMMediaStrings::mMovieFile, &EESMMediaStrings::mSoundFile, &EESMMediaStrings::mInstructions, &EESMMediaStrings::mMapFile};

#endif
