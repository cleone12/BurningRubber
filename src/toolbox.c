/*
 * toolbox.c - Window, Menu, Dialog and Alert Managers (see toolbox.h).
 */
#include "toolbox.h"
#include "resources.h"
#include "text.h"

#include <SDL.h>
#include <stdio.h>

void (*gQuitHook)(void);

/* ================================================================== */
/* Window Manager                                                      */
/* ================================================================== */

enum { documentProc = 0, dBoxProc = 1, plainDBoxProc = 2, noGrowDocProc = 4 };
#define MAX_WINDOWS 16
#define MENUBAR_H 20

struct WindowRecord {
    GrafPort port;
    int procID;
    Rect content;           /* global coordinates */
    Point origin;           /* local coordinates of the content's top-left */
    Str255 title;
    uint8_t contentColor;
    bool clipsMenuBar;
    bool needsUpdate;
    int slot;
    struct WindowRecord *next;
};

static WindowPtr gWindowList;          /* front to back */
static WindowPtr gSlots[MAX_WINDOWS];
static int gMBarHeight = MENUBAR_H;

static void DrawMenuBarNow(void);

/* Content colour from a 'wctb'/'dctb'/'actb' (part 0 = wContentColor). */
static uint8_t ContentColor(uint32_t type, int id, uint8_t fallback)
{
    int32_t size;
    const uint8_t *d = GetResource(type, id, &size);
    if (!d || size < 8) return fallback;
    int n = rd16s(d + 6) + 1;
    for (int i = 0; i < n && 8 + i * 8 + 8 <= size; i++)
        if (rd16s(d + 8 + i * 8) == 0) {
            RGBColor c = { rd16(d + 10 + i * 8), rd16(d + 12 + i * 8), rd16(d + 14 + i * 8) };
            return Color2Index(&c);
        }
    return fallback;
}

static void UpdatePort(WindowPtr w)
{
    GrafPort *p = &w->port;
    int cw = w->content.right - w->content.left, ch = w->content.bottom - w->content.top;
    p->base = gScreenPort.base;
    p->stride = gScreenPort.stride;
    SetRect(&p->bounds, w->origin.h - w->content.left, w->origin.v - w->content.top,
            w->origin.h - w->content.left + SCREEN_W, w->origin.v - w->content.top + Plat_ScreenHeight());
    SetRect(&p->portRect, w->origin.h, w->origin.v, w->origin.h + cw, w->origin.v + ch);
    p->clip = p->portRect;
    if (w->clipsMenuBar) {
        Rect below = p->bounds;
        below.top = (INTEGER)(p->bounds.top + MENUBAR_H);
        SectRect(&p->clip, &below, &p->clip);
    }
}

static void StructRect(int procID, const Rect *c, Rect *s)
{
    *s = *c;
    switch (procID) {
    case dBoxProc: InsetRect(s, -8, -8); break;
    case plainDBoxProc: InsetRect(s, -1, -1); break;
    default: SetRect(s, c->left - 1, c->top - 19, c->right + 2, c->bottom + 2); break;
    }
}

static void DrawFrame(int procID, const Rect *c, const unsigned char *title, uint8_t contentColor)
{
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    Rect r;
    switch (procID) {
    case plainDBoxProc:
        r = *c; InsetRect(&r, -1, -1);
        PenNormal(); gScreenPort.fg = kBlackIndex; FrameRect(&r);
        break;
    case dBoxProc:
        r = *c; InsetRect(&r, -8, -8);
        FillRectIndex(&r, kBlackIndex);
        InsetRect(&r, 1, 1); FillRectIndex(&r, kWhiteIndex);
        InsetRect(&r, 1, 1); FillRectIndex(&r, kBlackIndex);
        InsetRect(&r, 2, 2); FillRectIndex(&r, contentColor);
        break;
    default: {
        Rect t;
        SetRect(&t, c->left - 1, c->top - 19, c->right + 1, c->top);
        FillRectIndex(&t, kBlackIndex);
        Rect in = t; InsetRect(&in, 1, 1);
        FillRectIndex(&in, kWhiteIndex);
        for (int y = t.top + 4; y <= t.top + 14; y += 2) {
            SetRect(&r, t.left + 3, y, t.right - 3, y + 1);
            FillRectIndex(&r, kBlackIndex);
        }
        TextFont(systemFont); TextSize(12); TextMode(srcOr);
        int tw = StringWidth(title);
        if (title[0]) {
            int mid = (t.left + t.right) / 2;
            SetRect(&r, mid - tw / 2 - 6, t.top + 1, mid + tw / 2 + 6, t.bottom - 1);
            FillRectIndex(&r, kWhiteIndex);
            gScreenPort.fg = kBlackIndex;
            MoveTo(mid - tw / 2, t.top + 14);
            DrawString(title);
        }
        r = *c; InsetRect(&r, -1, -1);
        PenNormal(); gScreenPort.fg = kBlackIndex; FrameRect(&r);
        SetRect(&r, c->right + 1, t.top + 1, c->right + 2, c->bottom + 2); FillRectIndex(&r, kBlackIndex);
        SetRect(&r, t.left + 1, c->bottom + 1, c->right + 2, c->bottom + 2); FillRectIndex(&r, kBlackIndex);
        break;
    }
    }
    SetPort(save);
}

static void DrawDesktop(const Rect *area)
{
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    Rect v = *area;
    if (SectRect(&v, &gScreenPort.bounds, &v))
        for (int y = v.top; y < v.bottom; y++)
            for (int x = v.left; x < v.right; x++)
                gScreenPort.base[y * SCREEN_W + x] = ((x ^ y) & 1) ? 86 : 249;   /* 50% grey dither */
    SetPort(save);
}

static void DrawWindowAll(WindowPtr w)
{
    DrawFrame(w->procID, &w->content, w->title, w->contentColor);
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    if (w->clipsMenuBar) gScreenPort.clip.top = MENUBAR_H;
    FillRectIndex(&w->content, w->contentColor);
    SetPort(save);
    w->needsUpdate = true;
}

void RedrawScreen(void)
{
    DrawDesktop(&gScreenPort.bounds);
    WindowPtr order[MAX_WINDOWS];
    int n = 0;
    for (WindowPtr w = gWindowList; w && n < MAX_WINDOWS; w = w->next) order[n++] = w;
    for (int i = n - 1; i >= 0; i--) DrawWindowAll(order[i]);
    if (gMBarHeight) DrawMenuBarNow();
}

static void FitScreen(void)
{
    int need = 480;
    for (WindowPtr w = gWindowList; w; w = w->next) {
        Rect s;
        StructRect(w->procID, &w->content, &s);
        if (w->procID == plainDBoxProc) s = w->content;   /* its border may sit off-screen */
        if (s.bottom > need) need = s.bottom;
    }
    if (need > SCREEN_H_MAX) need = SCREEN_H_MAX;
    if (need != Plat_ScreenHeight()) {
        Plat_SetScreenHeight(need);
        for (WindowPtr w = gWindowList; w; w = w->next) UpdatePort(w);
    }
}

/* Places content rect c (size only meaningful) according to a WIND/DLOG/ALRT
 * positioning word, relative to the main screen or the front window. */
static void Position(Rect *c, int procID, uint16_t pos)
{
    int w = c->right - c->left, h = c->bottom - c->top;
    Rect s, area;
    StructRect(procID, c, &s);
    int sw = s.right - s.left, sh = s.bottom - s.top;
    int kind = pos & 0x7FFF;
    if (kind != 0x280A && kind != 0x300A && kind != 0x380A && kind != 0x680A && kind != 0x700A) {
        if ((pos & 0xFF) != 0x0A) return;   /* no auto-positioning */
    }
    int screenH = Plat_ScreenHeight();
    if (sh + MENUBAR_H > screenH && sh + MENUBAR_H <= SCREEN_H_MAX) screenH = sh + MENUBAR_H;
    SetRect(&area, 0, MENUBAR_H, SCREEN_W, screenH);
    WindowPtr parent = gWindowList;
    if ((pos & 0x8000) && parent) StructRect(parent->procID, &parent->content, &area);
    bool alertPos = (pos & 0x7F00) == 0x3000 || (pos & 0x7F00) == 0x7000;
    int left = area.left + (area.right - area.left - sw) / 2;
    int top = alertPos ? area.top + (area.bottom - area.top - sh) / 3 : area.top + (area.bottom - area.top - sh) / 2;
    if (top < MENUBAR_H) top = MENUBAR_H;
    SetRect(c, left + (c->left - s.left), top + (c->top - s.top), 0, 0);
    c->right = (INTEGER)(c->left + w);
    c->bottom = (INTEGER)(c->top + h);
}

WindowPtr GetNewWindow(int id, WindowPtr behind)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('W', 'I', 'N', 'D'), id, &size);
    if (!d || size < 19) return NULL;
    WindowPtr w = calloc(1, sizeof(struct WindowRecord));
    if (!w) return NULL;
    SetRect(&w->content, rd16s(d + 2), rd16s(d), rd16s(d + 6), rd16s(d + 4));
    w->procID = rd16s(d + 8);
    PStrCopy(w->title, d + 18);
    int32_t posAt = 18 + 1 + d[18];
    if (posAt & 1) posAt++;
    uint16_t pos = posAt + 2 <= size ? rd16(d + posAt) : 0;
    w->contentColor = ContentColor(FOURCC('w', 'c', 't', 'b'), id, kWhiteIndex);
    w->clipsMenuBar = true;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!gSlots[i]) { gSlots[i] = w; w->slot = i; break; }
    Position(&w->content, w->procID, pos);
    PortReset(&w->port);   /* a window's background colour stays white */
    if (!behind) {
        w->next = gWindowList;
        gWindowList = w;
    } else {
        w->next = behind->next;
        behind->next = w;
    }
    int oldH = Plat_ScreenHeight();
    FitScreen();
    UpdatePort(w);
    if (!behind && oldH == Plat_ScreenHeight()) DrawWindowAll(w);
    else RedrawScreen();
    return w;
}

void DisposeWindow(WindowPtr w)
{
    if (!w) return;
    WindowPtr *pp = &gWindowList;
    while (*pp && *pp != w) pp = &(*pp)->next;
    if (*pp) *pp = w->next;
    gSlots[w->slot] = NULL;
    if (thePort == &w->port) thePort = &gScreenPort;
    free(w);
    FitScreen();
    RedrawScreen();
}

GrafPtr WindowPort(WindowPtr w) { return w ? &w->port : NULL; }

void MoveWindow(WindowPtr w, int h, int v, bool front)
{
    (void)front;
    int cw = w->content.right - w->content.left, ch = w->content.bottom - w->content.top;
    SetRect(&w->content, h, v, h + cw, v + ch);
    FitScreen();
    UpdatePort(w);
}

void SizeWindow(WindowPtr w, int width, int height, bool update)
{
    w->content.right = (INTEGER)(w->content.left + width);
    w->content.bottom = (INTEGER)(w->content.top + height);
    FitScreen();
    UpdatePort(w);
    /* the Window Manager paints newly exposed content in the content colour */
    DrawWindowAll(w);
    if (!update) w->needsUpdate = false;
}

static WindowPtr WindowOfPort(GrafPtr p)
{
    for (WindowPtr w = gWindowList; w; w = w->next)
        if (&w->port == p) return w;
    return NULL;
}

void SetOrigin(int h, int v)
{
    WindowPtr w = WindowOfPort(thePort);
    if (!w) return;
    w->origin.h = (INTEGER)h;
    w->origin.v = (INTEGER)v;
    UpdatePort(w);
}

void SetWindowClipsMenuBar(WindowPtr w, bool clips)
{
    if (!w) return;
    w->clipsMenuBar = clips;
    UpdatePort(w);
}

int FindWindow(Point where, WindowPtr *wOut)
{
    *wOut = NULL;
    if (gMBarHeight && where.v < gMBarHeight) return inMenuBar;
    for (WindowPtr w = gWindowList; w; w = w->next) {
        if (PtInRect(where, &w->content)) { *wOut = w; return inContent; }
        Rect s;
        StructRect(w->procID, &w->content, &s);
        if (PtInRect(where, &s)) {
            *wOut = w;
            return (w->procID == documentProc || w->procID == noGrowDocProc) && where.v < w->content.top ? inDrag : inContent;
        }
    }
    return inDesk;
}

void DragWindow(WindowPtr w, Point start)
{
    Rect s, o;
    StructRect(w->procID, &w->content, &s);
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    gScreenPort.clip.top = MENUBAR_H;
    PenSize(1, 1);
    Point p = start, last = start;
    o = s;
    /* dotted outline drawn with XOR, like the Window Manager's gray outline */
    #define OUTLINE(rr) do { Rect e_; \
        SetRect(&e_, (rr).left, (rr).top, (rr).right, (rr).top + 1); InvertRect(&e_); \
        SetRect(&e_, (rr).left, (rr).bottom - 1, (rr).right, (rr).bottom); InvertRect(&e_); \
        SetRect(&e_, (rr).left, (rr).top + 1, (rr).left + 1, (rr).bottom - 1); InvertRect(&e_); \
        SetRect(&e_, (rr).right - 1, (rr).top + 1, (rr).right, (rr).bottom - 1); InvertRect(&e_); } while (0)
    OUTLINE(o);
    while (StillDown()) {
        GetMouseGlobal(&p);
        if (p.v < MENUBAR_H) p.v = MENUBAR_H;
        if (p.h != last.h || p.v != last.v) {
            OUTLINE(o);
            o = s;
            OffsetRect(&o, p.h - start.h, p.v - start.v);
            OUTLINE(o);
            last = p;
        }
        SDL_Delay(5);
    }
    OUTLINE(o);
    #undef OUTLINE
    SetPort(save);
    int dh = last.h - start.h, dv = last.v - start.v;
    /* keep the whole window on the screen */
    if (s.left + dh < 0) dh = -s.left;
    if (s.right + dh > SCREEN_W) dh = SCREEN_W - s.right;
    if (s.bottom + dv > Plat_ScreenHeight()) dv = Plat_ScreenHeight() - s.bottom;
    if (s.top + dv < MENUBAR_H) dv = MENUBAR_H - s.top;
    if (dh || dv) {
        MoveWindow(w, w->content.left + dh, w->content.top + dv, false);
        RedrawScreen();
    }
}

WindowPtr EventWindow(const EventRecord *ev)
{
    if (ev->message < MAX_WINDOWS) return gSlots[ev->message];
    return NULL;
}

static bool UpdateHook(EventRecord *ev)
{
    /* back to front, like the Window Manager's update order is not
     * guaranteed; any order works for this game */
    for (WindowPtr w = gWindowList; w; w = w->next)
        if (w->needsUpdate) {
            w->needsUpdate = false;
            memset(ev, 0, sizeof *ev);
            ev->what = updateEvt;
            ev->message = (uint32_t)w->slot;
            ev->when = TickCount();
            ev->modifiers = CurrentModifiers();
            return true;
        }
    return false;
}

void InvalRect(const Rect *r)
{
    (void)r;
    WindowPtr w = WindowOfPort(thePort);
    if (w) w->needsUpdate = true;
}

void BeginUpdate(WindowPtr w) { if (w) w->needsUpdate = false; }
void EndUpdate(WindowPtr w) { (void)w; }

void GlobalToLocal(Point *p) { p->h += thePort->bounds.left; p->v += thePort->bounds.top; }
void LocalToGlobal(Point *p) { p->h -= thePort->bounds.left; p->v -= thePort->bounds.top; }

void GetMouse(Point *p)
{
    GetMouseGlobal(p);
    GlobalToLocal(p);
}

/* ================================================================== */
/* save-under for dialogs and menus                                    */
/* ================================================================== */

typedef struct { Rect r; uint8_t *pix; } SaveUnder;

static void SaveBits(SaveUnder *s, const Rect *r)
{
    s->r = *r;
    if (!SectRect(&s->r, &gScreenPort.bounds, &s->r)) { s->pix = NULL; return; }
    int w = s->r.right - s->r.left, h = s->r.bottom - s->r.top;
    s->pix = malloc((size_t)w * h);
    if (!s->pix) return;
    for (int y = 0; y < h; y++)
        memcpy(s->pix + y * w, gScreenPort.base + (s->r.top + y) * SCREEN_W + s->r.left, (size_t)w);
}

static void RestoreBits(SaveUnder *s)
{
    if (!s->pix) return;
    int w = s->r.right - s->r.left, h = s->r.bottom - s->r.top;
    for (int y = 0; y < h; y++)
        memcpy(gScreenPort.base + (s->r.top + y) * SCREEN_W + s->r.left, s->pix + y * w, (size_t)w);
    free(s->pix);
    s->pix = NULL;
}

/* ================================================================== */
/* Menu Manager                                                        */
/* ================================================================== */

#define MAX_ITEMS 24
struct Menu {
    int id;
    Str255 title;
    int n;
    Str255 item[MAX_ITEMS];
    unsigned char key[MAX_ITEMS];
    uint32_t enable;
    uint8_t itemColor[MAX_ITEMS], keyColor[MAX_ITEMS];
    uint8_t titleColor, bgColor;
    Rect titleRect;
    bool inBar;
};

static struct Menu gMenus[8];
static int gMenuCount;
static struct Menu *gBar[8];
static int gBarCount;
static int gHilitedMenu;
static uint8_t gBarColor = kWhiteIndex, gBarTitleColor = kBlackIndex;

static uint8_t Gray(void) { return 250; }  /* 0x777777 */

static bool MctbEntry(int mctbID, int menuID, int item, RGBColor rgb[4])
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('m', 'c', 't', 'b'), mctbID, &size);
    if (!d || size < 2) return false;
    int n = rd16s(d);
    for (int i = 0; i < n && 2 + i * 30 + 30 <= size; i++) {
        const uint8_t *e = d + 2 + i * 30;
        if (rd16s(e) == menuID && rd16s(e + 2) == item) {
            for (int k = 0; k < 4; k++)
                rgb[k] = (RGBColor){ rd16(e + 4 + k * 6), rd16(e + 6 + k * 6), rd16(e + 8 + k * 6) };
            return true;
        }
    }
    return false;
}

MenuHandle GetMenu(int id)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('M', 'E', 'N', 'U'), id, &size);
    if (!d || size < 15 || gMenuCount == 8) return NULL;
    struct Menu *m = &gMenus[gMenuCount++];
    memset(m, 0, sizeof *m);
    m->id = id;
    m->enable = rd32(d + 10);
    PStrCopy(m->title, d + 14);
    int32_t o = 15 + d[14];
    while (o < size && d[o] && m->n < MAX_ITEMS) {
        PStrCopy(m->item[m->n], d + o);
        o += 1 + d[o];
        if (o + 4 > size) break;
        m->key[m->n] = d[o + 1];
        o += 4;
        m->n++;
    }
    /* colours: bar entry (0,0), menu title entry (id,0), item entries */
    RGBColor c[4];
    uint8_t defItem = kBlackIndex, defBg = kWhiteIndex;
    m->titleColor = kBlackIndex;
    if (MctbEntry(id, 0, 0, c)) {
        gBarTitleColor = Color2Index(&c[0]);
        gBarColor = Color2Index(&c[1]);
        defItem = Color2Index(&c[2]);
        defBg = Color2Index(&c[3]);
    }
    if (MctbEntry(id, id, 0, c)) {
        m->titleColor = Color2Index(&c[0]);
        defItem = Color2Index(&c[2]);
        defBg = Color2Index(&c[3]);
    } else {
        m->titleColor = gBarTitleColor;
    }
    m->bgColor = defBg;
    for (int i = 0; i < m->n; i++) {
        m->itemColor[i] = m->keyColor[i] = defItem;
        if (MctbEntry(id, id, i + 1, c)) {
            m->itemColor[i] = Color2Index(&c[1]);
            m->keyColor[i] = Color2Index(&c[2]);
        }
    }
    return m;
}

void InsertMenu(MenuHandle m, int beforeID)
{
    (void)beforeID;
    if (m && gBarCount < 8) { gBar[gBarCount++] = m; m->inBar = true; }
}

void EnableItem(MenuHandle m, int item) { if (m) m->enable |= 1u << item; }
void DisableItem(MenuHandle m, int item) { if (m) m->enable &= ~(1u << item); }

void GetMenuItemText(MenuHandle m, int item, unsigned char *out)
{
    if (m && item >= 1 && item <= m->n) PStrCopy(out, m->item[item - 1]);
    else out[0] = 0;
}

void SetMenuBarHeight(int h) { gMBarHeight = h; }
int GetMenuBarHeight(void) { return gMBarHeight; }

static void LayoutBar(void)
{
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    TextFont(systemFont); TextSize(12);
    int x = 10;
    for (int i = 0; i < gBarCount; i++) {
        int w = StringWidth(gBar[i]->title) + 20;
        SetRect(&gBar[i]->titleRect, x, 0, x + w, MENUBAR_H - 1);
        x += w;
    }
    SetPort(save);
}

static void DrawTitle(struct Menu *m, bool hilite)
{
    uint8_t bg = hilite ? m->titleColor : gBarColor, fg = hilite ? gBarColor : m->titleColor;
    FillRectIndex(&m->titleRect, bg);
    gScreenPort.fg = fg;
    TextMode(srcOr);
    MoveTo(m->titleRect.left + 10, 14);
    DrawString(m->title);
}

static void DrawMenuBarNow(void)
{
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    TextFont(systemFont); TextSize(12);
    LayoutBar();
    Rect r;
    SetRect(&r, 0, 0, SCREEN_W, MENUBAR_H - 1);
    FillRectIndex(&r, gBarColor);
    SetRect(&r, 0, MENUBAR_H - 1, SCREEN_W, MENUBAR_H);
    FillRectIndex(&r, kBlackIndex);
    for (int i = 0; i < gBarCount; i++) DrawTitle(gBar[i], gBar[i]->id == gHilitedMenu);
    /* the rounded corners of the screen */
    static const int corner[5] = { 5, 3, 2, 1, 1 };
    for (int y = 0; y < 5; y++) {
        SetRect(&r, 0, y, corner[y], y + 1); FillRectIndex(&r, kBlackIndex);
        SetRect(&r, SCREEN_W - corner[y], y, SCREEN_W, y + 1); FillRectIndex(&r, kBlackIndex);
    }
    SetPort(save);
}

void DrawMenuBar(void) { if (gMBarHeight) DrawMenuBarNow(); }

void HiliteMenu(int id)
{
    gHilitedMenu = id;
    if (gMBarHeight) DrawMenuBarNow();
}

static bool ItemEnabled(struct Menu *m, int item)
{
    return (m->enable & 1) && (m->enable & (1u << item)) && m->item[item - 1][1] != '-';
}

static void MenuGeometry(struct Menu *m, Rect *r)
{
    TextFont(systemFont); TextSize(12);
    int w = 0;
    for (int i = 0; i < m->n; i++) {
        int iw = StringWidth(m->item[i]) + (m->key[i] ? 40 : 0);
        if (iw > w) w = iw;
    }
    w += 30;
    SetRect(r, m->titleRect.left, MENUBAR_H, m->titleRect.left + w, MENUBAR_H + m->n * 16 + 2);
    if (r->right > SCREEN_W - 2) OffsetRect(r, SCREEN_W - 2 - r->right, 0);
}

static void DrawMenuItem(struct Menu *m, const Rect *mr, int item, bool hilite)
{
    Rect ir;
    SetRect(&ir, mr->left + 1, mr->top + 1 + (item - 1) * 16, mr->right - 1, mr->top + 1 + item * 16);
    bool en = ItemEnabled(m, item);
    uint8_t bg = m->bgColor, fg = en ? m->itemColor[item - 1] : Gray(), kfg = en ? m->keyColor[item - 1] : Gray();
    if (hilite && en) { uint8_t t = bg; bg = fg; fg = t; kfg = t; }
    FillRectIndex(&ir, bg);
    if (m->item[item - 1][1] == '-') {
        for (int x = ir.left; x < ir.right; x += 2) {
            Rect dot;
            SetRect(&dot, x, ir.top + 8, x + 1, ir.top + 9);
            FillRectIndex(&dot, kBlackIndex);
        }
        return;
    }
    TextMode(srcOr);
    gScreenPort.fg = fg;
    MoveTo(ir.left + 14, ir.top + 12);
    DrawString(m->item[item - 1]);
    if (m->key[item - 1]) {
        unsigned char k[3] = { 2, 0x11, m->key[item - 1] };
        gScreenPort.fg = kfg;
        MoveTo(ir.right - 10 - StringWidth(k), ir.top + 12);
        DrawString(k);
    }
}

static void DrawMenu(struct Menu *m, const Rect *mr)
{
    Rect r = *mr;
    FillRectIndex(&r, kBlackIndex);
    InsetRect(&r, 1, 1);
    FillRectIndex(&r, m->bgColor);
    Rect sh;
    SetRect(&sh, mr->right, mr->top + 2, mr->right + 1, mr->bottom + 1); FillRectIndex(&sh, kBlackIndex);
    SetRect(&sh, mr->left + 2, mr->bottom, mr->right + 1, mr->bottom + 1); FillRectIndex(&sh, kBlackIndex);
    for (int i = 1; i <= m->n; i++) DrawMenuItem(m, mr, i, false);
}

long MenuSelect(Point start)
{
    (void)start;
    GrafPtr save = thePort;
    SetPort(&gScreenPort);
    gScreenPort.clip = gScreenPort.bounds;
    LayoutBar();
    struct Menu *open = NULL;
    Rect mr;
    SaveUnder su = { 0 };
    int hiItem = 0;
    long result = 0;
    for (;;) {
        Point p;
        GetMouseGlobal(&p);
        struct Menu *over = NULL;
        if (p.v < MENUBAR_H)
            for (int i = 0; i < gBarCount; i++)
                if (PtInRect(p, &gBar[i]->titleRect)) over = gBar[i];
        if (over && over != open) {
            if (open) { RestoreBits(&su); DrawTitle(open, false); }
            open = over;
            DrawTitle(open, true);
            MenuGeometry(open, &mr);
            Rect sr = mr;
            sr.right++; sr.bottom++;
            SaveBits(&su, &sr);
            DrawMenu(open, &mr);
            hiItem = 0;
        }
        int item = 0;
        if (open && PtInRect(p, &mr)) item = (p.v - mr.top - 1) / 16 + 1;
        if (item < 1 || item > (open ? open->n : 0)) item = 0;
        if (open && item != hiItem) {
            if (hiItem) DrawMenuItem(open, &mr, hiItem, false);
            if (item) DrawMenuItem(open, &mr, item, true);
            hiItem = item;
        }
        if (!StillDown()) {
            if (open && hiItem && ItemEnabled(open, hiItem)) {
                /* blink the chosen item, as the Menu Manager does */
                for (int b = 0; b < 2; b++) {
                    DrawMenuItem(open, &mr, hiItem, false); Delay(3, NULL);
                    DrawMenuItem(open, &mr, hiItem, true); Delay(3, NULL);
                }
                result = ((long)open->id << 16) | hiItem;
            }
            break;
        }
        SDL_Delay(5);
    }
    if (open) {
        RestoreBits(&su);
        gHilitedMenu = result ? open->id : 0;
        DrawMenuBarNow();
    }
    SetPort(save);
    return result;
}

long MenuKey(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 32);
    for (int i = 0; i < gBarCount; i++) {
        struct Menu *m = gBar[i];
        for (int k = 1; k <= m->n; k++)
            if (m->key[k - 1] == ch && ItemEnabled(m, k)) {
                HiliteMenu(m->id);
                return ((long)m->id << 16) | k;
            }
    }
    return 0;
}

/* ================================================================== */
/* Dialog Manager                                                      */
/* ================================================================== */

enum { userItem = 0, btnCtrl = 4, chkCtrl = 5, radCtrl = 6, resCtrl = 7, statText = 8, editText = 16,
       iconItem = 32, picItem = 64, itemDisable = 128 };
#define MAX_DITEMS 40

typedef struct {
    uint8_t type;
    Rect rect;
    Str255 text;
    int resID;
    int value;
    int hilite;
} DItem;

struct Dialog {
    struct WindowRecord win;
    int n;
    DItem item[MAX_DITEMS];
    int defaultItem, cancelItem;
    bool ring;
    int editItem, selStart, selEnd;
    bool caretOn;
    uint32_t caretTick;
    bool drawn;
    SaveUnder save;
    struct Dialog *below;
};

static struct Dialog *gDialogStack;
static Str255 gParam[4];

void ParamText(const unsigned char *a, const unsigned char *b, const unsigned char *c, const unsigned char *d)
{
    const unsigned char *src[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) {
        if (src[i]) PStrCopy(gParam[i], src[i]);
        else gParam[i][0] = 0;
    }
}

static void Expand(const unsigned char *s, unsigned char *out)
{
    int n = 0;
    for (int i = 1; i <= s[0]; i++) {
        if (s[i] == '^' && i < s[0] && s[i + 1] >= '0' && s[i + 1] <= '3') {
            const unsigned char *p = gParam[s[i + 1] - '0'];
            for (int k = 1; k <= p[0] && n < 255; k++) out[1 + n++] = p[k];
            i++;
        } else if (n < 255) {
            out[1 + n++] = s[i];
        }
    }
    out[0] = (unsigned char)n;
}

/* TETextBox, left aligned, in the current port's font. */
static void TextBox(const unsigned char *t, const Rect *r)
{
    FontInfo fi;
    GetFontInfo(&fi);
    int lh = fi.ascent + fi.descent + fi.leading;
    int y = r->top + fi.ascent;
    int i = 1;
    while (i <= t[0] && y - fi.ascent < r->bottom) {
        int lineStart = i, lastBreak = -1, w = 0;
        while (i <= t[0] && t[i] != '\r') {
            int cw = CharWidth(t[i]);
            if (w + cw > r->right - r->left && lastBreak > 0) { i = lastBreak; break; }
            if (t[i] == ' ') lastBreak = i + 1;
            w += cw;
            i++;
        }
        int end = i;
        while (end > lineStart && t[end - 1] == ' ') end--;
        MoveTo(r->left, y);
        DrawText(t, lineStart, end - lineStart);
        if (i <= t[0] && t[i] == '\r') i++;
        y += lh;
    }
}

static void DrawButtonFace(struct Dialog *d, DItem *it, bool pressed)
{
    (void)d;
    Rect r = it->rect;
    PaintRoundRect(&r, 16, 16, pressed ? kBlackIndex : kWhiteIndex);
    FrameRoundRect(&r, 16, 16, 1, kBlackIndex);
    TextFont(systemFont); TextSize(12); TextMode(srcOr);
    FontInfo fi;
    GetFontInfo(&fi);
    thePort->fg = it->hilite == 255 ? Gray() : (pressed ? kWhiteIndex : kBlackIndex);
    int w = StringWidth(it->text);
    MoveTo((r.left + r.right - w) / 2, r.top + (r.bottom - r.top - fi.ascent - fi.descent) / 2 + fi.ascent);
    DrawString(it->text);
}

static void DrawCheckBox(DItem *it, bool pressed)
{
    Rect r = it->rect, box;
    int h = r.bottom - r.top;
    SetRect(&box, r.left + 2, r.top + (h - 12) / 2, r.left + 14, r.top + (h - 12) / 2 + 12);
    FillRectIndex(&box, kWhiteIndex);
    PenNormal();
    thePort->fg = kBlackIndex;
    FrameRect(&box);
    if (pressed) { Rect in = box; InsetRect(&in, 1, 1); FrameRect(&in); }
    if (it->value) {
        MoveTo(box.left, box.top); LineTo(box.right - 1, box.bottom - 1);
        MoveTo(box.right - 1, box.top); LineTo(box.left, box.bottom - 1);
    }
    TextFont(systemFont); TextSize(12); TextMode(srcOr);
    FontInfo fi;
    GetFontInfo(&fi);
    thePort->fg = it->hilite == 255 ? Gray() : kBlackIndex;
    MoveTo(r.left + 18, r.top + (h - fi.ascent - fi.descent) / 2 + fi.ascent);
    DrawString(it->text);
}

static void DrawEditText(struct Dialog *d, int k)
{
    DItem *it = &d->item[k];
    Rect fr = it->rect;
    InsetRect(&fr, -3, -3);
    FillRectIndex(&fr, kWhiteIndex);
    PenNormal();
    thePort->fg = kBlackIndex;
    FrameRect(&fr);
    TextFont(systemFont); TextSize(12); TextMode(srcOr);
    FontInfo fi;
    GetFontInfo(&fi);
    int x = it->rect.left, base = it->rect.top + fi.ascent;
    MoveTo(x, base);
    DrawString(it->text);
    if (d->editItem == k + 1) {
        int s = d->selStart, e = d->selEnd;
        Rect hr;
        if (s != e) {
            SetRect(&hr, x + TextWidth(it->text, 1, s), it->rect.top, x + TextWidth(it->text, 1, e), it->rect.top + fi.ascent + fi.descent);
            InvertRect(&hr);
        } else if (d->caretOn) {
            int cx = x + TextWidth(it->text, 1, s);
            SetRect(&hr, cx, it->rect.top, cx + 1, it->rect.top + fi.ascent + fi.descent);
            FillRectIndex(&hr, kBlackIndex);
        }
    }
}

static void DrawItem(struct Dialog *d, int k, bool pressed)
{
    DItem *it = &d->item[k];
    GrafPtr save = thePort;
    SetPort(&d->win.port);
    Rect clip = d->win.port.clip;
    ClipToRect(&d->win.port.portRect);
    unsigned char buf[256];
    switch (it->type & 127) {
    case btnCtrl: DrawButtonFace(d, it, pressed); break;
    case chkCtrl: case radCtrl: DrawCheckBox(it, pressed); break;
    case statText:
        FillRectIndex(&it->rect, d->win.contentColor);
        TextFont(systemFont); TextSize(12); TextMode(srcOr);
        thePort->fg = kBlackIndex;
        Expand(it->text, buf);
        TextBox(buf, &it->rect);
        break;
    case editText: DrawEditText(d, k); break;
    case iconItem: {
        const CIcon *ic = GetCIcon(it->resID);
        if (ic) PlotCIcon(&it->rect, ic);
        else { thePort->fg = kBlackIndex; thePort->bg = kWhiteIndex; PlotIconID(&it->rect, it->resID); }
        break;
    }
    case picItem: DrawPicture(it->resID, &it->rect); break;
    default: break;
    }
    if (d->ring && d->defaultItem == k + 1 && (it->type & 127) == btnCtrl) {
        Rect r = it->rect;
        InsetRect(&r, -4, -4);
        FrameRoundRect(&r, 16, 16, 3, it->hilite == 255 ? Gray() : kBlackIndex);
    }
    d->win.port.clip = clip;
    SetPort(save);
}

static void DrawDialogItems(struct Dialog *d)
{
    for (int k = 0; k < d->n; k++) DrawItem(d, k, false);
    d->drawn = true;
}

static bool ParseDITL(struct Dialog *d, int id)
{
    int32_t size;
    const uint8_t *p = GetResource(FOURCC('D', 'I', 'T', 'L'), id, &size);
    if (!p || size < 2) return false;
    int n = rd16s(p) + 1;
    int32_t o = 2;
    for (int i = 0; i < n && i < MAX_DITEMS; i++) {
        if (o + 14 > size) break;
        DItem *it = &d->item[i];
        memset(it, 0, sizeof *it);
        SetRect(&it->rect, rd16s(p + o + 6), rd16s(p + o + 4), rd16s(p + o + 10), rd16s(p + o + 8));
        it->type = p[o + 12];
        int len = p[o + 13];
        o += 14;
        if (o + len > size) break;
        int t = it->type & 127;
        if (t == iconItem || t == picItem || t == resCtrl) it->resID = len >= 2 ? rd16s(p + o) : 0;
        else { it->text[0] = (unsigned char)len; memcpy(it->text + 1, p + o, (size_t)len); }
        o += len + (len & 1);
        d->n = i + 1;
    }
    for (int i = 0; i < d->n; i++)
        if ((d->item[i].type & 127) == editText) {
            d->editItem = i + 1;
            d->selStart = d->selEnd = d->item[i].text[0];
            break;
        }
    return true;
}

static struct Dialog *NewDialogWindow(const Rect *bounds, uint16_t pos, int ditl, uint8_t contentColor)
{
    struct Dialog *d = calloc(1, sizeof(struct Dialog));
    if (!d) return NULL;
    d->win.procID = dBoxProc;
    d->win.content = *bounds;
    d->win.contentColor = contentColor;
    d->win.slot = MAX_WINDOWS;   /* dialogs never get update events */
    Position(&d->win.content, dBoxProc, pos);
    UpdatePort(&d->win);
    PortReset(&d->win.port);
    UpdatePort(&d->win);
    d->win.port.bg = contentColor;
    d->defaultItem = 1;
    if (!ParseDITL(d, ditl)) { free(d); return NULL; }
    Rect s;
    StructRect(dBoxProc, &d->win.content, &s);
    SaveBits(&d->save, &s);
    DrawFrame(dBoxProc, &d->win.content, (const unsigned char *)"", contentColor);
    d->below = gDialogStack;
    gDialogStack = d;
    return d;
}

DialogPtr GetNewDialog(int id)
{
    int32_t size;
    const uint8_t *p = GetResource(FOURCC('D', 'L', 'O', 'G'), id, &size);
    if (!p || size < 21) return NULL;
    Rect r;
    SetRect(&r, rd16s(p + 2), rd16s(p), rd16s(p + 6), rd16s(p + 4));
    int32_t posAt = 20 + 1 + p[20];
    if (posAt & 1) posAt++;
    uint16_t pos = posAt + 2 <= size ? rd16(p + posAt) : 0;
    return NewDialogWindow(&r, pos, rd16s(p + 18), ContentColor(FOURCC('d', 'c', 't', 'b'), id, kWhiteIndex));
}

void DisposeDialog(DialogPtr d)
{
    if (!d) return;
    struct Dialog **pp = &gDialogStack;
    while (*pp && *pp != d) pp = &(*pp)->below;
    if (*pp) *pp = d->below;
    RestoreBits(&d->save);
    if (thePort == &d->win.port) thePort = &gScreenPort;
    free(d);
}

GrafPtr DialogPort(DialogPtr d) { return d ? &d->win.port : NULL; }

void SetDialogDefaultItem(DialogPtr d, int item) { d->defaultItem = item; d->ring = true; if (d->drawn) DrawItem(d, item - 1, false); }
void SetDialogCancelItem(DialogPtr d, int item) { d->cancelItem = item; }

static DItem *Item(DialogPtr d, int item) { return (d && item >= 1 && item <= d->n) ? &d->item[item - 1] : NULL; }

void GetDialogItemRect(DialogPtr d, int item, Rect *r)
{
    DItem *it = Item(d, item);
    if (it) *r = it->rect;
    else SetRect(r, 0, 0, 0, 0);
}

void SetDialogItemText(DialogPtr d, int item, const unsigned char *text)
{
    DItem *it = Item(d, item);
    if (!it) return;
    PStrCopy(it->text, text);
    if (d->editItem == item) d->selStart = d->selEnd = it->text[0];
    if (d->drawn) DrawItem(d, item - 1, false);
}

void GetDialogItemText(DialogPtr d, int item, unsigned char *text)
{
    DItem *it = Item(d, item);
    if (it) PStrCopy(text, it->text);
    else text[0] = 0;
}

void SelectDialogItemText(DialogPtr d, int item, int start, int end)
{
    DItem *it = Item(d, item);
    if (!it) return;
    d->editItem = item;
    d->selStart = start < it->text[0] ? start : it->text[0];
    d->selEnd = end < it->text[0] ? end : it->text[0];
    if (d->drawn) DrawItem(d, item - 1, false);
}

void SetControlValue(DialogPtr d, int item, int value)
{
    DItem *it = Item(d, item);
    if (!it) return;
    it->value = value;
    if (d->drawn) DrawItem(d, item - 1, false);
}

int GetControlValue(DialogPtr d, int item)
{
    DItem *it = Item(d, item);
    return it ? it->value : 0;
}

void HiliteControl(DialogPtr d, int item, int state)
{
    DItem *it = Item(d, item);
    if (!it) return;
    it->hilite = state;
    if (d->drawn) DrawItem(d, item - 1, false);
}

void SysBeep(int duration) { (void)duration; }

static bool TrackItem(struct Dialog *d, int k)
{
    DItem *it = &d->item[k];
    bool inside = true, last = false;
    int t = it->type & 127;
    bool control = t == btnCtrl || t == chkCtrl || t == radCtrl;
    if (!control) return true;
    if (it->hilite == 255) return false;
    for (;;) {
        Point p;
        GrafPtr save = thePort;
        SetPort(&d->win.port);
        GetMouse(&p);
        SetPort(save);
        inside = PtInRect(p, &it->rect);
        if (inside != last) { DrawItem(d, k, inside); last = inside; }
        if (!StillDown()) break;
        SDL_Delay(5);
    }
    if (last) DrawItem(d, k, false);
    return inside;
}

static void FlashButton(struct Dialog *d, int item)
{
    if (!Item(d, item) || (d->item[item - 1].type & 127) != btnCtrl) return;
    DrawItem(d, item - 1, true);
    Delay(8, NULL);
    DrawItem(d, item - 1, false);
}

static void EditKey(struct Dialog *d, unsigned char ch)
{
    DItem *it = &d->item[d->editItem - 1];
    unsigned char *t = it->text;
    int s = d->selStart < d->selEnd ? d->selStart : d->selEnd;
    int e = d->selStart < d->selEnd ? d->selEnd : d->selStart;
    if (ch == 28) { if (s == e && s > 0) s--; d->selStart = d->selEnd = s; }
    else if (ch == 29) { if (s == e && e < t[0]) e++; d->selStart = d->selEnd = e; }
    else if (ch == 8 || ch == 127) {
        if (s == e && s > 0 && ch == 8) s--;
        if (s == e && e < t[0] && ch == 127) e++;
        memmove(t + 1 + s, t + 1 + e, (size_t)(t[0] - e));
        t[0] = (unsigned char)(t[0] - (e - s));
        d->selStart = d->selEnd = s;
    } else if (ch >= 32) {
        int len = t[0] - (e - s);
        if (len >= 255) return;
        memmove(t + 1 + s, t + 1 + e, (size_t)(t[0] - e));
        t[0] = (unsigned char)len;
        memmove(t + 2 + s, t + 1 + s, (size_t)(len - s));
        t[1 + s] = ch;
        t[0]++;
        d->selStart = d->selEnd = s + 1;
    }
    d->caretOn = true;
    d->caretTick = TickCount();
    DrawItem(d, d->editItem - 1, false);
}

void ModalDialog(void *filter, INTEGER *itemHit)
{
    (void)filter;
    struct Dialog *d = gDialogStack;
    *itemHit = 0;
    if (!d) return;
    if (!d->drawn) DrawDialogItems(d);
    for (;;) {
        EventRecord ev;
        if (Plat_QuitRequested() && gQuitHook) gQuitHook();
        if (!GetNextEvent(mDownMask | keyDownMask | (1 << autoKey), &ev)) {
            if (d->editItem && TickCount() - d->caretTick >= 32) {
                d->caretOn = !d->caretOn;
                d->caretTick = TickCount();
                DrawItem(d, d->editItem - 1, false);
            }
            SDL_Delay(4);
            continue;
        }
        if (ev.what == mouseDown) {
            Point p = ev.where;
            GrafPtr save = thePort;
            SetPort(&d->win.port);
            GlobalToLocal(&p);
            SetPort(save);
            for (int k = 0; k < d->n; k++) {
                DItem *it = &d->item[k];
                if ((it->type & itemDisable) || !PtInRect(p, &it->rect)) continue;
                if ((it->type & 127) == editText) {
                    d->editItem = k + 1;
                    d->selStart = d->selEnd = it->text[0];
                    DrawItem(d, k, false);
                }
                if (TrackItem(d, k)) { *itemHit = (INTEGER)(k + 1); return; }
                break;
            }
        } else {
            unsigned char ch = (unsigned char)(ev.message & charCodeMask);
            if (ch == 13 || ch == 3) {
                if (d->defaultItem && Item(d, d->defaultItem) && d->item[d->defaultItem - 1].hilite != 255) {
                    FlashButton(d, d->defaultItem);
                    *itemHit = (INTEGER)d->defaultItem;
                    return;
                }
            } else if (ch == 27 || ((ev.modifiers & cmdKey) && ch == '.')) {
                if (d->cancelItem) {
                    FlashButton(d, d->cancelItem);
                    *itemHit = (INTEGER)d->cancelItem;
                    return;
                }
            } else if (d->editItem && !(ev.modifiers & cmdKey)) {
                EditKey(d, ch);
                *itemHit = (INTEGER)d->editItem;
                return;
            }
        }
    }
}

INTEGER Alert(int id, void *filter)
{
    int32_t size;
    const uint8_t *p = GetResource(FOURCC('A', 'L', 'R', 'T'), id, &size);
    if (!p || size < 12) return 1;
    Rect r;
    SetRect(&r, rd16s(p + 2), rd16s(p), rd16s(p + 6), rd16s(p + 4));
    uint16_t stages = rd16(p + 10);
    uint16_t pos = size >= 14 ? rd16(p + 12) : 0;
    struct Dialog *d = NewDialogWindow(&r, pos, rd16s(p + 8), ContentColor(FOURCC('a', 'c', 't', 'b'), id, kWhiteIndex));
    if (!d) return 1;
    int stage1 = stages & 0xF;
    d->defaultItem = (stage1 & 8) ? 2 : 1;
    d->ring = (stage1 & 4) != 0;
    INTEGER hit;
    do {
        ModalDialog(filter, &hit);
    } while ((d->item[hit - 1].type & 127) != btnCtrl);
    DisposeDialog(d);
    return hit;
}

void TB_Init(void)
{
    gUpdateEventHook = UpdateHook;
    LayoutBar();
}
