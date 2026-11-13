// C:\EEX\Low-Level Engine\File System\File System.cpp (name by analogy with Error System.cpp; it sorts before
// FSArchive.cpp case-insensitively). It is the first unit to include the File System headers, so it emits their
// inline members (0x4020..0x4789) in include order, then its globals, FSGetVolumeAndDirectory and FSInitialize.
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

// Not the umbrella: the inline block holds UDataCompression, FSArchive, FSChunk, UReferenceCounter,
// FSPersistent, TSLockable, FSPersistentList and FSMemoryFile, in that order.
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/TSScheduler.h"
#include "lle/UDataCompression.h"
#include "lle/FSArchive.h"
#include "lle/FSRestorable.h"
#include "lle/FSChunk.h"
#include "lle/FSPath.h"
#include "lle/FSPersistent.h"
#include "lle/TSReadWriteLock.h"
#include "TSLockable.h"
#include "lle/FSPersistentList.h"
#include "lle/FSMemoryFile.h"
#include "lle/File System.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1000422a
// SYNTHETIC: LLE 0x10004311
// SYNTHETIC: LLE 0x10004724

UString gStartupVolume(256);
UString gStartupDirectory(256);

// FUNCTION: LLE 0x100047df
// Splits a path into its volume (drive or UNC server share) and its directory (with a trailing backslash) into outSpec
void FSGetVolumeAndDirectory(const UString& inPath, FSFileSpec& outSpec)
{
    UString theVolume(256);
    UString theDirectory(256);
    theVolume = inPath.GetSubstring(0, 2);
    if (theVolume != "\\\\")
        theDirectory = inPath.GetSubstring(2, inPath.GetLength() - 2);
    else {
        unsigned long theOffset = inPath.Find("\\", 2);
        theVolume = inPath.GetSubstring(0, theOffset);
        theDirectory = inPath.GetSubstring(theOffset, inPath.GetLength() - theOffset);
    }
    theDirectory += "\\";
    outSpec.SetVolume(theVolume);
    outSpec.SetDirectory(theDirectory);
}

// FUNCTION: LLE 0x100048e7
// Records the startup volume and directory (upper case) from the path, or from the current directory when the path
// is empty or not a directory
void FSInitialize(const UString& inPath)
{
    char theCurrentDirectory[PLAT_MAX_PATH] = {0};
    FSFileSpec theSpec;
    UString thePath(PLAT_MAX_PATH);
    bool theFound = false;
    if (inPath.GetLength() > 0) {
        FSGetVolumeAndDirectory(inPath, theSpec);
        theFound = theSpec.IsDirectory();
    }
    if (!theFound) {
        PlatGetCurrentDirectory(theCurrentDirectory, PLAT_MAX_PATH);
        thePath = theCurrentDirectory;
        FSGetVolumeAndDirectory(thePath, theSpec);
    }
    gStartupVolume = theSpec.GetVolume();
    gStartupDirectory = theSpec.GetDirectory();
    gStartupVolume.ToUpper();
    gStartupDirectory.ToUpper();
}

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x10004020
// SYNTHETIC: LLE 0x1000403b
// SYNTHETIC: LLE 0x10004095
// SYNTHETIC: LLE 0x100040cb
// SYNTHETIC: LLE 0x100040cf
// SYNTHETIC: LLE 0x1000413e
// SYNTHETIC: LLE 0x10004199
// SYNTHETIC: LLE 0x1000419e
// SYNTHETIC: LLE 0x100041a7
// SYNTHETIC: LLE 0x100041b2
// SYNTHETIC: LLE 0x100041f1
// SYNTHETIC: LLE 0x10004285
// SYNTHETIC: LLE 0x10004299
// SYNTHETIC: LLE 0x100042d8
// SYNTHETIC: LLE 0x1000436c
// SYNTHETIC: LLE 0x10004370
// SYNTHETIC: LLE 0x10004380
// SYNTHETIC: LLE 0x10004384
// SYNTHETIC: LLE 0x10004394
// SYNTHETIC: LLE 0x100043b4
// SYNTHETIC: LLE 0x100043b9
// SYNTHETIC: LLE 0x100043ce
// SYNTHETIC: LLE 0x100043dd
// SYNTHETIC: LLE 0x100043f3
// SYNTHETIC: LLE 0x10004409
// SYNTHETIC: LLE 0x1000441b
// SYNTHETIC: LLE 0x10004422
// SYNTHETIC: LLE 0x1000442b
// SYNTHETIC: LLE 0x10004446
// SYNTHETIC: LLE 0x10004495
// SYNTHETIC: LLE 0x10004499
// SYNTHETIC: LLE 0x100044c8
// SYNTHETIC: LLE 0x100044d8
// SYNTHETIC: LLE 0x100044e0
// SYNTHETIC: LLE 0x100044e8
// SYNTHETIC: LLE 0x100044f0
// SYNTHETIC: LLE 0x100044f8
// SYNTHETIC: LLE 0x1000450d
// SYNTHETIC: LLE 0x10004574
// SYNTHETIC: LLE 0x100045b7
// SYNTHETIC: LLE 0x100045d6
// SYNTHETIC: LLE 0x10004630
// SYNTHETIC: LLE 0x10004634
// SYNTHETIC: LLE 0x10004644
// SYNTHETIC: LLE 0x10004652
// SYNTHETIC: LLE 0x10004656
// SYNTHETIC: LLE 0x1000466d
// SYNTHETIC: LLE 0x100046c4
// SYNTHETIC: LLE 0x10004715
// TEMPLATE: LLE 0x100049ec
// TEMPLATE: LLE 0x10004a4c
// TEMPLATE: LLE 0x10004a81
// TEMPLATE: LLE 0x10004ae1
// TEMPLATE: LLE 0x10004c6a
// TEMPLATE: LLE 0x10004da4
// TEMPLATE: LLE 0x10004e08

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x1000477f
// SYNTHETIC: LLE 0x10004789
// SYNTHETIC: LLE 0x10004799
// SYNTHETIC: LLE 0x100047a5
// SYNTHETIC: LLE 0x100047af
// SYNTHETIC: LLE 0x100047b9
// SYNTHETIC: LLE 0x100047c9
// SYNTHETIC: LLE 0x100047d5
// TEMPLATE: LLE 0x100049b8
// TEMPLATE: LLE 0x10004b16
// TEMPLATE: LLE 0x10004b87
// SYNTHETIC: LLE 0x10004be7
// TEMPLATE: LLE 0x10004ced
// TEMPLATE: LLE 0x10004d72
// SYNTHETIC: LLE 0x10004de8
