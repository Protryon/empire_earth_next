// C:\EEX\Empire Earth\User Interface Thread\EEUIFileComboBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFileComboBox). The game's file combo box: 0x2624a6..0x26272b (ctor, ??_G, dtor, CreateEntries, SelectFile;
// EEUIFMainGame.cpp's init chain at 0x26272b). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include <set>
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x6624a6
// Creates a file combo box listing the files of a path that match a pattern
EEUIFileComboBox::EEUIFileComboBox(FSPath* inPath, const UString& inPattern, unsigned long inId,
                                   unsigned long inTextBoxType, std::vector<UWideString> inData, GEFont* inFont,
                                   const GE2DPoint& inLocation, unsigned long inWidth,
                                   unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                   unsigned long inMaxChars, unsigned long inBorderThickness, bool inUnused)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, (float)inWidth, 20.0f),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness)
{
    UString theUnused;
    this->mPath = inPath;
    this->mPath->Reference();
    this->mPattern = inPattern;
    this->CreateEntries();
}

// SYNTHETIC: EXE 0x662567
// (the scalar deleting destructor, emitted with the vtable; slot EEUIFileComboBox+0x0)

// FUNCTION: EXE 0x662583
// Destroys the file combo box, releasing its path
EEUIFileComboBox::~EEUIFileComboBox()
{
    this->mPath->Dereference();
}

// FUNCTION: EXE 0x6625ae
// Refills the list: "none" first, then each matching file name without its 4-character extension
void EEUIFileComboBox::CreateEntries()
{
    FSFileSpec theUnused;
    std::set<UString> theFiles;
    std::set<UString>::const_iterator it;
    UWideString theLine;
    this->DeleteAllEntries();
    gResource->GetString(IDS_SE_MAP_NONE, theLine);
    this->AppendLine(theLine);
    this->mPath->ListFiles(this->mPattern, theFiles);
    for (it = theFiles.begin(); it != theFiles.end(); ++it) {
        theLine = *it;
        theLine.InsertString("", theLine.GetLength() - 4, true);
        this->AppendLine(theLine);
    }
    this->SetCurrentSelection(0);
}

// FUNCTION: EXE 0x662699
// Selects the line equal to a file name, or the first line ("none") when it is not listed
long EEUIFileComboBox::SelectFile(const UWideString& inName)
{
    long theResult = kErrorNotFound;
    UWideString theLine;
    unsigned long i; for (i = 0; i < this->GetListBox()->GetLineCount(); i++) {
        theLine = this->GetListBox()->GetData(i);
        if (theLine == inName) {
            theResult = kErrorOK;
            break;
        }
    }
    if (theResult == kErrorOK)
        this->SetCurrentSelection(i);
    else
        this->SetCurrentSelection(0);
    return theResult;
}
