/*
 * toolbox.h - Window, Menu, Dialog and Alert Managers.
 *
 * Everything is built from the game's own WIND, MENU, mctb, DLOG, DITL,
 * ALRT, wctb, dctb and actb resources and drawn in a System 7 style on the
 * 8-bit screen. Dialogs and menus save and restore the pixels beneath them;
 * windows get update events when they are created or uncovered.
 */
#ifndef TOOLBOX_H
#define TOOLBOX_H

#include "platform.h"

/* ---- Window Manager ---- */
typedef struct WindowRecord *WindowPtr;

enum { inDesk = 0, inMenuBar = 1, inSysWindow = 2, inContent = 3, inDrag = 4 };

void TB_Init(void);
/* Called when the user closes the window while a dialog is up. */
extern void (*gQuitHook)(void);
WindowPtr GetNewWindow(int id, WindowPtr behind);   /* behind NULL = in front */
void DisposeWindow(WindowPtr w);
GrafPtr WindowPort(WindowPtr w);
void MoveWindow(WindowPtr w, int h, int v, bool front);
void SizeWindow(WindowPtr w, int width, int height, bool update);
void SetOrigin(int h, int v);            /* current port must be a window */
/* visRgn control used by HideMenuBar/ShowMenuBar in Tools.p */
void SetWindowClipsMenuBar(WindowPtr w, bool clips);
int FindWindow(Point where, WindowPtr *w);
void DragWindow(WindowPtr w, Point start);
WindowPtr EventWindow(const EventRecord *ev);   /* for update events */
void InvalRect(const Rect *r);
void BeginUpdate(WindowPtr w);
void EndUpdate(WindowPtr w);
void GlobalToLocal(Point *p);
void LocalToGlobal(Point *p);
void GetMouse(Point *p);                 /* local to the current port */
void RedrawScreen(void);                 /* repaint desktop, windows, menu bar */

/* ---- Menu Manager ---- */
typedef struct Menu *MenuHandle;
MenuHandle GetMenu(int id);
void InsertMenu(MenuHandle m, int beforeID);
void DrawMenuBar(void);
long MenuSelect(Point start);
long MenuKey(unsigned char ch);
void HiliteMenu(int id);
void EnableItem(MenuHandle m, int item);
void DisableItem(MenuHandle m, int item);
void GetMenuItemText(MenuHandle m, int item, unsigned char *out);
void SetMenuBarHeight(int h);            /* LMSetMBarHeight */
int GetMenuBarHeight(void);

/* ---- Dialog Manager ---- */
typedef struct Dialog *DialogPtr;
DialogPtr GetNewDialog(int id);
void DisposeDialog(DialogPtr d);
GrafPtr DialogPort(DialogPtr d);
void ModalDialog(void *filter, INTEGER *itemHit);
void SetDialogDefaultItem(DialogPtr d, int item);
void SetDialogCancelItem(DialogPtr d, int item);
void GetDialogItemRect(DialogPtr d, int item, Rect *r);
void SetDialogItemText(DialogPtr d, int item, const unsigned char *text);
void GetDialogItemText(DialogPtr d, int item, unsigned char *text);
void SelectDialogItemText(DialogPtr d, int item, int start, int end);
void SetControlValue(DialogPtr d, int item, int value);
int GetControlValue(DialogPtr d, int item);
void HiliteControl(DialogPtr d, int item, int state);
void ParamText(const unsigned char *a, const unsigned char *b, const unsigned char *c, const unsigned char *d);
INTEGER Alert(int id, void *filter);
void SysBeep(int duration);

#endif
