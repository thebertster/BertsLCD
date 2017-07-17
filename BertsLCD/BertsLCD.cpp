// BertsLCD.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "BertsLCD.h"
#include "lmcons.h"
#include "Strsafe.h"
#include "shellapi.h"

#pragma comment(lib, "WtsApi32.lib")
#include "wtsapi32.h"

//#pragma comment(lib, "LogitechLCDLib.lib")
//#include "LogitechLCDLib.h"

#define LOGI_LCD_TYPE_MONO    (0x00000001)
#define LOGI_LCD_TYPE_COLOR   (0x00000002)

#define LOGI_LCD_MONO_BUTTON_0 (0x00000001)
#define LOGI_LCD_MONO_BUTTON_1 (0x00000002)
#define LOGI_LCD_MONO_BUTTON_2 (0x00000004)
#define LOGI_LCD_MONO_BUTTON_3 (0x00000008)

#define LOGI_LCD_COLOR_BUTTON_LEFT   (0x00000100)
#define LOGI_LCD_COLOR_BUTTON_RIGHT  (0x00000200)
#define LOGI_LCD_COLOR_BUTTON_OK     (0x00000400)
#define LOGI_LCD_COLOR_BUTTON_CANCEL (0x00000800)
#define LOGI_LCD_COLOR_BUTTON_UP     (0x00001000)
#define LOGI_LCD_COLOR_BUTTON_DOWN   (0x00002000)
#define LOGI_LCD_COLOR_BUTTON_MENU   (0x00004000)

const int LOGI_LCD_MONO_WIDTH = 160;
const int LOGI_LCD_MONO_HEIGHT = 43;

const int LOGI_LCD_COLOR_WIDTH = 320;
const int LOGI_LCD_COLOR_HEIGHT = 240;

typedef void(__cdecl *LogiLcdShutdownFunc)(void);
typedef void(__cdecl *LogiLcdUpdateFunc)(void);
typedef bool(__cdecl *LogiLcdInitFunc)(wchar_t*, int);
typedef bool(__cdecl *LogiLcdIsConnectedFunc)(int);
typedef bool(__cdecl *LogiLcdMonoSetTextFunc)(int, wchar_t*);
typedef bool(__cdecl *LogiLcdMonoSetBackgroundFunc)(BYTE[]);
LogiLcdShutdownFunc LogiLcdShutdown;
LogiLcdInitFunc LogiLcdInit;
LogiLcdIsConnectedFunc LogiLcdIsConnected;
LogiLcdUpdateFunc LogiLcdUpdate;
LogiLcdMonoSetTextFunc LogiLcdMonoSetText;
LogiLcdMonoSetBackgroundFunc LogiLcdMonoSetBackground;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hLcdLib = NULL;
HINSTANCE hInst = NULL;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
NOTIFYICONDATA niData;

UINT_PTR timer = 0;

BYTE pixels[LOGI_LCD_MONO_WIDTH * LOGI_LCD_MONO_HEIGHT];

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void				DoTick();
void				ShowContextMenu(HWND);
BOOL				InitialiseLCD();

BOOL				lcdInitialised = FALSE;
SYSTEMTIME			lastDt;
wchar_t				lastUser[UNLEN]=TEXT("");
int					counter = 0;
int                 resetCounter = 0;
int					oldProgress = -1;
HWND				hWndMM = NULL;

BOOL				isDisconnected = FALSE;

#define WM_TRAYICON WM_APP
#define ID_NOTIFYICON 1
#define ID_1SECOND 101
#define SCROLL_LENGTH 27
#define LINE_LEN 48
#define SCROLL_PAUSE 3
#define TIMER_RES 1000

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;
	HKEY hLibKey = NULL;
	TCHAR szLogitechLibKey[MAX_LOADSTRING] = TEXT("");
	TCHAR szLogitechLibPath[MAX_PATH + 1];
	DWORD logitechLibPathLen = MAX_PATH + 1;
	DWORD regKeyType;
	BOOL gotLogitechLibPath = FALSE;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_BERTSLCD, szWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDS_LOGITECHLIBKEY, szLogitechLibKey, MAX_LOADSTRING);

	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szLogitechLibKey, 0L, KEY_READ, &hLibKey) == ERROR_SUCCESS)
	{
		gotLogitechLibPath = (RegQueryValueEx(hLibKey, NULL, NULL, &regKeyType, (LPBYTE)szLogitechLibPath, &logitechLibPathLen) == ERROR_SUCCESS);

		RegCloseKey(hLibKey);
	}

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return 0;
	}

	if (gotLogitechLibPath)
	{
		hLcdLib = LoadLibrary(szLogitechLibPath);

		if (hLcdLib == NULL)
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}

	LogiLcdShutdown = (LogiLcdShutdownFunc)GetProcAddress(hLcdLib, "LogiLcdShutdown");
	LogiLcdInit = (LogiLcdInitFunc)GetProcAddress(hLcdLib, "LogiLcdInit");
	LogiLcdIsConnected = (LogiLcdIsConnectedFunc)GetProcAddress(hLcdLib, "LogiLcdIsConnected");
	LogiLcdUpdate = (LogiLcdUpdateFunc)GetProcAddress(hLcdLib, "LogiLcdUpdate");
	LogiLcdMonoSetText = (LogiLcdMonoSetTextFunc)GetProcAddress(hLcdLib, "LogiLcdMonoSetText");
	LogiLcdMonoSetBackground = (LogiLcdMonoSetBackgroundFunc)GetProcAddress(hLcdLib, "LogiLcdMonoSetBackground");

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BERTSLCD));

	lcdInitialised = InitialiseLCD();

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (lcdInitialised) LogiLcdShutdown();

	FreeLibrary(hLcdLib);

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BERTSLCD));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_BERTSLCD);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_BERTSLCD));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;
   ZeroMemory(&niData, sizeof(NOTIFYICONDATA));

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   //ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   niData.uID = ID_NOTIFYICON;
   niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
   niData.hIcon =
	   (HICON)LoadImage(hInstance,
	   MAKEINTRESOURCE(IDI_BERTSLCD),
	   IMAGE_ICON,
	   GetSystemMetrics(SM_CXSMICON),
	   GetSystemMetrics(SM_CYSMICON),
	   LR_DEFAULTCOLOR);
   niData.hWnd = hWnd;
   niData.uCallbackMessage = WM_TRAYICON;
   Shell_NotifyIcon(NIM_ADD, &niData);

   WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION);
   timer = SetTimer(hWnd, ID_1SECOND, TIMER_RES, NULL);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_WTSSESSION_CHANGE:
		switch (wParam)
		{
		case WTS_CONSOLE_DISCONNECT:
			if (timer != 0)
			{
				KillTimer(hWnd, ID_1SECOND);
				timer = 0;
			}
			if (lcdInitialised)
			{
				lcdInitialised = FALSE;
				LogiLcdShutdown();
			}
			isDisconnected = TRUE;
			break;
		case WTS_CONSOLE_CONNECT:
			if (!lcdInitialised)
			{
				lcdInitialised = InitialiseLCD();
			}
			if (timer == 0)
			{
				timer = SetTimer(hWnd, ID_1SECOND, TIMER_RES, NULL);
			}
			isDisconnected = FALSE;
			break;
		}
		break;
	case WM_POWERBROADCAST:
		if (!isDisconnected)
		{
			switch (wParam)
			{
			case PBT_APMSUSPEND:
				if (timer != 0)
				{
					KillTimer(hWnd, ID_1SECOND);
					timer = 0;
				}
				if (lcdInitialised)
				{
					lcdInitialised = FALSE;
					LogiLcdShutdown();
				}
				return TRUE;
			case PBT_APMRESUMEAUTOMATIC:
				if (!lcdInitialised)
				{
					lcdInitialised = InitialiseLCD();
				}
				if (timer == 0)
				{
					timer = SetTimer(hWnd, ID_1SECOND, TIMER_RES, NULL);
				}
				return TRUE;
			}
		}
		return FALSE;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &niData);
		PostQuitMessage(0);
		break;
	case WM_TIMER:
		if (timer != 0) DoTick();
		break;
	case WM_TRAYICON:
		switch (lParam)
		{
		case 516:
			ShowContextMenu(hWnd);
			break;
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void ShowContextMenu(HWND hWnd)
{
	HMENU hMenu;
	HMENU hSubMenu;
	POINT pt;

	GetCursorPos(&pt);

	hMenu = GetMenu(hWnd);
	hSubMenu = GetSubMenu(hMenu, 0);

	SetForegroundWindow(hWnd);
	TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
	PostMessage(hWnd, WM_NULL, 0, 0);
}

void DoTick()
{
	wchar_t date[16];
	wchar_t time[16];
	wchar_t line1[LINE_LEN];
	wchar_t line2[LINE_LEN];
	wchar_t line3[LINE_LEN];
	wchar_t user[UNLEN];
	DWORD	uLen = UNLEN;
	SYSTEMTIME dt;
	HWND	oldhWndMM;

	BOOL updateScreen = FALSE;

	if (!lcdInitialised)
	{
		if (!(lcdInitialised = InitialiseLCD())) return;
	}

	if (!LogiLcdIsConnected(LOGI_LCD_TYPE_MONO))
	{
		if (resetCounter++ > 10)
		{
			LogiLcdShutdown();
			lcdInitialised = false;
		}
		
		return;
	}

	GetLocalTime(&dt);
	GetUserName(user, &uLen);

	if ((dt.wMinute != lastDt.wMinute) || (dt.wHour != lastDt.wHour) || (dt.wDay != lastDt.wDay) || (dt.wMonth != lastDt.wMonth) || (dt.wYear != lastDt.wYear) || CompareStringOrdinal(user, -1, lastUser, -1, FALSE) != CSTR_EQUAL)
	{
		GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &dt, TEXT("HH:mm"), time, 16);
		GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &dt, TEXT("ddd dd MMM"), date, 16, NULL);

		StringCchPrintf(line1, LINE_LEN, TEXT("%s %s  %s"), date, time, user);

		LogiLcdMonoSetText(0, line1);
		LogiLcdMonoSetText(1, TEXT(""));

		updateScreen = TRUE;
		CopyMemory(&lastDt, &dt, sizeof(SYSTEMTIME));
		StringCchCopy(lastUser, UNLEN, user);
	}
	
	oldhWndMM = hWndMM;

	hWndMM = FindWindow(TEXT("Winamp v1.x"), NULL);
	
	if (hWndMM != NULL)
	{
		int status = 0;
		int trackPos = 0;
		int trackLen = 0;
		wchar_t trackTitle[2048];
		size_t trackTitleLen = 0;
		int progress = 0;

		if (oldhWndMM == NULL) oldProgress = -1;

		GetWindowText(hWndMM, trackTitle, sizeof(trackTitle));

		StringCchLength(trackTitle, sizeof(trackTitle), &trackTitleLen);

		if (trackTitleLen > SCROLL_LENGTH)
		{
			size_t offset;

			offset = (counter % (trackTitleLen - SCROLL_LENGTH + SCROLL_PAUSE));

			if (offset >= trackTitleLen - SCROLL_LENGTH + SCROLL_PAUSE - 1)
			{
				StringCchPrintf(line2, LINE_LEN, TEXT(""));
			}
			else
			{
				if (offset > trackTitleLen - SCROLL_LENGTH)
				{
					offset = trackTitleLen - SCROLL_LENGTH;
				}

				StringCchCopyN(line2, LINE_LEN, &trackTitle[offset], SCROLL_LENGTH);
			}
		}
		else
		{
			StringCchCopy(line2, LINE_LEN, trackTitle);
		}

		status = (int) SendMessage(hWndMM, WM_USER, 0, 104);
		trackPos = (int) SendMessage(hWndMM, WM_USER, 0, 105);
		trackLen = (int) SendMessage(hWndMM, WM_USER, 1, 105);

		if (trackLen <= 0 || trackPos <= 0)
		{
			trackLen = 0;
			trackPos = 0;
			progress = 0;
		}
		else
		{
			progress = (trackPos * LOGI_LCD_MONO_WIDTH) / (trackLen * 1000);
		}

		if (progress >= LOGI_LCD_MONO_WIDTH) progress = LOGI_LCD_MONO_WIDTH - 1;

		if (progress != oldProgress)
		{
			int i;

			for (i = 0; (i <= progress); i++)
			{
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 4) + i] = 0xff;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 3) + i] = 0xff;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 2) + i] = 0xff;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 1) + i] = 0xff;
			}
			for (; (i <= oldProgress); i++)
			{
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 4) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 3) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 2) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 1) + i] = 0;
			}

			LogiLcdMonoSetBackground(pixels);

			oldProgress = progress;
		}

		switch (status)
		{
		case 1:
			StringCchPrintf(line3, LINE_LEN, TEXT("%02d:%02d / %02d:%02d"), (trackPos / (1000 * 60)), ((trackPos / 1000) % 60), (trackLen / 60), (trackLen % 60));
			break;
		case 3:
			StringCchPrintf(line3, LINE_LEN, TEXT("PAUSED"));
			break;
		default:
			StringCchPrintf(line3, LINE_LEN, TEXT("STOPPED"));
			break;
		}

		LogiLcdMonoSetText(2, line2);
		LogiLcdMonoSetText(3, line3);
		updateScreen = TRUE;
	}
	else
	{
		if (oldhWndMM != NULL)
		{
			for (int i = 0; (i < LOGI_LCD_MONO_WIDTH); i++)
			{
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 4) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 3) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 2) + i] = 0;
				pixels[LOGI_LCD_MONO_WIDTH*(LOGI_LCD_MONO_HEIGHT - 1) + i] = 0;
			}
			LogiLcdMonoSetBackground(pixels);
			LogiLcdMonoSetText(2, TEXT(""));
			LogiLcdMonoSetText(3, TEXT(""));
			updateScreen = TRUE;
		}
	}

	if (updateScreen)
	{
		LogiLcdUpdate();
	}

	counter++;
}

BOOL InitialiseLCD()
{
	ZeroMemory(pixels, LOGI_LCD_MONO_WIDTH * LOGI_LCD_MONO_HEIGHT);
	ZeroMemory(&lastDt, sizeof(lastDt));
	StringCchCopy(lastUser, UNLEN, TEXT(""));

	counter = 0;
	resetCounter = 0;
	oldProgress = -1;
	hWndMM = NULL;

	return LogiLcdInit(szTitle, LOGI_LCD_TYPE_MONO);
}