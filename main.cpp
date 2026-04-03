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
#include <shellapi.h>

#include <vector>

#include "definition.h"
#include "global.h"
#include "file.h"
#include "basic.h"


using std::ifstream;
using std::ofstream;
using std::ios;

#ifndef EM_SETCUEBANNER
#define ECM_FIRST 0x1500
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

#define RADAR_PORT 11451
#define SEARCH_MSG "DISCOVER_PORTAL"
#define REPLY_PREFIX "PORTAL_REPLY:"

WSADATA wsaData;
char g_szFullFilePath[MAX_PATH];
char g_szRecvFolderPath[MAX_PATH];
HBRUSH hWhiteBrush = NULL;
HFONT hFontBold = NULL;

struct PeerInfo {
	char name[256];
	char ip[32];
	DWORD lastSeen;
	bool isOnline;
};
CRITICAL_SECTION g_PeerLock;
std::vector<PeerInfo> g_PeerList;

struct TransferData {
	HWND hwnd;
	char ip[32];
	char port[10];
	char filePath[MAX_PATH];
};

struct FileInfo {
	char fileName[260];
	long long fileSize;
	char senderName[256];
};

struct RecvParam {
	HWND hwnd;
	int port;
	char savePath[MAX_PATH];
};

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
		ID_COPY_LOG, ID_CLEAR_LOG,
	};

	int recvControls[] = {
		ID_EDIT_RECV_PORT, ID_BUTTON_BROWSE_DIR, ID_STATIC_SAVE_PATH, ID_BUTTON_START_RECV, ID_BUTTON_STOP_RECV, ID_STATIC_PORT_HINT,
		ID_EDIT_SAVE_PATH, ID_BUTTON_OPEN_FOLDER,
		ID_COPY_LOG, ID_CLEAR_LOG,
		ID_STATIC_SENDER_HINT, ID_STATIC_SENDER_NAME, ID_STATIC_SENDER_IP,
	};

	int radarControls[] = {
		ID_LISTBOX_LAN_RADAR,
	};

	int sendErrorControls[] = {
		ID_IP_FORMAT_ERROR, ID_IP_EMPTY, ID_TOO_BIG, ID_FORMAT_ERROR, ID_PENDING_EMPTY, ID_INVALID_IP,
	};

	int recvErrorControls[] = {
		ID_STATIC_EMPTY_PORT, ID_STATIC_INVALID_PORT,
	};

	int settingsControls[] = {
		ID_LIST_BOX_SETTINGS_OPTION,
		ID_GROUPBOX_GENERAL,
		ID_GRUOPBOX_NETWORK,
		ID_GROUPBOX_ABOUT,
	};

	int helpHideControls[] = {
		ID_LOG_DISPLAY, ID_STATIC_SPEED, ID_PROGRESS_BAR,
	};

	for (int id : sendControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	for (int id : recvControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	for (int id : radarControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	for (int id : sendErrorControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	for (int id : recvErrorControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	for (int id : settingsControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);

	if (pageIndex == TAB_SETTINGS) {
		for (int id : helpHideControls) ShowWindow(GetDlgItem(hwnd, id), SW_HIDE);
	} else {
		for (int id : helpHideControls) ShowWindow(GetDlgItem(hwnd, id), SW_SHOW);
	}

	if (pageIndex == TAB_SEND) {
		for (int id : sendControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_SHOW);
	} else if (pageIndex == TAB_RECEIVE) {
		for (int id : recvControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_SHOW);
	} else if (pageIndex == TAB_RADAR) {
		for (int id : radarControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_SHOW);
	} else if (pageIndex == TAB_SETTINGS) {
		for (int id : settingsControls)
			ShowWindow(GetDlgItem(hwnd, id), SW_SHOW);
		ShowSettingsPage(hwnd, 0);
	}
	InvalidateRect(hwnd, NULL, TRUE);
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

void UpdateSenderInfo(HWND hwnd, char* senderName, char* senderIP) {
	char displayName[64], displayIP[32], logInfo[128];

	sprintf(displayName, "Name: %s", senderName);
	sprintf(displayIP, "IP: %s", senderIP);
	SetWindowText(GetDlgItem(hwnd, ID_STATIC_SENDER_NAME), displayName);
	SetWindowText(GetDlgItem(hwnd, ID_STATIC_SENDER_IP), displayIP);

	sprintf(logInfo, "[Receive] Sender info obtained!! Name: %s\tIP: %s", senderName, senderIP);
	AddLog(hwnd, logInfo);
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

	AddLog(data->hwnd, "[Send] Attempting to connect to remote host...");
	if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		LogSocketError(data->hwnd, "Connect", WSAGetLastError());
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
		if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
		SetWindowText(GetDlgItem(data->hwnd, ID_CONFIRM), "Confirm and Transfer");
		EnableWindow(GetDlgItem(data->hwnd, ID_CONFIRM), TRUE);
		delete data;
		return 0;
	}
	AddLog(data->hwnd, "[Send] Connection established! Initializing transfer...");

	// Open File
	wLen = MultiByteToWideChar(CP_ACP, 0, data->filePath, -1, NULL, 0);
	wPath = new wchar_t[wLen];
	MultiByteToWideChar(CP_ACP, 0, data->filePath, -1, wPath, wLen);

	hFile = CreateFileW(wPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	delete[] wPath;
	wPath = NULL;

	if (hFile == INVALID_HANDLE_VALUE) {
		sprintf(logBuf, "[Send] File Open Failed! Error: %lu", GetLastError());
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
	strcpy(info.senderName, g_Settings.nickname);
//	DWORD size = sizeof(info.senderName);
//	GetComputerName(info.senderName, &size);

	send(clientSocket, (char*)&info, sizeof(info), 0);
	AddLog(data->hwnd, "[Send] Waiting for recipient to confirm...");

	char accpeted[16];
	recv(clientSocket, accpeted, sizeof(accpeted), 0);

	if (strcmp(accpeted, "Rejected") == 0) {
		MessageBox(data->hwnd, "Sender rejected.", "Transfer interrupted", MB_OK | MB_ICONINFORMATION);
		AddLog(data->hwnd, "[Send] Send request was rejected.");
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
		if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
		SetWindowText(GetDlgItem(data->hwnd, ID_CONFIRM), "Confirm and Transfer");
		EnableWindow(GetDlgItem(data->hwnd, ID_CONFIRM), TRUE);
		delete data;
		return 0;
	}

	AddLog(data->hwnd, "[Send] Recipient accepted!! Start to transfer...");

	// Get the progress
	recvRes = recv(clientSocket, (char*)&remoteOffset, sizeof(remoteOffset), 0);
	if (recvRes > 0 && remoteOffset > 0) {
		sprintf(logBuf, "[Send] Resume point found: %I64d MB", remoteOffset / 1024 / 1024);
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

			lastUpdateTime = currentTime;
			lastSent = totalSent;
		}
	}

	if (totalSent >= fileSize) {
		AddLog(data->hwnd, "[Send] Transfer completed successfully!");
		SendMessage(GetDlgItem(data->hwnd, ID_PROGRESS_BAR), PBM_SETPOS, 100, 0);
		SetWindowText(GetDlgItem(data->hwnd, ID_STATIC_SPEED), "Completed!");
	} else AddLog(data->hwnd, "[Send] Transfer aborted or connection lost!");

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
		AddLog(hwnd, "[Receive] Bind/Listen failed!");
		closesocket(listenSocket);
		goto thread_end;
	}
	{
		AddLog(hwnd, "[Receive] Server started, waiting for connection...");

		sockaddr_in clientAddr;
		int clientAddrLen = sizeof(clientAddr);
		SOCKET acceptSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);

		if (acceptSocket != INVALID_SOCKET) {
			AddLog(hwnd, "[Receive] Connected by client.");

			// File Information
			FileInfo info;
			if (recv(acceptSocket, (char*)&info, sizeof(info), 0) > 0) {

				char* ipStr = inet_ntoa(clientAddr.sin_addr);
				char* nameStr = info.senderName;
				UpdateSenderInfo(hwnd, nameStr, ipStr);

				char returnMessage[16], msgBox[128];
				sprintf(msgBox, "%s wants to send you a file. Do you want to accept it?\n\nSender's IP: %s", nameStr, ipStr);
				if (MessageBox(hwnd, msgBox, "Send Request", MB_YESNO | MB_ICONINFORMATION) == IDYES) strcpy(returnMessage, "Accepted");
				else {
					AddLog(hwnd, "[Receive] Rejected request!!");
					strcpy(returnMessage, "Rejected");
					send(acceptSocket, returnMessage, sizeof(returnMessage), 0);
					closesocket(listenSocket);
					closesocket(acceptSocket);
					goto thread_end;
				}
				AddLog(hwnd, "[Receive] Accepted request!!");
				send(acceptSocket, returnMessage, sizeof(returnMessage), 0);

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
					sprintf(logBuf, "[Receive] Resuming: %s from %lld bytes", info.fileName, localSize);
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
					AddLog(hwnd, totalReceived >= info.fileSize ? "[Receive] Mission Accomplished!! Check your folder now!" : "[Receive] Oh no!! Transfer aborted or connection lost!");

//					ShowNotification(NULL, "Transfer Mission Accomplished!!", "Hero Broom's File Transfer", NIIF_INFO, 5000);
					CreateNotificationThread("Transfer Mission Accomplished!!", "Hero Broom's File Transfer");
				} else {
					AddLog(hwnd, "[Receive] Error: Cannot open file for writing.");
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

LRESULT CALLBACK HeaderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	if (uMsg == WM_SETCURSOR) {
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return TRUE;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI RadarSenderThread(LPVOID lpParam) {
	HWND hwnd = (HWND)lpParam;
//	AddLog(hwnd, (char*)"[Radar] RadarSenderThread running!!!");
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	BOOL bOpt = TRUE;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOpt, sizeof(bOpt));

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(RADAR_PORT);
	addr.sin_addr.s_addr = INADDR_BROADCAST;

	while (true) {
		addr.sin_addr.s_addr = INADDR_BROADCAST;
		sendto(sock, SEARCH_MSG, strlen(SEARCH_MSG), 0, (sockaddr*)&addr, sizeof(addr));

		EnterCriticalSection(&g_PeerLock);

		DWORD now = GetTickCount();
		for (size_t i = 0; i < g_PeerList.size(); i++) {
//			char log[128];
//			sprintf(log, "[Radar] Last seen %s in %d. (Currently %d, isOnline = %d)", g_PeerList[i].name, (int)g_PeerList[i].lastSeen, (int)now, (int)g_PeerList[i].isOnline);
//			AddLog(hwnd, log);

			if (g_PeerList[i].isOnline && (now - g_PeerList[i].lastSeen > 6000)) {
				g_PeerList[i].isOnline = 0;
//				AddLog(hwnd, "[Radar] Offline information posted!!");
				PostMessage(hwnd, WM_UPDATE_RADAR_STATUS, i, 0);
			}
//			else if (!g_PeerList[i].isOnline && (now - g_PeerList[i].lastSeen <= 6000)) {
//				g_PeerList[i].isOnline = 1;
////				AddLog(hwnd, "[Radar] Online information posted!!");
//				PostMessage(hwnd, WM_UPDATE_RADAR_STATUS, i, 1);
//			}

		}

		LeaveCriticalSection(&g_PeerLock);

//		AddLog(hwnd, (char*)"[Sender] Broadcast and Local signals sent!");
		Sleep(2000);
	}
	closesocket(sock);
	return 0;
}

DWORD WINAPI RadarReceiverThread(LPVOID lpParam) {
	HWND hwnd = (HWND)lpParam;

	Sleep(200);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		AddLog(hwnd, (char*)"[Radar] Socket creation failed!");
		return 0;
	}

	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));

	sockaddr_in localAddr;
	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons((unsigned short)RADAR_PORT);
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
//	localAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		char errorMsg[100];
		sprintf(errorMsg, "[Radar] Bind failed! Error code: %d", err);
		AddLog(hwnd, errorMsg);
		closesocket(sock);
		return 0;
	}

//	char errorMsg[100];
//	sprintf(errorMsg, "[Radar] Bind Return code: %d", WSAGetLastError());
//	AddLog(hwnd, errorMsg);
//	AddLog(hwnd, (char*)"[Radar] System online, listening...");

	char buf[1024];
	sockaddr_in remoteAddr;
	int addrLen = sizeof(remoteAddr);

	while (true) {
		int len = recvfrom(sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&remoteAddr, &addrLen);
		if (len > 0) {
			buf[len] = '\0';
			char* senderIP = inet_ntoa(remoteAddr.sin_addr);
			char myHostName[256];
			strcpy(myHostName, g_Settings.nickname);
//			gethostname(myHostName, 256);
//			char logMsg[1024];
//			sprintf(logMsg, "[Raw Message] Received message from %s : %s", senderIP, buf);
//			AddLog(hwnd, logMsg);

			if (strcmp(buf, SEARCH_MSG) == 0) {
				char reply[512];
				sprintf(reply, "%s%s", REPLY_PREFIX, myHostName);

				sockaddr_in broadcastAddr;
				memset(&broadcastAddr, 0, sizeof(broadcastAddr));
				broadcastAddr.sin_family = AF_INET;
				broadcastAddr.sin_port = htons((unsigned short)RADAR_PORT);
				broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST; // ȫ�ֹ㲥

				sendto(sock, reply, (int)strlen(reply), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
//				AddLog(hwnd, (char*)"[Reply] Broadcasted my presence to the network!");
			}

			else if (strncmp(buf, REPLY_PREFIX, strlen(REPLY_PREFIX)) == 0) {
				char* peerName = buf + strlen(REPLY_PREFIX);

				if (strcmp(peerName, myHostName) != 0) {
//					AddLog(hwnd, (char*)"[!!!] SUCCESS! Found a reply. Updating table...");

					PeerInfo* info = new PeerInfo;
					strncpy(info->name, peerName, 63);
					strncpy(info->ip, senderIP, 31);

					bool found = 0;
					EnterCriticalSection(&g_PeerLock);
					for (size_t i = 0; i < g_PeerList.size(); i++) {
						if (strcmp(g_PeerList[i].ip, senderIP) == 0) {
							g_PeerList[i].lastSeen = GetTickCount();
							if (g_PeerList[i].isOnline == 0) {
//								AddLog(hwnd, "[Radar] Online information posted!!");
								char log[128];
								sprintf(log, "[Friends] Heads up! %s just joined the network!!", g_PeerList[i].name);
								AddLog(hwnd, log);

							}
							g_PeerList[i].isOnline = 1;
							PostMessage(hwnd, WM_UPDATE_RADAR_STATUS, i, 1);
							found = 1;
						}
					}

					if (!found) {
//						char log[128];
//						sprintf(log, "[Radar] New peer information obtained: %s from %s!!", peerName, senderIP);
//						AddLog(hwnd, log);

						PeerInfo newRecord;
						strncpy(newRecord.name, peerName, 63);
						strncpy(newRecord.ip, senderIP, 31);
						newRecord.lastSeen = GetTickCount();
						newRecord.isOnline = 1;

//						AddLog(hwnd, "[Radar] Online information posted!!");
						char log[128];
						sprintf(log, "[Friends] Heads up! %s just joined the network!!", newRecord.name);
						AddLog(hwnd, log);
						g_PeerList.push_back(newRecord);

						PostMessage(hwnd, WM_UPDATE_RADAR, (WPARAM)info, 0);
					}

					LeaveCriticalSection(&g_PeerLock);
					
					delete info;
				}
			}
		}
	}

	closesocket(sock);
	return 0;
}

void ShowContextMenu(HWND hwnd, int itemIndex) {
	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, 1001, "Copy IP Adress");
	AppendMenu(hMenu, MF_STRING, 1002, "Send to this Computer");

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)itemIndex);

	POINT pt;
	GetCursorPos(&pt);

	TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
	DestroyMenu(hMenu);
}

void GetLocalIPInfo(char* outName, char* outIP) {
	strcpy(outName, "Unknown-PC");
	strcpy(outIP, "0.0.0.0");

	char szHostName[256];
	if (gethostname(szHostName, sizeof(szHostName)) == 0) {
		strcpy(outName, szHostName);
		hostent* pHost = gethostbyname(szHostName);
		if (pHost != NULL) {
			for (int i = 0; pHost->h_addr_list[i] != NULL; i++) {
				IN_ADDR addr;
				memcpy(&addr, pHost->h_addr_list[i], sizeof(IN_ADDR));
				char* pszIP = inet_ntoa(addr);

				if (strcmp(pszIP, "127.0.0.1") != 0) {
					strcpy(outIP, pszIP);
					break;
				}
			}
		}
	}
}

void InsertRadarListbox(HWND hListBox, int lineNum, char* cpName, char* cpIP, char* cpStatus) {
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.iItem = lineNum;
	lvi.iSubItem = 0;
	lvi.pszText = (char*)"";
	ListView_InsertItem(hListBox, &lvi);

	ListView_SetItemText(hListBox, lineNum, 1, cpName);
	ListView_SetItemText(hListBox, lineNum, 2, cpIP);
	ListView_SetItemText(hListBox, lineNum, 3, cpStatus);
}

void GetConfigPath(char* szPath) {
	char appDataPath[MAX_PATH];

	if (SHGetSpecialFolderPathA(NULL, appDataPath, CSIDL_LOCAL_APPDATA, TRUE)) {
		sprintf(szPath, "%s\\HBTF", appDataPath);
		CreateDirectoryA(szPath, NULL);
		strcat(szPath, "\\config.ini");
	}
}

void UpdateGlobalSettings(SettingOptions& Settings) {
	char szIniPath[MAX_PATH];
	GetConfigPath(szIniPath);

	WritePrivateProfileString("Status", "FirstRun", Settings.firstStart, szIniPath);

	WritePrivateProfileString("Local PC", "UserName", Settings.nickname, szIniPath);
	WritePrivateProfileString("Local PC", "LocalIP", Settings.localIP, szIniPath);

	WritePrivateProfileString("General", "DefaultSavePath", Settings.savePath, szIniPath);
	WritePrivateProfileString("General", "AutoStart", (Settings.autoStart ? "1" : "0"), szIniPath);
	WritePrivateProfileString("General", "ShowNotification", (Settings.showNotification ? "1" : "0"), szIniPath);

}

void GetGlobalSettings(SettingOptions& Settings) {
	char szIniPath[MAX_PATH];
	GetConfigPath(szIniPath);


	char firstRun[10] = {0};
	char localName[128], localIP[32];
	GetLocalIPInfo(localName, localIP);
	char downloadPath[MAX_PATH];
//	SHGetFolderPath(FOLDERID_Downloads, 0, NULL, &downloadPath);
	SHGetFolderPath(NULL, CSIDL_PROFILE | CSIDL_FLAG_CREATE, NULL, 0, downloadPath);
	strcat(downloadPath, "\\Downloads");

	strcpy(firstRun, Settings.firstStart);
	GetPrivateProfileString("Status", "FirstRun", "Yes", firstRun, 10, szIniPath);
	strcpy(Settings.firstStart, firstRun);

	GetPrivateProfileString("Local PC", "UserName", localName, Settings.nickname, 128, szIniPath);
	GetPrivateProfileString("Local PC", "LocalIP", localIP, Settings.localIP, 32, szIniPath);

	GetPrivateProfileString("General", "DefaultSavePath", downloadPath, Settings.savePath, MAX_PATH, szIniPath);
	Settings.autoStart = GetPrivateProfileInt("General", "AutoStart", 0, szIniPath);
	Settings.showNotification = GetPrivateProfileInt("General", "ShowNotification", 0, szIniPath);

	strcpy(g_szRecvFolderPath, Settings.savePath);

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

		MessageBox(NULL, guideText, "First Run Guide", MB_OK | MB_ICONINFORMATION);

		WritePrivateProfileString("Status", "FirstRun", "No", szIniPath);
	}
}

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

		case WM_INITDIALOG: {
			LOGFONT lf;
			GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lf);
			lf.lfWeight = FW_BOLD;
			hFontBold = CreateFontIndirect(&lf);
		}

		case WM_CTLCOLORSTATIC: {
			HDC hdcStatic = (HDC)wParam;
			HWND hStaticWnd = (HWND)lParam;

			if (GetDlgCtrlID(hStaticWnd) == ID_STATIC_SPEED || GetDlgCtrlID(hStaticWnd) == ID_STATIC_SENDER_IP || GetDlgCtrlID(hStaticWnd) == ID_STATIC_SENDER_NAME) {
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

			//Init Drag and Drop
			DragAcceptFiles(hwnd, TRUE);

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
			HFONT hMiddleFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			                               DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                               VARIABLE_PITCH, TEXT("Comic Sans MS"));
			HFONT hTinyFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			                             DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			                             VARIABLE_PITCH, TEXT("Comic Sans MS"));

			HWND hTab = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 10, 80, 615, 360, hwnd, (HMENU)ID_TAB_CTRL, NULL, NULL);
			SendMessage(hTab, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			TCITEM tie;
			tie.mask = TCIF_TEXT;
			tie.pszText = "Send File";
			TabCtrl_InsertItem(hTab, TAB_SEND, &tie);
			tie.pszText = "Receive File";
			TabCtrl_InsertItem(hTab, TAB_RECEIVE, &tie);
			tie.pszText = "LAN Radar";
			TabCtrl_InsertItem(hTab, TAB_RADAR, &tie);
			tie.pszText = "Settings && Help";
			TabCtrl_InsertItem(hTab, TAB_SETTINGS, &tie);

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

			//Browse Directory Button

			HWND hBrowseDir = CreateWindow("BUTTON", "Browse...", WS_VISIBLE | WS_CHILD | SS_LEFT | BS_OWNERDRAW, 22, 167, 146, 26, hwnd, (HMENU)ID_BUTTON_BROWSE_DIR, NULL, NULL);
			SendMessage(hBrowseDir, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			HWND hSavePath = CreateWindow("EDIT", g_Settings.savePath, WS_VISIBLE | WS_CHILD | SS_LEFT | ES_AUTOHSCROLL, 182, 167, 416, 24, hwnd, (HMENU)ID_EDIT_SAVE_PATH, NULL, NULL);
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

			//LAN Radar Function
//			HWND hRadarList = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", NULL, WS_CHILD | LBS_STANDARD | WS_HSCROLL, 20, 115, 595, 135, hwnd, (HMENU)ID_LISTBOX_LAN_RADAR, GetModuleHandle(NULL), NULL);

			HWND hRadarList = CreateWindowEx(0, WC_LISTVIEW, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL, 20, 115, 595, 135, hwnd, (HMENU)ID_LISTBOX_LAN_RADAR, GetModuleHandle(NULL), NULL);
			ListView_SetExtendedListViewStyle(hRadarList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			SendMessage(hRadarList, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			LVCOLUMN lvc;
			lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
			lvc.fmt = LVCFMT_CENTER;

			lvc.iSubItem = 0;
			lvc.pszText = (char*)"";
			lvc.cx = 0;
			ListView_InsertColumn(hRadarList, 0, &lvc);
			lvc.iSubItem = 1;
			lvc.pszText = (char*)"Computer Name";
			lvc.cx = 190;
			ListView_InsertColumn(hRadarList, 1, &lvc);
			lvc.iSubItem = 2;
			lvc.pszText = (char*)"IP Address";
			lvc.cx = 240;
			ListView_InsertColumn(hRadarList, 2, &lvc);
			lvc.iSubItem = 3;
			lvc.pszText = (char*)"Status";
			lvc.cx = 160;
			ListView_InsertColumn(hRadarList, 3, &lvc);

			char localName[256], localIP[32], displayName[256];
			GetLocalIPInfo(localName, localIP);
			strcpy(localName, g_Settings.nickname);
			sprintf(displayName, "%s (Me)", localName);
			InsertRadarListbox(hRadarList, 0, displayName, localIP, "Active");

//			for (int i = 1; i < 50; i++) {
//			    LVITEM lvi = { 0 };
//			    lvi.mask = LVIF_TEXT;
//			    lvi.iItem = i;
//			    lvi.iSubItem = 0;
//			    lvi.pszText = (char*)"";
//			    ListView_InsertItem(hRadarList, &lvi);
//
//			    char nameBuf[32];
//			    sprintf(nameBuf, "Computer-%02d", i);
//			    ListView_SetItemText(hRadarList, i, 1, nameBuf);
//			    ListView_SetItemText(hRadarList, i, 2, (char*)"192.168.1.100");
//			    ListView_SetItemText(hRadarList, i, 3, (char*)"Active");
//			}

			HWND hHeader = (HWND)SendMessage(hRadarList, LVM_GETHEADER, 0, 0);
			SetWindowSubclass(hHeader, HeaderSubclassProc, 0, 0);

			//Show Sender Info

			HWND hSenderHint = CreateWindow("STATIC", "Sender Info:", WS_VISIBLE | WS_CHILD, 485, 275, 115, 20, hwnd, (HMENU)ID_STATIC_SENDER_HINT, NULL, NULL);
			SendMessage(hSenderHint, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			HWND hSenderName = CreateWindow("STATIC", "Name: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 485, 305, 115, 40, hwnd, (HMENU)ID_STATIC_SENDER_NAME, NULL, NULL);
			SendMessage(hSenderName, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			HWND hSenderIP = CreateWindow("STATIC", "IP Address: -", WS_VISIBLE | WS_CHILD, 485, 335, 115, 20, hwnd, (HMENU)ID_STATIC_SENDER_IP, NULL, NULL);
			SendMessage(hSenderIP, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Settings Page

			HWND hSettingsOption = CreateWindowExA(0, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 20, 115, 180, 330, hwnd, (HMENU)ID_LIST_BOX_SETTINGS_OPTION, NULL, NULL);
			SendMessage(hSettingsOption, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			SendMessage(hSettingsOption, LB_ADDSTRING, 0, (LPARAM)"General");
			SendMessage(hSettingsOption, LB_ADDSTRING, 0, (LPARAM)"Network Config");
			SendMessage(hSettingsOption, LB_ADDSTRING, 0, (LPARAM)"About HBTF");

			//General Settings

			hSettingsGeneral = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 210, 110, 401, 331, hwnd, (HMENU)ID_GROUPBOX_GENERAL, NULL, NULL);
//			HWND hGroupGeneral = CreateWindowExA(0, "BUTTON", "General Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 210, 115, 400, 330, hwnd, (HMENU)ID_GROUPBOX_GENERAL, NULL, NULL);
//			SendMessage(hGroupGeneral, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			HWND hGroupGeneral = CreateWindowExA(0, "BUTTON", "General Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 400, 325, hSettingsGeneral, (HMENU)ID_TITLE_GENERAL, NULL, NULL);
			SendMessage(hGroupGeneral, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			HWND hStaticNick = CreateWindow("STATIC", "Your Nickname (Visible to Others): ", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 30, 300, 25, hSettingsGeneral, (HMENU)ID_STATIC_NICK, NULL, NULL);
			SendMessage(hStaticNick, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			char displayNick[128];
			strcpy(displayNick, g_Settings.nickname);
			HWND hEditNick = CreateWindow("EDIT", displayNick, WS_CHILD | WS_VISIBLE | SS_LEFT | ES_AUTOHSCROLL, 22, 57, 340, 24, hSettingsGeneral, (HMENU)ID_EDIT_NICK, NULL, NULL);
			SendMessage(hEditNick, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			SendMessage(hEditNick, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"Enter your nick name");

			HWND hStaticBrowse = CreateWindow("STATIC", "Default Save Path: ", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 90, 300, 25, hSettingsGeneral, (HMENU)ID_STATIC_DEFAULT_PATH, NULL, NULL);
			SendMessage(hStaticBrowse, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			HWND hEditDefaultPath = CreateWindow("EDIT", g_Settings.savePath, WS_CHILD | WS_VISIBLE | SS_LEFT | ES_AUTOHSCROLL, 22, 117, 240, 24, hSettingsGeneral, (HMENU)ID_EDIT_DEFAULT_PATH, NULL, NULL);
			SendMessage(hEditDefaultPath, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			SendMessage(hEditDefaultPath, EM_SETCUEBANNER, (WPARAM)TRUE, (LPARAM)L"Enter default save path");
			HWND hButtonBrowse = CreateWindow("BUTTON", "Browse...", WS_CHILD | WS_VISIBLE | SS_LEFT | BS_OWNERDRAW, 275, 115, 90, 25, hSettingsGeneral, (HMENU)ID_BUTTON_BROWSE, NULL, NULL);
			SendMessage(hButtonBrowse, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));

			HWND hButtonAutoStart = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW, 22, 162, 16, 16, hSettingsGeneral, (HMENU)ID_CHECK_AUTO_START, NULL, NULL);
			SendMessage(hButtonAutoStart, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			HWND hStaticAutoStart = CreateWindow("STATIC", "Auto Start on Boot", WS_CHILD | WS_VISIBLE | SS_LEFT, 42, 160, 200, 25, hSettingsGeneral, (HMENU)ID_STATIC_AUTO_START, NULL, NULL);
			SendMessage(hStaticAutoStart, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));

			HWND hButtonNotification = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW, 22, 192, 16, 16, hSettingsGeneral, (HMENU)ID_CHECK_NOTIFICATION, NULL, NULL);
			SendMessage(hButtonNotification, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			HWND hStaticNotification = CreateWindow("STATIC", "Enable Notifications (Currently Unavailable)", WS_CHILD | WS_VISIBLE | SS_LEFT, 42, 190, 400, 25, hSettingsGeneral, (HMENU)ID_STATIC_NOTIFICATION, NULL, NULL);
			SendMessage(hStaticNotification, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));


			HWND hButtonGenSave = CreateWindow("BUTTON", "Save Changes", WS_CHILD | WS_VISIBLE | SS_LEFT | BS_OWNERDRAW, 20, 270, 150, 30, hSettingsGeneral, (HMENU)ID_BUTTON_SAVE_GENERAL, NULL, NULL);
			SendMessage(hButtonGenSave, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));

			SetWindowSubclass(hSettingsGeneral, SettingsPageSubclass, 0, 0);

			//Network Settings Page

			hSettingsNetwork = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 210, 110, 401, 331, hwnd, (HMENU)ID_GRUOPBOX_NETWORK, NULL, NULL);

			HWND hGroupNetwork = CreateWindowExA(0, "BUTTON", "Network Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 400, 325, hSettingsNetwork, (HMENU)ID_TITLE_NETWORK, NULL, NULL);
			SendMessage(hGroupNetwork, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			HWND hButtonNetSave = CreateWindow("BUTTON", "Save Changes", WS_CHILD | WS_VISIBLE | SS_LEFT | BS_OWNERDRAW, 20, 270, 150, 30, hSettingsNetwork, (HMENU)ID_BUTTON_SAVE_NETWORK, NULL, NULL);
			SendMessage(hButtonNetSave, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));

//			CreateWindow("STATIC", "Debug Text", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 200, 20, hSettingsNetwork, NULL, NULL, NULL);

			char displayedIP[128];
			sprintf(displayedIP, "Local IP Address: %s", g_Settings.localIP);

			HWND hEditSettingsIP = CreateWindow("STATIC", displayedIP, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 20, 30, 300, 25, hSettingsNetwork, (HMENU)ID_EDIT_READONLY_IP, NULL, NULL);
			SendMessage(hEditSettingsIP, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));
			HWND hButtonCopyIP = CreateWindow("BUTTON", "Copy IP Address", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 255, 28, 140, 25, hSettingsNetwork, (HMENU)ID_BUTTON_COPY_IP, NULL, NULL);
			SendMessage(hButtonCopyIP, WM_SETFONT, (WPARAM)hMiddleFont, MAKELPARAM(TRUE, 0));

			SetWindowSubclass(hSettingsNetwork, SettingsPageSubclass, 1, 0);

			//About Page

			hSettingsAbout = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 210, 110, 401, 331, hwnd, (HMENU)ID_GROUPBOX_ABOUT, NULL, NULL);

			HWND hGroupAbout = CreateWindowExA(0, "BUTTON", "About HBTF", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 400, 325, hSettingsAbout, (HMENU)ID_TITLE_NETWORK, NULL, NULL);
			SendMessage(hGroupAbout, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));

			HWND hLogoStatic = CreateWindowEx(0, "STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP, 20, 35, 256, 256, hSettingsAbout, NULL, GetModuleHandle(NULL), NULL);
//			HBITMAP hBmp = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_ABOUT_LOGO));
			HBITMAP hBmp = (HBITMAP)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_ABOUT_LOGO), IMAGE_BITMAP, 128, 128, LR_COPYFROMRESOURCE);
			if (hBmp != NULL) {
				SendMessage(hLogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
			} else {
				MessageBox(hwnd, "Load Image Failed!!", "Debug", MB_OK);
			}

			HWND hStaticTitle = CreateWindow("STATIC", "Hero_Broom's File Transfer", WS_CHILD | WS_VISIBLE | SS_LEFT, 150, 30, 300, 40, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticTitle, WM_SETFONT, (WPARAM)hDefaultFont, MAKELPARAM(TRUE, 0));
			HWND hStaticVersion = CreateWindow("STATIC", "Version 1.0.1 (Stable Build)", WS_CHILD | WS_VISIBLE | SS_LEFT, 150, 65, 300, 40, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticVersion, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			HWND hStaticAuthor = CreateWindow("STATIC", "Developed by Hero_Broom", WS_CHILD | WS_VISIBLE | SS_LEFT, 150, 85, 300, 40, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticAuthor, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			HWND hStaticCopyright = CreateWindow("STATIC", "Copyright (C) 2026 Hero_Broom", WS_CHILD | WS_VISIBLE | SS_LEFT, 150, 105, 300, 40, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticCopyright, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));
			HWND hStaticIntro = CreateWindow("STATIC", "A lightweight, high-speed LAN file transfer tool.", WS_CHILD | WS_VISIBLE | SS_LEFT, 150, 135, 200, 50, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticIntro, WM_SETFONT, (WPARAM)hSmallFont, MAKELPARAM(TRUE, 0));

			//Draw Github Link
			HWND hStaticLink = CreateWindowEx(0, WC_LINK, "<A HREF=\"https://github.com/HeroBroom56/Hero_Broom-s-File-Transfer\">Repository</A> | <A HREF=\"https://github.com/HeroBroom56\">Author</A> | <A HREF=\"https://github.com/HeroBroom56/Hero_Broom-s-File-Transfer/blob/main/LICENSE\">License</A>", WS_VISIBLE | WS_CHILD | SS_RIGHT, 30, 175, 300, 15, hSettingsAbout, NULL, NULL, NULL);
			SendMessage(hStaticLink, WM_SETFONT, (WPARAM)hTinyFont, MAKELPARAM(TRUE, 0));

			SetWindowSubclass(hSettingsAbout, SettingsPageSubclass, 2, 0);

			CreateThread(NULL, 0, RadarSenderThread, (LPVOID)hwnd, 0, NULL);
			CreateThread(NULL, 0, RadarReceiverThread, (LPVOID)hwnd, 0, NULL);

			ShowTabPage(hwnd, TAB_SEND);
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

				if (curSel == TAB_SEND) {
					//Edit Box
					RoundRect(hdc, 280, 108, 600, 138, 10, 10);
					RoundRect(hdc, 210, 163, 600, 193, 10, 10);
				} else if (curSel == TAB_RECEIVE) {
					RoundRect(hdc, 280, 108, 600, 138, 10, 10);
					RoundRect(hdc, 180, 165, 600, 193, 10, 10);
				} else if (curSel == TAB_RADAR) {
					//Nothing to draw (at least now?)
				} else if (curSel == TAB_SETTINGS) {
					RoundRect(hdc, 18, 113, 202, 434, 8, 8);
				}

				if (curSel != TAB_SETTINGS) {
					// Log information and File information
					RoundRect(hdc, 20, 260, 455, 405, 15, 15);
					RoundRect(hdc, 470, 260, 615, 435, 15, 15);
				}

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

			if (pnmh->code == HDN_ITEMCHANGINGA || pnmh->code == HDN_ITEMCHANGINGW) {
				NMHEADER* pHeader = (NMHEADER*)lParam;
				if (pHeader->pitem->mask & HDI_WIDTH) {
					SetWindowLongPtr(hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			if (pnmh->idFrom == ID_LISTBOX_LAN_RADAR && pnmh->code == NM_RCLICK) {
				NMITEMACTIVATE* nmItem = (NMITEMACTIVATE*)lParam;
				if (nmItem->iItem != -1) {
					ShowContextMenu(hwnd, nmItem->iItem);
				}
			}

			if (pnmh->idFrom == ID_LISTBOX_LAN_RADAR && pnmh->code == NM_DBLCLK) {
				LPNMITEMACTIVATE pnmItem = (LPNMITEMACTIVATE)lParam;
				int iItem = pnmItem->iItem;
				if (iItem != -1) {
					char szIP[32];
					ListView_GetItemText(GetDlgItem(hwnd, ID_LISTBOX_LAN_RADAR), iItem, 2, szIP, sizeof(szIP));

					SetWindowText(GetDlgItem(hwnd, ID_INPUT_TARGET_IP), szIP);
					SendMessage(GetDlgItem(hwnd, ID_TAB_CTRL), TCM_SETCURSEL, TAB_SEND, 0);
					ShowTabPage(hwnd, TAB_SEND);
				}
			}

			if (pnmh->idFrom == ID_LISTBOX_LAN_RADAR && pnmh->code == NM_CUSTOMDRAW) {
				LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

				switch (lplvcd->nmcd.dwDrawStage) {
					case CDDS_PREPAINT: {
						SetWindowLongPtr(hwnd, DWLP_MSGRESULT, (LPARAM)CDRF_NOTIFYITEMDRAW);
						return TRUE;
					}

					case CDDS_ITEMPREPAINT: {
						if (lplvcd->nmcd.dwItemSpec == 0) {
							SelectObject(lplvcd->nmcd.hdc, hFontBold);
							lplvcd->clrText = RGB(0, 102, 204);
							SetWindowLongPtr(hwnd, DWLP_MSGRESULT, (LPARAM)CDRF_NEWFONT);
							return TRUE;
						}
						break;
					}

					default:
						break;
				}
			}

			break;
		}

		case WM_LBUTTONDOWN: {
			SetFocus(hwnd);
			break;
		}

		case WM_COMMAND: {

			switch LOWORD(wParam) {

				case 1001: {
					HWND hRadarList = GetDlgItem(hwnd, ID_LISTBOX_LAN_RADAR);
					int nItem = ListView_GetNextItem(hRadarList, -1, LVNI_SELECTED);
					if (nItem != -1) {
						char ipText[64];
						ListView_GetItemText(hRadarList, nItem, 2, ipText, sizeof(ipText));

						if (OpenClipboard(hwnd)) {
							EmptyClipboard();
							HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, strlen(ipText) + 1);
							if (hGlob) {
								memcpy(GlobalLock(hGlob), ipText, strlen(ipText) + 1);
								GlobalUnlock(hGlob);
								SetClipboardData(CF_TEXT, hGlob);
							}
							CloseClipboard();
							AddLog(hwnd, (char*)"[UI] IP Copied to clipboard!!");
						}
					}
				}

				case 1002: {
					int iItem = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);
					char szIP[32];
					if (iItem != -1) {
						ListView_GetItemText(GetDlgItem(hwnd, ID_LISTBOX_LAN_RADAR), iItem, 2, szIP, sizeof(szIP));

						SetWindowText(GetDlgItem(hwnd, ID_INPUT_TARGET_IP), szIP);
						SendMessage(GetDlgItem(hwnd, ID_TAB_CTRL), TCM_SETCURSEL, TAB_SEND, 0);
						ShowTabPage(hwnd, TAB_SEND);
					}
				}

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
						if (curSel == TAB_RECEIVE) {
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
						sprintf(logMsg, "[UI] Source file loaded: %s", fileName);
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
							AddLog(hwnd, "[Send] Failed to create thread!");
							delete data;
						} else {
							CloseHandle(hThread);
							AddLog(hwnd, "[Send] Transfer thread began in background!");
						}
					}
					break;
				}


				case ID_COPY_LOG: {
					if (ExportLogToFile(GetDlgItem(hwnd, ID_LOG_DISPLAY))) {
						AddLog(hwnd, "[UI] Log exported!");
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
						sprintf(logMsg, "[UI] Output path set to: %s", selectedDir);
						AddLog(hwnd, logMsg);
					}
					break;
				}

				case ID_EDIT_SAVE_PATH: {
					if (HIWORD(wParam) == EN_KILLFOCUS) {
						char buffer[128];
						GetWindowText((HWND)lParam, buffer, 128);
						strncpy(g_szRecvFolderPath, buffer, MAX_PATH);
						g_szRecvFolderPath[MAX_PATH - 1] = '\0';

						char logMsg[MAX_PATH + 50];
						sprintf(logMsg, "[UI] Save directory changed: %s", g_szRecvFolderPath);
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
						if (curSel == TAB_SEND) {
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

				case ID_LIST_BOX_SETTINGS_OPTION: {
					if (HIWORD(wParam) == LBN_SELCHANGE) {
						HWND hListBox = (HWND)lParam;
						int selIndex = (int)SendMessage(hListBox, LB_GETCURSEL, 0, 0);

						if (selIndex != LB_ERR) {
							ShowSettingsPage(hwnd, selIndex);
						}
					}
					break;
				}

				default:
					break;
			}
			break;
		}

		case WM_DROPFILES: {
			HDROP hDrop = (HDROP)wParam;
			char selectedPath[MAX_PATH];

			UINT fileCount = DragQueryFile(hDrop, 0, selectedPath, MAX_PATH);
			if (fileCount > 0) {
				SetFileNameWithEllipsis(GetDlgItem(hwnd, ID_INFO_NAME), selectedPath);
				MessageBox(hwnd, "File loaded via Drag & Drop!!", "Load File", MB_OK | MB_ICONINFORMATION);
				char* fileName = strrchr(selectedPath, '\\');
				fileName = (fileName != NULL) ? fileName + 1 : selectedPath;
				SetFileNameWithEllipsis(GetDlgItem(hwnd, ID_INFO_NAME), fileName);

				strncpy(chosenFileName, fileName, sizeof(chosenFileName) - 1);
				chosenFileName[sizeof(chosenFileName) - 1] = '\0';

				strncpy(g_szFullFilePath, selectedPath, sizeof(g_szFullFilePath) - 1);
				g_szFullFilePath[sizeof(g_szFullFilePath) - 1] = '\0';

				char logMsg[MAX_PATH + 20];
				sprintf(logMsg, "[UI] Source file loaded: %s", fileName);
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

					SYSTEMTIME st;
					FileTimeToSystemTime(&fad.ftCreationTime, &st);
					char szDate[100];
					sprintf(szDate, "Created:\n%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
					SetWindowText(GetDlgItem(hwnd, ID_INFO_DATE), szDate);

				}
				InvalidateRect(hwnd, NULL, TRUE);
			}

			DragFinish(hDrop);

			break;
		}

		case WM_UPDATE_RADAR: {
			PeerInfo* info = (PeerInfo*)wParam;
//			MessageBox(hwnd, "Obtain peer information!!!", "Debug", MB_OK);
			if (!info) return 0;

			HWND hRadarList = GetDlgItem(hwnd, ID_LISTBOX_LAN_RADAR);

			int rowCount = ListView_GetItemCount(hRadarList);
			bool isDuplicate = false;

			for (int i = 0; i < rowCount; i++) {
				char existingIp[32];
				ListView_GetItemText(hRadarList, i, 2, existingIp, 32);
				if (strcmp(existingIp, info->ip) == 0) {
					isDuplicate = true;
					ListView_SetItemText(hRadarList, i, 3, (char*)"Active");
					break;
				}
			}

			if (!isDuplicate) {
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_TEXT;
				lvi.iItem = rowCount;
				lvi.iSubItem = 0;
				lvi.pszText = (char*)"";
				int newIndex = ListView_InsertItem(hRadarList, &lvi);

				ListView_SetItemText(hRadarList, newIndex, 1, info->name);   // Computer Name
				ListView_SetItemText(hRadarList, newIndex, 2, info->ip);     // IP Address
				ListView_SetItemText(hRadarList, newIndex, 3, (char*)"Active"); // Status
			}

			delete info;
			return 0;
		}

		case WM_UPDATE_RADAR_STATUS: {
			int index = (int)wParam;
			int isOnline = (int)lParam;

			HWND hList = GetDlgItem(hwnd, ID_LISTBOX_LAN_RADAR);

//			char updateLog[128];
//			sprintf(updateLog, "[Radar] Update information obtained!! {%s, %s, %d, %s}", g_PeerList[index].name, g_PeerList[index].ip, g_PeerList[index].lastSeen, (g_PeerList[index].isOnline ? "Online" : "Offline"));
//			AddLog(hwnd, updateLog);

			char targetIP[32];
			EnterCriticalSection(&g_PeerLock);
			strcpy(targetIP, g_PeerList[index].ip);
			LeaveCriticalSection(&g_PeerLock);

			int lvIndex = -1;
			int rowCount = ListView_GetItemCount(hList);
			for (int i = 0; i < rowCount; i++) {
				char currentIP[32];
				ListView_GetItemText(hList, i, 2, currentIP, 32);
				if (strcmp(currentIP, targetIP) == 0) {
					lvIndex = i;
					break;
				}
			}

//			sprintf(updateLog, "[Radar] List index found!! The information is in line %d.", lvIndex);
//			AddLog(hwnd, updateLog);

			if (lvIndex != -1) {
				if (isOnline == 0) {
					ListView_SetItemText(hList, lvIndex, 3, "Offline");
					char log[128];
					sprintf(log, "[Friends] Aww, %s just left...", g_PeerList[index].name);
					AddLog(hwnd, log);
				} else {
					ListView_SetItemText(hList, lvIndex, 3, "Active");
				}
			}

			break;
		}

		case WM_UPDATE_CONFIG: {
			UpdateGlobalSettings(g_Settings);
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

	InitializeCriticalSection(&g_PeerLock);

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		MessageBox(NULL, "WinSock init failed!", "Error", MB_OK);
		return 0;
	}

//	DefaultNotification();

	InitCommonControls();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) {
		CoInitialize(NULL);
	}

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

	GetGlobalSettings(g_Settings);

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "WindowClass", "File Transfer",
	                      WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	                      CW_USEDEFAULT, /* x */
	                      CW_USEDEFAULT, /* y */
	                      640, /* width */
	                      480, /* height */
	                      NULL, NULL, hInstance, NULL);

	if (hwnd == NULL) {
		MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}


//	ShowNotification(hwnd, "Test Message", "Title", NIIF_INFO, 5000);

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);


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
	
	DeleteCriticalSection(&g_PeerLock);

	return msg.wParam;
}
