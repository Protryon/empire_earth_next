// C:\EEX\Empire Earth\Actions\EEAAddChatMessage.cpp (file name inferred: no __FILE__; RTTI class
// EEAAddChatMessage, vtable 0x43e340). The first unit of the link: 0x1000..0x16c0 (EEABootClientFromCanceledGame's
// Virtual4 starts at 0x16c0). A `??_I` helper COMDAT, the U2D header keys' `$E` chains, five unit keys (only this
// object has them), the UList node pools' template-static chain and their UMemoryPool COMDATs, then Virtual4.
// UNIT-FLAGS: -O1 -G6 -MD -GR -DLLE_CURRENT_HEADERS
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
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
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UMemoryPool.h"
#include "lle/UList.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEAction.h"
#include "ee/EEAAddChatMessage.h"

// unit keys (provisional names; unreferenced in the exe: only their chains use them)
const UString kDefaultName("default");
const UString kCivExtension(".civ");
const UString kCivilizationsFolder("Civilizations");
const UString kDefaultFolder("Default");
const UString kUsersFolder("Users");

// Appends a chat line to the lobby's chat box, keeping it scrolled to the end when it was already there or it is ours
void EEAAddChatMessage::Virtual4()
{
    if (gUIManager->GetCurrentForm() != gUIManager->GetFormByID(kFormLobby))
        return;
    UITextLabel* theChat = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormLobby)->GetControl(kLobbyChatPreviousMessages));
    bool theAtEnd;
    if (!this->mFromLocalPlayer && theChat->GetScrollBar() &&
        theChat->GetScrollBar()->GetSlider()->GetMaxRange() !=
            theChat->GetScrollBar()->GetSlider()->GetCurrentRangeValue())
        theAtEnd = false;
    else
        theAtEnd = true;
    theChat->AppendText(this->mText);
    if (theAtEnd == true && theChat->GetScrollBar())
        theChat->SetStartingDisplayLine(theChat->GetScrollBar()->GetSlider()->GetMaxRange());
}

void EEAAddChatMessage::Virtual8()
{
}
