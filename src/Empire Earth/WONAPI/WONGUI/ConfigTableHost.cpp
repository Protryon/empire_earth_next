// The resource configs' table reader's host (R6 WON round 39): the 45 pure-table config handlers are ee_won's
// cfgtab.rs programs; their reads, frame and setters are WONCfgTable.inl (shared with tests/won/won_cfgtab_test.cpp).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/won_gui/guiconf.h"
#include "ee/wonapi/WONGUIShell.h"
extern bool gShowParseTime; // ComponentConfigHost.cpp's
extern int gLinesParsed;
namespace WONAPI {
void CfgSkinClickSound(ComponentConfig*); // below
#include "ee/wonapi/WONCfgTable.inl"
}

// What the handlers' units kept besides their instructions (ButtonConfig.cpp's FITTEXT, WONSkinConfig's click sound);
// since R6 WONLobby round 26 the bindings, predefined children and named values of the deleted config classes are their
// kinds' (ee_won_gui cfgkind.rs, WONCompCfg.inl).
namespace WONAPI {
// A config of a kind: a ContainerConfig for the container kinds (the original classes' constructors)
ComponentConfig* CfgNewOfKind(EeWonCfgKind inKind)
{
    if(ee_won_cfg_kind_is(inKind,EeWonCfgKind::Container)) return new ContainerConfig(inKind);
    return new ComponentConfig(inKind);
}
// Grows the button to fit the largest of a comma-separated list of texts, keeping its current text
void ComponentConfig::HandleFitText()
{
    Button* aButton=static_cast<Button*>(this->mComponent.get());
    int width=aButton->Width();
    int height=aButton->Height();
    GUIString oldText=aButton->GetText();
    for(;;)
    {
        GUIString text=this->ReadString(false);
        aButton->SetText(text,true);
        int desiredWidth=0,desiredHeight=0;
        aButton->GetDesiredSize(desiredWidth,desiredHeight);
        if(desiredWidth>width) width=desiredWidth;
        if(desiredHeight>height) height=desiredHeight;
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    aButton->SetText(oldText,true);
    aButton->SetSize(width,height);
}
// WONSkinConfig's BUTTONCLICKSOUND: a sound read, then the click sound of the skin's template buttons and every named
// button type
void CfgSkinClickSound(ComponentConfig* inConfig)
{
    SmartPtr<Sound> sound=inConfig->ReadSound();
    ee_won_skin_click_sound(WONSkinGlobal(),sound.get());
}
}
