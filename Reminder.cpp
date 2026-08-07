// Reminder.cpp
#include "Reminder.h"
#include <algorithm>

std::vector<Reminder> g_reminders;

static int NextReminderId() {
    int maxId = 0;
    for (const auto& r : g_reminders) maxId = (r.id > maxId) ? r.id : maxId;
    return maxId + 1;
}

bool Reminder::IsDueNow() const {
    if (fired) return false;
    ULONGLONG now = SystemTimeToTicks(NowLocalSystemTime());
    ULONGLONG threshold = SystemTimeToTicks(due);
    if (snoozedUntil != 0 && snoozedUntil > threshold) threshold = snoozedUntil;
    return now >= threshold;
}

void SortRemindersByDate() {
    std::sort(g_reminders.begin(), g_reminders.end(), [](const Reminder& a, const Reminder& b) {
        return SystemTimeToTicks(a.due) < SystemTimeToTicks(b.due);
    });
}

Reminder* FindReminderById(int id) {
    for (auto& r : g_reminders) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

int AddReminder(const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    const SYSTEMTIME& due, COLORREF colour) {
    Reminder r;
    r.id = NextReminderId();
    r.name = name;
    r.description = description;
    r.subject = subject;
    r.notes = notes;
    r.due = due;
    r.colour = colour;
    r.fired = false;
    r.snoozedUntil = 0;
    g_reminders.push_back(r);
    return r.id;
}

bool UpdateReminder(int id, const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    const SYSTEMTIME& due, COLORREF colour) {
    Reminder* r = FindReminderById(id);
    if (!r) return false;
    r->name = name;
    r->description = description;
    r->subject = subject;
    r->notes = notes;
    r->due = due;
    r->colour = colour;
    // Re-arm: editing a fired/snoozed reminder should bring it back into play.
    r->fired = false;
    r->snoozedUntil = 0;
    return true;
}

void DeleteReminder(int id) {
    for (size_t i = 0; i < g_reminders.size(); i++) {
        if (g_reminders[i].id == id) {
            g_reminders.erase(g_reminders.begin() + i);
            return;
        }
    }
}
