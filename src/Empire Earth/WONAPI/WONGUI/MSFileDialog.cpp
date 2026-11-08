// MSFileDialog: the Windows open/save file dialog (WON SDK source; not found in the original exe)
// Not part of any unit; it would build with -O2 -MD -GR -GX -DEE_DINKUM_SXL233 like the other WONGUI units
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <list>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONMSFileDialog.h"
using namespace std;
namespace WONAPI {
MSFileDialog::MSFileDialog(bool isSave)
{
	this->mIsSave = isSave;
	this->mAllowMultiple = false;
	this->mWarnIfExists = false;
	this->mGetDir = false;
	this->mNoChangeDir = false;

	this->mFileMustExist = !isSave;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && !defined(_XBOX)
static MSFileDialog *gFileDialog = NULL;
static string gLastPath;
static bool gGotDir;
static UINT CALLBACK MyOFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	if(gFileDialog==NULL)
		return 0;

	if (uiMsg == WM_NOTIFY && gFileDialog->IsGetDir())
    {
     	LPOFNOTIFYA lpofn = (LPOFNOTIFYA)lParam;
        if(lpofn->hdr.code==CDN_FOLDERCHANGE)
		{
			string aNewPath;
			char aBuf[MAX_PATH+1];
			SendMessageA(::GetParent(hdlg),CDM_GETFOLDERPATH,MAX_PATH,(LPARAM)aBuf);
			aNewPath = aBuf;
			if(aNewPath==gLastPath) // user hit ok button
			{
				PostMessageA(::GetParent(hdlg),WM_COMMAND,MAKELONG( IDCANCEL, 0 ) ,NULL);
				gGotDir = true;
	       		return 1;
			}
			else
				gLastPath = aNewPath;
		}
    } 
	else if(uiMsg==CDN_INCLUDEITEM && gFileDialog->IsGetDir())
	{
		OFNOTIFYEXA *anInfo = (OFNOTIFYEXA*)lParam;

	}
	else if(uiMsg==WM_INITDIALOG)
	{

		if(gFileDialog->GetOkTitle().length()!=0)
		{
			string aStr = gFileDialog->GetOkTitle();
			SendMessageA(::GetParent(hdlg),CDM_SETCONTROLTEXT,IDOK,(LPARAM)aStr.c_str()); 
		}
	}
	

	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Makes the dialog pick a folder instead of a file (the filter list becomes a dummy Directories entry)
void MSFileDialog::SetGetDir(bool getDir)
{
	this->mGetDir = getDir;
#if defined(_WIN32)
	this->mFilterList.clear();
	this->AddFilter("*.zzz","Directories");
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the dialog's title
void MSFileDialog::SetTitle(const GUIString &theTitle)
{
	this->mTitle = theTitle;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the text of the OK button
void MSFileDialog::SetOkTitle(const GUIString &theOkTitle)
{
	this->mOkTitle = theOkTitle;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the file name the dialog starts with
void MSFileDialog::SetInitialFileName(const GUIString &theFileName)
{
	this->mInitialFileName = theFileName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the folder the dialog starts in
void MSFileDialog::SetInitialDir(const GUIString &theDir)
{
	this->mInitialDir = theDir;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Allows picking several files (open dialogs only)
void MSFileDialog::SetAllowMultiple(bool allowMultiple)
{
	if(!this->mIsSave)
		this->mAllowMultiple = allowMultiple;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets whether the picked file must already exist
void MSFileDialog::SetFileMustExist(bool mustExist)
{
	this->mFileMustExist = mustExist;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets whether to confirm overwriting an existing file (save dialogs only)
void MSFileDialog::SetWarnIfExists(bool warnIfExists)
{
	if(this->mIsSave)
		this->mWarnIfExists = warnIfExists;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets whether the dialog leaves the current directory unchanged
void MSFileDialog::SetNoChangeDir(bool noChange)
{
	this->mNoChangeDir = noChange;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Adds a file type filter with its description
void MSFileDialog::AddFilter(const GUIString &theFilter, const GUIString &theDescription)
{
	this->mFilterList.push_back(FilterPair(theFilter,theDescription));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Returns the picked files
const MSFileDialog::FileList& MSFileDialog::GetFileList()
{
	return this->mFileList;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Returns the first picked file, or an empty string
GUIString MSFileDialog::GetFile()
{
	if(this->mFileList.empty())
		return "";
	else
		return this->mFileList.front();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Shows the open or save dialog; returns false when cancelled, else fills the file list (a folder in folder mode;
// each picked file joined to the folder with '/' when several are allowed)
bool MSFileDialog::DoDialog(Window *theParent)
{
	this->mFileList.clear();
#if defined(_WIN32) && !defined(_XBOX)
	const int MAX_FILE_NAME = 8192;
	char aFileName[MAX_FILE_NAME];
	char aInitialDir[MAX_FILE_NAME];
	OPENFILENAMEA aStruct;
	memset(&aStruct,0,sizeof(aStruct));
	aStruct.lStructSize = sizeof(aStruct);
	aStruct.hwndOwner = static_cast<MSWindow*>(theParent)->GetHWND();

	std::string aTitle = this->mTitle;
	aStruct.lpstrTitle = aTitle.c_str();
	aStruct.lpfnHook = MyOFNHookProc;
	gFileDialog = this;
	gGotDir = false;
	gLastPath.erase();

	std::string aFilterStr;
	for(FilterList::iterator anItr = this->mFilterList.begin(); anItr!=this->mFilterList.end(); ++anItr)
	{
		const FilterPair &aPair = *anItr;
		aFilterStr.append(aPair.mDesc);
		aFilterStr.append(1,'\0');
		aFilterStr.append(aPair.mFilter);
		aFilterStr.append(1,'\0');
	}
	aFilterStr.append(2,'\0');
	aStruct.lpstrFilter = aFilterStr.c_str();

	aFileName[0] = '\0';
	std::string strFileName = this->mInitialFileName;
	strcpy(aFileName, strFileName.c_str());
	aStruct.lpstrFile = aFileName;
	aStruct.nMaxFile = MAX_FILE_NAME-1;

	if(this->mInitialDir.length())
	{
		std::string strInitialDir = this->mInitialDir;
		strcpy(aInitialDir, strInitialDir.c_str());
		aStruct.lpstrInitialDir = aInitialDir;
	}

	aStruct.Flags = OFN_EXPLORER;
	if(this->mAllowMultiple)
		aStruct.Flags |= OFN_ALLOWMULTISELECT;
	if(this->mWarnIfExists)
		aStruct.Flags |= OFN_OVERWRITEPROMPT;
	if(this->mFileMustExist)
		aStruct.Flags |= OFN_FILEMUSTEXIST;
	if(this->mNoChangeDir)
		aStruct.Flags |= OFN_NOCHANGEDIR;
	if(this->mGetDir || this->mOkTitle.length()!=0)
		aStruct.Flags |= OFN_ENABLEHOOK;

	BOOL aResult = FALSE;
	if(this->mIsSave)
		aResult = GetSaveFileNameA(&aStruct);
	else
		aResult = GetOpenFileNameA(&aStruct);

	gFileDialog = NULL;
	if(gGotDir) // user just selected a directory
		strcpy(aFileName,gLastPath.c_str());
	else if(!aResult)
		return false;
	else if(this->mGetDir) // strip filename off the folder
	{
		WONFile aFile(aFileName);
		string aFileNameStr = aFile.GetFileDirectory();
		strcpy(aFileName,aFileNameStr.c_str());
	}

	if(this->mAllowMultiple)
	{
		const char *aPtr = aFileName;
		GUIString aDirectory = aPtr;
		aPtr+=aDirectory.length()+1;
		while(true)
		{
			GUIString aStr = aPtr;
			if(aStr.length()==0)
			{
				if(this->mFileList.empty()) // only one file
					this->mFileList.push_back(aDirectory);
			
				break;
			}
			else
			{
				GUIString aFullPath = aDirectory;
				aFullPath.append('/');
				aFullPath.append(aStr);
				this->mFileList.push_back(aFullPath);
			}

			aPtr += aStr.length() + 1;
		}
	}
	else
		this->mFileList.push_back(aFileName);

	return true;
#else
	return false;
#endif	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
