#include <windows.h>

#define ID_MAIN_TEXTBOX 20
#define ID_MAIN_LISTBOX 21

#define INI_FILE "shakychat.ini"
#define LOG_FILE "log.txt"
#define HISTORY_FILE "history.txt"
#define MAX_LINE 80
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400

#define assert(expression) if(!(expression)) {*(int *)0 = 0;}
#define arrayCount(array) (sizeof(array) / sizeof((array)[0]))

void clearArray(char *, int);
void writeFile(char *, char *);
void writeFileW(char *, wchar_t *);
void readSettings(char *, HWND);
void writeSettings(char *, HWND);
void fillListbox(char *, HWND);
// void push(struct Node **, char *);
void append(struct Node **, char *);
void readHistory(char *);
// void printList(struct Node *);

LRESULT CALLBACK mainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customListboxProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK customTextProc(HWND, UINT, WPARAM, LPARAM);

struct Node
{
	char text[MAX_LINE];
	struct Node *next;
};
