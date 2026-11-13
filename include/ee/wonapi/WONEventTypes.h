// WONGUI's event types (WindowEventType), apart so the goldens (tests/won) compare the events the Rust component
// objects fire (rust/crates/ee_won/src/guihost.rs ee_won_gui_event_type) with the original's enum.
#ifndef EE_WONAPI_WONEVENTTYPES_H
#define EE_WONAPI_WONEVENTTYPES_H
namespace WONAPI {
// SDK event and flag constants; the input event ids 1..14 and the popup bit tests in the exe agree.
enum WindowEventType
	{
		WindowEventType_None,

		InputEvent_SizeChange,
		InputEvent_MouseMove,
		InputEvent_MouseDown,
		InputEvent_MouseDoubleClick,
		InputEvent_MouseDownCheckDoubleClick, 
		InputEvent_MouseUp,
		InputEvent_MouseWheel,
		InputEvent_KeyDown,
		InputEvent_KeyUp,
		InputEvent_KeyChar,
		InputEvent_GotFocus,
		InputEvent_LostFocus,
		InputEvent_MouseExit,
		InputEvent_WindowClose,

		ComponentEvent_ButtonPressed,
		ComponentEvent_Scroll,
		ComponentEvent_TextLinkActivated,
		ComponentEvent_HyperLinkActivated,
		ComponentEvent_HyperLinkOver,
		ComponentEvent_InputReturn,
		ComponentEvent_InputTextChanged,

		ComponentEvent_ListSelChanged,
		ComponentEvent_ListItemClicked,
		ComponentEvent_ListItemUpClicked,
		ComponentEvent_MenuItemSelected,
		ComponentEvent_TreeItemOpened,
		ComponentEvent_ComboSelChanged,
		ComponentEvent_TabBarTabSelected,
		ComponentEvent_TabCtrlTabSelected,
		ComponentEvent_EndPopup,
		ComponentEvent_BrowserStatusChange,
		ComponentEvent_BrowserLocationChange,
		ComponentEvent_BrowserButtonChange,
		ComponentEvent_SpinnerValueChanged,

		ComponentEvent_ContextStart	= 10000, 
		ComponentEvent_ContextEnd = 20000,

		Event_User_Start = 100000
	};
}
#endif
