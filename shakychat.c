#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <wchar.h>

#include "shakychat.h"

static HWND mainHwnd;
static WNDPROC originalListboxProc;
static WNDPROC originalTextProc;
static struct Node *head = NULL;

//TODO load history into linked list, INPROG, re-work functions
//TODO make window sizeable
//TODO cmdline to set as client & ip
//TODO cmdline to set as server & ip
//TODO make it encrypted?

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	MSG msg = {0};
	WNDCLASSEX wc = {0};

	// reset log file
	FILE *lf = fopen(LOG_FILE, "w");
	if (lf == NULL)
		MessageBox(NULL, "Can't open log file", "Error", MB_ICONEXCLAMATION | MB_OK);
	else
		fclose(lf);

	writeFile(LOG_FILE, "Command line:-");
	writeFileW(LOG_FILE, lpCmdLine);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = mainWndProc;
	wc.lpszClassName = "ShakyChat";
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
		return EXIT_FAILURE;
	}

	mainHwnd = CreateWindowEx(WS_EX_LEFT,
		wc.lpszClassName,
		"ShakyChat v0.2",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL);

	if (!mainHwnd)
	{
		MessageBox(NULL, "Window creation failed", "Error", MB_ICONEXCLAMATION | MB_OK);
		return EXIT_FAILURE;
	}

	readSettings(INI_FILE, mainHwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(mainHwnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return EXIT_SUCCESS;
}

LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND lbList, eText;

	switch (msg)
	{
		case WM_CREATE:
			// listbox
			lbList = CreateWindowEx(WS_EX_LEFT, "ListBox", NULL,
				WS_VISIBLE | WS_CHILD | LBS_DISABLENOSCROLL | LBS_NOSEL | WS_BORDER | WS_VSCROLL,
				10, 10, 560, 300, hwnd, (HMENU)ID_MAIN_LISTBOX, NULL, NULL);
			originalListboxProc = (WNDPROC)SetWindowLongPtr(lbList, GWLP_WNDPROC, (LONG_PTR)customListboxProc);

			// textbox
			eText = CreateWindowEx(WS_EX_LEFT, "Edit", "",
				WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
				10, 310, 560, 25, hwnd, (HMENU)ID_MAIN_TEXTBOX, NULL, NULL);
			originalTextProc = (WNDPROC)SetWindowLongPtr(eText, GWLP_WNDPROC, (LONG_PTR)customTextProc);

			readHistory(HISTORY_FILE);
			fillListbox(HISTORY_FILE, lbList);
			// printList(head);
			break;
		case WM_COMMAND:
			// if (LOWORD(wParam) == ID_MAIN_QUIT)
			// {
			// 	// shutDown(mainHwnd);
			// 	writeSettings(INI_FILE);
			// }

			if (LOWORD(wParam) == ID_MAIN_LISTBOX)
			{
				writeFile(LOG_FILE, "listbox clicked");

				// a row was selected
				// if (HIWORD(wParam) == LBN_SELCHANGE)
				// {
					// get row index
					// state.selectedRow = SendMessage(lbList, LB_GETCURSEL, 0, 0);

					// if (state.selectedRow != LB_ERR)
					// {
						// EnableWindow(bEdit, TRUE);
						// EnableWindow(bDelete, TRUE);
					// }
				// }

				// a row was double-clicked
				// if (HIWORD(wParam) == LBN_DBLCLK)
				// {
					// get row index
					// state.selectedRow = SendMessage(lbList, LB_GETCURSEL, 0, 0);

					// if (state.selectedRow != LB_ERR)
					// 	editEntry();
				// }
			}
			break;
		case WM_SIZE:
			writeFile(LOG_FILE, "WM_SIZE");
			break;
		case WM_SIZING:
			writeFile(LOG_FILE, "WM_SIZING");
			break;
		case WM_KEYUP:
			writeFile(LOG_FILE, "WM_KEYUP");
			switch (wParam)
			{
				case VK_ESCAPE:
					writeSettings(INI_FILE, hwnd);
					PostQuitMessage(0);
					break;
			}
			break;
		case WM_DESTROY:
			writeFile(LOG_FILE, "WM_DESTROY");
			writeSettings(INI_FILE, hwnd);
			PostQuitMessage(0);
			break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK customListboxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYUP:
			switch (wParam)
			{
				// case VK_RETURN:
					// get row index
					// state.selectedRow = SendMessage(lbList, LB_GETCURSEL, 0, 0);

					// if (state.selectedRow != LB_ERR)
					// 	editEntry();
					// break;
			}
			break;
	}
	return CallWindowProc(originalListboxProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK customTextProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYUP:
			switch (wParam)
			{
				case VK_ESCAPE:
					//
					break;
				case VK_RETURN:
					//
					break;
				case 'A': // CTRL A
					if (GetAsyncKeyState(VK_CONTROL))
						SendMessage(hwnd, EM_SETSEL, 0, -1);
					break;
			}
			break;
	}
	return CallWindowProc(originalTextProc, hwnd, msg, wParam, lParam);
}

static void clearArray(char *array, int length)
{
	for (int i = 0; i < length; ++i)
		array[i] = '\0';
}

static void writeFile(char *f, char *s)
{
	FILE *lf = fopen(f, "a");
	if (lf == NULL)
	{
		MessageBox(NULL, "Can't open file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	fprintf(lf, "%s\n", s);
	fclose(lf);
}

static void writeFileW(char *f, wchar_t *s)
{
	FILE *lf = fopen(f, "a");
	if (lf == NULL)
	{
		MessageBox(NULL, "Can't open file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	fwprintf(lf, L"%ls\n", s);
	fclose(lf);
}

static void writeSettings(char *iniFile, HWND hwnd)
{
	FILE *f = fopen(iniFile, "w");
	if (f == NULL)
	{
		writeFile(LOG_FILE, "Error saving settings!");
		return;
	}

	RECT rc = {0};
	GetWindowRect(hwnd, &rc);
	int windowWidth = rc.right - rc.left;
	int windowHeight = rc.bottom - rc.top;
	int windowCol = rc.left;
	int windowRow = rc.top;

	char buf[100];
	sprintf(buf, "wrote windowWidth=%d\nwrote windowHeight=%d\nwrote windowCol=%d\nwrote windowRow=%d", windowWidth, windowHeight, windowCol, windowRow);
	writeFile(LOG_FILE, buf);

	fprintf(f, "window_row=%d\n", windowRow);
	fprintf(f, "window_col=%d\n", windowCol);
	fprintf(f, "window_width=%d\n", windowWidth);
	fprintf(f, "window_height=%d\n", windowHeight);
	fclose(f);
}

static void readSettings(char *iniFile, HWND hwnd)
{
	FILE *f = fopen(iniFile, "r");
	if (f == NULL)
	{
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		SetWindowPos(hwnd, HWND_TOP, (screenWidth - WINDOW_WIDTH) / 2, (screenHeight - WINDOW_HEIGHT) / 2, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_SHOWWINDOW);
		return;
	}

	int windowWidth = 0;
	int windowHeight = 0;
	int windowCol = 0;
	int windowRow = 0;
	char line[MAX_LINE];

	while (fgets(line, MAX_LINE, f) != NULL)
	{
		if (line[0] == '#')
			continue;

		char setting[MAX_LINE];
		char value[MAX_LINE];
		char *l = line;
		char *s = setting;
		char *v = value;

		clearArray(setting, MAX_LINE);
		clearArray(value, MAX_LINE);

		// read setting
		while (*l && *l != '=')
			*s++ = *l++;
		*s = '\0';

		// read value
		++l;
		while (*l)
			*v++ = *l++;
		*v = '\0';

		if (strcmp(setting, "window_row") == 0)		windowRow = atoi(value);
		if (strcmp(setting, "window_col") == 0)		windowCol = atoi(value);
		if (strcmp(setting, "window_width") == 0)	windowWidth = atoi(value);
		if (strcmp(setting, "window_height") == 0)	windowHeight = atoi(value);
	}

	fclose(f);

	char buf[100];
	sprintf(buf, "read windowWidth=%d\nread windowHeight=%d\nread windowCol=%d\nread windowRow=%d", windowWidth, windowHeight, windowCol, windowRow);
	writeFile(LOG_FILE, buf);

	if (windowWidth == 0)	windowWidth = WINDOW_WIDTH;
	if (windowHeight == 0)	windowHeight = WINDOW_HEIGHT;
	if (windowRow == 0)
	{
	 	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		windowRow = (screenHeight - windowHeight) / 2;
	}
	if (windowCol == 0)
	{
	 	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		windowCol = (screenWidth - windowWidth) / 2;
	}

	sprintf(buf, "updated windowWidth=%d\nupdated windowHeight=%d\nupdated windowCol=%d\nupdated windowRow=%d", windowWidth, windowHeight, windowCol, windowRow);
	writeFile(LOG_FILE, buf);

	SetWindowPos(hwnd, HWND_TOP, windowCol, windowRow, windowWidth, windowHeight, SWP_SHOWWINDOW);
}

static void fillListbox(char *historyFile, HWND hwnd)
{
	// FILE *f = fopen(historyFile, "r");
	// if (f == NULL)
	// {
	// 	writeFile(LOG_FILE, "No history found");
	// 	return;
	// }

	// char line[MAX_LINE];
	int count = 0;

	// while (fgets(line, MAX_LINE, f) != NULL)
	// 	SendMessage(hwnd, LB_ADDSTRING, count++, (LPARAM)line);

	// fclose(f);

	struct Node *n = head;
	while (n != NULL)
	{
		SendMessage(hwnd, LB_ADDSTRING, count++, (LPARAM)n->text);
		n = n->next;
	}

}

static void append(struct Node **head_ref, char *text)
{
	struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
	// newNode->text = text;
	newNode->next = NULL;
	strcpy(newNode->text, text);

	// if list is empty make newNode into head
	if (*head_ref == NULL)
	{
		*head_ref = newNode;
		return;
	}

	struct Node *last = *head_ref;
	while (last->next != NULL)
		last = last->next;

	last->next = newNode;
}

static void readHistory(char *historyFile)
{
	FILE *f = fopen(historyFile, "r");
	if (f == NULL)
	{
		writeFile(LOG_FILE, "No history found");
		return;
	}

	char line[MAX_LINE];

	while (fgets(line, MAX_LINE, f) != NULL)
		append(&head, line);

	fclose(f);
}

// static void printList(struct Node *n)
// {
// 	while (n != NULL)
// 	{
// 		char buf[MAX_LINE];
// 		sprintf(buf, "Node %s", n->text);
// 		writeFile(LOG_FILE, buf);
// 		n = n->next;
// 	}
// }
