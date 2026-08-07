// Reminder.h
// Reminder data model and in-memory store.
//
// Pseudocode (from planning doc) for creation:
//   Get name / description(optional) / subject(optional) / date / colour(default black)
//   Put info in an array entry, put entry into the reminders array, refresh UI
// Deletion:
//   Get reminder id, find it in the array, remove it and shift the rest down
//   (done here with std::vector::erase, which does exactly that).
#pragma once

#include "Common.h"

struct Reminder {
    int id = 0;
    std::wstring name;
    std::wstring description;
    std::wstring subject;
    std::wstring notes;
    SYSTEMTIME due{};
    COLORREF colour = DEFAULT_COLOUR;
    bool fired = false;          // dismissed / acknowledged -> becomes an "old reminder"
    ULONGLONG snoozedUntil = 0;  // 0 = not snoozed, else ticks (see SystemTimeToTicks)

    bool IsDueNow() const;
};

extern std::vector<Reminder> g_reminders;

void SortRemindersByDate();
Reminder* FindReminderById(int id);

int AddReminder(const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    const SYSTEMTIME& due, COLORREF colour);

bool UpdateReminder(int id, const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    const SYSTEMTIME& due, COLORREF colour);

void DeleteReminder(int id);
