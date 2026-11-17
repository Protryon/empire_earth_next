//! Sanity of the golden file itself: the scenarios exist and are well formed.
#[test]
fn golden_file_has_all_sections() {
    let golden = include_str!("golden_fs.txt");
    for kind in ["mem", "chunk-file", "chunk-load", "chunk-skip", "arch-file", "arch-find", "arch-tag"] {
        assert!(golden.lines().any(|l| l.split('\t').next() == Some(kind)), "missing {kind}");
    }
}
