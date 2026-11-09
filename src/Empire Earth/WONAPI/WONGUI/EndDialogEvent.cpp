// EndDialogEvent: ends a dialog with a result when delivered (not identified in the original exe)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONEndDialogEvent.h"
using namespace WONAPI;
EndDialogEvent::EndDialogEvent(int theResult, Window *theWindow, Component *theComponent) :
  mResult(theResult), mWindowRef(theWindow), mComponent(theComponent) 
{
	this->mWindow = theWindow; 
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
EndDialogEvent::~EndDialogEvent()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EndDialogEvent::Deliver()
{
	this->mWindow->EndDialog(this->mResult,this->mComponent);
}
