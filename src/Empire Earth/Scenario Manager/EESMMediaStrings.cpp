// C:\EEX\Empire Earth\Scenario Manager\EESMMediaStrings.cpp (__FILE__ 0x4c5944, used by 0x1c5828's ESVERIFYs).
// 0x1c5783..0x1c5f73: the seven media/briefing strings of a scenario (the trigger text file's sections, Export
// 0x1c5d2e, Import 0x1c5dfe and Validate 0x1c5f20, are ee_scenario text.rs). Unit flags: the exe
// module set. EESMEffect's COMDAT tail (the EESMUnitPriorityLess sort family) ends at 0x1c5783.
#include <windows.h>
#include <stdio.h>
#include <mbstring.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioHost.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// FUNCTION: EXE 0x5c5783
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

// FUNCTION: EXE 0x5c5828
// Finds the movie (0), sound (1: .wav, then .mp3) or map picture (2) in the data path, with the campaign prefix on
// request; a missing file is reported in outError and returns false
bool EESMMediaStrings::GetMediaFile(const UWideString& inScenarioName, unsigned long inKind, UWideString& outFile,
                                    UWideString& outError, bool inCampaignPrefix)
{
    std::vector<UWideString> theTokens;
    UWideString thePrefixedPath1;
    UWideString thePrefixedPath2;
    UWideString theName;
    UWideString thePath1;
    UWideString thePath2;
    UWideString theNoneString;
    UWideString theFile1;
    UWideString theFile2;
    UWideString theScenario;
    bool theResult = true;
    switch (inKind) {
    case 0:
        theName = this->mMovieFile;
        thePath1 = "Movies\\";
        thePath2 = "Movies\\";
        thePrefixedPath1 = "Movies\\";
        thePrefixedPath2 = "Movies\\";
        theFile1 = theName + ".bik";
        theFile2 = theName + ".bik";
        thePath1 += theFile1;
        thePath2 += theFile2;
        thePrefixedPath1 += gU52A280 + theFile1;
        thePrefixedPath2 += gU52A280 + theFile2;
        break;
    case 1:
        theName = this->mSoundFile;
        thePath1 = "Sounds\\";
        thePath2 = "Sounds\\";
        thePrefixedPath1 = "Sounds\\";
        thePrefixedPath2 = "Sounds\\";
        theFile1 = theName + ".wav";
        theFile2 = theName + ".mp3";
        thePath1 += theFile1;
        thePath2 += theFile2;
        thePrefixedPath1 += gU52A280 + theFile1;
        thePrefixedPath2 += gU52A280 + theFile2;
        break;
    case 2:
        theName = this->mMapFile;
        thePath1 = "Scenarios\\";
        thePath2 = "Scenarios\\";
        thePrefixedPath1 = "Scenarios\\";
        thePrefixedPath2 = "Scenarios\\";
        theFile1 = theName + ".jpg";
        theFile2 = theName + ".jpg";
        thePath1 += theFile1;
        thePath2 += theFile2;
        thePrefixedPath1 += gU52A280 + theFile1;
        thePrefixedPath2 += gU52A280 + theFile2;
        break;
    }
    gResource->GetString(IDS_SE_MAP_NONE, theNoneString);
    if (theName != theNoneString && theName != gEmptyString) {
        if (gDataPath->FindFile(thePath1) == 0) {
            outFile = inCampaignPrefix ? thePrefixedPath1 : thePath1;
        } else if (gDataPath->FindFile(thePath2) == 0) {
            outFile = inCampaignPrefix ? thePrefixedPath2 : thePath2;
        } else {
            ESVERIFY_AT(inScenarioName.Tokenize("/.\\", theTokens), 181);
            ESVERIFY_AT(theTokens.size() == 3, 182);
            theScenario = theTokens[1];
            // Build the message as a string: sprintf into outError's own buffer overran it for long file
            // names (and wrote into the shared empty string when outError was empty).
            outError = theFile1 + "\n";
            theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5c5e78
// Reads the seven media strings from the saved file; returns the first error (ee_scenario save.rs strings)
long EESMMediaStrings::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mAftermath, &this->mHints, &this->mHistory, &this->mMovieFile,
                                 &this->mSoundFile, &this->mInstructions, &this->mMapFile};
    EESMStream theStream = {&inFile, theStrings, NULL, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_strings_io(&theTable, 7, false);
}

// FUNCTION: EXE 0x5c5ecc
// Writes the seven media strings to the saved file; returns the first error
long EESMMediaStrings::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mAftermath, &this->mHints, &this->mHistory, &this->mMovieFile,
                                 &this->mSoundFile, &this->mInstructions, &this->mMapFile};
    EESMStream theStream = {&outFile, theStrings, NULL, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_strings_io(&theTable, 7, true);
}

