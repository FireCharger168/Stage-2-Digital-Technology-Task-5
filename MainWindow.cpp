// MainWindow.cpp
// Main window: a tab control switching between a Reminders panel and a
// Tasks panel, each with a filter box, a colour-coded listview, and its
// action buttons.
#include "MainWindow.h"
#include "Reminder.h"
#include "Task.h"
#include "Persistence.h"
#include "EntryDialog.h"
#include "NotificationPopup.h"

namespace {

const wchar_t* kClassName = L"MainAppWindowClass";
const int MARGIN = 10;

HINSTANCE g_hInst = nullptr;
HWND g_hTab = nullptr;

HWND g_hRFilterLabel, g_hRFilter, g_hRList, g_hRAdd, g_hREdit, g_hRDelete, g_hRShowPast;
HWND g_hTFilterLabel, g_hTFilter, g_hTList, g_hTAdd, g_hTEdit, g_hTDelete, g_hTRestore, g_hTMoveUp, g_hTMoveDown, g_hTRecycle;

bool g_showPastReminders = false;
bool g_showRecycleBin = false;

std::vector<int> g_reminderRowIds; // listview row -> Reminder id
std::vector<int> g_taskRowIds;     // listview row -> Task id

// ---- forward declarations (definitions below, order kept readable) ----
void CreateControls(HWND hwnd);
void LayoutControls(HWND hwnd);
void SetupListColumns(HWND hList);
std::wstring SubjectWithNotes(const std::wstring& subject, const std::wstring& notes);
void ApplyTabVisibility();
void UpdateTaskButtonsForMode();
void RefreshReminderList(HWND hwnd);
void RefreshTaskList(HWND hwnd);
void RefreshAll(HWND hwnd);
int GetSelectedReminderId();
int GetSelectedTaskId();
EntryResult ReminderToEntryResult(const Reminder& r);
EntryResult TaskToEntryResult(const Task& t);
void OnReminderAdd(HWND hwnd);
void OnReminderEdit(HWND hwnd);
void OnReminderDelete(HWND hwnd);
void OnTaskAdd(HWND hwnd);
void OnTaskEdit(HWND hwnd);
void OnTaskDeleteOrPurge(HWND hwnd);
void OnTaskRestore(HWND hwnd);
void OnTaskMove(HWND hwnd, bool up);
LRESULT HandleListCustomDraw(NMLVCUSTOMDRAW* cd, bool isReminderList);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---- setup ----

void SetupListColumns(HWND hList) {
    LVCOLUMN lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0; lvc.cx = 170; lvc.pszText = const_cast<LPWSTR>(L"Name");
    ListView_InsertColumn(hList, 0, &lvc);
    lvc.iSubItem = 1; lvc.cx = 200; lvc.pszText = const_cast<LPWSTR>(L"Subject / Notes");
    ListView_InsertColumn(hList, 1, &lvc);
    lvc.iSubItem = 2; lvc.cx = 140; lvc.pszText = const_cast<LPWSTR>(L"Date");
    ListView_InsertColumn(hList, 2, &lvc);
    lvc.iSubItem = 3; lvc.cx = 190; lvc.pszText = const_cast<LPWSTR>(L"Description");
    ListView_InsertColumn(hList, 3, &lvc);

    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
}

// Notes ride along with the subject cell instead of getting a column of
// their own - if there's nothing in Notes, the cell just shows the subject.
std::wstring SubjectWithNotes(const std::wstring& subject, const std::wstring& notes) {
    if (notes.empty()) return subject;
    if (subject.empty()) return notes;
    return subject + L" - " + notes;
}

void CreateControls(HWND hwnd) {
    g_hTab = CreateWindowEx(0, WC_TABCONTROL, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_TAB, g_hInst, nullptr);
    TCITEM tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"Reminders");
    TabCtrl_InsertItem(g_hTab, 0, &tie);
    tie.pszText = const_cast<LPWSTR>(L"Tasks");
    TabCtrl_InsertItem(g_hTab, 1, &tie);

    // ---- Reminders panel (visible by default: tab 0 selected) ----
    g_hRFilterLabel = CreateWindowEx(0, L"STATIC", L"Filter:", WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10, hwnd, nullptr, g_hInst, nullptr);
    g_hRFilter = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_FILTER, g_hInst, nullptr);
    g_hRShowPast = CreateWindowEx(0, L"BUTTON", L"Show past reminders",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_SHOWPAST, g_hInst, nullptr);
    g_hRList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_LIST, g_hInst, nullptr);
    SetupListColumns(g_hRList);
    g_hRAdd = CreateWindowEx(0, L"BUTTON", L"Add...", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_ADD, g_hInst, nullptr);
    g_hREdit = CreateWindowEx(0, L"BUTTON", L"Edit...", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_EDIT, g_hInst, nullptr);
    g_hRDelete = CreateWindowEx(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_R_DELETE, g_hInst, nullptr);

    // ---- Tasks panel (hidden until its tab is selected) ----
    g_hTFilterLabel = CreateWindowEx(0, L"STATIC", L"Filter:", WS_CHILD,
        0, 0, 10, 10, hwnd, nullptr, g_hInst, nullptr);
    g_hTFilter = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_FILTER, g_hInst, nullptr);
    g_hTRecycle = CreateWindowEx(0, L"BUTTON", L"Recycle bin",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_RECYCLE, g_hInst, nullptr);
    g_hTList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_LIST, g_hInst, nullptr);
    SetupListColumns(g_hTList);
    g_hTAdd = CreateWindowEx(0, L"BUTTON", L"Add...", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_ADD, g_hInst, nullptr);
    g_hTEdit = CreateWindowEx(0, L"BUTTON", L"Edit...", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_EDIT, g_hInst, nullptr);
    g_hTDelete = CreateWindowEx(0, L"BUTTON", L"Delete", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_DELETE, g_hInst, nullptr);
    g_hTRestore = CreateWindowEx(0, L"BUTTON", L"Restore", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_RESTORE, g_hInst, nullptr);
    g_hTMoveUp = CreateWindowEx(0, L"BUTTON", L"Move Up", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_MOVEUP, g_hInst, nullptr);
    g_hTMoveDown = CreateWindowEx(0, L"BUTTON", L"Move Down", WS_CHILD | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_T_MOVEDOWN, g_hInst, nullptr);

    UpdateTaskButtonsForMode();
    LayoutControls(hwnd);
}

void LayoutControls(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    MoveWindow(g_hTab, 0, 0, rc.right, rc.bottom, TRUE);

    RECT area = rc;
    TabCtrl_AdjustRect(g_hTab, FALSE, &area);

    int x = area.left + MARGIN;
    int y = area.top + MARGIN;
    int w = area.right - area.left - 2 * MARGIN;
    int bottom = area.bottom - MARGIN;

    const int FILTER_H = 22;
    const int BTN_H = 28;
    const int GAP = 6;

    int listTop = y + FILTER_H + GAP;
    int listH = bottom - listTop - BTN_H - GAP;
    if (listH < 40) listH = 40;
    int by = listTop + listH + GAP;

    // Reminders panel
    MoveWindow(g_hRFilterLabel, x, y, 40, FILTER_H, TRUE);
    MoveWindow(g_hRFilter, x + 44, y, 200, FILTER_H, TRUE);
    MoveWindow(g_hRShowPast, x + 254, y, 170, FILTER_H, TRUE);
    MoveWindow(g_hRList, x, listTop, w, listH, TRUE);
    MoveWindow(g_hRAdd, x, by, 100, BTN_H, TRUE);
    MoveWindow(g_hREdit, x + 106, by, 100, BTN_H, TRUE);
    MoveWindow(g_hRDelete, x + 212, by, 100, BTN_H, TRUE);

    // Tasks panel
    MoveWindow(g_hTFilterLabel, x, y, 40, FILTER_H, TRUE);
    MoveWindow(g_hTFilter, x + 44, y, 200, FILTER_H, TRUE);
    MoveWindow(g_hTRecycle, x + 254, y, 170, FILTER_H, TRUE);
    MoveWindow(g_hTList, x, listTop, w, listH, TRUE);
    MoveWindow(g_hTAdd, x, by, 90, BTN_H, TRUE);
    MoveWindow(g_hTEdit, x + 96, by, 90, BTN_H, TRUE);
    MoveWindow(g_hTDelete, x + 192, by, 100, BTN_H, TRUE);
    MoveWindow(g_hTRestore, x + 298, by, 90, BTN_H, TRUE);
    MoveWindow(g_hTMoveUp, x + 394, by, 90, BTN_H, TRUE);
    MoveWindow(g_hTMoveDown, x + 490, by, 100, BTN_H, TRUE);
}

void ApplyTabVisibility() {
    bool showReminders = (TabCtrl_GetCurSel(g_hTab) == 0);
    int cmdR = showReminders ? SW_SHOW : SW_HIDE;
    int cmdT = showReminders ? SW_HIDE : SW_SHOW;
    ShowWindow(g_hRFilterLabel, cmdR); ShowWindow(g_hRFilter, cmdR);
    ShowWindow(g_hRShowPast, cmdR); ShowWindow(g_hRList, cmdR);
    ShowWindow(g_hRAdd, cmdR); ShowWindow(g_hREdit, cmdR); ShowWindow(g_hRDelete, cmdR);

    ShowWindow(g_hTFilterLabel, cmdT); ShowWindow(g_hTFilter, cmdT);
    ShowWindow(g_hTRecycle, cmdT); ShowWindow(g_hTList, cmdT);
    ShowWindow(g_hTAdd, cmdT); ShowWindow(g_hTEdit, cmdT); ShowWindow(g_hTDelete, cmdT);
    ShowWindow(g_hTRestore, cmdT); ShowWindow(g_hTMoveUp, cmdT); ShowWindow(g_hTMoveDown, cmdT);
}

void UpdateTaskButtonsForMode() {
    if (g_showRecycleBin) {
        SetWindowText(g_hTDelete, L"Delete Forever");
        EnableWindow(g_hTAdd, FALSE);
        EnableWindow(g_hTEdit, FALSE);
        EnableWindow(g_hTMoveUp, FALSE);
        EnableWindow(g_hTMoveDown, FALSE);
        EnableWindow(g_hTRestore, TRUE);
    } else {
        SetWindowText(g_hTDelete, L"Delete");
        EnableWindow(g_hTAdd, TRUE);
        EnableWindow(g_hTEdit, TRUE);
        EnableWindow(g_hTMoveUp, TRUE);
        EnableWindow(g_hTMoveDown, TRUE);
        EnableWindow(g_hTRestore, FALSE);
    }
}

// ---- list population ----

void RefreshReminderList(HWND) {
    SortRemindersByDate();
    ListView_DeleteAllItems(g_hRList);
    g_reminderRowIds.clear();

    std::wstring filter = GetWindowTextStr(g_hRFilter);
    for (const auto& r : g_reminders) {
        if (!g_showPastReminders && r.fired) continue;
        if (!filter.empty() && !ContainsCI(r.name, filter) &&
            !ContainsCI(r.subject, filter) && !ContainsCI(r.description, filter) &&
            !ContainsCI(r.notes, filter)) continue;

        int row = (int)g_reminderRowIds.size();
        LVITEM lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(r.name.c_str());
        ListView_InsertItem(g_hRList, &lvi);

        std::wstring subjectCell = SubjectWithNotes(r.subject, r.notes);
        ListView_SetItemText(g_hRList, row, 1, const_cast<LPWSTR>(subjectCell.c_str()));
        std::wstring dateStr = FormatDateTime(r.due);
        ListView_SetItemText(g_hRList, row, 2, const_cast<LPWSTR>(dateStr.c_str()));
        ListView_SetItemText(g_hRList, row, 3, const_cast<LPWSTR>(r.description.c_str()));

        g_reminderRowIds.push_back(r.id);
    }
}

void RefreshTaskList(HWND) {
    ListView_DeleteAllItems(g_hTList);
    g_taskRowIds.clear();

    std::wstring filter = GetWindowTextStr(g_hTFilter);
    for (const auto& t : g_tasks) {
        bool matchesView = g_showRecycleBin ? t.deleted : !t.deleted;
        if (!matchesView) continue;
        if (!filter.empty() && !ContainsCI(t.name, filter) &&
            !ContainsCI(t.subject, filter) && !ContainsCI(t.description, filter) &&
            !ContainsCI(t.notes, filter)) continue;

        int row = (int)g_taskRowIds.size();
        LVITEM lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(t.name.c_str());
        ListView_InsertItem(g_hTList, &lvi);

        std::wstring subjectCell = SubjectWithNotes(t.subject, t.notes);
        ListView_SetItemText(g_hTList, row, 1, const_cast<LPWSTR>(subjectCell.c_str()));
        std::wstring dateStr = t.hasDate ? FormatDate(t.due) : L"(no date)";
        ListView_SetItemText(g_hTList, row, 2, const_cast<LPWSTR>(dateStr.c_str()));
        ListView_SetItemText(g_hTList, row, 3, const_cast<LPWSTR>(t.description.c_str()));

        g_taskRowIds.push_back(t.id);
    }
}

void RefreshAll(HWND hwnd) {
    RefreshReminderList(hwnd);
    RefreshTaskList(hwnd);
    SaveAllData();
}

int GetSelectedReminderId() {
    int sel = ListView_GetNextItem(g_hRList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)g_reminderRowIds.size()) return -1;
    return g_reminderRowIds[sel];
}

int GetSelectedTaskId() {
    int sel = ListView_GetNextItem(g_hTList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)g_taskRowIds.size()) return -1;
    return g_taskRowIds[sel];
}

// ---- entry dialog bridging ----

EntryResult ReminderToEntryResult(const Reminder& r) {
    EntryResult e;
    e.name = r.name;
    e.description = r.description;
    e.subject = r.subject;
    e.notes = r.notes;
    e.hasDate = true;
    e.due = r.due;
    e.colour = r.colour;
    return e;
}

EntryResult TaskToEntryResult(const Task& t) {
    EntryResult e;
    e.name = t.name;
    e.description = t.description;
    e.subject = t.subject;
    e.notes = t.notes;
    e.hasDate = t.hasDate;
    e.due = t.due;
    e.colour = t.colour;
    return e;
}

// ---- command handlers ----

void OnReminderAdd(HWND hwnd) {
    EntryResult e = ShowEntryDialog(hwnd, EntryKind::Reminder, L"Add Reminder", nullptr);
    if (!e.saved) return;
    AddReminder(e.name, e.description, e.subject, e.notes, e.due, e.colour);
    RefreshAll(hwnd);
}

void OnReminderEdit(HWND hwnd) {
    int id = GetSelectedReminderId();
    if (id < 0) {
        MessageBox(hwnd, L"Select a reminder to edit first.", L"No selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    Reminder* r = FindReminderById(id);
    if (!r) return;
    EntryResult prefill = ReminderToEntryResult(*r);
    EntryResult e = ShowEntryDialog(hwnd, EntryKind::Reminder, L"Edit Reminder", &prefill);
    if (!e.saved) return;
    UpdateReminder(id, e.name, e.description, e.subject, e.notes, e.due, e.colour);
    RefreshAll(hwnd);
}

void OnReminderDelete(HWND hwnd) {
    int id = GetSelectedReminderId();
    if (id < 0) {
        MessageBox(hwnd, L"Select a reminder to delete first.", L"No selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (MessageBox(hwnd, L"Delete this reminder?", L"Confirm delete", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    DeleteReminder(id);
    RefreshAll(hwnd);
}

void OnTaskAdd(HWND hwnd) {
    EntryResult e = ShowEntryDialog(hwnd, EntryKind::Task, L"Add Task", nullptr);
    if (!e.saved) return;
    AddTask(e.name, e.description, e.subject, e.notes, e.hasDate, e.due, e.colour);
    RefreshAll(hwnd);
}

void OnTaskEdit(HWND hwnd) {
    int id = GetSelectedTaskId();
    if (id < 0) {
        MessageBox(hwnd, L"Select a task to edit first.", L"No selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    Task* t = FindTaskById(id);
    if (!t) return;
    EntryResult prefill = TaskToEntryResult(*t);
    EntryResult e = ShowEntryDialog(hwnd, EntryKind::Task, L"Edit Task", &prefill);
    if (!e.saved) return;
    UpdateTask(id, e.name, e.description, e.subject, e.notes, e.hasDate, e.due, e.colour);
    RefreshAll(hwnd);
}

void OnTaskDeleteOrPurge(HWND hwnd) {
    int id = GetSelectedTaskId();
    if (id < 0) {
        MessageBox(hwnd, L"Select a task first.", L"No selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_showRecycleBin) {
        if (MessageBox(hwnd, L"Permanently delete this task? This cannot be undone.",
            L"Confirm delete forever", MB_YESNO | MB_ICONWARNING) != IDYES) return;
        PurgeTask(id);
    } else {
        DeleteTask(id);
    }
    RefreshAll(hwnd);
}

void OnTaskRestore(HWND hwnd) {
    int id = GetSelectedTaskId();
    if (id < 0) {
        MessageBox(hwnd, L"Select a task to restore first.", L"No selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    RestoreTask(id);
    RefreshAll(hwnd);
}

void OnTaskMove(HWND hwnd, bool up) {
    int id = GetSelectedTaskId();
    if (id < 0) return;
    if (up) MoveTaskUp(id); else MoveTaskDown(id);
    RefreshAll(hwnd);

    for (size_t i = 0; i < g_taskRowIds.size(); i++) {
        if (g_taskRowIds[i] == id) {
            ListView_SetItemState(g_hTList, (int)i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_hTList, (int)i, FALSE);
            break;
        }
    }
}

// ---- custom draw (per-row colour coding) ----

LRESULT HandleListCustomDraw(NMLVCUSTOMDRAW* cd, bool isReminderList) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        int row = (int)cd->nmcd.dwItemSpec;
        COLORREF colour = GetSysColor(COLOR_WINDOWTEXT);
        if (isReminderList) {
            if (row >= 0 && row < (int)g_reminderRowIds.size()) {
                Reminder* r = FindReminderById(g_reminderRowIds[row]);
                if (r && r->colour != DEFAULT_COLOUR) colour = r->colour;
            }
        } else {
            if (row >= 0 && row < (int)g_taskRowIds.size()) {
                Task* t = FindTaskById(g_taskRowIds[row]);
                if (t && t->colour != DEFAULT_COLOUR) colour = t->colour;
            }
        }
        cd->clrText = colour;
        cd->clrTextBk = GetSysColor(COLOR_WINDOW);
        return CDRF_NEWFONT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

// ---- window procedure ----

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hInst = ((CREATESTRUCT*)lParam)->hInstance;
        LoadAllData();
        CreateControls(hwnd);
        RefreshReminderList(hwnd);
        RefreshTaskList(hwnd);
        SetTimer(hwnd, ID_TIMER_CHECK, 1000, nullptr);
        return 0;

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 780;
        mmi->ptMinTrackSize.y = 480;
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)lParam;

        if (hdr->idFrom == IDC_TAB && hdr->code == TCN_SELCHANGE) {
            ApplyTabVisibility();
            return 0;
        }
        if (hdr->idFrom == IDC_R_LIST && hdr->code == NM_CUSTOMDRAW) {
            return HandleListCustomDraw((NMLVCUSTOMDRAW*)lParam, true);
        }
        if (hdr->idFrom == IDC_R_LIST && hdr->code == NM_DBLCLK) {
            OnReminderEdit(hwnd);
            return 0;
        }
        if (hdr->idFrom == IDC_T_LIST && hdr->code == NM_CUSTOMDRAW) {
            return HandleListCustomDraw((NMLVCUSTOMDRAW*)lParam, false);
        }
        if (hdr->idFrom == IDC_T_LIST && hdr->code == NM_DBLCLK && !g_showRecycleBin) {
            OnTaskEdit(hwnd);
            return 0;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int notifyCode = HIWORD(wParam);

        if (notifyCode == BN_CLICKED) {
            if (id == IDC_R_ADD) { OnReminderAdd(hwnd); return 0; }
            if (id == IDC_R_EDIT) { OnReminderEdit(hwnd); return 0; }
            if (id == IDC_R_DELETE) { OnReminderDelete(hwnd); return 0; }
            if (id == IDC_R_SHOWPAST) {
                g_showPastReminders = (IsDlgButtonChecked(hwnd, IDC_R_SHOWPAST) == BST_CHECKED);
                RefreshReminderList(hwnd);
                return 0;
            }
            if (id == IDC_T_ADD) { OnTaskAdd(hwnd); return 0; }
            if (id == IDC_T_EDIT) { OnTaskEdit(hwnd); return 0; }
            if (id == IDC_T_DELETE) { OnTaskDeleteOrPurge(hwnd); return 0; }
            if (id == IDC_T_RESTORE) { OnTaskRestore(hwnd); return 0; }
            if (id == IDC_T_MOVEUP) { OnTaskMove(hwnd, true); return 0; }
            if (id == IDC_T_MOVEDOWN) { OnTaskMove(hwnd, false); return 0; }
            if (id == IDC_T_RECYCLE) {
                g_showRecycleBin = (IsDlgButtonChecked(hwnd, IDC_T_RECYCLE) == BST_CHECKED);
                UpdateTaskButtonsForMode();
                RefreshTaskList(hwnd);
                return 0;
            }
        }
        if (id == IDC_R_FILTER && notifyCode == EN_CHANGE) { RefreshReminderList(hwnd); return 0; }
        if (id == IDC_T_FILTER && notifyCode == EN_CHANGE) { RefreshTaskList(hwnd); return 0; }
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_CHECK) {
            for (auto& r : g_reminders) {
                if (r.IsDueNow() && !IsNotificationOpenFor(r.id)) {
                    ShowReminderNotification(hwnd, g_hInst, r.id);
                }
            }
        }
        return 0;

    case WM_APP_DATA_CHANGED:
        RefreshAll(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_CHECK);
        SaveAllData();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

void RegisterMainWindowClass(HINSTANCE hInst) {
    WNDCLASS wc{};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
}

HWND CreateMainWindow(HINSTANCE hInst) {
    return CreateWindowEx(0, kClassName, L"Reminders & Tasks",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 940, 620,
        nullptr, nullptr, hInst, nullptr);
}
