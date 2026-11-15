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

#include "rust/ee_ui.h"
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

// The Rust file lists' calls out (EeUiOp 650..667): T's own list methods, a directory search, the path
struct UIFileListHost {
    struct Search {                         // the original's locals
        FSFileSpec mFile;
        FSFileSpec mDirectory;
    };
    static UIListBox<UWideString>* List(UIFileListBox* l) { return l; }
    static UIListBox<UWideString>* List(UIFileComboBox* c) { return c->GetList(); }
    template <class T> static intptr_t Op(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t, float* f)
    {
        T* t = static_cast<T*>(inObj);
        Search* s = reinterpret_cast<Search*>(a);
        FSPath* p = reinterpret_cast<FSPath*>(inOp == EeUiOp::FlDeleteFile || inOp == EeUiOp::FlDirFirst ? b : a);
        UString theName;
        UWideString theLine;
        switch (inOp) {
        case EeUiOp::FlDeleteAll: t->DeleteAllEntries(); break;
        case EeUiOp::FlSearchNew: return (intptr_t) new Search;
        case EeUiOp::FlSearchDelete: delete s; break;
        case EeUiOp::FlDirCount: return p->GetDirectoryCount();
        case EeUiOp::FlDirFirst:
            p->GetDirectory((unsigned long)c, s->mDirectory);
            return s->mDirectory.GetFirstFile(s->mFile);
        case EeUiOp::FlNext: return s->mDirectory.GetNextFile(s->mFile);
        case EeUiOp::FlRewind: s->mDirectory.GetFirstFile(s->mFile); break;
        case EeUiOp::FlNameLength: return strlen(s->mFile.GetFile());
        case EeUiOp::FlNameCopy: Copy((const char*)s->mFile.GetFile(), b, c); break;
        case EeUiOp::FlAppend:
        case EeUiOp::FlInsertSorted:
            theName = reinterpret_cast<const char*>(a);
            theLine = theName;
            if (inOp == EeUiOp::FlAppend)
                t->AppendLine(theLine);
            else
                List(t)->InsertLine(theLine, 0, UWSNoCaseAlphaSort());
            break;
        case EeUiOp::FlSelection: {
            unsigned long theSelection = 0;
            long theResult = t->GetCurrentSelection(theSelection);
            uint32_t theBits = (uint32_t)theSelection;
            memcpy(f, &theBits, 4);
            return theResult;
        }
        case EeUiOp::FlLineLength:
        case EeUiOp::FlLineCopy:
            theName = List(t)->GetData((unsigned long)a);
            if (inOp == EeUiOp::FlLineLength)
                return strlen(theName);
            Copy((const char*)theName, b, c);
            break;
        case EeUiOp::FlDeleteFile: {
            FSFileSpec* theFile = NULL;
            uint32_t theFound = 0;
            theName = reinterpret_cast<const char*>(a);
            long theResult = p->FindFile(theName, theFile);
            if (theResult == kErrorOK) {
                theFound = 1;
                theResult = theFile->Delete();
                delete theFile;
            }
            memcpy(f, &theFound, 4);
            return theResult;
        }
        case EeUiOp::FlDeleteLine: t->DeleteLine((unsigned long)a); break;
        case EeUiOp::FlPathReference: p->Reference(); break;
        case EeUiOp::FlPathRelease: p->Dereference(); break;
        default: break;
        }
        return 0;
    }
    // the bytes into the Rust buffer b of c bytes, NUL-terminated
    static void Copy(const char* inBytes, intptr_t b, intptr_t c)
    {
        char* theBuffer = reinterpret_cast<char*>(b);
        size_t n = strlen(inBytes);
        if (n >= (size_t)c)
            n = (size_t)c - 1;
        memcpy(theBuffer, inBytes, n);
        theBuffer[n] = 0;
    }
};

// FUNCTION: LLE 0x1006fc07
// Creates an empty file list box with no search path
UIFileListBox::UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                             UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UILBString(inId, std::vector<UWideString>(), inFont, inAutoSelect, inRectangle, inForm, inIndex,
                 inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, UIFileListHost::Op<UIFileListBox>, false);
}

UIFileListBox::~UIFileListBox() { ee_ui_file_list_free(this->mRust); }
void UIFileListBox::AddValidExtension(const UString& inExtension)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::AddExtension, (intptr_t)(const char*)inExtension);
}
void UIFileListBox::CreateEntries(unsigned long inSortType)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::CreateEntries, (intptr_t)inSortType);
}
long UIFileListBox::DeleteSelectedFile() { return (long)ee_ui_file_list_call(this->mRust, EeUiFileListCall::DeleteSelected, 0); }
FSPath* UIFileListBox::GetFilePath() const { return (FSPath*)ee_ui_file_list_call(this->mRust, EeUiFileListCall::FilePath, 0); }
void UIFileListBox::SetSearchPaths(FSPath* inPath)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::SetSearchPaths, (intptr_t)inPath);
}

// FUNCTION: LLE 0x1006f60f
// Creates an empty file combo box (the given entries are ignored) with no search path
UIFileComboBox::UIFileComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                               GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
                               unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                               unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, std::vector<UWideString>(), inFont,
                 GERectangle(inLocation, GE2DPoint(inLocation.GetX() + inWidth, inLocation.GetY() + kScrollBarWidth)),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, UIFileListHost::Op<UIFileComboBox>, true);
}

UIFileComboBox::~UIFileComboBox() { ee_ui_file_list_free(this->mRust); }
void UIFileComboBox::AddValidExtension(const UString& inExtension)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::AddExtension, (intptr_t)(const char*)inExtension);
}
void UIFileComboBox::CreateEntries(unsigned long inSortType)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::CreateEntries, (intptr_t)inSortType);
}
long UIFileComboBox::DeleteSelectedFile() { return (long)ee_ui_file_list_call(this->mRust, EeUiFileListCall::DeleteSelected, 0); }
FSPath* UIFileComboBox::GetFilePath() const { return (FSPath*)ee_ui_file_list_call(this->mRust, EeUiFileListCall::FilePath, 0); }
void UIFileComboBox::SetSearchPaths(FSPath* inPath)
{
    ee_ui_file_list_call(this->mRust, EeUiFileListCall::SetSearchPaths, (intptr_t)inPath);
}
