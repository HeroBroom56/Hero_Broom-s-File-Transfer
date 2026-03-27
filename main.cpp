#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef WINVER
#define WINVER 0x0600
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define INITGUID

#include <stdio.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>

#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shlguid.h>
#include <objbase.h>


using std::ifstream;
using std::ofstream;
using std::ios;

#ifndef EM_SETCUEBANNER
#define ECM_FIRST 0x1500
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

#define ID_EDIT_IP 101
#define ID_PRESS_BUTTON 102
#define ID_IP_FORMAT_ERROR 103
#define ID_CHOOSE_FILE_BUTTON 104
#define ID_CHOSEN_FILE 105
#define ID_EDIT_PENDING_MAXIMUM 106
#define ID_TOO_BIG 107
#define ID_FORMAT_ERROR 108
#define ID_INFO_NAME 109
#define ID_INFO_SIZE 110
#define ID_INFO_DATE 111
#define ID_CONFIRM 112
#define ID_IP_EMPTY 113
#define ID_PENDING_EMPTY 114
#define ID_LOG_DISPLAY 115
#define ID_TAB_CTRL 116
#define ID_PORT_HINT 117
#define ID_PENDING_HINT 118
#define ID_INFO_HINT 119
#define ID_UI_BORDERS 120
#define ID_IP_HINT 121
#define ID_INPUT_TARGET_IP 122
#define ID_INVALID_IP 123
#define ID_COPY_LOG 124
#define ID_CLEAR_LOG 125

#define ID_EDIT_RECV_PORT 201
#define ID_BUTTON_BROWSE_DIR 202
#define ID_STATIC_SAVE_PATH 203
#define ID_BUTTON_START_RECV 204
#define ID_BUTTON_STOP_RECV 205
#define ID_STATIC_PORT_HINT 206
#define ID_EDIT_SAVE_PATH 207
#define ID_BUTTON_OPEN_FOLDER 208
#define ID_STATIC_INVALID_PORT 209
#define ID_STATIC_EMPTY_PORT 210

#define ID_PROGRESS_BAR 401
#define ID_STATIC_SPEED 402

WSADATA wsaData;
char g_szFullFilePath[MAX_PATH];
char g_szRecvFolderPath[MAX_PATH];
HBRUSH hWhiteBrush = NULL;

struct TransferData {
	HWND hwnd;
	char ip[32];
	char port[10];
	char filePath[MAX_PATH];
};

struct FileInfo {
	char fileName[260];
	long long fileSize;
};

struct RecvParam {
	HWND hwnd;
	int port;
	char savePath[MAX_PATH];
};

bool DirectoryExists(const char* path) {
	DWORD dwAttrib = GetFileAttributes(path);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsValidIP(const char* ipStr) {
	if (ipStr == NULL || strlen(ipStr) == 0) return false;
	unsigned long addr = inet_addr(ipStr);
	return (addr != INADDR_NONE);
}

void ShowTabPage(HWND hwnd, int pageIndex) {

	int sendControls[] = {
		ID_PORT_HINT, ID_EDIT_IP,
		ID_PENDING_HINT, ID_EDIT_PENDING_MAXIMUM,
		ID_CHOOSE_FILE_BUTTON, ID_CONFIRM,
		ID_INFO_HINT, ID_INFO_NAME, ID_INFO_SIZE, ID_INFO_DATE,
		ID_IP_HINT, ID_INPUT_TARGET_IP,
	};

	int recvControls[] = {
		ID_EDIT_RECV_PORT, ID_BUTTON_BROWSE_DIR, ID_STATIC_SAVE_PATH, ID_BUTTON_START_RECV, ID_BUTTON_STOP_RECV, ID_STATIC_PORT_HINT,
		ID_EDIT_SAVE_PATH, ID_BUTTON_OPEN_FOLDER,
	};

	int sendErrorControls[] = {
		ID_IP_FORMAT_ERROR, ID_IP_EMPTY, ID_TOO_BIG, ID_FORMAT_ERROR, ID_PENDING_EMPTY, ID_INVALID_IP,
	};

	int recvErrorControls[] = {
		ID_STATIC_EMPTY_PORT, ID_STATIC_INVALID_PORT,
	};

	if (pageIndex == 1) {
		for (int id : sendErrorControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	} else if (pageIndex == 0) {
		for (int id : recvErrorControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	}

	for (int id : sendControls)
		ShowWindow(GetDlgItem(hwnd, id), (pageIndex == 0) ? SW_SHOW : SW_HIDE);

	for (int id : recvControls)
		ShowWindow(GetDlgItem(hwnd, id), (pageIndex == 1) ? SW_SHOW : SW_HIDE);

	InvalidateRect(hwnd, NULL, TRUE);
}

BOOL SelectFolderEasy(HWND hwnd, char* outPath) {
	OPENFILENAME ofn;
	char szFile[MAX_PATH] = "Select Any File in Target Folder";
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrTitle = "Select any file in the target folder";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileName(&ofn)) {
		strcpy(outPath, szFile);
		char* lastBackslash = strrchr(outPath, '\\');
		if (lastBackslash) *lastBackslash = '\0';
		return TRUE;
	}
	return FALSE;
}

BOOL SelectFolderOld(HWND hwnd, char* outPath) {
	BROWSEINFO bi = { 0 };
	bi.hwndOwner = hwnd;
	bi.lpszTitle = "Select the folder to save received files:";
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl != NULL) {
		SHGetPathFromIDList(pidl, outPath);
		CoTaskMemFree(pidl);
		return TRUE;
	}
	return FALSE;
}

BOOL SelectFileToBuffer(HWND hwnd, char* szPath, int maxLen) {
	OPENFILENAME ofn;

	ZeroMemory(szPath, maxLen);
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof ofn;
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szPath;
	ofn.nMaxFile = maxLen;

	ofn.lpstrFilter = "All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	return GetOpenFileName(&ofn);
}

BOOL ExportLogToFile(HWND hwndLog) {
	int len = GetWindowTextLength(hwndLog);
	if (len <= 0) {
		MessageBox(NULL, "No log data to export!", "Tip", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	char* buffer = new char[len + 1];
	GetWindowText(hwndLog, buffer, len + 1);


	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));

	SYSTEMTIME st;
	GetLocalTime(&st);
	char szFileName[MAX_PATH];
	sprintf(szFileName, "export%04d%02d%02d%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetParent(hwndLog);
	ofn.lpstrFilter = "Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = "log";

	if (GetSaveFileName(&ofn)) {
		std::ofstream outFile(ofn.lpstrFile, std::ios::out | std::ios::trunc);
		if (outFile.is_open()) {
			outFile << buffer;
			outFile.close();
			MessageBox(NULL, "Log exported successfully!", "Success", MB_OK | MB_ICONINFORMATION);
		} else {
			MessageBox(NULL, "Unable to create export file!", "Error", MB_OK | MB_ICONERROR);
		}
		delete[] buffer;
		return 1;
	} else {
		delete[] buffer;
		return 0;
	}
}

void SetFileNameWithEllipsis(HWND hStatic, const char* fileName) {
	char DisplayBuf[MAX_PATH + 30];
	int len = strlen(fileName);

	if (len <= 8) {
		sprintf(DisplayBuf, "Name: %s", fileName);
	} else {
		char line1[10] = {0};
		char line2[20] = {0};

		strncpy(line1, fileName, 8);
		line1[8] = '\0';

		if (len > 25) {
			strncpy(line2, fileName + 8, 14);
			line2[14] = '\0';
			strcat(line2, "...");
		} else {
			strcpy(line2, fileName + 8);
		}
		sprintf(DisplayBuf, "Name: %s\n%s", line1, line2);
	}
	SetWindowText(hStatic, DisplayBuf);
}

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


bool CreateFolder(HWND hwnd, const char* pathBuf) {
	int ret = SHCreateDirectoryEx(hwnd, pathBuf, NULL);

	char info[256];
	const char* desc = "Unknown error.";

	switch (ret) {
		case ERROR_SUCCESS:
			desc = "Created path successfully!!";
			break;
		case ERROR_ALREADY_EXISTS:
			desc = "Path already exists!!";
			break;
		case ERROR_INVALID_NAME:
			desc = "Invalid path name!!";
			break;
		case ERROR_BAD_PATHNAME:
			desc = "Bad path name!!";
			break;
		case ERROR_FILENAME_EXCED_RANGE:
			desc = "Your path name is too long!!";
			break;
		case ERROR_ACCESS_DENIED:
			desc = "Access denied!!";
			break;
		case ERROR_PATH_NOT_FOUND:
			desc = "Path not found!!";
			break;
		case ERROR_SHARING_VIOLATION:
			desc = "Sharing violation!!";
			break;
		default:
			desc = "Create failed due to unknown error!!";
			break;
	}

	sprintf(info, "[Create Folder] Return Code %d: %s", ret, desc);
	AddLog(hwnd, info);

	return (ret == ERROR_SUCCESS || ret == ERROR_FILE_EXISTS);
}

void LogSocketError(HWND hwnd, const char* action, int errCode) {
	char info[256];
	const char* desc = "Unknown socket error.";

	switch (errCode) {
		case 10061:
			desc = "Connection refused (Target machine actively rejected it).";
			break;
		case 10060:
			desc = "Connection timed out (Host unreachable or firewall blocked).";
			break;
		case 10049:
			desc = "Address not available (Invalid IP address format address format).";
			break;
		case 10022:
			desc = "Invalid argument (WSAStartup not initialized?).";
			break;
		case 10054:
			desc = "Connection reset by peer (Target app crashed or disconnected).";
			break;
		case 10038:
			desc = "Socket operation on non-socket (Invalid socket handle).";
			break;
		default:
			desc = "Please check network status or error code.";
			break;
	}

	// [Connect Failed] Error 10061: Connection refused...
	sprintf(info, "[%s Failed] Error %d: %s", action, errCode, desc);
	AddLog(hwnd, info);
}

DWORD WINAPI SendThread(LPVOID lpParam) {
	TransferData* data = (TransferData*)lpParam;

	SOCKET clientSocket = INVALID_SOCKET;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	wchar_t* wPath = NULL;
	long long remoteOffset = 0;
	long long totalSent = 0;
	long long fileSize = 0;
	DWORD bytesRead;
	LARGE_INTEGER liSize, liMove;
	FileInfo info;
	char buffer[16384];
	char logBuf[512];
	int wLen, recvRes;
	
	// Create Socket
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(data->port));
	serverAddr.sin_addr.S_un.S_addr = inet_addr(data->ip);

	AddLog(data->hwnd, "Attempting to connect to remote host...");
	if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		LogSocketError(data->hwnd, "Connect", WSAGetLastError());
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
		if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
		SetWindowText(GetDlgItem(data->hwnd, ID_CONFIRM), "Confirm and Transfer");
		EnableWindow(GetDlgItem(data->hwnd, ID_CONFIRM), TRUE);
		delete data;
		return 0;
	}
	AddLog(data->hwnd, "Connection established! Initializing transfer...");

	// Open File
	wLen = MultiByteToWideChar(CP_ACP, 0, data->filePath, -1, NULL, 0);
	wPath = new wchar_t[wLen];
	MultiByteToWideChar(CP_ACP, 0, data->filePath, -1, wPath, wLen);

	hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	delete[] wPath;
	wPath = NULL;

	if (hFile == INVALID_HANDLE_VALUE) {
		sprintf(logBuf, "File Open Failed! Error: %lu", GetLastError());
		AddLog(data->hwnd, logBuf);
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
		if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
		SetWindowText(GetDlgItem(data->hwnd, ID_CONFIRM), "Confirm and Transfer");
		EnableWindow(GetDlgItem(data->hwnd, ID_CONFIRM), TRUE);
		delete data;
		return 0;
	}

	// Send file info
	GetFileSizeEx(hFile, &liSize);
	fileSize = liSize.QuadPart;

	memset(&info, 0, sizeof(info));
	const char* pName = strrchr(data->filePath, '\\');
	pName = pName ? pName + 1 : data->filePath;
	strncpy(info.fileName, pName, 259);
	info.fileSize = fileSize;

	send(clientSocket, (char*)&info, sizeof(info), 0);

	// Get the progress
	recvRes = recv(clientSocket, (char*)&remoteOffset, sizeof(remoteOffset), 0);
	if (recvRes > 0 && remoteOffset > 0) {
		sprintf(logBuf, "Resume point found: %I64d MB", remoteOffset / 1024 / 1024);
		AddLog(data->hwnd, logBuf);

		liMove.QuadPart = remoteOffset;
		SetFilePointerEx(hFile, liMove, NULL, FILE_BEGIN);
	}
	totalSent = remoteOffset;

	DWORD startTime = GetTickCount();
	DWORD lastUpdateTime = startTime;
	long long lastSent = remoteOffset;

	while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
		int sent = send(clientSocket, buffer, bytesRead, 0);
		if (sent == SOCKET_ERROR) break;
		totalSent += sent;
		while (GetTickCount() - lastUpdateTime <= 200);
		// Update UI every 500ms
		DWORD currentTime = GetTickCount();
		if (currentTime - lastUpdateTime > 500) {
			int percent = (int)(totalSent * 100 / fileSize);
			SendMessage(GetDlgItem(data->hwnd, ID_PROGRESS_BAR), PBM_SETPOS, percent, 0);

			// Calculate the Speed
			double duration = (currentTime - lastUpdateTime) / 1000.0;
			double speed = (totalSent - lastSent) / 1024.0 / duration; // KB/s

			char speedBuf[128];
			if (speed > 1024)
				sprintf(speedBuf, "%.2f MB/s", speed / 1024.0);
			else
				sprintf(speedBuf, "%.2f KB/s", speed);


			HWND hSpeedStatus = GetDlgItem(data->hwnd, ID_STATIC_SPEED);
			InvalidateRect(hSpeedStatus, NULL, TRUE);
			UpdateWindow(hSpeedStatus);
			char finalBuf[256];
			sprintf(finalBuf, "%-12s", speedBuf);

			SetWindowText(hSpeedStatus, finalBuf);

			// 重置记录点
			lastUpdateTime = currentTime;
			lastSent = totalSent;
		}
	}

	if (totalSent >= fileSize) {
		AddLog(data->hwnd, "Transfer completed successfully!");
		SendMessage(GetDlgItem(data->hwnd, ID_PROGRESS_BAR), PBM_SETPOS, 100, 0);
		SetWindowText(GetDlgItem(data->hwnd, ID_STATIC_SPEED), "Completed!");
	} else AddLog(data->hwnd, "TTransfer aborted or connection lost!");

	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
	SetWindowText(GetDlgItem(data->hwnd, ID_CONFIRM), "Confirm and Transfer");
	EnableWindow(GetDlgItem(data->hwnd, ID_CONFIRM), TRUE);
	delete data;
	return 0;
}

DWORD WINAPI RecvThread(LPVOID lpParam) {
	RecvParam* p = (RecvParam*)lpParam;
	HWND hwnd = p->hwnd;
	char logBuf[512];

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(p->port);

	if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR ||
	        listen(listenSocket, 5) == SOCKET_ERROR) {
		AddLog(hwnd, "Bind/Listen failed!");
		closesocket(listenSocket);
		goto thread_end;
	}
	{
		AddLog(hwnd, "Server started, waiting for connection...");

		sockaddr_in clientAddr;
		int clientAddrLen = sizeof(clientAddr);
		SOCKET acceptSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);

		if (acceptSocket != INVALID_SOCKET) {
			AddLog(hwnd, "Connected by client.");

			// File Information
			FileInfo info;
			if (recv(acceptSocket, (char*)&info, sizeof(info), 0) > 0) {

				wchar_t wFinalPath[MAX_PATH];
				char finalPath[MAX_PATH];
				sprintf(finalPath, "%s\\%s", p->savePath, info.fileName);
				MultiByteToWideChar(CP_ACP, 0, finalPath, -1, wFinalPath, MAX_PATH);

				// Check if the file already exists
				long long localSize = 0;
				HANDLE hFileCheck = CreateFileW(wFinalPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFileCheck != INVALID_HANDLE_VALUE) {
					LARGE_INTEGER sz;
					GetFileSizeEx(hFileCheck, &sz);
					localSize = sz.QuadPart;
					CloseHandle(hFileCheck);

					if (localSize >= info.fileSize) localSize = 0;
				}

				// Send present progress
				send(acceptSocket, (char*)&localSize, sizeof(localSize), 0);

				HANDLE hFile = CreateFileW(wFinalPath, FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

				if (hFile != INVALID_HANDLE_VALUE) {
					sprintf(logBuf, "Resuming: %s from %lld bytes", info.fileName, localSize);
					AddLog(hwnd, logBuf);

					char buffer[16384];
					long long totalReceived = localSize;
					int bytesRead;

					DWORD startTime = GetTickCount();
					DWORD lastUpdateTime = startTime;
					long long lastRecvBytes = localSize;

					while (totalReceived < info.fileSize) {
						// Receive data
						bytesRead = recv(acceptSocket, buffer, sizeof(buffer), 0);
						if (bytesRead <= 0) break;

						// Write
						DWORD bytesWritten;
						WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
						totalReceived += bytesRead;

						DWORD currentTime = GetTickCount();
						if (currentTime - lastUpdateTime >= 500) {

							int percent = (int)(totalReceived * 100 / info.fileSize);
							SendMessage(GetDlgItem(hwnd, ID_PROGRESS_BAR), PBM_SETPOS, percent, 0);

							double duration = (currentTime - lastUpdateTime) / 1000.0;
							if (duration > 0) {
								double speed = (totalReceived - lastRecvBytes) / 1024.0 / duration; // KB/s

								char speedBuf[256];
								if (speed > 1024) {
									sprintf(speedBuf, "%.2f MB/s", speed / 1024.0);
								} else {
									sprintf(speedBuf, "%.2f KB/s", speed);
								}

								HWND hStatus = GetDlgItem(hwnd, ID_STATIC_SPEED);
								InvalidateRect(hStatus, NULL, TRUE);
								UpdateWindow(hStatus);
								SetWindowText(hStatus, speedBuf);
							}

							lastUpdateTime = currentTime;
							lastRecvBytes = totalReceived;
						}
					}
					CloseHandle(hFile);
					SendMessage(GetDlgItem(hwnd, ID_PROGRESS_BAR), PBM_SETPOS, 100, 0);
					SetWindowText(GetDlgItem(hwnd, ID_STATIC_SPEED), "Completed!");
					AddLog(hwnd, totalReceived >= info.fileSize ? "Mission Accomplished!! Check your folder now!" : "Oh no!! Transfer aborted or connection lost!");
				} else {
					AddLog(hwnd, "Error: Cannot open file for writing.");
				}
			}
		}

		closesocket(acceptSocket);
		closesocket(listenSocket);
	}

thread_end:
	EnableWindow(GetDlgItem(hwnd, ID_BUTTON_START_RECV), TRUE);
	SetWindowText(GetDlgItem(hwnd, ID_BUTTON_START_RECV), "Confirm and Receive");
	delete p;
	return 0;
}

bool validPortNumber, validPendingNumber, validIP;
bool recvValidPortNumber;
char chosenFileName[256];

/* This is where all the input to the window goes to */
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {

		case WM_CLOSE: {
			int quit = MessageBox(hwnd, "Do you want to quit?", "Tip", MB_YESNO | MB_ICONQUESTION);
			if (quit == IDYES) DestroyWindow(hwnd);
			return 0;
		}

		/* Upon destruction, tell the main thread to stop */
		case WM_DESTROY: {
			DeleteObject(hWhiteBrush);
			PostQuitMessage(0);
			break;
		}

		case WM_CTLCOLORSTATIC: {
			HDC hdcStatic = (HDC)wParam;
			HWND hStaticWnd = (HWND)lParam;

			if (GetDlgCtrlID(hStaticWnd) == ID_STATIC_SPEED) {
				SetBkMode(hdcStatic, TRANSPARENT);
				SetTextColor(hdcStatic, RGB(0, 0, 0));
				return (LRESULT)hWhiteBrush;
			}

			if (GetDlgCtrlID(hStaticWnd) == ID_INFO_NAME) {
				char szText[MAX_PATH];
				GetWindowText(hStaticWnd, szText, MAX_PATH);

				RECT rect;
				GetClientRect(hStaticWnd, &rect);

				SetBkMode(hdcStatic, TRANSPARENT);
				DrawText(hdcStatic, szText, -1, &rect, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

				return (LRESULT)GetStockObject(WHITE_BRUSH);
			}

			int iCtrlID = GetDlgCtrlID(hStaticWnd);

			if (iCtrlID == ID_IP_FORMAT_ERROR || iCtrlID == ID_FORMAT_ERROR || iCtrlID == ID_TOO_BIG ||
			        iCtrlID == ID_IP_EMPTY || iCtrlID == ID_PENDING_EMPTY || iCtrlID == ID_INVALID_IP ||
			        iCtrlID == ID_STATIC_EMPTY_PORT || iCtrlID == ID_STATIC_INVALID_PORT) {
				SetTextColor(hdcStatic, RGB(255, 0, 0));
			}

			if (iCtrlID == ID_CHOSEN_FILE) {
				SetBkMode(hdcStatic, TRANSPARENT);
				return (LRESULT)GetStockObject(WHITE_BRUSH);
			}

			if (iCtrlID == ID_LOG_DISPLAY) {
				SetBkColor(hdcStatic, RGB(255, 255, 255));
				return (LRESULT)GetStockObject(WHITE_BRUSH);
			}

			SetBkMode(hdcStatic, TRANSPARENT);
			return (LRESULT)GetStockObject(NULL_BRUSH); // Return Null Brush
		}

		case WM_CREATE: {

			hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));

			//Create Fonts
			HFONT hDefaultFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			                                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                                VARIABLE_PITCH, TEXT("Comic Sans MS"));
			HFONT hSystemDefaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			HFONT hTitleFont = CreateFont(36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			                              DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                              VARIABLE_PITCH, TEXT("Comic Sans MS"));
			HFONT hSmallFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			                              DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                              VARIABLE_PITCH, TEXT("Comic Sans MS"));

			HWND hTab = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 10, 80, 615, 360, hwnd, (HMENU)ID_TAB_CTRL, NULL, NULL);
			SendMessage(hTab, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			TCITEM tie;
			tie.mask = TCIF_TEXT;
			tie.pszText = "Send File";
			TabCtrl_InsertItem(hTab, 0, &tie);
			tie.pszText = "Receive File";
			TabCtrl_InsertItem(hTab, 1, &tie);

			HWND hBorders = CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, 640, 480, hwnd, (HMENU)ID_UI_BORDERS, NULL, NULL);
			SetWindowPos(hBorders, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

			//Draw Title
			HWND hTitle = CreateWindow("STATIC", "Hero_Broom's File Transfer", WS_VISIBLE | WS_CHILD | SS_LEFT, 10, 10, 600, 50, hwnd, NULL, NULL, NULL);
			SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, MAKELPARAM(TRUE, 0));

			//Draw Github Link
			HWND hLink = CreateWindowEx(0, WC_LINK,
			                            "(Please support author on <A HREF=\"https://github.com/HeroBroom56\">GitHub</A>!!!)",
			                            WS_VISIBLE | WS_CHILD,
			                            30, 50, 300, 30, hwnd, NULL, NULL, NULL);
			SendMessage(hLink, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			//Draw Send Page

			//Draw Edit Box
			HWND hInputTip1 = CreateWindow("STATIC", "Please input your port number: ", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 110, 380, 50, hwnd, (HMENU)ID_PORT_HINT, NULL, NULL);
			SendMessage(hInputTip1, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			HWND hEditIP = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 282, 110, 316, 26, hwnd, (HMENU)ID_EDIT_IP, NULL, NULL);
			SendMessage(hEditIP, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			SendMessage(hEditIP, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"e.g., 8080");

			HWND hIPErrorTip = CreateWindow("STATIC", "Invalid port (Range: 1000-9999)", WS_CHILD | SS_LEFT, 285, 142, 300, 20, hwnd, (HMENU)ID_IP_FORMAT_ERROR, NULL, NULL);
			SendMessage(hIPErrorTip, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));
			HWND hIPErrorEmpty = CreateWindow("STATIC", "This field is required!!", WS_CHILD | SS_LEFT, 285, 142, 300, 20, hwnd, (HMENU)ID_IP_EMPTY, NULL, NULL);
			SendMessage(hIPErrorEmpty, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));

			//Max Pending Connections

//			HWND hPending = CreateWindow("STATIC", "Max Pending Connections: ", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 165, 300, 50, hwnd, (HMENU)ID_PENDING_HINT, NULL, NULL);
//			SendMessage(hPending, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

//			HWND hEditMaximum = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | SS_LEFT, 242, 165, 356, 26, hwnd, (HMENU)ID_EDIT_PENDING_MAXIMUM, NULL, NULL);
//			SendMessage(hEditMaximum, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
//			SendMessage(hEditMaximum, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"Recommended: 5~20");

//			HWND hTooBig = CreateWindow("STATIC", "Cannot be greater than 99999!!", WS_CHILD | SS_LEFT, 245, 195, 300, 20, hwnd, (HMENU)ID_TOO_BIG, NULL, NULL);
//			SendMessage(hTooBig, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));
//			HWND hFormatError = CreateWindow("STATIC", "Please enter a number!!", WS_CHILD | SS_LEFT, 245, 195, 300, 20, hwnd, (HMENU)ID_FORMAT_ERROR, NULL, NULL);
//			SendMessage(hFormatError, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));
//			HWND hPendingErrorEmpty = CreateWindow("STATIC", "This field is required!!", WS_CHILD | SS_LEFT, 245, 195, 300, 20, hwnd, (HMENU)ID_PENDING_EMPTY, NULL, NULL);
//			SendMessage(hPendingErrorEmpty, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));

			//Target IP

			HWND hIPHint = CreateWindow("STATIC", "Target IP Adress: ", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 165, 300, 50, hwnd, (HMENU)ID_IP_HINT, NULL, NULL);
			SendMessage(hIPHint, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			HWND hEditTargetIP = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | SS_LEFT, 212, 165, 356, 26, hwnd, (HMENU)ID_INPUT_TARGET_IP, NULL, NULL);
			SendMessage(hEditTargetIP, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			SendMessage(hEditTargetIP, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"Enter receiver's IP address");
			HWND hInvalidIP = CreateWindow("STATIC", "Invalid IP address format!!", WS_CHILD | SS_LEFT, 215, 195, 300, 20, hwnd, (HMENU)ID_INVALID_IP, NULL, NULL);
			SendMessage(hInvalidIP, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));


			//Draw Choose File Button
			HWND hChooseFile = CreateWindow("BUTTON", "Select File...", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 25, 220, 150, 30, hwnd, (HMENU)ID_CHOOSE_FILE_BUTTON, NULL, NULL);
			SendMessage(hChooseFile, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Draw Confirm Button
			HWND hConfirm = CreateWindow("BUTTON", "Start Transfer", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 185, 220, 150, 30, hwnd, (HMENU)ID_CONFIRM, NULL, NULL);
			SendMessage(hConfirm, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Draw Copy Log Button
			HWND hCopyLog = CreateWindow("BUTTON", "Export Log Info", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 345, 220, 150, 30, hwnd, (HMENU)ID_COPY_LOG, NULL, NULL);
			SendMessage(hCopyLog, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Draw Receive Page

			//Listen Port Number
			HWND hPortHint = CreateWindow("STATIC", "Please input listen port number: ", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 110, 380, 50, hwnd, (HMENU)ID_STATIC_PORT_HINT, NULL, NULL);
			SendMessage(hPortHint, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			HWND hEditPort = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | SS_LEFT | ES_AUTOHSCROLL, 282, 110, 316, 26, hwnd, (HMENU)ID_EDIT_RECV_PORT, NULL, NULL);
			SendMessage(hEditPort, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			SendMessage(hEditPort, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"e.g., 8080");

			HWND hPortErrorEmpty = CreateWindow("STATIC", "This field is required!!", WS_CHILD | SS_LEFT, 285, 142, 300, 20, hwnd, (HMENU)ID_STATIC_EMPTY_PORT, NULL, NULL);
			SendMessage(hPortErrorEmpty, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));
			HWND hPortErrorTip = CreateWindow("STATIC", "Invalid port (Range: 1000-9999)", WS_CHILD | SS_LEFT, 285, 142, 300, 20, hwnd, (HMENU)ID_STATIC_INVALID_PORT, NULL, NULL);
			SendMessage(hPortErrorTip, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));

			//Browse Direction Button

			HWND hBrowseDir = CreateWindow("BUTTON", "Browse...", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 22, 167, 146, 26, hwnd, (HMENU)ID_BUTTON_BROWSE_DIR, NULL, NULL);
			SendMessage(hBrowseDir, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			HWND hSavePath = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | SS_LEFT | ES_AUTOHSCROLL, 182, 167, 416, 24, hwnd, (HMENU)ID_EDIT_SAVE_PATH, NULL, NULL);
			SendMessage(hSavePath, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			SendMessage(hSavePath, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"Enter or select save directory");

			//Start Reveiving
			HWND hStartReceiving = CreateWindow("BUTTON", "Listen and Receive", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 25, 220, 150, 30, hwnd, (HMENU)ID_BUTTON_START_RECV, NULL, NULL);
			SendMessage(hStartReceiving, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));


			//Open Folder
			HWND hOpenFolder = CreateWindow("BUTTON", "Open Directory", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 185, 220, 150, 30, hwnd, (HMENU)ID_BUTTON_OPEN_FOLDER, NULL, NULL);
			SendMessage(hOpenFolder, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Clear Log Messages

			HWND hClearLog = CreateWindow("BUTTON", "Clear Log", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 505, 220, 80, 30, hwnd, (HMENU)ID_CLEAR_LOG, NULL, NULL);
			SendMessage(hClearLog, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//File Info

			HWND hInfoTitle = CreateWindow("STATIC", "File Info:", WS_VISIBLE | WS_CHILD, 485, 275, 115, 20, hwnd, (HMENU)ID_INFO_HINT, NULL, NULL);
			SendMessage(hInfoTitle, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			HWND hInfoName = CreateWindow("STATIC", "Name: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 485, 305, 115, 40, hwnd, (HMENU)ID_INFO_NAME, NULL, NULL);
			SendMessage(hInfoName, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			HWND hInfoSize = CreateWindow("STATIC", "Size: -", WS_VISIBLE | WS_CHILD, 485, 350, 115, 20, hwnd, (HMENU)ID_INFO_SIZE, NULL, NULL);
			SendMessage(hInfoSize, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			HWND hInfoDate = CreateWindow("STATIC", "Date: -", WS_VISIBLE | WS_CHILD, 485, 380, 115, 45, hwnd, (HMENU)ID_INFO_DATE, NULL, NULL);
			SendMessage(hInfoDate, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Log Display

			HWND hLog = CreateWindowEx(0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 30, 270, 415, 125, hwnd, (HMENU)ID_LOG_DISPLAY, NULL, NULL);
			SendMessage(hLog, WM_SETFONT, (WPARAM)hSystemDefaultFont, MAKELPARAM(TRUE, 0));

			//Progress Bar
			HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;
			HWND hProg = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 25, 410, 325, 20, hwnd, (HMENU)ID_PROGRESS_BAR, hInstance, NULL);
			SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

			//Show speed
			HWND hSpeedShow = CreateWindow("STATIC", "--- KB/s", WS_VISIBLE | WS_CHILD | SS_CENTER, 355, 410, 100, 20, hwnd, (HMENU)ID_STATIC_SPEED, NULL, NULL);
			SendMessage(hSpeedShow, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));


			ShowTabPage(hwnd, 0);

			break;
		}

		case WM_DRAWITEM: {

			LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;

			if (pDIS->CtlID == ID_CHOOSE_FILE_BUTTON || pDIS->CtlID == ID_CONFIRM ||
			        pDIS->CtlID == ID_COPY_LOG || pDIS->CtlID == ID_BUTTON_BROWSE_DIR ||
			        pDIS->CtlID == ID_BUTTON_START_RECV || pDIS->CtlID == ID_BUTTON_OPEN_FOLDER ||
			        pDIS->CtlID == ID_CLEAR_LOG) {
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

			if (pDIS->CtlID == ID_UI_BORDERS) {
				HDC hdc = pDIS->hDC;
				int curSel = TabCtrl_GetCurSel(GetDlgItem(hwnd, ID_TAB_CTRL));

				HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
				SelectObject(hdc, hPen);
				SelectObject(hdc, GetStockObject(NULL_BRUSH));

				if (curSel == 0) {
					RoundRect(hdc, 280, 108, 600, 138, 10, 10);
					RoundRect(hdc, 210, 163, 600, 193, 10, 10);
				} else {
					RoundRect(hdc, 280, 108, 600, 138, 10, 10);
					RoundRect(hdc, 180, 165, 600, 193, 10, 10);
				}

				// Log information and File information
				RoundRect(hdc, 20, 260, 455, 405, 15, 15);
				RoundRect(hdc, 470, 260, 615, 435, 15, 15);

				DeleteObject(hPen);
				return TRUE;
			}
			break;
		}

		case WM_SETCURSOR: {
			HWND hChild = (HWND)wParam;

			if (GetDlgCtrlID(hChild) == ID_CHOOSE_FILE_BUTTON || GetDlgCtrlID(hChild) == ID_CONFIRM ||
			        GetDlgCtrlID(hChild) == ID_COPY_LOG || GetDlgCtrlID(hChild) == ID_BUTTON_BROWSE_DIR ||
			        GetDlgCtrlID(hChild) == ID_BUTTON_START_RECV || GetDlgCtrlID(hChild) == ID_BUTTON_OPEN_FOLDER ||
			        GetDlgCtrlID(hChild) == ID_CLEAR_LOG) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
				return TRUE;
			}

			return DefWindowProc(hwnd, Message, wParam, lParam);
		}

//		case WM_PAINT: {
//			PAINTSTRUCT ps;
//			HDC hdc = BeginPaint(hwnd, &ps);
//
//			HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
//			SelectObject(hdc, hPen);
//			SelectObject(hdc, GetStockObject(NULL_BRUSH));
//
//			DeleteObject(hPen);
//			EndPaint(hwnd, &ps);
//			break;
//		}

		case WM_NOTIFY: {
			LPNMHDR pnmh = (LPNMHDR)lParam;

			if (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN) {
				if (GetDlgCtrlID(pnmh->hwndFrom) != ID_TAB_CTRL) {
					PNMLINK pnmLink = (PNMLINK)lParam;
					ShellExecuteW(NULL, L"open", pnmLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
					return TRUE;
				}
			}

			if (pnmh->idFrom == ID_TAB_CTRL && pnmh->code == TCN_SELCHANGE) {
				int curSel = TabCtrl_GetCurSel(pnmh->hwndFrom);
				ShowTabPage(hwnd, curSel);
			}
			break;
		}

		case WM_LBUTTONDOWN: {
			SetFocus(hwnd);
			break;
		}

		case WM_COMMAND: {

			switch LOWORD(wParam) {

				case ID_INPUT_TARGET_IP: {
					if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS) {
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						HWND hInvalidIP = GetDlgItem(hwnd, ID_INVALID_IP);
						if (IsValidIP(buffer)) {
							validIP = 1;
							ShowWindow(hInvalidIP, SW_HIDE);
						} else {
							validIP = 0;
							ShowWindow(hInvalidIP, SW_SHOW);
						}
					}

					break;
				}

				case ID_EDIT_IP: {
					if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS) {
						HWND hTab = GetDlgItem(hwnd, ID_TAB_CTRL);
						int curSel = TabCtrl_GetCurSel(hTab);
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						int len = strlen(buffer);
						HWND hFormatError = GetDlgItem(hwnd, ID_IP_FORMAT_ERROR);
						HWND hIPEmpty = GetDlgItem(hwnd, ID_IP_EMPTY);
						if (curSel == 1) {
							ShowWindow(hFormatError, SW_HIDE);
							ShowWindow(hIPEmpty, SW_HIDE);
						}
						if (len > 0) {
							bool isFourDigit = 1;
							if (len != 4) isFourDigit = 0;
							else {
								for (int i = 0; i < 4; i++) {
									if (buffer[i] < '0' || buffer[i] > '9') {
										isFourDigit = 0;
										break;
									}
								}
							}

							ShowWindow(hIPEmpty, SW_HIDE);
							if (!isFourDigit) {
								validPortNumber = 0;
								ShowWindow(hFormatError, SW_SHOW);
//								SetFocus((HWND)lParam);
							} else {
								validPortNumber = 1;
								ShowWindow(hFormatError, SW_HIDE);
							}

						} else {
							validPortNumber = 0;
							ShowWindow(hFormatError, SW_HIDE);
							ShowWindow(hIPEmpty, SW_SHOW);
						}
					}

					break;
				}

				case ID_EDIT_PENDING_MAXIMUM: {
					if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS) {
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						HWND hFormatError = GetDlgItem(hwnd, ID_FORMAT_ERROR);
						HWND hTooBigNumber = GetDlgItem(hwnd, ID_TOO_BIG);
						HWND hPendingEmpty = GetDlgItem(hwnd, ID_PENDING_EMPTY);
						int len = strlen(buffer);
						if (len > 0) {
							bool isValid = 1, isTooBig = 0;
							for (int i = 0; i < len; i++) {
								if (buffer[i] < '0' || buffer[i] > '9') {
									isValid = 0;
									break;
								}
							}
							if (isValid && len > 5) isTooBig = 1;

							ShowWindow(hPendingEmpty, SW_HIDE);
							if (!isValid || isTooBig) {
								validPendingNumber = 0;
								if (!isValid) {
									ShowWindow(hFormatError, SW_SHOW);
									ShowWindow(hTooBigNumber, SW_HIDE);
								} else {
									ShowWindow(hFormatError, SW_HIDE);
									ShowWindow(hTooBigNumber, SW_SHOW);
								}
//								SetFocus((HWND)lParam);
							} else {
								validPendingNumber = 1;
								ShowWindow(hFormatError, SW_HIDE);
								ShowWindow(hTooBigNumber, SW_HIDE);
							}
						} else {
							validPendingNumber = 0;
							ShowWindow(hFormatError, SW_HIDE);
							ShowWindow(hTooBigNumber, SW_HIDE);
							ShowWindow(hPendingEmpty, SW_SHOW);
						}
					}

					break;
				}

				case ID_CHOOSE_FILE_BUTTON: {
					char selectedPath[MAX_PATH];
					if (SelectFileToBuffer(hwnd, selectedPath, MAX_PATH)) {
						char* fileName = strrchr(selectedPath, '\\');
						fileName = (fileName != NULL) ? fileName + 1 : selectedPath;
						SetFileNameWithEllipsis(GetDlgItem(hwnd, ID_INFO_NAME), fileName);

						strncpy(chosenFileName, fileName, sizeof(chosenFileName) - 1);
						chosenFileName[sizeof(chosenFileName) - 1] = '\0';

						strncpy(g_szFullFilePath, selectedPath, sizeof(g_szFullFilePath) - 1);
						g_szFullFilePath[sizeof(g_szFullFilePath) - 1] = '\0';

						char logMsg[MAX_PATH + 20];
						sprintf(logMsg, "Source file loaded: %s", fileName);
						AddLog(hwnd, logMsg);

						// Obtain Detailed Information
						WIN32_FILE_ATTRIBUTE_DATA fad;
						if (GetFileAttributesEx(selectedPath, GetFileExInfoStandard, &fad)) {

							char szSize[50];
							unsigned __int64 fullSize = ((unsigned __int64)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
							double fileSize = (double)fullSize / 1024.0; // Convert into KB
							if (fileSize >= 1024) {
								fileSize /= 1024.0;
								if (fileSize >= 1024) {
									fileSize /= 1024.0;
									if (fileSize >= 1024) {
										fileSize /= 1024.0;
										sprintf(szSize, "Size: %.2f TB", fileSize);
									} else {
										sprintf(szSize, "Size: %.2f GB", fileSize);
									}
								} else {
									sprintf(szSize, "Size: %.2f MB", fileSize);
								}
							} else {
								sprintf(szSize, "Size: %.2f KB", fileSize);
							}
							SetWindowText(GetDlgItem(hwnd, ID_INFO_SIZE), szSize);

							// --- 处理创建日期 ---
							SYSTEMTIME st;
							FileTimeToSystemTime(&fad.ftCreationTime, &st);
							char szDate[100];
							sprintf(szDate, "Created:\n%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
							SetWindowText(GetDlgItem(hwnd, ID_INFO_DATE), szDate);

						}

						InvalidateRect(hwnd, NULL, TRUE);
					}
					break;
				}

				case ID_CONFIRM: {

					if (!validIP || !validPortNumber) {
						MessageBox(hwnd, "Invalid IP address format Address or Port Number!!", "Input Error", MB_OK | MB_ICONERROR);
						break;
					}

					if (strlen(chosenFileName) == 0) {
						MessageBox(hwnd, "Please choose a file first!", "No File Selected", MB_OK | MB_ICONWARNING);
						break;
					}

					int result = MessageBox(hwnd, "Are you sure you want to start the file transfer?", "Confirm Transfer", MB_YESNO | MB_ICONQUESTION);

					if (result == IDYES) {
						SendMessage(GetDlgItem(hwnd, ID_PROGRESS_BAR), PBM_SETPOS, 0, 0);
						EnableWindow(GetDlgItem(hwnd, ID_CONFIRM), FALSE);
						SetWindowText(GetDlgItem(hwnd, ID_CONFIRM), "Transferring...");

						TransferData* data = new TransferData;

						data->hwnd = hwnd;
						GetWindowText(GetDlgItem(hwnd, ID_INPUT_TARGET_IP), data->ip, 32);
						GetWindowText(GetDlgItem(hwnd, ID_EDIT_IP), data->port, 10);
						strcpy(data->filePath, g_szFullFilePath);

						HANDLE hThread = CreateThread(NULL, 0, SendThread, (LPVOID)data, 0, NULL);

						if (hThread == NULL) {
							AddLog(hwnd, "Failed to create thread!");
							delete data;
						} else {
							CloseHandle(hThread);
							AddLog(hwnd, "Transfer thread began in background!");
						}
					}
					break;
				}


				case ID_COPY_LOG: {
					if (ExportLogToFile(GetDlgItem(hwnd, ID_LOG_DISPLAY))) {
						AddLog(hwnd, "Log exported!");
					}
					break;
				}

				case ID_BUTTON_BROWSE_DIR: {

					char selectedDir[MAX_PATH] = {0};
					if (SelectFolderOld(hwnd, selectedDir)) {
						SetWindowText(GetDlgItem(hwnd, ID_EDIT_SAVE_PATH), selectedDir);
						strncpy(g_szRecvFolderPath, selectedDir, MAX_PATH);
						g_szRecvFolderPath[MAX_PATH - 1] = '\0';

						char logMsg[MAX_PATH + 50];
						sprintf(logMsg, "Output path set to: %s", selectedDir);
						AddLog(hwnd, logMsg);
					}
					break;
				}

				case ID_EDIT_SAVE_PATH: {
					if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS) {
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						strncpy(g_szRecvFolderPath, buffer, MAX_PATH);
						g_szRecvFolderPath[MAX_PATH - 1] = '\0';

						char logMsg[MAX_PATH + 50];
						sprintf(logMsg, "Save directory changed: %s", g_szRecvFolderPath);
						AddLog(hwnd, logMsg);
					}
					break;
				}

				case ID_CLEAR_LOG: {
					if (MessageBox(hwnd, "Are you sure to clear log information?", "Tip", MB_YESNO | MB_ICONQUESTION) == IDYES) {
						SetWindowText(GetDlgItem(hwnd, ID_LOG_DISPLAY), "");
					}
					break;
				}

				case ID_BUTTON_OPEN_FOLDER: {
					if (strlen(g_szRecvFolderPath) == 0) {
						MessageBox(hwnd, "You haven't select save path yet!!", "Error", MB_OK | MB_ICONERROR);
					} else {
						if (DirectoryExists(g_szRecvFolderPath)) {
							ShellExecute(NULL, "open", g_szRecvFolderPath, NULL, NULL, SW_SHOWNORMAL);
						} else {
							if (MessageBox(hwnd, "Path not exists!!\nAre you sure to create new folder?", "Create New Path", MB_YESNO | MB_ICONQUESTION) == IDYES) {
								CreateFolder(hwnd, g_szRecvFolderPath);
							}
						}
					}
					break;
				}

				case ID_BUTTON_START_RECV: {
					if (strlen(g_szRecvFolderPath) == 0) {
						MessageBox(hwnd, "You haven't select save path yet!!", "Error", MB_OK | MB_ICONERROR);
						break;
					} else {
						if (!DirectoryExists(g_szRecvFolderPath)) {
							if (MessageBox(hwnd, "Path not exists!!\nAre you sure to create new folder?", "Create New Path", MB_YESNO | MB_ICONQUESTION) == IDYES) {
								if (!CreateFolder(hwnd, g_szRecvFolderPath)) break;
							} else {
								break;
							}
						}
					}

					if (!recvValidPortNumber) {
						MessageBox(hwnd, "Invalid port number!!", "Error", MB_OK | MB_ICONERROR);
						break;
					}

					int result = MessageBox(hwnd, "Are you sure to begin receiving file?", "Confirm Receive", MB_YESNO | MB_ICONQUESTION);
					if (result == IDYES) {
						SendMessage(GetDlgItem(hwnd, ID_PROGRESS_BAR), PBM_SETPOS, 0, 0);
						EnableWindow((HWND)lParam, FALSE);
						SetWindowText((HWND)lParam, "Receiving...");

						char portBuf[16], pathBuf[MAX_PATH];
						GetWindowText(GetDlgItem(hwnd, ID_EDIT_RECV_PORT), portBuf, 16);
						GetWindowText(GetDlgItem(hwnd, ID_EDIT_SAVE_PATH), pathBuf, MAX_PATH);

						RecvParam* rp = new RecvParam;
						rp->hwnd = hwnd;
						rp->port = atoi(portBuf);
						strcpy(rp->savePath, pathBuf);

						CreateThread(NULL, 0, RecvThread, rp, 0, NULL);
					}

					break;
				}

				case ID_EDIT_RECV_PORT: {
					if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS) {
						HWND hTab = GetDlgItem(hwnd, ID_TAB_CTRL);
						int curSel = TabCtrl_GetCurSel(hTab);
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						int len = strlen(buffer);
						HWND hFormatError = GetDlgItem(hwnd, ID_STATIC_INVALID_PORT);
						HWND hIPEmpty = GetDlgItem(hwnd, ID_STATIC_EMPTY_PORT);
						if (curSel == 0) {
							ShowWindow(hFormatError, SW_HIDE);
							ShowWindow(hIPEmpty, SW_HIDE);
						}
						if (len > 0) {
							bool isFourDigit = 1;
							if (len != 4) isFourDigit = 0;
							else {
								for (int i = 0; i < 4; i++) {
									if (buffer[i] < '0' || buffer[i] > '9') {
										isFourDigit = 0;
										break;
									}
								}
							}

							ShowWindow(hIPEmpty, SW_HIDE);
							if (!isFourDigit) {
								recvValidPortNumber = 0;
								ShowWindow(hFormatError, SW_SHOW);
							} else {
								recvValidPortNumber = 1;
								ShowWindow(hFormatError, SW_HIDE);
							}

						} else {
							recvValidPortNumber = 0;
							ShowWindow(hFormatError, SW_HIDE);
							ShowWindow(hIPEmpty, SW_SHOW);
						}
					}

					break;
				}

				default:
					break;
			}
			break;
		}

		/* All other messages (a lot of them) are processed using default procedures */
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

/* The 'main' function of Win32 GUI programs: this is where execution starts */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	InitCommonControls();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) {
		CoInitialize(NULL);
	}

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&icex);

	WNDCLASSEX wc; /* A properties struct of our window */
	HWND hwnd; /* A 'HANDLE', hence the H, or a pointer to our window */
	MSG msg; /* A temporary location for all messages */

	/* zero out the struct and set the stuff we want to modify */
	memset(&wc, 0, sizeof(wc));
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.lpfnWndProc	 = WndProc; /* This is where we will send messages to */
	wc.hInstance	 = hInstance;
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);

	/* White, COLOR_WINDOW is just a #define for a system color, try Ctrl+Clicking it */
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = "WindowClass";
	wc.hIcon		 = LoadIcon(GetModuleHandle(NULL), "MAINICON"); /* Load a standard icon */
	wc.hIconSm		 = LoadIcon(GetModuleHandle(NULL), "MAINICON"); /* use the name "A" to use the project icon */

	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "WindowClass", "File Transfer",
	                      WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	                      CW_USEDEFAULT, /* x */
	                      CW_USEDEFAULT, /* y */
	                      640, /* width */
	                      480, /* height */
	                      NULL, NULL, hInstance, NULL);
	
	// FirstRun
		char firstRun[10] = {0};
		GetPrivateProfileString("Status", "FirstRun", "Yes", firstRun, 10, ".\\config.ini");
		
		if (strcmp(firstRun, "Yes") == 0) {
		    // Welcome
		    const char* guideText = 
			    "Hey! Welcome to Hero_Broom's File Transfer!!\n"
			    "------------------------------------------\n"
			    "1. [Quick Start]\n"
			    "   - Sending: Select File -> Enter Target IP -> GO!!\n"
			    "   - Receiving: Set Port -> Select Path -> Click Listen!!\n\n"
			    "2. [The 'Secret' of IP]\n"
			    "   - [IMPORTANT!!!] Make sure you and your friend are in the SAME LAN!!\n"
			    "   - Use 'ipconfig' in CMD to find your IP if you're lost!!\n\n"
			    "3. [Breakpoint Resume]\n"
			    "   - Connection lost? NO PROBLEM!! Just start again,\n"
			    "     it will resume from where it stopped!!\n\n"
			    "4. [Pro Tip]\n"
			    "   - If it's stuck, check your Firewall settings!!\n"
			    "   - Don't leave the Port field empty or it will CRY!!\n\n"
			    "Have fun transferring big files!! (Support me on GitHub!!)";
		
		    MessageBox(hwnd, guideText, "First Run Guide", MB_OK | MB_ICONINFORMATION);
		
		    WritePrivateProfileString("Status", "FirstRun", "No", ".\\config.ini");
		}

	if (hwnd == NULL) {
		MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		MessageBox(hwnd, "WinSock init failed!", "Error", MB_OK);
		return 0;
	}


	/*
		This is the heart of our program where all input is processed and
		sent to WndProc. Note that GetMessage blocks code flow until it receives something, so
		this loop will not produce unreasonably high CPU usage
	*/
	while (GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
		TranslateMessage(&msg); /* Translate key codes to chars if present */
		DispatchMessage(&msg); /* Send it to WndProc */
	}

	WSACleanup();
	CoUninitialize();

	return msg.wParam;
}
