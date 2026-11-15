// DX7Movie: a GEMovie playing an MPEG-1 movie (pl_mpeg) on its own thread into a system-memory frame buffer, which
// RenderFrame copies to the screen's back buffer (the original played Bink files: a movie the game names X.bik plays
// from X.mpg, tools/convert_movies.py); its sound goes through the mixer (SEMixer::SetMovieSource). Whole in Rust
// (rust/crates/ee_dx7/src/movobj.rs, R4.3 part 14): the class keeps its vtable and one member, the object; the calls on
// C++ objects are DX7Trampolines.cpp's host.
#ifndef DX7_DX7MOVIE_H
#define DX7_DX7MOVIE_H

#include "dx7/DX7Common.h"
#include "lle/GERectangle.h"
#include "lle/GEMovie.h"
#include "rust/ee_dx7.h"

class DX7_API DX7Movie : public GEMovie {
public:
    DX7Movie(const FSFileSpec& inFile, unsigned long inScreen);
    DX7Movie(const FSFileSpec& inFile, unsigned long inScreen, GERectangle inRectangle, bool inLoop);
    virtual ~DX7Movie() { ee_dx7mv_destroy(this->mDX7); }

    virtual UString GetFilename() const { return UString(reinterpret_cast<const char*>(ee_dx7mv_filename(this->mDX7))); }
    virtual bool GetIsLooping() const { return ee_dx7mv_flag(this->mDX7, 0); }
    virtual bool GetIsPlaying() const { return ee_dx7mv_flag(this->mDX7, 1); }
    virtual bool GetIsReady() const { return ee_dx7mv_flag(this->mDX7, 2); }
    virtual bool GetJustStarted() const { return ee_dx7mv_flag(this->mDX7, 3); }
    virtual long RenderFrame() { return ee_dx7mv_render(this->mDX7); }
    virtual void Stop() { ee_dx7mv_stop(this->mDX7); }

    EeDx7Movie* mDX7;
};

#endif
