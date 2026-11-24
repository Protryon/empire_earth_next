//! The C++ objects the screens' one-statement shims keep between their calls (UI lane round 15): the C++ files'
//! scratch statics (a string the next shim prints into, a list a case fills and the next one hands on, a point,
//! a file spec) are slots of a Rust-owned table.
//!
//! `ee_screens_scratch(slot, size, align, make)` gives the slot's object: the memory is Rust's (allocated zeroed
//! on the slot's first use, never freed), the C++ object is made in it by `make` (the type's default constructor:
//! `EEScratch<T>`, ee/EEScreensShims.h; none for a plain type, whose zeros are its value, as a static's). The
//! slot's first use runs under the table's lock, so two threads that reach a slot at once make it once.

use std::alloc::{alloc_zeroed, Layout};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Mutex;

/// The scratch objects: one slot per C++ object the shims keep between calls (each the type named), by the file
/// that uses it.
///
/// cbindgen:enum-class=false
#[repr(u32)]
#[allow(non_camel_case_types, clippy::enum_variant_names)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeScreensScratch {
    /// EEAEManager `std::vector<EEComplexUnit*>` (sAEMUnits): the living units around the location
    kScratchAEMUnits,
    /// EEAEManager `U2DRectangle<long>` (sAEMRect)
    kScratchAEMRect,
    /// EEAEManager `EEAreaEffectData` (sAESData): the record being read
    kScratchAESData,
    /// EEAEManager `unsigned long` (sAESRead)
    kScratchAESRead,
    /// EEAreaEffect `EETileCoordinate` (sAETile): the continent's tile
    kScratchAETile,
    /// EEAreaEffect `U2DRectangle<long>` (sAERect): the morale buildings' rectangle
    kScratchAERect,
    /// EEAreaEffect `std::vector<EEComplexUnit*>` (sAEUnits): and the units collected in it
    kScratchAEUnits,
    /// EECBCivilizationFiles `unsigned long` (sCFCost): an upgrade's cost (CanAfford's)
    kScratchCFCost,
    /// EECBCivilizationFiles `UWideString` (sCFFormat): the refusal's format and line
    kScratchCFFormat,
    /// EECBCivilizationFiles `UWideString` (sCFText)
    kScratchCFText,
    /// EECBCivilizationFiles `std::vector<unsigned char>` (sCivlBytes): the file's bytes (the original's local)
    kScratchCivlBytes,
    /// EECBCivilizationFiles `unsigned long` (sCivlGot)
    kScratchCivlGot,
    /// EECivCapture `std::vector<long>` (sCivcTypes): a unit set's types (the original's collector's)
    kScratchCivcTypes,
    /// EECivCapture `std::vector<long>` (sCivcIDs): a tech tree's node ids
    kScratchCivcIDs,
    /// EECivCapture `UWideString` (sCivcName): a premade file's name and upgrades as loaded
    kScratchCivcName,
    /// EECivCapture `std::vector<EECivUpgrade>` (sCivcLoaded)
    kScratchCivcLoaded,
    /// EECivilizationBuilder `FSPath*` (sCBPath): the file list's search path being made
    kScratchCBPath,
    /// EECivilizationBuilder `FSFileSpec` (sCBSpec): its directory
    kScratchCBSpec,
    /// EECivilizationBuilder `UWideString` (sCBString): a string read, printed or loaded
    kScratchCBString,
    /// EECivilizationBuilder `UWideString` (sCBFormat): a format read
    kScratchCBFormat,
    /// EECivilizationBuilder `unsigned long` (sCBWritten): the civilization file's bytes written
    kScratchCBWritten,
    /// EECivilizationBuilder `std::vector<EECivUpgrade>` (sCBUpgrades): a request's or a loaded civilization's upgrades
    kScratchCBUpgrades,
    /// EECivilizationBuilder `std::vector<unsigned long>` (sCBPlayers): the players whose civilization is reloaded
    kScratchCBPlayers,
    /// EECivilizationBuilder `GERectangle` (sCBRect): the list line's rectangle
    kScratchCBRect,
    /// EECivilizationManager `unsigned char[0x1000]` (sCivmChunk): a chunk read (the original's local)
    kScratchCivmChunk,
    /// EECivilizationManager `unsigned long` (sCivmRead)
    kScratchCivmRead,
    /// EECivilizationManager `unsigned char` (sCivmByte)
    kScratchCivmByte,
    /// EEScreens `EECivilization` (sCivilization): 0x962f08: the civilization the editors' sub-menu opens the builder
    /// on
    kScratchCivilization,
    /// EEScreens `UWideString` (sCampaignError): the last campaign load's error (EE_SCREENS_TEXT_CAMPAIGN_ERROR)
    kScratchCampaignError,
    /// EEScreens `UIForm*` (sSaveObject)
    kScratchSaveObject,
    /// EEScreens `EEGameOptionsPlayer*` (sLobbyBoot): the boot question's slot copy (its answer frees it)
    kScratchLobbyBoot,
    /// EEScreens `EEPSaveGameHeader*` (sLobbyHeader): the load screen's header (the next load replaces it)
    kScratchLobbyHeader,
    /// EEScreens `FSFileSpec*` (sLobbyFile): its file (the transfers read it; never freed, as the original's)
    kScratchLobbyFile,
    /// EEScreens `EEGameOptions*` (sLobbyCopy): the game type's copy of the options (a random map's)
    kScratchLobbyCopy,
    /// EEScreens `UWideString` (sLobbyMissing): the last options check's missing lines, each ended by a line break
    kScratchLobbyMissing,
    /// EEScreens `unsigned long` (sLobbyStartTime): 0x562fd0: the lobby's Start time (provisional name)
    kScratchLobbyStartTime,
    /// EEScreens `FSFileSpec*` (sLLCheck)
    kScratchLLCheck,
    /// EEScreens `EEGameOptions` (sLLOptions)
    kScratchLLOptions,
    /// EEScreens `EEGameOptions` (sLLCurrent)
    kScratchLLCurrent,
    /// EEScreens `std::vector<UWideString>` (sLLMissing)
    kScratchLLMissing,
    /// EEScreens `std::vector<EEComplexUnit*>` (sHotKeyUnits)
    kScratchHotKeyUnits,
    /// EEScreens `GE3DPoint` (sHotKeyTarget): the aircraft's target (a local of the original)
    kScratchHotKeyTarget,
    /// EEScreens `UWideString` (sChatLogString): the last read's string and colour (ee_screenscpp_cl_last)
    kScratchChatLogString,
    /// EEScreens `GEColor` (sChatLogColour)
    kScratchChatLogColour,
    /// EEScreens `unsigned long` (sChatLogValue): a write's value
    kScratchChatLogValue,
    /// EEScreens `float[4]` (sPgsRectangle): the rectangle register (left, top, right, bottom)
    kScratchPgsRectangle,
    /// EEScreens `NEProtocol*` (sPgsProtocol): the reports' protocol
    kScratchPgsProtocol,
    /// EEScreens `EECampaignFormManager*` (sPgsCampaign): Exit's campaign manager
    kScratchPgsCampaign,
    /// EEScreens `std::vector<EECivilizationEntry>` (sPdcsCivs): EE_SCREENS_GAME_PDCS_CIVS's, for the names and files
    kScratchPdcsCivs,
    /// EEScreens `UWideString` (sPdcsName): the last EE_SCREENS_TEXT_PDCS_LOAD's civilization
    kScratchPdcsName,
    /// EEScreens `std::vector<EECivUpgrade>` (sPdcsUpgrades)
    kScratchPdcsUpgrades,
    /// EEScreens `EESoundObject*` (sDebugSound): EE_SCREENS_GAME_DSU_SOUND's, for EE_SCREENS_GAME_DSU_VOLUME
    kScratchDebugSound,
    /// EEScreens `EEComplexUnit*` (sDsuUnit): EE_SCREENS_GAME_DSU_SOUND's first selected unit (a local of the original
    /// case)
    kScratchDsuUnit,
    /// EEScreens `UWideString` (sCampaignEditorTranslated): the last EE_SCREENS_TEXT_CE_TRANSLATE's
    kScratchCampaignEditorTranslated,
    /// EEScreens `EECampaign` (sCampaignEditorCampaign): 0x562df4 (gCampaignEditor's campaign)
    kScratchCampaignEditorCampaign,
    /// EEScreens `UWideString` (sCampaignEditorKept): the last string or error
    kScratchCampaignEditorKept,
    /// EEScreens `std::vector<UWideString>` (sCampaignEditorLines): the last scenario list
    kScratchCampaignEditorLines,
    /// EEScreens `std::set<UString>` (sCampaignEditorFiles): Reset's listed files (one set for both lists, as Reset's)
    kScratchCampaignEditorFiles,
    /// EEScreens `FSPath*[2]` (sCampaignEditorPaths): Reset's campaign and scenario paths
    kScratchCampaignEditorPaths,
    /// EEScreens `FSFileSpec` (sCampaignEditorDirectory): Reset's directory
    kScratchCampaignEditorDirectory,
    /// EEScreens `FSFileSpec*` (sCampaignEditorSpec): Reset's found campaign file (FindFile's out)
    kScratchCampaignEditorSpec,
    /// EEScreens `FSMemoryFile*` (sCampaignEditorFile): Reset's archived scenario
    kScratchCampaignEditorFile,
    /// EEScreens `std::vector<UWideString>` (sCEMScenarios): SET_SCENARIOS' lines (the original's local)
    kScratchCEMScenarios,
    /// EEScreens `FSFileSpec` (sCEMDelete): DELETE's file
    kScratchCEMDelete,
    /// EEScreens `unsigned char` (sCEMByte): CHECK's byte read
    kScratchCEMByte,
    /// EEScreens `UWideString` (sCEMFormat): CHECK's message format
    kScratchCEMFormat,
    /// EEScreens `UWideString` (sCEMMessage): and the message
    kScratchCEMMessage,
    /// EEScreens `std::vector<UString>` (sCEMFileList): the files' text (Reset's set in order)
    kScratchCEMFileList,
    /// EEScreens `EEWonNetGameInfo` (sWonInfo): the lobby's WON game info between its shims (a local of the original
    /// cases)
    kScratchWonInfo,
    /// EEScreens `UWideString` (sShimMessage): a validation's message between its shims (a local of the original case)
    kScratchShimMessage,
    /// EEScreens `NEProtocol*` (sShimProtocol): the protocol between its shims (a local of the original case)
    kScratchShimProtocol,
    /// EEScreens `EEStatisticsSeries` (sPgsSeries): the series a record's shims read (a copy: GetSeries')
    kScratchPgsSeries,
    /// EEScreens `ISEntity*` (sScreensMouse): the mouse axis a case reads (EEScreensMouseEntity's local)
    kScratchScreensMouse,
    /// EEScreens `UWideString` (sPgsFileName): PGS_CAMPAIGN_LOAD's file name and error
    kScratchPgsFileName,
    /// EEScreens `UWideString` (sPgsError)
    kScratchPgsError,
    /// EEScreens `std::vector<UWideString>` (sCELines): CE_APPEND's lines
    kScratchCELines,
    /// EEScreens `char[32]` (sServerHost): PGS_SERVER's host and port
    kScratchServerHost,
    /// EEScreens `char[20]` (sServerPort)
    kScratchServerPort,
    /// EEScreens `std::set<UString>` (sTextFiles): CAMPAIGN_FILES' set, path, directory and the set in order
    kScratchTextFiles,
    /// EEScreens `FSPath*` (sTextPath)
    kScratchTextPath,
    /// EEScreens `FSFileSpec` (sTextDirectory)
    kScratchTextDirectory,
    /// EEScreens `std::vector<UString>` (sTextLines)
    kScratchTextLines,
    /// EEScreens `std::vector<UWideString>` (sTextList): ADAPTERS', CS_SCENARIOS' and CS_COMPLETED's lists
    kScratchTextList,
    /// EEScreens `NEProtocol*` (sAdaptersProtocol)
    kScratchAdaptersProtocol,
    /// EEScreens `char[16][128]` (sOutputNames): OUTPUT_DEVICES' names
    kScratchOutputNames,
    /// EEScreens `EECampaignProgress*` (sTextProgress): CS_COMPLETED's progress
    kScratchTextProgress,
    /// EEScreens `UWideString` (sTextFormat): CS_PROGRESS' format and text
    kScratchTextFormat,
    /// EEScreens `UWideString` (sTextOut)
    kScratchTextOut,
    /// EEUIBGenericBackground `GETextureCoordinate[4]` (sGBCoordinates): a piece's coordinates (the original's local)
    kScratchGBCoordinates,
    /// EEUIBGenericBackground `FSFileSpec*` (sMBFile)
    kScratchMBFile,
    /// EEUIBGenericBackground `FSFileSpec` (sMBDirectory)
    kScratchMBDirectory,
    /// EEUIBGenericBackground `GESurface*` (sMBImage)
    kScratchMBImage,
    /// EEUIBGenericBackground `GESurface*` (sMBCell)
    kScratchMBCell,
    /// EEUIBGenericBackground `GESurfaceDescription` (sMBDescription)
    kScratchMBDescription,
    /// EEUIBGenericBackground `GESurfaceDescription` (sMBCellDescription)
    kScratchMBCellDescription,
    /// EEUIBGenericBackground `unsigned long` (sMBBits)
    kScratchMBBits,
    /// EEUIBGenericBackground `GERectangle` (sMBSource): a cell's copy
    kScratchMBSource,
    /// EEUIBGenericBackground `GERectangle` (sMBDestination): a cell's copy
    kScratchMBDestination,
    /// EEUICCommandUnits `GEColor` (sCUAmbientColor): the arrow's draw: the rasterizer's state put back after it
    kScratchCUAmbientColor,
    /// EEUICCommandUnits `GEViewport` (sCUViewport)
    kScratchCUViewport,
    /// EEUICCommandUnits `GETransformation` (sCUTransformation)
    kScratchCUTransformation,
    /// EEUICCommandUnits `GE3DPoint` (sCUNear): the ground point: the mouse's line
    kScratchCUNear,
    /// EEUICCommandUnits `GE3DPoint` (sCUFar)
    kScratchCUFar,
    /// EEUICCommandUnits `GE3DPoint` (sCUNearWorld)
    kScratchCUNearWorld,
    /// EEUICCommandUnits `GE3DPoint` (sCUFarWorld)
    kScratchCUFarWorld,
    /// EEUICCommandUnits `GE3DLine` (sCULine)
    kScratchCULine,
    /// EEUICCommandUnits `float` (sCUT)
    kScratchCUT,
    /// EEUICCommandUnits `GE3DPoint` (sCUTarget): the ground point, the point at the mouse
    kScratchCUTarget,
    /// EEUICCommandUnits `EETile*` (sCUTile)
    kScratchCUTile,
    /// EEUICCommandUnits `GERectangle` (sCUBounds): the selected group's bounds and centre
    kScratchCUBounds,
    /// EEUICCommandUnits `GE2DPoint` (sCUCenter)
    kScratchCUCenter,
    /// EEUICCommandUnits `GETextureDescription` (sCUDescription): the arrow texture's description
    kScratchCUDescription,
    /// EEUICPortrait `UWideString*` (sPortraitHelp)
    kScratchPortraitHelp,
    /// EEUICPortrait `UWideString` (sPTFormat): the tooltip's format
    kScratchPTFormat,
    /// EEUICPortrait `UWideString` (sPTName): the tooltip's name
    kScratchPTName,
    /// EEUIFError `GERectangle` (sDialogRectangle)
    kScratchDialogRectangle,
    /// EEUIFError `GEFont*` (sDialogFont)
    kScratchDialogFont,
    /// EEUIFError `float[9]` (sDialogFloats): the constructor's sizes, colours and layout (EE_SCREENS_DLG_FLOAT)
    kScratchDialogFloats,
    /// EEUIFError `UWideString` (sDialogText): a button's text (a local of the original case)
    kScratchDialogText,
    /// EEUIFError `UILayoutUtility` (sDialogLayout): the layout's and the background's layout (locals of the original
    /// cases)
    kScratchDialogLayout,
    /// EEUIFMGStatusBox `UWideString` (sSBSuffix): the gate's lock state (the original's local)
    kScratchSBSuffix,
    /// EEUIFMGStatusBox `GETextureDescription` (sSBReload): ReloadIcons' description
    kScratchSBReload,
    /// EEUIFMGStatusButton `UWideString` (sSBTString): the chat line's string (the original's local)
    kScratchSBTString,
    /// EEUIFMainGame `GE3DPoint` (sUHPoint): the point's out (a local of the original case)
    kScratchUHPoint,
    /// EEUIFMainGame `EETile*` (sShimTile): the mouse tile's other out (a local of the original case)
    kScratchShimTile,
    /// EEUIFMainGame `bool` (sCurShielded): EECanConvert's in-out shielded flag (a local of the original case)
    kScratchCurShielded,
    /// EEUIFMainGame `EETileCoordinate` (sMFTile): FIND_GATE_WALL's tile, PREVIEW_WALL's tiles
    kScratchMFTile,
    /// EEUIFMainGame `std::vector<EETileCoordinate>` (sMFTiles)
    kScratchMFTiles,
    /// EEUIFMainGame `UWideString` (sMFFormat): TEXT_WONDER's format
    kScratchMFFormat,
    /// EEUIFMainGame `GE3DPoint` (sMFPoint): MD_POINT's point
    kScratchMFPoint,
    /// EEUIFMainGame `bool` (sMFInRange): MD_CAN_CONVERT's in-range flag
    kScratchMFInRange,
    /// EEUIManager `ISEntity*` (sUMKey): the key um_key_load found
    kScratchUMKey,
    /// EEUIManager `GERectangle` (sUMParent): CreateUserDefinedControl's window and control rectangles
    kScratchUMParent,
    /// EEUIManager `GERectangle` (sUMRect): CreateUserDefinedControl's window and control rectangles
    kScratchUMRect,
    /// EEUIManager `std::vector<UWideString>` (sUMStrings): and its strings
    kScratchUMStrings,
    /// EEUIMiniMap `GEBitmap*` (sMapBitmap): 0x565328 the terrain texture
    kScratchMapBitmap,
    /// EEUIMiniMap `GEBitmap*` (sWorldBitmap): 0x56532c the units' texture (twice the scale)
    kScratchWorldBitmap,
    /// EEUIMiniMap `GEBitmap*` (sFlareBitmap): 0x565330 "textures\Flare"
    kScratchFlareBitmap,
    /// EEUIMiniMap `GEColor` (sFlareBlinkColor): 0x5652f8 (1, 1, 1)
    kScratchFlareBlinkColor,
    /// EEUIMiniMap `GETextureDescription` (sMMDescription): LoadMedia's description
    kScratchMMDescription,
    /// EEUIMiniMap `GERectangle` (sMMViewRect): DrawPolygons' kept view rectangle
    kScratchMMViewRect,
    /// EEUIMiniMap `GEViewport` (sMMViewport): and its viewport (the original's locals)
    kScratchMMViewport,
    /// EEUIShell `EETile*` (sShimTile): the paint areas' pick: its tile and point (locals of the original case)
    kScratchUIShellShimTile,
    /// EEUIShell `GE3DPoint` (sShimPoint)
    kScratchShimPoint,
    /// EEUIShell `EEMapTile*` (sShimMapTile): the map tile read's
    kScratchShimMapTile,
    /// EEUIShell `std::set<UString>` (sShFiles): FILE_APPEND_LISTED's files, in order, and the line
    kScratchShFiles,
    /// EEUIShell `std::vector<UString>` (sShLines)
    kScratchShLines,
    /// EEUIShell `UWideString` (sShText)
    kScratchShText,
    /// EEUIShell `UWideString` (sShFormat): APPEND_NUMBERED's format
    kScratchShFormat,
    /// EEUIShell `UILBNoSort<UWideString>` (sShNoSort): LIST_INSERT_BYTES' order
    kScratchShNoSort,
    /// EEUIShell `GETransformation` (sShTransformation): ROTATE's
    kScratchShTransformation,
    /// EEUIShell `ISEntity*` (sShKey): KEY's and SPACE's
    kScratchShKey,
    /// EEUIShell `GE3DLine` (sShLine): SCREEN_LINE's
    kScratchShLine,
    /// EEScreens `FSFileSpec` (EEScreensScenario's sScenario): the intro's scenario file (GetScenario's, for Start's
    /// load or game)
    kScratchScenario,
    /// the slot count
    kScratchCount,
}

const SLOTS: usize = EeScreensScratch::kScratchCount as usize;

static TABLE: [AtomicPtr<c_void>; SLOTS] = [const { AtomicPtr::new(std::ptr::null_mut()) }; SLOTS];
static MAKING: Mutex<()> = Mutex::new(());

/// Slot `slot`'s object (`size` bytes aligned to `align`), made on its first use by `make` (none: the zeros).
///
/// # Safety
/// Every call for a slot passes the same size, alignment and `make`; `make` constructs its object at the address
/// it is given and does not call this function.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_scratch(
    slot: EeScreensScratch,
    size: usize,
    align: usize,
    make: Option<unsafe extern "C" fn(*mut c_void)>,
) -> *mut c_void {
    let cell = &TABLE[slot as usize];
    let p = cell.load(Ordering::Acquire);
    if !p.is_null() {
        return p;
    }
    let _making = MAKING.lock().unwrap_or_else(|e| e.into_inner());
    let p = cell.load(Ordering::Acquire);
    if !p.is_null() {
        return p;
    }
    let Ok(layout) = Layout::from_size_align(size.max(1), align.max(1)) else {
        std::process::abort();
    };
    // SAFETY: the layout's size is not zero
    let p = unsafe { alloc_zeroed(layout) } as *mut c_void;
    if p.is_null() {
        std::process::abort();
    }
    if let Some(make) = make {
        // SAFETY: the contract: `make` constructs the slot's object in the fresh memory
        unsafe { make(p) };
    }
    cell.store(p, Ordering::Release);
    p
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn make_seven(p: *mut c_void) {
        // SAFETY: the test's slot is a u64
        unsafe { *(p as *mut u64) = 7 };
    }

    #[test]
    fn a_slot_is_made_once_and_kept() {
        // SAFETY: the last slot's number is a variant; a test binary has no C++ user of it
        let slot = unsafe { std::mem::transmute::<u32, EeScreensScratch>(SLOTS as u32 - 1) };
        // SAFETY: the slot is this test's only user, a u64
        let (a, b) = unsafe {
            let a = ee_screens_scratch(slot, 8, 8, Some(make_seven)) as *mut u64;
            assert_eq!(*a, 7);
            *a = 9;
            (a, ee_screens_scratch(slot, 8, 8, Some(make_seven)) as *mut u64)
        };
        assert_eq!(a, b);
        // SAFETY: the slot's object
        assert_eq!(unsafe { *b }, 9);
    }
}
