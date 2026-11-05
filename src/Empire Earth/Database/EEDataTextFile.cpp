// EEDbFindFile: the game's seam for the text db tables (new unit, not in the original game). The database loaders
// look their files up through this instead of FSPath::FindFile. When a table's text form (tools/eedata: the .dat's
// name with .csv or .yaml) is on the path, it is compiled (EEDataText.cpp) into the binary db file and served as an
// FSMemoryFile, so the loaders read the same record stream as from the .dat; otherwise the .dat is found as before.
// The path's own order applies: a loose Data\db\<table>.csv wins over the archive. When both the text and a .dat
// are loose, the text wins.
#include <windows.h>
#include <set>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EEDatabaseTables.h"
#include "ee/EECPBehavior.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EEUnitClassRecord.h"

#include "EEDataText.h"
#include "EEDataTextSpecs.h"

// The generated specs against the record structs the loaders read (the rest are checked by eedata.py check (c)
// against their sources' static_asserts)
static_assert(sizeof(EEWorldTableEntry) == kEEDTSize_EEWorldTableEntry, "dbworld record layout");
static_assert(sizeof(EECPBehavior) == kEEDTSize_EECPBehavior, "dbcpbehavior record layout");
static_assert(sizeof(EETechTreeRecord) == kEEDTSize_EETechTreeRecord, "dbtechtree record layout");
static_assert(sizeof(EEUnitClassRecord) == kEEDTSize_EEUnitClassRecord, "dbobjects record layout");

// Reads a whole file -> its bytes; false when it cannot be read
static bool EEReadWholeFile(FSFileSpec& inFile, std::string& outData)
{
    if (inFile.Open(kFileAccessRead) != kErrorOK)
        return false;
    unsigned long theLength = 0;
    bool theOK = inFile.GetLength(theLength) == kErrorOK;
    if (theOK) {
        outData.resize(theLength);
        unsigned long theRead = 0;
        theOK = theLength == 0 ||
                (inFile.Read(reinterpret_cast<unsigned char*>(&outData[0]), theLength, theRead) == kErrorOK &&
                 theRead == theLength);
    }
    inFile.Close();
    return theOK;
}

long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    // "db\dbworld.dat" -> directory "db\", table "dbworld"
    std::string theName = (const char*)inName;
    size_t theSlash = theName.find_last_of("\\/");
    std::string theDirectory = theSlash == std::string::npos ? std::string() : theName.substr(0, theSlash + 1);
    std::string theStem = theName.substr(theDirectory.size());
    if (theStem.size() > 4 && _stricmp(theStem.c_str() + theStem.size() - 4, ".dat") == 0) {
        theStem.resize(theStem.size() - 4);
        const char* theExtension = EEDataText::TextExtension(theStem.c_str());
        FSFileSpec* theText = NULL;
        if (theExtension != NULL &&
            inPath->FindFile(UString((theDirectory + theStem + theExtension).c_str()), theText) == kErrorOK &&
            theText != NULL) {
            std::string theSource;
            bool theRead = EEReadWholeFile(*theText, theSource);
            delete theText;
            std::vector<unsigned char> theData;
            std::string theError;
            if (!theRead)
                theError = theDirectory + theStem + theExtension + ": cannot be read";
            else if (!EEDataText::Compile(theStem.c_str(), theSource.data(), (unsigned long)theSource.size(),
                                          theData, theError))
                theError = theDirectory + theStem + theExtension + ": " + theError;
            if (!theError.empty()) {
                // a broken text table stops the game, with the place of the problem
                MessageBoxA(NULL, theError.c_str(), "Empire Earth: db text table", MB_OK | MB_ICONERROR);
                ESVerify(__FILE__, __LINE__, theError.c_str());
                outFile = NULL;
                return kErrorFailed;
            }
            unsigned char* theBuffer = new unsigned char[theData.size() ? theData.size() : 1];
            if (!theData.empty())
                memcpy(theBuffer, &theData[0], theData.size());
            outFile = new FSMemoryFile(theBuffer, (unsigned long)theData.size(), 1000, true);   // owns theBuffer
            return kErrorOK;
        }
    }
    return inPath->FindFile(inName, outFile);
}
