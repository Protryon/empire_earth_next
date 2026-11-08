// HTML document destructor and tag iteration (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"
#include "ee/wonapi/WONLobbyAnimation.h"
namespace WONAPI {
// FUNCTION: WON 0x0073c9e0
// Destroys an HTML document, releasing its tags, images and image maps
HTMLDocument::~HTMLDocument() {}
}

namespace WONAPI {
// Restarts tag iteration at the body start or the first tag (not identified in the original exe)
void HTMLDocument::Rewind(bool beginAtBodyStart)
{
	if(beginAtBodyStart)
		this->mTagItr = this->GetBodyStart();
	else
		this->mTagItr = this->mTagList.begin();
}

// Returns the next tag and advances, or NULL at the body end or list end (not identified in the original exe)
HTMLTag* HTMLDocument::GetNextTag(bool stopAtBodyEnd)
{
	if(stopAtBodyEnd)
	{
		if(this->mTagItr==this->mBodyEnd)
			return NULL;
	}
	else 
	{
		if(this->mTagItr==this->mTagList.end())
			return NULL;
	}

	return *this->mTagItr++;
}
}
