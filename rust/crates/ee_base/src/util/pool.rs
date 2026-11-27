//! `UMemoryPool<T>` and `UList<T>` whole in Rust (LLE round 8): the fixed-size object pool the units, the
//! doppelgangers, the tile unit lists and the contact lists allocate from, and the pooled doubly linked list.
//!
//! The C++ templates stay the types their users hold (`lle/UMemoryPool.h`, `lle/UList.h`); each pool holds one
//! [`EeUtilPool`] pointer and every member forwards. What only the C++ can do for a `T` is the [`EeUtilPoolHost`]:
//! `new T[n]` (a chunk's objects, constructed), `delete[]`, `new T` (the fallback once every chunk is full),
//! `delete`, and `push_back` onto a `std::vector<T*>`. An object's index in its chunk is the element difference
//! `(object - objects) / sizeof(T)` as the C++ pointer subtraction made it, truncated to `unsigned long`.
//!
//! The bookkeeping is the original's to the order of every choice (which chunk serves a request, which free index
//! is handed out next), so the objects a game gets, and so their addresses, are the C++'s:
//! - a chunk's free indices are a stack filled with `0..size`, its top the last: `New` hands out the chunk's last
//!   object first (size - 1, size - 2, ...), and a freed index is the next one handed out;
//! - `New` scans the chunks *before the last one* only (`theChunk != mLastChunk`), as the original's loops;
//! - `Delete` makes the object's chunk the current one; an object from no chunk was made by `new T`.

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};

/// What the C++ does for a pool's `T` (one-statement static members of `UMemoryPool<T>`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUtilPoolHost {
    /// `new T[count]`.
    pub new_array: unsafe extern "C" fn(count: usize) -> *mut c_void,
    /// `delete[] (T*)objects`.
    pub delete_array: unsafe extern "C" fn(objects: *mut c_void),
    /// `new T`.
    pub new_one: unsafe extern "C" fn() -> *mut c_void,
    /// `delete (T*)object`.
    pub delete_one: unsafe extern "C" fn(object: *mut c_void),
}

/// One block of preallocated objects and the stack of its free indices (`UMemoryPoolChunk<T>`).
struct Chunk {
    objects: *mut c_void,
    free: Vec<c_long>,
    /// The index of the free stack's top (-1: none free).
    top: isize,
}

impl Chunk {
    fn new(host: &EeUtilPoolHost, size: c_ulong) -> Chunk {
        // SAFETY: the host's `new T[size]`
        let objects = unsafe { (host.new_array)(size as usize) };
        let free = (0..size as c_long).collect::<Vec<_>>();
        Chunk { objects, free, top: size as isize - 1 }
    }
    fn count_free(&self) -> c_long {
        (self.top + 1) as c_long
    }
    fn count_used(&self) -> c_long {
        self.free.len() as c_long - self.count_free()
    }
    fn new_object(&mut self, elem: usize) -> *mut c_void {
        self.top -= 1;
        let index = self.free[(self.top + 1) as usize];
        (self.objects as *mut u8).wrapping_add(index as usize * elem) as *mut c_void
    }
    fn delete(&mut self, index: c_long) {
        self.top += 1;
        self.free[self.top as usize] = index;
    }
    /// The indices in use, ascending (`GetUsed`'s marks).
    fn used(&self) -> Vec<usize> {
        if self.count_used() <= 0 {
            return Vec::new();
        }
        let mut free = vec![false; self.free.len()];
        for &i in &self.free[..(self.top + 1) as usize] {
            free[i as usize] = true;
        }
        (0..free.len()).filter(|&i| !free[i]).collect()
    }
}

/// A pool (`UMemoryPool<T>`): its chunks and statistics.
pub struct EeUtilPool {
    elem: usize,
    host: EeUtilPoolHost,
    /// `mChunks` (`None`: the array is not allocated); its length is `mMaxChunks`, the first `chunk_count` set.
    chunks: Option<Vec<Option<Chunk>>>,
    /// `mCurrentChunk` as an index.
    current: Option<usize>,
    /// `mLastChunk` as an index (`mChunkCount - 1` when set up).
    last: Option<isize>,
    normal_news: c_long,
    normal_deletes: c_long,
    chunk_size: c_ulong,
    max_chunks: c_ulong,
    chunk_count: c_ulong,
    requests: c_long,
    deletions: c_long,
    peak_usage: c_long,
    is_set_up: bool,
}

impl EeUtilPool {
    /// A pool of `elem`-byte objects with the C++'s defaults (250 objects a chunk, one chunk at most).
    pub fn new(elem: usize, host: EeUtilPoolHost) -> EeUtilPool {
        EeUtilPool {
            elem,
            host,
            chunks: None,
            current: None,
            last: None,
            normal_news: 0,
            normal_deletes: 0,
            chunk_size: 250,
            max_chunks: 1,
            chunk_count: 0,
            requests: 0,
            deletions: 0,
            peak_usage: 0,
            is_set_up: false,
        }
    }

    fn chunk(&self, i: usize) -> &Chunk {
        self.chunks.as_ref().and_then(|c| c[i].as_ref()).expect("a set chunk")
    }
    fn chunk_mut(&mut self, i: usize) -> &mut Chunk {
        self.chunks.as_mut().and_then(|c| c[i].as_mut()).expect("a set chunk")
    }
    /// The chunks `New` scans: those before `mLastChunk`.
    fn scanned(&self) -> usize {
        match (self.chunks.is_some(), self.last) {
            (true, Some(l)) if l > 0 => l as usize,
            _ => 0,
        }
    }

    /// `SetSize`: fails (keeping the pool) while an object is in use.
    pub fn set_size(&mut self, chunk_size: c_ulong, max_chunks: c_ulong, chunk_count: c_ulong) -> bool {
        let mut result = self.chunks.is_none();
        if !result {
            result = true;
            let mut i = 0;
            while i < self.chunk_count as usize && result {
                result = self.chunk(i).count_used() == 0;
                i += 1;
            }
            if result {
                self.release();
            }
        }
        if result {
            self.chunk_size = chunk_size;
            self.max_chunks = max_chunks;
            self.chunk_count = chunk_count;
            let mut chunks: Vec<Option<Chunk>> = (0..max_chunks).map(|_| None).collect();
            for slot in chunks.iter_mut().take(chunk_count as usize) {
                *slot = Some(Chunk::new(&self.host, chunk_size));
            }
            self.current = if chunks.first().is_some_and(|c| c.is_some()) { Some(0) } else { None };
            self.chunks = Some(chunks);
            self.last = Some(chunk_count as isize - 1);
        }
        self.is_set_up = true;
        result
    }

    /// `AddChunk`.
    pub fn add_chunk(&mut self) -> bool {
        if self.chunk_count >= self.max_chunks {
            return false;
        }
        let i = self.chunk_count as usize;
        if self.chunks.is_some() {
            let chunk = Chunk::new(&self.host, self.chunk_size);
            if let Some(chunks) = &mut self.chunks {
                chunks[i] = Some(chunk);
            }
            self.current = Some(i);
            self.last = Some(i as isize);
            self.chunk_count += 1;
        } else {
            self.set_size(self.chunk_size, self.max_chunks, 1);
        }
        true
    }

    /// `New`: a free object of a chunk, else `new T`.
    pub fn new_object(&mut self) -> *mut c_void {
        let mut object: *mut c_void = core::ptr::null_mut();
        let mut add = self.chunks.is_none();
        if !add && self.current.is_none() {
            add = true;
            for i in 0..self.scanned() {
                if self.chunk(i).count_free() != 0 {
                    self.current = Some(i);
                    add = false;
                    break;
                }
            }
        }
        if add {
            self.add_chunk();
        }
        if let Some(c) = self.current {
            let elem = self.elem;
            object = self.chunk_mut(c).new_object(elem);
            if self.chunk(c).count_free() == 0 {
                self.current = None;
            }
        } else {
            for i in 0..self.scanned() {
                if self.chunk(i).count_free() > 0 {
                    let elem = self.elem;
                    object = self.chunk_mut(i).new_object(elem);
                    self.current = if self.chunk(i).count_free() > 0 { Some(i) } else { None };
                    break;
                }
            }
        }
        if object.is_null() {
            // SAFETY: the host's `new T`
            object = unsafe { (self.host.new_one)() };
            self.normal_news = self.normal_news.wrapping_add(1);
        }
        self.requests = self.requests.wrapping_add(1);
        let usage = self.requests.wrapping_sub(self.deletions);
        if self.peak_usage < usage {
            self.peak_usage = usage;
        }
        object
    }

    /// The object's element index in chunk `i` as the C++'s `(unsigned long)(object - mChunkObjects[i])`.
    fn index_in(&self, object: *mut c_void, i: usize) -> c_ulong {
        let base = self.chunks.as_ref().and_then(|c| c[i].as_ref()).map_or(0, |c| c.objects as isize);
        ((object as isize).wrapping_sub(base) / self.elem as isize) as c_ulong
    }

    /// `Delete`: back to its chunk, or `delete` for an object of `new T`.
    pub(crate) fn delete(&mut self, object: *mut c_void) {
        for i in 0..self.chunk_count as usize {
            let index = self.index_in(object, i);
            if index < self.chunk_size {
                self.current = Some(i);
                self.chunk_mut(i).delete(index as c_long);
                self.deletions = self.deletions.wrapping_add(1);
                return;
            }
        }
        self.normal_deletes = self.normal_deletes.wrapping_add(1);
        // SAFETY: an object of the host's `new T`
        unsafe { (self.host.delete_one)(object) };
        self.deletions = self.deletions.wrapping_add(1);
    }

    /// `CountUsed`.
    pub fn count_used(&self) -> c_long {
        if self.chunks.is_none() {
            return 0;
        }
        (0..self.chunk_count as usize).rev().map(|i| self.chunk(i).count_used()).sum()
    }

    /// `Release`: frees every chunk, but only when no object is in use.
    pub fn release(&mut self) -> bool {
        let result = self.count_used() == 0;
        if result {
            if let Some(chunks) = self.chunks.take() {
                for chunk in chunks.into_iter().take(self.chunk_count as usize).flatten() {
                    // SAFETY: the chunk's objects came from the host's `new T[]`
                    unsafe { (self.host.delete_array)(chunk.objects) };
                }
            }
            self.last = None;
            self.chunk_count = 0;
            self.current = None;
        }
        result
    }

    /// `GetUsed`: every object in use, the last chunk first, each chunk's in ascending order.
    pub fn used(&self) -> Vec<*mut c_void> {
        let mut out = Vec::new();
        if self.chunks.is_some() {
            for i in (0..self.chunk_count as usize).rev() {
                let c = self.chunk(i);
                out.extend(
                    c.used().into_iter().map(|u| (c.objects as *mut u8).wrapping_add(u * self.elem) as *mut c_void),
                );
            }
        }
        out
    }

    /// `DumpStats`' text.
    pub fn stats(&self, type_name: &str) -> String {
        format!(
            "!UMemoryPool< {} >: Stats are:\n\tRequests: {}\n\tDeletions: {}\n\tPeak usage: {}\n\tNormalNews: {}\n\t\
             NormalDeletes: {}\n\tLeaks: {}\n",
            type_name,
            self.requests,
            self.deletions,
            self.peak_usage,
            self.normal_news,
            self.normal_deletes,
            self.requests.wrapping_sub(self.deletions)
        )
    }

    /// Whether `SetSize` has run.
    pub fn is_set_up(&self) -> bool {
        self.is_set_up
    }
}

/// A new pool of `elem`-byte objects (`UMemoryPool<T>::UMemoryPool`).
#[no_mangle]
pub extern "C" fn ee_util_pool_new(elem: usize, host: EeUtilPoolHost) -> *mut EeUtilPool {
    Box::into_raw(Box::new(EeUtilPool::new(elem.max(1), host)))
}

/// `~UMemoryPool`: `Release`, then the pool itself (chunks still in use are left alive, as the C++ left them).
///
/// # Safety
/// `pool` is null or from [`ee_util_pool_new`], not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_free(pool: *mut EeUtilPool) {
    if pool.is_null() {
        return;
    }
    // SAFETY: the contract
    let mut pool = unsafe { Box::from_raw(pool) };
    pool.release();
    if let Some(chunks) = pool.chunks.take() {
        core::mem::forget(chunks.into_iter().flatten().map(|c| c.objects).collect::<Vec<_>>());
    }
}

macro_rules! with_pool {
    ($pool:ident, $default:expr, |$p:ident| $body:expr) => {{
        // SAFETY: the C++ pool's own pointer
        match unsafe { $pool.as_mut() } {
            Some($p) => $body,
            None => $default,
        }
    }};
}

/// `SetSize`.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_set_size(
    pool: *mut EeUtilPool,
    chunk_size: c_ulong,
    max_chunks: c_ulong,
    chunk_count: c_ulong,
) -> bool {
    with_pool!(pool, false, |p| p.set_size(chunk_size, max_chunks, chunk_count))
}

/// `New`.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_new_object(pool: *mut EeUtilPool) -> *mut c_void {
    with_pool!(pool, core::ptr::null_mut(), |p| p.new_object())
}

/// `Delete`.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`]; `object` from its `New`.
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_delete(pool: *mut EeUtilPool, object: *mut c_void) {
    with_pool!(pool, (), |p| p.delete(object))
}

/// `Release`.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_release(pool: *mut EeUtilPool) -> bool {
    with_pool!(pool, false, |p| p.release())
}

/// `CountUsed`.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_count_used(pool: *mut EeUtilPool) -> c_long {
    with_pool!(pool, 0, |p| p.count_used())
}

/// `GetUsed`: `push(list, object)` for every object in use (`std::vector<T*>::push_back`).
///
/// # Safety
/// `pool` from [`ee_util_pool_new`]; `push` takes `list`.
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_get_used(
    pool: *mut EeUtilPool,
    list: *mut c_void,
    push: unsafe extern "C" fn(list: *mut c_void, object: *mut c_void),
) {
    let used = with_pool!(pool, Vec::new(), |p| p.used());
    for object in used {
        // SAFETY: the C++ vector's push_back
        unsafe { push(list, object) };
    }
}

/// `DumpStats`: the statistics to the debugger output, or (as the original's compiled-away trace) nowhere.
///
/// # Safety
/// `pool` from [`ee_util_pool_new`]; `type_name` a C string (`typeid(T).name()`).
#[no_mangle]
pub unsafe extern "C" fn ee_util_pool_dump_stats(pool: *mut EeUtilPool, type_name: *const c_char, to_debugger: bool) {
    if !to_debugger {
        return;
    }
    // SAFETY: the contract
    let name = if type_name.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(type_name) }.to_string_lossy().into_owned()
    };
    let text = with_pool!(pool, return, |p| p.stats(&name));
    let mut bytes = text.into_bytes();
    bytes.push(0);
    // SAFETY: a C string
    unsafe { crate::plat_core::message::PlatDebugOutput(bytes.as_ptr() as *const c_char) };
}

/// A node of a `UList<T*>` (`UListNode<T*>`: the data, then the links).
#[repr(C)]
pub struct EeUtilListNode {
    pub data: *mut c_void,
    pub prev: *mut EeUtilListNode,
    pub next: *mut EeUtilListNode,
}

/// A `UList<T*>`: its head and count.
#[repr(C)]
pub struct EeUtilList {
    pub head: *mut EeUtilListNode,
    pub count: c_ulong,
}

/// `UList::InsertAfter`: a node from the list's pool holding `data`, after `previous` (null: at the head).
///
/// # Safety
/// `list` a live list whose nodes come from `pool`; `previous` null or one of its nodes.
#[no_mangle]
pub unsafe extern "C" fn ee_util_list_insert_after(
    list: *mut EeUtilList,
    pool: *mut EeUtilPool,
    previous: *mut EeUtilListNode,
    data: *mut c_void,
) {
    // SAFETY: the contract: every pointer is a live node of the list, or the new one
    unsafe {
        let node = ee_util_pool_new_object(pool) as *mut EeUtilListNode;
        (*node).data = data;
        (*node).prev = core::ptr::null_mut();
        (*node).next = core::ptr::null_mut();
        if !previous.is_null() {
            if !(*previous).next.is_null() {
                (*node).next = (*previous).next;
                (*(*previous).next).prev = node;
            }
            (*node).prev = previous;
            (*previous).next = node;
        } else {
            (*node).next = (*list).head;
            if !(*list).head.is_null() {
                (*(*list).head).prev = node;
            }
            (*list).head = node;
        }
        (*list).count = (*list).count.wrapping_add(1);
    }
}

/// `UList::AddHead`.
///
/// # Safety
/// As [`ee_util_list_insert_after`].
#[no_mangle]
pub unsafe extern "C" fn ee_util_list_add_head(list: *mut EeUtilList, pool: *mut EeUtilPool, data: *mut c_void) {
    // SAFETY: the contract
    unsafe {
        let node = ee_util_pool_new_object(pool) as *mut EeUtilListNode;
        (*node).data = data;
        (*node).next = (*list).head;
        (*node).prev = core::ptr::null_mut();
        if !(*list).head.is_null() {
            (*(*list).head).prev = node;
        }
        (*list).count = (*list).count.wrapping_add(1);
        (*list).head = node;
    }
}

/// `UList::Remove`: the node out of the list and back to the pool.
///
/// # Safety
/// `node` a node of `list`, from `pool`.
#[no_mangle]
pub unsafe extern "C" fn ee_util_list_remove(list: *mut EeUtilList, pool: *mut EeUtilPool, node: *mut EeUtilListNode) {
    // SAFETY: the contract
    unsafe {
        if !(*node).prev.is_null() {
            (*(*node).prev).next = (*node).next;
        } else {
            (*list).head = (*node).next;
        }
        if !(*node).next.is_null() {
            (*(*node).next).prev = (*node).prev;
        }
        ee_util_pool_delete(pool, node as *mut c_void);
        (*list).count = (*list).count.wrapping_sub(1);
    }
}

/// `UList::Clear`.
///
/// # Safety
/// `list` a live list whose nodes come from `pool`.
#[no_mangle]
pub unsafe extern "C" fn ee_util_list_clear(list: *mut EeUtilList, pool: *mut EeUtilPool) {
    // SAFETY: the contract
    unsafe {
        let mut node = (*list).head;
        while !node.is_null() {
            let next = (*node).next;
            ee_util_list_remove(list, pool, node);
            node = next;
        }
        (*list).head = core::ptr::null_mut();
        (*list).count = 0;
    }
}

/// `UList::Find`: the first node holding `data`, or null.
///
/// # Safety
/// `list` a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_util_list_find(list: *const EeUtilList, data: *mut c_void) -> *mut EeUtilListNode {
    // SAFETY: the contract
    unsafe {
        let mut node = (*list).head;
        while !node.is_null() {
            if (*node).data == data {
                return node;
            }
            node = (*node).next;
        }
    }
    core::ptr::null_mut()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    /// The fake host's objects: 16-byte cells from leaked boxes, logged.
    static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
    const ELEM: usize = 16;

    unsafe extern "C" fn new_array(count: usize) -> *mut c_void {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push(format!("new[{count}]"));
        Box::into_raw(vec![0u8; count * ELEM].into_boxed_slice()) as *mut u8 as *mut c_void
    }
    unsafe extern "C" fn delete_array(_objects: *mut c_void) {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push("delete[]".into());
    }
    unsafe extern "C" fn new_one() -> *mut c_void {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push("new".into());
        Box::into_raw(Box::new([0u8; ELEM])) as *mut c_void
    }
    unsafe extern "C" fn delete_one(_object: *mut c_void) {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push("delete".into());
    }
    const HOST: EeUtilPoolHost = EeUtilPoolHost { new_array, delete_array, new_one, delete_one };

    /// The list's host: cells of a node's size and alignment. The list test ran over `HOST` once, whose cells are
    /// 16 bytes under 24-byte nodes: the pool's first node (index 249 of 250) was written 1,976 bytes past the end of
    /// the chunk's 4,000, into whatever the other tests' threads had there (the test binary's STATUS_HEAP_CORRUPTION,
    /// PORT_LEARNINGS "The ee_base heap corruption").
    const NODE_WORDS: usize = core::mem::size_of::<EeUtilListNode>() / core::mem::size_of::<usize>();
    unsafe extern "C" fn new_nodes(count: usize) -> *mut c_void {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push(format!("new[{count}]"));
        Box::into_raw(vec![0usize; count * NODE_WORDS].into_boxed_slice()) as *mut usize as *mut c_void
    }
    unsafe extern "C" fn new_node() -> *mut c_void {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push("new".into());
        Box::into_raw(Box::new([0usize; NODE_WORDS])) as *mut c_void
    }
    const NODE_HOST: EeUtilPoolHost =
        EeUtilPoolHost { new_array: new_nodes, delete_array, new_one: new_node, delete_one };

    fn log() -> Vec<String> {
        core::mem::take(&mut *LOG.lock().unwrap_or_else(|e| e.into_inner()))
    }
    static SERIAL: Mutex<()> = Mutex::new(());

    fn index(pool: &EeUtilPool, chunk: usize, object: *mut c_void) -> usize {
        (object as usize - pool.chunk(chunk).objects as usize) / ELEM
    }

    #[test]
    fn objects_come_last_index_first_and_a_freed_one_comes_back_first() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let mut p = EeUtilPool::new(ELEM, HOST);
        assert!(p.set_size(4, 2, 1));
        assert_eq!(log(), ["new[4]"]);
        let a: Vec<_> = (0..4).map(|_| p.new_object()).collect();
        assert_eq!(a.iter().map(|&o| index(&p, 0, o)).collect::<Vec<_>>(), [3, 2, 1, 0]);
        assert!(p.current.is_none(), "a full chunk is no longer current");
        p.delete(a[2]);
        assert_eq!(p.new_object(), a[2]);
        // the fifth: a second chunk
        let e = p.new_object();
        assert_eq!(log(), ["new[4]"]);
        assert_eq!(index(&p, 1, e), 3);
        assert_eq!(p.count_used(), 5);
        assert_eq!(p.peak_usage, 5);
        assert!(!p.release(), "objects in use");
        for o in a.into_iter().chain([e]) {
            p.delete(o);
        }
        assert!(p.release());
        assert_eq!(log(), ["delete[]", "delete[]"]);
    }

    #[test]
    fn a_full_pool_falls_back_to_new_and_delete() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let mut p = EeUtilPool::new(ELEM, HOST);
        let a = p.new_object(); // AddChunk -> SetSize(250, 1, 1)
        assert_eq!(log(), ["new[250]"]);
        let mut all = vec![a];
        all.extend((0..249).map(|_| p.new_object()));
        let extra = p.new_object();
        assert_eq!(log(), ["new"]);
        assert_eq!(p.normal_news, 1);
        p.delete(extra);
        assert_eq!(log(), ["delete"]);
        assert_eq!(p.normal_deletes, 1);
        assert_eq!(p.requests - p.deletions, 250);
        assert!(p.stats("struct X").contains("Requests: 251\n\tDeletions: 1\n\tPeak usage: 251\n"));
        for o in all {
            p.delete(o);
        }
        assert!(p.release());
        log();
    }

    #[test]
    fn new_scans_only_the_chunks_before_the_last() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let mut p = EeUtilPool::new(ELEM, HOST);
        assert!(p.set_size(2, 3, 3));
        let a: Vec<_> = (0..2).map(|_| p.new_object()).collect(); // chunk 0 full, no current
        assert!(p.current.is_none());
        let b = p.new_object(); // scans chunk 0 (full), chunk 1 (free): current 1
        assert_eq!(index(&p, 1, b), 1);
        let c = p.new_object();
        assert_eq!(index(&p, 1, c), 0);
        // chunks 0 and 1 full; chunk 2 (the last) is never scanned: AddChunk fails (3 of 3), so new T
        let d = p.new_object();
        assert_eq!(log(), ["new[2]", "new[2]", "new[2]", "new"]);
        p.delete(d);
        for o in a.into_iter().chain([b, c]) {
            p.delete(o);
        }
        assert!(p.release());
        log();
    }

    #[test]
    fn used_objects_are_listed_last_chunk_first_ascending() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let mut p = EeUtilPool::new(ELEM, HOST);
        assert!(p.set_size(3, 2, 2));
        let a: Vec<_> = (0..3).map(|_| p.new_object()).collect();
        // chunk 1 is the last: New never scans it, and AddChunk fails (2 of 2), so the fourth is new T
        let b = p.new_object();
        assert_eq!(p.normal_news, 1);
        p.delete(a[1]);
        assert_eq!(p.used(), [a[2], a[0]]);
        p.delete(b);
        assert_eq!(log(), ["new[3]", "new[3]", "new", "delete"]);
        // added chunks are used: the last chunk's objects come first
        let mut q = EeUtilPool::new(ELEM, HOST);
        assert!(q.set_size(2, 3, 1));
        let c: Vec<_> = (0..3).map(|_| q.new_object()).collect();
        assert_eq!(index(&q, 1, c[2]), 1);
        assert_eq!(q.used(), [c[2], c[1], c[0]]);
        log();
    }

    #[test]
    fn set_size_fails_while_in_use() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let mut p = EeUtilPool::new(ELEM, HOST);
        assert!(p.set_size(2, 1, 1));
        let a = p.new_object();
        assert!(!p.set_size(8, 1, 1));
        assert!(p.is_set_up());
        p.delete(a);
        assert!(p.set_size(8, 1, 1));
        assert_eq!(log(), ["new[2]", "delete[]", "new[8]"]);
        assert!(p.release());
        log();
    }

    #[test]
    fn the_list_links_like_the_template() {
        let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
        log();
        let pool = ee_util_pool_new(core::mem::size_of::<EeUtilListNode>(), NODE_HOST);
        let mut list = EeUtilList { head: core::ptr::null_mut(), count: 0 };
        let datum = |i: usize| i as *mut c_void;
        unsafe {
            ee_util_list_insert_after(&mut list, pool, core::ptr::null_mut(), datum(1));
            ee_util_list_add_head(&mut list, pool, datum(2));
            let one = ee_util_list_find(&list, datum(1));
            ee_util_list_insert_after(&mut list, pool, one, datum(3));
            ee_util_list_insert_after(&mut list, pool, list.head, datum(4));
            let mut seen = Vec::new();
            let mut n = list.head;
            while !n.is_null() {
                seen.push((*n).data as usize);
                if !(*n).next.is_null() {
                    assert_eq!((*(*n).next).prev, n);
                }
                n = (*n).next;
            }
            assert_eq!(seen, [2, 4, 1, 3]);
            assert_eq!(list.count, 4);
            assert!(ee_util_list_find(&list, datum(9)).is_null());
            ee_util_list_remove(&mut list, pool, ee_util_list_find(&list, datum(2)));
            assert_eq!((*list.head).data as usize, 4);
            assert!((*list.head).prev.is_null());
            ee_util_list_clear(&mut list, pool);
            assert!(list.head.is_null());
            assert_eq!(ee_util_pool_count_used(pool), 0);
            ee_util_pool_free(pool);
        }
        log();
    }
}
