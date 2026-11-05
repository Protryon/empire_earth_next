// DX7Movie: a GEMovie playing an MPEG-1 movie (pl_mpeg) on its own thread into a system-memory frame buffer, which
// RenderFrame copies to the screen's back buffer. The original played Bink files (binkw32.dll); a movie the game
// names X.bik plays from X.mpg next to it (tools/convert_movies.py makes them). The movie's sound goes through the
// mixer (SEMixer::SetMovieSource).
#ifndef DX7_DX7MOVIE_H
#define DX7_DX7MOVIE_H

#include <vector>

#include "dx7/DX7Common.h"
#include "lle/GERectangle.h"
#include "lle/GEMovie.h"

class TSCriticalSection;
class TSEvent;
class TSThread;
class GESurface;

class DX7_API DX7Movie : public GEMovie {
public:
    // What the playback thread gets (PlayMovie's parameter): the movie and whether it loops
    struct PlayMovieParameter {
        DX7Movie* mMovie;
        bool mIsLooping;
    };

    DX7Movie(const FSFileSpec& inFile, unsigned long inScreen);
    DX7Movie(const FSFileSpec& inFile, unsigned long inScreen, GERectangle inRectangle, bool inLoop);

    virtual UString GetFilename() const { return this->mFilename; }
    virtual bool GetIsLooping() const;
    virtual bool GetIsPlaying() const { return this->mIsPlaying; }
    virtual bool GetIsReady() const { return this->mIsReady; }
    virtual bool GetJustStarted() const { return this->mJustStarted; }
    virtual long RenderFrame();
    virtual void Stop();

    // pl_mpeg's callbacks and the mixer's (the movie thread decodes, the device thread pulls the sound)
    void DecodedFrame(const unsigned char* inBGRA, unsigned long inStride);
    void DecodedSamples(const float* inInterleaved, unsigned long inCount);
    static void MixSound(void* inMovie, float* outFrames, unsigned long inCount);

private:
    virtual ~DX7Movie();
    long CreateFrameBuffer(unsigned long inWidth, unsigned long inHeight);
    void Initialize(const FSFileSpec& inFile, unsigned long inScreen, bool inLoop);
    static unsigned long PlayMovie(void* inParameter);
    bool ShouldStop();

    PlayMovieParameter mPlayMovieParameter;
    TSCriticalSection* mFrameAccess;
    TSThread* mThread;
    TSEvent* mFrameReadyEvent;
    TSEvent* mMovieStopped;
    GESurface* mFrameBuffer;
    GERectangle mSourceRectangle;
    GERectangle mDestinationRectangle;
    UString mFilename;                      // the name the game gave (.bik)
    unsigned long mScreen;
    volatile bool mIsPlaying;
    volatile bool mStopRequested;
    volatile bool mIsFinished;
    volatile bool mIsReady;
    volatile bool mJustStarted;
    bool mHasRectangle;

    // the decoded sound waiting for the mixer: interleaved stereo at the movie's rate, a ring of mSound.size() floats
    TSCriticalSection* mSoundAccess;
    std::vector<float> mSound;
    unsigned long mSoundRead;               // in floats
    unsigned long mSoundCount;              // floats waiting
    double mSoundPosition;                  // the fraction between the next two frames (resampling)
    double mSoundStep;                      // movie frames per device frame

    static TSCriticalSection* sMovieCriticalSection;
};

#endif
