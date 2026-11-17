use super::*;
use crate::hostop::*;

pub(crate) unsafe extern "C" fn sink(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: a Vec, the callee's bytes
    unsafe { vec_sink(context, data, length) }
}

fn call(code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8], ctx: Option<&Arc<EeWonAuthCtx>>) -> Reply {
    let core = |_: Obj| ctx.cloned();
    answer(code, o, n, data, [1, 2, 3, 4, 5, 6], &core).expect("answered here")
}

fn count(o: Obj) -> usize {
    let i = item(o).unwrap();
    Arc::strong_count(&i) - 1
}

const N: Obj = Obj(0);

#[test]
fn handles_are_tagged_and_counted() {
    let h = handle(Item::Cert(Arc::new(Cert::new(Kind::PubKeyBlock, b"junk"))));
    assert!(is_handle(h) && !is_handle(Obj(0x1000)));
    assert_eq!(count(h), 1);
    call(EE_WON_HOST_ADD_REF, [h, N, N], [0, 0], &[], None);
    assert_eq!(count(h), 2);
    call(EE_WON_HOST_RELEASE, [h, N, N], [0, 0], &[], None);
    assert_eq!(count(h), 1);
    // a C++ object's reference count is the C++ side's
    let core = |_: Obj| None;
    assert!(answer(EE_WON_HOST_RELEASE, [Obj(0x1000), N, N], [0, 0], &[], [0; 6], &core).is_none());
    assert!(answer(EE_WON_HOST_ADD_REF, [Obj(0x1000), N, N], [0, 0], &[], [0; 6], &core).is_none());
    assert!(answer(EE_WON_HOST_SOCKET_KILL, [N, N, N], [0, 0], &[], [0; 6], &core).is_none());
    release(h);
}

#[test]
fn junk_certificates_and_keys_are_invalid() {
    let c = call(EE_WON_HOST_CERT_NEW, [N; 3], [1, 0], b"junk", None).obj;
    assert_eq!(call(EE_WON_HOST_CERT_VALID, [c, N, N], [0, 0], &[], None).num, 0);
    assert!(call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, [c, N, N], [0, 0], b"x", None).bytes.is_none());
    let t = call(EE_WON_HOST_CERT_TIMES, [c, N, N], [0, 0], &[], None).bytes.unwrap();
    assert_eq!(t.len(), 16);
    let k = call(EE_WON_HOST_CERT_PRIV_NEW, [N; 3], [0, 0], &[], None).obj;
    assert_eq!(call(EE_WON_HOST_CERT_PRIV_SET, [k, N, N], [0, 0], b"junk", None).num, 0);
    call(EE_WON_HOST_CERT_PRIV_FREE, [k, N, N], [0, 0], &[], None);
    call(EE_WON_HOST_RELEASE, [c, N, N], [0, 0], &[], None);
}

#[test]
fn new_peer_data_holds_the_given_block_handle() {
    let b = call(EE_WON_HOST_CERT_NEW_BLOCK, [N; 3], [0, 0], b"junk", None).obj;
    let k = call(EE_WON_HOST_CERT_PRIV_NEW, [N; 3], [0, 0], &[], None).obj;
    let p = call(EE_WON_HOST_CERT_NEW_PEER, [N, b, N], [k.0 as i64, 9], &[], None).obj;
    assert_eq!(count(b), 2, "the peer data's key block");
    assert_eq!(call(EE_WON_HOST_CERT_PEER_BLOCK, [p, N, N], [0, 0], &[], None).obj, b, "borrowed");
    assert_eq!(count(b), 2);
    assert_eq!(call(EE_WON_HOST_CERT_PEER_DELTA, [p, N, N], [0, 0], &[], None).num, 9);
    assert_eq!(call(EE_WON_HOST_CERT_PEER_DELTA, [b, N, N], [0, 0], &[], None).num, 0, "a block's is 0");
    // the C++ shim's view of it
    let o = peer_obj(p);
    // SAFETY: the reference just made
    unsafe {
        assert_eq!(peerobj::ee_won_authobj_num(o, EE_WON_AUTHOBJ_PEER_DELTA, 0, 0, 0), 9);
        peerobj::ee_won_authobj_free(o);
    }
    assert!(peer_obj(b).is_null() && peer_obj(Obj(0x1000)).is_null());
    call(EE_WON_HOST_RELEASE, [p, N, N], [0, 0], &[], None);
    assert_eq!(count(b), 1, "released with the peer data");
    release(b);
    release(k);
}

#[test]
fn the_context_calls_reach_the_rust_context() {
    let ctx = Arc::new(EeWonAuthCtx::new(crate::authobj::tests::hooks(), b"none/_wonlogin.ks"));
    let c = Some(&ctx);
    let p0 = call(EE_WON_HOST_CERT_CTX_PEER, [N; 3], [0, 0], &[], c).obj;
    let b0 = call(EE_WON_HOST_CERT_PEER_BLOCK, [p0, N, N], [0, 0], &[], c).obj;
    assert!(is_handle(b0));
    assert_eq!(call(EE_WON_HOST_CERT_BLOCK_VALID, [b0, N, N], [0, 0], &[], c).num, 0);
    // a peer data with a delta, set and read back
    let k = call(EE_WON_HOST_CERT_PRIV_NEW, [N; 3], [0, 0], &[], c).obj;
    let p = call(EE_WON_HOST_CERT_NEW_PEER, [N, b0, N], [k.0 as i64, 42], &[], c).obj;
    call(EE_WON_HOST_CERT_SET_PEER, [N, p, N], [0, 0], &[], c);
    assert_eq!(ctx.peer_data().unwrap().delta, 42);
    let pd = call(EE_WON_HOST_PEER_CTX_DATA, [N; 3], [0, 0], &[], c).num as usize as *mut peerobj::EeWonAuthObj;
    // SAFETY: the reference the call made
    unsafe {
        assert_eq!(peerobj::ee_won_authobj_num(pd, EE_WON_AUTHOBJ_PEER_DELTA, 0, 0, 0), 42);
        peerobj::ee_won_authobj_free(pd);
    }
    // a certificate is no peer data: none
    let cert = call(EE_WON_HOST_CERT_NEW, [N; 3], [2, 0], b"", c).obj;
    call(EE_WON_HOST_CERT_SET_PEER, [N, cert, N], [0, 0], &[], c);
    assert!(ctx.peer_data().is_none());
    assert!(call(EE_WON_HOST_CERT_CTX_PEER, [N; 3], [0, 0], &[], c).obj.is_none());
    assert_eq!(call(EE_WON_HOST_CERT_USER_ID, [N; 3], [0, 0], &[], c).num, 0);
    // from a block: a new empty peer data over it
    call(EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK, [N, b0, N], [0, 0], &[], c);
    let pb = ctx.peer_data().unwrap();
    assert!(Arc::ptr_eq(pb.block.as_ref().unwrap(), &cert_of(b0).unwrap()));
    // the login's texts
    call(EE_WON_HOST_CERT_SET_PASSWORD, [N; 3], [0, 0], b"p\0w\0x", c);
    assert_eq!(call(EE_WON_HOST_CERT_PASSWORD, [N; 3], [0, 0], &[], c).bytes.unwrap(), b"p\0w\0");
    assert_eq!(call(EE_WON_HOST_CERT_USER, [N; 3], [0, 0], &[], c).bytes.unwrap(), b"");
    assert_eq!(call(EE_WON_HOST_HAS_AUTH, [N; 3], [0, 0], &[], c).num, 1);
    // sessions by the call's address
    assert!(call(EE_WON_HOST_GET_SESSION, [N; 3], [0, 0], &[], c).obj.is_none());
    let s = new_session(1, 5, unix_now());
    call(EE_WON_HOST_PUT_SESSION, [s, N, N], [0, 0], &[], c);
    assert_eq!(count(s), 2, "the cache's reference");
    assert_eq!(call(EE_WON_HOST_GET_SESSION, [N; 3], [0, 0], &[], c).obj, s);
    assert_eq!(count(s), 2, "the cache's reference is the caller's");
    for h in [p0, p, cert, k, s, s] {
        release(h);
    }
}

#[test]
fn without_a_context_the_calls_answer_nothing() {
    for code in
        [EE_WON_HOST_HAS_AUTH, EE_WON_HOST_CERT_USER_ID, EE_WON_HOST_PEER_CTX_DATA, EE_WON_HOST_PEER_REFRESH_BLOCK]
    {
        assert_eq!(call(code, [N; 3], [0, 0], &[], None).num, 0);
    }
    assert!(call(EE_WON_HOST_CERT_CTX_PEER, [N; 3], [0, 0], &[], None).obj.is_none());
    assert!(call(EE_WON_HOST_CERT_USER, [N; 3], [0, 0], &[], None).bytes.is_none());
    assert!(call(EE_WON_HOST_GET_SESSION, [N; 3], [0, 0], &[], None).obj.is_none());
}

fn new_session(auth_type: i32, id: u16, at: i64) -> Obj {
    let mut key = crate::blowfish::Blowfish::default();
    assert!(key.set_key(b"0123456789abcdef"));
    let s = Box::into_raw(Box::new(EeWonAuthSession::new(auth_type, id, key, 4, at)));
    call(hostop::EE_WON_HOST_PEER_NEW_SESSION, [N; 3], [s as usize as i64, 0], &[], None).obj
}

unsafe extern "C" fn keep(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: the Vec the test passed; the callee's bytes
    let v = unsafe { &mut *(context as *mut Vec<u8>) };
    *v = unsafe { crate::ffi::bytes(data, length) }.to_vec();
}

fn crypt(session: Obj, decrypt: bool, msg: &[u8]) -> (i32, Option<Vec<u8>>) {
    let mut out = Vec::new();
    let mut replaced = false;
    // SAFETY: a handle (or none), the bytes, a Vec sink, a flag
    let st = unsafe {
        ee_won_session_crypt(
            session.ptr(),
            decrypt,
            msg.as_ptr(),
            msg.len(),
            keep,
            &mut out as *mut Vec<u8> as *mut c_void,
            &mut replaced,
        )
    };
    (st, replaced.then_some(out))
}

#[test]
fn a_new_session_is_a_handle_owning_the_ops_session() {
    assert!(call(hostop::EE_WON_HOST_PEER_NEW_SESSION, [N; 3], [0, 0], &[], None).obj.is_none());
    let s = new_session(1, 0x1234, 77);
    assert!(is_handle(s) && count(s) == 1);
    assert_eq!(session_last_use(s), 77);
    assert_eq!(session_last_use(N), 0, "not a session");
    // the op's GetSession(): a new reference; its key's bytes
    let r = op_session(hostop::EE_WON_HOST_PEER_AUTH_SESSION, s);
    assert_eq!((r.obj, count(s)), (s, 2));
    release(r.obj);
    let k = op_session(hostop::EE_WON_HOST_PEER_AUTH_KEY, s);
    assert_eq!(k.bytes.as_deref(), Some(&b"0123456789abcdef"[..]));
    // no session: nothing
    let none = op_session(hostop::EE_WON_HOST_PEER_AUTH_SESSION, N);
    assert!(none.obj.is_none() && none.bytes.is_none());
    assert!(op_session(hostop::EE_WON_HOST_PEER_AUTH_KEY, N).bytes.is_none());
    release(s);
}

#[test]
fn the_transform_encrypts_and_decrypts_through_the_handle() {
    let (a, b) = (new_session(1, 0x1234, 0), new_session(1, 0x1234, 0));
    // CryptTransform's reference
    // SAFETY: live handles
    unsafe { ee_won_session_hold(a.ptr()) };
    assert_eq!(count(a), 2);
    let plain = [9u8, 0, 0, 0, b'h', b'e', b'l', b'l', b'o'];
    let (st, enc) = crypt(a, false, &plain);
    let enc = enc.expect("encrypted");
    assert_eq!((st, enc[4], enc[5], enc[6]), (0, 12, 0x34, 0x12), "the session's header and id");
    assert!(session_last_use(a) > 0, "the use time is now");
    // a received message comes without its length field
    let (st, dec) = crypt(b, true, &enc[4..]);
    assert_eq!((st, dec.as_deref()), (0, Some(&b"hello"[..])));
    // the same message again: the sequence number is behind
    assert_eq!(crypt(b, true, &enc[4..]).0, 100086);
    // anything but a session leaves the message
    assert_eq!(crypt(N, false, &plain), (0, None));
    // SAFETY: the transform's reference
    unsafe { ee_won_session_release(a.ptr()) };
    assert_eq!(count(a), 1);
    release(a);
    release(b);
}
