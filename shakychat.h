#include <windows.h>

#define ID_MAIN_TEXTBOX 20
#define ID_MAIN_LISTBOX 21
#define ID_TIMER1 1

#define INI_FILE "shakychat.ini"
#define LOG_FILE "shakychat.log"
#define HISTORY_FILE "shakychat.txt"
#define HISTORY_LIMIT 20
#define MAX_LINE 62
#define WINDOW_WIDTH 570
#define WINDOW_HEIGHT 400
#define WINDOW_HEIGHT_MINIMUM 200

#define assert(expression) if(!(expression)) {*(int *)0 = 0;}
#define arrayCount(array) (sizeof(array) / sizeof((array)[0]))

void clearArray(char *, int);
void writeFile(char *, char *);
void writeFileW(char *, wchar_t *);
void readSettings(char *, HWND);
void writeSettings(char *, HWND);
void fillListbox(char *, HWND);
void append(struct Node **, char *, size_t);
void deleteHead();
void readHistory(char *);
void writeHistory(char *);

LRESULT CALLBACK mainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customListboxProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customTextProc(HWND, UINT, WPARAM, LPARAM);

struct Node
{
	char *text;
	struct Node *next;
};
