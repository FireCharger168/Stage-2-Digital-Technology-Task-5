// Common.h
// Shared includes, control IDs, and small helpers used across the app.
#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

// Opts into ComCtl32 v6 so controls render with the modern Windows look
// instead of the old Windows 2000 style.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

// ===== Custom app messages =====
#define WM_APP_DATA_CHANGED (WM_APP + 1)

// ===== Shared timer id (reused from the original starter prototype) =====
#define ID_TIMER_CHECK 1

// ===== Main window controls =====
#define IDC_TAB 1010

#define IDC_R_FILTER_LABEL 1100
#define IDC_R_FILTER       1101
#define IDC_R_LIST         1102
#define IDC_R_ADD          1103
#define IDC_R_EDIT         1104
#define IDC_R_DELETE       1105
#define IDC_R_SHOWPAST     1106

#define IDC_T_FILTER_LABEL 1200
#define IDC_T_FILTER       1201
#define IDC_T_LIST         1202
#define IDC_T_ADD          1203
#define IDC_T_EDIT         1204
#define IDC_T_DELETE       1205
#define IDC_T_RESTORE      1206
#define IDC_T_MOVEUP       1207
#define IDC_T_MOVEDOWN     1208
#define IDC_T_RECYCLE      1209

// ===== Entry dialog (Add/Edit Reminder or Task) controls =====
#define IDC_ED_NAME      2001
#define IDC_ED_DESC      2002
#define IDC_ED_SUBJECT   2003
#define IDC_ED_NOTES     2004
#define IDC_ED_DATE      2005
#define IDC_ED_TIME      2006
#define IDC_ED_COLOURBTN 2007
#define IDC_ED_OK        2008
#define IDC_ED_CANCEL    2009

// ===== Notification popup controls =====
#define IDC_NP_SNOOZE  3001
#define IDC_NP_DISMISS 3002

// ===== Colour used to mean "no explicit colour chosen, use the default text colour" =====
const COLORREF DEFAULT_COLOUR = RGB(0, 0, 0);

// FILETIME-style 100ns tick constants, used for due-date / snooze maths.
const ULONGLONG TICKS_PER_SECOND = 10000000ULL;
const ULONGLONG TICKS_PER_MINUTE = 60ULL * TICKS_PER_SECOND;

// ===== Time helpers =====
// Both sides of every comparison in this app go through this same function,
// so the result is only ever used for ordering/duration maths - never shown
// to the user or compared against a "real" UTC value.
ULONGLONG SystemTimeToTicks(const SYSTEMTIME& st);
SYSTEMTIME NowLocalSystemTime();
std::wstring FormatDate(const SYSTEMTIME& st);
std::wstring FormatDateTime(const SYSTEMTIME& st);

// ===== String helpers =====
std::wstring GetWindowTextStr(HWND h);
bool ContainsCI(const std::wstring& haystack, const std::wstring& needle);

// Escaping for the pipe-delimited save file: turns \, | and newlines into
// safe sequences so a saved field can never be mistaken for a delimiter.
std::wstring EscapeForStorage(const std::wstring& s);
std::wstring UnescapeFromStorage(const std::wstring& s);
std::vector<std::wstring> SplitFields(const std::wstring& line, wchar_t delim);

// UTF-8 <-> UTF-16 conversion (the save file is UTF-8 on disk).
std::string WStringToUtf8(const std::wstring& w);
std::wstring Utf8ToWString(const std::string& s);
