// C:\EEX\Empire Earth\Scenario Manager\EESMMediaStrings.cpp (__FILE__ 0x4c5944, used by 0x1c5828's ESVERIFYs).
// 0x1c5783..0x1c5f73: the seven media/briefing strings of a scenario (the trigger text file's sections, Export
// 0x1c5d2e, Import 0x1c5dfe and Validate 0x1c5f20, are ee_scenario text.rs). Unit flags: the exe
// module set. EESMEffect's COMDAT tail (the EESMUnitPriorityLess sort family) ends at 0x1c5783.
#include <windows.h>
#include <stdio.h>
#include <mbstring.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioHost.h"
// Appends the scenario's sound and map picture files (lower case; the movie is not listed) to the list and each
// missing file's error text to outError; returns false if one is missing
// (ee_scenario entry.rs: the search of each, GetMediaFile 0x1c5828, in the data path: the movie in Movies, the sound
// in Sounds as .wav then .mp3, the map picture in Scenarios, with the campaign prefix on request; the error text
// built as a string, where the original's sprintf into outError's buffer overran it for long names)
bool EESMMediaStrings::ListMediaFiles(const UWideString& inFile, std::vector<UWideString>& ioFiles,
                                      UWideString& outError, bool inFlag)
{
    return ee_scenario_en_media_files(this, (void*)&inFile, &ioFiles, &outError, inFlag);
}

// Reads the seven media strings from the saved file; returns the first error (ee_scenario save.rs strings)
long EESMMediaStrings::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_scenario_en_media_io(this, &inFile, false);
}

// Writes the seven media strings to the saved file; returns the first error
long EESMMediaStrings::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_scenario_en_media_io(this, &outFile, true);
}

// the strings in the save's order (ee_scenario entry.rs EE_SCENARIO_EN_MEDIA_STRING)
UWideString EESMMediaStrings::* const EESMMediaStrings::sStrings[7] = {&EESMMediaStrings::mAftermath, &EESMMediaStrings::mHints, &EESMMediaStrings::mHistory, &EESMMediaStrings::mMovieFile, &EESMMediaStrings::mSoundFile, &EESMMediaStrings::mInstructions, &EESMMediaStrings::mMapFile};

