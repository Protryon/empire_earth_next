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
    call(EE_WON_HOST_PUT_SESSION, [Obj(900), N, N], [0, 0], &[], c);
    assert_eq!(call(EE_WON_HOST_GET_SESSION, [N; 3], [0, 0], &[], c).obj, Obj(900));
    for h in [p0, p, cert, k] {
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
