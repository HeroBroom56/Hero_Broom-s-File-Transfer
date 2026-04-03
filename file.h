#ifndef FILE_H
#define FILE_H

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <stdio.h>

using std::ifstream;
using std::ofstream;

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


bool DirectoryExists(const char* path) {
	DWORD dwAttrib = GetFileAttributes(path);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


#endif
