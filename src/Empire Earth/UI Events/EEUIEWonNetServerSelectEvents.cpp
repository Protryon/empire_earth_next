// C:\EEX\Empire Earth\UI Events\EEUIEWonNetServerSelectEvents.cpp (file name inferred: no __FILE__; class
// prefix EEUIEWNSS). The WON server list form's events: 0x1eb7fb..0x1ebc13. Cancel's ProcessEvent is ICF-folded into
// 0x1eb7e8 (EEUIEWonNetModifyUserAccountEvents.cpp's copy). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <stdlib.h>
#include <string>
#include <list>

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
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"

#include "ee/EEWonNet.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUIEWonNetServerSelectEvents.h"
#include "ee/EEUIManager.h"

long EEUIEWNSSCancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return inForm->GetManager()->Pop();
}

// Connects to the WON server selected in the server list (nothing when none or out of range)
long EEUIEWNSSOk::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    WONAPI::DirEntityList theServers(gWonNet->mServers);
    WONAPI::DirEntityList::iterator theIterator;
    unsigned long theSelection;
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)inForm->GetControl(kSelectServerListBox);
    if (!theList->GetHasSelection())
        return kErrorOK;
    theList->GetCurrentSelection(theSelection);
    if (theSelection >= theServers.size())
        return kErrorOK;
    unsigned long i = 0;
    for (theIterator = theServers.begin(); theIterator != theServers.end() && i < theSelection; i++)
        ++theIterator;
    gWonNet->SelectServer((*theIterator)->mDisplayName, (*theIterator)->GetNetAddrAsIP());
    return kErrorOK;
}

// Sets up the WON server list: hooks OK and Cancel, lists each server's display name (or name), selects a lone one
long EEUIEWNSSOpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    WONAPI::DirEntityList theServers;
    std::wstring theName;
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)inForm->GetControl(kSelectServerListBox);
    UWideString theText;
    unsigned long theCount = 0;
    char theBuffer[1024];
    EEUIEWNSetTriggerEvent(new EEUIEWNSSCancel, inForm->GetControl(kSelectServerCancelButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNSSOk, inForm->GetControl(kSelectServerOKButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNSSOk, inForm->GetControl(kSelectServerListBox), kControlTriggerDoubleClicked);
    EEUIEWNSetTriggerEvent(new EEUIEWNSSOk, inForm->GetControl(kSelectServerListBox), kControlTriggerEnterPressed);
    theList->DeleteAllEntries();
    theServers = gWonNet->mServers;
    for (WONAPI::DirEntityList::iterator theIterator = theServers.begin(); theIterator != theServers.end(); ++theIterator) {
        if ((*theIterator)->mDisplayName.size() > 0)
            theName = (*theIterator)->mDisplayName;
        else
            theName = (*theIterator)->mName;
        wcstombs(theBuffer, theName.c_str(), sizeof(theBuffer));
        theText = UWideString(theBuffer);
        theList->AppendLine(theText);
        theCount++;
    }
    if (theCount != 0 && theCount == 1)
        theList->ChangeSelectionIndex(0, true, false);
    return kErrorOK;
}
