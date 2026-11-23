// EEDbFindFile: the game's seam for the db tables (new unit, not in the original game). The database loaders look
// their files up through this instead of FSPath::FindFile. A table is read only in its text form (tools/eedata: the
// .dat's name with .csv or .yaml): it is compiled (the Rust compiler, rust/crates/ee_data) into the table's record
// stream and served as an FSMemoryFile, so the loaders read the records they always read. There is no binary db file
// at run time (RUST_PLAN D9): a table without its text, or one with no text form, stops the game with its name.
// The path's own order applies: a Data\db\<table>.csv earlier on the path wins.
// The text tables reference each other's rows by id: one compiler content serves every table, reading the other
// tables' texts through the same path when a reference needs their ids (each text is parsed once).
// EE_DUMP_CONTENT=<file> (tests): every db table served is appended to <file> as the content dump
// (docs/MODDING_PLAN.md gate G1).
// port (game core lane, round 10): the bodies are Rust (ee_game gamecpp db_text.rs); the functions forward, their C++
// calls are EECore10OpB5's cases.
#include <windows.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatMessage.h"
#include "lle/GEColor.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EEDatabaseTables.h"
#include "ee/EECPBehavior.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EEUnitClassRecord.h"
#include "ee/EERules.h"

#include "EEDataText.h"
#include "EEDataTextSpecs.h"
#include "rust/game/gamecpp.h"

// The generated specs against the record structs the loaders read (the rest are checked by eedata.py check (c)
// against their sources' static_asserts)
static_assert(sizeof(EEWorldTableEntry) == kEEDTSize_EEWorldTableEntry, "dbworld record layout");
static_assert(sizeof(EECPBehavior) == kEEDTSize_EECPBehavior, "dbcpbehavior record layout");
static_assert(sizeof(EETechTreeRecord) == kEEDTSize_EETechTreeRecord, "dbtechtree record layout");
static_assert(sizeof(EEUnitClassRecord) == kEEDTSize_EEUnitClassRecord, "dbobjects record layout");

long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    return ee_gamecpp_db_find_file(inPath, (const char*)inName, (void**)&outFile);
}

void EELoadRules(FSPath* inPath)
{
    ee_gamecpp_db_load_rules(inPath);
}

void EEDumpRules()
{
    ee_gamecpp_db_dump_rules();
}

// ------------------------------------------------------------------------- round 10, lane B: the text tables' C++

int64_t EECore10OpB6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechCapture.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define XSPEC ((FSFileSpec*)p)

// the text tables' C++ calls (gamecpp db_text.rs; p the path, the file or the id map): one statement each
int64_t EECore10OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DxFindFile: return ((FSPath*)p)->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::DxOpen: return XSPEC->Open(kFileAccessRead);
    case O::DxLength: return XSPEC->GetLength(*(unsigned long*)a[0]);
    case O::DxClose: XSPEC->Close();
        break;
    case O::DxDelete: delete XSPEC;
        break;
    case O::DxMessage: PlatMessageShow((const char*)a[0], (const char*)a[1], PLAT_MESSAGE_ERROR);
        break;
    case O::DxVerify: ESVerify(__FILE__, __LINE__, (const char*)a[0]);
        break;
    case O::DxMemoryFile: return P64(static_cast<FSFileSpec*>(new FSMemoryFile((unsigned char*)memcpy(new unsigned char[a[1] != 0 ? a[1] : 1], (const void*)a[0], (size_t)a[1]), (unsigned long)a[1], 1000, true)));
    case O::DxRulesHandle: return P64(const_cast<EeDataRules*>(EEGetRules().RustRules()));
    case O::DxRulesRefresh: EEGetRules().Refresh();
        break;
    default: return EECore10OpB6(inOp, p, a, f);
    }
    return 0;
}
