/*
 * platform.c - SDL2 backend (see platform.h).
 */
#include "platform.h"
#include "prefs.h"

#include <SDL.h>
#include <stdio.h>

GrafPort gScreenPort;
static uint8_t gScreenPixels[SCREEN_W * SCREEN_H_MAX];
static int gScreenH = 480;

static SDL_Window *gWindow;
static SDL_Renderer *gRenderer;
static SDL_Texture *gTexture;
static uint32_t gRGBA[SCREEN_W * SCREEN_H_MAX];
static uint64_t gLastPresent;
static bool gQuit;
bool (*gUpdateEventHook)(EventRecord *ev);

#define QUEUE_SIZE 64
static EventRecord gQueue[QUEUE_SIZE];
static int gQueueHead, gQueueCount;

static bool gMacKeys[128];
static bool gButton;
static bool gCursorHidden, gCursorObscured;

/* Scripted input for automated testing (BR_TEST_SCRIPT=<file>). Each line
 * is "<tick> <command> [args]" with ticks (1/60 s) counted from start-up:
 *   down X Y | up X Y | move X Y | key NAME | keyup NAME | text CHARS |
 *   mods cmd|option|shift|none | shot FILE.bmp | quit
 * Coordinates are screen pixels; NAME is an SDL scancode name. */
typedef struct { uint32_t tick; char cmd[16]; char arg[200]; } TestStep;
static TestStep *gTest;
static int gTestCount, gTestNext;
static uint32_t gTestStart;
static bool gTestMouse;
static Point gTestPos;
static uint16_t gTestMods;
static void RunTestScript(void);

/* SDL scancode -> Mac virtual key code (ADB extended keyboard, ANSI). */
static int MacKeyCode(SDL_Scancode sc)
{
    switch (sc) {
    case SDL_SCANCODE_A: return 0x00; case SDL_SCANCODE_S: return 0x01; case SDL_SCANCODE_D: return 0x02;
    case SDL_SCANCODE_F: return 0x03; case SDL_SCANCODE_H: return 0x04; case SDL_SCANCODE_G: return 0x05;
    case SDL_SCANCODE_Z: return 0x06; case SDL_SCANCODE_X: return 0x07; case SDL_SCANCODE_C: return 0x08;
    case SDL_SCANCODE_V: return 0x09; case SDL_SCANCODE_NONUSBACKSLASH: return 0x0A;
    case SDL_SCANCODE_B: return 0x0B; case SDL_SCANCODE_Q: return 0x0C; case SDL_SCANCODE_W: return 0x0D;
    case SDL_SCANCODE_E: return 0x0E; case SDL_SCANCODE_R: return 0x0F; case SDL_SCANCODE_Y: return 0x10;
    case SDL_SCANCODE_T: return 0x11; case SDL_SCANCODE_1: return 0x12; case SDL_SCANCODE_2: return 0x13;
    case SDL_SCANCODE_3: return 0x14; case SDL_SCANCODE_4: return 0x15; case SDL_SCANCODE_6: return 0x16;
    case SDL_SCANCODE_5: return 0x17; case SDL_SCANCODE_EQUALS: return 0x18; case SDL_SCANCODE_9: return 0x19;
    case SDL_SCANCODE_7: return 0x1A; case SDL_SCANCODE_MINUS: return 0x1B; case SDL_SCANCODE_8: return 0x1C;
    case SDL_SCANCODE_0: return 0x1D; case SDL_SCANCODE_RIGHTBRACKET: return 0x1E; case SDL_SCANCODE_O: return 0x1F;
    case SDL_SCANCODE_U: return 0x20; case SDL_SCANCODE_LEFTBRACKET: return 0x21; case SDL_SCANCODE_I: return 0x22;
    case SDL_SCANCODE_P: return 0x23; case SDL_SCANCODE_RETURN: return 0x24; case SDL_SCANCODE_L: return 0x25;
    case SDL_SCANCODE_J: return 0x26; case SDL_SCANCODE_APOSTROPHE: return 0x27; case SDL_SCANCODE_K: return 0x28;
    case SDL_SCANCODE_SEMICOLON: return 0x29; case SDL_SCANCODE_BACKSLASH: return 0x2A; case SDL_SCANCODE_COMMA: return 0x2B;
    case SDL_SCANCODE_SLASH: return 0x2C; case SDL_SCANCODE_N: return 0x2D; case SDL_SCANCODE_M: return 0x2E;
    case SDL_SCANCODE_PERIOD: return 0x2F; case SDL_SCANCODE_TAB: return 0x30; case SDL_SCANCODE_SPACE: return 0x31;
    case SDL_SCANCODE_GRAVE: return 0x32; case SDL_SCANCODE_BACKSPACE: return 0x33; case SDL_SCANCODE_ESCAPE: return 0x35;
    case SDL_SCANCODE_LGUI: case SDL_SCANCODE_RGUI: return 0x37;
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return 0x38;
    case SDL_SCANCODE_CAPSLOCK: return 0x39;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x3A;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x3B;
    case SDL_SCANCODE_KP_PERIOD: return 0x41; case SDL_SCANCODE_KP_MULTIPLY: return 0x43;
    case SDL_SCANCODE_KP_PLUS: return 0x45; case SDL_SCANCODE_NUMLOCKCLEAR: return 0x47;
    case SDL_SCANCODE_KP_DIVIDE: return 0x4B; case SDL_SCANCODE_KP_ENTER: return 0x4C;
    case SDL_SCANCODE_KP_MINUS: return 0x4E; case SDL_SCANCODE_KP_EQUALS: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52; case SDL_SCANCODE_KP_1: return 0x53; case SDL_SCANCODE_KP_2: return 0x54;
    case SDL_SCANCODE_KP_3: return 0x55; case SDL_SCANCODE_KP_4: return 0x56; case SDL_SCANCODE_KP_5: return 0x57;
    case SDL_SCANCODE_KP_6: return 0x58; case SDL_SCANCODE_KP_7: return 0x59; case SDL_SCANCODE_KP_8: return 0x5B;
    case SDL_SCANCODE_KP_9: return 0x5C;
    case SDL_SCANCODE_F5: return 0x60; case SDL_SCANCODE_F6: return 0x61; case SDL_SCANCODE_F7: return 0x62;
    case SDL_SCANCODE_F3: return 0x63; case SDL_SCANCODE_F8: return 0x64; case SDL_SCANCODE_F9: return 0x65;
    case SDL_SCANCODE_F11: return 0x67; case SDL_SCANCODE_F13: return 0x69; case SDL_SCANCODE_F14: return 0x6B;
    case SDL_SCANCODE_F10: return 0x6D; case SDL_SCANCODE_F12: return 0x6F; case SDL_SCANCODE_F15: return 0x71;
    case SDL_SCANCODE_INSERT: return 0x72; case SDL_SCANCODE_HOME: return 0x73; case SDL_SCANCODE_PAGEUP: return 0x74;
    case SDL_SCANCODE_DELETE: return 0x75; case SDL_SCANCODE_F4: return 0x76; case SDL_SCANCODE_END: return 0x77;
    case SDL_SCANCODE_F2: return 0x78; case SDL_SCANCODE_PAGEDOWN: return 0x79; case SDL_SCANCODE_F1: return 0x7A;
    case SDL_SCANCODE_LEFT: return 0x7B; case SDL_SCANCODE_RIGHT: return 0x7C; case SDL_SCANCODE_DOWN: return 0x7D;
    case SDL_SCANCODE_UP: return 0x7E;
    default: return -1;
    }
}

uint16_t CurrentModifiers(void)
{
    SDL_Keymod m = SDL_GetModState();
    uint16_t r = 0;
    if (m & (KMOD_GUI | KMOD_CTRL)) r |= cmdKey;   /* Command: Cmd on macOS, Ctrl elsewhere */
    if (m & KMOD_SHIFT) r |= shiftKey;
    if (m & KMOD_ALT) r |= optionKey;
    if (m & KMOD_CAPS) r |= alphaLock;
    r |= gTestMods;
    if (!gButton) r |= 0x0080;                     /* btnState: set while the button is up */
    return r;
}

static void Enqueue(INTEGER what, uint32_t message, Point where, uint16_t mods)
{
    if (gQueueCount == QUEUE_SIZE) return;
    EventRecord *e = &gQueue[(gQueueHead + gQueueCount++) % QUEUE_SIZE];
    e->what = what;
    e->message = message;
    e->when = TickCount();
    e->where = where;
    e->modifiers = mods;
}

void PostEvent(const EventRecord *ev) { Enqueue(ev->what, ev->message, ev->where, ev->modifiers); }

static Point WindowToScreen(int x, int y)
{
    float lx = 0, ly = 0;
    SDL_RenderWindowToLogical(gRenderer, x, y, &lx, &ly);
    Point p;
    p.h = (INTEGER)SDL_clamp((int)lx, 0, SCREEN_W - 1);
    p.v = (INTEGER)SDL_clamp((int)ly, 0, gScreenH - 1);
    return p;
}

static void ApplyWindowPrefs(void)
{
    SDL_SetWindowFullscreen(gWindow, gPortPrefs.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool Plat_Init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) return false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    int w = gPortPrefs.windowW > 0 ? gPortPrefs.windowW : SCREEN_W * 2;
    int h = gPortPrefs.windowH > 0 ? gPortPrefs.windowH : 480 * 2;
    gWindow = SDL_CreateWindow("Burning Rubber", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!gWindow) return false;
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
    if (!gRenderer) return false;
    gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 SCREEN_W, SCREEN_H_MAX);
    if (!gTexture) return false;
    Plat_SetScreenHeight(480);
    ApplyWindowPrefs();
    SDL_StartTextInput();

    const char *script = SDL_getenv("BR_TEST_SCRIPT");
    FILE *f = script ? fopen(script, "r") : NULL;
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            TestStep st;
            memset(&st, 0, sizeof st);
            if (sscanf(line, "%u %15s %199[^\n]", &st.tick, st.cmd, st.arg) < 2) continue;
            TestStep *n = realloc(gTest, sizeof(TestStep) * (size_t)(gTestCount + 1));
            if (!n) break;
            gTest = n;
            gTest[gTestCount++] = st;
        }
        fclose(f);
        gTestMouse = true;
        gTestStart = TickCount();
    }

    memset(&gScreenPort, 0, sizeof gScreenPort);
    gScreenPort.base = gScreenPixels;
    gScreenPort.stride = SCREEN_W;
    PortReset(&gScreenPort);
    SetRect(&gScreenPort.bounds, 0, 0, SCREEN_W, gScreenH);
    gScreenPort.portRect = gScreenPort.clip = gScreenPort.bounds;
    SetPort(&gScreenPort);   /* InitGraf */
    return true;
}

void Plat_Shutdown(void)
{
    if (gWindow && !gPortPrefs.fullscreen) SDL_GetWindowSize(gWindow, &gPortPrefs.windowW, &gPortPrefs.windowH);
    if (gTexture) SDL_DestroyTexture(gTexture);
    if (gRenderer) SDL_DestroyRenderer(gRenderer);
    if (gWindow) SDL_DestroyWindow(gWindow);
    SDL_Quit();
}

int Plat_ScreenHeight(void) { return gScreenH; }

void Plat_SetScreenHeight(int h)
{
    if (h > SCREEN_H_MAX) h = SCREEN_H_MAX;
    gScreenH = h;
    SetRect(&gScreenPort.bounds, 0, 0, SCREEN_W, h);
    gScreenPort.portRect = gScreenPort.bounds;
    gScreenPort.clip = gScreenPort.bounds;
    if (gRenderer) SDL_RenderSetLogicalSize(gRenderer, SCREEN_W, h);
}

void Plat_Present(void)
{
    uint32_t pal[256];
    for (int i = 0; i < 256; i++)
        pal[i] = 0xFF000000u | ((uint32_t)gHWPalette[i][0] << 16) | ((uint32_t)gHWPalette[i][1] << 8) | gHWPalette[i][2];
    int n = SCREEN_W * gScreenH;
    for (int i = 0; i < n; i++) gRGBA[i] = pal[gScreenPixels[i]];
    SDL_Rect src = { 0, 0, SCREEN_W, gScreenH };
    SDL_UpdateTexture(gTexture, &src, gRGBA, SCREEN_W * 4);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);
    SDL_RenderCopy(gRenderer, gTexture, &src, NULL);
    SDL_RenderPresent(gRenderer);
    gPaletteDirty = false;
    gLastPresent = SDL_GetTicks64();
}

static void SaveScreenshot(const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, gScreenH, 8, SDL_PIXELFORMAT_INDEX8);
    if (!s) return;
    SDL_Color c[256];
    for (int i = 0; i < 256; i++) { c[i].r = gHWPalette[i][0]; c[i].g = gHWPalette[i][1]; c[i].b = gHWPalette[i][2]; c[i].a = 255; }
    SDL_SetPaletteColors(s->format->palette, c, 0, 256);
    for (int y = 0; y < gScreenH; y++) memcpy((uint8_t *)s->pixels + y * s->pitch, gScreenPixels + y * SCREEN_W, SCREEN_W);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}

static void RunTestScript(void)
{
    while (gTestNext < gTestCount && TickCount() - gTestStart >= gTest[gTestNext].tick) {
        TestStep *st = &gTest[gTestNext++];
        int x = 0, y = 0;
        sscanf(st->arg, "%d %d", &x, &y);
        Point p = { (INTEGER)y, (INTEGER)x };
        if (!strcmp(st->cmd, "move")) gTestPos = p;
        else if (!strcmp(st->cmd, "down")) { gTestPos = p; gButton = true; Enqueue(mouseDown, 0, p, CurrentModifiers()); }
        else if (!strcmp(st->cmd, "up")) { gTestPos = p; gButton = false; Enqueue(mouseUp, 0, p, CurrentModifiers()); }
        else if (!strcmp(st->cmd, "key") || !strcmp(st->cmd, "keyup")) {
            SDL_Scancode sc = SDL_GetScancodeFromName(st->arg);
            int k = MacKeyCode(sc);
            bool down = !strcmp(st->cmd, "key");
            if (k >= 0) gMacKeys[k] = down;
            if (down) {
                SDL_Keycode sym = SDL_GetKeyFromScancode(sc);
                int ch = sym == SDLK_RETURN ? 13 : sym == SDLK_ESCAPE ? 27 : (sym >= 32 && sym < 127) ? (int)sym : 0x10;
                Enqueue(keyDown, (uint32_t)(((k < 0 ? 0 : k) << 8) | ch), (Point){ 0, 0 }, CurrentModifiers());
            }
        }
        else if (!strcmp(st->cmd, "text"))
            for (const char *c = st->arg; *c; c++) Enqueue(keyDown, (unsigned char)*c, (Point){ 0, 0 }, CurrentModifiers());
        else if (!strcmp(st->cmd, "mods"))
            gTestMods = !strcmp(st->arg, "cmd") ? cmdKey : !strcmp(st->arg, "option") ? optionKey :
                        !strcmp(st->arg, "shift") ? shiftKey : 0;
        else if (!strcmp(st->cmd, "shot")) SaveScreenshot(st->arg);
        else if (!strcmp(st->cmd, "quit")) gQuit = true;
    }
}

void Plat_Pump(void)
{
    SDL_Event e;
    if (gTest) RunTestScript();
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            gQuit = true;
            break;
        case SDL_KEYDOWN: {
            int k = MacKeyCode(e.key.keysym.scancode);
            if (k >= 0) gMacKeys[k] = true;
            uint16_t mods = CurrentModifiers();
            SDL_Keycode sym = e.key.keysym.sym;
            if (sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                gPortPrefs.fullscreen = !gPortPrefs.fullscreen;
                ApplyWindowPrefs();
                break;
            }
            /* Printable characters arrive as SDL_TEXTINPUT unless Command is
             * held (menu shortcuts); everything else is posted here. */
            int ch = 0;
            switch (sym) {
            case SDLK_RETURN: ch = 13; break;
            case SDLK_KP_ENTER: ch = 3; break;
            case SDLK_BACKSPACE: ch = 8; break;
            case SDLK_TAB: ch = 9; break;
            case SDLK_ESCAPE: ch = 27; break;
            case SDLK_LEFT: ch = 28; break;
            case SDLK_RIGHT: ch = 29; break;
            case SDLK_UP: ch = 30; break;
            case SDLK_DOWN: ch = 31; break;
            case SDLK_DELETE: ch = 127; break;
            default:
                if ((mods & cmdKey) && sym >= 32 && sym < 127) ch = (int)sym;
                else if (sym >= 32 && sym < 127) ch = -1;   /* comes as text */
                break;
            }
            if (ch == 0 && k >= 0) ch = 0x10;   /* function keys etc. */
            if (ch > 0 && !e.key.repeat)
                Enqueue(keyDown, (uint32_t)(((k < 0 ? 0 : k) << 8) | ch), (Point){ 0, 0 }, mods);
            else if (ch > 0)
                Enqueue(autoKey, (uint32_t)(((k < 0 ? 0 : k) << 8) | ch), (Point){ 0, 0 }, mods);
            break;
        }
        case SDL_KEYUP: {
            int k = MacKeyCode(e.key.keysym.scancode);
            if (k >= 0) gMacKeys[k] = false;
            break;
        }
        case SDL_TEXTINPUT: {
            uint16_t mods = CurrentModifiers();
            if (mods & cmdKey) break;
            for (const unsigned char *s = (const unsigned char *)e.text.text; *s; s++)
                if (*s >= 32 && *s < 127) Enqueue(keyDown, *s, (Point){ 0, 0 }, mods);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                gButton = true;
                Enqueue(mouseDown, 0, WindowToScreen(e.button.x, e.button.y), CurrentModifiers());
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                gButton = false;
                Enqueue(mouseUp, 0, WindowToScreen(e.button.x, e.button.y), CurrentModifiers());
            }
            break;
        case SDL_MOUSEMOTION:
            if (gCursorObscured) {
                gCursorObscured = false;
                if (!gCursorHidden) SDL_ShowCursor(SDL_ENABLE);
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                memset(gMacKeys, 0, sizeof gMacKeys);
                gButton = false;
                Enqueue(osEvt, 0x01000000u, (Point){ 0, 0 }, CurrentModifiers());   /* suspend */
            } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                Enqueue(osEvt, 0x01000001u, (Point){ 0, 0 }, CurrentModifiers());   /* resume */
            }
            gLastPresent = 0;
            break;
        default:
            break;
        }
    }
    /* Present at most ~60 times a second; this is the only path to the display. */
    if (SDL_GetTicks64() - gLastPresent >= 15) Plat_Present();
}

bool Plat_QuitRequested(void) { return gQuit; }
void Plat_ClearQuit(void) { gQuit = false; }

void Plat_Sleep(int ms) { Plat_Pump(); SDL_Delay((Uint32)ms); }

uint32_t TickCount(void) { return (uint32_t)(SDL_GetTicks64() * 60 / 1000); }

void WaitUntilTick(uint32_t tick)
{
    for (;;) {
        Plat_Pump();
        if ((int32_t)(TickCount() - tick) >= 0) break;
        SDL_Delay(1);
    }
}

void Delay(long ticks, long *finalTicks)
{
    WaitUntilTick(TickCount() + (uint32_t)(ticks > 0 ? ticks : 0));
    if (finalTicks) *finalTicks = (long)TickCount();
}

bool GetNextEvent(int mask, EventRecord *ev)
{
    Plat_Pump();
    for (int i = 0; i < gQueueCount; i++) {
        EventRecord *e = &gQueue[(gQueueHead + i) % QUEUE_SIZE];
        if (!(mask & (1 << e->what))) continue;
        *ev = *e;
        for (int j = i; j > 0; j--)
            gQueue[(gQueueHead + j) % QUEUE_SIZE] = gQueue[(gQueueHead + j - 1) % QUEUE_SIZE];
        gQueueHead = (gQueueHead + 1) % QUEUE_SIZE;
        gQueueCount--;
        return true;
    }
    if ((mask & (1 << updateEvt)) && gUpdateEventHook && gUpdateEventHook(ev)) return true;
    memset(ev, 0, sizeof *ev);
    ev->what = nullEvent;
    ev->when = TickCount();
    ev->modifiers = CurrentModifiers();
    GetMouseGlobal(&ev->where);
    return false;
}

bool WaitNextEvent(int mask, EventRecord *ev, long sleep)
{
    if (GetNextEvent(mask, ev)) return true;
    if (sleep > 0) {
        uint32_t until = TickCount() + (uint32_t)sleep;
        while ((int32_t)(TickCount() - until) < 0) {
            SDL_Delay(1);
            if (GetNextEvent(mask, ev)) return true;
        }
    } else {
        SDL_Delay(1);
    }
    return false;
}

void FlushEvents(int mask, int stopMask)
{
    (void)stopMask;
    Plat_Pump();
    int kept = 0;
    EventRecord tmp[QUEUE_SIZE];
    for (int i = 0; i < gQueueCount; i++) {
        EventRecord *e = &gQueue[(gQueueHead + i) % QUEUE_SIZE];
        if (!(mask & (1 << e->what))) tmp[kept++] = *e;
    }
    gQueueHead = 0;
    gQueueCount = kept;
    memcpy(gQueue, tmp, sizeof(EventRecord) * (size_t)kept);
}

void GetKeys(bool keys[129])
{
    Plat_Pump();
    keys[0] = false;
    for (int i = 0; i < 128; i++) keys[i + 1] = gMacKeys[i];
}

bool Button(void) { Plat_Pump(); return gButton; }
bool StillDown(void) { Plat_Pump(); return gButton; }

void GetMouseGlobal(Point *pt)
{
    if (gTestMouse) { *pt = gTestPos; return; }
    int x, y;
    SDL_GetMouseState(&x, &y);
    *pt = WindowToScreen(x, y);
}

void InitCursor(void) { gCursorHidden = false; gCursorObscured = false; SDL_ShowCursor(SDL_ENABLE); }
void ShowCursor(void) { gCursorHidden = false; gCursorObscured = false; SDL_ShowCursor(SDL_ENABLE); }
void HideCursor(void) { gCursorHidden = true; SDL_ShowCursor(SDL_DISABLE); }

void ObscureCursor(void)
{
    if (!gCursorObscured) {
        gCursorObscured = true;
        SDL_ShowCursor(SDL_DISABLE);
    }
}
