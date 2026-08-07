// Persistence.h
// Saves/loads g_reminders and g_tasks to a small UTF-8 text file under
// %APPDATA%\ReminderTaskApp\data.txt so entries survive between runs.
#pragma once

void SaveAllData();
void LoadAllData();
