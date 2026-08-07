// NotificationPopup.cpp
#include "NotificationPopup.h"
#include "Reminder.h"

namespace {

const wchar_t* kClassName = L"ReminderPopupClass";
const int POPUP_W = 300;
const int POPUP_H = 130;
const int POPUP_MARGIN = 12;

struct PopupCtrls {
    int reminderId = 0;
    HWND mainWnd = nullptr;
    HWND hTitle = nullptr, hBody = nullptr, hSnooze = nullptr, hDismiss = nullptr;
};

std::vector<HWND> g_openPopups;

HFONT BoldFont() {
    static HFONT f = nullptr;
    if (!f) {
        LOGFONT lf{};
        GetObject((HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        f = CreateFontIndirect(&lf);
    }
    return f;
}

void RepositionPopups() {
    RECT work{};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.right - POPUP_W - POPUP_MARGIN;
    int y = work.bottom - POPUP_H - POPUP_MARGIN;
    for (HWND h : g_openPopups) {
        SetWindowPos(h, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        y -= (POPUP_H + POPUP_MARGIN);
    }
}

LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    PopupCtrls* pc = (PopupCtrls*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = ((CREATESTRUCT*)lParam)->hInstance;
        Reminder* r = FindReminderById(pc->reminderId);
        std::wstring title = r ? r->name : L"Reminder";
        std::wstring body;
        if (r) {
            if (!r->subject.empty()) body += r->subject + L"\r\n";
            body += r->description;
        }
        pc->hTitle = CreateWindowEx(0, L"STATIC", title.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
            12, 10, POPUP_W - 24, 20, hwnd, nullptr, hInst, nullptr);
        SendMessage(pc->hTitle, WM_SETFONT, (WPARAM)BoldFont(), TRUE);
        pc->hBody = CreateWindowEx(0, L"STATIC", body.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
            12, 34, POPUP_W - 24, 54, hwnd, nullptr, hInst, nullptr);
        pc->hSnooze = CreateWindowEx(0, L"BUTTON", L"Snooze 10 min", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12, 92, 130, 26, hwnd, (HMENU)(INT_PTR)IDC_NP_SNOOZE, hInst, nullptr);
        pc->hDismiss = CreateWindowEx(0, L"BUTTON", L"Dismiss", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            158, 92, 130, 26, hwnd, (HMENU)(INT_PTR)IDC_NP_DISMISS, hInst, nullptr);
        return 0;
    }

    case WM_COMMAND: {
        if (!pc) break;
        int id = LOWORD(wParam);
        if (id == IDC_NP_SNOOZE && HIWORD(wParam) == BN_CLICKED) {
            Reminder* r = FindReminderById(pc->reminderId);
            if (r) {
                ULONGLONG now = SystemTimeToTicks(NowLocalSystemTime());
                r->snoozedUntil = now + 10ULL * TICKS_PER_MINUTE;
            }
            HWND mainWnd = pc->mainWnd;
            DestroyWindow(hwnd);
            PostMessage(mainWnd, WM_APP_DATA_CHANGED, 0, 0);
            return 0;
        }
        if (id == IDC_NP_DISMISS && HIWORD(wParam) == BN_CLICKED) {
            Reminder* r = FindReminderById(pc->reminderId);
            if (r) r->fired = true;
            HWND mainWnd = pc->mainWnd;
            DestroyWindow(hwnd);
            PostMessage(mainWnd, WM_APP_DATA_CHANGED, 0, 0);
            return 0;
        }
        return 0;
    }

    case WM_NCDESTROY: {
        for (size_t i = 0; i < g_openPopups.size(); i++) {
            if (g_openPopups[i] == hwnd) {
                g_openPopups.erase(g_openPopups.begin() + i);
                break;
            }
        }
        delete pc;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        RepositionPopups();
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

void RegisterNotificationPopupClass(HINSTANCE hInst) {
    WNDCLASS wc{};
    wc.lpfnWndProc = PopupProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
}

bool IsNotificationOpenFor(int reminderId) {
    for (HWND h : g_openPopups) {
        PopupCtrls* pc = (PopupCtrls*)GetWindowLongPtr(h, GWLP_USERDATA);
        if (pc && pc->reminderId == reminderId) return true;
    }
    return false;
}

void ShowReminderNotification(HWND mainWnd, HINSTANCE hInst, int reminderId) {
    if (IsNotificationOpenFor(reminderId)) return;

    PopupCtrls* pc = new PopupCtrls();
    pc->reminderId = reminderId;
    pc->mainWnd = mainWnd;

    HWND h = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClassName, L"Reminder",
        WS_POPUP | WS_BORDER,
        0, 0, POPUP_W, POPUP_H, mainWnd, nullptr, hInst, pc);
    if (!h) {
        delete pc;
        return;
    }

    g_openPopups.push_back(h);
    RepositionPopups();
    ShowWindow(h, SW_SHOWNOACTIVATE);
}
