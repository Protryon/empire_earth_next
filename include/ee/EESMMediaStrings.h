// C:\EEX\Empire Earth\Scenario Manager\EESMMediaStrings.h (RECONSTRUCTION; provisional name). Seven wide
// strings of a scenario (EESMMediaStrings.cpp, __FILE__ 0x4c5944; Load 0x1c5e78 and Save 0x1c5ecc take the save
// version and the file). sizeof 0x8c: EEPSaveGameHeader +0x438..+0x4c4. Its implicit default ctor is an out-of-line
// COMDAT (0x14bd35, emitted after EEPSaveGameHeader's ctor, its first user): seven UWideString default ctors in
// member order. Member names are provisional (by offset).
// Requires lle/LowLevelEngine.h.
#ifndef EE_EESMMEDIASTRINGS_H
#define EE_EESMMEDIASTRINGS_H

class FSFileSpec;

class EESMMediaStrings {
public:
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x1c5e78
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x1c5ecc
    // 0x1c5783 (`ret 0x10`; EECampaign::ListScenarioMedia 0x28cc2e: the scenario file, the media file list, the
    // error text, a flag; returns bool; provisional name)
    bool ListMediaFiles(const UWideString& inFile, std::vector<UWideString>& ioFiles, UWideString& outError, bool inFlag);
    long Import(const UString& inLine, unsigned long inKind, const UWideString& inText);  // 0x1c5dfe (`ret 0xc`; 0: the line was a media string; provisional name)
    long Validate(const UString& inLine, unsigned long inKind);  // 0x1c5f20 (`ret 8`; provisional name)
    long Export(FSFileSpec& inFile);        // 0x1c5d2e (`ret 4`; EEScenarioManager::ExportText; provisional name)

private:
    // 0x1c5828 (`ret 0x14`; ListMediaFiles' per-kind worker: 0 movie, 1 sound, 2 picture; provisional name)
    bool GetMediaFile(const UWideString& inScenarioName, unsigned long inKind, UWideString& outFile,
                      UWideString& outError, bool inCampaignPrefix);

    friend class EEUIFMainGame; // actual briefing-string read at266f03; nominal access placement provisional
    friend struct EEScreensMedia;           // the campaign scenario select's Start (UI Events/EEScreens.cpp) prefixes
                                            // mSoundFile/mMapFile with the campaign's scenario prefix (0x1d2910)
    friend class EEUISCIFormManager;        // reads the strings (OpenMap/OpenMovie/OpenSound, the Get* copies; 0x1e72cf..)
    friend class EESMEffect;                // Execute 0x1be3eb..0x1be4e6 appends to / clears mW14, mHistory, mInstructions
    friend class EESEInstructions;          // the editor's instructions tab: Virtual1C 0x171f8b writes mAftermath..mMapFile, Virtual14 0x171ce3 reads them
    friend void EEDisplayScenarioIntro();   // 0x27a470 prefixes mSoundFile/mMapFile likewise
    UWideString mAftermath;                // +0x00
    UWideString mHints;                     // +0x14
    UWideString mHistory;                   // +0x28
    UWideString mMovieFile;                 // +0x3c
    UWideString mSoundFile;                 // +0x50
    UWideString mInstructions;              // +0x64
    UWideString mMapFile;                   // +0x78
};

#endif
