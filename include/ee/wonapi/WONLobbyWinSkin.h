// WinSkin (not identified in the original exe).
#ifndef EE_WONAPI_WONLOBBYWINSKIN_H
#define EE_WONAPI_WONLOBBYWINSKIN_H

#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbyStretchImage.h"

namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class WinSkin : public RefCount
{
protected:
	std::string mFileName;
	std::string mFilePath;

	typedef std::map<int,int> ColorMap;
	typedef std::map<int,SmartPtr<Font> > FontMap;

	Window *mWindow;
	ColorMap mColorMap;
	FontMap mFontMap;

	int GetColor(int theIndex);
	Font* GetFont(int theIndex);

	int GetColor(const char *theSection, const char *theKey);
	int GetColorSchemeColor(const char *theKey);
	Font* GetFont(const char *theSection, const char *theKey);

	SmartPtr<RawImage> GetRawImage(const char *theSection, const char *theKey, bool checkTrans = true);
	SmartPtr<StretchImage> GetStretchImage(const char *theSection, const char *theKey);
	std::string GetPathParam(const char *theSection, const char *theKey);
	std::string GetStrParam(const char *theSection, const char *theKey, const char *theDefault = "");
	int GetIntParam(const char *theSection, const char *theKey, int theDefault = 0);

	SmartPtr<WONButton> CalcButton(const char *theSection, const char *theBitmapName, WONButton *orig);
	SmartPtr<WONButton> CalcCheckButton(const char *theBitmapName, WONButton *orig);
	void CalcArrows();
	void CalcComboButton();
	void CalcColorScheme();
	void CalcTabCtrlFrame();
	void CalcSunkEdge();
	void CalcPopupBack();
	void CalcDialogBack();
	void CalcCloseButton();

	SmartPtr<WONSkin> mSkin;


public:
	WinSkin();
	WONSkin* Load(const char *theFilePath);

	WONSkin* GetSkin() { return this->mSkin; }
};
typedef SmartPtr<WinSkin> WinSkinPtr;

};

#endif