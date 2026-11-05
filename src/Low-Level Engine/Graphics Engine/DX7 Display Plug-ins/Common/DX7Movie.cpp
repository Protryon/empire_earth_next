// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Movie.cpp
// The movie player: MPEG-1 video and MP2 audio through pl_mpeg (src/ThirdParty/pl_mpeg), replacing the original's Bink
// player. A movie plays on its own thread (PlayMovie, one movie at a time), paced by pl_mpeg's clock: every decoded
// frame is converted into a system-memory frame buffer in the back buffer's pixel format, which RenderFrame copies to
// the back buffer; the decoded sound waits in a ring that the mixer's device thread pulls (MixSound), resampled to
// the device rate. The game names movies X.bik; the player reads X.mpg next to it (tools/convert_movies.py).
#include <windows.h>
#include "gx/GX.h"
#include <stdio.h>
#include <string.h>

#include "pl_mpeg/pl_mpeg.h"
#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Movie.h"

TSCriticalSection* DX7Movie::sMovieCriticalSection;

const unsigned long kSoundRingFloats = 2 * 32768;   // about 0.7 s of stereo at 44.1 kHz waiting for the mixer

// Creates a movie shown over the whole back buffer and starts its playback thread
DX7Movie::DX7Movie(const FSFileSpec& inFile, unsigned long inScreen)
{
    if (DX7Movie::sMovieCriticalSection == NULL)
        TSScheduler::CreateCriticalSection(DX7Movie::sMovieCriticalSection);
    this->mHasRectangle = false;
    this->Initialize(inFile, inScreen, false);
}

// Creates a movie drawn into the given rectangle, optionally looping, and starts its playback thread
DX7Movie::DX7Movie(const FSFileSpec& inFile, unsigned long inScreen, GERectangle inRectangle, bool inLoop)
{
    if (DX7Movie::sMovieCriticalSection == NULL)
        TSScheduler::CreateCriticalSection(DX7Movie::sMovieCriticalSection);
    this->mHasRectangle = true;
    this->mDestinationRectangle = inRectangle;
    this->Initialize(inFile, inScreen, inLoop);
}

// Destroys the movie's events, playback thread and locks
DX7Movie::~DX7Movie()
{
    TSScheduler::DestroyEvent(this->mMovieStopped);
    TSScheduler::DestroyEvent(this->mFrameReadyEvent);
    TSScheduler::DestroyThread(this->mThread);
    TSScheduler::DestroyCriticalSection(this->mFrameAccess);
    TSScheduler::DestroyCriticalSection(this->mSoundAccess);
}

// Creates the system-memory frame buffer at the movie's size in the back buffer's pixel format; on success sets the
// source rectangle (the destination defaults to the movie's size)
long DX7Movie::CreateFrameBuffer(unsigned long inWidth, unsigned long inHeight)
{
    GESurfaceDescription theDescription;
    GEScreen* theScreen = GERasterizer::GetActiveRasterizer()->GetScreen(this->mScreen);
    GESurface* theTarget = GERasterizer::GetActiveRasterizer()->GetSecondarySurface(this->mScreen);
    theDescription.mType = kSurfaceSystemMemory;
    theDescription.mPixelFormat = theTarget->GetPixelFormat();
    theDescription.mPixelDepth = theTarget->GetPixelDepth();
    theDescription.mSurfaceVolatility = 0;
    theDescription.mWidth = inWidth;
    theDescription.mHeight = inHeight;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    if (!this->mHasRectangle)
        this->mDestinationRectangle = GERectangle(0.0f, 0.0f, (float)theDescription.mWidth, (float)theDescription.mHeight);
    long theResult = theScreen->CreateSurface(theDescription, &this->mFrameBuffer);
    if (theResult == kErrorOK) {
        this->mSourceRectangle.SetLeft(0.0f);
        this->mSourceRectangle.SetTop(0.0f);
        this->mSourceRectangle.SetRight((float)(long)inWidth);
        this->mSourceRectangle.SetBottom((float)(long)inHeight);
    }
    return theResult;
}

// Returns whether the movie loops
bool DX7Movie::GetIsLooping() const
{
    return this->mPlayMovieParameter.mIsLooping;
}

// Sets up a movie: builds the file name, takes a reference, creates the locks and events, then the playback thread.
// (The original reset the rectangle flag here, so a movie created with a rectangle ignored it, and started the thread
// before creating the events it uses.)
void DX7Movie::Initialize(const FSFileSpec& inFile, unsigned long inScreen, bool inLoop)
{
    this->mFrameBuffer = NULL;
    this->mIsPlaying = false;
    this->mStopRequested = false;
    this->mIsFinished = false;
    this->mIsReady = false;
    this->mScreen = inScreen;
    this->mPlayMovieParameter.mMovie = this;
    this->mPlayMovieParameter.mIsLooping = inLoop;
    this->mJustStarted = true;
    this->mThread = NULL;
    this->mSound.assign(kSoundRingFloats, 0.0f);
    this->mSoundRead = 0;
    this->mSoundCount = 0;
    this->mSoundPosition = 0.0;
    this->mSoundStep = 0.0;
    char theName[MAX_PATH];
    _snprintf(theName, MAX_PATH, "%s%s%s", (const char*)inFile.GetVolume(), (const char*)inFile.GetDirectory(),
              (const char*)inFile.GetFile());
    theName[MAX_PATH - 1] = 0;
    this->mFilename = theName;
    this->Reference();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mFrameAccess) == keseOK, 183);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSoundAccess) == keseOK, 183);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mFrameReadyEvent, false) == keseOK, 185);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMovieStopped, false) == keseOK, 186);
    this->mMovieStopped->Clear();
    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, DX7Movie::PlayMovie, (void*)&(this->mPlayMovieParameter)) == keseOK, 184);
}

// Waits for a decoded frame and copies it to the back buffer (stretched to the destination); 7 once playback has ended
long DX7Movie::RenderFrame()
{
    long theResult = kErrorOK;
    GERectangle theDestination;
    if (!this->mJustStarted && !this->mIsPlaying)
        theResult = 7;
    else {
        this->mFrameReadyEvent->Wait(INFINITE);
        this->mFrameAccess->Enter();
        GESurface* theTarget = GERasterizer::GetActiveRasterizer()->GetSecondarySurface(this->mScreen);
        GESurface* theFrame;
        if (this->mIsPlaying && (theFrame = this->mFrameBuffer) != NULL) {
            theFrame->Reference();
            if (!this->mHasRectangle) {
                theDestination.SetLeft(0.0f);
                theDestination.SetTop(0.0f);
                theDestination.SetRight((float)(long)theTarget->GetWidth());
                theDestination.SetBottom((float)(long)theTarget->GetHeight());
            } else
                theDestination = this->mDestinationRectangle;
            theResult = theFrame->CopyTo(theTarget, this->mSourceRectangle, theDestination, false);
            theFrame->Dereference();
        }
        this->mFrameAccess->Leave();
    }
    return theResult;
}

// Packs one row of BGRA pixels into a frame buffer row of the given pixel format (GEPixelFormatID)
static void PackRow(const unsigned char* inBGRA, unsigned char* outRow, unsigned long inWidth, unsigned long inFormat)
{
    unsigned short* the16 = reinterpret_cast<unsigned short*>(outRow);
    unsigned long* the32 = reinterpret_cast<unsigned long*>(outRow);
    for (unsigned long x = 0; x < inWidth; x++, inBGRA += 4) {
        unsigned long b = inBGRA[0], g = inBGRA[1], r = inBGRA[2];
        switch (inFormat) {
        case kPixelFormatA8R8G8B8:
        case kPixelFormatX8R8G8B8:
            the32[x] = 0xff000000 | r << 16 | g << 8 | b;
            break;
        case kPixelFormatA4R4G4B4:
            the16[x] = (unsigned short)(0xf000 | (r >> 4) << 8 | (g >> 4) << 4 | b >> 4);
            break;
        case kPixelFormatA1R5G5B5:
        case kPixelFormatX1R5G5B5:
            the16[x] = (unsigned short)(0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
            break;
        case kPixelFormatR5G5B5A1:
        case kPixelFormatR5G5B5X1:
            the16[x] = (unsigned short)((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | 1);
            break;
        default:   // kPixelFormatR5G6B5
            the16[x] = (unsigned short)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
            break;
        }
    }
}

// pl_mpeg decoded a frame (BGRA): converts it into the frame buffer and wakes RenderFrame
void DX7Movie::DecodedFrame(const unsigned char* inBGRA, unsigned long inStride)
{
    this->mFrameAccess->Enter();
    if (this->mFrameBuffer && this->mFrameBuffer->Lock() == kErrorOK) {
        unsigned long theWidth = (unsigned long)this->mSourceRectangle.GetRight();
        unsigned long theHeight = (unsigned long)this->mSourceRectangle.GetBottom();
        unsigned char* theRow = this->mFrameBuffer->GetMemoryAddress();
        unsigned long theFormat = this->mFrameBuffer->GetPixelFormat();
        for (unsigned long y = 0; y < theHeight; y++, theRow += this->mFrameBuffer->GetBytesPerRow())
            PackRow(inBGRA + y * inStride, theRow, theWidth, theFormat);
        this->mIsReady = true;
        this->mFrameReadyEvent->Set();
        this->mFrameBuffer->Unlock();
    }
    this->mFrameAccess->Leave();
}

// pl_mpeg decoded sound (interleaved stereo at the movie's rate): queues it for the mixer (dropped when the ring is
// full)
void DX7Movie::DecodedSamples(const float* inInterleaved, unsigned long inCount)
{
    this->mSoundAccess->Enter();
    unsigned long theSize = this->mSound.size();
    for (unsigned long i = 0; i < 2 * inCount && this->mSoundCount < theSize; i++) {
        this->mSound[(this->mSoundRead + this->mSoundCount) % theSize] = inInterleaved[i];
        this->mSoundCount++;
    }
    this->mSoundAccess->Leave();
}

// The mixer's device thread: adds the next device frames of the movie's sound (linear resampling)
void DX7Movie::MixSound(void* inMovie, float* outFrames, unsigned long inCount)
{
    DX7Movie* theMovie = static_cast<DX7Movie*>(inMovie);
    theMovie->mSoundAccess->Enter();
    const std::vector<float>& theRing = theMovie->mSound;
    unsigned long theSize = theRing.size();
    for (unsigned long i = 0; i < inCount && theMovie->mSoundCount >= 4; i++) {
        unsigned long a = theMovie->mSoundRead;
        unsigned long b = (a + 2) % theSize;
        float f = (float)theMovie->mSoundPosition;
        outFrames[2 * i] += theRing[a] + (theRing[b] - theRing[a]) * f;
        outFrames[2 * i + 1] += theRing[a + 1] + (theRing[b + 1] - theRing[a + 1]) * f;
        theMovie->mSoundPosition += theMovie->mSoundStep;
        while (theMovie->mSoundPosition >= 1.0 && theMovie->mSoundCount >= 2) {
            theMovie->mSoundPosition -= 1.0;
            theMovie->mSoundRead = (theMovie->mSoundRead + 2) % theSize;
            theMovie->mSoundCount -= 2;
        }
    }
    theMovie->mSoundAccess->Leave();
}

static void PLMVideo(plm_t* inPLM, plm_frame_t* inFrame, void* inMovie)
{
    static std::vector<unsigned char> sBGRA;   // one movie plays at a time (sMovieCriticalSection)
    unsigned long theStride = inFrame->width * 4;
    if (sBGRA.size() < theStride * inFrame->height)
        sBGRA.resize(theStride * inFrame->height);
    plm_frame_to_bgra(inFrame, &sBGRA[0], (int)theStride);
    static_cast<DX7Movie*>(inMovie)->DecodedFrame(&sBGRA[0], theStride);
}

static void PLMAudio(plm_t* inPLM, plm_samples_t* inSamples, void* inMovie)
{
    static_cast<DX7Movie*>(inMovie)->DecodedSamples(inSamples->interleaved, inSamples->count);
}

// Playback thread (one movie at a time): decodes the movie in real time until stopped or finished, then frees the
// frame buffer, releases the movie's reference and signals that it stopped
unsigned long DX7Movie::PlayMovie(void* inParameter)
{
    DX7Movie::sMovieCriticalSection->Enter();
    DX7Movie* theMovie = static_cast<PlayMovieParameter*>(inParameter)->mMovie;
    bool theLoop = static_cast<PlayMovieParameter*>(inParameter)->mIsLooping;
    theMovie->mThread->SetPriority(0);
    UMSetFloatingPointPrecision(0x10000);
    // the converted movie next to the name the game gave (X.bik -> X.mpg)
    char thePath[MAX_PATH];
    _snprintf(thePath, MAX_PATH, "%s", (const char*)theMovie->mFilename);
    thePath[MAX_PATH - 1] = 0;
    char* theDot = strrchr(thePath, '.');
    if (theDot && strchr(theDot, '\\') == NULL && (size_t)(theDot - thePath) + 5 <= MAX_PATH)
        strcpy(theDot, ".mpg");
    plm_t* thePLM = GetFileAttributesA(thePath) != INVALID_FILE_ATTRIBUTES ? plm_create_with_filename(thePath) : NULL;
    if (thePLM != NULL && plm_get_num_video_streams(thePLM) > 0) {
        if (theMovie->CreateFrameBuffer(plm_get_width(thePLM), plm_get_height(thePLM)) != kErrorOK)
            theMovie->mIsFinished = true;
        plm_set_loop(thePLM, theLoop);
        plm_set_video_decode_callback(thePLM, PLMVideo, theMovie);
        SEMixer* theMixer = SEMixer::GetActiveMixer();
        unsigned long theRate = theMixer ? theMixer->GetOutputRate() : 0;
        bool theSound = theRate && plm_get_num_audio_streams(thePLM) > 0 && plm_get_samplerate(thePLM) > 0;
        plm_set_audio_enabled(thePLM, theSound);
        if (theSound) {
            plm_set_audio_decode_callback(thePLM, PLMAudio, theMovie);
            plm_set_audio_lead_time(thePLM, (double)PLM_AUDIO_SAMPLES_PER_FRAME / plm_get_samplerate(thePLM));
            theMovie->mSoundStep = (double)plm_get_samplerate(thePLM) / theRate;
            theMixer->SetMovieSource(DX7Movie::MixSound, theMovie);
        }
        theMovie->mIsPlaying = true;
        theMovie->mJustStarted = false;
        LARGE_INTEGER theFrequency, theLast, theNow;
        QueryPerformanceFrequency(&theFrequency);
        QueryPerformanceCounter(&theLast);
        while (!theMovie->ShouldStop()) {
            if (UMGetFloatingPointPrecision() != 0x10000)
                UMSetFloatingPointPrecision(0x10000);
            if (UMGetFloatingPointRoundingMode() != 0)
                UMSetFloatingPointRoundingMode(0);
            QueryPerformanceCounter(&theNow);
            double theElapsed = (double)(theNow.QuadPart - theLast.QuadPart) / theFrequency.QuadPart;
            theLast = theNow;
            plm_decode(thePLM, theElapsed < 0.1 ? theElapsed : 0.1);
            if (!theLoop && plm_has_ended(thePLM))
                theMovie->mIsFinished = true;
            Sleep(1);
        }
        if (theSound)
            theMixer->SetMovieSource(NULL, NULL);   // the device thread no longer pulls after this returns
        theMovie->mFrameAccess->Enter();
        theMovie->mIsPlaying = false;
        theMovie->mFrameReadyEvent->Set();
        if (theMovie->mFrameBuffer != NULL) {
            theMovie->mFrameBuffer->Dereference();
            theMovie->mFrameBuffer = NULL;
        }
        theMovie->mFrameAccess->Leave();
        Sleep(100);
    }
    if (thePLM != NULL)
        plm_destroy(thePLM);
    theMovie->mJustStarted = false;
    theMovie->Dereference();
    DX7Movie::sMovieCriticalSection->Leave();
    theMovie->mMovieStopped->Set();
    return 0;
}

// Returns whether a stop was requested or the movie has finished
bool DX7Movie::ShouldStop()
{
    return this->mStopRequested || this->mIsFinished;
}

// Asks the playback thread to stop and waits until it has
void DX7Movie::Stop()
{
    this->mStopRequested = true;
    this->mMovieStopped->Wait(INFINITE);
}
