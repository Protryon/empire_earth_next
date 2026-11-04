// C:\EEX\Empire Earth\Scenario Manager\EESMMediaStrings.cpp (__FILE__ 0x4c5944, used by 0x1c5828's ESVERIFYs).
// 0x1c5783..0x1c5f73: the seven media/briefing strings of a scenario. Unit flags: the exe
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
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")

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
        theFile = gU4E5B98;
        if (this->GetMediaFile(inFile, i, theFile, theError, inFlag)) {
            if (theFile != gU4E5B98) {
                _mbslwr((unsigned char*)(const char*)theFile);
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
    if (theName != theNoneString && theName != gU4E5B98) {
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

// FUNCTION: EXE 0x5c5d2e
// Writes the aftermath, hints, instructions and history texts as {tag} sections; returns 0, or 9 on a write error
long EESMMediaStrings::Export(FSFileSpec& inFile)
{
    int theResult = fprintf(inFile, "{%s}\n", "1_After Math");
    if (theResult >= 0) {
        theResult = fprintf(inFile, "%s\n\n", (const char*)this->mAftermath);
        if (theResult >= 0) {
            theResult = fprintf(inFile, "{%s}\n", "0_Hints");
            if (theResult >= 0) {
                theResult = fprintf(inFile, "%s\n\n", (const char*)this->mHints);
                if (theResult >= 0) {
                    theResult = fprintf(inFile, "{%s}\n", "3_Instructions");
                    if (theResult >= 0) {
                        theResult = fprintf(inFile, "%s\n\n", (const char*)this->mInstructions);
                        if (theResult >= 0) {
                            theResult = fprintf(inFile, "{%s}\n", "2_History");
                            if (theResult >= 0) {
                                theResult = fprintf(inFile, "%s\n\n", (const char*)this->mHistory);
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult >= 0 ? 0 : 9;
}

// FUNCTION: EXE 0x5c5dfe
// Stores an imported text section by its tag (history, instructions, hints, aftermath); returns 3 for an unknown tag
long EESMMediaStrings::Import(const UString& inLine, unsigned long inKind, const UWideString& inText)
{
    long theResult;
    if (inLine == "2_History") {
        this->mHistory = inText;
        theResult = 0;
    } else if (inLine == "3_Instructions") {
        this->mInstructions = inText;
        theResult = 0;
    } else if (inLine == "0_Hints") {
        this->mHints = inText;
        theResult = 0;
    } else if (inLine == "1_After Math") {
        this->mAftermath = inText;
        theResult = 0;
    } else
        theResult = 3;
    return theResult;
}

// FUNCTION: EXE 0x5c5e78
// Reads the seven media strings from the saved file; returns the first error
long EESMMediaStrings::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mAftermath.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mHints.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mHistory.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mMovieFile.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mSoundFile.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mInstructions.Load(inFile);
    if (theResult == 0) {
        theResult = this->mMapFile.Load(inFile);
    }
    return theResult;
}

// FUNCTION: EXE 0x5c5ecc
// Writes the seven media strings to the saved file; returns the first error
long EESMMediaStrings::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mAftermath.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mHints.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mHistory.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mMovieFile.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mSoundFile.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mInstructions.Save(outFile);
    if (theResult == 0) {
        theResult = this->mMapFile.Save(outFile);
    }
    return theResult;
}

// FUNCTION: EXE 0x5c5f20
// Returns 0 when the line is one of the four text section tags, else 3
long EESMMediaStrings::Validate(const UString& inLine, unsigned long inKind)
{
    bool theKnown = inLine == "2_History" || inLine == "3_Instructions" || inLine == "0_Hints";
    if (theKnown)
        return 0;
    return inLine == "1_After Math" ? 0 : 3;
}
