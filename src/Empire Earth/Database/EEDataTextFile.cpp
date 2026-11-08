// EEDbFindFile: the game's seam for the text db tables (new unit, not in the original game). The database loaders
// look their files up through this instead of FSPath::FindFile. When a table's text form (tools/eedata: the .dat's
// name with .csv or .yaml) is on the path, it is compiled (EEDataText.cpp) into the binary db file and served as an
// FSMemoryFile, so the loaders read the same record stream as from the .dat; otherwise the .dat is found as before.
// The path's own order applies: a loose Data\db\<table>.csv wins over the archive. When both the text and a .dat
// are loose, the text wins.
// The text tables reference each other's rows by id: one EEDataText::Content serves every table, reading the other
// tables' texts through the same path when a reference needs their ids (each text is parsed once).
// EE_DUMP_CONTENT=<file> (tests): every db table served, text or .dat, is appended to <file> as the content dump
// (EEDataDump.cpp; docs/MODDING_PLAN.md gate G1).
#include <windows.h>
#include <set>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatMessage.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EEDatabaseTables.h"
#include "ee/EECPBehavior.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EEUnitClassRecord.h"

#include "EEDataDump.h"
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

// the path and folder the text tables are read from (the path of the current lookup)
static FSPath* sTextPath = NULL;
static std::string sTextDirectory;

static bool EEReadTextTable(const char* inTable, std::string& outText)
{
    const char* theExtension = EEDataText::TextExtension(inTable);
    FSFileSpec* theText = NULL;
    if (theExtension == NULL || sTextPath == NULL ||
        sTextPath->FindFile(UString((sTextDirectory + inTable + theExtension).c_str()), theText) != kErrorOK ||
        theText == NULL)
        return false;
    bool theRead = EEReadWholeFile(*theText, outText);
    delete theText;
    return theRead;
}

static EEDataText::Content& EETextContent()
{
    static EEDataText::Content sContent(EEReadTextTable);
    return sContent;
}

// EE_DUMP_CONTENT: appends a served table's content dump
static void EEDumpContent(const std::string& inTable, const unsigned char* inData, size_t inSize)
{
    const char* theFile = getenv("EE_DUMP_CONTENT");
    if (theFile == NULL || *theFile == 0)
        return;
    std::string theJson, theError;
    if (!EEDataText::DumpTable(inTable.c_str(), inData, inSize, theJson, theError))
        theJson = "\"" + inTable + "\":\"" + theError + "\"";
    FILE* f = fopen(theFile, "ab");
    if (f) {
        fprintf(f, "%s\n", theJson.c_str());
        fclose(f);
    }
}

// the row ids of the tables loaded from text, by table (EEDbRowIds)
static std::map<long, std::string>& EERowIdsOf(const std::string& inTable)
{
    static std::map<std::string, std::map<long, std::string> > sIds;
    return sIds[inTable];
}

bool EEDbRowIds(const char* inTable, std::map<long, std::string>& outIds)
{
    outIds = EERowIdsOf(inTable);
    return !outIds.empty();
}

long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    // "db\dbworld.dat" -> directory "db\", table "dbworld"
    std::string theName = (const char*)inName;
    size_t theSlash = theName.find_last_of("\\/");
    std::string theDirectory = theSlash == std::string::npos ? std::string() : theName.substr(0, theSlash + 1);
    std::string theStem = theName.substr(theDirectory.size());
    if (theStem.size() > 4 && PlatStrICmp(theStem.c_str() + theStem.size() - 4, ".dat") == 0) {
        theStem.resize(theStem.size() - 4);
        for (char& c : theStem)
            c = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);   // the loaders spell some names in mixed case
        const char* theExtension = EEDataText::TextExtension(theStem.c_str());
        FSFileSpec* theText = NULL;
        if (theExtension != NULL &&
            inPath->FindFile(UString((theDirectory + theStem + theExtension).c_str()), theText) == kErrorOK &&
            theText != NULL) {
            delete theText;
            sTextPath = inPath;
            sTextDirectory = theDirectory;
            std::vector<unsigned char> theData;
            std::string theError;
            if (!EETextContent().Compile(theStem.c_str(), theData, theError))
                theError = theDirectory + theStem + theExtension + ": " + theError;
            if (!theError.empty()) {
                // a broken text table stops the game, with the place of the problem
                PlatMessageShow("Empire Earth: db text table", theError.c_str(), PLAT_MESSAGE_ERROR);
                ESVerify(__FILE__, __LINE__, theError.c_str());
                outFile = NULL;
                return kErrorFailed;
            }
            if (theStem == "dbobjects" || theStem == "dbtechtree") {   // the ids the computer player's scripts name
                std::string theIgnored;
                EETextContent().Ids(theStem.c_str(), EERowIdsOf(theStem), theIgnored);
            }
            unsigned char* theBuffer = new unsigned char[theData.size() ? theData.size() : 1];
            if (!theData.empty())
                memcpy(theBuffer, &theData[0], theData.size());
            EEDumpContent(theStem, theData.empty() ? NULL : &theData[0], theData.size());
            outFile = new FSMemoryFile(theBuffer, (unsigned long)theData.size(), 1000, true);   // owns theBuffer
            return kErrorOK;
        }
        if (getenv("EE_DUMP_CONTENT") != NULL && theExtension != NULL) {   // the binary load, for the dump
            FSFileSpec* theDat = NULL;
            std::string theBytes;
            if (inPath->FindFile(inName, theDat) == kErrorOK && theDat != NULL && EEReadWholeFile(*theDat, theBytes))
                EEDumpContent(theStem, reinterpret_cast<const unsigned char*>(theBytes.data()), theBytes.size());
            delete theDat;
        }
    }
    return inPath->FindFile(inName, outFile);
}

#include "ee/EERules.h"

void EELoadRules(FSPath* inPath)
{
    // the rules documents sit next to the db tables (db\combat.yaml ...); their references read the tables' texts
    // through the same path
    sTextPath = inPath;
    sTextDirectory = "db\\";
    std::string theError;
    if (!EEGetRules().Load(EETextContent(), theError)) {
        theError = "db\\" + theError;
        PlatMessageShow("Empire Earth: db rules", theError.c_str(), PLAT_MESSAGE_ERROR);
        ESVerify(__FILE__, __LINE__, theError.c_str());
    }
}

void EEDumpRules()
{
    const char* theFile = getenv("EE_DUMP_CONTENT");
    if (theFile == NULL || *theFile == 0)
        return;
    std::string theJson;
    EEGetRules().Dump(theJson);
    FILE* f = fopen(theFile, "ab");
    if (f) {
        fwrite(theJson.data(), 1, theJson.size(), f);
        fclose(f);
    }
}
