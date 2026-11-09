// No original address identified: WON SDK account lookup/reset DBProxy operations
#ifndef EE_WONAPI_WONACCOUNTFROMCDKEY_H
#define EE_WONAPI_WONACCOUNTFROMCDKEY_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// This Op is called by the client to retrieve account information
// using a valid product cd key and community name to identify the account
class GetAccountFromCDKeyOp: public DBProxyOp
{
	// Attributes
protected:
	std::string		mCDKey;		// [in]
	unsigned long	mCommunity; // [in]

	std::wstring	mUserName;	// [out]
	std::wstring	mPassword;	// [out]
	std::string		mEmail;		// [out]
	std::string		mLastLogin; // [out]
	std::string		mActivated; // [out]
	

	SmartPtr<ProfileData>	mProfileData;

	// Overrides
protected:
	enum MSGTYPE { MSGTYPE = 10 };				// DO NOT MODIFY (MUST MATCH WITH SERVER)
	virtual WONStatus CheckResponse();			// Recv - For Extended Unpack
	virtual void RunHook();						// Send - For Extended Pack
	virtual void Reset();

public:
	void SetCDKey(const char* theCDKey);

	std::wstring& GetUserName() { return this->mUserName; }
	std::wstring& GetPassword() { return this->mPassword; }
	std::string&  GetEmail()	{ return this->mEmail; }
	std::string&  GetLastLogin(){ return this->mLastLogin; }
	std::string&  GetActivated(){ return this->mActivated; }
	std::string&  GetCDKey()	{ return this->mCDKey; }

	void SetProfileData(ProfileData* theData) { this->mProfileData = theData; }
	ProfileData* GetProfileData() { return this->mProfileData.get(); }
	// Constructor
public:
	GetAccountFromCDKeyOp(ServerContext* theContext, std::string theCDKey, unsigned long theCommunity);

	// Destructor
protected:
	virtual ~GetAccountFromCDKeyOp() {}		// Safety Lock -- Enforces SmartPtr Use.


};
typedef SmartPtr<GetAccountFromCDKeyOp> GetAccountFromCDKeyOpPtr;		// Smart Ptr Template


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// This Op is called by the client to retrieve account information
// using a valid product cd key and community name to identify the account
class GetAccountFromCDKeyExOp: public GetAccountFromCDKeyOp
{
	// Attributes
public:
	enum ACTIONFLAG { RESET = 0, BAN = 1 };

protected:
	/* Inherited
	std::string		mCDKey;		// [in]
	unsigned long	mCommunity; // [in]

	std::wstring	mUserName;	// [out]
	std::wstring	mPassword;	// [out]
	std::string		mEmail;		// [out]
	std::string		mLastLogin; // [out]
	std::string		mActivated; // [out]

	SmartPtr<ProfileData>	mProfileData;
	*/
	ACTIONFLAG		mActionFlag;

	// Overrides
protected:
	enum MSGTYPE { MSGTYPE = 12 };				// DO NOT MODIFY (MUST MATCH WITH SERVER)
	//virtual WONStatus Virtual14();			// Recv - For Extended Unpack
	virtual void RunHook();						// Send - For Extended Pack
	//virtual void Virtual18();

public:
	/* Inherited
	void SetCDKey(const char* theCDKey);
	std::string& GetCDKey() { return mCDKey; };

	std::wstring& GetUserName() { return mUserName; }
	std::wstring& GetPassword() { return mPassword; }
	std::string&  GetEmail()	{ return mEmail; }
	std::string&  GetLastLogin(){ return mLastLogin; }
	std::string&  GetActivated(){ return mActivated; }

	void SetProfileData(ProfileData* theData) { mProfileData = theData; }
	ProfileData* GetProfileData() { return mProfileData.get(); }
	*/

	void SetActionFlag(ACTIONFLAG actionFlag) { this->mActionFlag = actionFlag; }
	ACTIONFLAG GetActionFlag() { return this->mActionFlag; }
	static const char* GetActionString(ACTIONFLAG actionFlag);

	// Constructor
public:
	GetAccountFromCDKeyExOp(ServerContext* theContext, std::string theCDKey, unsigned long theCommunity);

	// Destructor
protected:
	virtual ~GetAccountFromCDKeyExOp() {}		// Safety Lock -- Enforces SmartPtr Use.


};
typedef SmartPtr<GetAccountFromCDKeyExOp> GetAccountFromCDKeyOpExPtr;		// Smart Ptr Template


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}
#endif
