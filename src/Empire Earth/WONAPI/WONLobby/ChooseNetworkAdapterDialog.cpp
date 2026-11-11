// Dialog that lets the player pick the network adapter (local address) the lobby uses
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/ee_rust.h"
// The dialog's decisions are ee_won's dialogs module (rust/crates/ee_won/src/dialogs.rs); this unit keeps the
// dialog, its controls and events.
using namespace WONAPI;
// Adapter list row holding the adapter's host address (RTTI in the global namespace, not WONAPI)
class NetworkAdapterListItem : public MultiListItem {
public:
    NetworkAdapterListItem(long inHost) : mHost(inHost) {}
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    long mHost; // 0x28; 0x2c bytes in all (the inherited column vector is at 0x18)
};
// FUNCTION: WON 0x7f9c00
// Sorts the default adapter row (host 0) first, the others by their column text
int NetworkAdapterListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int anOrder=ee_won_dlg_adapter_order((int32_t)this->mHost,(int32_t)static_cast<const NetworkAdapterListItem*>(inOther)->mHost);
    if(anOrder!=2) return anOrder;
    return MultiListItem::Compare(inOther,inList);
}
namespace WONAPI {

ChooseNetworkAdapterDialog::ChooseNetworkAdapterDialog() {}
// FUNCTION: WON 0x7f9c80
// Destroys the adapter dialog (members only)
ChooseNetworkAdapterDialog::~ChooseNetworkAdapterDialog() {}
// FUNCTION: WON 0x7f9d50
// Looks up the adapter list and the activate button from the dialog layout
void ChooseNetworkAdapterDialog::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mAdapterList,"AdapterList",__FILE__,60);
    SafeGetComponent(aConfig,this->mActivateButton,"ActivateButton",__FILE__,61);
}
// FUNCTION: WON 0x7f9da0
// Lists the default entry and every adapter (name, address), selecting the one in use
void ChooseNetworkAdapterDialog::Reset()
{
    this->mAdapterList->BeginUpdate();
    this->mAdapterList->Clear(false);
    this->mAdapterList->InsertItem(new NetworkAdapterListItem(0),-1);
    this->mAdapterList->SetString(0,0,::ChooseNetworkAdapterDialog_Default_String,NULL);
    if(WONAPI::IPAddr::GetLocalHost()==0) this->mAdapterList->SetSelItem(0,false);
    std::list<LobbyNetAddress> anAddresses;
    LobbyGetNetAddresses(anAddresses,true);
    const std::list<LobbyNetAddress>& aList=anAddresses;
    for(std::list<LobbyNetAddress>::const_iterator anItr=aList.begin();anItr!=aList.end();++anItr) {
        const LobbyNetAddress& aNet=*anItr;
        IPAddr anAddress(aNet.mHandle,0);
        GUIString aName(aNet.mName);
        if(aName.length()==0) aName=::ChooseNetworkAdapterDialog_Unknown_String;
        this->mAdapterList->InsertItem(new NetworkAdapterListItem(aNet.mHandle),-1);
        this->mAdapterList->SetString(this->mAdapterList->GetNumItems()-1,0,aName,NULL);
        this->mAdapterList->SetString(this->mAdapterList->GetNumItems()-1,1,anAddress.GetHostString(true),NULL);
        if(WONAPI::IPAddr::GetLocalHost()==aNet.mHandle) this->mAdapterList->SetSelItem(this->mAdapterList->GetNumItems()-1,true);
    }
    this->mAdapterList->EndUpdate();
}
// FUNCTION: WON 0x007fa1d0
// Returns the selected adapter's address, or the current local host when nothing is selected
unsigned long ChooseNetworkAdapterDialog::GetSelectedAddress()
{
    NetworkAdapterListItem* anItem=static_cast<NetworkAdapterListItem*>(this->mAdapterList->mSelItem);
    return ee_won_dlg_selected_address(anItem!=NULL,anItem==NULL ? 0 : (uint32_t)anItem->mHost,
                                       (uint32_t)WONAPI::IPAddr::GetLocalHost());
}
// Passes every event to Dialog; no marker: the original's 13-byte body is shared
void ChooseNetworkAdapterDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    Dialog::HandleComponentEvent(inEvent);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f9c40
// (public: __thiscall WONAPI::ChooseNetworkAdapterDialog::ChooseNetworkAdapterDialog(void)    [??0ChooseNetworkAdapterDialog@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f9c60
// (public: virtual void * __thiscall WONAPI::ChooseNetworkAdapterDialog::`scalar deleting destructor'(unsigned int)    [??_GChooseNetworkAdapterDialog@WO)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fa110
// (public: __thiscall WONAPI::MultiListItem::MultiListItem(void)    [??0MultiListItem@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fa0f0
// (public: virtual void * __thiscall NetworkAdapterListItem::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x007fa160
// (public: virtual __thiscall NetworkAdapterListItem::~NetworkAdapterListItem(void))
