// C:\EEX\Empire Earth\UI Events\EEUIEWonNetStatsEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNS). The WON statistics form's open event: 0x1ebc13..0x1ebc47. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
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
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"

#include "ee/EEWonNet.h"
#include "ee/EEUICWonNetStatBox.h"
#include "ee/EEUIEWonNetStatsEvents.h"
#include "ee/EEUIManager.h"

// FUNCTION: EXE 0x5ebc13
// Refreshes the WON statistics box and the player's own stats line
long EEUIEWNSOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWonNet) {
        static_cast<EEUICWonNetStatBox*>(inForm->GetControl(kWonStatsStatBox))->Refresh();
        static_cast<EEUICWonNetStatBox*>(inForm->GetControl(kWonStatsMyStatsLine))->Refresh();
    }
    return kErrorOK;
}

// ICF-FOLDED: EXE 0x6547cf
// Closes the debug sound panel by popping the top form
// (no address of its own: the same body as EEDSUIHideDebugSoundPanel::ProcessEvent, which ICF kept)
long EEUIEWNSCancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUIManager->Pop();
    return kErrorOK;
}
