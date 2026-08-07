// Persistence.cpp
#include "Persistence.h"
#include "Common.h"
#include "Reminder.h"
#include "Task.h"
#include <shlobj.h>
#include <fstream>
#include <iterator>

#pragma comment(lib, "shell32.lib")

static std::wstring GetDataFilePath() {
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return L"data.txt";
    }
    std::wstring dir = std::wstring(path) + L"\\ReminderTaskApp";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\data.txt";
}

static void WriteLine(std::ofstream& out, const std::wstring& line) {
    std::string utf8 = WStringToUtf8(line + L"\n");
    out.write(utf8.data(), (std::streamsize)utf8.size());
}

void SaveAllData() {
    std::ofstream out(GetDataFilePath(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;

    for (const auto& r : g_reminders) {
        std::wstring line = L"REMINDER|" + std::to_wstring(r.id) + L"|" +
            EscapeForStorage(r.name) + L"|" +
            EscapeForStorage(r.description) + L"|" +
            EscapeForStorage(r.subject) + L"|" +
            EscapeForStorage(r.notes) + L"|" +
            std::to_wstring(r.due.wYear) + L"|" +
            std::to_wstring(r.due.wMonth) + L"|" +
            std::to_wstring(r.due.wDay) + L"|" +
            std::to_wstring(r.due.wHour) + L"|" +
            std::to_wstring(r.due.wMinute) + L"|" +
            std::to_wstring((unsigned long)r.colour) + L"|" +
            std::to_wstring(r.fired ? 1 : 0) + L"|" +
            std::to_wstring(r.snoozedUntil);
        WriteLine(out, line);
    }
    for (const auto& t : g_tasks) {
        std::wstring line = L"TASK|" + std::to_wstring(t.id) + L"|" +
            EscapeForStorage(t.name) + L"|" +
            EscapeForStorage(t.description) + L"|" +
            EscapeForStorage(t.subject) + L"|" +
            EscapeForStorage(t.notes) + L"|" +
            std::to_wstring(t.hasDate ? 1 : 0) + L"|" +
            std::to_wstring(t.due.wYear) + L"|" +
            std::to_wstring(t.due.wMonth) + L"|" +
            std::to_wstring(t.due.wDay) + L"|" +
            std::to_wstring(t.due.wHour) + L"|" +
            std::to_wstring(t.due.wMinute) + L"|" +
            std::to_wstring((unsigned long)t.colour) + L"|" +
            std::to_wstring(t.deleted ? 1 : 0);
        WriteLine(out, line);
    }
}

static void LoadReminderLine(const std::vector<std::wstring>& f) {
    if (f.size() < 14) return;
    Reminder r;
    r.id = _wtoi(f[1].c_str());
    r.name = UnescapeFromStorage(f[2]);
    r.description = UnescapeFromStorage(f[3]);
    r.subject = UnescapeFromStorage(f[4]);
    r.notes = UnescapeFromStorage(f[5]);
    r.due.wYear = (WORD)_wtoi(f[6].c_str());
    r.due.wMonth = (WORD)_wtoi(f[7].c_str());
    r.due.wDay = (WORD)_wtoi(f[8].c_str());
    r.due.wHour = (WORD)_wtoi(f[9].c_str());
    r.due.wMinute = (WORD)_wtoi(f[10].c_str());
    r.colour = (COLORREF)_wtoi64(f[11].c_str());
    r.fired = _wtoi(f[12].c_str()) != 0;
    r.snoozedUntil = (ULONGLONG)_wtoi64(f[13].c_str());
    g_reminders.push_back(r);
}

static void LoadTaskLine(const std::vector<std::wstring>& f) {
    if (f.size() < 14) return;
    Task t;
    t.id = _wtoi(f[1].c_str());
    t.name = UnescapeFromStorage(f[2]);
    t.description = UnescapeFromStorage(f[3]);
    t.subject = UnescapeFromStorage(f[4]);
    t.notes = UnescapeFromStorage(f[5]);
    t.hasDate = _wtoi(f[6].c_str()) != 0;
    t.due.wYear = (WORD)_wtoi(f[7].c_str());
    t.due.wMonth = (WORD)_wtoi(f[8].c_str());
    t.due.wDay = (WORD)_wtoi(f[9].c_str());
    t.due.wHour = (WORD)_wtoi(f[10].c_str());
    t.due.wMinute = (WORD)_wtoi(f[11].c_str());
    t.colour = (COLORREF)_wtoi64(f[12].c_str());
    t.deleted = _wtoi(f[13].c_str()) != 0;
    g_tasks.push_back(t);
}

void LoadAllData() {
    g_reminders.clear();
    g_tasks.clear();

    std::ifstream in(GetDataFilePath(), std::ios::binary);
    if (!in.is_open()) return;

    std::string rawUtf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::wstring content = Utf8ToWString(rawUtf8);

    size_t pos = 0;
    while (pos <= content.size()) {
        size_t nl = content.find(L'\n', pos);
        std::wstring line = (nl == std::wstring::npos) ? content.substr(pos) : content.substr(pos, nl - pos);
        if (!line.empty()) {
            std::vector<std::wstring> f = SplitFields(line, L'|');
            if (!f.empty() && f[0] == L"REMINDER") LoadReminderLine(f);
            else if (!f.empty() && f[0] == L"TASK") LoadTaskLine(f);
        }
        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }

    SortRemindersByDate();
}
