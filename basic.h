#ifndef BASIC_H
#define BASIC_H

#include "definition.h"
#include <stdio.h>
#include <shellapi.h>
#include <thread>

void AddLog(HWND hwnd, const char* text) {
	HWND hLog = GetDlgItem(hwnd, ID_LOG_DISPLAY);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char timeStr[20];
	sprintf(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s%s\r\n", timeStr, text);

	int len = GetWindowTextLength(hLog);
	SendMessage(hLog, EM_SETSEL, len, len);
	SendMessage(hLog, EM_REPLACESEL, FALSE, (LPARAM)buffer);

	SendMessage(hLog, WM_VSCROLL, SB_BOTTOM, 0);
}

typedef struct {
	HWND hwnd;
	char msg[128];
	char title[128];
	UINT flag;
	int duration;
	int uID;
}notificationParams;


void ShowNotification(HWND hwnd, char notificationMsg[128], char notificationTitle[128], UINT notificationFlag, int sleepDuration, int uID = 1) {
	NOTIFYICONDATA nid = {0};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = uID;
//	nid.uFlags = NIF_INFO;
	strcpy(nid.szInfo, notificationMsg);
	strcpy(nid.szInfoTitle, notificationTitle);
//	nid.dwInfoFlags = notificationFlag; // NIIF_INFO, NIIF_ERROR, NIIF_WARNING, NIIF_INFO, NIIF_NONE
	nid.dwInfoFlags = NIIF_INFO; // NIIF_INFO, NIIF_ERROR, NIIF_WARNING, NIIF_INFO, NIIF_NONE
	nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	nid.uFlags = NIF_ICON | NIF_INFO;
	
	
	Shell_NotifyIcon(NIM_ADD, &nid);
	Sleep(sleepDuration);
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

DWORD WINAPI NotificationThread(LPVOID lpParam) {
    notificationParams* p = (notificationParams*)lpParam;
    ShowNotification(p->hwnd, p->msg, p->title, p->flag, p->duration, p->uID);
    free(p);
    return 0;
}

void CreateNotificationThread(const char* msg, const char* title, int duration = 5000, UINT flag = NIIF_INFO, int uID = 1) {
    notificationParams* p = (notificationParams*)malloc(sizeof(notificationParams));
    p->hwnd = NULL;
    strncpy(p->msg, msg, sizeof(p->msg) - 1);
    p->msg[sizeof(p->msg) - 1] = '\0';
    strncpy(p->title, title, sizeof(p->title) - 1);
    p->title[sizeof(p->title) - 1] = '\0';
    p->flag = flag;
    p->duration = duration;
    p->uID = uID;

    DWORD threadId;
    HANDLE hThread = CreateThread(NULL, 0, NotificationThread, p, 0, &threadId);
    if (hThread != NULL) {
        CloseHandle(hThread);
    } else {
        free(p);
    }
}

void CopyToClipboard(const char* text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
        memcpy(GlobalLock(hGlob), text, strlen(text) + 1);
        GlobalUnlock(hGlob);
        SetClipboardData(CF_TEXT, hGlob);
        CloseClipboard();
    }
}

#endif
