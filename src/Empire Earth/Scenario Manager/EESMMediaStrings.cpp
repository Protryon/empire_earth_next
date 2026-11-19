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
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// Appends the scenario's sound and map picture files (lower case; the movie is not listed) to the list and each
// missing file's error text to outError; returns false if one is missing
bool EESMMediaStrings::ListMediaFiles(const UWideString& inFile, std::vector<UWideString>& ioFiles,
                                      UWideString& outError, bool inFlag)
{
    UWideString theError;
    UWideString theFile;
    bool theResult = true;
    for (unsigned long i = 1; i < 3; i++) {
        theFile = gEmptyString;
        if (this->GetMediaFile(inFile, i, theFile, theError, inFlag)) {
            if (theFile != gEmptyString) {
                PlatMbsLwr((unsigned char*)(const char*)theFile);
                ioFiles.push_back(theFile);
            }
        } else {
            theResult = false;
            outError += theError;
        }
    }
    return theResult;
}

// Finds the movie (0), sound (1: .wav, then .mp3) or map picture (2) in the data path, with the campaign prefix on
// request; a missing file is reported in outError and returns false
bool EESMMediaStrings::GetMediaFile(const UWideString& inScenarioName, unsigned long inKind, UWideString& outFile,
                                    UWideString& outError, bool inCampaignPrefix)
{
    static const char* const kFolders[] = {"Movies\\", "Sounds\\", "Scenarios\\"};
    static const char* const kExtensions[][2] = {{".bik", ".bik"}, {".wav", ".mp3"}, {".jpg", ".jpg"}};
    const UWideString* theNames[] = {&this->mMovieFile, &this->mSoundFile, &this->mMapFile};
    UWideString theNoneString;
    gResource->GetString(IDS_SE_MAP_NONE, theNoneString);
    if (inKind > 2 || *theNames[inKind] == theNoneString || *theNames[inKind] == gEmptyString)
        return true;
    UWideString theFolder = kFolders[inKind];
    UWideString theFiles[2] = {*theNames[inKind] + kExtensions[inKind][0], *theNames[inKind] + kExtensions[inKind][1]};
    for (int i = 0; i < 2; i++) {
        // the first file found in the data path, as found or with the campaign's folder before its name
        UWideString thePath = theFolder + theFiles[i];
        if (gDataPath->FindFile(thePath) == 0) {
            outFile = inCampaignPrefix ? theFolder + (gU52A280 + theFiles[i]) : thePath;
            return true;
        }
    }
    std::vector<UWideString> theTokens;
    ESVERIFY_AT(inScenarioName.Tokenize("/.\\", theTokens), 181);
    ESVERIFY_AT(theTokens.size() == 3, 182);
    // Build the message as a string: sprintf into outError's own buffer overran it for long file names (and wrote
    // into the shared empty string when outError was empty).
    outError = theFiles[0] + "\n";
    return false;
}

// Reads the seven media strings from the saved file; returns the first error (ee_scenario save.rs strings)
long EESMMediaStrings::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mAftermath, &this->mHints, &this->mHistory, &this->mMovieFile,
                                 &this->mSoundFile, &this->mInstructions, &this->mMapFile};
    EESMStream theStream = {&inFile, theStrings, NULL, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_strings_io(&theTable, 7, false);
}

// Writes the seven media strings to the saved file; returns the first error
long EESMMediaStrings::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mAftermath, &this->mHints, &this->mHistory, &this->mMovieFile,
                                 &this->mSoundFile, &this->mInstructions, &this->mMapFile};
    EESMStream theStream = {&outFile, theStrings, NULL, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_strings_io(&theTable, 7, true);
}

