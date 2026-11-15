// C:\EEX\Low-Level Engine\User Interface\UIFileListBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006fae7..0x1006ff58: the U2D key init chains, the ctor (0x1006fc07), the dtor, then the out-of-line members in
// alphabetical order. No COMDATs of its own: the InsertLine<UWSNoCaseAlphaSort> instance it calls is the one
// UIFileComboBox.cpp emits. The inline members and the implicit copy members are emitted by UIControlsTable.cpp.
// UIFontTable.cpp starts at 0x1006ff58.
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
#include <string.h>

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
#include "lle/UILBString.h"
#include "lle/UIFileListBox.h"

// Creates an empty file list box with no search path
UIFileListBox::UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                             UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UILBString(inId, std::vector<UWideString>(), inFont, inAutoSelect, inRectangle, inForm, inIndex,
                 inBorderThickness),
      mFilePath(NULL)
{
}

// Destroys the file list box, releasing its search path
UIFileListBox::~UIFileListBox()
{
    if (this->mFilePath)
        this->mFilePath->Dereference();
}

// Fills the list with every file in the search path that has a valid extension (names without it), appended or sorted
void UIFileListBox::CreateEntries(unsigned long inSortType)
{
    UString theName;
    UWideString theLine;
    FSFileSpec theFile;
    FSFileSpec theDirectory;
    this->DeleteAllEntries();
    for (unsigned long i = 0; i < this->mFilePath->GetDirectoryCount(); i++) {
        this->mFilePath->GetDirectory(i, theDirectory);
        long theResult = theDirectory.GetFirstFile(theFile);
        while (theResult == kErrorOK) {
            unsigned long theLength = strlen(theFile.GetFile()) - 4;
            const char* theExtension = (const char*)theFile.GetFile() + theLength;
            for (unsigned long j = 0; j < this->mValidExtensions.size(); j++) {
                if (strcmp(theExtension, this->mValidExtensions[j]) != 0)
                    continue;
                theName = theFile.GetFile().GetSubstring(0, theLength);
                theLine = theName;
                switch (inSortType) {
                case 0:
                    this->AppendLine(theLine);
                    break;
                case 1:
                    this->InsertLine(theLine, 0, UWSNoCaseAlphaSort());
                    break;
                default:
                    this->InsertLine(theLine, 0, UWSNoCaseAlphaSort());
                    break;
                }
                break;
            }
            theResult = theDirectory.GetNextFile(theFile);
        }
        theDirectory.GetFirstFile(theFile);
    }
}

// Deletes the selected file from disk (the listed name plus the first valid extension that finds a file) and removes
// its line; returns the delete or lookup error
long UIFileListBox::DeleteSelectedFile()
{
    UString theName;
    unsigned long theSelection;
    long theResult = this->GetCurrentSelection(theSelection);
    if (theResult == kErrorOK) {
        theName = this->GetData(theSelection);
        for (unsigned long i = 0; i < this->mValidExtensions.size(); i++) {
            // Fixed: the extensions used to be appended to theName cumulatively ("name.e1.e2"), so only the first
            // one was tried correctly
            UString theFileName(theName);
            theFileName += this->mValidExtensions[i];
            FSFileSpec* theFile = NULL;
            theResult = this->mFilePath->FindFile(theFileName, theFile);
            if (theResult != kErrorOK)
                continue;
            theResult = theFile->Delete();
            if (theResult == kErrorOK) {
                this->DeleteLine(theSelection);
                delete theFile;
                break;
            }
            delete theFile;
        }
    }
    return theResult;
}

// Sets the search path the file list is built from, holding a reference to it
void UIFileListBox::SetSearchPaths(FSPath* inPath)
{
    if (this->mFilePath)
        this->mFilePath->Dereference();
    this->mFilePath = inPath;
    if (this->mFilePath)
        this->mFilePath->Reference();
}
