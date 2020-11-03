#include <windows.h>

#define ID_MAIN_TEXTBOX 20
#define ID_MAIN_LISTBOX 21
#define ID_TIMER1 1
#define ID_TIMER2 2

#define INI_FILE "shakychat.ini"
#define LOG_FILE "shakychat.log"
#define HISTORY_FILE "shakychat.txt"
#define HISTORY_LIMIT 200
#define MAX_LINE 62
#define WINDOW_WIDTH 570
#define WINDOW_HEIGHT 400
#define WINDOW_HEIGHT_MINIMUM 200

struct State
{
	u_short port;
	char ip[16];
	bool isServer;
	bool serverListening;
	bool serverConnected;
	bool serverWaitingThreadStarted;
	bool clientTalking;
	bool clientConnected;
	bool clientWaitingThreadStarted;
	bool writing;
	bool scrollListbox;
} state;

static void clearNewlines(char *, int);
static void writeFile(char *, char *);
static void readSettings(char *, HWND);
static void writeSettings(char *, HWND);
static void readHistory(char *);
static void writeHistory(char *);
static void parseCommandLine(LPWSTR);
static void serverConfig(PVOID);
static void serverWaiting(PVOID);
static void serverShutdown();
static void clientConfig(PVOID);
static void clientWaiting(PVOID);
static void clientShutdown();
static void getWSAErrorText(char *, int);
static void addNewText(char *, size_t);
static void shutDown(void);

LRESULT CALLBACK mainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customListboxProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customTextProc(HWND, UINT, WPARAM, LPARAM);
