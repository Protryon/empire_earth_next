//! A saved game's transfer between the lobby game's host and its clients as Rust objects (R6 WONLobby round 7; the
//! original EELobbyFileTransfer.cpp): EELobbyFileTransfer (the host serves the file on a port and sends it to every
//! client that connects; a client checks whether it has the host's file, by size and checksum, and else downloads
//! it, reporting its progress to the lobby game at most once a second) and its send operation SendFileOp (the file
//! in 0x2000-byte chunks over one client's socket).
//!
//! The C++ keeps `EELobbyFileTransfer` as a `RefCount` shell (the lobby game's `SmartPtr`, the completions'
//! references; the host's file name and address) and `SendFileOp` as a `SocketOp` shell whose virtuals forward
//! here. Every call on a socket, an op, the lobby game, a file or the clock is one op of the host
//! (`WONLobby/EELobbyGameHost.cpp`), the original's statements with no decision between them. The listening socket
//! and the clients' sockets are held here (a reference each, given back on release), the clients in pointer order
//! (`std::set<SmartPtr<AsyncSocket>>`'s).

use std::cell::{Cell, RefCell};
use std::collections::BTreeSet;
use std::ffi::c_void;

use crate::status::{WS_FAILED_TO_OPEN_FILE, WS_SUCCESS, WS_TIMED_OUT};

/// The host's ops (`obj` the transfer's shell or the send op's).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeXferOp {
    // ---- the transfer
    /// `EELobbyGetSavedGameInfo(a, *(uint32_t*)b, *(uint32_t*)c)` (`a` a NUL-terminated path).
    SavedGameInfo = 1,
    /// `(unsigned long)PlatTimeMilliseconds64()`.
    Time,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `((EELobbyGame*)a)->UpdateTransferProgress(b, c)`.
    GameProgress,
    /// `((AsyncOp*)a)->Succeeded()` (0 or 1).
    Succeeded,
    /// `static_cast<SocketOp*>(a)->GetSocket()` (no reference).
    OpSocket,
    /// `static_cast<AcceptOp*>(a)->GetAcceptedSocket()` (no reference).
    AcceptedSocket,
    /// `static_cast<RecvChunkOp*>(a)->GetRecvOp()->GetBytesRead()`.
    BytesRead,
    /// `((RefCount*)a)->AddRef()`.
    AddRef,
    /// `((RefCount*)a)->Release()`.
    Release,
    /// `((AsyncSocket*)a)->Kill()`.
    Kill,
    /// `new QueueSocket(0)` with a reference added.
    NewSocket,
    /// `((QueueSocket*)a)->Fn2B0D90(b, 0)`: the status.
    Bind,
    /// `((QueueSocket*)a)->Listen(1000)`: the status.
    Listen,
    /// `a->SetRepeatCompletion(completion(OnClientAccepted)); a->SetRepeatOp(new AcceptOp(NULL))`.
    RepeatAccept,
    /// `a->SetCloseCompletion(completion(OnClientClosed)); a->QueueOp(new SendFileOp(path b, c bytes), -1);
    /// a->QueueOp(new CloseOp(NULL), -1)`.
    QueueSend,
    /// `this->mHost.IsValid()` (0 or 1).
    HostValid,
    /// `a->SetCloseCompletion(completion(OnClientClosed))`; a `RecvBytesOp(0, NULL)` with the completion
    /// `OnFileReceived`, the file path (`b`, `c` bytes) and the progress completion `OnProgress` every 10000 bytes;
    /// `a->QueueOp(new ConnectOp(this->mHost), -1); a->QueueOp(op, -1)`.
    QueueDownload,
    // ---- the send op
    /// `PlatFOpen(a, "rb")` (`a` a NUL-terminated path): the `FILE*` or 0.
    Open = 40,
    /// `fread(b, 1, c, (FILE*)a)`: the count.
    Read,
    /// `fclose((FILE*)a)`.
    Close,
    /// `GetSocket()->WaitForWrite(TimeLeft())`.
    WaitForWrite,
    /// `GetSocket()->Send(a, b, (int*)c)`: the status.
    Send,
    /// `TimeLeft()`.
    TimeLeft,
}

/// The transfer's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeXferCall {
    /// `SetPath(a, b bytes)`.
    SetPath = 1,
    /// `SetFileInfo`'s size `a` and checksum `b` (the shell keeps the name).
    SetFileInfo,
    /// `IsFileHere()` (0 or 1).
    IsFileHere,
    /// `Serve(a)`: the status.
    Serve,
    /// `Stop()`.
    Stop,
    /// `CloseClients()`.
    CloseClients,
    /// `Download()`.
    Download,
    /// the completions' callbacks (`a` the op).
    OnClientClosed,
    OnClientAccepted,
    OnProgress,
    OnFileReceived,
    /// a member: 0 `mLocalSize`, 1 `mLocalChecksum`, 2 `mSize`, 3 `mChecksum`, 4 `mLastProgressTime`,
    /// 5 `mConnection`, 6 the number of clients, 7 `mPath`'s length.
    Field,
}

/// The send op's inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeXferSendCall {
    /// `Start()`: the status.
    Start = 1,
    /// `Continue()`: the status.
    Continue,
    /// `CleanupHook()` (the file closed).
    Cleanup,
}

/// The host: `(object, op, a, b, c) -> result`.
pub type EeWonEeXferHostFn =
    Option<unsafe extern "C" fn(obj: *mut c_void, op: EeWonEeXferOp, a: isize, b: isize, c: isize) -> isize>;

const CHUNK: usize = 0x2000;

fn host_call(host: EeWonEeXferHostFn, obj: *mut c_void, op: EeWonEeXferOp, a: isize, b: isize, c: isize) -> isize {
    match host {
        // SAFETY: the host's contract (an op on its own object)
        Some(h) => unsafe { h(obj, op, a, b, c) },
        None => 0,
    }
}

/// A path with its NUL, as the C++ `std::string`'s `c_str()`.
fn c_path(path: &[u8]) -> Vec<u8> {
    let mut p = path.to_vec();
    p.push(0);
    p
}

/// One transfer.
pub struct EeWonEeXfer {
    obj: *mut c_void,
    host: EeWonEeXferHostFn,
    connection: Cell<isize>,
    clients: RefCell<BTreeSet<usize>>,
    path: RefCell<Vec<u8>>,
    local_size: Cell<u32>,
    local_checksum: Cell<u32>,
    size: Cell<u32>,
    checksum: Cell<u32>,
    last_progress: Cell<u32>,
}

type Op = EeWonEeXferOp;

impl EeWonEeXfer {
    pub fn new(obj: *mut c_void, host: EeWonEeXferHostFn) -> Self {
        EeWonEeXfer {
            obj,
            host,
            connection: Cell::new(0),
            clients: RefCell::new(BTreeSet::new()),
            path: RefCell::new(Vec::new()),
            local_size: Cell::new(0),
            local_checksum: Cell::new(0),
            size: Cell::new(0),
            checksum: Cell::new(0),
            last_progress: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        host_call(self.host, self.obj, op, a, b, c)
    }

    fn set_path(&self, path: &[u8]) {
        *self.path.borrow_mut() = path.to_vec();
        self.local_checksum.set(0);
        self.local_size.set(0);
        let p = c_path(path);
        let mut size = 0u32;
        let mut checksum = 0u32;
        self.op(
            Op::SavedGameInfo,
            p.as_ptr() as isize,
            &mut size as *mut u32 as isize,
            &mut checksum as *mut u32 as isize,
        );
        self.local_size.set(size);
        self.local_checksum.set(checksum);
    }

    fn is_file_here(&self) -> bool {
        self.size.get() == self.local_size.get() && self.checksum.get() == self.local_checksum.get()
    }

    /// A client's socket into the set, with a reference (none when it was there).
    fn insert_client(&self, socket: isize) {
        let fresh = self.clients.borrow_mut().insert(socket as usize);
        if fresh {
            self.op(Op::AddRef, socket, 0, 0);
        }
    }

    fn on_client_closed(&self, op: isize) {
        let socket = self.op(Op::OpSocket, op, 0, 0);
        let had = self.clients.borrow_mut().remove(&(socket as usize));
        if had {
            self.op(Op::Release, socket, 0, 0);
        }
    }

    fn on_client_accepted(&self, op: isize) {
        if self.op(Op::Succeeded, op, 0, 0) == 0 {
            return;
        }
        let socket = self.op(Op::AcceptedSocket, op, 0, 0);
        self.insert_client(socket);
        let path = self.path.borrow().clone();
        self.op(Op::QueueSend, socket, path.as_ptr() as isize, path.len() as isize);
    }

    fn on_progress(&self, op: isize) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game == 0 {
            return;
        }
        let time = self.op(Op::Time, 0, 0, 0) as u32;
        if time.wrapping_sub(self.last_progress.get()) < 1000 {
            return;
        }
        self.last_progress.set(time);
        let read = self.op(Op::BytesRead, op, 0, 0);
        self.op(Op::GameProgress, game, read as i32 as isize, self.size.get() as i32 as isize);
    }

    fn on_file_received(&self, op: isize) {
        let game = self.op(Op::MyGame, 0, 0, 0);
        if game != 0 && self.op(Op::Succeeded, op, 0, 0) != 0 {
            let size = self.size.get() as i32 as isize;
            self.op(Op::GameProgress, game, size, size);
        }
    }

    fn serve(&self, port: u16) -> isize {
        self.stop();
        let socket = self.op(Op::NewSocket, 0, 0, 0);
        self.connection.set(socket);
        let status = self.op(Op::Bind, socket, port as isize, 0);
        if status != 0 {
            return status;
        }
        let status = self.op(Op::Listen, socket, 0, 0);
        if status != 0 {
            return status;
        }
        self.op(Op::RepeatAccept, socket, 0, 0);
        0
    }

    fn stop(&self) {
        self.close_clients();
        let c = self.connection.get();
        if c != 0 {
            self.op(Op::Kill, c, 0, 0);
            self.connection.set(0);
            self.op(Op::Release, c, 0, 0);
        }
    }

    fn close_clients(&self) {
        let clients: Vec<usize> = self.clients.borrow().iter().copied().collect();
        for &s in &clients {
            self.op(Op::Kill, s as isize, 0, 0);
        }
        let clients: Vec<usize> = std::mem::take(&mut *self.clients.borrow_mut()).into_iter().collect();
        for s in clients {
            self.op(Op::Release, s as isize, 0, 0);
        }
    }

    fn download(&self) {
        let empty = self.path.borrow().is_empty();
        if empty || self.op(Op::HostValid, 0, 0, 0) == 0 {
            return;
        }
        let time = self.op(Op::Time, 0, 0, 0) as u32;
        self.last_progress.set(time);
        let socket = self.op(Op::NewSocket, 0, 0, 0);
        let path = self.path.borrow().clone();
        self.op(Op::QueueDownload, socket, path.as_ptr() as isize, path.len() as isize);
        self.insert_client(socket);
        self.op(Op::Release, socket, 0, 0); // the local SmartPtr
    }

    pub fn call(&self, call: EeWonEeXferCall, a: isize, b: isize) -> isize {
        type C = EeWonEeXferCall;
        match call {
            C::SetPath => {
                let path = if a == 0 || b <= 0 {
                    Vec::new()
                } else {
                    // SAFETY: the caller's `b` bytes at `a`
                    unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) }.to_vec()
                };
                self.set_path(&path);
            }
            C::SetFileInfo => {
                self.size.set(a as u32);
                self.checksum.set(b as u32);
            }
            C::IsFileHere => return self.is_file_here() as isize,
            C::Serve => return self.serve(a as u16),
            C::Stop => self.stop(),
            C::CloseClients => self.close_clients(),
            C::Download => self.download(),
            C::OnClientClosed => self.on_client_closed(a),
            C::OnClientAccepted => self.on_client_accepted(a),
            C::OnProgress => self.on_progress(a),
            C::OnFileReceived => self.on_file_received(a),
            C::Field => {
                return match a {
                    0 => self.local_size.get() as isize,
                    1 => self.local_checksum.get() as isize,
                    2 => self.size.get() as isize,
                    3 => self.checksum.get() as isize,
                    4 => self.last_progress.get() as isize,
                    5 => self.connection.get(),
                    6 => self.clients.borrow().len() as isize,
                    7 => self.path.borrow().len() as isize,
                    _ => 0,
                }
            }
        }
        0
    }

    /// The shell's destructor: the members' references given back (the clients, then the listening socket).
    fn release_all(&self) {
        let clients: Vec<usize> = std::mem::take(&mut *self.clients.borrow_mut()).into_iter().collect();
        for s in clients {
            self.op(Op::Release, s as isize, 0, 0);
        }
        let c = self.connection.replace(0);
        if c != 0 {
            self.op(Op::Release, c, 0, 0);
        }
    }
}

/// One send op: the path, the open file, the chunk and its read position.
pub struct EeWonEeXferSend {
    obj: *mut c_void,
    host: EeWonEeXferHostFn,
    path: Vec<u8>,
    file: Cell<isize>,
    buffer: RefCell<Box<[u8; CHUNK]>>,
    pos: Cell<u32>,
    len: Cell<u32>,
}

impl EeWonEeXferSend {
    pub fn new(obj: *mut c_void, host: EeWonEeXferHostFn, path: &[u8]) -> Self {
        EeWonEeXferSend {
            obj,
            host,
            path: path.to_vec(),
            file: Cell::new(0),
            buffer: RefCell::new(Box::new([0; CHUNK])),
            pos: Cell::new(0),
            len: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        host_call(self.host, self.obj, op, a, b, c)
    }

    fn close_file(&self) {
        let f = self.file.get();
        if f != 0 {
            self.op(Op::Close, f, 0, 0);
            self.file.set(0);
        }
    }

    fn read_chunk(&self) -> bool {
        let ptr = self.buffer.borrow_mut().as_mut_ptr() as isize;
        let mut count = self.op(Op::Read, self.file.get(), ptr, CHUNK as isize) as i32;
        if count < 0 {
            count = 0;
        }
        self.pos.set(0);
        self.len.set(count as u32);
        count > 0
    }

    fn start(&self) -> i64 {
        if self.path.is_empty() {
            return WS_FAILED_TO_OPEN_FILE;
        }
        let p = c_path(&self.path);
        let f = self.op(Op::Open, p.as_ptr() as isize, 0, 0);
        self.file.set(f);
        if f == 0 {
            return WS_FAILED_TO_OPEN_FILE;
        }
        self.read_chunk();
        self.cont()
    }

    fn cont(&self) -> i64 {
        loop {
            if self.pos.get() < self.len.get() {
                self.op(Op::WaitForWrite, 0, 0, 0);
                let mut sent: i32 = 0;
                let data = self.buffer.borrow().as_ptr() as isize + self.pos.get() as isize;
                let status =
                    self.op(Op::Send, data, (self.len.get() - self.pos.get()) as isize, &mut sent as *mut i32 as isize)
                        as i64;
                if sent > 0 {
                    self.pos.set(self.pos.get() + sent as u32);
                }
                if status == WS_TIMED_OUT && self.op(Op::TimeLeft, 0, 0, 0) == 0 {
                    return WS_TIMED_OUT;
                }
                if status != WS_SUCCESS {
                    return status;
                }
            } else if !self.read_chunk() {
                self.close_file();
                return WS_SUCCESS;
            }
        }
    }

    pub fn call(&self, call: EeWonEeXferSendCall) -> i32 {
        (match call {
            EeWonEeXferSendCall::Start => self.start(),
            EeWonEeXferSendCall::Continue => self.cont(),
            EeWonEeXferSendCall::Cleanup => {
                self.close_file();
                0
            }
        }) as i32
    }
}

// ---- the C ABI

/// A new transfer for the C++ shell `obj`.
#[no_mangle]
pub extern "C" fn ee_won_eexfer_new(obj: *mut c_void, host: EeWonEeXferHostFn) -> *mut EeWonEeXfer {
    Box::into_raw(Box::new(EeWonEeXfer::new(obj, host)))
}

/// The shell's destructor.
///
/// # Safety
/// `p` came from [`ee_won_eexfer_new`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eexfer_free(p: *mut EeWonEeXfer) {
    if !p.is_null() {
        // SAFETY: the contract
        let x = unsafe { Box::from_raw(p) };
        x.release_all();
    }
}

/// A method of the transfer.
///
/// # Safety
/// `p` is a live transfer; the host's calls may come back into it (a completion run at once).
#[no_mangle]
pub unsafe extern "C" fn ee_won_eexfer_call(p: *const EeWonEeXfer, call: EeWonEeXferCall, a: isize, b: isize) -> isize {
    // SAFETY: the contract; the state is in cells, no borrow is held across a host call
    match unsafe { p.as_ref() } {
        Some(x) => x.call(call, a, b),
        None => 0,
    }
}

/// A new send op for the C++ shell `obj`, of the file at `path` (`len` bytes).
///
/// # Safety
/// `path` holds `len` readable bytes (or `len` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_eexfer_send_new(
    obj: *mut c_void,
    host: EeWonEeXferHostFn,
    path: *const u8,
    len: usize,
) -> *mut EeWonEeXferSend {
    let p = if path.is_null() || len == 0 {
        &[][..]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(path, len) }
    };
    Box::into_raw(Box::new(EeWonEeXferSend::new(obj, host, p)))
}

/// The send op's destructor (the file is not closed: the op's cleanup does that).
///
/// # Safety
/// `p` came from [`ee_won_eexfer_send_new`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eexfer_send_free(p: *mut EeWonEeXferSend) {
    if !p.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// A virtual of the send op.
///
/// # Safety
/// `p` is a live send op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eexfer_send_call(p: *const EeWonEeXferSend, call: EeWonEeXferSendCall) -> i32 {
    // SAFETY: the contract
    match unsafe { p.as_ref() } {
        Some(s) => s.call(call),
        None => 0,
    }
}

#[cfg(test)]
#[path = "eexfer_tests.rs"]
mod tests;
