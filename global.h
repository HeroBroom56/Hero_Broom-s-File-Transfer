#ifndef GLOBAL_H
#define GLOBAL_H

#include "definition.h"
#include "file.h"
#include "basic.h"
#include <windows.h>
#include <commctrl.h>

HWND hSettingsGeneral, hSettingsNetwork, hSettingsAbout;

void SetAutoStart(HWND hwnd, bool autoStart) {
	char filePath[MAX_PATH];
	GetModuleFileNameA(NULL, filePath, MAX_PATH);

	HKEY hKey;
	LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);

	if (result != ERROR_SUCCESS) {
		char errMsg[64];
		sprintf(errMsg, "[General Settings] Unable to open the registry. Error Code : %d", (int)result);
		AddLog(hwnd, errMsg);
		return;
	}

	if (autoStart) {
		result = RegSetValueEx(hKey, REG_APP_NAME, 0, REG_SZ, (const BYTE*)filePath, (DWORD)(strlen(filePath) + 1));
	} else {
		result = RegDeleteValue(hKey, REG_APP_NAME);
	}

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS) {
		char errMsg[64];
		sprintf(errMsg, "[General Settings] Unable to set the registry entry. Error Code : %d", (int)result);
		AddLog(hwnd, errMsg);
		return;
	}

	return;
}

void ShowSettingsPage(HWND hwndParent, int pageIndex) {
//	char logMsg[64];
//	sprintf(logMsg, "[Debug] Currently Page Index : %d", pageIndex);
//	AddLog(hwndParent, logMsg);

	ShowWindow(hSettingsAbout, SW_HIDE);
	ShowWindow(hSettingsGeneral, SW_HIDE);
	ShowWindow(hSettingsNetwork, SW_HIDE);

	switch (pageIndex) {
		case 0: {
			ShowWindow(hSettingsGeneral, SW_SHOW);
			break;
		}
		case 1: {
			ShowWindow(hSettingsNetwork, SW_SHOW);
			break;
		}
		case 2: {
			ShowWindow(hSettingsAbout, SW_SHOW);
			break;
		}

		default:
			break;
	}
}

LRESULT CALLBACK SettingsPageSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (msg) {
		case WM_CTLCOLORSTATIC: {
			HDC hdcStatic = (HDC)wParam;
			SetBkMode(hdcStatic, TRANSPARENT);
			return (LRESULT)GetStockObject(NULL_BRUSH);
		}

		case WM_NOTIFY: {
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN) {
				if (GetDlgCtrlID(pnmh->hwndFrom) != ID_TAB_CTRL) {
					PNMLINK pnmLink = (PNMLINK)lParam;
					ShellExecuteW(NULL, L"open", pnmLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}
		}

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
			SelectObject(hdc, hPen);
			SelectObject(hdc, GetStockObject(NULL_BRUSH));

			if (uIdSubclass == 0) {
				RoundRect(hdc, 20, 55, 365, 82, 10, 10);
				RoundRect(hdc, 20, 115, 265, 142, 10, 10);
			} else if (uIdSubclass == 1) {

			} else if (uIdSubclass == 2) {
				MoveToEx(hdc, 140, 130, NULL);
				LineTo(hdc, 380, 130);
			}

			DeleteObject(hPen);
			EndPaint(hwnd, &ps);

			return 0;
		}

		case WM_DRAWITEM: {
			LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;

			if (pDIS->CtlID == ID_CHECK_AUTO_START || pDIS->CtlID == ID_CHECK_NOTIFICATION) {
				HDC hdc = pDIS->hDC;
				RECT rect = pDIS->rcItem;

				FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

				SetTextColor(hdc, RGB(0, 0, 0));
				SetBkMode(hdc, TRANSPARENT);

				bool buttonChecked = 0;
				if (pDIS->CtlID == ID_CHECK_AUTO_START) buttonChecked = g_Settings.autoStart;
				else if (pDIS->CtlID == ID_CHECK_NOTIFICATION) buttonChecked = g_Settings.showNotification;

//				char text[64];
//				GetWindowText(pDIS->hwndItem, text, 64);
//				DrawText(hdc, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

				HPEN hPen;
				if (buttonChecked) hPen = CreatePen(PS_SOLID, 1, RGB(0, 162, 232));
				else hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
				HBRUSH hBrush = CreateSolidBrush(RGB(0, 162, 232));
				SelectObject(hdc, hPen);
				if (buttonChecked) SelectObject(hdc, hBrush);
				else SelectObject(hdc, GetStockObject(NULL_BRUSH));
				RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 6, 6);

				if (buttonChecked) {
					int boxX = rect.left, boxY = rect.top;

					HPEN hCheckPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
					HPEN hOld = (HPEN)SelectObject(hdc, hCheckPen);

					MoveToEx(hdc, boxX + 2, boxY + 7, NULL);
					LineTo(hdc, boxX + 6, boxY + 12);
					LineTo(hdc, boxX + 13, boxY + 4);

					SelectObject(hdc, hOld);
					DeleteObject(hCheckPen);
				}

				DeleteObject(hPen);
				DeleteObject(hBrush);

				return TRUE;
			}

			if (pDIS->CtlID == ID_BUTTON_SAVE_GENERAL || pDIS->CtlID == ID_BUTTON_BROWSE || pDIS->CtlID == ID_BUTTON_SAVE_NETWORK ||
			        pDIS->CtlID == ID_BUTTON_COPY_IP) {
				HDC hdc = pDIS->hDC;
				RECT rect = pDIS->rcItem;

				FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

				SetTextColor(hdc, RGB(0, 0, 0));

				SetBkMode(hdc, TRANSPARENT);

				char text[64];
				GetWindowText(pDIS->hwndItem, text, 64);
				DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
				SelectObject(hdc, hPen);
				SelectObject(hdc, GetStockObject(NULL_BRUSH));
				RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
				DeleteObject(hPen);

				return TRUE;
			}
		}

		case WM_SETCURSOR: {
			HWND hChild = (HWND)wParam;
			int currentID = GetDlgCtrlID(hChild);

			if (currentID == ID_BUTTON_SAVE_GENERAL || currentID == ID_BUTTON_BROWSE ||
			        currentID == ID_CHECK_AUTO_START || currentID == ID_BUTTON_COPY_IP ||
			        currentID == ID_BUTTON_SAVE_NETWORK || currentID == ID_CHECK_NOTIFICATION) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
				return TRUE;
			}

			break;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == ID_BUTTON_SAVE_GENERAL) {
				if (MessageBox(hwnd, "Are you sure to save the changes?", "Confirm", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
					GetWindowText(GetDlgItem(hwnd, ID_EDIT_NICK), g_Settings.nickname, 32);
					GetWindowText(GetDlgItem(hwnd, ID_EDIT_DEFAULT_PATH), g_Settings.savePath, MAX_PATH);

					HWND hMainWnd = GetAncestor(hwnd, GA_ROOT);
					PostMessage(hMainWnd, WM_UPDATE_CONFIG, 0, 0);

					AddLog(hMainWnd, "[General Settings] Settings saved!!");
					SetAutoStart(hMainWnd, g_Settings.autoStart);

					MessageBox(hwnd, "Settings Saved!!", "Save Config", MB_OK | MB_ICONINFORMATION);
				}
			} else if (LOWORD(wParam) == ID_BUTTON_SAVE_NETWORK) {
				if (MessageBox(hwnd, "Are you sure to save the changes?", "Confirm", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
					HWND hMainWnd = GetAncestor(hwnd, GA_ROOT);
					PostMessage(hMainWnd, WM_UPDATE_CONFIG, 0, 0);

					AddLog(hMainWnd, "[Network Settings] Settings saved!!");
					MessageBox(hwnd, "Settings Saved!!", "Save Config", MB_OK | MB_ICONINFORMATION);
				}
			} else if (LOWORD(wParam) == ID_BUTTON_BROWSE) {
				char selectedDir[MAX_PATH] = {0};
				if (SelectFolderOld(hwnd, selectedDir)) {
					SetWindowText(GetDlgItem(hwnd, ID_EDIT_SAVE_PATH), selectedDir);
					strncpy(g_Settings.savePath, selectedDir, MAX_PATH);
					g_Settings.savePath[MAX_PATH - 1] = '\0';
					SetWindowText(GetDlgItem(hwnd, ID_EDIT_DEFAULT_PATH), selectedDir);
				}
			} else if (LOWORD(wParam) == ID_CHECK_AUTO_START && HIWORD(wParam) == BN_CLICKED) {
//				char msgBox[64];
//				sprintf(msgBox, "Checkbox status changed!!\nCurrent status: %s", g_Settings.autoStart ? "Checked" : "Unchecked");
//				MessageBox(hwnd, msgBox, "Status Changed", MB_OK | MB_ICONINFORMATION);
				g_Settings.autoStart ^= 1;
				InvalidateRect(GetDlgItem(hwnd, ID_CHECK_AUTO_START), NULL, TRUE);
			} else if (LOWORD(wParam) == ID_CHECK_NOTIFICATION && HIWORD(wParam) == BN_CLICKED) {
				g_Settings.showNotification ^= 1;
				InvalidateRect(GetDlgItem(hwnd, ID_CHECK_NOTIFICATION), NULL, TRUE);
			} else if (LOWORD(wParam) == ID_BUTTON_COPY_IP) {
//				Beep(750, 300);
				CopyToClipboard(g_Settings.localIP);
				SetWindowText(GetDlgItem(hwnd, ID_BUTTON_COPY_IP), "Copied!");
				SetTimer(hwnd, 104, 1200, NULL);
			}
			break;
		}

		case WM_TIMER: {
			if (wParam == 104) {
				SetWindowText(GetDlgItem(hwnd, ID_BUTTON_COPY_IP), "Copy IP Address");
				KillTimer(hwnd, 104);
			}
			break;
		}

		default:
			break;
	}
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

#endif
