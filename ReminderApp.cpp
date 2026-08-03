// ReminderApp.cpp
// Digital Technology Task 5 - Reminder & Task Manager
// Pure Win32 API + Common Controls, no external libraries and no .rc resource
// script required (dialogs are built at runtime from CreateWindow calls).
//
// Implements the plan from the Task 5 brainstorming notes:
//   - Reminders: name*, description, subject, date*, notes, colour (default black)
//     sorted soonest -> latest, filterable, colour codable, editable, deletable,
//     with a "show past reminders" toggle and popup notifications with snooze.
//   - Tasks: name*, description, subject, optional date, notes, colour (default
//     black), filterable, colour codable, editable, deletable to a recycle bin
//     (with restore), and reorderable (move up / move down).
//
// Compile (MinGW):
//   g++ ReminderApp.cpp -o ReminderApp.exe -mwindows -lcomctl32
// Compile (MSVC, Developer Command Prompt):
//   cl ReminderApp.cpp /link user32.lib gdi32.lib comctl32.lib comdlg32.lib

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <cstdlib>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ---------------------------------------------------------------------------
// Control / timer IDs
// ---------------------------------------------------------------------------
#define IDC_TAB              1001
#define IDC_REM_LIST         1002
#define IDC_REM_FILTER       1003
#define IDC_REM_CHK_PAST     1004
#define IDC_REM_BTN_NEW      1005
#define IDC_REM_BTN_EDIT     1006
#define IDC_REM_BTN_DELETE   1007

#define IDC_TASK_LIST        1010
#define IDC_TASK_FILTER      1011
#define IDC_TASK_CHK_BIN     1012
#define IDC_TASK_BTN_NEW     1013
#define IDC_TASK_BTN_EDIT    1014
#define IDC_TASK_BTN_DELETE  1015
#define IDC_TASK_BTN_MOVEUP  1016
#define IDC_TASK_BTN_MOVEDOWN 1017
#define IDC_TASK_BTN_RESTORE 1018

#define IDT_NOTIFY_CHECK     2001

// Editor window control IDs (each editor is its own top-level window, so IDs
// can be reused between the reminder editor and the task editor).
#define IDC_ED_NAME          101
#define IDC_ED_SUBJECT       102
#define IDC_ED_DESC          103
#define IDC_ED_NOTES         104
#define IDC_ED_DATE          105
#define IDC_ED_COLOURBTN     106
#define IDC_ED_COLOURPREVIEW 107

#define IDC_POPUP_SNOOZE     201
#define IDC_POPUP_DISMISS    202
#define IDC_POPUP_TEXT       203

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------
struct Reminder {
    int id = 0;
    std::wstring name;
    std::wstring description;
    std::wstring subject;
    std::wstring notes;
    SYSTEMTIME date{};
    COLORREF colour = RGB(0, 0, 0);
};

struct Task {
    int id = 0;
    std::wstring name;
    std::wstring description;
    std::wstring subject;
    std::wstring notes;
    bool hasDate = false;
    SYSTEMTIME date{};
    COLORREF colour = RGB(0, 0, 0);
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static std::vector<Reminder> g_reminders;
static std::vector<Task> g_tasks;
static std::vector<Task> g_taskBin;
static int g_nextReminderId = 1;
static int g_nextTaskId = 1;
static std::vector<int> g_openPopups; // reminder ids currently showing a notification

static HWND g_hTab;
static HWND g_hReminderList, g_hReminderFilter, g_hReminderFilterLbl, g_hChkShowPast;
static HWND g_hRemBtnNew, g_hRemBtnEdit, g_hRemBtnDelete;
static HWND g_hTaskList, g_hTaskFilter, g_hTaskFilterLbl, g_hChkShowBin;
static HWND g_hTaskBtnNew, g_hTaskBtnEdit, g_hTaskBtnDelete, g_hTaskBtnMoveUp, g_hTaskBtnMoveDown, g_hTaskBtnRestore;

static bool g_showPastReminders = false;
static bool g_showTaskBin = false;

static COLORREF g_customColours[16] = { 0 };

// ---------------------------------------------------------------------------
// Small string / time utilities
// ---------------------------------------------------------------------------
static long long ToComparable(const SYSTEMTIME& st) {
    return (long long)st.wYear * 10000000000LL + (long long)st.wMonth * 100000000LL +
           (long long)st.wDay * 1000000LL + (long long)st.wHour * 10000LL +
           (long long)st.wMinute * 100LL + st.wSecond;
}

static std::wstring FormatDate(const SYSTEMTIME& st) {
    wchar_t buf[64];
    wsprintfW(buf, L"%02d/%02d/%04d %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
    return buf;
}

static SYSTEMTIME AddMinutes(const SYSTEMTIME& st, int minutes) {
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart += (ULONGLONG)minutes * 60ULL * 10000000ULL;
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    SYSTEMTIME out{};
    FileTimeToSystemTime(&ft, &out);
    return out;
}

static std::string WToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], len, nullptr, nullptr);
    return out;
}

static std::wstring Utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

// Fields are stored pipe-delimited on one line, so '|', '\\' and newlines
// inside a field are escaped when saving and reversed when loading.
static std::wstring EscapeForFile(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'\n') out += L"\\n";
        else if (c == L'\r') continue;
        else if (c == L'\\') out += L"\\\\";
        else if (c == L'|') out += L"\\p";
        else out += c;
    }
    return out;
}

static std::wstring UnescapeFromFile(const std::wstring& s) {
    std::wstring out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            if (s[i + 1] == L'n') { out += L'\n'; ++i; continue; }
            if (s[i + 1] == L'p') { out += L'|'; ++i; continue; }
            if (s[i + 1] == L'\\') { out += L'\\'; ++i; continue; }
        }
        out += s[i];
    }
    return out;
}

static std::vector<std::wstring> SplitPipe(const std::wstring& s) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(L'|', start);
        if (pos == std::wstring::npos) { parts.push_back(s.substr(start)); break; }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

static std::wstring ToLowerCopy(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}

static bool ContainsFilter(const std::wstring& haystack, const std::wstring& needleLower) {
    if (needleLower.empty()) return true;
    return ToLowerCopy(haystack).find(needleLower) != std::wstring::npos;
}

static LPWSTR NonConst(const std::wstring& s) { return const_cast<LPWSTR>(s.c_str()); }

// ---------------------------------------------------------------------------
// Persistence (simple pipe-delimited UTF-8 text files, one per list)
// ---------------------------------------------------------------------------
static void SaveReminders() {
    std::ofstream f("reminders_data.txt", std::ios::binary | std::ios::trunc);
    f << g_nextReminderId << "\n";
    for (auto& r : g_reminders) {
        std::wstringstream line;
        line << r.id << L"|" << EscapeForFile(r.name) << L"|" << EscapeForFile(r.description) << L"|"
             << EscapeForFile(r.subject) << L"|" << EscapeForFile(r.notes) << L"|"
             << r.date.wYear << L"|" << r.date.wMonth << L"|" << r.date.wDay << L"|"
             << r.date.wHour << L"|" << r.date.wMinute << L"|" << (unsigned long)r.colour;
        f << WToUtf8(line.str()) << "\n";
    }
}

static void LoadReminders() {
    g_reminders.clear();
    std::ifstream f("reminders_data.txt", std::ios::binary);
    if (!f) return;
    std::string firstLine;
    if (!std::getline(f, firstLine)) return;
    if (!firstLine.empty()) g_nextReminderId = std::atoi(firstLine.c_str());
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto parts = SplitPipe(Utf8ToW(line));
        if (parts.size() < 11) continue;
        Reminder r;
        r.id = _wtoi(parts[0].c_str());
        r.name = UnescapeFromFile(parts[1]);
        r.description = UnescapeFromFile(parts[2]);
        r.subject = UnescapeFromFile(parts[3]);
        r.notes = UnescapeFromFile(parts[4]);
        r.date.wYear = (WORD)_wtoi(parts[5].c_str());
        r.date.wMonth = (WORD)_wtoi(parts[6].c_str());
        r.date.wDay = (WORD)_wtoi(parts[7].c_str());
        r.date.wHour = (WORD)_wtoi(parts[8].c_str());
        r.date.wMinute = (WORD)_wtoi(parts[9].c_str());
        r.colour = (COLORREF)wcstoul(parts[10].c_str(), nullptr, 10);
        if (r.id >= g_nextReminderId) g_nextReminderId = r.id + 1;
        g_reminders.push_back(r);
    }
}

static void SaveTaskVector(const std::vector<Task>& v, const char* filename, int nextId) {
    std::ofstream f(filename, std::ios::binary | std::ios::trunc);
    f << nextId << "\n";
    for (auto& t : v) {
        std::wstringstream line;
        line << t.id << L"|" << EscapeForFile(t.name) << L"|" << EscapeForFile(t.description) << L"|"
             << EscapeForFile(t.subject) << L"|" << EscapeForFile(t.notes) << L"|"
             << (t.hasDate ? 1 : 0) << L"|"
             << t.date.wYear << L"|" << t.date.wMonth << L"|" << t.date.wDay << L"|"
             << t.date.wHour << L"|" << t.date.wMinute << L"|" << (unsigned long)t.colour;
        f << WToUtf8(line.str()) << "\n";
    }
}

static void LoadTaskVector(std::vector<Task>& v, const char* filename) {
    v.clear();
    std::ifstream f(filename, std::ios::binary);
    if (!f) return;
    std::string firstLine;
    if (!std::getline(f, firstLine)) return;
    int fileNextId = firstLine.empty() ? 1 : std::atoi(firstLine.c_str());
    if (fileNextId > g_nextTaskId) g_nextTaskId = fileNextId;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto parts = SplitPipe(Utf8ToW(line));
        if (parts.size() < 12) continue;
        Task t;
        t.id = _wtoi(parts[0].c_str());
        t.name = UnescapeFromFile(parts[1]);
        t.description = UnescapeFromFile(parts[2]);
        t.subject = UnescapeFromFile(parts[3]);
        t.notes = UnescapeFromFile(parts[4]);
        t.hasDate = _wtoi(parts[5].c_str()) != 0;
        t.date.wYear = (WORD)_wtoi(parts[6].c_str());
        t.date.wMonth = (WORD)_wtoi(parts[7].c_str());
        t.date.wDay = (WORD)_wtoi(parts[8].c_str());
        t.date.wHour = (WORD)_wtoi(parts[9].c_str());
        t.date.wMinute = (WORD)_wtoi(parts[10].c_str());
        t.colour = (COLORREF)wcstoul(parts[11].c_str(), nullptr, 10);
        if (t.id >= g_nextTaskId) g_nextTaskId = t.id + 1;
        v.push_back(t);
    }
}

static void SaveTasks() { SaveTaskVector(g_tasks, "tasks_data.txt", g_nextTaskId); }
static void LoadTasks() { LoadTaskVector(g_tasks, "tasks_data.txt"); }
static void SaveTaskBin() { SaveTaskVector(g_taskBin, "tasks_bin_data.txt", g_nextTaskId); }
static void LoadTaskBin() { LoadTaskVector(g_taskBin, "tasks_bin_data.txt"); }

// ---------------------------------------------------------------------------
// Reminder Indexing (sorted soonest -> latest) & Task Default Indexing
// ---------------------------------------------------------------------------
static void SortReminders() {
    std::sort(g_reminders.begin(), g_reminders.end(), [](const Reminder& a, const Reminder& b) {
        return ToComparable(a.date) < ToComparable(b.date);
    });
}

// ---------------------------------------------------------------------------
// List refresh / filtering
// ---------------------------------------------------------------------------
static void RefreshReminderList() {
    SortReminders();
    ListView_DeleteAllItems(g_hReminderList);
    wchar_t filterBuf[256] = L"";
    GetWindowTextW(g_hReminderFilter, filterBuf, 256);
    std::wstring filter = ToLowerCopy(filterBuf);
    SYSTEMTIME now; GetLocalTime(&now);
    long long nowC = ToComparable(now);

    int row = 0;
    for (auto& r : g_reminders) {
        if (!g_showPastReminders && ToComparable(r.date) < nowC) continue;
        if (!filter.empty()) {
            bool match = ContainsFilter(r.name, filter) || ContainsFilter(r.subject, filter) ||
                         ContainsFilter(r.description, filter) || ContainsFilter(r.notes, filter);
            if (!match) continue;
        }
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = NonConst(r.name);
        item.lParam = r.id;
        ListView_InsertItem(g_hReminderList, &item);
        ListView_SetItemText(g_hReminderList, row, 1, NonConst(r.subject));
        std::wstring dateStr = FormatDate(r.date);
        ListView_SetItemText(g_hReminderList, row, 2, NonConst(dateStr));
        ListView_SetItemText(g_hReminderList, row, 3, NonConst(r.description));
        row++;
    }
}

static void RefreshTaskList() {
    ListView_DeleteAllItems(g_hTaskList);
    wchar_t filterBuf[256] = L"";
    GetWindowTextW(g_hTaskFilter, filterBuf, 256);
    std::wstring filter = ToLowerCopy(filterBuf);
    std::vector<Task>& v = g_showTaskBin ? g_taskBin : g_tasks;

    int row = 0;
    for (auto& t : v) {
        if (!filter.empty()) {
            bool match = ContainsFilter(t.name, filter) || ContainsFilter(t.subject, filter) ||
                         ContainsFilter(t.description, filter) || ContainsFilter(t.notes, filter);
            if (!match) continue;
        }
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = NonConst(t.name);
        item.lParam = t.id;
        ListView_InsertItem(g_hTaskList, &item);
        ListView_SetItemText(g_hTaskList, row, 1, NonConst(t.subject));
        std::wstring dateStr = t.hasDate ? FormatDate(t.date) : L"-";
        ListView_SetItemText(g_hTaskList, row, 2, NonConst(dateStr));
        ListView_SetItemText(g_hTaskList, row, 3, NonConst(t.description));
        row++;
    }
}

static int GetSelectedId(HWND hList) {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return -1;
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    ListView_GetItem(hList, &item);
    return (int)item.lParam;
}

static void RemoveOpenPopup(int id) {
    g_openPopups.erase(std::remove(g_openPopups.begin(), g_openPopups.end(), id), g_openPopups.end());
}

static Reminder g_tempReminder;
static bool g_reminderEditorOK = false;
static HWND g_hRE_Name, g_hRE_Subject, g_hRE_Desc, g_hRE_Notes, g_hRE_Date, g_hRE_ColourPreview;
static HBRUSH g_hRE_ColourBrush = nullptr;

static LRESULT CALLBACK ReminderEditorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Name*:", WS_VISIBLE | WS_CHILD, 15, 15, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_Name = CreateWindowW(L"EDIT", g_tempReminder.name.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 15, 260, 22, hwnd, (HMENU)IDC_ED_NAME, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Subject:", WS_VISIBLE | WS_CHILD, 15, 45, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_Subject = CreateWindowW(L"EDIT", g_tempReminder.subject.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 45, 260, 22, hwnd, (HMENU)IDC_ED_SUBJECT, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Description:", WS_VISIBLE | WS_CHILD, 15, 75, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_Desc = CreateWindowW(L"EDIT", g_tempReminder.description.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 75, 260, 22, hwnd, (HMENU)IDC_ED_DESC, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Notes:", WS_VISIBLE | WS_CHILD, 15, 105, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_Notes = CreateWindowW(L"EDIT", g_tempReminder.notes.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
            115, 105, 260, 80, hwnd, (HMENU)IDC_ED_NOTES, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Date*:", WS_VISIBLE | WS_CHILD, 15, 200, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_Date = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | DTS_SHORTDATEFORMAT,
            115, 197, 260, 24, hwnd, (HMENU)IDC_ED_DATE, nullptr, nullptr);
        DateTime_SetSystemtime(g_hRE_Date, GDT_VALID, &g_tempReminder.date);

        CreateWindowW(L"STATIC", L"Colour:", WS_VISIBLE | WS_CHILD, 15, 235, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hRE_ColourPreview = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            115, 235, 40, 24, hwnd, (HMENU)IDC_ED_COLOURPREVIEW, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Choose Colour...", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            165, 235, 140, 24, hwnd, (HMENU)IDC_ED_COLOURBTN, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
            115, 275, 80, 28, hwnd, (HMENU)IDOK, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            205, 275, 80, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HWND hCtl = (HWND)lParam;
        if (hCtl == g_hRE_ColourPreview) {
            if (g_hRE_ColourBrush) DeleteObject(g_hRE_ColourBrush);
            g_hRE_ColourBrush = CreateSolidBrush(g_tempReminder.colour);
            return (LRESULT)g_hRE_ColourBrush;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ED_COLOURBTN: {
            CHOOSECOLORW cc{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = g_customColours;
            cc.rgbResult = g_tempReminder.colour;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColorW(&cc)) {
                g_tempReminder.colour = cc.rgbResult;
                InvalidateRect(g_hRE_ColourPreview, nullptr, TRUE);
            }
            break;
        }
        case IDOK: {
            wchar_t buf[512];
            GetWindowTextW(g_hRE_Name, buf, 512);
            if (wcslen(buf) == 0) {
                MessageBoxW(hwnd, L"Name is required.", L"Missing information", MB_OK | MB_ICONWARNING);
                break;
            }
            g_tempReminder.name = buf;
            GetWindowTextW(g_hRE_Subject, buf, 512); g_tempReminder.subject = buf;
            GetWindowTextW(g_hRE_Desc, buf, 512); g_tempReminder.description = buf;
            wchar_t notesBuf[4096];
            GetWindowTextW(g_hRE_Notes, notesBuf, 4096); g_tempReminder.notes = notesBuf;
            SYSTEMTIME st;
            DateTime_GetSystemtime(g_hRE_Date, &st);
            g_tempReminder.date = st;
            g_reminderEditorOK = true;
            DestroyWindow(hwnd);
            break;
        }
        case IDCANCEL:
            g_reminderEditorOK = false;
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_CLOSE:
        g_reminderEditorOK = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_hRE_ColourBrush) { DeleteObject(g_hRE_ColourBrush); g_hRE_ColourBrush = nullptr; }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool RunReminderEditor(HWND owner, Reminder& r, bool isNew) {
    g_tempReminder = r;
    g_reminderEditorOK = false;
    EnableWindow(owner, FALSE);
    HWND hEditor = CreateWindowExW(WS_EX_DLGMODALFRAME, L"ReminderEditorClass",
        isNew ? L"New Reminder" : L"Edit Reminder",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 360,
        owner, nullptr, GetModuleHandle(nullptr), nullptr);
    ShowWindow(hEditor, SW_SHOW);
    UpdateWindow(hEditor);
    MSG msg;
    while (IsWindow(hEditor) && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hEditor, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (g_reminderEditorOK) r = g_tempReminder;
    return g_reminderEditorOK;
}

// ---------------------------------------------------------------------------
// Task Creation / Editing dialog
// ---------------------------------------------------------------------------
static Task g_tempTask;
static bool g_taskEditorOK = false;
static HWND g_hTE_Name, g_hTE_Subject, g_hTE_Desc, g_hTE_Notes, g_hTE_Date, g_hTE_ColourPreview;
static HBRUSH g_hTE_ColourBrush = nullptr;

static LRESULT CALLBACK TaskEditorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Name*:", WS_VISIBLE | WS_CHILD, 15, 15, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_Name = CreateWindowW(L"EDIT", g_tempTask.name.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 15, 260, 22, hwnd, (HMENU)IDC_ED_NAME, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Subject:", WS_VISIBLE | WS_CHILD, 15, 45, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_Subject = CreateWindowW(L"EDIT", g_tempTask.subject.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 45, 260, 22, hwnd, (HMENU)IDC_ED_SUBJECT, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Description:", WS_VISIBLE | WS_CHILD, 15, 75, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_Desc = CreateWindowW(L"EDIT", g_tempTask.description.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            115, 75, 260, 22, hwnd, (HMENU)IDC_ED_DESC, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"Notes:", WS_VISIBLE | WS_CHILD, 15, 105, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_Notes = CreateWindowW(L"EDIT", g_tempTask.notes.c_str(),
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
            115, 105, 260, 80, hwnd, (HMENU)IDC_ED_NOTES, nullptr, nullptr);

        // DTS_SHOWNONE gives a built-in checkbox so the date can be left blank -
        // tasks only need context, not a date (per the brainstorming notes).
        CreateWindowW(L"STATIC", L"Date:", WS_VISIBLE | WS_CHILD, 15, 200, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_Date = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP | DTS_SHORTDATEFORMAT | DTS_SHOWNONE,
            115, 197, 260, 24, hwnd, (HMENU)IDC_ED_DATE, nullptr, nullptr);
        if (g_tempTask.hasDate) DateTime_SetSystemtime(g_hTE_Date, GDT_VALID, &g_tempTask.date);
        else DateTime_SetSystemtime(g_hTE_Date, GDT_NONE, nullptr);

        CreateWindowW(L"STATIC", L"Colour:", WS_VISIBLE | WS_CHILD, 15, 235, 90, 20, hwnd, nullptr, nullptr, nullptr);
        g_hTE_ColourPreview = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            115, 235, 40, 24, hwnd, (HMENU)IDC_ED_COLOURPREVIEW, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Choose Colour...", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            165, 235, 140, 24, hwnd, (HMENU)IDC_ED_COLOURBTN, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
            115, 275, 80, 28, hwnd, (HMENU)IDOK, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            205, 275, 80, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HWND hCtl = (HWND)lParam;
        if (hCtl == g_hTE_ColourPreview) {
            if (g_hTE_ColourBrush) DeleteObject(g_hTE_ColourBrush);
            g_hTE_ColourBrush = CreateSolidBrush(g_tempTask.colour);
            return (LRESULT)g_hTE_ColourBrush;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ED_COLOURBTN: {
            CHOOSECOLORW cc{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = g_customColours;
            cc.rgbResult = g_tempTask.colour;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColorW(&cc)) {
                g_tempTask.colour = cc.rgbResult;
                InvalidateRect(g_hTE_ColourPreview, nullptr, TRUE);
            }
            break;
        }
        case IDOK: {
            wchar_t buf[512];
            GetWindowTextW(g_hTE_Name, buf, 512);
            if (wcslen(buf) == 0) {
                MessageBoxW(hwnd, L"Name is required.", L"Missing information", MB_OK | MB_ICONWARNING);
                break;
            }
            g_tempTask.name = buf;
            GetWindowTextW(g_hTE_Subject, buf, 512); g_tempTask.subject = buf;
            GetWindowTextW(g_hTE_Desc, buf, 512); g_tempTask.description = buf;
            wchar_t notesBuf[4096];
            GetWindowTextW(g_hTE_Notes, notesBuf, 4096); g_tempTask.notes = notesBuf;
            SYSTEMTIME st;
            DWORD dres = DateTime_GetSystemtime(g_hTE_Date, &st);
            g_tempTask.hasDate = (dres == GDT_VALID);
            if (g_tempTask.hasDate) g_tempTask.date = st;
            g_taskEditorOK = true;
            DestroyWindow(hwnd);
            break;
        }
        case IDCANCEL:
            g_taskEditorOK = false;
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_CLOSE:
        g_taskEditorOK = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_hTE_ColourBrush) { DeleteObject(g_hTE_ColourBrush); g_hTE_ColourBrush = nullptr; }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool RunTaskEditor(HWND owner, Task& t, bool isNew) {
    g_tempTask = t;
    g_taskEditorOK = false;
    EnableWindow(owner, FALSE);
    HWND hEditor = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TaskEditorClass",
        isNew ? L"New Task" : L"Edit Task",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 360,
        owner, nullptr, GetModuleHandle(nullptr), nullptr);
    ShowWindow(hEditor, SW_SHOW);
    UpdateWindow(hEditor);
    MSG msg;
    while (IsWindow(hEditor) && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hEditor, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (g_taskEditorOK) t = g_tempTask;
    return g_taskEditorOK;
}

// ---------------------------------------------------------------------------
// Reminder notification popup (topmost, non-modal, snooze / dismiss)
// ---------------------------------------------------------------------------
static LRESULT CALLBACK NotifyPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        int id = (int)(LONG_PTR)((CREATESTRUCTW*)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)id);
        std::wstring text = L"(reminder was removed)";
        for (auto& r : g_reminders) {
            if (r.id == id) {
                text = r.name;
                if (!r.subject.empty()) text += L"  [" + r.subject + L"]";
                if (!r.description.empty()) text += L"\r\n" + r.description;
                break;
            }
        }
        CreateWindowW(L"STATIC", text.c_str(), WS_VISIBLE | WS_CHILD, 10, 10, 290, 70, hwnd, (HMENU)IDC_POPUP_TEXT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Snooze 10 min", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 10, 95, 140, 28, hwnd, (HMENU)IDC_POPUP_SNOOZE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Dismiss", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 160, 95, 140, 28, hwnd, (HMENU)IDC_POPUP_DISMISS, nullptr, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        int id = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        switch (LOWORD(wParam)) {
        case IDC_POPUP_SNOOZE:
            for (auto& r : g_reminders) {
                if (r.id == id) { r.date = AddMinutes(r.date, 10); break; }
            }
            RemoveOpenPopup(id);
            SaveReminders();
            RefreshReminderList();
            DestroyWindow(hwnd);
            break;
        case IDC_POPUP_DISMISS:
            g_reminders.erase(std::remove_if(g_reminders.begin(), g_reminders.end(),
                [id](const Reminder& r) { return r.id == id; }), g_reminders.end());
            RemoveOpenPopup(id);
            SaveReminders();
            RefreshReminderList();
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        RemoveOpenPopup((int)GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowNotificationPopup(int reminderId) {
    HWND hPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"NotifyPopupClass", L"Reminder",
        WS_POPUP | WS_CAPTION | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 160,
        nullptr, nullptr, GetModuleHandle(nullptr), (LPVOID)(LONG_PTR)reminderId);
    RECT wa{ 0, 0, 1024, 768 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    SetWindowPos(hPopup, HWND_TOPMOST, wa.right - 330, wa.bottom - 180, 0, 0, SWP_NOSIZE);
    ShowWindow(hPopup, SW_SHOWNOACTIVATE);
}

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------
static void ShowPanel(int tabIndex) {
    int remShow = (tabIndex == 0) ? SW_SHOW : SW_HIDE;
    int taskShow = (tabIndex == 1) ? SW_SHOW : SW_HIDE;
    HWND remCtrls[] = { g_hReminderList, g_hReminderFilter, g_hReminderFilterLbl, g_hChkShowPast,
                        g_hRemBtnNew, g_hRemBtnEdit, g_hRemBtnDelete };
    for (HWND h : remCtrls) ShowWindow(h, remShow);
    HWND taskCtrls[] = { g_hTaskList, g_hTaskFilter, g_hTaskFilterLbl, g_hChkShowBin,
                         g_hTaskBtnNew, g_hTaskBtnEdit, g_hTaskBtnDelete,
                         g_hTaskBtnMoveUp, g_hTaskBtnMoveDown, g_hTaskBtnRestore };
    for (HWND h : taskCtrls) ShowWindow(h, taskShow);
    if (taskShow == SW_SHOW) {
        ShowWindow(g_hTaskBtnRestore, g_showTaskBin ? SW_SHOW : SW_HIDE);
        ShowWindow(g_hTaskBtnMoveUp, g_showTaskBin ? SW_HIDE : SW_SHOW);
        ShowWindow(g_hTaskBtnMoveDown, g_showTaskBin ? SW_HIDE : SW_SHOW);
    }
}

static void CreateMainControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_VISIBLE | WS_CHILD,
        10, 10, 760, 540, hwnd, (HMENU)IDC_TAB, hInst, nullptr);
    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"Reminders"; TabCtrl_InsertItem(g_hTab, 0, &tie);
    tie.pszText = (LPWSTR)L"Tasks"; TabCtrl_InsertItem(g_hTab, 1, &tie);

    // --- Reminders panel ---
    g_hReminderFilterLbl = CreateWindowW(L"STATIC", L"Filter:", WS_CHILD, 25, 45, 40, 20, hwnd, nullptr, hInst, nullptr);
    g_hReminderFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
        70, 43, 180, 22, hwnd, (HMENU)IDC_REM_FILTER, hInst, nullptr);
    g_hChkShowPast = CreateWindowW(L"BUTTON", L"Show past reminders", WS_CHILD | BS_AUTOCHECKBOX,
        260, 45, 180, 20, hwnd, (HMENU)IDC_REM_CHK_PAST, hInst, nullptr);

    g_hReminderList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | WS_TABSTOP,
        25, 75, 720, 370, hwnd, (HMENU)IDC_REM_LIST, hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hReminderList, LVS_EX_FULLROWSELECT);
    {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 150; col.pszText = (LPWSTR)L"Name"; ListView_InsertColumn(g_hReminderList, 0, &col);
        col.cx = 120; col.pszText = (LPWSTR)L"Subject"; ListView_InsertColumn(g_hReminderList, 1, &col);
        col.cx = 120; col.pszText = (LPWSTR)L"Date"; ListView_InsertColumn(g_hReminderList, 2, &col);
        col.cx = 320; col.pszText = (LPWSTR)L"Description"; ListView_InsertColumn(g_hReminderList, 3, &col);
    }

    g_hRemBtnNew = CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_TABSTOP, 25, 455, 90, 28, hwnd, (HMENU)IDC_REM_BTN_NEW, hInst, nullptr);
    g_hRemBtnEdit = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_TABSTOP, 125, 455, 90, 28, hwnd, (HMENU)IDC_REM_BTN_EDIT, hInst, nullptr);
    g_hRemBtnDelete = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_TABSTOP, 225, 455, 90, 28, hwnd, (HMENU)IDC_REM_BTN_DELETE, hInst, nullptr);

    // --- Tasks panel ---
    g_hTaskFilterLbl = CreateWindowW(L"STATIC", L"Filter:", WS_CHILD, 25, 45, 40, 20, hwnd, nullptr, hInst, nullptr);
    g_hTaskFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
        70, 43, 180, 22, hwnd, (HMENU)IDC_TASK_FILTER, hInst, nullptr);
    g_hChkShowBin = CreateWindowW(L"BUTTON", L"Show recycle bin", WS_CHILD | BS_AUTOCHECKBOX,
        260, 45, 180, 20, hwnd, (HMENU)IDC_TASK_CHK_BIN, hInst, nullptr);

    g_hTaskList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | WS_TABSTOP,
        25, 75, 720, 340, hwnd, (HMENU)IDC_TASK_LIST, hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hTaskList, LVS_EX_FULLROWSELECT);
    {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 150; col.pszText = (LPWSTR)L"Name"; ListView_InsertColumn(g_hTaskList, 0, &col);
        col.cx = 120; col.pszText = (LPWSTR)L"Subject"; ListView_InsertColumn(g_hTaskList, 1, &col);
        col.cx = 100; col.pszText = (LPWSTR)L"Date"; ListView_InsertColumn(g_hTaskList, 2, &col);
        col.cx = 340; col.pszText = (LPWSTR)L"Description"; ListView_InsertColumn(g_hTaskList, 3, &col);
    }

    g_hTaskBtnNew = CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_TABSTOP, 25, 425, 80, 28, hwnd, (HMENU)IDC_TASK_BTN_NEW, hInst, nullptr);
    g_hTaskBtnEdit = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_TABSTOP, 115, 425, 80, 28, hwnd, (HMENU)IDC_TASK_BTN_EDIT, hInst, nullptr);
    g_hTaskBtnDelete = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_TABSTOP, 205, 425, 110, 28, hwnd, (HMENU)IDC_TASK_BTN_DELETE, hInst, nullptr);
    g_hTaskBtnMoveUp = CreateWindowW(L"BUTTON", L"Move Up", WS_CHILD | WS_TABSTOP, 325, 425, 90, 28, hwnd, (HMENU)IDC_TASK_BTN_MOVEUP, hInst, nullptr);
    g_hTaskBtnMoveDown = CreateWindowW(L"BUTTON", L"Move Down", WS_CHILD | WS_TABSTOP, 425, 425, 90, 28, hwnd, (HMENU)IDC_TASK_BTN_MOVEDOWN, hInst, nullptr);
    g_hTaskBtnRestore = CreateWindowW(L"BUTTON", L"Restore", WS_CHILD | WS_TABSTOP, 525, 425, 90, 28, hwnd, (HMENU)IDC_TASK_BTN_RESTORE, hInst, nullptr);

    ShowPanel(0);
    TabCtrl_SetCurSel(g_hTab, 0);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        LoadReminders();
        LoadTasks();
        LoadTaskBin();
        CreateMainControls(hwnd);
        RefreshReminderList();
        RefreshTaskList();
        SetTimer(hwnd, IDT_NOTIFY_CHECK, 15000, nullptr);
        return 0;

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->hwndFrom == g_hTab && pnmh->code == TCN_SELCHANGE) {
            ShowPanel(TabCtrl_GetCurSel(g_hTab));
            return 0;
        }
        if (pnmh->code == NM_CUSTOMDRAW && (pnmh->hwndFrom == g_hReminderList || pnmh->hwndFrom == g_hTaskList)) {
            LPNMLVCUSTOMDRAW lvcd = (LPNMLVCUSTOMDRAW)lParam;
            if (lvcd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (lvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                int id = (int)lvcd->nmcd.lItemlParam;
                COLORREF col = RGB(0, 0, 0);
                if (pnmh->hwndFrom == g_hReminderList) {
                    for (auto& r : g_reminders) if (r.id == id) { col = r.colour; break; }
                } else {
                    std::vector<Task>& v = g_showTaskBin ? g_taskBin : g_tasks;
                    for (auto& t : v) if (t.id == id) { col = t.colour; break; }
                }
                lvcd->clrText = col;
                return CDRF_NEWFONT;
            }
        }
        break;
    }

    case WM_TIMER:
        if (wParam == IDT_NOTIFY_CHECK) {
            SYSTEMTIME now; GetLocalTime(&now);
            long long nowC = ToComparable(now);
            for (auto& r : g_reminders) {
                if (ToComparable(r.date) <= nowC &&
                    std::find(g_openPopups.begin(), g_openPopups.end(), r.id) == g_openPopups.end()) {
                    g_openPopups.push_back(r.id);
                    ShowNotificationPopup(r.id);
                }
            }
        }
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        switch (id) {
        case IDC_REM_FILTER:
            if (code == EN_CHANGE) RefreshReminderList();
            return 0;
        case IDC_REM_CHK_PAST:
            if (code == BN_CLICKED) {
                g_showPastReminders = (IsDlgButtonChecked(hwnd, IDC_REM_CHK_PAST) == BST_CHECKED);
                RefreshReminderList();
            }
            return 0;
        case IDC_REM_BTN_NEW: {
            Reminder r{};
            r.id = g_nextReminderId++;
            r.colour = RGB(0, 0, 0);
            GetLocalTime(&r.date);
            if (RunReminderEditor(hwnd, r, true)) {
                g_reminders.push_back(r);
                SaveReminders();
                RefreshReminderList();
            } else {
                g_nextReminderId--;
            }
            return 0;
        }
        case IDC_REM_BTN_EDIT: {
            int rid = GetSelectedId(g_hReminderList);
            if (rid < 0) return 0;
            for (auto& r : g_reminders) {
                if (r.id == rid) {
                    if (RunReminderEditor(hwnd, r, false)) {
                        SaveReminders();
                        RefreshReminderList();
                    }
                    break;
                }
            }
            return 0;
        }
        case IDC_REM_BTN_DELETE: {
            int rid = GetSelectedId(g_hReminderList);
            if (rid < 0) return 0;
            if (MessageBoxW(hwnd, L"Delete this reminder?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                g_reminders.erase(std::remove_if(g_reminders.begin(), g_reminders.end(),
                    [rid](const Reminder& r) { return r.id == rid; }), g_reminders.end());
                SaveReminders();
                RefreshReminderList();
            }
            return 0;
        }

        case IDC_TASK_FILTER:
            if (code == EN_CHANGE) RefreshTaskList();
            return 0;
        case IDC_TASK_CHK_BIN:
            if (code == BN_CLICKED) {
                g_showTaskBin = (IsDlgButtonChecked(hwnd, IDC_TASK_CHK_BIN) == BST_CHECKED);
                SetWindowTextW(g_hTaskBtnDelete, g_showTaskBin ? L"Delete Permanently" : L"Delete");
                ShowWindow(g_hTaskBtnRestore, g_showTaskBin ? SW_SHOW : SW_HIDE);
                ShowWindow(g_hTaskBtnMoveUp, g_showTaskBin ? SW_HIDE : SW_SHOW);
                ShowWindow(g_hTaskBtnMoveDown, g_showTaskBin ? SW_HIDE : SW_SHOW);
                RefreshTaskList();
            }
            return 0;
        case IDC_TASK_BTN_NEW: {
            Task t{};
            t.id = g_nextTaskId++;
            t.colour = RGB(0, 0, 0);
            t.hasDate = false;
            GetLocalTime(&t.date);
            if (RunTaskEditor(hwnd, t, true)) {
                g_tasks.push_back(t);
                SaveTasks();
                RefreshTaskList();
            } else {
                g_nextTaskId--;
            }
            return 0;
        }
        case IDC_TASK_BTN_EDIT: {
            if (g_showTaskBin) {
                MessageBoxW(hwnd, L"Restore the task before editing.", L"Recycle Bin", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            int tid = GetSelectedId(g_hTaskList);
            if (tid < 0) return 0;
            for (auto& t : g_tasks) {
                if (t.id == tid) {
                    if (RunTaskEditor(hwnd, t, false)) {
                        SaveTasks();
                        RefreshTaskList();
                    }
                    break;
                }
            }
            return 0;
        }
        case IDC_TASK_BTN_DELETE: {
            int tid = GetSelectedId(g_hTaskList);
            if (tid < 0) return 0;
            if (!g_showTaskBin) {
                auto it = std::find_if(g_tasks.begin(), g_tasks.end(), [tid](const Task& t) { return t.id == tid; });
                if (it != g_tasks.end()) {
                    g_taskBin.push_back(*it);
                    g_tasks.erase(it);
                    SaveTasks(); SaveTaskBin();
                    RefreshTaskList();
                }
            } else {
                if (MessageBoxW(hwnd, L"Permanently delete this task? This cannot be undone.", L"Confirm", MB_YESNO | MB_ICONWARNING) == IDYES) {
                    g_taskBin.erase(std::remove_if(g_taskBin.begin(), g_taskBin.end(),
                        [tid](const Task& t) { return t.id == tid; }), g_taskBin.end());
                    SaveTaskBin();
                    RefreshTaskList();
                }
            }
            return 0;
        }
        case IDC_TASK_BTN_MOVEUP: {
            if (g_showTaskBin) return 0;
            int tid = GetSelectedId(g_hTaskList);
            for (size_t i = 1; i < g_tasks.size(); ++i) {
                if (g_tasks[i].id == tid) { std::swap(g_tasks[i], g_tasks[i - 1]); break; }
            }
            SaveTasks();
            RefreshTaskList();
            return 0;
        }
        case IDC_TASK_BTN_MOVEDOWN: {
            if (g_showTaskBin) return 0;
            int tid = GetSelectedId(g_hTaskList);
            for (size_t i = 0; i + 1 < g_tasks.size(); ++i) {
                if (g_tasks[i].id == tid) { std::swap(g_tasks[i], g_tasks[i + 1]); break; }
            }
            SaveTasks();
            RefreshTaskList();
            return 0;
        }
        case IDC_TASK_BTN_RESTORE: {
            if (!g_showTaskBin) return 0;
            int tid = GetSelectedId(g_hTaskList);
            auto it = std::find_if(g_taskBin.begin(), g_taskBin.end(), [tid](const Task& t) { return t.id == tid; });
            if (it != g_taskBin.end()) {
                g_tasks.push_back(*it);
                g_taskBin.erase(it);
                SaveTasks(); SaveTaskBin();
                RefreshTaskList();
            }
            return 0;
        }
        }
        break;
    }

    case WM_DESTROY:
        KillTimer(hwnd, IDT_NOTIFY_CHECK);
        SaveReminders();
        SaveTasks();
        SaveTaskBin();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    wc.lpfnWndProc = MainWndProc;
    wc.lpszClassName = L"ReminderTaskMainClass";
    RegisterClassW(&wc);

    wc.lpfnWndProc = ReminderEditorProc;
    wc.lpszClassName = L"ReminderEditorClass";
    RegisterClassW(&wc);

    wc.lpfnWndProc = TaskEditorProc;
    wc.lpszClassName = L"TaskEditorClass";
    RegisterClassW(&wc);

    wc.lpfnWndProc = NotifyPopupProc;
    wc.lpszClassName = L"NotifyPopupClass";
    wc.hbrBackground = CreateSolidBrush(RGB(255, 250, 205));
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"ReminderTaskMainClass", L"Reminder & Task Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 640,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
