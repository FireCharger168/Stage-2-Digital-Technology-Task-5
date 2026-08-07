// EntryDialog.cpp
#include "EntryDialog.h"
#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

namespace {

const wchar_t* kClassName = L"EntryDialogClass";

const int MARGIN = 16;
const int LABEL_W = 90;
const int FIELD_X = MARGIN + LABEL_W;
const int DLG_W = 430;
const int FIELD_W = DLG_W - FIELD_X - MARGIN - 16;
const int ROW_H = 24;
const int ROW_GAP = 10;
const int NOTES_H = 64;

struct EntryState {
    EntryKind kind = EntryKind::Reminder;
    bool done = false;
    EntryResult result;

    HWND hName = nullptr, hDesc = nullptr, hSubject = nullptr, hNotes = nullptr;
    HWND hDate = nullptr, hTime = nullptr, hColourBtn = nullptr;
    HWND hOk = nullptr, hCancel = nullptr;
};

HWND MakeLabel(HWND parent, HINSTANCE hInst, const wchar_t* text, int x, int y, int w) {
    return CreateWindowEx(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, ROW_H, parent, nullptr, hInst, nullptr);
}

HWND MakeEdit(HWND parent, HINSTANCE hInst, int id, int x, int y, int w, int h, DWORD extraStyle) {
    return CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | extraStyle,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

void LayoutAndCreateControls(HWND hwnd, HINSTANCE hInst, EntryState* st) {
    int y = MARGIN;

    MakeLabel(hwnd, hInst, L"Name:*", MARGIN, y, LABEL_W);
    st->hName = MakeEdit(hwnd, hInst, IDC_ED_NAME, FIELD_X, y, FIELD_W, ROW_H, 0);
    y += ROW_H + ROW_GAP;

    MakeLabel(hwnd, hInst, L"Description:", MARGIN, y, LABEL_W);
    st->hDesc = MakeEdit(hwnd, hInst, IDC_ED_DESC, FIELD_X, y, FIELD_W, ROW_H, 0);
    y += ROW_H + ROW_GAP;

    MakeLabel(hwnd, hInst, L"Subject:", MARGIN, y, LABEL_W);
    st->hSubject = MakeEdit(hwnd, hInst, IDC_ED_SUBJECT, FIELD_X, y, FIELD_W, ROW_H, 0);
    y += ROW_H + ROW_GAP;

    MakeLabel(hwnd, hInst, L"Notes:", MARGIN, y, LABEL_W);
    st->hNotes = MakeEdit(hwnd, hInst, IDC_ED_NOTES, FIELD_X, y, FIELD_W, NOTES_H,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL);
    y += NOTES_H + ROW_GAP;

    const wchar_t* dateLabel = (st->kind == EntryKind::Reminder) ? L"Date/Time:*" : L"Date:";
    MakeLabel(hwnd, hInst, dateLabel, MARGIN, y, LABEL_W);
    DWORD dtpStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT;
    if (st->kind == EntryKind::Task) dtpStyle |= DTS_SHOWNONE;
    int dateW = (st->kind == EntryKind::Reminder) ? (FIELD_W - 130) : FIELD_W;
    st->hDate = CreateWindowEx(0, DATETIMEPICK_CLASS, L"", dtpStyle,
        FIELD_X, y, dateW, ROW_H, hwnd, (HMENU)(INT_PTR)IDC_ED_DATE, hInst, nullptr);
    if (st->kind == EntryKind::Reminder) {
        st->hTime = CreateWindowEx(0, DATETIMEPICK_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_TIMEFORMAT,
            FIELD_X + dateW + 8, y, 118, ROW_H, hwnd, (HMENU)(INT_PTR)IDC_ED_TIME, hInst, nullptr);
    }
    y += ROW_H + ROW_GAP;

    MakeLabel(hwnd, hInst, L"Colour:*", MARGIN, y, LABEL_W);
    st->hColourBtn = CreateWindowEx(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        FIELD_X, y, 160, ROW_H + 4, hwnd, (HMENU)(INT_PTR)IDC_ED_COLOURBTN, hInst, nullptr);
    y += ROW_H + 4 + ROW_GAP + 6;

    st->hOk = CreateWindowEx(0, L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        DLG_W - MARGIN - 180, y, 80, 28, hwnd, (HMENU)(INT_PTR)IDC_ED_OK, hInst, nullptr);
    st->hCancel = CreateWindowEx(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        DLG_W - MARGIN - 90, y, 90, 28, hwnd, (HMENU)(INT_PTR)IDC_ED_CANCEL, hInst, nullptr);
    y += 28 + MARGIN;

    RECT client{ 0, 0, DLG_W, y };
    AdjustWindowRectEx(&client, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
    SetWindowPos(hwnd, nullptr, 0, 0, client.right - client.left, client.bottom - client.top,
        SWP_NOMOVE | SWP_NOZORDER);
}

LRESULT CALLBACK EntryDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    EntryState* st = (EntryState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        LayoutAndCreateControls(hwnd, cs->hInstance, st);
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == IDC_ED_COLOURBTN && st) {
            COLORREF c = st->result.colour;
            HBRUSH hb = CreateSolidBrush(c);
            FillRect(dis->hDC, &dis->rcItem, hb);
            DeleteObject(hb);
            FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
            if (dis->itemState & ODS_SELECTED) {
                RECT inner = dis->rcItem;
                InflateRect(&inner, -2, -2);
                FrameRect(dis->hDC, &inner, (HBRUSH)GetStockObject(GRAY_BRUSH));
            }
            int luminance = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
            SetTextColor(dis->hDC, luminance > 128 ? RGB(0, 0, 0) : RGB(255, 255, 255));
            SetBkMode(dis->hDC, TRANSPARENT);
            DrawText(dis->hDC, L"Choose Colour...", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!st) break;

        if (id == IDC_ED_COLOURBTN && HIWORD(wParam) == BN_CLICKED) {
            static COLORREF customColours[16] = {};
            CHOOSECOLOR cc{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = customColours;
            cc.rgbResult = st->result.colour;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) {
                st->result.colour = cc.rgbResult;
                InvalidateRect(st->hColourBtn, nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_ED_OK && HIWORD(wParam) == BN_CLICKED) {
            std::wstring name = GetWindowTextStr(st->hName);
            if (name.empty()) {
                MessageBox(hwnd, L"Please enter a name.", L"Missing name", MB_OK | MB_ICONWARNING);
                SetFocus(st->hName);
                return 0;
            }

            st->result.name = name;
            st->result.description = GetWindowTextStr(st->hDesc);
            st->result.subject = GetWindowTextStr(st->hSubject);
            st->result.notes = GetWindowTextStr(st->hNotes);

            SYSTEMTIME datePart{};
            DateTime_GetSystemtime(st->hDate, &datePart);
            if (st->kind == EntryKind::Reminder) {
                SYSTEMTIME timePart{};
                DateTime_GetSystemtime(st->hTime, &timePart);
                datePart.wHour = timePart.wHour;
                datePart.wMinute = timePart.wMinute;
                datePart.wSecond = 0;
                datePart.wMilliseconds = 0;
                st->result.hasDate = true;
            } else {
                LRESULT gd = DateTime_GetSystemtime(st->hDate, &datePart);
                st->result.hasDate = (gd == GDT_VALID);
            }
            st->result.due = datePart;

            st->result.saved = true;
            st->done = true;
            DestroyWindow(hwnd);
            return 0;
        }

        if (id == IDC_ED_CANCEL && HIWORD(wParam) == BN_CLICKED) {
            st->result.saved = false;
            st->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (st) {
            st->result.saved = false;
            st->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

void RegisterEntryDialogClass(HINSTANCE hInst) {
    WNDCLASS wc{};
    wc.lpfnWndProc = EntryDialogProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
}

EntryResult ShowEntryDialog(HWND owner, EntryKind kind, const wchar_t* title, const EntryResult* prefill) {
    EntryState st;
    st.kind = kind;
    if (prefill) {
        st.result = *prefill;
    } else {
        st.result.colour = DEFAULT_COLOUR;
        st.result.due = NowLocalSystemTime();
        st.result.hasDate = (kind == EntryKind::Reminder);
    }
    st.result.saved = false;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE);
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, kClassName, title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, DLG_W, 400,
        owner, nullptr, hInst, &st);
    if (!hDlg) return st.result;

    SetWindowTextW(st.hName, st.result.name.c_str());
    SetWindowTextW(st.hDesc, st.result.description.c_str());
    SetWindowTextW(st.hSubject, st.result.subject.c_str());
    SetWindowTextW(st.hNotes, st.result.notes.c_str());

    if (kind == EntryKind::Task && !st.result.hasDate) {
        DateTime_SetSystemtime(st.hDate, GDT_NONE, &st.result.due);
    } else {
        DateTime_SetSystemtime(st.hDate, GDT_VALID, &st.result.due);
    }
    if (kind == EntryKind::Reminder) {
        DateTime_SetSystemtime(st.hTime, GDT_VALID, &st.result.due);
    }

    RECT ownerRect{}, dlgRect{};
    GetWindowRect(owner, &ownerRect);
    GetWindowRect(hDlg, &dlgRect);
    int w = dlgRect.right - dlgRect.left;
    int h = dlgRect.bottom - dlgRect.top;
    int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - w) / 2;
    int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - h) / 2;
    SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(owner, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(st.hName);

    MSG msg{};
    while (!st.done && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostMessage(hDlg, WM_CLOSE, 0, 0);
        }
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return st.result;
}
