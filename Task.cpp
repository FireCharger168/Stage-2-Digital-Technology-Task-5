// Task.cpp
#include "Task.h"
#include <algorithm>

std::vector<Task> g_tasks;

static int NextTaskId() {
    int maxId = 0;
    for (const auto& t : g_tasks) maxId = (t.id > maxId) ? t.id : maxId;
    return maxId + 1;
}

Task* FindTaskById(int id) {
    for (auto& t : g_tasks) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

int AddTask(const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    bool hasDate, const SYSTEMTIME& due, COLORREF colour) {
    Task t;
    t.id = NextTaskId();
    t.name = name;
    t.description = description;
    t.subject = subject;
    t.notes = notes;
    t.hasDate = hasDate;
    t.due = due;
    t.colour = colour;
    t.deleted = false;
    g_tasks.push_back(t);
    return t.id;
}

bool UpdateTask(int id, const std::wstring& name, const std::wstring& description,
    const std::wstring& subject, const std::wstring& notes,
    bool hasDate, const SYSTEMTIME& due, COLORREF colour) {
    Task* t = FindTaskById(id);
    if (!t) return false;
    t->name = name;
    t->description = description;
    t->subject = subject;
    t->notes = notes;
    t->hasDate = hasDate;
    t->due = due;
    t->colour = colour;
    return true;
}

void DeleteTask(int id) {
    Task* t = FindTaskById(id);
    if (t) t->deleted = true;
}

void RestoreTask(int id) {
    Task* t = FindTaskById(id);
    if (t) t->deleted = false;
}

void PurgeTask(int id) {
    for (size_t i = 0; i < g_tasks.size(); i++) {
        if (g_tasks[i].id == id) {
            g_tasks.erase(g_tasks.begin() + i);
            return;
        }
    }
}

// Swaps with the nearest neighbour that's in the same list (active vs
// recycle bin), so reordering active tasks can never shuffle bin contents.
void MoveTaskUp(int id) {
    for (size_t i = 0; i < g_tasks.size(); i++) {
        if (g_tasks[i].id != id) continue;
        for (size_t j = i; j-- > 0; ) {
            if (g_tasks[j].deleted == g_tasks[i].deleted) {
                std::swap(g_tasks[i], g_tasks[j]);
                return;
            }
        }
        return;
    }
}

void MoveTaskDown(int id) {
    for (size_t i = 0; i < g_tasks.size(); i++) {
        if (g_tasks[i].id != id) continue;
        for (size_t j = i + 1; j < g_tasks.size(); j++) {
            if (g_tasks[j].deleted == g_tasks[i].deleted) {
                std::swap(g_tasks[i], g_tasks[j]);
                return;
            }
        }
        return;
    }
}
