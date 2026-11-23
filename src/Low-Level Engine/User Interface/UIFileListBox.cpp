// C:\EEX\Low-Level Engine\User Interface\UIFileListBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original units: UIFileListBox.cpp 0x1006fae7..0x1006ff58 and UIFileComboBox.cpp 0x1006f4ef..0x1006fae7.
//
// UIFileListBox and UIFileComboBox are whole in Rust (rust/crates/ee_ui/src/filelistobj.rs, R6 ui round 21): the
// members below forward to the Rust object, which calls out through UIFileListHost (the list's own methods, a
// directory search, the path's lookup and the file's delete, the path's references). UIFileComboBox.cpp's
// forwarders live here too.
#include <windows.h>
#include <string.h>

#include "rust/ui/filelistobj.h"
#include "rust/ui/obj.h"
#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UIEditBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileListBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

// The Rust file lists' calls out (EeUiOp 650..667; ee_ui lle_hosts.rs): a directory search (a), the path, the list's
// own methods (the list box's or the combo box's: FILE_LIST, FILE_COMBO), the lines' bytes into the Rust buffer b of
// c bytes
struct UIFileSearch {                       // the original's locals
    FSFileSpec mFile;
    FSFileSpec mDirectory;
};
#define search reinterpret_cast<UIFileSearch*>(a)
#define path reinterpret_cast<FSPath*>(a)
EE_UI_CASE(fl_search_new) { return (intptr_t) new UIFileSearch; }
EE_UI_VOID(fl_search_delete) { delete search; }
EE_UI_CASE(fl_dir_count) { return path->GetDirectoryCount(); }
EE_UI_VOID(fl_dir_get) { reinterpret_cast<FSPath*>(b)->GetDirectory((unsigned long)c, search->mDirectory); }
EE_UI_CASE(fl_first) { return search->mDirectory.GetFirstFile(search->mFile); }
EE_UI_CASE(fl_next) { return search->mDirectory.GetNextFile(search->mFile); }
EE_UI_VOID(fl_rewind) { search->mDirectory.GetFirstFile(search->mFile); }
EE_UI_CASE(fl_name_length) { return strlen(search->mFile.GetFile()); }
EE_UI_VOID(fl_name_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)search->mFile.GetFile()); }
EE_UI_VOID(fl_path_reference) { path->Reference(); }
EE_UI_VOID(fl_path_release) { path->Dereference(); }
#undef path
#undef search
extern "C" intptr_t ee_uicpp_fl_find_file(intptr_t inName, intptr_t inPath, FSFileSpec** outFile)
{ return reinterpret_cast<FSPath*>(inPath)->FindFile(UString(reinterpret_cast<const char*>(inName)), *outFile); }
extern "C" intptr_t ee_uicpp_fl_file_delete(FSFileSpec* inFile) { return inFile->Delete(); }
extern "C" void ee_uicpp_fl_file_free(FSFileSpec* inFile) { delete inFile; }
#define FILE_LIST static_cast<UIFileListBox*>(inObj)
#define FILE_COMBO static_cast<UIFileComboBox*>(inObj)
static UWideString sFileLine;   // the combo box's appended line (its AppendLine takes a reference)
EE_UI_VOID(fl_list_delete_all) { FILE_LIST->DeleteAllEntries(); }
EE_UI_VOID(fl_list_append) { FILE_LIST->AppendLine(UWideString(reinterpret_cast<const char*>(a))); }
EE_UI_VOID(fl_list_insert_sorted) { FILE_LIST->InsertLine(UWideString(reinterpret_cast<const char*>(a)), 0, UWSNoCaseAlphaSort()); }
EE_UI_CASE(fl_list_selection) { return FILE_LIST->GetCurrentSelection(*reinterpret_cast<unsigned long*>(f)); }
EE_UI_CASE(fl_list_line_length) { return strlen(FILE_LIST->GetData((unsigned long)a)); }
EE_UI_VOID(fl_list_line_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)FILE_LIST->GetData((unsigned long)a)); }
EE_UI_VOID(fl_list_delete_line) { FILE_LIST->DeleteLine((unsigned long)a); }
EE_UI_VOID(fl_combo_delete_all) { FILE_COMBO->DeleteAllEntries(); }
EE_UI_VOID(fl_combo_append) { FILE_COMBO->AppendLine(sFileLine = reinterpret_cast<const char*>(a)); }
EE_UI_VOID(fl_combo_insert_sorted) { FILE_COMBO->GetList()->InsertLine(UWideString(reinterpret_cast<const char*>(a)), 0, UWSNoCaseAlphaSort()); }
EE_UI_CASE(fl_combo_selection) { return FILE_COMBO->GetCurrentSelection(*reinterpret_cast<unsigned long*>(f)); }
EE_UI_CASE(fl_combo_line_length) { return strlen(FILE_COMBO->GetList()->GetData((unsigned long)a)); }
EE_UI_VOID(fl_combo_line_copy) { snprintf(reinterpret_cast<char*>(b), (size_t)c, "%s", (const char*)FILE_COMBO->GetList()->GetData((unsigned long)a)); }
EE_UI_VOID(fl_combo_delete_line) { FILE_COMBO->DeleteLine((unsigned long)a); }
#undef FILE_COMBO
#undef FILE_LIST

// Creates an empty file list box with no search path
UIFileListBox::UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                             UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UILBString(inId, std::vector<UWideString>(), inFont, inAutoSelect, inRectangle, inForm, inIndex,
                 inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, ee_ui_file_list_host, false);
}

UIFileListBox::~UIFileListBox() { ee_ui_file_list_free(this->mRust); }

// Creates an empty file combo box (the given entries are ignored) with no search path
UIFileComboBox::UIFileComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                               GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
                               unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                               unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, std::vector<UWideString>(), inFont,
                 GERectangle(inLocation, GE2DPoint(inLocation.GetX() + inWidth, inLocation.GetY() + kScrollBarWidth)),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, ee_ui_file_combo_host, true);
}

UIFileComboBox::~UIFileComboBox() { ee_ui_file_list_free(this->mRust); }
