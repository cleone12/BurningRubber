/*
 * powershell.c - translation of PowerShell.p (the title screen).
 */
#include "br.h"

enum {
    RollConst = 0,
    RollInConst = 73 - 56,
    Plate1 = 85 - 56,
    Plate2 = 90 - 56,
    Plate3 = 95 - 56,
    Done = 100 - 56
};

static INTEGER PState, TState, TSize;
static const uint8_t *ScrollText;
static int32_t ScrollTextSize;
static GrafPtr PGWorld;
static Point raceTLpt, oldTLpt;

static void GWorldInitPic(void)
{
    Rect DrawingRect;
    GrafPtr old = thePort;
    SetRect(&DrawingRect, 0, 0, 200 * 2, 92);
    DisposeGWorld(PGWorld);
    PGWorld = NewGWorld(&DrawingRect);
    SetPort(PGWorld);
    SetRect(&DrawingRect, 0, 0, 200, 92);
    DrawPicture(2010, &DrawingRect);
    SetRect(&DrawingRect, 200, 0, 200 * 2, 92);
    DrawPicture(2011, &DrawingRect);
    SetPort(old);
}

/* drawRACE and drawSPEED: slide one half of the logo picture in, painting
 * black over what it leaves behind. */
static void DrawLogo(int srcLeft, Point ou, Point *old, GrafPtr win)
{
    Rect scr, DrawingRect;
    GrafPtr save = thePort;
    SetPort(win);
    SetRect(&scr, srcLeft, 0, srcLeft + 200, 92);
    if (old->h < ou.h) SetRect(&DrawingRect, old->h, old->v, ou.h, old->v + 92);
    else SetRect(&DrawingRect, old->h, ou.v + 92, old->h + 200, old->v + 92);
    FillRectIndex(&DrawingRect, kBlackIndex);
    SetRect(&DrawingRect, ou.h, ou.v, 200 + ou.h, 92 + ou.v);
    if (PGWorld) CopyBits(PGWorld, win, &scr, &DrawingRect);
    *old = ou;
    SetPort(save);
}

static void drawRACE(Point ou, Point *old, GrafPtr win) { DrawLogo(200, ou, old, win); }
static void drawSPEED(Point ou, Point *old, GrafPtr win) { DrawLogo(0, ou, old, win); }

static void DrawPlate(int left, int right, int pict)
{
    Rect r;
    SetRect(&r, left, 237, right, 278);
    DrawPicture(pict, &r);
}

static void DrawShareware(void)
{
    Str255 s;
    MoveTo(15, 295);
    TextFont(0);
    TextSize(12);
    BackColor(whiteColor);
    TextMode(srcBic);
    GetString(s, 1000);
    DrawString(s);
}

static void PowerPlay(void)
{
    if (PState == RollConst) {
        SetPt(&raceTLpt, -45, 10);
        oldTLpt = raceTLpt;
        PlaySound(3001);
        GWorldInitPic();
    } else if (PState < RollInConst) {
        drawSPEED(raceTLpt, &oldTLpt, WindowPort(ShellWindow));
        OffsetPt(&raceTLpt, 13, 0);
    } else if (PState == RollInConst) {
        drawRACE(raceTLpt, &oldTLpt, WindowPort(ShellWindow));
        DisposeGWorld(PGWorld);
        PGWorld = NULL;
        EnableItem(AppleMenu, 1);
    }
    if (PState <= Plate3) {
        if (PState == Plate1) { DrawPlate(21, 106, 2020); PlaySound(3002); }
        if (PState == Plate2) { DrawPlate(214, 299, 2021); PlaySound(3002); }
        if (PState == Plate3) { DrawPlate(407, 491, 2022); PlaySound(3002); DrawShareware(); }
    }

    if (PState == Done) {
        TextFont(201);
        TextSize(34);
        TextMode(notSrcCopy);
        BackColor(redColor);
        if (!ScrollText) ScrollText = GetResource(FOURCC('T', 'E', 'X', 'T'), 1000, &ScrollTextSize);
        if (TSize == 0) TSize = TextWidth(ScrollText, 0, ScrollTextSize);
        if (TState < 0) TState = (INTEGER)(TState + TSize);
        if (TState > TSize) TState = (INTEGER)(TState - TSize);
        MoveTo(0 - TState, 200);
        if (ScrollText) DrawText(ScrollText, 0, ScrollTextSize);
        TState = (INTEGER)(TState + 15);
    }
    if (PState != Done) PState = (INTEGER)(PState + 1);
}

/* One plate button: show the pressed picture while the mouse is held. */
static bool TrackPlate(Point Hit, int left, int right, int pressed, int normal)
{
    Rect InRect;
    SetRect(&InRect, left, 237, right, 278);
    if (!PtInRect(Hit, &InRect)) return false;
    DrawPicture(pressed, &InRect);
    while (StillDown()) Plat_Sleep(5);
    GetMouse(&Hit);
    bool in = PtInRect(Hit, &InRect);
    DrawPicture(normal, &InRect);
    return in;
}

void HandlePSClick(Point Hit, INTEGER *reply)
{
    Rect InRect;
    *reply = 0;
    if (PState == Done) {
        SetRect(&InRect, 21, 237, 106, 278);
        if (PtInRect(Hit, &InRect)) { if (TrackPlate(Hit, 21, 106, 3020, 2020)) *reply = 1; return; }
        SetRect(&InRect, 214, 237, 299, 278);
        if (PtInRect(Hit, &InRect)) { if (TrackPlate(Hit, 214, 299, 3021, 2021)) *reply = 2; return; }
        if (TrackPlate(Hit, 407, 491, 3022, 2022)) *reply = 3;
    }
}

void PSUpdate(void)
{
    Rect DrawingRect;
    BeginUpdate(ShellWindow);
    SetPort(WindowPort(ShellWindow));
    if (PState >= RollInConst) {
        SetRect(&DrawingRect, 163, 10, 163 + 200, 102);
        DrawPicture(2011, &DrawingRect);
    }
    if (PState >= Plate1) DrawPlate(21, 106, 2020);
    if (PState >= Plate2) DrawPlate(214, 299, 2021);
    if (PState >= Plate3) {
        DrawPlate(407, 491, 2022);
        DrawShareware();
    }
    EndUpdate(ShellWindow);
}

void PowerShell(void)
{
    uint32_t time;
    if (!GamePlay) {
        if (!ShellWindow) return;
        SetPort(WindowPort(ShellWindow));
        time = TickCount();
        PowerPlay();
        WaitUntilTick(time + 2);
    } else {
        PState = Done;
    }
}

void PSAbout(void)
{
    const int SpeedStop = 169;
    DialogPtr AboutDlg;
    INTEGER hit;
    uint32_t Ticky;
    Rect DrawingRect;
    AboutDlg = GetNewDialog(2005);
    SetDialogDefaultItem(AboutDlg, 1);
    SetPt(&raceTLpt, -52, 12);
    oldTLpt = raceTLpt;
    SetPort(DialogPort(AboutDlg));
    SetRect(&DrawingRect, 0, 0, 360, 157);
    FillRectIndex(&DrawingRect, kBlackIndex);
    SetRect(&DrawingRect, 0, 157, 222, 190);
    FillRectIndex(&DrawingRect, kBlackIndex);
    GWorldInitPic();
    do {
        Ticky = TickCount();
        OffsetPt(&raceTLpt, 13, 0);
        drawSPEED(raceTLpt, &oldTLpt, DialogPort(AboutDlg));
        WaitUntilTick(Ticky + 2);
    } while (raceTLpt.h != SpeedStop);
    drawRACE(raceTLpt, &oldTLpt, DialogPort(AboutDlg));
    SetRect(&DrawingRect, -3, 0, 360, 190);
    PlaySound(3000);
    SetPort(DialogPort(AboutDlg));
    DrawPicture(1101, &DrawingRect);
    do {
        ModalDialog(NULL, &hit);
    } while (hit != 1 && hit != 2);
    DisposeDialog(AboutDlg);
    DisposeGWorld(PGWorld);
    PGWorld = NULL;
    if (hit == 2) PSHelp();
}

void PSHelp(void)
{
    Rect DrawingRect;
    SetPort(WindowPort(ShellWindow));
    SetRect(&DrawingRect, 0, 0, 513, 301);
    DrawPicture(1100, &DrawingRect);
    while (!Button()) Plat_Sleep(5);
    FillRectIndex(&DrawingRect, kBlackIndex);
    InvalRect(&DrawingRect);
    PSUpdate();
}
