// C:\EEX\Low-Level Engine\User Interface\UIFileComboBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006f4ef..0x1006fae7: the U2D key init chains, the ctor (0x1006f60f), the dtor, the out-of-line members in
// alphabetical order, then the COMDATs: the vector<UWideString>(allocator) ctor the ctor calls, the InsertLine member
// template for UWSNoCaseAlphaSort and the lower_bound pair it instantiates. The inline members and the implicit copy
// members are emitted by UIControlsTable.cpp. UIFileListBox.cpp's key chains start at 0x1006fae7.
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
#include "lle/UIEditBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"

// FUNCTION: LLE 0x1006f60f
// Creates an empty file combo box (the given entries are ignored) with no search path
UIFileComboBox::UIFileComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData,
                               GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
                               unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                               unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, std::vector<UWideString>(), inFont,
                 GERectangle(inLocation, GE2DPoint(inLocation.GetX() + inWidth, inLocation.GetY() + kScrollBarWidth)),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness),
      mFilePath(NULL)
{
}

// FUNCTION: LLE 0x1006f695
// Destroys the file combo box, releasing its search path
UIFileComboBox::~UIFileComboBox()
{
    if (this->mFilePath)
        this->mFilePath->Dereference();
}

// FUNCTION: LLE 0x1006f6c1
// Fills the list with every file in the search path that has a valid extension (full names), appended or sorted
void UIFileComboBox::CreateEntries(unsigned long inSortType)
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
                theName = theFile.GetFile();
                theLine = theName;
                switch (inSortType) {
                case 0:
                    this->AppendLine(theLine);
                    break;
                case 1:
                    this->mListBox->InsertLine(theLine, 0, UWSNoCaseAlphaSort());
                    break;
                default:
                    this->mListBox->InsertLine(theLine, 0, UWSNoCaseAlphaSort());
                    break;
                }
                break;
            }
            theResult = theDirectory.GetNextFile(theFile);
        }
        theDirectory.GetFirstFile(theFile);
    }
}

// FUNCTION: LLE 0x1006f848
// Deletes the selected file from disk (the combo box lists full file names, extension included) and removes its
// line on success; returns the delete or lookup error
long UIFileComboBox::DeleteSelectedFile()
{
    UString theName;
    unsigned long theSelection;
    long theResult = this->GetCurrentSelection(theSelection);
    if (theResult == kErrorOK) {
        theName = this->mListBox->GetData(theSelection);
        // Fixed: each valid extension used to be appended (cumulatively) to the listed name, which already has its
        // extension, so the file was never found
        FSFileSpec* theFile = NULL;
        theResult = this->mFilePath->FindFile(theName, theFile);
        if (theResult == kErrorOK) {
            theResult = theFile->Delete();
            if (theResult == kErrorOK)
                this->DeleteLine(theSelection);
            delete theFile;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1006f928
// Sets the search path the file list is built from, holding a reference to it
void UIFileComboBox::SetSearchPaths(FSPath* inPath)
{
    if (this->mFilePath)
        this->mFilePath->Dereference();
    this->mFilePath = inPath;
    if (this->mFilePath)
        this->mFilePath->Reference();
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1006f4ef
// SYNTHETIC: LLE 0x1006f4f9
// SYNTHETIC: LLE 0x1006f509
// SYNTHETIC: LLE 0x1006f515
// SYNTHETIC: LLE 0x1006f51f
// SYNTHETIC: LLE 0x1006f529
// SYNTHETIC: LLE 0x1006f539
// SYNTHETIC: LLE 0x1006f545
// SYNTHETIC: LLE 0x1006f54f
// SYNTHETIC: LLE 0x1006f559
// SYNTHETIC: LLE 0x1006f569
// SYNTHETIC: LLE 0x1006f575
// SYNTHETIC: LLE 0x1006f57f
// SYNTHETIC: LLE 0x1006f589
// SYNTHETIC: LLE 0x1006f599
// SYNTHETIC: LLE 0x1006f5a5
// SYNTHETIC: LLE 0x1006f5af
// SYNTHETIC: LLE 0x1006f5b9
// SYNTHETIC: LLE 0x1006f5c9
// SYNTHETIC: LLE 0x1006f5d5
// SYNTHETIC: LLE 0x1006f5df
// SYNTHETIC: LLE 0x1006f5e9
// SYNTHETIC: LLE 0x1006f5f9
// SYNTHETIC: LLE 0x1006f605

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1006f94b
// TEMPLATE: LLE 0x1006f961
// TEMPLATE: LLE 0x1006fa30
// TEMPLATE: LLE 0x1006fa68
