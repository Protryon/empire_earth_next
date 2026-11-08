// Special-purpose controls (not identified in the original exe).
#ifndef EE_WONAPI_WONLOBBYSPECIALPURPOSECONTROLS_H
#define EE_WONAPI_WONLOBBYSPECIALPURPOSECONTROLS_H

#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyBorderComponent.h"

namespace WONAPI
{

enum SortMode
{
	SortMode_String,
	SortMode_Numeric
};

///////////////////////////////////////////////////////////////////////////////
// Column header button
class ColHeaderButton : public Button
{
protected:
	SmartPtr<Font> mFont;
	std::string mText;
	DWORD mBackColor;
	bool mIsUp;
	SmartPtr<MultiListArea> mMultiListBox;
	int mCol;
	SortMode mSortMode;
	
	virtual ~ColHeaderButton();
	void DrawButtonText(Graphics &g, int xoff, int yoff);
	virtual void Activate();

public:
	virtual void Paint(Graphics &g);

public:
	ColHeaderButton();

	void SetText(const char *theText) { this->mText = theText; }
	void SetFont(Font *theFont) { this->mFont = theFont; }
	void SetBackColor(DWORD theBackColor) { this->mBackColor = theBackColor; }
	void SetMultiListBox(MultiListArea* theMultiListBox, int theCol, SortMode theSortMode) {this->mMultiListBox = theMultiListBox; this->mCol = theCol; this->mSortMode = theSortMode; }
};
typedef WONAPI::SmartPtr<ColHeaderButton> ColHeaderButtonPtr;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum ImageCheckButtonType
{
	ImageCheckButtonType_Normal				= 0,
	ImageCheckButtonType_Over				= 1,
	ImageCheckButtonType_Checked			= 2,
	ImageCheckButtonType_CheckedOver		= 3
};

class ImageCheckButton : public Button
{
protected:
	virtual ~ImageCheckButton();
	SmartPtr<Image> mImages[4];

	int mAlternateControlID;

public:
	ImageCheckButton(int theNormalControlID, int theCheckedControlID);
	virtual void Paint(Graphics &g);

	void SetImage(ImageCheckButtonType theType, Image *theImage) { this->mImages[theType] = theImage; }
	void AlternateControlIDs(void);
};
typedef WONAPI::SmartPtr<ImageCheckButton> ImageCheckButtonPtr;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class CheckButtonSplitterContainer : public Container
{
protected:
	virtual ~CheckButtonSplitterContainer();

	void ChangeAlternateButtonState(void);

	BorderComponent* mNormalSplitter;
	BorderComponent* mMaxMinSplitter;

	ImageCheckButton* mFirstMinButton;
	ImageCheckButton* mSecondMinButton;
	ImageCheckButton* mFirstMaxButton;
	ImageCheckButton* mSecondMaxButton;

	enum MaxMinStatus
	{
		Status_Min,
		Status_Normal,
		Status_Max
	};

	MaxMinStatus mStatus;

public:
	CheckButtonSplitterContainer(void);

	virtual void HandleComponentEvent(ComponentEvent *theEvent);
	virtual void MouseDown(int x, int y, int theButton);

	void AddSplitterBars(BorderComponent* theNormalSplitter, BorderComponent* theMaxMinSplitter);
	void AddMinButtons(ImageCheckButton* theFirstMinButton, ImageCheckButton* theSecondMinButton);
	void AddMaxButtons(ImageCheckButton* theFirstMaxButton, ImageCheckButton* theSecondMaxButton);
};
typedef WONAPI::SmartPtr<CheckButtonSplitterContainer> CheckButtonSplitterContainerPtr;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class BorderImageButton : public ImageButton
{
protected:
	virtual ~BorderImageButton() {}
	SmartPtr<BorderComponent> mImages[5];

public:
	BorderImageButton() : ImageButton() {}
	virtual void Paint(Graphics &g);
	virtual void Virtual08();

	void SetImage(ImageButtonType theType, BorderComponent *theImage) { this->mImages[theType] = theImage; }
};

typedef WONAPI::SmartPtr<BorderImageButton> BorderImageButtonPtr;

};

#endif