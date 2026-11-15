// C:\EEX\Empire Earth\Scenario Editor\EESEPSSetPlayers.cpp (file name inferred: no __FILE__).
// 0x172b0f..0x172d2c (the `$E` chains 0x172b0f..0x172c4a of four guarded vector<long> class statics, then the
// epoch event and the Set All Players button). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIRadioButton.h"
#include "lle/UIForm.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"
#include <vector>
#include <deque>
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEPlayerScreen.h"
#include "lle/UIEvent.h"
#include "ee/EESEPSSetPlayers.h"
#include "ee/EEStringIDs.h"
// the class statics' `$E` chains 0x172b0f / 0x172b58 / 0x172ba1 / 0x172bea (definition order)
std::vector<long> EESEPSSetPlayers::sV531FB0;
std::vector<long> EESEPSSetPlayers::sBuildingTechs;
std::vector<long> EESEPSSetPlayers::sV531F88;
std::vector<long> EESEPSSetPlayers::sTechnologyTechs;

// Creates the 100x25 'Set All Players' button with transparent text
EESEPSSetPlayers::EESEPSSetPlayers(const GE2DPoint& inPosition, GEFont* inFont, UIForm* inForm, unsigned long inIndex)
    : UIButton(0x144, NULL, NULL, NULL,
               GERectangle(inPosition, 100.0f, 25.0f),
               inForm, inIndex, 3)
{
    UWideString theText;
    gResource->GetString(IDS_SET_ALL_PLAYERS, theText);
    this->SetText(theText, inFont);
    GEText* theGEText = this->GetGEText();
    theGEText->SetFgColor(GEColor());
    theGEText->SetIsTransparent(true);
}

// Copies the first player's settings to every player when released
void EESEPSSetPlayers::ButtonUp()
{
    ((EESEPlayerScreen*)(static_cast<EEUIFScenarioEditor*>(this->GetForm()))->GetTabGroup(kEditorTabPlayers))->CopyFirstPlayerToAll();
    UIButton::ButtonUp();
}
