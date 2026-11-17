// The Windows skin's drawing (R6 WON round 36): MSGraphicsUtil.cpp's helpers and every MS control's paint, the
// statements copied from the C++ before the port (MSGraphicsUtil.cpp, MSButton.cpp, MSTabButton.cpp,
// MSRemainingPainters.cpp, MSArrowButton.cpp, MS3DFrame.cpp, MSPainters.cpp, MSBorderPainters.cpp,
// MSControlPainters.cpp, MSEtchedFrameGroup.cpp, MSSkinImage.cpp) over a logged Graphics, against ee_won's plans
// (mspaint.rs) executed as WONGUIDraw.h's WONDrawOne executes them over the same Graphics. The component's state
// the original read (its size, pressed, disabled, default, transparent, checked, the frames' flags, the
// scrollbars, the label, the sort type, the image's type and flags) is drawn at random for each paint, sizes
// from -3 to 60 with now and then a large one; every Graphics call is traced, and the base class's paint, which
// the C++ makes around the plan, is not part of either side. DrawButtonText is traced with its offsets.
//
// 300,000 paints; the traces must be the same. The first 400 paints and every later one that makes a kind of
// trace line none before made (per paint kind) are rust/crates/ee_won_gui/src/golden_mspaint.txt (`--print`), which
// the Rust tests replay.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/mspaint.h"

static int sFailures;
static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static std::vector<std::string> sTrace;
static void T(const char* fmt, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0, int f = 0)
{
    char aLine[200];
    snprintf(aLine, sizeof(aLine), fmt, a, b, c, d, e, f);
    sTrace.push_back(aLine);
}

typedef unsigned long DWORD;
enum StandardColor { StandardColor_3DDarkShadow = 0, StandardColor_3DFace, StandardColor_3DHilight,
    StandardColor_3DShadow, StandardColor_Scrollbar, StandardColor_ButtonText, StandardColor_GrayText,
    StandardColor_Hilight, StandardColor_HilightText, StandardColor_ToolTipBack, StandardColor_ToolTipText,
    StandardColor_MenuBack, StandardColor_MenuText, StandardColor_Back, StandardColor_Text };
enum Direction { Direction_Left, Direction_Right, Direction_Up, Direction_Down };
enum ListSortType { ListSortType_Ascending, ListSortType_Descending, ListSortType_None };

// the logged Graphics: the calls the paints make, each one trace line
struct Graphics {
    void SetSysColor(int c) { T("sys %d", c); }
    DWORD GetStandardColor(int c) { return 0x5000 + (DWORD)c; }
    void FillRect(int x, int y, int w, int h) { T("fill %d %d %d %d", x, y, w, h); }
    void DrawRect(int x, int y, int w, int h) { T("rect %d %d %d %d", x, y, w, h); }
    void DrawLine(int x1, int y1, int x2, int y2) { T("line %d %d %d %d", x1, y1, x2, y2); }
    void Translate(int x, int y) { T("translate %d %d", x, y); }
    void SetPixel(int x, int y, DWORD c) { T("pixel %d %d %x", x, y, (int)c); }
    void FillEllipse(int x, int y, int w, int h) { T("ellipse %d %d %d %d", x, y, w, h); }
    void DrawArc(int x, int y, int w, int h, int s, int a) { T("arc %d %d %d %d %d %d", x, y, w, h, s, a); }
};

// the component's state a paint reads
struct World {
    int width, height;
    bool pressed, disabled, transparent, isDefault, frame2, checked, drawFrame, draw3D, vert, horz, invalid;
    int a, b, c, d, e;
};
static World w;

namespace orig {
// ---- MSGraphicsUtil.cpp (unmodified statements)
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawRect(0,0,width,height);
	g.SetSysColor(StandardColor_3DDarkShadow);

	if(down)
	{
		g.DrawLine(0,0,width-1,0);
		g.DrawLine(0,0,0,height-1);

		g.SetSysColor(StandardColor_3DFace);
		g.DrawLine(width-2,1,width-2,height-2);
		g.DrawLine(width-2,height-2,2,height-2);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,1,width-2,1);
		g.DrawLine(1,1,1,height-2);
	}
	else
	{
		g.DrawLine(width-1,0,width-1,height-1);
		g.DrawLine(0,height-1,width-1,height-1);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,height-2,width-2,height-2);
		g.DrawLine(width-2,height-2,width-2,1);
	}

	g.Translate(-x,-y);
}

void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	if(!down)
	{
		g.SetSysColor(StandardColor_3DDarkShadow);
		g.DrawLine(width-1,0,width-1,height-1);
		g.DrawLine(0,height-1,width-1,height-1);

		g.SetSysColor(StandardColor_3DHilight);
		g.DrawLine(1,1,1,height-2);
		g.DrawLine(1,1,width-2,1);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,height-2,width-2,height-2);
		g.DrawLine(width-2,height-2,width-2,1);
	}
	else
	{
		g.SetSysColor(StandardColor_3DShadow);
		g.DrawRect(0,0,width,height);
	}

	g.Translate(-x,-y);
}

struct MSGraphicsUtil {
    static void DrawArrow(Graphics&,Direction,int,int,int,int,bool);
    static void DrawFrame3(Graphics&,int,int,int,int,bool);
    static void DrawTab(Graphics&,int,int,int,int);
    static void DrawCheck(Graphics&,int,int,int,int,bool,bool);
    static void DrawRadio(Graphics&,int,int,int,int,bool,bool);
    static void DrawCloseX(Graphics&,int,int,int,int);
};

void MSGraphicsUtil::DrawFrame3(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawRect(0,0,width,height);
	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(width-1,0,width-1,height-1);
	g.DrawLine(0,height-1,width-1,height-1);
	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(1,1,width-2,1);
	g.DrawLine(1,1,1,height-2);
	g.SetSysColor(StandardColor_3DFace);
	g.DrawLine(width-2,1,width-2,height-2);
	g.DrawLine(1,height-2,width-2,height-2);

	g.Translate(-x,-y);
}

void MSGraphicsUtil::DrawArrow(Graphics &g, Direction theDirection, int x, int y, int width, int height, bool disabled)
{
	int i = 0;

	if(disabled)
		g.SetSysColor(StandardColor_GrayText);
	else
		g.SetSysColor(StandardColor_ButtonText);

	int arrow_size = 4;

	int arrow_size2 = arrow_size/2;


	if(theDirection==Direction_Left)
	{
		int x = width/2-arrow_size2, y = height/2;
		for(i=0; i<arrow_size; i++)
			g.DrawLine(x+i, y-i, x+i, y+i);

		if(disabled)
		{
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+i,y-i+arrow_size2,x+i,y+i);
		}
	}
	else if(theDirection==Direction_Right)
	{
		int x = width/2-arrow_size2, y = height/2;
		for(i=0; i<arrow_size; i++)
		{
			g.DrawLine(x+(3-i), y-i, x+(3-i), y+i);
			if(!disabled || i>=3)
				continue;
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+(arrow_size-i-1),y+i+1,x+(arrow_size-i-1),y+i+2);
			g.SetSysColor(StandardColor_GrayText);
		}

		if(disabled)
			g.SetPixel(x+4,y+1,g.GetStandardColor(StandardColor_3DHilight));
	}
	else if(theDirection==Direction_Up)
	{
		int x = width/2, y = height/2-arrow_size2;
		for(i=0; i<arrow_size; i++)
			g.DrawLine(x-i, y+i, x+i, y+i);

		if(disabled)
		{
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x-i+2, y+i, x+i, y+i);
		}
	}
	else if(theDirection==Direction_Down)
	{
		int x = width/2, y = height/2-arrow_size2;
		for(i=0; i<arrow_size; i++)
		{
			g.DrawLine(x-(arrow_size-i-1), y+i, x+(arrow_size-i-1), y+i);
			if(!disabled || i<=0)
				continue;
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+(arrow_size-i-1)+1,y+i,x+(arrow_size-i-1)+2,y+i);
			g.SetSysColor(StandardColor_GrayText);
		}

		if(disabled)
			g.SetPixel(x+1,y+arrow_size,g.GetStandardColor(StandardColor_3DHilight));
	}
}

void MSGraphicsUtil::DrawCheck(Graphics &g, int x, int y, int width, int height, bool down, bool checked)
{
	g.Translate(x,y);

	if(down)
		g.SetSysColor(StandardColor_3DFace);
	else
		g.SetSysColor(StandardColor_Back);

	g.FillRect(0,0,width,height);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(0,0,width-1,0);
	g.DrawLine(0,0,0,height-1);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(width-1,0,width-1,height-1);
	g.DrawLine(0,height-1,width-1,height-1);

	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawLine(1,1,width-2,1);
	g.DrawLine(1,1,1,height-2);

	g.SetSysColor(StandardColor_3DFace);
	g.DrawLine(width-2,1,width-2,height-2);
	g.DrawLine(1,height-2,width-2,height-2);

	if(checked)
	{
		g.SetSysColor(StandardColor_Text);
		int x = 3;
		int y = height/2 - 1;
		int i;
		for(i=0; i<3; i++, x++, y++)
			g.DrawLine(x,y,x,y+2);

		y-=2;
		for(i=0; i<4; i++, x++, y--)
			g.DrawLine(x,y,x,y+2);
	}
	g.Translate(-x,-y);
}

void MSGraphicsUtil::DrawRadio(Graphics &g, int x, int y, int width, int height, bool down, bool checked)
{
	g.Translate(x,y);

	if(down)
		g.SetSysColor(StandardColor_3DFace);
	else
		g.SetSysColor(StandardColor_Back);

	g.FillEllipse(0,0,width,height);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawArc(0,0,width,height,45,180);
	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawArc(1,1,width-2,height-2,45,180);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawArc(0,0,width,height,235,180);
	g.SetSysColor(StandardColor_3DFace);
	g.DrawArc(1,1,width-2,height-2,235,180);

	if(checked)
	{
		g.SetSysColor(StandardColor_Text);
		g.FillEllipse(width/2-2,height/2-2,4,4);
	}

	g.Translate(-x,-y);
}

void MSGraphicsUtil::DrawTab(Graphics &g, int x, int y, int width, int height)
{
	g.SetSysColor(StandardColor_3DFace);
	g.FillRect(x,y+2,width-x,height-y-2);
	g.DrawLine(x+2,y+1,width-1,y+1);
	g.DrawLine(x+1,y+2,width-1,y+2);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(x,y+2,x,height-1);
	g.SetPixel(x+1,y+1,g.GetStandardColor(StandardColor_3DHilight));
	g.DrawLine(x+2,y,width-2,y);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(width-2,y+2,width-2,height-1);
	g.SetSysColor(StandardColor_3DDarkShadow);
	g.SetPixel(width-2,y+1,g.GetStandardColor(StandardColor_3DDarkShadow));
	g.DrawLine(width-1,y+2,width-1,height-1);
}

void MSGraphicsUtil::DrawCloseX(Graphics &g, int x, int y, int width, int height)
{
	g.Translate(x,y);
	g.SetSysColor(StandardColor_ButtonText);
	int hmid = width/2;
	int vmid = height/2;
	int left = hmid-4;
	int right = hmid+3;
	int top = vmid-4;
	int bottom = vmid+3;
	for(int i=0; i<4; i++)
	{
		g.DrawLine(left+i,top+i,left+i+1,top+i);
		g.DrawLine(right-i,top+i,right-1-i,top+i);
		g.DrawLine(left+i,bottom-i,left+i+1,bottom-i);
		g.DrawLine(right-i,bottom-i,right-i-1,bottom-i);
	}
	g.Translate(-x,-y);
}

// ---- the components' paints (unmodified statements but for the base paint, made by the C++ around the plan,
// and the component's reads, which are the world's)
struct Comp {
    int Width() { return w.width; }
    int Height() { return w.height; }
    bool IsPressed() { return w.pressed; }
    bool IsDisabled() { return w.disabled; }
    bool IsTransparent() { return w.transparent; }
    bool IsDefaultButton() { return w.isDefault; }
    int GetLeftTextPad() { return w.c; }
    void DrawButtonText(Graphics&, int x, int y) { T("text %d %d", x, y); }
};

struct MSButton : Comp {
    bool mUseFrame2;
    void Paint(Graphics& g) {
        bool down=this->IsPressed();
        int offset=down?1:0;
        g.SetSysColor(StandardColor_3DFace);
        if(!this->IsTransparent()) g.FillRect(0,0,this->Width(),this->Height());
        int x=0,y=0,width=this->Width(),height=this->Height();
        bool frame2=this->mUseFrame2;
        if(this->IsDefaultButton()) {
            g.SetSysColor(StandardColor_3DDarkShadow);
            g.DrawRect(0,0,this->Width(),this->Height());
            x++;y++;width-=2;height-=2;
            if(down) frame2=true;
        }
        if(frame2) LobbyDrawFrame(g,x,y,width,height,down);
        else LobbyDraw3DFrame(g,x,y,width,height,down);
        this->DrawButtonText(g,offset,offset);
    }
};
struct MSTabButton : Comp {
    void Paint(Graphics& g) {
        MSGraphicsUtil::DrawTab(g,0,0,this->Width(),this->Height());
        this->DrawButtonText(g,0,0);
    }
};
struct MSLabel : Comp {
    void Paint(Graphics& g) {
        g.SetSysColor(StandardColor_3DFace);
        if(!this->IsTransparent())
            g.FillRect(0,0,this->Width(),this->Height());
        this->DrawButtonText(g,0,0);
    }
};
struct TextW { int GetWidth() { return w.b; } };
struct MSHeaderButton : Comp {
    TextW aText; TextW* mText;
    MSHeaderButton() : mText(&aText) {}
    void Paint(Graphics& g) {
        ListSortType sortType=(ListSortType)w.a;
        if(sortType==ListSortType_None)
            return;
        g.SetSysColor(StandardColor_ButtonText);
        int x;
        if(this->mText->GetWidth()==0)
            x=(this->Width()-4)/2;
        else
            x=this->mText->GetWidth()+10+this->GetLeftTextPad();
        int y=this->Height()/2-2;
        int offset=this->IsPressed()?1:0;
        x+=offset;
        y+=offset;
        if(sortType==ListSortType_Ascending)
        {
            for(int i=0;i<4;i++)
                g.DrawLine(x-i,y+i,x+i,y+i);
        }
        else if(sortType==ListSortType_Descending)
        {
            for(int i=0;i<4;i++)
                g.DrawLine(x-(3-i),y+i,x+(3-i),y+i);
        }
    }
};
struct MSCloseButton : Comp {
    void Paint(Graphics& g) {
        bool down=this->IsPressed();
        int offset=down?1:0;
        g.SetSysColor(StandardColor_3DFace);
        g.FillRect(0,0,this->Width(),this->Height());
        LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),down);
        MSGraphicsUtil::DrawCloseX(g,offset,offset,this->Width(),this->Height());
    }
};
struct MSScrollThumb : Comp {
    void Paint(Graphics& g) {
        g.SetSysColor(StandardColor_3DFace);
        g.FillRect(0,0,this->Width(),this->Height());
        LobbyDrawFrame(g,0,0,this->Width(),this->Height(),false);
    }
};
struct MSArrowButton : Comp {
    Direction mDirection;
    void Paint(Graphics& g) {
        bool down=this->IsPressed();
        g.SetSysColor(StandardColor_3DFace);
        g.FillRect(0,0,this->Width(),this->Height());
        LobbyDrawFrame(g,0,0,this->Width(),this->Height(),down);
        if(down) MSGraphicsUtil::DrawArrow(g,this->mDirection,1,1,this->Width(),this->Height(),this->IsDisabled());
        else MSGraphicsUtil::DrawArrow(g,this->mDirection,0,0,this->Width(),this->Height(),this->IsDisabled());
    }
};
struct MS3DFrame : Comp {
    int mType; bool mIsUp;
    void Paint(Graphics& g) {
        switch(this->mType) {
        case 1:LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
        case 2:LobbyDrawFrame(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
        case 3:MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
        }
    }
};
struct MSPopup : Comp {
    void Paint(Graphics& g)
    {
        g.SetSysColor(StandardColor_MenuBack);
        g.FillRect(0,0,this->Width(),this->Height());
        g.SetSysColor(StandardColor_3DHilight);
        g.DrawLine(1,1,this->Width()-2,1);
        g.DrawLine(1,1,1,this->Height()-2);
        g.SetSysColor(StandardColor_3DDarkShadow);
        g.DrawLine(this->Width()-1,0,this->Width()-1,this->Height()-1);
        g.DrawLine(1,this->Height()-1,this->Width()-1,this->Height()-1);
        g.SetSysColor(StandardColor_3DShadow);
        g.DrawLine(this->Width()-2,0,this->Width()-2,this->Height()-2);
        g.DrawLine(2,this->Height()-2,this->Width()-2,this->Height()-2);
    }
};
struct MSDlgBack : Comp {
    void Paint(Graphics& g)
    {
        g.SetSysColor(StandardColor_3DFace);
        g.FillRect(0,0,this->Width(),this->Height());
    }
};
struct MSEtchedFrame : Comp {
    void Paint(Graphics& g)
    {
        g.SetSysColor(StandardColor_3DHilight);
        g.DrawRect(0,0,this->Width(),this->Height());
        g.DrawRect(1,1,this->Width()-1,this->Height()-1);
        g.SetSysColor(StandardColor_3DShadow);
        g.DrawRect(0,0,this->Width()-1,this->Height()-1);
    }
};
struct MSInputBox : Comp {
    void Paint(Graphics& g)
    {
        MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),true);
    }
};
struct ScrollRect { int mInvalidState; int Left() { return w.a; } int Top() { return w.b; } int Width() { return w.c; } int Height() { return w.d; } };
struct MSScrollbar : Comp {
    bool mIsTransparent; ScrollRect aRect;
    ScrollRect* GetScrollRect() { aRect.mInvalidState = w.invalid ? 1 : 0; return &aRect; }
    void Paint(Graphics& g)
    {
        if(this->GetScrollRect()->mInvalidState!=0 && !this->mIsTransparent)
        {
            g.SetSysColor(StandardColor_Scrollbar);
            g.FillRect(this->GetScrollRect()->Left(),this->GetScrollRect()->Top(),this->GetScrollRect()->Width(),this->GetScrollRect()->Height());
        }
    }
};
struct MSComboBox : Comp {
    bool mDrawFrame;
    void Paint(Graphics& g)
    {
        if(this->mDrawFrame)
            MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),true);
    }
};
struct Bar { bool vert; bool IsVisible() { return vert ? w.vert : w.horz; } int Left() { return vert ? w.a : 0; }
    int Top() { return vert ? w.b : 0; } int Width() { return vert ? w.c : 0; } int Height() { return vert ? w.d : w.e; } };
struct MSScrollContainer : Comp {
    bool mTransparent, mDrawFrame, mDraw3DFrame; Bar aVert, aHorz;
    Bar* GetScrollbar(bool v) { aVert.vert = true; aHorz.vert = false; return v ? &aVert : &aHorz; }
    void Paint(Graphics& g)
    {
        if(this->GetScrollbar(true)->IsVisible() && this->GetScrollbar(false)->IsVisible() && !this->mTransparent)
        {
            g.SetSysColor(StandardColor_Scrollbar);
            g.FillRect(this->GetScrollbar(true)->Left(),this->GetScrollbar(true)->Top()+this->GetScrollbar(true)->Height(),
                       this->GetScrollbar(true)->Width(),this->GetScrollbar(false)->Height());
        }
        if(!this->mDrawFrame)
            return;
        if(this->mDraw3DFrame)
            LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),true);
        else
        {
            g.SetSysColor(StandardColor_3DDarkShadow);
            g.DrawRect(0,0,this->Width(),this->Height());
        }
    }
};
struct Lbl { int Height() { return w.a; } int Left() { return w.b; } int Right() { return w.c; } };
struct MSEtchedFrameGroup : Comp {
    Lbl aLabel; Lbl* mLabel;
    MSEtchedFrameGroup() : mLabel(&aLabel) {}
    void Paint(Graphics& g) {
        int dy=this->mLabel->Height()/2;
        g.Translate(0,dy);
        int right=this->Width()-1;
        int bottom=this->Height()-1-dy;
        int labelLeft=this->mLabel->Left();
        int labelRight=this->mLabel->Right();
        g.SetSysColor(StandardColor_3DHilight);
        g.DrawLine(0,1,labelLeft,1);
        g.DrawLine(labelRight,1,right,1);
        g.DrawLine(right,0,right,bottom);
        g.DrawLine(0,bottom,right,bottom);
        g.DrawLine(1,1,1,bottom);
        g.SetSysColor(StandardColor_3DShadow);
        g.DrawLine(0,0,labelLeft,0);
        g.DrawLine(labelRight,0,right-1,0);
        g.DrawLine(right-1,0,right-1,bottom-1);
        g.DrawLine(0,bottom-1,right-1,bottom-1);
        g.DrawLine(0,0,0,bottom-1);
        g.Translate(0,-dy);
    }
};
struct WONMSSkin {
    enum MSImageFlag { MSImageFlag_Down=1,MSImageFlag_Default=2,MSImageFlag_Disabled=4,MSImageFlag_Checked=8 };
    enum MSImageType {
        MSImageType_Frame1,MSImageType_Frame2,MSImageType_Frame3,
        MSImageType_LeftArrow,MSImageType_RightArrow,MSImageType_UpArrow,MSImageType_DownArrow,
        MSImageType_PushButton,MSImageType_Checkbox,MSImageType_RadioButton,
        MSImageType_ScrollThumb,MSImageType_ScrollGutter,MSImageType_ScrollCorner,
        MSImageType_ListHeader,MSImageType_TabButton,MSImageType_TabCtrlFrame,
        MSImageType_ComboPopupFrame,MSImageType_CloseButton
    };
    struct MSImage {
        unsigned char mImageType,mImageFlags;
        int mWidth, mHeight;
        void Paint(Graphics&);
        void DrawPushButton(Graphics&,bool,bool=false);
        void DrawArrowButton(Graphics&,Direction);
        void DrawCloseButton(Graphics&);
        bool FlagSet(unsigned char flags) { return this->mImageFlags&flags?true:false; }
    };
};
void WONMSSkin::MSImage::DrawPushButton(Graphics &g, bool useFrame2, bool checkDefault)
{

	bool down = this->FlagSet(MSImageFlag_Down);

	int x = 0, y = 0, width = this->mWidth, height = this->mHeight;

	g.SetSysColor(StandardColor_3DFace);
	g.FillRect(0,0,width,height);

	if(checkDefault && (this->FlagSet(MSImageFlag_Default) || down))
	{
		g.SetSysColor(StandardColor_3DDarkShadow);
		g.DrawRect(0,0,width,height);
		x++;
		y++;
		width-=2;
		height-=2;

		if(down)
			useFrame2 = true;
	}

	if(useFrame2)
		LobbyDrawFrame(g,x,y,width,height,down);
	else
		LobbyDraw3DFrame(g,x,y,width,height,down);
}
void WONMSSkin::MSImage::DrawArrowButton(Graphics &g, Direction theDirection)
{
	this->DrawPushButton(g,true,false);
	if(this->FlagSet(MSImageFlag_Down))
		MSGraphicsUtil::DrawArrow(g,theDirection,1,1,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Disabled));
	else
		MSGraphicsUtil::DrawArrow(g,theDirection,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Disabled));
}
void WONMSSkin::MSImage::DrawCloseButton(Graphics &g)
{
	this->DrawPushButton(g,false,false);
	if(this->FlagSet(MSImageFlag_Down))
		MSGraphicsUtil::DrawCloseX(g,1,1,this->mWidth,this->mHeight);
	else
		MSGraphicsUtil::DrawCloseX(g,0,0,this->mWidth,this->mHeight);
}
void WONMSSkin::MSImage::Paint(Graphics &g)
{
	switch(this->mImageType)
	{
		case MSImageType_Frame1:		LobbyDraw3DFrame(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;
		case MSImageType_Frame2:		LobbyDrawFrame(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;
		case MSImageType_Frame3:		MSGraphicsUtil::DrawFrame3(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;

		case MSImageType_LeftArrow:		this->DrawArrowButton(g,Direction_Left);	return;
		case MSImageType_UpArrow:		this->DrawArrowButton(g,Direction_Up);	return;
		case MSImageType_RightArrow:	this->DrawArrowButton(g,Direction_Right);	return;
		case MSImageType_DownArrow:		this->DrawArrowButton(g,Direction_Down);	return;

		case MSImageType_PushButton:	this->DrawPushButton(g,false,true);	return;
		case MSImageType_TabCtrlFrame:	this->DrawPushButton(g,false,false); return;
		case MSImageType_ListHeader:	this->DrawPushButton(g,true,false);   return;
		case MSImageType_ScrollThumb:	this->DrawPushButton(g,true,false); return;
		case MSImageType_CloseButton:	this->DrawCloseButton(g); return;

		case MSImageType_TabButton:		MSGraphicsUtil::DrawTab(g,0,0,this->mWidth,this->mHeight); return;
		case MSImageType_Checkbox:		MSGraphicsUtil::DrawCheck(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down),this->FlagSet(MSImageFlag_Checked)); return;
		case MSImageType_RadioButton:	MSGraphicsUtil::DrawRadio(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down),this->FlagSet(MSImageFlag_Checked)); return;

		case MSImageType_ScrollGutter:	g.SetSysColor(StandardColor_Scrollbar); g.FillRect(0,0,this->mWidth,this->mHeight); return;
		case MSImageType_ScrollCorner:  g.SetSysColor(StandardColor_3DFace); g.FillRect(0,0,this->mWidth,this->mHeight); return;

		case MSImageType_ComboPopupFrame: g.SetSysColor(StandardColor_3DDarkShadow); g.DrawRect(0,0,this->mWidth,this->mHeight); return;
	}

}

// one paint of the kind, over the world
static void Paint(int kind)
{
    Graphics g;
    switch ((EeWonMsPaintKind)kind) {
    case EeWonMsPaintKind::Frame3D: LobbyDraw3DFrame(g, w.a, w.b, w.width, w.height, w.pressed); break;
    case EeWonMsPaintKind::Frame: LobbyDrawFrame(g, w.a, w.b, w.width, w.height, w.pressed); break;
    case EeWonMsPaintKind::Frame3: MSGraphicsUtil::DrawFrame3(g, w.a, w.b, w.width, w.height, w.pressed); break;
    case EeWonMsPaintKind::Arrow: MSGraphicsUtil::DrawArrow(g, (Direction)w.a, w.b, w.c, w.width, w.height, w.disabled); break;
    case EeWonMsPaintKind::Check: MSGraphicsUtil::DrawCheck(g, w.a, w.b, w.width, w.height, w.pressed, w.checked); break;
    case EeWonMsPaintKind::Radio: MSGraphicsUtil::DrawRadio(g, w.a, w.b, w.width, w.height, w.pressed, w.checked); break;
    case EeWonMsPaintKind::Tab: MSGraphicsUtil::DrawTab(g, w.a, w.b, w.width, w.height); break;
    case EeWonMsPaintKind::CloseX: MSGraphicsUtil::DrawCloseX(g, w.a, w.b, w.width, w.height); break;
    case EeWonMsPaintKind::Button: { MSButton c; c.mUseFrame2 = w.frame2; c.Paint(g); break; }
    case EeWonMsPaintKind::TabButton: { MSTabButton c; c.Paint(g); break; }
    case EeWonMsPaintKind::Label: { MSLabel c; c.Paint(g); break; }
    case EeWonMsPaintKind::HeaderSort: { MSHeaderButton c; c.Paint(g); break; }
    case EeWonMsPaintKind::CloseButton: { MSCloseButton c; c.Paint(g); break; }
    case EeWonMsPaintKind::ScrollThumb: { MSScrollThumb c; c.Paint(g); break; }
    case EeWonMsPaintKind::ArrowButton: { MSArrowButton c; c.mDirection = (Direction)w.a; c.Paint(g); break; }
    case EeWonMsPaintKind::Frame3DComponent: { MS3DFrame c; c.mType = w.a; c.mIsUp = !w.pressed; c.Paint(g); break; }
    case EeWonMsPaintKind::Popup: { MSPopup c; c.Paint(g); break; }
    case EeWonMsPaintKind::DlgBack: { MSDlgBack c; c.Paint(g); break; }
    case EeWonMsPaintKind::EtchedFrame: { MSEtchedFrame c; c.Paint(g); break; }
    case EeWonMsPaintKind::InputBox: { MSInputBox c; c.Paint(g); break; }
    case EeWonMsPaintKind::Scrollbar: { MSScrollbar c; c.mIsTransparent = w.transparent; c.Paint(g); break; }
    case EeWonMsPaintKind::ComboBox: { MSComboBox c; c.mDrawFrame = w.drawFrame; c.Paint(g); break; }
    case EeWonMsPaintKind::ScrollContainer: {
        MSScrollContainer c; c.mTransparent = w.transparent; c.mDrawFrame = w.drawFrame; c.mDraw3DFrame = w.draw3D;
        c.Paint(g); break;
    }
    case EeWonMsPaintKind::EtchedGroup: { MSEtchedFrameGroup c; c.Paint(g); break; }
    case EeWonMsPaintKind::SkinImage: {
        WONMSSkin::MSImage c; c.mImageType = (unsigned char)w.a; c.mImageFlags = (unsigned char)w.b;
        c.mWidth = w.width; c.mHeight = w.height; c.Paint(g); break;
    }
    }
}
} // namespace orig

// ---- the port: the plan, executed as WONGUIDraw.h's WONDrawOne does
static void Run(Graphics& g, const EeWonDrawOp& o)
{
    switch (o.kind) {
    case EE_WON_DRAW_SET_SYS_COLOR: g.SetSysColor(o.a); break;
    case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_LINE: g.DrawLine(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_TRANSLATE: g.Translate(o.a, o.b); break;
    case EE_WON_DRAW_PIXEL: g.SetPixel(o.a, o.b, (DWORD)(unsigned int)o.c); break;
    case EE_WON_DRAW_SYS_PIXEL: g.SetPixel(o.a, o.b, g.GetStandardColor(o.c)); break;
    case EE_WON_DRAW_FILL_ELLIPSE: g.FillEllipse(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_ARC: g.DrawArc(o.a, o.b, o.c, o.d, o.e, o.f); break;
    case EE_WON_DRAW_CALL:
        if (o.a == EE_WON_MSPAINT_CALL_TEXT) T("text %d %d", o.b, o.c);
        else T("call %d", o.a);
        break;
    default: T("op %d", o.kind); break;
    }
}

static EeWonMsPaint Inputs(int kind)
{
    EeWonMsPaint p;
    memset(&p, 0, sizeof(p));
    p.kind = (EeWonMsPaintKind)kind;
    p.flags = (w.pressed ? EE_WON_MSPAINT_PRESSED : 0) | (w.disabled ? EE_WON_MSPAINT_DISABLED : 0) |
              (w.transparent ? EE_WON_MSPAINT_TRANSPARENT : 0) | (w.isDefault ? EE_WON_MSPAINT_DEFAULT : 0) |
              (w.frame2 ? EE_WON_MSPAINT_FRAME2 : 0) | (w.checked ? EE_WON_MSPAINT_CHECKED : 0) |
              (w.drawFrame ? EE_WON_MSPAINT_DRAW_FRAME : 0) | (w.draw3D ? EE_WON_MSPAINT_DRAW_3D : 0) |
              (w.vert ? EE_WON_MSPAINT_VERT : 0) | (w.horz ? EE_WON_MSPAINT_HORZ : 0) |
              (w.invalid ? EE_WON_MSPAINT_INVALID : 0);
    p.width = w.width; p.height = w.height;
    p.a = w.a; p.b = w.b; p.c = w.c; p.d = w.d; p.e = w.e;
    // the image's type and flags are bytes, as the C++ host passes them
    if (kind == (int)EeWonMsPaintKind::SkinImage) { p.a = (unsigned char)w.a; p.b = (unsigned char)w.b; }
    return p;
}

static void PortPaint(int kind)
{
    EeWonMsPaint p = Inputs(kind);
    EeWonDrawOp aOps[8];
    size_t n = ee_won_mspaint_plan(&p, aOps, 8);
    std::vector<EeWonDrawOp> aMore(n ? n : 1);
    n = ee_won_mspaint_plan(&p, &aMore[0], aMore.size());
    if (n > 8) for (int i = 0; i < 8; i++) if (memcmp(&aOps[i], &aMore[i], sizeof(EeWonDrawOp))) T("cap differs %d", i);
    Graphics g;
    for (size_t i = 0; i < n; i++) Run(g, aMore[i]);
}

static int Size()
{
    if (R(40) == 0) return (int)(R(2) ? 0x7ffffff0 + R(16) : -(int)R(100000));
    return (int)R(64) - 3;
}
static void Draw(int kind)
{
    w.width = Size(); w.height = Size();
    w.pressed = R(2); w.disabled = R(2); w.transparent = R(2); w.isDefault = R(2); w.frame2 = R(2);
    w.checked = R(2); w.drawFrame = R(2); w.draw3D = R(2); w.vert = R(2); w.horz = R(2); w.invalid = R(2);
    w.a = Size(); w.b = Size(); w.c = Size(); w.d = Size(); w.e = Size();
    switch ((EeWonMsPaintKind)kind) {
    case EeWonMsPaintKind::Arrow: case EeWonMsPaintKind::ArrowButton: w.a = (int)R(6) - 1; break;
    case EeWonMsPaintKind::HeaderSort: w.a = (int)R(4); if (R(3) == 0) w.b = 0; break;
    case EeWonMsPaintKind::Frame3DComponent: w.a = (int)R(6) - 1; break;
    case EeWonMsPaintKind::SkinImage: w.a = R(10) == 0 ? (int)R(256) : (int)R(19); w.b = (int)R(16); break;
    default: break;
    }
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kPaints = 300000;
    std::set<std::string> aMet;
    int aPrinted = 0;
    long long aLines = 0;
    sSeed = 36;
    for (int i = 0; i < kPaints; i++) {
        int kind = 1 + (int)R(25);
        Draw(kind);
        sTrace.clear();
        orig::Paint(kind);
        std::vector<std::string> aOrig = sTrace;
        sTrace.clear();
        PortPaint(kind);
        aLines += (long long)aOrig.size();
        if (aOrig != sTrace) {
            if (++sFailures < 10) {
                printf("FAIL paint %d kind %d size %d %d a %d b %d\n", i, kind, w.width, w.height, w.a, w.b);
                for (size_t j = 0; j < aOrig.size() || j < sTrace.size(); j++)
                    printf("  %-34s | %s\n", j < aOrig.size() ? aOrig[j].c_str() : "",
                           j < sTrace.size() ? sTrace[j].c_str() : "");
            }
        }
        if (aPrint) {
            bool aNew = i < 400;
            for (size_t j = 0; j < aOrig.size(); j++) {
                char aKey[64];
                snprintf(aKey, sizeof(aKey), "%d %s", kind, aOrig[j].substr(0, aOrig[j].find(' ')).c_str());
                if (aMet.insert(aKey).second) aNew = true;
            }
            if (aNew) {
                EeWonMsPaint p = Inputs(kind);
                printf("P %d %u %d %d %d %d %d %d %d\n", (int)p.kind, p.flags, p.width, p.height, p.a, p.b, p.c, p.d, p.e);
                for (size_t j = 0; j < aOrig.size(); j++) printf("%s\n", aOrig[j].c_str());
                aPrinted++;
            }
        }
    }
    if (aPrint) return 0;
    printf("won_mspaint_test: %d paints, %lld trace lines, %d failures\n", kPaints, aLines, sFailures);
    return sFailures ? 1 : 0;
}
