// GT_HelloWorldWin32.cpp
// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include "minihost.h"

// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("win32app");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("Win32 Guided Tour Application");

HINSTANCE hInst;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
	if (!checkPlatform ())
	{
		printf ("Platform verification failed! Please check your Compiler Settings!\n");
		return 1;
	}

	LoadPlugin();

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Win32 Guided Tour"),
            NULL);

        return 1;
    }

    hInst = hInstance; // Store instance handle in our global variable

    // The parameters to CreateWindow explained:
    // szWindowClass: the name of the application
    // szTitle: the text that appears in the title bar
    // WS_OVERLAPPEDWINDOW: the type of window to create
    // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
    // 500, 100: initial size (width, length)
    // NULL: the parent of this window
    // NULL: this application does not have a menu bar
    // hInstance: the first parameter from WinMain
    // NULL: not used in this application
    HWND hWnd = CreateWindow(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 100,
        NULL,
        NULL,
        hInstance,
        effect
    );

    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Win32 Guided Tour"),
            NULL);

        return 1;
    }

    // The parameters to ShowWindow explained:
    // hWnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    ShowWindow(hWnd,
        nCmdShow);
    UpdateWindow(hWnd);

	// parse input file
	init_table();
	is.open("C:\\Documents and Settings\\George\\My Documents\\luma2\\input.txt", ifstream::in);
	int ret = yyparse();
	is.close();

	// start the audio after everything has been initialized
	StartAudio();

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	//-----------------------
	case WM_CREATE :
	{
		SetWindowText (hWnd, "VST Editor");
		//SetTimer (hwnd, 1, 20, 0);

		if (effect)
		{
			printf ("HOST> Open editor...\n");
			effect->dispatcher (effect, effEditOpen, 0, 0, hWnd, 0);

			printf ("HOST> Get editor rect..\n");
			ERect* eRect = 0;
			effect->dispatcher (effect, effEditGetRect, 0, 0, &eRect, 0);
			if (eRect)
			{
				int width = eRect->right - eRect->left;
				int height = eRect->bottom - eRect->top;
				if (width < 100)
					width = 100;
				if (height < 100)
					height = 100;

				RECT wRect;
				SetRect (&wRect, 0, 0, width, height);
				AdjustWindowRectEx (&wRect, GetWindowLong (hWnd, GWL_STYLE), FALSE, GetWindowLong (hWnd, GWL_EXSTYLE));
				width = wRect.right - wRect.left;
				height = wRect.bottom - wRect.top;

				SetWindowPos (hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
			}
		}
	}	break;

	//-----------------------
	/*case WM_TIMER :
		if (effect)
			effect->dispatcher (effect, effEditIdle, 0, 0, 0, 0);
		break;*/

	//-----------------------
	case WM_CLOSE :
	{
		//KillTimer (hwnd, 1);

		printf ("HOST> Close editor..\n");
		if (effect)
			effect->dispatcher (effect, effEditClose, 0, 0, 0, 0);

		Cleanup();

		DestroyWindow(hWnd);

	}	break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}