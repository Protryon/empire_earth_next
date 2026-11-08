// WON SDK version/directory op; no original address identified
#ifndef __WON_GETVERSIONOP_H__
#define __WON_GETVERSIONOP_H__
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONDirExtended.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"



namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
typedef std::set<std::string> VersionSet;
typedef std::map<std::string,std::string> VersionDescriptionMap;
enum VersionResult
{
	Version_Invalid,
	Version_Valid_NotLatest,
	Version_Valid_Latest
};	

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class VersionBaseOp : public AsyncOpWithTracker
{
public:
	typedef int(*VersionCompare)(const std::string &s1, const std::string &s2);

protected:
	std::string mProduct;
	std::string mExtraConfig;
	SmartPtr<ServerContext> mDirServers;

	typedef std::set<std::string> FilterStringSet;
	FilterStringSet mFilterStringSet;
	

	VersionSet mValidVersionSet;
	VersionSet mFilteredVersionSet;
	std::string mHighestValidVersion;
	VersionCompare mVersionCompare;

	VersionDescriptionMap mVersionDescriptionMap;
	VersionDescriptionMap mFilteredVersionDescriptionMap;

	std::wstring GetVersionDir();
	void BuildValidVersionSet(const DirEntity *theEntity);
	static int DefaultVersionCompare(const std::string &s1, const std::string &s2) { return s1.compare(s2); }
	void BuildVersionFilterSet(const std::string &theVersionString, FilterStringSet &theVersionFilterSet);

public:
	VersionBaseOp() { this->mVersionCompare = VersionBaseOp::DefaultVersionCompare; }

	void SetDirServers(ServerContext *theDirServers) { this->mDirServers = theDirServers; }
	void SetProduct(const std::string &theProduct) { this->mProduct = theProduct; }
	void SetExtraConfig(const std::string &theExtraConfig) { this->mExtraConfig = theExtraConfig; }

	const std::string& GetProduct() { return this->mProduct; }
	const std::string& GetExtraConfig() { return this->mExtraConfig; }
	ServerContext* GetDirServers() { return this->mDirServers; }
	
	void SetValidVersionSet(const VersionSet &theSet) { this->mValidVersionSet = theSet; }
	void SetFiltereVersionSet(const VersionSet &theSet) { this->mFilteredVersionSet = theSet; }
	const VersionSet& GetValidVersionSet() { return this->mValidVersionSet; }
	const VersionSet& GetFilteredVersionSet() { return this->mFilteredVersionSet; }
	const std::string& GetHighestValidVersion() { return this->mHighestValidVersion; }

	const VersionDescriptionMap& GetVersionDescriptionMap() { return this->mVersionDescriptionMap; }

	void SetVersionCompare(VersionCompare theCompareFunc) { this->mVersionCompare = theCompareFunc; }

	void SetFilterString(const std::string &theFilterString);
	bool IsFiltered(const std::string &theVersion, const std::string &theVersionData);
	VersionResult CheckVersion(const std::string &theVersionId);
	bool VersionIsLatest(const std::string &theVersionId);
	bool VersionIsValid(const std::string &theVersionId);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GetVersionOp : public VersionBaseOp
{
protected:
	void DoFinish(GetDirOp *theOp);

	virtual bool CallbackHook(AsyncOp *theOp, int theId);
	virtual void RunHook();

public:
	GetVersionOp(ServerContext *theDirServers, const std::string &theProduct);

};

typedef SmartPtr<GetVersionOp> GetVersionOpPtr;

}; // namespace WONAPI

#endif

