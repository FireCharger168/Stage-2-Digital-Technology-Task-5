// StarterForm.cpp
// Simple Win32 GUI starter form for the overlay/notification app.
// Pure Win32 API, no external libraries required.
//
// Compile (MinGW):
//   g++ StarterForm.cpp -o StarterForm.exe -mwindows
// Compile (MSVC, Developer Command Prompt):
//   cl StarterForm.cpp /link user32.lib gdi32.lib

#include <windows.h>

#define ID_BUTTON_START 101
#define ID_LABEL_STATUS 102

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"StarterFormWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Overlay App - Starter Form",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hStatusLabel;

    switch (msg) {
        case WM_CREATE: {
            CreateWindow(
                L"STATIC", L"Overlay engine not running.",
                WS_VISIBLE | WS_CHILD,
                20, 20, 340, 20,
                hwnd, (HMENU)ID_LABEL_STATUS, nullptr, nullptr
            );

            CreateWindow(
                L"BUTTON", L"Start Overlay",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 60, 150, 30,
                hwnd, (HMENU)ID_BUTTON_START, nullptr, nullptr
            );

            hStatusLabel = GetDlgItem(hwnd, ID_LABEL_STATUS);
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BUTTON_START) {
                // Placeholder: this is where you'd launch/attach the
                // C++ overlay hook DLL (see 01 - Overlay Engine.md).
                SetWindowText(hStatusLabel, L"Overlay started (placeholder).");
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
