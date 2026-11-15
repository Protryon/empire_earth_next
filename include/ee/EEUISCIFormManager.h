// EEUISCIFormManager (RECONSTRUCTION; file name inferred; provisional field names). The scenario
// info form's (0x13) manager: RTTI EEUISCIFormManager > UIFormManager, vtable 0x446a34, sizeof 0xa8. Get() 0x1e7270
// creates it on first use (EESMMediaStrings at +0x04; the inline ctor calls Clear 0x1e71cc). Its members are defined
// at the top of EEUIEScenarioIntroEvents.cpp (0x1e71cc..0x1e76c1; the ??_G 0x1e83cb closes that unit). The four
// files are FSFileSpec* (gDataPath->FindFile's out type; Get* copy them through FSFileSpec::operator=). Member names
// are provisional. Requires lle/UIFormManager.h, ee/EESMMediaStrings.h.
#ifndef EE_EEUISCIFORMMANAGER_H
#define EE_EEUISCIFORMMANAGER_H

class FSFileSpec;

class EEUISCIFormManager : public UIFormManager {
public:
    EEUISCIFormManager() { this->Clear(); }  // inline (Get: UIFormManager's vptr, the media strings, then Clear)
    virtual ~EEUISCIFormManager() { this->DeleteObjects(); }  // inline (the ??_G 0x1e83cb expands it)

    void Clear();                           // 0x1e71cc
    void DeleteObjects();                   // 0x1e7205: deletes the four files (the post-game Open calls it)
    static EEUISCIFormManager* Get();       // 0x1e7270
    UWideString GetBriefing();              // 0x1e72cf (mMediaStrings.mInstructions)
    bool OpenMap();                         // 0x1e72e4 (mMapFile + ".jpg", else ".png", in "Scenarios\")
    bool OpenMovie();                       // 0x1e73bd (mMovieFile + ".bik" in "Movies\")
    bool OpenScenario();                    // 0x1e7444 (mFile's own name, in "Scenarios\" or the data path)
    bool OpenSound();                       // 0x1e74dc (mSoundFile + ".mp3" in "Sounds\")
    UWideString GetHistory();               // 0x1e7563 (mHistory)
    UWideString GetHints();                 // 0x1e7578 (mW14)
    bool GetMap(FSFileSpec& outSpec);       // 0x1e758d
    bool GetMovie(FSFileSpec& outSpec);     // 0x1e75b5
    bool GetScenario(FSFileSpec& outSpec);  // 0x1e75dd
    bool GetSound(FSFileSpec& outSpec);     // 0x1e7605
    bool HasMedia();                        // 0x1e762d (0x27a56f)
    void SetBlinking(bool inBlinking);      // 0x1e766a (the main game form's info button; 0x1be512, 0x26615a)
    // 0x1d2ae5 (out of line COMDAT; the SCL/SPL load events call it): owns the opened scenario file
    void SetFile(FSFileSpec* inFile) { delete this->mFile; this->mFile = inFile; }

    // inline setters (SCLE LoadFile's inline budget; provisional names)
    void SetMediaStrings(const EESMMediaStrings& inStrings) { this->mMediaStrings = inStrings; }
    void SetIsCampaign(bool inFlag) { this->mIsCampaign = inFlag; }

    EESMMediaStrings mMediaStrings;         // +0x04 (sizeof 0x8c)
    FSFileSpec* mMap;                       // +0x90 (OpenMap)
    FSFileSpec* mMovie;                     // +0x94 (OpenMovie)
    FSFileSpec* mFile;                      // +0x98 (SetFile; OpenScenario)
    FSFileSpec* mSound;                     // +0x9c (OpenSound)
    bool mBA0;                              // +0xa0 (Clear)
    bool mIsInGame;                         // +0xa1 (the in-game LoadScenario clears it; SIE Back closes the form)
    bool mIsLoadingFromGame;                // +0xa2 (and sets this one; SIE Start posts the saved game)
    bool mIsCampaign;                       // +0xa3 (the CSSE StartScenario sets it: a campaign scenario)
    bool mScrollBriefingToEnd;              // +0xa4 (SIE Open sets it, Briefing scrolls to the end and clears it)
};

#endif
