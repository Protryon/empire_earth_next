
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "rust/common.h"
#include "rust/won_gui/barobj.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/comboobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/ctobj.h"
#include "rust/won_gui/dialogobj.h"
#include "rust/won_gui/fontmap.h"
#include "rust/won_gui/gfxobj.h"
#include "rust/won_gui/grabobj.h"
#include "rust/won_gui/guicfg.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guiedit.h"
#include "rust/won_gui/guievent.h"
#include "rust/won_gui/guifocus.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/guilist.h"
#include "rust/won_gui/guimenu.h"
#include "rust/won_gui/guimulti.h"
#include "rust/won_gui/guiscroll.h"
#include "rust/won_gui/guistring.h"
#include "rust/won_gui/guistrobj.h"
#include "rust/won_gui/guitext.h"
#include "rust/won_gui/guitree.h"
#include "rust/won_gui/guiwin.h"
#include "rust/won_gui/guiwrap.h"
#include "rust/won_gui/imgdec.h"
#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/inputobj.h"
#include "rust/won_gui/labelobj.h"
#include "rust/won_gui/layoutobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/menuobj.h"
#include "rust/won_gui/msbtnobj.h"
#include "rust/won_gui/mspaint.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/paneobj.h"
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platfont.h"
#include "rust/won_gui/platgfx.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
#include "rust/won_gui/rootobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/skinlay.h"
#include "rust/won_gui/splitobj.h"
#include "rust/won_gui/stretchobj.h"
#include "rust/won_gui/surface.h"
#include "rust/won_gui/tabobj.h"
#include "rust/won_gui/textobj.h"
#include "rust/won_gui/treeobj.h"
#include "rust/won_gui/winobj.h"
#include "rust/won_gui/wmcache.h"
#include "rust/won_gui/wmloop.h"
#include "rust/won_gui/wmobj.h"
#include "rust/won_gui/wonbtnobj.h"
#include "rust/won_gui/wrapobj.h"
#include "rust/won_lobby/chatobj.h"
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/eeresource.h"
#include "rust/won_lobby/eesound.h"
#include "rust/won_lobby/eexfer.h"
#include "rust/won_lobby/ffi_lan.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbymap.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/persist.h"
#include "rust/won_lobby/prefsobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_lobby/routing.h"
#include "rust/won_lobby/scrolltext.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/stagingobj.h"
#include "rust/won_lobby/statusstr.h"
#include "rust/won_net/admin.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/auth.h"
#include "rust/won_net/authcpp.h"
#include "rust/won_net/authctx.h"
#include "rust/won_net/authflow.h"
#include "rust/won_net/authlogin.h"
#include "rust/won_net/authobj.h"
#include "rust/won_net/blowfish.h"
#include "rust/won_net/buffer.h"
#include "rust/won_net/cachemap.h"
#include "rust/won_net/cdkeystore.h"
#include "rust/won_net/cert.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/chat.h"
#include "rust/won_net/config.h"
#include "rust/won_net/configobj.h"
#include "rust/won_net/db.h"
#include "rust/won_net/debug.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/dirchange.h"
#include "rust/won_net/dirops.h"
#include "rust/won_net/elgamal.h"
#include "rust/won_net/ffi.h"
#include "rust/won_net/ffi_net.h"
#include "rust/won_net/ffi_ops.h"
#include "rust/won_net/ffi_parse.h"
#include "rust/won_net/file.h"
#include "rust/won_net/filepush.h"
#include "rust/won_net/ftp.h"
#include "rust/won_net/ftpop.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/fwop.h"
#include "rust/won_net/gamespy.h"
#include "rust/won_net/getcert.h"
#include "rust/won_net/hostauth.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostnative.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/http.h"
#include "rust/won_net/httpcache.h"
#include "rust/won_net/httpcpp.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpengine.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/ipobj.h"
#include "rust/won_net/md5.h"
#include "rust/won_net/miscops.h"
#include "rust/won_net/multiping.h"
#include "rust/won_net/ops.h"
#include "rust/won_net/patch.h"
#include "rust/won_net/path.h"
#include "rust/won_net/peerauth.h"
#include "rust/won_net/peerobj.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/perftimer.h"
#include "rust/won_net/qsock.h"
#include "rust/won_net/random.h"
#include "rust/won_net/regauth.h"
#include "rust/won_net/regkey.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqlast.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/reqop.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routeconn.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routemsg.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/servconn.h"
#include "rust/won_net/servctx.h"
#include "rust/won_net/server.h"
#include "rust/won_net/smartroute.h"
#include "rust/won_net/sockobj.h"
#include "rust/won_net/sockop.h"
#include "rust/won_net/sockops.h"
#include "rust/won_net/sockthread.h"
#include "rust/won_net/strcase.h"
#include "rust/won_net/titlecfg.h"
#include "rust/won_net/udpmgr.h"
#include "rust/won_net/udpops.h"
#include "rust/won_net/userstatus.h"
#include "rust/won_net/verop.h"
#include "rust/won_net/wonfile.h"
namespace WONAPI {

QuickPlayCtrl::QuickPlayCtrl() {}
// Finds the welcome label, the four skill radios and the play button in the layout
void QuickPlayCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mWelcomeLabel,"WelcomeLabel",__FILE__,29);
    SafeGetComponent(config,this->mNoviceRadio,"NoviceRadio",__FILE__,30);
    SafeGetComponent(config,this->mIntermediateRadio,"IntermediateRadio",__FILE__,31);
    SafeGetComponent(config,this->mAdvancedRadio,"AdvancedRadio",__FILE__,32);
    SafeGetComponent(config,this->mExpertRadio,"ExpertRadio",__FILE__,33);
    SafeGetComponent(config,this->mPlayButton,"PlayButton",__FILE__,34);
    this->SubscribeToBroadcast(1);
}
// Welcomes the logged-in user by name, or shows the plain welcome text
void QuickPlayCtrl::RefreshUserName()
{
    LobbyUserInfo* user=LobbyGetCurrentUser();
    if(user!=NULL)
        this->mWelcomeLabel->SetText(LobbyFormatString(LOBBY_STRING(InternetQuickPlay_UserWelcome_String),user->mUserName),true);
    else
        this->mWelcomeLabel->SetText(LOBBY_STRING(InternetQuickPlay_Welcome_String),true);
}
// Refreshes the welcome on entering the screen; Play sends a quick-play request for the chosen skill (1-4)
void QuickPlayCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case 100014:
        this->RefreshUserName();
        return;
    case ComponentEvent_ButtonPressed:
      if(inEvent->mComponent==this->mPlayButton.get()) {
        int gameType=0;
        if(this->mNoviceRadio->IsChecked()) gameType=1;
        else if(this->mIntermediateRadio->IsChecked()) gameType=2;
        else if(this->mAdvancedRadio->IsChecked()) gameType=3;
        else if(this->mExpertRadio->IsChecked()) gameType=4;
        this->DeliverEvent(new QuickPlayEvent(gameType),false);
      }
      break;
    }
    Dialog::HandleComponentEvent(inEvent);
}
}
