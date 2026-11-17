//! `GEModelManager` as a whole Rust object (R4.3, seventh part; `texobj.rs` is its twin, GETextureManager): the cache of models by name (the original's
//! `std::map<UString, GEModel*>`, in `strcmp` order), the model search path (held), the lock and the memory footprint,
//! with every method of GEModelManager.cpp.
//!
//! The C++ `GEModelManager` stays the type the game and `GEModel` use (`gModelManager`, `GEModel::mManager`); its
//! methods are one-line forwarders and its only member is this object. Everything the original did to another object
//! goes through the C++ host (`GEModelManagerHost`), one operation per call the original made, in its order: the lock
//! (`TSCriticalSection`, recursive: a model's load may come back into its manager), the path's references, the models'
//! constructors, `SetSubModel`, `StoreLoaded`, `SizeOf`, `Verify`, `UReferenceCounter::Dereference` and reads of their
//! reference counts, the file lookup and the dump's lines. The models, files and paths are opaque addresses here.
//!
//! **Calls back in.** A host operation may call the manager again (a model's load may create models; a released
//! model's destructor releases textures, and a future one models): no Rust reference to the object lives across a host
//! call, and the cache is walked by key (the next name after the current one), so a walk goes on as the original's
//! `std::map` iterator did around an insertion or another entry's removal.

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::ops::Bound::{Excluded, Unbounded};

use crate::ge::managers::{self, NOT_FOUND, OK};

/// A call the managers make on another object (the C++ hosts `GEModelManagerHost` and `GETextureManagerHost`).
/// `obj`, `a`, `b` and `c` are the operation's arguments as each describes; the result is the call's.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeManOp {
    /// `TSScheduler::CreateCriticalSection` (with its `ESVERIFY`): the lock.
    LockCreate = 1,
    /// `TSScheduler::DestroyCriticalSection(obj)`.
    LockDestroy,
    /// `obj->Enter()`.
    LockEnter,
    /// `obj->Leave()`.
    LockLeave,
    /// `obj->Reference()` (an `FSPath`).
    PathReference,
    /// `obj->Dereference()` (an `FSPath`).
    PathDereference,
    /// `obj->mReferenceCount` (a model or texture).
    RefCount,
    /// `obj->UReferenceCounter::Dereference()` (a model or texture).
    BaseDereference,
    /// The dump's heading into the file `obj` (`FSFileSpec&`).
    DumpHeader,
    /// The dump's line of the cached `obj` (its reference count) named `a` (`const char*`) into the file `b`.
    DumpLine,
    /// `new GEModel(counts and flags of a, UString(b), this)`: `a` the forwarder's `GEModelCreateArgs*`, `b` the
    /// created name (`const char*`).
    ModelNew = 20,
    /// `CreateSubModel`'s body under the lock: `new GEModel(counts and flags of a, obj->mName, this)`,
    /// `obj->SetSubModel(model, b)`, `model->UReferenceCounter::Dereference()`; the model.
    ModelNewSub,
    /// `GEFindModelFile(obj, UString(a), *b, *c)`: `obj` the path, `a` the name (`const char*`), `b` the shared
    /// frames (`std::map<unsigned long, long>*`), `c` the file's out pointer (`EeGeCemModel**`); the result.
    ModelFindFile,
    /// `new GEModel(this)`.
    ModelNewEmpty,
    /// `obj->StoreLoaded(a, *b)`: `a` the file, `b` the shared frames; the result.
    ModelStoreLoaded,
    /// `obj->SizeOf()`.
    ModelSizeOf,
    /// `obj->Verify()` (the result ignored, as the original did).
    ModelVerify,
    /// `ee_ge_cem_free(obj)`.
    ModelFreeFile,
    /// `GEFindTextureFile(obj, UString(a), *c)`: `obj` the path, `a` the name (`const char*`), `c` the file's out
    /// pointer (`EeOfTexture**`); the result.
    TexFindFile = 40,
    /// `ee_of_texture_free(obj)`.
    TexFreeFile,
    /// `new GETexture(this)`.
    TexNew,
    /// `new GETexture(obj, this)` (`obj` a `GESurface*`).
    TexNewFromSurface,
    /// `obj->Load(a's file, *a's description, b, a's MIP maps, drop and frame format)`: `a` an `EeGeTexLoad*`, `b`
    /// the screen; the result.
    TexLoad,
    /// A field of the texture `obj` (`a` an `EeGeTexField`).
    TexGet,
    /// The field `a` (`EeGeTexField`) of the texture `obj` = `b`.
    TexSet,
    /// `obj->SetLogicalShift(a)`.
    TexSetLogicalShift,
    /// `obj->GetStage()`'s setting `a` (`EeGeTexStage`; the bias as its bits).
    TexStage,
    /// `obj->SetStage...(b, -1)` for the setting `a` (`EeGeTexStage`; the bias as its bits).
    TexSetStage,
    /// `obj->GetFrameTexture(a)`.
    TexFrame,
    /// `obj->Reference()`.
    TexReference,
    /// `obj->Dereference()` (`GETexture`'s: it may remove the texture from its manager).
    TexDereference,
    /// The description `a` (`GESurfaceDescription*`) given `obj->GetSurface()`'s pixel format, depth, volatility and
    /// 3D and transparency flags.
    TexSurfaceDescription,
    /// `obj->Recreate(a)` (`a` the screen); the result.
    TexRecreate,
    /// `obj->GetAllocatedSize()`.
    TexAllocatedSize,
    /// The texture pack's log line of `obj` named `a` with `b` MIP levels dropped (when `EE_TEXTURE_PACK_LOG` is set).
    TexPackLog,
}

/// The C++ host: `owner` is the C++ manager the object belongs to.
pub type EeGeManHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeManOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize;

/// The original's buffer for a created model's name (`char theBuffer[1024]`): the name is cut to 1,023 bytes.
const CREATED_NAME_CAPACITY: usize = 1024;

/// The C string at `s` as bytes (empty for NULL).
///
/// # Safety
/// `s` is NULL or NUL-terminated.
pub(crate) unsafe fn c_bytes<'a>(s: *const c_char) -> &'a [u8] {
    if s.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { CStr::from_ptr(s) }.to_bytes()
    }
}

/// `bytes` NUL-terminated (a name from a C string or the counters' format: no NUL inside).
pub(crate) fn c_string(bytes: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(bytes.len() + 1);
    v.extend(bytes.iter().copied().take_while(|&b| b != 0));
    v.push(0);
    v
}

/// The entry after `key` in `map` (the first for `None`): the walk of a `std::map` that its callees may change.
pub(crate) fn next_after(map: &BTreeMap<Vec<u8>, usize>, key: Option<&[u8]>) -> Option<(Vec<u8>, usize)> {
    match key {
        None => map.iter().next(),
        Some(k) => map.range::<[u8], _>((Excluded(k), Unbounded)).next(),
    }
    .map(|(k, v)| (k.clone(), *v))
}

/// The model manager.
pub struct EeGeModelManager {
    host: EeGeManHostFn,
    owner: *mut c_void,
    /// `mModels`: name -> model (the cache's reference).
    models: BTreeMap<Vec<u8>, usize>,
    /// `mPath` (referenced).
    path: usize,
    /// `mCriticalSection`.
    lock: usize,
    /// `mMemoryFootprint`: the cached models' `SizeOf`s.
    footprint: c_ulong,
}

/// Every access to the object is one short statement: a host call may come back in and use it.
///
/// # Safety
/// `p` is a live object from `ee_ge_modman_new`.
unsafe fn mm<'a>(p: *mut EeGeModelManager) -> &'a mut EeGeModelManager {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

impl EeGeModelManager {
    /// A host call.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeManOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeManOp, obj: usize, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { ((*p).host, (*p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner, op, obj as *mut c_void, a, b, c) }
    }

    /// # Safety
    /// `p` is live.
    unsafe fn enter(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::LockEnter, mm(p).lock, 0, 0, 0) };
    }

    /// # Safety
    /// `p` is live.
    unsafe fn leave(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::LockLeave, mm(p).lock, 0, 0, 0) };
    }

    /// The constructor: no models, the path referenced, then the lock.
    ///
    /// # Safety
    /// `host` is valid for `owner` from now until `destroy`.
    pub unsafe fn create(host: EeGeManHostFn, owner: *mut c_void, path: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(Self { host, owner, models: BTreeMap::new(), path: 0, lock: 0, footprint: 0 }));
        // SAFETY: p is live
        unsafe {
            mm(p).path = path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathReference, path, 0, 0, 0);
            }
            let lock = Self::op(p, EeGeManOp::LockCreate, 0, 0, 0, 0) as usize;
            mm(p).lock = lock;
        }
        p
    }

    /// The destructor: every unreferenced model removed, the path released, the lock destroyed, the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            Self::remove_all(p);
            let path = mm(p).path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathDereference, path, 0, 0, 0);
            }
            Self::op(p, EeGeManOp::LockDestroy, mm(p).lock, 0, 0, 0);
            drop(Box::from_raw(p));
        }
    }

    /// `CreateModel`: a new model registered under the next created name ("GEModelManager::CreateModel(name_N)").
    ///
    /// # Safety
    /// `p` is live; `name` is NUL-terminated; `args` is the host's.
    pub unsafe fn create_model(p: *mut Self, args: *const c_void, name: *const c_char) -> usize {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut created = managers::next_created_model_name(c_bytes(name));
            created.truncate(CREATED_NAME_CAPACITY - 1);
            let key = c_string(&created);
            let model = Self::op(p, EeGeManOp::ModelNew, 0, args as isize, key.as_ptr() as isize, 0) as usize;
            mm(p).models.insert(key[..key.len() - 1].to_vec(), model);
            Self::leave(p);
            model
        }
    }

    /// `CreateSubModel`: a model attached to `parent` at `index` (the parent holds it; not cached).
    ///
    /// # Safety
    /// `p` is live; `args` is the host's; `parent` a model.
    pub unsafe fn create_sub_model(p: *mut Self, args: *const c_void, index: c_ulong, parent: usize) -> usize {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let model = Self::op(p, EeGeManOp::ModelNewSub, parent, args as isize, index as isize, 0) as usize;
            Self::leave(p);
            model
        }
    }

    /// `DumpAllLoadedMedia`: the heading and each cached model's line, in name order.
    ///
    /// # Safety
    /// `p` is live; `file` is the host's.
    pub unsafe fn dump(p: *mut Self, file: usize) {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            Self::op(p, EeGeManOp::DumpHeader, file, 0, 0, 0);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, model)) = next_after(&mm(p).models, at.as_deref()) {
                let name = c_string(&key);
                Self::op(p, EeGeManOp::DumpLine, model, name.as_ptr() as isize, file as isize, 0);
                at = Some(key);
            }
            Self::leave(p);
        }
    }

    /// `LoadModel`: the cached model of that name, else its file found, stored and cached; 0 when there is none or it
    /// does not store.
    ///
    /// # Safety
    /// `p` is live; `name` is NUL-terminated; `textures` is the host's.
    pub unsafe fn load(p: *mut Self, name: *const c_char, textures: *mut c_void) -> usize {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let key = c_bytes(name).to_vec();
            let mut model = 0;
            if let Some(&cached) = mm(p).models.get(&key) {
                model = cached;
            } else {
                let mut file: usize = 0;
                let path = mm(p).path;
                let found = Self::op(
                    p,
                    EeGeManOp::ModelFindFile,
                    path,
                    name as isize,
                    textures as isize,
                    &mut file as *mut usize as isize,
                ) as c_long;
                if found == OK {
                    model = Self::op(p, EeGeManOp::ModelNewEmpty, 0, 0, 0, 0) as usize;
                    let stored = Self::op(p, EeGeManOp::ModelStoreLoaded, model, file as isize, textures as isize, 0);
                    if stored as c_long == OK {
                        mm(p).models.insert(key, model);
                        let size = Self::op(p, EeGeManOp::ModelSizeOf, model, 0, 0, 0) as c_ulong;
                        mm(p).footprint = mm(p).footprint.wrapping_add(size);
                    } else {
                        Self::op(p, EeGeManOp::BaseDereference, model, 0, 0, 0);
                        model = 0;
                    }
                    Self::op(p, EeGeManOp::ModelFreeFile, file, 0, 0, 0);
                }
            }
            Self::leave(p);
            model
        }
    }

    /// The cache's removal of `model` (named `key`) at its reference count: released and uncached when the cache's
    /// reference is the last; the decision's result.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn remove_entry(p: *mut Self, key: &[u8], model: usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            let count = Self::op(p, EeGeManOp::RefCount, model, 0, 0, 0) as c_ulong;
            let (remove, result) = managers::remove_decision(count);
            if remove {
                let size = Self::op(p, EeGeManOp::ModelSizeOf, model, 0, 0, 0) as c_ulong;
                mm(p).footprint = mm(p).footprint.wrapping_sub(size);
                Self::op(p, EeGeManOp::BaseDereference, model, 0, 0, 0);
                mm(p).models.remove(key);
            }
            result
        }
    }

    /// `RemoveAllModels`: every model nobody else references released; 5 when some are still in use.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn remove_all(p: *mut Self) -> c_long {
        let mut result = OK;
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, model)) = next_after(&mm(p).models, at.as_deref()) {
                let decision = Self::remove_entry(p, &key, model);
                if decision != OK {
                    result = decision;
                }
                at = Some(key);
            }
            Self::leave(p);
        }
        result
    }

    /// `RemoveModel`: `model` released if nobody else references it; 3 when it is not cached, 5 when in use.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn remove(p: *mut Self, model: usize) -> c_long {
        let mut result = NOT_FOUND;
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, cached)) = next_after(&mm(p).models, at.as_deref()) {
                if cached == model {
                    result = Self::remove_entry(p, &key, model);
                    break;
                }
                at = Some(key);
            }
            Self::leave(p);
        }
        result
    }

    /// `SetModelPath`: the old path released, the new one held.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_path(p: *mut Self, path: usize) {
        // SAFETY: the caller's contract
        unsafe {
            let old = mm(p).path;
            if old != 0 {
                Self::op(p, EeGeManOp::PathDereference, old, 0, 0, 0);
            }
            mm(p).path = path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathReference, path, 0, 0, 0);
            }
        }
    }

    /// `VerifyModels`: every cached model's self check (results ignored).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn verify(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, model)) = next_after(&mm(p).models, at.as_deref()) {
                Self::op(p, EeGeManOp::ModelVerify, model, 0, 0, 0);
                at = Some(key);
            }
            Self::leave(p);
        }
    }
}

/// `GEModelManager::GEModelManager(path)`: the object of the C++ manager `owner`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_modman_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_new(
    host: EeGeManHostFn,
    owner: *mut c_void,
    path: *mut c_void,
) -> *mut EeGeModelManager {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::create(host, owner, path as usize) }
}

/// `~GEModelManager`.
///
/// # Safety
/// `p` is from `ee_ge_modman_new` (freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_free(p: *mut EeGeModelManager) {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::destroy(p) }
}

/// `CreateModel` (`args` the host's counts and flags, `name` the caller's name).
///
/// # Safety
/// `p` is live; `name` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_create(
    p: *mut EeGeModelManager,
    args: *const c_void,
    name: *const c_char,
) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::create_model(p, args, name) as *mut c_void }
}

/// `CreateSubModel`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_create_sub(
    p: *mut EeGeModelManager,
    args: *const c_void,
    index: c_ulong,
    parent: *mut c_void,
) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::create_sub_model(p, args, index, parent as usize) as *mut c_void }
}

/// `DumpAllLoadedMedia(file)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_dump(p: *mut EeGeModelManager, file: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::dump(p, file as usize) }
}

/// `GetMemoryFootprint`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_footprint(p: *mut EeGeModelManager) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { mm(p).footprint }
}

/// `GetModelCount`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_count(p: *mut EeGeModelManager) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { mm(p).models.len() as c_ulong }
}

/// `LoadModel(name, textures)`.
///
/// # Safety
/// `p` is live; `name` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_load(
    p: *mut EeGeModelManager,
    name: *const c_char,
    textures: *mut c_void,
) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::load(p, name, textures) as *mut c_void }
}

/// `RemoveAllModels`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_remove_all(p: *mut EeGeModelManager) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::remove_all(p) }
}

/// `RemoveModel(model)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_remove(p: *mut EeGeModelManager, model: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::remove(p, model as usize) }
}

/// `SetModelPath(path)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_set_path(p: *mut EeGeModelManager, path: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::set_path(p, path as usize) }
}

/// `VerifyModels`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_modman_verify(p: *mut EeGeModelManager) {
    // SAFETY: the caller's contract
    unsafe { EeGeModelManager::verify(p) }
}

#[cfg(test)]
#[path = "manobj_tests.rs"]
pub(crate) mod tests;
