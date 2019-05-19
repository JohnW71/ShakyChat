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
static struct Node *head = NULL;
static u_short port = 5150;
static char ip[16];
static char errorMsg[MAX_LINE];
static bool isServer = true;
static bool serverReady = false;
static bool clientConnected = false;
static bool serverWaitingStarted = false;

SOCKADDR_IN serverAddr;
SOCKET listeningSocket;
SOCKET newConnection;
SOCKET sendingSocket;

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
		"ShakyChat v0.6",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL);

	if (!mainHwnd)
	{
		MessageBox(NULL, "Window creation failed", "Error", MB_ICONEXCLAMATION | MB_OK);
		return EXIT_FAILURE;
	}

	parseCommandLine(lpCmdLine);
	if (isServer)
		_beginthread(serverConfig, 0, NULL);
	else
		_beginthread(clientConfig, 0, NULL);

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
			SetTimer(hwnd, ID_TIMER1, 100, NULL);

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

			// populate listbox
			readHistory(HISTORY_FILE);
			fillListbox(HISTORY_FILE, listboxHwnd);

			// limit text field length
			SendMessage(textboxHwnd, EM_LIMITTEXT, MAX_LINE-2, 0);
			SetFocus(textboxHwnd);
			break;
		case WM_TIMER:
			if (wParam == ID_TIMER1)
			{
				// scroll to last row on startup, can't do this during WM_CREATE as listbox is not visible at that point
				if (!scrolled)
				{
					scrolled = true;
					SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
					KillTimer(hwnd, ID_TIMER1);
				}
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
					writeFile(LOG_FILE, "VK_ESCAPE");
					writeSettings(INI_FILE, hwnd);
					writeHistory(HISTORY_FILE);
					if (isServer)
						serverShutdown();
					else
						clientShutdown();
					PostQuitMessage(0);
					break;
			}
			break;
		case WM_DESTROY:
			writeFile(LOG_FILE, "WM_DESTROY");
			writeSettings(INI_FILE, hwnd);
			writeHistory(HISTORY_FILE);
			if (isServer)
				serverShutdown();
			else
				clientShutdown();
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
				case VK_ESCAPE:
					writeFile(LOG_FILE, "VK_ESCAPE");
					writeSettings(INI_FILE, mainHwnd);
					writeHistory(HISTORY_FILE);
					PostQuitMessage(0);
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
					writeFile(LOG_FILE, "VK_ESCAPE");
					writeSettings(INI_FILE, mainHwnd);
					writeHistory(HISTORY_FILE);
					PostQuitMessage(0);
					break;
				case VK_RETURN:
					writeFile(LOG_FILE, "VK_RETURN");
					char text[MAX_LINE];
					GetWindowText(hwnd, text, MAX_LINE);
					if (strlen(text) > 0)
					{
						// add text to linked list
						strcat(text, "\n");
						append(&head, text, strlen(text));

						// add text to listbox
						SendMessage(listboxHwnd, LB_ADDSTRING, 0, (LPARAM)text);

						// keep list size limited
						LRESULT rowCount = SendMessage(listboxHwnd, LB_GETCOUNT, 0, 0);
						if (rowCount > HISTORY_LIMIT)
						{
							SendMessage(listboxHwnd, LB_DELETESTRING, 0, 0);
							deleteHead();
						}

						// scroll to last row
						SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
						SetWindowText(hwnd, "");

						// transmit message
						char buf[MAX_LINE] = "\0";
						if (isServer)
						{
							// send data to client
							int bytesSent = send(newConnection, text, (int)strlen(text), 0);

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
							}
						}
						else
						{
							// send data to server
							int bytesSent = send(sendingSocket, text, (int)strlen(text), 0);

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
							}
						}
					}
					break;
				case 'A': // CTRL A
					if (GetAsyncKeyState(VK_CONTROL))
					{
						writeFile(LOG_FILE, "CTRL A");
						SendMessage(hwnd, EM_SETSEL, 0, -1);
					}
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

static void writeFile(char *filename, char *text)
{
	FILE *f = fopen(filename, "a");
	if (f == NULL)
	{
		MessageBox(NULL, "Can't open file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	fprintf(f, "%s\n", text);
	fclose(f);
}

// static void writeFileW(char *filename, wchar_t *text)
// {
// 	FILE *f = fopen(filename, "a");
// 	if (f == NULL)
// 	{
// 		MessageBox(NULL, "Can't open file", "Error", MB_ICONEXCLAMATION | MB_OK);
// 		return;
// 	}

// 	fwprintf(f, L"%ls\n", text);
// 	fclose(f);
// }

static void writeSettings(char *iniFile, HWND hwnd)
{
	writeFile(LOG_FILE, "writeSettings()");
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
	writeFile(LOG_FILE, "readSettings()");
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
		if (strcmp(setting, "window_height") == 0)	windowHeight = atoi(value);
	}

	fclose(f);

	if (windowHeight == 0)	windowHeight = WINDOW_HEIGHT;
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

static void fillListbox(char *historyFile, HWND hwnd)
{
	writeFile(LOG_FILE, "fillListbox()");
	int count = 0;
	struct Node *node = head;
	while (node != NULL)
	{
		SendMessage(hwnd, LB_ADDSTRING, count++, (LPARAM)node->text);
		node = node->next;
	}
}

static void append(struct Node **head_ref, char *text, size_t length)
{
	struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
	newNode->next = NULL;
	newNode->text = malloc(length);
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

static void deleteHead()
{
	writeFile(LOG_FILE, "deleteHead()");
	if (head == NULL)
	{
		writeFile(LOG_FILE, "Null head");
		return;
	}

	// store current head node
	struct Node *previous = head;

	// move head to next node
	head = previous->next;
	free(previous->text);
	free(previous);
}

static void readHistory(char *historyFile)
{
	writeFile(LOG_FILE, "readHistory()");
	FILE *f = fopen(historyFile, "r");
	if (f == NULL)
	{
		writeFile(LOG_FILE, "No history found");
		return;
	}

	char line[MAX_LINE];
	while (fgets(line, MAX_LINE, f) != NULL)
	{
		size_t length = strlen(line);
		append(&head, line, length);
	}

	fclose(f);
}

static void writeHistory(char *historyFile)
{
	writeFile(LOG_FILE, "writeHistory()");
	FILE *f = fopen(historyFile, "w");
	if (f == NULL)
	{
		MessageBox(NULL, "Can't write history file", "Error", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	struct Node *node = head;
	while (node != NULL)
	{
		fprintf(f, "%s", node->text);
		node = node->next;
	}

	fclose(f);
}

static void parseCommandLine(LPWSTR lpCmdLine)
{
	// temporary forced command line
	// wcscpy(lpCmdLine, L"123.123.123.123 5150");

	if (wcslen(lpCmdLine) == 0)
	{
		writeFile(LOG_FILE, "Starting as server");
		return;
	}

	writeFile(LOG_FILE, "Starting as client");
	isServer = false;
	char commandLine[MAX_LINE];
	// convert LPWSTR to char *
	wcstombs(commandLine, lpCmdLine, MAX_LINE);
	char buf[MAX_LINE];
	sprintf(buf, "Command line converted: %s", commandLine);
	writeFile(LOG_FILE, buf);

	// get ip address
	int i = 0;
	while (commandLine[i] != ' ')
	{
		ip[i] = commandLine[i];
		++i;
	}
	ip[i++] = '\0';

	// get port number
	char portText[MAX_LINE];
	clearArray(portText, MAX_LINE);
	int p = 0;
	while (commandLine[i] != '\0')
		portText[p++] = commandLine[i++];
	port = (u_short)atoi(portText);

	sprintf(buf, "IP: %s\nPort: %d", ip, port);
	writeFile(LOG_FILE, buf);
}

static void getWSAErrorText(char *text, int code)
{
	char buf[MAX_LINE] = "\0";
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // flags
				NULL,										// lpsource
				code,										// message id
				MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),	// languageid
				buf,										// output buffer
				sizeof(buf),								// size of msgbuf, bytes
				NULL);										// va_list of arguments

	if (! *buf)
		sprintf(buf, "%d", code); // provide error # if no string available

	strcpy(text, buf);
}

static void serverConfig(PVOID pvoid)
{
	writeFile(LOG_FILE, "serverConfig()");

	char buf[MAX_LINE];
	WSADATA wsa;

	// initialize winsock v2.2
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "serverConfig: WSAStartup failed with error %s", errorMsg);
		writeFile(LOG_FILE, buf);
		return;
	}
	sprintf(buf, "serverConfig: Winsock DLL found.\nServer: Current status is: %s", wsa.szSystemStatus);
	writeFile(LOG_FILE, buf);

	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
	{
		sprintf(buf, "serverConfig: DLL does not support Winsock version %u.%u!", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
		writeFile(LOG_FILE, buf);
		WSACleanup();
		return;
	}
	sprintf(buf, "serverConfig: DLL supports Winsock version %u.%u", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
	writeFile(LOG_FILE, buf);
	sprintf(buf, "serverConfig: DLL supports highest version %u.%u", LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion));
	writeFile(LOG_FILE, buf);

	// create a new socket to listen for client connections
	listeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (listeningSocket == INVALID_SOCKET)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "serverConfig: socket() failed with error %s", errorMsg);
		writeFile(LOG_FILE, buf);
		WSACleanup();
		return;
	}
	writeFile(LOG_FILE, "serverConfig: socket()");

	// configure SOCKADDR_IN structure to tell bind to listen for connections on all interfaces using port 5150
	serverAddr.sin_family = AF_INET; // ipv4
	serverAddr.sin_port = htons(port); // host to network byte order
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// associate the address information with the socket using bind
	// call the bind function, passing the created socket and sockaddr_in struct as parameters
	if (bind(listeningSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "serverConfig: bind() failed with error %s", errorMsg);
		writeFile(LOG_FILE, buf);
		closesocket(listeningSocket);
		WSACleanup();
		return;
	}
	writeFile(LOG_FILE, "serverConfig: bind()");

	newConnection = (SOCKET)SOCKET_ERROR;
	while (1)
	{
		// listen for client connections. backlog of 5 is common
		if (listen(listeningSocket, 5) == SOCKET_ERROR)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "serverConfig: listen() failed with error %s", errorMsg);
			writeFile(LOG_FILE, buf);
			closesocket(listeningSocket);
			WSACleanup();
			return;
		}

		// accept a new connection when available
		// reset newConnection socket to SOCKET_ERROR
		// note that newConnection socket is not listening
		// newConnection = (SOCKET)SOCKET_ERROR;
		while (!serverReady && newConnection == SOCKET_ERROR)
		{
			// accept connection on listeningSocket and assign
			// it to newConnection socket. Let listeningSocket
			// keep listening for more connections
			writeFile(LOG_FILE, "serverConfig: accept()");
			newConnection = accept(listeningSocket, NULL, NULL);
			// writeFile(LOG_FILE, "serverConfig: new client connected, ready to send & receive data");
			serverReady = true;
			break;
		}

		if (!serverWaitingStarted)
		{
			serverWaitingStarted = true;
			_beginthread(serverWaiting, 0, NULL);
		}
	}
}

static void serverWaiting(PVOID pvoid)
{
	writeFile(LOG_FILE, "serverWaiting()");

	char buf[MAX_LINE];
	bool waiting = true;
	int senderInfoLength;
	SOCKADDR_IN senderInfo;

	while (waiting && serverReady)
	{
		// check for incoming message
		char recvbuf[MAX_LINE] = "\0";
		int bytesReceived = recv(newConnection, recvbuf, sizeof(recvbuf), 0);

		if (bytesReceived > 0)
		{
			writeFile(LOG_FILE, "serverWaiting: recv()");
			writeFile(LOG_FILE, "serverWaiting: new client connected");

			// info on receiver side
			getsockname(listeningSocket, (SOCKADDR *)&serverAddr, (int *)sizeof(serverAddr));
//TODO exception here
			sprintf(buf, "serverWaiting: receiving on IP: %s", inet_ntoa(serverAddr.sin_addr));
			writeFile(LOG_FILE, buf);
			sprintf(buf, "serverWaiting: receiving on port: %d", htons(serverAddr.sin_port));
			writeFile(LOG_FILE, buf);

			// allocate resources
//TODO exception here
			memset(&senderInfo, 0, sizeof(senderInfo)); // fill with 0
			senderInfoLength = sizeof(senderInfo);

			// info on sender side
			getpeername(newConnection, (SOCKADDR *)&senderInfo, &senderInfoLength);
			sprintf(buf, "serverWaiting: sender's IP address: %s", inet_ntoa(senderInfo.sin_addr));
			writeFile(LOG_FILE, buf);
			sprintf(buf, "serverWaiting: sender's port number: %d", htons(senderInfo.sin_port));
			writeFile(LOG_FILE, buf);

			// print received bytes. note that this is the total bytes received,
			// not the size of the declared buffer
			sprintf(buf, "serverWaiting: bytes received: %d", bytesReceived);
			writeFile(LOG_FILE, buf);
			writeFile(LOG_FILE, recvbuf);

			// add text to linked list
			char text[MAX_LINE] = "> ";
			strcat(text, recvbuf);
			append(&head, text, strlen(text));

			// add text to listbox
			SendMessage(listboxHwnd, LB_ADDSTRING, 0, (LPARAM)text);

			// keep list size limited
			LRESULT rowCount = SendMessage(listboxHwnd, LB_GETCOUNT, 0, 0);
			if (rowCount > HISTORY_LIMIT)
			{
				SendMessage(listboxHwnd, LB_DELETESTRING, 0, 0);
				deleteHead();
			}

			// scroll to last row
			SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
			SetWindowText(textboxHwnd, "");
		}
		// no data
		else if (bytesReceived == 0)
		{
			// writeFile(LOG_FILE, "Server: nothing received");
			// writeFile(LOG_FILE, "Server: connection closed/failed");
			// newConnection = (SOCKET)SOCKET_ERROR;
			// waiting = false;
		}
		// others
		else
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "serverWaiting: recv() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, buf);
			newConnection = (SOCKET)SOCKET_ERROR;
			waiting = false;
		}
	}
}

static void serverShutdown()
{
	writeFile(LOG_FILE, "serverShutdown()");

	char buf[MAX_LINE];

	if (newConnection != SOCKET_ERROR)
	{
		// clean up all send/receive comms, ready for new one
		if (shutdown(newConnection, SD_SEND) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "serverShutdown: shutdown() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, buf);
		}
		else
			writeFile(LOG_FILE, "serverShutdown: shutdown()");
	}

	writeFile(LOG_FILE, "serverShutdown: listeningSocket is timed out");

	// when all data comms and listening are finished close the socket
	if (closesocket(listeningSocket) != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "serverShutdown: can't close listeningSocket, error: %s", errorMsg);
		writeFile(LOG_FILE, buf);
	}
	else
		writeFile(LOG_FILE, "serverShutdown: closing listeningSocket");

	// clean up
	if (WSACleanup() != 0)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "serverShutdown: WSACleanup() failed, error: %s", errorMsg);
		writeFile(LOG_FILE, buf);
	}
	else
		writeFile(LOG_FILE, "serverShutdown: WSACleanup()");
}

static void clientConfig(PVOID pvoid)
{
	writeFile(LOG_FILE, "clientConfig()");

	char buf[MAX_LINE];
	WSADATA wsa;

	// initialize winsock v2.2
	WSAStartup(MAKEWORD(2,2), &wsa);
	sprintf(buf, "clientConfig: Winsock DLL status is %s", wsa.szSystemStatus);
	writeFile(LOG_FILE, buf);

	// create client socket
	sendingSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sendingSocket == INVALID_SOCKET)
	{
		getWSAErrorText(errorMsg, WSAGetLastError());
		sprintf(buf, "clientConfig: socket() failed, error: %s", errorMsg);
		writeFile(LOG_FILE, buf);
		WSACleanup();
		return;
	}
	writeFile(LOG_FILE, "clientConfig: socket()");

	// create SOCKADDR_IN struct to connect to a listening server
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(ip);

	while (!clientConnected && sendingSocket)
	{
		// connect to the server with sendingSocket
		int response = connect(sendingSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (response != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "clientConfig: connect() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, buf);
			// closesocket(sendingSocket);
			Sleep(2000);
			// WSACleanup();
			// return;
		}
		else
			clientConnected = true;
	}
	writeFile(LOG_FILE, "clientConfig: connect()");
	writeFile(LOG_FILE, "clientConfig: ready for sending & receiving...");

	// info on receiver side
	getsockname(sendingSocket, (SOCKADDR *)&serverAddr, (int *)sizeof(serverAddr));
	sprintf(buf, "clientConfig: receiving from IP: %s", inet_ntoa(serverAddr.sin_addr));
	writeFile(LOG_FILE, buf);
	sprintf(buf, "clientConfig: receiving from port: %d", htons(serverAddr.sin_port));
	writeFile(LOG_FILE, buf);

	_beginthread(clientWaiting, 0, NULL);
}

static void clientWaiting(PVOID pvoid)
{
	writeFile(LOG_FILE, "clientWaiting()");

	SOCKADDR_IN senderInfo;
	char buf[MAX_LINE];
	char sendbuf[MAX_LINE] = "\0";
	int bytesSent;
	int senderInfoLength;
	bool waiting = true;

	while (waiting)
	{
		// check for incoming message
		char recvbuf[MAX_LINE] = "\0";
		int bytesReceived = recv(sendingSocket, recvbuf, sizeof(recvbuf), 0);

		if (bytesReceived > 0)
		{
			writeFile(LOG_FILE, "clientWaiting: recv()");
			sprintf(buf, "clientWaiting: bytes received: %d, %s", bytesReceived, recvbuf);
			writeFile(LOG_FILE, buf);

			// add text to linked list
			char text[MAX_LINE] = "> ";
			strcat(text, recvbuf);
			append(&head, text, strlen(text));

			// add text to listbox
			SendMessage(listboxHwnd, LB_ADDSTRING, 0, (LPARAM)text);

			// keep list size limited
			LRESULT rowCount = SendMessage(listboxHwnd, LB_GETCOUNT, 0, 0);
			if (rowCount > HISTORY_LIMIT)
			{
				SendMessage(listboxHwnd, LB_DELETESTRING, 0, 0);
				deleteHead();
			}

			// scroll to last row
			SendMessage(listboxHwnd, WM_VSCROLL, SB_BOTTOM, 0);
			SetWindowText(textboxHwnd, "");
		}

		// send data to server/receiver
		if (strlen(sendbuf) > 0)
		{
			bytesSent = send(sendingSocket, sendbuf, (int)strlen(sendbuf), 0);
			writeFile(LOG_FILE, "clientWaiting: send()");

			if (bytesSent == SOCKET_ERROR)
			{
				getWSAErrorText(errorMsg, WSAGetLastError());
				sprintf(buf, "clientWaiting: send() error %s", errorMsg);
				writeFile(LOG_FILE, buf);
			}
			else
			{
				// info on this sender side
				// allocate resources
				memset(&senderInfo, 0, sizeof(senderInfo)); // fill with 0
				senderInfoLength = sizeof(senderInfo);

				getsockname(sendingSocket, (SOCKADDR *)&senderInfo, &senderInfoLength);
				sprintf(buf, "clientWaiting: sending from IP: %s", inet_ntoa(senderInfo.sin_addr));
				writeFile(LOG_FILE, buf);
				sprintf(buf, "clientWaiting: sending from port: %d", htons(senderInfo.sin_port));
				writeFile(LOG_FILE, buf);
				sprintf(buf, "clientWaiting: sent %d bytes: %s", bytesSent, sendbuf);
				writeFile(LOG_FILE, buf);
			}

			clearArray(sendbuf, MAX_LINE);
		}
	}
}

static void clientShutdown()
{
	writeFile(LOG_FILE, "clientShutdown()");

	char buf[MAX_LINE];

	if (sendingSocket)
	{
		// clean up all send/receive comms, ready for new one
		if (clientConnected && shutdown(sendingSocket, SD_BOTH) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "clientShutdown: shutdown() failed, error: %s", errorMsg);
			writeFile(LOG_FILE, buf);
		}
		else
			writeFile(LOG_FILE, "clientShutdown: shutdown()");

		// close socket
		if (closesocket(sendingSocket) != 0)
		{
			getWSAErrorText(errorMsg, WSAGetLastError());
			sprintf(buf, "clientShutdown: can't close sendingSocket, error: %s", errorMsg);
			writeFile(LOG_FILE, buf);
		}
		else
			writeFile(LOG_FILE, "clientShutdown: closing sendingSocket...");
	}

	if (WSACleanup() != 0)
		writeFile(LOG_FILE, "clientShutdown: WSACleanup() failed!");
	else
		writeFile(LOG_FILE, "clientShutdown: WSACleanup()");
}
