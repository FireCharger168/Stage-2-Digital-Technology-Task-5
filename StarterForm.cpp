// StarterForm.cpp

// ===== Includes =====
#include <windows.h>
#include <cstdio>

// ===== Constants =====
#define MAX_REMINDERS 20
#define ID_EDIT_TEXT    101
#define ID_EDIT_MINUTES 102
#define ID_BUTTON_ADD   103
#define ID_TIMER_CHECK  1

// Types
struct Reminder {
    wchar_t text[256];
    ULONGLONG dueTimeMs;
    bool fired; // False if it has poped up
};

// Global variables
Reminder g_reminders[MAX_REMINDERS];
int g_reminderCount = 0;

HWND hEditText, hEditMinutes;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { // Don't touch this it holds the window together
    switch (msg) {

        // Create window
    case WM_CREATE:
        CreateWindow(L"STATIC", L"Reminder text:", WS_VISIBLE | WS_CHILD,
            20, 20, 120, 20, hwnd, nullptr, nullptr, nullptr);
        hEditText = CreateWindow(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            150, 20, 300, 24, hwnd, (HMENU)ID_EDIT_TEXT, nullptr, nullptr);

        CreateWindow(L"STATIC", L"Minutes from now:", WS_VISIBLE | WS_CHILD,
            20, 60, 120, 20, hwnd, nullptr, nullptr, nullptr);
        hEditMinutes = CreateWindow(L"EDIT", L"1", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            150, 60, 60, 24, hwnd, (HMENU)ID_EDIT_MINUTES, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Add Reminder", WS_VISIBLE | WS_CHILD,
            20, 100, 150, 30, hwnd, (HMENU)ID_BUTTON_ADD, nullptr, nullptr);

        // Start a timer to check for reminders
        SetTimer(hwnd, ID_TIMER_CHECK, 1000, nullptr);
        return 0;

        // Button clicked: read inputs and store as a new reminder
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_ADD && HIWORD(wParam) == BN_CLICKED) {
            wchar_t textBuf[256] = {};
            wchar_t minutesBuf[16] = {};
            GetWindowText(hEditText, textBuf, 256);
            GetWindowText(hEditMinutes, minutesBuf, 16);

            // Check input
            if (wcslen(textBuf) == 0) {
                MessageBox(hwnd, L"Please enter reminder text.", L"Missing text", MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (g_reminderCount >= MAX_REMINDERS) {
                MessageBox(hwnd, L"Too many reminders.", L"Limit reached", MB_OK | MB_ICONWARNING);
                return 0;
            }

            int minutes = _wtoi(minutesBuf);
            if (minutes <= 0) minutes = 1;

            // Store reminder
            Reminder& r = g_reminders[g_reminderCount++];
            wcscpy_s(r.text, textBuf);
            r.dueTimeMs = GetTickCount64() + (ULONGLONG)minutes * 60 * 1000;
            r.fired = false;

            SetWindowText(hEditText, L"");
            MessageBox(hwnd, L"Reminder added.", L"Status", MB_OK);
        }
        return 0;

        // Timer tick: check if any reminder is due, then pop a notification
    case WM_TIMER:
        if (wParam == ID_TIMER_CHECK) {
            ULONGLONG now = GetTickCount64();
            for (int i = 0; i < g_reminderCount; i++) {
                Reminder& r = g_reminders[i];
                if (!r.fired && now >= r.dueTimeMs) {
                    r.fired = true;
                    MessageBox(hwnd, r.text, L"Reminder", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                }
            }
        }
        return 0;

        // --- Window closing: clean up and exit ---
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_CHECK);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===== Entry point =====
int WINAPI WinMain(HINSTANCE  hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"StarterFormWindowClass";

    // --- Register window class ---
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // --- Create the main window ---
    HWND hwnd = CreateWindow(
        CLASS_NAME, L"Reminder App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 220,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    // --- Message loop ---
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}