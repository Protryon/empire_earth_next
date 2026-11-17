use super::*;
use crate::hostop::*;

const N: Obj = Obj(0);

/// A new empty directory under the system's temporary directory, '/' separated, with its separator.
fn scratch(name: &str) -> String {
    let d = std::env::temp_dir().join(format!("ee_won_httpcache_{}_{}", name, std::process::id()));
    let _ = std::fs::remove_dir_all(&d);
    std::fs::create_dir_all(&d).unwrap();
    format!("{}/", d.to_string_lossy().replace('\\', "/"))
}

fn call(code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8]) -> Reply {
    answer(code, o, n, data).expect("answered here")
}

fn new_cache(path: &str) -> Obj {
    hostauth::handle(Item::HttpCache(HttpCache::new(path.as_bytes(), false)))
}

fn entry(cache: Obj, location: &str) -> Obj {
    call(EE_WON_HOST_CACHE_ENTRY, [cache, N, N], [0, 0], location.as_bytes()).obj
}

fn get(e: Obj, what: i64) -> Reply {
    call(EE_WON_HOST_ENTRY_GET, [e, N, N], [what, 0], &[])
}

fn set(e: Obj, what: i64, n: i64, data: &[u8]) {
    call(EE_WON_HOST_ENTRY_SET, [e, N, N], [what, n], data);
}

fn text(r: Reply) -> String {
    String::from_utf8(r.bytes.unwrap()).unwrap()
}

#[test]
fn entries_get_free_ids_and_numbered_files() {
    let d = scratch("ids");
    let c = new_cache(&format!("{d}c"));
    assert!(std::path::Path::new(&format!("{d}c")).is_dir());
    let a = entry(c, "http://h/news/a.txt");
    let a2 = entry(c, "http://h/news/a.txt");
    assert_eq!(a, a2, "one location, one entry");
    let b = entry(c, "noslash");
    assert_eq!(text(get(a, 2)), format!("{d}c/000001.txt"));
    assert_eq!(text(get(b, 2)), format!("{d}c/000002.dat"));
    assert_eq!(get(a, 0).num, 0);
    assert_eq!(get(a, 1).num, 0);
    assert_eq!(text(get(a, 3)), "");
    for o in [a, a2, b, c] {
        hostauth::release(o);
    }
}

#[test]
fn the_index_keeps_valid_recent_entries_and_deletes_the_rest() {
    let d = scratch("index");
    let c = new_cache(&format!("{d}c/"));
    let a = entry(c, "http://h/a.png");
    let b = entry(c, "http://h/b.png");
    set(a, 0, 1, &[]);
    set(a, 1, 0x1_0000_0003, &[]);
    set(a, 2, 0, b"tag");
    std::fs::write(text(get(a, 2)), b"a").unwrap();
    std::fs::write(text(get(b, 2)), b"b").unwrap();
    // the cache's last reference: the index written, b's file deleted
    hostauth::release(c);
    assert!(std::path::Path::new(&text(get(a, 2))).exists());
    assert!(!std::path::Path::new(&text(get(b, 2))).exists());
    hostauth::release(a);
    hostauth::release(b);
    let c = new_cache(&format!("{d}c"));
    let a = entry(c, "http://h/a.png");
    assert_eq!(get(a, 0).num, 1);
    // the file holds 32 bits of the time
    assert_eq!(get(a, 1).num, 3);
    assert_eq!(text(get(a, 3)), "tag");
    // b's id is free again
    let b = entry(c, "http://h/b2.png");
    assert_eq!(text(get(b, 2)), format!("{d}c/000002.png"));
    for o in [a, b, c] {
        hostauth::release(o);
    }
}

#[test]
fn a_stale_file_at_a_new_entrys_name_is_deleted() {
    let d = scratch("stale");
    std::fs::create_dir_all(format!("{d}c")).unwrap();
    std::fs::write(format!("{d}c/000001.txt"), b"old").unwrap();
    let c = new_cache(&format!("{d}c"));
    let a = entry(c, "http://h/x.txt");
    assert!(!std::path::Path::new(&format!("{d}c/000001.txt")).exists());
    hostauth::release(a);
    hostauth::release(c);
}

#[test]
fn only_handles_of_the_right_kind_are_answered() {
    let d = scratch("kinds");
    let c = new_cache(&format!("{d}c"));
    let e = entry(c, "x");
    // a C++ object goes to the C++ side
    assert!(answer(EE_WON_HOST_CACHE_ENTRY, [Obj(0x1000), N, N], [0, 0], b"x").is_none());
    // an entry is no cache, a cache no entry
    assert!(answer(EE_WON_HOST_CACHE_ENTRY, [e, N, N], [0, 0], b"x").is_none());
    assert!(answer(EE_WON_HOST_ENTRY_GET, [c, N, N], [0, 0], &[]).is_none());
    assert_eq!(call(EE_WON_HOST_CACHE_UPDATE, [c, e, N], [0, 0], &[]).num, 0);
    hostauth::release(e);
    hostauth::release(c);
}

#[test]
fn the_global_cache_is_made_once_and_reset() {
    let d = scratch("global");
    // SAFETY: the bytes of a string
    let g = unsafe { ee_won_http_cache_global(d.as_ptr(), d.len()) };
    // SAFETY: as above
    assert_eq!(g, unsafe { ee_won_http_cache_global(d.as_ptr(), d.len()) });
    assert!(std::path::Path::new(&format!("{d}_wonHTTPCache")).is_dir());
    let e = entry(Obj::of(g), "http://h/i.bmp");
    set(e, 0, 1, &[]);
    hostauth::release(e);
    ee_won_http_cache_reset_global();
    assert!(std::path::Path::new(&format!("{d}_wonHTTPCache/cachemap.dat")).exists());
}
