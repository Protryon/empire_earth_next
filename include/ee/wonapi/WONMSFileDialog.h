#ifndef EE_WON_MS_FILE_DIALOG_H
#define EE_WON_MS_FILE_DIALOG_H
#include <list>
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class MSFileDialog : public Dialog
{
public:
	struct FilterPair
	{
		GUIString mFilter;
		GUIString mDesc;

		FilterPair() { }
		FilterPair(const GUIString &f, const GUIString &d) : mFilter(f), mDesc(d) { }
	};
	typedef std::list<FilterPair> FilterList;
	typedef std::list<GUIString> FileList;

protected:
	bool mIsSave;
	bool mGetDir;
	bool mAllowMultiple;
	bool mWarnIfExists;
	bool mFileMustExist;
	bool mNoChangeDir;
	FilterList mFilterList;
	FileList mFileList;
	GUIString mTitle;
	GUIString mOkTitle;
	GUIString mInitialFileName;
	GUIString mInitialDir;

public:
	const GUIString& GetOkTitle() { return this->mOkTitle; }
	bool IsGetDir() { return this->mGetDir; }

public:
	MSFileDialog(bool isSave = false);

	void SetTitle(const GUIString &theTitle);
	void SetOkTitle(const GUIString &theOkTitle);
	void SetInitialFileName(const GUIString &theFileName);
	void SetInitialDir(const GUIString &theDir);
	void SetAllowMultiple(bool allowMultiple);
	void SetWarnIfExists(bool warnIfExists);
	void SetFileMustExist(bool mustExist);
	void SetGetDir(bool getDir);
	void SetNoChangeDir(bool noChange);
	void AddFilter(const GUIString &theFilter, const GUIString &theDescription);

	const FileList& GetFileList();
	GUIString GetFile();

	bool DoDialog(Window *theParent);

};
typedef SmartPtr<MSFileDialog> MSFileDialogPtr;
}
#endif
