// EntryDialog.h
// Shared Add/Edit popup used for both Reminders and Tasks.
#pragma once

#include "Common.h"

enum class EntryKind { Reminder, Task };

struct EntryResult {
    bool saved = false; // true only if the user clicked OK
    std::wstring name, description, subject, notes;
    bool hasDate = true;
    SYSTEMTIME due{};
    COLORREF colour = DEFAULT_COLOUR;
};

void RegisterEntryDialogClass(HINSTANCE hInst);

// Blocks (runs its own message loop) until the user saves or cancels.
// Pass prefill = nullptr to create a new entry, or an existing EntryResult
// (e.g. built from a Reminder/Task) to edit it.
EntryResult ShowEntryDialog(HWND owner, EntryKind kind, const wchar_t* title, const EntryResult* prefill);
