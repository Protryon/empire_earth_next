// C:\EEX\Empire Earth\Persistent\EESaveLoad.h (RECONSTRUCTION; file name inferred). The free functions
// of EESaveLoad.cpp (0x14c6b1..0x14e839) that other units call, and its global error message. Names are provisional
// (by behaviour). Requires lle/UString.h, lle/UWideString.h.
#ifndef EE_EESAVELOAD_H
#define EE_EESAVELOAD_H

// the last save/load error message (0x531c70; defined in EESaveLoad.cpp)
extern UWideString gLoadSaveErrorMessage;

// the resource string ids of the save/load errors (names from the ESVERIFY text 0x4c1b94)
const long keeslSaveGameInvalid = 0x1bf1;
const long keeCouldntOpenFile = 0x1bf0;     // ESVERIFY text 0x4bfa44 (EEGameState::Create)

// 0x14d066: saves the game to inFileName (an FSFileSpec built from it), inTime = TMGetTime(), inDescription by value;
// returns 0 on success (EEASaveGame::Virtual4, EEMLoadOrSaveGame 0xc10f1; provisional name)
long SaveGame(const UString& inFileName, unsigned long inTime, bool inFlag, UString inDescription);
// 0x14c741: the save file name for inName (EEAQuickSaveLoad::Virtual4) (provisional name; types inferred)
UString GetSaveTextName(const UString& inName, bool inFlag);
// 0x14cbc7: keeps the inMax first files matching inPattern in inDirectory and deletes the rest (EEMLoadOrSaveGame
// FnC1052's auto-saves) (provisional name; one prototype)
long TrimAutoSaves(const UString& inPattern, const UString& inDirectory, unsigned long inMax);
UString GetDateText();                  // 0x14ced5 (asctime of now, or "Date/time failed."; provisional name)
// 0x14ce06: writes the save header and the game into inFile (EEAQuickSaveLoad::Virtual4 passes its memory file
// twice) (provisional name; types inferred)
long WriteSaveGame(const UString& inName, unsigned long inTime, FSFile& inFile, FSFile& inOther);
// 0x14cf33: loads the game from inFile (EEAQuickSaveLoad::Virtual4) (provisional name; types inferred)
long LoadSavedGameFile(FSFileSpec& inFile, bool inArg2, bool inQuiet, bool inResumeGraphics);

#endif
