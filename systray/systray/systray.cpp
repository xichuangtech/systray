// systray.cpp : Defines the exported functions for the DLL application.
//

// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "systray.h"
#include <new>

// Message posted into message loop when Notification Icon is clicked
#define WM_SYSTRAY_MESSAGE (WM_USER + 1)

static NOTIFYICONDATA nid;
static HWND hWnd;
static HMENU hTrayMenu;

void (*systray_menu_item_selected)(int menu_id);
void (*systray_leftmouse_clicked)();

void reportWindowsError(const char* action) {
	LPTSTR pErrMsg = NULL;
	DWORD errCode = GetLastError();
	DWORD result = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
			FORMAT_MESSAGE_FROM_SYSTEM|
			FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL,
			errCode,
			LANG_NEUTRAL,
			pErrMsg,
			0,
			NULL);
	printf("Systray error %s: %d %ls\n", action, errCode, pErrMsg);
}

void ShowMenu(HWND hWnd) {
	POINT p;
	if (0 == GetCursorPos(&p)) {
		reportWindowsError("get tray menu position");
		return;
	};
	SetForegroundWindow(hWnd); // Win32 bug work-around
	TrackPopupMenu(hTrayMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);

}

int GetMenuItemId(int index) {
	MENUITEMINFO menuItemInfo;
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_DATA;
	if (0 == GetMenuItemInfo(hTrayMenu, index, TRUE, &menuItemInfo)) {
		reportWindowsError("get menu item id");
		return -1;
	}
	return menuItemInfo.dwItemData;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_MENUCOMMAND:
			{
				int menuId = GetMenuItemId(wParam);
				if (menuId != -1) {
					systray_menu_item_selected(menuId);
				}
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SYSTRAY_MESSAGE:
			switch(lParam) {
				case WM_RBUTTONUP:
					ShowMenu(hWnd);
					break;
				case WM_LBUTTONUP:
					systray_leftmouse_clicked();
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			};
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void MyRegisterClass(HINSTANCE hInstance, TCHAR* szWindowClass) {
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = 0;
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow, TCHAR* szWindowClass) {
	HWND hWnd = CreateWindow(szWindowClass, TEXT(""), WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	if (!hWnd) {
		return 0;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

HBITMAP IconToBitmap(HICON hIcon, SIZE* pTargetSize = NULL)
{
	ICONINFO info = { 0 };
	if (hIcon == NULL
		|| !GetIconInfo(hIcon, &info)
		|| !info.fIcon)
	{
		return NULL;
	}

	INT nWidth = 0;
	INT nHeight = 0;
	if (pTargetSize != NULL)
	{
		nWidth = pTargetSize->cx;
		nHeight = pTargetSize->cy;
	}
	else
	{
		if (info.hbmColor != NULL)
		{
			BITMAP bmp = { 0 };
			GetObject(info.hbmColor, sizeof(bmp), &bmp);

			nWidth = bmp.bmWidth;
			nHeight = bmp.bmHeight;
		}
	}

	if (info.hbmColor != NULL)
	{
		DeleteObject(info.hbmColor);
		info.hbmColor = NULL;
	}

	if (info.hbmMask != NULL)
	{
		DeleteObject(info.hbmMask);
		info.hbmMask = NULL;
	}

	if (nWidth <= 0
		|| nHeight <= 0)
	{
		return NULL;
	}

	INT nPixelCount = nWidth * nHeight;

	HDC dc = GetDC(NULL);
	INT* pData = NULL;
	HDC dcMem = NULL;
	HBITMAP hBmpOld = NULL;
	bool* pOpaque = NULL;
	HBITMAP dib = NULL;
	BOOL bSuccess = FALSE;

	do
	{
		BITMAPINFOHEADER bi = { 0 };
		bi.biSize = sizeof(BITMAPINFOHEADER);
		bi.biWidth = nWidth;
		bi.biHeight = -nHeight;
		bi.biPlanes = 1;
		bi.biBitCount = 32;
		bi.biCompression = BI_RGB;
		dib = CreateDIBSection(dc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (VOID**)&pData, NULL, 0);
		if (dib == NULL) break;

		memset(pData, 0, nPixelCount * 4);

		dcMem = CreateCompatibleDC(dc);
		if (dcMem == NULL) break;

		hBmpOld = (HBITMAP)SelectObject(dcMem, dib);
		::DrawIconEx(dcMem, 0, 0, hIcon, nWidth, nHeight, 0, NULL, DI_MASK);

		pOpaque = new(std::nothrow) bool[nPixelCount];
		if (pOpaque == NULL) break;
		for (INT i = 0; i < nPixelCount; ++i)
		{
			pOpaque[i] = !pData[i];
		}

		memset(pData, 0, nPixelCount * 4);
		::DrawIconEx(dcMem, 0, 0, hIcon, nWidth, nHeight, 0, NULL, DI_NORMAL);

		BOOL bPixelHasAlpha = FALSE;
		UINT* pPixel = (UINT*)pData;
		for (INT i = 0; i<nPixelCount; ++i, ++pPixel)
		{
			if ((*pPixel & 0xff000000) != 0)
			{
				bPixelHasAlpha = TRUE;
				break;
			}
		}

		if (!bPixelHasAlpha)
		{
			pPixel = (UINT*)pData;
			for (INT i = 0; i <nPixelCount; ++i, ++pPixel)
			{
				if (pOpaque[i])
				{
					*pPixel |= 0xFF000000;
				}
				else
				{
					*pPixel &= 0x00FFFFFF;
				}
			}
		}

		bSuccess = TRUE;

	} while (FALSE);


	if (pOpaque != NULL)
	{
		delete[]pOpaque;
		pOpaque = NULL;
	}

	if (dcMem != NULL)
	{
		SelectObject(dcMem, hBmpOld);
		DeleteDC(dcMem);
	}

	ReleaseDC(NULL, dc);

	if (!bSuccess)
	{
		if (dib != NULL)
		{
			DeleteObject(dib);
			dib = NULL;
		}
	}

	return dib;
}



BOOL createMenu() {
	hTrayMenu = CreatePopupMenu();
	MENUINFO menuInfo;
	menuInfo.cbSize = sizeof(MENUINFO);
	menuInfo.fMask = MIM_APPLYTOSUBMENUS | MIM_STYLE;
	menuInfo.dwStyle = MNS_NOTIFYBYPOS;
	return SetMenuInfo(hTrayMenu, &menuInfo);
}

BOOL addNotifyIcon() {
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = 100;
	nid.uCallbackMessage = WM_SYSTRAY_MESSAGE;
	nid.uFlags = NIF_MESSAGE;
	return Shell_NotifyIcon(NIM_ADD, &nid);
}

int nativeLoop(void (*systray_ready)(int ignored), void (*_systray_menu_item_selected)(int menu_id), void(*_systray_leftmouse_clicked)()) {
	systray_menu_item_selected = _systray_menu_item_selected;
	systray_leftmouse_clicked = _systray_leftmouse_clicked;

	HINSTANCE hInstance = GetModuleHandle(NULL);
	TCHAR* szWindowClass = TEXT("SystrayClass");
	MyRegisterClass(hInstance, szWindowClass);
	hWnd = InitInstance(hInstance, FALSE, szWindowClass); // Don't show window
	if (!hWnd) {
		return EXIT_FAILURE;
	}
	if (!createMenu() || !addNotifyIcon()) {
		return EXIT_FAILURE;
	}
	systray_ready(0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}   
	return EXIT_SUCCESS;
}


void setIcon(const wchar_t* iconFile) {
	HICON hIcon = (HICON) LoadImage(NULL, iconFile, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
	if (hIcon == NULL) {
		reportWindowsError("load icon image");
	} else {
		nid.hIcon = hIcon;
		nid.uFlags = NIF_ICON;
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
}

void setTooltip(const wchar_t* tooltip) {
	wcsncpy_s(nid.szTip, tooltip, sizeof(nid.szTip)/sizeof(wchar_t));
	nid.uFlags = NIF_TIP;
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void add_or_update_menu_item(int menuId, wchar_t* title, wchar_t* tooltip, short disabled, short checked) {
	MENUITEMINFO menuItemInfo;
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA | MIIM_STATE;
	menuItemInfo.fType = MFT_STRING;
	menuItemInfo.dwTypeData = title;
	menuItemInfo.cch = wcslen(title) + 1;
	menuItemInfo.dwItemData = (ULONG_PTR)menuId;
	menuItemInfo.fState = 0;
	if (disabled == 1) {
		menuItemInfo.fState |= MFS_DISABLED;
	}
	if (checked == 1) {
		menuItemInfo.fState |= MFS_CHECKED;
	}

	int itemCount = GetMenuItemCount(hTrayMenu);
	int i;
	for (i = 0; i < itemCount; i++) {
		int id = GetMenuItemId(i);
		if (-1 == id) {
			continue;
		}
		if (menuId == id) {
			SetMenuItemInfo(hTrayMenu, i, TRUE, &menuItemInfo);
			break;
		}
	}
	if (i == itemCount) {
		InsertMenuItem(hTrayMenu, -1, TRUE, &menuItemInfo);
	}
}

void add_or_update_menu_item_with_icon(int menuId, wchar_t* title, wchar_t* tooltip, wchar_t* iconFile, short disabled, short checked) {
	SIZE size = { 16,16 };
	HICON hIcon = (HICON)LoadImage(NULL, iconFile, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);

	
	MENUITEMINFO menuItemInfo;
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_STRING | MIIM_DATA | MIIM_STATE|MIIM_BITMAP;
	menuItemInfo.fType = MFT_STRING|MFT_BITMAP;
	menuItemInfo.dwTypeData = title;
	menuItemInfo.cch = wcslen(title) + 1;
	menuItemInfo.dwItemData = (ULONG_PTR)menuId;
	menuItemInfo.fState = 0;
	if (disabled == 1) {
		menuItemInfo.fState |= MFS_DISABLED;
	}
	if (checked == 1) {
		menuItemInfo.fState |= MFS_CHECKED;
	}
	
	HBITMAP hbitmap = IconToBitmap(hIcon, &size);
	menuItemInfo.hbmpItem = hbitmap;

	int itemCount = GetMenuItemCount(hTrayMenu);
	int i;
	for (i = 0; i < itemCount; i++) {
		int id = GetMenuItemId(i);
		if (-1 == id) {
			continue;
		}
		if (menuId == id) {
			BOOL b = SetMenuItemInfo(hTrayMenu, i, TRUE, &menuItemInfo);
			if (!b) {
				reportWindowsError("InsertMenuItem");
			}
			break;
		}
	}
	if (i == itemCount) {
	  BOOL b=	InsertMenuItem(hTrayMenu, -1, TRUE, &menuItemInfo);
	  if (!b) {
		  reportWindowsError("InsertMenuItem");
	  }
	}
}


void quit() {
	Shell_NotifyIcon(NIM_DELETE, &nid);
}
