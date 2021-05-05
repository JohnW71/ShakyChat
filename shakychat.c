#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdbool.h>
#include <WinSock2.h>
#include <process.h>

#include "shakychat.h"

#pragma comment(lib, "ws2_32.lib") // winsock library

static HWND mainHwnd;
static HWND listboxHwnd;
static HWND textboxHwnd;
static WNDPROC originalListboxProc;
static WNDPROC originalTextProc;
static SOCKET listenSocket;
static SOCKET serverSocket;
static SOCKET clientSocket;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
					_In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	MSG msg = {0};
	WNDCLASSEX wc = {0};

	// reset log file
	FILE *f = fopen(LOG_FILE, "w");
	if (f == NULL)
		MessageBox(NULL, "Can't open log file", "Error", MB_ICONEXCLAMATION | MB_OK);
	else
		fclose(f);

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
		"ShakyChat v0.9",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL);

	if (!mainHwnd)
	{
		MessageBox(NULL, "Window creation failed", "Error", MB_ICONEXCLAMATION | MB_OK);
		return EXIT_FAILURE;
	}

	state.port = 5150;
	state.isServer = true;
	state.serverListening = true;
	state.serverConnected = false;
	state.serverWaitingThreadStarted = false;
	state.clientTalking = true;
	state.clientConnected = false;
	state.clientWaitingThreadStarted = false;
	state.writing = false;
	state.scrollListbox = false;

	parseCommandLine(lpCmdLine);
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
	static bool scrolled = false;

	switch (msg)
	{
		case WM_CREATE:
			SetTimer(hwnd, ID_TIMER1, 100, NULL); // populate listbox
			SetTimer(hwnd, ID_TIMER2, 200, NULL); // maintain focus in textbox

			// listbox
			listboxHwnd = CreateWindowEx(WS_EX_LEFT, "ListBox", NULL,
				WS_VISIBLE | WS_CHILD | LBS_DISABLENOSCROLL | LBS_NOSEL | WS_BORDER | WS_VSCROLL,
				10, 10, WINDOW_WIDTH-40, 300, hwnd, (HMENU)ID_MAIN_LISTBOX, NULL, NULL);
			originalListboxProc = (WNDPROC)SetWindowLongPtr(listboxHwnd, GWLP_WNDPROC, (LONG_PTR)customListboxProc);

			// textbox
			textboxHwnd = CreateWindowEx(WS_EX_LEFT, "Edit", "",
				WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
				10, 310, WINDOW_WIDTH-40, 25, hwnd, (HMENU)ID_MAIN_TEXTBOX, NULL, NULL);
			originalTextProc = (WNDPROC)SetWindowLongPtr(textboxHwnd, GWLP_WNDPROC, (LONG_PTR)customTextProc);

			break;
		case WM_TIMER:
			if (wParam == ID_TIMER1)
			{
				// populate listbox
				readHistory(HISTORY_FILE);

				// limit text field length
				SendMessage(textboxHwnd, EM_LIMITTEXT, MAX_LINE-4, 0);
				SetFocus(textboxHwnd);

				// scroll to last row on startup, can't do this during WM_CREATE as listbox is not visible at that point
				if (!scrolled)
				{
					scrolled = true;
					SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
					KillTimer(hwnd, ID_TIMER1);
				}

				if (state.isServer)
					_beginthread(serverConfig, 0, NULL);
				else
					_beginthread(clientConfig, 0, NULL);
			}
			if (wParam == ID_TIMER2)
			{
				SetFocus(textboxHwnd);
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == ID_MAIN_LISTBOX)
			{
				SetFocus(textboxHwnd);
			}
			break;
		case WM_SIZE:
			RECT rc = {0};
			GetWindowRect(hwnd, &rc);
			int windowHeight = rc.bottom - rc.top;

			// resize listbox & textbox to fit window
			SetWindowPos(listboxHwnd, HWND_TOP, 10, 10, WINDOW_WIDTH-40, windowHeight-90, SWP_SHOWWINDOW);
			SetWindowPos(textboxHwnd, HWND_TOP, 10, windowHeight-75, WINDOW_WIDTH-40, 25, SWP_SHOWWINDOW);

			// maintain main window width
			SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, WINDOW_WIDTH, windowHeight, SWP_SHOWWINDOW);

			// force minimum height
			if (windowHeight < WINDOW_HEIGHT_MINIMUM)
				SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, WINDOW_WIDTH, WINDOW_HEIGHT_MINIMUM, SWP_SHOWWINDOW);
			break;
		case WM_KEYUP:
			switch (wParam)
			{
				case VK_ESCAPE:
					shutDown();
					break;
			}
			break;
		case WM_DESTROY:
			shutDown();
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
				case VK_ESCAPE:
					shutDown();
					break;
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
					shutDown();
					break;
				case VK_RETURN:
					char errorMsg[256];
					char text[MAX_LINE];
					GetWindowText(hwnd, text, MAX_LINE);
					if (strlen(text) > 0)
					{
						clearNewlines(text, MAX_LINE);
						addNewText(text, strlen(text));

						// transmit message
						char buf[MAX_LINE];
						if (state.isServer)
						{
							if (state.serverConnected) // send data to client
							{
								int bytesSent = send(serverSocket, text, (int)strlen(text), 0);

								if (bytesSent == SOCKET_ERROR)
								{
									getWSAErrorText(errorMsg, WSAGetLastError());
									sprintf(buf, "Server: send() error %s", errorMsg);
									writeFile(LOG_FILE, buf);
								}
								else
								{
									sprintf(buf, "Server: sent %d bytes: %s", bytesSent, text);
									writeFile(LOG_FILE, buf);
									SetWindowText(textboxHwnd, "");
								}
							}
							else
							{
								strcpy(text, "#Client not connected!");
								addNewText(text, strlen(text));
							}
						}
						else
						{
							if (state.clientConnected) // send data to server
							{
								int bytesSent = send(clientSocket, text, (int)strlen(text), 0);

								if (bytesSent == SOCKET_ERROR)
								{
									getWSAErrorText(errorMsg, WSAGetLastError());
									sprintf(buf, "Client: send() error %s", errorMsg);
									writeFile(LOG_FILE, buf);
								}
								else
								{
									sprintf(buf, "Client: sent %d bytes: %s", bytesSent, text);
									writeFile(LOG_FILE, buf);
									SetWindowText(textboxHwnd, "");
								}
							}
							else
							{
								strcpy(text, "#Server not connected!");
								addNewText(text, strlen(text));
							}
						}
					}
					break;
				case 'A': // CTRL A
					if (GetAsyncKeyState(VK_CONTROL))
					{
						SendMessage(hwnd, EM_SETSEL, 0, -1);
					}
					break;
			}
			break;
	}
	return CallWindowProc(originalTextProc, hwnd, msg, wParam, lParam);
}

static void shutDown(void)
{
	if (state.isServer)
	{
		state.serverConnected = false;
		state.serverListening = false;
		serverShutdown();
	}
	else
	{
		state.clientConnected = false;
		state.clientTalking = false;
		clientShutdown();
	}

	writeSettings(INI_FILE, mainHwnd);
	writeHistory(HISTORY_FILE);
	PostQuitMessage(0);
}

static void addNewText(char *text, size_t length)
{
	// add text to listbox
	SendMessage(listboxHwnd, LB_ADDSTRING, 0, (LPARAM)text);

	// keep list size limited
	LRESULT rowCount = SendMessage(listboxHwnd, LB_GETCOUNT, 0, 0);
	if (rowCount > HISTORY_LIMIT)
		SendMessage(listboxHwnd, LB_DELETESTRING, 0, 0);

	// scroll to last row
	if (state.scrollListbox)
		SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

 static void clearNewlines(char *array, int length)
{
	for (int i = 0; i < length; ++i)
		if (array[i] == '\n')
			array[i] = '\0';
}

static void writeFile(char *filename, char *text)
{
	while(state.writing)
	{
		// char t[MAX_LINE] = "#Writing blocked";
		// addNewText(t, strlen(t));
		Sleep(100);
	}

	state.writing = true;

	FILE *f = fopen(filename, "a");
	if (f == NULL)
	{
		MessageBox(NULL, "Can't open file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	fprintf(f, "%s\n", text);
	fclose(f);
	state.writing = false;
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
	int windowHeight = rc.bottom - rc.top;
	int windowCol = rc.left;
	int windowRow = rc.top;

	fprintf(f, "window_row=%d\n", windowRow);
	fprintf(f, "window_col=%d\n", windowCol);
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

	int windowHeight = 0;
	int windowCol = 0;
	int windowRow = 0;
	char line[MAX_LINE];

	while (fgets(line, MAX_LINE, f) != NULL)
	{
		if (line[0] == '#')
			continue;

		char setting[MAX_LINE] = {0};
		char value[MAX_LINE] ={0};
		char *l = line;
		char *s = setting;
		char *v = value;

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
		if (strcmp(setting, "window_height") == 0)	windowHeight = atoi(value);
	}

	fclose(f);

	if (windowHeight == 0)
		windowHeight = WINDOW_HEIGHT;
	if (windowRow == 0)
	{
	 	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		windowRow = (screenHeight - windowHeight) / 2;
	}
	if (windowCol == 0)
	{
	 	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		windowCol = (screenWidth - WINDOW_WIDTH) / 2;
	}

	SetWindowPos(hwnd, HWND_TOP, windowCol, windowRow, WINDOW_WIDTH, windowHeight, SWP_SHOWWINDOW);
}

static void readHistory(char *historyFile)
{
	FILE *f = fopen(historyFile, "r");
	if (f == NULL)
	{
		char text[MAX_LINE] = "No history found";
		writeFile(LOG_FILE, text);
		addNewText(text, strlen(text));
		return;
	}

	char line[MAX_LINE];
	while (fgets(line, MAX_LINE, f) != NULL)
		addNewText(line, strlen(line));

	// scrollbox is not scrolled to bottom during initial history load
	// after this it will be working again
	state.scrollListbox = true;
	fclose(f);
}

static void writeHistory(char *historyFile)
{
	FILE *f = fopen(historyFile, "w");
	if (f == NULL)
	{
		MessageBox(NULL, "Can't write history file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	LRESULT rowCount = SendMessage(listboxHwnd, LB_GETCOUNT, 0, 0);
	for (int i = 0; i < rowCount; ++i)
	{
		char row[MAX_LINE] = "\0";
		int textLen = (int)SendMessage(listboxHwnd, LB_GETTEXT, i, (LPARAM)row);
		row[textLen] = '\0';
		clearNewlines(row, MAX_LINE);
		fprintf(f, "%s\n", row);
	}

	fclose(f);
}

static void parseCommandLine(LPWSTR lpCmdLine)
{
	if (wcslen(lpCmdLine) == 0)
	{
		writeFile(LOG_FILE, "Starting as server");
		return;
	}

	writeFile(LOG_FILE, "Starting as client");
	state.isServer = false;
	char commandLine[MAX_LINE];
	wcstombs(commandLine, lpCmdLine, MAX_LINE); // convert LPWSTR to char *
	char logMessage[MAX_LINE];
	sprintf(logMessage, "Command line: %s", commandLine);
	writeFile(LOG_FILE, logMessage);

	// get ip address
	int i = 0;
	while (commandLine[i] != ' ')
	{
		if (!isdigit(commandLine[i]) && commandLine[i] != '.')
		{
			writeFile(LOG_FILE, "ERROR invalid digit found in IP address");
			shutDown();
		}
		state.ip[i] = commandLine[i];
		++i;
	}
	state.ip[i++] = '\0';

	// get port number
	char portText[MAX_LINE] = {0};
	int p = 0;
	while (commandLine[i] != '\0')
	{
		if (!isdigit(commandLine[i]))
		{
			writeFile(LOG_FILE, "ERROR invalid digit found in port number");
			shutDown();
		}
		portText[p++] = commandLine[i++];
	}
	state.port = (u_short)atoi(portText);

	sprintf(logMessage, "Command line IP: %s\nCommand line Port: %d", state.ip, state.port);
	writeFile(LOG_FILE, logMessage);
}

static void getWSAErrorText(char *text, int code)
{
	char buffer[256] = {0};
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // flags
				   NULL,													   // lpsource
				   code,													   // logMessage id
				   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),				   // languageid
				   buffer,													   // output buffer
				   sizeof(buffer),											   // size of msgbuf, bytes
				   NULL);													   // va_list of arguments

	if (!*buffer)
		// sprintf(buffer, "%d", code); // provide error # if no string available
		_itoa(code, buffer, 10);

	strcpy(text, buffer);
}

static void serverConfig(PVOID pvoid)
{
	char logMessage[MAX_LINE];
	char errorMsg[256];
	WSADATA wsa;

	// initialize winsock v2.2
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "serverConfig: ERROR WSAStartup failed with error %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
		return;
	}
	sprintf(logMessage, "serverConfig: Winsock DLL found.\nServer: Current status is: %s", wsa.szSystemStatus);
	writeFile(LOG_FILE, logMessage);

	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
	{
		sprintf(logMessage, "serverConfig: ERROR DLL does not support Winsock version %u.%u!", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
		writeFile(LOG_FILE, logMessage);
		WSACleanup();
		return;
	}

	sprintf(logMessage, "serverConfig: DLL supports Winsock version %u.%u", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
	writeFile(LOG_FILE, logMessage);
	sprintf(logMessage, "serverConfig: DLL supports highest version %u.%u", LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion));
	writeFile(LOG_FILE, logMessage);

	// create a new socket to listen for client connections
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "serverConfig: ERROR socket() failed with error %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
		WSACleanup();
		return;
	}

	writeFile(LOG_FILE, "serverConfig: socket() completed");

	// configure SOCKADDR_IN structure to tell bind to listen for connections on all interfaces using port 5150
	SOCKADDR_IN serverAddress;
	serverAddress.sin_family = AF_INET;		 // ipv4
	serverAddress.sin_port = htons(state.port); // host to network byte order
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	// associate the address information with the socket using bind
	// call the bind function, passing the created socket and sockaddr_in struct
	if (bind(listenSocket, (SOCKADDR *)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "serverConfig: ERROR bind() failed with error %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	writeFile(LOG_FILE, "serverConfig: bind() completed");

	serverSocket = (SOCKET)SOCKET_ERROR;
	while (state.serverListening)
	{
		// listen for client connections. backlog of 5 is common
		if (listen(listenSocket, 5) == SOCKET_ERROR)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "serverConfig: ERROR listen() failed with error %s", errorMsg);
			writeFile(LOG_FILE, logMessage);
			closesocket(listenSocket);
			WSACleanup();
			return;
		}

		// writeFile(LOG_FILE, "serverConfig: listen() completed");

		// accept a new connection when available
		while (serverSocket == SOCKET_ERROR)
		{
			// accept connection on listenSocket and assign it to serverSocket
			// let listenSocket keep listening for more connections
			writeFile(LOG_FILE, "serverConfig: accept() listenSocket");
			serverSocket = accept(listenSocket, NULL, NULL);
			writeFile(LOG_FILE, "serverConfig: accept() listenSocket ok");
			state.serverConnected = true;

			writeFile(LOG_FILE, "serverWaiting: new client connected");
			char text[MAX_LINE] = "#Client connected!";
			addNewText(text, strlen(text));
		}

		// in case shutdown started while listening got a new connection
		if (!state.serverListening)
			return;

		if (!state.serverWaitingThreadStarted)
		{
			state.serverWaitingThreadStarted = true;
			_beginthread(serverWaiting, 0, NULL);
		}
	}
}

static void serverWaiting(PVOID pvoid)
{
	char errorMsg[256];
	bool waiting = true;

	while (waiting && state.serverConnected)
	{
		char logMessage[MAX_LINE] = {0};
		char buffer[MAX_LINE] = {0};

		// check for incoming message
		int bytesReceived = recv(serverSocket, buffer, sizeof(buffer), 0);

		if (bytesReceived > 0) // data
		{
			writeFile(LOG_FILE, "serverWaiting: recv() completed");

			// print received bytes. note that this is the total bytes received, not the size of the declared buffer
			sprintf(logMessage, "serverWaiting: bytes received: %d", bytesReceived);
			writeFile(LOG_FILE, logMessage);
			writeFile(LOG_FILE, buffer);

			// add new text
			char text[MAX_LINE] = "> ";
			strcat(text, buffer);
			clearNewlines(text, MAX_LINE);
			addNewText(text, strlen(text));

			// bring window to front, first line only would be permanently in front
			SetWindowPos(mainHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			SetWindowPos(mainHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		}

		if (bytesReceived == 0) // disconnection
		{
			char text[MAX_LINE] = "#Connection error!";
			addNewText(text, strlen(text));

			serverSocket = (SOCKET)SOCKET_ERROR;
			waiting = false;
			state.serverWaitingThreadStarted = false;
		}

		if (bytesReceived < 0) // error
		{
			if (WSAGetLastError() == 10054)
			{
				char text[MAX_LINE] = "#Client has disconnected!";
				addNewText(text, strlen(text));
			}
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "serverWaiting: ERROR recv() failed, error: %ld %s", WSAGetLastError(), errorMsg);
			writeFile(LOG_FILE, logMessage);
			serverSocket = (SOCKET)SOCKET_ERROR;
			waiting = false;
			state.serverWaitingThreadStarted = false;
		}
	}
}

static void serverShutdown()
{
	char logMessage[MAX_LINE];
	char errorMsg[256];

	if (serverSocket != SOCKET_ERROR)
	{
		if (shutdown(serverSocket, SD_SEND) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "serverShutdown: ERROR shutdown() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, logMessage);
		}
		else
			writeFile(LOG_FILE, "serverShutdown: shutdown() completed");
	}

	if (closesocket(listenSocket) != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "serverShutdown: ERROR can't close listenSocket, error: %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
	}
	else
		writeFile(LOG_FILE, "serverShutdown: closing listenSocket completed");

	if (WSACleanup() != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "serverShutdown: ERROR WSACleanup() failed, error: %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
	}
	else
		writeFile(LOG_FILE, "serverShutdown: WSACleanup() completed");
}

static void clientConfig(PVOID pvoid)
{
	char logMessage[MAX_LINE];
	char errorMsg[256];
	WSADATA wsa;

	// initialize winsock v2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(logMessage, "clientConfig: ERROR WSAStartup failed with error %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
		return;
	}

	sprintf(logMessage, "clientConfig: Winsock DLL found.\nClient: Current status is: %s", wsa.szSystemStatus);
	writeFile(LOG_FILE, logMessage);

	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
	{
		sprintf(logMessage, "clientConfig: ERROR DLL does not support Winsock version %u.%u!", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
		writeFile(LOG_FILE, logMessage);
		WSACleanup();
		return;
	}

	sprintf(logMessage, "clientConfig: DLL supports Winsock version %u.%u", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
	writeFile(LOG_FILE, logMessage);
	sprintf(logMessage, "clientConfig: DLL supports highest version %u.%u", LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion));
	writeFile(LOG_FILE, logMessage);

	clientSocket = (SOCKET)SOCKET_ERROR;
	SOCKADDR_IN serverAddress;

	while (state.clientTalking)
	{
		while(clientSocket == SOCKET_ERROR)
		{
			// create client socket
			clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (clientSocket == INVALID_SOCKET)
			{
				getWSAErrorText(errorMsg, WSAGetLastError());
				sprintf(logMessage, "clientConfig: ERROR socket() failed, error: %s", errorMsg);
				writeFile(LOG_FILE, logMessage);
				Sleep(2000);
			}
			else
			{
				writeFile(LOG_FILE, "clientConfig: socket() completed");

				// create SOCKADDR_IN struct to connect to a listening server
				serverAddress.sin_family = AF_INET;
				serverAddress.sin_port = htons(state.port);
				serverAddress.sin_addr.s_addr = inet_addr(state.ip);
				writeFile(LOG_FILE, "clientConfig: inet_addr() completed");
			}
		}

		while (!state.clientConnected && clientSocket != (SOCKET)SOCKET_ERROR)
		{
			// connect to the server with clientSocket
			if (connect(clientSocket, (SOCKADDR *)&serverAddress, sizeof(serverAddress)) != 0)
			{
				//TODO failure here could be due to incorrect ip/port, should handle this
				getWSAErrorText(errorMsg, WSAGetLastError());
				sprintf(logMessage, "clientConfig: ERROR connect() failed, error: %s", errorMsg);
				writeFile(LOG_FILE, logMessage);
				Sleep(2000);
			}
			else
			{
				state.clientConnected = true;
				writeFile(LOG_FILE, "clientConfig: connect() completed");
			}
		}

		if (!state.clientWaitingThreadStarted && state.clientConnected)
		{
			int len = sizeof(serverAddress);
			getsockname(clientSocket, (SOCKADDR *)&serverAddress, &len);

			writeFile(LOG_FILE, "clientConfig: getsockname() completed");
			sprintf(logMessage, "clientConfig: connected to IP: %s, port: %d", inet_ntoa(serverAddress.sin_addr), htons(serverAddress.sin_port));
			writeFile(LOG_FILE, logMessage);

			char text[MAX_LINE] = "#Server connected!";
			addNewText(text, strlen(text));

			state.clientWaitingThreadStarted = true;
			_beginthread(clientWaiting, 0, NULL);
		}
	}
}

static void clientWaiting(PVOID pvoid)
{
	while (state.clientConnected)
	{
		char errorMsg[256];
		char buffer[MAX_LINE] = {0};
		char logMessage[MAX_LINE] = {0};

		// check for incoming message
		int bytesReceived = recv(clientSocket, buffer, MAX_LINE, 0);

		if (bytesReceived > 0) // data
		{
			writeFile(LOG_FILE, "clientWaiting: recv() completed");
			sprintf(logMessage, "clientWaiting: bytes received: %d", bytesReceived);
			writeFile(LOG_FILE, logMessage);
			writeFile(LOG_FILE, buffer);

			// add new text
			char text[MAX_LINE] = "> ";
			strcat(text, buffer);
			clearNewlines(text, MAX_LINE);
			addNewText(text, strlen(text));

			// bring window to front, first line only would be permanently in front
			SetWindowPos(mainHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			SetWindowPos(mainHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		}

		if (bytesReceived == 0) // disconnection
		{
			char text[MAX_LINE] = "#Server has disconnected!";
			addNewText(text, strlen(text));

			writeFile(LOG_FILE, "clientWaiting: ERROR potential disconnection");
			clientSocket = (SOCKET)SOCKET_ERROR;
			state.clientWaitingThreadStarted = false;
			state.clientConnected = false;
		}

		if (bytesReceived < 0) // error
		{
			if (WSAGetLastError() == 10053)
			{
				char text[MAX_LINE] = "#Connection error!";
				addNewText(text, strlen(text));
			}
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "clientWaiting: ERROR recv() failed, error: %ld %s", WSAGetLastError(), errorMsg);
			writeFile(LOG_FILE, logMessage);
			clientSocket = (SOCKET)SOCKET_ERROR;
			state.clientWaitingThreadStarted = false;
			state.clientConnected = false;
		}
	}
}

static void clientShutdown()
{
	char logMessage[MAX_LINE];
	char errorMsg[256];

	if (clientSocket != SOCKET_ERROR)
	{
		// clean up all send/receive comms, ready for new one
		if (state.clientConnected && shutdown(clientSocket, SD_BOTH) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "clientShutdown: ERROR shutdown() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, logMessage);
		}
		else
			writeFile(LOG_FILE, "clientShutdown: shutdown() completed");

		// close socket
		if (closesocket(clientSocket) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(logMessage, "clientShutdown: ERROR can't close clientSocket, error: %s", errorMsg);
			writeFile(LOG_FILE, logMessage);
		}
		else
			writeFile(LOG_FILE, "clientShutdown: closing clientSocket completed");
	}

	if (WSACleanup() != 0)
	{
		sprintf(logMessage, "clientShutdown: ERROR WSACleanup() failed, error %s", errorMsg);
		writeFile(LOG_FILE, logMessage);
	}
	else
		writeFile(LOG_FILE, "clientShutdown: WSACleanup() completed");
}
