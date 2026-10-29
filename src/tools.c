/*
 * tools.c - translation of Tools.p (and the variables of Globals.p).
 */
#include "br.h"

#include <stdio.h>

/* Globals.p */
WindowPtr ShellWindow, RaceWindow;
MenuHandle AppleMenu, FileMenu, OptMenu;
bool GamePlay, GamePause;
INTEGER CarID, Level;
uint8_t ErrFlags;
INTEGER Depth = 8;
bool SoundDisable;
bool Cheater;
RGBColor Sb, Sf;

void ExitToShell(void)
{
    Snd_Shutdown();
    Text_Shutdown();
    Plat_Shutdown();   /* records the window size */
    Prefs_Save();
    Res_Shutdown();
    exit(0);
}

static bool CheckGameend(void)
{
    bool r = false;
    if (GamePlay) {
        if (Alert(1003, NULL) == 1) r = true;
    } else {
        r = true;
    }
    return r;
}

void Quit(void)
{
    Plat_ClearQuit();   /* a close request is answered by this alert */
    if (CheckGameend()) {
        ShowMenuBar();
        ExitToShell();
    }
}

void OffsetPt(Point *pt, int h, int v)
{
    pt->h += h;
    pt->v += v;
}

void InitSound(void)
{
    /* SndNewChannel for the engine and the two effect channels; the engine
     * channel's userInfo holds 'snd ' 2000. The mixer is opened once. */
    static bool opened;
    ErrFlags |= 0x80;
    if ((PrefFlag(kFlagEngine) || PrefFlag(kFlagSoundFX)) && !opened && !SoundDisable) {
        opened = true;
        if (!Snd_Init()) SoundDisable = true;
    }
    ErrFlags &= (uint8_t)~0x80;
}

void InitProg(void)
{
    /* InitMac: InitGraf ... InitCursor; the desktop is painted by InitWindows */
    RedrawScreen();
    InitCursor();

    /* SystemCheck: screen size, system version, sound hardware and colour
     * depth checks are all satisfied by the port. */
    GamePlay = false;

    ShellWindow = GetNewWindow(1000, NULL);

    AppleMenu = GetMenu(1000);
    InsertMenu(AppleMenu, 0);
    FileMenu = GetMenu(1001);
    InsertMenu(FileMenu, 0);
    OptMenu = GetMenu(1002);
    InsertMenu(OptMenu, 0);
    DrawMenuBar();

    gRandSeed = (int32_t)TickCount();
    InitSound();

    Sf = RGB(0xF000, 0xF000, 0x0);
    Sb = RGB(0, 0, 0);
    TextFont(0);
}

void DoError(int id)
{
    if (id == 0) return;
    unsigned char s[256];
    NumToString(id, s);
    ParamText(s, (const unsigned char *)"", (const unsigned char *)"", (const unsigned char *)"");
    Alert(1000, NULL);
    ExitToShell();
}

/* The race window is centred on the screen with SetOrigin, as in BeginGame. */
static void RestoreRaceOrigin(void)
{
    SetOrigin(-thePort->portRect.right / 2 + 310, -thePort->portRect.bottom / 2 + 240);
}

void HideMenuBar(void)
{
    Rect DrawRect;
    if (RaceWindow) {
        SetPort(WindowPort(RaceWindow));
        SetOrigin(0, 0);
        SetWindowClipsMenuBar(RaceWindow, false);
        SetRect(&DrawRect, 0, 0, thePort->portRect.right, 20);
        FillRectIndex(&DrawRect, kBlackIndex);
        SetMenuBarHeight(0);
        RestoreRaceOrigin();
    }
}

void ReHideMenuBar(void)
{
    if (RaceWindow) {
        SetPort(WindowPort(RaceWindow));
        SetOrigin(0, 0);
        SetWindowClipsMenuBar(RaceWindow, false);
        RestoreRaceOrigin();
    }
}

void ShowMenuBar(void)
{
    if (RaceWindow) {
        SetPort(WindowPort(RaceWindow));
        SetOrigin(0, 0);
        SetWindowClipsMenuBar(RaceWindow, true);
        RestoreRaceOrigin();
    }
    SetMenuBarHeight(20);
    DrawMenuBar();
}

static void InsertScore(LONGINT points, const unsigned char *Name, int place)
{
    /* 'HIGH' 127+i is gHighScores[i - 1] */
    for (int i = 9; i >= place; i--) gHighScores[i] = gHighScores[i - 1];
    gHighScores[place - 1].Score = points;
    PStrCopy(gHighScores[place - 1].Name, Name);
    Prefs_CommitScores();
    DisplayScores();
}

void EnterHighScore(LONGINT points)
{
    DialogPtr ScoreDlog;
    Str255 NameString;
    INTEGER hit;
    FlushEvents(everyEvent, 0);
    if (!Cheater)
        if (points > gHighScores[10].Score) {
            ScoreDlog = GetNewDialog(2006);
            SetDialogDefaultItem(ScoreDlog, 1);
            SelectDialogItemText(ScoreDlog, 2, 0, 32767);
            do {
                ModalDialog(NULL, &hit);
            } while (hit != 1);
            GetDialogItemText(ScoreDlog, 2, NameString);
            DisposeDialog(ScoreDlog);
            for (int i = 10; i >= 1; i--) {
                if (points < gHighScores[i - 1].Score) {
                    InsertScore(points, NameString, i + 1);
                    return;
                } else if (i == 1) {
                    InsertScore(points, NameString, 1);
                }
            }
        }
}

void DisplayScores(void)
{
    DialogPtr ScoreList;
    Str255 ScoreString;
    INTEGER Hit;
    ScoreList = GetNewDialog(2004);
    for (int i = 1; i <= 10; i++) {
        SetDialogItemText(ScoreList, i + 1, gHighScores[i - 1].Name);
        NumToString(gHighScores[i - 1].Score, ScoreString);
        SetDialogItemText(ScoreList, i + 11, ScoreString);
    }
    Hit = 0;
    while (Hit != 1) ModalDialog(NULL, &Hit);
    DisposeDialog(ScoreList);
}
