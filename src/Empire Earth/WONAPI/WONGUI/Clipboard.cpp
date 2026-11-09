// Windows clipboard text get and set
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <memory.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
using namespace WONAPI;
// FUNCTION: WON 0x007cea00
// Returns the clipboard's text, preferring Unicode text over ANSI text; empty when there is neither
GUIString Clipboard::Get()
{
	GUIString aStr;

	OpenClipboard(NULL);
	HANDLE aHandle = GetClipboardData(CF_UNICODETEXT);
	if(aHandle==NULL)
	{
		aHandle = GetClipboardData(CF_TEXT);
		if(aHandle!=NULL)
			aStr = GUIString((char*)aHandle);
	}
	else
		aStr = GUIString((wchar_t*)aHandle);

	CloseClipboard();	

	return aStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// The original copies the 2-byte zero .bss word 0x4e60bc (the empty wide string EEWonNet.cpp also uses).
extern wchar_t gEmptyWideString[2];
// FUNCTION: WON 0x007ceb90
// Replaces the clipboard contents with the string, as Unicode text if a one-time test shows the clipboard
// accepts it, otherwise as ANSI text
void Clipboard::Set(const GUIString &theStr)
{
    HGLOBAL hData;
    LPVOID pData;
    OpenClipboard(NULL);
    EmptyClipboard();

	static bool checkedUnicode = false;
	static bool supportUnicode = false;
	if(!checkedUnicode)
	{
		checkedUnicode = true;

		hData = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, 2);
		pData = GlobalLock(hData);
		memcpy((void*)pData,gEmptyWideString,2);

		supportUnicode = SetClipboardData(CF_UNICODETEXT,hData)!=NULL?true:false;
		GlobalUnlock(hData);
	}


	int aLength = theStr.length()+1;
	int aCharSize = supportUnicode?sizeof(wchar_t):sizeof(char);
	hData = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, aLength*aCharSize);
	pData = GlobalLock(hData);

	if(supportUnicode)
	{
		std::wstring aStr = theStr;
		memcpy((void*)pData, aStr.c_str(), aLength*aCharSize);
	}
	else
	{
		std::string aStr = theStr;
		memcpy((void*)pData, aStr.c_str(), aLength*aCharSize);
	}

	GlobalUnlock(hData);
    SetClipboardData(supportUnicode?CF_UNICODETEXT:CF_TEXT, hData);
    CloseClipboard();
}
