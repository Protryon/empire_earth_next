// New (port): the data path's file lookup of EESaveLoad.cpp (0x14ce98), in a unit of its own. The UI's backgrounds,
// error box and scenario intro find their files through it: in EESaveLoad.cpp, they sat in the C++ dependency
// cycle with the saving and loading (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts"). It names only the data
// path and the file system.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EmpireEarth.h"

// Finds a save file by name in a directory on the data path; returns the find error
long EEOpenSaveGameFile(const UString& inName, const UString& inDirectory, FSFileSpec*& outFile)
{
    UString thePath = inDirectory + inName;
    return gDataPath->FindFile(thePath, outFile);
}
