// The skins' layouts (R6 WON round 37): ee_won's skinlay.rs lays out the composite controls' parts and sizes them,
// through SkinnedContainers.cpp's host
#ifndef EE_WONAPI_WONSKINLAYOUT_H
#define EE_WONAPI_WONSKINLAYOUT_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/skinlay.h"
extern "C" intptr_t ee_won_guicpp_skin_call(void*,EeWonGuiOp,intptr_t,intptr_t,intptr_t,intptr_t);
inline void WONSkinLayout(WONAPI::Component* c,EeWonSkinLayKind k,int a=0,intptr_t b=0,int cc=0,int* w=NULL,int* h=NULL)
{ ee_won_skin_layout(k,c,ee_won_guicpp_skin_call,a,b,cc,w,h); }
#endif
