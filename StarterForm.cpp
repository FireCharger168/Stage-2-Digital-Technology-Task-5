// StarterForm.cpp
// Minimal Win32 window with a single button.
// Compile (MinGW): g++ StarterForm.cpp -o StarterForm.exe -mwindows

#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_COMMAND) {
        // Button clicked (placeholder action)
        MessageBox(hwnd, L"Overlay started (placeholder).", L"Status", MB_OK);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"StarterFormWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        CLASS_NAME, L"Overlay App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) return 0;

    CreateWindow(
        L"BUTTON", L"Start Overlay",
        WS_VISIBLE | WS_CHILD,
        20, 60, 150, 30,
        hwnd, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
