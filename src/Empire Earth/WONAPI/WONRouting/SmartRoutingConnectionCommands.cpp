// WON SDK smart-routing chat commands and quoted-word parsing
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <ctype.h>
#include <wctype.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingTypes.h"
using namespace std;
namespace WONAPI {
static bool Compare(const std::wstring &s1, const std::wstring &s2, bool &partialMatch)
{
	partialMatch = false;
	wstring::const_iterator i1 = s1.begin();
	wstring::const_iterator i2 = s2.begin();

	while ((i1 != s1.end()) && (i2 != s2.end()))
	{
		if (towupper(*i1) != towupper(*i2))
			return false;

		i1++;  i2++;
	}

	if(s1.size()<=s2.size())
		partialMatch = true;

	return (s1.size() == s2.size());
}

static void SkipWhitespace(const std::wstring &s1, int &thePos)
{
	while(thePos<s1.length() && iswspace(s1[thePos]))
		thePos++;
}

static std::wstring GetWord(const std::wstring &s1, int &thePos, bool doToUpper, bool checkQuotes)
{
	SkipWhitespace(s1,thePos);
	if(thePos>=s1.length())
		return L"";

	std::wstring aWord;
	if(checkQuotes && s1[thePos]==L'"')
	{
		int anEndQuotePos = s1.find_first_of(L'"',thePos+1);
		if(anEndQuotePos!=string::npos)
		{
			aWord = s1.substr(thePos+1,anEndQuotePos-thePos-1);
			thePos = anEndQuotePos+1;
			if(doToUpper)
				aWord = WStringToUpperCase(aWord);

			return aWord;
		}
	}

	while(thePos<s1.length())
	{
		wchar_t aChar = s1[thePos];
		if(doToUpper)
			aChar = towupper(aChar);

		if(aChar==L' ')
			return aWord;
		
		aWord+=aChar;
		thePos++;
	}

	return aWord;
}

static std::wstring GetRestOfLine(const std::wstring &s1, int thePos)
{
	SkipWhitespace(s1,thePos);
	if(thePos>=s1.length())
		return L"";

	std::wstring aWord;
	if(s1[thePos]==L'"')
	{
		int anEndQuotePos = s1.find_first_of(L'"',thePos+1);
		if(anEndQuotePos!=string::npos)
		{
			aWord = s1.substr(thePos+1,anEndQuotePos-thePos-1);
			return aWord;
		}
	}

	int anEndLinePos = s1.find_last_not_of(' ');
	if(anEndLinePos==string::npos || anEndLinePos<=thePos)
		return L"";

	return s1.substr(thePos,anEndLinePos-thePos+1);
}

SmartRoutingConnection::MatchResult SmartRoutingConnection::GetMemberByNameRef(RoutingGroupInfoPtr theGroup, 
																 const std::wstring &theName, 
																 RoutingMemberInfoPtr &theMemberOut)
{
	AutoCrit aCrit(this->mDataCrit);
	MatchResult aResult = MatchResult_NotFound;

	RoutingMemberMap::iterator anItr = theGroup->mMembers.begin();
	for(; anItr!=theGroup->mMembers.end(); ++anItr)
	{
		RoutingClientInfo *aClient = anItr->second->mClientInfo;
		if(aClient==NULL)
			continue;

		bool partialMatch = false;
		bool exactMatch = false;

		exactMatch = Compare(theName,aClient->mName,partialMatch);
		if(exactMatch)
		{
			if(aResult==MatchResult_Exact)
				return MatchResult_Ambiguous;

			aResult = MatchResult_Exact;
			theMemberOut = anItr->second;
		}
		else if(partialMatch)
		{
			if(aResult==MatchResult_NotFound)
			{
				aResult = MatchResult_Partial;
				theMemberOut = anItr->second;
			}
			else if(aResult==MatchResult_Partial)
				aResult = MatchResult_Ambiguous;
		}
	}

	return aResult;
}

SmartRoutingConnection::MatchResult SmartRoutingConnection::GetMemberByNameRef(unsigned short theGroupId, 
																 const std::wstring &theName, 
																 RoutingMemberInfoPtr &theMemberOut)
{
	AutoCrit aCrit(this->mDataCrit);

	RoutingGroupInfoPtr aGroup = this->GetGroup(theGroupId);
	if(aGroup.get()==NULL)
		return MatchResult_NotFound;
	else
		return this->GetMemberByNameRef(aGroup,theName,theMemberOut);
}

bool SmartRoutingConnection::HandleCommand(const std::wstring &theInput, int theGroupIdContext)
{
	AutoCrit aCrit(this->mDataCrit);

	int aPos = 1;
	std::wstring aCommand = GetWord(theInput,aPos,true,false);
	
	CommandStringMap::iterator anItr = this->mCommandStringMap.find(aCommand);
	if(anItr==this->mCommandStringMap.end())
	{
		this->mLastInputCommand = InputCommand_None;
		this->mLastInputError = InputError_InvalidCommand;
		return false;
	}

	this->mLastInputCommand = anItr->second;

	// Extract name if needed
	bool needName = false;
	switch(this->mLastInputCommand)
	{
		case InputCommand_Ignore:
		case InputCommand_Unmute:
		case InputCommand_ServerUnmute:
			this->mLastMatchName = GetRestOfLine(theInput,aPos);
			needName = true;
			break;

		case InputCommand_Whisper:
		case InputCommand_Mute:
		case InputCommand_ServerMute:
		case InputCommand_Ban:
		case InputCommand_ServerBan:
		case InputCommand_Invite:
		case InputCommand_Uninvite:
			this->mLastMatchName = GetWord(theInput,aPos,false,true);
			needName = true;
			break;
	}
	
	unsigned long aNonClientCDKeyIdToBan = -1;

	// Find corresponding client/member
	if(needName)
	{
		MatchResult aResult;
		
		// Is this a clientID or user name?
		if (this->mLastMatchName.empty())
		{
			this->mLastInputError = InputError_ClientNotFound;
			return false;
		}

		if (this->mLastInputCommand == InputCommand_ServerBan && (*this->mLastMatchName.begin() == '%'))
		{
			this->mLastMatchName.erase(this->mLastMatchName.begin());
			aNonClientCDKeyIdToBan = atoi(WStringToString(this->mLastMatchName).c_str());
		}
		else
		{		
			if(this->mServerWideCommands)
				aResult = this->FindClient(this->mLastMatchName, this->mLastTargetClient);
			else
			{
				aResult = this->GetMemberByNameRef(theGroupIdContext,this->mLastMatchName,this->mLastTargetMember);
				if(this->mLastTargetMember.get()!=NULL)
					this->mLastTargetClient = this->mLastTargetMember->mClientInfo;
			}

			if(aResult < this->mMatchTolerance)
			{
				if(aResult==MatchResult_Ambiguous)
					this->mLastInputError = InputError_ClientAmbiguous;
				else
					this->mLastInputError = InputError_ClientNotFound;

				return false;
			}
		}
	}

	SkipWhitespace(theInput,aPos);

	switch(this->mLastInputCommand)
	{
		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Whisper:
		{
			if(aPos>=theInput.length())
			{
				this->mLastInputError = InputError_NoChat;
				return false;
			}

			std::wstring aChat = theInput.substr(aPos);
			this->SendChat(aChat,RoutingChatFlag_IsWhisper,this->mLastTargetClient->mClientId,NULL);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Emote:
		{
			if(aPos>=theInput.length())
			{
				this->mLastInputError = InputError_NoChat;
				return false;
			}

			std::wstring aChat = theInput.substr(aPos-1);
			this->SendChat(aChat,RoutingChatFlag_IsEmote,theGroupIdContext,NULL);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Ignore:
		{
			this->IgnoreClient(this->mLastTargetClient,!((SmartRoutingClientInfo*)this->mLastTargetClient.get())->mIgnored);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Away:
		{
			bool away = (this->mMyClient->mFlags&RoutingClientFlag_IsAway)?true:false;
			this->SetClientFlags(RoutingClientFlag_IsAway,away?0:RoutingClientFlag_IsAway,0,0,NULL);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Invite:
		case InputCommand_Uninvite:
		{
			std::wstring aComment = theInput.substr(aPos);
			bool isInvited = this->mLastInputCommand==InputCommand_Invite;
			this->InviteClient(theGroupIdContext, this->mLastTargetClient->mClientId, isInvited, aComment,NULL);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_BecomeModerator:
		{
			this->BecomeModerator(!(this->mMyClient->mFlags&RoutingClientFlag_IsModerator),NULL);
			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Mute:
		case InputCommand_Ban:
		case InputCommand_ServerMute:
		case InputCommand_ServerBan:
		{		
			bool allowPerm = false;
			if (this->mLastInputCommand == InputCommand_ServerMute || this->mLastInputCommand == InputCommand_ServerBan)
				allowPerm = true;

			DWORD aSeconds = 3600; // 1 hour
			string aTimeStr = WStringToString(GetWord(theInput,aPos,true,false));
			if(aTimeStr=="INFINITE" && allowPerm)
				aSeconds = 0;
			else if(aTimeStr.length()>0)
			{
				char aUnitChar = aTimeStr[aTimeStr.length()-1];
				if(isalpha(aUnitChar))
					aTimeStr = aTimeStr.substr(0,aTimeStr.length()-1);

				aSeconds = atoi(aTimeStr.c_str());
				switch(aUnitChar)
				{
					case 'D': aSeconds*=24;
					case 'H': aSeconds*=60;	
					case 'M': aSeconds*=60; 
					case 'S': break;
					default: 
						this->mLastInputError = InputError_BadTime;
						return false;
				}

				if(aSeconds==0)
				{
					this->mLastInputError = InputError_BadTime;
					return false;
				}
			}
			
			// Get comment
			wstring aComment = GetWord(theInput,aPos,false,true);
			switch(this->mLastInputCommand)
			{
				case InputCommand_Mute:
					this->MuteClient(theGroupIdContext,true,aSeconds,aComment,this->mLastTargetClient->mClientId,NULL); 
					break;
			
				case InputCommand_ServerMute:
					this->MuteClient(RoutingId_Global,true,aSeconds,aComment,this->mLastTargetClient->mClientId,NULL); 
					break;

				case InputCommand_Ban:
					this->BanClient(theGroupIdContext,true,aSeconds,aComment,this->mLastTargetClient->mClientId,NULL); 
					break;

				case InputCommand_ServerBan:
					if (aNonClientCDKeyIdToBan == -1)
						this->BanClient(RoutingId_Global,true,aSeconds,aComment,this->mLastTargetClient->mClientId,NULL);
					else
						this->BanClientByCDKeyId(RoutingId_Global,true,aSeconds,aComment,aNonClientCDKeyIdToBan,NULL);
					break;
			}

			return true;
		}

		///////////////////////////////////////////
		///////////////////////////////////////////
		case InputCommand_Unmute:
		case InputCommand_ServerUnmute:
		{
			int aGroupId = theGroupIdContext;
			if(this->mLastInputCommand==InputCommand_ServerUnmute)
				aGroupId = RoutingId_Global;

			this->MuteClient(aGroupId,false,0,L"",this->mLastTargetClient->mClientId,NULL);
			return true;
		}
		
	}

	return true;
}

bool SmartRoutingConnection::HandleInput(const std::wstring &theInput, int theGroupIdContext)
{
	AutoCrit aCrit(this->mDataCrit);
	
	this->mLastInputError = InputError_None;
	this->mLastTargetMember = NULL;
	this->mLastTargetClient = NULL;
	this->mLastMatchName.erase();

	if(this->mMyClient.get()==NULL)
	{
		this->mLastInputError = InputError_NotLoggedIn;
		return false;
	}
		
	if(theGroupIdContext==-1)
		theGroupIdContext = this->mDefaultGroupId;

	if(theInput[0]==L'/')
		return this->HandleCommand(theInput, theGroupIdContext);
	
	if(this->mDoColonEmote && theInput[0]==L':')
	{
		this->mLastInputCommand = InputCommand_Emote;

		wstring aChat = theInput.substr(1);
		int aPos = 0;
		SkipWhitespace(aChat,aPos);
		if(aPos==aChat.length())
		{
			this->mLastInputError = InputError_NoChat;
			return false;
		}

		this->SendChat(aChat,RoutingChatFlag_IsEmote,theGroupIdContext,NULL);
		return true;
	}

	this->mLastInputCommand = InputCommand_Broadcast;

	int aPos = 0;
	SkipWhitespace(theInput,aPos);
	if(aPos==theInput.length())
	{
		this->mLastInputError = InputError_NoChat;
		return false;
	}

	this->SendChat(theInput,0,theGroupIdContext,NULL);
	return true;
}

void SmartRoutingConnection::ClearCommandStrings()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mCommandStringMap.clear();
}

void SmartRoutingConnection::SetDoColonEmote(bool doColonEmote)
{
	this->mDoColonEmote = doColonEmote;	
}
}
