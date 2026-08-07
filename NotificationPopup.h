// NotificationPopup.h
// Topmost "toast" overlay used for reminder popup notifications, with
// Snooze/Dismiss actions (UI plan: "Overlay"; Reminder Messages: "Snooze?",
// "Popup notifications").
#pragma once

#include "Common.h"

void RegisterNotificationPopupClass(HINSTANCE hInst);
bool IsNotificationOpenFor(int reminderId);
void ShowReminderNotification(HWND mainWnd, HINSTANCE hInst, int reminderId);
