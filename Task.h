// Task.h
// Task data model and in-memory store.
//
// Tasks differ from Reminders: no required date (context only), manually
// orderable (move up/down) instead of date-sorted, and deleting one is a
// soft-delete into a recycle bin (restore, or purge to permanently remove
// and shift the rest down - same idea as the reminder-deletion pseudocode).
#pragma once

#include "Common.h"

struct Task {
    int id = 0;
    std::wstring name;
    std::wstring description;
    std::wstring subject;
    std::wstring notes;
    bool hasDate = false;
    SYSTEMTIME due{};
    COLORREF colour = DEFAULT_COLOUR;
    bool deleted = false; // true = sitting in the recycle bin
};

extern std::vector<Task> g_tasks;

Task* FindTaskById(int id);

int AddTask(const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    bool hasDate, const SYSTEMTIME& due, COLORREF colour);

bool UpdateTask(int id, const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    bool hasDate, const SYSTEMTIME& due, COLORREF colour);

void DeleteTask(int id);   // soft delete -> recycle bin
void RestoreTask(int id);  // recycle bin -> active
void PurgeTask(int id);    // permanent delete, shifts the rest down

void MoveTaskUp(int id);
void MoveTaskDown(int id);
