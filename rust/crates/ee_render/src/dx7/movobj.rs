//! `DX7Movie` whole (`Common/DX7Movie.cpp`, deleted): the playback parameters, the thread-system objects, the frame
//! buffer and its rectangles, the file name, the state flags the game and the playback thread share, the sound ring
//! and the `plm_t`, with every method of the class over [`crate::movie`]'s algorithms. The C++ class keeps its vtable
//! (the game calls it through `GEMovie`), the `UReferenceCounter` base and one member, the object; pl_mpeg's two decode
//! callbacks, the thread entry and the mixer's pull are C++ statics that forward here; every call on another C++ object
//! (the thread system, the mixer, the active rasterizer's surfaces, the file spec) is one host operation
//! ([`EeDx7MovieOp`]); pl_mpeg's queries and the clock are [`MovieSys`].
//!
//! Three threads reach the object: the game (render, stop, the getters), the playback thread (the play loop and
//! pl_mpeg's callbacks) and the mixer's device thread (the sound pull). The flags are atomics (`volatile bool` in the
//! original); the frame buffer and rectangles are used under `mFrameAccess` and the ring under `mSoundAccess`, as the
//! original did; no reference to the object is held across a host call.
#![allow(clippy::missing_safety_doc, clippy::not_unsafe_ptr_arg_deref, clippy::unnecessary_cast)] // c_ulong per target

use core::cell::UnsafeCell;
use core::ffi::{c_ulong, c_void};
use core::sync::atomic::{AtomicBool, AtomicPtr, Ordering::SeqCst};

use crate::dx7::api::{SurfaceDescription, H, NULL};
use crate::dx7::movie::{self, MovieHost, RenderHost, SoundRing, FP_PRECISION_DOUBLE, MOVIE_MAX_PATH};

/// The calls the movie makes on C++ objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeDx7MovieOp {
    /// `TSScheduler::CreateCriticalSection(p)` (`ESVERIFY_AT(.. == keseOK, a)` when `a` is not 0): the section.
    CsNew = 1,
    /// `TSScheduler::DestroyCriticalSection(obj)`.
    CsDelete,
    /// `obj->Enter()`.
    CsEnter,
    /// `obj->Leave()`.
    CsLeave,
    /// `TSScheduler::CreateEvent(p, false)` (`ESVERIFY_AT(.. == keseOK, a)`): the event.
    EventNew,
    /// `TSScheduler::DestroyEvent(obj)`.
    EventDelete,
    /// `obj->Set()`.
    EventSet,
    /// `obj->Clear()`.
    EventClear,
    /// `obj->Wait(INFINITE)`.
    EventWait,
    /// `TSScheduler::CreateThread(p, the playback entry, obj)` (`ESVERIFY_AT(.. == keseOK, a)`; `obj` the object): the
    /// thread.
    ThreadNew,
    /// `TSScheduler::DestroyThread(obj)`.
    ThreadDelete,
    /// `obj->SetPriority(a)`.
    ThreadPriority,
    /// `obj->Reference()` (the C++ movie).
    MovieReference,
    /// `obj->Dereference()` (the C++ movie).
    MovieDereference,
    /// `snprintf((char*)a, MAX_PATH, "%s%s%s", volume, directory, file)` of the `FSFileSpec*` `obj`.
    FileName,
    /// `UMGetFloatingPointPrecision()` (`a` 0) or `UMGetFloatingPointRoundingMode()` (`a` 1).
    FpGet,
    /// `UMSetFloatingPointPrecision(b)` (`a` 0) or `UMSetFloatingPointRoundingMode(b)` (`a` 1).
    FpSet,
    /// The `plm_t*` of the movie at the NUL-terminated path `a` (`PlatPathGetAttributes`, `PlatPathToNative`,
    /// `plm_create_with_filename`), NULL when it cannot be opened.
    Open,
    /// `plm_set_video_decode_callback((plm_t*)obj, the frame entry, (void*)a)`.
    InstallVideo,
    /// `plm_set_audio_decode_callback((plm_t*)obj, the samples entry, (void*)a)`, then `plm_set_audio_lead_time` of
    /// the `f64` whose bits are `b`.
    InstallAudio,
    /// `SEMixer::GetActiveMixer() ? GetOutputRate() : 0`.
    MixerRate,
    /// `SEMixer::GetActiveMixer()->SetMovieSource(the mixer entry, obj)`, or `(NULL, NULL)` when `obj` is NULL.
    MixerSource,
    /// `GERasterizer::GetActiveRasterizer()->GetScreen(a)`.
    ActiveScreen,
    /// `GERasterizer::GetActiveRasterizer()->GetSecondarySurface(a)`.
    ActiveTarget,
    /// `((GEScreen*)obj)->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b)`.
    ScreenCreateSurface,
    /// `((uint32_t*)a)[0..4]` = the surface's pixel format, depth, width and height.
    SurfaceInfo,
    /// `obj->Lock()`.
    SurfaceLock,
    /// `obj->Unlock()`.
    SurfaceUnlock,
    /// `((void**)a)[0] = obj->GetMemoryAddress(); ((uintptr_t*)a)[1] = obj->GetBytesPerRow()`.
    SurfaceMemory,
    /// `obj->Reference()` (a surface).
    SurfaceReference,
    /// `obj->Dereference()` (a surface).
    SurfaceDereference,
    /// `obj->CopyTo((GESurface*)a, source, destination, false)`: `b` is 8 floats, the source and destination
    /// rectangles (left, top, right, bottom).
    SurfaceCopy,
}

/// The host of the movie objects (several threads call it).
pub type EeDx7MovieHostFn = unsafe extern "C" fn(op: EeDx7MovieOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// pl_mpeg's queries and the platform clock (`plm_*`, `PlatCounter*`, `PlatSleep`).
pub trait MovieSys {
    fn video_streams(&mut self, plm: H) -> i32;
    fn width(&mut self, plm: H) -> i32;
    fn height(&mut self, plm: H) -> i32;
    fn set_loop(&mut self, plm: H, looping: bool);
    fn audio_streams(&mut self, plm: H) -> i32;
    fn sample_rate(&mut self, plm: H) -> i32;
    fn set_audio_enabled(&mut self, plm: H, enabled: bool);
    fn decode(&mut self, plm: H, seconds: f64);
    fn has_ended(&mut self, plm: H) -> bool;
    fn destroy(&mut self, plm: H);
    fn counter_frequency(&mut self) -> i64;
    fn counter_ticks(&mut self) -> i64;
    fn sleep(&mut self, milliseconds: u32);
}

/// `sMovieCriticalSection`: one movie plays at a time.
static MOVIE_SECTION: AtomicPtr<c_void> = AtomicPtr::new(core::ptr::null_mut());

/// The movie.
pub struct EeDx7Movie {
    host: EeDx7MovieHostFn,
    /// The C++ movie (`this`, as its `GEMovie*`).
    owner: H,
    /// `mPlayMovieParameter.mIsLooping`.
    pub looping: bool,
    /// `mFrameAccess`, `mSoundAccess`, `mFrameReadyEvent`, `mMovieStopped`, `mThread`.
    frame_access: AtomicPtr<c_void>,
    sound_access: AtomicPtr<c_void>,
    frame_ready: AtomicPtr<c_void>,
    stopped: AtomicPtr<c_void>,
    thread: AtomicPtr<c_void>,
    /// `mFrameBuffer` (a `GESurface*`).
    frame_buffer: AtomicPtr<c_void>,
    /// `mSourceRectangle`, `mDestinationRectangle` (left, top, right, bottom; under `mFrameAccess`).
    source: UnsafeCell<[f32; 4]>,
    destination: UnsafeCell<[f32; 4]>,
    /// `mFilename` (the name the game gave, NUL-terminated).
    pub filename: Vec<u8>,
    /// `mScreen`.
    pub screen: c_ulong,
    pub is_playing: AtomicBool,
    pub stop_requested: AtomicBool,
    pub is_finished: AtomicBool,
    pub is_ready: AtomicBool,
    pub just_started: AtomicBool,
    pub has_rectangle: bool,
    /// The decoded sound (under `mSoundAccess`).
    ring: UnsafeCell<SoundRing>,
    /// `mPLM` (the `plm_t*`).
    plm: AtomicPtr<c_void>,
}

/// # Safety
/// `p` is live.
unsafe fn mv<'a>(p: *mut EeDx7Movie) -> &'a EeDx7Movie {
    // SAFETY: the caller's contract; every field shared between threads is an atomic or used under its section
    unsafe { &*p }
}

/// `GEPackBGRXRow`: `width` pixels of 4 bytes (B, G, R, ignored) into a row of the pixel format.
///
/// # Safety
/// `source` holds `width` pixels; `row` has room for `width` pixels of the format.
pub unsafe fn pack_bgrx_row(source: *const u8, row: *mut u8, width: usize, format: c_ulong) {
    for x in 0..width {
        // SAFETY: the caller's contract
        let (b, g, r) = unsafe {
            (u32::from(*source.add(4 * x)), u32::from(*source.add(4 * x + 1)), u32::from(*source.add(4 * x + 2)))
        };
        // SAFETY: the caller's contract (unaligned writes: the row is a surface's)
        unsafe {
            match format {
                0 | 1 => (row as *mut u32).add(x).write_unaligned(0xff00_0000 | r << 16 | g << 8 | b),
                2 => (row as *mut u16).add(x).write_unaligned((0xf000 | (r >> 4) << 8 | (g >> 4) << 4 | b >> 4) as u16),
                3 | 5 => {
                    (row as *mut u16).add(x).write_unaligned((0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | b >> 3) as u16)
                }
                4 | 6 => (row as *mut u16)
                    .add(x)
                    .write_unaligned(((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | 1) as u16),
                _ => (row as *mut u16).add(x).write_unaligned(((r >> 3) << 11 | (g >> 2) << 5 | b >> 3) as u16),
            }
        }
    }
}

impl EeDx7Movie {
    /// A host call made by movie `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as [`EeDx7MovieOp`] describes.
    unsafe fn op(p: *mut Self, op: EeDx7MovieOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (the host is copied out before the call)
        let host = unsafe { mv(p).host };
        // SAFETY: the host's contract
        unsafe { host(op, obj, a, b) }
    }

    /// `DX7Movie(inFile, inScreen[, inRectangle, inLoop])`: the movie section on first use, the fields, the name, a
    /// reference, the sections and events, then the playback thread (`Initialize`). `rect` (left, top, right, bottom):
    /// the destination when the movie has one.
    ///
    /// # Safety
    /// `file` is an `FSFileSpec*`; `owner` the C++ movie.
    pub unsafe fn create(
        host: EeDx7MovieHostFn,
        owner: H,
        file: H,
        screen: c_ulong,
        rect: Option<[f32; 4]>,
        looping: bool,
    ) -> *mut Self {
        if MOVIE_SECTION.load(SeqCst).is_null() {
            // SAFETY: the host's contract
            MOVIE_SECTION.store(unsafe { host(EeDx7MovieOp::CsNew, NULL, 0, 0) } as H, SeqCst);
        }
        let p = Box::into_raw(Box::new(EeDx7Movie {
            host,
            owner,
            looping,
            frame_access: AtomicPtr::new(NULL),
            sound_access: AtomicPtr::new(NULL),
            frame_ready: AtomicPtr::new(NULL),
            stopped: AtomicPtr::new(NULL),
            thread: AtomicPtr::new(NULL),
            frame_buffer: AtomicPtr::new(NULL),
            source: UnsafeCell::new([0.0; 4]),
            destination: UnsafeCell::new(rect.unwrap_or([0.0; 4])),
            filename: Vec::new(),
            screen,
            is_playing: AtomicBool::new(false),
            stop_requested: AtomicBool::new(false),
            is_finished: AtomicBool::new(false),
            is_ready: AtomicBool::new(false),
            just_started: AtomicBool::new(true),
            has_rectangle: rect.is_some(),
            ring: UnsafeCell::new(SoundRing::new()),
            plm: AtomicPtr::new(NULL),
        }));
        // SAFETY: p is live and not yet shared
        unsafe {
            let mut name = [0u8; MOVIE_MAX_PATH];
            Self::op(p, EeDx7MovieOp::FileName, file, name.as_mut_ptr() as isize, 0);
            let length = name.iter().position(|c| *c == 0).unwrap_or(MOVIE_MAX_PATH - 1);
            let mut filename = name[..=length].to_vec();
            filename[length] = 0;
            (*p).filename = filename;
            Self::op(p, EeDx7MovieOp::MovieReference, owner, 0, 0);
            (*p).frame_access.store(Self::op(p, EeDx7MovieOp::CsNew, NULL, 183, 0) as H, SeqCst);
            (*p).sound_access.store(Self::op(p, EeDx7MovieOp::CsNew, NULL, 183, 0) as H, SeqCst);
            (*p).frame_ready.store(Self::op(p, EeDx7MovieOp::EventNew, NULL, 185, 0) as H, SeqCst);
            (*p).stopped.store(Self::op(p, EeDx7MovieOp::EventNew, NULL, 186, 0) as H, SeqCst);
            Self::op(p, EeDx7MovieOp::EventClear, (*p).stopped.load(SeqCst), 0, 0);
            (*p).thread.store(Self::op(p, EeDx7MovieOp::ThreadNew, p as H, 184, 0) as H, SeqCst);
        }
        p
    }

    /// `~DX7Movie`: the events, the thread, the sections, the ring; then frees the object.
    ///
    /// # Safety
    /// `p` was made by [`Self::create`] and is not used again.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            Self::op(p, EeDx7MovieOp::EventDelete, mv(p).stopped.load(SeqCst), 0, 0);
            Self::op(p, EeDx7MovieOp::EventDelete, mv(p).frame_ready.load(SeqCst), 0, 0);
            Self::op(p, EeDx7MovieOp::ThreadDelete, mv(p).thread.load(SeqCst), 0, 0);
            Self::op(p, EeDx7MovieOp::CsDelete, mv(p).frame_access.load(SeqCst), 0, 0);
            Self::op(p, EeDx7MovieOp::CsDelete, mv(p).sound_access.load(SeqCst), 0, 0);
            drop(Box::from_raw(p));
        }
    }

    /// The playback thread (`PlayMovie`).
    ///
    /// # Safety
    /// `p` is live when the thread starts (the thread's reference keeps it until its end).
    pub unsafe fn play<S: MovieSys>(p: *mut Self, sys: &mut S) {
        // SAFETY: the caller's contract
        let (name, looping, host, stopped, owner) = unsafe {
            let m = mv(p);
            (m.filename[..m.filename.len() - 1].to_vec(), m.looping, m.host, m.stopped.load(SeqCst), m.owner)
        };
        let mut host = Playback { p, sys, host, stopped, owner };
        movie::play_movie(&mut host, &name, looping);
    }

    /// `RenderFrame`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn render(p: *mut Self) -> i32 {
        movie::render_frame(&mut Render(p))
    }

    /// `Stop`: asks the playback thread to stop and waits until it has.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn stop(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            mv(p).stop_requested.store(true, SeqCst);
            Self::op(p, EeDx7MovieOp::EventWait, mv(p).stopped.load(SeqCst), 0, 0);
        }
    }

    /// `DecodedFrame`: pl_mpeg decoded a frame (BGRA): into the frame buffer, then wakes `RenderFrame`.
    ///
    /// # Safety
    /// `p` is live; `bgra` holds the source rectangle's rows of `stride` bytes.
    pub unsafe fn decoded_frame(p: *mut Self, bgra: *const u8, stride: u32) {
        // SAFETY: the caller's contract; the frame buffer is used under mFrameAccess
        unsafe {
            let section = mv(p).frame_access.load(SeqCst);
            Self::op(p, EeDx7MovieOp::CsEnter, section, 0, 0);
            let frame = mv(p).frame_buffer.load(SeqCst);
            if !frame.is_null() && Self::op(p, EeDx7MovieOp::SurfaceLock, frame, 0, 0) == 0 {
                let source = *mv(p).source.get();
                let (width, height) = (source[2] as c_ulong, source[3] as c_ulong);
                let mut memory: [usize; 2] = [0; 2];
                Self::op(p, EeDx7MovieOp::SurfaceMemory, frame, memory.as_mut_ptr() as isize, 0);
                let mut info = [0u32; 4];
                Self::op(p, EeDx7MovieOp::SurfaceInfo, frame, info.as_mut_ptr() as isize, 0);
                let format = c_ulong::from(info[0]);
                movie::convert_frame(bgra, stride, memory[0] as *mut u8, memory[1] as u32, height as u32, |s, d| {
                    pack_bgrx_row(s, d, width as usize, format)
                });
                mv(p).is_ready.store(true, SeqCst);
                Self::op(p, EeDx7MovieOp::EventSet, mv(p).frame_ready.load(SeqCst), 0, 0);
                Self::op(p, EeDx7MovieOp::SurfaceUnlock, frame, 0, 0);
            }
            Self::op(p, EeDx7MovieOp::CsLeave, section, 0, 0);
        }
    }

    /// `DecodedSamples`: queues interleaved stereo for the mixer (dropped when the ring is full).
    ///
    /// # Safety
    /// `p` is live; `samples` holds `count` stereo frames.
    pub unsafe fn decoded_samples(p: *mut Self, samples: *const f32, count: u32) {
        // SAFETY: the caller's contract; the ring is used under mSoundAccess
        unsafe {
            let section = mv(p).sound_access.load(SeqCst);
            Self::op(p, EeDx7MovieOp::CsEnter, section, 0, 0);
            (*mv(p).ring.get()).push(core::slice::from_raw_parts(samples, 2 * count as usize));
            Self::op(p, EeDx7MovieOp::CsLeave, section, 0, 0);
        }
    }

    /// `MixSound`: the mixer's device thread adds the next `count` device frames.
    ///
    /// # Safety
    /// `p` is live; `out` holds `count` stereo frames.
    pub unsafe fn mix(p: *mut Self, out: *mut f32, count: u32) {
        // SAFETY: the caller's contract; the ring is used under mSoundAccess
        unsafe {
            let section = mv(p).sound_access.load(SeqCst);
            Self::op(p, EeDx7MovieOp::CsEnter, section, 0, 0);
            let frames = count as usize;
            (*mv(p).ring.get()).mix(core::slice::from_raw_parts_mut(out, 2 * frames), frames);
            Self::op(p, EeDx7MovieOp::CsLeave, section, 0, 0);
        }
    }
}

/// The playback thread's side of the object ([`MovieHost`]); `stopped` and `owner` are read before the thread's
/// dereference (the movie may be gone after it).
struct Playback<'a, S: MovieSys> {
    p: *mut EeDx7Movie,
    sys: &'a mut S,
    host: EeDx7MovieHostFn,
    stopped: H,
    owner: H,
}

impl<S: MovieSys> Playback<'_, S> {
    fn op(&self, op: EeDx7MovieOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the movie is live until the thread's dereference
        unsafe { EeDx7Movie::op(self.p, op, obj, a, b) }
    }
    fn m(&self) -> &EeDx7Movie {
        // SAFETY: the movie is live until the thread's dereference
        unsafe { mv(self.p) }
    }
    fn plm(&self) -> H {
        self.m().plm.load(SeqCst)
    }
}

impl<S: MovieSys> MovieHost for Playback<'_, S> {
    fn thread_begin(&mut self) {
        self.op(EeDx7MovieOp::CsEnter, MOVIE_SECTION.load(SeqCst), 0, 0);
        self.op(EeDx7MovieOp::ThreadPriority, self.m().thread.load(SeqCst), 0, 0);
        self.op(EeDx7MovieOp::FpSet, NULL, 0, FP_PRECISION_DOUBLE as isize);
    }
    fn open(&mut self, path: &[u8]) -> bool {
        let mut z = path.to_vec();
        z.push(0);
        let plm = self.op(EeDx7MovieOp::Open, NULL, z.as_ptr() as isize, 0) as H;
        self.m().plm.store(plm, SeqCst);
        !plm.is_null()
    }
    fn video_stream_count(&mut self) -> i32 {
        let plm = self.plm();
        self.sys.video_streams(plm)
    }
    fn width(&mut self) -> i32 {
        let plm = self.plm();
        self.sys.width(plm)
    }
    fn height(&mut self) -> i32 {
        let plm = self.plm();
        self.sys.height(plm)
    }
    fn create_frame_buffer(&mut self, width: i32, height: i32) -> i32 {
        let screen = self.m().screen as isize;
        let gscreen = self.op(EeDx7MovieOp::ActiveScreen, NULL, screen, 0) as H;
        let target = self.op(EeDx7MovieOp::ActiveTarget, NULL, screen, 0) as H;
        let mut info = [0u32; 4];
        self.op(EeDx7MovieOp::SurfaceInfo, target, info.as_mut_ptr() as isize, 0);
        let (w, h) = (width as c_ulong, height as c_ulong);
        let mut d: SurfaceDescription = movie::frame_buffer_description(info[0], info[1], w as u32, h as u32);
        if !self.m().has_rectangle {
            // SAFETY: the rectangles are this thread's until playback starts
            unsafe { *self.m().destination.get() = [0.0, 0.0, d.mWidth as f32, d.mHeight as f32] };
        }
        let mut frame: H = NULL;
        let result = self.op(
            EeDx7MovieOp::ScreenCreateSurface,
            gscreen,
            &mut d as *mut SurfaceDescription as isize,
            &mut frame as *mut H as isize,
        );
        self.m().frame_buffer.store(frame, SeqCst);
        if result == 0 {
            // SAFETY: as above
            unsafe { *self.m().source.get() = [0.0, 0.0, w as i32 as f32, h as i32 as f32] };
        }
        result as i32
    }
    fn set_finished(&mut self) {
        self.m().is_finished.store(true, SeqCst);
    }
    fn set_loop(&mut self, looping: bool) {
        let plm = self.plm();
        self.sys.set_loop(plm, looping);
    }
    fn install_video_callback(&mut self) {
        self.op(EeDx7MovieOp::InstallVideo, self.plm(), self.p as isize, 0);
    }
    fn mixer_rate(&mut self) -> u32 {
        self.op(EeDx7MovieOp::MixerRate, NULL, 0, 0) as u32
    }
    fn audio_stream_count(&mut self) -> i32 {
        let plm = self.plm();
        self.sys.audio_streams(plm)
    }
    fn sample_rate(&mut self) -> i32 {
        let plm = self.plm();
        self.sys.sample_rate(plm)
    }
    fn set_audio_enabled(&mut self, enabled: bool) {
        let plm = self.plm();
        self.sys.set_audio_enabled(plm, enabled);
    }
    fn install_audio_callback(&mut self, lead_time: f64) {
        self.op(EeDx7MovieOp::InstallAudio, self.plm(), self.p as isize, lead_time.to_bits() as isize);
    }
    fn set_sound_step(&mut self, step: f64) {
        // SAFETY: as the original, outside mSoundAccess (the mixer is not attached yet)
        unsafe { (*self.m().ring.get()).set_step(step) };
    }
    fn set_mixer_source(&mut self, attach: bool) {
        self.op(EeDx7MovieOp::MixerSource, if attach { self.p as H } else { NULL }, 0, 0);
    }
    fn set_playing_started(&mut self) {
        self.m().is_playing.store(true, SeqCst);
        self.m().just_started.store(false, SeqCst);
    }
    fn counter_frequency(&mut self) -> i64 {
        self.sys.counter_frequency()
    }
    fn counter_ticks(&mut self) -> i64 {
        self.sys.counter_ticks()
    }
    fn should_stop(&mut self) -> bool {
        self.m().stop_requested.load(SeqCst) || self.m().is_finished.load(SeqCst)
    }
    fn fp_precision(&mut self) -> u32 {
        self.op(EeDx7MovieOp::FpGet, NULL, 0, 0) as u32
    }
    fn set_fp_precision(&mut self, precision: u32) {
        self.op(EeDx7MovieOp::FpSet, NULL, 0, precision as isize);
    }
    fn fp_rounding(&mut self) -> u32 {
        self.op(EeDx7MovieOp::FpGet, NULL, 1, 0) as u32
    }
    fn set_fp_rounding(&mut self, mode: u32) {
        self.op(EeDx7MovieOp::FpSet, NULL, 1, mode as isize);
    }
    fn decode(&mut self, elapsed: f64) {
        let plm = self.plm();
        self.sys.decode(plm, elapsed);
    }
    fn has_ended(&mut self) -> bool {
        let plm = self.plm();
        self.sys.has_ended(plm)
    }
    fn sleep(&mut self, milliseconds: u32) {
        self.sys.sleep(milliseconds);
    }
    fn frame_enter(&mut self) {
        self.op(EeDx7MovieOp::CsEnter, self.m().frame_access.load(SeqCst), 0, 0);
    }
    fn frame_leave(&mut self) {
        self.op(EeDx7MovieOp::CsLeave, self.m().frame_access.load(SeqCst), 0, 0);
    }
    fn clear_playing(&mut self) {
        self.m().is_playing.store(false, SeqCst);
    }
    fn frame_ready_set(&mut self) {
        self.op(EeDx7MovieOp::EventSet, self.m().frame_ready.load(SeqCst), 0, 0);
    }
    fn release_frame_buffer(&mut self) {
        let frame = self.m().frame_buffer.load(SeqCst);
        if !frame.is_null() {
            self.op(EeDx7MovieOp::SurfaceDereference, frame, 0, 0);
            self.m().frame_buffer.store(NULL, SeqCst);
        }
    }
    fn clear_just_started(&mut self) {
        self.m().just_started.store(false, SeqCst);
    }
    fn destroy(&mut self) {
        let plm = self.plm();
        self.sys.destroy(plm);
        self.m().plm.store(NULL, SeqCst);
    }
    fn dereference(&mut self) {
        self.op(EeDx7MovieOp::MovieDereference, self.owner, 0, 0);
    }
    fn thread_end(&mut self) {
        // the movie may be gone (the dereference above): the host and the event were read before
        let host = self.host;
        // SAFETY: the host's contract
        unsafe {
            host(EeDx7MovieOp::CsLeave, MOVIE_SECTION.load(SeqCst), 0, 0);
            host(EeDx7MovieOp::EventSet, self.stopped, 0, 0);
        }
    }
}

/// The game thread's side ([`RenderHost`]).
struct Render(*mut EeDx7Movie);

impl Render {
    fn op(&self, op: EeDx7MovieOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the movie is live during the game's call
        unsafe { EeDx7Movie::op(self.0, op, obj, a, b) }
    }
    fn m(&self) -> &EeDx7Movie {
        // SAFETY: the movie is live during the game's call
        unsafe { mv(self.0) }
    }
    fn target(&self) -> H {
        self.op(EeDx7MovieOp::ActiveTarget, NULL, self.m().screen as isize, 0) as H
    }
}

impl RenderHost for Render {
    fn just_started(&mut self) -> bool {
        self.m().just_started.load(SeqCst)
    }
    fn is_playing(&mut self) -> bool {
        self.m().is_playing.load(SeqCst)
    }
    fn wait_frame_ready(&mut self) {
        self.op(EeDx7MovieOp::EventWait, self.m().frame_ready.load(SeqCst), 0, 0);
    }
    fn frame_enter(&mut self) {
        self.op(EeDx7MovieOp::CsEnter, self.m().frame_access.load(SeqCst), 0, 0);
    }
    fn frame_leave(&mut self) {
        self.op(EeDx7MovieOp::CsLeave, self.m().frame_access.load(SeqCst), 0, 0);
    }
    fn frame_buffer_present(&mut self) -> bool {
        !self.m().frame_buffer.load(SeqCst).is_null()
    }
    fn has_rectangle(&mut self) -> bool {
        self.m().has_rectangle
    }
    fn destination_rectangle(&mut self) -> [f32; 4] {
        // SAFETY: under mFrameAccess
        unsafe { *self.m().destination.get() }
    }
    fn target_size(&mut self) -> (u32, u32) {
        let mut info = [0u32; 4];
        self.op(EeDx7MovieOp::SurfaceInfo, self.target(), info.as_mut_ptr() as isize, 0);
        (info[2], info[3])
    }
    fn copy_frame(&mut self, destination: [f32; 4]) -> i32 {
        let target = self.target();
        let frame = self.m().frame_buffer.load(SeqCst);
        self.op(EeDx7MovieOp::SurfaceReference, frame, 0, 0);
        // SAFETY: under mFrameAccess
        let source = unsafe { *self.m().source.get() };
        let rects = [
            source[0],
            source[1],
            source[2],
            source[3],
            destination[0],
            destination[1],
            destination[2],
            destination[3],
        ];
        let result = self.op(EeDx7MovieOp::SurfaceCopy, frame, target as isize, rects.as_ptr() as isize);
        self.op(EeDx7MovieOp::SurfaceDereference, frame, 0, 0);
        result as i32
    }
}

#[cfg(not(test))]
pub use ffi::*;

#[cfg(not(test))]
mod ffi {
    use core::ffi::{c_int, c_ulong, c_void};

    use super::*;

    /// cbindgen:ignore
    mod sys {
        use core::ffi::{c_int, c_void};
        extern "C" {
            pub fn plm_get_num_video_streams(plm: *mut c_void) -> c_int;
            pub fn plm_get_width(plm: *mut c_void) -> c_int;
            pub fn plm_get_height(plm: *mut c_void) -> c_int;
            pub fn plm_set_loop(plm: *mut c_void, looping: c_int);
            pub fn plm_get_num_audio_streams(plm: *mut c_void) -> c_int;
            pub fn plm_get_samplerate(plm: *mut c_void) -> c_int;
            pub fn plm_set_audio_enabled(plm: *mut c_void, enabled: c_int);
            pub fn plm_decode(plm: *mut c_void, seconds: f64);
            pub fn plm_has_ended(plm: *mut c_void) -> c_int;
            pub fn plm_destroy(plm: *mut c_void);
            pub fn PlatCounterFrequency() -> i64;
            pub fn PlatCounterTicks() -> i64;
            pub fn PlatSleep(milliseconds: u32);
        }
    }
    use sys::*;

    /// pl_mpeg and the platform clock.
    struct RawSys;

    impl MovieSys for RawSys {
        fn video_streams(&mut self, plm: H) -> i32 {
            unsafe { plm_get_num_video_streams(plm) }
        }
        fn width(&mut self, plm: H) -> i32 {
            unsafe { plm_get_width(plm) }
        }
        fn height(&mut self, plm: H) -> i32 {
            unsafe { plm_get_height(plm) }
        }
        fn set_loop(&mut self, plm: H, looping: bool) {
            unsafe { plm_set_loop(plm, c_int::from(looping)) }
        }
        fn audio_streams(&mut self, plm: H) -> i32 {
            unsafe { plm_get_num_audio_streams(plm) }
        }
        fn sample_rate(&mut self, plm: H) -> i32 {
            unsafe { plm_get_samplerate(plm) }
        }
        fn set_audio_enabled(&mut self, plm: H, enabled: bool) {
            unsafe { plm_set_audio_enabled(plm, c_int::from(enabled)) }
        }
        fn decode(&mut self, plm: H, seconds: f64) {
            unsafe { plm_decode(plm, seconds) }
        }
        fn has_ended(&mut self, plm: H) -> bool {
            unsafe { plm_has_ended(plm) != 0 }
        }
        fn destroy(&mut self, plm: H) {
            unsafe { plm_destroy(plm) }
        }
        fn counter_frequency(&mut self) -> i64 {
            unsafe { PlatCounterFrequency() }
        }
        fn counter_ticks(&mut self) -> i64 {
            unsafe { PlatCounterTicks() }
        }
        fn sleep(&mut self, milliseconds: u32) {
            unsafe { PlatSleep(milliseconds) }
        }
    }

    /// `DX7Movie(inFile, inScreen)` (`rect` NULL) or `DX7Movie(inFile, inScreen, inRectangle, inLoop)` (`rect` a
    /// `GERectangle`): `file` is the `FSFileSpec*`, `owner` the C++ movie.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_new(
        host: EeDx7MovieHostFn,
        owner: *mut c_void,
        file: *const c_void,
        screen: c_ulong,
        rect: *const f32,
        looping: bool,
    ) -> *mut EeDx7Movie {
        let rect = (!rect.is_null())
            .then(|| unsafe { crate::dx7::surfobj::ge_rect(rect) })
            .map(|r| [r.left, r.top, r.right, r.bottom]);
        unsafe { EeDx7Movie::create(host, owner, file as H, screen, rect, looping) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_destroy(p: *mut EeDx7Movie) {
        unsafe { EeDx7Movie::destroy(p) }
    }

    /// The playback thread's entry (`PlayMovie`'s parameter is the object).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_play(p: *mut EeDx7Movie) -> c_ulong {
        unsafe { EeDx7Movie::play(p, &mut RawSys) };
        0
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_render(p: *mut EeDx7Movie) -> i32 {
        unsafe { EeDx7Movie::render(p) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_stop(p: *mut EeDx7Movie) {
        unsafe { EeDx7Movie::stop(p) }
    }

    /// pl_mpeg's video callback (after `plm_frame_to_bgra`).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_frame(p: *mut EeDx7Movie, bgra: *const u8, stride: c_ulong) {
        unsafe { EeDx7Movie::decoded_frame(p, bgra, stride as u32) }
    }

    /// pl_mpeg's audio callback: `count` interleaved stereo frames.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_samples(p: *mut EeDx7Movie, samples: *const f32, count: c_ulong) {
        unsafe { EeDx7Movie::decoded_samples(p, samples, count as u32) }
    }

    /// The mixer's movie source (`SEMixer::SetMovieSource`'s context is the object).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_mix(p: *mut c_void, out: *mut f32, count: c_ulong) {
        unsafe { EeDx7Movie::mix(p as *mut EeDx7Movie, out, count as u32) }
    }

    /// The name the game gave (NUL-terminated).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_filename(p: *mut EeDx7Movie) -> *const u8 {
        unsafe { mv(p).filename.as_ptr() }
    }

    /// The flags by number: 0 looping, 1 playing, 2 ready, 3 just started.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7mv_flag(p: *mut EeDx7Movie, which: u32) -> bool {
        let m = unsafe { mv(p) };
        match which {
            0 => m.looping,
            1 => m.is_playing.load(SeqCst),
            2 => m.is_ready.load(SeqCst),
            _ => m.just_started.load(SeqCst),
        }
    }
}

#[cfg(test)]
#[path = "movobj_tests.rs"]
mod tests;
