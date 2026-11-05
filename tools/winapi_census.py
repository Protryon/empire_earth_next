"""Census of the Windows-specific dependencies of the first-party source (src/ and include/, third-party excluded).

  python -I tools/winapi_census.py [--by-file N] [--family F] [--tsv PATH] [--md]

Every identifier of every first-party .cpp/.c/.h (comments and string literals stripped) is matched against a table of
Windows/MSVC names grouped by family (files, threads/sync, time, registry, windowing, GDI, Winsock, COM, DirectX,
MSVC string functions, MSVC language extensions, Windows typedefs, ...), plus the Windows-only #include lines.
Output: counts by family (sites, distinct names, files), by area (engine subsystem / game folder) and the top files
per family; every site goes to the TSV (default build/winapi_census.tsv: file, line, family, name, area).

--family F lists one family's names by count with their top files. --md prints the family table as Markdown (the
PORT.md cross-platform section is written from it). Areas marked [out] are owned by other work (the DX7 renderer,
the Input System, the GE screen/surface/window classes) and are catalogued only.

Heuristics: a name after `.`, `->` or `Class::` is a first-party member, not a Win32 use. Names that first-party
classes also use as members (TSScheduler::CreateEvent, UIControl::DrawTextA, ...; listed at the end) count only
where written `::Name`: an unqualified use of one of them is taken to be the member. Members spelled as the A/W
expansion of a Win32 macro (`DrawTextA`: windows.h renames a method `DrawText`) are their own family,
win-macro-names: they keep their spelling only while windows.h is included everywhere.
"""
import argparse
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

THIRD_PARTY = ("src/ThirdParty/", "include/winsdk/", "include/jpeglib/", "include/miniaudio/", "include/pl_mpeg/")
EXTS = {".cpp", ".c", ".h"}

# (family, exact names, name regex). A name matches the first family that lists it (order matters: specific first).
FAMILIES = [
    ("directx", set(), r"(?:I?Direct(?:Draw|3D|Input|Sound|Music|Show)\w*|LPDIRECT\w+|DDSURFACEDESC\w*|DDSCAPS\w*|"
                       r"DDPIXELFORMAT|DDBLTFX|DDCOLORKEY|DDCAPS\w*|DDSD_\w+|DDPF_\w+|DDBLT_\w+|DDLOCK_\w+|DDSCL_\w+|"
                       r"DDERR_\w+|DDENUM\w*|DD_OK|D3D[A-Z]\w*|LPD3D\w*|DI[A-Z]+_\w+|DIDEVICE\w+|DIPROP\w*|LPDI\w+|"
                       r"c_dfDI\w+|IID_IDirect\w+|DS_OK|DSBUFFERDESC|DSERR_\w+|DSBCAPS_\w+)"),
    ("winsock", {"socket", "WSAStartup", "WSACleanup", "WSAGetLastError", "WSASetLastError", "bind", "listen",
                 "accept", "connect", "send", "recv", "sendto", "recvfrom", "select", "closesocket", "ioctlsocket",
                 "gethostbyname", "gethostbyaddr", "gethostname", "inet_addr", "inet_ntoa", "htons", "htonl", "ntohs",
                 "ntohl", "setsockopt", "getsockopt", "getsockname", "getpeername", "shutdown", "SOCKET",
                 "SOCKADDR", "SOCKADDR_IN", "sockaddr", "sockaddr_in", "in_addr", "hostent", "HOSTENT", "fd_set",
                 "FD_SET", "FD_ZERO", "FD_ISSET", "FD_CLR", "timeval", "INVALID_SOCKET", "SOCKET_ERROR", "WSADATA",
                 "GetAdaptersInfo", "GetIpAddrTable", "IP_ADAPTER_INFO", "MIB_IPADDRTABLE", "MIB_IPADDRROW",
                 "INADDR_ANY", "INADDR_NONE", "INADDR_BROADCAST", "SOCK_STREAM", "SOCK_DGRAM", "AF_INET", "PF_INET",
                 "IPPROTO_TCP", "IPPROTO_UDP", "SOL_SOCKET", "SO_REUSEADDR", "SO_BROADCAST", "SO_RCVBUF",
                 "SO_SNDBUF", "SO_LINGER", "TCP_NODELAY", "FIONBIO", "FIONREAD", "linger", "MAXHOSTNAMELEN"},
     r"(?:WSA[A-Z]\w*)"),
    ("com", {"CoInitialize", "CoInitializeEx", "CoUninitialize", "CoCreateInstance", "CoTaskMemFree", "IUnknown",
             "HRESULT", "SUCCEEDED", "FAILED", "S_OK", "S_FALSE", "E_FAIL", "E_NOTIMPL", "E_OUTOFMEMORY",
             "E_INVALIDARG", "E_NOINTERFACE", "E_POINTER", "GUID", "REFGUID", "REFIID", "REFCLSID", "IID", "CLSID",
             "DEFINE_GUID", "BSTR", "VARIANT", "SysAllocString", "SysFreeString", "LPUNKNOWN", "STDMETHOD",
             "STDMETHODIMP", "CLSCTX_INPROC_SERVER", "CLSCTX_ALL", "COINIT_MULTITHREADED",
             "COINIT_APARTMENTTHREADED", "IsEqualGUID", "IsEqualIID"}, r"(?:CLSID_\w+|IID_\w+)"),
    ("registry", {"HKEY", "PHKEY", "REGSAM", "HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER", "HKEY_CLASSES_ROOT",
                  "HKEY_USERS", "HKEY_CURRENT_CONFIG"}, r"(?:Reg[A-Z][a-zA-Z]+(?:Ex)?[AW]?|REG_[A-Z_]+|KEY_[A-Z_]+)"),
    ("time", {"timeGetTime", "timeBeginPeriod", "timeEndPeriod", "timeSetEvent", "timeKillEvent", "timeGetDevCaps",
              "TIMECAPS", "TIME_PERIODIC", "TIME_ONESHOT", "TIME_CALLBACK_FUNCTION", "LPTIMECALLBACK",
              "QueryPerformanceCounter", "QueryPerformanceFrequency", "GetTickCount", "GetTickCount64", "Sleep",
              "SleepEx", "GetLocalTime", "GetSystemTime", "GetSystemTimeAsFileTime", "FileTimeToSystemTime",
              "SystemTimeToFileTime", "FileTimeToLocalFileTime", "LocalFileTimeToFileTime", "CompareFileTime",
              "SYSTEMTIME", "FILETIME", "LPSYSTEMTIME", "LPFILETIME", "LARGE_INTEGER", "ULARGE_INTEGER",
              "SetTimer", "KillTimer", "GetDateFormat", "GetDateFormatA", "GetTimeFormat", "GetTimeFormatA",
              "TIMERR_NOERROR", "MMRESULT"}, None),
    ("threads", {"CreateThread", "_beginthread", "_beginthreadex", "_endthread", "_endthreadex", "ExitThread",
                 "TerminateThread", "SuspendThread", "ResumeThread", "SetThreadPriority", "GetThreadPriority",
                 "GetCurrentThread", "GetCurrentThreadId", "GetExitCodeThread", "SetThreadAffinityMask",
                 "SetProcessAffinityMask", "GetProcessAffinityMask", "SwitchToThread", "WaitForSingleObject",
                 "WaitForMultipleObjects", "MsgWaitForMultipleObjects", "WaitForSingleObjectEx", "CreateEvent",
                 "CreateEventA", "CreateEventW", "OpenEvent", "SetEvent", "ResetEvent", "PulseEvent", "CreateMutex",
                 "CreateMutexA", "CreateMutexW", "OpenMutex", "OpenMutexA", "ReleaseMutex", "CreateSemaphore",
                 "CreateSemaphoreA", "ReleaseSemaphore", "CRITICAL_SECTION", "LPCRITICAL_SECTION",
                 "InitializeCriticalSection", "InitializeCriticalSectionAndSpinCount", "EnterCriticalSection",
                 "LeaveCriticalSection", "DeleteCriticalSection", "TryEnterCriticalSection", "TlsAlloc", "TlsFree",
                 "TlsGetValue", "TlsSetValue", "INFINITE", "WAIT_OBJECT_0", "WAIT_TIMEOUT", "WAIT_FAILED",
                 "WAIT_ABANDONED", "THREAD_PRIORITY_NORMAL", "THREAD_PRIORITY_HIGHEST", "THREAD_PRIORITY_LOWEST",
                 "THREAD_PRIORITY_ABOVE_NORMAL", "THREAD_PRIORITY_BELOW_NORMAL", "THREAD_PRIORITY_IDLE",
                 "THREAD_PRIORITY_TIME_CRITICAL", "LPTHREAD_START_ROUTINE", "CREATE_SUSPENDED", "STILL_ACTIVE",
                 "DuplicateHandle", "SetPriorityClass", "GetPriorityClass"}, r"(?:Interlocked\w+)"),
    ("files", {"CreateFile", "CreateFileA", "CreateFileW", "ReadFile", "WriteFile", "FindFirstFile", "FindFirstFileA",
               "FindNextFile", "FindNextFileA", "FindClose", "GetModuleFileName", "GetModuleFileNameA",
               "GetFileSize", "GetFileSizeEx", "SetFilePointer", "SetEndOfFile", "FlushFileBuffers", "DeleteFile",
               "DeleteFileA", "CopyFile", "CopyFileA", "MoveFile", "MoveFileA", "MoveFileEx", "CreateDirectory",
               "CreateDirectoryA", "RemoveDirectory", "RemoveDirectoryA", "GetCurrentDirectory",
               "GetCurrentDirectoryA", "SetCurrentDirectory", "SetCurrentDirectoryA", "GetFileAttributes",
               "GetFileAttributesA", "GetFileAttributesEx", "SetFileAttributes", "SetFileAttributesA", "GetTempPath",
               "GetTempPathA", "GetTempFileName", "GetTempFileNameA", "GetFullPathName", "GetFullPathNameA",
               "GetDiskFreeSpace", "GetDiskFreeSpaceA", "GetDiskFreeSpaceEx", "GetDiskFreeSpaceExA", "GetDriveType",
               "GetDriveTypeA", "GetLogicalDrives", "GetLogicalDriveStrings", "GetLogicalDriveStringsA",
               "GetVolumeInformation", "GetVolumeInformationA", "GetFileTime", "SetFileTime",
               "GetWindowsDirectory", "GetWindowsDirectoryA", "GetSystemDirectory", "GetSystemDirectoryA",
               "SHGetFolderPath", "SHGetFolderPathA", "SHGetSpecialFolderPath", "SHGetSpecialFolderPathA",
               "CreateFileMapping", "MapViewOfFile", "UnmapViewOfFile", "WIN32_FIND_DATA", "WIN32_FIND_DATAA",
               "LPWIN32_FIND_DATA", "INVALID_HANDLE_VALUE", "INVALID_FILE_SIZE", "INVALID_FILE_ATTRIBUTES",
               "FILE_ATTRIBUTE_DIRECTORY", "FILE_ATTRIBUTE_NORMAL", "FILE_ATTRIBUTE_READONLY",
               "FILE_ATTRIBUTE_HIDDEN", "FILE_ATTRIBUTE_SYSTEM", "FILE_ATTRIBUTE_ARCHIVE", "GENERIC_READ",
               "GENERIC_WRITE", "FILE_SHARE_READ", "FILE_SHARE_WRITE", "OPEN_EXISTING", "OPEN_ALWAYS",
               "CREATE_ALWAYS", "CREATE_NEW", "TRUNCATE_EXISTING", "FILE_BEGIN", "FILE_CURRENT", "FILE_END",
               "FILE_FLAG_SEQUENTIAL_SCAN", "FILE_FLAG_RANDOM_ACCESS", "MAX_PATH", "_MAX_PATH", "_MAX_DRIVE",
               "_MAX_DIR", "_MAX_FNAME", "_MAX_EXT", "_findfirst", "_findnext", "_findclose", "_finddata_t",
               "_access", "_mkdir", "_rmdir", "_getcwd", "_chdir", "_getdrive", "_chdrive", "_splitpath",
               "_makepath", "_fullpath", "_unlink", "_stat", "_fstat", "_open", "_close", "_read", "_write",
               "_lseek", "_filelength", "_fileno", "_O_RDONLY", "_O_BINARY", "_S_IREAD", "_S_IWRITE",
               "DRIVE_CDROM", "DRIVE_FIXED", "DRIVE_REMOVABLE", "DRIVE_REMOTE", "DRIVE_RAMDISK",
               "SearchPath", "SearchPathA", "GetShortPathName", "GetLongPathName", "PathFileExists"}, None),
    ("messagebox", {"MessageBox", "MessageBoxA", "MessageBoxW", "MessageBoxEx", "MessageBeep"}, r"(?:MB_[A-Z]+)"),
    ("gdi", {"CreateFont", "CreateFontA", "CreateFontIndirect", "CreateFontIndirectA", "SelectObject",
             "DeleteObject", "GetDC", "ReleaseDC", "CreateCompatibleDC", "CreateDC", "CreateDCA", "DeleteDC",
             "CreateDIBSection", "CreateCompatibleBitmap", "CreateBitmap", "BitBlt", "StretchBlt", "StretchDIBits",
             "SetDIBitsToDevice", "GetDIBits", "SetDIBits", "TextOut", "TextOutA", "TextOutW", "ExtTextOut",
             "ExtTextOutA", "ExtTextOutW", "DrawText", "DrawTextA", "DrawTextW", "GetTextExtentPoint32",
             "GetTextExtentPoint32A", "GetTextExtentPoint32W", "GetTextExtentPoint", "GetTextExtentPointA",
             "GetTextMetrics", "GetTextMetricsA", "GetTextMetricsW", "SetTextColor", "SetBkColor", "SetBkMode",
             "SetTextAlign", "CreatePen", "CreateSolidBrush", "FillRect", "FrameRect", "Rectangle", "Ellipse",
             "LineTo", "MoveToEx", "Polygon", "Polyline", "GetPixel", "SetPixel", "HDC", "HFONT", "HBITMAP",
             "HBRUSH", "HPEN", "HGDIOBJ", "HPALETTE", "HRGN", "RGB", "GetRValue", "GetGValue", "GetBValue",
             "COLORREF", "BITMAPINFO", "BITMAPINFOHEADER", "BITMAPFILEHEADER", "RGBQUAD", "LOGFONT", "LOGFONTA",
             "LOGFONTW", "TEXTMETRIC", "TEXTMETRICA", "TEXTMETRICW", "ABC", "GetCharABCWidths",
             "GetCharABCWidthsA", "GetCharABCWidthsW", "GetCharWidth", "GetCharWidth32", "GetCharWidthA",
             "GetCharWidth32A", "GetCharWidth32W", "AddFontResource", "AddFontResourceA", "RemoveFontResource",
             "RemoveFontResourceA", "GetDeviceCaps", "GetObject", "GetObjectA", "GetStockObject", "PAINTSTRUCT",
             "BeginPaint", "EndPaint", "InvalidateRect", "ValidateRect", "UpdateWindow", "GdiFlush",
             "SetStretchBltMode", "BI_RGB", "BI_BITFIELDS", "DIB_RGB_COLORS", "SRCCOPY", "TRANSPARENT", "OPAQUE",
             "FW_NORMAL", "FW_BOLD", "ANSI_CHARSET", "DEFAULT_CHARSET", "OUT_DEFAULT_PRECIS",
             "CLIP_DEFAULT_PRECIS", "ANTIALIASED_QUALITY", "NONANTIALIASED_QUALITY", "DEFAULT_QUALITY",
             "DEFAULT_PITCH", "FF_DONTCARE", "VARIABLE_PITCH", "FF_SWISS", "TA_LEFT", "TA_TOP", "TA_BASELINE",
             "DT_LEFT", "DT_CENTER", "DT_RIGHT", "DT_TOP", "DT_VCENTER", "DT_SINGLELINE", "DT_CALCRECT",
             "DT_WORDBREAK", "DT_NOPREFIX", "DT_NOCLIP", "BLACK_BRUSH", "WHITE_BRUSH", "NULL_BRUSH", "PS_SOLID",
             "LOGPIXELSX", "LOGPIXELSY", "BITSPIXEL", "HORZRES", "VERTRES", "PALETTEENTRY",
             "GetSystemPaletteEntries", "CreatePalette", "SelectPalette", "RealizePalette",
             "GetFontUnicodeRanges", "GetGlyphOutline", "GetGlyphOutlineW", "GLYPHMETRICS", "MAT2",
             "GGO_GRAY8_BITMAP", "GGO_BITMAP", "GGO_METRICS"}, None),
    ("ime", set(), r"(?:Imm[A-Z]\w+|HIMC|COMPOSITIONFORM|CANDIDATEFORM|CANDIDATELIST|GCS_\w+|IME_\w+|IMN_\w+|"
                   r"IMC_\w+|CFS_\w+|ISC_\w+|IACE_\w+|CPS_\w+|NI_\w+|LANG_[A-Z]+|SUBLANG_\w+|PRIMARYLANGID|"
                   r"SUBLANGID|MAKELANGID|GetKeyboardLayout\w*|HKL|LANGID|GetUserDefaultLangID|"
                   r"GetSystemDefaultLangID|GetUserDefaultLCID|LCID)"),
    ("windowing", {"CreateWindow", "CreateWindowA", "CreateWindowEx", "CreateWindowExA", "CreateWindowExW",
                   "RegisterClass", "RegisterClassA", "RegisterClassEx", "RegisterClassExA", "UnregisterClass",
                   "UnregisterClassA", "DefWindowProc", "DefWindowProcA", "DefWindowProcW", "CallWindowProc",
                   "CallWindowProcA", "PeekMessage", "PeekMessageA", "GetMessage", "GetMessageA", "DispatchMessage",
                   "DispatchMessageA", "TranslateMessage", "PostMessage", "PostMessageA", "SendMessage",
                   "SendMessageA", "PostThreadMessage", "PostThreadMessageA", "PostQuitMessage", "ShowWindow",
                   "SetWindowPos", "MoveWindow", "GetWindowRect", "GetClientRect", "AdjustWindowRect",
                   "AdjustWindowRectEx", "ClientToScreen", "ScreenToClient", "SetCursor", "ShowCursor", "LoadCursor",
                   "LoadCursorA", "LoadIcon", "LoadIconA", "LoadImage", "LoadImageA", "SetCursorPos",
                   "GetCursorPos", "ClipCursor", "SetCapture", "ReleaseCapture", "GetCapture", "GetAsyncKeyState",
                   "GetKeyState", "GetKeyboardState", "MapVirtualKey", "MapVirtualKeyA", "MapVirtualKeyEx",
                   "ToAscii", "ToAsciiEx", "ToUnicode", "VkKeyScan", "SetFocus", "GetFocus", "SetForegroundWindow",
                   "GetForegroundWindow", "SetActiveWindow", "GetActiveWindow", "FindWindow", "FindWindowA",
                   "DestroyWindow", "IsWindow", "IsIconic", "IsZoomed", "IsWindowVisible", "EnableWindow",
                   "GetSystemMetrics", "SetWindowLong", "SetWindowLongA", "GetWindowLong", "GetWindowLongA",
                   "SetWindowLongPtr", "SetWindowLongPtrA", "GetWindowLongPtr", "GetWindowLongPtrA",
                   "EnumDisplaySettings", "EnumDisplaySettingsA", "ChangeDisplaySettings", "ChangeDisplaySettingsA",
                   "OpenClipboard", "CloseClipboard", "GetClipboardData", "SetClipboardData", "EmptyClipboard",
                   "IsClipboardFormatAvailable", "CF_TEXT", "CF_UNICODETEXT", "DialogBox", "DialogBoxParam",
                   "DialogBoxParamA", "CreateDialog", "CreateDialogParam", "CreateDialogParamA", "EndDialog",
                   "GetDlgItem", "SetDlgItemText", "SetDlgItemTextA", "GetDlgItemText", "GetDlgItemTextA",
                   "SetWindowText", "SetWindowTextA", "GetWindowText", "GetWindowTextA", "LoadString",
                   "LoadStringA", "LoadStringW", "HWND", "HMENU", "HICON", "HCURSOR", "WPARAM", "LPARAM",
                   "LRESULT", "MSG", "LPMSG", "WNDCLASS", "WNDCLASSA", "WNDCLASSEX", "WNDCLASSEXA", "WNDPROC",
                   "DLGPROC", "DEVMODE", "DEVMODEA", "MINMAXINFO", "CREATESTRUCT", "WINDOWPLACEMENT",
                   "GetWindowPlacement", "SetWindowPlacement", "MAKELPARAM", "MAKEWPARAM", "LOWORD", "HIWORD",
                   "LOBYTE", "HIBYTE", "MAKELONG", "MAKEWORD", "GET_X_LPARAM", "GET_Y_LPARAM", "SetWindowsHookEx",
                   "SetWindowsHookExA", "UnhookWindowsHookEx", "CallNextHookEx", "HHOOK", "KBDLLHOOKSTRUCT",
                   "SystemParametersInfo", "SystemParametersInfoA", "RegisterWindowMessage",
                   "RegisterWindowMessageA", "GetParent", "SetParent", "GetWindow", "EnumWindows", "BringWindowToTop",
                   "FlashWindow", "FlashWindowEx", "FLASHWINFO", "SetMenu", "WaitMessage", "GetQueueStatus",
                   "MsgWaitForMultipleObjectsEx", "CW_USEDEFAULT", "PM_REMOVE", "PM_NOREMOVE", "SW_SHOW", "SW_HIDE",
                   "SW_SHOWNORMAL", "SW_MINIMIZE", "SW_RESTORE", "SW_SHOWMINIMIZED", "SW_SHOWMAXIMIZED",
                   "SWP_NOSIZE", "SWP_NOMOVE", "SWP_NOZORDER", "SWP_SHOWWINDOW", "SWP_NOACTIVATE", "HWND_TOP",
                   "HWND_TOPMOST", "HWND_NOTOPMOST", "IDC_ARROW", "IDI_APPLICATION", "GWL_STYLE", "GWL_EXSTYLE",
                   "GWL_WNDPROC", "GWLP_WNDPROC", "GWL_USERDATA", "GWLP_USERDATA", "SM_CXSCREEN", "SM_CYSCREEN",
                   "IDOK", "IDCANCEL", "IDYES", "IDNO", "SetWindowRgn", "GetUpdateRect", "RedrawWindow",
                   "WindowFromPoint", "TrackMouseEvent", "TRACKMOUSEEVENT", "GetDoubleClickTime",
                   "GetKeyNameText", "GetKeyNameTextA", "keybd_event", "mouse_event", "SendInput", "INPUT",
                   "RegisterHotKey", "UnregisterHotKey", "GetKeyboardLayoutName"},
     r"(?:WM_\w+|VK_\w+|WS_\w+|CS_\w+|MK_\w+|SC_[A-Z]+|SIZE_[A-Z]+|WA_[A-Z]+|CDS_\w+|DISP_CHANGE_\w+|DM_[A-Z]+|"
     r"ENUM_\w+_SETTINGS|HTCLIENT|HT[A-Z]+|EM_[A-Z]+|ES_[A-Z]+|BN_[A-Z]+|BS_[A-Z]+|LB_[A-Z]+|LBS_\w+|"
     r"CB_[A-Z]+|CBS_\w+|EN_[A-Z]+|WH_[A-Z_]+|SPI_\w+)"),
    ("process", {"LoadLibrary", "LoadLibraryA", "LoadLibraryW", "LoadLibraryEx", "LoadLibraryExA",
                 "GetProcAddress", "FreeLibrary", "GetModuleHandle", "GetModuleHandleA", "HMODULE", "HINSTANCE",
                 "FARPROC", "GetLastError", "SetLastError", "FormatMessage", "FormatMessageA", "OutputDebugString",
                 "OutputDebugStringA", "GetVersionEx", "GetVersionExA", "GetVersion", "OSVERSIONINFO",
                 "OSVERSIONINFOA", "OSVERSIONINFOEX", "GetSystemInfo", "SYSTEM_INFO", "GlobalMemoryStatus",
                 "GlobalMemoryStatusEx", "MEMORYSTATUS", "MEMORYSTATUSEX", "VirtualAlloc", "VirtualFree",
                 "VirtualProtect", "VirtualQuery", "MEMORY_BASIC_INFORMATION", "HeapAlloc", "HeapFree",
                 "HeapReAlloc", "HeapCreate", "HeapDestroy", "GetProcessHeap", "HeapSize", "GlobalAlloc",
                 "GlobalFree", "GlobalLock", "GlobalUnlock", "GlobalSize", "LocalAlloc", "LocalFree", "HGLOBAL",
                 "HLOCAL", "GMEM_MOVEABLE", "GMEM_FIXED", "GMEM_ZEROINIT", "GHND", "MEM_COMMIT", "MEM_RESERVE",
                 "MEM_RELEASE", "PAGE_READWRITE", "ShellExecute", "ShellExecuteA", "ShellExecuteEx", "WinExec",
                 "CreateProcess", "CreateProcessA", "STARTUPINFO", "STARTUPINFOA", "PROCESS_INFORMATION",
                 "ExitProcess", "TerminateProcess", "GetCurrentProcess", "GetCurrentProcessId", "OpenProcess",
                 "GetCommandLine", "GetCommandLineA", "GetEnvironmentVariable", "GetEnvironmentVariableA",
                 "SetEnvironmentVariable", "SetUnhandledExceptionFilter", "UnhandledExceptionFilter",
                 "EXCEPTION_POINTERS", "LPEXCEPTION_POINTERS", "EXCEPTION_RECORD", "CONTEXT", "RaiseException",
                 "EXCEPTION_EXECUTE_HANDLER", "EXCEPTION_CONTINUE_SEARCH", "EXCEPTION_CONTINUE_EXECUTION",
                 "IsBadReadPtr", "IsBadWritePtr", "IsBadCodePtr", "IsBadStringPtr", "GetComputerName",
                 "GetComputerNameA", "GetUserName", "GetUserNameA", "FindResource", "FindResourceA",
                 "FindResourceEx", "LoadResource", "LockResource", "SizeofResource", "FreeResource",
                 "EnumResourceNames", "EnumResourceNamesA", "EnumResourceTypes", "ENUMRESNAMEPROC", "HRSRC",
                 "MAKEINTRESOURCE", "MAKEINTRESOURCEA", "RT_RCDATA", "RT_STRING", "RT_BITMAP", "RT_ICON",
                 "RT_CURSOR", "RT_DIALOG", "RT_MENU", "GetStartupInfo", "GetStartupInfoA", "WinMain",
                 "DllMain", "DLL_PROCESS_ATTACH", "DLL_PROCESS_DETACH", "DLL_THREAD_ATTACH", "DLL_THREAD_DETACH",
                 "CloseHandle", "HANDLE", "LPHANDLE", "GetFileVersionInfo", "GetFileVersionInfoA",
                 "GetFileVersionInfoSize", "GetFileVersionInfoSizeA", "VerQueryValue", "VerQueryValueA",
                 "VS_FIXEDFILEINFO", "MultiByteToWideChar", "WideCharToMultiByte", "GetACP", "GetOEMCP",
                 "GetCPInfo", "CP_ACP", "CP_UTF8", "IsDBCSLeadByte", "IsDBCSLeadByteEx", "CharNext", "CharNextA",
                 "CharPrev", "CharPrevA", "CharUpper", "CharUpperA", "CharLower", "CharLowerA", "IsCharAlpha",
                 "IsCharAlphaA", "IsCharAlphaNumeric", "IsCharAlphaNumericA", "lstrcpy", "lstrcpyA", "lstrcpyn",
                 "lstrcpynA", "lstrlen", "lstrlenA", "lstrlenW", "lstrcmp", "lstrcmpA", "lstrcmpi", "lstrcmpiA",
                 "lstrcat", "lstrcatA", "CompareString", "CompareStringA", "GetStringTypeA", "GetStringTypeW",
                 "GetLocaleInfo", "GetLocaleInfoA", "LOCALE_USER_DEFAULT", "LOCALE_SYSTEM_DEFAULT",
                 "CreateToolhelp32Snapshot", "Process32First", "Process32Next", "PROCESSENTRY32",
                 "SetErrorMode", "SEM_FAILCRITICALERRORS", "DebugBreak", "IsDebuggerPresent", "GetCurrentTime",
                 "MiniDumpWriteDump", "ERROR_SUCCESS", "ERROR_FILE_NOT_FOUND", "ERROR_ALREADY_EXISTS",
                 "ERROR_NO_MORE_FILES", "ERROR_PATH_NOT_FOUND", "ERROR_ACCESS_DENIED", "NO_ERROR",
                 "ERROR_MORE_DATA", "ERROR_INSUFFICIENT_BUFFER", "ERROR_NOT_ENOUGH_MEMORY"}, None),
    ("msvc-string", {"_stricmp", "stricmp", "_strnicmp", "strnicmp", "_strcmpi", "strcmpi", "_wcsicmp", "wcsicmp",
                     "_wcsnicmp", "wcsnicmp", "_snprintf", "_vsnprintf", "_snwprintf", "_vsnwprintf", "_scprintf",
                     "wsprintf", "wsprintfA", "wsprintfW", "wvsprintf", "wvsprintfA", "_itoa", "itoa", "_ltoa",
                     "ltoa", "_ultoa", "ultoa", "_itow", "_ltow", "_ultow", "_i64toa", "_ui64toa", "_atoi64",
                     "_strtoi64", "_strtoui64", "_wtoi", "_wtol", "_wtof", "_strlwr", "strlwr", "_strupr", "strupr",
                     "_wcslwr", "_wcsupr", "_strdup", "_wcsdup", "_strrev", "_strset", "_strnset", "_gcvt", "_fcvt",
                     "_ecvt", "gcvt", "fcvt", "ecvt", "_mbscpy", "_mbslen", "_mbsicmp", "_mbschr", "_mbsrchr",
                     "_mbsinc", "_mbsdec", "_ismbblead", "_mbclen", "sprintf_s", "swprintf_s", "strcpy_s",
                     "strncpy_s", "strcat_s", "wcscpy_s", "wcsncpy_s", "wcscat_s", "_stricmp_l", "_vscprintf",
                     "_swprintf", "_vswprintf", "_wfopen", "_wcsset", "_strncoll", "_stricoll", "_tcslen",
                     "_tcscpy", "_tcscmp", "_tcsicmp", "_tcschr", "_tcsrchr", "_tcsstr", "_tcsncpy", "_tcscat",
                     "_stprintf", "_sntprintf", "_tprintf", "_ttoi", "_tfopen", "TCHAR", "_TCHAR", "_T", "TEXT",
                     "LPSTR", "LPCSTR", "LPTSTR", "LPCTSTR", "LPWSTR", "LPCWSTR", "PSTR", "PCSTR", "PWSTR", "PCWSTR",
                     "WCHAR", "CHAR", "_wcsrev", "_strtime", "_strdate", "_getch", "_kbhit", "_putch", "_cputs"},
     None),
    ("msvc-lang", {"__int64", "__int32", "__int16", "__int8", "__stdcall", "__cdecl", "__fastcall", "__thiscall",
                   "__declspec", "__forceinline", "_asm", "__asm", "__try", "__except", "__finally", "__leave",
                   "_MSC_VER", "WINAPI", "CALLBACK", "APIENTRY", "PASCAL", "FAR", "NEAR", "_cdecl", "_stdcall",
                   "_fastcall", "__based", "__assume", "__noop", "_alloca", "__uuidof", "_inline", "__inline",
                   "__w64", "__ptr64", "__unaligned", "_ftol", "_finite", "_isnan", "_copysign", "_hypot",
                   "_rotl", "_rotr", "_lrotl", "_lrotr", "_byteswap_ulong", "_byteswap_ushort", "_controlfp",
                   "_control87", "_clearfp", "_statusfp", "_PC_24", "_PC_53", "_PC_64", "_MCW_PC", "_MCW_RC",
                   "_RC_NEAR", "_RC_CHOP", "_CRT_SECURE_NO_WARNINGS", "_set_new_handler", "_heapchk", "_msize",
                   "_expand", "_aligned_malloc", "_aligned_free", "__argc", "__argv", "_environ", "_getpid",
                   "_exit", "_onexit", "_CrtSetDbgFlag", "_CrtDumpMemoryLeaks", "_ASSERT", "_ASSERTE",
                   "_countof", "UNREFERENCED_PARAMETER", "_ReturnAddress", "_AddressOfReturnAddress"}, None),
    ("win-types", {"DWORD", "WORD", "BYTE", "BOOL", "BOOLEAN", "UINT", "ULONG", "USHORT", "UCHAR", "LONG", "SHORT",
                   "INT", "FLOAT", "LPVOID", "LPCVOID", "PVOID", "LPDWORD", "LPWORD", "LPBYTE", "LPBOOL", "LPLONG",
                   "PDWORD", "PBYTE", "PLONG", "SIZE_T", "DWORD_PTR", "LONG_PTR", "ULONG_PTR", "UINT_PTR",
                   "INT_PTR", "DWORDLONG", "LONGLONG", "ULONGLONG", "TRUE", "FALSE", "RECT", "LPRECT", "LPCRECT",
                   "POINT", "LPPOINT", "SIZE", "LPSIZE", "POINTS", "VOID", "LPINT", "ATOM", "HGLRC",
                   "SECURITY_ATTRIBUTES", "LPSECURITY_ATTRIBUTES", "OVERLAPPED", "LPOVERLAPPED",
                   "SetRect", "SetRectEmpty", "IntersectRect", "UnionRect", "PtInRect", "OffsetRect", "InflateRect",
                   "IsRectEmpty", "EqualRect", "CopyRect", "ZeroMemory", "CopyMemory", "MoveMemory", "FillMemory",
                   "SecureZeroMemory"}, None),
]

WIN_HEADERS = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.M)
WIN_HEADER_NAMES = {"windows.h", "winsock.h", "winsock2.h", "ws2tcpip.h", "mmsystem.h", "io.h", "direct.h",
                    "process.h", "tchar.h", "objbase.h", "shlobj.h", "imm.h", "iphlpapi.h", "ddraw.h", "d3d.h",
                    "d3dtypes.h", "d3dcaps.h", "dinput.h", "dsound.h", "dmusici.h", "conio.h", "malloc.h",
                    "winbase.h", "winuser.h", "wingdi.h", "winreg.h", "windowsx.h", "commctrl.h", "commdlg.h",
                    "shellapi.h", "mbstring.h", "crtdbg.h", "excpt.h", "winerror.h", "basetsd.h", "wtypes.h",
                    "winnt.h", "winver.h", "mmreg.h", "msacm.h", "vfw.h", "dbghelp.h", "tlhelp32.h", "ole2.h",
                    "objidl.h", "unknwn.h", "initguid.h", "share.h", "sys/locking.h", "dxerr8.h", "d3dx.h",
                    "multimon.h", "intrin.h", "float.h"}

# areas: path prefix -> label (first match wins); [out] = owned by other work (catalogued only)
AREAS = [
    ("src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/", "renderer DX7 [out]"),
    ("include/dx7/", "renderer DX7 [out]"),
    ("src/Low-Level Engine/Input System/", "input system [out]"),
    ("src/Low-Level Engine/Graphics Engine/GEScreen", "GE screen/surface/window [out]"),
    ("src/Low-Level Engine/Graphics Engine/GESurface", "GE screen/surface/window [out]"),
    ("src/Low-Level Engine/Graphics Engine/GEWindow", "GE screen/surface/window [out]"),
    ("include/lle/GEScreen", "GE screen/surface/window [out]"),
    ("include/lle/GESurface", "GE screen/surface/window [out]"),
    ("include/lle/GEWindow", "GE screen/surface/window [out]"),
    ("include/lle/IS", "input system [out]"),
    ("include/lle/Input System", "input system [out]"),
    ("src/Low-Level Engine/Network Engine/", "network engine (Winsock)"),
    ("include/lle/NE", "network engine (Winsock)"),
    ("src/Low-Level Engine/Graphics Engine/", "graphics engine (other)"),
    ("include/lle/GE", "graphics engine (other)"),
    ("src/Low-Level Engine/Sound Engine/", "sound engine"),
    ("include/lle/SE", "sound engine"),
    ("src/Low-Level Engine/Thread System/", "thread system"),
    ("include/lle/TS", "thread system"),
    ("src/Low-Level Engine/Time Manager/", "time manager"),
    ("include/lle/TM", "time manager"),
    ("src/Low-Level Engine/File System/", "file system"),
    ("include/lle/FS", "file system"),
    ("include/lle/File System", "file system"),
    ("src/Low-Level Engine/Error System/", "error system"),
    ("include/lle/ES", "error system"),
    ("src/Low-Level Engine/User Interface/", "engine UI"),
    ("include/lle/UI", "engine UI"),
    ("include/lle/User Interface", "engine UI"),
    ("src/Low-Level Engine/Utilities/", "engine utilities"),
    ("include/lle/U", "engine utilities"),
    ("src/Low-Level Engine/", "engine (other)"),
    ("include/lle/", "engine headers (other)"),
    ("src/Platform/", "platform layer"),
    ("include/platform/", "platform layer"),
    ("src/Empire Earth/Random Map/", "game: random map"),
    ("src/Empire Earth/User Interface Thread/", "game: UI thread / lobby"),
    ("src/Empire Earth/", "game (other)"),
    ("include/ee/", "game headers"),
    ("src/RTS Engine/", "RTS engine"),
]

COMMENT_STRING = re.compile(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)
IDENT = re.compile(r"\b[A-Za-z_]\w*\b")


def area_of(rel):
    for prefix, label in AREAS:
        if rel.startswith(prefix):
            return label
    return "other"


def compile_families():
    out = []
    for fam, names, rx in FAMILIES:
        out.append((fam, names, re.compile(rf"^{rx}$") if rx else None))
    return out


def classify(name, fams, cache={}):
    if name in cache:
        return cache[name]
    hit = None
    for fam, names, rx in fams:
        if name in names or (rx is not None and rx.match(name)):
            hit = fam
            break
    cache[name] = hit
    return hit


def strip(text):
    """Comments and string/char literals blanked out, newlines kept (line numbers stay right)."""
    return COMMENT_STRING.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def first_party_files():
    for top in ("src", "include"):
        for p in sorted((ROOT / top).rglob("*")):
            if p.suffix.lower() not in EXTS or not p.is_file():
                continue
            rel = p.relative_to(ROOT).as_posix()
            if rel.startswith(THIRD_PARTY):
                continue
            yield rel, p


def scan():
    global shadowed
    fams = compile_families()
    shadowed = shadowed_names(list(first_party_files()))
    global shadowed_win32
    shadowed_win32 = sorted(n for n in shadowed if classify(n, fams))
    sites = []   # (file, line, family, name, area)
    for rel, p in first_party_files():
        raw = p.read_bytes().decode("latin-1").replace("\r\n", "\n")
        area = area_of(rel)
        for m in WIN_HEADERS.finditer(raw):
            h = m.group(1).replace("\\", "/").lower()
            if h in WIN_HEADER_NAMES:
                sites.append((rel, raw.count("\n", 0, m.start()) + 1, "win-headers", h, area))
        text = strip(raw)
        line = 1
        pos = 0
        for m in IDENT.finditer(text):
            name = m.group(0)
            fam = classify(name, fams)
            if fam is None:
                continue
            before = text[max(0, m.start() - 80):m.start()]
            member = MEMBER_BEFORE.search(before)
            if member or name in shadowed:
                # a first-party member spelled like a Win32 name is no Win32 use, unless it is the A/W expansion of
                # a Win32 macro (windows.h renames such members: a method DrawText is DrawTextA in every unit)
                if name in shadowed and not member and GLOBAL_BEFORE.search(before):
                    pass   # ::Name: an explicit global call from inside a class that shadows the name
                elif MACRO_AW.match(name) and name[:-1] in AW_MACROS:
                    fam = "win-macro-names"
                else:
                    continue
            line += text.count("\n", pos, m.start())
            pos = m.start()
            sites.append((rel, line, fam, name, area))
    return sites


MEMBER_BEFORE = re.compile(r"(?:\.|->|\b(?!std\b)[A-Za-z_]\w*\s*::)\s*$")
GLOBAL_BEFORE = re.compile(r"(?<![\w>])::\s*$")
MACRO_AW = re.compile(r"^[A-Z]\w+[AW]$")
# Win32 functions windows.h defines as A/W macros (those whose names first-party classes also use)
AW_MACROS = {"DrawText", "CreateEvent", "GetMessage", "LoadString", "CreateWindow", "SendMessage", "PostMessage",
             "GetObject", "CreateFont", "LoadImage", "LoadBitmap", "LoadIcon", "LoadCursor", "CreateDirectory",
             "CreateFile", "DeleteFile", "CopyFile", "MoveFile", "FindWindow", "GetClassName", "SetWindowText",
             "GetWindowText", "CreateMutex", "CreateSemaphore", "GetCurrentDirectory", "SetCurrentDirectory",
             "GetUserName", "GetComputerName", "TextOut", "ExtTextOut", "GetTextMetrics", "RegisterClass",
             "DefWindowProc", "CallWindowProc", "PeekMessage", "DispatchMessage", "GetModuleFileName",
             "GetModuleHandle", "LoadLibrary", "FindResource", "GetProp", "SetProp", "RemoveProp", "CreateDialog",
             "DialogBox", "MessageBox", "GetFileAttributes", "SetFileAttributes", "GetTempPath", "GetFullPathName",
             "FormatMessage", "OutputDebugString", "GetVersionEx", "GetCommandLine", "GetEnvironmentVariable",
             "SetEnvironmentVariable", "CharUpper", "CharLower", "CharNext", "CharPrev", "GetCharWidth",
             "GetTextExtentPoint", "GetTextExtentPoint32", "EnumFonts", "AddFontResource", "RemoveFontResource",
             "GetKeyNameText", "MapVirtualKey", "VkKeyScan", "SystemParametersInfo", "GetDriveType",
             "GetVolumeInformation", "GetDiskFreeSpace", "SearchPath", "PlaySound", "UpdateResource",
             "EnumDisplaySettings", "ChangeDisplaySettings", "RegisterWindowMessage", "CreateProcess",
             "ShellExecute", "GetStartupInfo", "OpenEvent", "OpenMutex", "LoadMenu", "LoadAccelerators", "CreateDC",
             "GetProfileString", "GetPrivateProfileString", "WritePrivateProfileString", "GetPrivateProfileInt",
             "SetDlgItemText", "GetDlgItemText", "IsDialogMessage", "TranslateAccelerator", "CopyImage",
             "DrawState", "GrayString", "TabbedTextOut", "GetTabbedTextExtent", "GetLocaleInfo",
             "GetNumberFormat", "GetDateFormat", "GetTimeFormat", "CompareString", "StartDoc", "GetMonitorInfo"}


def shadowed_names(files):
    """Names first-party code uses in class scope (`Class::Name`, `obj.Name`, `p->Name`): spelled like Win32 names,
    but where they appear unqualified (inside their class) they are the class's own."""
    out = set()
    rx = re.compile(r"(?:\b(?!std\b)[A-Za-z_]\w*\s*::|\.|->)\s*([A-Za-z_]\w*)")
    for _, p in files:
        out.update(rx.findall(strip(p.read_bytes().decode("latin-1"))))
    return out


shadowed = set()
shadowed_win32 = []


def table(rows, headers):
    w = [max(len(str(r[i])) for r in [headers, *rows]) for i in range(len(headers))]
    lines = ["  ".join(str(h).ljust(w[i]) for i, h in enumerate(headers))]
    for r in rows:
        lines.append("  ".join(str(c).rjust(w[i]) if isinstance(c, int) else str(c).ljust(w[i])
                               for i, c in enumerate(r)))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--by-file", type=int, default=5, metavar="N", help="top N files per family (default 5)")
    ap.add_argument("--family", help="list one family's names and files")
    ap.add_argument("--tsv", default=str(ROOT / "build" / "winapi_census.tsv"))
    ap.add_argument("--md", action="store_true", help="print the family table as Markdown")
    a = ap.parse_args()
    sites = scan()
    out = pathlib.Path(a.tsv)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes("".join("\t".join(map(str, s)) + "\n" for s in sites).encode("utf-8"))

    order = [f for f, _, _ in FAMILIES] + ["win-headers"]
    by_fam = collections.defaultdict(list)
    for s in sites:
        by_fam[s[2]].append(s)
    files_all = {s[0] for s in sites}

    if a.family:
        rows = by_fam.get(a.family, [])
        names = collections.Counter(s[3] for s in rows)
        print(f"{a.family}: {len(rows)} sites, {len(names)} names, {len({s[0] for s in rows})} files")
        for n, c in names.most_common():
            fc = collections.Counter(s[0] for s in rows if s[3] == n).most_common(3)
            print(f"{c:6}  {n:36} " + "; ".join(f"{f.rsplit('/', 1)[-1]} {k}" for f, k in fc))
        return

    def in_scope(s):
        return "[out]" not in s[4]
    fam_rows = []
    for f in order:
        rows = by_fam.get(f, [])
        if not rows:
            continue
        ins = [s for s in rows if in_scope(s)]
        top = collections.Counter(s[3] for s in rows).most_common(4)
        fam_rows.append((f, len(rows), len(ins), len({s[3] for s in rows}), len({s[0] for s in rows}),
                         ", ".join(f"{n} {c}" for n, c in top)))
    hdr = ("family", "sites", "in scope", "names", "files", "top names")
    if a.md:
        print("| " + " | ".join(hdr) + " |")
        print("|" + "---|" * len(hdr))
        for r in fam_rows:
            print("| " + " | ".join(str(c) for c in r) + " |")
        tot = len(sites)
        print(f"| total | {tot} | {sum(1 for s in sites if in_scope(s))} | | {len(files_all)} | |")
        print()
        areas = collections.Counter(s[4] for s in sites)
        print("| area | sites | files | families (sites) |")
        print("|---|---|---|---|")
        for ar, n in areas.most_common():
            fs = collections.Counter(s[2] for s in sites if s[4] == ar).most_common(4)
            print(f"| {ar} | {n} | {len({s[0] for s in sites if s[4] == ar})} | "
                  + ", ".join(f"{f} {c}" for f, c in fs) + " |")
        return
    print(f"Windows-specific dependency census: {len(sites)} sites in {len(files_all)} of "
          f"{sum(1 for _ in first_party_files())} first-party files -> {out}\n")
    print(table(fam_rows, hdr))
    print()
    areas = collections.Counter(s[4] for s in sites)
    print(table([(ar, n, len({s[0] for s in sites if s[4] == ar}),
                  ", ".join(f"{f} {c}" for f, c in collections.Counter(
                      s[2] for s in sites if s[4] == ar).most_common(4)))
                 for ar, n in areas.most_common()], ("area", "sites", "files", "families")))
    if a.by_file:
        print()
        for f in order:
            rows = by_fam.get(f, [])
            if not rows:
                continue
            fc = collections.Counter(s[0] for s in rows).most_common(a.by_file)
            print(f"{f}: " + "; ".join(f"{p} {c}" for p, c in fc))
    print(f"\nWin32 names first-party classes also use (counted only as ::Name): {', '.join(shadowed_win32)}")


if __name__ == "__main__":
    sys.exit(main())
