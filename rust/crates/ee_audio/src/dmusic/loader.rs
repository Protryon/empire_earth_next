//! The loader (dmusic's `Loader.c`, `Segment.c`'s and `Band.c`'s downloads): a segment by name, with its styles
//! and its bands' instrument collections, each file read through the resolvers (the conductor's: the music folder)
//! and kept in a cache by its reference's GUID. The collections are SoundFonts: `tools/dls2sf2.py` turned each
//! DirectMusic collection (`X.dls`) into `X.sf2` once, at data build time, so a reference to `X.dls` loads `X.sf2`.

use std::ffi::{c_char, c_void, CString};
use std::io::Cursor;
use std::sync::{Arc, Mutex};

use rustysynth::SoundFont;

use super::files::{
    Band, Guid, Kind, Reference, Segment, Style, INSTRUMENT_GENERAL_MIDI, INSTRUMENT_ROLAND_GS,
    INSTRUMENT_VALID_BANKSELECT, INSTRUMENT_VALID_PATCH, INSTRUMENT_YAMAHA_XG,
};

/// A resolver: a file's bytes by name, or None.
pub type Resolve = Box<dyn FnMut(&str) -> Option<Vec<u8>> + Send>;

/// An instrument collection: a SoundFont, and how its presets are numbered.
pub struct Collection {
    pub guid: Guid,
    pub font: Arc<SoundFont>,
    /// Converted from a DirectMusic collection (`tools/dls2sf2.py`, its INFO's ISFT): the presets carry the whole
    /// DLS bank (MSB << 8 | LSB, drum kits 128) and a missing one is not replaced. A SoundFont of its own (the general
    /// MIDI set built into the program) numbers its variation banks by the MSB alone, and a missing variation or drum
    /// kit plays the capital tone or the standard kit (dmusic's EE patch).
    pub dls_numbering: bool,
}

impl std::fmt::Debug for Collection {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Collection").field("presets", &self.font.get_presets().len()).finish()
    }
}

impl Collection {
    /// Parses a SoundFont (None: not one).
    pub fn parse(bytes: &[u8], guid: Guid) -> Option<Collection> {
        let font = SoundFont::new(&mut Cursor::new(bytes)).ok()?;
        let dls_numbering = font.get_info().get_tools().contains("dls2sf2");
        Some(Collection { guid, font: Arc::new(font), dls_numbering })
    }
}

/// The file a collection reference loads: `X.dls` is `X.sf2`.
pub fn collection_file(file: &str) -> String {
    match file.len().checked_sub(4) {
        Some(n) if file.is_char_boundary(n) && file[n..].eq_ignore_ascii_case(".dls") => format!("{}.sf2", &file[..n]),
        _ => file.to_string(),
    }
}

/// Why a load failed (`DmResult`'s, as far as the game sees them: it only tests for success).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Error {
    NotFound,
    FileCorrupt,
    InvalidArgument,
}

/// `DmLoader` (with `DmLoader_DOWNLOAD`: a segment is downloaded when it is loaded).
#[derive(Default)]
pub struct Loader {
    resolvers: Mutex<Vec<Resolve>>,
    styles: Mutex<Vec<Arc<Style>>>,
    collections: Mutex<Vec<Arc<Collection>>>,
}

/// A C resolver (`DmLoaderResolverCallback`): a `malloc`'d buffer the loader frees, its length written.
pub type CResolve = unsafe extern "C" fn(*mut c_void, *const c_char, *mut usize) -> *mut c_void;

impl Loader {
    pub fn add_resolver(&self, resolve: Resolve) {
        self.resolvers.lock().expect("no panics while locked").push(resolve);
    }

    /// A C resolver and its context; what it returns is copied and freed with the C runtime's `free`.
    ///
    /// # Safety
    /// The callback must stay callable with `context` for the loader's lifetime and return NULL or a `malloc`'d
    /// buffer of the length it writes.
    pub unsafe fn add_c_resolver(&self, resolve: CResolve, context: *mut c_void) {
        let context = context as usize;
        self.add_resolver(Box::new(move |name| {
            let name = CString::new(name).ok()?;
            let mut length = 0usize;
            // SAFETY: the caller's contract
            let data = unsafe { resolve(context as *mut c_void, name.as_ptr(), &mut length) };
            if data.is_null() {
                return None;
            }
            let bytes = unsafe { std::slice::from_raw_parts(data as *const u8, length) }.to_vec();
            unsafe { crate::sound::sys::free(data) };
            Some(bytes)
        }));
    }

    /// `DmLoader_resolveName`: the first resolver that knows the file.
    fn resolve(&self, name: &str) -> Option<Vec<u8>> {
        let mut resolvers = self.resolvers.lock().expect("no panics while locked");
        resolvers.iter_mut().find_map(|r| r(name))
    }

    /// `DmLoader_getSegment`: parsed and downloaded.
    pub fn segment(&self, name: &str) -> Result<Arc<Segment>, Error> {
        let bytes = self.resolve(name).ok_or(Error::NotFound)?;
        let mut segment = Segment::parse(&bytes).ok_or(Error::FileCorrupt)?;
        self.download(&mut segment)?;
        Ok(Arc::new(segment))
    }

    /// `DmSegment_download`: the bands' instruments, and each style with its bands.
    fn download(&self, segment: &mut Segment) -> Result<(), Error> {
        let messages = segment.messages.get_mut().expect("no panics while locked");
        let mut result = Ok(());
        for m in messages.iter_mut() {
            match &mut m.kind {
                Kind::Band(band) => match band {
                    Some(b) => self.download_band(b),
                    None => result = Err(Error::InvalidArgument),
                },
                Kind::Style { reference, style } => match self.style(reference) {
                    Ok(s) => {
                        for b in &s.bands {
                            self.download_band(b);
                        }
                        *style = Some(s);
                    }
                    Err(e) => result = Err(e),
                },
                _ => {}
            }
            if result.is_err() {
                break;
            }
        }
        segment.downloaded = true;
        result
    }

    /// `DmLoader_getStyle`.
    fn style(&self, reference: &Reference) -> Result<Arc<Style>, Error> {
        let file = reference.file.as_deref().ok_or(Error::NotFound)?;
        if let Some(s) = self.styles.lock().expect("no panics while locked").iter().find(|s| s.guid == reference.guid) {
            return Ok(s.clone());
        }
        let bytes = self.resolve(file).ok_or(Error::NotFound)?;
        let style = Arc::new(Style::parse(&bytes).ok_or(Error::FileCorrupt)?);
        self.styles.lock().expect("no panics while locked").push(style.clone());
        Ok(style)
    }

    /// `DmBand_download`: every instrument with a valid patch gets its collection (the general MIDI, GS and XG ones
    /// all from `gm.dls`, the original's one built-in collection); an instrument whose collection is missing stays
    /// silent.
    fn download_band(&self, band: &Band) {
        for instrument in &band.instruments {
            let mut slot = instrument.collection.lock().expect("no panics while locked");
            if slot.is_some() {
                continue;
            }
            if instrument.options & (INSTRUMENT_VALID_PATCH | INSTRUMENT_VALID_BANKSELECT) == 0 {
                continue;
            }
            let mut reference = instrument.reference.clone();
            if instrument.options & (INSTRUMENT_GENERAL_MIDI | INSTRUMENT_ROLAND_GS | INSTRUMENT_YAMAHA_XG) != 0 {
                reference.file = Some("gm.dls".into());
            }
            *slot = self.collection(&reference).ok();
        }
    }

    /// `DmLoader_getDownloadableSound`: the collection (a SoundFont), its GUID the reference's.
    fn collection(&self, reference: &Reference) -> Result<Arc<Collection>, Error> {
        let file = reference.file.as_deref().ok_or(Error::NotFound)?;
        let cache = self.collections.lock().expect("no panics while locked");
        if let Some(c) = cache.iter().find(|c| c.guid == reference.guid) {
            return Ok(c.clone());
        }
        drop(cache);
        let bytes = self.resolve(&collection_file(file)).ok_or(Error::NotFound)?;
        let c = Arc::new(Collection::parse(&bytes, reference.guid).ok_or(Error::FileCorrupt)?);
        self.collections.lock().expect("no panics while locked").push(c.clone());
        Ok(c)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_dls_reference_loads_the_soundfont() {
        assert_eq!(collection_file("Honor.dls"), "Honor.sf2");
        assert_eq!(collection_file("Rivers DLS.DLS"), "Rivers DLS.sf2");
        assert_eq!(collection_file("gm.dls"), "gm.sf2");
        assert_eq!(collection_file("x.sf2"), "x.sf2");
        assert_eq!(collection_file("ls"), "ls");
    }

    #[test]
    fn the_built_in_set_numbers_its_banks_as_a_soundfont() {
        let c = Collection::parse(crate::dmusic::GENERAL_MIDI, [1; 16]).expect("EE.sf2 parses");
        assert!(!c.dls_numbering);
        assert!(!c.font.get_presets().is_empty());
        assert!(Collection::parse(b"RIFF\0\0\0\0sfbk", [0; 16]).is_none());
    }

    #[test]
    fn a_missing_segment_or_style() {
        let loader = Loader::default();
        loader.add_resolver(Box::new(|name| (name == "bad.sgt").then(|| b"nope".to_vec())));
        assert_eq!(loader.segment("none.sgt").unwrap_err(), Error::NotFound);
        assert_eq!(loader.segment("bad.sgt").unwrap_err(), Error::FileCorrupt);
    }
}
