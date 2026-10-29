/*
 * platform.h - SDL2 backend: the screen, the Event Manager, the keyboard map,
 * ticks and the cursor.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "mac.h"
#include "qd.h"

#define SCREEN_W 640
#define SCREEN_H_MAX 522

/* Event Manager */
enum { nullEvent = 0, mouseDown = 1, mouseUp = 2, keyDown = 3, keyUp = 4, autoKey = 5,
       updateEvt = 6, diskEvt = 7, activateEvt = 8, osEvt = 15 };
#define everyEvent 0xFFFF
#define keyDownMask (1 << keyDown)
#define mDownMask (1 << mouseDown)
#define mUpMask (1 << mouseUp)
#define charCodeMask 0xFF
enum { cmdKey = 0x0100, shiftKey = 0x0200, alphaLock = 0x0400, optionKey = 0x0800, controlKey = 0x1000 };

typedef struct {
    INTEGER what;
    uint32_t message;
    uint32_t when;
    Point where;        /* global (screen) coordinates */
    uint16_t modifiers;
} EventRecord;

bool Plat_Init(void);
void Plat_Shutdown(void);

/* The physical screen: SCREEN_W x height, palette indices. */
extern GrafPort gScreenPort;
int Plat_ScreenHeight(void);
void Plat_SetScreenHeight(int h);

/* Polls SDL and presents the screen if it is due. Call from every wait. */
void Plat_Pump(void);
void Plat_Present(void);
bool Plat_QuitRequested(void);
void Plat_ClearQuit(void);

uint32_t TickCount(void);
void Plat_Sleep(int ms);   /* pumps events, then sleeps */
void WaitUntilTick(uint32_t tick);
void Delay(long ticks, long *finalTicks);

bool GetNextEvent(int mask, EventRecord *ev);
bool WaitNextEvent(int mask, EventRecord *ev, long sleep);
void FlushEvents(int mask, int stopMask);
void PostEvent(const EventRecord *ev);
/* Supplied by the Window Manager: returns a pending update event, if any. */
extern bool (*gUpdateEventHook)(EventRecord *ev);

/* GetKeys(): keys[i] is true while the key with Mac virtual key code i - 1
 * is down, matching the original's packed array[1..128] of boolean. */
void GetKeys(bool keys[129]);
bool Button(void);
bool StillDown(void);
void GetMouseGlobal(Point *pt);
uint16_t CurrentModifiers(void);

void InitCursor(void);
void ShowCursor(void);
void HideCursor(void);
void ObscureCursor(void);

#endif
