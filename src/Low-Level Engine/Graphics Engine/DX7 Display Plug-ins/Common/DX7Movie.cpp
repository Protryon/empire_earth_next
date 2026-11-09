// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Movie.cpp
// The movie player: MPEG-1 video and MP2 audio through pl_mpeg (src/ThirdParty/pl_mpeg), replacing the original's Bink
// player. A movie plays on its own thread (PlayMovie, one movie at a time), paced by pl_mpeg's clock: every decoded
// frame is converted into a system-memory frame buffer in the back buffer's pixel format, which RenderFrame copies to
// the back buffer; the decoded sound waits in a ring that the mixer's device thread pulls (MixSound), resampled to
// the device rate. The game names movies X.bik; the player reads X.mpg next to it (tools/convert_movies.py).
#include <windows.h>
#include <stdint.h>
#include "gx/GX.h"
#include <stdio.h>
#include <string.h>

#include "pl_mpeg/pl_mpeg.h"
#include "lle/LowLevelEngine.h"
#include "platform/PlatPath.h"
#include "platform/PlatFile.h"
#include "lle/U2DGeometry.h"
#include "lle/GEPixelPack.h"
#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Movie.h"
#include "rust/ee_rust.h"
#include "platform/PlatTime.h"

TSCriticalSection* DX7Movie::sMovieCriticalSection;

// The player's logic is in Rust (rust/crates/ee_dx7/src/movie.rs); this file keeps the class, its thread-system objects
// and the trampolines into the engine (the mixer, pl_mpeg, the surfaces).

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
    ee_dx7m_ring_destroy(this->mSoundRing);
}

// Creates the system-memory frame buffer at the movie's size in the back buffer's pixel format; on success sets the
// source rectangle (the destination defaults to the movie's size)
long DX7Movie::CreateFrameBuffer(unsigned long inWidth, unsigned long inHeight)
{
    GESurfaceDescription theDescription;
    GEScreen* theScreen = GERasterizer::GetActiveRasterizer()->GetScreen(this->mScreen);
    GESurface* theTarget = GERasterizer::GetActiveRasterizer()->GetSecondarySurface(this->mScreen);
    ee_dx7m_frame_buffer_description(reinterpret_cast<EeDx7SurfaceDescription*>(&theDescription), theTarget->GetPixelFormat(),
                                     theTarget->GetPixelDepth(), inWidth, inHeight);
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
    this->mSoundRing = ee_dx7m_ring_new();
    this->mPLM = NULL;
    char theName[MAX_PATH];
    snprintf(theName, MAX_PATH, "%s%s%s", (const char*)inFile.GetVolume(), (const char*)inFile.GetDirectory(),
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
    return ee_dx7m_render_frame(this);
}

// pl_mpeg decoded a frame (BGRA): converts it into the frame buffer and wakes RenderFrame
void DX7Movie::DecodedFrame(const unsigned char* inBGRA, unsigned long inStride)
{
    this->mFrameAccess->Enter();
    if (this->mFrameBuffer && this->mFrameBuffer->Lock() == kErrorOK) {
        unsigned long theWidth = (unsigned long)this->mSourceRectangle.GetRight();
        unsigned long theHeight = (unsigned long)this->mSourceRectangle.GetBottom();
        ee_dx7m_convert_frame(inBGRA, inStride, this->mFrameBuffer->GetMemoryAddress(), this->mFrameBuffer->GetBytesPerRow(),
                              theWidth, theHeight, this->mFrameBuffer->GetPixelFormat());
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
    ee_dx7m_ring_push(this->mSoundRing, inInterleaved, inCount);
    this->mSoundAccess->Leave();
}

// The mixer's device thread: adds the next device frames of the movie's sound (linear resampling)
void DX7Movie::MixSound(void* inMovie, float* outFrames, unsigned long inCount)
{
    DX7Movie* theMovie = static_cast<DX7Movie*>(inMovie);
    theMovie->mSoundAccess->Enter();
    ee_dx7m_ring_mix(theMovie->mSoundRing, outFrames, inCount);
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

// Playback thread (one movie at a time): the Rust loop (ee_dx7m_play_movie) decodes until stopped or finished, then
// frees the frame buffer, releases the movie's reference and signals that it stopped
unsigned long DX7Movie::PlayMovie(void* inParameter)
{
    DX7Movie* theMovie = static_cast<PlayMovieParameter*>(inParameter)->mMovie;
    bool theLoop = static_cast<PlayMovieParameter*>(inParameter)->mIsLooping;
    const char* theName = (const char*)theMovie->mFilename;
    ee_dx7m_play_movie(theMovie, reinterpret_cast<const unsigned char*>(theName), strlen(theName), theLoop);
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

// ---- the trampolines the Rust player calls (rust/crates/ee_dx7/src/movie_ffi.rs)
extern "C" {
void ee_dx7cpp_movie_thread_begin(DX7Movie* m)
{
    DX7Movie::sMovieCriticalSection->Enter();
    m->mThread->SetPriority(0);
    UMSetFloatingPointPrecision(0x10000);
}

bool ee_dx7cpp_movie_open(DX7Movie* m, const unsigned char* inPath, size_t inLength)
{
    char thePath[MAX_PATH];
    memcpy(thePath, inPath, inLength);
    thePath[inLength] = 0;
    // the game's path on the native file system (case-insensitive lookup off Windows; the same path on Win32)
    char theNative[PLAT_MAX_PATH];
    m->mPLM = PlatPathGetAttributes(thePath) != PLAT_ATTR_INVALID && PlatPathToNative(thePath, theNative, sizeof(theNative))
                  ? plm_create_with_filename(theNative) : NULL;
    return m->mPLM != NULL;
}

int32_t ee_dx7cpp_movie_video_streams(DX7Movie* m) { return plm_get_num_video_streams((plm_t*)m->mPLM); }
int32_t ee_dx7cpp_movie_width(DX7Movie* m) { return plm_get_width((plm_t*)m->mPLM); }
int32_t ee_dx7cpp_movie_height(DX7Movie* m) { return plm_get_height((plm_t*)m->mPLM); }
int32_t ee_dx7cpp_movie_create_frame_buffer(DX7Movie* m, int32_t inWidth, int32_t inHeight) { return m->CreateFrameBuffer(inWidth, inHeight); }
void ee_dx7cpp_movie_set_finished(DX7Movie* m) { m->mIsFinished = true; }
void ee_dx7cpp_movie_set_loop(DX7Movie* m, bool inLoop) { plm_set_loop((plm_t*)m->mPLM, inLoop); }
void ee_dx7cpp_movie_install_video_callback(DX7Movie* m) { plm_set_video_decode_callback((plm_t*)m->mPLM, PLMVideo, m); }
uint32_t ee_dx7cpp_movie_mixer_rate(DX7Movie* m)
{
    SEMixer* theMixer = SEMixer::GetActiveMixer();
    return theMixer ? theMixer->GetOutputRate() : 0;
}
int32_t ee_dx7cpp_movie_audio_streams(DX7Movie* m) { return plm_get_num_audio_streams((plm_t*)m->mPLM); }
int32_t ee_dx7cpp_movie_sample_rate(DX7Movie* m) { return plm_get_samplerate((plm_t*)m->mPLM); }
void ee_dx7cpp_movie_set_audio_enabled(DX7Movie* m, bool inEnabled) { plm_set_audio_enabled((plm_t*)m->mPLM, inEnabled); }
void ee_dx7cpp_movie_install_audio_callback(DX7Movie* m, double inLeadTime)
{
    plm_set_audio_decode_callback((plm_t*)m->mPLM, PLMAudio, m);
    plm_set_audio_lead_time((plm_t*)m->mPLM, inLeadTime);
}
void ee_dx7cpp_movie_set_sound_step(DX7Movie* m, double inStep) { ee_dx7m_ring_set_step(m->mSoundRing, inStep); }
void ee_dx7cpp_movie_set_mixer_source(DX7Movie* m, bool inAttach)
{
    SEMixer* theMixer = SEMixer::GetActiveMixer();
    if (inAttach)
        theMixer->SetMovieSource(DX7Movie::MixSound, m);
    else
        theMixer->SetMovieSource(NULL, NULL);
}
void ee_dx7cpp_movie_set_playing_started(DX7Movie* m) { m->mIsPlaying = true; m->mJustStarted = false; }
bool ee_dx7cpp_movie_should_stop(DX7Movie* m) { return m->ShouldStop(); }
uint32_t ee_dx7cpp_movie_fp_precision() { return UMGetFloatingPointPrecision(); }
void ee_dx7cpp_movie_set_fp_precision(uint32_t inPrecision) { UMSetFloatingPointPrecision(inPrecision); }
uint32_t ee_dx7cpp_movie_fp_rounding() { return UMGetFloatingPointRoundingMode(); }
void ee_dx7cpp_movie_set_fp_rounding(uint32_t inMode) { UMSetFloatingPointRoundingMode(inMode); }
void ee_dx7cpp_movie_decode(DX7Movie* m, double inElapsed) { plm_decode((plm_t*)m->mPLM, inElapsed); }
bool ee_dx7cpp_movie_has_ended(DX7Movie* m) { return plm_has_ended((plm_t*)m->mPLM); }
void ee_dx7cpp_movie_frame_enter(DX7Movie* m) { m->mFrameAccess->Enter(); }
void ee_dx7cpp_movie_frame_leave(DX7Movie* m) { m->mFrameAccess->Leave(); }
void ee_dx7cpp_movie_clear_playing(DX7Movie* m) { m->mIsPlaying = false; }
void ee_dx7cpp_movie_frame_ready_set(DX7Movie* m) { m->mFrameReadyEvent->Set(); }
void ee_dx7cpp_movie_release_frame_buffer(DX7Movie* m)
{
    if (m->mFrameBuffer != NULL) {
        m->mFrameBuffer->Dereference();
        m->mFrameBuffer = NULL;
    }
}
void ee_dx7cpp_movie_clear_just_started(DX7Movie* m) { m->mJustStarted = false; }
void ee_dx7cpp_movie_destroy(DX7Movie* m) { plm_destroy((plm_t*)m->mPLM); m->mPLM = NULL; }
void ee_dx7cpp_movie_dereference(DX7Movie* m) { m->Dereference(); }
void ee_dx7cpp_movie_thread_end(DX7Movie* m)
{
    DX7Movie::sMovieCriticalSection->Leave();
    m->mMovieStopped->Set();
}
bool ee_dx7cpp_movie_is_playing(DX7Movie* m) { return m->mIsPlaying; }
bool ee_dx7cpp_movie_just_started(DX7Movie* m) { return m->mJustStarted; }
void ee_dx7cpp_movie_wait_frame_ready(DX7Movie* m) { m->mFrameReadyEvent->Wait(INFINITE); }
bool ee_dx7cpp_movie_frame_buffer_present(DX7Movie* m) { return m->mFrameBuffer != NULL; }
bool ee_dx7cpp_movie_has_rectangle(DX7Movie* m) { return m->mHasRectangle; }
void ee_dx7cpp_movie_destination_rectangle(DX7Movie* m, float* outRect)
{
    outRect[0] = m->mDestinationRectangle.GetLeft();
    outRect[1] = m->mDestinationRectangle.GetTop();
    outRect[2] = m->mDestinationRectangle.GetRight();
    outRect[3] = m->mDestinationRectangle.GetBottom();
}
void ee_dx7cpp_movie_target_size(DX7Movie* m, uint32_t* outWidth, uint32_t* outHeight)
{
    GESurface* theTarget = GERasterizer::GetActiveRasterizer()->GetSecondarySurface(m->mScreen);
    *outWidth = theTarget->GetWidth();
    *outHeight = theTarget->GetHeight();
}
int32_t ee_dx7cpp_movie_copy_frame(DX7Movie* m, const float* inDestination)
{
    GESurface* theTarget = GERasterizer::GetActiveRasterizer()->GetSecondarySurface(m->mScreen);
    GESurface* theFrame = m->mFrameBuffer;
    theFrame->Reference();
    GERectangle theDestination;
    theDestination.SetLeft(inDestination[0]);
    theDestination.SetTop(inDestination[1]);
    theDestination.SetRight(inDestination[2]);
    theDestination.SetBottom(inDestination[3]);
    long theResult = theFrame->CopyTo(theTarget, m->mSourceRectangle, theDestination, false);
    theFrame->Dereference();
    return theResult;
}
void ee_dx7cpp_pack_bgrx_row(const unsigned char* inSource, unsigned char* outDestination, unsigned long inWidth, unsigned long inFormat)
{
    GEPackBGRXRow(inSource, outDestination, inWidth, inFormat);
}
}
