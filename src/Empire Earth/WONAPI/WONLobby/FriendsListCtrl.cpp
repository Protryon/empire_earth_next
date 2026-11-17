// FriendsListCtrl: the lobby's friends list (online friends first, then a separator, then the rest)
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
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
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
#include "rust/won_gui/jpeg.h"
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
// Creates a friends list entry with no friend (a separator line until a friend is set)
FriendsListItem::FriendsListItem() { this->mSeparator=true; this->mSortGroup=0; }
// The implicit destructor (0x7bb0a0) releases mFriend, then destroys the TextListItem base
// Sizes a separator (8 pixels high when shown, else 1) or a friend's text entry
void FriendsListItem::CalcDimensions(ListArea* inList)
{
    if(this->mFriend.get()==NULL) {
        this->mWidth=1;
        if(this->mSeparator) this->mHeight=8;
        else this->mHeight=1;
    } else TextListItem::CalcDimensions(inList);
}
// Orders entries by sort group, highest first (online friends, separator, others), then by text
int FriendsListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int result=static_cast<const FriendsListItem*>(inOther)->mSortGroup-this->mSortGroup;
    if(result!=0) return result;
    return TextListItem::Compare(inOther,inList);
}
// Draws a shown separator as a line across the list, or a friend's text entry
void FriendsListItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    if(this->mFriend.get()==NULL) {
        if(this->mSeparator) {
            int width=inList->GetTotalWidth()+10;
            if(inList->GetPaintWidth()>width) width=inList->GetPaintWidth();
            int y=this->mHeight>>2;
            inGraphics.SetSysColor(StandardColor_Text);
            inGraphics.DrawLine(0,y,width,y);
        }
    } else TextListItem::Paint(inGraphics,inList);
}
// constructor emitted at 0x7bb250
FriendsListCtrl::FriendsListCtrl() {}
// The implicit destructor (0x7bb2a0) releases four members, then destroys the Container base
// Looks up the friends list and the Remove and Locate buttons, creates the separator and listens to broadcasts
void FriendsListCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mFriendsList,"FriendsList",__FILE__,109);
    SafeGetComponent(config,this->mRemoveButton,"RemoveButton",__FILE__,110);
    SafeGetComponent(config,this->mLocateButton,"LocateButton",__FILE__,111);
    this->mSeparator=new FriendsListItem;
    this->mSeparator->mSortGroup=1;
    this->mSeparator->mSelectable=false;
    this->SubscribeToBroadcast(1);
    this->UpdateButtons();
}
// Shows the separator only when it has entries on both sides
void FriendsListCtrl::UpdateSeparator()
{
    this->mFriendsList->RefreshItem(this->mSeparator.get());
    int pos=this->mFriendsList->GetItemPos(this->mSeparator.get());
    bool show=pos>0&&pos<this->mFriendsList->GetNumItems()-1;
    if(this->mSeparator->mSeparator!=show) {
        this->mSeparator->mSeparator=show;
        this->mFriendsList->RefreshItem(this->mSeparator.get());
    }
}
// Fills an entry from a friend: name, icon (searching, in game, in chat, unknown), sort group, and italic unless online
void FriendsListCtrl::UpdateFriend(LobbyFriend* inFriend,FriendsListItem* inItem)
{
    inItem->mFriend=inFriend;
    if((inFriend->GetFlags()&3)!=0&&!inFriend->GetAnonymous()) inItem->mSortGroup=2;
    else inItem->mSortGroup=0;
    Image* anImage;
    if((inFriend->GetFlags()&4)!=0) anImage=LOBBY_IMAGE(LobbyGlobal_Searching_Image);
    else if(!inFriend->GetAnonymous()) {
        if((inFriend->GetFlags()&2)!=0) anImage=LOBBY_IMAGE(FriendsListCtrl_InGame_Image);
        else if((inFriend->GetFlags()&1)!=0) anImage=LOBBY_IMAGE(FriendsListCtrl_InChat_Image);
        else anImage=LOBBY_IMAGE(FriendsListCtrl_Unknown_Image);
    } else anImage=LOBBY_IMAGE(FriendsListCtrl_Unknown_Image);
    inItem->mImage=anImage;
    inItem->mText=inFriend->GetName();
    if((inFriend->GetFlags()&3)!=0&&!inFriend->GetAnonymous()) {
        inItem->mColor=0x8000000e;
        inItem->mFont=this->GetDefaultFont();
    } else {
        inItem->mColor=0x80000006;
        inItem->mFont=this->GetDefaultFontMod(2,0);
    }
}
// Returns the entry of the friend with the same user id, or NULL
FriendsListItem* FriendsListCtrl::FindFriend(LobbyFriend* inFriend)
{
    for(int i=0;i<this->mFriendsList->GetNumItems();i++) {
        FriendsListItem* item=static_cast<FriendsListItem*>(this->mFriendsList->GetItem(i));
        if(item->mFriend.get()!=NULL&&item->mFriend->GetUserId()==inFriend->GetUserId())
            return item;
    }
    return NULL;
}
// Returns the friend of the selected entry, or NULL
LobbyFriend* FriendsListCtrl::GetSelectedFriend()
{
    FriendsListItem* item=static_cast<FriendsListItem*>(this->mFriendsList->GetSelItem());
    if(item!=NULL) return item->mFriend.get();
    return NULL;
}
// Adds, removes or refreshes a friend's entry, then updates the buttons and separator
void FriendsListCtrl::HandleFriendChanged(FriendChangedEvent* inEvent)
{
    if(inEvent->IsAdd()) {
        SmartPtr<FriendsListItem> item=new FriendsListItem;
        this->UpdateFriend(inEvent->mFriend.get(),item.get());
        this->mFriendsList->InsertItem(item.get(),-1);
    } else if(inEvent->IsDelete()) {
        FriendsListItem* item=this->FindFriend(inEvent->mFriend.get());
        if(item==NULL) return;
        this->mFriendsList->RemoveItem(item);
    } else {
        FriendsListItem* item=this->FindFriend(inEvent->mFriend.get());
        if(item==NULL) return;
        this->UpdateFriend(inEvent->mFriend.get(),item);
        this->mFriendsList->RefreshItem(item);
    }
    this->UpdateButtons();
    this->UpdateSeparator();
}
// Refreshes every friend's entry from its friend, then the buttons and separator
void FriendsListCtrl::RefreshFriends()
{
    this->mFriendsList->BeginUpdate();
    for(int i=0;i<this->mFriendsList->GetNumItems();i++) {
        FriendsListItem* item=static_cast<FriendsListItem*>(this->mFriendsList->GetItem(i));
        if(item!=NULL&&item->mFriend.get()!=NULL) this->UpdateFriend(item->mFriend.get(),item);
    }
    this->mFriendsList->EndUpdate();
    this->UpdateButtons();
    this->UpdateSeparator();
}
// Rebuilds the list from the lobby's friends plus the separator
void FriendsListCtrl::FillFriends() {
    this->mFriendsList->BeginUpdate();
    this->mFriendsList->Clear(true);
    this->mFriendsList->InsertItem(this->mSeparator.get(),-1);
    for(size_t i=0;i<LobbyGetFriendCount();++i) {
        SmartPtr<FriendsListItem> item=new FriendsListItem;
        this->UpdateFriend(LobbyGetFriendAt(i),item.get());
        this->mFriendsList->InsertItem(item.get(),-1);
    }
    this->mFriendsList->EndUpdate();
    this->UpdateButtons();
    this->UpdateSeparator();
}
// Enables Locate and Remove only while an entry is selected
void FriendsListCtrl::UpdateButtons()
{
    if(this->mFriendsList->GetSelItem()==NULL) {
        this->mLocateButton->SetEnabled(false);
        this->mRemoveButton->SetEnabled(false);
    } else {
        this->mLocateButton->SetEnabled(true);
        this->mRemoveButton->SetEnabled(true);
    }
}
// Handles Remove, Locate, selection, the right-click friend menu, double-click locate and friend list events
void FriendsListCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mRemoveButton.get()) {
            LobbyFriend* aFriend=this->GetSelectedFriend();
            if(aFriend!=NULL) this->DeliverEvent(new FriendActionEvent(3,aFriend,0),false);
        } else if(inEvent->mComponent==this->mLocateButton.get()) {
            LobbyFriend* aFriend=this->GetSelectedFriend();
            if(aFriend!=NULL) this->DeliverEvent(new FriendActionEvent(1,aFriend,0),false);
        } else break;
        return;
    case ComponentEvent_ListSelChanged:this->UpdateButtons();return;
    case ComponentEvent_ListItemClicked: {
        FriendsListItem* item=static_cast<FriendsListItem*>(this->mFriendsList->GetSelItem());
        ListItemClickedEvent* click=static_cast<ListItemClickedEvent*>(inEvent);
        if(item!=NULL&&item==click->mItem.get()&&item->mFriend.get()!=NULL) {
            if(click->GetButton()==2) {
                this->DeliverEvent(new FriendActionEvent(5,item->mFriend.get(),0),false);
                return;
            }
            if(click->GetButton()==0&&click->IsDoubleClick())
                this->DeliverEvent(new FriendActionEvent(1,item->mFriend.get(),0),false);
        }
        return;
    }
    case 100014:this->FillFriends();return;
    case LobbyEvent_FriendChanged:this->HandleFriendChanged(static_cast<FriendChangedEvent*>(inEvent));return;
    case 100056:this->RefreshFriends();return;
    }
    Component::HandleComponentEvent(inEvent);
}
}
