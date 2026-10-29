/*
 * game.c - translation of Game.p.
 */
#include "br.h"

void DoGame(void)
{
    uint32_t Timer;
    Rect DRECT;
    if (GamePlay) {
        Timer = TickCount();
        SetPort(ScreenWorld);
        if (!GamePause) {
            DoBackground();
            DoObjects();
            DoCar();
            DoAnimations();
            DoControl();
            DoDisplay();
        }
        if (GamePause) {
            KillEngine();
            SetRect(&DRECT, 320 - 43, 240 - 21, 320 + 43, 240 + 21);
            DrawPicture(2040, &DRECT);
            URects[15] = DRECT;
        }
        BlitToScreen();
        WaitUntilTick(Timer + (uint32_t)GamePrefs.Speed);
        if (GameOCount == 6) EndGame();
    }
}

static void InitWorlds(void)
{
    Rect drawRect;
    Depth = 8;   /* ScreenCT = 'clut' 1008, the standard 8-bit palette */
    SetRect(&drawRect, 0, 0, 620, 480);
    ScreenWorld = NewGWorld(&drawRect);
    SetPort(ScreenWorld);
    EraseRect(&drawRect);

    SetRect(&drawRect, 0, -44, GamePrefs.TextureSize, 480);
    if (PrefFlag(kFlagBackTex)) Backworld = NewGWorld(&drawRect);
}

static void FadeBlackWindow(void)
{
    const int FadeSpeed = 40;
    for (int i = 1; i <= FadeSpeed; i++) {
        ScreenBlood(1, true, true, true);
        Delay(1, NULL);   /* let each step reach the display */
    }
    if (PrefFlag(kFlagBlankScreen)) {
        RaceWindow = GetNewWindow(1001, NULL);
        MoveWindow(RaceWindow, 0, 0, false);
        SizeWindow(RaceWindow, SCREEN_W, Plat_ScreenHeight(), false);
        SetPort(WindowPort(RaceWindow));
        SetOrigin(-thePort->portRect.right / 2 + 310, -thePort->portRect.bottom / 2 + 240);
        HideMenuBar();
        SetCLUT();
    } else {
        SetCLUT();
        RaceWindow = GetNewWindow(1002, NULL);
    }
}

static void InitInterface(void)
{
    GamePlay = true;
    EnableItem(FileMenu, 2);
    EnableItem(FileMenu, 4);
    DisableItem(FileMenu, 1);
    DisableItem(OptMenu, 1);
    DisableItem(OptMenu, 7);
    DisposeWindow(ShellWindow);
    ShellWindow = NULL;

    FadeBlackWindow();
}

/* The instrument panel's outline: everything that is black in PICT 2000
 * (plus the strip right of it) is panel; the game view only shows through
 * the rest. Clip holds 1 where BlitToScreen may draw. */
static void InitMask(void)
{
    Rect GWldSize, DrawingRect;
    SetRect(&GWldSize, 0, 0, 620, 100);
    GrafPtr WriteWorld = NewGWorld(&GWldSize);
    GrafPtr save = thePort;
    SetPort(WriteWorld);
    FillRectIndex(&GWldSize, kWhiteIndex);
    SetRect(&DrawingRect, 0, 0, 100, 100);
    DrawPicture(2000, &DrawingRect);
    SetRect(&DrawingRect, 100, 72, 620, 100);
    FillRectIndex(&DrawingRect, kBlackIndex);
    SetPort(save);

    Rect cb;
    SetRect(&cb, 0, 480 - 100, 620, 480);
    DisposeGWorld(Clip);
    Clip = NewGWorld(&cb);
    for (int i = 0; i < 620 * 100; i++) Clip->base[i] = WriteWorld->base[i] == kWhiteIndex;
    DisposeGWorld(WriteWorld);
}

static void Drawscreen(void)
{
    Rect DrawRect;
    SetPort(WindowPort(RaceWindow));
    SetRect(&DrawRect, 0, 0, 620, 480);
    FillRectIndex(&DrawRect, kBlackIndex);
    SetRect(&DrawRect, 310 - 83, 240 - 21, 310 + 83, 240 + 21);
    DrawPicture(2030, &DrawRect);
    InitMask();
    BeginUpdate(RaceWindow);
    EndUpdate(RaceWindow);
}

void BeginGame(void)
{
    InitWorlds();
    InitInterface();
    Drawscreen();
    PreLoadSprites();

    CarID = 1000;

    Score = 0;
    Lives = 3;
    oldScore = -1;
    oldLives = -1;
    Lastlive = 0;

    GameOCount = 0;
    GameOver = false;

    EnterLevel(Level);

    int32_t size;
    const uint8_t *c = GetResource(FOURCC('C', 'A', 'R', ' '), CarID, &size);
    if (c && size >= 20) {
        MyCar.BaseIcon = rd16s(c);
        MyCar.Crash = rd16s(c + 2);
        MyCar.Drawn = rd16s(c + 4);
        MyCar.Speed = rd16s(c + 6);
        MyCar.Acc = rd16s(c + 8);
        MyCar.Brakes = rd16s(c + 10);
        MyCar.Steering = rd16s(c + 12);
        MyCar.Frames = rd16s(c + 14);
        MyCar.Bumping = rd16s(c + 16);
        MyCar.OffRoad = rd16s(c + 18);
    }
}

static bool CancelTest(void)
{
    bool r = false;
    if (GameOCount == 6) r = true;
    else if (Alert(1003, NULL) == 1) r = true;
    else if (PrefFlag(kFlagBlankScreen)) ReHideMenuBar();
    return r;
}

void EndGame(void)
{
    if (CancelTest()) {
        ShowCursor();
        ShowMenuBar();
        KillEngine();

        ShellWindow = GetNewWindow(1000, RaceWindow);
        GamePlay = false;

        DisposeWindow(RaceWindow);
        RaceWindow = NULL;
        DisposeGWorld(Backworld);
        Backworld = NULL;
        DisposeGWorld(ScreenWorld);
        ScreenWorld = NULL;
        DisposeLevel();

        EnableItem(FileMenu, 1);
        EnableItem(OptMenu, 1);
        EnableItem(OptMenu, 7);
        DisableItem(FileMenu, 2);
        DisableItem(FileMenu, 4);

        EnterHighScore(Score);
    }
}

void GameUpdate(void)
{
    BeginUpdate(RaceWindow);
    DrawInstr();
    EndUpdate(RaceWindow);
}
