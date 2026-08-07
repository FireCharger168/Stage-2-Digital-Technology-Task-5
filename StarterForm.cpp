// StarterForm.cpp
// Entry point. The original prototype's window/listview/timer code has grown
// into MainWindow.cpp, EntryDialog.cpp and NotificationPopup.cpp - this file
// just wires the app together and runs the message loop.
#include "Common.h"
#include "MainWindow.h"
#include "EntryDialog.h"
#include "NotificationPopup.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icex = { sizeof(icex),
        ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    RegisterMainWindowClass(hInstance);
    RegisterEntryDialogClass(hInstance);
    RegisterNotificationPopupClass(hInstance);

    HWND hwnd = CreateMainWindow(hInstance);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
