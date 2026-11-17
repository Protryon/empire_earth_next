// The original Error System's debug-output filter (ESFilterDebugOutput and its globals, src/Low-Level Engine/Error
// System/Error System.cpp at 810a3eb2, before it moved to rust/crates/ee_base/src/error), unchanged, for the golden driver
// es_filter_golden.cpp, which supplies UString, FSFileSpec, gStartupVolume and gStartupDirectory.
static std::vector<UString*> gFilters;            // 0x1010f6b8: the lines of Filter.txt (ESFilterDebugOutput)
static long gFilterMode = 1;
static bool gOutputToDebugger;
bool gErrorSystemFiltersInitialized;              // 0x1010f6dc (exported)

// Loads Filter.txt once, then returns whether a debug message passes the filters ('!' always passes)
// Filter.txt lives in the startup directory; "-" / "+" lines set the filter mode, "OutputToDebugger" sets a flag
// nothing reads.
bool ESFilterDebugOutput(const char* inText)
{
    char theLine[1025];                         // 0x404 bytes; fgets is told 1024
    FSFileSpec* theSpec;
    FILE* theFile;
    UString* theFilter;
    bool theResult;
    if (!gErrorSystemFiltersInitialized && gStartupVolume.GetLength() > 0 && gStartupDirectory.GetLength() > 0) {
        theSpec = new FSFileSpec;
        theSpec->SetVolume(gStartupVolume);
        theSpec->SetDirectory(gStartupDirectory);
        theSpec->SetFile(UString("Filter.txt"));
        gFilters.clear();
        if (theSpec->FOpen("r") == kErrorOK) {
            theFile = *theSpec;
            while (fgets(theLine, 1024, theFile) != NULL) {
                unsigned long theLength = strlen(theLine);
                if (theLength <= 0)
                    continue;
                if (theLine[theLength - 1] == '\n')
                    theLine[theLength - 1] = 0;
                theFilter = new UString(theLine);
                if (gFilters.size() == 0) {
                    if (strcmp(theLine, "-") == 0) {
                        gFilterMode = 1;
                        delete theFilter;
                    } else if (strcmp(theLine, "+") == 0) {
                        gFilterMode = 0;
                        delete theFilter;
                    } else if (strcmp(theLine, "OutputToDebugger") == 0) {
                        gOutputToDebugger = true;
                        delete theFilter;
                    } else if (theFilter != NULL)
                        gFilters.push_back(theFilter);
                // Reproduced as found: ';' converts through UString(unsigned long), the capacity
                // constructor, i.e. the filter is compared with an empty string
                } else if (theFilter != NULL && *theFilter != ';')
                    gFilters.push_back(theFilter);
            }
        }
        gErrorSystemFiltersInitialized = true;
        delete theSpec;
    }
    if (inText[0] == '!')
        theResult = true;
    else if (gFilters.size() == 0)
        theResult = gFilterMode == 1;
    else if (gFilterMode == 1) {
        theResult = true;
        for (unsigned long i = 0; i < gFilters.size() && theResult; i++) {
            theFilter = gFilters[i];
            if (strstr(inText, *theFilter))
                theResult = false;
        }
    } else {
        theResult = false;
        for (unsigned long i = 0; i < gFilters.size() && !theResult; i++) {
            theFilter = gFilters[i];
            if (strstr(inText, *theFilter))
                theResult = true;
        }
    }
    return theResult;
}

