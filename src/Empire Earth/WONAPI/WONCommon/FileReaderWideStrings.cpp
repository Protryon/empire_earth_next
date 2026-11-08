// FileReader wide-string reads from the WON SDK
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
void FileReader::ReadRawWString(std::wstring &theWStr, int theNumChars)
{
	theWStr.erase();
	theWStr.reserve(theNumChars);
	// UTF-16 units, converted one by one (wchar_t is 4 bytes outside Windows)
	for(int i=0; i<theNumChars; i++)
		WideAppendUtf16Unit(theWStr,this->ReadShort());
}

void FileReader::ReadWString(std::wstring &theWStr, unsigned char theLengthFieldSize)
{
	this->ReadRawWString(theWStr,this->ReadLength(theLengthFieldSize));
}
}
